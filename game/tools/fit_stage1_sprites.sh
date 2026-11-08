#!/bin/sh
# fit_stage1_sprites.sh: stage 1 sprite CRAM lines 0-1 and every family's
# recoloured tiles (tools/sprpal.py), then the HUD against them.
cd "$(dirname "$0")/.." || exit 1
python3 tools/sprpal.py data/stage1/pal.bin data/s1spr/sprpal.bin data/willow data/shot=1 data/spark=1 \
  data/s1spr/soldier data/s1spr/beast data/s1spr/archer data/s1spr/arrow data/s1spr/sorcerer data/s1spr/fireball \
  data/s1spr/squiggle data/s1spr/log data/s1spr/platform data/s1spr/chest data/s1spr/hidden data/s1spr/gold data/s1spr/burst data/s1spr/splash data/s1spr/sparkle data/s1spr/bosshead=b data/s1spr/bosspaw=b data/s1spr/bossfire data/s1spr/windmill data/s1spr/spearman data/s1spr/axe data/s1spr/arenabeast data/s1spr/shopkeeper data/s1spr/companion data/s1spr/spear data/s1spr/spell=1 "$@"
# The sorcerer's ground fire, emitter and flames (added after the fit): recoloured for its line 1.
KEEP_LINE1=data/s1spr/sprpal.bin python3 tools/sprpal.py data/stage1/pal.bin /tmp/s1flame_pal.bin data/willow data/s1spr/groundfire data/s1spr/flame | tail -1
# High Aldwin (art pass): shown only at the stage start, where the scenery
# lines' pens 10-15 are ordinary colours (stagepack CYCLE_FROM): all 15 pens
# (his white and grey robe; line 1 alone made it green-grey); ALLOW_LINE0: Willow's line has his skin tones (06a).
ALLOW_LINE0=1 KEEP_LINE1=data/s1spr/sprpal.bin STATIC=15 SPR_PERCEPT=1 SPR_CHROMA=1 python3 tools/sprpal.py data/stage1/pal.bin /tmp/s1aldwin_pal.bin data/willow data/s1spr/aldwin | tail -1
STATIC=9 LINES_FROM=data/hud/hud_tiles.h python3 tools/hudpack.py data/stage1/pal.bin data/hud data/s1spr/sprpal.bin | tail -1
python3 tools/satpack.py data/s1spr/*/ data/shot data/spark > /dev/null
