# Queen Bavmorda (final boss) and her spells: behaviour contract

Class 18 kind 4, routine **0x29972..0x2A6D9** (pose tables 0x2A6DA..0x2AC80;
node lists: the other stage-6 agent). Spells: class 12 kinds 39 / 17 / 41,
class 10 kinds 16 / 21 / 22 / 23 / 17, candelabras class 12 kind 40, altar
brazier class 4 kind 28. Extends `stage6.md` 6.4-6.6 and **supersedes them**
where they differ (section 10 lists the corrections).

Tags: **VERIFIED** = code and trace agree (run D7, `S/ramD7/`, Bavmorda record
**FFF810**, every 2 keys = 2 updates; sounds `S/snd_d7s.txt`, key offset ~+766;
run R `S/ramR/` for the whirlwind), **SOURCE** = code only, **UNKNOWN** = not
established. Speeds in **1/256 px per update**, **y grows upwards** (floor:
Bavmorda y 3160). Arena camera fixed at (1024, 3072). `S` = the stage-6 scratch
dir named in `stage6.md` 0. Rank byte = FF8346 (D7: 0x37).

Shared helpers (behaviour only): rand = 0x1D8E (byte LFSR); 0x1F30 / 0x1F34 =
set pose root from table[+0F] / table[d0]; 0x1F40 = set root directly; 0x1F4C =
step animation; 0x208C = x += table[+0F].x, y += table[+0F].y (table at +18,
entries of 4 bytes); 0x20F4 = angle 0..255 from self to a point (**0 = up,
64 = right, 128 = down, 192 = left**), 0x20D2 = same to the player (FF984C,
FF9850); 0x3C48 = point terrain probe at (x, y + dy[+13]) from 0x3C74
(selector: 0 -> -8, 3 -> -32, 4 -> 0, 14 -> -56) returning "landed";
0x2162 = remove if outside x-camX in [-48, 432], y-camY in [-32, 288];
0x2244 = publish / on-screen flag +1 (x in [camX-32, camX+416)); 0x2B4E /
0x2B9C = take a free class-10 / class-12 record; 0x2C86 = take a palette-job
record; 0x786A = sound; 0x786C = sound only if the caller is on screen.

Direction tables used (index 0 = straight up, increasing clockwise):

| Table | Dirs | Speed (px/update) | User |
|---|---|---|---|
| 0xED068 | 2 (0 right, 1 left) | 256 (1) | walk (phases 2, 4) |
| 0xED864 | 32 | 512 (2) | glide (phase 6) |
| 0xEF5E4 | 128 | 768 (3) | final-phase flight to the altar (phase 12) |
| 0xEF1E4 | 128 | 512 (2) | final-phase flight to centre (phase 14) |
| 0x2634C | 128 | ellipse 768 x / 672 y | spark spiral / orbit |
| 0xEE4E4 | 64 | 1024 (4) | aimed spark |
| 0xEDEE4 + 0x100 n | 64 | 256 + 128 n, n = 0..10 (1 .. 6) | energy ball |
| 0xED464 | 16 | 768 (3) | brazier fireball |
| 0xED070 | 2 | 384 (1.5) | brazier hop |
All VERIFIED (ROM; speeds checked in D7 for walk, glide, spark, ball).

## 1. Record fields and wake-up

| Field | Meaning | Tag |
|---|---|---|
| +0 | 1 = vulnerable and harmful to touch; 2 = immune and harmless (both the shot pass 0x3A452/0x3A42C and the body pass 0x3A8E2 skip a boss with +0 != 1) | VERIFIED (code; D7) |
| +1 | on-screen flag (0x2244) | SOURCE |
| +2 / +3 / +4 | state (0 wake, 2 fight, 4 whirlwind, 6 removed) / phase / sub-phase | VERIFIED |
| +6.l / +0A.l | x / y, 16.16 | VERIFIED |
| +0F | motion direction: 0/1 in the walk, 0..31 in the glide, 0..127 in phases 12/14 | VERIFIED |
| +13 | probe selector 3 (feet = y - 32) | VERIFIED |
| +18.l | direction table (above) | VERIFIED |
| +22 | phase timer (byte; word in phase 14 sub 4) | VERIFIED |
| +2A / +2B | energy / last-seen energy, init **180** | VERIFIED |
| +2C | whirlwind speed | VERIFIED |
| +30 | pose index: 0/1 (face right/left) on foot, 0..8 in flight (table 0x2A966) | VERIFIED |
| +31 | invulnerability countdown (68 after a hit; 0xFF = locked in phases 12/14) | VERIFIED |
| +32 | side: 0 = right half (walk range 1248..1376), 1 = left half (1056..1184) | VERIFIED |
| +33 | star-burst countdown (table 0x29AD8) | VERIFIED |
| +36, +50..+57 | bob state for 0x28050 (phase 14) | SOURCE |
| +38.w | take-off countdown (table 0x29B16) | VERIFIED |
| +3A | pose-turn timer (14) | VERIFIED |
| +3C.l | glide waypoint pointer | VERIFIED |
| +40 | glide steering timer (2) | VERIFIED |
| +41 | energy-ball check countdown | VERIFIED |
| +42 | set to 2 at each lightning (never read in this routine) | SOURCE |
| +43 | current lightning code 1..4 | VERIFIED |
| +44.w / +46 / +48 / +4A | whirlwind rise / max speed / accel / rise step | VERIFIED |
| +4C.l | attack-order pointer (0x29A10 sequences) | VERIFIED (0x29A18 in D7) |
| +60 / +62 | point the flight pose faces in phases 12/14 | SOURCE |
| +64 / +66 | walk range low / high (from 0x29B86[+32]) | VERIFIED |
| +68 | final phase already used (1) | SOURCE |
| +69 | final-phase flash interval (9) | SOURCE |

Globals: **FF83F0/FF83F2** = her x/y, written after every update in every state
(sparks and glows follow it); **FF83F4** = star burst active; **FF83F5** =
lightning code that hit the floor (consumed by a candelabra); **FF83EF** =
the altar was struck (wakes the brazier); **FF83F6** = whirlwind progress for
the ending; **FF0EC2** = live spark count. VERIFIED.

**State 0 (0x29994)**: FF83EA = 1 every update (player frozen) and phases:

| Phase | Behaviour | Tag |
|---|---|---|
| 0 init (0x299B0) | +0 = 2, probe 3, energy 180 into +2A/+2B and FF83CC/FF83CD (bar), clear +31, FF83F4, FF83F5, +68; motion 0xED068; +30 = +0F (1, facing left from the placement table); attack order +4C = 0x29A10[rand & 3]; root 0x2A6DA[+0F] (idle, 4 nodes x 10 ticks loop) | VERIFIED |
| 2 (0x29A2C) | next update (no on-screen wait): FF83CE = 1. If **FF838B** != 0 (dialogue already shown) -> straight to the fight (below). Else FF838B = 1, **FF83A2 = 1** (final-boss flag: the clear becomes the ending), music **0x0F**, +22 = 96, FF83E9 = 1 (clock stops), FF82A6 = 1 (text busy), FF0E58 = 1, message **0x0449** style 0x0306 (d0 192, 0x15CC) -> phase 4 | VERIFIED |
| 4 (0x29A80) | FF83E9 = FF83EA = 1 each update; wait FF82A6 = 0 (text done), then **96** updates; clear the window (0x6770) | VERIFIED (D7 3274..3670 = 300 text + 96) |
| fight start (0x29A9C) | FF83E9 = FF83EA = 0, state 2 phase 0, +22 = **30**, **+0 = 1**, draw the bar (0x4AC6) | VERIFIED (3670) |

## 2. State 2: the fight (0x29B36)

Each update: phase handler (table 0x29B4A), then the hit routine 0x2A45A
(section 4), then publish. A hit never interrupts a phase (no reaction).
VERIFIED.

### 2.1 Decision inputs
| Item | Rule | Tag |
|---|---|---|
| Attack order 0x29A10[rand & 3] | **A 4,1,3,2  B 1,4,2,3  C 3,2,4,1  D 2,3,1,4** (then FF). The codes are **not phases**: code c = candelabra variant c at **x 1088 / 1168 / 1264 / 1344** (table 0x2A256[c]). Each code is used once, by the lightning (phase 10); after the 4th there is **no more lightning** for the fight (the pointer is never reset) | VERIFIED (D7 order A: lightning at x 1344 then 1088) |
| Star-burst delay +33 = 0x29AD8[((rank & 0xE0) >> 5) * 4 + (rand & 3)] | rows: 0: 80 140 180 255; 1: 60 120 180 240; 2: 60 110 170 220; 3: 50 100 140 200; 4: 50 80 130 180; 5: 40 60 100 140; 6: 30 50 80 100; 7: 20 40 60 80 | VERIFIED (D7 row 1: 120, 240) |
| Take-off delay +38 = 0x29B16[rand & 15] | 320 (4/16), 640 (6/16), 960 (6/16); counts down only in phases 2 and 4 (0x29AF8) and **sticks at 0** until used | VERIFIED |
| Ball check (0x2A18E, glide only) | +41 counts down (first value 80); at 0: reload +41 = (energy >= 90 ? 0x2A1D8 : 0x2A1E8)[rank >> 4]; then if bit (rand & 15) of mask 0x2A1F8[rank >> 4] is set -> **phase 8** | VERIFIED (D7: reload 70, ball) |

| rank >> 4 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| interval, energy >= 90 | 80 | 80 | 70 | 70 | 60 | 60 | 50 | 50 | 40 | 40 | 30 | 30 | 20 | 20 | 10 | 10 |
| interval, energy < 90 | 60 | 60 | 50 | 50 | 40 | 40 | 30 | 30 | 30 | 20 | 20 | 20 | 10 | 10 | 10 | 10 |
| mask (hex) | 2453 | 58A4 | 4595 | 2A95 | 6A95 | 6A95 | 73B5 | A69F | AED9 | 6DB5 | DEBD | ABDF | B6DB | FDFB | BEFF | FBF7 |
| chance /16 | 6 | 6 | 7 | 7 | 8 | 8 | 10 | 10 | 10 | 10 | 12 | 12 | 11 | 14 | 14 | 14 |
SOURCE (ROM tables) / VERIFIED (row 3).

### 2.2 Phases
Pose roots: 0x2A6DA = idle/walk {0: 0x2A6E2, 1: 0x2A76E}; 0x2A6DE = cast
{0x2A7FA, 0x2A844}; 0x2A88E = take-off {0x2A892, 0x2A8FC}; 0x2A966 = flight,
9 poses by +30 {0x2ABBC, 0x2AB70, 0x2AB20, 0x2AACC, 0x2A978, 0x2A994,
0x2A9E8, 0x2AA38, 0x2AA84} = facing right ... facing the viewer (4) ... left.
**Flight-pose rule** (0x2A260 / 0x2A2BA / 0x2A328): every 14 updates (+3A),
target index = clamp(((angle + 8) >> 4) - 4, 0, 8) toward the player (phases
6, 8), the candelabra point (X[code], 3160) (phase 10) or (+60, +62) (phases
12, 14); +30 moves one step toward it and the root is set if it changed.
**Foot facing**: +30 = 0 if her x < player x else 1, re-checked when (FF8019 &
15) == 0. VERIFIED.

| Phase / sub | Behaviour | Pose | Tag |
|---|---|---|---|
| **0** (0x29B5A) | hold **30** updates (no animation) -> phase 2, +32 = 0, roll +38 and +33 | idle | VERIFIED |
| **2/0** walk set-up (0x29B8E) | range +64/+66 = 0x29B86[+32]: side 0 **[1248, 1376)**, side 1 **[1056, 1184)**; motion 0xED068; +30 = face player; root 0x2A6DA[+30]; bound rule below; -> sub 2 | walk | VERIFIED |
| **2/2** walk (0x29BD6) | +33 -= 1: at 0 -> **phase 4** (+33 re-rolled). Else if +38 = 0 -> sub 4. Else animate, x += **+-256** by +0F, bound rule: x < low -> +0F = 0 (right), x >= high -> +0F = 1 (left); foot facing every 16 | walk (0x2A6DA) | VERIFIED (3700..3820: 1216 -> 1335) |
| **2/4** take-off (0x29C1C/0x29C3A) | re-roll +38; timer **24**; +0F = +32; root 0x2A88E[+32]; after 24 -> phase 6, +3A = 14, +30 = 8 * +0F (pose 0 or 8) | take-off (6 x 4 ticks) | VERIFIED |
| **4/0** star burst (0x29C82) | if a class-12 record is free: **FF83F4 = 1**, glow class 12 kind 39 at her x, y (section 3.1); +22 = **180**; root 0x2A6DE[+30 = face player] | cast | VERIFIED (3820) |
| **4/2** | 180 updates standing, animating -> sub 4, root 0x2A6DA[face player] | cast | VERIFIED (3822..4002) |
| **4/4** (0x29CE2) | walk exactly as 2/2 (bounds, foot facing) **until FF83F4 = 0** (last spark gone) -> phase 2 sub 0 | walk | VERIFIED (4002..4244) |
| **6/0** glide set-up (0x29D3C) | motion 0xED864 (32 dirs, **512**); +40 = 2; +3C = path start (side 0: list 1 entry 5 = (1376, 3248); side 1: list 2 entry 5 = (1056, 3248)); +0F = **4** (up-right) if side 0, **28** (up-left) if side 1; +41 = **80** | flight 0x2A966 | VERIFIED |
| **6/2** glide (0x29D7C) | animate; move by +0F; **steer** every 2 updates: d = ((angle to waypoint + 4) >> 3) - +0F (mod 32); d = 0 none, d < 16 -> +0F + 1, else - 1. **Waypoint** reached when \|x - wx\| <= 14 and \|y - wy\| <= 14 -> next; at the list end (FFFF): if (player x >= 1216 ? 0 : 1) == +32 -> sub 4 (land), +32 ^= 1, +0F = **18** (down-left) if +32 became 0, **14** (down-right) if 1; else loop to the list start. Then: **lightning check** (next code c: \|x - X[c]\| <= 1 -> consume, +43 = c, phase 10) else ball check (2.1) | flight | VERIFIED (4270..4786) |
| **6/4** descent (0x29DE6) | move by +0F (14: +195, -472; 18: -195, -472) until the probe (y - 32) lands -> phase 2 sub 0, +0F = face player | flight | VERIFIED (4786..4838: 52 updates, +39, -96 px) |
| **8** energy ball (0x29E08) | hovers (no motion). Sub 0: +22 = **20**, glow class 12 kind 17 var 1 (3.2), sound **0xA9** (always). Sub 2: after 20: +22 = 40, ball class 10 kind 21, +0 = 1, direction = (angle to player + 2) >> 2 (64 dirs), at her x, y. Sub 4: after 40 -> phase 6 sub 2 (+41 keeps running) | flight, faces the player | VERIFIED (4444..4504) |
| **10** lightning (0x29EAE) | hovers. Sub 0: +22 = **60**, glow class 12 kind 41 (3.3). Sub 2: after 60: +22 = 30, bolt head class 10 kind 22 var 0, +0 = 2, +0F = code, at her x, y; sound **0xA6**. Sub 4: after 30 -> phase 6 sub 2 | flight, faces (X[code], 3160) | VERIFIED (4272..4364, 4684..4774) |
| **12** final: to the altar (0x29F4C) | entered from a hit (section 4). Every update +31 = 0xFF (immune) and the flash job (4). Sub 0: motion 0xEF5E4 (**768**), +0F = (angle to (1376, 3288) + 1) >> 1. Sub 2: move; re-aim when (FF8019 & 1) = 0; within **+-2** px of (1376, 3288) -> sub 4. Sub 4 = phase-10 sub 0 (kind 41 glow, 60). Sub 6: after 60: bolt head class 10 kind 22 **var 3**, sound 0xA6, +22 = 30. Sub 8: after 30 -> phase 14, (+60, +62) = (1216, 3136) | flight, faces the altar (1376, 3152) | SOURCE |
| **14** final: hover (0x2A080) | sub 0: +3A = 1, +31 = 0xFF, motion 0xEF1E4 (**512**), aim at **(1216, 3272)**, flash job. Sub 2: move, re-aim every 2, flash; within +-2 px -> **+31 = 1** (vulnerable next update), bob set-up (+36 = 0, step 2/2, limit 80/80, speed 80), +22.w = **480**. Sub 4: 480 updates of: sound **0xA3** when (FF8019 & 63) = 0, and the **bob** 0x28050. Sub 6: 32 updates, **y -= 1 px** each. Sub 8: sound + bob forever. **No spells** in phase 14 | flight, faces (1216, 3136) (pose 4) | SOURCE |

Bob 0x28050 (vertical only): q0 y += v, v -= 2 until < 0 (v = 0, q1); q1 y -= v,
v += 2 until > 80 (v = 80, q2); q2 y -= v, v -= 2 until < 0 (q3); q3 y += v,
v += 2 until > 80 (v = 80, q0). About 41 updates per quarter, 6.4 px per
quarter: +6.4 / -12.8 / +6.4 px around her arrival height. SOURCE.

Glide paths (0x2A38A, (x, y) pairs):
- list 1: (1088,3224) (1144,3248) (1216,3264) (1288,3248) (1344,3224)
  **(1376,3248)** (1344,3272) (1288,3248) (1216,3232) (1144,3248) (1088,3272)
  (1056,3248) end -> list 1 start.
- list 2 (0x2A3C0): (1344,3224) (1288,3248) (1216,3264) (1144,3248)
  (1088,3224) **(1056,3248)** (1088,3272) (1144,3248) (1216,3232) (1288,3248)
  (1344,3272) (1376,3248) end -> list 2 start.
A figure-8 at 2 px/update, ~3224..3272 high; she lands on the side **away
from the player**. VERIFIED (D7 4270..4786; landing side and x).

### 2.3 Flow and durations
0 (30) -> 2 -> {4 (180 + until the sparks die, ~420 in D7) -> 2} ...
2 -> take-off (24) -> 6 -> {8 (60), 10 (90)} -> 6 ... -> descent -> 2.
Energy <= 24 (once): -> 12 -> 14 (to the end). D7: 2 (120) -> 4 (424) -> 6
(lightning 4 at once, ball, ball, lightning 1) -> lands 4838 -> 2 (240) -> 4
-> 6 ... VERIFIED.

### 2.4 Body
Hurts on touch only while +0 = 1 (selector 0x3A95A[4] = plain box, 1 point;
body passes only on frames FF8019 & 3 = 3, combat.md 1). Boxes: class-18
table 0x3B07A by node; idle (16,32,32,52). Palette 29. SOURCE / VERIFIED
(idle box, roots.txt).

## 3. Spells

All spawn at **her origin (x, y)** (no hand offset). Every class-10 spell
record removes itself when its owner (+26) reaches state >= 4 (her defeat).
Damage: class-10 projectiles hurt only with **+0 = 1**, 1 point, plain box
(0x3AABC: kinds 16, 21, 22, 17 -> plain; kind 23 -> 3ABBA, protection item
honoured); boxes from 0x3B0D2. VERIFIED (code 0x3AA86).

### 3.1 Star burst: glow (class 12 kind 39, 0x25CA2) and 8 sparks (class 10 kind 16, 0x25D8C)
| Step | Behaviour | Tag |
|---|---|---|
| glow state 0 | +0 = 2; FF0EC2 = 0; set = (rand >> 3) & 3 (byte offset (rand & 24) * 2 into 0x25CB6); for i = 7..0: if a class-10 record is free: spark +0 = **2**, kind 16, at FF83F0/F2, owner, **orbit time +3C = set[i]**, +0F = 0, **delay +22 = 1 + 8 (7 - i)** (1, 9, ..., 57); FF0EC2 += 1. Then root 0x261CA, **60** updates following her (FF83F0/F2), then freed | VERIFIED (3822: 8 sparks, delays 1..57) |
| sets 0x25CB6 | 0: 30 70 110 150 190 230 270 310; 1: 310 30 70 110 150 190 230 270; 2: 110 190 270 30 310 70 230 150; 3: 310 270 230 190 150 110 70 30 (indexed by i) | VERIFIED (set 0 in D7) |
| spark state 0 | probe 4 (point = x, y), offset (ox, oy) = 0, motion 0x2634C, root 0x261F0 (20 ticks, then 3-node 4-tick loop) | VERIFIED |
| sub 0 wait | delay counts down; position = her position | VERIFIED |
| sub 2 **spiral** (80 updates) | each update offset += 0x2634C[+0F], position = her + offset; every 4 updates +0F +=/-= s (+ if her +30 = 0 (faces right), - if 1), mod 128, s = 44, 42, ..., 6 (20 steps); when s reaches 4: **+0 = 1** (harmful), sub 4, remember her +30 | VERIFIED (3838..3918) |
| sub 4 **orbit** (+3C updates) | same offset motion; every 2 updates: if her +30 changed -> +0F += 64 (reverse) and remember; then +0F +4 (she faces right) / -4 (left): one turn per 64 updates, ~30 x 27 px ellipse that **follows her** | VERIFIED (3918..4148, reversal at 4134) |
| sub 6 **aimed** | motion 0xEE4E4, +0F = (angle to the player + 2) >> 2 (aimed **once**, then straight) at **1024** (4 px/update) in absolute coordinates; removed when the probe hits terrain or by 0x2162 | VERIFIED (4148..4170: lands y 3128) |
| end | FF0EC2 -= 1; when 0: **FF83F4 = 0** (her phase 4 ends). Also freed when her +68 != 0 (final phase) or her state >= 4 | VERIFIED (4244) |
Life of one spark: delay + 80 + set[i] + flight (D7 longest 422). No sound.

### 3.2 Energy ball: glow (class 12 kind 17 var 1, 0x36942) and ball (class 10 kind 21, 0x25F38)
| Step | Behaviour | Tag |
|---|---|---|
| glow | var != 0: +12 = 1, +10.w = 8 (draw flags, meaning UNKNOWN); root 0x36C2A **18** updates, then 0x36C4E **60** (var 0: 120), then freed; follows the owner's x, y | VERIFIED (4444..4524) |
| ball state 0 | probe 0 (y - 8), level 0 (table 0xEDEE4 = **256**), +22 = 16, root 0x26224 (3 x 6 ticks loop) | VERIFIED |
| sub 0 flight | every update: x, y += table[+0F]; after **16** updates and then every **4**: level + 1 (max 10): speed **256, 384, 512 ... 1536**; probe lands -> sound **0x37** (on screen only), sub 2, root 0x2624C (burst, 7 x 5 ticks + hold), +22 = 40. Every update 0x2162 | VERIFIED (4464..4514) |
| sub 2 burst | **40** updates in place, still harmful; then +0 = 2 and freed | VERIFIED (4514..4554) |
No homing; no timeout other than the screen edge; touching the player does
not end it (plain handler). VERIFIED / SOURCE (last point).

### 3.3 Lightning: glow (class 12 kind 41, 0x3652C) and bolts (class 10 kind 22, 0x2602E)
| Step | Behaviour | Tag |
|---|---|---|
| glow | +0 = 2, root 0x365E8 (6, 6, then 2-node loop), **80** updates following FF83F0/F2, freed | VERIFIED |
| head (var 0) | not drawn, does not move. Sub 0: spawns **var 1** (+0 = 2, +0F = code) at the head, timer 3, count 7. Sub 2: every **3** updates spawns **var 2** (+0 = 2, +0F = 0) at the head; after 7 -> freed | VERIFIED (4334..4354) |
| segments (var 1, 2) | probe 14 (y - 56), root 0x26330 (2 nodes x 3 ticks loop); every **3** updates **y -= 16**, then probe: landed -> var 1 only: **FF83F5 = code**; freed | VERIFIED |
| harm | var 0/1/2 have **+0 = 2: the lightning column does not hurt** | VERIFIED (RAM +0 = 2; 0x3AA86) |
Geometry: a vertical chain of 8 segments 16 px apart under her, growing at
16 px / 3 updates from her y to the floor; from y 3269 each segment lands at
~**3178** after ~18 updates (FF83F5 = 1 at 4762, 18 after the head). Quirk:
cast from floor height (D7 4334, y 3161) the probe point starts inside the
floor and misses it; the segments fall ~1000 px off screen and land at y
2160 at 4524, so the candelabra fires 190 updates late. VERIFIED (D7) /
UNKNOWN (why the probe misses: terrain internals; reproduce as "fall until the
probe meets a surface from above").

Var 3 / var 4 (phase 12): head var 3 spawns **8 x var 4** every 3 updates,
**+0 = 1 (harmful)**, probe 3 (y - 32); a landing var 4 sets **FF83EF = 1**
(the altar is struck). SOURCE.

### 3.4 Candelabras (class 12 kind 40, 0x3648C) and walking flames (class 10 kind 23, 0x265D4)
| Step | Behaviour | Tag |
|---|---|---|
| candelabra | placed var 1..4 at x 1088/1168/1264/1344, y 3160, +0 = 1, root 0x36570 (static). Sub 0: when FF83F5 = its variant: FF83F5 = 0, +22 = **60**, sub 2. Sub 2: animates (flicker, palettes 28/8) **60** updates, then spawns **3 flames** (+0 = 1, kind 23 var 1) at x **-32, 0, +32**, y **3143**, and is freed | VERIFIED (4524 -> 4584; 4762 -> 4824) |
| flame | sound **0x37** on its first update; root 0x26656 (8 nodes x 4 ticks, loop from 0x2666E); **stationary** (no motion) for **240** updates; then +0 = 2, root 0x266BA (3 nodes x 4, hold) **10** updates, freed. Off-screen check every 16 updates | VERIFIED (x constant; 4584..4836) |
At most 4 candelabras per fight (one per lightning code). Flames hurt with the
protection item honoured. VERIFIED / SOURCE.

### 3.5 Altar brazier (class 4 kind 28, 0x21884) and its fireballs (class 10 kind 17, 0x2654C)
| Step | Behaviour | Tag |
|---|---|---|
| init | +0 = 2 (intangible), +40.w = 63, +22 = **95**, +0E = 2, motion 0xED070 (**384**), +0F = 1, root 0x21A70 | VERIFIED (RAM) |
| always | every update: +40 -= 1; at 0: +40 = 0x21906[rand & 6 / 2] = **256, 384, 128, 511** and sound **0x8F** (crackle). FF83C3 != 0 (stage clear) -> spawns class 12 kind 0 var 5 (+0 = 1) at its place and is removed | VERIFIED (0x8F intervals 511, 384, 511, 384, 256 in D7s) |
| asleep | while FF83EF = 0 nothing else (it stays asleep for the whole fight unless the final phase strikes it) | VERIFIED (D7) |
| wake | FF83EF != 0: +22 counts **95** more updates, then **+0 = 1**, +0E = 0, vy = **1024**, hop counter = 6, timer 8, root 0x21A80 | SOURCE |
| hop | timer 8 -> counter -= 1 (at 0: counter = 5 and turn round), root 0x21A90; **rise**: vy -= 48, y += vy, x +-1.5 px by facing; vy < 0 -> peak: if the counter is odd, **6 fireballs** (dirs 4, 5, 6, 10, 11, 12 of 0xED464: right, down-right x2, down-left x2, left, **3 px/update**) at (x, y + 16); **fall**: vy += 48 (max 1024), y -= vy, x moves, probe (0x3B0A); landed -> next hop (vy 1024, timer 8) | SOURCE |
| fireball | moves straight until off screen or stage clear; plain damage | SOURCE |
Touching the awake brazier hurts (selector 0x3A740[28] = plain). It has no
energy check: it cannot be destroyed. SOURCE.

## 4. Damage taken (0x2A45A, every state-2 update after the phase)
| Item | Value | Tag |
|---|---|---|
| Invulnerability | +31 != 0: -1; on reaching 0: +0 = 1. **68** updates per hit | VERIFIED (3780 inv 65 ... 3846 hit again) |
| Kill check first | +2A = 0 -> defeat (section 5), even while +31 runs | VERIFIED |
| Hit | +2A != +2B: FF83CC = energy (bar redraw 0x4AC6), +2B = +2A, +0 = 2, +31 = 68, sound **0x32** | VERIFIED (9 x 0x32 for 9 hits) |
| Flash | palette job kind 1 (scripted), colours **0xE6E60**, object palette **29**, 3 updates per step, sequence **22** (0x0D80 table): frames 1,0,1,0 ... 1 = white (0xE6E60 frame 0) / normal (frame 1) alternating every 3 updates for ~63 updates | VERIFIED (obj29 changes in vidD7) / SOURCE (sequence) |
| Final-phase trigger | on a hit with +68 = 0 and **energy <= 24**: instead of the flash: (+60, +62) = (1376, 3152), phase 12 sub 0, +31 = 0xFF, +68 = 1, +69 = 9, palette job sequence **23** (frames 0, 1: one white blink). In phases 12 and 14 sub 0/2 a new seq-23 job every **9** updates (white 3 of every 9) | SOURCE |
| Energy loss | the shot's power +2A per hit (1 for Willow's ordinary magic shot: D7 180 -> 171 over 9 hits). **Immune** to class-8 shot kinds **0, 4, 8**: her column of the shot-vs-boss table (0x3A4E6[4] = 4) uses 0x3A544 for them, which only stops the shot (no damage); kinds 1, 2, 3, 5, 6, 7 use the ordinary handlers. Which spells these are: magic.md (kind 4 = spell 9 flash, kind 8 = spell 12 blast per 0xCC50) | VERIFIED (sword-free D7) / SOURCE (immunities) |
| Bar | FF83CC = energy, full = 180 | VERIFIED |
| Reaction | none; the shot pass sets +0 = 2 the same update, her routine registers the loss on the next | VERIFIED (3776) |

## 5. Defeat and the whirlwind
**Energy 0 (0x2A4DC)**: sound **0x20FB** (music fade), score event **0x181A**
(0x1F14: +1,000,000), **FF83C3 = 2** (stage clear -> the ending starts the same
update, stage6.md 7), FF83E9 = 1, **FF83CF = 1**, +0 = 2, **state 4** phase 0,
FF83CC = 0 and bar redraw. The same record becomes the whirlwind at her
current x, y. VERIFIED (D7 5672, R 3670).

State 4 (0x2A574; every update: phase, animate, publish; y += rise in every
moving phase via 0x2A60A: +44 = +4A, y += +44/256):

| Phase | Behaviour | Tag |
|---|---|---|
| 0 | +22 = 64, max +46 = **384**, accel +48 = **4**, rise step +4A = **16**; sound **0x95**; root **0x2AC04** (32, 8, 6 ticks, then a 3-node 6-tick loop from 0x2AC58; palettes 8/29; box (40,136,0,96)) | VERIFIED |
| 2 | 64 updates in place -> speed = 384, rise = 0, flag +22 = 1 | VERIFIED (3674..3738) |
| 4 | speed -= accel, **x += speed** (right); on speed < 0: phase 6 and, first time only, **ending music 0x1D + 0x01FA** (0x78E0); quirk: that call leaves the speed at the sound routine's d0 (**255** in R, **259** in D7) instead of the small negative value | VERIFIED |
| 6 | speed += accel, **x -= speed** until speed >= max -> 8 | VERIFIED |
| 8 | speed -= accel, x -= speed until < 0 -> 10 (the negative value is kept) | VERIFIED |
| 10 | if **on screen** (+1): speed += accel, x += speed until >= max -> 12. **Off screen: removed (state 6), FF83F6 = 2** | VERIFIED |
| 12 | speed -= accel, x += speed; on < 0: **max += 384, accel += 4, rise += 16**, **FF83F6 = 1**, -> 6 | VERIFIED |

Each quarter swing lasts ~96-98 updates; swing n covers 72n px each way
(pass 1: +72, -114 (quirk), +146; pass 2: -293, +293; pass 3: -440 ...);
rise 16n/256 px/update (pass 1 ~26 px). It leaves at the first phase 10 whose
left end is off screen (x < camX - 32 = 992). R (death x 1216): phase 4 at
3738, F6 = 1 at **4158**, removed / F6 = 2 at **4742** (3 passes); D7 (death x
1101): F6 = 1 at 6160, F6 = 2 at **6354** (2 passes). The ending's staff roll
waits for F6 = 1, its sub-step 6 for F6 = 2 (stage6.md 7.2). VERIFIED.

## 6. Sounds
| Command | Event | Where | Tag |
|---|---|---|---|
| music 0x0F (+0x01FA) | first wake | 0x29A44 | VERIFIED |
| 0x2F | dialogue letters | 0x15CC | VERIFIED |
| **0x8F** | brazier crackle every 128..511 updates (dialogue and fight) | 0x218FC | VERIFIED |
| **0xA9** | energy-ball cast | 0x29E54 | VERIFIED (2) |
| **0x37** | ball impact (on screen only, 0x786C) and each flame's birth | 0x25FB2, 0x265E8 | VERIFIED (1 + 6) |
| **0xA6** | lightning cast (phase 10 and phase 12) | 0x29F2E, 0x2A05A | VERIFIED (2) |
| **0x32** | Bavmorda hit | 0x2A490 | VERIFIED (9) |
| 0xA3 | every 64 updates in phase 14 | 0x2A182 | SOURCE |
| 0x20FB | defeat (music fade) | 0x2A4DC | VERIFIED |
| **0x95** | whirlwind start | 0x2A5B2 | VERIFIED |
| 0x1D + 0x01FA | ending music at the first reversal (~161 updates after 0x95) | 0x78E0 | VERIFIED |
The star burst and the sparks make no sound. VERIFIED.

## 7. The grey "lightning" flash (not Bavmorda's)
The room flashes grey from the **area's permanent palette controllers**, not
from her code (no FF8343 / 0xAC4 call in 0x29972..0x2A6D9 or the spell
routines). Controllers 3-6 (stage6.md 2.6), palette-job kind 1 (0x0D1A):
scroll2 palettes **13-16**, sequence **21** (0x0E76: 39 steps, colour frame 0 =
light grey at steps 1, 21, 33, 36, frame 1 = normal otherwise), **3 updates
per step**: a 117-update loop with 3-update grey flashes at offsets ~3, 63,
99, 108. Controllers 1-2 (scroll3 28/29, sequences 19/20, 19 steps x 3 = 57
updates, frames 0..6) animate the storm sky. They step only while the camera
is inside their window (x 768..2304, y 3072..6912) or after FF83C3 / FF8293
is set. VERIFIED (palette diffs in vidD7 every 6 frames: flashes at 3660,
3672, 3732, 3768, 3894, ... period 234 = 2 x 117) / SOURCE (sequence data).

## 8. Native notes
- Codes are lightning columns; keep the one-shot order per fight.
- Spark orbit and spiral are integrated offsets added to her live position;
  aim once at the player when the orbit time ends.
- The lightning column is harmless; its gameplay effect is the candelabra
  flames (and, in the final phase, the brazier).
- Reproduce the whirlwind speed quirk (start of the first leftward swing at
  ~256) or the staff-roll timing shifts by ~60 updates.

## 9. Not traced (SOURCE only)
Phases 12 and 14, bolt var 3/4, the brazier waking, its fireballs and sound
0xA3 never ran in D7 or R (D7 was killed at 171 by the harness, R at the
wake). A run that plays her down to energy 24 is needed to verify them.

## 10. Corrections to stage6.md 6.4-6.6, 2.6, 5.5, 9
- Attack orders are 4,1,3,2 / 1,4,2,3 / 3,2,4,1 / 2,3,1,4 (not "04 03 02") and
  are lightning x columns (candelabras), not a phase order. Phase 4 is driven
  by +33 (0x29AD8), take-off by +38 (0x29B16), the ball by the +41 roll.
- 0xA6 is the **lightning** cast (both addresses), not the star burst; 0x37 is
  the ball impact and the flames, not the sparks; the hit sound is 0x32.
- Sparks are aimed once (no homing); they spiral, orbit, then fly at 4 px.
- Flames are **stationary** (240 + 10 updates), not walking.
- Invulnerability is 68 updates; the hit flash is sequence 22 (63 updates).
- 0x2A518 is the **energy <= 24 final phase** (altar strike, hover), not the
  defeat; 0x181A is the +1,000,000 score event, not a palette fade.
- FF83F6 = 1 is set at the end of each full sweep cycle (first at the end of
  cycle 1), not at the first reversal; the first reversal starts music 0x1D.
- The grey flash is the scroll2 13-16 controllers (section 7).
- The brazier (5.5) wakes only after the final-phase bolt hits the altar.
- `FF8E58` in the Kael/Bavmorda wake is a5 - 0x71A8 = **FF0E58** (a5 =
  FF8000).
