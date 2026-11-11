#!/usr/bin/env python3
"""earthfix.py: stage 1's earth path in brown, as the arcade's.

The fit (palopt) shares olive 0x466 between the path (arcade scroll2 group
4, pens 10-14: browns 85,68,0 / 102,85,51 in the dumps) and the stone (group
8, and group 4's own grey pens), so the path reads grey. A pixel changes only
where every cell showing its tile has arcade group 4 pen 10-14 there and it
is 0x466: it takes line 3's light brown 0x468. A line-2 tile with such
pixels moves to line 3 (no line-2 entry is free: the far layer uses pen 6),
when every colour it has is also in line 3 (else it stays as it was), its cells' plane words to line 3. Nothing else changes. Run on the packed files
(tools/stagepack.py output, after tools/maskprio.py); not idempotent: rerun
on fresh files."""
import struct, collections, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402

SD = "data/stage1"
mp = open(SD + "/map.bin", "rb").read(); C = len(mp) // 128
ld = open(SD + "/loads.bin", "rb").read()
nc, pool = struct.unpack_from(">HH", ld, 0)
offs = struct.unpack_from(">%dH" % (2 * nc + 1), ld, 4)
e0 = 4 + 2 * (2 * nc + 1)
tiles = bytearray(open(SD + "/tiles.bin", "rb").read())
pal = list(struct.unpack(">32H", open(SD + "/pal.bin", "rb").read()))
cells = open("data/stage1_fg.cells", "rb").read(); ccol = len(cells) // 4 // 32
with zipfile.ZipFile(ROOT / "willow.zip") as z:
    gfx = assemble_region(z)
dec = {}
slot = {}
vote = collections.defaultdict(lambda: [0] * 64)      # tile -> per pixel: +1 path pen, -1000 other
lines = collections.defaultdict(set)
users_at = collections.defaultdict(list)
for c in range(C):
    for i in range(offs[c], offs[c + 1]):
        t, s = struct.unpack_from(">HH", ld, e0 + 4 * i); slot[s] = t
    for r in range(64):
        w = mp[(c * 64 + r) * 2] << 8 | mp[(c * 64 + r) * 2 + 1]
        if not w or (w & 0x7FF) - 16 not in slot:
            continue
        t = slot[(w & 0x7FF) - 16]
        lines[t].add((w >> 13) & 3)
        users_at[t].append((c * 64 + r) * 2)
        code, attr = struct.unpack_from(">HH", cells, ((r // 2) * ccol + c // 2) * 4)
        key = 0x4000 + (code & 0x1FFF)
        if key not in dec:
            dec[key] = decode_tile(gfx, key, 16)
        a = dec[key]; g = attr & 31
        v = vote[t]
        for y in range(8):
            for x in range(8):
                ty = 7 - y if w & 0x1000 else y          # the stored tile's pixel
                tx = 7 - x if w & 0x800 else x
                ay = (r % 2) * 8 + y; ax = (c % 2) * 8 + x
                if attr & 64: ay = 15 - ay
                if attr & 32: ax = 15 - ax
                pen = a[ay][ax]
                v[ty * 8 + tx] += 1 if g == 4 and 10 <= pen <= 14 else -1000
assert all(t not in [] for t in vote)
def lv(w):
    return (w & 14, w >> 4 & 14, w >> 8 & 14)
def near3(w):                                    # nearest line-3 entry (pens 1..15)
    return min(range(1, 16), key=lambda p: sum((a - b) ** 2 for a, b in zip(lv(w), lv(pal[16 + p]))))
old3, new3 = pal[16:].index(0x466), pal[16:].index(0x468)
mp = bytearray(mp)
n = px = moved = skipped = 0
for t, v in vote.items():
    if len(lines[t]) != 1 or not any(x > 0 for x in v):
        continue
    ln = next(iter(lines[t]))
    tb = bytearray(tiles[t * 32:t * 32 + 32]); hit = 0
    if ln == 2 and any(pal[p] not in pal[17:] for p in {q for b in tb for q in (b >> 4, b & 15) if q}):
        skipped += 1
        continue                                 # (a colour line 3 lacks: stays olive)
    for i in range(64):
        sh = 4 if i % 2 == 0 else 0
        p = (tb[i // 2] >> sh) & 15
        if not p:
            continue
        col = pal[(0 if ln == 2 else 16) + p]
        q = new3 if v[i] > 0 and col == 0x466 else (p if ln == 3 else near3(col))
        if v[i] > 0 and col == 0x466: hit += 1
        tb[i // 2] = (tb[i // 2] & ~(15 << sh) & 255) | q << sh
    if not hit:
        continue
    tiles[t * 32:t * 32 + 32] = tb; n += 1; px += hit
    if ln == 2:
        moved += 1
        for k in users_at[t]:
            w = (mp[k] << 8 | mp[k + 1]) & ~0x6000 | 3 << 13
            mp[k:k + 2] = struct.pack(">H", w)
open(SD + "/map.bin", "wb").write(mp)
open(SD + "/tiles.bin", "wb").write(tiles)
print(f"{n} tiles, {px} pixels to brown; {moved} tiles moved from line 2 to line 3, {skipped} kept (a colour line 3 lacks)")
