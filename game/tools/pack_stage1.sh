#!/bin/sh
# pack_stage1.sh PALDUMP.cps [BOSSDUMP.cps [INTRODUMP.cps [ARENADUMP.cps [COMPANIONDUMP.cps]]]]: every stage 1 sprite family's poses
# (the boss families take their colours from BOSSDUMP, a dump of the fight;
# the intro's windmill and High Aldwin from INTRODUMP, the forest intro)
# (tools/posepack.py, MIRROR=1) into data/s1spr/FAMILY. Root tables from
# specs/stage1_enemies.md section 8 (ADDR:N = N-entry table, rADDR = chain,
# hADDR = pose held by code).
cd "$(dirname "$0")/.." || exit 1
D=$1
BD=${2:-$1}
ID=${3:-$1}
AD=${4:-$BD}
CD=${5:-$1}
# Boxes (combat.md 1.2): enemies body-vs-player 3AC4A and vs shots 3AE62;
# enemy projectiles 3B0D2; surfaces vs shots 3B1AA.
# pk FAMILY BOXBASES ROOT...
pk() { n=$1; b=$2; shift 2; mkdir -p data/s1spr/$n; rm -f data/s1spr/$n/*.src.bin; BOXES=$b MIRROR=1 PREFIX=$(echo $n | tr a-z A-Z) python3 tools/posepack.py "$D" data/s1spr/$n "$@" | tail -1 | sed "s/^/$n: /"; }
E=3AC4A,3AE62; P=3B0D2,3B0D2; S=3B1AA,3B1AA
pk soldier $E 1116E 11226 1133A 1136E 11416 11466 11482 114BA 114F2 1155E 11562 h1145A h1149E H114D6 113FA h11432 h1144E
pk beast $E 11FC8 11FCC 11FD0 11FD4 11FD8 11FDC 11FE0 11FE4 11FE8 11FEC 11FF0 11FF4 11FF8 11FFC
pk archer $E 13154 13158 1315C 13160 13164 1314C 13634 1316C:16 1318C:16 19DFE 19E02
pk arrow $P 23856:16 23B06:16
pk sorcerer $E r14FE8 r15004 15026:4 1502E:4 151AE:16 h150FE r1528A
pk fireball $P 24046:4
pk squiggle $P 23F6E:4
# The sorcerer's ground fire: emitter (class 10 kind 3, 0x23E76) and flames (kind 4, 0x24152; the chandelier flame's routine and art)
pk groundfire $P r240FE
pk flame $P r241E4
pk log $S r2F612
pk platform $S h2F734
pk chest $E h37CC6 h37CD6
pk hidden $E r3784E
pk gold $E 2D314:12
# Shopkeeper at a shop door (class 12 kind 8, specs/stage1_shops_companion.md 1.1): root 0x32C9C, held
pk shopkeeper $E h32C9C
# Effects (class 12 kind 0, routine 3222E: roots 0x32288 + word[variant], lifetimes 0x32260): bursts v4/v5, splash v0; sparkles (kind 2) 0x15498
pk burst $E r323B0 r32414
pk splash $E r3229C
pk sparkle $E r15498
# The hidden companion (specs/stage1_shops_companion.md 2.4; palette from CD, a frame with it on screen): blink, bounce,
# then right/left: walk, jump, roll, wind-up, throw, air throw; its spear (2.5)
D=$CD
pk companion $E r3784E r37868 r3766E r376A2 r376D6 r376F2 r3770E r3777E h377EE h3780E h377FE h3781E h3782E h3783E
pk spear $P h0F3D8 h0F3E8
D=$1
# Stage-1 boss sprites (specs/stage1_boss.md): head 0x16762 (palette 27), paw 0x168D4 (24), breath flames by n (0x242FA, palette 8)
D=$BD
pk bosshead $E r16762
pk bosspaw $E r168D4
pk bossfire $P 242FA:10
# Scripted beast at the arena entrance (5.11 / stage1_spearman.md 3): walk 0,1; stand 2,3 (colours from ARENADUMP:
# at the fight palette 3 is the boss's red flash)
D=$AD
pk arenabeast $E r12024 r12070 h12098 h120A8
# Axe thrower ("spearman", specs/stage1_spearman.md 5): walk 0,1; stand 2,3; hurt 4,5; hurt with the axe
# out 6,7; chop 8,9; throw 10,11; chop strike (held) 12,13; follow-through 14,15; catch 16,17 (facing 0 / 1)
pk spearman $E r1858C r18618 r186A8 r1871E r18798 r187A4 r187B4 r187DE r18810 r188AC r18954 r18A66 r18884 r18920 r189C8 r18ADA r189F2 r18B04
pk axe $P r247D8 r24812
# Intro actors (specs/intro.md 6.4): windmill 0x325BC (palette 0x1F), High Aldwin 0x32AA2, 0x32ACA (0x0A)
D=$ID
pk windmill $E r325BC
pk aldwin $E r32AA2 r32ACA
# Willow's charged spells 7-12 (specs/magic.md 4): bolt R/L, bolt end R/L, rising bolt R/L, fire walkers
# v2 / v4 / v6 R/L, the blast's radial piece moving, then its 8 direction pieces (table 0x34CE2).
pk spell $P rF3FC rF418 rF438 rF448 rFA8C rFAB4 rF8A8 rF91E rF7C4 rF834 rF6E0 rF750 r34C96 34CE2:8
