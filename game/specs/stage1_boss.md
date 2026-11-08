# Stage 1 boss and stage end specification

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin` (sha256
0b66efe8...). Behaviour spec for the native rewrite (NATIVE_ENGINE_PLAN.md
sections 2 and 6). Damage, contact schedule, death and restart rules are in
`combat.md`; HUD drawing in `hud.md`; ordinary stage-1 enemies (including the
gate guardian at camera X 1952) in `stage1_enemies.md`. Arcade addresses are
citations only.

Tags: **VERIFIED** (disassembly read here plus a headless arcade trace or the
transliteration `native/engine/src/actor18_*.h`, `actor4_ten.h`,
`actor4_eleven.h`, `actor10_five.h`, `results_bonus.h`), **SOURCE** (one
source), **UNKNOWN**.

Units: world Y **up**; 8.8 px/frame speeds; hit box = (xOffset, width,
yOffset, height), box = [x-xOffset, x-xOffset+width] x [y-yOffset,
y-yOffset+height]. One update = one 60 Hz frame.

Animation nodes (as magic.md 3): +0 piece count (bit 15: long pieces), +2
attribute (low 5 bits = CPS object palette, bits 5/6 flips; high byte non-zero
= packed w x h block of 16x16 tiles, w = (attr>>8 & 15)+1, h = (attr>>12 &
15)+1), +4 selector (bit 15: next pointer is a long), +6 geometry index, +8
ticks (0xFFFF hold); the next node follows the piece list. Setters 1F30 (root
= table + signed word[facing]), 1F40 (root given), stepper 1F4C. Extract poses
offline with these chains (the scratch walker used here reproduces the node
lists below).

Traces (headless lockstep, reference program, `SCRIPT=random INVINCIBLE=1`
with coverage assists; `KILLBOSS=1` zeroes the head's energy): C, F, H, I,
B (video dumps of the arena, fire breath and death). Frame numbers below are
from those traces and only illustrate durations.

---

## 1. Overview

The stage-1 boss is a **giant blue beast** (a furry hound-like monster) made
of three actors, summoned by a green-robed sorcerer at the end of the forest:

| Part | Pool / kind | Role | Energy | Hit by shots | Hurts on touch |
|---|---|---|---|---|---|
| Body | class 18, kind 0 (pool FFF490, 8 x 128 bytes; first allocation slot 7 = FFF890) | animation, attack choice, death | none | deflects all normal shots (boss class 0, BOSS_DEFLECT) | yes (3A96E, every 4th frame) |
| Head | class 4, kind 10 (163D2) | the only damageable part; fire breath | **32** (FF83CC bar) | yes (enemy class 0, REACT) | yes (3A782) |
| Paw | class 4, kind 11 (167B8) | claw swipe | 127, never dies (absorbs shots) | yes (REACT, damage ignored) | yes (3A782) |

Kill the head (32 points of shot power) to win. VERIFIED (disassembly; trace
H: body slot 7 kind 0, head and paw allocated together, bar 0x20).

## 2. Arena and summoning

1. The camera's maximum X is 1952 until the gate guardian there is beaten
   (FF83E7 set); camera event 42D82 then sets max X = **3328** (0xD00),
   auto-scroll mode, FF838A = 1. The camera scrolls up to 3328 and stops: the
   arena is the screen at camera X 3328..3711, camera Y 0. VERIFIED (disasm
   42D82; traces F/I: camera 0x0D00, mode 4).
2. The **sorcerer** (class 4, kind 7, 0x1432C, state handler around 14960) is
   present there. When its intro starts (trace I frame 9894): FF83E9 = 1
   (clock stopped), FF82A6 = 1 (text busy), FF0E58 = 1 (this is the flag that
   refuses spell 9, magic.md open question 1), animation root **0x1528A**
   (3 nodes x 10 ticks, palette 21), **boss music: music word 1** (788A: post
   0x0001, then 0x01FA), and a dialogue box message (15CC job 0x0424 / 0x0306):
   "THE BABY YOU ARE LOOKING FOR IS IN THE HANDS OF BAVMORDA. YOU WOULD BE
   WISE TO GIVE UP." VERIFIED (disasm 1497C..149C2; frame dump B 10200 shows
   the box and text).
3. When the text is finished (FF82A6 clear) and its timer runs out: FF83E9 =
   0, root **0x150FE** (8, 6 ticks, then hold), then after 38 more updates it
   **spawns the body**: class 18 kind 0 at x = **3600** (0xE10), y = camera Y
   + 56 (= 56), FF83CE += 1; then it leaves (state 6). VERIFIED (disasm
   149C8..14A44; trace I: clock frozen 9877..10311, body appears 10306,
   sorcerer gone 10333).
4. The time limit keeps running during the fight (frozen only during the
   dialogue). Restart inside the arena uses checkpoint 6: x 3392, y 72, time
   **1:00**, music word 1 (combat.md 3.5). VERIFIED (ROM 0x30A8).

## 3. Body (class 18 kind 0, routine 26DE8)

Record fields: +0 active (1 contact/hittable, 2 not), +2 state, +3 substate,
+4 phase, +0x22 timer, +0x30 pose counter ("control", 0..7), +0x32 pending
command for the children, +0x34 hit/interrupt flag, +0x36 attack pose, +0x38
command, +0x40/+0x46 links to paw/head.

### 3.1 States
| State | Behaviour | Tag |
|---|---|---|
| 0 init (26DFC) | +0 = 2, state 2, timer 1. If FF838B == 0 ("boss seen"): FF838B = 1, timer **40**, root **0x27222** (rising/appearing: 4 nodes x 10 ticks, last held, palette 25), palette controller (2C86): colours 0xE69C0 -> palette 3, sequence 5, delay 6 | VERIFIED |
| 2.0.0 intro (26E82) | animate; when the timer ends: palette controller 0xE68A0 -> palette 24 (5, 6); spawn **head** (class 4 kind 10, +0 = 1) at (x-32, y+80) and **paw** (class 4 kind 11, +0 = 2) at (x-80, y+24), both linked to the body; phase 2, timer **60**, FF0E96 += 1, root **0x27318**, head-flash palette pair (1663A, section 4.4) | VERIFIED (trace H: head appears 40 updates after the body) |
| 2.0.2 wait (26F40) | 60 updates, no animation; then +0 = 1, substate 2, arm the attack timer (3.2), pending = 0 | VERIFIED (trace H 10347 -> 10407) |
| 2.2 idle (26F5E) | animate the 8-pose loop; on each timer expiry choose an attack (3.2) | VERIFIED |
| 2.4 attack (26FF6) | phase 0: keep animating until the current pose value 0x2702C[control] == attack pose, then pending (+0x32) = command, phase 2 (animation stops: the body holds that pose). Phase 2: wait until the children clear pending, then substate 2 | VERIFIED (trace H: 10535 attack chosen, 10544 pending set, 10653 back to idle) |
| 4 death (270AC) | section 6 | VERIFIED |
| 6 retire (2720E) | section 6 | VERIFIED |

While idle or attacking, if +0x34 is set the body saves state/substate/phase
and enters state 4 substate 0, waiting until +0x34 clears (then restores).
Which code sets +0x34: UNKNOWN (only clears were found, at 1673E); the native
game can omit it unless a trace shows the body freezing on hits.

### 3.2 Attack choice (timer expiry in substate 2)
- Timer = **0x2709C**[rank >> 4] = 64 (rank 0x00-0x5F), 32 (0x60-0x9F), 16
  (0xA0-0xDF), 8 (0xE0-0xFF) updates; rank = FF8346 high byte (combat.md 9).
  Default start rank 0x30 rising ~14/minute: 64 in normal play. VERIFIED
  (table; trace H rank 0x51 -> volleys 178..246 updates apart).
- i = (playerX - 3328) >> 6 (player screen column of 64 px):

  | i (player x) | 0 (3328-3391) | 1 (3392-3455) | 2 (3456-3519) | 3 (3520-3583) | 4 (3584-3647) | 5 (3648-3711) |
  |---|---|---|---|---|---|---|
  | command (0x26FDE[i]) | 1 fire | 1 fire | 0x80 claw | 0 none | 5 fire | 3 fire |
  | attack pose (0x26FE2[i]) | 5 | 3 | 3 | 1 | 1 | 1 |

  (Entries 4 and 5 run on into the next tables in ROM; they are what the
  program reads. Only bit 0 (fire) and bit 7 (claw) are acted on.)
- command 0: no attack this time.
- command 0x80: attack (claw) unconditionally.
- otherwise: if playerY >= 80 (Willow in the air): command 1, pose 1, attack.
  Else draw r = RNG & 15 and read **0x26FE6**[r] = 1,1,-1,0,1,0,1,1,0,1,1,1,1,0,-1,1:
  0 -> no attack (3/16); positive -> attack as chosen (11/16); negative (2/16)
  -> if the attack pose is already 3 the command becomes 0x80 (claw), else
  the pose becomes 3 (fire from pose 3).
- After an attack completes, the next choice happens at the next timer
  expiry (the timer is re-armed when it expires, attack or not).
VERIFIED (disasm 26F5E..26FF4).

RNG: 1D8E, a 16-bit LFSR at FF8284: feedback = bit 9 xor bit 1 rotated into
bit 15 (roxr), result = low byte. Shared with every other random user, so
the exact sequence depends on everything else; the fidelity contract allows
same-distribution randomness. SOURCE.

### 3.3 Body animation and pose values
Active loop root **0x27318** (palette 24, unpacked pieces), control index k =
node k (the counter advances when a node's last tick ends):

| k | node | ticks | pieces | pose value (0x2702C) | hurt box (0x3B07A) | head offset (0x16564) |
|---|---|---|---|---|---|---|
| 0 | 0x27318 | 10 | 53 | 1 | (40,136,0,96) | (-32,+80) |
| 1 | 0x2738C | 6 | 57 | 2 | (56,152,0,96) | (-52,+68) |
| 2 | 0x27408 | 6 | 55 | 3 | (64,160,0,88) | (-72,+56) |
| 3 | 0x27480 | 6 | 64 | 4 | (80,176,0,88) | (-92,+44) |
| 4 | 0x2750A | 10 | 64 | 5 | (96,192,0,72) | (-112,+32) |
| 5 | 0x27594 | 10 | 64 | 4 | (80,176,0,88) | (-92,+44) |
| 6 | 0x2761E | 10 | 55 | 3 | (64,160,0,88) | (-72,+56) |
| 7 | 0x27696 | 10 | 57 | 2 | (56,152,0,96) | (-52,+68) |

One loop = 68 updates: the beast rears back and lunges forward (pose 5 =
lowest, most forward head). Body box at x 3600: from x-40..x+96 (pose 1) to
x-96..x+96 (pose 5), y 56..152 (pose 5: 56..128). VERIFIED (node walk; ROM).
Intro root **0x27222**: 0x27222 (14 pieces), 0x27248 (20), 0x2727A (25),
0x272B6 (44, hold), 10 ticks each, palette 25, box (40,136,0,96).
Death root **0x27716**: 44, 25, 20, 14 pieces x 10 ticks then an empty node
(0x2780C, hold): the beast sinks away. VERIFIED.

## 4. Head (class 4 kind 10, routine 163D2)

### 4.1 Init and following
State 0: energy +0x2A = +0x2B = **32**, FF83CC = 32 and the boss bar drawn
(4AC6: "ENEMY" label, ceil(32/8) = 4 blue cells, hud.md 3.9), root
**0x16762**. Every update while idle it follows the body: when the body's
control changes, head = body + 0x16564[control] (table in 3.3). VERIFIED.

Head animation 0x16762: 5 nodes x 8 ticks (0x16762, 0x1676E, 0x1677A,
0x167A0, 0x167AC hold), palette 27, packed 4x3 block (attr 0x231B; the third
node is 14 unpacked pieces). Hit box for shots (0x3AE62) and for touching
(0x3AC4A) both **(24,48,20,40)** on every node. VERIFIED (node walk).

### 4.2 Fire breath (substate 2, when the body's pending has bit 0)
| Phase | Behaviour |
|---|---|
| start | sound **0x8D**, timer **20** |
| 0 | 20 updates, head animates |
| 2 | after 10 updates, then every **6**: spawn one fireball (class 10 kind 5) with n = 9, 8, ..., 1 at head + 0x164DE[n]; 9 fireballs in 58 updates; if the class-10 pool is full the volley ends early |
| 6 | **24** updates, animating; then substate 0, body pending cleared, root 0x16762 |

Total **108** updates per volley. VERIFIED (disasm 16432..16532; trace H
10544 -> 10652). Fireball offsets 0x164DE (n: dx, dy): 9 (-16,-8), 8
(-24,-16), 7 (-40,-24), 6 (-64,-24), 5 (-88,-24), 4 (-120,-16), 3 (-152,-8),
2 (-184,0), 1 (-216,+8): a line of flames running left from the mouth and
growing (frame dump B 10600). The head (and body) do not move during the
volley, so the line's height depends on the attack pose (head offset
3.3): pose 5 head at (x-112, y+32) puts the flames lowest and farthest left.

### 4.3 Fireball (class 10 kind 5, routine 24294)
- Stationary. Variant = n (stored in +0x0F). Active (+0 = 1, hurts the
  player via 3AAF0) for **24** updates (0x242C0[n] = 24 for n 1..9), then +0
  = 2 for **4** more updates (visible, harmless), then removed. VERIFIED.
- Animation root table **0x242FA** (by n): n 8, 9 -> **0x2430E** (small: 7
  nodes x 4 ticks, looping; boxes (2,4,8,9), (4,8,4,8), (6,12,2,8)); n 5..7 ->
  **0x24366** (medium: boxes (6,12,12,13), (8,16,14,16), (10,20,10,20),
  (14,28,2,22)); n 1..4 -> **0x243BE** (large: (12,24,14,15), (20,36,20,22),
  (20,40,20,40), (16,26,8,24)). Palette 8. Boxes from 0x3B0D2. VERIFIED
  (node walk).
- Not affected by player shots (class 10 is not a shot target); spell 12's
  blast removes enemy projectiles per 0x7102 (magic.md). SOURCE.

### 4.4 Taking damage
Shots hit the head with REACT (combat.md 7.2): the head goes +0 = 2 and
energy -= shot power. The head notices the change on its next update:

| Situation | Response | Tag |
|---|---|---|
| idle, energy > 0 (16584) | copy energy, FF83CC = energy, redraw bar, sound **0x32**, save state, state 4: **16** updates of recovery (still following the body), then +0 = 1 and the saved state resumes | VERIFIED |
| idle, energy 0 (recovery end, 16706) | FF83E9 = 1 (clock stops), sound **0x8E**, body -> death (state 4 substate 2), paw +0 = 2, head dead (state 4 substate 6) | VERIFIED (trace H: hit 10777, death 10795 = 16 + 2 updates) |
| fire breath, energy > 0 (165D2) | copy, bar, sound 0x32, hittable again after **17** updates (+0x44 countdown); the volley continues | VERIFIED |
| fire breath, energy 0 | bar 0, sound 0x8E, +0 = 2, body death, paw +0 = 2, head dead (no FF83E9 here; FF83CF is set by the body's death anyway) | VERIFIED |

Every hit also starts the hit flash (1663A): two palette controllers,
colours 0xE6940 -> object palette 27 and 0xE6A60 -> palette 24, sequence
length 6, delay 4, limited to the region x 2976..3487 (x 0xBA0, width
0x200), y 0..767. So the head (27) and body/paw (24) flash. SOURCE for the
controller field meanings (`actor18_entry.h`).

Hits per kill at magic level L (power L+1): 32, 16, 11, 8, 7, 6, 5 for L =
0..6, with at most one hit per 16 (idle) or 17 (breathing) updates. Charged
spells: magic.md (kinds 3/7 PIERCE do energy at once; spell 9 blast does
nothing to a 32-energy head). Derived.

## 5. Paw (class 4 kind 11, routine 167B8)

- Init: +0 = 1, energy 127/127, lane 2, origin = spawn point (x-80, y+24 of
  the body = 3520, 80), root **0x168D4**. VERIFIED.
- Idle: it is published (drawn and collidable) **only while the body's control
  is 2 or 6** (table 0x16830 = 0,0,1,0,0,0,1,0); otherwise invisible and
  harmless. While published and the body's pending has bit 7: claw swipe.
  VERIFIED.
- Claw swipe: animate root 0x168D4 (8 nodes x 8 ticks, palette 24); each time
  a node ends, control += 1 and the paw moves by 0x1689A[control]: 1
  (-24,+8), 2 (-16,+32), 3 (+24,+24), 4 (-24,-32), 5 (+16,-32), 6 (+16,-8), 7
  (+8,0); when control wraps to 0 (64 updates): body pending cleared, paw back
  to its origin, root reset. VERIFIED (disasm 16838..16896).
- Boxes (shots 0x3AE62 and touch 0x3AC4A are equal): nodes 0x168D4
  (24,48,16,32), 0x168F0 (16,32,32,64), 0x1690E (32,64,10,20), 0x1692C
  (10,20,20,40), 0x16942 (32,64,10,20), 0x16960 (16,32,32,64), 0x1697E
  (16,32,24,48), 0x1698A (24,48,20,40). VERIFIED (node walk).
- Shots hitting it are absorbed: energy drops, it sets itself +0 = 1 again at
  once (168BA), it never dies. VERIFIED.
- At the boss's death it becomes +0 = 2 and is removed with the head.

## 6. Death sequence and stage end

| Time (from the killing hit) | Event | Tag |
|---|---|---|
| +0..16 | head recovery (idle kill) | VERIFIED |
| +18 | sound 0x8E, FF83E9 = 1; body state 4 substate 2: **FF83CF = 1** (scene lock: clock stops, spell 9 refused), body +0 = 2 (no more contact), timer 192 | VERIFIED |
| next 240 | every **6** updates, 40 times: explosion: variant from 0x2715C[RNG & 15] (4,4,5,5,5,4,5,4,4,5,5,4,4,5,5,4), class-12 kind 0 effect at body + (0x271EE[RNG & 15], 0x271EE[RNG & 15] + 48) with 0x271EE = 0,-16,16,-32,48,-48,20,-20,24,-24,40,-40,8,-8,4,-4; sound **0x33**; hit-flash palettes again | VERIFIED |
| about +211 (timer 192) | head and paw removed (state 6); body death root **0x27716** (collapse), timer 192 | VERIFIED (trace H 10988) |
| about +260 (40th burst) | body state 6: **score award job 0x180A = 2000 points**; reward list 13 (2564): **15 pickups** (class 14 kind 0, variant 5, count 14..0) at (x, y+128) | VERIFIED (trace H: score 0x2050 -> 0x4050) |
| about +261 | retire: FF83D0, FF83CE, FF838B cleared; a class-16 "stage exit" object (kind 1, 374C6) is started (if none is free: FF83C3 = 1 at once) | VERIFIED |
| same | camera event 42E02 (FF838B clear and FF83CF set): camera max X = 16000, auto-scroll; the camera then moves to the stage's final max (3456, player.md 7) | VERIFIED (trace H: camera 0xD00 -> 0xD80) |
| about +461 | stage-exit object: after **200** updates FF83C3 = 1 | VERIFIED (trace H 11038 -> 11238) |

What pickup variant 5 is (gold value or item): UNKNOWN here; see the pickup
spec (class 14, 2564 reward lists, table 0x2544).

### 6.1 Stage clear (top mode FF8004 = 4, A5E4)
FF83C3 = 1 makes the play loop (95CA) switch to mode 4: sound **0x10** (stage
clear; not on the final stage), player state 8 (scripted), steps from 0.

| Step (FF8008) | Behaviour | Duration (trace E/H) | Tag |
|---|---|---|---|
| 0 | clear all player shots (2884); scripted input on (FF7892 = 1, stream 0x47E8E for stage 1, table 0xA626); step 2 | 1 | VERIFIED |
| 2 | the game keeps running (actors, collisions, drawing) while Willow walks out under the scripted input; when the exit routine (D9B4..D9EA) sets FF83C3 = 2: step 4 and at once: 3-update timer, results texts "STAGE1" (message 0x0842; stages 2..6 0x0844, 0x086B..0x086E from 0xA6D2) and "CLEAR POINTS" (0x084A) | 94 | VERIFIED |
| 4 | after 3 updates: texts "TIME ... X .00 =" (0x084B) and "LIFE ... X .00 =" (0x084C) | 3 | VERIFIED |
| 6 | compute the bonuses (6.2), jobs 0x100E/0x1010 | 1 | VERIFIED |
| 8 | count: every update sound **0x30**; both counters +100 per update until they reach their bonus; then total -> score | max(time, life bonus)/100 + 2 | VERIFIED (E: 174 s -> 176 updates; stage 2 x2: 205 s -> 412) |
| 10 | 120 updates, fade out | ~176 | VERIFIED |
| 12 | wait for the fade | 12 | VERIFIED |
| 14 | sounds 0xF4, **0x08FB**; scene setup for the story page; FF8343 = 0xA7B0[stage] (1); timer 256; fade in | 1 | SOURCE |
| 16 | 60 updates, then type story page 0xA8EA[stage*4 + page] (stage 1: message **39**, one page: "THANK YOU VERY MUCH FOR SAVING ME. TO SHOW MY APPRECIATION I WILL JOIN YOUR FIGHT." over Madmartigan's portrait, rows 24-26, column 3; per-character sound 0x2F is suppressed in this mode) | 52 | VERIFIED (frame dump E 2890) |
| 18 | wait until typing ends, 60 updates; next page if 0xA8AE[stage] > 0 (stage 1: 0) ; then fade, sound off (784C), stage index + 1, magic-continue minimums (2E1C), per-stage flags cleared (2D80), checkpoint 0 and last-safe position of the new stage (2F8A), rank -4 / stage floor (381BC); all modes 0 -> **stage 2 entry** (FF8004 = 0: map scene etc., intro spec) | ~356 | VERIFIED (trace E: stage word 1 at 2946) |

No shop belongs to the stage end (shops are entered inside stages; shop
spec). SOURCE.

### 6.2 Results bonus (A994..AC42)
- Time: s = remaining seconds = minutes*60 + seconds (BCD, table 0xA980 for
  the minutes); shown at (14,13) (3 digits, big digit font 0x7B..). Time bonus
  = s * stageNumber * 100 (FF83C6 long = s, added stageIndex more times, then
  shifted one BCD byte). The clock is frozen since the boss died (FF83CF), so s
  is the time left at the kill (trace C: 0:33).
- Life: v = 8 * (HP + armour) (BCD byte, wraps to 0 on overflow), shown at
  (16,17); life bonus = v * stageNumber * 100 (= 800 per heart/armour point in
  stage 1).
- Stage number shown at (22,13) and (22,17) followed by the fixed "00" (so
  "X100" in stage 1, "X200" in stage 2).
- Counted values at (30,13) and (30,17) (8 digits, leading blanks), total at
  (18,23); the total is added to the score once (6284) at the end of counting.
- Layout (scroll1 big font, palette 14): row 6 "STAGE1" from column 18, row 9
  "CLEAR POINTS" from column 12, row 13 "TIME" at column 6, "X" (0x7FEA) at
  column 20, "=" (0x7FE8) at column 28; row 17 "LIFE" likewise.
VERIFIED (disasm; `results_bonus.h`; trace E: 174 s, 3 HP -> 17,400 + 2,400
= 19,800; stage 2: 205 s x 2 = 41,000 + 2 HP x 1,600 = 3,200 -> 44,200).

Big results font (2x2 cells, TL code, TR +1, BL +16, BR +17): letters
0x7680 + 2*(letter - 'A') for A..H (0x7680..0x768E), I..P at 0x76A0..0x76AE,
Q..X at 0x76C0..0x76CE (so S = 0x76C4, T = 0x76C6); digits 0x7B22 (0),
0x7B00 + 2*(d-1) (1..8), 0x7B20 (9); blank 0x71A6. SOURCE (decoded from the
frame dump: "STAGE", "CLEAR POINTS", "TIME", "LIFE" all match this rule).

## 7. Sounds (jsr 786A with d0)
| Cmd | Event |
|---|---|
| music word 1 (+0x01FA) | sorcerer appears / checkpoint-6 restart (boss music) |
| 0x8D | fire breath starts |
| 0x32 | head hit |
| 0x8E | head killed |
| 0x33 | each death explosion (40) |
| 0x10 | stage clear |
| 0x30 | bonus count tick (every update while counting) |
| 0xF4, 0x08FB | before the story page |
SOURCE (call sites; meanings of the codes: audio.md).

## 8. Native state and data
- Body: x, y, state/substate/phase, timer, control (0..7), attack pose,
  command, pending, animation. Head: energy, copy, hit delay, saved state,
  volley counter. Paw: origin, control, energy (127, cosmetic). Fireballs:
  up to 9 stationary hazards with a 24 + 4 update life.
- Offline data: roots 0x27222, 0x27318, 0x27716, 0x16762, 0x168D4, 0x2430E,
  0x24366, 0x243BE, 0x1528A, 0x150FE with their pieces; tables 0x2702C,
  0x2709C, 0x26FDE, 0x26FE2, 0x26FE6, 0x16564, 0x164DE, 0x1689A, 0x16830,
  0x242C0, 0x2715C, 0x271EE; palettes and controllers 0xE69C0, 0xE68A0,
  0xE6940, 0xE6A60 (object palettes 3?, 24, 25, 27, 8, 21); geometry 0x3B07A,
  0x3AE62, 0x3AC4A, 0x3B0D2. `native/engine/tools/build_actor4_ten.py` and
  `build_actor10_five.py` already walk the head and fireball chains.
- VRAM: the body uses up to 64 16x16 pieces per pose (palette 24): plan it as
  a resident multi-sprite or a plane-B picture (sgdk-sprites-vram); the
  approved cuts allow approximating colours, not the motion.

## 9. Open questions (UNKNOWN)
1. What sets the body's +0x34 (hit interrupt); none found.
2. Death in the arena: does the sorcerer scene replay on restart (FF838B stays
   1 after a death, so the body would skip its 40-update intro)? Trace a
   death at the boss.
3. Pickup variant 5 (15 dropped by the boss): value.
4. Palette controller 0xE69C0 -> "palette 3": object or scroll palette bank
   (+0x3C = 0); check 2C86's consumer.
5. The exit walk's exact path (scripted input 0x47E8E) and the exit
   condition at D9B4; trace player x during step 2.
6. The sound meaning of music word 1 / 0x01FA and of 0x08FB, 0x10FB
   (`SNDLOG=1` on a B1 candidate run prints the arcade command timeline).
7. Not trace-checked: a real (shot) kill, the claw swipe timing against
   the player, damage from the fireballs.
