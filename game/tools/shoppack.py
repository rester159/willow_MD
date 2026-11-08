#!/usr/bin/env python3
"""shoppack.py SHOPDUMP.cps OUTDIR: the shop screen (specs/stage1_shops_companion.md
1.3-1.5) as ready Genesis data. Offline only: nothing of the arcade program runs.

Background: SHOPDUMP's scroll2 (the shelf, item icon cells left out: black)
under scroll3 (curtains, boxes, the shopkeeper's window), on the full
384-pixel CPS line, recomposed to 320: above the text box only plain curtain
columns are dropped (64 of 72), so every box and the window keep their
pixels; the text box loses 64 columns (its outer curtain and the middle of
its empty interior). Lines 2 and 3 fitted, line 1 =
the sprites'/HUD's fixed line (usable too); line 0 fitted to the icon
colours the other lines serve worst. The owl's blink frames 1 and 2 (table 0x34B34) are packed
as alternate cells; the big price glyphs (G, 0-9, text palette 9) and YES/NO
letters (palette 0x0E) as 2x2-cell tiles on the box's black.

Sprites (each 16x16 quarter on its best line, 0-3): every item icon
(28, ROM tables 0x34046 / 0x3407E / 0x341BE: tall icons show their middle 32
rows, as the arcade's boxes clip them), the cursor pieces (objects 0x0F,
0x1F, 0x2F, 0x3F) and the bought marker pieces (0x6F, 0x4F, 0x7F, 0x5F),
palette 8.

Writes: tiles.bin, map.bin (40x28 words: tile from 0 | line << 13 | flips),
alt.bin (owl frames 1, 2: per frame SHOP_OWL_CELLS words, cells in
shop.h), pal.bin (64 CRAM words), spr.bin (sprite tiles: 28 icons x 16,
then 8 pieces x 4; per 16x16 quarter, column-major), shop.h."""
import os, struct, sys
from pathlib import Path

os.environ["CPSFULL"] = "1"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from intropack import (ROOT, q3, cram_word, cram, dist, kmeans, picture, pack_tile, cpsframe,
                       decode_tile, cps_rgb)  # noqa: E402

W, H, LEFT, TOP = 384, 224, 64, 16
SPLIT = 146                     # first row of the text box band
BUDGET = 1049                   # VRAM tiles 16..1064 (the stage pool and far layer, idle in the shop)
KEEP = [2, 1, 1, 1, 1, 2]       # curtain columns kept per run above the box
BOT_DROP = list(range(0, 12)) + list(range(172, 212)) + list(range(372, 384))
ICON_X = [16, 64, 288, 336]     # CPS screen x of the item columns (cursor table 0x3498A - 64)
ICON_Y = [32, 72, 112]
OWL = 0x917AA8


def layer(d, n, skip=()):
    """scroll layer n of dump d as {(x, y): palette index}, cells at skip addresses left out."""
    base = (d.reg(0x800100 + 2 * n) << 8) & 0xFFFFFF
    size = {2: 16, 3: 32}[n]
    sx = d.reg({2: 0x800110, 3: 0x800114}[n])
    sy = d.reg({2: 0x800112, 3: 0x800116}[n])
    page = {2: 0x400, 3: 0x600}[n]
    out = {}
    for y in range(H):
        yy = (y + TOP + sy) % (64 * size)
        row = yy // size
        for x in range(W):
            xx = (x + LEFT + sx) % (64 * size)
            col = xx // size
            idx = (row & 15) + (col << 4) + ((row & 48) << 6) if n == 2 else (row & 7) + (col << 3) + ((row & 56) << 6)
            a = base + idx * 4
            if a in skip:
                out[x, y] = None
                continue
            code, attr = d.word(a), d.word(a + 2)
            if n == 2:
                t = cpsframe.tile(0x4000 + (code & 0x1FFF), 16)
            else:
                code &= 0x3FFF
                if not 0xA00 <= code <= 0xDFF: continue
                t = cpsframe.tile(code, 32)
            px, py = xx % size, yy % size
            pen = t[size - 1 - py if attr & 64 else py][size - 1 - px if attr & 32 else px]
            if pen != 15: out[x, y] = page + (attr & 31) * 16 + pen
    return out


def main():
    dump, outdir = sys.argv[1:3]
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    u16 = lambda a: struct.unpack_from(">H", rom, a)[0]
    d = cpsframe.Dump(dump)
    pal = [q3(cps_rgb(d.word(0x91C000 + 2 * i))) for i in range(0x1000)]

    skip = set()
    for s in range(12):
        a = (u16(0x340B6 + s * 4) << 16) | u16(0x340B6 + s * 4 + 2)
        for c in range(2):
            for r in range(3): skip.add(a + c * 64 + r * 4)

    def frame(owl):
        vid = bytearray(d.vid)
        for k, off in enumerate((0, 4, 0x20, 0x24)):
            o = OWL + off - 0x900000
            vid[o:o + 4] = struct.pack(">HH", u16(0x34B34 + owl * 8 + k * 2), 2)
        d2 = cpsframe.Dump.__new__(cpsframe.Dump)
        d2.regs, d2.vid = d.regs, bytes(vid)
        s2, s3 = layer(d2, 2, skip), layer(d2, 3)
        img = {}
        for y in range(H):
            for x in range(W):
                v = s3.get((x, y))
                if v is None: v = s2.get((x, y))
                img[x, y] = pal[v] if v is not None else (0, 0, 0)
        return img

    imgs = [frame(f) for f in range(3)]

    # Column maps: native x -> CPS x, per band.
    red = lambda c: c[0] > c[1] + 1 and c[0] > c[2] + 1
    curtain = [all(red(imgs[0][x, y]) for y in range(16, SPLIT)) for x in range(W)]
    top, run, k = [], [], 0              # the first KEEP[i] columns of the i-th curtain run
    for x in range(W + 1):
        if x < W and curtain[x]: run.append(x); continue
        if run: top += run[:KEEP[k]]; k += 1; run = []
        if x < W: top.append(x)
    assert k == len(KEEP) and len(top) == 320, (k, len(top))
    bot = [x for x in range(W) if x not in BOT_DROP]
    assert len(bot) == 320

    def native(img):
        return {(x, y): img[(top if y < SPLIT else bot)[x], y] for y in range(H) for x in range(320)}

    nat = [native(i) for i in imgs]
    owl_cells = [i for i in range(1120) if any(nat[f][(i % 40) * 8 + x, (i // 40) * 8 + y] != nat[0][(i % 40) * 8 + x, (i // 40) * 8 + y]
                                               for f in (1, 2) for y in range(8) for x in range(8))]

    # Big glyphs (2x2 cells each) on the box's black.
    def apal(p):
        return [pal[0x200 + p * 16 + i] for i in range(16)]
    gfx = cpsframe.gfx()
    digit = [0x7B22] + [0x7B00 + 2 * k for k in range(8)] + [0x7B20]
    glyphs = [(0x768C, 9)] + [(c, 9) for c in digit] + [(c, 0x0E) for c in (0x76E0, 0x7688, 0x76C4, 0x76AA, 0x76AC)]

    # ---- Sprites.
    def obj16(code, p):
        t = cpsframe.tile(code, 16)
        return [[None if t[y][x] == 15 else pal[p * 16 + t[y][x]] for x in range(16)] for y in range(16)]
    icons = []
    for item in range(28):
        code, attr, tall = u16(0x34046 + item * 2), u16(0x3407E + item * 2), rom[0x341BE + item]
        g = pal[0x400 + (attr & 31) * 16: 0x400 + (attr & 31) * 16 + 16]
        img = [[None] * 32 for _ in range(48)]
        for ty in range(3 if tall else 2):
            for tx in range(2):
                t = cpsframe.tile(0x4000 + ((code + tx + ty * 8) & 0x1FFF), 16)
                for y in range(16):
                    for x in range(16):
                        if t[y][x] != 15: img[ty * 16 + y][tx * 16 + x] = g[t[y][x]]
        icons.append(img[8:40] if tall else img[:32])
    pieces = [obj16(c, 8) for c in (0x0F, 0x1F, 0x2F, 0x3F, 0x6F, 0x4F, 0x7F, 0x5F)]
    # Every 16x16 quarter of the icons and the cursor/marker pieces joins
    # the palette fit (transparent pixels as the boxes' black).
    quarters = [[[im[qy * 16 + y][qx * 16 + x] for x in range(16)] for y in range(16)]
                for im in icons for qy in (0, 1) for qx in (0, 1)] + pieces

    big = dict(nat[0])
    rows_extra = 0
    def put_cell(k, cell):
        for y in range(8):
            for x in range(8):
                big[(k % 40) * 8 + x, H + (k // 40) * 8 + y] = cell[y * 8 + x]
    k = 0
    for f in (1, 2):
        for i in owl_cells:
            put_cell(k, [nat[f][(i % 40) * 8 + x, (i // 40) * 8 + y] for y in range(8) for x in range(8)]); k += 1
    while k % 40: put_cell(k, [(0, 0, 0)] * 64); k += 1
    gk = k
    for code, p in glyphs:
        ap = apal(p)
        for dc in (0, 1, 16, 17):
            px = decode_tile(gfx, code + dc, 8)
            put_cell(k, [(0, 0, 0) if pen == 15 else ap[pen] for row in px for pen in row]); k += 1
    qk = k
    nrows = (k + 39) // 40
    for kk in range(k, nrows * 40): put_cell(kk, [(0, 0, 0)] * 64)

    sprpal = struct.unpack(">32H", (ROOT / "game/data/s1spr/sprpal.bin").read_bytes())
    lines = [[None] * 16, [None] + [cram(w) for w in sprpal[17:32]], [None] * 16, [None] * 16]
    tiles, words, n = picture(big, 320, H + nrows * 8, lines, {2: list(range(1, 16)), 3: list(range(1, 16))}, BUDGET)
    nalt = len(owl_cells)
    n = max(w & 0x7FF for w in words[:1120 + qk]) + 1       # the icons' cells only steered the fit
    assert n <= BUDGET, n
    tiles = tiles[:n * 32]
    (out / "tiles.bin").write_bytes(tiles)
    (out / "map.bin").write_bytes(struct.pack(">1120H", *words[:1120]))
    (out / "alt.bin").write_bytes(struct.pack(">%dH" % (2 * nalt), *words[1120:1120 + 2 * nalt]))
    gwords = words[1120 + gk:1120 + gk + 4 * len(glyphs)]

    def err(im, ln):
        return sum(min(dist(c, x) for x in ln) for r in im for c in r if c is not None)
    # Line 0: the icon colours lines 1-3 serve worst.
    hist = {}
    for q in quarters:
        for r in q:
            for c in r:
                if c is not None and min(dist(c, x) for l in (1, 2, 3) for x in lines[l][1:]) > 2:
                    hist[c] = hist.get(c, 0) + 1
    l0 = kmeans(hist, 15) if hist else []
    lines[0] = [None] + l0 + [(0, 0, 0)] * (15 - len(l0))
    ln4 = [lines[l][1:] for l in range(4)]
    qline = [min(range(4), key=lambda l: err(q, ln4[l])) for q in quarters]

    def stiles(im, ln):
        b = b""
        for tx in range(len(im[0]) // 8):
            for ty in range(len(im) // 8):
                b += pack_tile([0 if im[ty * 8 + y][tx * 8 + x] is None else
                                1 + min(range(15), key=lambda i: dist(im[ty * 8 + y][tx * 8 + x], ln[i]))
                                for y in range(8) for x in range(8)])
        return b
    spr = b"".join(stiles(q, ln4[l]) for q, l in zip(quarters, qline))
    (out / "spr.bin").write_bytes(spr)
    (out / "pal.bin").write_bytes(struct.pack(">64H", *[cram_word(c) if c else 0 for l in lines for c in l]))

    def nx(x):
        return min(range(320), key=lambda i: abs(top[i] - x))
    h = ["/* generated by tools/shoppack.py */",
         f"#define SHOP_TILES {n}",
         f"#define SHOP_OWL_CELLS {nalt}",
         "static const u16 shop_owl_cell[SHOP_OWL_CELLS] = { " + ", ".join(map(str, owl_cells)) + " };",
         "static const s16 shop_icon_x[4] = { " + ", ".join(str(nx(x)) for x in ICON_X) + " };",
         "static const s16 shop_icon_y[3] = { " + ", ".join(map(str, ICON_Y)) + " };",
         "/* CRAM line of each 16x16 quarter (TL, TR, BL, BR) of each item's icon, then of the 8 pieces */",
         "static const u8 shop_icon_line[28][4] = { " + ", ".join("{ %s }" % ", ".join(map(str, qline[i * 4:i * 4 + 4])) for i in range(28)) + " };",
         "static const u8 shop_piece_line[8] = { " + ", ".join(map(str, qline[112:])) + " };",
         "/* big glyphs, 4 words each (TL, TR, BL, BR): G, 0-9, Y, E, S, N, O */",
         "static const u16 shop_glyph[16][4] = { " + ", ".join("{ %s }" % ", ".join("0x%04x" % w for w in gwords[i * 4:i * 4 + 4]) for i in range(16)) + " };"]
    (out / "shop.h").write_text("\n".join(h) + "\n")
    with open(out / "preview.ppm", "wb") as f:
        f.write(b"P6\n320 %d\n255\n" % H)
        f.write(bytes(min(255, v * 36) for y in range(H) for x in range(320) for v in nat[0][x, y]))
    print(f"shop: {n} tiles, {nalt} owl cells, sprite tiles {len(spr) // 32}")


if __name__ == "__main__":
    main()
