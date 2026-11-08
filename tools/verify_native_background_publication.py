#!/usr/bin/env python3
"""Verify post-IRQ native low planes against the same current source maps.

This checks source pen15 transparency, prepared reference provenance, native
name words, hardware scroll and the complete 320x224 low-plane composite.
It does not admit object priority, Window/HUD, timing or all-game coverage.
"""
import argparse
import csv
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import struct
import zipfile

from cps1_gfx import assemble_region, decode_tile
from prepare_native_route_background import physical_code, cell_index
from test_native_background_context import REGIONS

ROOT = Path(__file__).resolve().parents[1]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def word(data, at):
    return (data[at] << 8) | data[at+1]


def verify(run, assets, canonical, output, complete_register_diagnostic=False):
    run, assets, canonical, output = map(Path.resolve, (run, assets, canonical, output))
    require(not output.exists(), 'Use a fresh output directory')
    output.mkdir(parents=True)
    pins = {}

    def pin(path, digest=None):
        path = path.resolve()
        actual = sha(path)
        require(digest is None or actual == digest, f'Changed input: {path}')
        pins[str(path)] = actual
        return path.read_bytes()

    pin(Path(__file__))
    manifest = json.loads(pin(run/'observer-manifest.json'))
    strict_parity = manifest.get('run', {}).get('exit_code') == 0
    register_failures = []
    if not strict_parity:
        require(complete_register_diagnostic and manifest.get('diagnostic_register_failure_collection') and
                manifest['run']['exit_code'] == 1, 'Observer did not complete with strict state parity')
        pin(run/'register-failures.csv')
        register_failures = list(csv.DictReader((run/'register-failures.csv').open()))
        require(register_failures and f'Diagnostic register failures: {len(register_failures)}; NOT ADMITTED' in
                (run/'observer.log').read_text(), 'Register diagnostic did not complete')
        rendered = json.loads(pin(run/'render-summary.json'))
        require(rendered['paired_vdp_frames'] == manifest['run']['frames'], 'Diagnostic frames are incomplete')
    for name, digest in manifest['inputs'].items():
        p = Path(name)
        pin(p if p.is_absolute() else ROOT/p, digest)
    pin(run/'test_native_render_cadence.c', manifest['generated_sha256'])
    pin(run/'test-native-render-cadence', manifest['binary_sha256'])
    pin(run/'observer.log', manifest['run']['log_sha256'])
    require(manifest.get('live_bg_contract'), 'No explicit live background contract')
    if (run/'background-layout.json').exists():
        layout = json.loads(pin(run/'background-layout.json'))
    else:
        require(complete_register_diagnostic and not strict_parity, 'Missing complete publication layout')
        offset, regions = 0, []
        for name, size, address in REGIONS:
            regions.append({'name':name,'offset':offset,'size':size,'source_start':address})
            offset += size
        layout = {'samples':json.loads(pin(run/'background-summary.json'))['samples'],
                  'index':'background-publications.csv',
                  'source':{'record_bytes':offset,'regions':regions,'file':'background-source-contexts.bin'},
                  'vram':{'record_bytes':65536,'file':'background-vram.bin'},
                  'vdp_state':{'record_bytes':288,'file':'background-vdp-state.bin'}}
    require(layout['vram']['record_bytes'] == 65536 and layout['vdp_state']['record_bytes'] == 288, 'Unexpected VDP schema')
    regions = {r['name']: (r['offset'], r['size']) for r in layout['source']['regions']}
    require({k: regions[k][1] for k in ('scroll2','scroll3','palettes','ports','work_ram')} ==
            {'scroll2':16384,'scroll3':16384,'palettes':6144,'ports':512,'work_ram':65536}, 'Unexpected source schema')
    for directory in (assets, canonical):
        metadata = json.loads(pin(directory/'manifest.json'))
        for name, digest in metadata['hashes'].items():
            p = Path(name)
            pin(p if p.is_absolute() else ROOT/p, digest)
        for name, digest in metadata['outputs'].items():
            pin(directory/name, digest)
    entries = json.loads(pin(assets/'entries.json'))
    source_entries = json.loads(pin(canonical/'entries.json'))
    lookup = {(d['layer'], d['code'], d['attribute']): d for d in entries}
    canonical_lookup = {(d['layer'], d['code'], d['attribute']): d for d in source_entries}
    require(len(lookup) == len(entries) and lookup.keys() == canonical_lookup.keys(), 'Source keys differ')
    patterns = pin(canonical/'canonical-patterns.bin')
    palette, cram = pin(assets/'source-palettes.bin'), pin(assets/'cram.bin')
    resident = dict(struct.iter_unpack('>H32s', pin(assets/'physical-tiles.bin')))
    resident[0] = bytes(32)
    for key, d in lookup.items():
        for target, source in zip(d['references'], canonical_lookup[key]['references'], strict=True):
            require(target['pattern'] == source['pattern'], f'Canonical ID differs: {key}')
            if target['word'] != 65535:
                require(target['word'] & 0xf800 == source['attributes'], f'Reference flags differ: {key}')
                require(resident[target['word'] & 2047] == patterns[source['pattern']*32:source['pattern']*32+32], f'Pattern differs: {key}')
    zip_path = ROOT/'willow.zip'
    pin(zip_path)
    with zipfile.ZipFile(zip_path) as z:
        gfx = assemble_region(z)

    @lru_cache(None)
    def source_tile(layer, code):
        size = 16 if layer == 2 else 32
        physical = physical_code(layer, code)
        return decode_tile(gfx, physical, size) if physical is not None else [[15]*size for _ in range(size)]

    @lru_cache(None)
    def source_cell(layer, code, attr, sub):
        key = (layer, code, attr)
        require(key in lookup, f'Unprepared live source key {key}')
        target = lookup[key]['references'][sub]
        ref = canonical_lookup[key]['references'][sub]
        size = 16 if layer == 2 else 32
        tile = source_tile(layer, code)
        packed = patterns[ref['pattern']*32:ref['pattern']*32+32]
        flags, colors = ref['attributes'], []
        for y in range(8):
            for x in range(8):
                dx, dy = (sub % (size//8))*8+x, (sub//(size//8))*8+y
                pen = tile[size-1-dy if attr&64 else dy][size-1-dx if attr&32 else dx]
                tx, ty = (7-x if flags&0x800 else x), (7-y if flags&0x1000 else y)
                native = (packed[ty*4+tx//2] >> (0 if tx&1 else 4)) & 15
                require((pen == 15) == (native == 0), f'Original pen transparency differs: {key}/{sub}/{x}/{y}')
                colors.append(-1 if pen == 15 else ((flags>>13)&3)*16+native)
        return target['word'], tuple(colors)

    @lru_cache(None)
    def native_cell(name):
        tile = name & 2047
        require(tile in resident, f'Plane references an unowned pattern {tile}')
        raw, colors = resident[tile], []
        for y in range(8):
            for x in range(8):
                tx, ty = (7-x if name&0x800 else x), (7-y if name&0x1000 else y)
                pen = (raw[ty*4+tx//2] >> (0 if tx&1 else 4)) & 15
                colors.append(-1 if not pen else ((name>>13)&3)*16+pen)
        return tuple(colors)

    source = pin(run/layout['source']['file'])
    vram = pin(run/layout['vram']['file'])
    vdp = pin(run/layout['vdp_state']['file'])
    pin(run/layout['index'])
    rows = list(csv.DictReader((run/layout['index']).open()))
    stride = layout['source']['record_bytes']
    require(len(rows) == layout['samples'] and len(source) == len(rows)*stride and
            len(vram) == len(rows)*65536 and len(vdp) == len(rows)*288, 'Incomplete publication data')
    checked, skipped, total_pixels, source_pixels = [], {'cold':0,'inactive':0}, 0, 0
    for sample, row in enumerate(rows):
        require(int(row['sample']) == sample, 'Publication ordering changed')
        src = memoryview(source)[sample*stride:(sample+1)*stride]
        state = memoryview(vdp)[sample*288:(sample+1)*288]
        vr = memoryview(vram)[sample*65536:(sample+1)*65536]
        rec = {name: src[at:at+size] for name,(at,size) in regions.items()}
        if int(row['cold_partial']):
            skipped['cold'] += 1
            continue
        if word(rec['work_ram'],0x8004) != 2 or word(rec['work_ram'],0x8008) != 2:
            skipped['inactive'] += 1
            continue
        label = f'sample{sample}/frame{row["frame"]}'
        require(rec['palettes'] == palette and state[:128] == cram, f'{label}: source/native palette differs')
        for tile, raw in resident.items():
            require(vr[tile*32:tile*32+32] == raw, f'{label}: resident tile{tile} differs')
        ports, regs = rec['ports'], state[256:288]
        require(all(word(ports, at) == value for at,value in ((0x104,0x9180),(0x106,0x9140),(0x10a,0x91c0),(0x122,0x3e),(0x170,0x12f8))), f'{label}: unsupported source layer/view mode')
        require(regs[16] == 1 and regs[11] == 0 and regs[2] == 0x30 and regs[4] == 7 and regs[13] == 0x3c and regs[7]&63 == 0, f'{label}: unsupported native low-plane mode')
        views = [word(ports, at) for at in (0x110,0x112,0x114,0x116)]
        require(views == [int(row[k]) for k in ('s2x','s2y','s3x','s3y')], f'{label}: current source view differs from publication')
        for layer in range(2):
            require(word(vr,0xf000+layer*2) == (-(96+views[layer*2]))&65535, f'{label}: wrong Hscroll layer{layer}')
            require(word(state,128+layer*2) == (16+views[layer*2+1])&255, f'{label}: wrong Vscroll layer{layer}')
        expected, actual = [bytearray(320*224) for _ in range(2)]
        near_opaque = 0
        for y in range(224):
            for x in range(320):
                resolved = []
                for li, layer, size, base in ((0,2,16,0xc000),(1,3,32,0xe000)):
                    sx, sy = (96+x+views[li*2])%(size*64), (16+y+views[li*2+1])%(size*64)
                    at = cell_index(layer,sx//size,sy//size)*4
                    code, attr = word(rec[f'scroll{layer}'],at), word(rec[f'scroll{layer}'],at+2)
                    sub = ((sy%size)//8)*(size//8)+(sx%size)//8
                    ref, colors = source_cell(layer,code,attr,sub)
                    fitted = colors[(sy&7)*8+(sx&7)]
                    nx, ny = (x-word(vr,0xf000+li*2))&511, (y+word(state,128+li*2))&255
                    name = word(vr,base+((ny//8)*64+nx//8)*2)
                    require(not name&0x8000, f'{label}: high-priority plane word at{x},{y}/layer{layer}')
                    native = native_cell(name)[(ny&7)*8+(nx&7)]
                    resolved.append((ref,fitted,name,native,code,attr,sub))
                near, far = resolved
                require(near[0] != 65535 and near[2] == near[0], f'{label}: near name word at{x},{y}: source{near[4]:04x}/{near[5]:04x}/{near[6]} expected{near[0]:04x} actual{near[2]:04x}')
                if near[1] < 0:
                    require(far[0] != 65535 and far[2] == far[0], f'{label}: exposed far name word at{x},{y}: source{far[4]:04x}/{far[5]:04x}/{far[6]} expected{far[0]:04x} actual{far[2]:04x}')
                else:
                    near_opaque += 1
                fit = near[1] if near[1] >= 0 else far[1] if far[1] >= 0 else 0
                got = near[3] if near[3] >= 0 else far[3] if far[3] >= 0 else 0
                require(fit == got, f'{label}: composite palette index at{x},{y}: expected{fit} actual{got}')
                expected[y*320+x] = fit
                actual[y*320+x] = got
        total_pixels += 320*224
        source_pixels += 2*320*224
        checked.append({'sample':sample,'frame':int(row['frame']),'capture':int(row['capture']),
                        'current_scroll_words':views,'near_opaque_pixels':near_opaque,
                        'composite_sha256':hashlib.sha256(actual).hexdigest(),
                        'late_publication':max(int(row['master_cycles']),int(row['retire_upper_master'])) > int(row['deadline_master'])})
        if len(checked)%32 == 0:
            print(f'Checked {len(checked)} current-source low-plane publications', flush=True)
    require(checked, 'No complete active low-plane publications')
    require(all(sha(Path(name)) == digest for name,digest in pins.items()), 'Verifier input changed while running')
    report = {'schema_version':1,'scope':__doc__,'strict_source_observer_completed':strict_parity,
              'source_register_failures_retained':len(register_failures),
              'successful_active_publications_checked':len(checked),'skipped':skipped,
              'current_source_layer_pixels_checked':source_pixels,'exact_composite_pixels':total_pixels,
              'original_source_subtiles_decoded':source_cell.cache_info().currsize,
              'mismatches':0,'late_publications':sum(r['late_publication'] for r in checked),
              'timing_admitted':False,'actor_priority_or_window_admitted':False,
              'missing_publications':json.loads(pin(run/'background-summary.json'))['paths'][0]['missing_active_publications'],
              'inputs':pins}
    (output/'frames.json').write_text(json.dumps(checked,indent=2)+'\n')
    (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k != 'inputs'},indent=2))
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run',type=Path,required=True)
    p.add_argument('--assets',type=Path,default=ROOT/'build/native-background-residency/prepared-v1')
    p.add_argument('--canonical',type=Path,default=ROOT/'build/native-route-background-audit/checkpoint-v1/canonical-v1')
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--complete-register-diagnostic',action='store_true',
                   help='Check pixels from a completed explicit register-failure collector; retain all source failures and exit1')
    a = p.parse_args()
    report = verify(a.run,a.assets,a.canonical,a.output,a.complete_register_diagnostic)
    if not report['strict_source_observer_completed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
