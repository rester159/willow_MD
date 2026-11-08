#!/usr/bin/env python3
"""shopfix.py SHOPDIR: re-fit the shop picture's cells to the arcade source.

tools/shoppack.py fits CRAM lines 2 and 3 and lets cells use line 1, the
HUD/sprite line, as it is. Line 1 has no black and no shop red, so the
cells it won drew the arcade's black as 0x222 and its reds as 0x028, and
line 3 drew black as 0x004: grey strips in the icon boxes and the text box,
squares in the red bands (player report, 2026-11-07).

The shop picture is plane B over a black backdrop (src/shop.c: background
colour 0, CRAM 0 = 0x000), so index 0 is exact black in every line. Each
cell (but the owl's blink cells, whose frames in alt.bin keep their tiles)
is re-quantised from SHOPDIR/preview.ppm (shoppack's arcade source at 320,
3-bit levels) onto whichever of lines 1-3 matches it best, its black pixels
on index 0; a cell is only changed when the error drops. Old tiles stay
where they are (the owl frames and the big glyphs use them); new ones are
appended. Writes tiles.bin, map.bin and SHOP_TILES in shop.h; run
tools/pack_compress.sh afterwards."""
import re, struct, sys
from pathlib import Path

D = Path(sys.argv[1])
hdr = (D / "preview.ppm").read_bytes().split(b"\n", 3)
assert hdr[1] == b"320 224"
src = hdr[3]
pal = struct.unpack(">64H", (D / "pal.bin").read_bytes())
words = list(struct.unpack(">1120H", (D / "map.bin").read_bytes()))
tiles = bytearray((D / "tiles.bin").read_bytes())
h = (D / "shop.h").read_text()
owl = set(int(v) for v in re.search(r"shop_owl_cell\[SHOP_OWL_CELLS\] = \{([^}]*)\}", h).group(1).split(","))


def lv(w):                      # CRAM word -> (r, g, b) 3-bit levels
    return ((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7)


def d2(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b))


def pix(w, i):
    t = w & 0x7FF
    x, y = i % 8, i // 8
    if w & 0x800: x = 7 - x
    if w & 0x1000: y = 7 - y
    o = t * 32 + y * 4 + x // 2
    return (tiles[o] >> 4 if x % 2 == 0 else tiles[o]) & 15


def shown(L, p):
    return (0, 0, 0) if p == 0 else lv(pal[L * 16 + p])


def fit(cell_src, L):
    out, err = [], 0
    for s in cell_src:
        best = min(range(16), key=lambda p: (d2(shown(L, p), s), p))
        out.append(best); err += d2(shown(L, best), s)
    return out, err


def tile_bytes(px):
    return bytes((px[i] << 4) | px[i + 1] for i in range(0, 64, 2))


def flips(px):
    rows = [px[r * 8:r * 8 + 8] for r in range(8)]
    yield px, 0
    yield [v for r in rows for v in r[::-1]], 0x800
    yield [v for r in rows[::-1] for v in r], 0x1000
    yield [v for r in rows[::-1] for v in r[::-1]], 0x1800


index = {}
for t in range(len(tiles) // 32):
    index.setdefault(bytes(tiles[t * 32:t * 32 + 32]), t)
changed = moved = 0
before = after = 0
for k in range(1120):
    if k in owl:
        continue
    c, r = k % 40, k // 40
    w = words[k]
    L0 = (w >> 13) & 3
    if L0 == 0:
        continue                # line 0: the icon colours (not part of the background fit)
    cs = []
    for i in range(64):
        X, Y = c * 8 + i % 8, r * 8 + i // 8
        o = (Y * 320 + X) * 3
        cs.append(tuple(v // 36 for v in src[o:o + 3]))
    cur = sum(d2(shown(L0, pix(w, i)), cs[i]) for i in range(64))
    best = min(((fit(cs, L), L) for L in (1, 2, 3)), key=lambda e: (e[0][1], e[1] != L0))
    (px, err), L = best
    before += cur
    if err >= cur:
        after += cur
        continue
    after += err
    t = None
    for f_px, f in flips(px):
        b = tile_bytes(f_px)
        if b in index:
            t = index[b] | f
            break
    if t is None:
        index[tile_bytes(px)] = len(tiles) // 32
        t = len(tiles) // 32
        tiles += tile_bytes(px)
    words[k] = t | (L << 13) | (w & 0x8000)
    changed += 1; moved += L != L0
(D / "tiles.bin").write_bytes(bytes(tiles))
(D / "map.bin").write_bytes(struct.pack(">1120H", *words))
n = len(tiles) // 32
(D / "shop.h").write_text(re.sub(r"#define SHOP_TILES \d+", f"#define SHOP_TILES {n}", h))
print(f"shop: {changed} cells re-fitted ({moved} to another line), error {before} -> {after}, tiles {n}")
