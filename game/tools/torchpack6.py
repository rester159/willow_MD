#!/usr/bin/env python3
"""torchpack6.py S6DIR: stage 6's animated scenery (stage6_families.md 8):
the wall torches (class 12 kind 6 variant 6: 2 x 2 16-px cells, 3 frames)
in areas 0-3 and area 2's chandeliers (variant 7: 3 x 2 cells, 2 frames),
from each area's dump (scroll2 palette 7).

A tile has one CRAM line, and the flames' oranges are only in the stage's
sprite line 1 while the wall around them needs the scenery's: so the block
is split. The plane keeps one static wall block on the scenery lines (2-3,
the area's pal.bin), with every pixel a flame covers in any frame painted
the darkest wall colour; the flames are sprites on line 1 (flame pixels
only, the rest transparent), one per frame.

Writes data/stage6/aN/anim_tiles.bin (the plane tiles, then each frame's
sprite tiles column-major: torch 16, chandelier 16 + 8), anim_words.bin
(torch 4 x 4 plane words, chandelier 4 x 6; tile index from 0 | line |
flips) and anim.h (ANIM_PLANE_TILES, ANIM_TORCH_SPR, ANIM_CHAND_SPR)."""
import struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402
from cps1_palette import cps_rgb  # noqa: E402
from intropack import q3, flip, pack_tile  # noqa: E402
import pack_stage6_areas as areas  # noqa: E402

TORCH = [[(0x0D80, 0x0D88), (0x0D81, 0x0D89)], [(0x0E20, 0x0E28), (0x0E21, 0x0E29)], [(0x0E22, 0x0E2A), (0x0E23, 0x0E2B)]]
CHAND = [[(0x0DD0, 0x0DD8), (0x0DD1, 0x0DD9), (0x0DD2, 0x0DDA)], [(0x0E30, 0x0E38), (0x0E31, 0x0E39), (0x0E32, 0x0E3A)]]
W = (2, 4, 3)


def main():
    s6 = Path(sys.argv[1])
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    spr = struct.unpack(">32H", (ROOT / "game/data/s6spr/sprpal.bin").read_bytes())
    cw = lambda w: ((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7)
    line1 = [cw(w) for w in spr[16:32]]
    d = lambda a, b: sum(k * (x - y) ** 2 for k, x, y in zip(W, a, b))
    fire = lambda c: c[0] >= 4 and c[0] >= c[2] + 2          # reds, oranges, yellows
    for n in range(4):
        out = ROOT / f"game/data/stage6/a{n}"
        raw = (s6 / areas.AREAS[n]["dump"]).read_bytes()[0x80:]
        pal_ram = raw[0x1C000:0x1C000 + 0x2000]
        colour = lambda p: q3(cps_rgb(int.from_bytes(pal_ram[(0x400 + 7 * 16 + p) * 2:][:2], "big")))
        stage = struct.unpack(">32H", (out / "pal.bin").read_bytes())
        lines = [[cw(w) for w in stage[0:16]], [cw(w) for w in stage[16:32]]]
        plane_tiles, pindex, words, sprites = [], {}, [], []
        for frames in (TORCH, CHAND) if n == 2 else (TORCH,):
            ncol = len(frames[0]) * 2
            blocks = []
            for fr in frames:
                b = {}
                for c, (top, bot) in enumerate(fr):
                    for k, code in enumerate((top, bot)):
                        t = decode_tile(gfx, 0x4000 + code, 16)
                        for y in range(16):
                            for x in range(16):
                                b[(c * 16 + x, k * 16 + y)] = None if t[y][x] == 15 else colour(t[y][x])
                blocks.append(b)
            flame = {p for b in blocks for p, c in b.items() if c is not None and fire(c)}
            walls = [c for c in blocks[0].values() if c is not None and not fire(c)]
            dark = min(walls, key=lambda c: sum(c)) if walls else (0, 0, 0)
            # The static wall block (frame 0 without its flames), scenery lines.
            base = {p: (dark if p in flame else c) for p, c in blocks[0].items()}
            for r in range(4):
                for c in range(ncol):
                    px = [base[(c * 8 + x, r * 8 + y)] for y in range(8) for x in range(8)]
                    best = None
                    for li in range(2):
                        pens = [0 if q is None else 1 + min(range(15), key=lambda i: d(q, lines[li][1 + i])) for q in px]
                        err = sum(0 if q is None else d(q, lines[li][p]) for q, p in zip(px, pens))
                        if best is None or err < best[0]: best = (err, li, tuple(pens))
                    _, li, pens = best
                    for fl in range(4):
                        key = (li, tuple(flip(list(pens), fl)))
                        if key in pindex: words.append(pindex[key] | (fl << 11) | ((2 + li) << 13)); break
                    else:
                        pindex[(li, pens)] = len(plane_tiles); plane_tiles.append(pens); words.append(pindex[(li, pens)] | ((2 + li) << 13))
            # The flames: sprites on line 1, column-major 8x8 tiles (32 px wide pieces).
            for b in blocks:
                for x0 in range(0, ncol * 8, 32):
                    wcells = min(4, ncol - x0 // 8)
                    for c in range(wcells):
                        for r in range(4):
                            sprites.append([1 + min(range(15), key=lambda i: d(q, line1[1 + i])) if (q := b[(x0 + c * 8 + x, r * 8 + y)]) is not None and (x0 + c * 8 + x, r * 8 + y) in flame else 0
                                            for y in range(8) for x in range(8)])
        tiles = plane_tiles + sprites
        (out / "anim_tiles.bin").write_bytes(b"".join(pack_tile(list(t)) for t in tiles))
        (out / "anim_words.bin").write_bytes(struct.pack(">%dH" % len(words), *words))
        np = len(plane_tiles)
        (out / "anim.h").write_text(f"/* generated by tools/torchpack6.py */\n#define A{n}_PLANE_TILES {np}\n#define A{n}_TORCH_SPR {np}\n"
                                    f"#define A{n}_CHAND_SPR {np + 48}\n#define A{n}_TILES {len(tiles)}\n")
        print(f"area {n}: {np} plane tiles, {len(sprites)} sprite tiles")


if __name__ == "__main__":
    main()
