#!/bin/sh
# frametime.sh KEYSFILE FRAMES: (idle numbers: a -DIDLE_MEASURE build; dropped
# frames: a normal build) per frame, the 68000 time left before
# VBlank after the game's work (src/main.c idle_spin: a spin on the VDP
# VBlank flag, ~37.7 cycles an iteration, calibrated with an 18,000-cycle
# loop), and dropped frames (an emulator frame in which game_frame did not
# advance). An NTSC frame is ~127,800 cycles. (The VDP H/V counter does not
# give mid-frame time in this emulator; do not use it for timing.)
cd "$(dirname "$0")/.." || exit 1
A=$(grep " idle_spin$" out/symbol.txt | awk '{print substr($1,5)}')
G=$(grep " game_frame$" out/symbol.txt | awk '{print substr($1,5)}')
K=$(tr ';' '\n' < $1 | awk -F: -v t=100 '{split($1,a,"-"); printf "%d-%d:%s;", a[1]+t, a[2]+t, $2}')
H=../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/menutest
KEYS="$K" LOG=1 PEEK="FF$A:2,FF$G:2" $H "$HOME/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" out/rom.bin $2 2>&1 | grep peek | python3 -c "
import sys
r=[l.split() for l in sys.stdin]
idle=[int(x[1].split(':')[1],16)*37.7 for x in r][90:]
g=[int(x[2].split(':')[1],16) for x in r][90:]
drops=sum(1 for a,b in zip(g,g[1:]) if b==a)
s=sorted(idle)
print('idle cycles left: min %d (%.0f%%) p1 %d median %d | dropped frames %d of %d' % (s[0], 100*s[0]/127800, s[len(s)//100], s[len(s)//2], drops, len(g)-1))"
