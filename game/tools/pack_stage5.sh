#!/bin/sh
# pack_stage5.sh S5DIR: stage 5's sprite families (specs/stage5.md 3.3, 4-6, 10)
# into data/s5spr/FAMILY, coloured from stage-5 arcade dumps (S5DIR = the
# stage-5 trace folder; no palette changes during play, 2.6), then the
# stage's sprite CRAM fit (tools/sprpal.py: line 0 Willow's, line 1 fitted;
# the player picks Willow or Madmartigan on the select screen, 1.3, and line
# 0 is then the chosen form's: data/willow/pal.bin or data/mad/pal.bin), the
# HUD recolour (data/hud5) and src/s5spr.{s,h} (symbols s5_). Both forms'
# weapons are here: Willow's shots, sparks and spells and Madmartigan's sword
# (recoloured copies).
cd "$(dirname "$0")/.." || exit 1
S5=$1
pk() { n=$1; b=$2; shift 2; mkdir -p data/s5spr/$n; rm -f data/s5spr/$n/*.src.bin; BOXES=$b MIRROR=1 PREFIX=S5_$(echo $n | tr a-z A-Z) python3 tools/posepack.py "$D" data/s5spr/$n "$@" | tail -1 | sed "s/^/$n: /"; }
E=3AC4A,3AE62; P=3B0D2,3B0D2; B=3B07A,3B07A; S=3B1AA,3B1AA
D=$S5/vid/v001200.cps
# Shared with stages 1-4 (their roots, stage 5's colours).
pk soldier $E 1116E 11226 1133A 1136E 11416 11466 11482 114BA 114F2 1155E 11562 h1145A h1149E H114D6 113FA h11432 h1144E
pk archer $E 13154 13158 1315C 13160 13164 1314C 13634 1316C:16 1318C:16 19DFE 19E02
pk arrow $P 23856:16 23B06:16
pk spearman $E r1858C r18618 r186A8 r1871E r18798 r187A4 r187B4 r187DE r18810 r188AC r18954 r18A66 r18884 r18920 r189C8 r18ADA r189F2 r18B04
pk axe $P r247D8 r24812
pk chest $E h37CC6 h37CD6
pk gold $E 2D314:12
pk burst $E r323B0 r32414
pk bat $E r225E4@22 r22630@22
# Trolls (5.1, palette 24): death fall, walk, crouch, land, leap up, leap, grapple, held,
# throw, grapple throw, hurt; their orbs (5.5): flight, impact.
pk troll $E 20D84 20D88 20E64 20E68 20F1C 20F60 2102C:1 2115A:1 21270 21338 r21468 r214AC
pk orb $P r25B6E r25C6A
# Corridor doorways (5.3, palette 18): held open, the loop.
pk doorway $E 21700 21704
# Stone pillars (5.4, palette 19): var 0 intact, flash, cracked; var 2, flash, cracked.
pk pillar $S r31BF4 r31C10 r31C44 r31C60 r31C8A r31CDA
# Spiked carts (5.3, palette 19): intact by facing, cracked by facing (4-node wheel loops).
pk cart $S r21D04 r21DA0 r21E3C r21ED8
# Eborsisk (6, specs/stage5.md; S5/boss2/roots_boss.txt): the heads by facing x mouth (28;
# head B's live palette 27 is the same art), the fire flames (8) by flame group, the pillar
# debris (11). The two necks are plane-B pictures (tools/bosspack5.py below).
pk head $B h2CC46@28 h2CC92@28 h2CCE2@28 h2CD30@28 h2CD82@28 h2CDD2@28 h2CE20@28 h2CE70@28 h2CEC0@28
pk flame $P r26C46@8 r26C9E@8 r26CF6@8 r26D4E@8
pk debris $E r32526@11
# Select screen (1.3): the cursor frame (palette 8).
pk cursor $P r36206@8
# Willow's shots, sparks and spells (stage 1's art) and Madmartigan's sword, recoloured.
for f in shot spark; do rm -rf data/s5spr/$f; cp -R data/$f data/s5spr/$f; done
rm -rf data/s5spr/spell; cp -R data/s1spr/spell data/s5spr/spell
rm -rf data/s5spr/sword; cp -R data/s2spr/sword data/s5spr/sword
# What shows during Eborsisk's fight stays on line 1 (=1): the boss lends scenery lines
# 2-3 to the re-tiled arena and the necks (tools/bosspack5.py).
STATIC=15 python3 tools/sprpal.py data/stage5/pal.bin data/s5spr/sprpal.bin data/willow data/s5spr/shot=1 data/s5spr/spark=1 \
  data/s5spr/soldier data/s5spr/archer data/s5spr/arrow data/s5spr/spearman data/s5spr/axe data/s5spr/chest data/s5spr/gold=1 \
  data/s5spr/burst=1 data/s5spr/bat data/s5spr/troll data/s5spr/orb data/s5spr/doorway data/s5spr/pillar data/s5spr/cursor \
  data/s5spr/cart data/s5spr/head=1 data/s5spr/flame=1 data/s5spr/debris=1 \
  data/s5spr/spell=1 data/s5spr/sword=1 | tail -2
python3 tools/satpack.py data/s5spr/*/ > /dev/null
python3 tools/gen_spr_data.py data/s5spr src/s5spr.s src/s5spr.h s5_
STATIC=15 LINES_FROM=data/hud/hud_tiles.h python3 tools/hudpack.py data/stage5/pal.bin data/hud5 data/s5spr/sprpal.bin | tail -1
# The necks (6.4): the sprite neck's poses as pixels, both necks composed into 16 plane-B
# pictures, the arena re-tiled (tools/bosspack5.py; calibration: tools/neckpack5.py).
rm -rf data/s5spr/neck; PIXOUT=data/boss5/sprneck.pkl BOXES=$B python3 tools/posepack.py "$S5/boss2/vidW/v017790.cps" /tmp/neck5 h2C9BC@26 h2CA5A@26 h2CAF2@26 h2CB8C@26 > /dev/null 2>&1
python3 tools/bosspack5.py "$S5/boss2/vidW/v017790.cps" data/boss5/sprneck.pkl data/boss5 | tail -1
STORY=5 SPRPAL=data/s5spr/sprpal.bin python3 tools/storypack2.py "$S5/vidC/v003968.cps" data/intro | tail -1
