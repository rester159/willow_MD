#!/usr/bin/env python3
"""Source1554 reachable instruction audit for physical level6 platform binding."""
from pathlib import Path
import argparse,collections,hashlib,json,re,subprocess
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/native-irq-mask-audit/v1');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 source=ROOT/'build/reference_maincpu.bin';rom=source.read_bytes();assert sha(source)=='0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1'
 command=['m68k-elf-objdump','-D','-b','binary','-m','m68k:68000','--insn-width=20',str(source)]
 decoded={}
 def parse(text):
  for line in text.splitlines():
   m=re.match(r'^\s*([0-9a-f]+):\s*((?:[0-9a-f]{4} )+)\s*\t(.+)$',line)
   if m:
    at=int(m[1],16);raw=bytes.fromhex(m[2]);assembly=m[3].strip();op,_,args=assembly.partition(' ');args=args.strip()
    assert rom[at:at+len(raw)]==raw
    decoded[at]=dict(pc=at,bytes=raw.hex(),size=len(raw),op=op,args=args,assembly=assembly)
 parse(subprocess.check_output(command,text=True))
 pending=[0x1554];seen=set();edges=[];unresolved=[];special=[];calls=set()
 def add(at,to,why):
  edges.append(dict(pc=at,to=to,kind=why));pending.append(to)
 while pending:
  at=pending.pop()
  if at in seen:continue
  if at not in decoded:
   parse(subprocess.check_output(command+[f'--start-address={at}',f'--stop-address={at+32}'],text=True))
  if at not in decoded:raise AssertionError(hex(at))
  q=decoded[at];seen.add(at);op=q['op'];args=q['args'];next_pc=at+q['size']
  if '%sr' in args or '%ccr' in args or '%usp' in args or op in ['rte','rtr','trap','stop','reset','illegal','trapv']:
   special.append(q)
  if op in ['rts','rte','rtr']:continue
  if op in ['stop','reset','illegal'] or op.startswith('.'):
   unresolved.append(dict(**q,reason='nonordinary instruction'));continue
  if op=='trap':
   vector=32+int(args.lstrip('#'));to=int.from_bytes(rom[vector*4:vector*4+4],'big');calls.add(to);add(at,to,'trap');add(at,next_pc,'trap_return');continue
  branch=re.fullmatch(r'b(ra|sr|hi|ls|cc|cs|ne|eq|vc|vs|pl|mi|ge|lt|gt|le)[sw]?',op)
  direct=re.fullmatch(r'0x([0-9a-f]+)',args)
  if branch:
   assert direct,(hex(at),q);to=int(direct[1],16);add(at,to,'call'if branch[1]=='sr'else'branch')
   if branch[1]=='sr':calls.add(to)
   if branch[1]!='ra':add(at,next_pc,'fallthrough')
   continue
  if op.startswith('db'):
   to=re.search(r',0x([0-9a-f]+)$',args);assert to,(hex(at),q);add(at,int(to[1],16),'db');add(at,next_pc,'fallthrough');continue
  if op in ['jsr','jmp']:
   if direct:
    to=int(direct[1],16);add(at,to,'call'if op=='jsr'else'jump')
    if op=='jsr':calls.add(to)
   else:unresolved.append(dict(**q,reason='indirect control flow'))
   if op=='jsr':add(at,next_pc,'call_return')
   continue
  add(at,next_pc,'next')
 # Exact opcode checks back the textual disassembly audit. RTE consumes the
 # untouched original exception frame; it is not a live-mask data consumer.
 live_sr=[];usp=[];stack=[]
 for at in sorted(seen):
  q=decoded[at];word=int(q['bytes'][:4],16)
  if (word&0xffc0) in [0x40c0,0x46c0] or word in [0x007c,0x027c,0x0a7c,0x4e72]:live_sr.append(q)
  if word&0xfff0==0x4e60:usp.append(q)
  if '%sp' in q['args']:stack.append(q)
 assert not unresolved and not live_sr and not usp
 assert {q['pc']for q in special}=={0x15b8,0x1ccc,0x1d06}
 assert {q['pc']for q in stack}=={0x1554,0x15b4}
 result=dict(scope=__doc__,source_sha256=sha(source),auditor_sha256=sha(Path(__file__)),instructions=len(seen),minimum_pc=min(seen),maximum_pc=max(seen),call_targets=sorted(calls),special=special,live_sr=live_sr,usp=usp,stack=stack,unresolved=unresolved,edges=edges,
  conclusion='Keeping physical interrupt mask6 through this original handler does not alter any source live-SR-dependent computation; the only status return is source15B8 RTE of the untouched exception frame. This preserves exclusion of another logically equal-priority sourceIRQ2 when carried by GenesisIRQ6.',
  limitations='Static reachable original-code audit, not timing/nesting test. Assumes valid source data and normal calls/returns; memory corruption/exceptions are not modeled. Added native wrappers/hooks must independently preserve the physical mask and exception frame. Cold overload/backlog remains a separate issue.')
 (out/'reachable.disasm').write_text('\n'.join(f'{at:06x}: {decoded[at]["bytes"]:20} {decoded[at]["assembly"]}'for at in sorted(seen))+'\n')
 (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps({k:result[k]for k in ['instructions','minimum_pc','maximum_pc','call_targets','special','unresolved']},indent=2))
if __name__=='__main__':main()
