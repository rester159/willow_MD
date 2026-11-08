#!/usr/bin/env python3
"""Audit source-produced topology changes without assuming a frozen sprite list.

Reads a pinned source/native publication trace and lossless original-pen atlas.
It emits evidence only: no palette assignment, VDP allocation, or live hook.
"""
from pathlib import Path
import argparse, collections, csv, hashlib, json, struct, zipfile
from cps1_gfx import assemble_region
from native_object_tiles import verify as verify_original_pens
ROOT = Path(__file__).resolve().parents[1]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

def children(code, attribute):
    """Original row-major display order and blocked-code/local-flip rule."""
    if code > 0x27ff or attribute & 0xff00 == 0xff00:
        return []
    columns, rows = ((attribute >> 8) & 15) + 1, (attribute >> 12) + 1
    result = []
    for row in range(rows):
        for column in range(columns):
            cx = columns - 1 - column if attribute & 32 else column
            cy = rows - 1 - row if attribute & 64 else row
            child = (code & 0xfff0) + ((code + cx) & 15) + cy * 16
            result.append((column * 16, row * 16, child))
    return result

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture', type=Path, default=ROOT/'build/native-object-publication/checkpoint-metadata-v1')
    p.add_argument('--atlas', type=Path, default=ROOT/'build/cartridge-gap-audit/full-object-capacity/full_object_tiles.json')
    p.add_argument('--archive', type=Path, default=ROOT/'willow.zip')
    p.add_argument('--asset-binding', type=Path, default=ROOT/'build/cartridge-gap-audit/frame1200_window_v1/asset-binding.json')
    p.add_argument('--output', type=Path, default=ROOT/'build/cartridge-gap-audit/dynamic-template-catalog-v1')
    a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    source = a.capture.resolve(); checkpoint = json.loads((source/'checkpoint.json').read_text())
    files = ['captured-original-banks.bin', 'publication-latches.csv', 'metadata-acquisitions.csv', 'producer-epochs.csv', 'evidence.json']
    for name in files:
        assert sha(source/name) == checkpoint['files'][name], name
    raw = (source/files[0]).read_bytes()
    captures = list(csv.DictReader((source/files[1]).open()))
    acquisitions = list(csv.DictReader((source/files[2]).open()))
    assert len(raw) == len(captures)*2048 and len(captures) == len(acquisitions)
    atlas = json.loads(a.atlas.read_text()); blocks = dict(atlas['entries'])
    binary = a.atlas.with_suffix('.bin'); assert sha(binary) == atlas['binary_sha256']
    patterns = binary.read_bytes(); assert patterns[:128] == bytes(128)
    binding = json.loads(a.asset_binding.read_text())
    frozen_assets = {(x['code'], x['source_group']): x for x in binding['entries']}
    active = []; all_complete_templates = set(); rejected = 0
    for row, acquired in zip(captures, acquisitions):
        capture = int(row['capture']); count = int(row['count'])
        assert capture == int(acquired['capture'])
        records = list(struct.iter_unpack('>4H', raw[(capture-1)*2048:capture*2048]))
        valid = row['valid_complete_epoch'] == '1'
        if not valid:
            rejected += 1
        else:
            assert acquired['native_valid'] == '1' and count == int(acquired['native_count'])
            # Cleanup only guarantees X/attribute zero; code/Y may retain history.
            assert all(rec[0] == rec[3] == 0 for rec in records[count:])
            all_complete_templates.update((r[2], r[3]) for r in records[:count])
        if row['manager'] == row['scene'] == '2':
            assert valid
            active.append((row, records[:count]))
    template_records = collections.defaultdict(list)
    transitions = []; previous = None; changed = collections.Counter()
    code_pairs = set(); code_set = set(); groups = set(); layouts = set(); frame_requirements = []
    for row, records in active:
        capture = int(row['capture'])
        topology = tuple((ordinal, r[2], r[3]) for ordinal, r in enumerate(records))
        layouts.add(topology)
        if previous is not None:
            prevrow, prevrecords = previous
            prior = tuple((ordinal, r[2], r[3]) for ordinal, r in enumerate(prevrecords))
            kind = 'unchanged_records' if records == prevrecords else 'origin_only' if topology == prior else 'template_or_order_change'
            changed[kind] += 1
            max_records = max(len(records), len(prevrecords))
            changed_slots = [n for n in range(max_records) if n >= len(records) or n >= len(prevrecords) or records[n][2:] != prevrecords[n][2:]]
            transitions.append({'previous_capture': int(prevrow['capture']), 'capture': capture, 'frame': int(row['frame']), 'bank': int(row['selected_bank']), 'epoch': int(row['producer_epoch']), 'kind': kind, 'changed_template_ordinals': changed_slots})
        previous = row, records
        live_blocks = set(); live_pairs = set(); cells = 0; footprint = 0
        for ordinal, (_, _, code, attribute) in enumerate(records):
            template_records[(code, attribute)].append((capture, ordinal))
            groups.add(attribute & 31)
            for dx, dy, child in children(code, attribute):
                assert child in blocks, child
                code_set.add(child); code_pairs.add((child, attribute & 31)); footprint += 1
                if blocks[child]:
                    live_blocks.add(blocks[child]); live_pairs.add((child, attribute & 31)); cells += 1
        frame_requirements.append({'capture': capture, 'source_records': len(records), 'complete_template_children_including_transparent': footprint, 'complete_nontransparent_cells': cells, 'lossless_pen_blocks': len(live_blocks), 'lossless_pen_tiles': len(live_blocks)*4, 'lossless_pen_bytes': len(live_blocks)*128, 'nontransparent_code_group_pairs': len(live_pairs), 'block_ids': sorted(live_blocks)})
    templates = []
    for (code, attribute), uses in sorted(template_records.items()):
        cells = []; missing = []; blank = 0
        for dx, dy, child in children(code, attribute):
            block = blocks[child]
            cells.append({'offset': (dy << 8)|dx, 'code': child, 'source_group': attribute & 31, 'lossless_pen_block': block, 'transparent': block == 0})
            if not block: blank += 1
            if (child, attribute & 31) not in frozen_assets: missing.append(child)
        templates.append({'source_code': code, 'source_attribute': attribute, 'columns': ((attribute>>8)&15)+1, 'rows': (attribute>>12)+1, 'source_group': attribute & 31, 'occurrences': len(uses), 'first_capture': uses[0][0], 'last_capture': uses[-1][0], 'ordinals': sorted({u[1] for u in uses}), 'transparent_children': blank, 'native_nontransparent_cells': len(cells)-blank, 'unresolved_in_frozen1200_binding': sorted(set(missing)), 'children': cells})
    assert sha(a.archive) == atlas['archive_sha256']
    with zipfile.ZipFile(a.archive) as archive:
        gfx = assemble_region(archive)
    verified_pixels = verify_original_pens(gfx, [(code, blocks[code]) for code in sorted(code_set)], patterns)
    nonblank_blocks = {blocks[c] for c in code_set if blocks[c]}
    missing = sorted(pair for pair in code_pairs if pair not in frozen_assets)
    missing_nonblank = [pair for pair in missing if blocks[pair[0]]]
    missing_by_group = collections.Counter(group for code, group in missing_nonblank)
    replacements = []; previous_blocks = set()
    for frame in frame_requirements:
        current = set(frame['block_ids']); additions = current - previous_blocks
        replacements.append({'capture': frame['capture'], 'new_lossless_blocks_vs_previous': len(additions), 'new_lossless_bytes_vs_previous': len(additions)*128})
        previous_blocks = current
    nonblank_cells = sum(t['native_nontransparent_cells'] for t in templates)
    summary = {
        'scope': 'Read-only topology/catalog requirements from the pinned463-active-capture source route; not full-game coverage, a palette fit, resident allocation, live renderer or FPS admission.',
        'capture_count': len(captures), 'active_captures': len(active), 'unproved_metadata_fallback_captures': rejected,
        'active_record_count_histogram': dict(sorted(collections.Counter(len(r) for _, r in active).items())),
        'active_templates_code_full_attribute': len(templates), 'active_unique_ordered_topologies': len(layouts),
        'active_adjacent_transitions': dict(changed), 'all_metadata_complete_templates_code_full_attribute': len(all_complete_templates),
        'original_decoded_pixels_verified': verified_pixels, 'active_source_groups': sorted(groups), 'active_child_codes': len(code_set), 'active_code_group_pairs': len(code_pairs),
        'lossless_unique_nontransparent_pen_blocks': len(nonblank_blocks), 'lossless_pen_union_bytes': len(nonblank_blocks)*128,
        'unfiltered_peak_nontransparent_cells': max(f['complete_nontransparent_cells'] for f in frame_requirements),
        'unfiltered_peak_lossless_pen_tiles': max(f['lossless_pen_tiles'] for f in frame_requirements),
        'unfiltered_peak_lossless_pen_bytes': max(f['lossless_pen_bytes'] for f in frame_requirements),
        'peak_new_pen_bytes_vs_immediate_previous_including_initial': max(f['new_lossless_bytes_vs_previous'] for f in replacements),
        'peak_new_pen_bytes_after_initial': max(f['new_lossless_bytes_vs_previous'] for f in replacements[1:]),
        'frozen1200_missing_code_group_pairs': len(missing), 'frozen1200_missing_nontransparent_code_group_pairs': len(missing_nonblank),
        'frozen1200_missing_nontransparent_pairs_by_group': dict(sorted(missing_by_group.items())),
        'template_nontransparent_cell_sum': nonblank_cells, 'resolved16_cell_bytes_if_all_templates_resident': nonblank_cells*4,
        'resolved16_descriptor_bytes_if_all_templates_resident': len(templates)*10,
        'max_instance_bytes_68000': max(len(r) for _, r in active)*10,
        'presentation_epochs_available': {'latch': True, 'resident': False, 'palette': False, 'view': False},
        'pattern_count_limitations': 'Pen-only numbers use original unflipped15-visible-pen resources. Palette-specific remaps and grouped atlases can split/duplicate blocks. No source camera/global-flip or crop culling was used, so resource maxima are conservative complete-template demands. Complete transparent tiles are omitted only from exact original pen data.',
        'hashes': {str(path.resolve()): sha(path) for path in [Path(__file__), source/'checkpoint.json', *(source/f for f in files), a.atlas, binary, a.asset_binding, a.archive, ROOT/'tools/native_object_tiles.py', ROOT/'tools/cps1_gfx.py']},
    }
    (out/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    (out/'template-catalog.json').write_text(json.dumps({'scope': summary['scope'], 'templates': templates}, indent=2)+'\n')
    (out/'active-transitions.json').write_text(json.dumps(transitions, indent=2)+'\n')
    (out/'frame-requirements.json').write_text(json.dumps(frame_requirements, indent=2)+'\n')
    (out/'adjacent-pattern-additions.json').write_text(json.dumps(replacements, indent=2)+'\n')
    (out/'codes.json').write_text(json.dumps(sorted(code_set))+'\n')
    (out/'missing-frozen1200-assets.json').write_text(json.dumps({'code_group_pairs': missing, 'nontransparent_code_group_pairs': missing_nonblank}, indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k!='hashes'}, indent=2))
if __name__ == '__main__': main()
