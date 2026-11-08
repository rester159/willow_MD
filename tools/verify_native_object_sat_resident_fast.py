#!/usr/bin/env python3
"""Resident lookup parity and target cost on frozen deferred budget fixture."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess
from build_vdp_presentation_probe import ROOT,build
from native_object_sat_resident_index import emit_resident_header
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/native-resident-fast');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
base=ROOT/'build/native-budget-m68k/checkpoint-v2/presentation'
text=(base/'latch_fixture.h').read_text();match=re.search(r'fixtureAssets\[\]=\{(.*?)\};',text,re.S);assert match
triples=[tuple(map(int,m)) for m in re.findall(r'\{(\d+),(\d+),(\d+)\}',match.group(1))]
assets={key:(attribute,status) for key,attribute,status in triples};assert len(assets)==len(triples)
header,index=emit_resident_header(assets,77)
header+=f'#define RESIDENT_TEST_ENTRIES {len(assets)}\nstatic const struct {{uint32_t key;uint16_t attribute,status;}} fixtureOriginal[]={{\n'
header+=''.join(f'{{{key},{attribute},{status}}},\n' for key,(attribute,status) in sorted(assets.items()))+'};\n'
synthetic={}
for code,groups in [(0,range(32)),(65535,range(32)),(0x7ab,[0,5,31]),(0x280f,[1,30])]:
 for group in groups:
  status=2 if group%3==0 else 1
  attribute=65535 if status==2 else ((group&3)<<13)|(group*4)
  synthetic[(code<<5)|group]=(attribute,status)
header+=emit_resident_header(synthetic,77,'syntheticResident')[0]
header+=f'#define SYNTHETIC_TEST_ENTRIES {len(synthetic)}\nstatic const struct {{uint32_t key;uint16_t attribute,status;}} syntheticAssets[]={{\n'
header+=''.join(f'{{{key},{attribute},{status}}},\n' for key,(attribute,status) in sorted(synthetic.items()))+'};\n'
(out/'resident_fixture.h').write_text(header);(out/'index.json').write_text(json.dumps(index,indent=2)+'\n')
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'src'),'-I',str(out),str(ROOT/'tools/test_native_object_sat_resident_fast.c'),'-o',str(out/'test-host')],check=True)
with (out/'host.log').open('w') as log:subprocess.run([str(out/'test-host')],stdout=log,stderr=subprocess.STDOUT,check=True)
inputs={n:base/f'{n}.bin' for n in ['vram','cram','vsram','registers']}
proof=out/'parity';proof.mkdir(exist_ok=True);shutil.copy2(out/'resident_fixture.h',proof/'resident_fixture.h')
build(inputs,proof,native_source=ROOT/'tools/test_native_object_sat_resident_fast.c',native_include=proof)
# Read-only native self-test harness from the pinned budget proof.
shutil.copy2(ROOT/'build/native-budget-m68k/checkpoint-v2/parity/capture.c',proof/'capture.c')
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'tools'),str(proof/'capture.c'),'-o',str(proof/'capture')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
with (proof/'capture.log').open('w') as log:subprocess.run([str(proof/'capture'),str(core),str(proof/'probe.bin'),str(proof/'unused.ppm')],stdout=log,stderr=subprocess.STDOUT,check=True)
bench=out/'presentation';bench.mkdir(exist_ok=True)
for name in ['willow_native_object_sat.h','willow_native_object_latch.h','willow_native_object_latch_sat.h','willow_native_object_sat_budget_m68k.h','latch_fixture.h']:shutil.copy2(base/name,bench/name)
shutil.copy2(ROOT/'src/willow_native_object_sat_resident_fast.h',bench/'willow_native_object_sat_resident_fast.h')
(bench/'resident_fixture.h').write_text(emit_resident_header(assets,77)[0])
source=(base/'native.c').read_text();start=source.index('static uint8_t residentAsset(');end=source.index('\nuint16_t nativeObjectPresentation',start)
source=source[:start]+'''#include "resident_fixture.h"
static uint8_t residentAsset(void*context,uint16_t code,uint16_t source,int16_t x,int16_t y,uint16_t*attribute)
{
 (void)context;(void)x;(void)y;
 return willowNativeObjectSatResidentLookup(&fixtureResidentIndex,77,code,source,attribute);
}
'''+source[end:]
(bench/'native.c').write_text(source)
build(inputs,bench,native_source=bench/'native.c',native_include=bench)
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-DWILLOW_PRESENTATION_PHASES','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(ROOT/'tools/capture_vdp_presentation.c'),'-o',str(bench/'capture')],check=True)
with (bench/'capture.log').open('w') as log:subprocess.run([str(bench/'capture'),str(core),str(bench/'probe.bin'),str(bench/'hardware.ppm')],stdout=log,stderr=subprocess.STDOUT,check=True)
assert (bench/'hardware.ppm').read_bytes()==(base/'hardware.ppm').read_bytes()
phases={}
for line in (bench/'capture.log').read_text().splitlines():
 if line.startswith('PHASE '):
  _,name,cycles,ms=line.split();phases[name]={'master_cycles':int(cycles),'ms':float(ms)}
files=[ROOT/'src/willow_native_object_sat_resident_fast.h',ROOT/'tools/native_object_sat_resident_index.py',ROOT/'tools/test_native_object_sat_resident_fast.c',Path(__file__),out/'resident_fixture.h',out/'host.log',proof/'probe.bin',proof/'capture.log',bench/'native.c',bench/'probe.bin',bench/'capture.log',bench/'hardware.ppm',core]
e={'scope':'Optional immutable resident lookup; frozen deferred budget/static frame1020, no runtime cache admission','host_code_group_pairs':4194304,'native_cases':16384+(len(assets)+len(synthetic))*32*4,'synthetic_entries':len(synthetic),'entries':len(assets),'page_count':index['page_count'],'page_index_bytes':2*(len(index['page_directory'])+len(index['code_offsets'])),'entry_bytes':4*len(assets),'epoch':77,'presentation_pixels_exact':71680,'sat_words_exact':152,'timing':phases,'baseline_object_ms':14.976167828,'hashes':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}}
(out/'evidence.json').write_text(json.dumps(e,indent=2)+'\n');print(json.dumps(e,indent=2))
