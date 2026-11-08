# Willow's magic behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin` (sha256
0b66efe8...). Behaviour spec for the native rewrite (NATIVE_ENGINE_PLAN.md
sections 2 and 6). Continues `player.md` 3.5/3.6; HUD drawing is in `hud.md`
3.6. Arcade addresses are citations only. Tags: **VERIFIED** (disassembly read
here plus the transliteration `native/engine/src/actor8_magic.h`,
`actor12_magic.h`, `player_shots.h`, `magic_presentation.h`, or ROM table plus
builder tool), **SOURCE** (one source), **UNKNOWN**.

Units as player.md: world Y increases **upward**; speeds are 8.8 px/frame;
hit box = (xOffset, width, yOffset, height), box = [x-xOffset, x-xOffset+width]
x [y-yOffset, y-yOffset+height]. One update = one 60 Hz frame.

---

## 1. Hold counter, charge meter, sparks

| Item | Value | Tag |
|---|---|---|
| Press | button-1 rising edge (FF0E92 bit 4): fires the normal shot (player.md 3.5), clears the hold counter (player +0x31) and the release level FF0E94, enables sparks (player +0x3F) with spark countdown 10 | VERIFIED (C610) |
| Hold | while button 1 is held: counter +1/frame up to 127; sound **0x31** when it reaches 16; at 127 sound **0x44** every 16 frames (frame & 15 == 0) | VERIFIED (C636..C6BE) |
| Meter | FF8376 (u16) += inc[level] each frame while the counter is non-zero, capped; inc/cap per magic level 0..6 (ROM 0x4BFA / 0x4C08): 0x11F/0x8FFF, 0x14F/0xA7FF, 0x16F/0xB7FF, 0x17F/0xBFFF (levels 3-6). Madmartigan: 0x11F/0x8FFF. Runs after the player handler (4B0E); blocked/reset while FF83EA is set | VERIFIED |
| Release | button released: counter < 16 -> nothing (counter and level cleared). Otherwise FF0E94 = meter high byte H, counter cleared, sparks off, attack pending (+0x3C = 1) -> the spell is chosen and spawned in the attack handler (CB24). Meter reset to 0 with sound **0xF4** and the HUD bar redrawn empty | VERIFIED (C67E..C6AA, 4AF2) |
| Sparks | while enabled, countdown (player +0x44) -1/frame; at 0 reload with **0xC706**[counter>>4] = 10,6,5,4,3,2,2,1,1,... (denser as the charge builds) and spawn one spark if the class-2 pool has more than one free slot (FF97FE > 1) | VERIFIED (C6C0..C704) |

Charge spark actor (class 2, kind 0, routine **0xFB7C**): position = player
(x, y) + one of 16 random (dx,dy) word pairs from **0xFB90** (standing) or
**0xFBD0** (crouched, player +0x3E set); random 0..3 picks lifetime
**0xFC60** = 9, 16, 7, 24 frames and animation root from table **0xFC64**
(word offsets) -> roots **0xFC96** (9 nodes x 1 tick, then hold), **0xFD12**
(8 x 2 ticks), **0xFD82** (7 x 1 tick), **0xFDE6** (24-frame one; not walked
here). Stationary, no collision, removed at the end of its lifetime.
VERIFIED (FC10..FC92). Decoration: count and random offsets may be
approximated (fidelity contract).

### Spell selection at release
tier = **0xCB08**[(H+1) >> 3] (28 bytes: 0,0,0,0,0, 1,1,1,1,1, 2,2,2,2, 3,3,3,3, 4,4,4, 5,5, 6,6,6,6,6);
spell = **0xCB4C**[magicLevel*8 + tier] (table in player.md 3.6). VERIFIED (CB24).
Hold time (meter steps) needed for each spell, from those tables:

| Level | spells reached (spell@frames held) |
|---|---|
| 0 | 0@1, 1@35, 2@71, 3@100 (no charged spell) |
| 1 | 1@1, 2@30, 3@61, 4@85, **7**@110 |
| 2 | 2@1, 3@28, 4@56, 5@78, **7**@100, **8**@117 |
| 3 | 3@1, 4@27, 5@53, 6@75, **7**@96, **8**@112, **9**@123 |
| 4..6 | level+0@1, +1@27, +2@53, +3@75, +4@96, +5@112, +6@123 (L6: 12@123) |

(Releases under 16 frames never cast.) Derived; not trace-checked.

## 2. Spawning (CB24..CCEE)

Spell -> class-8 kind: table **0xCC50** = 1,1,1,1,1,1,1,3,7,4,6,5,8 for spells
0..12. Jump table **0xCB84**. VERIFIED.

| Spell | Kind | Spawn rule | Tag |
|---|---|---|---|
| 0-6 | 1 (normal shot, variant = spell) | refused if 4 normal shots alive (FFF99D) | VERIFIED |
| 7, 8, 10 | 3, 7, 6 | one actor at player x, y+8 (y+0 crouched), facing = player facing & 1 | VERIFIED |
| 9 | 4 | same, but **refused** while FF0E58 or FF83CF is set (FF83CF is a scene lock that also stops the clock; FF0E58 UNKNOWN) | VERIFIED |
| 11 | 5 (variant 0 = generator) x2 | at player x, y; first faces the player's way, second the opposite way. No muzzle effect | VERIFIED (CC60) |
| 12 | 8 radial class-12 pieces (kind 4, directions 7..0) then one kind 8 | at player x, y (y-8 crouched); stops adding pieces when the class-12 pool is full; kind 8 skipped if class 8 is full | VERIFIED (CC90) |

Spells 0..10 also spawn a muzzle effect (class 2, kind 1, facing = player
facing) through 29D0. A spell does nothing if the class-8 pool (20 slots) is
full. Spells 7-12 have no count limit of their own. VERIFIED.

## 3. Class-8 projectile mechanics (shared)

- Pool: 20 slots x 64 bytes at FFC2C6, dispatched every frame by kind (+0x20)
  through table **0xE71C**: kind 0 E740, 1 EC84, 2 EDFA, **3 EEC6, 4 F45C,
  5 F516, 6 F4DC, 7 F994, 8 FADC**. VERIFIED (E6F4).
- Record (offsets used here): +0 active (1 live, 2 spent/no collision), +2
  state (0 init, 2 run, 4 end, 6 remove), +3 phase, +6/+8 x.fraction, +0A/+0C
  y.fraction, +0F facing (0 right, 1 left), +13 floor-probe set, +14 animation
  node pointer, +18 motion-table pointer, +1C ticks left, +21 variant, +22
  timer, +24 landed, +2A power, +2B last power, +2C vertical speed, +30..+36
  hit box words. VERIFIED (field writes in the routines below).
- Animation: node = +0 piece count (bit 15: long pieces), +2 attribute (high
  byte set = no piece list), +4 selector (bit 15: next pointer is a long),
  +8 ticks (0xFFFF hold); next node follows the piece list. Setter **0x1F30**:
  root = table + signed word[facing]; **0x1F40**: root given directly; both
  load ticks from node+8. Stepper **0x1F4C**: ticks-1, advance on 0. VERIFIED.
- Motion **0x208C**: x += table[facing].dx*256, y += table[facing].dy*256
  (8.8 into 16.16). Off-screen check **0x2212**, run when ((frame + slot) & 3)
  == 0: removed if x < camX-32 or x > camX+416, or y < camY-16 or y > camY+272.
  **0x2244** publishes the actor for drawing and collision. VERIFIED / SOURCE.
- Damage: rule table **0x3A0AA** (per attack kind, 4 enemy target classes;
  enemy class per kind from 0x3A07A) and **0x3A4F6** via 0x3A4D6 (bosses, 3
  classes). Policies (combat_contact.h names): VERIFIED (ROM decode with the
  build_combat_data.py mapping)

| Kind | vs enemy classes 0/1/2/3 | vs boss classes 0/1/2 |
|---|---|---|
| 3, 7 | PIERCE / PIERCE_SHIELD / PIERCE_BASE / DEFLECT | ABSORB / PIERCE / PIERCE |
| 4 | BLAST / BLAST / BLAST_BASE / STOP | IGNORE / BLAST / IGNORE |
| 5, 8 | ONCE / ONCE / ONCE_BASE / DEFLECT | IGNORE / ONCE / ONCE (8: IGNORE / ONCE / IGNORE) |
| 6 | STOP x4 (never published, no effect) | - |

  PIERCE: target loses min(power, energy); shot power -= target energy (the
  shot keeps going while power > 0); a target already in its hit state
  (active 2) zeroes the shot's power. ONCE: target loses `power` once (until
  it leaves its hit state); the spell continues. BLAST: no box test; every
  admitted target not already hit with energy <= power dies at once and gives
  its reward. DEFLECT: shot ends with its impact animation. ABSORB: shot spent.
  `_BASE`: only targets with variant 0. SOURCE (combat_contact.h; policy
  routines 0x3A106..0x3A5FE not re-read).

## 4. The charged spells

### Spell 7: kind 3, piercing bolt (EEC6..F2xx)
- Init: sound **0x21**; motion table **0xED0BC** = (+0x0600, 0) right,
  (-0x0600, 0) left: **6 px/frame horizontal**, no gravity; hit box
  **(10,20,20,40)**; power **6** (pierce). VERIFIED.
- Run: off-screen check, power watch (power 0 -> spent, end phase 0), move,
  animate, wall test 0x3744 (a wall tile in the moving direction -> x snapped
  to the wall face, spent, end phase 2), publish. VERIFIED.
- End (state 4): 12 frames of end animation, **still moving 6 px/frame**,
  then removed. Phase 0 (power used up): table **0xF434** -> 0xF438 R / 0xF448 L;
  phase 2 (wall): table **0xEFB8** -> 0xF2EC R / 0xF320 L (4 nodes x 3 ticks,
  then hold). VERIFIED (EF66..EFB0).
- Flight animation: table **0xF3F8** -> roots **0xF3FC** (R) / **0xF418** (L):
  2 nodes x 4 ticks, looping (6 pieces each). VERIFIED.
- Lifetime: until power is spent, a wall, or off-screen. VERIFIED.

### Spell 8: kind 7, rising piercing bolt (F994..FA86)
- Init: sound **0x24**; motion table **0xED09C** = (+0x0400,0)/(-0x0400,0):
  4 px/frame base; hit box **(10,20,30,60)**; power **8** (pierce); speed
  +0x2C = 0. Animation table **0xFA88** -> **0xFA8C** R / **0xFAB4** L: 3 nodes x
  2 ticks looping (12 pieces). VERIFIED.
- Each frame after the base move (FA20): s = min(s+2, 768); x += s (forward,
  i.e. minus when facing left); s = min(s+16, 1792); y += s (upward). From
  rest s grows 18/frame until the horizontal part caps after ~43 frames, then
  the bolt moves 4+3 px/frame forward and 3.06 px/frame upward: a forward
  bolt that curves up and accelerates. VERIFIED.
- No wall test. Power 0 -> removed at once (no end animation); off-screen ->
  removed. VERIFIED.

### Spell 9: kind 4, flash (F45C..F4CE)
- Init: power **8**, image root **0xF4D0** (a node with no pieces: invisible),
  no sound of its own. VERIFIED.
- Phase 0: timer 3, sets FF8349 = 0xFF -> the next interrupt fills scroll1 rows
  2..27 (48 columns) with solid white glyph 0x7025 (palette 0 pen 8 = white).
  After 3 frames: FF8349 = 1 -> HUD rebuilt (hud.md 4); 5 more frames; then
  removed. About 9 frames total, published every frame. VERIFIED (66B0, 6714).
- Effect: BLAST each published frame: every admitted enemy with energy <= 8
  (not already hit) is killed with its reward; stronger enemies and most boss
  parts are unaffected. What "admitted" means (on-screen lane? flag) is
  UNKNOWN; read the target-admission code before writing it.

### Spell 10: kind 6, time stop (F4DC, 690C)
- The actor plays sound **0x23** once (only if latch FF83D6 is clear), sets
  FF83D6 = 0xFF and is removed. It never collides. VERIFIED.
- Latch routine 690C (once per frame, in the interrupt): first frame: player
  invulnerable (player +0x50 = 255), player +0 = 2, +0x4E = 4, hurt timers
  cleared; **freeze** FF83D2 = 1 (stops ordinary enemies, moving surfaces and
  the stage clock); a row-scroll wave is written to 0x922000 from ROM
  **0x6A46** (32 entries >> 4, odd rows 0, repeated twice). Then each frame:
  invulnerability 255, wave offset += 2, row-scroll on, until the offset
  reaches 256 (**128 frames**); then freeze off, latch clear, invulnerability
  1. VERIFIED (690C..69E2; magic_presentation.h).
- Whether bosses also freeze: UNKNOWN (check boss routines for FF83D2).
- Native: the wave is a per-line horizontal scroll (Genesis HSCROLL per line).

### Spell 11: kind 5, two fire walkers (F516..F6DA)
- Generator (variant 0, invisible, no collision): sound **0x22** at start;
  timer 1, then every 8 frames: x += **+32** (R) / **-32** (L) (first word of
  0xF564 / 0xF570; the rest of those tables is unused), and spawn a piece at
  the generator's new x, y with variant 2, 2, 4, 4, 6, 6 (phases 0..10), if a
  class-8 slot is free. After 6 steps it is removed. Pieces land at +32,
  +64, ..., +192 px, 8 frames apart. Two generators (both directions) run at
  once. VERIFIED (F53E..F626).
- Piece (variants 2/4/6, F628): lifetime **54** frames; power **10** (ONCE);
  floor-probe set 1; hit box by variant from **0xF684**+v*4:
  v2 **(16,32,16,32)**, v4 **(24,48,32,48)**, v6 **(32,64,48,64)**. VERIFIED.
- Piece motion: falls from the spawn height: speed starts 0, +48/frame (cap
  1792) until a floor probe hits; on alternate frames it may land on a
  platform surface (kind 0 whose eligibility byte at 0x7038 allows it, within
  dy 12..20, dx -14..62: y = platform y + 20). Once landed it stays. SOURCE
  (actor8_magic.h; routines 6FFE / 3C48 / 4950).
- Animation: variant table 0xF67C (+word[v]) -> v2 0xF8A4: roots 0xF8A8 R /
  0xF91E L; v4 0xF7C0: 0xF7C4 / 0xF834; v6 0xF6DC: 0xF6E0 / 0xF750. Each:
  8 nodes x 6 ticks then hold (grows over 48 frames); 4, 9, 16 pieces per
  node for v2/v4/v6. VERIFIED (node walk). What it looks like (flame pillar?)
  is UNKNOWN: render the poses.

### Spell 12: kind 8 blast + 8 radial pieces (FADC..FB44, class 12)
- Kind 8: sound **0x25**; timer **62** frames; power **20** (ONCE); hit box
  **(80,160,80,160)** = a 160x160 square centred on the spawn point; does not
  move; image 0xF4D0 (invisible). Each frame it also **cancels enemy
  projectiles** overlapping its box (one of two projectile lanes per frame,
  709C even / 70AE odd frames): action per enemy-shot kind from **0x7102**
  (32 bytes): 0 -> destroyed with its end animation, 6 -> removed, 0xFF ->
  immune (kinds 8, 21, 22); shot bounds from pose geometry 0x3B0D2. VERIFIED.
- Radial pieces (class 12 kind 4, decoration, no damage): motion **0xED2C4**
  per direction n (dx,dy in 8.8): 0 (0,+1708) up, 1 (+1376,+1360), 2
  (+2048,0) right, 3 (+1376,-1360), 4 (0,-1708) down, 5 (-1376,-1360), 6
  (-2048,0), 7 (-1376,+1360). Fractions cleared at start. 12 frames moving
  (root **0x34C96**: 6 nodes x 4 ticks), 10 frames still, then 40 frames on
  root **0x34CE2**[n] (8 nodes x 5 ticks then hold), then removed: 62 frames,
  matching kind 8. VERIFIED (ROM; actor12_magic.h; build_magic_radial.py).

## 5. Normal shots and muzzle effect (for completeness)
Spells 0-6 = kind 1, variant = spell; speed, power, hit box, 64-frame life in
player.md 3.5 (animation table 0xECFE -> roots 0xEFBC..). Muzzle effect: class
2 kind 1 (table 0xFB70 entry 1 = 0xFE3E). SOURCE.

## 6. Raising the magic level (FF8366, 0..6)

- **Shops** are the normal source. Every shop's first item is the next level:
  upgrade table **0x33E0A**[min(level+1, 6)] = item 4, 14, 5, 3, 2, 13 for
  levels 1..6; buying sets the level (absolute) and redraws the charge bar
  (longer bar). Prices (BCD gold, 0x34874 + item*4): L1 **850**, L2 **2000**,
  L3 **4500**, L4 **8000**, L5 **10000**, L6 **15000**. At level 6 the item
  shows as already bought. VERIFIED (ROM tables) / SOURCE (shop.h flow,
  33DBE). NG+ doubles prices (AGENTS.md).
- Madmartigan's sword (FF8367, 0..4): table 0x33E12 -> items 25, 0, 9, 11
  (2000, 4000, 6000, 10000). SOURCE.
- DIP "magic continue" (DSW-B bit 7 -> FFF9A6): routine 2E1C, run from A956
  when the stage index advances, raises the levels to stage minimums: stage
  index 0/1: magic >= 1; 2: sword >= 1; 3: magic >= 3; 4: sword >= 2; 5/6:
  magic >= 5 and sword >= 3. When A936/A956 runs (continue? demo?) is UNKNOWN.
- Debug DIP (FF828F) with a held input sets magic 6 / sword 4 (BF12); the
  attract demo sets magic 1 (8D7E). Not gameplay.
- **Death and continue keep the level**: the only writers of FF8366 are the
  new-game clear (2D18), shop purchases, 2E1C, the debug and demo paths, and
  the two-player block exchange. VERIFIED (all stores found by search).

## 7. Native state and data to extract
- Variables: hold counter, meter (u16), release level, sparks flag/countdown,
  magic level, sword level, the freeze flag + wave offset (spell 10), flash
  latch (spell 9), class-8 pool (20) and the class-12 radial pieces.
- Offline data: tables 0x4BFA, 0x4C08, 0xCB08, 0xCB4C, 0xCC50, 0xED0BC,
  0xED09C, 0xF564/F570 (first words), 0xF684 boxes, 0x7038, 0x7102,
  0xED2C4, 0xC706, 0xFB90, 0xFBD0, 0xFC60, 0x6A46 wave, and all animation
  roots listed (tools: `native/engine/tools/build_magic_projectiles.py`,
  `build_magic_radial.py`, `build_actor2_spark.py` already walk them).

## 8. Open questions (UNKNOWN)
1. Spell 9 "admitted" target rule, and FF0E58 (spell-9 refusal).
2. Whether spell 10's freeze stops bosses and boss projectiles.
3. Exact policy routines 0x3A106..0x3A5FE (rules summarised from the
   transliteration only).
4. Visual identity of each spell (render the listed poses).
5. When the "magic continue" stage minimums are applied (A936 caller).
6. Not trace-checked: all timings above. Capture headless traces for each
   spell (class-8 slots at FFC2C6: kind +0x20, x +6, y +0x0A, state +2,
   power +0x2A) at magic levels 1..6 with long holds.
