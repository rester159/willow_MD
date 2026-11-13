#!/usr/bin/env python3
"""skyfix.py (run once, on the V0.8 sky files): the stage 1 intro sky's 15 line-2 cells (the far castle, the
trees at the band's foot) re-fitted to the colours they now get. They were
fitted (tools/intropack.py) against stage 1's line 2 before the V0.8 earth
fix re-coloured it, so they showed green blocks in the pan (V0.8 bug report).
Each cell: its source pixels (sky_preview.ppm), refit to line 0 (the sky's)
or line 2 (stage 1's line 2 with the sky's pens 10-15), whichever is closer;
transparent pixels stay transparent. Only those cells' own tiles change."""
import struct
def rgb3x(w): return (((w >> 1) & 7), ((w >> 5) & 7), ((w >> 9) & 7))
def d2x(a, b): return sum((x - y) ** 2 for x, y in zip(a, b))
D = 'data/intro/'
m = list(struct.unpack('>2208H', open(D + 'sky_map.bin', 'rb').read()))
t = bytearray(open(D + 'sky_tiles.bin', 'rb').read())
sp = struct.unpack('>28H', open(D + 'sky_pal.bin', 'rb').read())
st = struct.unpack('>32H', open('data/stage1/pal.bin', 'rb').read())
L0 = list(sp[:16]); L2 = list(st[:10]) + list(sp[16:22])
# The sky's line 0 leaves pens 14-15 unused (0000): two dark colours for the
# castle and trees there (2-means over these cells' source pixels that line
# 0 and line 2 match worst), on the 3-bit scale.
def cells():
    for k, w in enumerate(m):
        if w and (w >> 13) & 3 == 2: yield k, w
pv0 = open(D + 'sky_preview.ppm', 'rb').read().split(b'\n', 3); W0 = int(pv0[1].split()[0]); px0 = pv0[3]
bad = []
for k, w in cells():
    cx, cy = k % 48, k // 48
    for y in range(8):
        for x in range(8):
            o = ((cy * 8 + y) * W0 + cx * 8 + x) * 3; c = tuple(v * 7 // 255 for v in px0[o:o + 3])
            e = min(min(d2x(c, rgb3x(q)) for q in L0[1:13]), min(d2x(c, rgb3x(q)) for q in L2[1:]))
            if e >= 2: bad.append(c)
cent = [min(bad, key=sum), max(bad, key=sum)] if bad else [(1, 1, 2), (2, 2, 3)]
for _ in range(10):
    g = [[], []]
    for c in bad: g[min((0, 1), key=lambda j: d2x(c, cent[j]))].append(c)
    cent = [tuple(round(sum(v[i] for v in gg) / len(gg)) for i in range(3)) if gg else cent[j] for j, gg in enumerate(g)]
word = lambda c: (c[2] << 9) | (c[1] << 5) | (c[0] << 1)
L0[14], L0[15] = word(cent[0]), word(cent[1])
sp = list(sp); sp[14], sp[15] = L0[14], L0[15]
open(D + 'sky_pal.bin', 'wb').write(struct.pack('>28H', *sp))
print('pens 14-15:', hex(L0[14]), hex(L0[15]), 'from', len(bad), 'pixels')
pv = open(D + 'sky_preview.ppm', 'rb').read().split(b'\n', 3); W, H = map(int, pv[1].split()); px = pv[3]
def rgb3x(w): return (((w >> 1) & 7), ((w >> 5) & 7), ((w >> 9) & 7))
def d2x(a, b): return sum((x - y) ** 2 for x, y in zip(a, b))
def rgb3(w): return (((w >> 1) & 7), ((w >> 5) & 7), ((w >> 9) & 7))
def d2(a, b): return sum((x - y) ** 2 for x, y in zip(a, b))
tot = [0, 0]
for k, w in enumerate(m):
    if not w or (w >> 13) & 3 != 2: continue
    cx, cy, i = k % 48, k // 48, w & 0x7FF
    hf, vf = w & 0x800, w & 0x1000
    best = None
    for line, pal in ((0, L0), (2, L2)):
        e = 0; pens = [0] * 64
        for y in range(8):
            for x in range(8):
                sx, sy = 7 - x if hf else x, 7 - y if vf else y       # tile pixel (sx, sy) shows at screen (x, y)
                b = t[i * 32 + sy * 4 + sx // 2]; old = (b >> 4) if sx % 2 == 0 else b & 15
                if not old: continue
                o = ((cy * 8 + y) * W + cx * 8 + x) * 3
                c = tuple(v * 7 // 255 for v in px[o:o + 3])        # source colour on the 3-bit scale
                j = min(range(1, 16), key=lambda j: d2(c, rgb3(pal[j]))); e += d2(c, rgb3(pal[j])); pens[sy * 8 + sx] = j
        if best is None or e < best[0]: best = (e, line, pens)
    e, line, pens = best
    for p in range(32):
        t[i * 32 + p] = (pens[p * 2] << 4) | pens[p * 2 + 1]
    m[k] = (w & ~0x6000) | (line << 13)
    tot[line // 2] += 1
open(D + 'sky_tiles.bin', 'wb').write(t)
open(D + 'sky_map.bin', 'wb').write(struct.pack('>2208H', *m))
print('refit: %d cells to line 0, %d kept on line 2' % tuple(tot))
