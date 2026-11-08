#!/bin/sh
# Stage 3's music (0x04 island, 0x05 lake, 0x06 boss) and the front end's
# 0x16 ranking / 0x18 name entry; stage-3 effects. Same steps as run.sh.
set -e
H="$(cd "$(dirname "$0")" && pwd)"; R="$H/../../.."; D="$H/../../data/audio"
CORE="$R/build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/gpgx-cps/genesis_plus_gx_libretro.dylib"
SND="$R/build/native-audio-admission/soundcpu.bin"
JAVA=/opt/homebrew/opt/openjdk@21/bin/java; XGM2="$R/build/sgdk-native/bin/xgm2tool.jar"
# Posted with 0x01FA (fade-in interval 1); loops found by opmlog.
for m in 04 05; do
  "$H/opmlog" "$CORE" "$SND" 0x$m "$D/music$m.opmlog" --then 0xFA@8:1
  python3 "$H/opm2opn.py" "$D/music$m.opmlog" "$D/music$m.vgm" --lead 1 --report "$D/music$m.json" > /dev/null
done
# 0x06 boss (no fade-in word), 0x18 name entry, 0x16 ranking.
for m in 06 16 18; do
  "$H/opmlog" "$CORE" "$SND" 0x$m "$D/music$m.opmlog"
  python3 "$H/opm2opn.py" "$D/music$m.opmlog" "$D/music$m.vgm" --lead 1 --report "$D/music$m.json" > /dev/null
done
for m in 04 05 06 16 18; do
  "$JAVA" -Djava.awt.headless=true -jar "$XGM2" "$D/music$m.vgm" "$D/music$m.xgc" > /dev/null
done
for e in 3e; do
  "$H/opmlog" "$CORE" "$SND" 0x$e "$D/sfx$e.opmlog" --max 500
  "$H/opmrender" "$D/sfx$e.opmlog" "$D/render/sfx${e}_opm.wav" --tail 3 > /dev/null
  python3 "$H/wavtrim.py" "$D/render/sfx${e}_opm.wav" "$D/render/sfx${e}_opm.wav"
done
for c in 81 8f 91 92 93 94 95; do
  python3 -I "$H/okiwav.py" "$R/willow.zip" "$SND" 0x$c "$D/render/oki$c.wav" > /dev/null
done
