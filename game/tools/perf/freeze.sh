#!/bin/bash
cd "/Users/rester159/genesis ports/_capcom/willow/game"
F() { bash tools/perf/r.sh $1 r${1}z VIS_EXTRA=$2 FREEZE_CACHE="/Users/rester159/genesis ports/_capcom/willow/game/data/stage$1/decode.cache" $3 > out/perf/r${1}z.out 2>&1; }
F 2 0:255:0:63 FREEZE_POOL=404 &
#F 1 0:479:0:63 &
#F 3 0:767:0:63 &
#F 5 0:767:0:95 FREEZE_POOL=490 &
#F 4 0:2047:0:1023 &
wait
