#!/usr/bin/env python3
"""sprpal.py STAGE_PAL OUT_PAL FAMILY_DIR...: fit a stage's sprite CRAM
(approved cut 4: fixed sprite palettes per stage, colours approximate) and
recolour every sprite family for it.

CRAM line 0 stays Willow's exact 15 colours (FAMILY_DIR "willow" must come
first). Line 1 is fitted (weighted k-means, 15 colours) to the pixels of all
other families. Then each sprite piece (an entry of poses.bin) takes the
line that shows it best: 1 or the static pens 1..STATIC of the scenery
lines 2-3 (never Willow's line 0: it flashes when she is hurt) (STAGE_PAL; STATIC=9 when their upper pens cycle). Families listed
with "=1" after the directory (e.g. data/shot=1) are drawn by code with a
fixed line and always use line 1.

Each family's poses.bin / tiles.bin from tools/posepack.py are rewritten in
place (originals kept as poses.src.bin / tiles.src.bin and re-read on later
runs): tile pens refer to the chosen line, and entry size bits 4-5 hold the
line (src/sprite.c). OUT_PAL: 32 CRAM words, lines 0 and 1. KEEP_LINE1=PAL (a stage's sprpal.bin)
keeps that fit's line 1, to add families without changing the others.
"""
import os, struct, sys
from pathlib import Path

WEIGHT = (2, 4, 3)
STATIC = int(os.environ.get("STATIC", "9"))
# Scenery lines a sprite piece may borrow (stage 2: "2" - line 3 is the far
# layer's, its colours change during the chase).
SCENERY = [int(x) for x in os.environ.get("SCENERY_LINES", "2,3").split(",") if x]


def _lab(c):
    def lin(v):
        v = v * 36 / 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(v) for v in c)
    l = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b) ** (1 / 3)
    m = (0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b) ** (1 / 3)
    s_ = (0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b) ** (1 / 3)
    k = float(os.environ.get("SPR_CHROMA", "4")) ** 0.5
    return (0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s_,
            k * (1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s_),
            k * (0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s_))


_LABS = {}
SPR_PERCEPT = os.environ.get("SPR_PERCEPT") == "1"   # art pass: perceived distance (hue x4), for a single family's pass


def dist(a, b):
    if SPR_PERCEPT:
        for c in (a, b):
            if c not in _LABS:
                _LABS[c] = _lab(c)
        p, q = _LABS[a], _LABS[b]
        return int(400 * ((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2) * 10)
    return sum(k * (x - y) ** 2 for k, x, y in zip(WEIGHT, a, b))


def cram(w):
    return ((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7)


def word(c):
    return (c[0] << 1) | (c[1] << 5) | (c[2] << 9)


def kmeans(hist, k, iters=20):
    pts = sorted(hist.items(), key=lambda kv: -kv[1])
    cents = [p for p, _ in pts[:k]]
    while len(cents) < k:
        cents.append((0, 0, 0))
    for _ in range(iters):
        acc = [[0, 0, 0, 0] for _ in cents]
        for p, w in pts:
            a = acc[min(range(k), key=lambda j: dist(p, cents[j]))]
            a[0] += p[0] * w; a[1] += p[1] * w; a[2] += p[2] * w; a[3] += w
        new = [tuple(round(a[c] / a[3]) for c in range(3)) if a[3] else cents[i] for i, a in enumerate(acc)]
        if new == cents:
            break
        cents = new
    return cents


class Family:
    def __init__(self, path, fixed):
        self.dir, self.fixed = Path(path), fixed
        for n in ("poses", "tiles"):
            src = self.dir / f"{n}.src.bin"
            if not src.exists():
                src.write_bytes((self.dir / f"{n}.bin").read_bytes())
        self.poses = bytearray((self.dir / "poses.src.bin").read_bytes())
        self.tiles = bytearray((self.dir / "tiles.src.bin").read_bytes())
        pal = (self.dir / "pal.bin").read_bytes()
        self.pal = [cram(struct.unpack_from(">H", pal, i * 2)[0]) for i in range(16)]
        self.count = int((self.dir / "layout.h").read_text().split("_COUNT ")[1].split()[0])
        # pieces: (entry offset in poses, first tile, tile count)
        self.pieces = []
        for i in range(self.count):
            t0, nt, ns, e0 = struct.unpack_from(">HBBH", self.poses, i * 6)
            for k in range(ns):
                at = self.count * 6 + (e0 + k) * 6
                size, t = self.poses[at + 2], self.poses[at + 3]
                n = ((size >> 2 & 3) + 1) * ((size & 3) + 1)
                self.pieces.append((at, t0 + t, n))

    def hist(self, first, n):
        h = {}
        for b in self.tiles[first * 32:(first + n) * 32]:
            for pen in (b >> 4, b & 15):
                if pen:
                    c = self.pal[pen]
                    h[c] = h.get(c, 0) + 1
        return h


def main():
    stage_pal, out_pal = sys.argv[1:3]
    fams = []
    for a in sys.argv[3:]:
        d, _, fixed = a.partition("=")
        weight = 1.0
        if fixed.startswith("w"):               # "DIR=wN": its pixels count N times in line 1's fit
            weight, fixed = float(fixed[1:]), ""
        f = Family(d, int(fixed) if fixed and fixed != "b" else None)
        f.boss = fixed == "b"
        f.weight = weight
        fams.append(f)
    willow, others = fams[0], fams[1:]
    line0 = willow.pal[1:]
    total = {}
    for f in others:
        seen = set()
        for _, t, n in f.pieces:
            if t in seen:
                continue
            seen.add(t)
            for c, w in f.hist(t, n).items():
                total[c] = total.get(c, 0) + w * (4 if f.fixed is not None else 1) * f.weight
    # EXTRA="r,g,b,weight;...": colours of plane art drawn on line 1 (stage 6's
    # storm, tools/far6.py) that the fit must also serve (Genesis 3-bit channels).
    for e in [x for x in os.environ.get("EXTRA", "").split(";") if x]:
        r, g, b, w = map(int, e.split(","))
        total[(r, g, b)] = total.get((r, g, b), 0) + w
    # Line 1: colours not already served well by line 0 count more.
    resid = dict(total)
    keep = os.environ.get("KEEP_LINE1")
    if keep:                                # recolour new families for a stage's existing fit (its line 1 unchanged)
        kw = struct.unpack(">32H", Path(keep).read_bytes())
        line1 = [cram(w) for w in kw[17:32]]
    else:
        line1 = kmeans(resid, 15)
    sw = struct.unpack(">32H", Path(stage_pal).read_bytes())
    scen = [[cram(w) for w in sw[0:16]][1:STATIC + 1], [cram(w) for w in sw[16:32]][1:STATIC + 1]]
    lines = [line0, line1, scen[0], scen[1]]
    # Boss families ("DIR=b"): the scenery lines' cycle pens 10-15 are lent
    # to the boss while it shows (the waterfall is far off): 12 colours fitted
    # to the boss pixels the other lines serve worst, darker half on line 2.
    boss_h = {}
    for f in others:
        if f.boss:
            for _, t, n in f.pieces:
                for c, w in f.hist(t, n).items():
                    boss_h[c] = boss_h.get(c, 0) + w
    fur = []
    if boss_h:
        poor = {c: w for c, w in boss_h.items() if min(dist(c, l) for l in line1 + scen[0] + scen[1]) > 2}
        fur = kmeans(poor or boss_h, 12)
        fur.sort(key=lambda c: c[0] * 2 + c[1] * 4 + c[2] * 3)
        Path(os.environ.get("FURPAL", "data/s1spr/furpal.bin")).write_bytes(b"".join(struct.pack(">H", word(c)) for c in fur))
    boss_lines = [line0, line1, scen[0] + fur[:6], scen[1] + fur[6:]]

    def err(h, line):
        return sum(w * min(dist(c, l) for l in line) for c, w in h.items())

    report = []
    for f in [willow] + others:
        tiles = bytearray(f.tiles)
        done, tot_err, use = {}, 0, [0, 0, 0, 0]
        for at, t, n in f.pieces:
            if t not in done:
                h = f.hist(t, n)
                lns = boss_lines if f.boss else lines
                if f is willow:
                    l = 0
                elif f.fixed is not None:
                    l = f.fixed
                else:                       # line 0 flashes with Willow when she is hurt: never for others
                    # (ALLOW_LINE0=1, art pass: High Aldwin's own pass - he shows in the intro, where
                    # nobody is hurt, and line 0 has the skin colours his face needs)
                    l = min(([0] if os.environ.get("ALLOW_LINE0") else []) + [1] + SCENERY, key=lambda j: err(h, lns[j]))
                tot_err += err(h, lns[l])
                memo = {}
                for i in range(t * 32, (t + n) * 32):
                    b = f.tiles[i]
                    out = 0
                    for sh, pen in ((4, b >> 4), (0, b & 15)):
                        if pen:
                            if pen not in memo:
                                memo[pen] = 1 + min(range(len(lns[l])), key=lambda j: dist(f.pal[pen], lns[l][j]))
                            out |= memo[pen] << sh
                    tiles[i] = out
                done[t] = l
            use[done[t]] += 1
            f.poses[at + 2] = (f.poses[at + 2] & 0x8F) | (done[t] << 4)
        (f.dir / "tiles.bin").write_bytes(bytes(tiles))
        (f.dir / "poses.bin").write_bytes(bytes(f.poses))
        report.append(f"{f.dir.name}: pieces per line {use}, error {tot_err}")
    Path(out_pal).write_bytes(b"".join(struct.pack(">H", w) for w in
                                       [0] + [word(c) for c in line0] + [0] + [word(c) for c in line1]))
    print("\n".join(report))


if __name__ == "__main__":
    main()
