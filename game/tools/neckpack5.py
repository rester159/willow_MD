#!/usr/bin/env python3
"""neckpack5.py DUMP.cps RENDER.ppm KEYINFO OUTDIR: Eborsisk's scroll3 neck
(specs/stage5.md 6.4) as four plane-B pictures for the far layer's
machinery (src/stage.c stage_far_override).

The arcade redraws the neck from tile lists 0x2C7B4[pose] (63 words, tile =
0xA00 + word, 0 blank, attribute 0x1F: scroll3 palette 31) in 8 rows of
32x32 tiles, row r starting at column 0x2C7A4[r] (bytes: +4 a tile down,
+32 a tile right) and holding 0x2C79C[r] + 1 tiles. Here the four poses are
composed on one grid of 10 x 8 tiles (columns -4..5 of the base), coloured
from the dump's live palette onto the stage's sprite line 1 (CRAM line 1,
data/s5spr/sprpal.bin: nearest colour; the scroll3 neck is the sprite
neck's twin), cut into 8x8 tiles, deduplicated with flips across the poses
and merged to at most BUDGET tiles (the far layer's VRAM, idle in the
arena). The base's screen position against the anchor comes from a
scroll3-only render of the dump (RENDER, CPSFULL) and KEYINFO = "camx camy
ax ay pose": picture pixel (u, v) of pose p shows at world x = u + anchorX + DX,
world y = anchorY + DY - v (DX, DY in neck.h).

Writes tiles.bin, map0..3.bin (40 x 32 words, column-major as the far
layer's bd_map: tile index from 0 | line 1 | priority | flips) and neck.h."""
import os, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402
from cps1_palette import cps_rgb  # noqa: E402

BUDGET = int(os.environ.get("BUDGET", "454"))
BASE = int(os.environ.get("BASE", "509"))      # the far layer's first VRAM tile (data/stage5/layout.h STAGE_BD_BASE)
W = lambda r, a: struct.unpack_from(">H", r, a)[0]


def compose(rom, gfx, pose, pal):
    img = {}
    lst = 0x2C7B4 + W(rom, 0x2C7B4 + 2 * pose)
    k = 0
    for r in range(8):
        off = struct.unpack_from(">h", rom, 0x2C7A4 + 2 * r)[0]
        dc, dr = (off - 4 * r + 128) // 32 - 4, r          # row r's first column (from the base)
        assert off == (dc * 32) + 4 * r, (r, off)
        for i in range(rom[0x2C79C + r] + 1):
            w = W(rom, lst + 2 * k); k += 1
            if not w:
                continue
            t = decode_tile(gfx, 0xA00 + w, 32)
            for y in range(32):
                for x in range(32):
                    p = t[y][x]
                    if p != 15:
                        img[((dc + i + 4) * 32 + x, dr * 32 + y)] = pal[p]
    return img


def main():
    dump, render, info, outdir = sys.argv[1:5]
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    raw = Path(dump).read_bytes()[0x80:]
    pal_ram = raw[0x1C000:0x1E000]
    pal = [cps_rgb(int.from_bytes(pal_ram[(0x600 + 31 * 16 + i) * 2:][:2], "big")) for i in range(16)]
    camx, camy, ax, ay, cpose = map(int, info.split())
    pics = [compose(rom, gfx, p, pal) for p in range(4)]
    # Calibrate: where the render shows pose cpose's picture (pixel match).
    d = Path(render).read_bytes(); i = 0
    for _ in range(3): i = d.index(b"\n", i) + 1
    rp = d[i:]
    lit = {(x, y) for y in range(224) for x in range(384) if rp[(y * 384 + x) * 3:(y * 384 + x) * 3 + 3] != b"\0\0\0"}
    best = None
    pts = list(pics[cpose].keys())[::7]
    # coarse-to-fine: the picture's top-left on the render
    def score(ox, oy):
        return sum(1 for (u, v) in pts if (u + ox, v + oy) in lit)
    for oy in range(-256, 224, 8):
        for ox in range(-320, 384, 8):
            s = score(ox, oy)
            if not best or s > best[0]: best = (s, ox, oy)
    _, bx, by = best
    for oy in range(by - 8, by + 9):
        for ox in range(bx - 8, bx + 9):
            s = score(ox, oy)
            if s > best[0]: best = (s, ox, oy)
    _, ox, oy = best
    # render (x, y) = world (x + camx, camy + 239 - y) on the native screen + 32 in x (CPSFULL 384 wide)
    DX = ox + camx - ax
    DY = camy + 239 - oy - ay     # world y of picture row 0 = anchorY + DY
    print(f"calibration: picture at render ({ox}, {oy}), {best[0]} of {len(pts)} points; DX {DX} DY {DY}")
    # Colours onto sprite line 1.
    sp = struct.unpack(">32H", (ROOT / "game/data/s5spr/sprpal.bin").read_bytes())
    line1 = [((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7) for w in sp[16:32]]
    q = lambda c: tuple(min(7, (v + 18) // 36) for v in c)
    dist = lambda a, b: 2 * (a[0] - b[0]) ** 2 + 4 * (a[1] - b[1]) ** 2 + 3 * (a[2] - b[2]) ** 2
    cache = {}
    def pen(c):
        c = q(c)
        if c not in cache: cache[c] = 1 + min(range(15), key=lambda j: dist(c, line1[1 + j]))
        return cache[c]
    # Tiles (8x8) of all poses, flip-deduplicated.
    def flips(t):
        rows = [t[y * 8:(y + 1) * 8] for y in range(8)]
        h = [r[::-1] for r in rows]
        return [tuple(sum(rows, ())), tuple(sum(h, ())), tuple(sum(rows[::-1], ())), tuple(sum(h[::-1], ()))]
    uniq, index, cells = [], {}, []
    use = {}
    for p, img in enumerate(pics):
        grid = {}
        for c in range(40):
            for r in range(32):
                px = tuple(pen(img[(c * 8 + x, r * 8 + y)]) if (c * 8 + x, r * 8 + y) in img else 0 for y in range(8) for x in range(8))
                if not any(px):
                    continue
                for f, v in enumerate(flips(px)):
                    if v in index:
                        grid[(c, r)] = (index[v], f); break
                else:
                    index[px] = len(uniq); uniq.append(px); grid[(c, r)] = (index[px], 0)
                use[grid[(c, r)][0]] = use.get(grid[(c, r)][0], 0) + 1
        cells.append(grid)
    print(f"{len(uniq)} distinct tiles over the four poses")
    # Merge to BUDGET: repeatedly fold the least-used tile into its nearest (flips allowed).
    alias = list(range(len(uniq)))
    live = set(range(len(uniq)))
    def tdist(a, b):
        return sum(dist(line1[x], line1[y]) if x and y else (0 if x == y else 40) for x, y in zip(a, b))
    while len(live) > BUDGET:
        k = min(live, key=lambda t: use.get(t, 0))
        live.discard(k)
        bestf = None
        for j in live:
            for f, v in enumerate(flips(uniq[j])):
                e = tdist(uniq[k], v)
                if bestf is None or e < bestf[0]: bestf = (e, j, f)
        alias[k] = (bestf[1], bestf[2])
        use[bestf[1]] = use.get(bestf[1], 0) + use.get(k, 0)
    order = sorted(live)
    new = {t: n for n, t in enumerate(order)}
    def resolve(t, f):
        while not isinstance(alias[t], int) or alias[t] != t:
            j, g = alias[t]
            f ^= g; t = j
        return new[t], f
    def pack(px):
        b = bytearray()
        for y in range(8):
            for x in range(0, 8, 2): b.append((px[y * 8 + x] << 4) | px[y * 8 + x + 1])
        return bytes(b)
    (out / "tiles.bin").write_bytes(b"".join(pack(uniq[t]) for t in order))
    for p, grid in enumerate(cells):
        words = []
        for c in range(40):
            for r in range(32):
                if (c, r) in grid:
                    t, f = resolve(*grid[(c, r)])
                    words.append((t + BASE) | ((f & 1) << 11) | ((f >> 1) << 12) | (1 << 13) | 0x8000)
                else:
                    words.append(0)
        (out / f"map{p}.bin").write_bytes(struct.pack(">1280H", *words))
    (out / "neck.h").write_text(f"/* generated by tools/neckpack5.py */\n#define NECK_TILES {len(order)}\n#define NECK_DX {DX}\n#define NECK_DY {DY}\n")
    print(f"neck: {len(order)} tiles (budget {BUDGET}), DX {DX}, DY {DY}")


if __name__ == "__main__":
    main()
