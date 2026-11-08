#!/bin/sh
# Stage 4's music (0x07 the camp, 0x08 the ride, 0x09 the cave; all posted
# with 0x01FA) and effects (FM 0x28 parry clash, 0x39 hoofbeats; OKI 0x96-0x9A,
# 0x9E, 0x9F). Same steps as run.sh.
set -e
H="$(cd "$(dirname "$0")" && pwd)"; R="$H/../../.."; D="$H/../../data/audio"
CORE="$R/build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/gpgx-cps/genesis_plus_gx_libretro.dylib"
SND="$R/build/native-audio-admission/soundcpu.bin"
JAVA=/opt/homebrew/opt/openjdk@21/bin/java; XGM2="$R/build/sgdk-native/bin/xgm2tool.jar"
# 0x07: the loop finder misses its period (10208 ticks from tick 136):
# looped over its last full periods (--loop 4289; 2026-11-03).
for m in 07 08 09; do
  "$H/opmlog" "$CORE" "$SND" 0x$m "$D/music$m.opmlog" --then 0xFA@8:1
  L=; [ $m = 07 ] && L="--loop 4289"
  python3 "$H/opm2opn.py" "$D/music$m.opmlog" "$D/music$m.vgm" --lead 1 $L --report "$D/music$m.json" > /dev/null
  "$JAVA" -Djava.awt.headless=true -jar "$XGM2" "$D/music$m.vgm" "$D/music$m.xgc" > /dev/null
done
for e in 28 39; do
  "$H/opmlog" "$CORE" "$SND" 0x$e "$D/sfx$e.opmlog" --max 500
  "$H/opmrender" "$D/sfx$e.opmlog" "$D/render/sfx${e}_opm.wav" --tail 3 > /dev/null
  python3 "$H/wavtrim.py" "$D/render/sfx${e}_opm.wav" "$D/render/sfx${e}_opm.wav"
done
for c in 96 97 98 99 9a 9e 9f; do
  python3 -I "$H/okiwav.py" "$R/willow.zip" "$SND" 0x$c "$D/render/oki$c.wav" > /dev/null
done
