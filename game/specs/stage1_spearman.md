# Stage 1 "spearman" (axe thrower), its returning axe, and the scripted arena beast

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin`. Behaviour
spec for the native Genesis rewrite; completes `stage1_enemies.md` section 5.8
(spearman) and the kind-1 variant-3 part of 5.11. Arcade addresses are
citations only. Format, units and tags as `stage1_enemies.md`:

- **VERIFIED**: two independent sources agree (disassembly read for this file
  plus the transliteration `native/engine/src/actor4_spear.h`,
  `actor10_returning*.h`, `actor4_one.h`, or plus the arcade traces below).
- **SOURCE**: one source only. **UNKNOWN**: not established.

World X right, world **Y up**; speeds 8.8 px/frame added to a 16.16 position;
one update = one 60 Hz frame. Hit box = (xOffset, width, yOffset, height),
box = [x-xOffset, x-xOffset+width] x [y-yOffset, y-yOffset+height]. Facing
0 = right, 1 = left. RAM: a5 = FF8000. Record fields as `stage1_enemies.md`
section 3 (+0 active, +1 on-screen, +2 state, +3 phase, +4 sub, +6 x, +0A y,
+0F facing, +14 node, +18 motion table, +22 timer, +2A/+2B energy).

**Name.** The earlier files call kind 14 the "spearman". The trace frames show
a bearded, green-armoured warrior who throws a **single-bladed hand axe** that
spins and comes back, and chops with the same axe at close range. This file
keeps "spearman" for the actor and calls the projectile the **axe**. VERIFIED
(rendered trace frames 7568, 7600, 7640, 9420, 9444).

**Traces** (headless lockstep `ref` runs of the original program on the CPS
core; no GUI). The shared lockstep binary has no RAM poke for `ref`, so a
scratch copy of the harness source (the shop agent's `ls_shop.c` with
`POKES=t:addr:val`, plus a `RAMFROM`/`RAMTO` dump window) was compiled in the
scratch directory; the shared binaries were not rebuilt. Base:
`SCRIPT=random STAGE=0 INVINCIBLE=1 KILLBOSS=1 GATEASSIST=900 NUDGE=600`.
- **S1**: plus `POKES=6300:af70:0` (zero the sorcerer mid-boss energy byte, slot
  35 = FFAF46 +2A, at frame 6300; its own update then runs the normal death:
  bridge collapse, FF83E7++, FF83D0 cleared at 6304). Work RAM every frame
  6100..10400, video every 4 frames from 7000. The player reaches the
  spearman, hits it four times and kills it (7542..7612).
- **S2**: S1 inputs until frame 7515, then no input (`SCRIPT=keys RANDUNTIL`);
  the NUDGE driver still moves the player 48 px right every 600 frames. RAM
  every frame 7400..9400, video every 2 frames 7500..8900. Seven axe throws.
- **S3**: S2 continued, RAM 9300..10600, video 9380..9680: close-range chops.
- The scripted beast is read from the earlier `stage1_enemies.md` trace (t2).

Scratch: `/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/spear/`
(`run1.sh`..`run3.sh`, `ls_spear.c`, `tl.py`, timelines `tl2.txt`, `tl3.txt`,
`births1.txt`, `k14_chains.txt`, `png/`). Trace frame numbers are lockstep
VBlank keys (not the same numbers as the enemies trace: this harness build
starts the program at frame 146 instead of 100).

The rank during S1..S3 was 0x48E1..0x5559 (difficulty block 1, see 2.4).

---

## 1. Spearman (class 4 kind 14 variant 0)

Stage table entry 48: class 4, variant 0, kind 14, direction 1 (facing
left), (2760, 104). Routine 0xFFC8[14] = 0x176F6..0x18BD8. Pool slot 39 in the
traces (d7 ordinal = 49 - slot = 10). VERIFIED.

### 1.1 Constants

| Item | Value | Tag |
|---|---|---|
| Energy | **4** (+2A = +2B = 4 at activation, 0x17786) | VERIFIED (trace: 4 -> 3 -> 2 -> 1 -> 0) |
| Score | event 0x1804 -> index 4 = **200** points (0x18508) | VERIFIED (S1: score 1550 -> 1750 at 7611) |
| Drop | set 0x18544[(variant >> 1) & 15]: variant 0 -> set **2** = one variant-5 pickup = **100 gold** (variant 2 -> set 1 = 10 gold) | VERIFIED (S1: class 14 variant 5 born at 7611 at the death point) |
| Palette | 18 on every node (attr low 5 bits) | VERIFIED (ROM) |
| Lane | +0E = 0 (class-4 lane 0: shot pass 39FD4, body pass 3A6AA) | VERIFIED |
| Target class (shots) | 0x3A07A[14] = 0, like the other stage-1 enemies | VERIFIED (ROM) |
| Body-contact handler | 0x3A740[14] = 0 -> plain box test 3A782 (1 HP to the player) | VERIFIED (ROM, combat.md 2.1) |
| Floor-probe selector | +13 = 5 (feet probes, 1.3) | VERIFIED |
| Motion tables (+18) | **walk 0x18BC8** = (+320, -512) right / (-320, -512) left: **1.25 px/frame** with a 2 px/frame ground push; **advance 0x18BD0** = (+512, -512) / (-512, -512): **2.0 px/frame** | VERIFIED (ROM; S2: 1.25 and 2.0 px/frame) |

### 1.2 Hit boxes (the geometry base question from 5.8)

`stage1_enemies.md` decoded node +6 through 0x3B1C2, which is the **player's**
fixed hurt-box table (19 entries; combat.md 1.2). Class-4 enemies use two
other tables, indexed by the same node word +6 (geometry index):

| Use | Base | Spearman (geometry 24, every node) | Tag |
|---|---|---|---|
| Body touching Willow (pass 3A6AA) | 0x3AC4A | **(24,48,32,48)** | VERIFIED (ROM, combat.md) |
| Hit by a player shot (pass 39FD4) | 0x3AE62 | **(24,48,32,48)** | VERIFIED |

So the spearman is 48 px wide and 48 px tall, from 32 px below its origin to
16 px above (y - 32 is its feet, 1.3). Every pose (walk, stand, chop, throw,
hurt) has geometry 24: **the chop and the throw do not extend the hit area**;
the only melee damage is body contact. VERIFIED (node walk of all chains,
`k14_chains.txt`).

Note for `stage1_enemies.md`: its soldier (12,24,18,34), beast (13,26,26,44)
and archer boxes were also read from 0x3B1C2. With the enemy tables the
soldier/archer geometry 0 is (18,36,20,32) body / (18,36,24,48) shots and the
beast geometry 17 is (18,36,16,25) both. Those sections should be re-derived
(combat.md 1.2 already lists the right bases). VERIFIED (ROM lookups).

### 1.3 Terrain helpers used below

- **Feet** (0x3B0A with selector 5, table 0x3B66 + 5*8 = 0x3B8E): front foot at
  (x+12, y-32) facing right or (x-12, y-32) facing left: if that point is
  inside floor material (cell flag 8 and below the floor surface, 0x4190)
  **+1F = 1** ("ground rises ahead"), else 0. Then centre foot at (x, y-32):
  if inside floor material, **y is moved up to the floor surface**, y fraction
  cleared, +24 = 1, result "on floor"; else result 0 (0x40A0). Floor shapes
  and slopes as player.md section 6. VERIFIED (disassembly; S2: y 104 -> 111
  at activation).
- **Cell probe** 0x3734(x, y): 1 if the collision cell at the point has the
  floor flag (descriptor byte 0 bit 3), else 0. **Cell probe with shape**
  0x373C: same, and reports "slope" when the floor shape byte has bit 7 clear,
  "flat" when set. These test the 16x16 cell only, not the surface height.
  VERIFIED (disassembly 0x460E/0x4634).
- **Gravity** (0x1832A): if the vertical speed +42 <= -1920, y -= 7 (speed
  unchanged); else speed -= 40, y += speed (8.8). SOURCE+transliteration
  agree: VERIFIED.
- **Ground push**: walking applies the motion table's -512 (2 px/frame down),
  standing poses do y -= 2; both are followed by the feet probe, which snaps
  back to the floor. VERIFIED.

**Terrain check** (0x18368, run after every walk/advance/return-home step):
1. Feet probe. If +1F is set: done (keep walking).
2. Probe A = cell-with-shape at (x+16, y-40) facing right / (x-16, y-40) left
   (8 px below the feet, 16 ahead). Floor there: slope -> **phase 22**; flat
   -> done.
3. No floor at A: x2 = x+40 / x-40 (kept in +48).
   Probe B = cell at (x2, y+24) (56 px above the feet): floor -> **phase 16**
   (turn back: wall/high step).
4. Probe C = cell-with-shape at (x2, y-24) (8 px above the feet): floor:
   slope -> phase 22, flat -> done.
5. Probe D = cell at (x2, y-56) (24 px below the feet): floor -> **phase 12**
   (step down off the ledge); else -> **phase 16** (turn back: drop too deep).
VERIFIED (disassembly; transliteration `willowNativeSpearTerrain`; S2: at the
spearman's ledge the left-walking spearman always turns at x 2730/2731).

Phase changes clear the sub-phase (+4 = 0).

### 1.4 Shared decisions

- **Face player** (0x18088): facing = 1 if x > playerX, else 0.
- **Vertical window**: (y - playerY) in [-32, +31] (`(u16)(y - pY + 32) < 64`).
- **Near** (0x1805E): if in the vertical window and (x - playerX) in
  [-160, +159] -> Choose; else nothing.
- **Ahead** (0x17FFE): if in the vertical window and the player is in front
  (d = x - pX when facing left, pX - x when facing right, d >= 0) and d < 160
  -> Choose-by-distance with d (no re-facing).
- **Choose** (0x18122): if in the vertical window: face player, d = |dx| in
  the facing direction, Choose-by-distance. Otherwise **phase 8** (return
  home). (Variants >= 10 except 14 take phase 4 instead; not stage 1.)
- **Choose-by-distance** (0x18150): row = row table 0x181CA[(d >> 4) & 15]
  (d >= 256 wraps), column = random (0x1D8E) & 15, block = (rank high byte
  FF8346 >> 2) & 0x30 (rank 0x0000-0x3FFF block 0, 0x4000-0x7FFF block 1,
  0x8000-0xBFFF block 2, 0xC000+ block 3); phase = row[block + column]; the
  row pointer is kept in +36. VERIFIED (disassembly, transliteration rows at
  data bytes 140/156, S2/S3 choices consistent).

### 1.5 Action table (ROM 0x181CA)

Row selector by d >> 4 (word offsets from 0x181CA): 0..2 -> row A (0x181EA),
3 -> B (0x1822A), 4..7 -> C (0x1826A), 8..9 -> D (0x182AA), 10..15 -> E
(0x182EA). So: **A: d 0-47, B: 48-63, C: 64-127, D: 128-159, E: 160-255**.
Each row = 4 difficulty blocks x 16 bytes. Values are phases: **2 chop, 4
advance, 6 throw, 8 return home**. Exact bytes:

```
A 0x181EA  b0: 2 2 2 6 2 2 2 2 2 2 2 2 2 2 2 2   (chop 15, throw 1)
           b1: 2 6 2 6 2 2 2 2 2 2 2 2 2 2 6 2   (13 / 3)
           b2: 2 2 2 6 2 2 2 2 2 6 2 2 6 2 6 2   (12 / 4)
           b3: 6 6 2 6 6 2 6 2 2 2 6 2 2 6 2 6   (8 / 8)
B 0x1822A  b0: 2 4 6 2 4 6 2 4 6 2 4 6 2 4 6 6   (chop 5, adv 5, throw 6)
           b1: 2 6 6 2 4 6 6 6 6 6 4 6 2 4 6 6   (3 / 3 / 10)
           b2: 6 4 6 2 4 6 6 4 6 6 4 6 6 4 6 6   (1 / 5 / 10)
           b3: 6 4 6 6 4 6 6 4 6 6 4 6 6 4 6 6   (0 / 5 / 11)
C 0x1826A  b0: 6 4 6 4 4 6 6 4 6 4 4 6 6 4 6 6   (adv 7, throw 9)
           b1: 6 4 6 4 4 6 6 4 6 6 4 6 6 6 6 6   (5 / 11)
           b2: 6 4 6 6 6 6 6 6 6 6 4 6 6 4 6 6   (3 / 13)
           b3: 6 6 6 4 6 6 6 6 6 4 6 6 6 6 6 6   (2 / 14)
D 0x182AA  b0: 6 4 6 4 4 6 6 4 6 4 4 6 4 4 6 4   (adv 9, throw 7)
           b1: 6 4 6 4 4 6 6 4 6 6 4 6 4 6 6 4   (7 / 9)
           b2: 6 6 6 4 6 6 6 4 6 6 4 6 4 4 6 6   (5 / 11)
           b3: 6 6 6 4 4 6 6 6 6 6 6 6 4 6 6 6   (3 / 13)
E 0x182EA  all 64 bytes = 8 (return home)
```
VERIFIED (ROM dump; transliteration rows identical).

### 1.6 States and phases

**State 0, wake-up** (variant 0, table 0x17716 -> 0x17728):
| Phase | Rule | Tag |
|---|---|---|
| 0 | every frame: if (u16)(x - cameraX) <= 448 and (u16)(y - cameraY) <= 276 -> phase 2, +13 = 5 | VERIFIED (S2: frame 7432, camera X 2315) |
| 2 | feet probe (snaps y to the floor), phase 4 | VERIFIED (y 104 -> 111) |
| 4 | activate: +4E = 0, state 2, phase 0, active 1, lane 0, energy 4/4, home (+30, +32) = (x, y), +34 = +3A = +3B = 0, pose stand (0x186A4), feet probe | VERIFIED (S2: 7434) |
Not drawn, not collidable and not moving while dormant (no publication call).

**State 2, active** (0x17922), each frame: if not frozen (+4E) and not paused
(FF83D2): run the phase. Then every 32 frames ((FF8019 + d7) & 31 == 0) the
stage retirement 0x21C6 (stage 1: removed (state 6) if x < cameraMinX FF838C).
Then the freeze test, the hit test, publication. VERIFIED.

| Phase | Behaviour | Tag |
|---|---|---|
| **0 walk** | sub 0: sub 2, timer 40, face player, motion walk, pose walk (0x18588). sub 2: timer-1; at 0 -> sub 4, timer 40, pose stand (0x186A4); else Ahead, animate, move, terrain check. sub 4 (stand): timer-1; at 0 -> facing = 0x17A22[+34] (0,1,1,0), +34 = (+34+1)&3, sub 0, Near; animate; y -= 2; feet probe. (The table facing lasts one frame: sub 0 faces the player again.) | VERIFIED (S2 9314..9394: 40 walk frames + 40 stand frames) |
| **2 chop** | sub 0: sub 2, timer **45**, face player, pose chop (0x18808: 12, 30, 4, hold). sub 2: timer-1, animate; at 0 -> sub 4, timer **15**, sound **0x8B** (if on screen), pose chop hold (0x1880C). sub 4: timer-1; at 0 -> Choose. No ground push in this phase. Cycle = 1 + 45 + 15 = **61 frames** | VERIFIED (S3: 9394, 9455, 9516.. repeating every 61 frames at d = 47) |
| **4 advance** | sub 0: sub 2, motion advance, timer = 0x17ABE[rand&15] = **16,32,48,64,80,96,112,128** (each twice), face player, pose walk. sub 2: timer-1; at 0 -> Choose; else animate **twice** (double-speed walk cycle), every 16 frames ((FF8019+d7)&15 == 0) face the player again (facing 1 if x >= playerX) and restart the walk pose if the facing changed, move, terrain check | VERIFIED (S2 7522: timer 96, 2 px/frame, node every 6 frames) |
| **6 throw** | see 1.7 | VERIFIED |
| **8 return home** | sub 0: sub 2, facing = 0 if homeX >= x else 1, timer 60, motion walk, pose walk. sub 2: if (x - homeX) in [-10, +9] -> phase 0, sub 2, +34 = 0, timer 1 (one walk frame, then the 40-frame stand). Else timer-1; at 0 -> sub 4, timer 30, pose stand. Else move, animate, Ahead, terrain check. sub 4: timer-1; at 0 -> sub 0 and Near; animate, feet probe | VERIFIED (S2 7473..7481) |
| 10 jump | sub 0: 20 frames rising with +42 = 640, gravity, horizontal motion only, animate; then falls until the feet probe finds floor -> Choose. **Never selected for variant 0** (no table or code path sets phase 10) | SOURCE (dead code here) |
| **12 step down** | sub 0: +42 = -512. sub 2: feet probe; while on floor: gravity, horizontal move, animate (speed keeps growing while the floor holds it up); when the floor is gone -> sub 4. sub 4: feet probe; no floor -> gravity, horizontal move, animate; floor -> Choose. Horizontal motion = the last motion table (walk or advance) | VERIFIED (disassembly + transliteration); not seen in the traces |
| 14, 18, 20 | other variants (scripted entry, attached rider); not stage 1 | SOURCE |
| **16 turn back** | sub 0: facing ^= 1, timer 4, pose stand. sub 2: timer-1; at 0 -> sub 4, timer 10, pose walk (no animation, no motion during the 4 frames). sub 4: timer-1; at 0 -> **phase 8**; animate, move, feet probe. (sub 6, a falling variant, is unreachable for variant 0.) Total 1 + 4 + 10 frames, then return home | VERIFIED (S2: 7458 -> 7459 -> 7463 -> 7473) |
| **22 slope** | if +1F: Choose. Else animate, move, feet probe; off the floor -> phase 16 | VERIFIED (disassembly + transliteration); not seen |

**Freeze** (0x17F6C, variant 0): if cameraY - 128 >= 0 and y is outside
[cameraY - 128, cameraY + 384], +4E = 1 (the phase code is skipped) and the
off-camera test 0x2162 runs (removed if x - cameraX outside [-48, 432] or
y - cameraY outside [-32, 288]); otherwise +4E = 0. Stage 1 camera Y stays
below 128 in this area, so the spearman never freezes there. VERIFIED.

**Hit test** (0x17FA0), after the phase code:
- If **FF83D0** is set: sound **0x33** (only if on screen), state 6, burst
  class 12 kind 0 **variant 5** at its position (0x492C). No score, no drop.
  VERIFIED (disassembly + transliteration; the enemies trace saw it at 8044).
- Else if +2A != +2B: +2B = +2A, active 2 (not collidable), saved phase +3D =
  phase, state 4, phase 0. VERIFIED (S1 7542/7543).

**State 4, hurt / death** (0x1844C):
| Phase | Rule | Tag |
|---|---|---|
| 0 | sound **0x32**, phase 2, hurt timer +3F = **10**, action timer +22 -= 10 and if the byte result is negative as a signed byte (>= 128) it becomes 1; pose: axe out (+3B set) -> 0x187B0, else 0x18794 (both held). Consequence: hurt early in the 200-frame catch wait (remaining >= 138) leaves timer 1, so on resuming the thrower ends the wait next frame without catching, and the axe flies on (2) | VERIFIED (S1: 7543 -> 7544; byte rule in disassembly 0x1847E and transliteration) |
| 2 | +3F - 1; when 0: energy 0 -> phase 4, sound **0x33**, burst class 12 kind 0 **variant 5**. Energy left -> state 2, active 1, and: axe out -> back to the saved phase (throw) at its saved sub-phase with pose 0x1894C (throw follow-through); otherwise pose stand and **Choose** | VERIFIED (S1: 10 hurt frames, then Choose -> throw; death burst at 7610) |
| 4 | state 6, score 200, drop set 2 (one 100-gold pickup) at its position | VERIFIED (S1: 7611) |
**State 6**: +3A += 1 (tells a flying axe its owner is gone), record freed.
VERIFIED (S1: freed at 7612).

Hits land through the shot pass every other frame (combat.md); a shot that
hits sets active 2 at once, so a hurt spearman is not hit again until it
resumes. S1 hit timings 7542, 7574, 7586, 7598 (the third hit came two frames
after recovery).

### 1.7 Throw (phase 6) and catch

| Sub | Rule | Frames | Tag |
|---|---|---|---|
| 0 | sub 2, timer **42**, face player, pose throw (0x18948: 10, 20, 10, 15, 6, 6, hold) | 1 | VERIFIED |
| 2 | timer-1, animate; at 0 -> sub 4 | 42 | VERIFIED (S2 7555..7597) |
| 4 | allocate a class-10 record (0x2B4E); if one is free: active 1, kind **7**, facing = thrower facing, x = thrower x **+32 (right) / -32 (left)** (0x17BA0), y = thrower y **+8**, +3A = +4D = 0, owner +26 = thrower, thrower +36 = axe, sound **0x8B** (if on screen). Then (even if none was free) sub 6, +3B += 1 (axe out), timer 12, pose follow-through 0x1894C | 1 | VERIFIED (S2 7598: axe at 2710,119 for a thrower at 2742,111) |
| 6 | timer-1; at 0 -> sub 8, timer **200** | 12 | VERIFIED |
| 8 | timer-1; at 0 -> catch. Else if axe +4D is set -> catch. Else if (x - axe.x) in [-64, +63] -> axe +4D += 1, catch | <= 200 | VERIFIED (S2: caught at 7722 at distance 61) |
| catch | sub 10, +3B = 0, timer **24**, pose catch (0x18950: 6, 6, hold) | | VERIFIED |
| 10 | timer-1, animate; at 0 -> Choose | 24 | VERIFIED (S2 7722 -> 7746) |
After every sub (variant 0): y -= 2 and feet probe. VERIFIED.

Observed throw cycle (S2, axe thrown left from x 2742): sub 0 at 7554, axe
spawned 7598, turned round 7663 at x 2521 (189 px from the spawn point),
caught 7722 (124 frames after spawn), catch pose until 7746, then a new
action. The same 1 + 42 + 1 + 12 + ~112 + 24 = **~192-frame** cycle repeated
at distances 95..143 (S2 7811, 8141 and three chained throws to 8717; S3
9699). Edge cases from the code (not observed):
- No free class-10 record: no axe, +36 keeps its old pointer; sub 8 then reads
  that stale record (its x / +4D), normally waiting the full 200 frames.
- The 200-frame timeout ends the wait without marking the axe; the axe then
  keeps flying (2.4).

---

## 2. The axe (class 10 kind 7)

Class-10 pool FFC7C6 (20 x 64 bytes), table 0x2353C[7] = **0x2469C**. S2/S3
used slot 19 (ordinal 19 - slot = 0). VERIFIED.

| Item | Value | Tag |
|---|---|---|
| Spawn | thrower (x +-32, y + 8), facing = thrower facing; created during the thrower's update, its own state 0 runs the same frame (after the class-4 pass) | VERIFIED (S2 7598) |
| State 0 | state 2, pose 0x247D4[facing], timer +22 = 8, +38 (rebounded) = 0; facing 0: speed +32 = **+1536**, acceleration +30 = **-24**; facing 1: speed **-1536**, acceleration **+24** | VERIFIED (ROM + trace) |
| Move (0x24744) | speed += acceleration; x += speed (8.8; 6 px/frame at the start); animate; y never changes | VERIFIED (S2: -1536, -1512, -1488 ...; y 119 constant) |
| Phase 0 | 8-frame launch: timer-1; while non-zero move (7 moves); at 0 -> phase 2 with **no move** that frame | VERIFIED (S2 7599..7606) |
| Phase 2 | if owner +3A != 0 (owner dead/removed) -> phase 4. Else if own +4D (caught) -> **state 6 (removed)**. Else move | VERIFIED (S2 7722) |
| Flight shape | decelerates 24/256 px per frame: stops 57 frames into phase 2 (65 frames after spawn), **189 px** from the spawn point, then accelerates back past the thrower without limit; the thrower catches it within 64 px | VERIFIED (S2: turn at 7663, x 2521; S3 rightward throw from 2830 turns at 3019) |
| Phase 4 (owner gone) | surface test (0x75C8): touching a class-6 lane-0 surface -> state 6. Else on frames where (FF8019 + ordinal) & 7 == 0 the off-camera test 0x2162 runs **instead of** moving (removed when x - cameraX outside [-48, 432] or y - cameraY outside [-32, 288]); other frames move | VERIFIED (disassembly + transliteration); not seen |
| Lifetime | **no timer**: ends only when caught, when FF83D0 is set, or (owner gone) at a surface / off camera. While the owner lives there is no off-camera removal | VERIFIED (disassembly + transliteration) |
| Terrain (once) | while not rebounded, each move: class-6 surface test (0x75C8; surface kinds by table 0x7602: kinds 1, 2, 3, 6, 7, 8, 10 count; logs (kind 0) do not), else wall probe 0x3CDC with selector +13 = 0 at (x, y-8) using the wall rule of the cell (descriptor bit 0 when facing right, bit 1 when facing left; x pushed out of the wall). On contact: +38 = 1, speed = **-1024** (thrown right) / **+1024** (thrown left), acceleration unchanged (-24 / +24): it comes straight back faster. Only one rebound; no further terrain tests | VERIFIED (disassembly + transliteration); not seen in the traces |
| FF83D0 set | state 6, active 2, burst class 12 kind 0 **variant 4** (0x4908), no sound | VERIFIED (disassembly + transliteration) |
| State 4 | +4D = 1, freed (no caller found in this area) | SOURCE |
| Player contact | class-10 pass, lane 0 (+0E = 0): tested on frames FF8019 & 3 = 0 or 2 (3AA3C); handler 0x3AABC[7] = 0 -> plain 3AAF0: Willow loses 1 HP (armour/protection rules as combat.md 2.2); **the axe is not destroyed and keeps flying** | VERIFIED (ROM tables, combat.md) |
| Player shots | no pass tests player shots against class 10 (combat.md 1): **the axe cannot be shot down or blocked** | VERIFIED |
| Hit box | geometry 0 in 0x3B0D2 = **(8,16,8,16)** (16x16 centred) | VERIFIED (ROM) |
| Palette | 18 (node attr) | VERIFIED |
| Pose | 0x247D4: facing 0 root **0x247D8**, facing 1 root **0x24812**; 4 nodes x **2 ticks**, loop (spin: 8-frame cycle). First node 4 separate pieces, the other three one packed 2x2 block | VERIFIED (ROM; S2 node sequence 24812, 24824, 24830, 2483C every 2 frames) |

---

## 3. Scripted beast at the boss-arena entrance (class 4 kind 1 variant 3)

Stage table entry 55: class 4, variant 3, kind 1, direction 1 (facing left),
(3744, 72). Kind-1 routine 0x115EE, variant table 0x115FC -> 0x11640, state
table 0x1164C. VERIFIED.

| State / phase | Rule | Tag |
|---|---|---|
| State 0 (0x11F4E), first update after the stage load | if **FF838B** ("boss already met", set by the boss body's init, boss spec) is set -> state 6 (removed at once: continue/restart after reaching the boss). Else active **2**, FF0E96 = 0, state 2, timer **72**, motion 0xED078 = **+512 / -512 (2 px/frame)**, pose 0x11FCC[facing] (beast charge cycle, held on its first node while waiting) | VERIFIED (disassembly; t2: state 2 from the first gameplay frame) |
| State 2 phase 0 | wait until its on-screen flag +1 is set by publication (x - cameraX + 32 <= 448 and y - cameraY <= 272, i.e. camera X >= 3328 here) -> phase 2. **No other flag**: it starts walking as soon as the camera reaches the arena | VERIFIED (t2: frame 9880, camera 3328, the same moment the arrival script starts) |
| Phase 2 | timer-1; at 0 -> phase 4, pose 0x11FD4[facing] (stand, held). Else x += motion (2 px/frame toward its facing, y unchanged) and animate (charge cycle, 6 nodes x 5 ticks). 71 moves = **142 px**: 3744 -> 3602 | VERIFIED (t2: 3744 at 9880 -> 3602 at 9952) |
| Phase 4 | wait until **FF0E96 != 0** -> state 4. FF0E96 is incremented by the boss body when it spawns the head and paw (0x26F2E, boss spec section "intro") | VERIFIED (t2: FF0E96 = 1 at 10350, state 6 at 10350, freed 10352) |
| State 4 -> 6 | state 6 next frame, then freed (0x11F36). No effect, sound, score or drop: it simply disappears | VERIFIED (t2) |

- **Never collidable**: active stays 2 for its whole life, and the shot and
  body passes test only active == 1. It cannot be hurt and never hurts the
  player; it has no energy (+2A not set). VERIFIED (disassembly; combat.md 1).
- No stage-retirement or off-camera test in this variant. Drawn (published
  every frame through 0x2244) from the first update, visible once on screen.
- Boxes, for completeness: geometry 17 -> (18,36,16,25) body and shots
  (0x3AC4A / 0x3AE62); unused. Palette 3 (node attr 0x1303 / 0x1323: packed
  4x2 blocks, 0x20 = horizontal flip for facing 1).
- Animation: walk/charge table **0x11FCC**: facing 0 root **0x12024**, facing
  1 root **0x12070** (6 nodes x 5 ticks, loop); stand table **0x11FD4**:
  facing 0 **0x12098**, facing 1 **0x120A8** (single node, held). VERIFIED (ROM;
  t2 node sequence 12070, 1207C, 12088, 1204C, 12058, 12064 every 5 frames,
  stand 120A8).
- The transliteration (`actor4_one.h`, variant 3) maps FF838B to `bossGate`
  and FF0E96 to `*scriptReady`; its null-pointer check on `scriptReady` is not
  a wake condition. VERIFIED.

---

## 4. Sounds (command numbers; `jsr 0x786C` = only when the actor is on screen, `0x786A` = always)

| Sound | When | Tag |
|---|---|---|
| 0x8B | chop strike (end of the 45-frame wind-up); axe release | VERIFIED (disassembly 0x17A66, 0x17B82; transliteration 139) |
| 0x32 | spearman hurt (every hit, also the fatal one) | VERIFIED (0x1846C; transliteration 50) |
| 0x33 | spearman death (after the 10 hurt frames); spearman cleared by FF83D0 (on screen only) | VERIFIED (0x184B2, 0x17FCA; transliteration 51) |
None for the axe or the scripted beast. (The sound log of the harness works
only for the cartridge image, so the commands were not captured from the
traces.)

---

## 5. Animation roots (for the pose packer)

Node format and stepping: `stage1_enemies.md` section 3. Tables are read with
0x1F30 (table + signed word[facing]); facing-1 nodes carry attr bit 0x20
(horizontal flip). Ticks per node; "hold" = 0xFFFF. All spearman nodes:
palette 18, geometry 24. Full listing with piece counts:
`.../scratchpad/agents/spear/k14_chains.txt`.

| Use | Table | Facing 0 root | Facing 1 root | Nodes (ticks) |
|---|---|---|---|---|
| Walk / advance (advance steps it twice per frame) | 0x18588 | **0x1858C** | **0x18618** | 11, 11, 11, 11, loop |
| Stand | 0x186A4 | **0x186A8** | **0x1871E** | 12, 12, 12, loop |
| Hurt (no axe out) | 0x18794 | **0x18798** | **0x187A4** | hold (packed) |
| Hurt (axe out) | 0x187B0 | **0x187B4** | **0x187DE** | hold |
| Chop wind-up | 0x18808 | **0x18810** | **0x188AC** | 12, 30, 4, hold |
| Chop strike (held) | 0x1880C | 0x18884 | 0x18920 | hold (last node of the chop chain) |
| Throw | 0x18948 | **0x18954** | **0x18A66** | 10, 20, 10, 15, 6, 6, hold |
| Throw follow-through | 0x1894C | 0x189C8 | 0x18ADA | 15, 6, 6, hold (from throw node 4) |
| Catch | 0x18950 | 0x189F2 | 0x18B04 | 6, 6, hold (from throw node 5) |
| Axe (class 10 kind 7) | 0x247D4 | **0x247D8** | **0x24812** | 2, 2, 2, 2, loop; palette 18, geometry 0 |
| Scripted beast walk | 0x11FCC | **0x12024** | **0x12070** | 5 x 6, loop; palette 3, geometry 17 |
| Scripted beast stand | 0x11FD4 | **0x12098** | **0x120A8** | hold |

Unique chains to extract: 0x1858C, 0x18618, 0x186A8, 0x1871E, 0x18798,
0x187A4, 0x187B4, 0x187DE, 0x18810, 0x188AC, 0x18954, 0x18A66 (the strike,
follow-through and catch roots are tails of these), 0x247D8, 0x24812, 0x12024,
0x12070, 0x12098, 0x120A8. Not used by stage 1: direct roots 0x1858C/0x18618
via 0x1F40 (variant 4 script), 0x18810 direct, 0x18B78 (10, 10, hold; variant 4
entry).

---

## 6. Open points (UNKNOWN)

1. Phases 12 (step down) and 22 (slope), the axe's wall/surface rebound, the
   owner-gone axe path and the FF83D0 clear were not reached in S1..S3; the
   rules above come from the disassembly read here and agree with the
   transliteration. A trace that puts the player beyond the ledge left of
   x 2730 (or throws toward a wall) would confirm them.
2. Exact role of the ledge at x ~2730: which of probes B/D sends the walker
   back (both lead to phase 16, so behaviour is the same).
3. Class-10 state 4 (sets +4D, frees): no writer found in this area.
4. Native deviation to decide: an axe that misses its catch (owner hurt for
   10 frames as it passes, owner hurt early in the wait so its timer drops to
   1 (1.6 state 4), or the 200-frame timeout) flies on with no removal
   while the owner lives; the native game may remove it off camera and should
   record that as a deviation.
