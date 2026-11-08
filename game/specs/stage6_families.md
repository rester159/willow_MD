# Stage 6 families: behaviour contracts (supplement to stage6.md)

Arcade: Willow (CPS-1), `build/reference_maincpu.bin`. Research only; arcade
addresses are citations. Conventions as `stage6.md` (world **Y up**, speeds 8.8
px/update, 1 update = 1/60 s, box = (xOff, width, yOff, height), records:
+0 collidable/active, +1 on-screen flag, +2 state, +3 phase, +4 sub, +6 x,
+0A y, +0F facing/"dir", +14 node, +20 kind, +21 variant, +22 timer, +2A/+2B
energy/last energy). Facing 0 = right, 1 = left unless stated.

Tags: **VERIFIED** = code read here + RAM dumps agree; **SOURCE** = code only;
**UNKNOWN** = not established.

Evidence: `S/full.dis`, `S/ram*/` dumps (S = scratch dir of stage6.md 0). Helper
scripts written for this file (headless, read-only) are in
`/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/fam/`:
`tr.py` (record state changes), `rng.py` (per-variant ranges/nodes), `cr.py`
(crusher cycles), `nw.py` (node chain walker with boxes).

Shared helpers referenced below:
- **On-screen flag +1**: 0x2244 sets +1 when x - camX in [-32, 416] and y - camY
  in [0, 272]; 0x2296 (class-4 knights) uses [-64, 448] x [-48, 320]. SOURCE.
- **Activity window 0x22CAC / 0x321F6**: x - camX in [-64, 448], y - camY in
  [-32, 288]. SOURCE.
- Allocators: 0x2A1A class 4, 0x2AB6 class 6, 0x2B4E class 10, 0x2B9C class 12.
  SOURCE.
- Sound calls: 0x786A = always, 0x786C = only when the caller is on screen.

---

## 0. Corrections to stage6.md found while writing this

1. **Class 12 kind 36 is not a chandelier**: it is the **chain of a spike
   crusher** (created by 0x31FF4, section 3). The area-2 chandeliers are the
   class 12 kind 6 **variant 7** cell animators (section 8). stage6.md 5.8 wrong.
2. **Class 4 kind 27 is the cart pusher itself**, not a separate spawner
   (as stage5.md 5.3); its "dir" byte is a **push budget** (section 6).
3. **Kael phase 16 is the parry**, not phase 6; **0xA2 is Kael's hit sound**
   (0x1E8D2, played when his energy drops), not his slash. The slash sound is
   **0x91** (0x1E842). Section 9.
4. **Bavmorda's candelabra flames do not walk**: class 10 kind 23 stays where
   it is created. Sound **0x37** is played by each flame at birth (0x265EC), not
   by her sparks. **0xA6** is played by her lightning casts (phase 10 at
   0x29F2E and phase 12 at 0x2A05A), not by the star burst. Sections 7, 10.
5. **The mortar is not stationary**: when active it bobs vertically 1 px/update
   over 40 px (section 4).
6. stage5.md 5.7 cites 0x333FA for the stage-5 torch variant 5; the variant
   table 0x32FC2 gives **variant 5 = 0x3336A, variant 6 = 0x333FA** (the
   FF8FA8 removal test belongs to variant 6). Section 8.

---

## 1. Mace knight (class 4 kind 19, routine 0x1A7F2), variants 1-10

### 1.1 What differs between variants
One routine for all variants. Only these differ (SOURCE 0x1A816, 0x1A876,
0x1AC04, 0x1ADCA):

| Variant | Patrol window (0x1AC50 + 8v: centre, half-width) | x range | Placed at (dir) | Wake |
|---|---|---|---|---|
| 0 (stage 3) | 0x0820, 128 | 1952..2208 | (2088, 384) | waits until \|dx\| <= 160 and \|dy\| <= 32, then FF83E6 = 1 |
| 1 | 0x0370, 48 | 832..928 | (912, 1624) (1) | none (active at once) |
| 2 | 0x03B0, 48 | 896..992 | (912, 1696) (1) | none |
| 3 | 0x0888, 40 | 2144..2224 | (2208, 2480) (1) | none |
| 4 | 0x08B8, 40 | 2192..2272 | (2208, 2400) (1) | none |
| 5, 6, 7 | 0x0EC0, 64 | 3712..3840 | (3776, 416/800/1312) (1) | none |
| 8, 9 | 0x0D40, 64 | 3328..3456 | (3392, 528/1056) (0) | none |
| 10 | 0x0DD0, 80 | 3456..3616 | (3504, 2160) (0) | none |

- Variant 0 only: skipped at init when FF838A is set (spawns its reward
  platform instead), sets FF83E6 on wake, and on death spawns class 6 kind 1
  variant 1 at the **hard-coded (2336, 256)** and sets FF83E6 = 0, FF83E7 = 1,
  FF838A = 1. Variants 1-10 do none of this (0x1ADCA returns for v != 0).
  SOURCE.
- The window is the only per-variant data: the clamp 0x1AC04 snaps x to
  centre +- half-width and turns the knight (walk direction flipped). VERIFIED
  (x ranges: v1 831..929, v2 895..993, v5 3711..3841 in runs D2/B/D6; the
  1-px overshoot is the move after the clamp).
- **The knight only thinks while its +1 flag is set** (0x1A8BA: on-screen with
  the 0x2296 margin, 64 px left/right, 48 px below, 48 px above the screen).
  Off screen its AI and animation stop. The "waiting pose" is therefore just
  the first node of the walk root for its placement facing: **0x1AE14 (dir 0)**
  / **0x1AEA8 (dir 1)**. There is no separate waiting state. VERIFIED (v3, v4,
  v6-v10 frozen in that node for whole runs; never on screen).

### 1.2 Behaviour (all variants once active; = stage3.md 5.3)
State table 0x1A7FE: 0 init (0x1A806), 2 active (0x1A8B6), 4 dying
(0x1AD70), 6 removed (0x2A36). Active phases 0x1A8DA:

| Phase | Behaviour | Tag |
|---|---|---|
| init | energy 48, colour override 15 (+12 = 1, +10 = 15), motion 0x1A8B2 = **224** (0.875 px/update), look facing +40 = +0F, walk counter +42 = 20, attack timer +22 = 20, root 0x1AE10[+40] | VERIFIED (hp 48) |
| 0 walk (0x1A962) | attack timer -1; at 0: if 0x1A8E2[(rank>>4)*8 + rand&7] != 0 -> phase 2, else timer 20. Patrol clamp (turn at edge). If \|playerX - x\| <= 48: walk direction flipped, back-off timer 0x1AA0A[rand&3] = 32/48/64/80, phase 6. Every 64 updates ((FF8019 + slot) & 63 = 0): look facing +40 := toward the player; if it changed -> phase 4 (turn). Walk counter +42 -1; at 0: walk direction +0F = 0x1A9E4[rand&7] = 0,1,1,0,0,0,1,1, +42 = 96 (32 if +0F differs from +40: he walks backwards facing the player). Move 224/256 px along +0F, animate | VERIFIED (attacks, turns, moonwalk in D2/B) / SOURCE (tables) |
| 2 attack (0x1AB0A) | entered with timer 80, +47 = 1; pose 0x1AF3C (standing, +41 = 0) or 0x1B030 (crouched, +41 = 1) chosen by 0x1AA6A[(rank>>4)*8 + rand&7]. At timer **32**: spawn the 8 flail segments (1.3) at (x + 72 if +40 = 0, x - 72 if +40 = 1; y, or y - 16 when crouched), +49 = 1, sound **0x91** (0x786C). Ends at timer 0, or early once +49 is set and the tip has gone (+4A = 0): phase 0, timer 20, walk root | VERIFIED (segments at knight x + 72) |
| 4 turn (0x1ABAC) | 40 updates of 0x1B104[old +40], then phase 0, timer 20, walk root | VERIFIED (nodes 0x1B108.., 0x1B1B8..) |
| 6 back-off (0x1ABCE) | ends (phase 0, timer 20) when \|dx\| > 160 or the timer runs out; else clamp, move, animate | SOURCE |

Rank tables (rank = FF8346; row = rank >> 4): attack chance 0x1A8E2 = 2/8 for
rows 0-11, 3/8 row 12, 4/8 row 13, 5/8 row 14, 6/8 row 15. Crouched chance
0x1AA6A = 4/8 rows 0-3, 5/8 rows 4-7, 6/8 rows 8-11, 7/8 rows 12-15. SOURCE
(stage-6 rank ~50..110 in the runs = rows 3..6).

Hit (0x1ACF2): energy changed -> override 14, +0 = 2 for **15** updates
(flash 14/15 every 4), sound **0x92** (0x786C); then +0 = 1. Shots use damage
class 1 (shielded, combat.md 7.1). Death (0x1AD86): +0 = 2, sound **0x33**,
drop set **6** (3 x 500 gold), score **0x180A = 2000**, burst class 12 kind 0
var 5. SOURCE (no knight killed in the runs).

Poses (palette 15; box (16,32,24,44) standing/walk/standing thrust,
(16,32,16,24) crouched): walk 0x1AE10 (f0 0x1AE14 0x1AE3A 0x1AE5E 0x1AE80;
f1 0x1AEA8 0x1AECE 0x1AEF2 0x1AF14; 10 ticks, loop); standing attack 0x1AF3C
(f0 0x1AF40 0x1AF64 0x1AF8A; f1 0x1AFB8 0x1AFDC 0x1B002; 16, 32, 127);
crouched attack 0x1B030 (f0 0x1B034 0x1B052 0x1B072; f1 0x1B09C 0x1B0BA
0x1B0DA); turn 0x1B104 (f0 0x1B108 0x1B12C 0x1B14E 0x1B16E 0x1B190; f1
0x1B1B8 0x1B1DC 0x1B1FE 0x1B21E 0x1B240; 8 x 4 then hold). VERIFIED (all seen
in D2/B/D6 dumps).

### 1.3 The flail (class 10 kind 8, routine 0x2484C, variants 0-7)
A **straight horizontal line** of 8 segments (no swing, no vertical motion);
the stage-6 art (tile codes 0x1950/0x1940) is the same as stage 3's "lance".

| Item | Value | Tag |
|---|---|---|
| Spawn | all 8 at the same point (+30 = start x, +0F = knight's +40), linked +26 = knight; tip = variant 0 clears FF0EB6[knight variant] and sets knight +4A = 1 | VERIFIED |
| Out | all move outward at **1536 = 6 px/update** (0xED0BC) by +0F | VERIFIED (12-px steps per 2 dumps) |
| Stop | segment v stops when \|x - start\| reaches 0x24938[v] = **84, 72, 60, 48, 36, 24, 12, 0** px; shafts (v1-7) then wait (phase 2), v7 never moves | VERIFIED (B 17716..17732: v1..v7 at start + 72..0) |
| Turn | the tip (v0) reaches 84 px after 14 updates: sets FF0EB6[knight var] = 1; every segment sees it and heads back (phase 4) | VERIFIED |
| Return | 6 px/update toward its start; removed when back (x passes +30); the tip's removal clears knight +4A (ends the attack) | VERIFIED (shafts removed 17734..17746, tip 17748) |
| Hurt | only the **tip** (variant 0) damages the player (combat.md 2.1 handler 3AB6A); a hit also sets FF0EB6 -> the chain retracts at once | SOURCE |
| Poses | root table 0x249A4[v]: tip 0x249DC -> f0 **0x249E0** / f1 0x249F2 (2 pieces 0x1950, 0x1940); shaft 0x249B4 -> f0 **0x249B8** / f1 0x249CA (2 pieces 0x1950, 0x1950); palette 15, held (127 ticks, self-loop), box (8,16,8,16) | VERIFIED |

Native: reach = 84 px beyond the spawn point (156 px from the knight); a
segment's displayed x = start +- min(6t, 84 - 12v) while extending.

---

## 2. Flipping stepping stones (class 6 kind 9, routine 0x31D04)

Fourteen slabs over the area-1 spikes. They never move; they alternate between
flat and edge-on. **Trigger: player x proximity (contact in x only), then a
fixed timer cycle.** SOURCE 0x31D18..0x31FC0, VERIFIED run B 10600..14900.

### 2.1 Groups and the "dir" byte
| Stone | var | dir (+0F) | Start | Role |
|---|---|---|---|---|
| (784,848) | 0 | 0 | flat | leader of group A (counter FF0F0C) |
| (880,848) (1024,816) (1120,912) (1184,816) (1280,912) | 1-5 | 1 | edge-on | group A followers (wait for FF0F0C != 0) |
| (1536,816) | 6 | 0 | flat | leader of group B (counter FF0F0D, because var != 0) |
| (1600,848) .. (1984,848) | 7-13 | 255 | edge-on | group B followers (wait for FF0F0D != 0) |

Init (0x31D18): dir 0 -> FF0F0C = FF0F0D = 0, phase 0, node 0x31FCE (flat);
dir != 0 -> phase 2, node 0x31FE6 (edge-on). Per variant: flat time +34 =
0x31D68[v] = **120** (all), edge time +36 = 0x31D88[v] = **180** (all), first
delay +3A = 0x31DA8[v] = **120, 100, 200, 300, 400, 500, 120, 100, 200, 300,
400, 500, 600, 700** (v0..v13). VERIFIED (timers in RAM: v1 100 .. v5 500).

### 2.2 States
| Phase / sub | Node | Duration | Standable | Notes |
|---|---|---|---|---|
| 0 (leader waiting) | 0x31FCE flat | until trigger | yes | while +1 is set: rewrites its cells each update; trigger = **\|playerX - x\| < 16** (y not tested) and +1 set -> counter += 1, phase 4 sub 4, timer = +3A |
| 2 (follower waiting) | 0x31FE6 edge-on | until its group counter != 0 | no | then phase 6 sub 4, timer = +3A |
| 4 sub 0 | (previous) | 1 update | becomes yes | sets the cells (if +1), +38 = 1 |
| 4 sub 2 | 0x31FC2 (tilting) | 8 | yes | |
| 4 sub 4 | 0x31FCE flat | +34 = 120 (first time after a trigger: +3A) | yes | |
| 4 sub 6 | 0x31FC2 | 8 | yes | |
| 6 sub 0 | (previous) | 1 | becomes no | clears the cells (if +1), +38 = 0 |
| 6 sub 2 | 0x31FDA (tilting) | 8 | no | |
| 6 sub 4 | 0x31FE6 edge-on | +36 = 180 (first time for followers: +3A) | no | |
| 6 sub 6 | 0x31FDA | 8 | no | then 4 sub 0 |

Steady cycle = **334 updates**: 0x31FC2 9, flat 120, 0x31FC2 8, 0x31FDA 9,
edge-on 180, 0x31FDA 8 (the 1-update subs 0 add one update to the previous
node). VERIFIED (B: v0 flat at 11036, 11370, 11704; period 334). Effect:
touching x 768..800 starts a **wave**: the leader stays flat 120 more updates
while followers open 100, 200, ... 500 (group A) or 100 .. 700 (group B)
updates later, then all cycle forever. VERIFIED (group A trigger at key 10702,
player x 772; group B trigger near 13008).

### 2.3 Standable surface
No sprite contact: class-6 kind 9 has no handler in the player-vs-surface
table (0x39B1A[9] = 0xFF) and cannot be shot. The stone writes **two scroll2
map cells** (0x31F4C / 0x31F68 via 0x31F78): the cells containing (x - 8,
y + 8) and (x + 8, y + 8), attribute bits 10-15 = **collision id 37** (thin
platform, attr | 0x9400) when flat, **id 0** (air) when not. So the slab is a
32-px wide (x - 16 .. x + 15), one-cell-tall thin platform. It is rewritten
whenever +1 changes (0x31F34), because the CPS map window is streamed. SOURCE.
Observed: Madmartigan stood at **y = stone y + 39** on flat stones (B, 18
samples). VERIFIED (observation only). Native: toggle a 2-cell collision
override (id 37 / none) in the collision map, no plane write needed beyond the
sprite.

Poses: 0x31FCE flat (4 pieces), 0x31FC2 / 0x31FDA tilting (4), 0x31FE6
edge-on (2), palette 28, held; box (8,16,48,96) is unused for contact.
VERIFIED (nodes).

---

## 3. Spike crushers (class 6 kind 10, routine 0x31FF4)

| Item | Value | Tag |
|---|---|---|
| Init (0x32008) | create the chain anchor (class 12 kind 36, variant = **dir**, at (x, y + 16), linked); if dir = 2 also create an **archer** class 4 kind 3 **variant 8** at (x, y + 43), facing 1, linked (it rides the block); rest y +30 = y; fall speed +2C = 0; dir cleared; motion 0x32064 = (0, +1024) = **up 4 px/update**; probe +13 = 13 (12 for variant 2); node 0x32222; phase 2 if in the 0x321F6 window else phase 0 | VERIFIED (archers v8 at crusher y + 43 = 1747 in D2; anchors at y + 16) |
| Phase 0 (dormant) | every 8 updates: if in window -> phase 2 | SOURCE |
| Sub 0 | wait timer = 0x320EA[16 v + rand&15]: v0 {64,128,128,64,96,64,128,96,96,96,64,128,128,128,128,128}; v1 {64,32,96,64,96,96,32,96,64,96,96,96,32,96,64,32}; v2 {16,64,64,32,64,64,16,32,64,32,64,16,64,32,64,16} | VERIFIED (96, 64, 127.. at top) |
| Sub 2 | wait at the top | VERIFIED |
| Sub 4 fall | y -= speed; speed += **128** (0.5 px/update^2), cap **1792** (7 px/update); floor test 0x3C48; landed -> sound **0xA8** (0x786C), timer 60. **+2C is never reset**: only the first drop accelerates from 0, later drops fall at 7 px/update from the start | VERIFIED (first drop 28 updates, later ~18 for 131 px) |
| Sub 6 | 60 updates on the floor | VERIFIED |
| Sub 8 rise | up 4 px/update until y >= rest y (then y = rest), sub 0 | VERIFIED (131 px in 32-33 updates) |
| Out of window | every 8 updates: out of the 0x321F6 window -> phase 0 (freezes where it is) | SOURCE |
| Time stop | FF83D2 freezes the subs | SOURCE |

Variants / dir (area 2): v0 dir 2 at y 1704 (x 280, 536, 1176: with archer),
v0 dir 0 at y 1752 (408, 664, ... ), v1 dir 0 (1048, 1752), v2 dir 0 (1560..1944,
1752). Ranges: rest 1704 -> floor 1573 (131 px), rest 1752 -> 1573 (179 px),
v2 1752 -> 1645 (107 px; floor is the staircase). The drop length is set by
the floor, not the variant. VERIFIED.

**Contact** (class-6 handler 0x39E74, combat.md 2.1): solid block x +- 24,
y - 19 .. y + 19 (plus the player's box); the player can **stand on top**
(y = block y + 19 + player +0x54); otherwise pushed 2 px/update sideways and
**hurt (1 HP) when \|dx\| < 32**, not while FF799E or player +0 = 2. Shots are
**deflected** (class-6 kind 10 = damage class 3); indestructible. SOURCE.
Node 0x32222 (9 pieces, palette 26, box (16,32,16,32) = shot box). VERIFIED.

**Chain (class 12 kind 36, 0x362F0)**: anchor variants 0 (dir 0; node 0x363EE,
4 pieces) and 2 (dir 2; 0x3641E, 7 pieces) at rest y + 16; links variant 1
(0x3640C, 2 pieces of tile 0x0AC0, 32 px). While the crusher falls, a link
whose (y - crusher y) >= 32 spawns a new variant-1 link 32 px below itself;
when the rising crusher passes a link (link y < crusher y) the link is removed;
links idle while the crusher is at rest. All palette 26, held. SOURCE / VERIFIED
(var 1 links at y 1576..1736, anchors 1720 / 1768).

**Archer variant 8** (class 4 kind 3, branch 0x136C0 of routine 0x125D0):
rides the crusher at crusher y + 43 (link +26) and shoots horizontally when
low on screen; full contract in section 11.

---

## 4. Mortar (class 4 kind 33, routine 0x22A72) and its part (class 12 kind 37)

State table 0x22A7E: 0 init, 2 active (0x22B22), 4 dying (0x22D3A), 6 removed.

| Item | Value | Tag |
|---|---|---|
| Init | energy **24**, override palette 5, probe 5, +0F = 1, timer +22 = 40, motion **0x22B1A = (0, +256) / (0, -256): vertical 1 px/update**; +31 = 1 if playerX < x else 0 (fixed for life: pose side and bomb side); root 0x22D88[+31]. Next update: class 12 kind 37 at (x + 4, or x - 4 if +31 = 1; y) (0x22AE2) | VERIFIED (hp 24, nodes 0x22D90/0x22E08) / SOURCE |
| Phase 0 dormant (0x22B42) | every 8 updates: in the 0x22CAC window -> phase 2 | VERIFIED (never activated in any run: all dumps phase 0) |
| Phase 2 active (0x22B60) | every 8 updates: out of window -> phase 0 (attack countdown cleared). Each update: move 1 px/update vertically by +0F; +22 -1, at 0: +22 = 40, flip +0F -> **bobs between y and y - 40** (first down). Attack countdown +32: if 0 and \|playerX - x\| <= 128: +32 = 0x22C9C[rand&15] = 48,64,96,64,48,96,128,64,48,64,96,64,64,96,64,96; if non-zero: -1, at 0 -> phase 4. Animate (4-node loop) | SOURCE |
| Phase 4 fire (0x22BAE) | sub 0: timer 42, pose 0x22D8C[+31]; at timer 16: **bomb class 10 kind 10 variant 0** at (x - 16 if +31 = 0, x + 16 if +31 = 1; y + 16), facing +31; at 0: phase 2, pose 0x22D88[+31]. No vertical motion while firing. Out of window -> phase 0 | SOURCE |
| Bomb | variant 0 of stage3.md 5.2: falls from rest, gravity 48/update to 1792, floor test; on the floor 4 shards + burst (sound 0x3E, palette 13); 1 HP on contact | SOURCE |
| Hit (0x22CD8) | override 14 and +0 = 2 for **15** updates (no alternation), sound **0x32** (0x786A); FF83D0 set -> silent death | SOURCE |
| Death | sound **0x33**, drop 0x22D4E[rand&7] = **2,1,2,3,2,4,2,2** (sets as stage3.md 5.1), score **0x1806 = 500**, burst class 12 kind 0 var 5; FF83D0 path: burst only | SOURCE |
| Retire | none (no 0x21C6 call) | SOURCE |

Poses (palette 5, box (12,24,24,40) = the bomb trooper's box): idle loop
0x22D88: f0 0x22D90 0x22DAE 0x22DCA 0x22DE8 / f1 0x22E08 0x22E26 0x22E42
0x22E60 (10 ticks); fire 0x22D8C: f0 0x22E80 (10) 0x22E9C (16) 0x22EBC (16)
loop / f1 0x22EDC 0x22EF8 0x22F18. VERIFIED (walk) / SOURCE (use).

Part class 12 kind 37 (0x36448): static, never updated after creation, node
**0x36472** (6 pieces: 5 x tile 0x0AC0 = the crusher-chain link tile, palette
26, held, box (8,16,8,16)); not removed with the mortar. VERIFIED (x 3460..3628
static in every run). What the art shows (a soldier on a hanging chain?):
UNKNOWN (no on-screen dump; render a frame with the mortar visible).

---

## 5. Tower bomb trooper (class 4 kind 34, routine 0x22F38)

State table 0x22F56: 0 init, 2 active (0x23024), 4 dying (0x23338), 6 removed.
Every 8 updates 0x21C6 (stage index 5: removed when x < FF838C). SOURCE.
None was ever activated in the runs (all three in init phase 2, node 0x1DD4E).
Everything below is SOURCE.

| Item | Value |
|---|---|
| Init phase 0 | window W pair (+5A, +5C) = 0x22FE2[rand&12] = (160,320) (176,352) (144,288) (168,336); palette 5, probe 5; first-ground time +23 = 0x22FF2[rand&3] = **60, 120, 180, 240**; energy **24**; motion 0xED078 (2 px/update); **ladder bounds** (+60 top, +62 bottom) = 0x22FFE[4v]: v0 (1488, 1344), v1 (1744, 1600), v2 (2000, 1856); root 0x1DD4A; floor probe |
| Init phase 2 | wait until +1 (on screen, 0x2244) -> active phase 0 |
| FF83D0 | -> state 4 phase 2 (silent death) |

**Active phase 0 = ground AI** (0x2305A) for +23 updates, as the kind-23
trooper's phase 6 (stage3.md 5.1) **without the dodge jump**: sub 0 decide
(column \|dx\|>>4, tables 0x1D560 / 0x1D660 by rank & 0xF0; c > 15 -> choice
4); choice & 3 != 0 -> face the player, sub 4 throw (timer 48, root 0x1DC72);
else sub 2 walk (root 0x1DD4A): \|dx\| > W -> face and walk toward the player;
\|dx\| <= 96 -> walk away; else re-decide; each step y - 1 + floor probe; no
floor ahead -> turn back, step back, throw with choice 0x23182[rand&3] =
1,2,3,3. Sub 4: at timer 16 bomb class 10 kind 10 **variant choice + 4**
(5/6/7, lobbed) at (x - 16 if +0F = 0, x + 16 if +0F = 1; y + 8); at 0: wait
0x23186[rand&3] = 40, 20, 60, 10 (sub 6, root 0x1DB9A), then sub 0.

**When +23 runs out: phase 2 = ladder mode** (+40 = 1), never left again:

| Sub | Behaviour |
|---|---|
| 0 / 2 | walk (2 px/update) toward the centre column **x = 3584** until \|x - 3584\| <= 48 |
| 4 | motion 0x22FF6 = vertical 1 px/update; +0F = up if playerY >= y else down; climb time 0x232B2[rand&3] = **30, 60, 90, 120**; root **0x233A6** (climb loop f0 0x233AE 0x233CC 0x233E6 0x23404 / f1 0x23422 0x23440 0x2345A 0x23478, 10 ticks) |
| 6 | climb; clamped to [bottom +62, top +60] (turns at the ends); at 0: face the player in x, timer 20, root **0x233AA** (f0 0x23496 10 -> 0x234B4 hold / f1 0x234D6 -> 0x234F4) |
| 8 | at timer 8: bomb **variant 5** (+40 + 4) at (x -+ 16, y + 8); at 0: sub 4 |

Bomb variant 5 (stage3.md 5.2): 3 px/update horizontally, up 1280 decreasing
80/update, then falls; 4 shards and a burst on the floor.

Hit: kind-23 handler 0x1D900 (15 updates, flash 14/5 every 4, sound **0x32**).
Death (0x23354): sound **0x33**, drop 0x2334C[rand&7] = **2,1,2,3,2,4,2,2**,
score **0x1806 = 500**, burst class 12 kind 0 var 5; phase 2 (FF83D0): burst
only. Poses also: stand 0x1DB9A (0x1DB9E / 0x1DBAE), ground throw 0x1DC72
(0x1DC76 / 0x1DCBA), run 0x1DD4A (0x1DD4E / 0x1DDC6); all palette 5, box
(12,24,24,40).

---

## 6. Spiked carts: pusher (class 4 kind 27, 0x214FA) and cart (class 6 kind 8, 0x21AE2)

### 6.1 Pusher (the "spawner")
Variant table 0x2151A: **v0 and v4 -> 0x21520** (wake on screen, push
left), **v2 -> 0x215A6** (immediate, push right). Stage 5 uses v4, stage 6 v0
(placed) and v2 (events).

| Item | Value | Tag |
|---|---|---|
| Wake v0/v4 | when 0 <= x - camX <= 448, 0 <= y - camY <= 256, FF83EA clear | VERIFIED (D3b: x 568 woke at camX 124) |
| Wake v2 | at once (created by the event, 6.3) | VERIFIED |
| On wake | **+36 = dir byte** (push budget), +0F forced 1 (v0/v4) or 0 (v2); create the cart (class 6 kind 8, +0F = pusher's, linked both ways); +0 = **2** (cannot be hit), +35 = 0, timer 64, motion 0xED068 (1 px/update), root 0x21704 | VERIFIED |
| Push (phase 0) | every 64 updates: 0x21624[rand&15] (4/16) -> stop 64 updates (phase 2, +31 = 1, root 0x21700 held); else move 1 px/update, animate. **Variants != 4: +36 -1 per moving update; at 0 -> end** | VERIFIED (dir 96: x 568 -> 473 over 95 moving updates, one 64-update stop) |
| End test (each update) | x < FF838C + 128, or the player is behind (player x > x while facing left, <= x while facing right; 0x21652), or the cart's +32 set (destroyed) -> state 4 | VERIFIED / SOURCE |
| End (state 4) | class 4 kind 14 **variant 10 + v** (v4 counts as 0: v0/v4 -> **10**, v2 -> **12**) at its position and facing (energy 8, the axe brute), +26 copied; pusher removed, +35 += 1 (tells the cart) | VERIFIED (v0 -> var 10, v2 -> var 12 in D3b) |
| Retire | every 32 updates 0x21C6 | SOURCE |

dir values (area 3): table entries 128, 64, 96, 96, 64, 64, 160, 64, 96
(px of pushing); events 100, 192, 192, 160. Stage 5 (v4) has no budget: it
pushes until the cart breaks, the player gets behind, or the camera-min test.

### 6.2 Cart
| Item | Value | Tag |
|---|---|---|
| Init | energy **30**, palette from the node (19); flash palette +10 = 20 (+12 off); pose 0x21CD4[+0F] | VERIFIED |
| Follow (phase 0) | x = pusher x + 48 (+0F = 0) / - 48 (+0F = 1), y = pusher y, every update; while the pusher moves (+31 = 0) the wheel loop animates (4 nodes x 8 ticks, +30 = node index) | VERIFIED |
| Pusher gone (+35 set) | phase 2: the cart stays where it is (no follow, no animation) as an obstacle until destroyed or retired (every 32 updates 0x21C6) | VERIFIED (carts sat at fixed x after their pusher left) |
| Hit | energy changed: +0 = 3, palette flash (+12 toggled every 3) for **10** updates, cracked pose set while flashing; after: +0 = 1, pose **cracked if energy < 10**, else intact. Hit at energy 0 -> state 4 | VERIFIED (hp 30 -> 25 one per hit, act 3 flashes) |
| Damage stages | only two: intact 0x21CD8[f][+30&3] (f0 0x21D04 0x21D2A 0x21D50 0x21D76 / f1 0x21DA0 0x21DC6 0x21DEC 0x21E12) and cracked 0x21CF0[f][+30&3] (f0 0x21E3C 0x21E62 0x21E88 0x21EAE / f1 0x21ED8 0x21EFE 0x21F24 0x21F4A); 14 pieces, palette 19 | VERIFIED |
| Energy 0 | state 4: +0 = 3, burst class 12 kind 0 var 5 (0x492C), 15 updates, then +32 += 1 (the pusher ends) and removed. **No score, no drop** (no 0x1F14/0x2508 call in 0x21AE2..0x21CD4) | VERIFIED (D3b state 4) / SOURCE (no score) |
| Contact (handler 0x39DA6) | solid x +- 24, y - 32 .. y + 32; standable top (player y = cart y + 32 + player +0x54); from below pushed under; from the side the player is snapped out and **hurt only on the cart's front side** (the side its +0F faces) | SOURCE |
| Shots | class-6 kind 8 = damage class 0 (REACT): every hit costs shot power | SOURCE |

Brute score/drop after the cart: the axe brute's (stage6.md 3.3: 200, 100 gold).

### 6.3 Events 3-6 (class-4 event kind 27, routine 0x394FE)
Entry: class 4, kind 27, var 0, +0F = budget, endX, y, startX, spawnY (14-byte
entries at 0x9E70). Every 16 updates: once endX - camX <= 384 (unsigned) and
y - camY <= 256 -> armed. Armed: when \|endX - playerX\| < 10 -> one **variant-2
pusher** at (startX, spawnY) = 256 px to the left, budget = +0F (100, 192, 192,
160), pushing **right** behind the player; the event retires. Retires also via
0x21C6. VERIFIED (D3b: player x 615 vs endX 624 -> pusher at (368, 2480), key
4626). The rank re-arm table 0x395B0 is unused.

---

## 7. Bavmorda's chamber objects

### 7.1 Altar brazier (class 4 kind 28, routine 0x21884)
| Item | Value | Tag |
|---|---|---|
| Init | +0 = 2 (intangible), sound timer +40 = 63, wake delay +22 = 95, motion 0xED070 (**1.5 px/update** horizontal), +0F = 1, node 0x21A70 | VERIFIED (node held in D7) |
| Ambient sound | every state-2 update: +40 -1; at 0: +40 = 0x21906[rand&6] = **256, 384, 128, 511**, sound **0x8F** (0x786A). Runs from the area-6 entry, also during the dialogue | VERIFIED (6 x 0x8F in D7s) / SOURCE |
| Stage clear (FF83C3) | burst class 12 kind 0 var 5, removed | VERIFIED (gone at her death) |
| Phase 0 | waits for **FF83EF**; then 95 updates; then +0 = 1, phase 2, vertical speed 1024, hop count +23 = 6, timer 8, node 0x21A80 | SOURCE |
| Phase 2 crouch | 8 updates; hop count -1, at 0: +23 = 5 and turn; node 0x21A90 (10, 10, 10, hold) | SOURCE |
| Phase 4 rise | y += speed, speed -48/update from 1024 (4 px/update), x moves 1.5 px/update by +0F; at the apex (speed < 0): if the hop count is odd -> **6 shots** class 10 kind 17 at (x, y + 16), directions 4, 5, 6, 10, 11, 12 of the 16-way table 0xED464 (right, 22.5 deg and 45 deg below horizontal on both sides, left), 3 px/update | SOURCE |
| Phase 6 fall | speed +48 to 1024 down, x moves, floor probe; landed -> phase 2 | SOURCE |
| Shots / body | kind 28 is damage class 3 (DEFLECT): indestructible; body hurts on contact once +0 = 1. Shot node 0x265A0 (4 x 10 ticks, palette 8, box (2,4,2,4)); removed off screen or at stage clear | SOURCE |

**FF83EF** is set only by Bavmorda's class 10 kind 22 **variant 4** bolts on
landing (0x261A8), and those come only from her **phase 12**, which starts
once, when her energy falls to **24 or less** (0x2A49E). So the brazier comes
alive only for the last 24 HP of the fight. Not reached in D7 (energy 171):
SOURCE.

### 7.2 Candelabras (class 12 kind 40, 0x3648C), variants 1-4
At x 1088, 1168, 1264, 1344 (y 3160). Node 0x36570 (palette 28) held still.
| Step | Behaviour | Tag |
|---|---|---|
| wait | until **FF83F5 = own variant** (set by a landing kind-22 lightning head, 10.4) -> FF83F5 = 0, timer 60 | VERIFIED (var 4 at 4524, var 1 at 4764 in D7) |
| possessed | 60 updates flickering 0x36570 (palette 28) <-> 0x3659A (palette 8), 3 ticks each | VERIFIED |
| flames | **3 x class 10 kind 23 variant 1** at (x - 32, x, x + 32; y 3143); the candelabra is removed | VERIFIED (4584, 4824) |

Flames (0x265D4): sound **0x37** each at birth; **stationary**; 240 updates
burning (0x26656, 0x26662 then loop 0x2666E..0x266AA, 4 ticks, palette 6, box
(6,12,12,14)), then +0 = 2 and 10 updates of 0x266BA, 0x266C6, 0x266D2;
removed; removed early when outside camera x [-48, 432] / y [-32, 288]
(0x2162, every 16 updates). Contact hurts. VERIFIED (x fixed, 250-update lives).
A candelabra not struck stays for the whole fight.

---

## 8. Scenery animators (class 12 kind 6, 0x32FB4) in stage 6

Variant table 0x32FC2: v0 0x32FD4, v1/v2 0x330C8, v3/v4 0x3329C, v5 0x3336A,
**v6 0x333FA**, **v7 0x3349C**, v8 0x33560. **No sprites**: they rewrite
scroll2 map cells (code word + attribute word **0x0007** = palette 7,
collision id 0). Init 0x32FE8: +30 = map cell address of (x, y) (0x43658),
frame 0, timer 7. Native: animated plane-A tiles at those cells.

| Variant | Block (from the cell containing (x, y): columns right, rows down) | Frames (per column: top, bottom) | Rate | Visible test | Tag |
|---|---|---|---|---|---|
| 6 wall torch (areas 0-3) | 2 x 2 | f0 (0x0D80,0x0D88)(0x0D81,0x0D89); f1 (0x0E20,0x0E28)(0x0E21,0x0E29); f2 (0x0E22,0x0E2A)(0x0E23,0x0E2B); cycle 0,1,2 | every 7 updates while visible | 0x3303E: x in camX-16..+400, y in camY-16..+272 | VERIFIED (vidD2 cells at (160,1664) cycle 0x0D80 -> 0x0E20 -> 0x0E22) |
| 7 chandelier (area 2: (448,1696) (1216,1696) (1376,1696)) | 3 x 2 | fA (0x0DD0,0x0DD8)(0x0DD1,0x0DD9)(0x0DD2,0x0DDA); fB (0x0E30,0x0E38)(0x0E31,0x0E39)(0x0E32,0x0E3A); alternate A/B | every 7 updates while visible | 0x334E8: x in camX-24..+408, y in camY-24..+280 | VERIFIED (vidD2: 0x0DD0 <-> 0x0E30 at all three) |

Variant 6 is removed when FF8FA8 differs from its +24 (area reload); variant 7
has no such test. Not visible -> phase 2 (idle) until visible again. SOURCE.
Class 12 kind 36 is the crusher chain (section 3), not scenery.

---

## 9. Kael's parry spark and sword wave

### 9.1 Parry = Kael phase 16 (0x1E5A2), spark class 12 kind 28
Entry test 0x1E958 (each update in his hit handler 0x1E886): cooldown +47
(8 after each check) clear, no attack in progress (+40, +41, +42 = 0), Kael
turned to face the player, the player facing Kael (FF9855 bit 0), then for
each player shot/sword record (FFC2C6, 20 x 64) with +0 = 2: chance
0x1EA20[(rank >> 1) & 0x70 + rand&15] (+128 table offset for shot variant !=
0) and the record within dx +-40, dy +-32 -> phase 16. SOURCE.

| Sub | Behaviour | Tag |
|---|---|---|
| 0 | +0 = 2 (immune), +42 += 1, sound **0x28**, spark **class 12 kind 28** at (x + 16 if +0F = 0, x - 16 if 1; y); face the player; pose 0x1ED06 (f0 0x1EEFA / f1 0x1EF54, box (16,34,32,56)); 8 updates | VERIFIED (D4b 4422: spark at x 298 = 282 + 16) |
| 2 | FF83EA = 2 (player frozen), player shove speed +44 = +1408 (facing 0) / -1408, deceleration 64/update; pose 0x1ED0A (f0 0x1EF22 / f1 0x1EF7C, box (8,48,32,56)); 8 updates | VERIFIED (nodes) |
| 4 | each update the **player's x** += shove speed (5.5 px/update decreasing); 8 updates; then FF83EA = 0 | SOURCE |
| 6 | 20 updates; then +0 = 1, +42 = 0, phase 6 | SOURCE |

Spark routine 0x267A6: 12 updates, root **0xF2EC** (direct, 4 nodes 0xF2EC
0xF2F8 0xF304 0xF310, 3 ticks, palette 8, box (8,16,8,16), no damage),
removed. VERIFIED (D4b 4424..4436, 5094..5106).

### 9.2 Sword wave (class 10 kind 13, routine 0x25604)
Spawned by 0x1E842 (from phases at 0x1E0F6, 0x1E1E2, 0x1E53C) with sound
**0x91**, at (x + 40 if +0F = 0, x - 40 if 1; y), facing = Kael's, linked.

| Item | Value | Tag |
|---|---|---|
| Flight (state 2) | 14 updates: speed starts 768 and +128/update (facing 0; negated for 1) -> 3.5, 4.0 ... 10 px/update, **94.5 px** in total; y += Kael's vertical speed (+3A) each update | VERIFIED (D4b 3078..3092: x 328 -> 419) |
| Fade (state 4) | 10 updates in place, animating; then removed | VERIFIED |
| Nodes | facing 0: 0x256A4 (4, 8 pieces) -> 0x256B0 (2) -> 0x256C6 (2) -> 0x256DC (hold); facing 1: 0x256F2 -> 0x256FE -> 0x25714 -> 0x2572A; palette 25; **box (8,16,8,16) on every node**. The first node is held during the flight (no animation in state 2) | VERIFIED |
| Damage | class-10 projectile plain box, 1 HP; hurts during flight and fade | SOURCE |

---

## 10. Bavmorda (class 18 kind 4, 0x29972): poses and the phases that use them

Record slot: class-18 pool slot 7 (FFF490 + 7 x 128) in D7. Pose roots are set
with 0x1F30/0x1F34 (index d0) and animated by 0x1F4C. Box base 0x3B07A.

### 10.1 Every pose table
| Table (index) | Nodes (ticks) | Box | Used by | Tag |
|---|---|---|---|---|
| 0x2A6DA[facing] idle/glide | f0 0x2A6E2 0x2A704 0x2A726 0x2A748; f1 0x2A76E 0x2A790 0x2A7B2 0x2A7D4 (10 each, loop) | (16,32,32,52) | init / dialogue (held: no animation in state 0), phase 2 walk, phase 4 subs 2-4; re-faced every 16 updates (0x2A43C: facing = player side) | VERIFIED |
| 0x2A6DE[facing] cast | f0 0x2A7FA (60) -> 0x2A81A hold; f1 0x2A844 (60) -> 0x2A864 hold | (16,32,32,52) | phase 4 sub 0-2 (star burst, 180 updates) | VERIFIED |
| 0x2A88E[side +32] take-off | side 0: 0x2A892 (8) 0x2A8B4 (8) 0x2A8D6 hold; side 1: 0x2A8FC (8) 0x2A91E (8) 0x2A940 (10) loop -> 0x2A8FC | (16,32,32,52) | phase 2 sub 4 (24 updates) | VERIFIED |
| 0x2A966[pose dir 0..8] flight | 0: 0x2ABBC/0x2ABDE; 1: 0x2AB70/0x2AB94; 2: 0x2AB20/0x2AB46; 3: 0x2AACC/0x2AAF4; 4: 0x2A978/0x2A984; 5: 0x2A994/0x2A9BC; 6: 0x2A9E8/0x2AA0E; 7: 0x2AA38/0x2AA5C; 8: 0x2AA84/0x2AAA6 (2-node loops, 7 ticks) | **(16,32,28,46)** | phases 6, 8, 10, 12, 14 | VERIFIED (all 18 nodes in D7) |
| 0x2AC04 whirlwind (direct) | 0x2AC04 (32) 0x2AC2C (8) 0x2AC38 (6) then loop 0x2AC58 0x2AC64 0x2AC70 (6); palettes 29 then 8 | (40,136,0,96) | death state 4 phase 2.. (stage6.md 6.6) | VERIFIED |

Flight pose index (0x2A260 / 0x2A2BA / 0x2A328): every **14** updates the
target index = ((angle to target + 8) >> 4) - 4, clamped 0..8, from 0x20D2 /
0x20F4; the shown index (+30) steps **one** toward it. Index 0 = facing right,
4 = facing down (toward a target straight below), 8 = facing left. Targets:
the player (phases 6, 8), candelabra +43 at y 3160 (phase 10), the waypoint
(+60, +62) (phases 12, 14). On take-off the index starts at facing x 8 (0 or
8). VERIFIED (D7: 0 -> 4 in 6-8-update steps while casting at the
candelabra below) / SOURCE (formula).

### 10.2 Phase map of state 2 (table 0x29B4A)
| Phase | Code | Behaviour | Poses | Tag |
|---|---|---|---|---|
| 0 | 0x29B5A | 30 updates; roll +38 = 0x29B16[rand&15] (320/640/960) and +33 | idle | VERIFIED |
| 2 | 0x29B70 | glide 1 px/update inside her side's range (+32 = 0: x 1248..1376; 1: 1056..1184; 0x29B86), turning at its ends; +33 = 0x29AD8[(rank & 0xE0) >> 3 + rand&3] (rows: 80/140/180/255, 60/120/180/240, 60/110/170/220, 50/100/140/200, 50/80/130/180, 40/60/100/140, 30/50/80/100, 20/40/60/80) counts down -> **phase 4**; +38 counts down in phases 2 and 4 -> at 0: take-off (sub 4, 24 updates, x keeps moving) -> **phase 6** | 0x2A6DA, 0x2A88E | VERIFIED (D7 3700: 120 updates = row 1 value) |
| 4 | 0x29C6C | star burst: class 12 kind 39 glow + FF83F4 = 1; 180 updates cast pose; then glide (sub 4) while FF83F4 != 0; then phase 2 | 0x2A6DE, 0x2A6DA | VERIFIED |
| 6 | 0x29D22 | flight along a **12-waypoint figure-eight** at 2 px/update (motion 0xED864, 32 directions, 0 = up, 8 = right), steering +0F one step per 2 updates toward the waypoint (0x2A3F6); waypoint reached within +-14 px. Path A (side 0) starts at (1376,3248), path B (side 1) at (1056,3248); lists 0x2A38A / 0x2A3C0: (1088,3224) (1144,3248) (1216,3264) (1288,3248) (1344,3224) (1376,3248) (1344,3272) (1288,3248) (1216,3232) (1144,3248) (1088,3272) (1056,3248) and its mirror. At the list end: if the player's side (x < 1216) equals +32 -> land (sub 4: descend until the floor, then phase 2; side toggled), else loop. Each update: lightning test (next code of her attack sequence +4C; when her x is within +-1 of candelabra x 0x2A256[code] -> phase 10); else energy-ball timer +41 (start 80; refill 0x2A1D8 (energy >= 90) or 0x2A1E8 (< 90) by rank row: 80..10 / 60..10) with chance bits 0x2A1F8[row] (6/16 .. 14/16) -> phase 8 | 0x2A966 | VERIFIED (path positions in D7) / SOURCE (tables) |
| 8 | 0x29E08 | energy ball: class 12 kind 17 var 1 glow + sound **0xA9**; 20 updates; class 10 kind 21 aimed at the player; 40 updates; back to phase 6 | 0x2A966 (aim at player) | VERIFIED |
| 10 | 0x29EAE | lightning: class 12 kind 41 glow (60 updates), then class 10 kind 22 **variant 0** with +0F = candelabra code, sound **0xA6**; 30 updates; back to phase 6 | 0x2A966 (aim at the candelabra) | VERIFIED |
| 12 | 0x29F4C | **once, when energy <= 24** (0x2A518): intangible (+31 = 0xFF each update), flies at 3 px/update (0xEF5E4) to (1376, 3288) above the altar; kind 41 glow 60 updates; class 10 kind 22 **variant 3** + sound **0xA6**; 30 updates; phase 14 | 0x2A966 | SOURCE (not reached) |
| 14 | 0x2A080 | flies at 2 px/update (0xEF1E4) to (1216, 3272); then tangible, hovers 480 updates with the vertical bob 0x28050 (speed 80, accel 2: about +-6 px, 160-update period), sinks 1 px/update for 32 updates, then hovers for good with sound **0xA3** every 64 updates | 0x2A966 | SOURCE |

Attack sequences 0x29A10[rand&3]: (4,3,2), (1,4,2,3), (3,2,4,1), (2,3,1,4),
FF-terminated: **the codes are candelabra variants** (struck in that order,
one per pass over its x); after FF no more lightning. VERIFIED (D7: +43 = 4
then 1, candelabras 4 and 1 burned).

### 10.3 Hit handling (0x2A45A)
+31 counts down to +0 = 1. Energy changed: FF83CC = energy, +0 = 2, +31 =
**68** updates immune, bar redraw, sound **0x32** (0x786A); energy <= 24 the
first time -> phase 12 (10.2); otherwise a palette-flash job (0x2C86, palette
29, job 0xE6E60). Energy 0 -> death (stage6.md 6.6). VERIFIED (hits 68 updates
apart are possible; D7 9 hits) / SOURCE.

### 10.4 Her lightning (class 10 kind 22, routine 0x2602E)
| Variant | Behaviour | Tag |
|---|---|---|
| 0 emitter (phase 10) | stays at her position; spawns v1 (head, +0 = 2, carries +0F) then a v2 trail segment every 3 updates, 7 times; removed | VERIFIED |
| 1 head / 2 trail | +0 = 2 (no contact), probe 14, node 0x26330 (2 x 3 ticks, palette 8, box (6,12,8,16)): **down 16 px every 3 updates**; on ground (0x3C48): v1 sets **FF83F5 = +0F** (candelabra), removed | VERIFIED (descent) |
| 3 emitter (phase 12) | spawns v4 every 3 updates, 8 times (+0 = 1) | SOURCE |
| 4 bolt | probe 3, descends like v1; on ground sets **FF83EF = 1** (brazier) | SOURCE |
| all | removed when Bavmorda's state >= 4 | SOURCE |
Observed: a head released at floor level (y 3161) took ~190 updates to signal
(it fell through the floor before its probe found ground); one released at
y 3269 signalled 18 updates later. VERIFIED (keys 4334 -> 4524, 4744 -> 4762).
Native: signal when the head reaches the chamber floor.

---

## 11. Crusher-riding archer (class 4 kind 3 variant 8)

Routine 0x125D0 dispatches on the variant byte (+21) through 0x125DC:
variants 0/2/4/6 -> the normal archer (0x125EA, stage1_enemies.md 5.9),
variants **8/10/12 -> 0x136C0**, a separate state machine with sub-mode
**+2E = variant - 8** (0 = v8 crusher rider 0x13752; 2 = v10 0x137E0; 4 = v12
0x1388E; v10/v12 are not crusher riders and are not described here). No
walking, no patrol, no 16-direction aim, no rank/difficulty table and no rand
anywhere in the v8 path. Evidence: `S/full.dis` 0x136C0..0x13B32, run D2
(`S/ramD2/`, dumps every 2 updates) archers in slots 37/38/39 at x 280/536/1176.
Helper used for the trace: `fam/tr.py ramD2 c4 3 8 0 12000 4b,22,2e,2b`.

### 11.1 Record fields used

| Field | Meaning | Tag |
|---|---|---|
| +26 (long) | **link = the creating crusher's record** (set by 0x3218A) | SOURCE |
| +0F | facing, 0 = right, 1 = left; creator sets 1 | VERIFIED |
| +2E | variant - 8 (0 for v8), set at activation | VERIFIED (D2: 0) |
| +2A/+2B | energy / last energy | VERIFIED |
| +22 | phase timer | VERIFIED |
| +4B | landing-hop counter | VERIFIED |
| +40 | hurt-recovery timer | SOURCE |
| +4C/+4D | phase/sub saved on hit | SOURCE |
| +42, +48, +18, +13 | death flight: vertical speed, wall-stop flag, x motion table, probe selector | SOURCE |

### 11.2 Creation (crusher init 0x32008 -> 0x3218A, crusher dir byte 2 only)

class-4 allocator 0x2A1A; +0 = 1, kind 3, variant 8, +26 = crusher, x = crusher
x, y = crusher y + **43**, facing 1, state 0. No other field is set. VERIFIED
(D2 key 1688: three v8 records at (280,1747) (536,1747) (1176,1747), crushers
rest y 1704).

### 11.3 States (+2, table 0x136CC)

| State | Behaviour | Tag |
|---|---|---|
| 0 dormant (0x136D4) | waits until **x - camX <= 432 and y - camY <= 288** (unsigned: both >= 0). No draw (0x2244 not called), so not shown and not hittable; does **not** follow the crusher | SOURCE; VERIFIED (1688: 280 active with camX 0, 536 and 1176 still state 0, node 0) |
| 0 -> 2 | +2E = 0, energy **4** (+2A = +2B = 4), +4B = 0, face the player (0x138A8), pose 0x131AC (0x1F34). Phase 0 | VERIFIED (hp 4, 2e 0) |
| 2 active (0x13722) | if time stop FF83D2 == 0: phase routine, then **follow the crusher** (11.4). Always: hit test 0x13920 (11.6); every 16 updates retirement 0x21C6 (stage 6: removed when x < FF838C); draw 0x2244 | VERIFIED / SOURCE (retire) |
| 4 hurt / dying (0x139A0) | 11.6, 11.7; draw 0x2244; no time-stop test | SOURCE |
| 6 | record freed (0x2A36) | VERIFIED (slot gone 2 updates after phase 8) |

Facing rule 0x138A8: facing = 1 (left) if x >= player x (FF984C), else 0.

### 11.4 Following the crusher (0x13964)

Each active update, **after** the phase routine: y = (+26 crusher).y + 43; x is
never changed (crushers only move vertically). Landing hop: if +4B == 0 and the
crusher is in sub 6 with timer +22 == 60 (its first update on the floor), +4B =
3; while +4B != 0 each update decrements it and, unless it just reached 0, adds
**+4** to y. Result: y = crusher + 47 for 2 updates after landing, then + 43.
Called in state 2 (not while FF83D2 is set) and in state 4 phases 0 (hurt, still
alive) and 10 (recovery); **not** in state 0 and **not** once dying. VERIFIED
(D2: y 1747 at rest, 1616 on the floor (crusher 1573), 1620 with +4B = 2 or 1
at keys 1864, 2074, 2860; y tracks the falling/rising crusher, e.g. 1673 ->
1617 over 8 updates and 1620 -> 1747 on the rise; tracks during recovery, 2554).

### 11.5 Shooting (state 2, phase +3, table 0x13762)

| Phase | Behaviour | Tag |
|---|---|---|
| 0 watch (0x13768) | if **y - camY <= 144** (unsigned; no x or player-distance test): fire now (below), face the player first, timer 8, pose **0x131B0** (release, 0x1F30), phase 2. Else on updates where (FF8019 + slot ordinal) & 7 == 0: face the player, pose 0x131AC (0x1F34) | VERIFIED (fires when its y <= camY + 144 = 1680; at rest y 1747 it never fires; re-faced to 0 at 2772 when the player passed it) |
| 2 release (0x137B2) | hold the release pose; timer - 1; at 0: face the player, timer **48**, pose **0x13164** (draw), phase 4 (that update also counts 1) | VERIFIED |
| 4 draw (0x137D2) | animate (0x1F4C); timer - 1; at 0 -> phase 0 | VERIFIED |

Shot period while in range: **56 updates** (fire, 8 release, 48 draw, fire on
the next phase-0 update). With the area-2 crushers (camY 1536, floor 1573) the
archer is in range from crusher y <= 1637 on the way down, through the 60
floor updates, until it passes 1680 on the way up: in practice **2 arrows per
drop** (one during the fall, one on the floor). VERIFIED (arrows at keys
1854/1910, 2064/2120, 3300/3356, 3686/3742, 6046/6102, 6288/6344).

**Arrow** (0x138BA): sound **0x34** (0x786C, only when on screen); class-10
allocator 0x2B4E; +0 = 1, kind **0**, variant **0** (left 0 by the free
routine), direction +0F = facing x 8 + 4 = **4 (right) or 12 (left)**, i.e.
always horizontal; position = archer x + (+10 facing right / -10 facing left),
y + 20 - 16 = **y + 4** (the -16 applies only when +2E == 0, i.e. v8). The y
used is the archer's y **before** this update's crusher follow (previous crusher
y + 43, + 4 during a hop). The arrow does not ride the crusher; it is the
ordinary arrow (stage1_enemies.md 5.9: 4 px/update, 64-update life, terrain
probe). VERIFIED (D2: dir 12, var 0, x = archer - 10, y 1684/1681 (archer
1680/1677) and 1620 (1616), moving 4 px/update; no dir-4 arrow seen because the
player was always to the left).

### 11.6 Hit (0x13920, every state-2 update)

- FF83D0 (clear-all) set: sound **0x33** (0x786C), state 6, burst 0x492C (class
  12 kind 0 variant 5); no score, no drop. SOURCE.
- Energy changed (+2A != +2B): +2B = +2A, +0 = 2 (not collidable), save +3/+4
  to +4C/+4D, state 4 phase 0. VERIFIED (2504 act 2 hp 3 -> 2506 state 4).
- State 4 phase 0: sound **0x32** (0x786A, always). Energy left: follow the
  crusher, phase 10, +40 = **10**, pose **0x13634** (hurt). Phase 10: follow;
  after 10 updates +0 = 1, state 2, **phase 2 with timer 1** (so the next update
  goes to the 48-update draw: a hit never fires an arrow immediately and
  restarts the cycle). VERIFIED (hit 2506 -> state 2 phase 2 at 2516 -> draw
  2518; same at 2538/2548, 2554/2564).
- Damage class 0 (0x3A07A[3]); body contact hurts the player with the plain box
  (0x3A740[3] = 0, combat.md 2.1). SOURCE (tables).

### 11.7 Death (energy 0; state 4 phases 2..8)

| Phase | Behaviour | Tag |
|---|---|---|
| 0 | (sound 0x32 as above) +13 = 1, vertical speed +42 = **+1024** (up 4 px/update), x motion table 0x19E8A (**+384 / -384**, 1.5 px/update), facing = 0 if x > player x else 1 (**flies away from the player**), +48 = 0, pose 0x19DFE; phase 2. **Detaches from the crusher** (no follow from here on) | VERIFIED (2808: x 280 -> 277 -> 274, player at 315; 6736 facing 0, x 1176 -> 1179 -> 1182, player 1142) |
| 2 rise | if speed < 0 -> phase 4 (+24 = 0). Else speed -= 64 (cap -1920), y += speed, animate, x += motion unless +48 (set when the wall probe 0x3BC6 hits) | VERIFIED (phase 4 after 18 updates: 2808 -> 2826) |
| 4 fall | every 16 updates off-camera removal 0x2162; floor probe 0x3B0A (terrain only: it falls **through** the crusher block) -> on landing phase 6; else as phase 2 (gravity -64, cap -7.5 px/update, x drift) | VERIFIED (archer killed on the resting crusher at y 1747 landed on the floor at 1583) |
| 6 landed | pose 0x19E02, 10 updates; then sound **0x33** (0x786A), burst class 12 kind 0 variant 5 (0x492C), timer 8, phase 8 | VERIFIED (2868 -> 2878, burst at (257,1583)) |
| 8 | state 6; score event 0x1800 (0x1F14) = **50 points**; drop 0x2508 with set 0x13ADC[rand & 15] = {1,2,1,1,1,1,1,1,1,1,1,1,1,2,1,1}: set 1 = one 10-gold pickup (class 14 kind 0 v2), 14/16; set 2 = one 100-gold pickup (v5), 2/16; freed next update | VERIFIED (score 200 -> 250 and 450 -> 500; drops v5, v5, v2 at the landing spot; slot gone at 2880) |

### 11.8 Poses (class-4 nodes, palette 17, box (18,36,20,32))

| Table (0x1F30/0x1F34) | Facing 0 / 1 root | Content | Used in |
|---|---|---|---|
| 0x131AC | 0x1345C / 0x134B4 | 1 node, hold (bow drawn, aiming) | activation, phase 0 idle |
| 0x131B0 | 0x134C4 / 0x134D4 | 1 node, hold (release) | phase 2 (8 updates after each shot) |
| 0x13164 | 0x13420 / 0x13478 | 5 nodes x 8 ticks (+0x0C each), then hold on 0x1345C / 0x134B4 | phase 4 draw (48 updates: 40 animating + 8 held) |
| 0x13634 | 0x13638 / 0x13674 | 4 nodes x 8 ticks, then hold (0x13668 / 0x136A4) | hurt recovery (10 updates) |
| 0x19DFE | 0x19E42 / 0x19E06 | 2, 2, 16 ticks, then hold (0x19E66 / 0x19E2A) | death flight |
| 0x19E02 | 0x19E72 / 0x19E36 | 1 node, hold | landed, 10 updates |

VERIFIED (D2 nodes: 0x134B4 idle, 0x134D4 after a shot, 0x13478..0x134A8 at 8
updates each, 0x13674 when hit, 0x19E06/0x19E12/0x19E1E/0x19E2A/0x19E36 and
facing-0 0x19E42..0x19E72 when killed).

### 11.9 Native contract (summary)

Spawn with the crusher (dir 2) at (x, y + 43), facing left, inactive and
invisible until x - camX in [0, 432] and y - camY in [0, 288]; then energy 4.
While alive: y = crusher y + 43 every update (+4 for the 2 updates after the
crusher's landing), frozen with the crusher by time stop. Fire a horizontal
arrow (class 10 kind 0 v0, dir 4/12 by facing, at (x +-10, y + 4), sound 0x34)
whenever in phase 0 and y - camY <= 144, then 8 updates release pose, 48
updates draw animation, back to watching; face the player at each fire and
every 8 updates while out of range. Hit: sound 0x32, 10-update hurt pose (not
collidable, still riding), then the 48-update draw. Kill: detach, fly away
from the player (up 4 px, gravity 0.25 px/update^2, 1.5 px/update sideways),
fall to the terrain floor, 10 updates, sound 0x33, burst, 50 points, 10 gold
(14/16) or 100 gold (2/16). Retired when x < FF838C.

UNKNOWN: whether collision tests use +1 (assumed: dormant state 0 is never
drawn, so treated as untouchable); behaviour if the player stands to the right
(code gives dir-4 arrows at x + 10; not observed); v10/v12 variants.

---

## 12. What stays UNKNOWN
- Mortar art: what the class 12 kind 37 five-link object and the palette-5
  figure depict together; whether the 40-px bob reads as climbing a chain.
- The tower troopers, mortars and tower knights were never activated in a run:
  sections 4, 5 are SOURCE only (upper tower not reached).
- Archer variant 8: see 11.9 (collision gate while dormant; dir-4 arrows not observed).
- The exact meaning of the player's +0x54 used by the cart/crusher contact
  handlers (0 or 24 in D2).
- Bavmorda phases 12/14 and the living brazier: SOURCE only (her energy never
  went below 171 in D7). The 0x20F4 angle convention is inferred from the
  observed pose indices.
- Kael's parry chance table rows (0x1EA20) beyond the code reading; the player
  shove distance when blocked by the arena edge.
- The kind-39 star-burst glow and kind-16 sparks' own sounds (not read).
