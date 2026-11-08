#!/usr/bin/env python3
"""Compare genuine closed continuations of five current columns D0 failures.

This is evidence for the captured states only. It does not install a register
normalizer, alter the live observer, or claim arbitrary task/IRQ equivalence.
"""
from pathlib import Path
import argparse,csv,hashlib,json,re,struct,subprocess
ROOT=Path(__file__).resolve().parents[1]
FRAMES=[1467,1468,1470,1472,1696]
CORE=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def word(b,n):return struct.unpack_from('>H',b,n)[0]
def long(b,n):return struct.unpack_from('>I',b,n)[0]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--snapshots',type=Path,default=ROOT/'build/native-d0-residue-audit/columns-snapshots-v2');p.add_argument('--output',type=Path,default=ROOT/'build/native-d0-residue-audit/columns-proof-v3');a=p.parse_args();snap=a.snapshots.resolve();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
 evidence=json.loads((snap/'evidence.json').read_text());assert evidence['exit_code']==3
 for name,digest in evidence['outputs'].items():assert sha(snap/name)==digest
 assert sha(snap/'d0-lineage.csv')==evidence['lineage_sha256'] and sha(CORE)==evidence['core_sha256']
 target=ROOT/'build/native-forest-extension/target-columns-v1-frozen';manifest=json.loads((target/'manifest.json').read_text());original=(snap/'oracle.bin').read_bytes();rom=bytearray(original)
 for name,digest in manifest['products'].items():assert sha(target/name)==digest
 # Apply only the already declared source-bus differences. Preserve the exact
 # Genesis carrier vectors/header that were present in the live serialized run.
 old=(target/'program-original.bin').read_bytes();native=(target/'program.bin').read_bytes()
 for n in range(len(old)):
  if old[n]!=native[n]:assert rom[n]==old[n];rom[n]=native[n]
 native_platform=(target/'platform.bin').read_bytes();rom[0x100010:0x100010+len(native_platform)]=native_platform
 native_path=out/'oracle-native.bin';native_path.write_bytes(rom)
 tracer=ROOT/'tools/trace_native_d0_columns.c';binary=out/'trace';cmd=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror',str(tracer),'-o',str(binary)];subprocess.run(cmd,check=True)
 runs=[]
 for frame in FRAMES:
  for path in [0,1]:
   stem=f'frame-{frame}-path-{path}';command=[str(binary),str(CORE),str(snap/'oracle.bin'if path==0 else native_path)]+[str(snap/(stem+'.'+ext))for ext in ['state','source','sram','cps','registers.bin']]+[str(out/stem)]
   result=subprocess.run(command,capture_output=True,text=True);runs.append({'frame':frame,'path':path,'command':command,'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr});assert result.returncode==0,runs[-1]
 (out/'runs.json').write_text(json.dumps(runs,indent=2)+'\n')
 decoded={}
 def decode(pc,both=True):
  if pc not in decoded:
   text=subprocess.check_output(['m68k-elf-objdump','-D','-b','binary','-m','m68k:68000','--insn-width=20',f'--start-address={pc}',f'--stop-address={pc+12}',str(snap/'oracle.bin')],text=True)
   match=next(re.match(r'^\s*([0-9a-f]+):\s*((?:[0-9a-f]{4} )+)\s*\t(.+)$',line)for line in text.splitlines()if re.match(r'^\s*'+format(pc,'x')+r':',line));raw=bytes.fromhex(match[2]);assembly=match[3].strip();op,_,args=assembly.partition(' ')
   assert original[pc:pc+len(raw)]==raw
   if both:assert bytes(rom[pc:pc+len(raw)])==raw
   decoded[pc]={'pc':pc,'bytes':raw.hex(),'assembly':assembly,'op':op,'args':args.strip(),'size':len(raw)}
  return decoded[pc]
 def use(q):
  op,args=q['op'],q['args']
  if '%d0'not in args:return 'no_d0_use'
  if op=='movel'and args.endswith(',%d0')and'%d0'not in args[:-4]:return 'independent_full_definition'
  if op=='moveq':return 'independent_full_definition'
  if op in ['dbf','dbra']or op.endswith(('w','b')):return 'low_word_or_byte_only'
  raise AssertionError(('Unproved incomingD0 scalar use',q))
 lineages=[{k:int(v)for k,v in q.items()}for q in rows(snap/'d0-lineage.csv')]
 cases=[]
 for frame in FRAMES:
  stem=lambda path:f'frame-{frame}-path-{path}'
  instructions=[rows(out/(stem(path)+'.instructions.csv'))for path in [0,1]];registers=[rows(out/(stem(path)+'.registers.csv'))for path in [0,1]];accesses=[rows(out/(stem(path)+'.accesses.csv'))for path in [0,1]]
  assert len(instructions[0])==len(instructions[1]) and len(accesses[0])==len(accesses[1])
  effects=[]
  for x,y in zip(*instructions):
   assert{xk:xv for xk,xv in x.items()if xk not in ['d0_before','d0_after']}=={yk:yv for yk,yv in y.items()if yk not in ['d0_before','d0_after']}
   q=decode(int(x['pc'],16));assert int(q['bytes'][:4],16)==int(x['opcode'],16);u=use(q)
   if u!='no_d0_use':effects.append(dict(q,step=int(x['step']),classification=u))
  assert effects[-1]['pc']==0x1e2c and effects[-1]['classification']=='independent_full_definition'
  assert all(q['classification']=='low_word_or_byte_only'for q in effects[:-1])
  for n,(x,y)in enumerate(zip(*registers)):
   assert all(x[k]==y[k]for k in x if k!='r0')
   if n<len(registers[0])-1:assert int(x['r0'])&65535==int(y['r0'])&65535
   else:assert x==y and int(x['r16'])==0x1e30 and int(x['r0'])==0xffffffff
  for x,y in zip(*accesses):assert{k:v for k,v in x.items()if k!='current_d0'}=={k:v for k,v in y.items()if k!='current_d0'}
  sources=[(snap/(stem(path)+'.source')).read_bytes()for path in [0,1]];before=[b[57856:]for b in sources];after=[(out/(stem(path)+'.final-ram.bin')).read_bytes()for path in [0,1]]
  # Every differing byte at the end must be the exact unchanged, previously
  # compared pair from the genuine snapshots, never a newly ignored write.
  prior_differences={n:(before[0][n],before[1][n])for n in range(65536)if before[0][n]!=before[1][n]}
  final_differences={n:(after[0][n],after[1][n])for n in range(65536)if after[0][n]!=after[1][n]}
  assert final_differences==prior_differences
  for path in [0,1]:
   for suffix in ['sram','cps']:assert(out/(stem(path)+'.final-'+suffix+'.bin')).read_bytes()==(snap/(stem(path)+'.'+suffix)).read_bytes()
   assert(out/(stem(path)+'.final-ports.bin')).read_bytes()==sources[path][55296:55808]
  assert before[0][:512]==before[1][:512];tasks=[]
  for at in range(0,512,32):
   if before[0][at]:tasks.append({'slot':0xff0000+at,'state':before[0][at],'sleep':before[0][at+1],'sr':word(before[0],at+2),'pc':long(before[0],at+4),'usp':long(before[0],at+8),'stack_top':long(before[0],at+12)})
  assert{q['slot']:q['pc']for q in tasks}=={0xff0060:0x1756,0xff00a0:0x1786}
  lineage=[]
  for path in [0,1]:
   selected=[q for q in lineages if q['frame']==frame and q['path']==path]
   # First actual bus read after3B57A. Four prior straight-line instructions
   # from the observed zero mode byte establish MOVE.L A0,D0 on this branch.
   producers=[(n,q)for n,q in enumerate(selected)if q['pc']in[0x3b588,0x3b5aa]and q['kind']==0]
   n,producer=producers[-1];prior=next(q for q in reversed(selected[:n])if q['pc']==0x3b578 and q['kind']==0)
   assert prior['address']==0x82a0 and prior['width']==1 and prior['value']==0
   assert producer['d0']==producer['a0']==prior['a0'] and producer['task']&0xffffff==0xff0060
   base=0x900000 if path==0 else 0x20c000;other=0x908000 if path==0 else 0x20d000
   assert any(bank<=producer['d0']<=bank+0x800 for bank in [base,other])
   wait=next(q for q in selected[n:]if q['pc']==0x932c and q['kind']==1)
   assert wait['d0']==(producer['d0']&0xffff0000)|1 and wait['task']&0xffffff==0xff0060
   assert wait['sp']&0xffffff==0xff040c
   stores=[q for q in selected if q['master']>=wait['master']and q['pc']==0x1754 and q['kind']==1 and q['width']==2 and q['address']in[0x3d0,0x3d2]]
   assert len(stores)==2 and {q['address']:q['value']for q in stores}=={0x3d0:wait['d0']>>16,0x3d2:wait['d0']&65535}
   assert all(q['d0']==wait['d0']and q['sp']&0xffffff==0xff040c for q in stores)
   assert long(before[path],0x3d0)==wait['d0'] and long(before[path],0x40c)==0x932c
   # Earlier source1756 reload events are retained separately; they are not
   # substituted for the fresh3B57A producer that proves these five failures.
   reloads=[q for q in selected if q['pc']==0x175a and q['kind']==0 and q['address']in[0x3d0,0x3d1,0x3d2,0x3d3,0x3d4]]
   lineage.append({'path':path,'mode_read':prior,'first_after_producer':producer,'wait_call':wait,'exact1750_d0_writes':stores,'saved_d0_address':0xff03d0,'saved_d0':wait['d0'],'observed_earlier1756_reload_bus_events':reloads})
  source_pointer=lineage[0]['first_after_producer']['d0'];native_pointer=lineage[1]['first_after_producer']['d0']
  base=0x900000 if source_pointer<0x908000 else 0x908000;translated=0x20c000 if base==0x900000 else 0x20d000
  assert native_pointer==translated+source_pointer-base
  cases.append({'frame':frame,'instructions_to_equal_full_definition':len(instructions[0]),'accesses_equal':len(accesses[0]),'total_master_cycles':sum(int(q['cycles'])for q in instructions[0]),'d0_uses':effects,'live_tasks':tasks,'initial_current_task':long(before[0],0x204),'lineage':lineage,'unchanged_prior_ram_difference_bytes':len(prior_differences),'new_differing_memory_bytes':0})
 # Pin source sites and current native wrapper preservation; no inferred
 # equivalence of arbitrary instruction ranges or source data words.
 guards={0x3b574:'4a2d02a0662a2008',0x9324:'303c00014eb81750',0x1750:'48e7fffe4e434cdf7fff4e75'}
 for at,hexbytes in guards.items():assert original[at:at+len(bytes.fromhex(hexbytes))]==bytes.fromhex(hexbytes) and bytes(rom[at:at+len(bytes.fromhex(hexbytes))])==bytes.fromhex(hexbytes)
 irq=[];pc=0x18d0
 while pc<=0x191a:
  q=decode(pc,False);assert not q['op'].startswith('b')and q['op']not in ['jmp','jsr','rts','rte']
  assert use(q)==('independent_full_definition'if pc==0x191a else use(q))
  if pc!=0x191a:assert use(q)in ['no_d0_use','low_word_or_byte_only']
  irq.append(q);pc+=q['size']
 assert irq[-1]['bytes']=='202d0058'
 paths=[Path(__file__).resolve(),tracer,snap/'evidence.json',snap/'d0-lineage.csv',snap/'oracle.bin',target/'manifest.json']+list(snap.glob('frame-*'))
 report={'scope':__doc__,'closed_five_state_effect_equivalence':True,'continuous_observer_equivalence_changed':False,'automatic_residue_guard_admitted':False,'all_game_equivalence_admitted':False,'cases':cases,'source_opcode_guards':{hex(k):v for k,v in guards.items()},'irq_unconditional_prefix':irq,'limits':'Closed continuations use genuine full serialized states and perform no CPU register writes, injected interrupts or instruction patches. They do not advance VDP scheduling or inject a new physicalVBlank. OriginalIRQ saves/restores incoming D0, and its unconditional18D0 prefix uses only its lowword/byte until191A; IRQ task wakeups can still alter future task selection. Reuse as an online guard requires current task/dispatch, producer/save/restore and IRQ lineage checks, not just these PCs or numeric upperwords. Source1756 reloads are recorded but fresh3B57A is the proven producer of each classified failure. No existing diagnostic is reclassified by this tool.','compile_command':cmd,'inputs':{str(q):sha(q)for q in paths},'outputs':{str(q.relative_to(out)):sha(q)for q in out.iterdir()if q.is_file()}}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');(out/'instructions.txt').write_text('\n'.join(f'{at:06x}: {q["bytes"]:20} {q["assembly"]}'for at,q in sorted(decoded.items()))+'\n');print(json.dumps({'cases':len(cases),'instructions':[q['instructions_to_equal_full_definition']for q in cases],'new_differing_memory_bytes':0,'automatic_guard_admitted':False},indent=2))
if __name__=='__main__':main()
