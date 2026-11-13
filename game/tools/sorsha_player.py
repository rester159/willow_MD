#!/usr/bin/env python3
"""sorsha_player.py: Sorsha as a Home-only player form (study phase 1).

Builds data/sorsha/poses.bin and nodes.bin in the player pack format
(tools/posepack.py: 70 poses, entries after them) from her stage 4 boss
poses (data/s4spr/sorsha, 52 poses; tiles used in place: s4_sorsha_tiles).
Each of Madmartigan's 70 pose slots (specs/madmartigan.md 4) gets one of her
poses; moves her art lacks (crouch sword, drop, climb) reuse near poses for
now. Nodes: Madmartigan's, except her 4-frame walk loop. Her feet are put
on his standing pose's bottom row."""
import struct
S = 'data/s4spr/sorsha/'; O = 'data/sorsha/'
P = open(S + 'poses.bin', 'rb').read(); NP = 52
def pose(i):
    t, nt, ns, e = struct.unpack('>HBBH', P[i * 6:i * 6 + 6])
    return t, nt, [list(struct.unpack('>hBBh', P[NP * 6 + (e + k) * 6:][:6])) for k in range(ns)]
def bottom(ents): return max(y + 8 * ((sz & 3) + 1) for y, sz, ti, x in ents)
M = open('data/mad/poses.bin', 'rb').read()
def mad(i):
    t, nt, ns, e = struct.unpack('>HBBH', M[i * 6:i * 6 + 6])
    return [struct.unpack('>hBBh', M[420 + (e + k) * 6:][:6]) for k in range(ns)]
shift = bottom(mad(14)) - bottom(pose(6)[2])
# slot -> her pose (right / left pairs as in her pack: left = mirrored entry)
R = {6: 8, 7: 9, 10: 14, 11: 15, 12: 16, 13: 17, 18: 19, 20: 22, 21: 23, 24: 25, 26: 28, 27: 29,
     30: 32, 31: 33, 34: 35, 36: 37, 38: 39, 40: 41, 42: 47, 43: 48, 44: 49, 45: 50, 46: 51}
slot = [10, 11, 12, 13, 10, 11] + [14, 15, 16, 17, 14, 15]   # walk R, L
slot += [18, 19, 6, 8]                                        # crouch R/L, stand R/L
slot += [24, 21, 25, 23]                                      # sword, follow: R, R, L, L
slot += [18, 18, 19, 19]                                      # crouch sword (missing: crouch)
slot += [27, 26, 29, 28]                                      # air sword, follow
slot += [7, 9]                                                # jump R/L (her jump = 2nd stand frame)
slot += [18, 18, 7, 7, 19, 19, 9, 9]                          # drop R/L (missing)
slot += [7, 7, 18, 18, 6, 9, 9, 19, 19, 8]                    # climb R/L (missing)
slot += [38, 40, 39, 41, 18, 19]                              # slide R, L, recover R/L
slot += [42, 43, 44, 45]                                      # road death
slot += [42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 51, 51]      # death R/L
assert len(slot) == 70, len(slot)
dirs = b''; ents = b''; ne = 0
for s in slot:
    t, nt, es = pose(s)
    assert nt <= 43 and len(es) <= 6, (s, nt, len(es))
    dirs += struct.pack('>HBBH', t, nt, len(es), ne)
    for y, sz, ti, x in es: ents += struct.pack('>hBBh', y + shift, sz, ti, x)
    ne += len(es)
import os; os.makedirs(O, exist_ok=True)
open(O + 'poses.bin', 'wb').write(dirs + ents)
n = bytearray(open('data/mad/nodes.bin', 'rb').read())
n[3 * 4 + 2] = 0; n[9 * 4 + 2] = 6                            # walk: 4-frame loops
open(O + 'nodes.bin', 'wb').write(bytes(n))
print('sorsha player poses', len(dirs + ents), 'bytes; feet shift', shift)
# Line 0 in 1P: her own 16 colours (exact); the hurt flash: each pen on the
# arcade's warm flash ramp by brightness (as Madmartigan's table looks).
pal = struct.unpack('>16H', open(S + 'pal.bin', 'rb').read()[:32])
open(O + 'pal.bin', 'wb').write(struct.pack('>16H', *pal))
ramp = [0x000E, 0x006E, 0x048E, 0x00CE, 0x06EE, 0x08EE, 0x0AEE, 0x0CEE]
lum = lambda w: ((w >> 1) & 7) * 3 + ((w >> 5) & 7) * 6 + ((w >> 9) & 7)
fl = [0 if w == 0 else ramp[min(7, lum(w) * 8 // 71)] for w in pal[1:]] + [0]
open(O + 'flash.bin', 'wb').write(struct.pack('>16H', *fl))
# Co-op (Home 2P, Willow + Sorsha): line 0 is the co-op line (data/coop/line0.bin)
# with Madmartigan's own slots 2, 5, 7, 11 (Willow never uses them) given four
# of her colours; her other pens go to the nearest slot. Her tiles in that
# layout are a second copy (only the tiles her 70 player poses use), with
# poses2.bin pointing at it; 1P keeps her original tiles and own colours.
import itertools
from collections import Counter
def rgb3(w): return (((w >> 1) & 7), ((w >> 5) & 7), ((w >> 9) & 7))
def d2(a, b): return sum((x - y) ** 2 for x, y in zip(rgb3(a), rgb3(b)))
L0 = list(struct.unpack('>16H', open('data/coop/line0.bin', 'rb').read()[:32]))
T = open(S + 'tiles.bin', 'rb').read()
used = sorted({t for i in range(70) for t in range(*(lambda d: (d[0], d[0] + d[1]))(struct.unpack('>HB', dirs[i * 6:i * 6 + 3])))})
cs = Counter()
for t in used:
    for b in T[t * 32:t * 32 + 32]: cs[b >> 4] += 1; cs[b & 15] += 1
hp = [p for p in range(1, 16) if cs[p]]
best = None
for four in itertools.combinations(hp, 4):
    L = L0[:]
    for s, p in zip((2, 5, 7, 11), four): L[s] = pal[p]
    e = sum(cs[p] * min(d2(pal[p], L[j]) for j in range(1, 16)) for p in hp)
    if best is None or e < best[0]: best = (e, L)
L = best[1]
lut = [0] + [min(range(1, 16), key=lambda j: d2(pal[p], L[j])) for p in range(1, 16)]
open(O + 'line0_2p.bin', 'wb').write(struct.pack('>16H', *L))
idx = {t: k for k, t in enumerate(used)}
open(O + 'tiles2p.bin', 'wb').write(bytes((lut[b >> 4] << 4) | lut[b & 15] for t in used for b in T[t * 32:t * 32 + 32]))
d2p = bytearray(dirs)
for i in range(70):
    t = struct.unpack('>H', dirs[i * 6:i * 6 + 2])[0]; d2p[i * 6:i * 6 + 2] = struct.pack('>H', idx[t])
open(O + 'poses2p.bin', 'wb').write(bytes(d2p) + ents)
off = sum(cs[p] for p in hp if d2(pal[p], L[lut[p]])) / sum(cs[p] for p in hp)
print('2P: %d tiles (%d bytes), %.0f%% of her pixels a shade off; line0 %s' % (len(used), len(used) * 32, off * 100, ' '.join('%04x' % w for w in L)))
