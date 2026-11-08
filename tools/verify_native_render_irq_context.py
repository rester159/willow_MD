#!/usr/bin/env python3
"""Prove native IRQ context substitution with original full handler and RTE."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--target',type=Path,default=ROOT/'build/native-render-cadence/reuse-v1')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-render-irq-context/v2')
    p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    a=p.parse_args();target,out=a.target.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((target/'manifest.json').read_text());syms=manifest['symbols']
    assert 'renderSourceIRQ' in syms and 'renderSourceContextReady' in syms
    before={}
    for name in ['program.bin','platform.bin','manifest.json','render.disasm','render.sym']:
        if name in manifest['products']:assert sha(target/name)==manifest['products'][name]
        shutil.copy2(target/name,out/name);before[name]=sha(out/name)
    source=(out/'program.bin').read_bytes()
    assert source[0x1554:0x1558].hex()=='48e7fffe' and source[0x15b4:0x15ba].hex()=='4cdf7fff4e73'
    platform=(out/'platform.bin').read_bytes();at=syms['renderSourceIRQ']-0x100010
    assert platform[at:at+4].hex()=='48e7fffe'
    pin=ROOT/'build/native-object-publication/checkpoint-visual-state-v1'
    checkpoint=json.loads((pin/'checkpoint.json').read_text())['files']
    for name in ['visual-contexts.csv','visual-contexts.bin']:
        assert sha(pin/name)==checkpoint[name]
    row=next(r for r in csv.DictReader((pin/'visual-contexts.csv').open()) if r['manager']==r['scene']=='2')
    with (pin/'visual-contexts.bin').open('rb') as f:
        f.seek((int(row['capture'])-1)*90624);raw=f.read(90624)
    ram=raw[8704:74240];assert len(ram)==65536
    sram=bytearray(65536);sram[:16384]=raw[74240:];sram[0xc800:0xd000]=raw[6656:8704]
    sram[0xd800:0xf000]=raw[:6144];sram[0xf800:0xfa00]=raw[6144:6656]
    (out/'source-ram.bin').write_bytes(ram);(out/'source-sram.bin').write_bytes(sram)
    macros={'IRQ_BRIDGE':syms['renderSourceIRQ'],'CONTEXT_READY':syms['renderSourceContextReady'],'PREPARE_SP':syms['renderPrepareSP']}
    (out/'irq-context-fixture.h').write_text(''.join(f'#define {name} 0x{value:x}u\n' for name,value in macros.items()))
    driver=ROOT/'tools/test_native_render_irq_context.c';helpers=ROOT/'tools/test_native_render_cache_closed.c'
    subprocess.run(['cc','-std=c11','-O2','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tools'),'-I'+str(out),str(driver),'-o',str(out/'test-irq')],check=True)
    with (out/'test.log').open('w') as f:
        subprocess.run([str(out/'test-irq'),str(a.core),str(out),str(out/'calls.csv')],stdout=f,stderr=subprocess.STDOUT,check=True)
    assert all(sha(out/name)==digest for name,digest in before.items())
    rows=list(csv.DictReader((out/'calls.csv').open()));assert len(rows)==1536
    result={'scope':__doc__,'pairs':len(rows),'real_irq_handler':'1554 source save or identical bridge save; unchanged1558..15B8 including original RTE',
            'source_capture_seed':row,'bridge_symbols':macros,'frozen_target_files':before,
            'source_program_sha256':sha(ROOT/'build/reference_maincpu.bin'),
            'tools':{str(x):sha(x) for x in [Path(__file__),driver,helpers,a.core]},
            'snapshot_inputs':{name:sha(out/name) for name in ['source-ram.bin','source-sram.bin','irq-context-fixture.h']},
            'result':(out/'test.log').read_text(),
            'limitations':'Closed real TRAP/RTE with source-derived seed and explicit legal branch controls; not actual physical VBlank cadence, arbitrary instruction-boundary interrupt injection through a live C call, full IRQ nesting admission, or FPS. Host writes occur only before calls. Reference runs original handler with logical source registers; candidate starts with distinct native registers when context-ready. Additional return-window mode clears ready and changes only C-caller-clobbered registers/CCR, preserving the source callee-saved context.'}
    (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n');print(result['result'])


if __name__=='__main__':main()
