#!/usr/bin/env python3
"""Compare full headless frames from the controlled source-layer hide probe."""
from pathlib import Path

def read(name):
    parts = (Path('build')/f'source-layer-{name}.ppm').read_bytes().split(b'\n',3)
    assert parts[:3] == [b'P6', b'320 224', b'255'], 'Unexpected capture format'
    assert len(parts[3]) == 320*224*3
    return parts[3]

before, hidden, restored = map(read, ('before','hidden','restored'))
assert before == restored, 'Restoration changed rendered pixels'
visible = changed = 0
for at in range(0,len(before),3):
    a,b = before[at:at+3],hidden[at:at+3]
    changed += a != b
    if b != b'\0\0\0':
        visible += 1
        assert a == b, f'Object color/position changed at pixel {at//3}'
assert visible and changed
print(f'320x224 restored frame exact; {visible} nonblack object pixels unchanged; {changed} background/text pixels removed')
