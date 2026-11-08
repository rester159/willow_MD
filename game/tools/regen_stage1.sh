#!/bin/sh
# regen_stage1.sh SCRATCH BD_FIT: rebuild stage 1's packed data in dependency
# order (stage planes -> sprite palette fit and HUD -> boss body -> intro).
# SCRATCH holds the arcade dumps the packers read (see M1_NOTES.md).
cd "$(dirname "$0")/.." || exit 1
S=$1; FIT=${2:-12}
B=../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1
# The far layer (arcade scroll3, scrolled on plane B: parallax, the user's
# choice 2026-11-02): 340 merged tiles (24 of the old 364 went to the HUD's item icons); the foreground merged at 320 to make room.
python3 tools/farlayer.py $S/wf/ref/v002800.cps data/stage1_far.ppm
BD_FIT=$FIT CYCLE=data/stage1/cycle.json FG_MERGE=320 BD_TILES=${BD_TILES:-340} python3 -u tools/stagepack.py data/stage1_fg.cells 2 \
  $S/wf/ref/v002800.cps $S/m1/wcam.txt data/stage1_far.ppm data/stage1 | tail -1
sh tools/pack_stage1.sh $S/wf/ref/v002800.cps $S/agents/combat/vidB/v010600.cps $S/agents/intro/nat/v001302.cps $S/agents/combat/vidB/v009920.cps $S/agents/shop/vidC/v005665.cps | tail -3
sh tools/fit_stage1_sprites.sh | tail -1
python3 tools/gen_spr_data.py data/s1spr src/s1spr.s src/s1spr.h
python3 tools/bosspack.py $S/agents/combat/vidB/v010600.cps data/stage1 data/s1spr/sprpal.bin data/boss1 | tail -1
MAP2=$S/agents/stage2/vidA/v003100.cps,$S/agents/stage2/vidA/v003106.cps python3 tools/intropack.py $S/agents/intro/nat/v000804.cps $S/agents/intro/nat/v000816.cps $S/agents/intro/nat/v001002.cps \
  $S/agents/intro/nat/v001446.cps $S/agents/combat/vidE/v002890.cps data/intro | grep -v distinct
python3 tools/shoppack.py $S/agents/shop/vidA/v006130.cps data/shop | tail -1
