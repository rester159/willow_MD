# questmap.py: Sorsha's Quest world map (Home only), over the shared map
# tiles (map 1's tiles and palette + map 6's extra tiles, tools/mapshare.py).
# Her chapters on the arcade's discs: I at Nockmaar castle (the arcade's VI
# disc), II-V on the arcade's own II-V discs, VI back at the castle; the
# Nelwyn village's disc is dark (not on her route). Per chapter: finished
# discs dark, the current one blinks (numeral <-> dark).
# Out: data/quest/qmap_map.bin (chapter 1's words, 1120), qmap_xtiles.bin
# (tiles no shared map has), qmap_diff.bin: per chapter 2-6 a count then
# (cell, word) pairs against chapter 1; qmap_alt.bin: per chapter a count
# then (cell, numeral word, dark word) for the blinking disc.
import struct, sys
D = 'data/intro/'; S = 'data/intro/share/'; O = 'data/quest/'
W, H = 40, 28
bt = open(D + 'map_tiles.bin', 'rb').read(); NB = len(bt) // 32
x6 = open(S + 'map6_xtiles.bin', 'rb').read()
T = bt + x6
def words(n): return list(struct.unpack('>1120H', open(S + n + '_map.bin', 'rb').read()))
m1 = list(struct.unpack('>1120H', open(D + 'map_map.bin', 'rb').read())); m6 = words('map6')
def pens(i):
    return [((T[i * 32 + y * 4 + x // 2] >> 4) if x % 2 == 0 else T[i * 32 + y * 4 + x // 2] & 15) for y in range(8) for x in range(8)]
def render(m):                                  # (line << 4 | pen) per pixel
    px = [0] * (320 * 224)
    for c, w in enumerate(m):
        pp = pens(w & 0x7FF); pl = (w >> 13) & 3; hf = w & 0x800; vf = w & 0x1000
        cx, cy = c % W, c // W
        for y in range(8):
            for x in range(8):
                sx = 7 - x if hf else x; sy = 7 - y if vf else y
                v = pp[sy * 8 + sx]
                px[(cy * 8 + y) * 320 + cx * 8 + x] = pl << 4 | v
    return px
P1, P6 = render(m1), render(m6)
# disc boxes (pixels, numerals' extent) as the mock-ups (out/perf/quest/qmock.py)
B = {'VI': (192, 34, 207, 43), 'V': (264, 58, 279, 68), 'II': (64, 74, 79, 84), 'III': (136, 122, 151, 131), 'I': (4, 130, 19, 140), 'IV': (240, 146, 255, 155)}
def box(k): x0, y0, x1, y1 = B[k]; return x0 - 3, y0 - 4, x1 + 3, y1 + 4
def inbox(k):
    x0, y0, x1, y1 = box(k)
    return [(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)]
# a disc's pixels: those that differ lit (map 1) vs dark (map 6)
MASK = {k: [(x, y) for x, y in inbox(k) if P1[y * 320 + x] != P6[y * 320 + x]] for k in ('I', 'II', 'III', 'IV', 'V')}
cnt = {}
for k in ('II', 'III', 'IV', 'V'):
    for x, y in MASK[k]:
        a, b = P1[y * 320 + x], P6[y * 320 + x]; cnt.setdefault(a, {}); cnt[a][b] = cnt[a].get(b, 0) + 1
DARK = {a: max(d, key=d.get) for a, d in cnt.items()}
MASK['VI'] = [(x, y) for x, y in inbox('VI') if P1[y * 320 + x] in DARK and DARK[P1[y * 320 + x]] != P1[y * 320 + x]]
# the village's I disc onto the castle: the offset that best covers the VI disc
sx0, sy0 = box('I')[:2]; dx0, dy0 = box('VI')[:2]
vi = set(MASK['VI'])
best = None
for oy in range(-3, 4):
    for ox in range(-3, 4):
        moved = set((x - sx0 + dx0 + ox, y - sy0 + dy0 + oy) for x, y in MASK['I'])
        sc = len(vi ^ moved)
        if best is None or sc < best[0]: best = (sc, ox, oy)
OX, OY = best[1] + dx0 - sx0, best[2] + dy0 - sy0
def lit(px, k):
    if k == 'Ic':                                # her chapter I at the castle
        for x, y in MASK['I']: px[(y + OY) * 320 + x + OX] = P1[y * 320 + x]
        return
    for x, y in MASK[k]: px[y * 320 + x] = P1[y * 320 + x]
def dark(px, k):
    if k == 'Ic':
        for x, y in MASK['I']: px[(y + OY) * 320 + x + OX] = P6[y * 320 + x]
        return
    if k == 'VI':
        for x, y in MASK['VI']: px[y * 320 + x] = DARK[P1[y * 320 + x]]
        return
    for x, y in MASK[k]: px[y * 320 + x] = P6[y * 320 + x]
base = list(P1)
for y in range(70, 129):                          # map 1's Willow by the village: map 6's pixels
    for x in range(0, 46): base[y * 320 + x] = P6[y * 320 + x]
dark(base, 'I')
def chapter(ch, on):
    px = list(base)
    if ch == 1:
        dark(px, 'VI')                           # the VI disc under her I
        (lit if on else dark)(px, 'Ic')
    elif ch == 6: (lit if on else dark)(px, 'VI')
    for k, n in (('II', 2), ('III', 3), ('IV', 4), ('V', 5)):
        if n < ch or (n == ch and not on): dark(px, k)
    return px
# tiles: shared index first (exact, with flips), else new
lib = {}
for i in range(len(T) // 32):
    pp = pens(i)
    for f in range(4):
        q = tuple(pp[(7 - y if f & 2 else y) * 8 + (7 - x if f & 1 else x)] for y in range(8) for x in range(8))
        lib.setdefault(q, (i, f))
PAL = struct.unpack('>64H', open(D + 'map_pal.bin', 'rb').read())
def rgb(w): return (((w >> 1) & 7), ((w >> 5) & 7), ((w >> 9) & 7))
def near(pl, v):                                 # another line's pen in line pl: the nearest colour (not pen 0)
    c = rgb(PAL[v]); best = None
    for k in range(1, 16):
        d = rgb(PAL[pl * 16 + k]); e = sum((a - b) ** 2 for a, b in zip(c, d))
        if best is None or e < best[0]: best = (e, k)
    return best[1]
new = []
def encode(px):
    out = []
    for c in range(W * H):
        cx, cy = c % W, c // W
        cell = [px[(cy * 8 + y) * 320 + cx * 8 + x] for y in range(8) for x in range(8)]
        lines = set(v >> 4 for v in cell if v & 15)
        pl = min(lines) if lines else 0
        if len(lines) > 1:                       # a cell mixing lines: keep the most used line's pens (pen 0 elsewhere)
            cnt = {}
            for v in cell:
                if v & 15: cnt[v >> 4] = cnt.get(v >> 4, 0) + 1
            pl = max(cnt, key=cnt.get)
        q = tuple((v & 15) if (v >> 4) == pl or not (v & 15) else near(pl, v) for v in cell)
        if q not in lib:
            idx = len(T) // 32 + len(new); new.append(q)
            lib[q] = (idx, 0)
        i, f = lib[q]
        out.append(pl << 13 | (f & 1) << 11 | (f >> 1) << 12 | i)
    return out
maps = {}
for ch in range(1, 7):
    maps[ch] = (encode(chapter(ch, True)), encode(chapter(ch, False)))
m = maps[1][0]
open(O + 'qmap_map.bin', 'wb').write(struct.pack('>1120H', *m))
xt = bytearray()
for q in new:
    for y in range(8):
        for x in range(0, 8, 2): xt.append(q[y * 8 + x] << 4 | q[y * 8 + x + 1])
open(O + 'qmap_xtiles.bin', 'wb').write(bytes(xt))
diff = bytearray(); alt = bytearray()
for ch in range(1, 7):
    lit, dk = maps[ch]
    d = [(c, lit[c]) for c in range(1120) if lit[c] != m[c]]
    if ch > 1: diff += struct.pack('>H', len(d)) + b''.join(struct.pack('>HH', *p) for p in d)
    a = [(c, lit[c], dk[c]) for c in range(1120) if lit[c] != dk[c]]
    alt += struct.pack('>H', len(a)) + b''.join(struct.pack('>HHH', *p) for p in a)
open(O + 'qmap_diff.bin', 'wb').write(bytes(diff))
open(O + 'qmap_alt.bin', 'wb').write(bytes(alt))
import os
keep = [l for l in open(O + 'quest.h').read().splitlines() if not l.startswith('#define QMAP_')] if os.path.exists(O + 'quest.h') else []
open(O + 'quest.h', 'w').write('\n'.join(['#define QMAP_XTILES %d' % len(new)] + keep) + '\n')
print('base tiles', NB, 'map6 extras', len(x6) // 32, 'new tiles', len(new), 'diff', len(diff), 'alt', len(alt))
