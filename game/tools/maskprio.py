#!/usr/bin/env python3
"""maskprio.py CELLS ROWS_BLOCKS STAGEDIR [MASKS]: the arcade's scroll2
priority-mask pens over the objects, as plane-A priority bits.

The arcade (layer order 0x12F8: scroll3, scroll2, objects, scroll1) draws
scroll2 below the objects, then puts back on top the pens its tile's mask
group lists (tile attr bits 7-8 pick CPS-B registers 80016E/16C/16A/168;
gameplay dumps hold 0, 0x003E, 0x00FE, 0x001E): the stage 2 fort's
handrails, for one, are in front of the characters. The Genesis can only
choose per 8x8 cell: a cell gets the priority bit when its front pens are
at least half of its opaque pixels (FRONT_MIN, a fraction) and number at
least MIN_PX.

Reads CELLS (the stage's arcade 16x16 cells: u16 code, u16 attr, rows of
ROWS_BLOCKS * 16 cells, as tools/stagepack.py), STAGEDIR/map.bin (column-
major u16 words); writes map.bin with bit 15 set on those cells (other bits
untouched) and map_meta.bin again (tools/maptrim.py --meta). Rerunning is
harmless: the bit is set from the arcade data each time, never cleared on a
cell that does not qualify unless RESET=1."""
import os, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402
import zipfile  # noqa: E402

cells_path, rows_blocks, stagedir = sys.argv[1:4]
masks = [int(x, 16) for x in (sys.argv[4] if len(sys.argv) > 4 else "0,3E,FE,1E").split(",")]
FRONT_MIN = float(os.environ.get("FRONT_MIN", "0.5"))
MIN_PX = int(os.environ.get("MIN_PX", "4"))
sd = Path(stagedir)
cells = Path(cells_path).read_bytes()
crow = int(rows_blocks) * 16
ccol = len(cells) // 4 // crow
raw = (sd / "map.bin").read_bytes()
R = crow * 2
C = len(raw) // 2 // R
assert C == ccol * 2, (C, ccol)
words = list(struct.unpack(">%dH" % (C * R), raw))

with zipfile.ZipFile(ROOT / "willow.zip") as z:
    gfx = assemble_region(z)
dec = {}
setn = clr = mixed = 0
for c in range(C):
    for r in range(R):
        i = c * R + r
        if not words[i]:
            continue
        o = ((r // 2) * ccol + c // 2) * 4
        code, attr = struct.unpack_from(">HH", cells, o)
        m = masks[(attr >> 7) & 3]
        front = back = 0
        if m:
            key = 0x4000 + (code & 0x1FFF)
            if key not in dec:
                dec[key] = decode_tile(gfx, key, 16)
            t = dec[key]
            qy, qx = (r % 2) * 8, (c % 2) * 8
            for y in range(8):
                sy = 15 - (qy + y) if attr & 64 else qy + y
                for x in range(8):
                    sx = 15 - (qx + x) if attr & 32 else qx + x
                    p = t[sy][sx]
                    if p == 15:
                        continue
                    if (m >> p) & 1:
                        front += 1
                    else:
                        back += 1
        want = front >= MIN_PX and front >= FRONT_MIN * (front + back)
        if front and back:
            mixed += 1
        if want and not words[i] & 0x8000:
            words[i] |= 0x8000; setn += 1
        elif not want and words[i] & 0x8000 and os.environ.get("RESET"):
            words[i] &= 0x7FFF; clr += 1
(sd / "map.bin").write_bytes(struct.pack(">%dH" % (C * R), *words))
subprocess.run([sys.executable, str(Path(__file__).resolve().parent / "maptrim.py"), "--meta", str(R),
                str(sd / "map.bin"), str(sd / "map_meta.bin")], check=True, stdout=subprocess.DEVNULL)
pri = sum(1 for w in words if w & 0x8000)
print(f"{sd.name}: {setn} cells given the priority bit ({clr} cleared), {pri} in all; {mixed} cells mix front and back pens")
