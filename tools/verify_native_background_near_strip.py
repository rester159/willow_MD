#!/usr/bin/env python3
"""Closed native near-column capture/resolve comparison; no VDP or live claim."""
from pathlib import Path
import argparse,csv,hashlib,json,re,shutil,statistics,subprocess
import prepare_native_background_near_columns
from prepare_native_background_near_columns import build as prepare_columns
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def embedded_header(source):
 prefix='/* Generated verbatim from tools/native_background_near_prepared.S. */\n#ifndef WILLOW_NATIVE_BACKGROUND_NEAR_PREPARED_M68K_H\n#define WILLOW_NATIVE_BACKGROUND_NEAR_PREPARED_M68K_H\nextern uint16_t willowNativeNearCapturePreparedM68k(WillowNativeNearCaptureArgs*);\nextern uint16_t willowNativeNearEnterPreparedM68k(WillowNativeNearEnterArgs*);\n__asm__(\n'
 return prefix+'\n'.join(json.dumps(line+'\n')for line in source.splitlines())+'\n);\n#endif\n'
def inputs(target,catalog):
 seeds=[Path(__file__),Path(prepare_native_background_near_columns.__file__),ROOT/'tools/native_background_near_prepared.S',ROOT/'tools/native_background_near_strip.c',ROOT/'tools/test_native_background_near_strip.c']
 result=set(seeds)
 pending=list(seeds)
 while pending:
  p=pending.pop()
  for name in re.findall(r'^#include\s+"([^"]+)"',p.read_text(),re.M):
   candidates=[p.parent/name,ROOT/'src'/name,target/name]
   found=next((q.resolve()for q in candidates if q.is_file()),None)
   if found and found not in result:result.add(found);pending.append(found)
 for name in ['catalog.bin','catalog_imports.h','layout.json','manifest.json']:result.add(catalog/name)
 result.add(target/'background-resources.bin')
 return sorted(result)
def build(out,target,catalog):
 assert (ROOT/'src/willow_native_background_near_prepared_m68k.h').read_text()==embedded_header((ROOT/'tools/native_background_near_prepared.S').read_text()),'Generated assembly header is stale'

 out.mkdir(parents=True,exist_ok=True)
 prepare_columns(catalog,out)
 assert (target/'background-resources.bin').read_bytes()[0x10000:]==(catalog/'catalog.bin').read_bytes()
 defines={'stripSurface':0xff4000,'stripKeys':0xff5100,'stripRing':0xff5300,'stripEpoch':0xff5310,'stripLease':0xff5314,'stripColumns':0xff5330,'stripCounts':0xff5340}
 subprocess.run(['m68k-elf-gcc','-m68000','-std=c99','-O2','-fomit-frame-pointer','-DWILLOW_NATIVE_NEAR_STRIP_PREPARED','-ffreestanding','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),'-I'+str(target),'-I'+str(out),'-c',str(ROOT/'tools/native_background_near_strip.c'),'-o',str(out/'strip.o')],check=True)
 subprocess.run(['m68k-elf-ld','-Ttext=0x150000','-e','stripCaptureFast',*[f'--defsym={k}={v:#x}'for k,v in defines.items()],str(out/'strip.o'),'-o',str(out/'strip.elf')],check=True)
 subprocess.run(['m68k-elf-objcopy','-O','binary',str(out/'strip.elf'),str(out/'strip.bin')],check=True)
 symbols=subprocess.check_output(['m68k-elf-nm','-n',str(out/'strip.elf')],text=True);(out/'strip.sym').write_text(symbols)
 sy={line.split()[2]:int(line.split()[0],16)for line in symbols.splitlines()if len(line.split())==3}
 (out/'strip-fixture.h').write_text('\n'.join(f'#define {k.upper()} {v:#x}u'for k,v in sy.items())+'\n')
 with(out/'strip.disasm').open('w')as f:subprocess.run(['m68k-elf-objdump','-d',str(out/'strip.elf')],stdout=f,check=True)
 subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(out),'-I'+str(ROOT/'src'),'-I'+str(target),str(ROOT/'tools/test_native_background_near_strip.c'),'-o',str(out/'test')],check=True)
 return target

def summary(out,core,before,target,catalog):
 rows=list(csv.DictReader((out/'calls.csv').open()))
 assert len(rows)==1536
 metrics={}
 for count_offset in (0,1):
  for height in (28,29):
   subset=[r for r in rows if int(r['count_offset'])==count_offset and int(r['rows'])==height]
   stats={}
   for key in ['capture_original','capture_fast','enter_original','enter_fast']:
    values=[int(r[key])for r in subset]
    stats[key]={'mean_master_ticks':statistics.mean(values),'min_master_ticks':min(values),'max_master_ticks':max(values),'mean_ms':statistics.mean(values)/53693.175}
   combined_before=stats['capture_original']['mean_ms']+stats['enter_original']['mean_ms']
   combined_after=stats['capture_fast']['mean_ms']+stats['enter_fast']['mean_ms']
   metrics[f'rows{height}_count_offset{count_offset}']={'cases':len(subset),'calls':stats,'combined_before_ms':combined_before,'combined_after_ms':combined_after,'reduction_percent':(1-combined_after/combined_before)*100}
 log=(out/'test.log').read_text()
 assert 'HOST PASS 8012' in log and 'NATIVE PASS 1536' in log and 'COUNT PASS 3456' in log and 'REFUSAL PASS 11' in log
 sourceHashes={str(p):sha(p)for p in before}
 assert sourceHashes=={str(p):v for p,v in before.items()},'An input changed during the build/test'
 evidence={'schema_version':1,'status':'pass','scope':'Closed native68000 near-column helper; no source execution, VDP output, live integration or FPS admission.',
 'host_sanitizers':['address','undefined'],'host_cases':8012,'native_parity_cases':1536,'native_parity_calls_per_case':4,
 'native_count_boundary_cases':3456,'native_count_accepted':294,'native_count_refused':3162,'native_refusal_scenarios':11,
 'native_count_bounds':[0,1,42,43,44,255],'native_count_44_to43':'accepted when the exact portable delta permits it',
 'fixed_defect':'Reject incoming packed count bytes above44 before arithmetic; corrupt255 must not wrap/carry into its paired byte.',
 'guards':['exact C callee-saved registers and stack return','all unowned WRAM bytes unchanged','native VDP write16 aborts','invalid Capture leaves output mask untouched','all accepted words/counts/dirty envelopes/column queues exact','refused Enter may leave discarded partial native RAM, never publication'],
 'rom_resource':json.loads((out/'manifest.json').read_text()),'persistent_ram_added':0,
 'clock':{'master_hz':53693175,'source':'Genesis Plus GX NTSC master tick convention','timing':'single-instruction native m68k_run calls; closed C-call total including wrapper and ABI; not live render budget'},
 'timing':metrics,'inputs':sourceHashes,'core':{'path':str(core),'sha256':sha(core)},
 'toolchain':{name:subprocess.check_output([name,'--version'],text=True).splitlines()[0]for name in ['clang','m68k-elf-gcc','m68k-elf-ld']},
 'outputs':{name:sha(out/name)for name in ['calls.csv','test.log','strip.bin','strip.elf','strip.disasm','strip.sym','strip-fixture.h','closed.bin']}}
 (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
 frozen=out/'frozen-inputs'
 for p in before:
  destination=frozen/p.relative_to(ROOT)
  destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,destination)
 (out/'rerun_frozen.py').write_text(f"""#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys
base=Path(__file__).resolve().parent
root=base/'frozen-inputs'
subprocess.run([sys.executable,str(root/'tools/verify_native_background_near_strip.py'),'--target',str(root/{str(target.relative_to(ROOT))!r}),'--catalog',str(root/{str(catalog.relative_to(ROOT))!r}),'--output',str(base/'rerun')],check=True)
""")
 return evidence

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--output',type=Path,default=ROOT/'build/native-background-near-strip/current')
 p.add_argument('--target',type=Path,default=ROOT/'build/native-render-cadence/background-live-v6')
 p.add_argument('--catalog',type=Path,default=ROOT/'build/native-background-catalog/prepared-v1')
 a=p.parse_args();out=a.output.resolve();target=a.target.resolve();catalog=a.catalog.resolve()
 before={q:sha(q)for q in inputs(target,catalog)}
 build(out,target,catalog)
 core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
 with(out/'calls.csv').open('w')as f,(out/'test.log').open('w')as log:subprocess.run([str(out/'test'),str(core),str(out),str(target)],cwd=ROOT,stdout=f,stderr=log,check=True)
 result=summary(out,core,before,target,catalog)
 print((out/'test.log').read_text())
 print(json.dumps({'evidence':str(out/'evidence.json'),'timing':result['timing']},indent=2))
if __name__=='__main__':main()
