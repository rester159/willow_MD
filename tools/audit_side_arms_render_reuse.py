#!/usr/bin/env python3
"""Read-only Side Arms source inventory and bounded Willow reuse opportunity.

Writes evidence in Willow only. Does not build/run/change Side Arms, emulate a
target, or claim CPU savings. SAT words must reproduce the pinned replay oracle.
"""
from pathlib import Path
import argparse
import collections
import csv
import hashlib
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def oracle(records, entries):
    # Reproduce the frozen native replay's independent source-derived geometry.
    # Exact comparison with its complete expected-word stream below catches
    # drift; this audit does not newly admit source renderer behavior.
    sat, counts, pixels = [], [0] * 224, [0] * 224
    for x, y, code, attr in records:
        if code > 0x27ff or attr & 0xff00 == 0xff00:
            continue
        columns, rows = ((attr >> 8) & 15) + 1, (attr >> 12) + 1
        for row in range(rows):
            for column in range(columns):
                cx = columns - 1 - column if attr & 32 else column
                cy = rows - 1 - row if attr & 64 else row
                child = (code & 0xfff0) + ((code + cx) & 15) + cy * 16
                sx, sy = ((x + column * 16) & 511) - 96, ((y + row * 16) & 511) - 16
                if sx <= -16 or sx >= 320 or sy <= -16 or sy >= 224:
                    continue
                entry = entries[child, attr & 31]
                if entry['status'] == 2:
                    continue
                sat.append([sy + 128, 0x500, entry['attribute'] | ((attr << 6) & 0x1800), sx + 128])
                for line in range(max(0, sy), min(224, sy + 16)):
                    counts[line] += 1
                    pixels[line] += 16
    assert len(sat) <= 80 and max(counts) <= 20 and max(pixels) <= 320
    for n, piece in enumerate(sat):
        piece[1] |= n + 1 if n + 1 < len(sat) else 0
    expected = [len(sat)] + [v for piece in sat for v in piece] + [v for pair in zip(counts, pixels) for v in pair]
    return sat, expected


def compare(current, previous):
    if previous is None:
        return {"cold": 1}
    records, sat, expected = current
    old_records, old_sat, old_expected = previous
    return {
        "transitions": 1,
        "identical_all_source_records": int(records == old_records),
        "identical_ordered_code_attribute": int([r[2:] for r in records] == [r[2:] for r in old_records]),
        "identical_sat_and_line_budget": int(expected == old_expected),
        "shared_ordinals": min(len(records), len(old_records)),
        "same_template_at_shared_ordinal": sum(a[2:] == b[2:] for a, b in zip(records, old_records)),
        "same_full_record_at_shared_ordinal": sum(a == b for a, b in zip(records, old_records)),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/side-arms-render-audit/v1')
    a = parser.parse_args()
    side = ROOT.parent/'side arms'
    context = ROOT/'build/native-object-publication/checkpoint-visual-state-v1'
    assets = ROOT/'build/native-route-objects/prepared-v1'
    replay = ROOT/'build/native-route-objects/replay-v1'
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    checkpoint = json.loads((context/'checkpoint.json').read_text())
    for name in ['visual-contexts.csv', 'visual-contexts.bin', 'captured-original-banks.bin']:
        assert sha(context/name) == checkpoint['files'][name], name
    manifest = json.loads((assets/'manifest.json').read_text())
    for name, digest in manifest['outputs'].items():
        assert sha(assets/name) == digest, name
    replay_evidence = json.loads((replay/'evidence.json').read_text())
    replay_trace = replay/'route_trace.h'
    assert sha(replay_trace) == replay_evidence['hashes'][str(replay_trace)]
    replay_log = replay/'capture.log'
    assert sha(replay_log) == replay_evidence['hashes'][str(replay_log)]
    costs = [float(line.split()[3]) for line in replay_log.read_text().splitlines() if line.startswith('COST ')]
    pinned_words = [int(n) for n in re.search(r'routeExpected\[\]=\{([^}]+)\}', replay_trace.read_text())[1].split(',')]
    rows = list(csv.DictReader((context/'visual-contexts.csv').open()))
    raw = (context/'captured-original-banks.bin').read_bytes()
    entries = {(e['code'], e['source_group']): e for e in manifest['entries']}
    expected_palette = (assets/'source-palettes.bin').read_bytes()
    adjacent = collections.Counter()
    per_bank = collections.Counter()
    either_bank = collections.Counter()
    words = []
    details = []
    old = None
    by_bank = {}
    # Model only the known SAT words at the admitted publication boundary;
    # trailing entries are irrelevant only because every final link is zero.
    sat_memory = [0] * 320
    full_words = changed_words = changed_entries = changed_runs = 0
    source_epochs = set()
    with (context/'visual-contexts.bin').open('rb') as visual:
        for row in rows:
            if row['manager'] != '2' or row['scene'] != '2':
                continue
            assert row['source_valid'] == row['native_valid'] == '1'
            assert row['source_epoch'] == row['native_epoch'] and row['source_count'] == row['native_count']
            capture, count, bank = int(row['capture']), int(row['source_count']), int(row['bank'])
            assert 0 <= count <= 8 and bank in (0, 1)
            records = list(struct.iter_unpack('>4H', raw[(capture-1)*2048:capture*2048]))
            assert all(r[0] == r[3] == 0 for r in records[count:])
            records = records[:count]
            visual.seek((capture-1)*90624)
            assert visual.read(6144) == expected_palette
            ports = visual.read(512)
            assert int.from_bytes(ports[0x10a:0x10c], 'big') == 0x91c0
            assert int.from_bytes(ports[0x122:0x124], 'big') == 0x003e
            assert int.from_bytes(ports[0x100:0x102], 'big') == int(row['prior_selector'], 16) == 0x9000 + bank*0x80
            sat, expected = oracle(records, entries)
            assert not sat or (sat[-1][1] & 127) == 0
            current = records, sat, expected
            ac, bc = compare(current, old), compare(current, by_bank.get(bank))
            adjacent.update(ac)
            per_bank.update(bc)
            own = by_bank.get(bank)
            other = by_bank.get(1-bank)
            own_hit = own is not None and records == own[0]
            other_hit = other is not None and records == other[0]
            either_bank.update({"acquisitions": 1, "own_full_source_hit": int(own_hit),
                                "other_full_source_hit": int(other_hit), "either_full_source_hit": int(own_hit or other_hit),
                                "other_only_hit": int(other_hit and not own_hit),
                                "either_exact_sat_budget_hit": int(any(expected == v[2] for v in by_bank.values()))})
            old = current
            by_bank[bank] = current
            words.extend(expected)
            flat = [v for piece in sat for v in piece] if sat else [0] * 4
            changed = [v != sat_memory[n] for n, v in enumerate(flat)]
            n_changed = sum(changed)
            n_entries = sum(any(changed[n:n+4]) for n in range(0, len(flat), 4))
            n_runs = sum(v and (n == 0 or not changed[n-1]) for n, v in enumerate(changed))
            full_words += len(flat)
            changed_words += n_changed
            changed_entries += n_entries
            changed_runs += n_runs
            sat_memory[:len(flat)] = flat
            source_epochs.add((bank, int(row['source_epoch'])))
            details.append({"capture": capture, "bank": bank, "epoch": int(row['source_epoch']), "records": count,
                            "pieces": len(sat), "adjacent": ac, "same_bank": bc,
                            "own_full_source_hit": own_hit, "other_full_source_hit": other_hit,
                            "sat_words": len(flat), "changed_words": n_changed, "changed_entries": n_entries,
                            "changed_word_runs": n_runs})
    assert len(details) == 464 and words == pinned_words
    assert len(costs) == len(details)
    source_names = ['README.md', 'docs/architecture.md', 'docs/qa.md', 'docs/multiplayer-qa.md',
                    'src/sprites.c', 'src/bg.c', 'src/bc_actor_draw.c', 'src/bc_actor_art.c',
                    'tools/build_assets.py', 'tools/qa_bc_actor_cache.py',
                    'tools/qa_sprite_emit_native.py', 'tools/qa_sprite_hash_native.py',
                    '.local/bc-art-cache-tag/verified.json']
    paths = [Path(__file__), ROOT/'tools/verify_native_route_objects.py',
             context/'checkpoint.json', context/'visual-contexts.csv', context/'visual-contexts.bin',
             context/'captured-original-banks.bin', assets/'manifest.json', replay_trace, replay_log, replay/'evidence.json']
    paths.extend(side/n for n in source_names)
    report = {
        "scope": "Read-only source review plus offline reuse statistics on 464 frozen active original-source acquisitions. No new target timing, live renderer, hardware transfer test or FPS result.",
        "active_acquisitions": len(details), "distinct_selected_bank_epochs": len(source_epochs),
        "all_pinned_replay_sat_and_budget_words_exact": len(words),
        "adjacent_acquisitions": dict(adjacent), "previous_acquisition_of_same_bank": dict(per_bank),
        "either_most_recent_bank_cache": dict(either_bank),
        "generous_replay_time_elimination_upper_bound_ms": {
            "own_bank_mean_over_all_acquisitions": sum(c for c, d in zip(costs, details) if d['own_full_source_hit'])/len(costs),
            "either_bank_mean_over_all_acquisitions": sum(c for c, d in zip(costs, details) if d['own_full_source_hit'] or d['other_full_source_hit'])/len(costs),
            "maximum_remaining_miss_cost": max(c for c, d in zip(costs, details) if not (d['own_full_source_hit'] or d['other_full_source_hit'])),
            "note": "Deliberately generous bound setting the old replay's entire selection/build/publish cost to zero on hits. Actual comparison/copy/upload work remains. Display-off isolated replay is not continuous producer timing, a measured retained implementation, or a bound on general gameplay."
        },
        "theoretical_sat_word_updates": {"full_upload_words": full_words, "changed_words": changed_words,
            "changed_entries": changed_entries, "changed_word_runs": changed_runs,
            "note": "Requires persistent correct hardware shadow, guarded count/link handling, and owned SAT VRAM. Command/comparison CPU cost excluded; not a measured win. First frame assumes SAT VRAM zero from cold boot."},
        "side_arms_tests_executed": False,
        "hashes": {str(p): sha(p) for p in paths},
    }
    (out/'evidence.json').write_text(json.dumps(report, indent=2)+'\n')
    (out/'captures.json').write_text(json.dumps(details, indent=2)+'\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'hashes'}, indent=2))


if __name__ == '__main__':
    main()
