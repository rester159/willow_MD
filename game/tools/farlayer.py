#!/usr/bin/env python3
"""farlayer.py PALDUMP.cps OUT.ppm [GRID]: a stage's far layer (arcade
scroll3) as one wide picture for tools/stagepack.py (the far layer scrolls
on plane B at the arcade's rates, both ways; NATIVE_ENGINE_PLAN.md).

Measured on stage-1 dumps (scroll registers vs pixels): the far-layer pixel
at screen (x, y) is far map (x + cam_x / 2 + 32, y + 132 - cam_y / 2) in this
picture's coordinates when cam_y = 0, i.e. picture row j is the ROM layer
row j - 240 (tools/cps1_source_layers.py convention). The picture starts
TOP rows above that (FAR_TOP, default 0; 144 covers a camera up to cam_y 288,
the far layer half of it; stage 1 uses 0: no free CRAM line for the cloud band), so screen row y shows picture row
y + TOP - cam_y / 2."""
import os, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region  # noqa: E402
from cps1_source_layers import SourceLayers  # noqa: E402
from cps1_palette import cps_rgb  # noqa: E402

WIDTH = int(os.environ.get("FAR_WIDTH", "2120"))   # stage 1: camera x up to 3456 -> far x up to 1760 + 320; stage 2: 4096 (its far layer repeats)
TOP = int(os.environ.get("FAR_TOP", "0"))
YOFF = int(os.environ.get("FAR_YOFF", "0"))   # far rows the layer sits higher at the native cam_y = 0 (stage 3: 128, its camera starts at y 256)
X0 = int(os.environ.get("FAR_X0", "0"))       # far x of the picture's column 0 (stage 6's tower and chamber: their areas' native x start at 3072 / 1024)


def main():
    dump, out = sys.argv[1:3]
    grid = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    sl = SourceLayers(rom, gfx, grid)
    pal = Path(dump).read_bytes()[0x80:][0x1C000:0x1E000]
    p3 = [cps_rgb(int.from_bytes(pal[0xC00 + i * 2:0xC02 + i * 2], "big")) for i in range(512)]
    o = bytearray()
    for j in range(-TOP, 224):
        for x in range(WIDTH):
            pen, g = sl.pen(3, x + X0, j - 240 - YOFF)
            o += bytes(p3[g * 16 + pen]) if pen != 15 else b"\0\0\0"
    Path(out).write_bytes(b"P6\n%d %d\n255\n" % (WIDTH, 224 + TOP) + o)


if __name__ == "__main__":
    main()
