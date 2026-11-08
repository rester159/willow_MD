#!/usr/bin/env python3
"""satpack.py DIR...: sat.bin beside each family's poses.bin (run after
tools/sprpal.py): every sprite entry ready for the sprite table, 4 words
{dy, size << 8, attribute (CRAM line << 13 | H-flip << 11 | tile offset),
dx}, in the same order as poses.bin's entries. The game adds the screen
position, the link and the pose's VRAM base (src/sprite.c)."""
import struct, sys
from pathlib import Path

for d in map(Path, sys.argv[1:]):
    p = (d / "poses.bin").read_bytes()
    count = int((d / "layout.h").read_text().split("_COUNT ")[1].split()[0])
    n = (len(p) - count * 6) // 6
    out = bytearray()
    for i in range(n):
        dy, size, t, dx = struct.unpack_from(">hBBh", p, count * 6 + i * 6)
        out += struct.pack(">hHHh", dy, (size & 0x0F) << 8, ((size >> 4) & 3) << 13 | (size >> 7) << 11 | t, dx)
    (d / "sat.bin").write_bytes(bytes(out))
    print(f"{d.name}: {n} entries")
