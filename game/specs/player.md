# Player (Willow) behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), main program `build/reference_maincpu.bin`
(sha256 0b66efe8...). This is a behaviour spec for the native Genesis rewrite
(NATIVE_ENGINE_PLAN.md sections 2 and 6). Arcade addresses are citations only.

Evidence tags:
- **VERIFIED**: two independent sources agree (ROM bytes or disassembly, plus the
  earlier transliteration in `native/engine/src/player_*.h`, or plus a captured
  arcade trace).
- **SOURCE**: one source only (usually the transliteration, which PORTING.md says
  was differential-tested against the original routines; not re-checked here).
- **UNKNOWN**: not established; says where to look next.

"Stage 1" below is arcade stage index 0. One update = one 60 Hz arcade frame.

---

## 1. Coordinates and units

| Item | Value | Tag |
|---|---|---|
| World X | 16-bit pixels, increases to the right | VERIFIED (walk trace + headers) |
| World Y | 16-bit pixels, **increases upward**. Map lookups use downward Y = `~y` (from 3E4C, build_world.py) | VERIFIED (contact.h + build_world.py) |
| Sub-pixel | Separate 16-bit fraction per axis. Speeds are signed 8.8 px/frame, added as `speed * 256` to a 16.16 position | VERIFIED (disasm C7EC: `ext.l; asl.l #8; add.l` to the position long) |
| Player origin | About 16 px above the feet (feet probes at y-16), horizontally centred | SOURCE (probe table 0x397E) |
| Screen X | `screenX = worldX - cameraX` (CPS visible 384 px wide) | VERIFIED (trace: camera = playerX-192 when player is centred; sprite_templates.h) |
| Screen Y | `screenRow = 240 - (worldY - cameraY)` in the 224-line visible area, so worldY-cameraY = 128 is mid-screen | SOURCE (sprite_templates.h: CPS y = 256-(worldY-cameraY), visible lines 16..239) |
| Facing | 0 = moving right, 1 = moving left, 2 = standing facing right, 3 = standing facing left. Bit 0 = left, bit 1 = "not moving". | VERIFIED (trace: facing 0 while walking right, 2 when standing; C52A) |
| Stage 1 start (checkpoint 0) | x = 192, y = 72, standing facing right | VERIFIED (ROM 0x3078; walk trace frame 1617: x 0xC0, y 0x48, facing 2) |

## 2. Per-frame update order

SOURCE (BF42 / C13E / DA8A as transliterated in player_dispatch.h, player_update.h, player_movement.h):
1. Sample input (held, pressed = rising edge vs last frame).
2. Run the current action handler (stand/walk, jump, attack, drop, climb, slide).
   Handlers apply their own displacement and foot probes.
3. Wall probes (all four points), then head probes.
4. Health step (hurt/invulnerability timers, hazard tiles, death check).
5. Pit check: `y < camera.minimumY` -> death (section 3.10).
6. Screen clamp: `x` clamped to `[cameraX+16, cameraX+368]` (C392).
7. Charge meter step (section 3.6).
8. Camera update uses the new player position the same frame (section 6).

## 3. States

Top-level life states (player_dispatch.h): 0 stage-start gate, 2 normal play,
4 dying, 6 removed (lives handled elsewhere), 10 hold, 12/14/16/18/20/22 scripted
(arrival, script walk/jump, cutscene). Normal play has actions 0 ground,
2 jump/fall, 4 attack, 6 drop, 8 climb, 10 slide. SOURCE.

### 3.1 Stand / walk (action 0)
- Horizontal speed **2.0 px/frame** (512 in 8.8) while left or right is held;
  0 when neither. No acceleration or deceleration: full speed on the first
  frame, stops dead on release. VERIFIED (ROM motion table 0xC01C =
  (+512,-512),(-512,-512),(0,-512),(0,-512) per facing; walk trace: x +6 every
  3 frames from the first frame of input).
- Every ground frame also moves y by -2 px (the -512 column), then the two foot
  probes snap the player up onto the floor. This is how slopes and small steps
  are followed. SOURCE (C3B6 tail: motion -> 38F6 feet -> C72E).
- Right wins if left and right are both held. With no direction the facing
  keeps its side and becomes "standing" (facing |= 2). SOURCE (C52A).
- Gate/arrival walks use the slower table 0xC02C (1.0 px/frame). SOURCE.
- Willow's collision radius for enemy proximity tests: 16 (Madmartigan 24). SOURCE (BFC2).

### 3.2 Crouch (still action 0, phase 4)
- Hold down on the ground: crouch, no horizontal movement; left/right only turn
  (facing 2 or 3). Release down: stand again. Head probes drop from +16 to 0.
  SOURCE (C3B6).
- Attack while crouched fires a low shot (section 3.5).

### 3.3 Walking off a ledge
- No floor under either foot: fall speed += 128/frame (0.5 px/frame^2, capped
  at 1792), on top of the -2 px ground push. When the fall speed reaches 384
  (third airborne frame) the player switches to the jump state's falling phase
  with speed 0, after which normal gravity (56) applies. SOURCE (C72E,
  player_motion.h).

### 3.4 Jump (action 2)
| Item | Value | Tag |
|---|---|---|
| Trigger | jump button rising edge on the ground (not with down held, see 3.8/3.9) | SOURCE |
| Initial rise speed | **1088** (4.25 px/frame); **1408** (5.5 px/frame) if **up** is held at take-off | VERIFIED (disasm C7CA/C7DA, `btst #3` on held input) |
| Gravity | **56** per frame (0.21875 px/frame^2), set at stage start | VERIFIED (disasm BF94/D514 `move.w #56`; player_gate.h) |
| Max fall speed | **1792** (7 px/frame); gravity is added only while speed < 1792 | VERIFIED (disasm C834) |
| Rise | take-off frame already moves: y += speed, then speed -= 56; rise ends when speed would go negative or a head probe hits a ceiling, then fall starts at speed 0 | VERIFIED (disasm C7EC-C818) |
| Fall | y -= speed, then speed += 56 (if < 1792); land when either foot probe finds floor; landing plays sound 0x2B and returns to ground | VERIFIED (disasm C820-C860; header) |
| Derived arc (flat ground) | normal: 20 rise frames, ~43 px high, ~41 frames airborne, ~82 px horizontal; up-jump: 26 rise frames, ~72 px, ~53 frames | derived from the above |
| Air control | Full and instant: each frame the held direction sets facing and x moves 2 px/frame (0 if no direction). Turning in the air is allowed. | SOURCE (C78A: facing from input, then motion table horizontal-only) |
| Pose | one static pose for the whole jump: node 32 (right) / 33 (left) | VERIFIED (manifest roots + ROM node ticks 0xFFFF) |
| Ledge grab | while airborne, holding **up** and a ledge probe finds ledge material -> climb (3.7) | SOURCE (C7BC, 3A84) |
| Fall effect | while airborne over a map cell with collision id 32, spawn a small effect actor, at most once per 8 frames. What it looks like (splash/leaves?) is UNKNOWN; check the class-12 effect kind 0 art. | SOURCE (C8B6) |

### 3.5 Attack: Willow's magic shot (action 4, and in the air)
Button 1 (A on Genesis). Willow attacks with a projectile, not a melee hit.
- **Timing** (both ground and air): the press sets an attack pending; the attack
  handler then runs: entry frame (pose "attack", timer 4), 4 more frames, then
  **the shot spawns on the 5th handler frame** and the "follow" pose shows for
  5 frames, then the attack ends. Total about 10 frames; on the ground the
  player returns to stand/walk. SOURCE (C9DA, player_attack.h).
- On the ground the attack stops horizontal motion (vertical push still runs).
  In the air the jump continues and air control stays active. SOURCE.
- Jump pressed during a ground attack cancels into a jump. SOURCE.
- Poses: standing 20/21 (R) 22/23 (L); crouching 24-27; air 28-31 (attack then
  follow; see section 4). VERIFIED (manifest roots).
- **Shot spawn**: at the player origin x, y+8 standing / y+0 crouching; facing
  = facing bit 0; a muzzle effect (class-2) spawns too. **At most 4 normal shots
  alive**; a fifth press does nothing. SOURCE (CB24).
- **Normal shot variant = current magic level (0..6)**. Per variant v:
  speed 1024+64v (4.0..5.5 px/frame, horizontal only), power v+1, lifetime 64
  frames, removed early when > 32 px outside the screen (checked every 4th
  frame) or when it hits a wall tile (wall flag in its direction).
  VERIFIED for speed/power (ROM 0xED0C, 0xECF6 = 1..7; actor8_one.h).
- Shot hit boxes (xOffset, width, yOffset, height), v = 0..6: (4,8,3,6),
  (5,10,4,8), (8,16,6,12), (10,20,8,16), (10,20,9,18), (12,24,12,24),
  (12,24,14,28). Box = [x-xOffset, x-xOffset+width] x [y-yOffset, y-yOffset+height].
  VERIFIED (ROM 0xED44; overlap test in combat_contact.h).
- Fires sound 32 on spawn. SOURCE.

### 3.6 Charged magic
- Holding button 1: a hold counter rises 1/frame to 127 (sound 0x31 at 16; at
  127, sound 0x44 every 16 frames). Charge sparks spawn at a cadence from table
  0xC706 = 10,6,5,4,3,2,2,1,... indexed by counter/16. SOURCE (C6xx, player_ground.h).
- The tap itself already fired a normal shot on the press frame.
- HUD **charge meter** rises while held by a per-magic-level increment up to a
  cap (both 16-bit, high byte = charge level): level 0: +0x011F to 0x8FFF;
  1: +0x014F to 0xA7FF; 2: +0x016F to 0xB7FF; 3-6: +0x017F to 0xBFFF. All fill
  in about 128 frames (2.1 s). Releasing or stopping resets it to 0 (sound
  0xF4). VERIFIED (ROM 0x4BFA/0x4C08; player_charge.h).
- **Release**: if the hold counter < 16, nothing more happens. Otherwise a
  charged attack starts with charge level L = meter high byte:
  tier = T[(L+1) >> 3] with T (0xCB08) = 0,0,0,0,0,1,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,5,5,6,6,6,6,6;
  spell = S[magicLevel][tier] (0xCB4C):
  L0: 0,1,2,3,3,3,3 / L1: 1,2,3,4,7,7,7 / L2: 2,3,4,5,7,8,8 /
  L3: 3,4,5,6,7,8,9 / L4: 4,5,6,7,8,9,10 / L5: 5,6,7,8,9,10,11 / L6: 6,7,8,9,10,11,12.
  VERIFIED (ROM tables; player_shots.h).
- Spells 0-6 are normal shot variants. 7, 8, 9, 10 spawn magic actor kinds 3,
  7, 4, 6 (kind 9 refused while a blocking magic is active); 11 spawns two
  kind-5 shots, one each way; 12 spawns 8 radial pieces plus a kind-8 shot.
  SOURCE. What each kind looks like and does (names, speeds, arcs, damage) is
  UNKNOWN here: spec it from actor8_magic.h / routine 0xE*** in the magic spec.

### 3.7 Climb onto a ledge (action 8)
- Ledge probe (facing side): (+8,+14) right / (-8,+14) left; needs a tile with
  both floor and ledge flags (0x88). The player is snapped to the ledge surface.
  SOURCE (0x3AF2, 3FA2).
- Then 5 steps of 5 frames: after step 1 y += 14, after step 5 y += 16 (x
  unchanged), then 5 more frames, then stand. About 31 frames. Poses 42-46
  (R) / 47-51 (L), 5 ticks each, ending on the crouch pose. SOURCE
  (CCF0, table 0xCD74).
- Whether stage 1 has any ledge tiles: UNKNOWN (scan stage 0 scroll2 attributes
  for collision ids with descriptor flags 0x88).

### 3.8 Slide (action 10): down + jump on normal floor
- Sound 0x2A; 3-frame wind-up (poses 52-54 R / 55-57 L); then slide at
  1792 (7 px/frame) in the facing direction, slowing by 64/frame until the speed
  goes negative (28 frames, ~101 px) or a wall probe on the facing side hits;
  then 6 + 6 recovery frames (pose 58/59). Jump can be pressed during recovery
  to cancel into a jump; attack can be pressed at any point (crouch attack).
  SOURCE (CDC2).

### 3.9 Drop through a platform (action 6): down + jump on ledge-material floor
- Floor descriptor bit 7 set: crouch-drop animation 3 x 6 frames
  (poses 34-37 R / 38-41 L), then 6 more frames, then fall starts at speed 512
  (2 px/frame) with gravity; after 8 frames it becomes a normal fall. Air
  control during the fall. SOURCE (C8F2).

### 3.10 Damage, invulnerability, death
- Every hit costs **1** HP, or 1 armour point if armour > 0 (armour items are
  consumed in order item 10, 8, 9). The hit sets the "already hit" flag, so
  further contacts in the same frame do nothing. SOURCE (obstacle_contact.h).
- When HP changes: sound 0x82, HUD redraw, **128 frames** of hurt
  invulnerability; the palette alternates every 4 frames. No knockback was found
  in the player code (obstacle actors push 2 px/frame). Whether enemies apply a
  knockback: UNKNOWN (check enemy attack routines' writes to the player record).
  SOURCE (C170).
- Protection item (item 11): consumes one, 128 frames of invulnerability
  instead of damage. SOURCE.
- Hazard tiles: collision id 39 under the player (alternating feet/head offset
  each frame) deals 1 hit. SOURCE.
- HP 0: death. Sound 0x83, facing flips, death poses 60-64 (R) / 65-69 (L),
  8 ticks each, then hold; after 40 frames the player is removed (lives/continue
  elsewhere). SOURCE (CED4).
- Pit: y below camera minimum Y -> removed at once (falls off the screen).
  A clock "rebound" flag instead makes the player bounce up at 3584
  (14 px/frame); when that flag is set is UNKNOWN (player_clock.h owner). SOURCE (C33E).
- Starting HP = maxHp from the vitality DIP setting. SOURCE.

### 3.11 Water
No water/swim state exists in the player code. Stage-specific surfaces (raft,
lifts) move the player from outside. A "drift" (raft) flag makes Willow use the
static stand pose and turns down+jump into a normal jump. SOURCE.

### 3.12 Madmartigan (form 1, later stages only)
Same structure with his own poses (70-139), 24 px collision radius, sword
instead of shots: sword actor spawns at attack start; 4+4+4 frame phases;
power tier from 0xDE0A by charge; sword hit box per level/facing from 0xE78C
(level 0 right: (0,40,16,40)). Outside the stage-1 scope; detail when needed.
SOURCE.

## 4. Input rules

| Item | Value | Tag |
|---|---|---|
| Bits | 1 right, 2 left, 4 down, 8 up, 16 button 1 (attack/magic), 32 button 2 (jump) | VERIFIED (CPS-1 input bits; disasm `btst #3` = up at C7D4; headers) |
| Genesis mapping | A = attack, B = jump (AGENTS.md) | SOURCE |
| Edge detection | pressed = held and not held last frame; sampled once per update | SOURCE (1F88/BF1E) |
| Buffering | none: a press is seen only on its frame. A press during an attack in progress does not queue. | SOURCE |
| Attack while walking | allowed; stops horizontal motion for the attack | SOURCE |
| Attack while jumping | allowed; jump arc and air control continue | SOURCE |
| Turn | instant, any frame, including in the air and while crouched (turn only) | SOURCE |
| Down+jump | slide, or drop through if standing on ledge-material floor | SOURCE |
| Up+jump | higher jump; up held in the air grabs ledges | VERIFIED (jump speed) / SOURCE (ledge) |

## 5. Animation

Native pose index = animation node index 0..139: the arcade animation nodes
reachable from the 32 player roots, sorted by ROM address
(`build/native-world/player-hold-data-v1/manifest.json`). The sprite cutter
`native/engine/tools/build_player_sprite_art.py` keeps this identity (atlas frame
N = pose N, both forms share one atlas). Pose 0xFFFE is a separate fixed pose
(ROM 0xD5A0) outside the atlas. Each node has a duration in frames
(0xFFFF = hold) and a next node. VERIFIED (manifest; ROM node +8; walk trace
shows pose 0xD092 -> 0xD09E after 6 frames).

| Use (Willow) | Right | Left | Frames per pose |
|---|---|---|---|
| Walk cycle (loops) | 0-5 | 6-11 | 6 each (36-frame cycle) |
| Stand | 14 | 15 | hold |
| Crouch | 12 | 13 | hold |
| Recover (after respawn) | 16-17 loop | 18-19 loop | 6 |
| Attack / follow | 20 / 21 | 22 / 23 | held for the attack phase |
| Crouch attack / follow | 24 / 25 | 26 / 27 | held |
| Air attack / follow | 28 / 29 | 30 / 31 | held |
| Jump and fall | 32 | 33 | hold |
| Drop (then fall pose) | 34-36, 37 | 38-40, 41 | 6, then hold |
| Climb (ends on crouch) | 42-46 | 47-51 | 5 each |
| Slide / slide hold | 52-53, 54 | 55-56, 57 | 6, then hold |
| Slide recover | 58 | 59 | hold |
| Death | 60-64 | 65-69 | 8 each, then hold |

Madmartigan: walk 70-75/76-81, crouch 82/83, stand 84/85, sword 86-97, jump
98/99, drop 100-107, climb 108-117, slide 118-123, scroll death 124-127, death
128-139. Walk poses advance only while walking; changing facing restarts the
facing's root. Hurt flashing is a palette swap, not a pose. SOURCE.

**Player hurt box by pose** (xOffset, width, yOffset, height; same box layout
as 3.5), from ROM 0x3B1C2 via each node's geometry index:
standing, walking, attacking, air attack, death: (12,24,18,34);
crouch, crouch attack, slide: (12,24,6,22); jump and air follow: (12,24,16,32);
drop: (12,24,8,24); climb steps 45/50: (12,24,16,18), 46/51: (12,24,16,16).
So Willow is 24 px wide and 34 px tall standing (y-18 .. y+16). VERIFIED (ROM
dump; build_contact_geometry.py uses the same lookup).

## 6. Ground and terrain collision

### Map format (all stages)
- Collision lives in the **foreground layer (CPS scroll2) map**: 16x16 px cells,
  each a (tile code, attribute) word pair; **collision id = attribute bits
  10-15** (0..63). VERIFIED (build_world.py; contact.h).
- Map pages are 256x256 px (16x16 cells, stored column-major). Per stage a
  page-grid mode (ROM 0x2DC1C: 16x16, 32x8, 64x32 or 256x2 pages) and a byte
  per page (pointer table 0x2DBF8) choosing a page at 0x80000 + page*1024.
  Lookups use downward Y (`~worldY`). SOURCE (build_world.py).
- Collision id -> 8-byte descriptor, table ROM 0x4712 (64 entries).
  Byte 0 flags: 1 = blocks moving right (wall), 2 = blocks moving left,
  4 = ceiling, 8 = floor, 0x80 = ledge/thin platform. Byte 1 = floor shape.
  Byte 2 = ceiling height (signed). Byte 3 / byte 4 = right / left wall X within
  the cell. VERIFIED (ROM dump; contact.h).
- Floor shape: bit 7 set -> flat floor at (shape & 31) px above the cell bottom
  (when >= 9, a first-foot probe in the lowest 7 px of that cell is ignored);
  bit 7 clear -> slope, height = (shape & 15) + (localX >> ((shape >> 4) & 3)),
  localX mirrored when bit 6 is set, capped at 16 (slopes 1:1, 1:2, 1:4, 1:8).
  Shape bits 6 and 7 both set -> after the snap, probe once more at the corrected
  height (floor continues into the cell above). SOURCE (3E4C).

### Probes (Willow, relative to the origin; y up)
- Feet: (+8,-16) and (-8,-16); order depends on facing; the second sees the
  first's correction. A foot inside a floor is moved up to the floor surface and
  the Y fraction cleared. Contact = either foot. VERIFIED (ROM 0x397E; 38F6 in header).
- Walls: right side (+16,-8),(+16,0); left side (-16,-8),(-16,0); X is pushed
  out to the wall face and the X fraction cleared. VERIFIED (ROM 0x3876).
- Head: (+8,+16),(-8,+16) standing, (+8,0),(-8,0) crouching; a hit moves y
  down and ends the rise. VERIFIED (ROM 0x3A64).
- An alternate foot set (y-32) is selected by a "stance" byte; when it is used
  is UNKNOWN (look for writes to player +71 / `fallLatch`).
- Stage edges: screen clamp [cameraX+16, cameraX+368] plus the camera's own
  limits; there is no other world-edge wall for the player. SOURCE.

## 7. Camera (stage 1)

Arcade camera X/Y = FF8408 / FF840C, same world units (Y up). Updated after
the player each frame.

| Item | Value | Tag |
|---|---|---|
| X follow | locked, no dead zone: camera X = player X - 192 (player at screen centre), applied the same frame | VERIFIED (walk trace frames 1623-1667; 4298E) |
| X limits | stage 1 initial min 0 (checkpoint start), max **1952** | VERIFIED (limits table build; ROM 0x2DCA) |
| Backtracking | moving right raises min X to (camera X - 768): at most 768 px of scroll back | SOURCE (4298E) |
| Y target | keep player Y - camera Y = 128 (mid-screen) | SOURCE (42B1C) |
| Y, player lower (distance < 128) | camera moves down at once to keep 128, clamped to min Y | SOURCE |
| Y, player higher, on ground | if within 130: snap; else accelerate 64/frame up to 1792 (7 px/frame) until within 128 | SOURCE (42BAC) |
| Y, player higher, airborne | camera rises only if the player is more than 200 above it, then 2 px/frame | SOURCE |
| Y limits stage 1 | min 0, max 2048 | VERIFIED (limits table) |
| Stage 1 events | at a scripted gate the camera max X becomes 3328, min X locks to the current X and the camera auto-scrolls right 3 px/frame until the player is within 194 of the left edge; when the player reaches max X (+64 before the gate) the stage-end walk starts and the camera auto-scrolls 2 px/frame to max X; later max X = 3456 | SOURCE (42D66 / 42924); the gate trigger is UNKNOWN (find writer of the camera event flag) |
| Start | camera centred on the player (X-192, Y-128) and clamped | SOURCE (42638) |

## 8. Open questions (UNKNOWN), where to look

1. Charged spell kinds 3-8 (actor8_magic.h, ROM 0xE***): names, motion, damage,
   duration; needed before charged magic can be written.
2. Enemy knockback on the player, and per-enemy damage (always 1?): check enemy
   attack routines and combat passes writing the player record.
3. What sets the clock "rebound" flag (pit bounce instead of death).
4. Stance byte (+71) use: alternate foot height (y-32).
5. Stage-1 camera gate trigger and the stage-end handoff (player state 14 arrival).
6. Whether stage 1 contains ledge, thin-platform, hazard (id 39) or id-32 tiles.
7. Magic level progression: which pickups/shop items raise the shot level 0-6.
8. Not trace-checked: jump arc, attack timing, slide, climb, vertical camera.
   Capture headless arcade traces (existing `tools/arcade_player_walk_trace.lua`
   pattern: player record at FF9846, x at +6, y at +10, facing +0x11, pose
   pointer +0x18, ticks +0x1E) with jump, up-jump, attack, hold-release and
   down+jump inputs, and compare frame by frame (plan section 6 step 4).
