#!/bin/sh
# Stage 6's music (0x0C the castle, 0x0D the tower, 0x0E Kael, 0x0F
# Bavmorda, 0x1D the ending; each posted with 0x01FA) and effects (specs/
# stage6.md 9). Same steps as run.sh.
set -e
H="$(cd "$(dirname "$0")" && pwd)"; R="$H/../../.."; D="$H/../../data/audio"
CORE="$R/build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/gpgx-cps/genesis_plus_gx_libretro.dylib"
SND="$R/build/native-audio-admission/soundcpu.bin"
JAVA=/opt/homebrew/opt/openjdk@21/bin/java; XGM2="$R/build/sgdk-native/bin/xgm2tool.jar"
# The loop finder misses 0x0C (period 12064 ticks from tick 136: looped
# over its last full periods, --loop 5217) and 0x0E (no period within the
# 65536 logged ticks: the whole log loops, an approximation).
for m in 0c 0d 0e 0f 1d; do
  "$H/opmlog" "$CORE" "$SND" 0x$m "$D/music$m.opmlog" --then 0xFA@8:1
  L=; [ $m = 0c ] && L="--loop 5217"; [ $m = 0e ] && L="--loop 0"
  python3 "$H/opm2opn.py" "$D/music$m.opmlog" "$D/music$m.vgm" --lead 1 $L --report "$D/music$m.json" > /dev/null
  "$JAVA" -Djava.awt.headless=true -jar "$XGM2" "$D/music$m.vgm" "$D/music$m.xgc" > /dev/null
  echo "music $m: $(stat -f %z "$D/music$m.xgc") bytes"
done
# OKI voices (9): 0xA2 Kael's slash, 0xA6 Bavmorda's star burst, 0xA9 her
# energy ball, 0x86-0x89 the ending's text chimes.
for c in a2 a6 a9 86 87 88 89; do
  python3 -I "$H/okiwav.py" "$R/willow.zip" "$SND" 0x$c "$D/render/oki$c.wav" > /dev/null
done
