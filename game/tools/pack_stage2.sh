#!/bin/sh
# pack_stage2.sh S2DIR: stage 2's sprite families (specs/stage2.md 3.3, 5, 10)
# into data/s2spr/FAMILY, coloured from stage-2 arcade dumps (S2DIR = the
# stage-2 trace folder, specs/stage2.md 10), then the stage's sprite CRAM fit
# (tools/sprpal.py: line 0 the player's, line 1 fitted) and src/s2spr.{s,h}
# (symbols prefixed s2_). Families shared with stage 1 use the same roots
# (tools/pack_stage1.sh) in stage 2's colours.
cd "$(dirname "$0")/.." || exit 1
S2=$1
D=$S2/vid/v003000.cps
pk() { n=$1; b=$2; shift 2; mkdir -p data/s2spr/$n; rm -f data/s2spr/$n/*.src.bin; BOXES=$b MIRROR=1 PREFIX=S2_$(echo $n | tr a-z A-Z) python3 tools/posepack.py "$D" data/s2spr/$n "$@" | tail -1 | sed "s/^/$n: /"; }
E=3AC4A,3AE62; P=3B0D2,3B0D2
# Shared with stage 1.
pk soldier $E 1116E 11226 1133A 1136E 11416 11466 11482 114BA 114F2 1155E 11562 h1145A h1149E H114D6 113FA h11432 h1144E
pk archer $E 13154 13158 1315C 13160 13164 1314C 13634 1316C:16 1318C:16 19DFE 19E02
pk arrow $P 23856:16 23B06:16
pk chest $E h37CC6 h37CD6
pk gold $E 2D314:12
pk burst $E r323B0 r32414
pk spearman $E r1858C r18618 r186A8 r1871E r18798 r187A4 r187B4 r187DE r18810 r188AC r18954 r18A66 r18884 r18920 r189C8 r18ADA r189F2 r18B04
# The wagon chase (specs/stage2.md 6.1), coloured from a chase frame: the wagon (body, rear and front
# wheel, the driver), the horse pulling it.
D=$S2/vid/v007000.cps
pk wagon $E h302CC r302F6 r30302
pk driver $E r30368
pk horse $E r30900 r30400
# The fort's own families (specs/stage2.md 5). Chandelier (5.2): hanging, crash -> wreck, each in
# palettes 19 and 20 (the code flips them every 8 frames): chains 0/1 hang, 2/3 crash.
D=$S2/vid/v004100.cps
pk chandelier $E h12582 h12582@20 r1259E r1259E@20
# Its flames (class 10 kind 12, palette 6).
D=$S2/vid/v004150.cps
pk flame $P r241E4
# Crate archer (5.3): pop-up, aim, recoil, duck, fall, landing (right / left each).
D=$S2/vid/v004420.cps
pk cratearcher $E 19CDE H19D32 H19D72 19DAA 19DFE 19E02
# Window shutter (5.4, palette 24) and the window crossbowman (palette 17): rising, aim chains by
# direction 0-4, 5, 6, 7-8, 9, 10, 11, 12-15 (aim, shot, hold), death.
D=$S2/vid/v003780.cps
pk shutter $P r35DFE
pk crossbow $E r1A1E4 r1A234 r1A28C r1A2D6 r1A324 r1A372 r1A3C0 r1A40E r1A458 r1A4B0
# Window lurker (5.5): out (to the empty window), out again, the empty window; its debris.
D=$S2/vid/v005060.cps
pk lurker $E r1A728 r1A77A h1A7CC
pk debris $P r1A7E6
# The door spearman's emerging (5.6), the door effect (class 12 kind 0 variant 7), its axe.
D=$S2/vid/v006160.cps
pk emerge $E r18B78
pk doorfx $E r32478
pk axe $P r247D8 r24812
# The chase's families (5.9-5.13). Mounted archer (palette 17): ride, low shot, high shot, right-side
# shot, hit/fall, lying (one facing: the other is drawn mirrored).
D=$S2/vid/v007000.cps
pk rider $E r1B9BE r1BA00 r1BB14 r1BC00 r1BD20 h1BD64
# Boarding soldier (palette 16): ride, strike, jump, fall, landed, dismount, thrown; then its deck poses
# (long pointers 0x1C940.., right / left pairs): stand, step, step, hit, crouch, leap, walk, rise.
D=$S2/vid/v007800.cps
pk boarder $E r1C790 r1C7D8 r1C848 r1C890 h1C86A r1C8B6 h1C91A r11182 r111DA r1122A r112B2 r11236 r112BE r115B2 r1157E r1159A r11566 r11476 r1146A r11492 r11486 r115BE r1158A
# Hawk (palette 31): flight, flight 2, 16 dive directions, hit; its rock.
D=$S2/vid/v008900.cps
pk hawk $E r196B4 r1971E H19766:16 h1990A
pk rock $P r24B12
# The hound (the stage-1 beast's art, palette 3): run, leap, fall, run on the road.
pk hound $E 11FCC 11FF4 11FF8 11FFC
D=$S2/vid/v003000.cps
# Madmartigan's sword (specs/madmartigan.md 5.2: levels 0-1, 2-3, 4; image 0 R/L, image 1 R/L),
# in the colours its actor writes to object palette 7 at the start of a swing (ROM 0xEA98 row 0).
python3 - "$D" "$S2/sworddump.cps" <<'PY'
import sys
raw = bytearray(open(sys.argv[1], 'rb').read()); o = 0x80 + 0x1C000
for pen, w in zip((0, 9, 10, 11, 12, 13, 14), (0x0FFF, 0x04CC, 0x06DE, 0x07FF, 0x09FF, 0x0BFF, 0x0DFF)):
    raw[o + (7 * 16 + pen) * 2:o + (7 * 16 + pen) * 2 + 2] = (0xF000 | w).to_bytes(2, 'big')
open(sys.argv[2], 'wb').write(raw)
PY
D=$S2/sworddump.cps
pk sword $P hEC1C hEC50 hEC34 hEC68 hEBA8 hEBE2 hEBC2 hEBFC hEB28 hEB68 hEB46 hEB86
D=$S2/vid/v003000.cps
# The player's line comes first in the fit: Madmartigan (data/mad, packed by tools/posepack.py with
# NODE_FIRST=70: his 70 poses, specs/madmartigan.md 4).
rm -rf data/s2player; mkdir -p data/s2player
for f in poses tiles; do cp data/mad/$f.bin data/s2player/$f.bin; done
for f in boxes nodes pal layout.h; do cp data/mad/$f* data/s2player/; done
STATIC=15 SCENERY_LINES=2 python3 tools/sprpal.py data/stage2/pal.bin data/s2spr/sprpal.bin data/s2player \
  data/s2spr/soldier data/s2spr/archer data/s2spr/arrow data/s2spr/chest data/s2spr/gold data/s2spr/burst=1 data/s2spr/spearman data/s2spr/wagon data/s2spr/driver data/s2spr/horse data/s2spr/sword \
  data/s2spr/chandelier data/s2spr/flame=1 data/s2spr/cratearcher data/s2spr/shutter data/s2spr/crossbow \
  data/s2spr/lurker data/s2spr/debris data/s2spr/emerge data/s2spr/doorfx data/s2spr/axe \
  data/s2spr/rider data/s2spr/boarder data/s2spr/hawk data/s2spr/rock data/s2spr/hound | tail -2
python3 tools/satpack.py data/s2spr/*/ > /dev/null
# The HUD's glyphs use sprite line 1 too: recolour them for this fit (line choices kept from stage 1).
STATIC=15 LINES_FROM=data/hud/hud_tiles.h python3 tools/hudpack.py data/stage2/pal.bin data/hud2 data/s2spr/sprpal.bin | tail -1
python3 tools/gen_spr_data.py data/s2spr src/s2spr.s src/s2spr.h s2_
# Cherlindrea's story page (7.1), after the fit (its text uses sprite line 1).
python3 tools/storypack2.py "$S2/vidC/v002420.cps" data/intro | tail -1
