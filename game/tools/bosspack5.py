#!/usr/bin/env python3
"""bosspack5.py DUMP.cps SPRNECK.pkl OUTDIR: stage 5's boss scene data
(specs/stage5.md 6): Eborsisk's two necks as plane-B pictures, and the arena
re-tiled to make room for them.

The arcade draws one neck on scroll3 (4 poses, tile lists 0x2C7B4) and the
other as a 72-piece sprite (root table 0x2C9B4); both hang on the same
anchor, so here they are composed into one plane-B picture per pair of
poses (16), 48 x 32 cells, the sprite neck over the far one. Picture pixel
(u, v) shows at world x = u + anchorX + DX, world y = anchorY + DY - v
(calibrated on a scroll3-only render by tools/neckpack5.py: DX -313, DY
151). Colours: CRAM line 3, 15 colours fitted to the necks.

To give the necks VRAM and a line, the arena (map columns 668..767, rows
64..95: what the locked camera can show) is re-tiled on CRAM line 2 alone
(15 colours fitted to it) into the far layer's VRAM (idle in the arena);
then the whole foreground pool and the rest of the far layer's VRAM hold
the necks' tiles, merged (tools/tilemerge, flips) to fit.

Writes arena_tiles.bin, arena_map.bin (100 x 32 words column-major),
arena_pal.bin, neck_tiles.bin, neck_maps.bin (16 x 48 x 32 words
column-major, far pose * 4 + sprite pose), neck_pal.bin and boss5.h."""
import pickle, struct, sys, zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import intropack as ip  # noqa: E402
import neckpack5 as nk  # noqa: E402

GAME = Path(__file__).resolve().parents[1]
C0, C1, R0 = 668, 768, 64           # arena columns, first row (32 rows)
POOL_BASE, POOL, BD_BASE, BD_TILES = 16, 493, 509, 454   # data/stage5/layout.h
DX, DY = -313, 151
PW, PH = 48, 32


def fit_tiles(img, w, h, line):
    """img {(x, y): rgb3 or None}: pens on line (15 colours), 8x8 cells."""
    cache = {}
    def pen(c):
        if c is None: return 0
        if c not in cache: cache[c] = 1 + min(range(15), key=lambda j: ip.dist(c, line[j]))
        return cache[c]
    return [[[pen(img.get((cx * 8 + x, cy * 8 + y))) for y in range(8) for x in range(8)] for cy in range(h)] for cx in range(w)]


def main():
    dump, sprpkl, outdir = sys.argv[1:4]
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    # ---- The arena from the assembled foreground (arcade colours, tools/stagemap.py).
    d = (GAME / "data/stage5_fg.ppm").read_bytes(); i = 0
    for _ in range(3): i = d.index(b"\n", i) + 1
    hdr = d[:i].split(); W = int(hdr[1]); px = d[i:]
    arena = {}
    hist = {}
    for y in range(R0 * 8, R0 * 8 + 256):
        for x in range(C0 * 8, C1 * 8):
            o = (y * W + x) * 3
            c = tuple(px[o:o + 3])
            if c == (255, 0, 255): continue
            c = ip.q3(c)
            arena[(x - C0 * 8, y - R0 * 8)] = c
            hist[c] = hist.get(c, 0) + 1
    aline = ip.kmeans(hist, 15); aline += [(0, 0, 0)] * (15 - len(aline))
    cells = fit_tiles(arena, C1 - C0, 32, aline)
    m = (GAME / "data/stage5/map.bin").read_bytes()
    atiles, aindex, awords = [], {}, []
    for cx in range(C1 - C0):
        for cy in range(32):
            t = cells[cx][cy]
            pri = struct.unpack_from(">H", m, ((C0 + cx) * 96 + R0 + cy) * 2)[0] & 0x8000
            if not any(t): awords.append(0); continue
            for f in range(4):
                k = tuple(ip.flip(t, f))
                if k in aindex: awords.append((BD_BASE + aindex[k]) | (f << 11) | (2 << 13) | pri); break
            else:
                aindex[tuple(t)] = len(atiles); atiles.append(t); awords.append((BD_BASE + aindex[tuple(t)]) | (2 << 13) | pri)
    assert len(atiles) <= BD_TILES
    print(f"arena: {len(atiles)} tiles on one line")
    # ---- The necks: 16 pictures.
    rom = (ip.ROOT / "build/reference_maincpu.bin").read_bytes()
    with zipfile.ZipFile(ip.ROOT / "willow.zip") as z:
        gfx = ip.assemble_region(z)
    raw = Path(dump).read_bytes()[0x80:]
    pr = raw[0x1C000:0x1E000]
    # Both necks as tools/posepack.py colours sprites (rgb3), the heads' colours too.
    import posepack
    pal31 = [posepack.rgb3(int.from_bytes(pr[(0x600 + 31 * 16 + k) * 2:][:2], "big")) for k in range(16)]
    far = [nk.compose(rom, gfx, p, pal31) for p in range(4)]
    spr = pickle.load(open(sprpkl, "rb"))
    pics, hist = [], {}
    for a in range(4):
        for b in range(4):
            img = dict(far[a])
            for (x, y), c in spr[b].items():
                u, v = x - DX, y + DY
                if 0 <= u < PW * 8 and 0 <= v < PH * 8: img[(u, v)] = tuple(c)     # posepack's rgb3: 3-bit already
            pics.append(img)
            for c in img.values(): hist[c] = hist.get(c, 0) + 1
    nline = ip.kmeans(hist, 15); nline += [(0, 0, 0)] * (15 - len(nline))
    budget = POOL + BD_TILES - len(atiles) - 12        # the last 12: the energy bars' glyphs (sprites)
    uniq, uindex, use, grids = [], {}, [], []
    for img in pics:
        cells = fit_tiles(img, PW, PH, nline)
        g = {}
        for cx in range(PW):
            for cy in range(PH):
                t = cells[cx][cy]
                if not any(t): continue
                for f in range(4):
                    k = tuple(ip.flip(t, f))
                    if k in uindex: g[(cx, cy)] = (uindex[k], f); use[uindex[k]] += 1; break
                else:
                    uindex[tuple(t)] = len(uniq); uniq.append(t); use.append(1); g[(cx, cy)] = (len(uniq) - 1, 0)
        grids.append(g)
    print(f"necks: {len(uniq)} distinct tiles over 16 pictures, budget {budget}")
    if len(uniq) > budget:
        rgb = [[nline[p - 1] if p else (0, 0, 0) for p in t] for t in uniq]
        rep = ip.merge([[tuple(ch * 36 for ch in c) for c in t] for t in rgb], use, budget)
    else:
        rep = [(k, 0) for k in range(len(uniq))]
    keep = sorted({r for r, _ in rep})
    slot = {t: n for n, t in enumerate(keep)}
    # VRAM: the pool first, then the far layer's tiles after the arena's.
    vram = list(range(POOL_BASE, POOL_BASE + POOL)) + list(range(BD_BASE + len(atiles), BD_BASE + BD_TILES - 12))
    words = []
    for g in grids:
        for cx in range(PW):
            for cy in range(PH):
                if (cx, cy) not in g: words.append(0); continue
                t, f = g[(cx, cy)]
                r, rf = rep[t]
                ff = f ^ rf
                words.append(vram[slot[r]] | ((ff & 1) << 11) | ((ff >> 1) << 12) | (3 << 13) | 0x8000)
    (out / "arena_tiles.bin").write_bytes(b"".join(ip.pack_tile(t) for t in atiles))
    (out / "arena_map.bin").write_bytes(struct.pack(">%dH" % len(awords), *awords))
    (out / "arena_pal.bin").write_bytes(struct.pack(">16H", 0, *[ip.cram_word(c) for c in aline]))
    (out / "neck_tiles.bin").write_bytes(b"".join(ip.pack_tile(uniq[t]) for t in keep))
    (out / "neck_maps.bin").write_bytes(struct.pack(">%dH" % len(words), *words))
    # Re-lay spans (src/stage.c stage_far_relay): for each change of one
    # neck's pose (16 x 6 pairs), per picture column the first and last row
    # that differ (255, 0: none). Index: u8 [16][16] pair -> entry (255: none).
    pics_w = [words[k * PW * PH:(k + 1) * PW * PH] for k in range(16)]
    index = bytearray([255] * 256); spans = bytearray()
    n = 0
    for a in range(16):
        for b in range(16):
            if a == b or (a // 4 != b // 4 and a % 4 != b % 4): continue
            index[a * 16 + b] = n; n += 1
            for c in range(PW):
                rr = [r for r in range(PH) if pics_w[a][c * PH + r] != pics_w[b][c * PH + r]]
                spans += bytes([rr[0], rr[-1]]) if rr else bytes([255, 0])
    (out / "neck_relay.bin").write_bytes(bytes(index) + bytes(spans))
    (out / "neck_pal.bin").write_bytes(struct.pack(">16H", 0, *[ip.cram_word(c) for c in nline]))
    n0 = min(len(keep), POOL)
    (out / "boss5.h").write_text("/* generated by tools/bosspack5.py */\n"
                                 f"#define ARENA_TILES {len(atiles)}\n#define ARENA_C0 {C0}\n#define ARENA_C1 {C1}\n#define ARENA_R0 {R0}\n"
                                 f"#define NECK_TILES {len(keep)}\n#define NECK_POOL_TILES {n0}\n#define NECK_FAR_AT {BD_BASE + len(atiles)}\n"
                                 f"#define BAR_VRAM {BD_BASE + BD_TILES - 12}\n#define NECK_DX {DX}\n#define NECK_DY {DY}\n#define NECK_W {PW}\n#define NECK_H {PH}\n")
    print(f"necks: {len(keep)} tiles; arena {len(atiles)}")


if __name__ == "__main__":
    main()
