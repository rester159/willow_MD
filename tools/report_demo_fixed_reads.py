"""Summarize sourceFixedByte arguments; this is traffic, not gameplay FPS."""
import argparse
import collections
import csv
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('prefix')
parser.add_argument('symbols', type=Path)
args = parser.parse_args()
rows = list(csv.DictReader(open(args.prefix + '-fixed-reads.csv')))
entry = next(int(line.split()[0], 16) for line in args.symbols.read_text().splitlines()
             if len(line.split()) >= 3 and line.split()[2] == 'sourceFixedByte')
hits = next(int(row['instructions']) for row in csv.DictReader(open(args.prefix + '-cycle-pcs.csv'))
            if int(row['pc'], 16) == entry)
assert len(rows) == hits, 'every compiled reader entry must have one captured argument'
frequency = collections.Counter(int(row['address'], 16) for row in rows)
assert all(0 <= at < 65536 for at in frequency)
print(f'{hits:,} sourceFixedByte calls verified against entry instruction count')
for first, count, label in [(0x9846, 128, 'player'), (0x8502, 3072, 'stage objects'),
                            (0xf042, 1024, 'palette controllers')]:
    calls = sum(n for at, n in frequency.items() if first <= at < first + count)
    print(f'{label}: {calls:,} calls ({calls / hits:.2%})')
print('Most frequent source addresses:')
for at, count in frequency.most_common(20):
    print(f'{at:04x}: {count:,}')
