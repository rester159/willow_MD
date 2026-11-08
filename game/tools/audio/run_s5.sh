#!/bin/sh
# Stage 5's music (0x0A the castle, 0x0B the boss; both posted with 0x01FA)
# and effects (OKI 0x9B troll hit, 0x9C troll death, 0x9D orb throw, 0xA8
# pillar landing; specs/stage5.md 9). Same steps as run.sh.
set -e
H="$(cd "$(dirname "$0")" && pwd)"; R="$H/../../.."; D="$H/../../data/audio"
CORE="$R/build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/gpgx-cps/genesis_plus_gx_libretro.dylib"
SND="$R/build/native-audio-admission/soundcpu.bin"
JAVA=/opt/homebrew/opt/openjdk@21/bin/java; XGM2="$R/build/sgdk-native/bin/xgm2tool.jar"
for m in 0a 0b; do
  "$H/opmlog" "$CORE" "$SND" 0x$m "$D/music$m.opmlog" --then 0xFA@8:1
  python3 "$H/opm2opn.py" "$D/music$m.opmlog" "$D/music$m.vgm" --lead 1 --report "$D/music$m.json" > /dev/null
  "$JAVA" -Djava.awt.headless=true -jar "$XGM2" "$D/music$m.vgm" "$D/music$m.xgc" > /dev/null
done
for c in 9b 9c 9d a8; do
  python3 -I "$H/okiwav.py" "$R/willow.zip" "$SND" 0x$c "$D/render/oki$c.wav" > /dev/null
done
