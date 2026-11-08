"""Audit guarded SRAM reservations and a proposed source-offset actor placement.

This proves interval coverage only. It does not relocate records, validate
hardware SRAM performance, or admit the layout to the active scheduler.
"""
import argparse
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--json', type=Path)
a = p.parse_args()
header = (root / 'src/willow_native_sram_layout.h').read_text()
values = {name: int(value, 0) for name, value in re.findall(
    r'^#define WILLOW_SRAM_(\w+) (0x[0-9a-f]+|\d+)UL$', header, re.M)}
reservations = []
for name, first in values.items():
    if name.endswith('_SIZE') or name == 'ACTOR_CANDIDATE':
        continue
    reservations.append(dict(name=name, first=first, end=first + values[name + '_SIZE']))
reservations.sort(key=lambda row: row['first'])
for row in reservations:
    assert 0x200000 <= row['first'] < row['end'] <= 0x210000, row
for left, right in zip(reservations, reservations[1:]):
    assert left['end'] <= right['first'], (left, right)
# Catch newly added absolute SRAM addresses that bypass the reservation map.
# Current expressions may also use named bases plus offsets; target compile-time
# sizeof assertions bound their structures, with explicit scalar slots above.
literal_count = 0
for path in sorted((root / 'native/intro_probe/src').rglob('*')):
    if path.suffix not in ('.c', '.h'):
        continue
    for literal in re.findall(r'\b0x20[0-9a-fA-F]{4}(?![0-9a-fA-F])', path.read_text()):
        at = int(literal, 16)
        assert any(r['first'] <= at < r['end'] for r in reservations), (str(path), literal)
        literal_count += 1
specs = [(int(first, 16), int(capacity), int(stride)) for first, capacity, stride in
         re.findall(r'\{(0x[0-9a-f]+),(\d+),(\d+)\}',
                    (root / 'src/willow_arcade_actor_pool.h').read_text())]
assert len(specs) == 9
actors = []
for i, (source, capacity, stride) in enumerate(specs[:8]):
    first = 0x200000 + source
    end = first + capacity * stride
    assert all(end <= r['first'] or first >= r['end'] for r in reservations), i
    actors.append(dict(class_id=(i + 1) * 2, source=source, first=first, end=end,
                       bytes=capacity * stride))
assert actors[0]['first'] == values['ACTOR_CANDIDATE']
assert sum(r['bytes'] for r in actors) == values['ACTOR_CANDIDATE_SIZE']
for left, right in zip(actors, actors[1:]):
    assert left['end'] == right['first'], (left, right)
assert actors[-1]['end'] <= 0x210000
occupied = sum(r['end'] - r['first'] for r in reservations)
for r in reservations:
    print(f"{r['first']:06x}..{r['end']:06x} {r['end']-r['first']:5,d} {r['name']}")
print(f'{occupied:,} reserved bytes; {65536-occupied:,} unreserved bytes before migration')
print(f'{literal_count} remaining absolute SRAM references fall in declared reservations')
for r in actors:
    print(f"Proposed class {r['class_id']:2}: {r['first']:06x}..{r['end']:06x} ({r['bytes']:,} bytes)")
print('All eight proposed actor payloads fit without moving any current SRAM owner.')
print('Actor placement is opt-in via WILLOW_SRAM_ACTOR_PROBE. Direct lookup,')
print('full source-trace parity and target-cycle evidence determine admission.')
if a.json:
    a.json.write_text(json.dumps(dict(reservations=reservations, proposed_actors=actors,
                                     reserved_bytes=occupied, experimental_flag="WILLOW_SRAM_ACTOR_PROBE"), indent=2)+'\n')
