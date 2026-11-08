# General Kael (stage-6 mid-boss): behaviour contract

Class 4 kind 24, routine **0x1DE3E..0x1ECF5** (pose tables 0x1ECF6..0x1ED29,
nodes to 0x1F29E). Extends `stage6.md` 6.1-6.3; **this file supersedes
stage6.md 6.2** (see section 9 for its corrections). Tags: VERIFIED = code and
trace agree (run D4b, `S/ramD4b/`, Kael = class-4 slot **49**, record
**FFB646**; `S/kael_states.txt`; sounds `S/snd_d4bs.txt`), SOURCE = read from
the code only, UNKNOWN = not established. All speeds are in 1/256 px per
update; **y grows upwards** (bridge floor: Kael y 3160, player y 3152).
Camera is fixed at (0, 3072) in the arena, so "camX + n" below is simply n.

Shared helpers used (behaviour, not code to port): rand = 0x1D8E (shared
LFSR, returns a byte; Kael always uses `rand & 15`); 0x1F30 = set pose root
from table[facing], 0x1F4C = step animation; 0x3B0A = ground probe (probe
selector +13 = 5, offsets 0x3B8E: x +-12, y -32) returning "landed"; 0x2244 =
draw / on-screen flag +1; 0x786A = sound; 0x788A = music.

## 1. Record fields

| Field | Meaning | Tag |
|---|---|---|
| +0 | 1 = collidable (can be hit, hurts on touch), 2 = immune (after a hit, during the parry, in state 4) | VERIFIED |
| +1 | on-screen flag (set by 0x2244 while x - camX + 32 <= 448) | SOURCE |
| +2 | state: 0 wake, 2 fight, 4 defeat, 6 remove (0x2A36) | VERIFIED |
| +3 / +4 / +5 | phase / sub-phase / sub-sub (air slash) | VERIFIED |
| +6.l / +0A.l | x / y, 16.16 fixed | VERIFIED |
| +0F | facing: 0 = right, 1 = left | VERIFIED |
| +13 | ground-probe selector 5 | VERIFIED |
| +18.l | motion table **0xED080**: word[0] = +640, word[2] = -640 (walk 2.5 px/update) | VERIFIED |
| +20 | kind 24 | VERIFIED |
| +22 | phase timer (byte) | VERIFIED |
| +2A / +2B | energy / last-seen energy (init **128**) | VERIFIED |
| +31 | current column class `d = ((x - camX) >> 5) & 14` (0x1ECE2): x-camX in [32d, 32d+64) | VERIFIED |
| +32 | target column class (from the decision byte's high nibble) | VERIFIED |
| +33 | "walk pose set" flag (phases 0/2) | SOURCE |
| +34.l | pointer to the chosen decision row | VERIFIED (0x1ECD2 in dumps) |
| +38 | phase-2 walk direction index into +18 (0 = right, 1 = left) | VERIFIED |
| +3A | vy (up positive) | VERIFIED |
| +3C | parry push deceleration (applied to +44) | VERIFIED |
| +3E | invulnerability timer after a hit (30) | VERIFIED |
| +40 | "ground/air slash in progress" flag (phases 6, 10) | VERIFIED |
| +41 | "airborne" flag (phases 8, 12, 14) | VERIFIED |
| +42 | "parrying" flag (phase 16) | VERIFIED |
| +43 | phase-14 update counter (unused otherwise) | VERIFIED |
| +44 | vx (phases 10, 12, 14); in phase 16 the **player's** push speed | VERIFIED |
| +46 | phase-14 no-ground-check window (48) | VERIFIED |
| +47 | parry-scan cooldown (8) | VERIFIED |

Guard (parry) is possible only while +40, +41 and +42 are all 0. Note: +41 is
not cleared on landing in phase 8 (only by phases 4/6/12/14 entry), so he cannot
parry while standing on the ledge. VERIFIED (dumps 2728..2756: +41 = 1).

## 2. State 0: wake (0x1DE52, phases 0-6)

| Phase | Behaviour | Tag |
|---|---|---|
| 0 init (0x1DE6A) | energy 128 (+2A/+2B), clear +40/+41/+42/+33/+3E/+47, probe 5, motion 0xED080, root 0x1ECF6 (stand) | VERIFIED |
| 2 (0x1DEA8) | wait until +1 (on screen). If **FF838A** != 0 (the dialogue was already shown once, e.g. after a death in the arena; it is set here on the first wake) -> phase 6 directly (no music, no text). Else: music **0x0E** (0x788A), timer 96, FF838A = FF83E6 = FF83D0 = FF83E9 = FF82A6 = FF8E58 = 1, message **0x0447** style 0x0306 (text in stage6.md 6.1) -> phase 4 | VERIFIED |
| 4 (0x1DF04) | every update FF83EA = FF83E9 = 1 (player frozen, clock stopped); wait FF82A6 = 0 (text done), then 96 updates; then FF83E9 = FF83EA = 0, clear the window (0x6770) -> phase 6 | VERIFIED (D4b 1994..2388) |
| 6 (0x1DF2E) | state 2 phase 0; bar FF83CC = **127** (bar maximum; energy 128 shows as full), draw bar (0x4AC6) | VERIFIED |

## 3. State 2: the fight (0x1DF40)

Each update: run the phase (jump table 0x1DF58), then clamp x to
[camX+16, camX+368] (0x1E762: x < 16 -> 16; x >= 368 -> 368), then the hit /
guard routine 0x1E886 (section 5), then draw. VERIFIED.

### 3.1 Decision ("decide", 0x1EC46)
1. **Proximity override** (0x1EB9E), dx = Kael.x - player.x:
   - -80 <= dx < 80: if column d >= 10 -> **phase 8**; else phase =
     table 0x1EBE4[rand & 15].
   - else -112 <= dx < 112: if d >= 10 -> **phase 8**; else table
     0x1EC36[rand & 15].
   - If the drawn phase is 14, +32 = 10 (leap to x 352). Otherwise +32 kept.
   - else no override.

   | Table | 16 entries | Summary | Tag |
   |---|---|---|---|
   | 0x1EBE4 (near) | 6 14 4 14 6 6 6 6 6 6 6 6 4 14 6 14 | slash 6: 10/16, leap 14: 4/16, stand 4: 2/16 | VERIFIED (ROM; repeated phase 6 at D4b 4860..5160) |
   | 0x1EC36 (mid) | 6 14 4 14 6 6 6 4 6 6 6 6 4 14 4 14 | 6: 8/16, 14: 4/16, 4: 4/16 | SOURCE |
2. **Otherwise** row = 0x1EC82 + word[0x1EC82 + d]; byte b = row[rand & 15];
   phase = b & 14, +32 = b >> 4. Rank does **not** enter the decision.

| d (x - camX) | Row | Bytes | Outcomes (phase, target column: count/16) |
|---|---|---|---|
| 0, 2 (0..127) | 0x1EC92 | 42 6E 42 8E 62 8E 42 04 62 42 8E 42 6E 04 8E 42 | walk->4: 6, walk->6: 2, stand: 2, leap->6: 2, leap->8: 4 |
| 4 (128..191) | 0x1ECA2 | AE 02 82 04 02 62 8E 62 62 8E 02 62 04 AE 02 82 | walk->0: 4, walk->6: 4, walk->8: 2, stand: 2, leap->8: 2, leap->10: 2 |
| 6 (192..255) | 0x1ECB2 | 02 AE 42 82 0E 42 82 A2 42 82 A2 02 82 AE 0E 42 | walk->0: 2, walk->4: 4, walk->8: 4, walk->10: 2, leap->0: 2, leap->10: 2 |
| 8 (256..319) | 0x1ECC2 | 0E 62 A2 42 A2 62 4E 04 62 4E 0E 62 A2 42 04 A2 | walk->4: 2, walk->6: 4, walk->10: 4, stand: 2, leap->0: 2, leap->4: 2 |
| 10+ (320..) | 0x1ECD2 | 82 08 82 08 6E 82 08 82 08 6E 08 08 08 08 08 08 | walk->8: 4, **ledge jump 8: 10**, leap->6: 2 |

(walk = phase 2, stand = phase 4, leap = phase 14.) VERIFIED (ROM; +34 =
0x1ECD2 and +32 = 10 in dumps) / SOURCE (per-entry odds).

### 3.2 Phases

| Phase | Behaviour | Pose root (0x1ECF6 + 4n) | Tag |
|---|---|---|---|
| **0** approach (0x1DF6A) | first fight phase only: timer **13**, face the player, walk toward the facing at **640** (2.5 px); every 8 updates ((FF8019 + loop index) & 7 == 0) re-face (pose restarts if facing changed); at 0 -> decide | 0x1ECFA walk (4 x 8 ticks, loop) | VERIFIED (2390..2404: x 304 -> 274) |
| **2** walk to column (0x1DFC2) | timer 16, clear +40/+41; +38 = 0 (right) if +32 > +31 else 1 (left); face the player (so he may walk backwards); each update: re-face every 8, x += **+-640** by +38, animate; when +31 == +32 -> decide; when the 16-update timer ends: proximity override (3.1 step 1), else timer 16 again | 0x1ECFA walk | VERIFIED (2404..2424: 274 -> 321) |
| **4** stand (0x1E054) | timer **16**, face the player, re-face every 4 updates; at 0 -> decide | 0x1ECF6 stand | VERIFIED |
| **6** ground slash (0x1E0AA) | vy = 0; face the player once; wind-up **30**; then +40 = 1, **slash** (5.1) and 16 updates of the slash pose (animated); then decide. No re-facing | 0x1ECFE wind-up (8 ticks + hold), then 0x1ED02 slash | VERIFIED (2524: 30 + 16; wave at 4490) |
| **8** ledge jump (0x1E112) | sub 0: timer 10, face, stand. sub 2: after 10 -> +41 = 1, +5 = 0, **vy = 1792**. sub 4 (rising, no x motion): vy -= **48**, y += vy; air-slash check (below); when vy < 0 -> sub 6. sub 6: vy -= 48, y += vy, ground probe; landed -> sound **0x2B**, stand, timer 30 (sub 8). sub 8: after 30 -> **phase 10**, with +44 = 0 if \|Kael.x - player.x\| < 32, else +44 = (player.x - Kael.x - 48) * 4 | 0x1ED0E jump; air slash 0x1ED12 / 0x1ED16; 0x1ECF6 on the ledge | VERIFIED (2672..2758: vy 1792 -> 16 in 37 updates, y 3160 -> 3287, lands on the ledge at **y 3279** after 8 updates) |
| air slash (+5, phases 8 and 14) | +5 = 0: when \|Kael.y - player.y\| < 24 and \|dx\| < 120 (phase 14 also needs y >= camY+56, always true here) -> +5 = 2: face, timer **10** (phase 14: **5**), wind-up pose; then +5 = 4: **slash** (5.1), timer 30, slash pose; +5 = 4/6 idle. Phase 14 re-faces every update while +5 = 0. In practice it fires on the first airborne update when the player is on the floor within 120 px | 0x1ED12 (10 ticks + hold), 0x1ED16 | VERIFIED (2682 +5 = 2, 2692 slash; 3072 phase 14) |
| **10** drop from the ledge (0x1E256) | sub 0: +40 = 1, face, **vy = 832**. sub 2: re-face every 8; x += +44; y -= 1; vy -= **32**; y += vy; vy < 0 -> sub 4, timer 14. sub 4: re-face every 8; same motion with vy -= **48**; after 14 -> sub 6 (no ground check yet, so he falls through the ledge). sub 6: same motion + ground probe; landed -> sound **0x2B**, **phase 4**. Every update of phase 10: if \|Kael.x - player.x\| < 24 and \|Kael.y - player.y\| < 32 -> **phase 12** | 0x1ED0E hop, 0x1ED1A, then 0x1ED1E plunge (sub 6) | VERIFIED (2922..2980: vy 832 -> -32 in 27 updates, peak y 3298; phase 12 at y 3184) |
| **12** hop away (0x1E33A) | sub 0: +41 = 1, **vy = 1152**, vx = **+512** if Kael.x < camX+320 else **-512** (see quirk), jump pose. sub 2: re-face every 8; x += vx, vy -= **80**, y += vy; vy < 0 -> sub 4. sub 4: same; when y <= camY+128 (3200) -> sub 6. sub 6: same + ground probe; landed -> clear +40/+41, sound **0x2B**, stand, 30 updates (sub 8) -> decide | 0x1ED0E | VERIFIED (2980..3042: x 342 -> 278, 2 px/update; 30 at the end) |
| **14** leap to a column (0x1E42C) | sub 0: timer 10, face, stand. sub 2: after 10 -> +41 = 1, +5 = 0, +46 = **48**, target x = camX + table 0x1E4A8[+32/2] = **32, 96, 160, 224, 288, 352, 416, 480**, **vx = (target - x) * 4**, **vy = 1664**. sub 4: air slash check; x += vx; vy -= **56**; y += vy; for the first 48 updates no ground probe, then probe; landed -> clear +40/+41, face, sound **0x2B**, stand, timer 30 (sub 6) -> decide | 0x1ED0E, air slash 0x1ED12 / 0x1ED16 | VERIFIED (3070: target 352 from x 278 -> vx 296; 60 airborne updates, peak y 3253, landed x 346) |
| **16** parry (0x1E5A2) | see section 4 | 0x1ED06 guard, 0x1ED0A push | VERIFIED |

Quirk (0x1E372): phase 12 reads the word at **Kael's record - 0x7C08**
(FF3A3E for slot 49) where camera X (FF8408) was clearly meant. It was 0 in
every D4b dump, as is camera X in the arena: natively use camera X. VERIFIED.

Ledge: in D4b every phase-8 jump started at x 342..347 (phase 8 only occurs
for column d >= 10, x >= 320) and landed on terrain at **y 3279**. Whether the
ledge spans other x is a terrain question (stage6.md 2.2). VERIFIED (x 342..347).

### 3.3 Phase flow summary
decide -> {2, 4, 6, 8, 14} (16 via the guard). 0 -> decide; 2 -> decide (or
override); 4 -> decide; 6 -> decide; 8 -> **10** -> 4 (landed) or **12** (near
the player) -> decide; 14 -> decide; 16 -> **6**. Typical D4b cycle at the
right edge: 14 (leap to 352) -> 8 -> 10 -> 12 -> 4/14. VERIFIED.

### 3.4 Durations (updates)
| Phase | Duration | Tag |
|---|---|---|
| 0 | 13 | VERIFIED |
| 2 | until the column is reached (2.5 px/update) | VERIFIED |
| 4 | 16 | VERIFIED |
| 6 | 30 + 16 | VERIFIED |
| 8 | 10 + 38 rise + fall to the ledge (8) + 30 | VERIFIED |
| 10 | 27 rise + 14 + fall (14-20) | VERIFIED |
| 12 | ~15 rise + fall + 30 | VERIFIED |
| 14 | 10 + ~60 airborne + 30 | VERIFIED |
| 16 | 8 + 8 + 20, then phase 6 (46) | VERIFIED |

## 4. Guard / parry (0x1E958, called each update before damage)

**Scan conditions**: +47 cooldown (if non-zero: decrement; scan only when it
reaches 0); +40 = +41 = +42 = 0; Kael faces the player (re-faced here) and the
player faces Kael (player facing FF9855 bit 0). VERIFIED (D4b 4422: player at
x 360 facing left, Kael at 282 facing right).

**Scan**: the player's sword/shot pool FFC2C6 (20 x 64 bytes); for each record
with **+0 == 2**:
1. +47 = **8** (cooldown, set even if the roll fails).
2. chance table 0x1EA20 row = (rank FF8346 high byte >> 5) & 7; +8 rows (offset
   128) if the record's variant +21 != 0 (charged/magic). Byte [rand & 15]:
   0 -> try the next record; 1 -> continue.

   | Rank row | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
   |---|---|---|---|---|---|---|---|---|
   | plain (variant 0) /16 | 2 | 4 | 7 | 10 | 11 | 12 | 13 | 14 |
   | charged (variant != 0) /16 | 5 | 7 | 9 | 11 | 12 | 13 | 14 | 11 |
   SOURCE (ROM 0x1EA20..0x1EB1F). D4b rank high byte 0x34..0x3B -> row 1.
3. point = record (x, y) + offsets 0x1EB4E[player form FF8367][player facing]
   (two x words summed, two y words summed): forms 0/1: facing right (+20,
   +16), left (-60, +16); forms 2/3: (+48, +16) / (-80, +16); form 4: (+68,
   +16) / (-80, +16). SOURCE.
4. if \|point.x - Kael.x\| <= 40 and \|point.y - Kael.y\| <= 32 -> phase 16
   sub 0 (the scan stops).

**Phase 16** (0x1E5A2):
| Sub | Behaviour | Tag |
|---|---|---|
| 0 | **+0 = 2** (immune: the player's hit does no damage), +42 = 1, clear +33/+40/+41, sound **0x28**, spark (class 12 kind 28) at (Kael.x **+16** facing right / **-16** facing left, Kael.y), face the player; guard pose 0x1ED06; timer 8 | VERIFIED (4422; 2 x 0x28 in D4bs for 2 parries) |
| 2 | after 8: **FF83EA = 2** (player control held), push speed +44 = **1408** away from Kael (+ when Kael faces right), +3C = **-64** (decel, sign opposite); push pose 0x1ED0A, timer 8 | VERIFIED (4432) |
| 4 | 8 updates: +44 += +3C, then **player x += +44** (Kael moves the player directly; speeds 1344..896, about 35 px total); at the end FF83EA = 0, timer 20 | VERIFIED (player 360 -> 368, stopped by the arena edge) |
| 6 | after 20: +0 = 1, +42 = 0, **phase 6** (counter slash) | VERIFIED (4460) |

Order caveat: a hit that the collision pass applied in the same update the guard
fired is still counted on the next update (the damage compare runs whenever
+3E = 0 and the guard is not active), so a parry predicts the incoming swing;
it does not cancel energy already lost. SOURCE.

## 5. Kael's attacks

### 5.1 Slash = sword wave (0x1E842 -> class 10 kind 13, 0x25604)
Every slash (phase 6, air slash in phases 8/14): sound **0x91**, then if a
class-10 record is free: kind 13, facing = Kael's, x = Kael.x **+40** (right)
/ **-40** (left), y = Kael.y, owner +26 = Kael. No record -> sound only.
VERIFIED (26 waves and 26 x 0x91 in D4b/D4bs).

| Wave step | Behaviour | Tag |
|---|---|---|
| state 0 (spawn update, not drawn) | timer 14, root table 0x256A0[facing]; speed +30 = **+-768**, accel +32 = **+-128** (forward) | VERIFIED |
| state 2 (14 updates) | speed += accel; x += speed (3 px rising to 10 px/update, **~95 px** total); y += owner's current vy (+3A) when non-zero (air-slash waves rise/fall with Kael) | VERIFIED (4490..4504: x 322 -> 417) |
| state 4 (10 updates) | stays, animates (nodes 0x256A4 -> 0x256B0 -> 0x256C6 -> 0x256DC); not drawn on its last update | VERIFIED |
| state 6 | freed (0x2B6A) | VERIFIED |
Damage: class-10 selector 0x3AABC[13] = 0 -> plain box handler, **1 point**
(combat.md 2.1). Box geometry: other agent's node work (0x3B0D2 table). SOURCE.

### 5.2 Body contact
Class-4 selector 0x3A740[24] = 0 -> plain box, 1 point, with the body box of the
current pose (table below). Note the wide ground-slash pose (56 px). SOURCE.

| Root (0x1ECF6+4n) | Pose | Nodes (right / left) | Body = shot box (xOff, w, yOff, h) |
|---|---|---|---|
| 0x1ECF6 | stand | 0x1ED2A / 0x1ED36 hold | (16,28,32,56) |
| 0x1ECFA | walk | 4 x 8 ticks loop | (10,20,..), (16,32,..), (14,22,..), (10,20,..) all y (32,56) |
| 0x1ECFE | ground wind-up | 0x1EE4E 8 ticks -> 0x1EE7A hold | (24,28,32,56) -> (16,56,32,56) |
| 0x1ED02 | ground slash | 0x1EE7A / 0x1EED0 hold | (16,56,32,56) |
| 0x1ED06 | parry guard | 0x1EEFA / 0x1EF54 | (16,34,32,56) |
| 0x1ED0A | parry push | 0x1EF22 / 0x1EF7C | (8,48,32,56) |
| 0x1ED0E | jump | 0x1EFAE / 0x1F034 | (16,26,22,54) |
| 0x1ED12 | air wind-up | 0x1EFDA 10 ticks -> 0x1F006 | (12,20,16,48) -> (16,40,16,48) |
| 0x1ED16 | air slash | 0x1F006 / 0x1F08C | (16,40,16,48) |
| 0x1ED1A | drop hop | 0x1F0BA / 0x1F120 | (16,24,26,50) |
| 0x1ED1E | plunge | 0x1F0EC / 0x1F152 | (16,32,8,36) |
| 0x1ED22 | defeat flash | 0x1F186 / 0x1F212 | body (18,36,20,32), shot (18,36,24,48) |
| 0x1ED26 | collapse | 3 nodes 8, 8, hold | as 0x1ED22 |
Palette 24 throughout. VERIFIED (ROM tables 0x3AC4A / 0x3AE62 via node +6).

## 6. Damage taken (0x1E886, each fight update)

| Item | Value | Tag |
|---|---|---|
| Invulnerability | if +3E != 0: +3E -= 1; on reaching 0: +0 = 1; no damage or guard processing while counting | VERIFIED (+3E 30 -> 0) |
| Detection | after the guard check: +2A != +2B -> hit | VERIFIED |
| Hit | +0 = 2, **+3E = 30**, +2B = +2A, FF83CC = energy, bar redraw (0x4AC6), sound **0xA2**, palette job (0x2C86): colours **0xE6E20** -> palette **24**, 13 steps x 5 updates (+3B = 13, +3A = 5) | VERIFIED (12 x 0xA2 = 11 hits + the kill) / SOURCE (job fields) |
| Reaction | **none**: the phase continues (no stagger, no knockback) | VERIFIED (hits at 2504 in phase 14, 2536 in the phase-6 wind-up) |
| Energy loss | 1 per player sword hit (D4b 128 -> 117 over 11 hits); other weapons per madmartigan.md / magic.md | VERIFIED (sword) |
| Energy 0 | state 4 phase 0, **FF83E9 = 1** (clock stops); registered only when +3E has run out | VERIFIED (energy 0 at 5592, +3E = 17 -> state 4 at 5610) |

The bar starts at 127 while energy is 128, so the first hit does not move it.

## 7. Defeat (state 4, 0x1E66E) and removal

| Phase | Behaviour | Tag |
|---|---|---|
| 0 | if y > camY+160 (3232, on the ledge) or +41 != 0: jump pose, vy = min(vy, 0) -> phase 2; else -> phase 6 directly | SOURCE (D4b died on the floor) |
| 2 / 4 | fall: vy -= 56, y += vy; phase 2 until y <= camY+104, then phase 4 with the ground probe | SOURCE |
| 6 | sound **0x95**, root 0x1ED22, **60** updates (the white flash is the hit palette job, 13 x 5 = 65 updates) | VERIFIED (5610..5670) |
| 8 | root 0x1ED26 (collapse, 8 + 8 ticks + hold), **60** updates | VERIFIED |
| 10 | at entry: score event **0x1812** (+10,000, 0x1F14) and drop set **9** (0x2508: 10 x 500 + 4 x 1000 + 1 x variant 10 at his position); **180** updates | VERIFIED |
| end | state 6 (removed, 0x2A36); **FF83E7 = 1** (opens the area-4 exit), FF838A = 1, FF83E6 = FF83D0 = 0 | VERIFIED |
Kael stays +0 = 2 (immune, harmless) throughout state 4. VERIFIED.

## 8. Sounds and music

| Command | Event | Where | Tag |
|---|---|---|---|
| music 0x0E (0x788A) | first wake | 0x1DEC0 | VERIFIED |
| 0x2F | dialogue letters (0x15CC message engine) | - | VERIFIED |
| **0x91** | every slash (wave spawn) | 0x1E842 | VERIFIED (26) |
| **0x2B** | landing (phases 8, 10, 12, 14) | 0x1E20C, 0x1E336, 0x1E414, 0x1E596 | SOURCE (shared with the player's landing in the log) |
| **0x28** | parry clash | 0x1E914 | VERIFIED (2) |
| **0xA2** | Kael hit | 0x1E8D2 | VERIFIED (12) |
| **0x95** | defeat | 0x1E6F0 | VERIFIED (1) |

## 9. Corrections to stage6.md 6.1-6.2
- Phase 14 is the **leap to a column** (not a walk); phase 2 is the walk;
  phase 0 the opening approach.
- Phase 6 is the **ground slash** (not a hit reaction); there is no hit
  reaction. Phase 16 is the **parry** (one spark per parry, then phase 6).
- Phase 10 is the **drop from the ledge**; phase 12 a 2 px/update **hop
  away** when he lands near the player (no sword box of its own).
- 0xA2 is Kael **being hit**; the slash sound is **0x91**.
- The "sword box" class 10 kind 13 is a forward-flying **sword wave**
  (~95 px in 14 updates, then 10 fading).
- Invulnerability is exactly 30 updates; FF838A means "dialogue shown",
  set at the first wake (and again at defeat).

## 10. UNKNOWN
- What FF83EA = 2 does inside the player handler during the push (input held
  is assumed; the player did not move by itself in D4b).
- Whether the ledge at y 3279 exists at x < 320 (only matters if phase 8 could
  start there; by the tables it cannot).
- Parry odds and phase-16 behaviour at higher rank rows and with charged
  swords / Willow's forms (only rank row 1, Madmartigan sword, observed).
- Kael's defeat from the ledge (state 4 phases 2/4) was not traced.
- Wave and spark box/node geometry: the other stage-6 research (not here).
