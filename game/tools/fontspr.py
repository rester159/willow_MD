#!/usr/bin/env python3
"""fontspr.py: data/intro/font.bin -> data/intro/font_spr.bin, the intro font
as sprite art (user, 2026-11-04: story text and stage titles as sprites
over the scene, no grey band).

Story and box glyphs: font.bin's, the band's ground pen 1 made transparent.
Title letters (user, 2026-11-06: "like in the arcade? white with slight
black shadow"): re-decoded from the arcade's big font (2 x 2 cells c, c+1 /
c+0x10, c+0x11, text palette 0x0E: pen 0 the near-black shadow, 1-6 white
and greys, 15 transparent), each arcade pen p stored as pen p + 1 (0 =
transparent); src/intro.c title_glyphs colours pens 1-7 per stage. Tiles
stored TL, BL, TR, BR: a 2 x 2 sprite's order."""
import collections, re, struct, sys, zipfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import intropack as ip  # noqa: E402

CODES = {"A": 0x7680, "C": 0x7684, "D": 0x7686, "E": 0x7688, "F": 0x768A, "G": 0x768C, "H": 0x768E,
         "I": 0x76A0, "K": 0x76A4, "L": 0x76A6, "M": 0x76A8, "N": 0x76AA, "O": 0x76AC, "P": 0x76AE,
         "R": 0x76C2, "S": 0x76C4, "T": 0x76C6, "Z": 0x76E2, "'": 0x7FEE,
         "1": 0x7B00, "2": 0x7B02, "3": 0x7B04, "4": 0x7B06, "5": 0x7B08, "6": 0x7B0A}

src = Path("data/intro/font.bin").read_bytes()
h = Path("data/intro/intro_text.h").read_text()
t0 = int(re.search(r"INTRO_TITLE0 (\d+)", h).group(1))
tab = [int(v) for v in re.search(r'font_title\[64\] = \{([^}]*)\}', h).group(1).split(',')]
n = len(src) // 32
tiles = []
for g in range(n):
    t = bytearray(src[g * 32:g * 32 + 32])
    for i in range(32):
        hi, lo = t[i] >> 4, t[i] & 15
        t[i] = ((0 if hi == 1 else hi) << 4) | (0 if lo == 1 else lo)
    tiles.append(bytes(t))
with zipfile.ZipFile(ip.ROOT / "willow.zip") as z:
    gfx = ip.assemble_region(z)
use = collections.Counter()
done = set()
for i, g in enumerate(tab):
    if not g:
        continue
    ch = chr(32 + i)
    assert ch in CODES, ch
    for k, dc in enumerate((0, 0x10, 1, 0x11)):        # TL, BL, TR, BR
        px = ip.decode_tile(gfx, CODES[ch] + dc, 8)
        pens = [0 if p == 15 else p + 1 for row in px for p in row]
        assert max(pens) <= 7
        use.update(p for p in pens if p)
        tiles[g + k] = ip.pack_tile(pens)
    done.update(range(g, g + 4))
assert done == set(range(t0, n)), "title glyphs not all re-decoded"
Path("data/intro/font_spr.bin").write_bytes(b"".join(tiles))
print(f"font_spr.bin: {n} glyphs; title letters from the arcade font, pens 1-7 use {sorted(use.items())}")
