#!/bin/bash
cd "/Users/rester159/genesis ports/_capcom/willow/game"
bash tools/perf/r.sh 2 r2f VIS_EXTRA=0:255:0:63 > out/perf/r2f.out 2>&1 &
bash tools/perf/r.sh 1 r1f VIS_EXTRA=0:479:0:63 > out/perf/r1f.out 2>&1 &
bash tools/perf/r.sh 3 r3f VIS_EXTRA=0:767:0:63 > out/perf/r3f.out 2>&1 &
bash tools/perf/r.sh 5 r5f VIS_EXTRA=0:767:0:95 > out/perf/r5f.out 2>&1 &
wait
