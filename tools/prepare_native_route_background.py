#!/usr/bin/env python3
"""Source-keyed prepared background resources; no captured maps in target data.

The capture only establishes required key coverage and working-set evidence.
Patterns derive from original graphics and full-precision source palette words.
The active route's immutable four-page Genesis CRAM is reused without edits.
No physical pattern addresses, source camera values, maps, or animation sequence
are compiled into the resource header. Native residency and timing remain caller
responsibilities. Other palette epochs explicitly require a different admission.
"""
from collections import Counter
from functools import lru_cache
from pathlib import Path
import argparse
import csv
import hashlib
import json
import math
import struct
import zipfile

from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from cps1_palette import cps_rgb
from native_palette_fit import color_error
from fit_native_object_palette_anchors import is_neutral_stone, preserves_neutral

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def physical_code(layer, code):
    """Exact WL24B PAL range decode, including transparent unmapped codes.

    cps1_v.cpp get_tile1_info passes the complete Scroll2 word; mapper shifts
    it by1 before admitting0..3FFF, so high bits must NOT silently wrap.
    Scroll3 get_tile2_info first masks3FFF, mapper shifts3 and admits5000..6FFF.
    """
    if layer == 2:
        return 0x4000 + code if code <= 0x1fff else None
    code &= 0x3fff
    return code if 0xa00 <= code <= 0xdff else None


def cell_index(layer, col, row):
    if layer == 2:
        return (row & 15) + ((col & 63) << 4) + ((row & 48) << 6)
    return (row & 7) + ((col & 63) << 3) + ((row & 56) << 6)


def prepare_catalog(gfx, source_palette, cram, keys):
    pages = [[tuple(((word >> shift) & 7)*34 for shift in (1, 5, 9))
              for word in struct.unpack_from('>16H', cram, page*32)] for page in range(4)]
    patterns = [bytes(32)]
    pattern_ids = {patterns[0]: 0}
    descriptors = []
    checks = neutral_checks = unmapped_pixels = 0
    metrics = Counter()
    page_counts = Counter()

    @lru_cache(None)
    def pixels(layer, code):
        size = 16 if layer == 2 else 32
        physical = physical_code(layer, code)
        if physical is None:
            return [[15]*size for _ in range(size)]
        return decode_tile(gfx, physical, size)

    @lru_cache(None)
    def choices(color, page):
        allowed = [pen for pen in range(1, 16)
                   if not is_neutral_stone(color) or preserves_neutral(color, pages[page][pen])]
        if not allowed:
            return None
        pen = min(allowed, key=lambda p: (color_error(color, pages[page][p]), p))
        return color_error(color, pages[page][pen]), pen

    for layer, code, attr in sorted(keys):
        size = 16 if layer == 2 else 32
        n = size//8
        raw = pixels(layer, code)
        words = struct.unpack_from('>16H', source_palette, (0x800 if layer == 2 else 0xc00)+(attr & 31)*32)
        colors = [cps_rgb(word) for word in words]
        refs, masks, maps = [], [], []
        for ty in range(n):
            for tx in range(n):
                pens = [raw[size-1-(ty*8+y) if attr & 64 else ty*8+y]
                           [size-1-(tx*8+x) if attr & 32 else tx*8+x]
                        for y in range(8) for x in range(8)]
                hist = Counter(colors[p] for p in pens if p != 15)
                allowed = [page for page in range(4) if all(choices(color, page) is not None for color in hist)]
                assert allowed, 'No shared page preserves source neutral hues'
                page = min(allowed, key=lambda p: (sum(count*choices(color, p)[0] for color, count in hist.items()), p))
                mapping = [0 if p == 15 else choices(colors[p], page)[1] if colors[p] in hist else 1 for p in range(16)]
                converted = [mapping[p] for p in pens]
                packed = genesis_tiles([converted[y*8:y*8+8] for y in range(8)])
                if packed not in pattern_ids:
                    pattern_ids[packed] = len(patterns)
                    patterns.append(packed)
                pattern = pattern_ids[packed]
                assert pattern < 16384
                refs.append((page << 14) | pattern)
                masks.append(sum(1 << p for p in set(pens) if p != 15))
                maps.append(mapping)
                page_counts[page] += bool(hist)
                for y in range(8):
                    for x in range(8):
                        pen = pens[y*8+x]
                        native = (packed[y*4+x//2] >> (0 if x & 1 else 4)) & 15
                        assert (native == 0) == (pen == 15)
                        assert native == mapping[pen]
                        if pen != 15:
                            color, output = colors[pen], pages[page][native]
                            metrics['pixels'] += 1
                            metrics['rgb_squared_error'] += sum((a-b)**2 for a, b in zip(color, output))
                            metrics['weighted_error'] += color_error(color, output)
                            if is_neutral_stone(color):
                                assert preserves_neutral(color, output)
                                neutral_checks += 1
                        checks += 1
                if physical_code(layer, code) is None:
                    unmapped_pixels += 64
        descriptors.append({'layer': layer, 'code': code, 'attribute': attr,
                            'physical_code': physical_code(layer, code), 'size': size,
                            'references': refs, 'source_pen_masks': masks, 'pen_maps': maps})
    return patterns, descriptors, {
        'verified_source_pixels': checks, 'neutral_source_pixels_checked': neutral_checks,
        'unmapped_source_pixels_transparent': unmapped_pixels,
        'opaque_pixels': metrics['pixels'], 'weighted_error': metrics['weighted_error'],
        'rgb_rmse': math.sqrt(metrics['rgb_squared_error']/max(1, 3*metrics['pixels'])),
        'opaque_subtile_pages': dict(page_counts)}


def read_capture(source):
    evidence_path = source/'capture-evidence.json'
    evidence = json.loads(evidence_path.read_text())
    layout = json.loads((source/'layout.json').read_text())
    index = source/layout.get('index', 'source-background-contexts.csv')
    binary = source/layout.get('binary', 'source-background-contexts.bin')
    for path in [source/'layout.json', index, binary, source/'captured-original-banks.bin']:
        assert sha(path) == evidence['outputs'][path.name], path
    rows = list(csv.DictReader(index.open()))
    raw = binary.read_bytes()
    assert len(raw) == len(rows)*layout['record_bytes']
    assert len(rows) == layout['records'] == evidence['captures']
    regions = {r['name']: (r['offset'], r['size']) for r in layout['regions']}
    required = {'scroll2': 16384, 'scroll3': 16384, 'scroll1': 16384,
                'palettes': 6144, 'ports': 512, 'row_scroll': 2048}
    for name, size in required.items():
        assert regions[name][1] == size, (name, regions.get(name))
    def record(row):
        at = (int(row['capture'])-1)*layout['record_bytes']
        return {name: memoryview(raw)[at+offset:at+offset+size]
                for name, (offset, size) in regions.items()}
    return rows, record, [evidence_path, source/'layout.json', index, binary]


def active(row):
    return int(row['manager']) == int(row['scene']) == 2


def upcoming_ports(record):
    """18D0 publishes OLD source staging, then computes the following stage."""
    ports = list(struct.unpack('>256H', record['ports']))
    for port, source in [(0x110, 0xf446), (0x112, 0xf448), (0x114, 0xf44a), (0x116, 0xf44c)]:
        ports[port//2] = struct.unpack_from('>H', record['work_ram'], source)[0]
    return ports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--objects', type=Path, default=ROOT/'build/native-route-objects/prepared-v2')
    parser.add_argument('--output', type=Path, default=ROOT/'build/native-route-background-audit/prepared-v1')
    args = parser.parse_args()
    source, objects, out = args.capture.resolve(), args.objects.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    rows, get, input_paths = read_capture(source)
    object_manifest = json.loads((objects/'manifest.json').read_text())
    for name in ['source-palettes.bin', 'cram.bin', 'object-patterns.bin']:
        assert sha(objects/name) == object_manifest['outputs'][name]
    palette = (objects/'source-palettes.bin').read_bytes()
    cram = (objects/'cram.bin').read_bytes()
    assert len(palette) == 6144 and len(cram) == 128
    all_keys, active_keys = set(), set()
    map_hashes = {layer: set() for layer in (2, 3)}
    all_palettes, active_palettes = set(), set()
    for row in rows:
        record = get(row)
        all_palettes.add(hashlib.sha256(record['palettes']).hexdigest())
        if active(row):
            assert record['palettes'] == palette, 'Active palette differs from admitted object resource epoch'
            active_palettes.add(hashlib.sha256(record['palettes']).hexdigest())
        for layer in (2, 3):
            data = record[f'scroll{layer}']
            keys = {(layer, code, attr) for code, attr in struct.iter_unpack('>2H', data)}
            all_keys.update(keys)
            if active(row):
                active_keys.update(keys)
                map_hashes[layer].add(hashlib.sha256(data).hexdigest())
    archive = ROOT/'willow.zip'
    with zipfile.ZipFile(archive) as z:
        gfx = assemble_region(z)
    patterns, descriptors, metrics = prepare_catalog(gfx, palette, cram, all_keys)
    lookup = {(d['layer'], d['code'], d['attribute']): d for d in descriptors}
    # Immutable key/shape/pattern data only. No capture map/coordinate is target data.
    header = '/* Exact source keys; fixed admitted source/CRAM epoch. No captured maps or origins. */\n#include <stdint.h>\n'
    header += 'typedef struct {uint32_t key;uint16_t offset;} WillowRouteBackgroundPair;\n'
    for layer in (2, 3):
        keys, refs, masks = [], [], []
        for d in descriptors:
            if d['layer'] != layer:
                continue
            keys.append((d['code'] << 16 | d['attribute'], len(refs)))
            refs.extend(d['references']);masks.extend(d['source_pen_masks'])
        assert len(refs) < 65536
        header += f'static const WillowRouteBackgroundPair routeBackground{layer}Pairs[]={{'+','.join('{%dUL,%d}' % v for v in keys)+'};\n'
        header += f'static const uint16_t routeBackground{layer}Refs[]={{'+','.join(map(str, refs))+'};\n'
        header += f'static const uint16_t routeBackground{layer}SourcePens[]={{'+','.join(map(str, masks))+'};\n'
    (out/'route_background.h').write_text(header)
    (out/'background-patterns.bin').write_bytes(b''.join(patterns))
    (out/'source-palettes.bin').write_bytes(palette)
    (out/'cram.bin').write_bytes(cram)
    (out/'entries.json').write_text(json.dumps(descriptors, separators=(',', ':'))+'\n')
    # Actual live ring + registers determine visibility in this audit only.
    # Originalraw viewport x64..447,y16..239; centered320crop startsx96.
    active_rows = [row for row in rows if active(row)]
    next_port_checks = 0
    for row, after in zip(active_rows, active_rows[1:]):
        assert int(after['capture']) == int(row['capture'])+1
        staged = upcoming_ports(get(row));actual = struct.unpack('>256H', get(after)['ports'])
        for at in (0x110, 0x112, 0x114, 0x116):
            assert staged[at//2] == actual[at//2], ('Source staging/publication mismatch', row['capture'], at)
            next_port_checks += 1
    frames, visible_union = [], set()
    last_visible = None
    registers = {at: set() for at in list(range(0x102, 0x124, 2))+list(range(0x168, 0x172, 2))}
    for row in active_rows:
        record = get(row);ports = upcoming_ports(record)
        for at, values in registers.items():
            values.add(ports[at//2])
        visible, per_layer, keys_used, mixed, high, low = set(), {}, set(), 0, 0, 0
        # Scalar scroll active route only. Row-scroll/global-flip must not be
        # silently approximated by this working-set calculation.
        assert ports[0x122//2] == 0x003e
        assert ports[0x104//2] == 0x9180 and ports[0x106//2] == 0x9140
        masks = [ports[at//2] for at in (0x16e, 0x16c, 0x16a, 0x168)]
        for layer, size, xr, yr in [(2, 16, 0x110, 0x112), (3, 32, 0x114, 0x116)]:
            extent = 64*size;xx = (96+ports[xr//2]) % extent;yy = (16+ports[yr//2]) % extent
            layer_set = set()
            for y in range(yy//8, (yy+223)//8+1):
                for x in range(xx//8, (xx+319)//8+1):
                    tx, ty = (x*8) % extent, (y*8) % extent
                    offset = cell_index(layer, tx//size, ty//size)*4
                    code, attr = struct.unpack_from('>2H', record[f'scroll{layer}'], offset)
                    key = layer, code, attr;d = lookup[key];keys_used.add(key)
                    subtile = (ty % size)//8*(size//8)+(tx % size)//8
                    ref = d['references'][subtile]
                    if ref & 16383:
                        layer_set.add(ref & 16383)
                    if layer == 2:
                        pens = d['source_pen_masks'][subtile];mask = masks[(attr >> 7) & 3] & 32767
                        foreground, background = pens & mask, pens & (~mask & 32767)
                        mixed += bool(foreground and background)
                        high += bool(foreground);low += bool(background)
            visible.update(layer_set);per_layer[layer] = len(layer_set)
        frames.append({'capture': int(row['capture']), 'frame': int(row['frame']),
                       'patterns': len(visible), 'new_vs_previous': len(visible-last_visible) if last_visible is not None else len(visible),
                       'per_layer_patterns': per_layer, 'visible_keys': len(keys_used),
                       'near_subtiles_mixed_priority': mixed, 'near_subtiles_high': high, 'near_subtiles_low': low})
        visible_union.update(visible);last_visible = visible
    static_vram = (ROOT/'build/cartridge-gap-audit/frame1200_window_v1/vram.bin').read_bytes()
    object_bytes = (objects/'object-patterns.bin').read_bytes()
    object_patterns = {object_bytes[at:at+32] for at in range(0, len(object_bytes), 32)}
    window_patterns = {static_vram[tile*32:(tile+1)*32] for tile in range(876, 981)}
    active_patterns = {ref & 16383 for key in active_keys for ref in lookup[key]['references']} - {0}
    dedup_object = {n for n in active_patterns if patterns[n] in object_patterns}
    dedup_window = {n for n in active_patterns if patterns[n] in window_patterns}
    paths = [Path(__file__).resolve(), archive, ROOT/'tools/cps1_gfx.py', ROOT/'tools/cps1_palette.py',
             ROOT/'tools/native_palette_fit.py', ROOT/'tools/fit_native_object_palette_anchors.py',
             ROOT/'build/cartridge-gap-audit/cps1_v.cpp', objects/'manifest.json', objects/'source-palettes.bin',
             objects/'cram.bin', objects/'object-patterns.bin', ROOT/'build/cartridge-gap-audit/frame1200_window_v1/vram.bin', *input_paths]
    report = {
        'scope': 'Exact route source-key coverage; patterns fitted once to admitted active palette. Capture maps/positions are audit inputs only, never native resource data. No native residency, priority compositor, live source write hook, VBlank or full-game admission.',
        'captures': len(rows), 'active_captures': len(active_rows), 'all_palette_variants': len(all_palettes),
        'active_palette_variants': len(active_palettes), 'nonactive_palette_admission': 'Not admitted; fixed palette interpretation is valid only after exact6144-byte current-epoch certification.',
        'all_map_keys': len(all_keys), 'active_map_keys': len(active_keys),
        'per_layer': {str(layer): {'all_keys': sum(k[0] == layer for k in all_keys),
                                 'active_keys': sum(k[0] == layer for k in active_keys),
                                 'active_map_variants': len(map_hashes[layer]),
                                 'unmapped_codes': sorted({k[1] for k in all_keys if k[0] == layer and physical_code(layer, k[1]) is None})} for layer in (2, 3)},
        'pattern_count_including_blank': len(patterns), 'pattern_bytes': len(patterns)*32,
        'active_map_pattern_union': len(active_patterns), 'active_visible_pattern_union': len(visible_union),
        'peak_active_visible_patterns': max(f['patterns'] for f in frames),
        'peak_new_visible_patterns_after_first': max(f['new_vs_previous'] for f in frames[1:]),
        'active_pattern_reuse_from_object_atlas': len(dedup_object),
        'active_pattern_reuse_from_static_window_atlas': len(dedup_window),
        'layout_constraints': {'pattern_ceiling_exclusive': 1536, 'low_reserved': [0, 15],
                               'objects': [981, 1480], 'static_window': [876, 980],
                               'bg_capacity_preserving_both': 915, 'bg_capacity_if_window_released': 1020},
        'active_register_values': {hex(0x800000+at): sorted(values) for at, values in registers.items()},
        'source_crop': {'raw_left': 96, 'raw_top': 16, 'width': 320, 'height': 224},
        'view_timing': 'Upcoming source18D0 publication from OLD WRAM F446/F448/F44A/F44C staging, not PRE1554 prior ports or logical camera. Every adjacent active snapshot verifies these values became actual source ports; final snapshot has no subsequent capture.',
        'next_capture_port_word_checks': next_port_checks,
        'priority': {'layer_control_values': sorted(registers[0x170]),
                     'max_visible_near_mixed_priority_subtiles': max(f['near_subtiles_mixed_priority'] for f in frames),
                     'max_visible_near_high_priority_subtiles': max(f['near_subtiles_high'] for f in frames),
                     'scope': 'Original pen-mask presence only; mixed8x8 cannot be collapsed to one Genesis priority flag without composition/overlap proof. Object-overlap and Scroll1 Window composition still separate.'},
        'conversion': metrics,
        'map_format': {'key': 'Exact32-bit code<<16|attribute, separately keyed by layer',
                       'reference': 'bits15..14 CRAM page; bits13..0 ROM8x8 pattern ID; row-major4/16 refs, original whole-tile flips baked',
                       'source_pen_masks': 'Original opaque pen presence for each8x8; source attribute bits7..8 retain priority group',
                       'physical_residency': 'Unassigned. No resource may overwrite reserved object tiles or silently evict a published background.'},
        'hashes': {str(path): sha(path) for path in paths},
        'outputs': {name: sha(out/name) for name in ['route_background.h', 'background-patterns.bin', 'source-palettes.bin', 'cram.bin', 'entries.json']}}
    (out/'frames.json').write_text(json.dumps(frames, indent=2)+'\n')
    report['outputs']['frames.json'] = sha(out/'frames.json')
    (out/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ['hashes', 'outputs', 'active_register_values']}, indent=2))


if __name__ == '__main__':
    main()
