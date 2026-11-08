#!/usr/bin/env python3
"""intropack.py MAPDUMP.cps MAPDUMP2.cps SKYDUMP.cps BOXDUMP.cps STORYDUMP.cps OUTDIR: the stage-1 intro's
pictures and text glyphs as ready Genesis data (specs/intro.md; approved
cuts 1, 3, 4). Offline only: nothing of the arcade program runs.

World map (scene A, 4.2): the arcade frame of MAPDUMP (scroll2 frame and
compass over the scroll3 map, the stage markers and Willow's figure, baked:
their blinking is left out) as one full-screen plane picture with its own
four CRAM lines.

Sky band (scene B, 6.1): the 30 cell rows above the stage map (map rows
-30..-1, world y 512..751) that the pan starts in, map columns 0..47:
scroll2 drawn over scroll3, both from the ROM's stage-1 grids
(tools/cps1_source_layers.py) in SKYDUMP's palettes. Cut 1, no parallax:
the far layer is taken at the camera that shows each row mid-screen. The
band shares CRAM lines 2-3 with the stage: their static pens 1-9 as they
are, pens 10-15 (the waterfall's cycle, unused at the stage start) fitted
here, and line 0 (Willow's: she is off screen while the sky shows), all
loaded for the sky's frames only. Its tiles go into the backdrop's 192
VRAM slots (k-medoids merge, tools/tilemerge).

Font (6.3, cut 3): the glyphs the intro's messages use, opaque (the
background pen is CRAM line 1 pen 1, near black), so the text sits on a
plain band: story text (text palette 3), the dialogue text (palette 0),
the big title letters (2x2 cells, palette 0x0E). All on line 1.

Writes (big-endian): map_tiles.bin, map_map.bin (40x28 words: tile from 0
| line << 13 | flips), map_pal.bin (64 CRAM words); sky_tiles.bin,
sky_map.bin (48 x 30 words, row-major, tile from 0 | line << 13 | flips),
sky_pal.bin (28 words: line 0, then lines 2 and 3 pens 10-15); box_tiles.bin,
box_map.bin (40 x 8 words for plane rows 2-9: tile | flips, 0xFFFF none); font.bin
(tiles), intro_text.h (strings, glyph tables, counts)."""
import os, struct, subprocess, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402
from cps1_source_layers import SourceLayers  # noqa: E402
from cps1_palette import cps_rgb  # noqa: E402
import cpsframe  # noqa: E402

WEIGHT = (2, 4, 3)
TEXT_PALS = 0xE6EA0
SKY_ROWS, SKY_COLS = 30, 48
EXT_ROWS = 16               # map rows 0-15 below the sky: the far layer where the stage map is empty
TOP_BUDGET = int(os.environ.get("TOP_BUDGET", "360"))   # rows above the map: the idle pose cache's VRAM (271 tiles) + the backdrop's spare slots (art pass: 410)
EXT_BUDGET = 192            # rows 0-15: the backdrop's VRAM (kept until the camera is below them)
MAP_BUDGET = 1049           # VRAM tiles 16..1064 (the stage pool and backdrop)


def q3(c):                  # 8-bit RGB -> Genesis 3-bit channels
    return tuple(min(7, (v + 16) >> 5) for v in c)


def cram_word(c):
    return (c[0] << 1) | (c[1] << 5) | (c[2] << 9)


def cram(w):
    return ((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7)


def dist(a, b):
    return sum(k * (x - y) ** 2 for k, x, y in zip(WEIGHT, a, b))


def kmeans(colors, k, iters=12):
    """Weighted k-means on {colour: weight}; returns up to k colours."""
    items = sorted(colors.items(), key=lambda t: -t[1])
    if len(items) <= k:
        return [c for c, _ in items]
    cent = [items[0][0]]
    while len(cent) < k:                    # farthest point start
        cent.append(max(items, key=lambda t: t[1] * min(dist(t[0], c) for c in cent))[0])
    for _ in range(iters):
        acc = [[0, 0, 0, 0] for _ in cent]
        for c, w in items:
            j = min(range(len(cent)), key=lambda i: dist(c, cent[i]))
            a = acc[j]
            a[0] += c[0] * w; a[1] += c[1] * w; a[2] += c[2] * w; a[3] += w
        cent = [tuple(round(a[i] / a[3]) for i in range(3)) if a[3] else cent[j] for j, a in enumerate(acc)]
    return list(dict.fromkeys(cent))


def merge(tiles_rgb, weights, k):
    """tools/tilemerge k-medoids with flips: representative (index, flips) per tile."""
    blob = struct.pack("<III", len(tiles_rgb), k, 0)
    for t, w in zip(tiles_rgb, weights):
        blob += struct.pack("<I", w) + bytes(v for px in t for v in px)
    exe = Path(__file__).with_name("tilemerge")
    out = subprocess.run([str(exe)], input=blob, capture_output=True, check=True, env=dict(os.environ, FLIPS="1")).stdout
    r = struct.unpack("<%dI" % len(tiles_rgb), out)
    return [(v & 0x3FFFFFFF, (v >> 30) & 3) for v in r]


def flip(t, f):
    rows = [t[y * 8:(y + 1) * 8] for y in range(8)]
    if f & 1: rows = [r[::-1] for r in rows]
    if f & 2: rows = rows[::-1]
    return [p for r in rows for p in r]


def pack_tile(pens):
    return bytes((pens[i] << 4) | pens[i + 1] for i in range(0, 64, 2))


def _oklab3(c):
    def lin(v):
        v = v * 36 / 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(v) for v in c)
    l = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b) ** (1 / 3)
    m = (0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b) ** (1 / 3)
    s_ = (0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b) ** (1 / 3)
    k = float(os.environ.get("CHROMA", "4")) ** 0.5
    return (0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s_,
            k * (1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s_),
            k * (0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s_))


_LABC = {}


def labdist(a, b):
    for c in (a, b):
        if c not in _LABC:
            _LABC[c] = _oklab3(c)
    p, q = _LABC[a], _LABC[b]
    return 1000 * ((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2)


def picopt(cells, cols, usable, lines, assign):
    """tools/palopt over the picture's 8x8 cells and their neighbours, on
    the lines in usable (all 15 pens free). Returns (assign, distance)."""
    uniq, idx_of = {}, []
    for t in cells:
        k = tuple(t)
        if k not in uniq:
            uniq[k] = len(uniq)
        idx_of.append(uniq[k])
    pats = list(uniq)
    weight = [0] * len(pats)
    for i in idx_of:
        weight[i] += 1
    colours = sorted({c for t in pats for c in t})
    ci = {c: i for i, c in enumerate(colours)}
    edges = {}
    n = len(cells)
    for i in range(n):
        a = idx_of[i]
        if (i + 1) % cols and i + 1 < n:
            b = idx_of[i + 1]
            for y in range(8):
                k = (a, b, ci[cells[i][y * 8 + 7]], ci[cells[i + 1][y * 8]])
                edges[k] = edges.get(k, 0) + 1
        if i + cols < n:
            b = idx_of[i + cols]
            for x in range(8):
                k = (a, b, ci[cells[i][56 + x]], ci[cells[i + cols][x]])
                edges[k] = edges.get(k, 0) + 1
    g9 = lambda c: c[0] | c[1] << 3 | c[2] << 6
    blob = struct.pack("<II", 15, len(colours)) + b"".join(struct.pack("<H", g9(c)) for c in colours)
    blob += struct.pack("<I", len(pats))
    for t, wgt in zip(pats, weight):
        h = {}
        for c in t:
            h[c] = h.get(c, 0) + 1
        blob += struct.pack("<fiI", wgt, -1, len(h)) + b"".join(struct.pack("<If", ci[c], m) for c, m in h.items())
    blob += struct.pack("<I", len(edges)) + b"".join(struct.pack("<IIfII", a, b, m, p, q) for (a, b, p, q), m in edges.items())
    for l in usable:
        ln = [c if c is not None else (0, 0, 0) for c in lines[l][1:16]]
        blob += b"".join(struct.pack("<H", g9(c)) for c in ln)
    env = dict(os.environ, LINES=str(len(usable)), KA="15")
    res = subprocess.run([str(Path(__file__).with_name("palopt"))], input=blob, capture_output=True, check=True, env=env)
    sys.stdout.write(res.stderr.decode())
    out = res.stdout
    for k, l in enumerate(usable):
        ws = struct.unpack_from("<15H", out, k * 30)
        lines[l][1:16] = [(w & 7, (w >> 3) & 7, (w >> 6) & 7) for w in ws]
    sides = out[len(usable) * 30:]
    return [usable[sides[i]] for i in idx_of], labdist


def picture(img, w, h, lines, free, budget):
    """img: {(x, y): rgb3}. lines: 4 lists of 16 colours (None = pen not
    usable); free: {line: [pen...]} pens to fit. Returns tiles, words, lines."""
    cells = [[img[(cx * 8 + x, cy * 8 + y)] for y in range(8) for x in range(8)]
             for cy in range(h // 8) for cx in range(w // 8)]
    usable = [l for l in range(4) if any(c is not None for c in lines[l][1:]) or free.get(l)]
    # Fit the free pens: tiles grouped by the line they prefer, then k-means.
    if free:
        # Start: tiles grouped by their mean colour (k-means over the means).
        means = [tuple(round(sum(c[k] for c in t) / 64) for k in range(3)) for t in cells]
        hist = {}
        for m in means: hist[m] = hist.get(m, 0) + 1
        cent = kmeans(hist, len(usable))
        assign = [usable[min(range(len(cent)), key=lambda i: dist(m, cent[i])) % len(usable)] for m in means]
        for it in range(4):
            for l, pens in free.items():
                hist = {}
                for t, a in zip(cells, assign):
                    if a != l: continue
                    for c in t:
                        fixed = [x for x in lines[l][1:] if x is not None]
                        if not fixed or min(dist(c, x) for x in fixed) > 2:
                            hist[c] = hist.get(c, 0) + 1
                got = kmeans(hist, len(pens)) if hist else []
                for i, p in enumerate(pens):
                    lines[l][p] = got[i] if i < len(got) else (lines[l][p] or (0, 0, 0))

            def terr(t, l):
                pal = [x for x in lines[l][1:] if x is not None]
                return sum(min(dist(c, x) for x in pal) for c in t)
            assign = [min(usable, key=lambda l: terr(t, l)) for t in cells]
    else:
        assign = [min(usable, key=lambda l: sum(min(dist(c, x) for x in lines[l][1:] if x is not None) for c in t)) for t in cells]
    dq = dist
    if os.environ.get("PICOPT") == "1" and free and all(len(free.get(l, [])) == 15 for l in usable):
        # Art pass: refine the lines with tools/palopt (perceived error, hue
        # x CHROMA, tile-border seams; LINES = the picture's lines) and pick
        # pens in the same perceived space.
        assign, dq = picopt(cells, w // 8, usable, lines, assign)
    pens = []
    for t, l in zip(cells, assign):
        pal = lines[l]
        pens.append([min((i for i in range(1, 16) if pal[i] is not None), key=lambda i: dq(c, pal[i])) for c in t])
    # Exact dedupe (with flips), then merge to the budget.
    rgb = [[lines[l][p] for p in t] for t, l in zip(pens, assign)]
    uniq, rep = {}, []
    for t, l in zip(pens, assign):
        key = None
        for f in range(4):
            k2 = (l, tuple(flip(t, f)))
            if k2 in uniq: key = (uniq[k2], f); break
        if key is None:
            uniq[(l, tuple(t))] = len(uniq); key = (uniq[(l, tuple(t))], 0)
        rep.append(key)
    order = sorted(uniq, key=lambda k: uniq[k])
    print(f"  {len(order)} distinct tiles before merging")
    if len(order) > budget:
        w = [0] * len(order)
        for r, _ in rep: w[r] += 1
        trgb = [[lines[l][p] for p in t] for l, t in order]
        m = merge(trgb, w, budget)
        rep = [(m[r][0], f ^ m[r][1]) for r, f in rep]
        keep = sorted(set(i for i, _ in rep))
        remap = {i: n for n, i in enumerate(keep)}
        order = [order[i] for i in keep]
        rep = [(remap[r], f) for r, f in rep]
        # a merged cell keeps its representative's line
    tiles = b"".join(pack_tile(list(t)) for _, t in order)
    words = [r | (order[r][0] << 13) | (f << 11) for r, f in rep]
    return tiles, words, len(order)


def main():
    map_dump, map_dump2, sky_dump, box_dump, story_dump, outdir = sys.argv[1:7]
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)

    # ---- World map: four lines of its own. MAPDUMP2 (the current stage's
    # marker in its other blink palette, 0x13): the cells that differ are
    # packed too, as alternates the game swaps every 10 updates.
    def frame(path):
        rgb = cpsframe.compose(cpsframe.Dump(path))
        return {(x, y): q3(rgb[(y * 320 + x) * 3:(y * 320 + x) * 3 + 3]) for y in range(224) for x in range(320)}
    def pack_map(dump_a, dump_b, pre):
        img = frame(dump_a)
        img2 = frame(dump_b)
        alt = [i for i in range(1120) if any(img[((i % 40) * 8 + x, (i // 40) * 8 + y)] != img2[((i % 40) * 8 + x, (i // 40) * 8 + y)]
                                             for y in range(8) for x in range(8))]
        arows = (len(alt) + 39) // 40
        big = dict(img)
        for k in range(arows * 40):
            i = alt[k] if k < len(alt) else alt[0]
            for y in range(8):
                for x in range(8):
                    big[((k % 40) * 8 + x, 224 + (k // 40) * 8 + y)] = img2[((i % 40) * 8 + x, (i // 40) * 8 + y)]
        lines = [[None] * 16 for _ in range(4)]
        tiles, words, n = picture(big, 320, 224 + arows * 8, lines, {l: list(range(1, 16)) for l in range(4)}, MAP_BUDGET)
        (out / f"{pre}_tiles.bin").write_bytes(tiles)
        (out / f"{pre}_map.bin").write_bytes(struct.pack(">1120H", *words[:1120]))
        (out / f"{pre}_alt.bin").write_bytes(b"".join(struct.pack(">HH", i, words[1120 + k]) for k, i in enumerate(alt)))
        (out / f"{pre}_pal.bin").write_bytes(struct.pack(">64H", *[cram_word(c) if c else 0 for l in lines for c in l]))
        print(f"world map {pre}: {n} tiles, {len(alt)} blinking cells")
        return len(alt)

    pack_map(map_dump, map_dump2, "map")
    # The map before stage 2 (stage2.md 1.2): marker II blinking, Madmartigan's figure (MAP2="dumpA,dumpB").
    map2_alt = pack_map(*os.environ["MAP2"].split(","), "map2") if os.environ.get("MAP2") else 0
    # Before stage 3 (stage3.md 1.2): marker III blinking, Willow by the lake (MAP3="dumpA,dumpB").
    map3_alt = pack_map(*os.environ["MAP3"].split(","), "map3") if os.environ.get("MAP3") else 0
    # Before stage 4 (stage4.md 1.2): marker IV blinking, Madmartigan in the mountains (MAP4).
    map4_alt = pack_map(*os.environ["MAP4"].split(","), "map4") if os.environ.get("MAP4") else 0
    # Before stage 5 (stage4.md 7.1): marker V blinking (MAP5).
    map5_alt = pack_map(*os.environ["MAP5"].split(","), "map5") if os.environ.get("MAP5") else 0
    # Before stage 6 (stage6.md 1.2): marker VI blinking, Willow and Madmartigan at Nockmaar (MAP6).
    map6_alt = pack_map(*os.environ["MAP6"].split(","), "map6") if os.environ.get("MAP6") else 0
    if os.environ.get("MAPS_ONLY"):          # only the world maps (intro_text.h keeps its other values)
        return

    # ---- Story page (stage1_boss.md 6.1 step 16): the portrait (STORYDUMP
    # without its text layer), lines 0, 2, 3 fitted; line 1 stays the text's.
    os.environ["CPSPLANE23"] = "1"
    simg = frame(story_dump)
    del os.environ["CPSPLANE23"]
    sprpal0 = struct.unpack(">32H", (ROOT / "game/data/s1spr/sprpal.bin").read_bytes())
    lines = [[None] * 16, [None] + [cram(w) for w in sprpal0[17:32]], [None] * 16, [None] * 16]
    tiles, words, n = picture(simg, 320, 224, lines, {l: list(range(1, 16)) for l in (0, 2, 3)}, MAP_BUDGET)
    (out / "story_tiles.bin").write_bytes(tiles)
    (out / "story_map.bin").write_bytes(struct.pack(">1120H", *words))
    (out / "story_pal.bin").write_bytes(struct.pack(">64H", *[cram_word(c) if c else 0 for l in lines for c in l]))
    print(f"story portrait: {n} tiles")

    # ---- Sky band above the stage map.
    raw = Path(sky_dump).read_bytes()[0x80:]
    pal_ram = raw[0x1C000:0x1C000 + 0x2000]
    sl = SourceLayers(rom, gfx, 0)
    p2 = [q3(cps_rgb(int.from_bytes(pal_ram[0x800 + i * 2:0x802 + i * 2], "big"))) for i in range(512)]
    p3 = [q3(cps_rgb(int.from_bytes(pal_ram[0xC00 + i * 2:0xC02 + i * 2], "big"))) for i in range(512)]
    smap = (ROOT / "game/data/stage1/map.bin").read_bytes()
    stage_word = lambda c, r: struct.unpack_from(">H", smap, (c * 64 + r) * 2)[0]
    rows = SKY_ROWS + EXT_ROWS
    img = {}
    for row in range(rows * 8):
        v = row - SKY_ROWS * 8                       # stage map pixel row (negative: above the map)
        cam = max(0, min(512, 384 - v))              # the camera showing row v mid-screen
        for x in range(SKY_COLS * 8):
            if v >= 0 and stage_word(x // 8, v // 8):
                img[(x, row)] = (0, 0, 0)            # the stage's own cell is shown there
                continue
            pen, g = sl.pen(2, x, v - 512)
            if pen != 15: img[(x, row)] = p2[g * 16 + pen]; continue
            pen, g = sl.pen(3, x, v - 512 + cam // 2)
            img[(x, row)] = p3[g * 16 + pen] if pen != 15 else (0, 0, 0)
    spal = struct.unpack(">32H", (ROOT / "game/data/stage1/pal.bin").read_bytes())
    # Art pass (SKY_FIXED15=1): stage 1's pens 10-15 are ordinary scenery
    # colours at the stage start (stagepack CYCLE_FROM), on screen with the
    # sky during the pan: the sky keeps all 15 of lines 2-3 as they are and
    # fits line 0 only.
    fixed15 = os.environ.get("SKY_FIXED15") == "1"
    nfix = 16 if fixed15 else 10
    lines = [[None] * 16, [None] * 16,
             [None] + [cram(w) for w in spal[1:nfix]] + [None] * (16 - nfix),
             [None] + [cram(w) for w in spal[17:16 + nfix]] + [None] * (16 - nfix)]
    # Palette: line 0 fitted to the sky and clouds above the map, the cycle
    # pens to the mountains below; then two tile sets with their own budgets.
    top = {(x, y): c for (x, y), c in img.items() if y < SKY_ROWS * 8}
    ext = {(x, y - SKY_ROWS * 8): c for (x, y), c in img.items() if y >= SKY_ROWS * 8}
    if not fixed15:
        lines[2][10:16] = [None] * 6
        lines[3][10:16] = [None] * 6
    picture(top, SKY_COLS * 8, SKY_ROWS * 8, lines, {0: list(range(1, 16))}, 100000)
    if not fixed15:
        picture(ext, SKY_COLS * 8, EXT_ROWS * 8, lines, {2: list(range(10, 16)), 3: list(range(10, 16))}, 100000)
    t1, w1, n1 = picture(top, SKY_COLS * 8, SKY_ROWS * 8, lines, {}, TOP_BUDGET)
    t2, w2, n2 = picture(ext, SKY_COLS * 8, EXT_ROWS * 8, lines, {}, EXT_BUDGET)
    words = w1 + [(w & 0xF800) | ((w & 0x7FF) + n1) for w in w2]
    (out / "sky_tiles.bin").write_bytes(t1 + t2)
    (out / "sky_map.bin").write_bytes(struct.pack(">%dH" % len(words), *words))
    (out / "sky_pal.bin").write_bytes(struct.pack(">28H", *([cram_word(lines[0][p]) if lines[0][p] else 0 for p in range(16)] +
                                                            [cram_word(lines[l][p]) for l in (2, 3) for p in range(10, 16)])))
    with open(out / "sky_preview.ppm", "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (SKY_COLS * 8, rows * 8))
        f.write(bytes(v * 36 for y in range(rows * 8) for x in range(SKY_COLS * 8) for v in img[(x, y)]))
    print(f"sky band: {n1} tiles above the map, {n2} in rows 0-{EXT_ROWS - 1}")
    sky_counts = (n1, n2)

    # ---- Font.
    sprpal = struct.unpack(">32H", (ROOT / "game/data/s1spr/sprpal.bin").read_bytes())
    line1 = [cram(w) for w in sprpal[16:32]]

    def apal(p):
        return [q3(cps_rgb(0xF000 | struct.unpack_from(">H", rom, TEXT_PALS + (p * 16 + i) * 2)[0])) for i in range(16)]

    def glyph(code, p):
        pal = apal(p)
        px = decode_tile(gfx, code, 8)
        return pack_tile([1 if pen == 15 else min(range(1, 16), key=lambda i: dist(pal[pen], line1[i])) for row in px for pen in row])

    story = ["ONE YOUNG MAN STARTS ON HIS JOURNEY", "TO SAVE A BABY SAINT FROM BAVMORDA'S", "WICKED HANDS.",
             "HIS NAME IS WILLOW."]
    box = ["WILLOW, BE CAREFUL. BAVMORDA'S FINGERS", "PENETRATE OUTSIDE OF THE VILLA."]
    font, idx = bytearray(), {}
    blank = pack_tile([1] * 64)
    font += blank
    for style, p, texts in (("S", 3, story), ("B", 0, box)):
        for ch in sorted(set("".join(texts)) - {" "}):
            idx[(style, ch)] = len(font) // 32
            font += glyph(0x7000 + ord(ch), p)
    big = {"A": 0x7680, "C": 0x7684, "D": 0x7686, "E": 0x7688, "G": 0x768C, "O": 0x76AC,
           "R": 0x76C2, "S": 0x76C4, "T": 0x76C6, "1": 0x7B00,
           # stage 2's title (stage2.md 1.3)
           "H": 0x768E, "I": 0x76A0, "L": 0x76A6, "N": 0x76AA, "F": 0x768A, "'": 0x7FEE, "2": 0x7B02}
    for ch, c in big.items():
        idx[("T", ch)] = len(font) // 32
        for dc in (0, 1, 0x10, 0x11):
            font += glyph(c + dc, 0x0E)
    (out / "font.bin").write_bytes(bytes(font))
    title0 = idx[("T", "A")]

    # ---- Dialogue box frame (6.3): the arcade's open, empty box (text layer
    # of BOXDUMP, plane rows 2-9), on line 1, flipped tiles shared. Loaded
    # over the title glyphs while the box shows (they follow it).
    bd = cpsframe.Dump(box_dump)
    bpx = cpsframe.scroll_layer(bd, 1)
    bpal = [q3(cps_rgb(bd.word(0x91C000 + 2 * i))) for i in range(0x1000)]
    interior = [bpx.get((20 * 8 + x, 4 * 8 + y)) for y in range(8) for x in range(8)]
    btiles, bmap, bkeys = bytearray(), [], {}
    for r in range(2, 10):
        for c in range(40):
            cell = [bpx.get((c * 8 + x, r * 8 + y)) for y in range(8) for x in range(8)]
            if r in (4, 5) and c < 38:
                cell = interior                    # where the text types: the empty interior
            if all(v is None for v in cell):
                bmap.append(0xFFFF)
                continue
            pens = [0 if v is None else min(range(1, 16), key=lambda i: dist(bpal[v[0]], line1[i])) for v in cell]
            w = None
            for f in range(4):
                k = tuple(flip(pens, f))
                if k in bkeys: w = bkeys[k] | (f << 11); break
            if w is None:
                bkeys[tuple(pens)] = len(btiles) // 32
                w = bkeys[tuple(pens)]
                btiles += pack_tile(pens)
            bmap.append(w)
    (out / "box_tiles.bin").write_bytes(bytes(btiles))
    (out / "box_map.bin").write_bytes(struct.pack(">%dH" % len(bmap), *bmap))
    print(f"box frame: {len(btiles) // 32} tiles (room {111 - title0})")
    assert len(btiles) // 32 <= 111 - title0

    def table(style):
        return "{ " + ", ".join(str(idx.get((style, chr(c)), 0)) for c in range(32, 96)) + " }"
    h = ["/* generated by tools/intropack.py */",
         f"#define INTRO_FONT_TILES {len(font) // 32}",
         f"#define INTRO_TITLE0 {title0}      /* first title glyph: the box frame is loaded there while it shows */",
         f"#define INTRO_BOX_TILES {len(btiles) // 32}",
         f"#define INTRO_MAP_TILES {len(Path(out / 'map_tiles.bin').read_bytes()) // 32}",
         f"#define INTRO_MAP_ALT {len(Path(out / 'map_alt.bin').read_bytes()) // 4}",
         f"#define INTRO_MAP2_TILES {len(Path(out / 'map2_tiles.bin').read_bytes()) // 32 if map2_alt else 0}",
         f"#define INTRO_MAP2_ALT {map2_alt}",
         f"#define INTRO_MAP3_TILES {len(Path(out / 'map3_tiles.bin').read_bytes()) // 32 if map3_alt else 0}",
         f"#define INTRO_MAP3_ALT {map3_alt}",
         f"#define STORY_TILES {len(Path(out / 'story_tiles.bin').read_bytes()) // 32}",
         f"#define INTRO_SKY_TILES {len(Path(out / 'sky_tiles.bin').read_bytes()) // 32}",
         f"#define INTRO_SKY_ROWS {SKY_ROWS}",
         f"#define INTRO_SKY_EXT_ROWS {EXT_ROWS}",
         f"#define INTRO_SKY_TOP_TILES {sky_counts[0]}   /* the rest: rows 0-{EXT_ROWS - 1}, in the backdrop's VRAM */",
         f"#define INTRO_SKY_COLS {SKY_COLS}",
         "/* glyph tile (from the font base) for ASCII 32..95, per style: story, box, title (TL; +1 BL... see intro.c) */",
         f"static const u8 font_story[64] = {table('S')};",
         f"static const u8 font_box[64] = {table('B')};",
         f"static const u8 font_title[64] = {table('T')};"]
    (out / "intro_text.h").write_text("\n".join(h) + "\n")
    print(f"font: {len(font) // 32} tiles")


if __name__ == "__main__":
    main()
