"""Report the optional CPU-only headless service profile (never playback FPS)."""
import argparse
import bisect
import collections
import csv
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('prefix', help='WILLOW_CYCLE_PROFILE_PREFIX used for the completed run')
parser.add_argument('symbols', type=Path, help='exact ROM symbol.txt for that run')
parser.add_argument('--updates', type=int, default=120, help='Expected number of complete timed updates')
args = parser.parse_args()
assert 1 <= args.updates <= 900
updates = args.updates
rows = list(csv.DictReader(open(args.prefix + '-cycles.csv')))
assert len(rows) == updates and [int(r['update']) for r in rows] == list(range(1, updates + 1))
cycles = [int(r['master_cycles']) for r in rows]
instructions = sum(int(r['instructions']) for r in rows)
assert all(c > 0 for c in cycles)
symbols = []
for line in args.symbols.read_text().splitlines():
    parts = line.split()
    if len(parts) >= 3 and parts[1].lower() == 't':
        symbols.append((int(parts[0], 16), parts[2]))
symbols.sort()
addresses = [s[0] for s in symbols]
costs = collections.Counter()
hits = collections.Counter()
for row in csv.DictReader(open(args.prefix + '-cycle-pcs.csv')):
    pc = int(row['pc'], 16)
    index = bisect.bisect_right(addresses, pc) - 1
    assert index >= 0
    name = symbols[index][1]
    costs[name] += int(row['master_cycles'])
    hits[name] += int(row['instructions'])
assert sum(costs.values()) <= sum(cycles)
assert sum(hits.values()) + updates == instructions, 'one injected JSR per update must be the only non-ROM instruction'
# Core system.h: NTSC master 53693175 Hz. NOP calibration establishes /7 CPU.
clock = 53693175
print('CPU-only compiled demo services; concurrent VDP/Z80/IRQ work excluded.')
print(f'{updates} updates: {sum(cycles):,} master cycles / {instructions:,} instructions')
print(f'Mean {sum(cycles)/updates/7:,.1f} CPU cycles, {sum(cycles)/updates/clock*1000:.3f} ms at NTSC clock')
print(f'Peak {max(cycles)/7:,.1f} CPU cycles, {max(cycles)/clock*1000:.3f} ms at NTSC clock')
print(f'CPU-only mean/peak versus 60 Hz budget: {sum(cycles)/updates/clock*60:.2f}x / {max(cycles)/clock*60:.2f}x')
print('Hottest compiled functions (includes core refresh delays, excludes injected JSR):')
for name, cost in costs.most_common(15):
    print(f'{cost/sum(cycles):6.2%} {cost:12,d} master cycles {hits[name]:10,d} instructions {name}')
