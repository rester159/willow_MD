#!/bin/bash
cd "/Users/rester159/genesis ports/_capcom/willow/game"
H=../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/menutest
rn() { # name flags keys frames
 rm -f out/.flags; sh tools/perf/b.sh "$2 -DHUDLINE" >/dev/null
 P=""; for v in hl_max_spr hl_max_px hl_over_lines hl_over_frames hl_lost_px hl_frames hl_total hl_hud_spr; do P="$P,FF$(grep " $v\$" out/symbol.txt | awk '{print substr($1,5)}'):2"; done
 KEYS="$3" LOG=$(( $4 - 1 )) PEEK="${P#,}" $H "$HOME/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" out/rom.bin $4 2>&1 | grep peek | tail -1 | python3 -c "
import sys
e=[int(x.split(':')[1],16) for x in sys.stdin.read().split()[1:]]
print('$1: max sprites/line',e[0],'max px/line',e[1],'over frames',e[3],'of',e[5],'table max',e[6],'frames at 80',e[7])"
}
rn "1P stage 1 route" "-DNO_INTRO -DINVINCIBLE" "$(cat out/perf/route100.keys)" 2600
rn "1P stage 5 top" "-DNO_INTRO -DINVINCIBLE -DSTAGE5_VIEW -DS5_CP=10" "$(cat out/perf/s5top.keys)" 3500
rn "1P stage 6 a2" "" "$(python3 tools/perf/dbgkeys.py 6 2 R)" 3200
rn "2P stage 1" "-DCOOP_AUTOJOIN" "$(python3 tools/perf/dbgkeys.py 1 1 R)" 3200
rn "2P stage 5 p10" "-DCOOP_AUTOJOIN" "$(python3 tools/perf/dbgkeys.py 5 10 R)" 3500
rn "2P stage 5 top" "-DCOOP_AUTOJOIN" "$(python3 tools/perf/s5dbg.py)" 3900
rn "2P stage 6 a2" "-DCOOP_AUTOJOIN" "$(python3 tools/perf/dbgkeys.py 6 2 R)" 3200
