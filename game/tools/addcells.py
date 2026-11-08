#!/usr/bin/env python3
"""addcells.py STAGEDIR TRIALDIR OUTDIR MAXSLOT [FORBID_FROM_COL:SLOTS.bin]: put the map cells a
stage's pack left out (rows the recorded camera never showed) into its data
without touching anything already there: every old word, tile, load entry and
VRAM slot stays, so the art pass's result and all data keyed to slots (boss
bodies, arenas) hold. TRIALDIR is a stagepack run of the whole map (FREEZE_CACHE:
its added cells already merged to fit the pool); only the cells empty in
STAGEDIR are taken from it. New tiles go after the old ones; each new run of a
tile takes a slot free over its columns + WINDOW, or, with none free, the
nearest tile already live at that column."""
import struct, sys
from pathlib import Path
WINDOW = 42
D, T, O = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]); MAXS = int(sys.argv[4])
forbid_from, forbid = 1 << 30, set()
import os
HIGH_END = int(os.environ.get("HIGH_END", 1 << 30)); MAXS_LOW = int(os.environ.get("MAXS_LOW", MAXS))
if len(sys.argv) > 5:
    a, b = sys.argv[5].split(":"); forbid_from = int(a)
    d = Path(b).read_bytes(); forbid = {v - 16 for v in struct.unpack(">%dH" % (len(d) // 2), d)}

def loads_of(p):
    raw = (p / "loads.bin").read_bytes()
    C, pool = struct.unpack_from(">HH", raw, 0)
    offs = struct.unpack_from(">%dH" % (2 * C + 1), raw, 4)
    base = 4 + (2 * C + 1) * 2
    lst = lambda k: [struct.unpack_from(">HH", raw, base + i * 4) for i in range(offs[k], offs[k + 1])]
    return C, pool, [lst(k) for k in range(C)], [lst(C + k) for k in range(C)]

def runs_of(right, left):
    st, en = {}, {}
    for c, l in enumerate(right):
        for k in l: st.setdefault(k, []).append(c)
    for c, l in enumerate(left):
        for k in l: en.setdefault(k, []).append(c)
    return [(s, e, k[0], k[1]) for k in st for s, e in zip(sorted(st[k]), sorted(en[k]))]

C, pool, right, left = loads_of(D)
tc, tp, tr, tl = loads_of(T)
assert C == tc
R = len((D / "map.bin").read_bytes()) // 2 // C
om = list(struct.unpack(">%dH" % (C * R), (D / "map.bin").read_bytes()))
tm = struct.unpack(">%dH" % (C * R), (T / "map.bin").read_bytes())
ot = (D / "tiles.bin").read_bytes(); tt = (T / "tiles.bin").read_bytes()
oruns = runs_of(right, left)
truns = runs_of(tr, tl)
tile_at = {}                                    # (slot, col) -> tile in the trial
for s, e, t, sl in truns:
    for c in range(s, e + 1): tile_at[(sl, c)] = t

_pal = struct.unpack(">32H", (D / "pal.bin").read_bytes())
def pal(x, p): return _pal[((x >> 13) & 1) * 16 + p]
def cdist(a, b): return sum(abs(((a >> s) & 14) - ((b >> s) & 14)) for s in (0, 4, 8)) // 2

def flip(b, f):
    px = [[(b[y * 4 + x // 2] >> (4 if x % 2 == 0 else 0)) & 15 for x in range(8)] for y in range(8)]
    if f & 1: px = [r[::-1] for r in px]
    if f & 2: px = px[::-1]
    o = bytearray(32)
    for y in range(8):
        for x in range(8): o[y * 4 + x // 2] |= px[y][x] << (4 if x % 2 == 0 else 0)
    return bytes(o)

index = {}                                      # tile bytes -> (old tile, flips to show it)
for i in range(len(ot) // 32):
    b = ot[i * 32:i * 32 + 32]
    for f in range(4): index.setdefault(flip(b, f), (i, f))
tiles = bytearray(ot)
cells = {}                                      # (c, r) -> (tile, flip, rest of the word)
added = 0
for c in range(C):
    for r in range(R):
        i = c * R + r
        if om[i] or not tm[i]: continue
        w = tm[i]; t = tile_at[((w & 0x7FF) - 16, c)]
        fl = (w >> 11) & 3
        b = flip(tt[t * 32:t * 32 + 32], fl)   # as shown
        if b not in index:
            n = len(tiles) // 32; tiles += b
            for f in range(4): index.setdefault(flip(b, f), (n, f))
        ti, f = index[b]
        cells[(c, r)] = (ti, f, w & 0xE000)
        added += 1

busy = {}
for s, e, t, sl in oruns: busy.setdefault(sl, []).append((s, e + WINDOW))
live = {}
for s, e, t, sl in oruns:
    for c in range(s, e + 1): live.setdefault((t, c), sl)
by_tile = {}
for (c, r), (t, f, x) in cells.items(): by_tile.setdefault(t, []).append((c, r))
# A tile already streamed nearby: its run grows over the added columns (the
# same slot, the load entries move), if the slot stays free there.
ext = 0
runs_by_tile = {}
for i, (s, e, t, sl) in enumerate(oruns): runs_by_tile.setdefault(t, []).append(i)
oruns = [list(x) for x in oruns]
for t, l in by_tile.items():
    if t not in runs_by_tile: continue
    want = sorted({c for c, r in l if (t, c) not in live})
    for i in runs_by_tile[t]:
        s, e, _, sl = oruns[i]
        ns, ne = s, e
        for c in want:
            if ns - WINDOW <= c < ns: ns = c
        for c in want:
            if ne < c <= ne + WINDOW: ne = c
        if (ns, ne) == (s, e): continue
        others = [b for b in busy[sl] if b != (s, e + WINDOW)]
        if not all(ne + WINDOW <= a or b <= ns for a, b in others): continue
        busy[sl] = others + [(ns, ne + WINDOW)]
        if ns != s: right[s].remove((t, sl)); right[ns].append((t, sl))
        if ne != e: left[e].remove((t, sl)); left[ne].append((t, sl))
        oruns[i][0], oruns[i][1] = ns, ne
        for c in range(ns, ne + 1): live.setdefault((t, c), sl)
        ext += 1
oruns = [tuple(x) for x in oruns]
nr, nf = [], 0
out = {}
pairs = []
for t, l in by_tile.items():
    need = sorted({c for c, r in l if (t, c) not in live})
    for c, r in l:
        if (t, c) in live: out[(c, r)] = live[(t, c)]
    if need:
        s = e = need[0]
        for c in need[1:]:
            if c - e > WINDOW: pairs.append((s, e, t)); s = c
            e = c
        pairs.append((s, e, t))
pairs.sort()
fails = []
for s, e, t in pairs:
    got = None
    for sl in range(MAXS if e + WINDOW < HIGH_END else MAXS_LOW):
        if e + WINDOW >= forbid_from and sl in forbid: continue
        if all(e + WINDOW <= a or b <= s for a, b in busy.get(sl, [])): got = sl; break
    if got is None:
        fails.append((s, e, t)); continue
    busy.setdefault(got, []).append((s, e + WINDOW))
    nr.append((s, e, t, got))
    for c in range(s, e + 1): live.setdefault((t, c), got)
    right[s].append((t, got)); left[e].append((t, got))
for (c, r), (t, f, x) in cells.items():
    if (c, r) in out or (t, c) in live: out[(c, r)] = live[(t, c)]
lost = 0
if fails:
    # No slot: the nearest tile (by pixels) already live at the column.
    px = lambda i: tiles[i * 32:i * 32 + 32]
    at_col = {}
    for s, e, tl_, sl in oruns + nr:
        for c in range(s, e + 1): at_col.setdefault(c, []).append((tl_, sl))
    for (c, r), (t, f, x) in list(cells.items()):
        if (c, r) in out: continue
        a = px(t); best = None
        for tl_, sl in at_col.get(c, []):
            b = px(tl_)
            d = 0
            for y in range(32):                 # pen by pen: transparency and colour
                for sh in (4, 0):
                    p, q = (a[y] >> sh) & 15, (b[y] >> sh) & 15
                    if p != q: d += 8 if not p or not q else 1 + (pal(x, p) != pal(x, q)) * cdist(pal(x, p), pal(x, q))
            if best is None or d < best[0]: best = (d, tl_, sl)
        cells[(c, r)] = (best[1], 0, x); out[(c, r)] = best[2]; lost += 1
for (c, r), (t, f, x) in cells.items():
    w = (16 + out[(c, r)]) | x | (f << 11)
    om[c * R + r] = w
offs, ents, n = [], bytearray(), 0
for lst in right + left:
    offs.append(n)
    for t, sl in lst: ents += struct.pack(">HH", t, sl); n += 1
offs.append(n)
O.mkdir(parents=True, exist_ok=True)
(O / "tiles.bin").write_bytes(bytes(tiles))
(O / "map.bin").write_bytes(struct.pack(">%dH" % (C * R), *om))
pool = max([pool] + [x[3] + 1 for x in nr])
(O / "loads.bin").write_bytes(struct.pack(">HH", C, pool) + b"".join(struct.pack(">H", o) for o in offs) + bytes(ents))
peak = max(len(l) for l in right)
print(f"{D.name}: {added} 8x8 cells added, {len(tiles) // 32 - len(ot) // 32} new tiles, {ext} runs lengthened, {len(nr)} new runs, {len(fails)} runs without a slot ({lost} cells mapped to a live tile), most loads at one column {peak}")
