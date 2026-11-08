#!/usr/bin/env python3
"""wipepack5.py OUTDIR: stage 5's tower doorway wipe (specs/stage5.md 5.6;
class 12 kind 35, 0x32DDC) as Genesis plane-A data.

The arcade writes scroll2 cells from ROM: frame 0 a 2 x 2 block (0x8D6DC)
at scroll2 0x9197DC, then every 8 updates a ring (table 0x32F7C: columns -
1, rows, source; two edge columns of rows + 1 cells each side, the top and
bottom cells of each middle column; frame 6 one-column edges), the write
address stepping 2 columns left and 1 cell up (frame 6: 1 column). This
replays those writes offline. Scroll2 cell (col, row) = ((x >> 4) & 63,
(~y >> 4) & 63) (0x43658): here x = 2048 + col * 16, the cell's top y =
1023 - row * 16.

Colours: each cell's pens take the colours seen where the same cell (code,
palette) appears in the assembled foreground (data/stage5_fg.cells / .ppm);
each 8x8 tile goes on the scenery line (2 or 3, data/stage5/pal.bin) that
shows it best.

Writes wipe5_tiles.bin (deduplicated, flips shared), wipe5_words.bin (the
final 48 x 28 plane-A words, row-major: tile from 0 | line | flips; 0 = not
written), wipe5_frame.bin (per 16-px cell, 24 x 14 row-major, the frame
that writes it), wipe5.h (tile count, origin as map cells)."""
import struct, sys, zipfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402
from intropack import flip, pack_tile  # noqa: E402

RINGS = [(1, 3, 0x8D658), (5, 5, 0x8D5D4), (9, 7, 0x8D550), (13, 9, 0x8D4CC), (17, 11, 0x8D448), (21, 13, 0x8D404)]


def main():
    out = Path(sys.argv[1])
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    u16 = lambda a: struct.unpack_from(">H", rom, a)[0]
    cells = {}                                  # (col, row) -> (code, attr, frame)

    def put(dst, src, frame):
        off = dst - 0x918000
        col, row = (off & 0xFC0) >> 6, ((off & 0x3000) >> 8) | ((off & 0x3C) >> 2)
        cells[col, row] = (u16(src), u16(src + 2), frame)

    # Frame 0: 2 columns of 2 cells.
    dst = 0x9197DC
    for k in range(2):
        for j in range(2):
            put(dst + k * 64 + j * 4, 0x8D6DC + k * 64 + j * 4, 0)
    dst -= 132
    for f, (cm1, rows, src) in enumerate(RINGS, 1):
        edge = 1 if f == 6 else 2
        a0, a1 = dst, src
        for k in range(edge):                   # left edge columns, rows + 1 cells
            for j in range(rows + 1):
                put(a0 + j * 4, a1 + j * 4, f)
            a0 += 64; a1 += 64
        for k in range(cm1 + 1):                # middle columns: top and bottom cells
            put(a0, a1, f)
            put(a0 + rows * 4, a1 + rows * 4, f)
            a0 += 64; a1 += 64
        for k in range(edge):                   # right edge columns
            for j in range(rows + 1):
                put(a0 + j * 4, a1 + j * 4, f)
            a0 += 64; a1 += 64
        dst -= 132
        if f + 1 == 6:
            dst += 64
    cols = sorted({c for c, _ in cells}); rws = sorted({r for _, r in cells})
    c0, r0 = cols[0], rws[0]
    W, H = cols[-1] - c0 + 1, rws[-1] - r0 + 1
    print(f"{len(cells)} cells written, rectangle {W} x {H} from scroll2 ({c0}, {r0})")
    # Colours from the assembled foreground.
    fc = (GAME / "data/stage5_fg.cells").read_bytes()
    ppm = (GAME / "data/stage5_fg.ppm").read_bytes()
    head = ppm.split(b"\n", 3); pw, ph = map(int, head[1].split()); px = head[3]
    mcols = pw // 16
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    pens = {}
    def pen_grid(code, attr):
        key = (code, attr & 0x60)
        if key not in pens:
            t = decode_tile(gfx, 0x4000 + (code & 0x1FFF), 16)
            pens[key] = [[t[15 - y if attr & 64 else y][15 - x if attr & 32 else x] for x in range(16)] for y in range(16)]
        return pens[key]
    want = {a & 31 for _, a, _ in cells.values()}
    votes = {}
    for i in range(len(fc) // 4):
        code, attr = struct.unpack_from(">HH", fc, i * 4)
        if attr & 31 not in want:
            continue
        g = pen_grid(code, attr)
        cx, cy = (i % mcols) * 16, (i // mcols) * 16
        for y in range(16):
            for x in range(16):
                p = g[y][x]
                if p == 15:
                    continue
                j = ((cy + y) * pw + cx + x) * 3
                votes.setdefault((attr & 31, p), Counter())[tuple(px[j:j + 3])] += 1
    colour = {k: c.most_common(1)[0][0] for k, c in votes.items()}
    q3 = lambda c: tuple(v >> 5 for v in c)
    pal = struct.unpack(">32H", (GAME / "data/stage5/pal.bin").read_bytes())
    rgb = lambda wd: ((wd >> 1) & 7, (wd >> 5) & 7, (wd >> 9) & 7)
    lines = {2: [rgb(w) for w in pal[0:16]], 3: [rgb(w) for w in pal[16:32]]}
    d = lambda a, b: sum((x - y) ** 2 for x, y in zip(a, b))
    missing = Counter()
    tiles, index, words = [], {}, [0] * (W * 2 * H * 2)
    frames = [255] * (W * H)
    used = Counter()
    for (col, row), (code, attr, f) in cells.items():
        cx, cy = col - c0, row - r0
        frames[cy * W + cx] = f
        g = pen_grid(code, attr)
        grp = attr & 31
        for ty in range(2):
            for tx in range(2):
                cell = [g[ty * 8 + y][tx * 8 + x] for y in range(8) for x in range(8)]
                cols_ = []
                for p in cell:
                    if p == 15:
                        cols_.append(None)
                    elif (grp, p) in colour:
                        cols_.append(q3(colour[grp, p]))
                    else:
                        missing[grp, p] += 1; cols_.append((0, 0, 0))
                best = None
                for ln, cl in lines.items():
                    m = [0 if c is None else min(range(1, 16), key=lambda i: d(c, cl[i])) for c in cols_]
                    e = sum(d(c, cl[i]) for c, i in zip(cols_, m) if c is not None)
                    if best is None or e < best[0]:
                        best = (e, ln, m)
                _, ln, t = best
                used[ln] += 1
                t = tuple(t)
                for fl in range(4):
                    key = tuple(flip(list(t), fl))
                    if key in index:
                        w = index[key] | (fl << 11) | (ln << 13); break
                else:
                    index[t] = len(tiles); tiles.append(t); w = index[t] | (ln << 13)
                words[(cy * 2 + ty) * W * 2 + cx * 2 + tx] = w | 0x0000
    if missing:
        print("pens without a colour (black):", dict(missing))
    print("tiles per line:", dict(used), "unique tiles:", len(tiles))
    # Origin as plane map cells (8 px): x = 2048 + col * 16; picture row = row - 16.
    mc, mr = (2048 + c0 * 16) // 8, (r0 - 16) * 2
    out.mkdir(parents=True, exist_ok=True)
    (out / "wipe5_tiles.bin").write_bytes(b"".join(pack_tile(list(t)) for t in tiles))
    (out / "wipe5_words.bin").write_bytes(struct.pack(f">{len(words)}H", *words))
    (out / "wipe5_frame.bin").write_bytes(bytes(frames))
    (out / "wipe5.h").write_text("/* generated by tools/wipepack5.py */\n"
                                 f"#define WIPE5_TILES {len(tiles)}\n#define WIPE5_W {W}\n#define WIPE5_H {H}\n"
                                 f"#define WIPE5_C {mc}\n#define WIPE5_R {mr}\n")
    print(f"wipe5: {W} x {H} cells at map cell ({mc}, {mr}), {len(tiles)} tiles")


if __name__ == "__main__":
    main()
