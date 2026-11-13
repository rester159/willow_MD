#!/usr/bin/env python3
"""rival.py: Madmartigan as Sorsha's rival at the snow camp (Sorsha's Quest,
Home only; out/audit/quest/PLAN.md, option A colours). Draws stage 4's
Sorsha-boss logic (src/fam_stage4.c sorsha_update, its 15 chains x facing)
with Madmartigan's player poses and tiles as they are (data/mad, no new
art): data/rival/anim.bin (the boss's chains on his pose slots,
specs/madmartigan.md 4 / tools/sorsha_player.py's slot list), sat.bin (his
entries on CRAM line 1: stage 4's line 1 takes his colours during the duel),
boxes.bin (his body box, twice, as the boss's two boxes per pose)."""
import struct
M = open('data/mad/poses.bin', 'rb').read(); N = 70
B = open('data/mad/boxes.bin', 'rb').read()
O = 'data/rival/'
# his slots: walk R 0-5, L 6-11, crouch 12/13, stand 14/15, sword 16/17 R
# 18/19 L, crouch sword 20-23, air sword 24/25 R 26/27 L, jump 28/29, slide
# 48/49, death 58-63 R 64-69 L
def ch(*steps, loop=0xFF): return bytes([len(steps)]) + b''.join(bytes([p, t]) for p, t in steps) + bytes([loop])
R = [ch((14, 0x30), (14, 0xFF)),                  # 0 intro
     ch((14, 0xFF)),                              # 2 stand
     ch(*[(k, 6) for k in range(6)], loop=0),     # 4 walk
     ch((12, 0xFF)),                              # 6 crouch
     ch((28, 0xFF)),                              # 8 jump
     ch((14, 0xFF)),                              # 10 slash wind-up
     ch((16, 8), (17, 0xFF)),                     # 12 slash
     ch((28, 0xFF)),                              # 14 air wind-up
     ch((24, 8), (25, 0xFF)),                     # 16 air slash
     ch((12, 0xFF)),                              # 18 thrust wind-up
     ch((20, 8), (21, 0xFF)),                     # 20 thrust (crouch sword)
     ch((16, 8), (17, 0xFF)),                     # 22 high thrust
     ch((48, 0xFF)),                              # 24 backstep
     ch((48, 0xFF)),                              # 26 backstep 2
     ch(*[(58 + k, 8) for k in range(5)], (63, 0xFF))]   # 28 defeated
L = {0: 1, 6: 6, 12: 13, 14: 15, 16: 18, 17: 19, 20: 22, 21: 23, 24: 26, 25: 27, 28: 29, 48: 49}
def left(c):
    n = c[0]; out = bytearray(c)
    for i in range(n):
        p = c[1 + 2 * i]
        out[1 + 2 * i] = p + 6 if p < 6 else p + 6 if 58 <= p <= 63 else L[p]
    return bytes(out)
anim = b''
for c in R: anim += c + left(c)
open(O + 'anim.bin', 'wb').write(anim + (b'\0' if len(anim) & 1 else b''))
sat = bytearray()
ne = (len(M) - N * 6) // 6
for i in range(ne):
    dy, size, t, dx = struct.unpack_from('>hBBh', M, N * 6 + i * 6)
    sat += struct.pack('>hHHh', dy, (size & 0x0F) << 8, 1 << 13 | (size >> 7) << 11 | t, dx)
open(O + 'sat.bin', 'wb').write(bytes(sat))
bx = b''
for i in range(N): bx += B[i * 8:i * 8 + 8] * 2
open(O + 'boxes.bin', 'wb').write(bx)
print('rival: anim', len(anim), 'sat', len(sat), 'boxes', len(bx))
