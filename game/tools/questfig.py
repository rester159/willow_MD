#!/usr/bin/env python3
"""questfig.py: Sorsha's figure on her world map (Sorsha's Quest): her stage
4 standing pose (data/s4spr/sorsha pose 6), recoloured to the map line that
fits her colours best (the map uses all four lines), as sprite tiles and
ready sprite-table entries. Out: data/quest/qfig_tiles.bin, qfig_spr.bin
(count, then {dy, size << 8, line << 13 | tile, dx}); data/quest/quest.h gets
QFIG_TILES."""
import struct
S = 'data/s4spr/sorsha/'; O = 'data/quest/'
P = open(S + 'poses.bin', 'rb').read(); N = 52
T = open(S + 'tiles.bin', 'rb').read()
sp = struct.unpack('>16H', open(S + 'pal.bin', 'rb').read()[:32])
mp = struct.unpack('>64H', open('data/intro/map_pal.bin', 'rb').read())
def rgb(w): return ((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7)
def d(a, b): return sum((x - y) ** 2 for x, y in zip(rgb(a), rgb(b)))
t0, nt, ns, e = struct.unpack('>HBBH', P[6 * 6:6 * 6 + 6])
ents = [struct.unpack('>hBBh', P[N * 6 + (e + k) * 6:][:6]) for k in range(ns)]
def maps(line): return [0] + [min(range(1, 16), key=lambda k: d(sp[v], mp[line * 16 + k])) for v in range(1, 16)]
M = [maps(l) for l in range(4)]
tiles = bytearray(); spr = struct.pack('>H', len(ents)); used = []
for dy, size, ti, dx in ents:
    w, h = ((size >> 2) & 3) + 1, (size & 3) + 1
    px = [b for i in range(w * h) for b in T[(t0 + ti + i) * 32:(t0 + ti + i + 1) * 32]]
    pens = [v for b in px for v in (b >> 4, b & 15) if v]
    line = min(range(4), key=lambda l: sum(d(sp[v], mp[l * 16 + M[l][v]]) for v in pens))
    used.append(line)
    base = len(tiles) // 32
    for b in px: tiles.append(M[line][b >> 4] << 4 | M[line][b & 15])
    spr += struct.pack('>hHHh', dy, (size & 15) << 8, line << 13 | (size >> 7) << 11 | base, dx)
nt = len(tiles) // 32
open(O + 'qfig_tiles.bin', 'wb').write(bytes(tiles)); open(O + 'qfig_spr.bin', 'wb').write(spr)
h = [l for l in open(O + 'quest.h').read().splitlines() if not l.startswith('#define QFIG_')]
open(O + 'quest.h', 'w').write('\n'.join(h + ['#define QFIG_TILES %d' % nt]) + '\n')
print('figure: lines', used, 'tiles', nt, 'sprites', len(ents))
