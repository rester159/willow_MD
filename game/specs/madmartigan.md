# Player (Madmartigan) behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), main program `build/reference_maincpu.bin`
(sha256 0b66efe8...). Behaviour spec for the native Genesis rewrite
(NATIVE_ENGINE_PLAN.md sections 2 and 6). Arcade addresses are citations only.
Companion to `player.md` (Willow): where Madmartigan behaves the same, this file
says so and cites `player.md` instead of repeating it. Closes `stage2.md`
open question 1.

Evidence tags (as `player.md`):
- **VERIFIED**: two independent sources agree (disassembly read for this file
  plus a headless arcade trace, or plus the transliteration in
  `native/engine/src/player_*.h`, `actor8_zero.h`, `sword_live_targets.h`).
- **SOURCE**: one source only (usually the disassembly read here).
- **UNKNOWN**: not established; says where to look next.

Units: world X right, world **Y up**; speeds signed **8.8 px/frame** (256 =
1 px/frame); one update = one 60 Hz arcade frame. Hit box = (xOffset, width,
yOffset, height) as `combat.md` 1.1/1.2. RAM: `a5` = FF8000; player record
**FF9846** (+0 active/vulnerable, +2 life state, +3 action, +4 phase, +5 sword
phase, +6 x, +0A y, +0F facing, +14 node pointer, +1C node ticks, +20 form,
+31 hold counter, +38 vertical speed, +3C sword word, +3E crouch flag, +48
airborne, +4A slide speed, +4C sword timer, +54 collision radius).

### Traces used (headless lockstep `ref`, original program, no GUI)
Harness `.../perf/arcade/b1/out/lockstep.keys ref` run from the `b1` folder with
core `.../perf/gpgx-cps/genesis_plus_gx_libretro.dylib`, `STAGE=1
OVERCLOCK=5000`. Scratch folder
`/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage2/`
(`S/` below; new files in `S/mad/`).

| Run | Settings | Output |
|---|---|---|
| **K** (new) | `SCRIPT=keys`, RAM every frame; play input from key ~1102 (KEYS p = key - 702): walk right 60, jump, up-jump (hit a ceiling), tap attack, crouch + crouch attack, jump + air attack, attack held 40 / 75 / 110 frames, down+jump slide | `S/mad/ramK/`, per-frame table `S/mad/k_player.txt` (built by `S/mad/extract.py`) |
| **D** (new) | `SCRIPT=die` (walk right, never attack), RAM every 2 | `S/mad/ramD/`, `S/mad/d_player.txt`: 2 hits, death, restart, 2 hits, death |
| **B** (existing, `stage2.md` 0) | random input, invincible, whole stage | `S/ram/`, table `S/mad/b_player.txt` |

Player hits in K come from stage enemies (HP 3 -> 2 -> 1 -> 0 at keys 1507,
1699, 1835), which also gave a death sequence. Disassembly listing of the
player region: `S/mad/dis_bf00_eb00.txt`; node walker `S/mad/nodes.py`.

---

## 0. Which stages, and how the program selects Madmartigan

- Form byte = player **+0x20**: **0 = Willow, 4 = Madmartigan** (word index into
  the dispatch pointer table at 0xBF3A: 0xBF42 Willow, **0xD4BA Madmartigan**).
  The transliteration normalises this to form 0/1 (`player.md` 3.12 says "form
  1"; the arcade value is 4). VERIFIED (disasm 0xBF2C; RAM +0x20 = 4 in K/B/D).
- Stage set-up 0x2F08 copies table **0x2F72**[stage*4] (word) to +0x20 and
  FF8386: stage indexes 0..5 -> 0, **4**, 0, **4**, 0, **4**. Stage index 4
  (stage 5) keeps the carried form FF8386 when FF83A3 is set (y + 8 if
  Madmartigan), else Willow; stage index 5 (stage 6) keeps the carried form at
  checkpoint FF8374 >= 5, else Madmartigan. So Madmartigan plays stages 2, 4 and
  (part of) 6. VERIFIED (ROM table) / SOURCE (stage 5/6 branches).
- Other writers of the form: actor routine **0x36120** (left input -> form 0 at
  x 64, right -> form 4 at x 224, attack/jump confirms: a character chooser) and
  0x2D938 (form 0 when an actor's +0x21 = 10). Where they run is UNKNOWN (find
  the spawner of the 0x36120 actor; likely a late stage).
- Shared with Willow: the record layout, the per-frame order of `player.md` 2
  (action handler, walls 3772, head 39FE, health C170, pit C33E, screen clamp
  C392, charge meter 4B0E; Madmartigan's normal update is 0xDA8A, same order),
  the freeze flag FF83EA (C120 path), camera (section 9), input sampling
  (section 4). VERIFIED (disasm DA8A vs C13E).

## 1. Coordinates, size and probes

| Item | Value | Tag |
|---|---|---|
| Origin | about **24 px above the feet** (feet probes at y-24), horizontally centred | VERIFIED (probe table 0x39BE; trace: standing y 80 on the stage-2 start floor) |
| Stage 2 start | (192, 80), stage-start gate as Willow, facing right (`stage2.md` 1.5) | VERIFIED |
| Collision radius (+0x54, enemy proximity tests) | **24** (Willow 16) | VERIFIED (BFC2: `moveq #24` when +0x20 != 0; gate reset as transliterated) |
| Object palette | CPS object palette **1** for every pose (Willow 0) | VERIFIED (node attr low 5 bits = 1, section 6) |

Probes (relative to the origin, y up). Tables are selected by `tst +0x20`:

| Probe | Madmartigan | Willow (`player.md` 6) | Tag |
|---|---|---|---|
| Feet (0x39BE, stance +0x47 = 0) | (+12,-24) and (-12,-24); order by facing as Willow | (+-8,-16) | VERIFIED (ROM; 38F6; feet at y-24 matches standing y 80 in traces) |
| Feet, stance 2 (alternate set) | (+-12,-32) | (+-8,-32) | SOURCE (when used: `player.md` open question 4) |
| Walls (0x38B6, any stance) | right (+16,-16),(+16,0); left (-16,-16),(-16,0) | y -8 and 0 | SOURCE (ROM; 3772) |
| Head (0x3A74) | standing (+10,+24),(-10,+24); crouching (+10,+8),(-10,+8) | +16 / 0 | SOURCE (ROM; 39FE). A head hit sets +0x56 = 1 (used by climb, 3.7) |
| Ledge grab (3A84 -> 3AB8) | first a ceiling test behind the head at (-10,+40) facing right / (+10,+40) facing left: if that cell has a ceiling there, **no grab**; then the ledge point (+8,+40) right / (-8,+40) left needs ledge material (flags 0x88) | (+-8,+14), no pre-test | VERIFIED (ROM 0x3AFA/0x3B02 + `player_ledge.h`) |

Floor/ceiling/wall rules, map format and descriptors are Willow's (`player.md` 6).

## 2. States

Life-state table at 0xD4CA (word offsets): 0 gate **0xD4E2**, 2 normal play
**0xDA8A**, 4 dying **0xE0DA**, 6 removed **0xE172** (-> Willow's 0xCF6A), 8
**0xE176** (returns for all stages, as Willow), 10 fixed hold **0xD576**, 12
wagon leap **0xD5A8**, 14 arrival walk **0xD732**, 16 chase-end walk **0xD80E**,
18 timed scene **0xD8CA**, 20 stage-exit scene **0xD94C**, 22 script walk
**0xDA0A**. VERIFIED (ROM table + `player_dispatch.h` form-1 states).

Normal-play action table at 0xDAB2: 0 ground **0xDACC**, 2 jump/fall
**0xDBD6**, 4 ground sword **0xDDA2**, 6 drop **0xDD04**, 8 climb **0xDF40**, 10
slide **0xE02A**. VERIFIED (ROM; trace K shows actions 0, 2, 4, 10).

### 2.1 Stand / walk (action 0) — same as Willow
- **2.0 px/frame**, instant start/stop, motion table **0xC01C** (the same table
  as Willow; set at the gate 0xD520), -2 px ground push + foot snap, facing
  rules (C52A), right wins over left. VERIFIED (trace K: x +2 every frame from
  key 1102; facing 0 walking, 2 standing). See `player.md` 3.1.
- Differences: no "alternate" (raft/drift) stand pose and down+jump is never
  turned into a normal jump by the drift flag (Madmartigan's ground code has no
  drift test). SOURCE (DACC vs C3B6; `player_ground.h` `madmartigan` flag).
- Walking off a ledge: Willow's C72E (`player.md` 3.3). SOURCE.

### 2.2 Crouch (action 0, phase 4)
Same rules as Willow (`player.md` 3.2): hold down -> crouch pose at once, no
movement, left/right only turn; release -> stand. Head probes switch to the
crouch pair (+8). Attack while crouched = crouch sword (3.4). VERIFIED (trace K
keys 1322..1347).

### 2.3 Jump and fall (action 2) — same numbers as Willow
| Item | Value | Tag |
|---|---|---|
| Rise speed | **1088**; **1408** with up held at take-off | VERIFIED (DC18/DC28; trace K: 1032 = 1088-56 after the first frame, 1352 = 1408-56) |
| Gravity / max fall | **56** (+0x34) / **1792** | VERIFIED (D514; trace) |
| Arc (flat floor) | 20 rise frames, **43 px** high, 41 frames airborne (key 1182 press, 1183..1222 airborne, 1223 landed) | VERIFIED (trace K) |
| Ceiling | a head hit ends the rise (speed 0, fall) | VERIFIED (trace K up-jump: rise stopped after 12 frames at y 137 under the fort ceiling) |
| Air control | full and instant, 2 px/frame (C786 -> 20B4) | SOURCE |
| Landing | sound **0x2B**, back to ground | VERIFIED (DCB6) |
| Pose | one held pose, root **0xE428** (nodes 0xE430 R / 0xE440 L); reselected only when facing changes | VERIFIED (trace) |
| Ledge grab | up held in the air + ledge probe (section 1) -> climb (2.7) | SOURCE (DCC2 -> C57A -> 3A84) |
| Fall effect | Willow's C8B6 (collision id 32 cell, `player.md` 3.4) | SOURCE |
| Press timing | as Willow (M1_NOTES): the press frame sets action 2 phase 0 with the old pose; the rise starts next update | VERIFIED (trace K key 1182/1183) |

### 2.4 Sword attack (ground action 4; air: inside the jump)
Button 1 (Genesis A). Input C610 is Willow's (`player.md` 3.5/3.6): a press
sets the sword word +0x3C (high byte = pending) and clears the charge value
FF8E94; a release after holding >= 16 frames sets it again with FF8E94 =
charge level (3.5).

Sequence (sword phase +5, timer +0x4C; ground table 0xDDC2, air table 0xDE02):
| Update | Player | Sword actor (section 5) | Tag |
|---|---|---|---|
| N (press) | ground: action 4 phase 0 set; pose unchanged | - | VERIFIED (trace K key 1302) |
| N+1 (phase 0, 0xDE22) | spawn sword actor if none is active (FF83CB = 0) and a class-8 slot is free; timer 4, phase 2; pose **attack**; on the ground +0x3E = 2 if down held (crouch sword) else 0 | spawned, invisible, state 0 | VERIFIED (trace: sword slot appears at key 1303) |
| N+1..N+4 | attack pose (4 updates) | invisible (its own 4-update wait) | VERIFIED |
| N+5 (timer 0 -> phase 4, +0x3C += 2) | **follow** pose | becomes visible and **collidable**, image 0, sound **0x26** | VERIFIED (trace: actor state 2 at key 1307, follow node 0xE2E8) |
| N+5..N+8 | follow | image 0 | VERIFIED |
| N+9 (phase 6, +0x3C += 2) | follow | image 1 | VERIFIED (trace: image index 2 at key 1311) |
| N+9..N+12 | follow | image 1 | VERIFIED |
| N+13 | ground: action 0, sword word cleared (pose still follow this update, stand from N+14); air: phase 0, keeps the current air pose until landing or a facing change | released (tier 0) or starts the charged extension (5.3) | VERIFIED (trace keys 1315/1316; air 1384) |

- **Total 12 updates of sword pose** (4 attack + 8 follow); Willow's shot is 10.
- Ground: no horizontal motion during the swing; the -2 px push/foot snap runs
  (0xDCF0 = vertical column only). **Jump pressed during the swing cancels it**
  into a jump (C716 -> 0xDB30, including down+jump slide/drop). SOURCE (DDAE).
- No attack input is read during a ground swing: a press or a held button there
  is ignored (the hold counter does not run, see 3.5). VERIFIED (trace K: a 40-
  frame hold gave hold count 26 = 40 - 14).
- Air: the jump arc and air control continue; the sword word keeps the swing
  going after landing (0xDBF6: landed with +0x3C != 0 -> action 4, the swing
  finishes on the ground). VERIFIED (trace B keys 1010..1022; SOURCE for the
  landing hand-over).
- Crouch sword keeps the crouch afterwards while down is held. VERIFIED (trace
  K key 1342).
- No combo: one swing at a time; the next press after N+13 starts a new swing.
  The sword actor is single (FF83CB): a swing that starts while the previous
  sword actor (charged extension) is still alive plays the poses but spawns no
  sword. SOURCE (DE22 test of FF83CB).

### 2.5 Charge and charged sword
- Hold counter (+0x31), sounds **0x31** at 16 and **0x44** every 16 frames at
  127, charge sparks (cadence 0xC706): Willow's C610/C6C0 unchanged
  (`player.md` 3.6). VERIFIED (shared code).
- HUD meter FF8376 (4B0E), Madmartigan branch: **+0x011F per held frame, cap
  0x8FFF** (fixed, not per level); the meter colour attribute adds the sword
  level FF8367 instead of the magic level; reset sound 0xF4 when released.
  VERIFIED (disasm 4B38..4B42 + `player_charge.h`; trace K: meter = 287 x hold
  count, e.g. 0x6BA0 at hold 96).
- Release with hold >= 16: FF8E94 = meter high byte L (from the previous
  update) and a second swing starts (2.4). Its sword actor gets **charge tier**
  = table **0xDE0A**[(L+1) >> 3] = 0,0,0,0,0,1,1,1,1,1,2,2,2,2,3,3,3,3,3,...
  (+0x21 of the actor). VERIFIED (ROM; trace K: L 29 -> tier 0, 68 -> 1, 107 -> 2;
  trace B: 39/42/51/56/57/71 -> 1).
- Held frames needed (counted after the tap swing ends, i.e. from about 14
  frames after the press): tier 1 >= **35**, tier 2 >= **71**, tier 3 >= **100**
  (meter full at 129). Derived from 287 x hold; tier 3 not reached in a trace.
- The tap itself already swung (tier 0). Releasing with hold < 16 does nothing.
  SOURCE (C67E).
- Willow's charged *spells* do not exist for Madmartigan: tier only changes the
  sword's power and adds the extension (5.3).

### 2.6 Slide (action 10): down + jump on normal floor
Madmartigan's own handler 0xE02A (differs from Willow):
| Step | Value | Tag |
|---|---|---|
| Start (phase 0) | sound **0x2A**; sword word and +5 cleared; facing &= 1; speed +0x4A = **1792**; root **0xE544** | VERIFIED (disasm; trace K key 1785) |
| Slide (phase 2) | each update: x += speed in the facing direction, then the facing-side wall probe (3814); speed -= 64; ends when the speed goes negative or a wall is hit: **28 updates, 101 px** on flat floor | VERIFIED (trace K: x 312 -> 413, steps 7,6,7,6,...,1,0) |
| Poses | 0xE54C wind-up (6 ticks) then 0xE558 slide (hold); facing left 0xE568/0xE574 | VERIFIED |
| Recover (phase 4) | facing |= 2, **6 updates** on root 0xE548 (0xE584 R / 0xE594 L), then stand | VERIFIED (trace K keys 1814..1820). Willow has 6 + 6 |
| Attack during the slide | any phase: a press -> crouch sword (0xDB64) | SOURCE (E036) |
| Jump during recovery | **not** possible (no jump test in the handler; Willow allows it) | SOURCE |

### 2.7 Drop through a platform (action 6): down + jump on ledge material
Handler 0xDD04 (differs from Willow `player.md` 3.9):
- Phase 0: sword word, +5, crouch, speed cleared; facing |= 2; airborne = 2;
  timer 6; root **0xE450** (nodes 0xE458.. R / 0xE48C.. L, 4 nodes x 8 ticks,
  last held).
- Phase 2: wait 6 updates; then phase 4 with timer 12.
- Phase 4: fall: speed += 56 (while < 1792), y -= speed, **no floor test** for
  12 updates (falls through the platform), then normal fall (action 2 phase 4).
- Charge sparks continue (C6C0); horizontal air motion C786 each update (facing
  has the standing bit, so 0 unless input changes facing). SOURCE (disasm).

### 2.8 Climb onto a ledge (action 8)
Handler 0xDF40 (differs from Willow `player.md` 3.7):
- Phase 0: if the head-hit flag +0x56 is set -> abort to a fall; else clear
  airborne and crouch, timer 6, root **0xE4C0** (0xE4C4 R / 0xE504 L: 5 nodes x 6
  ticks ending on the stand pose 0xE25C/0xE26C).
- Phase 2: wait 6 updates, then rise speed = **1344** (5.25 px/frame).
- Phase 4: y += speed, speed -= 56; when it would go negative -> phase 6 with
  speed 0. A head hit (+0x56) at any point -> fall (action 2 phase 4,
  airborne 2, stance 0, jump pose).
- Phase 6: y -= speed, speed += 56 while < 128; stops (action 0) as soon as the
  speed reaches 128 or a foot touches floor.
- So the climb is a vertical spring of about **66 px** (24 rise updates of a
  1344 start with gravity 56), then at most 3 falling updates (under 1 px)
  before he stands; Willow's climb is a stepped 30-px pull-up instead. Derived
  from the code; no trace. Whether stage 2/4 has ledge tiles: UNKNOWN (as
  `player.md` open question 6). SOURCE (disasm only; no climb in any trace).

### 2.9 Damage, invulnerability, death
| Item | Value | Tag |
|---|---|---|
| Hit cost, armour, protection, contact schedule | as Willow (`combat.md` 2.1, 2.2, 2.4; `player.md` 3.10) | VERIFIED (trace D/K: 3 -> 2 -> 1 -> 0) |
| Hurt sound | **0x84** (Willow 0x82) | VERIFIED (disasm C1B2; `player_health.h`) |
| Hurt timer | **128** updates, palette flash every 4 (flash/normal sets at 0xE5000/0xE5080 **+0x20**, written to object palette 1) | VERIFIED (trace K: +0x36 = 128 the update after the hit; `combat.md` 2.3) |
| Knockback | **none** | VERIFIED (trace D: x keeps +2/frame through the hit at key 1264) |
| Hazard tiles (id 39) | as Willow, but the alternate (odd-frame) probe is at **y-16** (Willow y-8); even frames y+16 standing / y+0 crouching | SOURCE (`player_health.h` form offset) |
| Death (HP 0) | life state 4: facing flips, timer **60**, root **0xE658** (0xE65C R / 0xE6A8 L: 6 nodes x **10** ticks, last held), sound **0x85**; after 60 updates -> state 6 (removed; lives/continue as Willow) | VERIFIED (trace K: HP 0 at 1835, state 4 1836, death nodes every 10, state 6/0 at 1899; trace D 1536 -> 1598) |
| Death on the wagon (road) | life state 4 action **4** (set by the wagon 0x2FE5A when the player's y <= 64 and the service invincibility is off): timer **96**, direct node **0xE600** (4 nodes, ticks from the road-speed table via 0x4982, loop), sound **0x85**; each update x -= road speed FF83DA (8.8), removed when x - cameraX + 32 < 0 or the timer ends. The body slides away along the road | SOURCE (disasm E12C..E170, 2FE52) |
| Willow-only death tail | Madmartigan's death does not run the pit/screen-clamp tail | SOURCE (`player_death.h`) |
| Pit (y below camera min) | as Willow (C33E runs in normal play) | SOURCE |

### 2.10 Scripted states used in stage 2
- **12 wagon leap** (0xD5A8): specified in `stage2.md` 5.12 (music fade 0x08FB,
  fall at gravity 24, walk 16 updates at 2 px, leap 1088 up and 2 px/frame
  toward x 1728, music 3, landing: state 2, clock 3:00, sound 0x2B). VERIFIED by
  re-reading the disassembly here (landing writes FF83C6 = 0x0300).
- **16 chase end** (0xD80E; set by the wagon contact pass 0x39C3A when FF83E2 =
  2): clears sword/charge, faces toward camera X + 192, walks (C782, 2 px/frame,
  with gravity 24 and foot snap) until it reaches camera X + 192, then stands
  facing right. Closes `stage2.md` open question 4. SOURCE.
- 14 (arrival walk on the slow table 0xC02C until FF8470), 18 (timed stand/
  walk scene), 20 (exit scene, spawns class kind 29 at (16256,80)), 22 (script
  walk with gravity): same shapes as Willow's; not used in stage 2 traces.
  SOURCE.
- 10 fixed hold (0xD576, pose 0xD5A0; set by 0x30F52): `player_hold.h`. SOURCE.

## 3. Input rules
Same as Willow (`player.md` 4): A = attack (button 1), B = jump (button 2), edge
detection, no buffering, instant turns. Differences: down+jump is never a plain
jump (no drift flag), a ground swing ignores attack input (2.4), slide recovery
cannot jump (2.6). VERIFIED (shared C52A/C610/C716) / SOURCE (differences).

## 4. Animation roots and nodes (pose packer)

Node format as `stage1_enemies.md` 3 / `player.md` 5. All poses use **object
palette 1**; left-facing nodes are the same tiles with attr bit 0x20
(horizontal flip). Native pose ids 70..139 from
`build/native-world/player-hold-data-v1/manifest.json` (same identity as the
Willow atlas). Hurt box = node +6 geometry index into **0x3B1C2** (`combat.md`
1.2). VERIFIED (ROM node walk with `S/mad/nodes.py`; manifest ids; trace node
pointers).

| Use | Root table (R/L) | Nodes R | Nodes L | Ticks | Tiles (attr) | Geo -> hurt box | Ids |
|---|---|---|---|---|---|---|---|
| Walk cycle | 0xE194 [0]/[1] | 0xE1A4, E1B0, E1BC, E1C8, E1D4, E1E0 (loop) | 0xE1F0..0xE22C | 6 each | 4x4 block (0x3301), codes 0x0400,0404,0440,0408,040C,0444 | 9 (10,20,26,50) | 70-75 / 76-81 |
| Stand | 0xE194 [2]/[3] | 0xE25C | 0xE26C | hold | 4x4, 0x0500 | 9 | 84 / 85 |
| Crouch | 0xE19C | 0xE23C | 0xE24C | hold | 4x3 (0x2301), 0x0514 | 10 (10,20,9,33) | 82 / 83 |
| Jump / fall | 0xE428 | 0xE430 | 0xE440 | hold | 4x4, 0x04C0 | 17 (13,26,26,44) | 98 / 99 |
| Sword attack | 0xE2A0 | 0xE2B8 | 0xE2F8 | hold | piece list of 17 code words (0x0448..) | 9 | 86 / 88 |
| Sword follow | 0xE2A4 | 0xE2E8 | 0xE328 | hold | 4x4, 0x044C | 9 | 87 / 89 |
| Crouch sword | 0xE2A8 | 0xE338 | 0xE370 | hold | 13 pieces (0x0494..) | 10 | 90 / 92 |
| Crouch follow | 0xE2AC | 0xE360 | 0xE398 | hold | 4x3, 0x049C | 10 | 91 / 93 |
| Air sword | 0xE2B0 | 0xE3A8 | 0xE3E8 | hold | 17 pieces (0x04C4..) | 9 | 94 / 96 |
| Air follow | 0xE2B4 | 0xE3D8 | 0xE418 | hold | 4x4, 0x04CC | 9 | 95 / 97 |
| Drop | 0xE450 | 0xE458, E464 (4x3), E470, E47C (4x4, held) | 0xE48C..0xE4B0 | 8 each | 0x0698, 069C, 06C0, 0684 | 10, 10, 11 (10,20,24,40), 12 (10,20,30,50) | 100-103 / 104-107 |
| Climb | 0xE4C0 | 0xE4C4, E4D0, E4DC, E4E8, E4F4 -> stand 0xE25C | 0xE504..0xE534 -> 0xE26C | 6 each | 4x4: 0x0640, 0680, 0648, 0644, 06C4 | 9, 13 (14,28,30,34), 14 (14,28,24,40), 15 (14,28,22,38), 9 | 108-112 / 113-117 |
| Slide | 0xE544 | 0xE54C (5x3, 0x0605), 0xE558 (5x2, 0x0600, held) | 0xE568, 0xE574 | 6, hold | | 10, 16 (20,40,7,31) | 118-119 / 120-121 |
| Slide recover | 0xE548 | 0xE584 (4x3, 0x060A) -> crouch 0xE23C | 0xE594 -> 0xE24C | 6, hold | | 10 | 122 / 123 |
| Death | 0xE658 | 0xE65C (4x4 0x054C), E668 (0x0590), E674 (0x0594), E680 (0x0598) (4x3), E68C (4x2 0x05AC), E698 (4x1 0x059C, held) | 0xE6A8..0xE6E4 | 10 each | | 0 | 128-133 / 134-139 |
| Road death | direct 0xE600 | 0xE600, E61E (10 pieces), E63C, E648 (4x2), loop; all x-flipped | - | from 0x4996 road table | 0x1EC0.., 0x1E8C, 0x1EAC | 0 | 124-127 |

- The slide-recover and climb chains end on crouch/stand nodes that belong to
  other roots: the packer must follow `next` (manifest ids already do).
- The air poses are reselected only on a facing change; after an air swing the
  air-follow pose stays up until landing. VERIFIED (trace K keys 1384..1400).
- Walk poses advance only while walking; "hold" nodes in the arcade decrement
  their 0xFFFF tick count harmlessly (seen in traces); treat as hold.
- Tile codes above are the first code word; art comes from `willow.zip` via
  `tools/cps1_gfx.py`, decoded as the existing pose packer does
  (`game/tools/posepack.py`, `native/engine/tools/build_player_sprite_art.py`).

**Madmartigan hurt box** (combat.md convention, box = [x-xOff, x-xOff+w] x
[y-yOff, y-yOff+h]): standing/walking/sword (10,20,26,50) = 20 px wide, 50 tall
(y-26..y+24); crouch/crouch sword/slide wind-up (10,20,9,33); jump (13,26,26,44);
slide (20,40,7,31). VERIFIED (ROM 0x3B1C2 entries 9..17).

## 5. The sword actor (class 8 kind 0, routine 0xE740)

Pool: class-8 player-attack pool **FFC2C6**, 20 slots x 64 bytes (shared with
Willow's shots); dispatched by kind from 0xE71C. One sword at a time (flag
**FF83CB**, cleared on release 0xEB08). VERIFIED (disasm; trace slots).

### 5.1 Spawn and set-up (state 0)
- Player (0xDE22) writes: +0 = 2, +0x20 = 0, x/y = player x/y, +0x2E =
  FF8E94 + 1, +0x21 = tier (2.5). VERIFIED (disasm; trace).
- First actor update (0xE800): hit box +0x30..+0x36 and power from the sword
  level **FF8367** (0..4) and the player's facing (5.4); follow-offset table by
  posture at spawn: **0xE904** if the player is airborne (+0x48), **0xE8F4** if
  crouching (+0x3E), else **0xE8E4**; image table by level: levels 0-1 0xE8E0,
  2-3 0xE8DC, 4 0xE8D8; timer 4; palette set (5.5). Then 4 updates invisible.
  VERIFIED (disasm + `actor8_zero.h`; trace).
- Then (0xE85C): state 2, +0 = 1 (collidable), image 0 placed at the player
  position + follow offset, sound **0x26**, published. VERIFIED.

### 5.2 Swing (state 2, phase 0)
- Each update the actor is re-placed at **player + offset** (it follows the
  player while the player moves, jumps or falls): offset **(+48, +16)** standing,
  **(+48, 0)** crouching, **(+48, +24)** airborne; x mirrored (-48) facing left.
  VERIFIED (ROM 0xE8E4; trace: (360,96) for the player at (312,80); crouch
  (360,80); air y+24 while falling).
- Image 0 for 4 updates, image 1 for 4 updates. Tier 0: then state 4 (released,
  FF83CB cleared). VERIFIED (trace K/B).
- Image roots (palette **7**, single held nodes, x-flip for left):
  | Sword level | Image 0 R / L | Image 1 R / L |
  |---|---|---|
  | 0, 1 | 0xEC1C / 0xEC50 (5 pieces 0x0508..) | 0xEC34 / 0xEC68 (7 pieces 0x050A..) |
  | 2, 3 | 0xEBA8 / 0xEBE2 (6 pieces 0x05E3..) | 0xEBC2 / 0xEBFC (9 pieces 0x05E7..) |
  | 4 | 0xEB28 / 0xEB68 (8 pieces 0x05C9..) | 0xEB46 / 0xEB86 (10 pieces 0x05F8..) |
  VERIFIED (ROM tables 0xEB10..0xEB27; manifest of `build_actor8_zero_data.py`).

### 5.3 Charged extension (state 2, phase 2; tier 1..3)
- After image 1, for **tier x 4** updates: the sword (image 1) moves outward
  **4 px per update** from the 48 px offset: x = player x + 48 + motion/256,
  motion += 1024 each update (mirrored facing left). Reach at the end: tier 1
  +64, tier 2 +80, tier 3 +96 px. y frozen at the last swing position.
  VERIFIED (disasm E98A; trace K: 364, 368, 372, 376 for tier 1; to 392 for
  tier 2 over 8 updates; trace B the same while walking).
- It keeps hitting during the extension (same box, same power).
- A spark effect (class 2 via 0x29D0, kind 2, variant random & 6, parent = the
  sword) spawns **every update** of the extension: the cadence index is read
  from the byte +0x2E, which the image-table pointer overwrote (= 0xE8 -> index
  29 -> cadence 1). A source quirk; keep it (one spark per update). VERIFIED
  (disasm EA10 + `build_actor8_zero_data.py` derivation). Spark look/motion:
  `actor2_spark.h` (SOURCE).
- The player is free (stands, walks, jumps) while the extension runs; a new
  swing in that time spawns no sword (FF83CB). VERIFIED (trace B key 1404).

### 5.4 Hit box and power per sword level
Hit box words (xOff, w, yOff, h) at **0xE78C**[level*16 + facing*8]:
| Level | Facing right | Facing left |
|---|---|---|
| 0, 1 | (0,40,16,40) | (40,40,16,40) |
| 2, 3 | (8,48,16,40) | (40,48,16,40) |
| 4 | (28,68,16,40) | (40,68,16,40) |

These boxes are **attacker** boxes in the unsigned test of `combat.md` 1.1,
`(A.x - T.x + A.xOff + T.xOff) <= A.w + T.w`, which places the attacker's box at
**[x + xOff - w, x + xOff]** x **[y + yOff - h, y + yOff]** against target boxes
read as [T.x - T.xOff, ...]. With the actor at player + (48, 16), the reach
relative to the player origin is (both facings symmetric, which confirms the
reading):
| Level | X reach (right; left mirrored) | Y (standing / crouch / air) |
|---|---|---|
| 0, 1 | px+8 .. px+48 | py-8..py+32 / py-24..py+16 / py..py+40 |
| 2, 3 | px+8 .. px+56 | same |
| 4 | px+8 .. px+76 | same |
plus 4 px per update outward during the extension. Implement the unsigned test
exactly rather than the derived intervals. VERIFIED (ROM; trace boxes) / SOURCE
(interpretation).

Power (target energy removed per hit) **0xE7DC**[level*4 + tier]:
| Level | Shop name (`stage1_shops_companion.md` 1.7) | tier 0 | 1 | 2 | 3 |
|---|---|---|---|---|---|
| 0 | starting sword | 1 | 2 | 3 | 4 |
| 1 | BATTLE SWORD | 2 | 4 | 6 | 8 |
| 2 | DRAGON SWORD | 3 | 5 | 7 | 9 |
| 3 | BATTLE BLADE | 6 | 8 | 12 | 14 |
| 4 | ULTIMATE SWORD | 12 | 14 | 16 | 18 |
VERIFIED (ROM; trace power byte p1/p2/p3 for level 0 tiers 0/1/2).

Sword level FF8367 only changes the box, power, images, HUD sword icon and meter
colour (HUD: `hud.md` "Madmartigan form"); timing is identical at every level.
Raised by the shop only (`stage1_shops_companion.md` 1.7), 0..4; debug DIP sets
4 (`magic.md`). SOURCE.

### 5.5 Sword colours
While alive the actor rewrites object **palette 7** colours 0 and 9..14 from
table **0xEA98** (8 rows of 7 CPS words, +0xF000 brightness bits) chosen by
`min(|motion|/2048, 7)`: row 0 = 0FFF 04CC 06DE 07FF 09FF 0BFF 0DFF (white/cyan),
later rows shift toward blue/violet (0F5F ... at row 6-7) as the extension grows.
Written only when the row changes (latch +0x3E). SOURCE (disasm EA44;
`attack_palette.h`). Native: one spare palette line or CRAM writes for 7
colours.

### 5.6 What the sword hits (collision passes, `combat.md` 1)
- The sword is a "player shot" (class 8, lane 0, +0 = 1): tested against
  class-4 enemies lane 0 on frames with FF8019 & 3 in {0,2} and lanes 1/2 on
  {1,3}; class-6 surfaces and class-18 bosses likewise. VERIFIED (shared pass;
  `combat.md` 1).
- Kind-0 handler list 0x3A0CE: plain hit **3A10C**: if the target +0 < 2, target
  +0 = 2 (hit) and target energy +0x2A -= power (floor 0); the sword is **not
  consumed** and keeps hitting other targets on later passes; one target per
  pass. Class-4 kinds by selector 0x3A07A: 2 and 5 never; 13 only when its
  variant +0x21 = 0; 28 blocks without damage (3A160); all others plain. Class-6
  kinds 7/8 plain, 10 blocks. SOURCE (disasm 39FD4..3A162). Enemy hit
  reactions, sounds 0x32/0x33 and drops are the enemies' (stage specs).
- **Cuts enemy projectiles** (0x709C/0x70AE, lane 0 on even frames, lane 1 on
  odd; class-10 box table 0x3B0D2), action per projectile kind from **0x7122**:
  kinds **0 (arrows), 9 (hawk's rock), 15, 17, 25** destroyed (projectile +0 =
  2, state 4); kind **7 (spear)** only from stage index 4 on (so **not** in
  stage 2); kind 8 destroyed and its owner's "attack connected" flag FF0EB6 set
  (variant 0); every other kind passes through. (Willow's shots use 0x7102.)
  VERIFIED (ROM table + `sword_live_targets.h`).

## 6. Sounds posted by Madmartigan
| Word | When | Tag |
|---|---|---|
| 0x26 | sword becomes visible (each swing) | VERIFIED (E87A; run S log has 0x26) |
| 0x2A | slide start | VERIFIED (E050) |
| 0x2B | landing (jump, scripted leap) | VERIFIED |
| 0x31 / 0x44 | charge reaches 16 / full-charge pulse every 16 frames | VERIFIED (shared C66E/C6B6) |
| 0xF4 | charge meter reset | VERIFIED (4B0E path; `player_charge.h`) |
| 0x84 | hurt | VERIFIED |
| 0x85 | death (normal and road) | VERIFIED |
| 0x08FB, 0x03 (music) | wagon leap (`stage2.md` 5.12) | SOURCE |
0x39 and 0x3B in run S (`stage2.md` 9) are not posted by the player code
(no such words in 0xD4BA..0xEB10 or the shared helpers); owner UNKNOWN (likely
chest/pickup or shop).

## 7. Terrain and platform interactions
- Floors, slopes, walls, ceilings, thin platforms: Willow's rules with
  Madmartigan's probe offsets (section 1). SOURCE.
- Drop-through on ledge material (bit 7 of the floor descriptor): 2.7.
- Surfaces/platforms (class 6) push or carry the player as for Willow
  (`combat.md` 1 step 2); the wagon deck is a class-6 surface (`stage2.md` 6).
  Falling off the wagon onto the road (y <= 64) is the road death (2.9).
  SOURCE.

## 8. HUD differences (pointer)
Charge bar 18 cells, sword icon instead of the magic level, colours from the
sword level: `hud.md` (Madmartigan form) and `player_hud.h`. SOURCE.

## 9. Camera
No camera routine reads the form byte (the only readers are listed in section
0 / 8 and the HUD/shop code), so the camera rules of `player.md` 7 apply with
Madmartigan's position; stage-2 specifics (fort, chase) are in `stage2.md`
2.3/2.4. Because his origin is 24 px above the feet (Willow 16), the same
"player Y - camera Y = 128" puts his feet 8 px lower on screen. SOURCE.

## 10. Open questions (UNKNOWN), where to look
1. **Climb** (2.8) and **drop** (2.7): no trace yet. Find a ledge-material tile
   in stage 2/4 (scan collision ids with descriptor flags 0x88, `player.md`
   6) and record a `SCRIPT=keys` trace (up held in a jump next to it; down+jump
   on it); confirm rise height and the abort on +0x56.
2. **Tier 3** charge and the sword at levels 1-4: trace a hold of >= 115 frames
   (tier 3, 12-update extension to +96) and swings after poking FF8367 = 1..4
   (needs a POKE option in `lockstep.c`, or buy swords in the stage-2 shop).
3. **Box orientation**: confirm the derived sword reach (section 5.4) against a
   trace in which an enemy is hit at a known distance (e.g. run B soldiers:
   compare sword x and enemy x on the frame its energy drops), or by rendering
   a `.cps` frame of a swing (`S/render.sh`).
4. **Road death** (2.9) and wagon landing: record a trace that falls off the
   wagon without `INVINCIBLE` (state 4 action 4, x drift by FF83DA).
5. Sword vs class-18 bosses (stage 4/6): handler table 0x3A4F6 (kind 0 ->
   0x3A51A) not walked target by target.
6. Who runs the **character chooser** actor 0x36120 and the form reset at
   0x2D938 (which stage, which checkpoint); stage 5/6 form carry-over via
   FF83A3 / FF8374.
7. Owner of sound words 0x39 and 0x3B in stage 2.
8. Attack-press timing nuance: a swing press is acted on the next update
   (as Willow, M1_NOTES); the charge release uses the meter of the previous
   update. A native trace comparison (`tools/tracecmp.py`) on run K's inputs
   should confirm frame-exact equality.
