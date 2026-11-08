#!/usr/bin/env python3
"""torchpack5.py OUTDIR: stage 5's interior wall torches (specs/stage5.md
5.7; class 12 kind 6 variant 5, 0x333FA): three frames of a 2 x 2 block of
16-px scroll2 cells (table 0x333E2, 8 bytes a frame: top-left, bottom-left,
top-right, bottom-right, as scroll2 RAM runs down a column; palette 2), as Genesis tiles, each 8x8 tile on the line that shows it best: the
stage's scenery lines 2-3 (data/stage5/pal.bin) or its fixed sprite line 1
(data/s5spr/sprpal.bin, warm colours for the flame), nearest colours. The arcade colours come from the assembled foreground
(data/stage5_fg.ppm), where every torch shows frame 0: each pen's colour is
read there.
Writes torch5_tiles.bin (deduplicated, flips shared), torch5_words.bin (3
frames x 4 rows x 4 words, row-major: tile index from 0 | line | flips) and
torch5.h."""
import struct, sys, zipfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402
from intropack import flip, pack_tile  # noqa: E402

FRAMES = 0x333E2
FLAME = {1, 2, 3, 4}             # the flame's pens (pale yellow, orange, amber, warm grey)
WORLD_TOP = 767
TORCHES = [(4432, 592), (4528, 592), (4576, 592), (4720, 528), (4656, 480), (4368, 368),
           (4512, 304), (4560, 208), (4432, 96), (4560, 96), (4688, 96)]   # src/stage5_objects.h


def main():
    out = Path(sys.argv[1])
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    u16 = lambda a: struct.unpack_from(">H", rom, a)[0]
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    frames = [[u16(FRAMES + f * 8 + k * 2) for k in range(4)] for f in range(3)]
    blocks = []
    for codes in frames:
        block = {}
        for k, code in enumerate(codes):
            t = decode_tile(gfx, 0x4000 + code, 16)
            bx, by = (k >> 1) * 16, (k & 1) * 16     # scroll2 entries run down a column first
            for y in range(16):
                for x in range(16):
                    block[(bx + x, by + y)] = t[y][x]
        blocks.append(block)
    # Pen colours from the foreground picture (frame 0 at each torch).
    ppm = (GAME / "data/stage5_fg.ppm").read_bytes()
    head = ppm.split(b"\n", 3)
    w, h = map(int, head[1].split()); px = head[3]
    votes = {}
    match = total = 0
    for tx, ty in TORCHES:
        x0, y0 = tx & ~15, (WORLD_TOP - ty) & ~15
        for (x, y), p in blocks[0].items():
            if p == 15: continue
            i = ((y0 + y) * w + x0 + x) * 3
            votes.setdefault(p, Counter())[tuple(px[i:i + 3])] += 1
    colour = {p: c.most_common(1)[0][0] for p, c in votes.items()}
    for p, c in votes.items():
        match += c.most_common(1)[0][1]; total += sum(c.values())
    pens = {p for b in blocks for p in b.values() if p != 15}
    missing = sorted(pens - set(colour))
    print(f"pen colours from the picture: {len(colour)} pens, {100 * match / max(1, total):.1f}% agreement; "
          f"pens only in frames 1-2: {missing}")
    if missing:
        sys.exit("pens without a colour")
    q3 = lambda c: tuple(v >> 5 for v in c)
    pal = struct.unpack(">32H", (GAME / "data/stage5/pal.bin").read_bytes())
    spr = struct.unpack(">32H", (GAME / "data/s5spr/sprpal.bin").read_bytes())
    lines = {1: spr[16:32], 2: pal[0:16], 3: pal[16:32]}   # the stage's fixed sprite line 1 has the flame's warm colours
    rgb = lambda wd: ((wd >> 1) & 7, (wd >> 5) & 7, (wd >> 9) & 7)
    d = lambda a, b: sum((x - y) ** 2 for x, y in zip(a, b))
    maps = {}
    for line, words16 in lines.items():
        cols = [rgb(words16[i]) for i in range(16)]
        maps[line] = {p: min(((d(q3(colour[p]), cols[1 + i]), 1 + i) for i in range(15)))
                      for p in pens}
    tiles, index, words = [], {}, []
    used = Counter()
    for block in blocks:
        for r in range(4):
            for c in range(4):
                cell = [block[(c * 8 + x, r * 8 + y)] for y in range(8) for x in range(8)]
                # each 8x8 tile on the line that shows it best (the flame's pens count 4 times)
                line = min(lines, key=lambda l: sum(maps[l][p][0] * (4 if p in FLAME else 1) for p in cell if p != 15))
                used[line] += 1
                t = tuple(0 if p == 15 else maps[line][p][1] for p in cell)
                for fl in range(4):
                    key = tuple(flip(list(t), fl))
                    if key in index:
                        words.append(index[key] | (fl << 11) | (line << 13)); break
                else:
                    index[t] = len(tiles); tiles.append(t); words.append(index[t] | (line << 13))
    print("tiles per line:", dict(used))
    out.mkdir(parents=True, exist_ok=True)
    (out / "torch5_tiles.bin").write_bytes(b"".join(pack_tile(list(t)) for t in tiles))
    (out / "torch5_words.bin").write_bytes(struct.pack(">48H", *words))
    (out / "torch5.h").write_text(f"/* generated by tools/torchpack5.py */\n#define TORCH5_TILES {len(tiles)}\n")
    print(f"torch5: {len(tiles)} tiles for 3 frames")


if __name__ == "__main__":
    main()
