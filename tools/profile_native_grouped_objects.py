#!/usr/bin/env python3
"""Measure grouped construction phases or the parent's full scene carrier."""
from pathlib import Path
import argparse,json,hashlib,subprocess
from build_vdp_presentation_probe import ROOT,build
from verify_native_object_grouped import prepare

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--native-commit',action='store_true');p.add_argument('--native-cells',action='store_true');p.add_argument('--carrier',action='store_true');p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib';scene=ROOT/'build/native-sprite-groups/frame1200'
 define=('#define WILLOW_GROUPED_NATIVE_COMMIT 1\n'if a.native_commit else '')+('#define WILLOW_GROUPED_NATIVE_CELLS 1\n'if a.native_cells else '')
 if a.carrier:
  import verify_native_grouped_scene as carrier
  original_build=carrier.build
  wrapper=out/'grouped_scene_variant.c';wrapper.write_text(define+'#include '+json.dumps(str(ROOT/'tools/native_grouped_scene.c'))+'\n')
  def selected_build(inputs,destination,**kwargs):
   kwargs['native_source']=wrapper
   return original_build(inputs,destination,**kwargs)
  carrier.build=selected_build
  report=carrier.verify(scene,out,core)
  report['native_scanline_commit']=a.native_commit
  report['native_inside_cell_loop']=a.native_cells
  report['native_commit']=a.native_commit;report['native_cells']=a.native_cells;report['variant_wrapper']=str(wrapper);report['hashes'][str(wrapper)]=digest(wrapper)
  report['hashes'][str(Path(__file__).resolve())]=digest(Path(__file__))
  (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['timing'],indent=2));return
 prepare(out)
 source=out/'profile.c';source.write_text(define+'''#include <stdint.h>
#include <stddef.h>
#define WILLOW_GROUPED_OBJECT_PROFILE(n) (*(volatile uint16_t*)0xff0004=(n))
#include "willow_native_object_grouped.h"
#include "willow_native_object_prepared.h"
#include "grouped_fixture.h"
void*memcpy(void*to,const void*from,size_t n){uint8_t*d=to;const uint8_t*s=from;while(n--)*d++=*s++;return to;}
static WillowGroupedObjectInstance instance[3];
static WillowGroupedObjectFrame frame;
static WillowGroupedObjectBinding binding;
static WillowGroupedObjectSeed seed;
static WillowObjectSatBudget budget;
static uint16_t sat[320];
static uint8_t ds[225],du[225];
uint16_t nativeObjectPresentation(void){
 frame.instances=instance;frame.count=3;frame.sourceRecords=256;
 frame.epoch=(WillowGroupedObjectEpoch){1,2,3,4};frame.view=(WillowObjectSatView){96,16,320,224,0};
 for(uint16_t n=0;n<3;n++)instance[n]=(WillowGroupedObjectInstance){groupedTemplates+n,fixtureOrigins[n][0],fixtureOrigins[n][1],n};
 if(willowGroupedObjectBind(&binding,&frame)||!willowGroupedObjectSeedEmpty(&seed,1))return 0;
 WillowObjectSatResult r=willowGroupedObjectBuildSeeded(&binding,&frame.epoch,&seed,1,&budget,sat,80,ds,du);
 if(r.status||r.pieces!=13||budget.count!=13||r.records!=256)return 0;
 *(volatile uint16_t*)0xff0004=6;return 1;
}
''')
 build({key:scene/(key+'.bin')for key in ['vram','cram','vsram','registers']},out,native_source=source)
 capture=(ROOT/'tools/capture_vdp_presentation.c').read_text()
 capture=capture.replace('if(frame!=30)return;','(void)d;(void)w;(void)h;(void)pitch;return;\n if(frame!=30)return;')
 capture=capture.replace('for(frame=0;frame<=30;frame++)retro_run();','for(frame=0;frame<=30;frame++)retro_run();captured=1;')
 capture=capture.replace('"text_budget","raw_capture","object_build_and_sat","diagnostic_assertions","static_vdp_upload"','"seed_budget","endpoint_clear","grouped_geometry","line_commit","post_commit"')
 capture=capture.replace('Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.','Native grouped phase benchmark completed; no image assertion, host target writes or GUI; format%d.')
 (out/'capture.c').write_text(capture)
 subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-DWILLOW_PRESENTATION_PHASES','-I',str(ROOT/'tools'),str(out/'capture.c'),'-o',str(out/'capture')],check=True)
 with(out/'capture.log').open('w')as log:subprocess.run([str(out/'capture'),str(core),str(out/'probe.bin'),str(out/'unused.ppm')],stdout=log,stderr=subprocess.STDOUT,check=True)
 phases={}
 for line in(out/'capture.log').read_text().splitlines():
  if line.startswith('PHASE '):
   _,name,ticks,ms=line.split();phases[name]={'master_ticks':int(ticks),'ms':float(ms)}
 assert len(phases)==5
 files=[Path(__file__),source,out/'capture.c',out/'probe.bin',out/'capture.log',out/'grouped_fixture.h',ROOT/'src/willow_native_object_grouped.h',core]
 report={'scope':'Closed frame1200 native grouped construction with resident resources. Seed initialization, endpoint clear, geometry and line commit separately timed; no latch, Window, SAT upload, gameplay or FPS claim. Marker-store overhead included consistently.','native_commit':a.native_commit,'native_cells':a.native_cells,'timing':phases,'total_ms':sum(p['ms']for p in phases.values()),'hashes':{str(f):digest(f)for f in files}}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
