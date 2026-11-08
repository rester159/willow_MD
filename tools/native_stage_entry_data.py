#!/usr/bin/env python3
"""Predecode immutable Stage 0 spawn descriptors from original ROM data."""
from pathlib import Path
from cps1_program import assemble_maincpu
p = assemble_maincpu(Path('willow.zip'))
def word(a):
    return int.from_bytes(p[a:a+2], 'big')
count = word(0x40470) + 1
assert count == 57
lines = ['/* Generated from original ROM data; layout is WillowStageSpawnEntry. */',
         'static const WillowStageSpawnEntry entryDescriptors[]={']
for a in range(0x40472, 0x40472+count*8, 8):
    variant, selector, x, y = (word(a+n) for n in (0, 2, 4, 6))
    filter_byte = p[0x403f6+(variant>>9)]
    coordinates = int(filter_byte == 0 or (filter_byte == 2 and p[0x40418+(selector>>8)] != 0))
    lines.append('{'+','.join(map(str, (variant, selector, x, y, coordinates)))+'},')
lines.append('};')
Path('native/intro_probe/src/entry_data.h').write_text('\n'.join(lines)+'\n')
print(f'Native stage entry: {count} immutable source descriptors in ROM')
