#!/bin/bash
cd "/Users/rester159/genesis ports/_capcom/willow/game"
bash tools/perf/r.sh 1 r1a > out/perf/r1a.out 2>&1 &
bash tools/perf/r.sh 3 r3a > out/perf/r3a.out 2>&1 &
bash tools/perf/r.sh 5 r5a > out/perf/r5a.out 2>&1 &
bash tools/perf/r.sh 4 r4a > out/perf/r4a.out 2>&1 &
wait
