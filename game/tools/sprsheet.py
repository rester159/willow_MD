#!/usr/bin/env python3
"""sprsheet.py OUT.ppm CRAM.bin STAGE_PAL FAMILY_DIR[:pose,pose...]...: a
contact sheet of sprite poses, each shown twice: left with the family's own
arcade colours (tiles.src.bin + pal.bin), right as the Genesis will show it
(tiles.bin with each piece's CRAM line). For checking tools/sprpal.py."""
import struct, sys
from pathlib import Path

def rgb(w):
    return bytes(((w >> s) & 7) * 36 for s in (1, 5, 9))

out, cram_p, stage_p = sys.argv[1:4]
cram = list(struct.unpack(">32H", Path(cram_p).read_bytes())) + list(struct.unpack(">32H", Path(stage_p).read_bytes()))
CELL = 96
sheet = []
for arg in sys.argv[4:]:
    d, _, sel = arg.partition(":")
    d = Path(d)
    poses = (d / "poses.bin").read_bytes()
    src_tiles = (d / "tiles.src.bin").read_bytes() if (d / "tiles.src.bin").exists() else (d / "tiles.bin").read_bytes()
    tiles = (d / "tiles.bin").read_bytes()
    pal = struct.unpack(">16H", (d / "pal.bin").read_bytes())
    count = int((d / "layout.h").read_text().split("_COUNT ")[1].split()[0])
    for i in ([int(x) for x in sel.split(",")] if sel else range(min(count, 6))):
        t0, nt, ns, e0 = struct.unpack_from(">HBBH", poses, i * 6)
        img = [[b"\x28\x28\x28"] * (2 * CELL) for _ in range(CELL)]
        for k in range(ns):
            dy, size, t, dx = struct.unpack_from(">hBBh", poses, count * 6 + (e0 + k) * 6)
            w, h, line, flip = (size >> 2 & 3) + 1, (size & 3) + 1, size >> 4 & 3, size >> 7
            for c in range(w):
                for r in range(h):
                    tile = t0 + t + c * h + r
                    for y in range(8):
                        for x in range(8):
                            xx = (w * 8 - 1 - (c * 8 + x)) if flip else c * 8 + x
                            sx, sy = dx + xx + CELL // 2, dy + r * 8 + y + CELL * 3 // 4
                            if not (0 <= sx < CELL and 0 <= sy < CELL):
                                continue
                            for half, tl, colour in ((0, src_tiles, lambda p: rgb(pal[p])),
                                                     (1, tiles, lambda p: rgb(cram[line * 16 + p]))):
                                b = tl[tile * 32 + y * 4 + x // 2]
                                p = b >> 4 if x % 2 == 0 else b & 15
                                if p:
                                    img[sy][sx + half * CELL] = colour(p)
        sheet.append(img)
cols = 4
rows = (len(sheet) + cols - 1) // cols
W, H = cols * 2 * CELL, rows * CELL
buf = bytearray(b"\x10\x10\x10" * W * H)
for n, img in enumerate(sheet):
    ox, oy = (n % cols) * 2 * CELL, (n // cols) * CELL
    for y in range(CELL):
        for x in range(2 * CELL):
            i = ((oy + y) * W + ox + x) * 3
            buf[i:i + 3] = img[y][x]
Path(out).write_bytes(b"P6\n%d %d\n255\n" % (W, H) + bytes(buf))
