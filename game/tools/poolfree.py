#!/usr/bin/env python3
"""poolfree.py STAGEDIR: borrow.bin, the foreground pool slots sprites may
use per camera zone (tools/stagepack.py output: loads.bin, layout.h).

The pool is sized for the stage's busiest 42-column window; elsewhere many
slots hold nothing that can come on screen. For camera zone z (map columns
16z .. 16z+15) a slot is free when no tile run that uses it overlaps the
columns the stage loads anywhere in the zone, with MARGIN columns either
side for the loader's catch-up. Format: u16 zone count; u16 offset (in
words) per zone + 1; then per zone pairs (u16 first slot, u16 length),
ranges of at least 16 slots."""
import struct, sys
from pathlib import Path

WINDOW, ZONE, MARGIN, MIN = 42, 16, 4, 16
d = Path(sys.argv[1])
raw = (d / "loads.bin").read_bytes()
C, pool = struct.unpack_from(">HH", raw, 0)
offs = struct.unpack_from(">%dH" % (2 * C + 1), raw, 4)
base = 4 + (2 * C + 1) * 2
lst = lambda k: [struct.unpack_from(">HH", raw, base + i * 4) for i in range(offs[k], offs[k + 1])]
starts, ends = {}, {}
for c in range(C):
    for t, sl in lst(c): starts.setdefault((t, sl), []).append(c)
    for t, sl in lst(C + c): ends.setdefault((t, sl), []).append(c)
runs = [(s, e, k[1]) for k in starts for s, e in zip(sorted(starts[k]), sorted(ends[k]))]
zones = (C + ZONE - 1) // ZONE
words, offsets = [], []
for z in range(zones):
    lo, hi = z * ZONE - MARGIN, z * ZONE + ZONE - 1 + WINDOW + MARGIN
    used = bytearray(pool)
    for s, e, sl in runs:
        if s <= hi and e >= lo:
            used[sl] = 1
    offsets.append(len(words))
    i = 0
    while i < pool:
        if used[i]:
            i += 1
            continue
        j = i
        while j < pool and not used[j]:
            j += 1
        if j - i >= MIN:
            words += [i, j - i]
        i = j
offsets.append(len(words))
out = struct.pack(">H", zones) + b"".join(struct.pack(">H", o) for o in offsets) + b"".join(struct.pack(">H", w) for w in words)
(d / "borrow.bin").write_bytes(out)
print(f"{zones} zones; free slots per zone: " + " ".join(
    str(sum(words[k + 1] for k in range(offsets[z], offsets[z + 1], 2))) for z in range(zones)))
