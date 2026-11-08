#!/usr/bin/env python3
"""Closed native68000 original6F2 versus post-clear initializer context proof."""
import argparse,csv,hashlib,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--target',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib');a=p.parse_args();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True);target=a.target.resolve();m=json.loads((target/'manifest.json').read_text());s=m['symbols'];cfg=m['background_far_stream'];offsets=cfg['native_layout']['offsets'];assert offsets['phase']==400 and offsets['residencyEpoch']==402 and offsets['missing']==432 and offsets['reserved']==434
 pins={str(target/'manifest.json'):sha(target/'manifest.json'),str(a.core):sha(a.core)}
 for name in ['manifest.json','program.bin','platform.bin','background-resources.bin','render-far-stream.bin','render-original-far-stream.bin','render.elf-native_render_cadence.su']:
  q=target/name;shutil.copy2(q,out/name);pins[str(q)]=sha(q)
 assert (out/'render-far-stream.bin').read_bytes()==(out/'render-original-far-stream.bin').read_bytes()
 original=ROOT/'build/reference_maincpu.bin';pins[str(original)]=sha(original);clear=original.read_bytes()[0x6f2:0x708];assert (out/'program.bin').read_bytes()[0x6f2:0x708]==clear
 blob=ROOT/'build/native-background-far-stream/prepared-v1/layout.json';layout=json.loads(blob.read_text());pool=next(r['address'] for r in layout['regions'] if r['name']=='pool');pins[str(blob)]=sha(blob)
 defines={'WRAPPER':s['renderFarAfterSourceClear'],'PREPARE_SP':s['renderPrepareSP'],'CACHE':s['backgroundFarCache'],'MISSING_REF':s['backgroundFarMissingRef'],'BSS_BEGIN':s['__render_bss_begin'],'BSS_END':s['__render_bss_end'],'POOL':pool}
 (out/'far-boot-fixture.h').write_text(''.join(f'#define {k} 0x{v:x}u\n' for k,v in defines.items()))
 driver=ROOT/'tools/test_native_far_boot_context.c';helper=ROOT/'tools/test_native_render_cache_closed.c'
 for q in [Path(__file__).resolve(),driver,helper,ROOT/'tools/native_render_cadence.S',ROOT/'tools/native_render_cadence.c']:pins[str(q)]=sha(q);shutil.copy2(q,out/q.name)
 cmd=['cc','-O2','-std=c11','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tools'),'-I'+str(out),str(driver),'-o',str(out/'test')];subprocess.run(cmd,check=True)
 with (out/'test.log').open('w') as f:subprocess.run([str(out/'test'),str(a.core),str(out),str(out/'calls.csv')],stdout=f,stderr=subprocess.STDOUT,check=True)
 assert all(sha(Path(q))==h for q,h in pins.items());rows=list(csv.DictReader((out/'calls.csv').open()));assert len(rows)==192
 evidence={'scope':__doc__,'source_clear_bytes':clear.hex(),'target_defines':defines,'compile':cmd,'pairs':96,'direct_ccr_cases':96,'max_private_stack_bytes':max(int(r['private_stack_bytes']) for r in rows),'native_initialization_master_cycles':sorted(set(int(r['native_master'])-int(r['source_master']) for r in rows if r['direct']=='0')),'result':(out/'test.log').read_text(),'exclusions':'Only declared FF4000..6FFF native BSS, FF7000..75FF private stack and20F590..F593 diagnostic phase; every postclear write checked against these ranges. All source registers/SP/SR/USP compared exactly. Both bus source-clear and auxiliary payloads are byte-identical; only this closed entry executes.','limitations':'Diagnostic host setup before calls only. Supervisor mask7 boot contract, no user-mode wrapper admission, IRQ cadence, hardware timing, fullstartup or FPS claim.','inputs':pins};(out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(evidence['result'])
if __name__=='__main__':main()
