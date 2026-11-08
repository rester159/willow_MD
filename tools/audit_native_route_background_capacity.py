#!/usr/bin/env python3
"""Read-only lossless flip reuse and original-pen priority-overlap capacity."""
from collections import Counter
from functools import lru_cache
from pathlib import Path
import argparse
import csv
import hashlib
import json
import struct
import zipfile
from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from audit_native_prepared_catalog import children
from prepare_native_route_background import ROOT, sha, physical_code, cell_index, read_capture, active, upcoming_ports


def unpack(pattern):
    return [[(pattern[y*4+x//2] >> (0 if x & 1 else 4)) & 15 for x in range(8)] for y in range(8)]


def flip(pattern, flags):
    pixels = unpack(pattern)
    if flags & 1:
        pixels = [row[::-1] for row in pixels]
    if flags & 2:
        pixels.reverse()
    return genesis_tiles(pixels)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, default=ROOT/'build/native-background-context/content-v2')
    parser.add_argument('--assets', type=Path, default=ROOT/'build/native-route-background-audit/prepared-v1')
    parser.add_argument('--objects', type=Path, default=ROOT/'build/native-route-objects/prepared-v2')
    parser.add_argument('--output', type=Path, default=ROOT/'build/native-route-background-audit/capacity-v1')
    a = parser.parse_args();out = a.output.resolve();out.mkdir(parents=True, exist_ok=True)
    source, assets, objects = a.capture.resolve(), a.assets.resolve(), a.objects.resolve()
    rows, get, paths = read_capture(source)
    manifest = json.loads((assets/'manifest.json').read_text())
    for name in ['background-patterns.bin', 'entries.json']:
        assert sha(assets/name) == manifest['outputs'][name]
    raw = (assets/'background-patterns.bin').read_bytes()
    patterns = [raw[at:at+32] for at in range(0, len(raw), 32)]
    descriptors = json.loads((assets/'entries.json').read_text())
    lookup = {(d['layer'], d['code'], d['attribute']): d for d in descriptors}
    canonical, indices, transformed = [], {}, []
    pixel_checks = 0
    for pattern in patterns:
        stored, flags = min((flip(pattern, flags), flags) for flags in range(4))
        if stored not in indices:
            indices[stored] = len(canonical);canonical.append(stored)
        transformed.append((indices[stored], flags))
        assert flip(stored, flags) == pattern
        pixel_checks += 64
    banks = (source/'captured-original-banks.bin').read_bytes()
    with zipfile.ZipFile(ROOT/'willow.zip') as z:
        gfx = assemble_region(z)
    object_manifest = json.loads((objects/'manifest.json').read_text())
    object_entries = {(e['code'], e['source_group']): e for e in object_manifest['entries']}

    @lru_cache(None)
    def pixels(layer, code):
        size = 16 if layer in (0, 2) else 32
        physical = code if layer == 0 else physical_code(layer, code)
        return decode_tile(gfx, physical, size) if physical is not None else [[15]*size for _ in range(size)]

    @lru_cache(None)
    def far_required(near, far, nx, ny, fx, fy):
        """Residency lower bound: retain far8x8 only if an actual visible pixel
        can show through an original transparent near pen. No RGB blending.
        This is NOT a runtime occlusion implementation or cache admission."""
        required = set();far_cells = {}
        for y in range(224):
            sy = (16+y+ny) & 1023
            source_far_y = (16+y+fy) & 2047
            x = 0
            while x < 320:
                sx = (96+x+nx) & 1023
                code, attr = struct.unpack_from('>2H', near, cell_index(2, sx//16, sy//16)*4)
                image = pixels(2, code);line = image[15-(sy & 15) if attr & 64 else sy & 15]
                run = min(16-(sx & 15), 320-x)
                for n in range(run):
                    pen = line[15-((sx+n) & 15) if attr & 32 else (sx+n) & 15]
                    if pen != 15:
                        continue
                    source_far_x = (96+x+n+fx) & 2047
                    key = source_far_x//8, source_far_y//8
                    if key not in far_cells:
                        code3, attr3 = struct.unpack_from('>2H', far, cell_index(3, source_far_x//32, source_far_y//32)*4)
                        d = lookup[3, code3, attr3]
                        subtile = (source_far_y & 31)//8*4+(source_far_x & 31)//8
                        old_id = d['references'][subtile] & 16383
                        far_cells[key] = transformed[old_id][0] if old_id else 0
                    if far_cells[key]:
                        required.add(far_cells[key])
                x += run
        return frozenset(required)

    frames, resident_sets, visible_union, overlap_patterns = [], [], set(), set()
    last = None;source_pixel_checks = 0;all_overlay_bytes = set()
    last_required = None;last_overlays = set()
    timing_path = ROOT/'build/native-background-context/timing-v1/upcoming-views.csv'
    timing = {int(r['capture']): r for r in csv.DictReader(timing_path.open())}
    actual_port_checks = 0
    for row in rows:
        if not active(row):
            continue
        rec = get(row);ports = upcoming_ports(rec);capture = int(row['capture'])
        for at, field in [(0x110, 's2x'), (0x112, 's2y'), (0x114, 's3x'), (0x116, 's3y')]:
            assert ports[at//2] == int(timing[capture][field+'_published'])
            actual_port_checks += 1
        assert ports[0x122//2] == 0x003e and ports[0x170//2] == 0x12f8
        visible = set();near_patterns = set()
        for layer, size, xr, yr in [(2, 16, 0x110, 0x112), (3, 32, 0x114, 0x116)]:
            extent = 64*size;xx = (96+ports[xr//2]) % extent;yy = (16+ports[yr//2]) % extent
            for y in range(yy//8, (yy+223)//8+1):
                for x in range(xx//8, (xx+319)//8+1):
                    tx, ty = (x*8) % extent, (y*8) % extent
                    code, attr = struct.unpack_from('>2H', rec[f'scroll{layer}'], cell_index(layer, tx//size, ty//size)*4)
                    d = lookup[layer, code, attr]
                    subtile = (ty % size)//8*(size//8)+(tx % size)//8
                    old_id = d['references'][subtile] & 16383
                    if old_id:
                        visible.add(transformed[old_id][0])
                        if layer == 2:
                            near_patterns.add(transformed[old_id][0])
        required_far = far_required(bytes(rec['scroll2']), bytes(rec['scroll3']),
                                    *(ports[at//2] for at in (0x110, 0x112, 0x114, 0x116)))
        visible_after_far_occlusion = near_patterns | required_far
        # Original source object geometry and pens; no native fitted pens decide
        # transparency or the CPS priority bitmap intersection.
        count = int(row['source_count']);at = (capture-1)*2048
        records = list(struct.iter_unpack('>4H', banks[at:at+count*8]))
        assert row['source_valid'] == row['native_valid'] == '1'
        assert row['source_count'] == row['native_count'] and row['source_epoch'] == row['native_epoch']
        opaque = set();actor_lines = [0]*224;actor_pixels = [0]*224;actor_count = 0
        for x, y, code, attr in records:
            for dx, dy, child in children(code, attr):
                sx, sy = ((x+dx) & 511)-96, ((y+dy) & 511)-16
                if sx <= -16 or sx >= 320 or sy <= -16 or sy >= 224:
                    continue
                image = pixels(0, child)
                if all(p == 15 for line in image for p in line):
                    assert object_entries[child, attr & 31]['status'] == 2
                    continue
                actor_count += 1
                for yy in range(max(0, sy), min(224, sy+16)):
                    actor_lines[yy] += 1;actor_pixels[yy] += 16
                    for xx in range(max(0, sx), min(320, sx+16)):
                        pen = image[15-(yy-sy) if attr & 64 else yy-sy][15-(xx-sx) if attr & 32 else xx-sx]
                        if pen != 15:
                            opaque.add((xx, yy))
        masks = [ports[at//2] & 32767 for at in (0x16e, 0x16c, 0x16a, 0x168)]
        overlaps = {};high_pixels = 0;object_tile_bits = Counter()
        for x, y in opaque:
            tx, ty = (96+x+ports[0x110//2]) & 1023, (16+y+ports[0x112//2]) & 1023
            code, attr = struct.unpack_from('>2H', rec['scroll2'], cell_index(2, tx//16, ty//16)*4)
            raw_tile = pixels(2, code)
            pen = raw_tile[15-(ty & 15) if attr & 64 else ty & 15][15-(tx & 15) if attr & 32 else tx & 15]
            source_pixel_checks += 1
            tile_key = tx//8, ty//8
            if pen == 15:
                continue
            high = bool(masks[(attr >> 7) & 3] & (1 << pen))
            object_tile_bits[tile_key] |= 1 if high else 2
            if high:
                high_pixels += 1
                sx, sy = x-(tx & 7), y-(ty & 7)
                overlaps[tile_key] = (sx, sy, code, attr, (ty & 15)//8*2+(tx & 15)//8)
        # Candidate only: one low-priority8x8 overlay before actors in SAT,
        # original high-mask pens retained and all others transparent. Window
        # remains high priority. Count full8px hardware fetch, even padding.
        total_lines = actor_lines[:];total_pixels = actor_pixels[:];frame_overlay = set()
        overlay_examples = []
        for sx, sy, code, attr, subtile in overlaps.values():
            d = lookup[2, code, attr];ref = d['references'][subtile]
            native = unpack(patterns[ref & 16383]);original = pixels(2, code)
            ox, oy = (subtile & 1)*8, (subtile//2)*8;mask = masks[(attr >> 7) & 3]
            high_only = []
            for y in range(8):
                line = []
                for x in range(8):
                    pen = original[15-(oy+y) if attr & 64 else oy+y][15-(ox+x) if attr & 32 else ox+x]
                    line.append(native[y][x] if pen != 15 and mask & (1 << pen) else 0)
                high_only.append(line)
            packed = genesis_tiles(high_only)
            stored = min(flip(packed, flags) for flags in range(4))
            frame_overlay.add(stored);all_overlay_bytes.add(stored)
            for yy in range(max(0, sy), min(224, sy+8)):
                total_lines[yy] += 1;total_pixels[yy] += 8
            overlay_examples.append({'x': sx, 'y': sy, 'code': code, 'attribute': attr,
                                     'subtile': subtile, 'page': ref >> 14})
        nonresident_overlays = frame_overlay-set(canonical)
        pair_overlay = nonresident_overlays | last_overlays
        required_pair = visible_after_far_occlusion | (last_required or set())
        visible_pair = visible | (last or set())
        visible_union.update(visible);overlap_patterns.update(nonresident_overlays)
        frames.append({'capture': capture, 'frame': int(row['frame']), 'canonical_visible_patterns': len(visible),
                       'new_canonical_patterns_vs_previous': len(visible-last) if last is not None else len(visible),
                       'opaque_object_pixels': len(opaque), 'source_high_pen_object_overlap_pixels': high_pixels,
                       'near_tiles_with_both_high_and_low_under_objects': sum(v == 3 for v in object_tile_bits.values()),
                       'actor_sat_entries': actor_count, 'high_overlay8x8_entries': len(overlaps),
                       'combined_sat_entries': actor_count+len(overlaps),
                       'combined_peak_line_sprites': max(total_lines), 'combined_peak_line_pixels': max(total_pixels),
                       'high_overlay_unique_patterns': len(frame_overlay), 'high_overlay_additional_patterns': len(nonresident_overlays),
                       'patterns_including_high_overlays': len(visible)+len(nonresident_overlays),
                       'near_patterns': len(near_patterns), 'far_patterns_reaching_visible_pixels': len(required_far),
                       'patterns_after_far_occlusion_lower_bound': len(visible_after_far_occlusion)+len(nonresident_overlays),
                       'previous_current_visible_union_including_overlays': len(visible_pair)+len(pair_overlay),
                       'previous_current_occlusion_union_lower_bound': len(required_pair)+len(pair_overlay),
                       'new_required_patterns_vs_previous': len(visible_after_far_occlusion-(last_required or set()))+len(nonresident_overlays-last_overlays),
                       'fits_sprite_limits': actor_count+len(overlaps) <= 80 and max(total_lines) <= 20 and max(total_pixels) <= 320,
                       'overlays': overlay_examples})
        resident_sets.append({'capture': capture, 'frame': int(row['frame']),
                              'visible_canonical_ids': sorted(visible),
                              'required_after_far_occlusion_canonical_ids': sorted(visible_after_far_occlusion),
                              'additional_overlay_patterns_hex': sorted(p.hex() for p in nonresident_overlays)})
        last = visible;last_required = visible_after_far_occlusion;last_overlays = nonresident_overlays
    summary = {'scope': 'Read-only exact8x8 H/V dedup and original-pen priority overlap on active content-v2 snapshots. No target allocation, live rendering or performance admission.',
               'all_patterns_before': len(patterns), 'all_canonical_patterns': len(canonical),
               'canonical_pattern_bytes': len(canonical)*32, 'canonical_roundtrip_pixel_checks': pixel_checks,
               'active_visible_canonical_union': len(visible_union),
               'peak_visible_canonical_patterns': max(f['canonical_visible_patterns'] for f in frames),
               'peak_new_visible_canonical_after_first': max(f['new_canonical_patterns_vs_previous'] for f in frames[1:]),
               'original_object_background_pixel_checks': source_pixel_checks,
               'frames_with_actual_high_pen_overlap': sum(f['source_high_pen_object_overlap_pixels'] > 0 for f in frames),
               'max_high_pen_overlap_pixels': max(f['source_high_pen_object_overlap_pixels'] for f in frames),
               'max_tiles_needing_mixed_priority_under_objects': max(f['near_tiles_with_both_high_and_low_under_objects'] for f in frames),
               'high_overlay_pattern_union': len(all_overlay_bytes), 'additional_high_overlay_pattern_union': len(overlap_patterns),
               'peak_overlay8x8_entries': max(f['high_overlay8x8_entries'] for f in frames),
               'peak_combined_sat_entries': max(f['combined_sat_entries'] for f in frames),
               'peak_combined_line_sprites': max(f['combined_peak_line_sprites'] for f in frames),
               'peak_combined_line_pixels': max(f['combined_peak_line_pixels'] for f in frames),
               'peak_patterns_including_overlays': max(f['patterns_including_high_overlays'] for f in frames),
               'peak_patterns_after_far_occlusion_lower_bound': max(f['patterns_after_far_occlusion_lower_bound'] for f in frames),
               'peak_previous_current_visible_union_including_overlays': max(f['previous_current_visible_union_including_overlays'] for f in frames),
               'peak_previous_current_occlusion_union_lower_bound': max(f['previous_current_occlusion_union_lower_bound'] for f in frames),
               'peak_new_required_after_first': max(f['new_required_patterns_vs_previous'] for f in frames[1:]),
               'actual_upcoming_source_port_word_checks': actual_port_checks,
               'far_occlusion_scope': 'Original transparent near pens determine far visibility for the upcoming staged view. Previous/current required-pattern union includes both consecutive displayed views. This remains a capacity lower bound only; dynamic preparation, guard columns, bounded write scheduling and Scroll1 replacement regions remain unimplemented.',
               'frames_exceeding_overlay_sprite_limits': sum(not f['fits_sprite_limits'] for f in frames),
               'overlay_limits': 'Single8x8 overlays are an unimplemented capacity candidate; requires source/VDP pixel composition proof, current view/mask/objects lease, preprepared pattern binding and placement ahead of actors but below Scroll1. Scroll1/HUD sprites excluded.',
               'hashes': {str(p): sha(p) for p in [Path(__file__).resolve(), ROOT/'tools/prepare_native_route_background.py', ROOT/'tools/audit_native_prepared_catalog.py', ROOT/'tools/cps1_gfx.py', timing_path, *paths, source/'captured-original-banks.bin', objects/'manifest.json', assets/'manifest.json', assets/'entries.json', assets/'background-patterns.bin']}}
    (out/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    (out/'frames.json').write_text(json.dumps(frames, indent=2)+'\n')
    (out/'resident-sets.json').write_text(json.dumps(resident_sets, separators=(',', ':'))+'\n')
    (out/'canonical-patterns.bin').write_bytes(b''.join(canonical))
    (out/'canonical-map.json').write_text(json.dumps(transformed)+'\n')
    print(json.dumps({k: v for k, v in summary.items() if k != 'hashes'}, indent=2))


if __name__ == '__main__':
    main()
