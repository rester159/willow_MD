#!/usr/bin/env python3
"""questpic.py: Sorsha's Quest's three new story pictures (the user's Gemini
"Nano Banana Pro" images in out/audit/quest/in/) for the game's story page.
Each 1024x1024 source: crop 3% top and bottom, then
box-filtered to 128x120 (16x15 tiles), ~20 flat colours (the comic-style
repaints are already flat), no dither, 9-bit grid; line 2 (and line 3 when
over 15 colours; pen 0 = black). Around it the arcade portraits' gold
frame band (att_mad's pens 12,5,15,6,8,14,15, 7 px, bevelled corners, line 1)
for an 18x17-cell picture. Tiles deduplicated with flips.
Out per picture N: data/quest/N_tiles.bin, N_map.bin (18x17 words), N_pal.bin
(64 words: line 1 frame, 2-3 picture), and a preview PPM in argv[1]."""
import struct, sys
from PIL import Image, ImageFilter, ImageEnhance      # Homebrew python3 (has Pillow)
PICS = ['p1_nockmaar', 'p6_return', 'p7_ending']
PV = sys.argv[1] if len(sys.argv) > 1 else '/tmp'
WT = (3, 4, 2)
def d2(a, b): return sum(w * (x - y) ** 2 for w, x, y in zip(WT, a, b))
L9 = [round(i * 255 / 7) for i in range(8)]
def q9(v): return min(L9, key=lambda l: abs(l - v))
def word(c): return (round(c[2] / 255 * 7) << 9) | (round(c[1] / 255 * 7) << 5) | (round(c[0] / 255 * 7) << 1)
BLACK = (0, 0, 0)
import colorsys
RAMPS = {'p6_return': [((.5, .75, .25, .08), 5), ((.05, .17, .35, .3), 5)]}  # blue tunics, Madmartigan's gold: (hue lo, hi, min sat, min value), tones
def keep_hue(c):
    """9-bit snap that keeps a dark tone's hue: the strongest channel rounds
    up, the weakest down (dark navy would otherwise snap to grey)."""
    hi, lo = max(range(3), key=lambda i: c[i]), min(range(3), key=lambda i: c[i])
    return tuple(min(v for v in L9 if v >= x) if i == hi else max(v for v in L9 if v <= x) if i == lo else q9(x) for i, x in enumerate(c))
def qstrip(px, n, hue=False):
    """px quantized to n flat colours (median cut + k-means, 9-bit grid)."""
    st = Image.new('RGB', (len(px), 1)); st.putdata(px)
    g = st.quantize(colors=n, method=Image.MEDIANCUT, kmeans=8, dither=Image.NONE).convert('RGB')
    g = g.point(lambda v: v) if hue else g.point(q9)
    return [keep_hue(g.getpixel((i, 0))) if hue else g.getpixel((i, 0)) for i in range(len(px))]
def digit(path, ncol=20, crop=0.03, ramps=()):
    """Comic-style repaints (already flat; out/audit/quest/gemini_digit_v3.py):
    contrast x1.08, box filter to 128x120, median cut + k-means to ncol flat
    colours, no dither, 9-bit grid. (No median filter: it smeared the faces.)
    ramps: hue groups quantized on their own (n tones each) so a small
    coloured area keeps its hue instead of merging into the browns."""
    im = Image.open(path).convert('RGB'); W, H = im.size; c = int(H * crop)
    im = ImageEnhance.Contrast(im.crop((0, c, W, H - c))).enhance(1.08).resize((128, 120), Image.BOX)
    px = [im.getpixel((x, y)) for y in range(120) for x in range(128)]
    grp = [0] * len(px)
    for i, p in enumerate(px):
        h, sat, v = colorsys.rgb_to_hsv(*[t / 255 for t in p])
        for k, ((lo, hi, smin, vmin), _) in enumerate(ramps):
            if lo < h < hi and sat > smin and v > vmin: grp[i] = k + 1; break
    out = [None] * len(px)
    for k in range(len(ramps) + 1):
        idx = [i for i in range(len(px)) if grp[i] == k]
        if not idx: continue
        n = ramps[k - 1][1] if k else ncol - sum(r[1] for r in ramps)
        for i, cc in zip(idx, qstrip([px[i] for i in idx], n, k > 0)): out[i] = cc
    return [out[y * 128:(y + 1) * 128] for y in range(120)]
def fit(img):
    """Lines 2-3 from the picture's own flat colours only (no new shades):
    one line when its colours fit 15 (+ black); else tiles alternate between
    two 15-colour subsets, a tile's pixels mapped to its line's nearest."""
    tiles = [[img[ty * 8 + y][tx * 8 + x] for y in range(8) for x in range(8)] for ty in range(15) for tx in range(16)]
    cnt = {}
    for t in tiles:
        for p in t:
            if p != BLACK: cnt[p] = cnt.get(p, 0) + 1
    cols = sorted(cnt, key=lambda c: -cnt[c])
    if len(cols) <= 15: return tiles, [0] * len(tiles), [cols, []]
    # two lines: tiles split by mean colour (2-means), then each line takes
    # its tiles' 15 most used colours and tiles move to the closer line
    mean = [tuple(sum(p[c] for p in t) / 64 for c in range(3)) for t in tiles]
    lo = min(mean, key=sum); cen = [lo, max(mean, key=lambda m: d2(m, lo))]
    for _ in range(8):
        side = [min((0, 1), key=lambda s: d2(m, cen[s])) for m in mean]
        cen = [tuple(sum(m[c] for m, sd in zip(mean, side) if sd == s) / max(1, side.count(s)) for c in range(3)) for s in (0, 1)]
    def err(t, pal): return sum(min([d2(p, BLACK)] + [d2(p, c) for c in pal]) for p in t)
    for _ in range(8):
        pals = []
        for s in (0, 1):
            w = {}
            for i, t in enumerate(tiles):
                if side[i] == s:
                    for p in t:
                        if p != BLACK: w[p] = w.get(p, 0) + 1
            pals.append(sorted(w, key=lambda c: -w[c])[:15])
        side = [min((0, 1), key=lambda s: err(t, pals[s])) for t in tiles]
    print('  2 lines, %d colours, mapping error %d' % (len(cols), sum(err(t, pals[s]) for t, s in zip(tiles, side))))
    return tiles, side, pals
FR = [12, 5, 15, 6, 8, 14, 15]
mp = struct.unpack('>64H', open('data/attract/att_mad_pal.bin', 'rb').read())
frame_pal = [mp[48 + p] for p in FR[:6]]                       # line 1 pens 1..6
def frame_pen(x, y, W, H, c=4):
    dx, dy = min(x, W - 1 - x), min(y, H - 1 - y)
    if dx + dy < c: return 0
    dep = min(dx, dy, dx + dy - c)
    return [1, 2, 3, 4, 5, 6, 3][dep] if dep < 7 else 0
def rgbw(w): return (((w >> 1) & 7) * 255 / 7, ((w >> 5) & 7) * 255 / 7, ((w >> 9) & 7) * 255 / 7)
tot = 0
for name in PICS:
    img = digit('out/audit/quest/in/%s.png' % name, **({'ncol': 30, 'ramps': RAMPS[name]} if name in RAMPS else {}))
    tiles, side, pals = fit(img)
    # final 144x136 pens (line << 4 | pen)
    W, H = 144, 136
    P = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            f = frame_pen(x, y, W, H)
            if f: P[y][x] = 1 << 4 | f
    for ti, t in enumerate(tiles):
        tx, ty = ti % 16, ti // 16; s = side[ti]; pal = pals[s]
        for k, p in enumerate(t):
            cand = [(d2(p, BLACK), 0)] + [(d2(p, c), j + 1) for j, c in enumerate(pal)]
            pen = min(cand)[1]
            P[8 + ty * 8 + k // 8][8 + tx * 8 + k % 8] = (2 + s) << 4 | pen if pen else 0
    lib = {}; out = bytearray(); words = []
    for cy_ in range(17):
        for cx_ in range(18):
            cell = [P[cy_ * 8 + y][cx_ * 8 + x] for y in range(8) for x in range(8)]
            lines = set(v >> 4 for v in cell if v & 15); line = lines.pop() if lines else 0
            assert not lines, (name, cx_, cy_)
            q = tuple(v & 15 for v in cell)
            hit = None
            for f in range(4):
                qq = tuple(q[(7 - y if f & 2 else y) * 8 + (7 - x if f & 1 else x)] for y in range(8) for x in range(8))
                if qq in lib: hit = (lib[qq], f); break
            if not hit:
                lib[q] = len(out) // 32; hit = (lib[q], 0)
                for y in range(8):
                    for x in range(0, 8, 2): out.append(q[y * 8 + x] << 4 | q[y * 8 + x + 1])
            i, f = hit
            words.append(line << 13 | (f & 2) << 11 | (f & 1) << 11 | i)
    pal = [0] * 64
    for j, w in enumerate(frame_pal): pal[17 + j] = w
    for s in (0, 1):
        for j, c in enumerate(pals[s]): pal[32 + s * 16 + 1 + j] = word(c)
    open('data/quest/%s_tiles.bin' % name, 'wb').write(bytes(out))
    open('data/quest/%s_map.bin' % name, 'wb').write(struct.pack('>%dH' % len(words), *words))
    open('data/quest/%s_pal.bin' % name, 'wb').write(struct.pack('>64H', *pal))
    # previews: source scaled and result
    pv = bytearray()
    for y in range(H):
        for x in range(W):
            v = P[y][x]; pv += bytes(int(c) for c in rgbw(pal[(v >> 4) * 16 + (v & 15)])) if v & 15 else b'\0\0\0'
    open('%s/%s_after.ppm' % (PV, name), 'wb').write(b'P6\n%d %d\n255\n' % (W, H) + pv)
    bv = bytearray()
    for y in range(H):
        for x in range(W):
            if 8 <= x < 136 and 8 <= y < 128: bv += bytes(int(c) for c in img[y - 8][x - 8])
            else: bv += b'\0\0\0'
    open('%s/%s_before.ppm' % (PV, name), 'wb').write(b'P6\n%d %d\n255\n' % (W, H) + bv)
    tot += len(out)
    h = [l for l in open('data/quest/quest.h').read().splitlines() if not l.startswith('#define QPIC%s_' % name[1])]
    open('data/quest/quest.h', 'w').write('\n'.join(h + ['#define QPIC%s_TILES %d' % (name[1], len(out) // 32)]) + '\n')
    print(name, 'unique tiles', len(out) // 32, 'bytes', len(out))
print('total raw', tot)
