#!/usr/bin/env python3
"""stagepack.py CELLS ROWS PALDUMP.cps CAMLOG BACKDROP.ppm OUTDIR: a stage's
foreground and backdrop as ready Genesis data (NATIVE_ENGINE_PLAN.md, M1).
Everything is decided here, offline; the game only copies.

Inputs: CELLS/ROWS from stagemap.py (16x16 cells, ROWS blocks of 256 px),
a CPS palette dump of the stage, the arcade's world camera per VBlank
(lockstep BYTELOG lines "B tN xh xl yh yl ..."; the view's map rectangle is
x cam_x + 32 .. + 320, y 272 - cam_y .. + 224, found by aligning arcade
frames with the map) and the area's backdrop picture (320x224).

Colours: two palette lines of 15 (CRAM lines 2 and 3) shared by foreground
and backdrop, fitted by k-means over 8x8 tiles: each tile takes the line
that shows it best, each line is refitted to its tiles' pixels.

Streaming: the foreground plane is 64x64 cells (the stage's full 512-px
height), scrolled horizontally. A tile's uses are split into runs along the
stage (a gap wider than a screen ends a run); each run is given a VRAM slot
so that no two runs sharing a slot are ever on screen together (interval
colouring, optimal). When the window's right edge reaches column k the game
loads the runs that start at k; moving left, those that end at k.

Writes to OUTDIR (big-endian):
  pal.bin    32 u16 CRAM words (line 2 then line 3; entry 0 unused)
  tiles.bin  unique foreground tiles, 32 bytes each (4bpp)
  map.bin    per map column, 64 u16 plane words (POOL_BASE + slot | line |
             flips), 0 = empty
  loads.bin  u16 column count C, u16 pool size; then 2*C+1 u16 offsets (in
             entries) into the entry list: right-edge lists for columns
             0..C-1, then left-edge lists; then entries of (u16 tile, u16 slot)
  bd_tiles.bin, bd_map.bin  the backdrop: tiles and a 40x28 map, or for a
                     far layer (picture wider than the screen) FC x FR column-major (u16 plane
             words; its tiles follow the pool in VRAM). With FAR_POOL the far
             layer is merged so that every window fits FAR_POOL VRAM slots and
             streamed like the foreground: its words name slots, and
  far_loads.bin  its load lists by far column (the loads.bin format)
  layout.h   the VRAM numbers the game is built with
  cycle.bin  (with CYCLE) per cycle state, the 12 CRAM words of the
             cycling entries: line 2 entries 10-15, then line 3 entries 10-15

Palette cycle (CYCLE=cycle.json from tools/cyclescan.py; stage 1's
waterfall): the arcade rotates scroll2 entries in two six-colour families.
Each family gets six reserved CRAM entries (pens 10-15), one per phase:
family A in line 2, family B in line 3; a pixel whose arcade entry runs the
family's sequence k steps ahead uses entry 10 + k, and an entry with its own
sequence takes the nearest family phase. Tiles with cycling pixels are
pinned to their family's line; the other colours are fitted to pens 1-9.
The game writes the next state's 12 words every frames_per_step frames.
"""
import os, pickle, struct, subprocess, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402

VIEW_W, VIEW_H = 320, 224
MARGIN_Y = 48            # camera y the trace never showed but play may reach
WINDOW = 42              # columns a window can touch: 40 + fine scroll + load ahead
WEIGHT = (2, 4, 3)       # rough perceptual weights (r, g, b)
BD_TILES = int(os.environ.get("BD_TILES", "0"))   # backdrop: merge to this many tiles (0: off)
BD_MERGE = int(os.environ.get("BD_MERGE", "0"))   # backdrop: merge tiles within this error
BD_FIT = int(os.environ.get("BD_FIT", "1"))       # colour fit weight of an always-seen backdrop cell
FAR_POOL = int(os.environ.get("FAR_POOL", "0"))
WORLD_TOP = int(os.environ.get("WORLD_TOP", "511"))   # the stage's top world y (stage 4: 8191); CAMLOG y = native camera y
FAR_SHIFT = int(os.environ.get("FAR_SHIFT", "1"))     # far layer = camera >> FAR_SHIFT (stage 4: 3)   # far layer: VRAM slots it streams through (0: all resident)
FAR_TOP = int(os.environ.get("FAR_TOP", "144"))   # far layer: picture rows above the cam_y = 0 view (tools/farlayer.py)
FAR_TOP_TILES = int(os.environ.get("FAR_TOP_TILES", "40"))   # far layer: tiles for those rows (clouds)
FG_MERGE = int(os.environ.get("FG_MERGE", "0"))   # foreground (approved cut 5): same, by threshold
FG_POOL = int(os.environ.get("FG_POOL", "0"))     # art pass: merge only where the streamed pool needs it (tools/poolmerge)
POOL_BASE = 16           # VRAM tile index of slot 0 (0-15: SGDK system tiles)
CYCLE = os.environ.get("CYCLE")
NFIT = 9 if CYCLE else 15  # fitted colours per line (pens 1..NFIT); the rest cycle
# Art pass: CYCLE_FROM=x: the cycle runs only from camera x onwards; tiles
# never on screen there may use pens NFIT+1..15 as ordinary colours (pal.bin
# holds them; the game swaps in the cycle at x: src/stage.c stage_cycle).
CYCLE_FROM = int(os.environ.get("CYCLE_FROM", "0"))
PSEUDO = 100             # a cycling pixel is (PSEUDO + family * 6 + phase, PSEUDO, PSEUDO)


# Art pass (CYC_KEEP=2): a cycling pixel as the tile mergers see it, one distinct colour per family phase.
CYC_CODE = [(7, 0, 0), (0, 7, 0), (0, 0, 7), (7, 7, 0), (7, 0, 7), (0, 7, 7),
            (3, 0, 0), (0, 3, 0), (0, 0, 3), (3, 3, 0), (3, 0, 3), (0, 3, 3)]


def is_cyc(p):
    return p is not None and p[0] >= PSEUDO


def ppm(path):
    d = Path(path).read_bytes()
    parts = d.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]


def chan(v):                          # 8-bit -> Genesis 3-bit level
    return min(7, (v + 16) >> 5)


def cps_word_rgb3(w):                 # CPS palette word -> Genesis (r, g, b) 0..7
    bright = 0x0F + ((w >> 12) << 1)
    return tuple(chan(((w >> s) & 15) * 0x11 * bright // 0x2D) for s in (8, 4, 0))


def _oklab(c):
    def lin(v):
        v = v * 36 / 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(v) for v in c)
    l = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b) ** (1 / 3)
    m = (0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b) ** (1 / 3)
    s_ = (0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b) ** (1 / 3)
    return (0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s_,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s_,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s_)


# Art pass (2026-11-04): PALOPT=1 fits the two lines with tools/palopt
# (perceived OKLab error plus tile-border seams) and quantises in OKLab.
PALOPT = os.environ.get("PALOPT") == "1"
_CK = float(os.environ.get("CHROMA", "1")) ** 0.5    # as tools/palopt.c CHROMA
LAB = {(r, g, b): (lambda o: (o[0], o[1] * _CK, o[2] * _CK))(_oklab((r, g, b))) for r in range(8) for g in range(8) for b in range(8)}


def dist(a, b):
    if PALOPT and a in LAB and b in LAB:
        p, q = LAB[a], LAB[b]
        return 1000 * ((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2)
    return sum(k * (x - y) ** 2 for k, x, y in zip(WEIGHT, a, b))


def kmeans(hist, k, init):
    """Weighted k-means over Genesis colours; hist {rgb3: weight}."""
    cents = list(init)[:k]
    pts = sorted(hist.items(), key=lambda kv: -kv[1])
    for p, _ in pts:
        if len(cents) >= k:
            break
        if p not in cents:
            cents.append(p)
    while len(cents) < k:
        cents.append((0, 0, 0))
    for _ in range(20):
        acc = [[0, 0, 0, 0] for _ in cents]
        for p, w in pts:
            i = min(range(len(cents)), key=lambda j: dist(p, cents[j]))
            a = acc[i]
            a[0] += p[0] * w; a[1] += p[1] * w; a[2] += p[2] * w; a[3] += w
        new = [tuple(round(a[c] / a[3]) for c in range(3)) if a[3] else cents[i] for i, a in enumerate(acc)]
        if new == cents:
            break
        cents = new
    return cents


def tile_err(hist, line):
    return sum(w * min(dist(p, c) for c in line) for p, w in hist.items())


def fit_lines(tiles, pins, iters=8):
    """tiles: list of (colour histogram, weight); pins: per tile a forced
    line or None. Returns 2 lines of NFIT."""
    total = {}
    for h, w in tiles:
        for p, n in h.items():
            total[p] = total.get(p, 0) + n * w
    seed = kmeans(total, 2, [])
    side = [pin if pin is not None else min((0, 1), key=lambda s: sum(n * dist(p, seed[s]) for p, n in h.items()))
            for (h, _), pin in zip(tiles, pins)]
    lines = [None, None]
    for it in range(iters):
        for s in (0, 1):
            hs = {}
            for (h, w), t in zip(tiles, side):
                if t == s:
                    for p, n in h.items():
                        hs[p] = hs.get(p, 0) + n * w
            lines[s] = kmeans(hs, NFIT, lines[s] or [])
        new = [pin if pin is not None else min((0, 1), key=lambda s: tile_err(h, lines[s]))
               for (h, _), pin in zip(tiles, pins)]
        moved = sum(a != b for a, b in zip(new, side))
        side = new
        err = sum(tile_err(h, lines[s]) * w for (h, w), s in zip(tiles, side))
        print(f"  fit pass {it}: {moved} tiles changed line, error {err}")
        if not moved:
            break
    return lines


def palopt(pats, hists, pins, lines, cell_rgb, bd_rgb, vis_frac, FC, FR, npics, fg_w, bd_scale, wide=None):
    """tools/palopt over the patterns and their map neighbours; returns
    (lines, side)."""
    idx = {t: i for i, t in enumerate(pats)}
    cols = sorted({p for h, _ in hists for p in h})
    ci = {c: i for i, c in enumerate(cols)}
    edges = {}

    def pair(ta, tb, n, horiz):
        a, b = idx[ta], idx[tb]
        for k in range(8):
            p, q = (ta[k][7], tb[k][0]) if horiz else (ta[7][k], tb[0][k])
            if p is None or q is None or is_cyc(p) or is_cyc(q):
                continue
            key = (a, b, ci[p], ci[q])
            edges[key] = edges.get(key, 0) + n
    for (c, r), t in cell_rgb.items():
        w = fg_w(c, r)
        if (c + 1, r) in cell_rgb:
            pair(t, cell_rgb[(c + 1, r)], w, True)
        if (c, r + 1) in cell_rgb:
            pair(t, cell_rgb[(c, r + 1)], w, False)
    for k, t in enumerate(bd_rgb):
        if FC:
            c, r = divmod(k, FR)
            right = (c + 1) * FR + r if c + 1 < FC else None
            down = k + 1 if r + 1 < FR else None
        else:
            pic, rest = divmod(k, 1120)
            r, c = divmod(rest, 40)
            right = k + 1 if c + 1 < 40 else None
            down = k + 40 if r + 1 < 28 else None
        w = vis_frac[k] * bd_scale
        if w <= 0:
            continue
        if right is not None:
            pair(t, bd_rgb[right], w, True)
        if down is not None:
            pair(t, bd_rgb[down], w, False)
    g9 = lambda c: c[0] | c[1] << 3 | c[2] << 6
    blob = struct.pack("<II", NFIT, len(cols)) + b"".join(struct.pack("<H", g9(c)) for c in cols)
    blob += struct.pack("<I", len(pats))
    KAN = 15 if wide else NFIT
    for t, (h, w), pin in zip(pats, hists, pins):
        blob += struct.pack("<fiI", w, (-1 if pin is None else pin) + (0x100 if wide and wide.get(t) else 0), len(h))
        blob += b"".join(struct.pack("<If", ci[p], n) for p, n in h.items())
    blob += struct.pack("<I", len(edges))
    blob += b"".join(struct.pack("<IIfII", a, b, n, p, q) for (a, b, p, q), n in edges.items())
    for l in lines:
        l = (list(l) + [(0, 0, 0)] * KAN)[:KAN]
        blob += b"".join(struct.pack("<H", g9(c)) for c in l)
    run = subprocess.run([str(Path(__file__).with_name("palopt"))], input=blob, capture_output=True, check=True,
                         env=dict(os.environ, KA=str(KAN)))
    sys.stdout.write(run.stderr.decode())
    res = run.stdout
    out = []
    for l in range(2):
        ws = struct.unpack_from("<%dH" % KAN, res, l * KAN * 2)
        out.append([(w & 7, (w >> 3) & 7, (w >> 6) & 7) for w in ws])
    sides = res[4 * KAN:]
    return out, {t: sides[i] for i, t in enumerate(pats)}


def pool_merge(cell_rgb, budget, keep=None):
    """tools/poolmerge: merge foreground tiles (flip-canonical) only in the
    column windows whose pool would exceed budget. Returns cell_rgb remapped.
    keep: positions whose tiles must stay exact (FREEZE_CACHE): the others may
    merge into them, never the reverse."""
    canon_of, cols, uses = {}, {}, {}
    for (c, r), t in cell_rgb.items():
        f = min(range(4), key=lambda k: repr(flip_rows(t, k)))
        k = flip_rows(t, f)
        # With keep, an added cell's tile is a pattern of its own even when it
        # equals a kept one: it can then go to another tile where the pool is
        # full, without touching the kept cells.
        ident = (k, 1 + c // 24) if keep is not None and (c, r) not in keep else (k, 0)   # by stretches of columns, so a merge lengthens little
        canon_of[(c, r)] = (ident, f)
        cols.setdefault(ident, set()).add(c)
        uses[ident] = uses.get(ident, 0) + 1
    keys = list(cols)
    keep_k = {i for i in keys if i[1] == 0} if keep is not None else set()
    blob = struct.pack("<III", len(keys), budget, WINDOW)
    for ident in keys:
        k = ident[0]
        cyc = any(is_cyc(p) for row in k for p in row)
        kept = 1 << 30 if ident in keep_k else 0
        if os.environ.get("CYC_KEEP") == "2":  # art pass: cycling tiles merge too, each phase a distinct colour
            blob += struct.pack("<I", uses[ident] | kept)
            blob += bytes(v for row in k for p in row for v in
                          ((255, 255, 255) if p is None else CYC_CODE[(p[0] - PSEUDO) % 12] if is_cyc(p) else p))
        else:
            blob += struct.pack("<I", uses[ident] | (1 << 31 if cyc else 0) | kept)
            blob += bytes(v for row in k for p in row for v in ((255, 255, 255) if p is None or is_cyc(p) else p))
        cs = sorted(cols[ident])
        blob += struct.pack("<I", len(cs)) + b"".join(struct.pack("<I", c) for c in cs)
    run = subprocess.run([str(Path(__file__).with_name("poolmerge"))], input=blob, capture_output=True, check=True)
    sys.stdout.write(run.stderr.decode())
    res = run.stdout
    out = {}
    idx = {k: i for i, k in enumerate(keys)}
    for pos, (k, f) in canon_of.items():
        v = struct.unpack_from("<I", res, 4 * idx[k])[0]
        rk, g = keys[v & 0x3FFFFFFF][0], v >> 30
        out[pos] = flip_rows(rk, g ^ f)
    return out


def reduce_tiles(cells, n, thresh=0, weights=None):
    """Lossy: merge similar 8x8 picture cells, to n distinct (k-medoids) or
    within error thresh (tools/tilemerge.c); weights: per cell (default 1).
    Returns the cell list remapped."""
    uses = {}
    for i, t in enumerate(cells):
        uses[t] = uses.get(t, 0) + (weights[i] if weights else 1)
    pats = list(uses)
    if n and len(pats) <= n:
        return cells
    blob = struct.pack("<III", len(pats), n, thresh) + b"".join(
        struct.pack("<I", uses[t]) + bytes(v for row in t for p in row for v in (p or (64, 64, 64))) for t in pats)
    env = dict(os.environ, FLIPS="1") if n else None    # mirrored matches: k-medoids (backdrop) only
    if n and os.environ.get("BD_PERCEPT"):               # art pass: the far layer's merge by perceived, edge-weighted error
        env.update(PERCEPT="1", BORDER=os.environ.get("BD_BORDER", "2"))
        env.pop("DCW", None)
    run = subprocess.run([str(Path(__file__).with_name("tilemerge"))], input=blob, capture_output=True, check=True, env=env)
    res = run.stdout
    sys.stdout.write(run.stderr.decode())
    def mirrored(t, f):
        if f & 1:
            t = tuple(r[::-1] for r in t)
        if f & 2:
            t = t[::-1]
        return t
    who = {}
    for i, t in enumerate(pats):
        v = struct.unpack_from("<I", res, 4 * i)[0]
        who[t] = mirrored(pats[v & 0x3FFFFFFF], v >> 30)
    return [who[t] for t in cells]


def interval_pool(col_sets):
    """Runs of each tile's columns (a gap wider than WINDOW ends a run),
    slotted by interval colouring. Returns (pool size, [(start, end, key)],
    {(start, key): slot}) for keys = the col_sets' order."""
    import heapq
    runs = []
    for k, cs in enumerate(col_sets):
        cs = sorted(cs)
        s0 = e = cs[0]
        for c in cs[1:]:
            if c - e > WINDOW:
                runs.append((s0, e, k)); s0 = c
            e = c
        runs.append((s0, e, k))
    runs.sort()
    heap, slots, n = [], {}, 0
    for s0, e, k in runs:
        if heap and heap[0][0] <= s0:
            _, sl = heapq.heappop(heap)
        else:
            sl = n; n += 1
        slots[(s0, k)] = sl
        heapq.heappush(heap, (e + WINDOW, sl))
    return n, runs, slots


# Art pass (2026-11-04): DITHER=k mixes the two palette colours that best
# make a pixel's colour, by a 4x4 ordered pattern aligned to the tile (the
# same source tile always gives the same Genesis tile, so dedup still works);
# k weighs how far apart the two mixed colours may be (higher: less mixing).
DITHER = float(os.environ.get("DITHER", "0"))
BAYER = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))
_mix = {}


def mix_of(p, line):
    """(i, j, r): pens i, j (0-based) and ratio r/16 of j that best show p."""
    key = (p, tuple(line))
    if key in _mix:
        return _mix[key]
    n = len(line)
    best = (dist(p, line[0]) * 16 * 16, 0, 0, 0)
    for i in range(n):
        ci = line[i]
        e = dist(p, ci) * 256
        if e < best[0]:
            best = (e, i, i, 0)
        for j in range(n):
            if j == i:
                continue
            cj = line[j]
            dd = dist(ci, cj)
            for r in range(1, 9):              # j is the minority colour (r <= 8 of 16)
                m = tuple((a * (16 - r) + b * r) for a, b in zip(ci, cj))   # x16
                e = sum(k * (16 * x - y) ** 2 for k, x, y in zip(WEIGHT, p, m)) + DITHER * dd * r * (16 - r)
                if e < best[0]:
                    best = (e, i, j, r)
    _mix[key] = best[1:]
    return _mix[key]


def quantise(rgb_rows, line, nfit=None):
    """8x8 of rgb3 or None (transparent) -> 8x8 Genesis pens 0..15."""
    NFIT = nfit or globals()["NFIT"]
    memo = {}
    out = []
    for y, row in enumerate(rgb_rows):
        r = []
        for x, p in enumerate(row):
            if p is None:
                r.append(0)
            elif is_cyc(p):
                r.append(1 + NFIT + (p[0] - PSEUDO) % 6)
            elif DITHER:
                i, j, k = mix_of(p, line[:NFIT])
                r.append(1 + (j if BAYER[y & 3][x & 3] < k else i))
            else:
                if p not in memo:
                    memo[p] = 1 + min(range(NFIT), key=lambda j: dist(p, line[j]))
                r.append(memo[p])
        out.append(tuple(r))
    return tuple(out)


def flip_rows(t, f):
    if f & 1:
        t = tuple(r[::-1] for r in t)
    if f & 2:
        t = t[::-1]
    return t


def quantise_canon(rgb_rows, line, nfit=None):
    """quantise() of the tile's canonical flip, flipped back: a tile and its
    mirror images dither alike (flip dedup keeps working)."""
    if not DITHER:
        return quantise(rgb_rows, line, nfit)
    f = min(range(4), key=lambda k: repr(flip_rows(rgb_rows, k)))
    return flip_rows(quantise(flip_rows(rgb_rows, f), line, nfit), f)


def forms(t):
    h = tuple(r[::-1] for r in t)
    return [(t, 0), (h, 0x800), (t[::-1], 0x1000), (h[::-1], 0x1800)]


def tile_bytes(t):
    return bytes((r[x] << 4) | r[x + 1] for r in t for x in range(0, 8, 2))


def main():
    cells_path, rows_blocks, dump_path, cam_path, bd_path, outdir = sys.argv[1:7]
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    cells = Path(cells_path).read_bytes()
    crow = int(rows_blocks) * 16                 # 16x16 cell rows
    ccol = len(cells) // 4 // crow
    MW, MH = ccol * 2, crow * 2                  # 8x8 columns, rows
    assert MH * 8 == WORLD_TOP + 1, "the map's height is the stage's (WORLD_TOP)"

    raw = Path(dump_path).read_bytes()

    def decode_cells():
        """Every visible 8x8 map cell as rgb3 rows (None: transparent)."""
        with zipfile.ZipFile(ROOT / "willow.zip") as z:
            gfx = assemble_region(z)
        dec = {}

        # Every visible 8x8 map cell as (rgb3 rows or None for empty).
        cell_rgb = {}
        for c in range(MW):
            r0, r1 = vis[c]
            for r in range(r0, r1 + 1):
                o = ((r // 2) * ccol + c // 2) * 4
                code, attr = int.from_bytes(cells[o:o + 2], "big"), int.from_bytes(cells[o + 2:o + 4], "big")
                key = 0x4000 + (code & 0x1FFF)
                if key not in dec:
                    dec[key] = decode_tile(gfx, key, 16)
                t = dec[key]
                g = attr & 31
                qy, qx = (r % 2) * 8, (c % 2) * 8
                rows = []
                empty = True
                for y in range(8):
                    sy = 15 - (qy + y) if attr & 64 else qy + y
                    row = []
                    for x in range(8):
                        sx = 15 - (qx + x) if attr & 32 else qx + x
                        p = t[sy][sx]
                        if p == 15:
                            row.append(None)
                        else:
                            row.append(pal(g, p)); empty = False
                    rows.append(tuple(row))
                if not empty:
                    cell_rgb[(c, r)] = tuple(rows)
        return cell_rgb

    dump = raw[0x80:]
    pal_base = (int.from_bytes(raw[0x0A:0x0C], "big") << 8) - 0x900000
    cyc_of, fam_seq, cyc_step = {}, [], 0
    if CYCLE:
        import json
        cj = json.loads(Path(CYCLE).read_text())
        cyc_step = cj["frames_per_step"]
        seqs = {tuple(map(int, k.split(","))): [cps_word_rgb3(w) for w in v] for k, v in cj["entries"].items()}
        # The two families: the sequences that most other entries are rotations of.
        def rot(a, k):
            return a[k:] + a[:k]
        bases = []
        for k, v in sorted(seqs.items()):
            if not any(v == rot(b, i) for b in bases for i in range(6)) and \
                    sum(any(w == rot(v, i) for i in range(6)) for w in seqs.values()) >= 6:
                bases.append(v)
        if len(bases) == 1:                     # stage 3: one six-entry family and a three-entry one
            for k, v in sorted(seqs.items()):
                if not any(v == rot(b, i) for b in bases for i in range(6)) and \
                        sum(any(w == rot(v, i) for i in range(6)) for w in seqs.values()) >= 3:
                    bases.append(v)
                    break
        assert len(bases) == 2, bases
        bases.sort(key=lambda b: sum(sum(c) for c in b))          # family A: the darker (blue water)
        fam_seq = bases
        # A group's family: the one its exact rotations belong to; an entry
        # with its own sequence takes the nearest phase of its group's family.
        gfam = {}
        for (g, p), v in seqs.items():
            for f in (0, 1):
                if any(v == rot(bases[f], i) for i in range(6)):
                    gfam[g] = f
        for (g, p), v in seqs.items():
            fams = (gfam[g],) if g in gfam else (0, 1)
            fam, ph = min(((f, i) for f in fams for i in range(6)),
                          key=lambda fi: sum(dist(a, b) for a, b in zip(v, rot(bases[fi[0]], fi[1]))))
            cyc_of[(g, p)] = (PSEUDO + fam * 6 + ph, PSEUDO, PSEUDO)
        print(f"cycle: {len(seqs)} entries -> 2 families x 6 phases, step {cyc_step} frames")

    def pal(g, p):
        if (g, p) in cyc_of:
            return cyc_of[(g, p)]
        return cps_word_rgb3(int.from_bytes(dump[pal_base + 0x800 + (g * 16 + p) * 2:][:2], "big"))

    # Rows each 8-px column can show: the arcade's own camera path, plus margin.
    vis = [None] * MW
    for line in Path(cam_path).read_text().split("\n"):
        f = line.split()
        if len(f) < 6:
            continue
        cx, cy = int(f[2] + f[3], 16), int(f[4] + f[5], 16)
        if cy > 0x100 and WORLD_TOP == 511:     # pre-play values
            continue
        x0, y0 = cx + 32, WORLD_TOP - 239 - cy
        r0, r1 = max(0, (y0 - MARGIN_Y) // 8), min(MH - 1, (y0 + VIEW_H + MARGIN_Y) // 8)
        for c in range(max(0, x0 // 8 - 1), min(MW, (x0 + VIEW_W) // 8 + 2)):
            v = vis[c]
            vis[c] = (r0, r1) if v is None else (min(v[0], r0), max(v[1], r1))
    if os.environ.get("VIS_FROM_CACHE"):        # art pass: the camera log is gone; the rows from the cache
        _k, _ = pickle.loads((Path(outdir) / "decode.cache").read_bytes())
        vis = list(_k[2])
    # Rows the recorded camera never showed but play can (bug 4, 2026-11-05:
    # the waterfall's base, seen when the player falls in and walks back
    # left): "c0:c1:r0:r1,..." 8-px columns c0..c1 also keep rows r0..r1.
    for e in filter(None, os.environ.get("VIS_EXTRA", "").split(",")):
        c0, c1, r0, r1 = map(int, e.split(":"))
        for c in range(c0, c1 + 1):
            v = vis[c]
            vis[c] = (r0, r1) if v is None else (min(v[0], r0), max(v[1], r1))
    if os.environ.get("VIS_GROW"):              # every column n 8-px rows further both ways
        g = int(os.environ["VIS_GROW"])
        vis = [v and (max(0, v[0] - g), min(MH - 1, v[1] + g)) for v in vis]
    first = min(c for c in range(MW) if vis[c])
    last = max(c for c in range(MW) if vis[c])
    for c in range(MW):                          # columns before/after the route: as the edge
        if vis[c] is None:
            vis[c] = vis[first] if c < first else vis[last]

    key = (Path(cells_path).read_bytes(), raw, tuple(vis), tuple(sorted(cyc_of.items())))
    cache = out / "decode.cache"
    cell_rgb = None
    if cache.exists():
        k, v = pickle.loads(cache.read_bytes())
        if k == key:
            cell_rgb = v
    if cell_rgb is None:
        cell_rgb = decode_cells()
        cache.write_bytes(pickle.dumps((key, cell_rgb)))

    # Art pass: DARK_MAP="rgb:rgb[,...]" (Genesis levels) recolours source
    # colours, e.g. 111:000 - an arcade near-black (about 17 of 255) rounds up
    # to level 1, twice as bright on the Genesis, and cells filled with it
    # show as grey boxes on the black around them (stage 4's cave).
    if os.environ.get("DARK_MAP"):
        dm = {}
        for pr in os.environ["DARK_MAP"].split(","):
            a_, b_ = pr.split(":")
            dm[tuple(int(x) for x in a_)] = tuple(int(x) for x in b_)
        cell_rgb = {k: tuple(tuple(dm.get(p, p) if p is not None and not is_cyc(p) else p for p in row) for row in t)
                    for k, t in cell_rgb.items()}

    # FREEZE_CACHE=a stage's decode.cache: the rows it holds go through every
    # merge as they did, so the art already packed stays exact; the rest of
    # VIS_EXTRA joins afterwards, merging only into what is there.
    frozen_new = {}
    if os.environ.get("FREEZE_CACHE"):
        _k, _ = pickle.loads(Path(os.environ["FREEZE_CACHE"]).read_bytes())
        ov = _k[2]
        for k in [k for k in cell_rgb if not (ov[k[0]] and ov[k[0]][0] <= k[1] <= ov[k[0]][1])]:
            frozen_new[k] = cell_rgb.pop(k)
        print(f"freeze: {len(cell_rgb)} cells kept, {len(frozen_new)} cells added")

    # Backdrop 8x8 cells: one picture per stage area (BACKDROP may list
    # several, comma-separated); they share one tile set, so switching area
    # rewrites only the plane's map.
    # A picture wider than the screen is the far layer (tools/farlayer.py):
    # it scrolls at half the camera's rate on plane B, its map column-major
    # (FC columns x 28 rows) for streaming.
    bd_rgb = []
    bd_pics = bd_path.split(",")
    FC, FR = 0, 28
    for path in bd_pics:
        bw, bh, bpx = ppm(path)
        cell = lambda c, r: tuple(tuple(tuple(chan(v) for v in bpx[((r * 8 + y) * bw + c * 8 + x) * 3:][:3])
                                        for x in range(8)) for y in range(8))
        if bw > VIEW_W or os.environ.get("FAR_WRAP"):     # a far layer (stage 4: 256 px wide, repeating)
            assert len(bd_pics) == 1 and bh in (VIEW_H, VIEW_H + FAR_TOP) and bw % 8 == 0
            FC, FR = bw // 8, bh // 8
            bd_rgb = [cell(c, r) for c in range(FC) for r in range(FR)]
        else:
            assert (bw, bh) == (VIEW_W, VIEW_H)
            bd_rgb += [cell(c, r) for r in range(28) for c in range(40)]

    # How often each backdrop cell shows through the foreground along the
    # arcade's camera path (the backdrop is static, the foreground scrolls):
    # the merge and the colour fit spend tiles and pens on what is seen.
    opaque = {k for k, v in cell_rgb.items() if all(p is not None for row in v for p in row)}
    cams = []
    for line in Path(cam_path).read_text().split("\n")[::8]:
        f = line.split()
        if len(f) < 6:
            continue
        cx, cy = int(f[2] + f[3], 16), int(f[4] + f[5], 16)
        if cy <= 0x100 or WORLD_TOP != 511:
            cams.append((cx + 32, WORLD_TOP - 239 - cy))
    seen = [0] * (FC * FR if FC else 1120)
    tall = FC and FR > 28                       # the far layer moves vertically too
    for x0, y0 in cams:
        fx = ((x0 - 32) >> FAR_SHIFT) + 32      # far layer x at screen x 0
        if FC and fx >= FC * 8 and os.environ.get("FAR_WRAP"): fx %= FC * 8
        fy = FAR_TOP - ((WORLD_TOP - 239 - y0) >> FAR_SHIFT) if tall else 0     # far layer row (px) at screen y 0
        for r in range(28):
            if tall and r >= 26: continue       # the bottom band is opaque
            py = y0 + r * 8
            for c in range(40):
                px = x0 + c * 8
                if any((xx // 8, yy // 8) not in opaque for xx in (px, px + 7) for yy in (py, py + 7)):
                    if FC:
                        for k in {((fx + c * 8) // 8) % (FC if os.environ.get("FAR_WRAP") else 1 << 30), ((fx + c * 8 + 7) // 8) % (FC if os.environ.get("FAR_WRAP") else 1 << 30)}:
                            for j in {(fy + r * 8) // 8, (fy + r * 8 + 7) // 8}:
                                if k < FC and 0 <= j < FR: seen[k * FR + j] += 1
                    else:
                        seen[r * 40 + c] += 1
    vis_frac = [n / max(1, len(cams)) for n in seen] * (1 if FC else len(bd_pics))
    print(f"backdrop: {sum(1 for v in vis_frac if v > 0)} of {len(vis_frac)} cells ever seen")
    bd_weight = [1 + int(99 * v) for v in vis_frac]
    if os.environ.get("BD_UNIFORM"):            # art pass: no camera log (rebuilt stages): every far cell weighs alike
        bd_weight = [50] * len(vis_frac)
    if FC and FAR_POOL:                          # streamed far layer: never-seen cells get no tiles of their own
        bd_weight = [int(100 * v) + (1 if v else 0) for v in vis_frac]

    nb = len(set(bd_rgb))
    if FC and FAR_POOL:
        # Streamed far layer: merge to as many tiles as keep every window's
        # distinct tiles (flip-canonical) within the VRAM pool.
        def canon(t):
            h = tuple(r[::-1] for r in t)
            return min((t, h, t[::-1], h[::-1]))

        def peak_of(cells):
            cols_of = {}
            for i, t in enumerate(cells):
                cols_of.setdefault(canon(t), set()).add(i // FR)
            return interval_pool(cols_of.values())[0]
        # The rows above the cam_y = 0 view (clouds, seen only from high up)
        # are merged on their own to FAR_TOP_TILES, the rest to fit the pool.
        TR = FAR_TOP // 8
        top_i = [i for i in range(len(bd_rgb)) if i % FR < TR]
        main_i = [i for i in range(len(bd_rgb)) if i % FR >= TR]
        top_m = reduce_tiles([bd_rgb[i] for i in top_i], FAR_TOP_TILES, 0, [bd_weight[i] for i in top_i]) if top_i else []
        src, k = bd_rgb, FAR_POOL * 2
        for _ in range(8):
            mm = reduce_tiles([bd_rgb[i] for i in main_i], k, 0, [bd_weight[i] for i in main_i])
            got = list(bd_rgb)
            for i, t in zip(top_i, top_m): got[i] = t
            for i, t in zip(main_i, mm): got[i] = t
            pk = peak_of(got)
            print(f"far layer: merged to {k} tiles -> window peak {pk} (pool {FAR_POOL})")
            if pk <= FAR_POOL and (pk > FAR_POOL * 0.97 or k >= nb): break
            k = min(nb, max(FAR_POOL // 2, int(k * (FAR_POOL * 0.985) / pk)))
        bd_rgb = got
    elif BD_TILES or BD_MERGE:
        bd_rgb = reduce_tiles(bd_rgb, BD_TILES, BD_MERGE, bd_weight)
    print(f"backdrop: {nb} distinct cells, {len(set(bd_rgb))} after merging")
    if FG_POOL and not os.environ.get("GLOBAL_FIRST"):
        cell_rgb = pool_merge(cell_rgb, FG_POOL)
    if os.environ.get("FG_FIT_POOL"):
        # Art pass: the global merge (cut 5) at the smallest threshold whose
        # streamed pool fits FG_FIT_POOL (tools/tilemerge with PERCEPT and
        # BORDER: perceived, edge-weighted error).
        budget = int(os.environ["FG_FIT_POOL"])
        keys = list(cell_rgb)
        src = [cell_rgb[k] for k in keys]

        def pool_of(cells):
            cols_of = {}
            for (c, r), v in zip(keys, cells):
                kk = min((flip_rows(v, f) for f in range(4)), key=repr)
                cols_of.setdefault(kk, set()).add(c)
            return interval_pool(list(cols_of.values()))[0]
        lo, hi, best = 0, int(os.environ.get("FG_FIT_HI", "400000")), None
        while hi - lo > max(4, lo // 50):
            mid = (lo + hi) // 2
            m = reduce_tiles(src, 0, mid)
            pk = pool_of(m)
            print(f"fg merge: threshold {mid} -> pool {pk}")
            if pk <= budget:
                hi, best = mid, m
            else:
                lo = mid
        if best is None:
            best = reduce_tiles(src, 0, hi)
        cell_rgb = dict(zip(keys, best))
    if FG_MERGE:
        keys = list(cell_rgb)
        if os.environ.get("CYC_KEEP"):         # art pass: cycling tiles never merged (the merge saw their phases as one colour)
            keys = [k for k in keys if not any(is_cyc(p) for row in cell_rgb[k] for p in row)]
        merged = reduce_tiles([cell_rgb[k] for k in keys], 0, FG_MERGE)
        cell_rgb = {**cell_rgb, **dict(zip(keys, merged))}
        if os.environ.get("CYC_KEEP") == "2":   # the cycling tiles merged among themselves, each phase a distinct colour
            ck = [k for k in cell_rgb if any(is_cyc(p) for row in cell_rgb[k] for p in row)]
            code = lambda t: tuple(tuple(CYC_CODE[(p[0] - PSEUDO) % 12] if is_cyc(p) else p for p in row) for row in t)
            back = {}
            for k in ck:
                back.setdefault(code(cell_rgb[k]), cell_rgb[k])
            mc = reduce_tiles([code(cell_rgb[k]) for k in ck], 0, FG_MERGE)
            cell_rgb.update({k: back[m] for k, m in zip(ck, mc)})
    if FG_POOL and os.environ.get("GLOBAL_FIRST"):  # art pass: the light global merge first, then only what the pool still needs
        cell_rgb = pool_merge(cell_rgb, FG_POOL)
    if frozen_new:
        keep = set(cell_rgb)
        cell_rgb.update(frozen_new)
        cell_rgb = pool_merge(cell_rgb, int(os.environ.get("FREEZE_POOL", FG_POOL)), keep)
    if os.environ.get("POOLONLY"):              # quick measurement: pool size only
        canon = {}
        for k, v in cell_rgb.items():
            h = tuple(r[::-1] for r in v)
            canon[k] = min((v, h, v[::-1], h[::-1]), key=repr)
        cols_of = {}
        for (c, r), v in canon.items():
            cols_of.setdefault(v, set()).add(c)
        ev = []
        for cs in cols_of.values():
            cs = sorted(cs); s = e = cs[0]
            for c in cs[1:]:
                if c - e > WINDOW:
                    ev.append((s, 1)); ev.append((e + WINDOW, -1)); s = c
                e = c
            ev.append((s, 1)); ev.append((e + WINDOW, -1))
        live = peak = 0
        for _, d in sorted(ev, key=lambda x: (x[0], x[1])):
            live += d; peak = max(peak, live)
        print(f"POOLONLY: {len(cols_of)} unique tiles, pool {peak}")
        return

    # Fit: one entry per distinct cell pattern, weighted by use.
    uses = {}
    fg_seen = {}
    if PALOPT and os.environ.get("SCREENW") == "1":
        # Art pass: every cell weighs by its screen time (the fraction of the
        # camera samples that show it), foreground and far layer alike, so
        # the fit spends colours on what the player sees.
        fg_seen = {}
        for x0, y0 in cams:
            for c in range(x0 // 8, (x0 + VIEW_W) // 8 + 1):
                for r in range(max(0, y0 // 8), min(MH, (y0 + VIEW_H) // 8 + 1)):
                    fg_seen[(c, r)] = fg_seen.get((c, r), 0) + 1
        for k, t in cell_rgb.items():
            uses[t] = uses.get(t, 0) + max(0.01, fg_seen.get(k, 0) / max(1, len(cams)))
        for t, v in zip(bd_rgb, vis_frac):
            uses[t] = uses.get(t, 0) + v * float(os.environ.get("BD_SCREEN", "1"))
    else:
        # BOOST=c0:c1:k (art pass): foreground columns c0..c1 count k times
        # (a stage's opening screen, seen under its title).
        bst = [int(x) for x in os.environ["BOOST"].split(":")] if os.environ.get("BOOST") else None
        for (c, r), t in cell_rgb.items():
            uses[t] = uses.get(t, 0) + (bst[2] if bst and bst[0] <= c <= bst[1] else 1)
        for t, v in zip(bd_rgb, vis_frac):          # seen backdrop cells count like BD_FIT foreground cells
            uses[t] = uses.get(t, 0) + v * BD_FIT     # by how often it is seen (rarely seen cells barely count)
    pats = list(uses)
    hists = []
    for t in pats:
        h = {}
        for row in t:
            for p in row:
                if p is not None and not is_cyc(p):
                    h[p] = h.get(p, 0) + 1
        hists.append((h, uses[t]))
    pins = []
    for t in pats:
        fams = {(p[0] - PSEUDO) // 6 for row in t for p in row if is_cyc(p)}
        assert len(fams) <= 1, "a tile with both cycle families"
        pins.append(fams.pop() if fams else None)
    if os.environ.get("SPLIT_LINES"):
        # The foreground on line 2 only, the backdrop on line 3 only (stage 2:
        # the far layer's colours change on their own during the chase,
        # tools/chasepack.py refits line 3).
        fg_set = set(cell_rgb.values())
        pins = [0 if t in fg_set else 1 for t in pats]
    print(f"{len(cell_rgb)} visible foreground cells, {len(pats)} distinct cell patterns")
    wide = {}
    if CYCLE_FROM and CYCLE and PALOPT:
        # Tiles never on screen once the camera reaches CYCLE_FROM (the
        # cycle off): foreground columns left of the view's left edge there,
        # far columns left of the far layer's edge.
        a_fg = (CYCLE_FROM + 32) // 8 - 1
        a_far = ((CYCLE_FROM >> FAR_SHIFT) + 32) // 8 - 1
        ok = {}
        for (c, r), t in cell_rgb.items():
            if (c, r) in frozen_new:            # added cells never take a kept tile's 15 colours away
                ok.setdefault(t, True)
                continue
            ok[t] = ok.get(t, True) and c < a_fg
        for k, t in enumerate(bd_rgb):
            ok[t] = ok.get(t, True) and bool(FC) and k // FR < a_far
        for t, pin in zip(pats, pins):
            if pin is not None:
                ok[t] = False
        grp = {}
        for t in pats:
            grp.setdefault(min((flip_rows(t, f) for f in range(4)), key=repr), []).append(t)
        for ts in grp.values():
            v = all(ok.get(t, False) for t in ts)
            for t in ts:
                wide[t] = v
        print(f"cycle from camera x {CYCLE_FROM}: {sum(wide.values())} of {len(pats)} patterns may use 15 colours")
    lines = fit_lines(hists, pins)
    side = {t: pin if pin is not None else min((0, 1), key=lambda s: tile_err(h, lines[s]))
            for t, (h, _), pin in zip(pats, hists, pins)}
    if PALOPT:
        lines, side = palopt(pats, hists, pins, lines, cell_rgb, bd_rgb, vis_frac, FC, FR, len(bd_pics),
                             (lambda c, r: max(0.01, fg_seen.get((c, r), 0) / max(1, len(cams)))) if fg_seen else (lambda c, r: 1),
                             float(os.environ.get("BD_SCREEN", "1")) if fg_seen else BD_FIT, wide)
        # Mirror images of one tile share a VRAM tile: give them one line
        # (the heaviest variant's), so they do not split into two tiles.
        grp = {}
        for t, (h, w) in zip(pats, hists):
            k = min((flip_rows(t, f) for f in range(4)), key=repr)
            grp.setdefault(k, []).append((w, t))
        for vs in grp.values():
            s0 = side[max(vs, key=lambda v: v[0])[1]]
            for _, t in vs:
                side[t] = s0
    extra = [list(l[NFIT:15]) for l in lines] if wide else None
    lines = [list(l[:NFIT]) + [(0, 0, 0)] * (NFIT - len(l[:NFIT])) for l in lines]
    wl = [lines[s] + extra[s] for s in (0, 1)] if extra else None    # the lines as wide tiles see them
    if CYCLE:                                   # pens NFIT+1.. : the family at state 0
        lines = [lines[f] + [fam_seq[f][ph] for ph in range(6)] for f in (0, 1)]
    wide_t, wide_b = set(), set()                # Genesis tiles (fg, backdrop) drawn with the wide lines

    # Unique Genesis tiles, flip-canonical.
    tid, tiles = {}, []

    def tile_of(rgb):
        s = side[rgb]
        w_ = wide.get(rgb)
        q = quantise_canon(rgb, wl[s], 15) if w_ else quantise_canon(rgb, lines[s])
        for f, fl in forms(q):
            if (f, s) in tid:
                return tid[(f, s)], fl, s
        tid[(q, s)] = len(tiles); tiles.append(q)
        if w_:
            wide_t.add(len(tiles) - 1)
        return len(tiles) - 1, 0, s

    cellw = {}
    for (c, r), rgb in cell_rgb.items():
        cellw[(c, r)] = tile_of(rgb)
    fg_count = len(tiles)

    # Runs per tile along the stage, then slots by interval colouring.
    cols_of = {}
    for (c, r), (t, _, _) in cellw.items():
        cols_of.setdefault(t, set()).add(c)
    runs = []                                    # (start, end, tile)
    for t, cs in cols_of.items():
        cs = sorted(cs)
        s = e = cs[0]
        for c in cs[1:]:
            if c - e > WINDOW:
                runs.append((s, e, t)); s = c
            e = c
        runs.append((s, e, t))
    runs.sort()
    free_at = []                                 # per slot: first column it may be reused from
    run_slot = {}
    import heapq
    heap = []                                    # (free column, slot)
    for s, e, t in runs:
        if heap and heap[0][0] <= s:
            _, slot = heapq.heappop(heap)
        else:
            slot = len(free_at); free_at.append(0)
        run_slot[(s, t)] = slot
        heapq.heappush(heap, (e + WINDOW, slot))
    pool = len(free_at)

    run_of = {}
    for s, e, t in runs:
        for c in range(s, e + 1):
            run_of[(t, c)] = run_slot[(s, t)]

    mapw = bytearray()
    for c in range(MW):
        for r in range(MH):
            if (c, r) in cellw:
                t, fl, s = cellw[(c, r)]
                w = (POOL_BASE + run_of[(t, c)]) | fl | ((2 + s) << 13)
            else:
                w = 0
            mapw += struct.pack(">H", w)

    right = [[] for _ in range(MW)]
    left = [[] for _ in range(MW)]
    for s, e, t in runs:
        right[s].append((t, run_slot[(s, t)]))
        left[e].append((t, run_slot[(s, t)]))
    offs, ents = [], bytearray()
    n = 0
    for lst in right + left:
        offs.append(n)
        for t, sl in lst:
            ents += struct.pack(">HH", t, sl); n += 1
    offs.append(n)
    loads = struct.pack(">HH", MW, pool) + b"".join(struct.pack(">H", o) for o in offs) + ents

    # Every stage shares one VRAM geometry (src/stagedata.h): POOL_SLOTS puts
    # the backdrop where the largest stage's pool ends.
    pool = max(pool, int(os.environ.get("POOL_SLOTS", "0")))

    # Backdrop tiles (own dictionary, its own VRAM block).
    btid, btiles, bmap, bref = {}, [], bytearray(), []
    for rgb in bd_rgb:
        s = side[rgb]
        w_ = wide.get(rgb)
        q = quantise_canon(rgb, wl[s], 15) if w_ else quantise_canon(rgb, lines[s])
        for f, fl in forms(q):
            if (f, s) in btid:
                i = btid[(f, s)]; break
        else:
            fl = 0; i = btid[(q, s)] = len(btiles); btiles.append(q)
            if w_:
                wide_b.add(i)
        bref.append((i, fl, s))
    far_loads, far_pool = b"", len(btiles)
    if FC and FAR_POOL:
        # Far layer streamed like the foreground, by far column.
        cols_of = {}
        for k, (i, _, _) in enumerate(bref):
            cols_of.setdefault(i, set()).add(k // FR)
        keys = list(cols_of)
        far_pool, fruns, fslot = interval_pool([cols_of[i] for i in keys])
        slot_at = {}
        for s0, e, k in fruns:
            for c in range(s0, e + 1):
                slot_at[(keys[k], c)] = fslot[(s0, k)]
        for k, (i, fl, s) in enumerate(bref):
            bmap += struct.pack(">H", (POOL_BASE + pool + slot_at[(i, k // FR)]) | fl | ((2 + s) << 13))
        fright = [[] for _ in range(FC)]
        fleft = [[] for _ in range(FC)]
        for s0, e, k in fruns:
            fright[s0].append((keys[k], fslot[(s0, k)]))
            fleft[e].append((keys[k], fslot[(s0, k)]))
        offs, ents, n = [], bytearray(), 0
        for lst in fright + fleft:
            offs.append(n)
            for t, sl in lst:
                ents += struct.pack(">HH", t, sl); n += 1
        offs.append(n)
        far_loads = struct.pack(">HH", FC, far_pool) + b"".join(struct.pack(">H", o) for o in offs) + ents
        print(f"far layer: {len(btiles)} tiles in ROM, VRAM pool {far_pool} (budget {FAR_POOL}), "
              f"most loads at one column {max(len(l) for l in fright)}")
        assert far_pool <= FAR_POOL + 8, far_pool
    else:
        for i, fl, s in bref:
            bmap += struct.pack(">H", (POOL_BASE + pool + i) | fl | ((2 + s) << 13))

    palw = bytearray()
    for s in (0, 1):
        palw += struct.pack(">H", 0)
        for (r, g, b) in (wl[s] if wl else lines[s]):
            palw += struct.pack(">H", (r << 1) | (g << 5) | (b << 9))

    (out / "pal.bin").write_bytes(bytes(palw))
    if CYCLE:
        cw = bytearray()
        for t in range(6):
            for f in (0, 1):
                for ph in range(6):
                    r, g, b = fam_seq[f][(t + ph) % 6]
                    cw += struct.pack(">H", (r << 1) | (g << 5) | (b << 9))
        cw += struct.pack(">H", CYCLE_FROM)       # camera x the cycle starts at (0: always; src/stage.c)
        (out / "cycle.bin").write_bytes(bytes(cw))
    skies = []                                   # per picture, its top-left colour: the background colour
    for k in range(len(bd_pics)):
        cell = bd_rgb[(FC // 2) * FR + FR - 28] if FC else bd_rgb[k * 1120]   # the far layer: a sky cell mid-stage
        skies.append(16 * (2 + side[cell]) + 1 + min(range(NFIT), key=lambda j: dist(cell[0][0], lines[side[cell]][j])))
    sky_entry = skies[0]
    (out / "bd_sky.bin").write_bytes(b"".join(struct.pack(">H", k) for k in skies))
    (out / "tiles.bin").write_bytes(b"".join(tile_bytes(t) for t in tiles))
    (out / "map.bin").write_bytes(bytes(mapw))
    (out / "loads.bin").write_bytes(loads)
    (out / "bd_tiles.bin").write_bytes(b"".join(tile_bytes(t) for t in btiles))
    (out / "bd_map.bin").write_bytes(bytes(bmap))
    (out / "far_loads.bin").write_bytes(far_loads)
    (out / "layout.h").write_text(
        "/* generated by tools/stagepack.py */\n"
        f"#define STAGE_POOL_BASE {POOL_BASE}\n#define STAGE_POOL {pool}\n"
        f"#define STAGE_BD_BASE {POOL_BASE + pool}\n#define STAGE_BD_TILES {far_pool}\n"
        f"#define STAGE_FAR_TILES {len(btiles)}\n#define STAGE_FAR_ROWS {FR}\n#define STAGE_FAR_TOP {FAR_TOP if FR > 28 else 0}\n"
        f"#define STAGE_FAR_STREAM {1 if far_loads else 0}\n"
        f"#define STAGE_COLS {MW}\n#define STAGE_FG_TILES {len(tiles)}\n"
        f"#define STAGE_SKY {sky_entry}\n#define STAGE_CYCLE_STEP {cyc_step}\n"
        f"#define STAGE_BD_PICTURES {len(bd_pics)}\n#define STAGE_FAR_COLS {FC}\n")
    peak_load = max(len(right[c]) for c in range(MW))
    print(f"foreground: {fg_count} tiles ({fg_count * 32} bytes), {len(runs)} runs, VRAM pool {pool} slots, "
          f"most loads at one column {peak_load}; backdrop {len(btiles)} tiles; map {MW}x{MH}")

    # Preview: the whole foreground over nothing, as the Genesis will show it
    # (a tall map: every fourth pixel).
    cram = [lines[0], lines[1]]
    SC = 1 if MH <= 64 else 4
    img = bytearray((MW * 8 // SC) * (MH * 8 // SC) * 3)
    for c in range(MW):
        for r in range(MH):
            w = int.from_bytes(mapw[(c * MH + r) * 2:][:2], "big")
            if not w:
                continue
            t, fl, s = cellw[(c, r)]
            q = tiles[t]
            pal_ = wl if t in wide_t else cram
            assert (w & 0x7FF) - POOL_BASE == run_of[(t, c)]
            if fl & 0x800:
                q = tuple(rw[::-1] for rw in q)
            if fl & 0x1000:
                q = q[::-1]
            for y in range(0, 8, SC):
                for x in range(0, 8, SC):
                    p = q[y][x]
                    if p:
                        rgb = pal_[s][p - 1]
                        i = ((((r * 8 + y) // SC) * (MW * 8 // SC)) + (c * 8 + x) // SC) * 3
                        img[i:i + 3] = bytes(v * 36 for v in rgb)
    bimg = bytearray()
    PW = FC * 8 if FC else VIEW_W
    PH = FR * 8 if FC else VIEW_H * len(bd_pics)
    for y in range(PH):
        for x in range(PW):
            at = ((x // 8) * FR + y // 8) if FC else ((y // 8) * 40 + x // 8)     # pictures one under another
            w = int.from_bytes(bmap[at * 2:][:2], "big")
            q = btiles[bref[at][0]]
            yy, xx = (7 - y % 8 if w & 0x1000 else y % 8), (7 - x % 8 if w & 0x800 else x % 8)
            p = q[yy][xx]
            bimg += bytes(v * 36 for v in (wl if bref[at][0] in wide_b else cram)[(w >> 13) - 2][p - 1]) if p else b"\0\0\0"
    (out / "bd_preview.ppm").write_bytes(b"P6\n%d %d\n255\n" % (PW, PH) + bytes(bimg))
    (out / "preview.ppm").write_bytes(b"P6\n%d %d\n255\n" % (MW * 8 // SC, MH * 8 // SC) + bytes(img))


if __name__ == "__main__":
    main()
