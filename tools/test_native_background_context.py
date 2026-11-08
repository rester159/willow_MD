#!/usr/bin/env python3
"""Capture live source background inputs without changing the render observer.

The reference snapshot is at PRE-source1554 prior-port object acquisition.
Background tile RAM is live: this boundary does not give it object-latch
semantics, nor admit a raster/compositing implementation. Both original and
relocated paths still run the complete unchanged source/native timing gate.
"""
from pathlib import Path
import argparse,csv,hashlib,json,subprocess
import test_native_render_cadence as cadence

ROOT=Path(__file__).resolve().parents[1]
PIN=ROOT/'build/native-render-cadence/checkpoint-content-v2'
REGIONS=[('scroll2',16384,0x918000),('scroll3',16384,0x914000),
         ('scroll1',16384,0x910000),('palettes',6144,0x91c000),
         ('ports',512,0x800000),('row_scroll',2048,0x922000),
         ('work_ram',65536,0xff0000)]
sha=cadence.sha

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--platform',type=Path,default=PIN/'target')
 p.add_argument('--output',type=Path,default=ROOT/'build/native-background-context/content-v2')
 p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
 p.add_argument('--frames',type=int,default=1800)
 a=p.parse_args();assert 1<=a.frames<=36000
 out=a.output.resolve();assert not out.exists(),'Use a fresh evidence directory'
 binary,manifest=cadence.build(a.platform,out)
 generated=out/'test_native_render_cadence.c'
 header=ROOT/'tools/test_native_background_context.h'
 text=generated.read_text()
 text=cadence.replace(text,'static void publicationLatch(void);','static void publicationLatch(void);\nstatic void backgroundContextCapture(void);')
 text=cadence.replace(text,'  publicationLatch();','  publicationLatch();\n  backgroundContextCapture();')
 text=cadence.replace(text,'int main(int argc,char**argv){','#include "'+str(header)+'"\nint main(int argc,char**argv){')
 text=cadence.replace(text,' if(!renderInit(core))return 2;',' if(!renderInit(core))return 2;\n if(!backgroundContextInit())return 2;')
 text=cadence.replace(text,' renderFinish();',' backgroundContextFinish();\n renderFinish();')
 generated.write_text(text)
 subprocess.run(manifest['command'],cwd=ROOT,check=True)
 manifest['inputs'].update({str(path):sha(path) for path in [Path(__file__).resolve(),header]})
 manifest.update(generated_sha256=sha(generated),binary_sha256=sha(binary),extension_scope=__doc__)
 with(out/'observer.log').open('w')as log:
  result=subprocess.run([str(binary),str(a.core),str(a.frames),'--coin-route',str(out)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
 manifest['run']={'exit_code':result.returncode,'frames':a.frames,'core_sha256':sha(a.core),'log_sha256':sha(out/'observer.log')}
 assert all(sha(Path(name))==digest for name,digest in manifest['inputs'].items()),'Observer input changed during run'
 (out/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 if result.returncode:raise SystemExit(result.returncode)
 rows=list(csv.DictReader((out/'source-background-contexts.csv').open()))
 latches=list(csv.DictReader((out/'publication-latches.csv').open()))
 assert len(rows)==len(latches)
 regions=[];offset=0
 for name,size,start in REGIONS:
  regions.append({'name':name,'offset':offset,'size':size,'source_start':f'{start:06X}'})
  offset+=size
 assert (out/'source-background-contexts.bin').stat().st_size==offset*len(rows)
 for sample,(r,b) in enumerate(zip(rows,latches)):
  assert int(r['sample'])==sample and int(r['capture'])==sample+1
  for left,right in [('frame','frame'),('capture','capture'),('master_cycles','master_cycles'),('manager','manager'),('scene','scene'),('bank','selected_bank'),('source_epoch','producer_epoch'),('source_count','count'),('source_valid','valid_complete_epoch')]:
   assert r[left]==b[right],(sample,left,r[left],b[right])
 layout={'schema_version':1,'byte_order':'big-endian source address order','records':len(rows),'record_bytes':offset,
         'index':'source-background-contexts.csv','binary':'source-background-contexts.bin','regions':regions,
         'object_banks':'captured-original-banks.bin','object_bank_bytes':2048,
         'boundary':'Reference PRE-source1554 prior-port object acquisition, after native metadata check and independent original bank copy; exactly the publication-latches.csv master cycle. Source CPS tile RAM remains live, not latched by this capture.',
         'work_ram_note':'All canonical reference WRAM bytes, including declared native-owned renderer/latch/stack regions. Not an assertion that every byte is original source-owned.',
         'candidate_scope':'Complete source RAM, seven CPS video regions, ports and native VDP state are paired by unchanged observer at frame end. No separate same-instruction candidate snapshot claim.'}
 (out/'layout.json').write_text(json.dumps(layout,indent=2)+'\n')
 active=[r for r in rows if r['manager']==r['scene']=='2']
 # The added host observation must not perturb any target-visible result or
 # emulated time. Compare the complete existing deterministic data products.
 same={}
 if a.frames==1800 and a.platform.resolve()==(PIN/'target').resolve():
  checkpoint=json.loads((PIN/'checkpoint.json').read_text())
  for name,digest in checkpoint['files'].items():
   if not name.startswith('observer/'):continue
   relative=name.removeprefix('observer/')
   if relative in {'observer-manifest.json','test-native-render-cadence','test_native_render_cadence.c'}:continue
   path=out/relative
   assert path.is_file() and sha(path)==digest,('Changed baseline output',relative)
   same[relative]=digest
 summary={'scope':__doc__,'frames':a.frames,'captures':len(rows),'active_captures':len(active),
          'first_active_capture':int(active[0]['capture']) if active else None,
          'last_active_capture':int(active[-1]['capture']) if active else None,
          'record_bytes':offset,'baseline_unchanged_outputs':same,
          'inputs':{str(PIN/'checkpoint.json'):sha(PIN/'checkpoint.json'),**manifest['inputs']},
          'outputs':{name:sha(out/name) for name in ['layout.json','source-background-contexts.csv','source-background-contexts.bin','captured-original-banks.bin','observer-manifest.json']}}
 (out/'capture-evidence.json').write_text(json.dumps(summary,indent=2)+'\n')
 print(json.dumps({k:v for k,v in summary.items() if k not in {'inputs','outputs','baseline_unchanged_outputs'}},indent=2))
 print(f'Unchanged baseline products: {len(same)}; evidence: {out / "capture-evidence.json"}')

if __name__=='__main__':main()
