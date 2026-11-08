#!/usr/bin/env python3
"""Reproduce closed native palette invalidation ABI/word/metadata proofs."""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import subprocess
from build_native_palette_epoch_hooks import ROOT,build,SOURCE_SHA

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'build/native-palette-epoch-audit/hooks-v1')
    p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    p.add_argument('--no-run',action='store_true');a=p.parse_args();out=a.output.resolve()
    baseline=json.loads((ROOT/'build/native-object-publication/checkpoint-compact-v1/evidence.json').read_text())
    assert sha(a.core)==baseline['core_sha256'] and sha(ROOT/'build/reference_maincpu.bin')==SOURCE_SHA
    if not a.no_run:
        build(out,0x10e000)
        subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-fstrict-aliasing','-fsanitize=address,undefined',
                        '-fno-sanitize-recover=all','-I',str(out),'tools/test_native_palette_epoch_hooks.c','-o',str(out/'test-hooks')],cwd=ROOT,check=True)
        with (out/'closed.log').open('w') as log:
            subprocess.run([str(out/'test-hooks'),str(a.core),str(out)],cwd=ROOT,check=True,stdout=log,stderr=subprocess.STDOUT)
    manifest=json.loads((out/'manifest.json').read_text())
    for name,digest in manifest['sources'].items():assert sha(ROOT/name)==digest,name
    for name,digest in manifest['products'].items():assert sha(out/name)==digest,name
    assert (out/'hooks.bin').read_bytes()==(out/'hooks-original.bin').read_bytes()
    assert sha(out/'hooks.bin')==manifest['blob_sha256']
    cases=list(csv.DictReader((out/'closed-cost.csv').open()))
    assert len(cases)==1536
    costs={}
    groups=collections.defaultdict(list)
    for row in cases:groups[row['family']].append(row)
    assert len(groups)==6
    for name,entries in groups.items():
        assert len(entries)==256 and {(int(r['original_bus']),int(r['test'])) for r in entries}=={(b,t) for b in range(2) for t in range(128)}
        v=[int(r['added_master']) for r in entries]
        if name=='fade_decision_no_write':assert v==[0]*256
        else:assert min(v)>0
        costs[name]={'cases':len(v),'min_added_master':min(v),'max_added_master':max(v),'min_added_ms':min(v)/53693.175,'max_added_ms':max(v)/53693.175}
    assert '1536 paired cases' in (out/'closed.log').read_text()
    result={'schema_version':1,'scope':'Closed original-versus-native leaf-hook verification. Not integrated continuous cadence, interrupt-injection, production ownership or full-ROM palette writer coverage.',
            'source_rom_sha256':SOURCE_SHA,'core_sha256':sha(a.core),'paired_cases':len(cases),'native_calls':len(cases)*2,
            'hook_bytes':manifest['bytes'],'hook_rom_extent':[manifest['rom_start'],manifest['rom_start']+manifest['bytes']],
            'extra_stack_bytes':2,'costs':costs,
            'verified':['All source output words, write order/count on original bus, all original register/SR/USP/SSP effects.',
                        'Every CCR, user/supervisor mode, original CPS reference bus and unchanged native SRAM bus.',
                        'Entry metadata is invalid before original body/store; BUSY covers all body instructions and restores outer depth.',
                        '256/512-word raw copies, exact brightness OR, both signed brightness increments, full5260 write/no-write branches.',
                        'All unrelated RAM, palette bus and SRAM bytes; only exact2-byte CCR stack scratch and8-byte metadata may differ.',
                        'Epoch0/FFFFFFFF and nested busy depth3; wrap leaves READY0 and requires caller overflow policy.'],
            'metadata':manifest['metadata'],'admission_requirements':['Whole route must abort any palette store outside these native bodies after a lease is ready.',
                        'Boot/reset must invalidate leases and initialize metadata; arbitrary/native source writers still require coverage.',
                        'Certify only BUSY0, nonzero epoch, exact full6144-byte match, then recheck epoch/BUSY before READY publication.',
                        'Publication must check READY, current epoch, BUSY0 and current original palette base/video control.',
                        'Never reuse this lease mechanism as a whole-game palette proof or freeze dynamic fades.'],
            'manifest_sha256':sha(out/'manifest.json'),
            'sources':{n:sha(ROOT/n) for n in ['tools/test_native_palette_epoch_hooks.c','tools/verify_native_palette_epoch_hooks.py','tools/test_native_cadence_compact_latch.c']},
            'files':{n:sha(out/n) for n in ['hooks.bin','hooks-original.bin','hooks-test.h','hooks.sym','hooks.disasm','closed.bin','closed-cost.csv','closed.log','test-hooks']}}
    (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ('paired_cases','native_calls','hook_bytes','hook_rom_extent','extra_stack_bytes','costs')},indent=2))

if __name__=='__main__':main()
