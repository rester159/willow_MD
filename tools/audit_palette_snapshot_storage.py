#!/usr/bin/env python3
"""Representation lower bound for existing captured colors, not replay ownership."""
from pathlib import Path
import hashlib
import json
import struct

rom = Path('build/reference_maincpu.bin').read_bytes()
trace_path = Path('build/arcade-trace/player-colors/frames.bin')
trace = trace_path.read_bytes()
stride = 4 + 1026 + 128 + 6144
assert len(trace) == 1151 * stride
words = struct.unpack('>65536H', rom[0xe0000:0x100000])
by_rgb = {}
for at in range(len(words) - 15):
    group = words[at:at + 16]
    rgb = tuple(w & 4095 for w in group)
    brightness = tuple(w >> 12 for w in group)
    # Same RGB and relative brightness signature have identical expressive
    # power under a modulo-16 offset; retain one representative only.
    shape = tuple((v - brightness[0]) & 15 for v in brightness)
    by_rgb.setdefault(rgb, set()).add(shape)
cache = {}
rows = []
for index in range(1151):
    record = trace[index * stride:(index + 1) * stride]
    frame = int.from_bytes(record[:4], 'big')
    assert frame == index + 650
    palette = struct.unpack('>3072H', record[1158:])
    mutable = []
    for group in range(192):
        colors = palette[group * 16:(group + 1) * 16]
        if colors not in cache:
            rgb = tuple(w & 4095 for w in colors)
            shape = tuple(((w >> 12) - (colors[0] >> 12)) & 15 for w in colors)
            cache[colors] = shape in by_rgb.get(rgb, ())
        if not cache[colors]:
            mutable.append(group)
    rows.append({'frame': frame, 'exceptional_groups': mutable})
peak = max(len(row['exceptional_groups']) for row in rows)
report = {
    'trace_sha256': hashlib.sha256(trace).hexdigest(),
    'rom_sha256': hashlib.sha256(rom).hexdigest(),
    'frames': len(rows), 'distinct_color_groups': len(cache),
    'peak_exceptional_groups': peak,
    'minimum_owner_bytes_at_peak': 480 + 34 * peak,
    'peak_frames': [row['frame'] for row in rows if len(row['exceptional_groups']) == peak],
    'scope': 'Snapshot representability lower bound only: searches all even ROM palette bases, not the actual source binding history. Does not size a safe runtime pool or prove campaign coverage.',
    'rows': rows,
}
Path('build/palette-snapshot-storage.json').write_text(json.dumps(report, indent=2) + '\n')
print({key: value for key, value in report.items() if key not in ('rows', 'peak_frames')})
