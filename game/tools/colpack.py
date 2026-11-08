#!/usr/bin/env python3
"""colpack.py CELLS ROWS OUT_DIR: a stage's collision as native data
(NATIVE_ENGINE_PLAN.md, M1; specs/player.md section 6).

The arcade keeps collision in the foreground map: a 16x16 cell's collision
id is attribute bits 10-15; id -> 8-byte descriptor at ROM 0x4712 (64
entries: flags, floor shape, ceiling height, right/left wall x). Read here
offline. Writes OUT_DIR/col.bin: per 16x16 cell one id byte, column-major
(cell column * cell rows + cell row, rows downward) and OUT_DIR/coldesc.bin
(the 64 descriptors as they are)."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
cells = Path(sys.argv[1]).read_bytes(); rows = int(sys.argv[2]) * 16; out = Path(sys.argv[3])
cols = len(cells) // 4 // rows
ids = bytearray()
used = {}
for c in range(cols):
    for r in range(rows):
        o = (r * cols + c) * 4
        i = int.from_bytes(cells[o + 2:o + 4], "big") >> 10
        ids.append(i); used[i] = used.get(i, 0) + 1
out.mkdir(parents=True, exist_ok=True)
(out / "col.bin").write_bytes(bytes(ids))
rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
(out / "coldesc.bin").write_bytes(rom[0x4712:0x4712 + 64 * 8])
print(f"{cols} x {rows} cells; ids used: " + ", ".join(f"{i}:{n}" for i, n in sorted(used.items())))
for i in sorted(used):
    print(i, rom[0x4712 + i * 8:0x4712 + i * 8 + 8].hex())
