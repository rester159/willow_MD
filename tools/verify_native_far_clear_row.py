#!/usr/bin/env python3
"""Actual68000 closed proof of the exact extracted far-row clear helper."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
from verify_native_far_finish_model import extract

ROOT = Path(__file__).resolve().parents[1]


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/native-far-finish-audit/clear-native-v1')
    args = parser.parse_args()
    out = args.output.resolve()
    assert not out.exists(), 'Preserve evidence; use a fresh output'
    out.mkdir(parents=True)
    paths = [ROOT/p for p in ['tools/native_render_background.inc', 'src/willow_native_far_cache.h',
        'src/willow_native_plane_surface.h', 'src/willow_native_presentation_plan.h',
        'tools/test_native_far_clear_row.c', 'tools/test_native_render_cache_closed.c',
        'tools/vendor/libretro.h', 'tools/verify_native_far_finish_model.py', 'tools/verify_native_far_clear_row.py']]
    core = Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
    inputs = {str(p): sha(p) for p in paths+[core]}
    for p in paths:
        dest=out/'inputs'/p.relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,dest)
    source=(out/'inputs/tools/native_render_background.inc').read_text()
    (out/'helper.inc').write_text(''.join(extract(source,n) for n in ['bgFarProtectWord','bgSet','bgFarClearRow']))
    fixture = '''#include <stdint.h>
#include <stddef.h>
#include "willow_native_far_cache.h"
#include "willow_native_plane_surface.h"
#define BG_INVALID 65535U
#define BG_FAR_CODE
#define RENDER_BACKGROUND_FAR_STREAM 1
typedef struct {WillowNativePlaneSurface planes[2];uint32_t columns[2][2];} Fixture;
#define bg (*(Fixture*)0xff4000)
#define backgroundFarCache (*(WillowNativeFarCache*)0x20f000)
#define IO ((volatile uint16_t*)0x20f800)
#include "helper.inc"
const uint32_t clearABI[]={sizeof(Fixture),sizeof(WillowNativePlaneSurface),
 (uint32_t)&bg.planes[1].words[0],(uint32_t)&bg.planes[1].dirty,
 (uint32_t)&bg.planes[1].first[0],(uint32_t)&bg.planes[1].last[0],
 (uint32_t)&bg.columns[1][0]};
void nativeFarClearProbe(void){bgFarClearRow(IO[0],IO[1],IO[2]);}
void nativeFarClearBaseline(void){
 uint16_t x=IO[0],y=IO[1],count=IO[2];x&=63;
 for(uint16_t n=0;n<count;n++,x=(x+1)&63)
  if(bg.planes[1].words[(y&31)*64+x])bgSet(1,x,y,0);
}
'''
    (out/'fixture.c').write_text(fixture)
    commands=[]
    def run(cmd,log=None):
        commands.append(cmd)
        if log:
            with (out/log).open('w') as stream:subprocess.run(cmd,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,check=True)
        else:subprocess.run(cmd,cwd=ROOT,check=True)
    run(['m68k-elf-gcc','-m68000','-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function',
         '-ffreestanding','-fno-builtin','-nostdlib','-I'+str(out/'inputs/src'),'-I'+str(out),
         '-Wl,-Ttext=0x102000,-e,nativeFarClearProbe',str(out/'fixture.c'),'-lgcc','-o',str(out/'fixture.elf')])
    run(['m68k-elf-objcopy','-O','binary',str(out/'fixture.elf'),str(out/'fixture.bin')])
    symbols=subprocess.check_output(['m68k-elf-nm','-n',str(out/'fixture.elf')],text=True);(out/'fixture.sym').write_text(symbols)
    def address(name):return int(re.search(r'^([0-9a-f]+) [A-Za-z] '+name+'$',symbols,re.M)[1],16)
    binary=(out/'fixture.bin').read_bytes()
    abi=struct.unpack_from('>7I',binary,address('clearABI')-0x102000)
    names=['STATE_BYTES','SURFACE_BYTES','WORDS','DIRTY','FIRST','LAST','COLUMNS']
    header=''.join(f'#define CLEAR_{name} {value:#x}u\n' for name,value in zip(names,abi))
    header+=f'#define CLEAR_ENTRY {address("nativeFarClearProbe"):#x}u\n#define BASELINE_ENTRY {address("nativeFarClearBaseline"):#x}u\n'
    (out/'clear-fixture.h').write_text(header)
    run(['m68k-elf-objdump','-d',str(out/'fixture.elf')],'fixture.disassembly')
    assert 'dbne' in (out/'fixture.disassembly').read_text()
    run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
         '-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(out),
         str(out/'inputs/tools/test_native_far_clear_row.c'),'-o',str(out/'test')])
    run([str(out/'test'),str(core),str(out/'fixture.bin'),str(out/'timings.csv')],'result.json')
    result=json.loads((out/'result.json').read_text())
    evidence={'scope':__doc__,'status':'pass','runtime_admitted':False,'actual_68000_executed':True,
       'result':result,'compiler_derived_abi':dict(zip(names,abi)), 'inputs':inputs,'commands':commands,
       'live_inputs_still_match':{str(p):sha(p)==inputs[str(p)] for p in paths},
       'limitations':['Closed calls; no live source tasks, IRQ preemption, publication or frame-rate admission.',
                      'State uses actual target surface type and exact extracted helper/Set/protection; surrounding bg layout is a closed fixture.'],
       'outputs':{str(p.relative_to(out)):sha(p) for p in out.rglob('*') if p.is_file()}}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
