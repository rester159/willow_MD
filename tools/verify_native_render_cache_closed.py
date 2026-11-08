#!/usr/bin/env python3
"""Closed adversarial calls into a frozen retained native renderer binary.

Copies inputs into a dedicated fixture, never edits the continuous renderer.
The host computes expected words independently and seeds only diagnostic calls.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import re
import shutil
import struct
import subprocess
from audit_side_arms_render_reuse import oracle

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(argv, **kwargs):
    return subprocess.run([str(x) for x in argv], check=True, **kwargs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--target', type=Path, default=ROOT/'build/native-render-cadence/retained-v2')
    p.add_argument('--output', type=Path, default=ROOT/'build/native-render-cache-closed/v1')
    p.add_argument('--core', type=Path, default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    p.add_argument('--snapshot', action='store_true', help='Use the already copied exact target and headers in output/inputs')
    a = p.parse_args()
    target, out = a.target.resolve(), a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    inputs = out/'inputs'
    inputs.mkdir(exist_ok=True)
    if a.snapshot:
        target = inputs
    m = json.loads((target/'manifest.json').read_text())
    assert m.get('retained_sat',m.get('retained_geometry',False))
    for name, digest in m['source_files'].items():
        path = inputs/Path(name).name if a.snapshot else ROOT/name
        if path.exists():
            assert sha(path) == digest, 'Frozen target source changed: '+name
    before = {str(target/'manifest.json'): sha(target/'manifest.json'), str(a.core): sha(a.core)}
    for name in ['render.bin', 'render.sym', 'render.disasm', 'manifest.json', 'render_resources.h']:
        if not a.snapshot:
            shutil.copy2(target/name, inputs/name)
        before[str(target/name)] = sha(target/name)
    source = ROOT/'tools/native_render_cadence.c'
    queue = [] if a.snapshot else [source]
    copied = set()
    resource_paths = [Path(n) for n in m['resource_files'] if n.endswith('/route_objects.h')]
    assert len(resource_paths) == 1
    assets = resource_paths[0].parent
    for n, digest in m['resource_files'].items():
        assert sha(Path(n)) == digest
    while queue:
        path = queue.pop()
        if path.name in copied:
            continue
        copied.add(path.name)
        shutil.copy2(path, inputs/path.name)
        before[str(path)] = sha(path)
        for name in re.findall(r'^#include "([^"]+)"', path.read_text(), re.M):
            choices = [path.parent/name, ROOT/'src'/name, assets/name, target/name]
            resolved = next((x for x in choices if x.exists()), None)
            assert resolved, name
            queue.append(resolved)
    if a.snapshot:
        before.update({str(path):sha(path) for path in inputs.iterdir() if path.is_file() and not path.name.endswith('.closed.bin')})
    manifest = json.loads((assets/'manifest.json').read_text())
    before[str(assets/'manifest.json')] = sha(assets/'manifest.json')
    for name in ['source-palettes.bin', 'route_objects.h']:
        assert sha(assets/name) == manifest['outputs'][name]
    # Ask the target compiler for offsets, never use host pointer-size layouts.
    exports = {'CACHE_SIZE': 'sizeof(RenderCache)', 'CACHES_OFFSET': 'offsetof(__typeof__(state),caches)',
               'BUDGET_OFFSET': 'offsetof(__typeof__(state),budget)', 'SOURCE_EPOCH': 'offsetof(RenderCache,sourceEpoch)',
               'PALETTE_EPOCH': 'offsetof(RenderCache,paletteEpoch)', 'RECORDS': 'offsetof(RenderCache,records)',
               'PIECES': 'offsetof(RenderCache,pieces)', 'READY': 'offsetof(RenderCache,ready)',
               'SOURCE_WORDS': 'offsetof(RenderCache,sourceWords)',
               'BUDGET_SPRITES': 'offsetof(WillowObjectSatBudget,sprites)', 'BUDGET_PIXELS': 'offsetof(WillowObjectSatBudget,pixels)'}
    if m.get('content_stream'):
        exports['GEOMETRY_ID']='offsetof(RenderCache,geometryId)'
    layout = out/'layout.c'
    layout.write_text('#define RENDER_RETAINED 1\n#include "native_render_cadence.c"\n'
                      '__attribute__((used,section(".test_layout"))) const uint32_t closedLayout[]={'+','.join(exports.values())+'};\n')
    run(['m68k-elf-gcc', '-m68000', '-O2', '-ffreestanding', '-fno-builtin', '-I'+str(inputs), '-c', layout, '-o', out/'layout.o'])
    run(['m68k-elf-objcopy', '-O', 'binary', '-j', '.test_layout', out/'layout.o', out/'layout.bin'])
    offsets = dict(zip(exports, struct.unpack('>'+str(len(exports))+'I', (out/'layout.bin').read_bytes())))
    syms = m['symbols']
    offsets['CACHES'] = syms['state'] + offsets.pop('CACHES_OFFSET')
    offsets['BUDGET'] = syms['state'] + offsets.pop('BUDGET_OFFSET')
    offsets.update(PREPARE=syms['nativeRenderPrepare'], PUBLISH=syms['nativeRenderPublish'], BOOT=syms['nativeRenderVDPBoot'])
    offsets['CONTENT_STREAM']=int(m.get('content_stream',False))
    known = {(t['code'], t['source_attribute']) for t in manifest['templates']}
    entries = {(e['code'], e['source_group']): e for e in manifest['entries']}
    base = [(200, 152, 0xb09, 0x220a), (232, 152, 0x33, 0x2220), (242, 80, 0x76b, 0x441f)]
    tests = []
    caches = {}
    last = None
    geometry = 0

    def add(name, records=base, bank=0, epoch=100, palette=7, valid=1, busy=0,
            bad_palette=0, bad_view=0, invalidate=0, post=0, reset=0):
        nonlocal caches, last, geometry
        records = list(records)
        if reset:
            caches, last, geometry = {}, None, 0
        old = caches.pop(bank, None)
        accept, status, reuse = True, 0, 0
        if valid != 1 or epoch == 0 or len(records) > 8 or busy or bad_view or bad_palette:
            accept, status = False, 11
        elif last and (epoch < last[0] or (epoch == last[0] and (bank, len(records)) != last[1:])):
            accept, status = False, 12
        elif any((attr & 0xff00) == 0xff00 or (code <= 0x27ff and (code, attr) not in known) for x, y, code, attr in records):
            accept = False
        sat, expected = [], []
        if accept:
            try:
                sat, expected = oracle(records, entries)
            except AssertionError:
                accept = False
        if accept:
            retained=next((c for c in [old,caches.get(bank^1)] if c and c[:2]==(records,palette)),None)
            reuse=int(retained is not None)
            if not reuse:geometry+=1
            accepted_geometry=retained[2] if reuse else geometry
            caches[bank] = (records, palette, accepted_geometry)
            last = epoch, bank, len(records)
        counts = expected[1+len(sat)*4::2] if accept else [0]*224
        assert len(counts) == 224
        tests.append(dict(name=name, records=records, bank=bank, epoch=epoch, paletteEpoch=palette,
                          valid=valid, busy=busy, badPalette=bad_palette, badView=bad_view, invalidate=invalidate,
                          post=post, reset=reset, accept=int(accept), status=status, reuse=reuse,
                          geometry=accepted_geometry if accept else 0,
                          pieces=len(sat), sat=[v for s in sat for v in s], sprites=counts))

    def seed(name):
        add(name+'_cold', reset=1)
        add(name+'_same_epoch')
        add(name+'_other_bank', bank=1, epoch=101)

    # Every source word bit at every baseline ordinal, including high coordinate
    # bits that affect raw identity even when wrapped geometry is unchanged.
    for ordinal in range(3):
        for field in range(4):
            for bit in range(16):
                name=f'word_{ordinal}_{field}_bit_{bit}'
                seed(name)
                records=[list(r) for r in base]
                records[ordinal][field] ^= 1 << bit
                add(name, [tuple(r) for r in records], epoch=102)
    for n, key in enumerate(sorted(known)):
        name=f'known_template_{n}'
        seed(name)
        records=[*base]
        records[0]=(base[0][0], base[0][1], *key)
        add(name, records, bank=n&1, epoch=102)
    for count in [0, 1, 2, 3, 4, 8, 9]:
        name=f'count_{count}'
        seed(name)
        records=base[:count]+[(120+(n%4)*40, 24+(n//4)*80, 0x24c, 0x1008) for n in range(max(0, count-3))]
        add(name, records, epoch=102)
    for name, records in [('swap',[base[1],base[0],base[2]]), ('reverse',list(reversed(base)))]:
        seed(name)
        add(name, records, epoch=102)
    for name, options in [
        ('lower_epoch',dict(epoch=99)), ('zero_epoch',dict(epoch=0)),
        ('same_epoch_other_bank',dict(epoch=101)), ('same_epoch_count',dict(epoch=101,bank=1,records=base[:2])),
        ('same_epoch_valid',dict(epoch=101,bank=1)), ('new_epoch_same',dict(epoch=102)),
        ('metadata_not_ready',dict(epoch=102,valid=0)), ('metadata_invalid_ready',dict(epoch=102,valid=2)),
        ('palette_epoch_changed',dict(epoch=102,palette=8,invalidate=1)),
        ('palette_content_changed',dict(epoch=102,palette=8,invalidate=1,bad_palette=1)),
        ('palette_busy',dict(epoch=102,busy=1)), ('palette_recertify',dict(epoch=102,invalidate=1)),
        ('view_palette_selector',dict(epoch=102,bad_view=1)), ('view_flip',dict(epoch=102,bad_view=2)),
        *[(f'post_prepare_invalidate_{n}',dict(epoch=102,post=n)) for n in range(1,6)],
    ]:
        seed(name)
        add(name, **options)
    # Keep a successful sequence across empty/growing/shrinking hardware lists.
    seed('list_sequence')
    for n, records in enumerate([[], [], base[:1], base[:2], base, [], base]):
        add(f'list_sequence_{n}', records, bank=n&1, epoch=102+n)
    header='/* Host-only mutation inputs and independent expected native words. */\n'
    for name,value in offsets.items():
        header+=f'#define {name} {value}u\n'
    header+='typedef struct {const char*name;unsigned bank,epoch,paletteEpoch,valid,busy,badPalette,badView,invalidate,post,reset,accept,status,reuse,pieces,geometry,count;uint16_t records[36],sat[320];uint8_t sprites[224];} Test;\n'
    header+='static const Test tests[]={\n'
    names=['bank','epoch','paletteEpoch','valid','busy','badPalette','badView','invalidate','post','reset','accept','status','reuse','pieces','geometry']
    for t in tests:
        header+='{'+json.dumps(t['name'])+','+','.join(str(t[n]) for n in names)+','+str(len(t['records']))+','
        for values in [[v for r in t['records'] for v in r],t['sat'],t['sprites']]:
            header+='{'+(','.join(map(str, values)) or '0')+'},'
        header=header.rstrip(',')+'},\n'
    header+='};\nstatic const uint8_t sourcePalette[]={'+','.join(map(str,(assets/'source-palettes.bin').read_bytes()))+'};\n'
    (out/'closed-fixture.h').write_text(header)
    (out/'cases.json').write_text(json.dumps(tests,indent=2)+'\n')
    driver=ROOT/'tools/test_native_render_cache_closed.c'
    run(['cc','-std=c11','-O2','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tools'),'-I'+str(out),driver,'-o',out/'test-closed'])
    with (out/'test.log').open('w') as log:
        run([out/'test-closed',a.core,inputs/'render.bin',out/'calls.csv'],stdout=log,stderr=subprocess.STDOUT)
    assert all(sha(Path(path))==digest for path,digest in before.items()), 'Inputs changed during closed proof'
    report={'scope':__doc__, 'cases':len(tests),'groups':sum(t['reset'] for t in tests),
            'accepted':sum(t['accept'] for t in tests),'refused':sum(not t['accept'] for t in tests),
            'expected_reuses':sum(t['reuse'] for t in tests),
            'target_binary_sha256':sha(inputs/'render.bin'), 'abi_layout':offsets,
            'result':(out/'test.log').read_text(), 'input_hashes':before,
            'tool_hashes':{str(x):sha(x) for x in [Path(__file__),driver,ROOT/'tools/audit_side_arms_render_reuse.py']},
            'limitations':'Closed C ABI calls with host setup between calls and display disabled after target boot; no actual interrupt injection, wrapper/context IRQ admission, game cadence, active-display transfer timing or FPS claim. Raw core cycle columns are diagnostic only and must not be interpreted as performance. Fixed catalog/residency/view; palette and view invalidations are tested. Budget arrays checked on actual rebuilds; reused caches carry previously certified geometry, not a new budget materialization.'}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['result'])


if __name__=='__main__':
    main()
