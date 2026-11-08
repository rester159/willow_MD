#!/usr/bin/env python3
"""Read-only snapshots of both source banks across native prepare/publication."""
from pathlib import Path
import argparse,json,subprocess
import test_native_render_cadence as cadence

ROOT=Path(__file__).resolve().parents[1]
PIN=ROOT/'build/native-render-cadence/checkpoint-content-v2'
HEADER=r'''
static FILE *residencyCSV,*residencyBinary;
static unsigned residencySamples;
static int residencyInit(void){
 residencyCSV=outputFile("object-residency.csv","w");residencyBinary=outputFile("object-residency.bin","wb");
 if(!residencyCSV||!residencyBinary)return 0;
 fputs("sample,frame,master_cycles,phase,manager,scene,bank,records,epoch,bank0_epoch,bank0_count,bank0_valid,bank1_epoch,bank1_count,bank1_valid,capture\n",residencyCSV);
 return 1;
}
static void residencyCapture(unsigned phase){
 if(renderPath)return;
 RenderSpan state=renderDescriptor(phase==4);
 fprintf(residencyCSV,"%u,%u,%" PRIu64 ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
  residencySamples++,frameNumber,stamp(),phase,rawWord(0x8004),rawWord(0x8008),state.bank,state.records,state.epoch,
  publicationBanks[0].epoch,publicationBanks[0].count,publicationBanks[0].valid,
  publicationBanks[1].epoch,publicationBanks[1].count,publicationBanks[1].valid,latchCaptures);
 if(fwrite(cps,1,2048,residencyBinary)!=2048||fwrite(cps+0x8000,1,2048,residencyBinary)!=2048)abort();
}
static void residencyFinish(void){if(fclose(residencyCSV)||fclose(residencyBinary))abort();}
'''

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--output',type=Path,default=ROOT/'build/native-object-residency/observer-v1')
 p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
 a=p.parse_args();out=a.output.resolve();assert not out.exists(),'Use a fresh evidence directory'
 binary,manifest=cadence.build(PIN/'target',out)
 original=ROOT/'tools/test_native_render_cadence_observer.h'
 text=original.read_text()
 text=cadence.replace(text,'#include "test_native_render_cadence_oracle.h"','#include "'+str(ROOT/'tools/test_native_render_cadence_oracle.h')+'"')
 text=cadence.replace(text,'static void renderPhase(unsigned encoded){','static void residencyCapture(unsigned);\nstatic void renderPhase(unsigned encoded){')
 text=cadence.replace(text,' if(phase==1){',' if(phase==1){\n  residencyCapture(1);')
 text=cadence.replace(text,' }else if(phase==2){',' }else if(phase==2){\n  residencyCapture(2);')
 text=cadence.replace(text,'  if(!renderPath&&!status){','  if(!renderPath&&!status){\n   residencyCapture(4);')
 header=out/'residency-observer.h';header.write_text(text)
 generated=out/'test_native_render_cadence.c';text=generated.read_text()
 text=cadence.replace(text,'#include "'+str(original)+'"','#include "'+str(header)+'"\n'+HEADER)
 text=cadence.replace(text,' if(!renderInit(core))return 2;',' if(!renderInit(core))return 2;\n if(!residencyInit())return 2;')
 text=cadence.replace(text,' renderFinish();',' residencyFinish();\n renderFinish();');generated.write_text(text)
 subprocess.run(manifest['command'],cwd=ROOT,check=True)
 manifest['inputs'].update({str(path):cadence.sha(path) for path in [Path(__file__).resolve(),header]})
 manifest.update(generated_sha256=cadence.sha(generated),binary_sha256=cadence.sha(binary),extension_scope=__doc__)
 with(out/'observer.log').open('w')as log:
  result=subprocess.run([str(binary),str(a.core),'1800','--coin-route',str(out)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
 manifest['run']={'exit_code':result.returncode,'frames':1800,'core_sha256':cadence.sha(a.core),'log_sha256':cadence.sha(out/'observer.log')}
 assert all(cadence.sha(Path(name))==digest for name,digest in manifest['inputs'].items())
 (out/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 if result.returncode:raise SystemExit(result.returncode)
 checkpoint=json.loads((PIN/'checkpoint.json').read_text());same={}
 for name,digest in checkpoint['files'].items():
  if not name.startswith('observer/'):continue
  relative=name.removeprefix('observer/')
  if relative in {'observer-manifest.json','test-native-render-cadence','test_native_render_cadence.c'}:continue
  assert cadence.sha(out/relative)==digest,relative
  same[relative]=digest
 evidence={'scope':__doc__,'record_bytes':4096,'regions':[{'name':'object0','offset':0,'size':2048,'source_start':'900000'},{'name':'object1','offset':2048,'size':2048,'source_start':'908000'}],
           'byte_order':'big-endian source address order','phases':{'1':'prepare begin','2':'successful prepare complete','4':'successful publication complete'},
           'target_unchanged_outputs':same,'observer_manifest_sha256':cadence.sha(out/'observer-manifest.json'),
           'outputs':{name:cadence.sha(out/name) for name in ['object-residency.csv','object-residency.bin']}}
 (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
 print('PASS: both source banks captured; all25 deterministic baseline outputs unchanged.')

if __name__=='__main__':main()
