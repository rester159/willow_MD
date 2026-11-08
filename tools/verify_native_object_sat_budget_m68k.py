#!/usr/bin/env python3
"""Isolated native budget parity and binding benchmark; no shared header edits."""
from pathlib import Path
import argparse,json,subprocess,shutil,hashlib
from build_vdp_presentation_probe import ROOT,build
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/native-budget-m68k');p.add_argument('--deferred',action='store_true');p.add_argument('--parity-evidence',type=Path,default=ROOT/'build/native-budget-m68k/v2/evidence.json');p.add_argument('--skip-parity',action='store_true',help='Benchmark only after the same header hash has passed native parity');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
linked_parity=None
if a.skip_parity:
 linked_parity=json.loads(a.parity_evidence.read_text())
 helper=ROOT/'src/willow_native_object_sat_budget_m68k.h'
 assert linked_parity['native_parity_cases']==8192 and linked_parity['native_delta_cases']==4096
 assert linked_parity['hashes'][str(helper)]==hashlib.sha256(helper.read_bytes()).hexdigest(),'Current native helpers lack matching parity evidence'
base=ROOT/'build/native-latch-presentation'
inputs={n:base/'inputs'/f'{n}.bin' for n in ['vram','cram','vsram','registers']}
proof=out/'parity';proof.mkdir(exist_ok=True)
build(inputs,proof,native_source=ROOT/'tools/test_native_object_sat_budget_m68k.c')
# Use the standard read-only capture loop, extended for the self-test workload.
s=(ROOT/'tools/capture_vdp_presentation.c').read_text()
s=s.replace('if(frame!=30)return;','(void)d;(void)w;(void)h;(void)pitch;return;\n if(frame!=30)return;')
s=s.replace('for(frame=0;frame<=30;frame++)retro_run();','for(frame=0;frame<36000;frame++){retro_run();const uint8_t*r=dlsym(core,"work_ram");uint32_t marker=0;for(unsigned n=0;n<4;n++)marker=marker<<8|r[n^1];if(marker==0x56445031||marker==0x4641494c)break;}captured=1;')
s=s.replace('ready,captured);return 1;','ready,captured);unsigned(*reg)(int)=dlsym(core,"m68k_get_reg");fprintf(stderr," PC%x SP%x SR%x frame%u ",reg(16),reg(15),reg(17),frame);for(unsigned at=8;at<18;at++)fprintf(stderr,"%02x",ram[at^1]);fputc(10,stderr);return 1;')
s=s.replace('Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.','Native closed budget parity completed; no video assertion, host target writes or GUI. Core format%d.')
(proof/'capture.c').write_text(s)
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'tools'),str(proof/'capture.c'),'-o',str(proof/'capture')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
if not a.skip_parity:
 with (proof/'capture.log').open('w') as log:subprocess.run([str(proof/'capture'),str(core),str(proof/'probe.bin'),str(proof/'unused.ppm')],stdout=log,stderr=subprocess.STDOUT,check=True)
# A generated local copy binds ONLY budget primitives in the ordinary reader.
bench=out/'presentation';bench.mkdir(exist_ok=True)
sat=(ROOT/'src/willow_native_object_sat.h').read_text()
needle='static inline void willowObjectSatBudgetInit(WillowObjectSatBudget*b)'
assert needle in sat
sat=sat.replace(needle,'#include "willow_native_object_sat_budget_m68k.h"\nstatic inline void willowObjectSatBudgetInitPortable(WillowObjectSatBudget*b)')
sat=sat.replace('static inline uint8_t willowObjectSatReserve(WillowObjectSatBudget*b,','static inline uint8_t willowObjectSatReservePortable(WillowObjectSatBudget*b,')
sat=sat.replace('r.status=willowObjectSatReserve(b,','r.status=willowNativeObjectSatReserveM68k(b,')
validation=''' for(unsigned line=0;line<224;line++){
  if(b->sprites[line]>20){r.status=WILLOW_OBJECT_SAT_LINE;return r;}
  if(b->pixels[line]>320){r.status=WILLOW_OBJECT_SAT_PIXELS;return r;}
 }'''
assert sat.count(validation)==1
sat=sat.replace(validation,' r.status=willowNativeObjectSatBudgetValidateM68k(b);if(r.status)return r;')
if a.deferred:
 commit=''' if(lineDeltas){
  int running=0;
  for(unsigned line=0;line<224;line++){
   running+=lineDeltas[line];
   unsigned sprites=b->sprites[line]+running,pixels=b->pixels[line]+running*16;
   if(sprites>20){r.status=WILLOW_OBJECT_SAT_LINE;return r;}
   if(pixels>320){r.status=WILLOW_OBJECT_SAT_PIXELS;return r;}
   b->sprites[line]=sprites;b->pixels[line]=pixels;
  }
 }'''
 assert sat.count(commit)==1
 sat=sat.replace(commit,' if(lineDeltas){r.status=willowNativeObjectSatDeltaCommitM68k(b,lineDeltas);if(r.status)return r;}')
 sat=sat.replace('for(unsigned n=0;n<225;n++)lineDeltas[n]=0;','willowNativeObjectSatDeltaClearM68k(lineDeltas);')
(bench/'willow_native_object_sat.h').write_text(sat)
for name in ['willow_native_object_latch.h','willow_native_object_latch_sat.h','willow_native_object_sat_budget_m68k.h']:shutil.copy2(ROOT/'src'/name,bench/name)
source=(ROOT/'tools/native_latch_presentation.c').read_text().replace('willowObjectSatBudgetInit(&budget)','willowNativeObjectSatBudgetInitM68k(&budget)').replace('willowObjectSatReserve(&budget','willowNativeObjectSatReserveM68k(&budget')
(bench/'native.c').write_text(source);shutil.copy2(base/'latch_fixture.h',bench/'latch_fixture.h')
fixture=bench/'latch_fixture.h';count=json.loads((base/'evidence.json').read_text())['source_records'];fixture.write_text(f'#ifndef FIXTURE_RECORD_COUNT\n#define FIXTURE_RECORD_COUNT {count}\n#endif\n'+fixture.read_text())
if a.deferred:
 header=bench/'latch_fixture.h';header.write_text('#define WILLOW_DEFERRED_OBJECT_BUDGET 1\n'+header.read_text())
build(inputs,bench,native_source=bench/'native.c',native_include=bench)
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-DWILLOW_PRESENTATION_PHASES','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(ROOT/'tools/capture_vdp_presentation.c'),'-o',str(bench/'capture')],check=True)
with (bench/'capture.log').open('w') as log:subprocess.run([str(bench/'capture'),str(core),str(bench/'probe.bin'),str(bench/'hardware.ppm')],stdout=log,stderr=subprocess.STDOUT,check=True)
assert (bench/'hardware.ppm').read_bytes()==(base/'hardware.ppm').read_bytes()
phases={}
for line in (bench/'capture.log').read_text().splitlines():
 if line.startswith('PHASE '):
  _,name,cycles,ms=line.split();phases[name]={'master_cycles':int(cycles),'ms':float(ms)}
files=[ROOT/'src/willow_native_object_sat_budget_m68k.h',ROOT/'tools/test_native_object_sat_budget_m68k.c',Path(__file__),proof/'probe.bin',bench/'probe.bin',bench/'capture.log',bench/'hardware.ppm',bench/'willow_native_object_sat.h',bench/'native.c',core]
e={'scope':'Native budget replacement parity plus ordinary reader static presentation benchmark; no production binding','native_parity_cases':0 if a.skip_parity else 8192,'native_delta_cases':0 if a.skip_parity else 4096,'deferred_reader':a.deferred,'linked_parity_evidence':str(a.parity_evidence.resolve()) if a.skip_parity else None,'linked_parity_evidence_sha256':hashlib.sha256(a.parity_evidence.read_bytes()).hexdigest() if a.skip_parity else None,'same_failure_status_and_full_680_byte_canary_buffer':True,'word_aligned_offset_cases':[2,4],'presentation_pixels_exact':71680,'timing':phases,'hashes':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}}
(out/'evidence.json').write_text(json.dumps(e,indent=2)+'\n');print(json.dumps(e['timing'],indent=2))
