#!/usr/bin/env python3
"""stagemap.py GRID ROW0 ROWS OUT_PREFIX [PALETTE_DUMP.cps]: a stage's
gameplay foreground (arcade scroll2) assembled offline from the ROM's block
grid, as data for the native engine (NATIVE_ENGINE_PLAN.md, milestone M0).

The arcade builds its gameplay foreground from 256x256-pixel blocks: grid
GRID (pointer table 2DBF8, mode table 2DC1C) names a block per grid cell;
block b's 16x16 cells (16x16 pixels each) are at ROM 0x80000 + b * 0x400,
column-major, 4 bytes a cell (code, attribute). Stage 1 is grid 0, rows
14-15 (found 2026-11-01 by matching the blocks B1's cell model holds during
play). This is reading the arcade's data offline; nothing of the arcade
program runs in the game.

Writes OUT_PREFIX.cells (u16 code, u16 attribute per 16x16 cell, row-major,
ROWS*16 rows x columns*16 columns, big-endian) and, with a palette dump,
OUT_PREFIX.ppm (the assembled foreground in arcade colours; pen 15 shows
magenta)."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402
from cps1_palette import cps_rgb  # noqa: E402
import zipfile  # noqa: E402

GRIDS, MODES = 0x2DBF8, 0x2DC1C
SHAPE = {0: (16, 16), 1: (8, 32), 2: (32, 64), 3: (1, 256)}   # mode 2: lookup 0x43722 (stage4.md correction 2)   # mode -> (rows, columns) of blocks


def word(p, a):
    return int.from_bytes(p[a:a + 2], "big")


def main():
    grid, row0, rows, out = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    prog = (ROOT / "build/reference_maincpu.bin").read_bytes()
    pointer = int.from_bytes(prog[GRIDS + 4 * grid:GRIDS + 4 * grid + 4], "big")
    brows, bcols = SHAPE[prog[MODES + grid]]
    used = [c for c in range(bcols) if any(prog[pointer + (row0 + r) * bcols + c] for r in range(rows))]
    width = (max(used) + 1) * 16
    cells = bytearray()
    grid_ids = []
    for r in range(rows * 16):
        br, cr = row0 + r // 16, r % 16
        for c in range(width):
            b = prog[pointer + br * bcols + c // 16]
            a = 0x80000 + b * 0x400 + ((c % 16) * 16 + cr) * 4
            cells += prog[a:a + 4]
        if cr == 0:
            grid_ids.append([prog[pointer + br * bcols + bc] for bc in range(width // 16)])
    Path(out + ".cells").write_bytes(bytes(cells))
    print(f"grid {grid} rows {row0}..{row0 + rows - 1}: {rows * 16} x {width} cells "
          f"({rows * 256} x {width * 16} px); blocks {grid_ids}")
    if len(sys.argv) > 5:
        dump = Path(sys.argv[5]).read_bytes()[0x80:]
        pal_base = (int.from_bytes(Path(sys.argv[5]).read_bytes()[0x0A:0x0C], "big") << 8) - 0x900000
        with zipfile.ZipFile(ROOT / "willow.zip") as z:
            gfx = assemble_region(z)
        H, W = rows * 256, width * 16
        img = bytearray(W * H * 3)
        cache = {}
        for r in range(rows * 16):
            for c in range(width):
                o = (r * width + c) * 4
                code, attr = int.from_bytes(cells[o:o + 2], "big"), int.from_bytes(cells[o + 2:o + 4], "big")
                key = 0x4000 + (code & 0x1FFF)
                if key not in cache:
                    cache[key] = decode_tile(gfx, key, 16)
                t = cache[key]
                for y in range(16):
                    for x in range(16):
                        pen = t[15 - y if attr & 64 else y][15 - x if attr & 32 else x]
                        if pen == 15:
                            rgb = (255, 0, 255)
                        else:
                            w = word(dump, pal_base + 0x800 + ((attr & 31) * 16 + pen) * 2)
                            rgb = cps_rgb(w)[:3]
                        i = ((r * 16 + y) * W + c * 16 + x) * 3
                        img[i:i + 3] = bytes(rgb)
        Path(out + ".ppm").write_bytes(b"P6\n%d %d\n255\n" % (W, H) + bytes(img))


if __name__ == "__main__":
    main()
