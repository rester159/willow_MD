#!/bin/sh
# pack_stage3.sh S3DIR: stage 3's sprite families (specs/stage3.md 3.3, 4-6, 10)
# into data/s3spr/FAMILY, coloured from stage-3 arcade dumps (S3DIR = the
# stage-3 trace folder), then the stage's sprite CRAM fit (tools/sprpal.py:
# line 0 Willow's, line 1 fitted) and src/s3spr.{s,h} (symbols s3_). Willow's
# shots, sparks and spells are recoloured copies (data/s3spr/shot, spark,
# spell) since line 1 differs from stage 1's.
cd "$(dirname "$0")/.." || exit 1
S3=$1
pk() { n=$1; b=$2; shift 2; mkdir -p data/s3spr/$n; rm -f data/s3spr/$n/*.src.bin; BOXES=$b MIRROR=1 PREFIX=S3_$(echo $n | tr a-z A-Z) python3 tools/posepack.py "$D" data/s3spr/$n "$@" | tail -1 | sed "s/^/$n: /"; }
E=3AC4A,3AE62; P=3B0D2,3B0D2
D=$S3/vid/v001500.cps
# Shared with stages 1-2 (stage 1's roots, stage 3's colours).
pk chest $E h37CC6 h37CD6
pk gold $E 2D314:12
pk burst $E r323B0 r32414
pk splash $E r3229C
# The hidden object and the companion it uncovers (stage3.md 8.3; stage 1's roots).
pk hidden $E r3784E
pk companion $E r3784E r37868 r3766E r376A2 r376D6 r376F2 r3770E r3777E h377EE h3780E h377FE h3781E h3782E h3783E
pk spearman $E r1858C r18618 r186A8 r1871E r18798 r187A4 r187B4 r187DE r18810 r188AC r18954 r18A66 r18884 r18920 r189C8 r18ADA r189F2 r18B04
pk axe $P r247D8 r24812
# Bomb trooper (5.1, override 5): walk, ledge throw, stand, ledge drop, crouch, jump, ground throw, run.
pk trooper $E 1D9EA@5 1DADE@5 1DB9A@5 1DBBE@5 1DC0E@5 1DC4E@5 1DC72@5 1DD4A@5
# Bomb (5.2): flight (palette 5), burst (13), shards (24).
pk bomb $P 2503E@5 25112@13 252DE@24
D=$S3/vid/v004570.cps
# Hawk variant 1 (4.2, override 22): carrying, empty claws, hit; its rock.
pk hawk $E 195F6@22 195FA@22 H195FE@22
pk rock $P r24B12
D=$S3/vid/v005600.cps
# Sinking pillars (5.4, palette 28).
pk pillar $E h3184E h3188C
D=$S3/vid/v006130.cps
# Lance knight (5.3, override 15): walk, standing thrust, crouched thrust, turn; lance tip / shaft.
pk knight $E 1AE10@15 1AF3C@15 1B030@15 1B104@15
pk lance $P 249A4:2@15
D=$S3/vid/v010400.cps
# The lake (5.5-5.7): Willow's boat (rest, tilt, tilt), the enemy raft (palette 24); raft
# archers (palette 17: the archer's chains, the raft pose table, the crossbowman's death).
pk boat $E h30EB8 h30EA0 h30EAC
pk raft $E h316F6
pk archer $E 13154 13158 1315C 13160 13164 1314C 13634 1316C:16 1318C:16 19DFE 19E02 H131AC r1A4B0
pk arrow $P 23856:16 23B06:16
D=$S3/vid/v013250.cps
# Leaping fish (5.9, palette 25): swim, leap, re-entry, surfaced, surfacing.
pk fish $E 1D07A H1D142:5 H1D188:5 1D1CE 1D232 1D0F2
# The boss (6, palette 26): glide, summon, cast (8 directions each); its bolt and the cast /
# summon glows (palette 27; the summon glow's override 8); the spirit serpents (palette 20):
# head and tail by 16 directions, body.
D=$S3/vid/v015300.cps
pk boss3 $E 2810C:8@26 281EE:8@26 2832A:8@26
D=$S3/vid/v015420.cps
pk bolt3 $P r255D0@27
pk glow $P 36A74:8@27 36A84:8@27 r36C2A@8 r36C4E@8
# The ending figure (6.13: Fin Raziel as a possum, palettes 29-31 in the nodes): run, held,
# the transformation burst, Fin Raziel.
D=$S3/vid/v016250.cps
pk figure $E r3795C r37984 r37994 r379F8
D=$S3/vid/v015790.cps
pk serpent $E H174EE:16@20 H175CE:16@20 176AE:1@20
# Willow's shots, sparks and spells: stage 1's art, recoloured for this fit.
for f in shot spark; do rm -rf data/s3spr/$f; cp -R data/$f data/s3spr/$f; done
rm -rf data/s3spr/spell; cp -R data/s1spr/spell data/s3spr/spell
STATIC=9 python3 tools/sprpal.py data/stage3/pal.bin data/s3spr/sprpal.bin data/willow data/s3spr/shot=1 data/s3spr/spark=1 \
  data/s3spr/chest data/s3spr/gold data/s3spr/burst data/s3spr/splash data/s3spr/hidden data/s3spr/companion data/s3spr/spearman data/s3spr/axe data/s3spr/trooper data/s3spr/bomb \
  data/s3spr/hawk data/s3spr/rock data/s3spr/pillar data/s3spr/knight data/s3spr/lance data/s3spr/boat data/s3spr/raft \
  data/s3spr/archer data/s3spr/arrow data/s3spr/fish data/s3spr/boss3 data/s3spr/figure data/s3spr/bolt3 data/s3spr/glow data/s3spr/serpent data/s3spr/spell | tail -2
python3 tools/satpack.py data/s3spr/*/ > /dev/null
python3 tools/gen_spr_data.py data/s3spr src/s3spr.s src/s3spr.h s3_
STATIC=9 LINES_FROM=data/hud/hud_tiles.h python3 tools/hudpack.py data/stage3/pal.bin data/hud3 data/s3spr/sprpal.bin | tail -1
STORY=3 SPRPAL=data/s3spr/sprpal.bin python3 tools/storypack2.py "$S3/vidC/v002438.cps" data/intro | tail -1
python3 tools/stormpack.py data/stage3 data/stage3/storm.bin
