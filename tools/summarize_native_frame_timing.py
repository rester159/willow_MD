#!/usr/bin/env python3
"""Separate active-gameplay CPU timing from reset/intro waits in probe CSVs."""
import argparse
import collections
import csv
import json
from pathlib import Path

MASTER_HZ = 53_693_175
FRAME_CYCLES = 896_040


def summarize(rows):
    cycles = [int(row['master_cycles']) for row in rows]
    if not cycles:
        return {'updates': 0}
    mean = sum(cycles) / len(cycles)
    return {
        'updates': len(cycles),
        'mean_cpu_ms': mean * 1000 / MASTER_HZ,
        'peak_cpu_ms': max(cycles) * 1000 / MASTER_HZ,
        'over_frame_budget': sum(value > FRAME_CYCLES for value in cycles),
        'cpu_only_updates_per_second': MASTER_HZ / mean if mean else None,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    parser.add_argument('--tail', type=int, help='Use only the last N updates')
    parser.add_argument('--output', type=Path, help='Also save the JSON summary')
    args = parser.parse_args()
    if args.tail is not None and args.tail < 1:
        parser.error('--tail must be positive')
    with args.csv.open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    if args.tail is not None:
        if len(rows) < args.tail:
            parser.error('CSV contains fewer updates than --tail requested')
        rows = rows[-args.tail:]
    groups = collections.defaultdict(list)
    for row in rows:
        groups[(int(row['manager']), int(row['scene']))].append(row)
    result = {
        'source': str(args.csv.resolve()),
        'scope': ('Headless CPU cost only; excludes display/audio integration. '
                  'States are sampled after each update. The mixed average '
                  'includes reset/intro/wait states and is not gameplay FPS.'),
        'master_clock_hz': MASTER_HZ,
        'frame_budget_master_cycles': FRAME_CYCLES,
        'mixed_states': summarize(rows),
        'active_gameplay_manager2_scene2': summarize(groups.get((2, 2), [])),
        'by_state': {f'{manager}/{scene}': summarize(values)
                     for (manager, scene), values in sorted(groups.items())},
    }
    rendered = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(rendered)
    print(rendered, end='')


if __name__ == '__main__':
    main()
