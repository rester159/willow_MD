#!/usr/bin/env python3
"""fgstats.py CELLS ROWS: unique 8x8 foreground tiles (flip-canonical, by
palette group) over a stage map from stagemap.py, and the peak over any
352-pixel-wide full-height strip (a screen plus a 2-column streaming margin)."""
import sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402

def main():
    cells = Path(sys.argv[1]).read_bytes(); rows = int(sys.argv[2]) * 16
    cols = len(cells) // 4 // rows
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    cache = {}
    colsets = []                          # per 8-px column: set of tile keys
    groups = set()
    for c in range(cols):
        left, right = set(), set()
        for r in range(rows):
            o = (r * cols + c) * 4
            code, attr = int.from_bytes(cells[o:o + 2], "big"), int.from_bytes(cells[o + 2:o + 4], "big")
            key = 0x4000 + (code & 0x1FFF)
            if key not in cache:
                cache[key] = decode_tile(gfx, key, 16)
            t = cache[key]
            for qy in (0, 8):
                for qx in (0, 8):
                    q = tuple(tuple(t[(15 - (qy + y)) if attr & 64 else qy + y][(15 - (qx + x)) if attr & 32 else qx + x]
                                    for x in range(8)) for y in range(8))
                    if all(p == 15 for row in q for p in row):
                        continue
                    forms = [q, tuple(rw[::-1] for rw in q), q[::-1], tuple(rw[::-1] for rw in q[::-1])]
                    k = (min(forms), attr & 31)
                    groups.add(attr & 31)
                    (left if (qx == 0) ^ bool(attr & 32) else right).add(k)
        colsets += [left, right]
    allk = set().union(*colsets)
    win = 352 // 8
    peak = max(len(set().union(*colsets[i:i + win])) for i in range(len(colsets) - win + 1))
    print(f"{cols * 16} x {rows * 16} px: {len(allk)} unique 8x8 tiles, {len({k[0] for k in allk})} ignoring palette; "
          f"peak {peak} in a 352-px strip; palette groups {sorted(groups)}")

if __name__ == "__main__":
    main()
