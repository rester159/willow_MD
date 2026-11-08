#!/usr/bin/env python3
"""logopack.py OUTDIR: the arcade's WILLOW title (the attract loop's logo
picture, data/attract/att_logo_*, rows 0-10) for the opening menu, on CRAM
lines 2-3 only (the menu's text owns lines 0-1).

The picture's line-1 tiles move to line 3: their pens are renumbered into
line 3's free entries (a colour line 3 already holds reuses that entry).
Writes logo_tiles.bin (the rows' tiles, renumbered from 0; remapped copies
appended), logo_map.bin (11 x 40 plane words: tile from 0 | line | flips; 0
= empty) and logo_pal.bin (lines 2 and 3, 32 CRAM words)."""
import struct, sys
from pathlib import Path

GAME = Path(__file__).resolve().parents[1]
ROWS = 11


def main():
    out = Path(sys.argv[1])
    t = (GAME / "data/attract/att_logo_tiles.bin").read_bytes()
    m = struct.unpack(">1120H", (GAME / "data/attract/att_logo_map.bin").read_bytes())
    pal = list(struct.unpack(">64H", (GAME / "data/attract/att_logo_pal.bin").read_bytes()))
    words = m[:ROWS * 40]
    pens = lambda ti: {p for b in t[ti * 32:ti * 32 + 32] for p in (b >> 4, b & 15)} - {0}
    used = {1: set(), 2: set(), 3: set()}
    for w in words:
        if w & 0x7FF:
            used[(w >> 13) & 3] |= pens(w & 0x7FF)
    line3 = pal[48:64]
    free = [p for p in range(1, 16) if p not in used[3]]
    remap = {}
    for p in sorted(used[1]):
        c = pal[16 + p]
        same = [q for q in used[3] if line3[q] == c]
        if same:
            remap[p] = same[0]
        else:
            q = free.pop(0)
            line3[q] = c
            remap[p] = q
            used[3].add(q)
    tiles, index, outw = [bytes(32)], {}, []
    for w in words:
        ti, ln = w & 0x7FF, (w >> 13) & 3
        if not ti:
            outw.append(0)
            continue
        data = t[ti * 32:ti * 32 + 32]
        if ln == 1:
            data = bytes(((remap.get(b >> 4, 0) if b >> 4 else 0) << 4) | (remap.get(b & 15, 0) if b & 15 else 0) for b in data)
            ln = 3
        if data not in index:
            index[data] = len(tiles)
            tiles.append(data)
        outw.append(index[data] | (w & 0x1800) | (ln << 13))
    out.mkdir(parents=True, exist_ok=True)
    (out / "logo_tiles.bin").write_bytes(b"".join(tiles))
    (out / "logo_map.bin").write_bytes(struct.pack(f">{len(outw)}H", *outw))
    (out / "logo_pal.bin").write_bytes(struct.pack(">32H", *(pal[32:48] + line3)))
    print(f"logo: {len(tiles)} tiles (tile 0 blank), line-1 pens -> line 3 {remap}")


if __name__ == "__main__":
    main()
