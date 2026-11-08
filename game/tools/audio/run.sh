#!/bin/sh
# Rebuild the offline audio tools and regenerate game/data/audio (headless).
set -e
H="$(cd "$(dirname "$0")" && pwd)"; R="$H/../../.."; D="$H/../../data/audio"
CORE="$R/build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/gpgx-cps/genesis_plus_gx_libretro.dylib"
SND="$R/build/native-audio-admission/soundcpu.bin"
JAVA=/opt/homebrew/opt/openjdk@21/bin/java; XGM2="$R/build/sgdk-native/bin/xgm2tool.jar"
mkdir -p "$D/render"
clang -Wall -Wextra -Werror -O2 -o "$H/opmlog" "$H/opmlog.c"
clang -O2 -w -I"$H/gpgx" -o "$H/vgmrender" "$H/vgmrender.c" "$H/gpgx/ym2612.c" "$H/gpgx/psg.c" "$H/gpgx/blip_buf.c"
clang++ -std=c++14 -O2 -w -I"$H/ymfm" -o "$H/opmrender" "$H/opmrender.cpp" "$H/ymfm/ymfm_opm.cpp"
"$H/opmlog" "$CORE" "$SND" 0x13 "$D/music13.opmlog"
python3 "$H/checkgolden.py" "$R/build/native-audio-admission/sequencer/stage0-fixtures.bin" "$D/music13.opmlog"
python3 "$H/opm2opn.py" "$D/music13.opmlog" "$D/music13.vgm" --report "$D/music13.json" > /dev/null
for e in 20 2a 2b 31 43 44; do
  "$H/opmlog" "$CORE" "$SND" 0x$e "$D/sfx$e.opmlog" --max 500
  python3 "$H/opm2opn.py" "$D/sfx$e.opmlog" "$D/sfx$e.vgm" --plan auto --report "$D/sfx$e.json" > /dev/null
  "$H/vgmrender" "$D/sfx$e.vgm" "$D/render/sfx${e}_opn.wav"
  "$H/opmrender" "$D/sfx$e.opmlog" "$D/render/sfx${e}_opm.wav"
done
# Remaining stage-1 cues (specs/audio_stage1.md).
# Music: 0x01 boss (posted as 0x0001 then 0x01FA: fade-in command FA, interval
# 1, two 60 Hz main-CPU IRQs = 8 service ticks later). Its sound RAM never
# repeats (track 5 has its own 5,336-tick period); the song loop is ticks
# 1030..10309 (9,280 ticks), found from the per-track states.
"$H/opmlog" "$CORE" "$SND" 0x01 "$D/music01.opmlog" --then 0xFA@8:1 --max 10309
# --lead 1: chip set-up one frame before the first notes; otherwise the first
# 60 Hz frame is over XGM2's frame size (xgm2tool warns, or fails for 0x15).
python3 "$H/opm2opn.py" "$D/music01.opmlog" "$D/music01.vgm" --loop 1030 --lead 1 --report "$D/music01.json" > /dev/null
# Jingles 0x10 stage clear, 0x12 map, 0x14 time/game over, 0x15 continue: they
# end (only the tick counter changes afterwards), converted as one-shots.
for m in 10 12 14 15; do
  "$H/opmlog" "$CORE" "$SND" 0x$m "$D/music$m.opmlog"
  python3 "$H/opm2opn.py" "$D/music$m.opmlog" "$D/music$m.vgm" --tail 0.3 --lead 1 --report "$D/music$m.json" > /dev/null
done
# 0x1B shop: posted with 0x02FA (fade-in interval 2); its sound RAM repeats
# from tick 282 (found by opmlog).
"$H/opmlog" "$CORE" "$SND" 0x1B "$D/music1b.opmlog" --then 0xFA@8:2
python3 "$H/opm2opn.py" "$D/music1b.opmlog" "$D/music1b.vgm" --lead 1 --report "$D/music1b.json" > /dev/null
# 0x02 stage 2, 0x03 the wagon chase (posted with 0x01FA), loops found by opmlog.
for m in 02 03; do
  "$H/opmlog" "$CORE" "$SND" 0x$m "$D/music$m.opmlog" --then 0xFA@8:1
  python3 "$H/opm2opn.py" "$D/music$m.opmlog" "$D/music$m.vgm" --lead 1 --report "$D/music$m.json" > /dev/null
done
for m in 01 10 12 14 15 1b 02 03; do
  "$H/vgmrender" "$D/music$m.vgm" "$D/render/music${m}_opn.wav" --seconds 45
  "$JAVA" -Djava.awt.headless=true -jar "$XGM2" "$D/music$m.vgm" "$D/music$m.xgc" > /dev/null
done
# FM effects: PCM rendered from the original YM2151 writes (ymfm) with the
# full release (they end with key-offs), the inaudible tail cut.
for e in 21 22 23 24 25 26 2d 2f 30 32 33 34 37 38 3a 42; do
  "$H/opmlog" "$CORE" "$SND" 0x$e "$D/sfx$e.opmlog" --max 500
  "$H/opmrender" "$D/sfx$e.opmlog" "$D/render/sfx${e}_opm.wav" --tail 3 > /dev/null
  python3 "$H/wavtrim.py" "$D/render/sfx${e}_opm.wav" "$D/render/sfx${e}_opm.wav"
done
# OKI voices: the original ADPCM samples, 13,300 Hz.
for c in 82 83 84 85 8a 8b 8c 8d 8e 90 a0 a1; do
  python3 -I "$H/okiwav.py" "$R/willow.zip" "$SND" 0x$c "$D/render/oki$c.wav" > /dev/null
done
"$H/vgmrender" "$D/music13.vgm" "$D/render/music13_opn.wav" --seconds 90
"$H/opmrender" "$D/music13.opmlog" "$D/render/music13_opm.wav" --tail 0
"$JAVA" -Djava.awt.headless=true -jar "$XGM2" "$D/music13.vgm" "$D/music13.xgc"
