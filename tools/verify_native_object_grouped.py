#!/usr/bin/env python3
"""Isolated grouped-consumer geometry, ownership and admission checks. No GUI."""
from pathlib import Path
import json,hashlib,subprocess,argparse
ROOT=Path(__file__).resolve().parents[1]
def prepare(out):
 source=ROOT/'build/native-prepared-objects/final1200/prepared-plan.json'
 group=ROOT/'build/native-sprite-groups/frame1200/group-plan.json'
 a=json.loads(source.read_text());b=json.loads(group.read_text())
 assert len(a['templates'])==len(b['templates'])==3
 lines=['#ifndef WILLOW_GROUPED_TEST_FIXTURE_H','#define WILLOW_GROUPED_TEST_FIXTURE_H']
 for n,(original,groups) in enumerate(zip(a['templates'],b['templates'])):
  lines.append(f'static const WillowPreparedObjectCell originalCells{n}[]={{'+','.join('{'+f"{c['offset']},{c['attribute']}"+'}'for c in original['cells'])+'};')
  cells=[];members=[]
  for c in groups:
   assert c['x']%16==c['y']%16==0 and c['width'] in(16,32) and c['height'] in(16,32) and not c['attribute']&0x1800
   size=c['size_word']>>8;assert size in(5,7,13,15)
   cells.append('{'+f"{(c['y']<<8)|c['x']|size},{c['attribute']}"+'}')
   members.append(str(c['mask'])+'u')
  lines.append(f'static const WillowGroupedObjectCell groupedCells{n}[]={{'+','.join(cells)+'};')
  lines.append(f'static const uint32_t memberMask{n}[]={{'+','.join(members)+'};')
 lines+=['static const WillowPreparedObjectTemplate originalTemplates[]={'+','.join('{'+f"originalCells{n},{len(t['cells'])},{t['base']},{t['attribute']}"+'}'for n,t in enumerate(a['templates']))+'};',
 'static const WillowGroupedObjectTemplate groupedTemplates[]={'+','.join('{'+f"groupedCells{n},{len(b['templates'][n])},{t['base']},{t['attribute']}"+'}'for n,t in enumerate(a['templates']))+'};',
 'static const uint32_t*const memberMasks[]={memberMask0,memberMask1,memberMask2};',
 'static const uint16_t fixtureOrigins[3][2]={'+','.join('{'+f"{i['source_x']},{i['source_y']}"+'}'for i in a['instances'])+'};','#endif']
 (out/'grouped_fixture.h').write_text(('\n'.join(lines)+'\n').replace('static const ','static const __attribute__((unused)) '))
 return [source,group,ROOT/'build/cartridge-gap-audit/frame1200_window_v1/vram.bin',ROOT/'build/native-sprite-groups/frame1200/vram.bin']
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--native',action='store_true');ap.add_argument('--native-commit',action='store_true');ap.add_argument('--native-cells',action='store_true');ap.add_argument('--output',type=Path,default=ROOT/'build/native-grouped-objects');a=ap.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 inputs=prepare(out);test=ROOT/'tools/test_native_object_grouped.c'
 inputs.extend(ROOT/'src'/name for name in ['willow_native_object_prepared.h','willow_native_object_sat.h','willow_native_object_sat_budget_m68k.h'])
 input_hashes={f:hashlib.sha256(f.read_bytes()).hexdigest()for f in inputs}
 subprocess.run(['clang']+(['-DWILLOW_GROUPED_NATIVE_CELLS=1']if a.native_cells else [])+['-Wall','-Wextra','-Werror','-O2','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'src'),'-I',str(out),str(test),'-o',str(out/'host-test')],check=True)
 with(out/'host.log').open('w')as log:subprocess.run([str(out/'host-test'),str(inputs[2]),str(inputs[3])],cwd=out,stdout=log,stderr=subprocess.STDOUT,check=True)
 native_files=[]
 if a.native:
  from build_vdp_presentation_probe import build
  target=out/'target';target.mkdir(exist_ok=True)
  scene=ROOT/'build/native-sprite-groups/frame1200'
  target_source=test
  if a.native_commit or a.native_cells:
   target_source=target/'test_native.c';target_source.write_text(('#define WILLOW_GROUPED_NATIVE_COMMIT 1\n'if a.native_commit else '')+('#define WILLOW_GROUPED_NATIVE_CELLS 1\n'if a.native_cells else '')+'#include '+json.dumps(str(test))+'\n')
  build({key:scene/(key+'.bin')for key in ['vram','cram','vsram','registers']},target,native_source=target_source,native_include=out)
  capture=(ROOT/'tools/capture_vdp_presentation.c').read_text()
  capture=capture.replace('if(frame!=30)return;','(void)d;(void)w;(void)h;(void)pitch;return;\n if(frame!=30)return;')
  capture=capture.replace('for(frame=0;frame<=30;frame++)retro_run();','for(frame=0;frame<120000;frame++){retro_run();const uint8_t*r=dlsym(core,"work_ram");uint32_t marker=0;for(unsigned n=0;n<4;n++)marker=marker<<8|r[n^1];if(marker==0x56445031||marker==0x4641494c)break;}captured=1;')
  capture=capture.replace('ready,captured);return 1;','ready,captured);unsigned(*reg)(int)=dlsym(core,"m68k_get_reg");fprintf(stderr," PC%x SP%x SR%x frame%u ",reg(16),reg(15),reg(17),frame);for(unsigned at=8;at<14;at++)fprintf(stderr,"%02x",ram[at^1]);fputc(10,stderr);return 1;')
  capture=capture.replace(' printf("Native VDP buffers published;', ' uint32_t cases=0;for(unsigned n=8;n<12;n++)cases=cases<<8|ram[n^1];if(cases!=8819||ram[12^1]||ram[13^1])return 1;\n printf("Native VDP buffers published;')
  capture=capture.replace('Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.','Native grouped consumer passed8819 closed geometry/budget cases plus epoch/metadata checks; no video assertion, host target writes or GUI; format%d.')
  (target/'capture.c').write_text(capture)
  subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'tools'),str(target/'capture.c'),'-o',str(target/'capture')],check=True)
  core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
  with(target/'capture.log').open('w')as log:subprocess.run([str(target/'capture'),str(core),str(target/'probe.bin'),str(target/'unused.ppm')],stdout=log,stderr=subprocess.STDOUT,check=True)
  native_files=[target_source,target/'probe.bin',target/'capture.c',target/'capture.log',target/'manifest.json',core]
 assert all(hashlib.sha256(f.read_bytes()).hexdigest()==h for f,h in input_hashes.items()),'Source dependency changed during validation'
 evidence=json.loads((out/'host-result.json').read_text());evidence['native_68000_closed_cases']=8819 if a.native else 0;evidence['native_commit']=a.native_commit;evidence['native_cells']=a.native_cells;evidence['hashes']={str(f):hashlib.sha256(f.read_bytes()).hexdigest()for f in native_files+inputs+[ROOT/'src/willow_native_object_grouped.h',test,Path(__file__),out/'grouped_fixture.h']}
 (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(evidence,indent=2))
if __name__=='__main__':main()
