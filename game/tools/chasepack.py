#!/usr/bin/env python3
"""chasepack.py CHASEDUMP.cps OUTDIR: stage 2's road and far layer
(specs/stage2.md 2.4, 2.6, 6.1) as ready Genesis data. Offline only. Run
after tools/stagepack.py (SPLIT_LINES=1: the fort on line 2): this replaces
the far layer it packed and line 3 of its palette.

Road (plane A): the road row of grid 1 is a sequence of six 256-px blocks
(110..115, 0x2DD24 + 0x100 + 7..207); each block's 16x16 cells (ROM 0x80000
+ b * 0x400) are decoded in CHASEDUMP's scroll2 colours and fitted to CRAM
line 2 (15 colours). Writes road_tiles.bin, road_blocks.bin (6 blocks x 32
columns x 32 rows of plane words, column-major; tile index CHASE_ROAD_BASE + n | line 2 |
flips; rows = map rows 32..63, world y 255..0) and road_seq.bin (block kind
0..5 per 256-px step of x, 0xFF = not road). The road takes the fort's line
2 and the pool slots the fort never uses (ROAD_BUDGET, merged to fit).

Far layer (plane B): scroll3 grid 1, 4096 px repeating, FAR_TOP rows above
the camera-0 view (as tools/farlayer.py), the whole stage's. Its colours change with the chase distance (2.6: ten steps,
scroll3 palettes 1, 3-8 from ROM tables 0xEC4C0.. + 32 * step), so its
(group, pen) entries are clustered into 15 pens on line 3 by their colours
at all ten steps together; far_pal.bin holds line 3 for each step (10 x 16
words; step 0 also goes into pal.bin line 3). bd_tiles.bin (merged to
FAR_BUDGET tiles), bd_map.bin (512 x 45 words, column-major, BD_BASE + tile |
line 3 | flips)."""
import os, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402
from cps1_source_layers import SourceLayers  # noqa: E402
from cps1_palette import cps_rgb  # noqa: E402
from intropack import q3, cram_word, dist, kmeans, merge, flip, pack_tile  # noqa: E402

FAR_TOP = 136
FAR_W = 4096
FAR_BUDGET = int(os.environ.get("FAR_BUDGET", "339"))
ROAD_BUDGET = int(os.environ.get("ROAD_BUDGET", "290"))   # 687 pool slots - the fort's 397
BD_BASE = 703
FAR_ROWS = (224 + FAR_TOP) // 8
STEP_TABLES = {3: 0xEC4C0, 7: 0xEC600, 1: 0xEC740, 4: 0xEC880, 5: 0xEC9C0, 6: 0xECB00, 8: 0xECC40}


def main():
    dump, outdir = sys.argv[1:3]
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    u16 = lambda a: struct.unpack_from(">H", rom, a)[0]
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    raw = Path(dump).read_bytes()[0x80:]
    pal_ram = raw[0x1C000:0x1C000 + 0x2000]
    word = lambda i: int.from_bytes(pal_ram[i * 2:i * 2 + 2], "big")

    # ---- Road.
    grid = struct.unpack_from(">I", rom, 0x2DBF8 + 4)[0]
    seq = [rom[grid + 0x100 + i] for i in range(256)]
    kinds = list(range(110, 116))
    road_seq = bytes(kinds.index(b) if b in kinds else 0xFF for b in seq)
    p2 = lambda g, p: q3(cps_rgb(word(0x400 + g * 16 + p)))
    blocks = []                                     # per kind: 32 x 32 cells of 64 rgb (None = transparent)
    dec = {}
    for b in kinds:
        cells = {}
        for cx in range(16):
            for cy in range(16):
                o = 0x80000 + b * 0x400 + (cx * 16 + cy) * 4
                code, attr = u16(o), u16(o + 2)
                key = 0x4000 + (code & 0x1FFF)
                if key not in dec: dec[key] = decode_tile(gfx, key, 16)
                t = dec[key]
                for qy in (0, 8):
                    for qx in (0, 8):
                        px = []
                        for y in range(8):
                            sy = 15 - (qy + y) if attr & 64 else qy + y
                            for x in range(8):
                                sx = 15 - (qx + x) if attr & 32 else qx + x
                                pen = t[sy][sx]
                                px.append(None if pen == 15 else p2(attr & 31, pen))
                        cells[(cx * 2 + qx // 8, cy * 2 + qy // 8)] = px
        blocks.append(cells)
    stage_pal = struct.unpack(">32H", (out / "pal.bin").read_bytes())
    line2 = [((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7) for w in stage_pal[1:16]]
    tid, tiles, bwords, cellref = {}, [], [], []
    for cells in blocks:
        for c in range(32):
            for r in range(32):
                px = cells[(c, r)]
                if all(p is None for p in px): cellref.append(None); continue
                pens = tuple(0 if p is None else 1 + min(range(15), key=lambda i: dist(p, line2[i])) for p in px)
                for f in range(4):
                    k = tuple(flip(list(pens), f))
                    if k in tid: cellref.append((tid[k], f)); break
                else:
                    tid[pens] = len(tiles); tiles.append(pens); cellref.append((tid[pens], 0))
    print(f"road: {len(tiles)} distinct tiles")
    if len(tiles) > ROAD_BUDGET:
        w = [0] * len(tiles)
        for r in cellref:
            if r: w[r[0]] += 1
        m = merge([[(0, 0, 0) if p == 0 else line2[p - 1] for p in t] for t in tiles], w, ROAD_BUDGET)
        keep = sorted(set(m[i][0] for i in range(len(tiles))))
        remap = {i: n for n, i in enumerate(keep)}
        cellref = [None if r is None else (remap[m[r[0]][0]], r[1] ^ m[r[0]][1]) for r in cellref]
        tiles = [tiles[i] for i in keep]
    road_base = 16 + 687 - ROAD_BUDGET                # VRAM tile of road tile 0 (CHASE_ROAD_BASE): stored in the words
    bwords = [0 if r is None else (road_base + r[0]) | (r[1] << 11) | (2 << 13) for r in cellref]
    (out / "road_tiles.bin").write_bytes(b"".join(pack_tile(list(t)) for t in tiles))
    (out / "road_blocks.bin").write_bytes(struct.pack(">%dH" % len(bwords), *bwords))
    (out / "road_seq.bin").write_bytes(road_seq)
    print(f"road: {len(tiles)} tiles on the fort's line 2")

    # ---- Far layer, ten colour steps.
    sl = SourceLayers(rom, gfx, 1)
    def step_colour(g, p, s):
        if g in STEP_TABLES:
            return q3(cps_rgb(0xF000 | u16(STEP_TABLES[g] + 32 * s + p * 2)))   # stored without the brightness nibble
        return q3(cps_rgb(word(0x600 + g * 16 + p)))
    ent = {}                                        # (group, pen) -> pixel count
    pix = []                                        # per cell: 64 entries or None
    for cy in range(FAR_ROWS):
        for cx in range(FAR_W // 8):
            cell = []
            for y in range(8):
                for x in range(8):
                    pen, g = sl.pen(3, cx * 8 + x, (cy * 8 + y) - FAR_TOP - 240)
                    if pen == 15: cell.append(None); continue
                    cell.append((g, pen)); ent[(g, pen)] = ent.get((g, pen), 0) + 1
            pix.append(cell)
    vec = {e: [c for s in range(10) for c in step_colour(e[0], e[1], s)] for e in ent}
    # k-means over the 30-value vectors (weighted), 15 pens
    items = sorted(ent, key=lambda e: -ent[e])
    cent = [vec[items[0]]]
    d2 = lambda a, b: sum((x - y) ** 2 for x, y in zip(a, b))
    while len(cent) < min(15, len(items)):
        cent.append(vec[max(items, key=lambda e: ent[e] * min(d2(vec[e], c) for c in cent))])
    for _ in range(12):
        acc = [[0.0] * 31 for _ in cent]
        for e in items:
            j = min(range(len(cent)), key=lambda i: d2(vec[e], cent[i]))
            for k in range(30): acc[j][k] += vec[e][k] * ent[e]
            acc[j][30] += ent[e]
        cent = [[a[k] / a[30] for k in range(30)] if a[30] else cent[i] for i, a in enumerate(acc)]
    pen_of = {e: 1 + min(range(len(cent)), key=lambda i: d2(vec[e], cent[i])) for e in items}
    steps = []
    for s in range(10):
        line = [0] + [cram_word(tuple(min(7, max(0, round(c[s * 3 + k]))) for k in range(3))) for c in cent]
        line += [0] * (16 - len(line))
        steps += line
    (out / "far_pal.bin").write_bytes(struct.pack(">160H", *steps))
    # tiles: pens, deduped with flips, then merged to the budget by their step-0 colours
    step0 = [(0, 0, 0)] + [tuple(round(c[k]) for k in range(3)) for c in cent]
    cells = [tuple(0 if e is None else pen_of[e] for e in cell) for cell in pix]
    uniq, rep = {}, []
    for t in cells:
        for f in range(4):
            k = tuple(flip(list(t), f))
            if k in uniq: rep.append((uniq[k], f)); break
        else:
            uniq[t] = len(uniq); rep.append((uniq[t], 0))
    order = sorted(uniq, key=lambda k: uniq[k])
    print(f"far: {len(order)} distinct tiles")
    if len(order) > FAR_BUDGET:
        w = [0] * len(order)
        for r, _ in rep: w[r] += 1
        m = merge([[step0[p] for p in t] for t in order], w, FAR_BUDGET)
        rep = [(m[r][0], f ^ m[r][1]) for r, f in rep]
        keep = sorted(set(i for i, _ in rep))
        remap = {i: n for n, i in enumerate(keep)}
        order = [order[i] for i in keep]
        rep = [(remap[r], f) for r, f in rep]
    fw = []
    for cx in range(FAR_W // 8):
        for cy in range(FAR_ROWS):
            r, f = rep[cy * (FAR_W // 8) + cx]
            fw.append((BD_BASE + r) | (f << 11) | (3 << 13))
    (out / "bd_tiles.bin").write_bytes(b"".join(pack_tile(list(t)) for t in order))
    (out / "bd_map.bin").write_bytes(struct.pack(">%dH" % len(fw), *fw))
    stage_pal = list(struct.unpack(">32H", (out / "pal.bin").read_bytes()))
    stage_pal[16:32] = steps[0:16]
    (out / "pal.bin").write_bytes(struct.pack(">32H", *stage_pal))
    # The background colour: the far layer's commonest pen in its top rows (the sky).
    cnt = {}
    for cell in pix[:(FAR_W // 8) * 4]:
        for e in cell:
            if e is not None: cnt[pen_of[e]] = cnt.get(pen_of[e], 0) + 1
    sky = 48 + max(cnt, key=cnt.get)
    with open(out / "chase.h", "w") as h:
        h.write("/* generated by tools/chasepack.py */\n")
        h.write(f"#define CHASE_ROAD_TILES {len(tiles)}\n#define CHASE_FAR_TILES {len(order)}\n")
        h.write(f"#define CHASE_FAR_COLS {FAR_W // 8}\n#define CHASE_SKY {sky}\n#define CHASE_ROAD_BASE {16 + 687 - ROAD_BUDGET}\n")
    print(f"far: {len(order)} tiles after merging, 15 pens x 10 steps on line 3")


if __name__ == "__main__":
    main()
