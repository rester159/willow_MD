#!/usr/bin/env python3
"""stormpack.py STAGEDIR OUT.bin: stage 3's storm (specs/stage3.md 2.6) as
CRAM states for the stage's scenery lines 2-3.

The arcade turns scroll3 palettes 16 and 17 (the far layer's sky, clouds
and mountains) through 8 steps (tables 0xEC200 / 0xEC300, 16 words a step;
palette 17's pens 6 and 8-14 change - stage3.md 2.6 says it does not, the
dumps v011000 / v012160 show it does): 0 day, 1-4 fading, 5 night, 6
lightning bright, 7 lightning dim. Our scenery lines are fitted to
the foreground and the far layer together, so for each CRAM entry this
finds the far pixels it shows (data/stage3 bd_map/bd_tiles against the
arcade layer, tools/farlayer.py's geometry) and, where those are mostly
palette-16 pixels, gives the entry the average of their step-s colours
(scaled by the entry's own day colour against those pixels' day average,
keeping the fit's correction). Other entries stay as they are.

Writes 8 x 32 big-endian CRAM words (line 2 then line 3, entry 0 unused)."""
import struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region  # noqa: E402
from cps1_source_layers import SourceLayers  # noqa: E402
from cps1_palette import cps_rgb  # noqa: E402

FAR_TOP, YOFF, GRID = 144, 128, 2
STEPS = {16: 0xEC200, 17: 0xEC300}
BD_BASE = 703


def cram(w):
    return tuple(((w >> s) & 7) * 36 for s in (1, 5, 9))


def word(c):
    q = [int(min(7, max(0, (v + 18) // 36))) for v in c]
    return (q[0] << 1) | (q[1] << 5) | (q[2] << 9)


def main():
    sdir, out = Path(sys.argv[1]), sys.argv[2]
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    sl = SourceLayers(rom, gfx, GRID)
    pal = struct.unpack(">32H", (sdir / "pal.bin").read_bytes())
    tiles = (sdir / "bd_tiles.bin").read_bytes()
    bmap = (sdir / "bd_map.bin").read_bytes()
    lay = {l.split()[1]: int(l.split()[2]) for l in (sdir / "layout.h").read_text().splitlines() if l.startswith("#define")}
    fc, fr = lay["STAGE_FAR_COLS"], lay["STAGE_FAR_ROWS"]
    steps = {g: [[cps_rgb(0xF000 | struct.unpack_from(">H", rom, a + s * 32 + i * 2)[0]) for i in range(16)] for s in range(8)] for g, a in STEPS.items()}
    acc = {}            # CRAM index -> [count16, countother, sum day rgb, sums per step]
    for c in range(1760 // 8, fc):         # the far x shown while the storm runs (camera x >= 3456)
        for r in range(fr):
            w = struct.unpack_from(">H", bmap, (c * fr + r) * 2)[0]
            t = (w & 0x7FF) - BD_BASE
            line = (w >> 13) & 3
            hf, vf = (w >> 11) & 1, (w >> 12) & 1
            if t < 0 or line < 2:
                continue
            for y in range(8):
                for x in range(8):
                    tx, ty = (7 - x) if hf else x, (7 - y) if vf else y
                    b = tiles[t * 32 + ty * 4 + tx // 2]
                    p = (b >> 4) if tx % 2 == 0 else (b & 15)
                    if not p:
                        continue
                    k = (line - 2) * 16 + p
                    pen, g = sl.pen(3, c * 8 + x, r * 8 + y - FAR_TOP - 240 - YOFF)
                    e = acc.setdefault(k, [0, 0, [0, 0, 0], [[0, 0, 0] for _ in range(8)]])
                    if pen == 15 or g not in steps:
                        e[1] += 1
                        continue
                    e[0] += 1
                    for i in range(3): e[2][i] += steps[g][0][pen][i]
                    for s in range(8):
                        for i in range(3): e[3][s][i] += steps[g][s][pen][i]
    states = []
    for s in range(8):
        st = list(pal)
        for k, (n16, nother, day, sums) in acc.items():
            if n16 < nother or not n16:
                continue
            own = cram(pal[k])
            d = [v / n16 for v in day]
            sc = [(o + 8) / (dv + 8) for o, dv in zip(own, d)]
            st[k] = word([sums[s][i] / n16 * sc[i] for i in range(3)])
        states.append(st)
    Path(out).write_bytes(b"".join(struct.pack(">32H", *st) for st in states))
    changed = sum(1 for k in range(32) if states[5][k] != pal[k])
    print(f"storm: {changed} of 30 scenery entries follow scroll3 palettes 16-17")


if __name__ == "__main__":
    main()
