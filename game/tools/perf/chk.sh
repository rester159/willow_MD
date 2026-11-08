#!/bin/bash
cd "/Users/rester159/genesis ports/_capcom/willow/game"
R=$PWD/willow-native-m2-20261106p.bin
H=../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/menutest
CORE="$HOME/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
HP=$1
# after-death map, six stages
for st in 0 1 2 3 4 5; do
K="60-62:D;100-102:A;140-142:U;160-162:U;180-182:D;200-202:D;220-222:C"
t=240; i=0; while [ $i -lt $st ]; do K="$K;$t-$((t+2)):R"; t=$((t+20)); i=$((i+1)); done
K="$K;$t-$((t+2)):D"; t=$((t+20)); K="$K;$t-$((t+2)):D"; t=$((t+20)); K="$K;$t-$((t+2)):R"; t=$((t+20)); K="$K;$t-$((t+2)):D"; t=$((t+20)); K="$K;$t-$((t+2)):A"
T=2400; O=out/perf/m$st; rm -rf $O; mkdir -p $O
POKE="$T:$HP:00;$((T+6)):$HP:00" KEYS="$K" DUMP=$((T+190)),$((T+630)),$((T+690)) DIR=$O $H "$CORE" "$R" $((T+710)) >/dev/null 2>&1
done
python3 tools/perf/sheet.py out/perf/m.png 3 out/perf/m?/*.ppm
# stage 6 stepped
rm -rf out/perf/s6; mkdir -p out/perf/s6
KEYS="$(python3 tools/perf/dbgkeys.py 6 2 R)" DUMP=$(seq 1500 1 1600 | paste -sd, -) DIR=out/perf/s6 $H "$CORE" "$R" 1605 >/dev/null 2>&1
python3 - <<'PY'
import os
fs=sorted(os.listdir('out/perf/s6'))
def rd(f): return open('out/perf/s6/'+f,'rb').read().split(b"\n",3)[3]
bl=[]
for i in range(1,len(fs)-1):
    a,b,c=rd(fs[i-1]),rd(fs[i]),rd(fs[i+1])
    bl.append(sum(1 for j in range(0,16*960,3) if a[j:j+3]==c[j:j+3] and a[j:j+3]!=b[j:j+3]))
print('stage 6 single-frame pixel blinks, top 16 lines, per frame: max',max(bl),'frames>0',sum(1 for x in bl if x),'of',len(bl))
PY
