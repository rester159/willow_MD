#!/bin/sh
# pack_stage6.sh S6DIR: stage 6's sprite families (specs/stage6.md 3.3, 4-6, 10)
# into data/s6spr/FAMILY, coloured from a stage-6 arcade dump (S6DIR = the
# stage-6 trace folder; the object palettes are loaded once at the stage
# set-up, 2.6), then the stage's sprite CRAM fit (tools/sprpal.py: line 0
# Madmartigan's, areas 0-4; Willow's line from the area-4 exit, src/game.c),
# the HUD recolour (data/hud6) and src/s6spr.{s,h} (symbols s6_, section .s6:
# the SSF2 banks above 4 MB, src/bank.c). Roots: S6DIR/roots.txt.
cd "$(dirname "$0")/.." || exit 1
S6=$1
pk() { n=$1; b=$2; shift 2; mkdir -p data/s6spr/$n; rm -f data/s6spr/$n/*.src.bin; BOXES=$b MIRROR=1 PREFIX=S6_$(echo $n | tr a-z A-Z) python3 tools/posepack.py "$D" data/s6spr/$n "$@" | tail -1 | sed "s/^/$n: /"; }
E=3AC4A,3AE62; P=3B0D2,3B0D2; B=3B07A,3B07A; S=3B1AA,3B1AA
D=$S6/vid/v002500.cps
# Shared with stages 1-5 (their roots, stage 6's colours; 4.1-4.6).
pk soldier $E 1116E 11226 1133A 1136E 11416 11466 11482 114BA 114F2 1155E 11562 h1145A h1149E H114D6 113FA h11432 h1144E
pk archer $E 13154 13158 1315C 13160 13164 1314C 13634 1316C:16 1318C:16 19DFE 19E02
pk arrow $P 23856:16 23B06:16
pk spearman $E r1858C r18618 r186A8 r1871E r18798 r187A4 r187B4 r187DE r18810 r188AC r18954 r18A66 r18884 r18920 r189C8 r18ADA r189F2 r18B04
pk axe $P r247D8 r24812
pk chest $E h37CC6 h37CD6
pk gold $E 2D314:12
pk burst $E r323B0 r32414
# Window shutter and crossbowman (4.2, stage 2's roots).
pk shutter $P r35DFE
pk crossbow $E r1A1E4 r1A234 r1A28C r1A2D6 r1A324 r1A372 r1A3C0 r1A40E r1A458 r1A4B0
# Mace knight (4.3, stage 3's lance-knight roots, palette 15) and the flail's segments (class 10 kind 8).
pk knight $E 1AE10@15 1AF3C@15 1B030@15 1B104@15
pk flail $P r249E0@15 r249B8@15
# Spiked carts (5.4): the spawner's wall (palette 18) and the cart (palette 19).
pk doorway $E 21700 21704
pk cart $S r21D04 r21DA0 r21E3C r21ED8
# Flipping stones (5.6, palette 28): flat, edge-on steps; spike crushers (5.7); chandeliers (5.8);
# the mortar (5.2, palette 5) and its part (kind 37); the tower bomb trooper (5.3, palette 5).
pk stone $S h31FC2 h31FCE h31FDA h31FE6
pk crusher $S h32222
# The archer riding a dir-2 crusher (stage6_families.md 11, palette 17): idle, release, draw,
# hurt, death flight, landed.
pk rarch $E 131AC 131B0 13164 13634 19DFE 19E02
pk chand $P h363EE h3640C h3641E
pk mortar $E 22D88@5 22D8C@5 h36472@26
# The bombs of the mortars and the tower troopers (stage 3's: falling, shards, burst).
pk bomb $P 2503E@5 25112@13 252DE@24
pk trooper $E 1DB9A@5 1DC72@5 1DD4A@5 233A6@5 233AA@5
# Doors (2.5, palette 27).
pk door $P h2DB16
# General Kael (6.1-6.3, palette 24): his 13 tables; his sword boxes (class 10 kind 13, palette 25).
pk kael $E 1ECF6 1ECFA 1ECFE 1ED02 1ED06 1ED0A 1ED0E 1ED12 1ED16 1ED1A 1ED1E 1ED22 1ED26
pk kslash $P h256A4@25 h256B0@25 h256C6@25 h256DC@25 h256F2@25 h256FE@25 h25714@25 h2572A@25
# Queen Bavmorda (stage6_bavmorda.md, palette 29): chains 0-1 walk, 2-3 cast, 4-5 take-off, 6-14
# flight (9 poses), 15 the whirlwind; her spells (6.5): sparks, the
# energy ball and its burst, bolt segments, candelabra flames, the glows; the altar brazier (5.5).
D=$S6/vidD7/v004446.cps   # her fight: the spells' palettes as they are then
pk bav $B 2A6DA 2A6DE 2A88E r2ABBC r2AB70 r2AB20 r2AACC r2A978 r2A994 r2A9E8 r2AA38 r2AA84 r2AC04
pk spell $P r261F0 r26224 r2624C@13 r26330 r26656@6 r266BA@6 26756@6 r36570 r365E8 r36C2A@27 r261CA r3659A@8 r265A0@8 r36C4E@27 rF2EC@8
pk altar $E h21A70 h21A80 r21A90
# The ending's smoke clouds (7.3, palette 23).
D=$S6/vid/v002500.cps
pk smoke $P r3685E
# Willow's shots, sparks and spells (stage 1's art) and Madmartigan's sword, recoloured.
for f in shot spark; do rm -rf data/s6spr/$f; cp -R data/$f data/s6spr/$f; done
rm -rf data/s6spr/spell1; cp -R data/s1spr/spell data/s6spr/spell1
rm -rf data/s6spr/sword; cp -R data/s2spr/sword data/s6spr/sword
EXTRA="0,0,7,1200;0,4,6,800;0,7,7,600" SCENERY_LINES= STATIC=15 python3 tools/sprpal.py data/stage6/a0/pal.bin data/s6spr/sprpal.bin data/mad data/s6spr/shot=1 data/s6spr/spark=1 \
  data/s6spr/soldier data/s6spr/archer data/s6spr/arrow data/s6spr/spearman data/s6spr/axe data/s6spr/chest data/s6spr/gold \
  data/s6spr/burst data/s6spr/shutter data/s6spr/crossbow data/s6spr/knight data/s6spr/flail data/s6spr/doorway data/s6spr/cart \
  data/s6spr/stone data/s6spr/crusher data/s6spr/rarch data/s6spr/chand data/s6spr/mortar data/s6spr/bomb data/s6spr/trooper data/s6spr/door \
  data/s6spr/kael data/s6spr/kslash data/s6spr/bav data/s6spr/spell data/s6spr/altar data/s6spr/smoke \
  data/s6spr/spell1=1 data/s6spr/sword=1 | tail -2
python3 tools/satpack.py data/s6spr/*/ > /dev/null
python3 tools/gen_spr_data.py data/s6spr src/s6spr.s src/s6spr.h s6_
STATIC=15 LINES_FROM=data/hud/hud_tiles.h python3 tools/hudpack.py data/stage6/a0/pal.bin data/hud6 data/s6spr/sprpal.bin | tail -1
