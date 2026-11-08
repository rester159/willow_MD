#!/usr/bin/env python3
"""Conservative pointer-consumer discovery, not whole-program alias proof."""
import collections,hashlib,json,re,subprocess
from pathlib import Path
import native_video_relocation as relocation
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/native-scroll1-audit';OUT.mkdir(exist_ok=True)
ROM=ROOT/'build/reference_maincpu.bin';blob=ROM.read_bytes()
patches=[p for p in relocation.patches_for(blob) if p.size==4 and 0x910000<=p.source<0x914000]
regs={**{f'%d{i}':i for i in range(8)},**{f'%a{i}':8+i for i in range(6)},'%fp':14,'%sp':15}
cache={};decoded=0
rx=re.compile(r'^\s*([0-9a-f]+):\s*((?:[0-9a-f]{4} ?)+)\s*\t([^\t]+)$')
def decode(pc):
 global decoded
 if pc in cache:return cache[pc]
 s=subprocess.check_output(['m68k-elf-objdump','-D','-b','binary','-m','m68k:68000',f'--start-address={pc}',f'--stop-address={min(pc+256,len(blob))}',str(ROM)],text=True)
 items=[]
 for line in s.splitlines():
  m=rx.match(line)
  if m:items.append((int(m[1],16),m[3].strip(),len(m[2].replace(' ',''))//2))
 for i,(at,text,length) in enumerate(items[:-1]):
  nxt=items[i+1][0] if i+1<len(items) else at+length
  cache.setdefault(at,(text,nxt))
 decoded+=1
 return cache.get(pc,('.unknown',pc+2))
def operands(s):
 out=[];depth=0;start=0
 for i,c in enumerate(s):
  if c=='(':depth+=1
  elif c==')':depth-=1
  elif c==',' and not depth:out.append(s[start:i]);start=i+1
 out.append(s[start:]);return out

def refs(arg):return {regs[r] for r in re.findall(r'%(?:d[0-7]|a[0-5]|fp|sp)\b',arg)}
def direct(arg):return regs.get(arg)
def memory(arg):return '@' in arg or arg.startswith('0x')
def target(arg):
 m=re.fullmatch(r'0x([0-9a-f]+)',arg)
 return int(m[1],16) if m else None
roots=[]
for p in patches:
 op=int.from_bytes(blob[p.offset-2:p.offset],'big')
 if op&0xf1ff in(0x41f9,0x207c):roots.append((p.offset-2,p.offset+4,1<<(8+((op>>9)&7))))
# Original ROM tables have explicit consumers; seed the loaded result only.
for start,nxt,mask in ((0x502a,0x502e,1<<8),(0x79fa,0x79fe,1<<8),(0x7b94,0x7b98,1<<8),(0x7b9c,0x7ba0,1<<12),(0x7eaa,0x7eae,1<<8),(0xbdb4,0xbdb8,1<<8)):
 roots.append((start,nxt,mask))
# Retained pointer fields with explicit original stores: BA62/BA70 and
# 45AFA/45B04/45B0C. These seeds apply only to those source lifecycle contexts.
for start,nxt,reg in ((0xbafa,0xbafe,8),(0xbc40,0xbc44,9),(0xbc78,0xbc7c,8),(0xbd82,0xbd86,8),(0xbd8e,0xbd92,8),(0xbe4a,0xbe4e,9),(0x45bf0,0x45bf4,8),(0x45bf4,0x45bf8,9),(0x45bf8,0x45bfc,10)):
 roots.append((start,nxt,1<<reg))
reads=collections.defaultdict(set);writes=collections.defaultdict(set);escapes=collections.defaultdict(set);limits=[];states=0
for ri,(root,start,mask) in enumerate(roots):
 todo=[(start,mask,())];seen=set()
 while todo:
  pc,mask,stack=todo.pop();key=(pc,mask,stack)
  if not mask or key in seen:continue
  seen.add(key);states+=1
  if len(seen)>15000:limits.append(hex(root));break
  if pc>=len(blob) or pc&1:escapes[('invalid-PC',pc)].add(root);continue
  ins,nxt=decode(pc);parts=ins.split(None,1);op=parts[0];args=operands(parts[1]) if len(parts)>1 else[]
  if op.startswith('.') or op in('illegal','stop','reset'):escapes[(ins,pc)].add(root);continue
  if op.startswith('rts'):
   if stack:todo.append((stack[-1],mask,stack[:-1]))
   continue
  if op.startswith('rte'):continue
  if op.startswith('trap'):
   escapes[(ins,pc)].add(root);continue
  if op.startswith(('jsr','bsr')):
   to=target(args[0])
   if to in(0x1750,0x1784):todo.append((nxt,mask,stack));continue # independently proven context-preserving waits
   if to is None or len(stack)>=12:escapes[(ins,pc)].add(root);continue
   todo.append((to,mask,stack+(nxt,)));continue
  if op=='jmp' or op.startswith('bra'):
   to=target(args[0])
   if to is None:escapes[(ins,pc)].add(root)
   else:todo.append((to,mask,stack))
   continue
  if op.startswith('db') or (op.startswith('b') and not op.startswith(('btst','bset','bclr','bchg'))):
   to=target(args[-1])
   if to is not None:todo.append((to,mask,stack))
   else:escapes[(ins,pc)].add(root)
   todo.append((nxt,mask,stack));continue
  # Effective-address consumers are recorded conservatively on every path.
  for ai,arg in enumerate(args):
   if memory(arg) and any(mask>>r&1 for r in refs(arg)):
    if op=='lea':continue
    write=(op.startswith(('move','clr','s')) and ai==len(args)-1 and not op.startswith(('sub','swap'))) or (op.startswith(('add','sub','and','or','eor','neg','not','bset','bclr','bchg','lsl','lsr','asl','asr','rol','ror')) and ai==len(args)-1)
    read=not(op.startswith(('move','clr')) and ai==len(args)-1)
    if read:reads[(pc,ins)].add(root)
    if write:writes[(pc,ins)].add(root)
  if op.startswith('movem'):
   escapes[('register-save/restore needs contextual mask audit: '+ins,pc)].add(root)
  elif args:
   d=direct(args[-1]);source=args[0]
   if op.startswith('move') or op=='lea':
    value=(not memory(source) or op=='lea') and any(mask>>r&1 for r in refs(source))
    if memory(args[-1]) and value:escapes[('stored pointer: '+ins,pc)].add(root)
    if d is not None:
     if value:mask|=1<<d
     else:mask&=~(1<<d)
   elif op.startswith('clr') and d is not None:mask&=~(1<<d)
   elif op.startswith(('add','sub','and','or','eor')) and d is not None and not memory(source):
    if any(mask>>r&1 for r in refs(source)):mask|=1<<d
  todo.append((nxt,mask,stack))
 if ri%40==0:print('root',ri,'states',states,'blocks',decoded,flush=True)
def entries(d):return [{'pc':f'{pc:06x}','instruction':ins,'roots':[f'{r:06x}' for r in sorted(roots)]} for (pc,ins),roots in sorted(d.items())]
raw={at:int.from_bytes(blob[at:at+4],'big') for at in range(0,len(blob)-3,2) if 0x910000<=int.from_bytes(blob[at:at+4],'big')<0x914000}
known={p.offset for p in patches}
result={'schema_version':1,'scope':'Conservative pointer-consumer discovery. Indirect jumps, stored pointers, MOVEM restoration and unresolved paths forbid global no-read admission; false positives need original-source review. Explicit indexed-table seed consumers must be checked.','source_sha256':hashlib.sha256(blob).hexdigest(),'validated_operand_count':len(patches),'roots':[{ 'pc':f'{r:06x}','resume':f'{pc:06x}','register_mask':mask} for r,pc,mask in roots],'operands':[{'offset':f'{p.offset:06x}','source':f'{p.source:08x}','purpose':p.purpose} for p in patches],'unclassified_raw_candidates':[{'offset':f'{at:06x}','value':f'{value:08x}'} for at,value in raw.items() if at not in known],'reads':entries(reads),'writes':entries(writes),'unresolved':[{'pc':f'{pc:06x}','reason':reason,'roots':[f'{r:06x}' for r in sorted(roots)]} for(reason,pc),roots in sorted(escapes.items())],'state_limit_roots':limits,'states':states,'decoded_blocks':decoded}
(OUT/'static-discovery.json').write_text(json.dumps(result,indent=2)+'\n')
print('Done',len(roots),'roots',len(reads),'read candidates',len(writes),'writes',len(escapes),'unresolved',len(limits),'limits')
