# Stage 1 enemies, hazards, objects and pickups: behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin` (sha256
0b66efe8...). Behaviour spec for the native Genesis rewrite (NATIVE_ENGINE_PLAN.md
sections 2 and 6). Continues `player.md` and `magic.md`. Arcade addresses are
citations only. The stage-1 **boss** (the creature met at camera X 3328, class-4
kinds 6/10/11 and class 18) is specified separately; this file stops at the
boss-arena entrance (section 5.11 lists what lives there so nothing is spawned
twice).

Evidence tags:
- **VERIFIED**: two independent sources agree (ROM bytes or disassembly read for
  this file, plus the earlier transliteration in `native/engine/src/*.h`, or plus
  a captured arcade trace).
- **SOURCE**: one source only (usually the transliteration, which PORTING.md says
  was differential-tested against the original routines; not re-checked here).
- **UNKNOWN**: not established; says where to look next.

Units as player.md: world X right, world **Y up**; speeds are signed **8.8
px/frame** (256 = 1 px/frame) added to a 16.16 position; one update = one 60 Hz
frame. Hit box = (xOffset, width, yOffset, height), box = [x-xOffset,
x-xOffset+width] x [y-yOffset, y-yOffset+height]. Facing for walkers: 0 = right,
1 = left. 16-direction values (projectiles, aiming): 0 = up, 4 = right, 8 = down,
12 = left, clockwise (VERIFIED: direction tables 0xED524/0xED4E4 below).
"Stage 1" = arcade stage index 0.

Trace evidence used throughout: a headless run of the original program
(lockstep harness, `SCRIPT=random STAGE=0 INVINCIBLE=1 KILLBOSS=1 GATEASSIST=900
NUDGE=600`) with full work-RAM dumps every 2 frames (frames 1700..11516 are stage
1 gameplay; stage cleared at 11517) and CPS video dumps every 10 frames, rendered
with `cpsframe.py` for visual identification. Scratch analysis scripts and dumps:
`/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/enemies/`
(`births.txt` = every actor birth/death with position, `*.roots` = animation root
extraction per family). Note: the harness assist opened the mid-boss gate at
frame 7038 before the mid-boss died (8362), so frames 7038..8362 show a
sequence the real game cannot produce (player past the gate with the mid-boss
alive); behaviour observations from that window are marked.

---

## 1. Stage 1 layout and flow

| Camera X | Area | What happens | Tag |
|---|---|---|---|
| 0..~1250 | Forest floor (ground y ~72..88) | Soldiers walk in from the screen edges (event 3), two soldiers placed at x 768 / 1072; hidden object somewhere near x 900..1312 | VERIFIED (tables + trace) |
| ~1250..1952 | Cliffs, cave, first log bridge (8 logs at y 157) | Placed soldiers, a beast (kind 1) patrolling, beasts emerging from a cave (event 1), chests, shop door at (1872, 311) | VERIFIED |
| 1952 (camera max) | Waterfall: long log bridge (22 logs at y 285) | Camera locks at X 1952. When the player arrives, the **sorcerer mid-boss** (kind 7 variant 2) appears. Killing it collapses the long bridge, creates two sinking platforms and opens the camera to X 3328 | VERIFIED (trace, camera code 0x42D88, kind 7 death 0x14EE0) |
| 1952..3328 | Waterfall cliffs, then forest again | Placed soldiers, chests (2416, 2608), beast event 2, spearman (2760, 104), archers (3056, 3200, 3328), edge soldiers again (event 4: camera 2816..3072) | VERIFIED |
| 3328 (camera max) | Boss arena entrance | Kind 7 variant 0 (dialogue trigger), kind 1 variant 3 (scripted beast), boss kind 6 at x 3776: see 5.11 / boss spec | VERIFIED (trace) |

**Camera gate** (stage 1 only, ROM 0x42D88): while FF838A is clear, the camera
max X stays 1952; when FF83E7 becomes non-zero (incremented by the mid-boss
death, 0x14F1E) the max X becomes 3328, min X locks to the current camera X and
the camera auto-scrolls right (player.md section 7). VERIFIED (disassembly +
trace: FF838A set at frame 7040 right after FF83E7 changed).

**Encounter flag FF83D0** ("screen clear while a (mid-)boss is active"): set when
the camera approaches the waterfall (trace: frame 6074, camera X 1822) and again
before the boss (frame 9782, camera X 3137); cleared by the mid-boss death
(0x14F26) and at the boss fight start (frame 11040). While set, kind-0 soldiers,
kind-3 archers and kind-14 spearmen that are active explode at once (sound 0x33,
burst effect, no reward), and the edge-soldier event retires. VERIFIED for the
flag timeline (trace) and readers (0x10C32 kind 0, 0x12E86 kind 3, 0x17FA0
kind 14, event 0x383B2); the **writer is UNKNOWN** (not a plain
`move.b #1,FF83D0`: search word/long writes to FF83CE/FF83D0; probably the camera
controller or the player arrival script).

**Arrival flag FF8470**: set while the player runs the scripted arrival walk at a
camera stop (player life state 14; trace frames 6140..7040 at camera 1952 and
9878..11258 at camera 3328). The mid-boss waits for it (5.6). SOURCE (trace +
reader 0x143B8); writer UNKNOWN (likely the player arrival script, player
state 14).

## 2. Spawn machinery

### 2.1 Stage object table (all placed actors), ROM 0x40470
Loaded once at stage start (and at continue/checkpoint restarts) by 0x40374:
pointer table 0x4034A[stage] -> word (count-1) then 8-byte entries
`class, variant, kind, direction, x(word), y(word)`. Every entry allocates one
record of its class at once; the actor itself then waits dormant until its own
wake-up rule (sections 5.x). VERIFIED (disassembly 0x40374..0x403F0; trace: all
57 entries present at frame 1700).

Load filter: class-4 entries are skipped if `x < cameraMinX (FF838C)` or if the
entry lies near the player (`|x - playerX + 160| <= 288` unsigned, i.e. x in
[playerX-160, playerX+128], and y in [playerY-100, playerY+100]); class 10 is
filtered only for variant 8 (table 0x40418); all other classes are never
filtered. VERIFIED (0x40378..0x403BA). (At a normal stage start the player is at
192,72 so nothing is filtered.)

Stage 1 entries (57), grouped:

| # | class / kind / variant | dir | x, y | What it is |
|---|---|---|---|---|
| 0 | 16 / 9 / 0 | 0 | 0, 0 (repositioned) | hidden object (5.5) |
| 1 | 12 / 8 / 0 | 4 | 1872, 311 | shop door (5.12) |
| 2 | 12 / 8 / 16 | 5 | 2688, 199 | shop door (5.12) |
| 3..10 | 6 / 0 / 0..7 | 1 | 1512..1624 step 16, 157 | short log bridge, 8 logs (5.3) |
| 11..15 | 16 / 4 / 0 | 0..4 | (1296,352) (1904,384) (2416,384) (2608,368) (1904,144) | treasure chests, dir = chest index (5.4) |
| 16..19 | 4 / 0 / 4 | 0 | (768,144) (1072,112) (1632,368) (1536,304) | soldier, placed (5.1) |
| 20 | 4 / 0 / 8 | 1 | 1616, 192 | soldier, ambush from above (5.1) |
| 21 | 4 / 0 / 4 | 1 | 1728, 176 | soldier, placed |
| 22..43 | 6 / 0 / 0..21 | 0 | 1960..2296 step 16, 285 | long log bridge, 22 logs (5.3) |
| 44..47 | 4 / 0 / 4 | 1 | (2376,392) (2376,216) (2384,128) (2528,128) | soldier, placed |
| 48 | 4 / 14 / 0 | 1 | 2760, 104 | spearman (5.8) |
| 49..51 | 4 / 3 / 2 | 1 | (3056,80) (3200,80) (3328,80) | archer (5.9) |
| 52 | 4 / 7 / 2 | 0 | 2128, 392 | sorcerer mid-boss (5.6) |
| 53 | 4 / 1 / 0 | 0 | 1616, 368 | beast, patrolling (5.2) |
| 54 | 4 / 7 / 0 | 0 | 3616, 128 | boss-arena dialogue trigger (5.11) |
| 55 | 4 / 1 / 3 | 1 | 3744, 72 | scripted beast at the boss arena (5.11) |
| 56 | 4 / 6 / 0 | 0 | 3776, 128 | stage-1 boss (boss spec) |

VERIFIED (ROM dump + trace births at frame 1700, identical positions).

### 2.2 Stage event table (spawners), ROM 0x9AD8
Loaded at stage start by 0x9A92 into the 48-slot event pool at FF8502 (64-byte
records; dispatched every frame by 0x3809C, family byte +0E -> table 0x380C2;
family 4 -> kind table 0x381FA). Entry = 14 bytes: family, kind, variant,
direction, then words parameters(+12), endX(+06), y(+08), startX(+0A),
spawnY(+0C). VERIFIED (0x9AA4..0x9AD2). Stage 1 has 5 events:

| Ev | kind / variant | startX | endX | y / spawnY | par | Meaning |
|---|---|---|---|---|---|---|
| 0 | 7 / 4 | 1280 | 2816 | 0 / 0 | 0 | roaming apparition anti-idle spawner (5.10) |
| 1 | 1 / 1 | 1816 | 1904 | 240 / 240 | 0 | cave beasts: up to 3, spawn at (1904,240) (5.2) |
| 2 | 1 / 1 | 2512 | 2512 | 192 / 192 | 1 | beasts: up to 3, spawn at (2512,192) (5.2) |
| 3 | 0 / 0 | 224 | 1280 | 88 / 88 | 0 | edge soldiers while camera X in [224,1280) (5.1) |
| 4 | 0 / 0 | 2816 | 3072 | 88 / 88 | 0 | edge soldiers while camera X in [2816,3072) |

VERIFIED (ROM dump; trace shows the soldiers and beasts these produce).

### 2.3 Difficulty rank FF8346
A 16-bit rank used by spawners and some AI tables; most tables index it as
`rank >> 12` (0..15) or `(rank >> 8) & 0xF0`.
- +1 every frame (FF79A4 = 0; other FF79A4 modes add 1 every other frame, 2 or
  3), saturating at 0xFFFF. VERIFIED (0x380E8; trace: +600 per 600 frames).
- New game: high byte = FF8291 (0x30 in the traced run = default DIP), low byte
  0 (0x381B0). Stage start (0x38146): if above FF8291, halve the excess; add
  0,8,16,24,24,24 (0x3819A, by stage) to the high byte; subtract
  0x381A0[min(FF839B,30)/2] (8,16,...,120). Losing a life: high byte -13, floor 0
  (0x38134, called from 0x96E2); 0x381BC subtracts 4 (caller 0xA962). SOURCE
  (disassembly read, callers' meaning UNKNOWN: FF839B is probably a
  death/continue count).
- Stage 1 trace: 0x302A at the start, 0x556E at the boss: `rank >> 12` = 3 for
  the first ~1.3 minutes, 4 until ~2.4 min, then 5. VERIFIED (trace).

### 2.4 Roster (stage 1, up to the boss arena)

| Family | Class / kind / variant | Count in stage | x range | Spawned by | HP | Score | Drop |
|---|---|---|---|---|---|---|---|
| Soldier, edge entrant | 4 / 0 / 0 | unlimited while events 3/4 run; at most `cap[rank>>12]` alive (3 at stage start) | camera-relative, camera X 224..1280 and 2816..3072 | events 3, 4 | 1 | 50 | 1 gold coin |
| Soldier, placed | 4 / 0 / 4 | 10 | 768..2528 | table | 1 | 50 | 100 gold (14/16) or 10 gold |
| Soldier, ambush | 4 / 0 / 8 (becomes 4) | 1 | 1616 | table | 1 | 50 | as placed |
| Beast, patrol | 4 / 1 / 0 | 1 | 1616 (patrols 1488..1744) | table | 2 | 100 | 10 or 100 gold (50/50) |
| Beast, emerging | 4 / 1 / 1 | up to 3 + 3 | 1904, 2512 | events 1, 2 | 2 | 100 | 10 or 100 gold |
| Log bridge pieces | 6 / 0 / 0..7 and 0..21 | 8 + 22 | 1512..1624, 1960..2296 | table | - | - | - |
| Treasure chest | 16 / 4 / 0 | 5 | 1296..2608 | table | 6 | - | fixed item per chest (5.4) |
| Hidden object | 16 / 9 / 0 | 1 | one of (896,160) (1248,128) (1312,224) | table | 20 | - | companion (class 16 kind 0) |
| Sorcerer mid-boss | 4 / 7 / 2 | 1 | appears ~2128, roams the waterfall | table | 12 | 500 | 3 x 200 gold |
| Sorcerer shots | 10 / 1, 2, 3 (+ kind 4 flames) | per attack | - | kind 7 | - | - | - |
| Sinking platform | 6 / 1 / 0 | 2 | (2048,80) (2192,80) | mid-boss death | - | - | - |
| Spearman | 4 / 14 / 0 | 1 | 2760 | table | 4 | 200 | UNKNOWN |
| Archer | 4 / 3 / 2 | 3 | 3056, 3200, 3328 | table | 4 | 50 | 10 gold (14/16) or 100 gold |
| Arrow | 10 / 0 / 0 | per shot | - | kind 3 | - | - | - |
| Roaming apparition | 4 / 7 / 4 | 0 or 1 at a time | near player | event 0 (only if the player idles) | 127 | - | none (UNKNOWN) |
| Gold pickups | 14 / 0 / 0..11 | per drop | - | enemy deaths | - | - | - |
| Chest rewards | 16 / 5 (gold), 16 / 6 (heart) | per chest | chest position | chest | - | - | - |
| Shop doors | 12 / 8 / 0, 16 | 2 | 1872, 2688 | table | - | - | - |
| Effects | 12 / 0 (bursts, splashes), 12 / 2 (sparkles) | - | - | various | - | - | - |

Names are descriptive (from rendered trace frames: soldier = armoured Nockmaar
foot soldier with sword; beast = large blue big-headed crawling creature; sorcerer
= red-robed skull-faced flying caster; archer = green armoured bowman). VERIFIED
(trace frames g4100, g4800, g5800, g6300, g8000, g9100). Spearman appearance:
UNKNOWN (never on screen in the trace).

## 3. Common rules for class-4 enemies

| Rule | Value | Tag |
|---|---|---|
| Pool | 50 records x 128 bytes at FF9DC6; updated in slot order every frame by 0xFFA2 (kind byte +20 -> table 0xFFC8: kind 0 0x10054, 1 0x115EE, 3 0x125D0, 7 0x1432C, 14 0x176F6) | VERIFIED |
| Record | +0 active (0 free, 1 live and collidable, 2 drawn but not collidable: hit reactions, appear/vanish), +1 on-screen flag, +2 state (0 dormant/init, 2 active, 4 dying, 6 remove), +3 phase, +4 sub-phase, +6/+8 x.fraction, +0A/+0C y.fraction, +0F facing, +14 animation node, +18 motion table pointer, +1C ticks, +20 kind, +21 variant, +22 timer, +24 floor contact, +2A energy, +2B last energy | VERIFIED |
| Hurt box | the current animation node's geometry: node word +6 -> record at 0x3B1C2 + (word << 3) (signed), 4 words (xOffset, width, yOffset, height). Per-family values in section 5 | SOURCE (same lookup as the player, player.md 5; not traced per enemy) |
| Damage taken | the player's shots/spells subtract their power from +2A (magic.md 3); stage-1 kinds 0, 1, 3, 7, 14 are all **target class 0** (table 0x3A07A) | VERIFIED (table byte = 0 for these kinds) |
| Hit detection | an actor reacts when +2A differs from +2B (sets +2B, switches to its hit or death path); only `active == 1` actors are tested | SOURCE |
| Damage dealt | touching an enemy or enemy projectile costs the player 1 HP (player.md 3.10); no per-enemy damage values exist in the player damage path. Body contact uses the enemy hurt box against the player box | SOURCE (player.md); per-family "harmless" poses UNKNOWN (check pass 0x39A52 sub-passes 0x39ADE/0x39B42 and the class tables 0x39B1A) |
| Off-camera retirement | every 16 (some 32/64) frames: if `x < cameraMinX` (FF838C) the actor is removed (0x21C6). Moving actors are also removed when outside camera [-48, +432] x [-32, +288] (0x2162) | VERIFIED |
| Freeze | a kind-0 soldier more than 128 px left of the camera left edge stops updating its AI until it comes back (still drawn/tested) | SOURCE |
| Score | queued event `0x18nn` (0x1F14 -> handler 0x626A): nn = index into BCD table 0x634C: 0=50, 2=100, 4=200, 6=500, 8=1000, 10=2000, 12=5000, 14=6000, 16=8000, 18=10000, 20=20000, 22=30000, 24=100000, 26=1000000 points, added to FF834A; high score FF703E | VERIFIED (disassembly + trace: score +50 per soldier) |
| Drops | 0x2508 with d0 = drop set: spawns (count) class-14 pickups at the actor position. Sets (0x2544): 0 = [v0], 1 = [v2], 2 = [v5], 3 = [v6], 4 = [v5,v6,v6], 5 = [v6,v6,v6], 6 = [v7 x3], 7..12 = 15-coin showers. Pickup variant v is worth table 0x4E70[v] gold: 1, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 | VERIFIED |
| Sounds | `jsr 0x786A` with d0; per family below. 0x32 = enemy hit, 0x33 = enemy explosion | VERIFIED (call sites) |
| Palette | CPS object palette = node attribute low 5 bits (node word +2 & 0x1F); per family below. Hit flashes are palette-controller sequences (kind 7) | SOURCE |

Animation node format (for the offline tool): +0 piece count (bit 15: long
pieces), +2 attribute (high byte non-zero = one packed WxH block: W = (attr>>8 &
15)+1, H = (attr>>12 & 15)+1, followed by one code word), +4 selector (bit 15:
the next-node pointer is a long stored after the piece list), +6 geometry
index, +8 ticks (0xFFFF hold), +10 piece code words. Next node = node + 10 +
size (size = count*2, doubled if bit 15; 2 if packed), or the long stored there.
Roots: `0x1F30` = table + signed word[facing]; `0x1F40` = direct node; stepper
`0x1F4C`. VERIFIED (disassembly 0x1F30..0x1F86). Some direct roots are single
poses that the code holds by re-setting them; their "next" runs into unrelated
data and must not be followed (marked "held by code" in section 8).

## 4. Effects referenced below (class 12)

- **Burst/explosion**: class 12 kind 0 with variant 4 (large; beasts, chests,
  sorcerer) or 5 (soldier, archer); sound 0x33 played by the caller. SOURCE.
- **Splash**: class 12 kind 0 variant 0 (actor sinking in water/collision id 32;
  sound 0x38) and from falling logs. SOURCE.
- **Sparkles**: class 12 kind 2, one per frame around the sorcerer while it
  materialises (trace: ~30 sparkles at 2112..2152, 368..416 at frame 6152).
  VERIFIED (trace).
Effects never collide. Exact art/lifetimes: UNKNOWN here (class 12 routines
0x3222E, 0x15460).

---

## 5. Families (order of appearance)

### 5.1 Soldier (class 4 kind 0), variants 0, 4, 8

Hit box (all walking/standing poses): **(12,24,18,34)**; falling (12,24,16,32);
knocked/dying (12,24,8,24)/(12,24,16,18). Palette **16**. Energy **1** (any hit
kills). Score index 0 (**50**). VERIFIED (0x102A8 sets +2A/+2B = 1; trace).

**Motion tables** (x, y per facing, 8.8): walk 0x115CE = (+512,-768) /
(-512,-768): 2.0 px/frame with a -3 px/frame ground push (floor probes snap back
up, as player.md 3.1); 0x115D6 = (+-576,-384) 2.25 px/frame; death/knock 0x115DE =
(+-384,-384) 1.5 px/frame; edge-entrant walk 0x115E6 = (+640,-768) right,
(-512,-768) left (2.5 / 2.0 px/frame). VERIFIED (ROM + trace: placed soldier
moves 20 px per 10 frames; dying soldier drifts 1.5 px/frame).

**Gravity** (falls, jumps): speed -= 40 per frame down to -1920 (7.5 px/frame),
then fixed 7 px/frame. Jump arc uses -48/frame. Death arc uses -64/frame. SOURCE
(actor4_zero_motion.h, actor4_zero.h).

#### Wake-up (state 0, 0x10068)
- **Variant 0 (edge entrant)**: if `y - cameraY > 320` -> removed. x = cameraX - 32
  (facing 0, enters from the left) or cameraX + 416 (facing 1, from the right).
  Then y -= 7 per frame until a floor is found under the forward foot; if the
  body centre is inside solid ground (descriptor flag 0x800) it is removed at
  once (trace: many 2-frame births). Then walk (phase 0), motion 0x115E6.
  VERIFIED (0x1007E..0x10120 + trace).
- **Variant 4 (placed)**: every 4th frame (clock + slot), when on screen
  (`x - cameraX <= 416`, `y - cameraY <= 256`, unsigned): y -= 7 per frame until it
  lands, then phase 8 (search) with a random pause 0x101F2[rand&3] = 32, 64, 96,
  128 frames and the "rise" animation (table 0x11226). VERIFIED (0x1017A..0x101EE).
- **Variant 8 (ambush)**: every 4th frame, when the player is within 48 px both
  ways (`|y - playerY| < 48`, `|x - playerX| < 48`): y = cameraY + 264 (above the
  top of the screen), variant becomes 4, phase 6 (dive) at its falling step with
  40 frames and vertical speed -1, falling pose 0x1145A. VERIFIED (0x10232..0x10298
  + trace: at frame 5302 y = 310 = cameraY 47 + 263).
- All: energy 1, motion table 0x115CE. 

#### Active state (state 2): phases (0x102C0 table 0x102F4)
Order each frame: AI phase (skipped while frozen or FF83D2 pause), every 16
frames the min-X retirement, then the damage check (FF83D0 set -> explode: sound
0x33, burst variant 5, removed, no reward), publish.

| Phase | Name | Behaviour | Tag |
|---|---|---|---|
| 0 | Walk | entering: face the player, timer 60, walk pose (table 0x1116E, 6 x 6 ticks loop). Each frame move 2.0 px/frame + ground push. Every 16 frames, if `|x - playerX| < 28`: pick a delay 0x10420[rand&7] = 16,24,32,32,40,48,56,64 frames, keep walking that long, then stop and turn toward the player (4 frames), wait, then: if the player is not within +-25 px vertically and 0x104A8[8 + rand&15] != 0 (9/16; the table runs into code bytes, keep them) walk 32 more frames then hold guard pose (table 0x1133A) 28 frames and retry; else resume walking toward the player. Every 60 grounded frames, if the player is more than 25 px above/below and 0x103E4[rand&15] != 0 (6/16): **change level** (see recovery). If the floor ends under the forward foot: probe (x+-16, y-33): a non-thin floor tile there -> phase 20 (walk down), else phase 8 (search) | SOURCE (actor4_zero_walk.h); tables VERIFIED |
| 2 | Jump up | wait timer (16), crouch pose (0x114BA) 32 frames, y -1, then rise: y +12, speed 1408 (5.5 px/frame), -48 per frame, pose 0x114D6; when falling, land on any floor; landing pose 10 frames, then phase 0 (or 8 if not grounded) | SOURCE (actor4_zero_motion.h; ROM 0x10500..0x105DA matches) |
| 4 | Ledge climb | 4 steps of 8 frames using offsets 0x10636: (+-32,+10), (0,+32), (0,+10), (-+8,0) (x sign by facing), step pose table 0x114F2; then phase 0, or 14 (fall) if no floor | SOURCE; table VERIFIED |
| 6 | Dive / drop | (used by variant 8 and by "drop to a lower level") pose 21 for 8 frames, rise at speed 384 with gravity 40, falling pose 24 frames, keep falling; **if the player box is near** (`|y - playerY| < 34`, `|x - playerX| < 28`) leap: speed 896 up, facing = player's facing, motion 0x115E6, 35 frames, wall-latched; on landing pose 8 frames, then phase 0/8. Falling into water (centre collision id 32): splash, sound 0x38, removed | SOURCE (actor4_zero_dive.h) |
| 8 | Search | look ahead 32 px (0x10878 = +-32): solid ahead at y-16 -> phase 12 (hop); floor ahead at y-41 -> phase 20 (step down) or, if thin (flag 0x80), phase 14 (drop through); player within +-30 px vertically -> turn round; otherwise stand (pose table 0x11226) for 32/64/96/128 frames, then try "change level", then turn and walk 60 frames re-facing the player every 60 frames | SOURCE (actor4_zero_search.h) |
| 12 | Hop | speed 896 for 6 frames, then fall to the floor, then walk 4 frames, then phase 0 (8 if no floor) | SOURCE |
| 14 | Fall/drop | speed -512, fall with gravity until grounded, then phase 0 | SOURCE |
| 16 | Turn | turn pose (0x1136E) 8 frames, then resume the saved phase | SOURCE |
| 18/20 | Off edge / step down | 18: fall straight down (active 2) until removed off-screen; 20: keep walking down until grounded, then turn toward the player | SOURCE |

**Change level ("recovery", 0x10C9C)**: skipped while the player-gate flag
(word FF988E = player record +0x48, tested at 0x1046C; native `playerGate`) is set and in arcade stage index 3. If the soldier
is already within +-30 px vertically of the player nothing happens. Player above: probe 4 cells upward at x+-40 (start y+39,
step 16) for solid ground -> jump (phase 2) facing the player. Player below:
probe downward from y-80 (step -16) -> drop (phase 6). SOURCE.

#### Death (state 4, 0x10E7A)
Sound **0x32**; flies up and away from the player: speed 1024 (4 px/frame) up,
-64 per frame, horizontal 1.5 px/frame (motion 0x115DE), pose table 0x1155E; when
falling, land on a floor (or sink: water -> sound 0x38, splash, removed without
reward); on landing sound **0x33**, burst (class 12 kind 0 variant 5), y +6,
pose table 0x11562 for 10 frames, then score 50 and the drop; removed. When an
edge entrant (variant 0) is removed the event population FF0F40 is decremented.
VERIFIED (trace: rise from y 81 to 104 and drift 1.5 px/frame; score +50; drop),
details SOURCE (actor4_zero.h).

**Drop**: variant 0 -> set 0 (one 1-gold coin). Other variants -> set
0x1114A[rand&15] = 2 (one 100-gold pickup) 14/16, or 1 (one 10-gold) 2/16.
VERIFIED (ROM + trace: var 4 soldiers drop pickup variant 5).

A hit that does not kill (impossible with energy 1 in stage 1) uses a 24-frame
knockback: +-24 px jump, speed -+640 changing by -+24 per frame, vertical -384.
SOURCE (0x11026).

#### Edge-soldier event (family 4 kind 0 variant 0, 0x38362)
- Waits until `cameraX >= startX`. Then, whenever `cap[rank>>12] > FF0F40`
  (alive edge soldiers; cap table 0x383C0 = 1,2,2,3,3,3,4,5,5,6,6,6,6,7,7,8), loads
  a delay 0x383D0[rand&15] = 16,128,192,64,128,48,64,128,48,128,64,32,64,128,192,64
  frames; when it expires spawns one variant-0 soldier with facing
  0x384D4[rand&15] (1 = from the right 14/16, 0 = from the left 2/16), y = spawnY,
  FF0F40 += 1, and waits again. VERIFIED (ROM; trace: event 3 produced ~25
  soldiers, mostly from the right).
- Retires when `cameraX >= endX`, when FF83D0 is set, or (every 16 frames) when
  `endX < cameraMinX`. VERIFIED (0x38390..0x383B8).

### 5.2 Beast (class 4 kind 1), variants 0 and 1

Hit box (all poses) **(13,26,26,44)**; palette **3**; energy **2**
(`move.w #0x0202,+2A`); score index 2 (**100**); drop set 0x11DE2[rand&15] = 1 or 2
(10 or 100 gold, 50/50). VERIFIED (ROM + trace hp 2, drop var 2).

Motion tables (8.8, +right/-left): walk 0xED070 = 384 (1.5 px/frame), travel
0xED068 = 256, script 0xED078 = 512, **charge 0xED088 = 768 (3.0 px/frame)**.
VERIFIED (ROM). Root r = table 0x11FC8 + 4r (+ facing word); see section 8.

**Variant 0 (placed, 1616,368)**: patrol limits x+-128 (1488..1744), pause timer
0x1169E[rand&15] = 120..240 frames. Active immediately. SOURCE.
**Variant 1 (event)**: emerges at the event position: pose root 0 for 24 frames
(not collidable, active 2), then x -= 8 and patrols. Limits come from the event
(event 1: 1752..1880; event 2: 2472..2560). VERIFIED (trace: born at 1904,240) /
SOURCE (timing).

Active phases (actor4_one.h, 0x11622..):
| Phase | Behaviour |
|---|---|
| 2 Walk | walk pose (root 2) at 1.5 px/frame. Every 4 frames: if the player is in range (beast y in [playerY-40, playerY+8] and playerX inside the patrol limits) -> engage: turn first if facing away, else charge. At a limit -> turn. Pause timer expiry -> phase 6 |
| 4 Turn | from a charge: skid at 768 decreasing by 24/frame (root 4) then turn; from walking: root 6 for 16 frames then turn |
| 6 Pause | stand (root 3) 60 frames (engages if the player comes in range), then **roar**: sound **0x8A**, root 7 for 58 frames |
| 8 Charge | root 1, 3.0 px/frame; past (limit - 32) -> skid-turn; when `|x - playerX| <= 8` -> phase 10 (overrun). Every 4 frames: if the player leaves the wide range (beast y outside [playerY-72, playerY+8], or within 48 while the player stands on it) -> back to walk |
| 10 Overrun | slide 768 decreasing 16/frame (root 4) until the limit, then root 5 for 8 frames, turn, charge again |
SOURCE (structure); speeds VERIFIED.

Hit (energy 2 -> 1): state 4, not collidable, sound **0x32**, facing set away
from the player and it slides that way at 768 decreasing 24/frame (root 8); then root 10 for 6 frames and resumes
walking. Off a ledge during the slide it falls (speed +32/frame up to 1792) until
it lands. Death: same slide, then sound **0x33**, burst variant 4, root 9 for 8
frames, score 100, drop; removed. SOURCE (actor4_one.h).

Every 64 frames: removed if `x < cameraMinX`. SOURCE.

#### Beast event (family 4 kind 1, 0x387CC area)
Starts on its first update: quota 1, 3 spawns left, limits = startX+64/-64
(parameters 0) or +48/-40 (parameters 1). Every 4 frames while dormant: when
`endX - cameraX <= 384` and `y - cameraY - 16 <= 224` -> spawn at (endX, y).
After each spawn the timer is 0x387C8[rand&3] = 64, 96, 128, 180 frames; the next
beast spawns when the timer runs out **and** the previous one has died (quota
restored by the child's death). After 3 spawns, or when endX < cameraMinX (checked
every 64 frames), the event retires. VERIFIED (trace: event 1 beasts born at
5512, 5712, 6014, each ~96 frames after the previous death) / SOURCE (quota
mechanism).

### 5.3 Log bridges (class 6 kind 0)

Each log is one 64-byte class-6 record (pool FFB6C6, 48 slots, kind table
0x2F372 -> 0x2F470). Variant = index along the bridge; direction = which bridge
(1 = short bridge at y 157, 0 = long waterfall bridge at y 285). Logs are
**standable surfaces** (platform collision, class 6 is the platform class);
they never hurt. Palette **11**; single pose root 0x2F612 (node chain of 4).
VERIFIED (ROM + rendered frames g5300, g6300).

- Init: rest heights y, y-2, y-4 (+30/+32/+34); sag step timer 4; collapse
  delay = 0x2F4E8[variant] = 6*variant+1 frames (1, 7, 13, ..., 127). Log 0 of
  each bridge clears its bridge's collapse flag (FF0F08 for direction 1, FF0F09
  for direction 0). VERIFIED (0x2F484..0x2F4E4).
- **Sag**: +24 selects the target height: 0 -> back to rest, 1 -> y-2, 2 -> y-4;
  moves 1 px every 4 frames toward it. The writer of +24 (the platform
  collision pass, presumably 2 under the player and 1 next to him) is UNKNOWN
  (search writes to class-6 +24 in pass 0x39A52). VERIFIED (logic) / UNKNOWN
  (writer).
- **Collapse**: when the bridge's flag is set, each log counts down its delay,
  then becomes not-standable (active 2) and falls: speed -= 40 per frame to
  -1920; every 4 frames the centre material is checked, and on water (negative
  result of 0x36F4) it spawns a class-12 kind 0 splash and is removed; also
  removed off camera. Result: the logs drop one after another from the bridge's
  start, ~6 frames apart. VERIFIED (ROM; trace: long bridge logs removed in order
  over frames 8366..8494 after the mid-boss died).
- Long bridge flag FF0F09 is set by the sorcerer's death (0x14F1A). The short
  bridge (FF0F08) is never collapsed in stage 1; it is removed when the camera
  min X passes it (gate opening). VERIFIED (trace: removed at 7042..7058).

### 5.4 Treasure chest (class 16 kind 4) and its rewards

Class 16 = stage objects (pool FFEAC6, 20 slots, table 0x36CA4: kind 4 0x37A08,
5 0x37CEC, 6 0x37DD0, 9 0x374FA). VERIFIED.

- Hit box (record +34..+3A) **(12,24,14,17)**; energy **6**. Pose 0x37CC6
  (palette 2). If its broken bit (FF8387 bit = chest index, i.e. the entry's
  direction 0..4) is already set, it is removed at load (stays broken after a
  continue). VERIFIED (0x37A1C..0x37A54; trace energy 6).
- Only the player's shots damage it (it scans the class-8 pool itself, 0x37B5E,
  box overlap with each live shot). Each hit: sound **0x32**, not hittable for
  12 frames. A chest never hurts the player and does not block movement.
  VERIFIED (code) / SOURCE (power use: subtracts shot power like enemies).
- Energy 0: set the broken bit, pose 0x37CD6 for 8 frames, then sound **0x33** +
  burst (class 12 kind 0 variant 4), 8 more frames, then spawn the reward object
  (class 16) from table 0x37EA6[stage] + rewardSet*10 + chestIndex*2 = (kind,
  variant), and remove the chest. VERIFIED (0x37A76..0x37B58).
- **rewardSet** FF83A4 = FF79A2, incremented modulo 4 at each new game (0x2D4C):
  the chest contents rotate between plays. Trace run used set 1. SOURCE.

Stage 1 rewards (kind/variant; chest index = entry direction):

| Set | chest 0 (1296,352) | 1 (1904,384) | 2 (2416,384) | 3 (2608,368) | 4 (1904,144) |
|---|---|---|---|---|---|
| 0 | heart | gold v0 | gold v5 | gold v0 | gold v6 |
| 1 | gold v0 | gold v0 | heart | gold v5 | gold v6 |
| 2 | gold v6 | gold v0 | gold v0 | heart | gold v5 |
| 3 | gold v5 | gold v6 | gold v0 | gold v0 | heart |
VERIFIED (ROM 0x37EB2).

**Reward gold object (class 16 kind 5)**: static at the chest position (variant
0 is drawn 8 px lower, pose root 6; other variants pose root 5). Collected by
overlap (checked every 8 frames, player box expanded 12 px each side): sound
**0x2D**, gold += 0x4E70[variant] (v0 = 1, v5 = 100, v6 = 200), shows label pose
27+variant rising with speed +4/frame up to 768 for 32 frames, then removed.
SOURCE (stage_objects.h); gold table VERIFIED.
**Heart (class 16 kind 6)**: drawn 4 px lower; on overlap sound **0x2D**, HP +1
(variant 0; variant 1 gives +2), capped at max HP, HUD redrawn, removed. SOURCE.

### 5.5 Hidden object (class 16 kind 9)

- If bit (variant) of FF83A5 is set (already found this game) it is removed.
  Otherwise it moves to a random position from 0x37542[stage] + 0x37532[rand&15]:
  stage 1 = **(896,160)** 6/16, **(1248,128)** 5/16, **(1312,224)** 5/16. Hit box
  **(10,20,16,20)**, energy **20**. VERIFIED (ROM; trace: placed at 1248,128,
  energy 20).
- Invisible unless the player owns item 3 (FF8355 non-zero, the "reveal" item):
  then it shows pose 0x3784E (animated). Without the item it uses an empty pose
  but **can still be shot** (the shot scan does not check visibility). VERIFIED
  (0x375AA..0x375EC) / SOURCE (what item 3 is).
- Each hit: sound **0x32**, 16 frames not hittable. Energy 0: set FF83A5 bit,
  spawn class 16 **kind 0** (the companion; behaviour belongs to the companion
  spec, COMPANION.md) at its position; removed. VERIFIED (0x3762A..0x37654).

### 5.6 Sorcerer mid-boss (class 4 kind 7 variant 2) and its attacks

Placed at (2128, 392) (below the long bridge, out of view). Hit box **(12,24,14,30)**
in flight/attack poses, (12,24,18,34) in its appear poses; palette **21** in the
pose data, record palette 22 with flash colours (see hit). Energy **12**. Score
index 6 (**500**). Drop set 5 = **three 200-gold pickups**. VERIFIED (ROM 0x143BE..
0x143DA, 0x14F2E..0x14F3A; trace energy 12, three variant-6 pickups at its death).

**Wake-up (state 0)**: every 16 frames, when on screen (`x - cameraX <= 384`,
`y - cameraY <= 256`) **and FF8470 is set** (player in the arrival script at the
camera stop 1952): FF83E6 += 1 (encounter active), sound **0x8C**, active 2, timer
20, remembers the player position, pose 0x14FE8. VERIFIED (0x14390..0x14410;
trace: woke at frame 6152, 12 frames after FF8470 was set).

**Appear (phase 0)**: 20 frames emitting one sparkle (class 12 kind 2) per frame;
pose 0x14FE8 for 24 frames with sparkles; pose 0x15004 for 20 frames; then
collidable (active 1), phase 2, motion toward the player's side, aim pose.
SOURCE (actor4_seven.h).

**Flight (phase 2)** and **tracking** (applied in phases 2 and 4):
- Every 8 frames (clock + slot) it re-reads the player: for x and y separately,
  step = +-2 px toward the player, counter = |distance| / 2; each frame while the
  counter is not exhausted it moves 2 px on that axis. So it covers half the
  distance to where the player was, at 2 px/frame per axis. SOURCE.
- Plus a wobble: every 3 frames a random direction 0..15 of table 0xED020
  (radius 80 in 8.8 = 0.31 px/frame), added every frame. VERIFIED (table) /
  SOURCE (rule).
- Flight also adds the direction vector of its motion table (0x14F68 "left"
  set or 0x14FA8 "right" set, 16 directions, length 384 = 1.5 px/frame), facing =
  16-direction aim at the player. Every 8 frames the aim is recomputed; if it
  changed, retarget table 0x14C2C[aim] gives either a new flight direction
  (value 0xFF: keep flying, new pose) or an attack pose 0..3 (aims 4, 5, 11, 12 ->
  poses 1, 3, 2, 0) which starts the attack phase. SOURCE / tables VERIFIED.

**Attack (phase 4)**:
1. Wait (timer), then **teleport**: facing ^= 2, new position = player + offset
   0x14CE0[facing&3] = (+160,0), (-160,0), (+144,+56), (-144,+56) (variant 2: if
   the player is more than 60 px from the screen centre (cameraX+192), facing bit 0
   is chosen so the sorcerer lands on the player's screen-centre side, i.e. the
   side with more room; otherwise it keeps its side), then a
   **blink**: 8 toggles of 3 frames (visible/hidden), not collidable.
2. Hover (attack motion 0x14F58: (-+128,0) or diagonal (-+118,-48): 0.5 px/frame)
   for delay 0x14622[rank>>12] = 32,48,40,32,32,32,24,22,20,18,16,14,12,8,8,8 frames;
   then roll chance table 0x14642[(rank>>8 & 0xF0) + rand&15] (row r has r+3
   ones of 16, e.g. row 3: 6/16): success -> cast, failure -> wait again.
3. Cast: pose 8+facing for 14 frames, then **fire** (below), pose 10 frames,
   back to step 2.
4. If it drifts off screen (`x - cameraX > 400` or `y - cameraY > 272`) or comes
   within 80 px horizontally of the player, it blinks out (30 frames) and
   re-teleports.
SOURCE (actor4_seven.h); tables VERIFIED (ROM).

**Fire** (0x14852): one random pick 0x14842[rand&15] = attack kind 2 (10/16),
1 (4/16) or 3 (2/16); all are class-10 records (pool FFC7C6, 20 slots,
table 0x2353C):
- **Kind 1 slow fireball** (0x23D76): moves (-+512, 0) or diagonally down
  (-+472, -195): 2 px/frame; pose table 0x24046 (7 nodes x 6 ticks, palette 6),
  hit box (12,24,16,32). Removed off camera (checked every 8 frames). Facing
  0..3 = left, right, down-left, down-right (the cast pose's aim). VERIFIED
  (ROM tables) / SOURCE (rule).
- **Kind 2 fast fire squiggle** (0x23DC0): (-+1024,0) or (-+945,-391): 4 px/frame;
  pose table 0x23F6E (4 nodes x 3 ticks, palette 8), hit box (12,24,6,22).
  VERIFIED (ROM + trace: kind 2 shots crossing the bridge at ~4 px/frame).
- **Kind 3 ground fire** (2/16): two emitters at playerX+40 (variant 0) and
  playerX-40 (variant 4), player's y; each checks the floor under it (16 px
  below) after 16 frames (pose 0x240FE); if there is floor it emits 3 flame
  children (class 10 kind 4) 24 frames apart, moving 16 px toward the player each
  time (step -16 / +16, table 0x23ED2). If the player is in a gate script the
  pick becomes kind 1. Flames' own behaviour (class 10 kind 4) is UNKNOWN here
  (routine at 0x24152). SOURCE.
All its shots cost the player 1 HP on contact (player.md 3.10). Not destroyable
by player shots: UNKNOWN (check class-10 entries in the shot-vs-target pass).

**Hit**: energy changed and > 0: invulnerable 28 frames (active 2), sound
**0x32**, palette-controller flash (sequence 13, delay 3, colour set 0xE6B60 for
variant 2), keeps attacking. SOURCE.

**Death** (energy 0, 0x14EE0): spawns the two **sinking platforms** (class 6
kind 1 at (2048,80) and (2192,80), 5.7), sound **0x33**, burst variant 4, sets
FF0F09 (long bridge collapses, 5.3), **FF83E7 += 1 (opens the camera gate)**,
clears FF83E6 and FF83D0, score 500, three 200-gold pickups; removed. VERIFIED
(disassembly 0x14EE0..0x14F3A; trace frame 8362: platforms, pickups, bridge
collapse, FF83D0 cleared).

### 5.7 Sinking platforms (class 6 kind 1, variant 0)

Created by the sorcerer's death at (2048,80) and (2192,80) (y 80 = low over the
water, below the collapsing bridge). Single pose 0x2F734 (palette 28). Standable.
- Phase 0: wait until stood on (+24 set by the platform pass).
- Phase 2: 64 frames.
- Phase 4: sinks 0.5 px/frame (-128 in 8.8) for 96 frames (48 px), then is removed.
Variant 1 (not used in stage 1) starts at (2336,280) and rises 0.5 px/frame for
96 frames then stays. VERIFIED (0x2F65E..0x2F732) / UNKNOWN (+24 writer, as 5.3).

### 5.8 Spearman (class 4 kind 14 variant 0) — partial

Placed at (2760,104). Hurt box: **UNKNOWN** (its nodes' geometry words decode to
nonsense with the standard lookup; check whether kind 14 uses record +30..+36 or
a different geometry base). Palette **18**. Energy **4** (`0x17786`). Score index 4
(**200**, 0x1804). VERIFIED (ROM).
- Wake-up: phase 0 waits until `x - cameraX <= 448` and `y - cameraY <= 276`
  (unsigned), phase 2 probes the floor, phase 4 activates: energy 4, pose table
  0x186A4. VERIFIED (0x1773A..0x177B2).
- Active (from actor4_spear.h, SOURCE): walks 40 frames (motion 17) re-checking
  the player, stands 40 frames, picks an action from a distance-indexed row
  table (rows by `|dx| >> 4`, random column, difficulty block): walk, step
  back, **throw** (42 frames wind-up, spawns a returning projectile at
  x+offset, y+8, sound **0x8B**, 12 frames follow-through, then waits up to 200
  frames for the projectile to come back within 64 px), jump (20 frames),
  climb/drop by terrain probes 40 px ahead. Retired every 32 frames if
  `x < cameraMinX`; explodes if FF83D0 is set.
- Trace: it activated at frame 8042 and was destroyed 2 frames later by FF83D0
  (set because the harness let the player pass the gate with the mid-boss alive),
  so its real stage-1 behaviour was **not observed**. Projectile kind, speed,
  return rule and drop set: UNKNOWN (routine 0x176F6..0x18BD8, returning shot in
  `actor10_returning*.h`). Record a scripted trace with the mid-boss killed.

### 5.9 Archer (class 4 kind 3 variant 2) and arrow

Placed at (3056,80), (3200,80), (3328,80). Hit box **(12,24,18,34)**; palette
**17**; energy **4** (0x12674). Score index 0 (**50**). Drop 0x13120[rand&15]: set 1
(10 gold) 14/16, set 2 (100 gold) 2/16. VERIFIED (ROM; trace drops variant 2).

- Wake-up: on screen (`x - cameraX <= 384`, `y - cameraY <= 288`) -> lands on the
  floor, phase 8 (patrol). VERIFIED/SOURCE.
- Freeze: AI paused while more than 64 px left of / 448 px right of the camera
  or 288 px above it. SOURCE.
- **Patrol (phase 8)**: face the player, walk (motion table 0x136B8 pair, walk
  pose table 0x13154) for up to 60 frames, then stand (table 0x1315C) 56 frames,
  repeat; turn round after 64/128 frames or at a ledge. Every 8 frames: if the
  player is within +-48 px vertically and +-160 px horizontally -> prepare.
  SOURCE (actor4_three_patrol.h).
- **Prepare (phase 4)**: face the player, pose 4 frames, draw pose 4 frames.
- **Aim and shoot (phase 6)**: draw (table 0x13164) 48 frames; compute the
  16-direction aim at the player (0 or 8 are bent to the facing side: 1/15 or
  7/9); rotate the bow one step every 8 frames toward it (rotation poses table
  0x1316C[dir]); 16 frames on target; then sound **0x34** and spawn an **arrow**
  (class 10 kind 0, direction = aim, position = archer + offset 0x12E46[dir]);
  firing pose table 0x1318C[dir]. Then 16 frames; if grounded stand 7 frames and
  restart walking; otherwise wait 40 frames and re-shoot with a difficulty-masked
  random chance. SOURCE; tables VERIFIED.
- Hit (energy left): recovery pose table 0x13634; death: airborne pose
  0x19DFE, landing 0x19E02 10 frames, sound **0x33**, burst variant 5, score, drop.
  SOURCE (0x12FA4).
- FF83D0 set: explodes. VERIFIED (reader 0x12E86).

**Arrow (class 10 kind 0 variant 0, 0x2359C)**: speed 4.0 px/frame along its
direction (table 0xED524: e.g. dir 12 = (-1024, 0), dir 13 = (-945, +391)); pose =
table 0x23856[dir] (single node, palette 17), hit box (12,24,16,18); lives 64
frames after a 1-frame start; every 4 frames a terrain probe at its tip (table
by direction): hitting a floor/wall ends it; end: 12-frame break pose (table
0x23B06[dir]); removed off camera (every 16 frames) or when FF83D0 is set.
VERIFIED (direction table, trace: arrows heading left at y 84) / SOURCE (timing).

### 5.10 Roaming apparition (event 0 -> class 4 kind 7 variant 4) — partial

Anti-idle spawner while `1280 <= cameraX < 2816`: timer 600 frames, reset
whenever any class-4 enemy is visible, the camera is outside the range, a
mid-boss encounter is active (FF83E6) or an apparition is already out. When the
timer expires it spawns kind 7 variant 4 at (playerX, playerY+80): energy 127,
same appear/flight/attack logic as 5.6 with its own chance row (0x14642+256: all
zero, so it never fires in stage 1 at rank < 16) and relocation facing
0x14C92 = 1 or 3. It is cancelled (sent to its exit) when more than one other
enemy becomes visible or an encounter starts. Retires when cameraMinX >= 2816.
SOURCE (stage_seven_spawn.h, actor4_seven.h); never seen in the trace. Its exit
path (phase 12) and death reward: UNKNOWN.

### 5.11 Boss arena entrance (camera 3328) — owned by the boss spec

Present from the stage table, listed so they are not spawned twice:
- **Kind 7 variant 0** at (3616,128): when within 384 px of the camera and the
  boss is not already beaten (FF838B clear) it starts the **dialogue scene**:
  pose, sound 0x01 and 0x1FA, message 0x0424/0x0306, waits for the message, then
  spawns a class-18 actor at (3600, cameraY+56) and leaves. Trace: frames
  9878..10336; class 18 slot 7 at (3600,56) from 10308. SOURCE (actor4_seven.h
  script) / VERIFIED (trace timing).
- **Kind 1 variant 3** at (3744,72): scripted beast (walks 72 frames at 2 px/frame
  when a script flag allows, then waits; removed if the boss gate is passed).
  SOURCE.
- Kind 6 (3776,128) = the boss; kinds 10/11 and class 10 kind 5 appear during
  the fight. See the boss spec.

### 5.12 Shop doors (class 12 kind 8)

(1872,311) variant 0 direction 4 and (2688,199) variant 16 direction 5: shop
entrance triggers (`actor12_shop.h`); harmless, not enemies. The blue
owl-hooded figure seen at the first one (frame g5800) belongs to it. Shop
behaviour is out of scope here. VERIFIED (table, class-12 kind-8 spawn rule).

## 6. Gold pickups (class 14 kind 0), every enemy drop

Pool FFDBC6 (60 x 64), routine 0x2CFC2. VERIFIED (trace: 46 pickups in stage 1,
variants 0, 2, 5, 6).

| Item | Value | Tag |
|---|---|---|
| Value | variant v -> 0x4E70[v] gold: 1, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 (BCD, added to FF8368; an overflow leaves the old total) | VERIFIED |
| Count index | a drop of n pickups gives them indexes n-1..0; horizontal speed 0x2D032[index] = 0, +64, -64, +128, -128, ..., +448, -448, +512 (single drops fall straight; showers fan out) | VERIFIED |
| Launch | stepY = bounce = 768 (3 px/frame up), timer 16, pose = table 0x2D314[variant] | VERIFIED |
| Rise | y += stepY, stepY -= 32; at < 0 -> fall | SOURCE |
| Fall | y -= stepY, stepY += 32 up to 896 (3.5 px/frame); no landing test for the first 16 frames; then odd frames test platforms, even frames the floor | SOURCE |
| Bounce | on landing stepX halves; bounce = (bounce/2) * 1.5 (768 -> 288 -> 108 -> 40 -> 15 -> 5 -> 1 -> 0); non-zero -> rise again with that speed; zero -> rest | SOURCE |
| Edges | when moving, every 4 frames: if past camera+8 (left) or camera+376 (right) the horizontal speed reverses | SOURCE |
| Collection | every 8 frames (clock + slot): player hurt box expanded by 12 px each side overlaps the pickup point -> sound **0x2D**, gold added (twice if the owner byte +2F is set; what sets it on enemies is UNKNOWN), label pose 12+v (24+v with owner) shown at player (x, y+24) rising (speed +4/frame up to 768) for 32 frames, then removed | VERIFIED (sound, gold) / SOURCE (rest) |
| Expiry | age +1 per frame; at age >= 300 while bouncing/resting: blink pose (root 36) 12 frames, removed | SOURCE |
| Far away | every 64 frames: removed if more than 96 px outside the camera view | SOURCE |
| Hit box | none (point test above) | SOURCE |

Hearts and the stage-object gold are class 16 (5.4), not class 14.

## 7. Sounds used (command numbers, `jsr 0x786A`)

0x2D pickup collected; 0x32 enemy hit (soldier, beast, chest, hidden object,
sorcerer); 0x33 explosion (deaths, chest break, sorcerer death); 0x34 arrow;
0x38 enemy sinks in water; 0x8A beast roar; 0x8B spearman throw; 0x8C sorcerer
appears; 0x01 + 0x1FA dialogue at the boss arena (5.11). VERIFIED (call sites in
the routines above) except 0x8B/0x01/0x1FA (SOURCE).

## 8. Animation roots (for the offline extraction tool)

Format and walk: section 3. "(held)" = a pose set and held by code; do not
follow its next pointer. Ticks listed per node. All addresses are ROM.

**Soldier (kind 0)**, palette 16:
| Table (facing 0 / 1 root) | Nodes | Use |
|---|---|---|
| 0x1116E (0x11182 / 0x111DA) | 6 x 6, loop | walk |
| 0x11226 (0x1122A / 0x112B2) | 8, 12 x 8, hold | rise / look around (variant 4 wake, search) |
| 0x1133A (0x1133E / 0x11356) | 8, hold | guard |
| 0x1136E (0x11372 / 0x1138E) | 3, hold | turn |
| 0x11416 (0x1141A / 0x11426) | 12, 12, 4, 4 loop | fall |
| 0x1145A direct | 4 (held) | ambush fall pose (variant 8) |
| 0x11466 (0x1146A / 0x11476), 0x11482 (0x11486 / 0x11492), 0x1149E direct | 4..6 | dive poses |
| 0x114BA (0x114BE / 0x114CA), 0x114D6 | 8 (held) | jump crouch / rise |
| 0x114F2 (0x114F6 / 0x1152A) | 8, 8, 8, hold | ledge climb steps |
| 0x1155E (0x1159A / 0x11566) | 8, 8, hold | death / knock |
| 0x11562 (0x115BE / 0x1158A) | hold | death landing |

**Beast (kind 1)**, palette 3, every pose (13,26,26,44): root r = table
0x11FC8 + 4r: r0 0x11FC8 emerge (7 nodes), r1 0x11FCC charge (6, loop), r2 0x11FD0
walk (4, loop), r3 0x11FD4 stand, r5 0x11FDC, r6 0x11FE0 turn (2), r7 0x11FE4 roar
(4), r8 0x11FE8 hit (2), r9 0x11FEC death, r10 0x11FF0 recover (2), r11 0x11FF4,
r12 0x11FF8, r13 0x11FFC (2, loop). r4 (0x11FD8, skid) was not found by the
lea/jsr scan: check it. SOURCE (mapping from actor4_one.h root numbering).

**Archer (kind 3)**, palette 17, (12,24,18,34): walk 0x13154 (6 loop), alt walk
0x13158, stand 0x1315C (3), prepare 0x13160 (7), draw 0x13164 (6), 0x131AC/0x131B0
(single), entry 0x1314C (9), recovery 0x13634 (5), death air 0x19DFE, death
landing 0x19E02; aim rotation 0x1316C[16 dirs], firing 0x1318C[16 dirs].

**Sorcerer (kind 7)**, palette 21 (box (12,24,14,30)): appear 0x14FE8 (2, loop),
0x15004; flight 0x15026[facing 0..3] (3, loop); attack poses 0x1502E[0..3]
(3); cast 0x151AE[0..3]; 0x150FE, 0x1528A direct. Aim-indexed flight poses
come from the same tables via the retarget rule. Sparkle effect 0x15498
(palette 23).

**Spearman (kind 14)**, palette 18: 0x18588, 0x186A4 (stand/walk), 0x18794,
0x187B0, 0x18808, 0x1880C, 0x18948, 0x1894C, 0x18950 (facing pairs), 0x18B78
direct. Use per table: UNKNOWN.

**Projectiles** (class 10): arrow 0x23856[dir 0..15] (palette 17), arrow
variant 1 0x2391E[dir] (palette 23), arrow break 0x23B06[dir], stuck arrow
0x23AF6; sorcerer slow fireball 0x24046[0..3] (palette 6), fast squiggle
0x23F6E[0..3] (palette 8), ground-fire emitter 0x240FE.

**Objects**: log 0x2F612 (palette 11); sinking platform 0x2F734 (palette 28);
chest 0x37CC6, broken 0x37CD6 (palette 2); hidden object empty 0x375C4,
revealed 0x3784E; pickups table 0x2D314[variant] (offset table), expiry blink
root 36, labels 12+v; stage-object gold/heart roots in table 0x2D32C area
(roots 5, 6, 7, 8, 27+v): addresses UNKNOWN (stage_objects animation builder).

Full chains with per-node ticks, palettes and boxes:
`/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/enemies/k0.roots` (and `k1`, `k3`, `k7`, `k14`, `c6`, `c10`, `c16`, `c14`).

## 9. Open questions (UNKNOWN), where to look

1. **FF83D0 writer** (encounter "clear the screen" flag set at camera ~1822 and
   ~3137) and **FF8470 writer** (arrival flag): search word/long writes covering
   FF83D0/FF8470 (e.g. `move.w ...,FF83CE` / `FF846E`) and the player state-14
   arrival script. Needed for the mid-boss start.
2. Exact **player-vs-enemy contact** rules: cadence (pass 0x39A52 alternates
   sub-passes by frame & 3), which poses are harmless, whether enemy projectiles
   can be shot down. Read 0x39ADE, 0x39B42, 0x3A452, 0x3AA3C.
3. **Spearman** (kind 14): hurt box source, throw projectile (returning shot)
   speed and path, drop set, the action table rows. Needs a scripted trace with
   the mid-boss killed so it is not destroyed by FF83D0.
4. Writer of class-6 **+24** (log sag / platform "stood on"): the platform
   collision pass.
5. **Sorcerer ground fire** flames (class 10 kind 4, 0x24152): speed, lifetime,
   hit box.
6. **Roaming apparition** exit/reward, and whether its zero chance row means it
   never attacks in stage 1.
7. Pickup **owner byte** (+2F double gold): which enemies set it.
8. **Item 3** (FF8355) that reveals the hidden object: which shop item.
9. The kind-0 AI is transcribed from `actor4_zero_*.h` (SOURCE). Before relying
   on it, trace-compare a few soldiers frame by frame (positions, phase byte
   +3, sub +4, timer +22) with scripted inputs (plan section 6 step 4); the dumps
   above already contain natural soldier runs (e.g. slots 29/30, frames
   2480..3060).
10. Natural mid-boss fight: the trace's gate assist distorted frames 7038..8362.
    Re-record with `GATEASSIST` off and a script that keeps firing upward on the
    bridge to observe its attack cadence and the bridge collapse with the player
    on it.
