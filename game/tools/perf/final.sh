#!/bin/bash
cd "/Users/rester159/genesis ports/_capcom/willow/game"
S="sh tools/perf/shown.sh"
rm -f out/.flags
echo "1P stage 1 route: $($S "-DNO_INTRO -DINVINCIBLE" "$(cat out/perf/route100.keys)" 2600 | tail -1)"
echo "1P stage 1 bridge collapse: $($S "-DNO_INTRO -DINVINCIBLE -DSORC_KILL_AT=1150" "$(cat out/perf/route100.keys)" 1700 | tail -1)"
echo "1P stage 5 top: $($S "-DNO_INTRO -DINVINCIBLE -DSTAGE5_VIEW -DS5_CP=10" "$(cat out/perf/s5top.keys)" 3500 | tail -1)"
echo "1P stage 6 a2: $($S "" "$(python3 tools/perf/dbgkeys.py 6 2 R)" 3200 | tail -1)"
echo "2P stage 1: $($S "-DCOOP_AUTOJOIN" "$(python3 tools/perf/dbgkeys.py 1 1 R)" 3200 | tail -1)"
echo "2P stage 5 p10: $($S "-DCOOP_AUTOJOIN" "$(python3 tools/perf/dbgkeys.py 5 10 R)" 3500 | tail -1)"
echo "2P stage 5 top: $($S "-DCOOP_AUTOJOIN" "$(python3 tools/perf/s5dbg.py)" 3900 | tail -1)"
echo "2P stage 6 a2: $($S "-DCOOP_AUTOJOIN" "$(python3 tools/perf/dbgkeys.py 6 2 R)" 3200 | tail -1)"
bash tools/perf/hl.sh
rm -f out/.flags; sh tools/perf/b.sh "-DNO_INTRO -DINVINCIBLE"; sh tools/tracecheck.sh | grep -c " 0 differ"; OPTS=--tolerant sh tools/magiccheck.sh 2>&1 | tail -1
rm -f out/.flags; sh tools/perf/b.sh ""; shasum out/rom.bin
