#!/usr/bin/env python3
"""checkgolden.py STAGE0_FIXTURES MUSIC13_OPMLOG: compare the YM2151 data
writes of the first 4,096 music services in the admission golden fixture
(test_music.c, 040C only) with opmlog's full-IRQ log (timer registers
10/11/12/14 excluded, as the golden run does not execute the IRQ wrapper)."""
import struct, sys, collections
gold = collections.defaultdict(list)
b = open(sys.argv[1], 'rb').read(); p = 0
while p < len(b):
    h = struct.unpack('>7I', b[p:p+28]); p += 28 + 4096
    tick, n = h[2], h[6]; reg = None; gold[tick]
    for i in range(n):
        a, v, w = struct.unpack('>3I', b[p:p+12]); p += 12
        if w and a == 0xf000: reg = v
        elif w and a == 0xf001 and reg not in (0x10, 0x11, 0x12, 0x14): gold[tick].append((reg, v))
mine = collections.defaultdict(list)
for line in open(sys.argv[2]):
    f = line.split()
    if f and f[0] == 'Y':
        t, r, v = int(f[1]), int(f[2], 16), int(f[3], 16)
        if r not in (0x10, 0x11, 0x12, 0x14): mine[t].append((r, v))
ticks = sorted(gold); bad = [t for t in range(1, ticks[-1] + 1) if gold.get(t, []) != mine.get(t, [])]
print("golden ticks %d..%d, %d YM data writes, mismatching ticks: %d%s" % (
    ticks[0], ticks[-1], sum(len(gold[t]) for t in ticks), len(bad), (" first %d" % bad[0]) if bad else ""))
sys.exit(1 if bad else 0)
