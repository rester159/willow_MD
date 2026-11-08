#!/bin/sh
# pack_stage4.sh S4DIR: stage 4's sprite families (specs/stage4.md 3.3, 4-6, 10)
# into data/s4spr/FAMILY, coloured from stage-4 arcade dumps (S4DIR = the
# stage-4 trace folder), then the stage's sprite CRAM fit (tools/sprpal.py:
# line 0 Madmartigan's, line 1 fitted), the HUD recolour (data/hud4) and
# src/s4spr.{s,h} (symbols s4_). Madmartigan's sword is a recoloured copy of
# stage 2's (data/s4spr/sword).
cd "$(dirname "$0")/.." || exit 1
S4=$1
pk() { n=$1; b=$2; shift 2; mkdir -p data/s4spr/$n; rm -f data/s4spr/$n/*.src.bin; BOXES=$b MIRROR=1 PREFIX=S4_$(echo $n | tr a-z A-Z) python3 tools/posepack.py "$D" data/s4spr/$n "$@" | tail -1 | sed "s/^/$n: /"; }
E=3AC4A,3AE62; P=3B0D2,3B0D2; B=3B07A,3B07A; S=3B1AA,3B1AA
# Shared with stages 1-3 (their roots, stage 4's colours).
D=$S4/vid/v002000.cps
pk soldier $E 1116E 11226 1133A 1136E 11416 11466 11482 114BA 114F2 1155E 11562 h1145A h1149E H114D6 113FA h11432 h1144E
# Falling snow (2.7, palette 27): four one-piece poses per depth.
pk flake $P H356A4:12@27
D=$S4/vid/v013600.cps
pk archer $E 13154 13158 1315C 13160 13164 1314C 13634 1316C:16 1318C:16 19DFE 19E02
pk arrow $P 23856:16 23B06:16
pk spearman $E r1858C r18618 r186A8 r1871E r18798 r187A4 r187B4 r187DE r18810 r188AC r18954 r18A66 r18884 r18920 r189C8 r18ADA r189F2 r18B04
pk axe $P r247D8 r24812
pk chest $E h37CC6 h37CD6
pk gold $E 2D314:12
pk burst $E r323B0 r32414
pk hidden $E r3784E
pk companion $E r3784E r37868 r3766E r376A2 r376D6 r376F2 r3770E r3777E h377EE h3780E h377FE h3781E h3782E h3783E
# The bomb trooper's art (5.3: kind-23 tables, override 5), its bombs.
D=$S4/vid/v002800.cps
pk trooper $E 1D9EA@5 1DADE@5 1DB9A@5 1DBBE@5 1DC0E@5 1DC4E@5 1DC72@5 1DD4A@5
pk bomb $P 2503E@5 25112@13 252DE@24
# Rolling snowballs (5.1, palette 27): sizes by variant, rolling, the burst.
D=$S4/vid/v004980.cps
pk snowball $E H22170:4@27 r22178@27 r2222C@27
# Bats (5.2, palette 22): hanging, flying.
D=$S4/vid/v010500.cps
pk bat $E r225E4@22 r22630@22
# Swooping hawk (5.4, palette 31).
D=$S4/vid/v005500.cps
pk hawk $E h196B4@31 h1971E@31 19766@31 h1990A@31 h19922@31 h1993E@31 h1994A@31
# Tent spear thrower (5.5, palette 30): stand, lunge, throw high, throw low, emerge; its spear.
D=$S4/vid/v016000.cps
pk tent $E 1F7AE@30 1F7B6@30 1F7BA@30 1F7BE@30 1F7C6@30
pk tspear $P 25898@30
# The dragon (6.1-6.5, palette 26): body, neck by step, head by step (normal / breathing); its fire (13).
D=$S4/vid/v012000.cps
pk dragon $B r2B4E0@26 H2B60E:7@26 H2B660:7@26 H2B66E:7@26
D=$S4/vid/v011920.cps
pk fire $P r269CC@13 r269F4@13
# Sorsha (6.8, palette 28): intro, stand, walk, crouch, jump, slash wind-up, slash, air
# wind-up, air slash, thrust wind-up, thrust, high thrust, backstep x2, defeated; slash box (6).
D=$S4/vid/v018300.cps
pk sorsha $B 2925A@28 2925E@28 29262@28 29266@28 2926A@28 2926E@28 29272@28 29276@28 2927A@28 2927E@28 29282@28 29286@28 2928A@28 2928E@28 29292@28
D=$S4/vid/v018680.cps
pk clash $P rF2EC
# The ride (6.13): the shield sled with both riders (start, mount, slopes 0-3).
D=$S4/vid/v020200.cps
pk shield $S r3129E r312BE 312DE:4
# The crash and the end scene (6.14): the snowball (27), the village figures.
D=$S4/vid/v021000.cps
pk crash $E r35A2A r35A5E r35B70
D=$S4/vid/v021480.cps
pk ending $E r35B90 r35BA0 r35C82 r35CB8
# The intro scene (1.3): the horse (16), Willow (0), the soldier (28).
D=$S4/vidA/v003500.cps
pk introhorse $E r1C790 r30390 r30400
# Madmartigan (line 0, exact) and his sword (recoloured for this fit).
D=$S4/vid/v013600.cps
rm -rf data/s4player; mkdir -p data/s4player
for f in poses tiles; do cp data/mad/$f.bin data/s4player/$f.bin; done
for f in boxes nodes pal layout.h; do cp data/mad/$f* data/s4player/; done
rm -rf data/s4spr/sword; cp -R data/s2spr/sword data/s4spr/sword
STATIC=15 python3 tools/sprpal.py data/stage4/pal.bin data/s4spr/sprpal.bin data/s4player \
  data/s4spr/soldier data/s4spr/flake data/s4spr/archer data/s4spr/arrow data/s4spr/spearman data/s4spr/axe data/s4spr/chest \
  data/s4spr/gold data/s4spr/burst data/s4spr/hidden data/s4spr/companion data/s4spr/trooper data/s4spr/bomb data/s4spr/snowball \
  data/s4spr/bat data/s4spr/hawk data/s4spr/tent data/s4spr/tspear data/s4spr/dragon data/s4spr/fire data/s4spr/sorsha \
  data/s4spr/clash data/s4spr/shield data/s4spr/crash data/s4spr/ending data/s4spr/introhorse data/s4spr/sword | tail -2
python3 tools/satpack.py data/s4spr/*/ > /dev/null
python3 tools/gen_spr_data.py data/s4spr src/s4spr.s src/s4spr.h s4_
STATIC=15 LINES_FROM=data/hud/hud_tiles.h python3 tools/hudpack.py data/stage4/pal.bin data/hud4 data/s4spr/sprpal.bin | tail -1
STORY=4 SPRPAL=data/s4spr/sprpal.bin python3 tools/storypack2.py "$S4/vidC/v002924.cps" data/intro | tail -1
