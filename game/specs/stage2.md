# Stage 2 (Cherlindrea's Forest): behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin` (sha256
0b66efe8...). Behaviour spec for the native Genesis rewrite (NATIVE_ENGINE_PLAN.md
sections 2 and 6), organised like `stage1_enemies.md` and `stage1_boss.md`, which
it continues. Arcade addresses are citations only. "Stage 2" = arcade stage
index 1 (FF8344 = 1).

Evidence tags (as the stage-1 specs):
- **VERIFIED**: two independent sources agree (disassembly read for this file plus
  a headless arcade trace, or plus the transliteration in `native/engine/src`).
- **SOURCE**: one source only (usually disassembly read here, not trace-checked).
- **UNKNOWN**: not established; says where to look.

Units: world X right, world **Y up**; speeds are signed **8.8 px/frame** (256 =
1 px/frame); one update = one 60 Hz frame. Hit box = (xOffset, width, yOffset,
height), box = [x-xOffset, x-xOffset+width] x [y-yOffset, y-yOffset+height].
16-direction values: 0 up, 4 right, 8 down, 12 left.

**Correction for the stage-1 spec (applies here too).** Class-4 hit boxes come
from the class-4 tables **0x3AC4A (touch, vs the player)** and **0x3AE62 (vs
player shots)** (combat.md 1.2); `stage1_enemies.md` section 3 quoted 0x3B1C2,
which is the player's table. With the class-4 tables every spearman node gives
**(24,48,32,48)** (closes stage1_enemies.md 5.8 "hurt box UNKNOWN"). Class-10
projectiles use 0x3B0D2. VERIFIED (node walks of the stage-2 families; the
stage-1 boxes quoted there should be re-walked with these tables).

**Content summary.** Stage 2 has **no mid-boss and no boss** (no boss bar, no
class-18 actor, no kind-7 sorcerer; FF83CC stays 0 for the whole stage). It has
two parts: (A) a village fort (stone building with wooden galleries, windows,
chandeliers, a shop) played as Madmartigan, x 0..~1700, and (B) a **wagon chase**:
the player boards a horse-drawn wagon at x 1728 and the stage becomes an
auto-scrolling ride with mounted archers, boarding soldiers, hawks and a hound;
the stage ends when the wagon has covered its distance, crashes and stops.
VERIFIED (trace B, below; disassembly of the wagon 0x2F740).

## 0. Traces used (headless lockstep, reference program; no GUI)

Harness `build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/lockstep.keys ref`
(`OVERCLOCK=5000`). Scratch dir
`/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage2/`
(below `S/`). Keys of RAM and video dumps = program VBlank count ("v"/key);
script time "t" runs ~3 frames behind the key in these runs.

| Run | Settings | Covers | Output |
|---|---|---|---|
| A (natural hand-over) | `SCRIPT=stand` (no input after Start), `ENDAT=2200` (stage-1 clear injected at t 2200) | stage-1 results and story, stage-2 map scene, title, play start | `S/a_dbg.log` (sound words + mode bytes, `DBGPC=7824`), `S/a2.log`, `S/ramA/` (every 4), `S/vidA/` (every 6 from 2560), `S/snd_transition.txt` |
| B (whole stage) | `SCRIPT=random STAGE=1 INVINCIBLE=1 GATEASSIST=900 NUDGE=600 KILLBOSS=1` | stage 2 from debug start (play at key 838) to the clear (12064) and into stage 3 | `S/b.log` (camera every 10), `S/ram/` (every 2), `S/vid/` (every 10), `S/births.txt` (every actor birth/death), `S/flags_b.txt` (stage flag changes) |
| S (sound) | as B plus `DBGPC=7824` | sound/music words of run B | `S/s.log`, `S/snd_stage2.txt` (t, word) |
| C (stage-2 clear) | `SCRIPT=stand STAGE=1 INVINCIBLE=1 ENDAT=1400` | stage-2 results, both story pages, stage-3 map | `S/c.log`, `S/ramC/`, `S/vidC/` (every 6 from 1400) |

Caveats: run B's player is driven by random input plus a 48-px nudge every 600
frames when the camera stalls; it boarded the wagon by jumping from the balcony,
not through the scripted leap (2.6, 5.12), so the clock was not reset to 3:00
and music 0x03 was never posted. The random input holds buttons during the
stage clear, which fast-forwards results and story (FF799C), so run C (no input)
is the reference for the end sequence. The debug stage start (run B) begins with
rank 0x3000; the natural hand-over (run A) gives 0x3851 (1.6).

---

## 1. Stage entry after stage 1

### 1.1 Hand-over timeline (run A, t = script frames)

| t | Mode (FF8004 / FF8008) | Event | Tag |
|---|---|---|---|
| 2569 | clear step 14 | sounds 0xF4, 0x08FB; stage-1 story page (stage1_boss.md 6.1) | VERIFIED |
| 2621 | step 18 | typing ends; 60 updates; fade; stage index -> 1 (FF8344), checkpoint 0 and last-safe position of the new stage (2F8A: (192,80)), rank update (1.6) | VERIFIED |
| 2977 | mode 0 step 2 | **world-map scene** (1.2): sounds **0xF0, 0xF7, 0x12** (map music) | VERIFIED |
| 2980 | step 4 | fade-in, 128-update hold | VERIFIED |
| 3159 | step 6 | sounds **0xF0, 0xF7** (music stop); fade-out; set-up 0xA0D0: FF8348 already set, stage != 0 -> straight to play (0xA220) | VERIFIED |
| 3215/3216 | play (2/0) sub 0 | stage reset 2DFE (clock **3:30**, HP = max, music word **2**: sounds **0x02, 0x01FA**), HUD 4F44, stage objects 4035E, stage events 9A92; 9-update timer | VERIFIED |
| ~3225 | sub 2 | fade-in job 0x1002 speed 2 (as stage 1) | VERIFIED (code 0x9460) |
| 3226..3327 | sub 4..6 | title (1.3), one sound **0x2F** per letter, 4 frames apart (26 letters) | VERIFIED |
| ~3336 | play step 2 | text cleared (679C); clock runs (first tick 3:30 -> 3:29); player gate as stage 1 (16 updates), then control | VERIFIED |

Map scene to play: **238** updates; play set-up to control: ~120 updates + the
16-update gate. Pressing button 1/2 during these scenes fast-forwards them (FF799C,
intro.md 2). VERIFIED.

### 1.2 World-map scene for stage 2
Identical to intro.md section 4 (scroll3 grid 7 at x 1024, scroll2 grid 7 at x
2560, markers, timings) except:
- Marker I uses palette **0x14** (cleared); marker II blinks **0x12/0x13** every 10
  frames; III..VI palette 0x12. VERIFIED (code 0x35166; render `vidA/v003100`).
- The hero figure is **Madmartigan**: position table 0x35268[1] = world
  **(2664,184)** (above marker II at (2664,160)); root 0x35280[1] -> **0x353E0**:
  node 0x353E0 (2x2 block code 0x2222, palette **0x16**, 42 ticks) -> 0x353EC
  (block 0x2224, 8 ticks) -> 0x353F8 (5 pieces 0x2226/0x2236/0x2204/0x2227/..,
  palette 0x16, **held**): he raises his sword once and holds the pose. SOURCE
  (chain read) / VERIFIED (render shows him above marker II).
- The same scene is shown before every restart after a life loss (run A: life
  lost at t3932 -> map at 3989..4172). VERIFIED (answers intro.md open question 2
  for restarts).

### 1.3 Title text (play sub-steps 4/6, 0x949C..0x952C)
- Message **0x0655**, style 0x1412 (big font, 4 frames/letter): `STAGE2`, cells
  columns 18-29, rows 13-14 (destination 0x12 -> 0x910934). VERIFIED (ROM record
  0x71BE8 decoded; render `vidA/v003334`).
- When it has finished: message **0x0666**, style 0x141E (destination 0x1E ->
  scroll1 0x910240 = column 4, row 16): `CHERLINDREA'S FOREST` (20 big letters,
  apostrophe code 0x7FEE, space 0x71A6), columns 4-43, rows 16-17. Then 5 more
  updates, text layer cleared, play step 2. VERIFIED (ROM 0x72180; render).
- Big-letter codes as intro.md 6.3 plus H 0x768E, I 0x76A0, L 0x76A6, N 0x76AA,
  F 0x768A, ' 0x7FEE.
- The title is typed on every stage start including restarts? The play set-up
  (FF8004 = 2 sub 0..6) runs on each entry, so yes: SOURCE (code path; a restart
  title was not captured; run A's restart at t3989 reached play set-up after the
  run ended).
- Cut 3 applies (title on its own rows, plain background), as for stage 1.

### 1.4 Music and clock
- Stage music: **music word 2** (sound 0x02 then 0x01FA) from checkpoint records
  0..8; checkpoint 9 (wagon) uses **music word 3** (0x03). VERIFIED (ROM 0x3114,
  traces A/S: 0x02 + 0x01FA at play set-up; no other music during stage 2 in run S).
- The scripted wagon leap (5.12) posts **0x08FB** (music fade) at its start and
  **music word 3** (0x03 + 0x01FA) at the jump, and sets the clock to **3:00**.
  SOURCE (player script 0xD5A8..0xD72C; not reached in run B).
- Clock: **3:30** at the stage start (checkpoints 0-2), 3:00 for checkpoints 3-9.
  Normal 1 s ticks (hud.md 3.5); it keeps running during the chase. The clock
  routine 4EA2 also stops while **FF83E2 >= 2**, which the wagon sets once it has
  reached the screen centre at the end (6.1): run B froze at 0:32 from key 11616
  to the clear (12064), and the results count that time. VERIFIED (disassembly
  0x4EB2 + trace).

### 1.5 Start position, player form
- Player start (2F08 -> per-stage resolver 0x30D8 -> record 0): **(192, 80)**,
  stage-start gate as stage 1 (player.md 3; facing right since FF8348 is set).
  Camera starts at (0,0). VERIFIED (trace A: pl 192,80 at key 3220).
- **The player is Madmartigan in stage 2**: 2F08 sets player +0x20 (form) and
  FF8386 from table 0x2F72[stage*4] = **4** for stage index 1 (0 for stages 1 and
  3, 4 for stages 2 and 4). VERIFIED (ROM; RAM player +0x20 = 4, FF8386 = 4 at
  key 840; renders show the armoured swordsman). Madmartigan's moves (sword
  swing, jump, damage) are **not specified anywhere yet**: player.md covers
  Willow only. This is the biggest gap for stage 2 (open question 1).

### 1.6 Checkpoints (restart table)
Resolver 0x30D8 (called by 2F08 at every (re)start), records **0x3114** (8 bytes:
x, y, time BCD, music word). VERIFIED (ROM).

| c | x | y | time | music | camera min X (x-512, >= 0) |
|---|---|---|---|---|---|
| 0 | 192 | 80 | 3:30 | 2 | 0 |
| 1 | 288 | 80 | 3:30 | 2 | 0 |
| 2 | 480 | 344 | 3:30 | 2 | 0 |
| 3 | 704 | 296 | 3:00 | 2 | 192 |
| 4 | 800 | 80 | 3:00 | 2 | 288 |
| 5 | 800 | 360 | 3:00 | 2 | 288 |
| 6 | 1056 | 80 | 3:00 | 2 | 544 |
| 7 | 1056 | 360 | 3:00 | 2 | 544 |
| 8 | 1312 | 80 | 3:00 | 2 | 800 |
| 9 | 1728 | 93 | 3:00 | 3 | 1216 (wagon deck) |

Selection from the last safe position (FF8370/FF8372, recorded while grounded,
combat.md 3.5): i = (x >> 8) * 2 + ((y >> 8) & 1), c = byte table **0x3106**[i];
the index only ever increases:

| x | 0-255 | 256-511 | 512-767 | 768-1023 | 1024-1279 | 1280-1535 | 1536-1791 |
|---|---|---|---|---|---|---|---|
| c (y < 256 / y >= 256) | 0 / 0 | 1 / 2 | 1 / 3 | 4 / 5 | 6 / 7 | 8 / 7 | 9 / 9 |

**Wagon restart** (FF8395 set = the wagon reached full speed, 6.1): the resolver
skips the table: FF8398 (chase virtual camera) = FF8398 & 0x7000 (rounded down
to a multiple of 4096) or 1728 if that is 0; FF8396 (chase distance) =
FF8398 - 1536 (>= 0); c = 9. The camera then starts in the chase mode at that
distance and the wagon starts at full speed when boarded (6.1). So a death
during the chase resumes at the last 4096-px mark (distance 0, 2560, 6656,
10752, ... in FF8396 terms) with 3:00 and music 3. VERIFIED (disassembly
0x30B0..0x30D4, 0x42638, 0x42DAC, 0x2F8C0) / not trace-checked.
FF8395 survives restarts (it is below the 2DFE clear range FF83C2..FF8401); where
it is cleared at the next stage: UNKNOWN (no byte clear found; probably a block
clear of FF8380..FF83C1 at new stage/new game).

### 1.7 Difficulty rank
Stage start (0x38146, stage1_enemies.md 2.3): excess over FF8291 halved, +8 for
stage index 1 (0x3819A), minus the death-count term. Natural hand-over: 0x3151 at
the end of stage 1 -> **0x3851** at the stage-2 start; it then rises ~1 per frame
(run B: 0x3000 -> 0x5B66 at the clear, ~11,200 frames). The debug stage select
starts at 0x3000 (no stage add). VERIFIED (traces A/B).

---

## 2. Map, camera, collision, palettes

### 2.1 Layers and stage size
- **Foreground (scroll2)**: grid **1** (0x2DBF8[1] = 0x2DD24), **mode 3**: index =
  ((~worldY) & 0x100) | (x >> 8), i.e. two block rows of 256 columns of 256x256-px
  blocks (16x16-px cells at ROM 0x80000 + b*0x400):
  - row index 0 (world y **256..511**, upper floors): blocks 96..102 at x 0..1791;
    nothing beyond.
  - row index 1 (world y **0..255**, ground): blocks 103..109 at x 0..1791 (fort
    ground floor and the road start), then the **road**: blocks 110..115 (110, 111
    repeated, 112..115 variants) for columns 7..207 (x 1792..53247).
  Assemble with `game/tools/stagemap.py 1 0 2 OUT` (row 0 = upper, row 1 = ground;
  3328 x 32 cells). VERIFIED (ROM; `S/map/s2grid1.cells`; tile lookup = 0x43658 /
  `tools/cps1_source_layers.py` with stage=1).
- **Background (scroll3)**: grid **1** (0x2EB24[1] = 0x2ED4C), mode 0 (16 x 16
  blocks of 256 px, 32x32 tiles at 0xC0000 + b*0x100): row 14 (world y 256..511):
  block 36 (sky) except column 6 = 0; row 15 (world y 0..255): 53, 53, 53, 54, 55,
  56, 57, 54, 58, 66, 67, 57, 54, 55, 57, 53 (forest/tree line). x wraps every 16
  blocks (4096 px). Far camera = near camera / 2 in x and y (trace: far 386 at
  near 773; far y 36 at near 72). VERIFIED.
- **Played area**: x 0..~1920, y 0..496 (camera Y 0..272 + 224) in the fort; the
  chase plays in the strip x 1536..1920, y ~48..200 while the road scrolls
  (2.4). Areas (renders, keys from run B):

  | Camera X | Area | Key frames |
  |---|---|---|
  | 0..~150 | forest edge outside the fort, start (player 192,80) | `vid/v000900` (title over it), `vid/v002000` |
  | ~150..1450 | the fort: stone walls, wooden galleries at several heights, ladders, barrels, windows (crossbowmen/lurker), chandeliers, chests on balconies, shop door "SHOP" at 1352 | `vid/v003000`, `v003780`, `v004100`, `v004420`, `v005050`, `v005600` |
  | ~1390..1536 | the tower and its balcony over the forest road; the wagon waits at (1728, 61) | `vid/v006200`, `v006300` |
  | 1536 (fixed) | wagon chase on the forest road, sky darkening to dusk | `vid/v006530`..`v011490` |
  | 1536 (fixed) | wagon crash / stop, stage clear | `vid/v011600`, `v011800`, `v012000` |

### 2.2 Collision (terrain)
As stage 1: the collision id of a 16x16 foreground cell is attribute bits 10-15
of the scroll2 cell (grid 1 above); id -> descriptor at ROM 0x4712
(`game/tools/colpack.py`, player.md 6). Ids present: in the fort 1, 2, 3, 4, 5,
6, 8, 9, 10, 12, 13, 14, 16, 17, 18, 19, 21, 22, 36, 37, 38, 44, 45, 46; on the
road only id **2** (a flat floor whose top is at world **y 48**: cell row 29 of
the 512-px map, every road column). VERIFIED (cell scan of grid 1). Ids 44-46
and 12-22 do not occur in stage 1: check their descriptors when the native
terrain code handles stage 2 (stairs/slopes; UNKNOWN which).
The wagon deck is not terrain: it is a class-6 platform (6.1); the player
stands on it at **y = wagon y + 32** (93 for the wagon at y 61). VERIFIED (trace).

### 2.3 Camera in the fort
- Limits at stage start (0x2DCA/0x2DD8/0x2DBE[1]): max X **0x7E00 = 32256**
  (effectively open), min Y **0**, max Y **0x110 = 272**. VERIFIED (ROM; RAM
  FF846A = 32256, FF8434 = 272).
- X follows the player at player X - 192 (player.md 7); moving right raises
  min X to camera X - 768 (backtracking at most 768 px). Y as player.md 7.
  VERIFIED (trace: min X = cam - 770 throughout).
- No per-x Y floor table in stage 2 (0x42712[1] = 0). VERIFIED.
- Edge soldiers are not allowed to enter through walls (4.1).

### 2.4 Camera during the wagon chase (special scroll effect)
- **Boarding** (FF83DD set by the wagon when the player first stands on it,
  6.1): camera event 0x42DAC (once, +0x66): FF8398 = **1536** (unless the wagon
  restart already set it), camera mode **8**. The camera X **snaps** to 1536 at
  once (run B: 1484 -> 1536 at key 6524) and min X = 768. VERIFIED.
- **Mode 8** (0x42A78) each frame: camera X := FF8398 (the virtual camera); if
  player X >= max X -> mode 2 (never in practice); else virtual X += FF83DA
  (road speed, 8.8; set by the wagon) and is clamped to max X (32256). Tile
  streaming and the CPS scroll2 X register (FF8032 = X & 0x3FF) use the virtual X;
  then the stage-2 post-hook (0x428D2) stores FF8398 = virtual X and puts the
  camera X (FF8408, used by every actor and by collision) back to **1536**.
  VERIFIED (disassembly; trace: FF8408 = 1536, FF8398 rising to 31923, FF8032 =
  FF8398 & 0x3FF).
- Result: **the actors live in a fixed screen-sized strip x 1536..1920** (the
  wagon at 1728, riders, hawks, the hound, the player) while the road and trees
  scroll under them at the road speed; scroll3 follows at half the virtual X.
  Objects lying on the road move left at the road speed (`x -= FF83DA`, 5.9-5.13).
  Chase distance **FF8396 = FF8398 - 1536** drives every chase spawner and the
  sky colours. VERIFIED.
- Speed profile (from the wagon, 6.1): 0 at boarding, **+4 per frame** (8.8) to
  **1536 = 6 px/frame** (reached 384 frames later), held until distance 28672,
  then the wagon's end script, then **-5 per frame** to 0. Run B: boarding key
  6524, full speed ~6908 (FF8395 set at 6906), FF83D0 at 11496 (virtual 30219),
  stop by ~11800, clear 12064 at virtual 31923. VERIFIED.
- Camera Y stays 0 during the chase (the strip is at road level). VERIFIED.
- **Native**: plane A shows the road (blocks 110..115 are a short repeating set,
  so a looping strip of road columns streamed from the virtual X is enough);
  plane B the forest backdrop (cut 1 allows a static backdrop, but the chase
  needs plane B to move or the ride looks frozen: decision for the user, open
  question 6); sprites stay in screen space (camera 1536).

### 2.5 Actor retirement rule in stage 2
The general "x < camera min X" retirement (21C6, every 16/32/64 frames per
family) is stage-specific: in stage 2, **while FF8395 or FF83DD is set, an actor
with x < 1536 is removed**; otherwise x < FF838C as stage 1. So boarding the wagon
clears the whole fort behind (run B: placed actors removed at keys 6510..6580).
VERIFIED (disassembly 0x21C6..0x2210; trace).

### 2.6 Palettes
- Loaded at stage set-up (0978, stage index 1): objects 0-15 **0xE5080**, objects
  16-31 **0xE5680** (0xE5480 + 0x200), scroll2 (32 palettes) **0xE8120**
  (0xE7D20 + 0x400), scroll1 text **0xE6EA0**, scroll3 (16 palettes) **0xEAF00**
  into scroll3 palettes 0-15 (0xA2E[1] = 0). SOURCE (disassembly 0x978..0xA2E;
  the dumps in `vid/` hold the live palettes for cross-checking).
- **Chase sky progression** (palette controller kind 7, routine 0x1252, started by
  the camera init for stage 2 with +3C = 3 -> scroll3 palette bank): while
  FF8395 is set, step s = table **0x1384**[FF8396 >> 8] (12 entries per step:
  s = distance / 3072, 0..9); when s changes it copies 10-step tables of 16
  colours into scroll3 palettes **3** (0xEC4C0 + 32s), **7** (0xEC600), **1**
  (0xEC740), **4** (0xEC880), **5** (0xEC9C0), **6** (0xECB00) and **8**
  (0xECC40). The sky goes from day to purple dusk over the ride. VERIFIED
  (disassembly; renders `vid/v007000` blue -> `vid/v011600` purple). Native: 10
  precomputed plane-B palette sets switched by distance.
- **Wall torches**: class-12 kind 6 variant 0 objects rewrite 2x2 scroll2 cells
  every 7 frames (3 frames: codes 0x0F40/48/41/49, 0x0F42/4A/43/4B,
  0x0F50/58/51/59, attr 7) at 9 places (3.1, 5.8): a tile animation, not a palette
  cycle. VERIFIED.
- No other palette cycle was found in the fort (no scroll2 controller started for
  stage 2). SOURCE (camera init 0x4266A starts only the kind-7 controller).
- Story page palette controllers: 7.3.

---

## 3. Placed objects and spawn events

### 3.1 Stage object table (0x4034A[1] = 0x4063A, 39 entries)
Loaded at stage start by 0x40374 (filters as stage1_enemies.md 2.1). Entry bytes
class, variant, kind, direction, x, y. VERIFIED (ROM; all 39 present at key 838
in run B with the same positions).

| # | class / kind / variant | dir | x, y | What it is (section) |
|---|---|---|---|---|
| 0 | 16 / 3 / 0 | 0 | 1544, 400 | wagon-leap trigger at the tower top (5.12) |
| 1 | 4 / 0 / 4 | 1 | 416, 392 | soldier, placed (stage1_enemies 5.1) |
| 2 | 4 / 0 / 4 | 0 | 704, 392 | soldier, placed |
| 3 | 4 / 0 / 4 | 0 | 1376, 392 | soldier, placed |
| 4 | 4 / 0 / 8 | 1 | 608, 168 | soldier, ambush from above |
| 5 | 4 / 0 / 4 | 0 | 576, 168 | soldier, placed |
| 6 | 4 / 0 / 8 | 0 | 792, 168 | soldier, ambush |
| 7 | 4 / 0 / 4 | 0 | 1184, 224 | soldier, placed |
| 8 | 16 / 4 / 0 | 0 | 544, 160 | treasure chest 0 (8.1) |
| 9 | 16 / 4 / 0 | 1 | 352, 384 | treasure chest 1 |
| 10 | 16 / 4 / 0 | 2 | 784, 160 | treasure chest 2 |
| 11 | 4 / 3 / 2 | 1 | 1360, 80 | archer (stage1_enemies 5.9) |
| 12 | 4 / 3 / 2 | 1 | 608, 80 | archer |
| 13 | 4 / 16 / 2 | 0 | 1176, 88 | crate archer (5.3) |
| 14 | 4 / 16 / 0 | 0 | 1240, 400 | crate archer |
| 15..17 | 12 / 10 / 0 | 0 | (640,408) (896,408) (992,408) | window shutter, releases a crossbowman (5.4) |
| 18, 19 | 12 / 10 / 0 | 0 | (992,168) (1120,168) | window shutter (5.4) |
| 20 | 4 / 18 / 0 | 1 | 1248, 112 | window lurker (5.5) |
| 21 | 4 / 14 / 8 | 1 | 1464, 384 | spearman, emerges from a door (5.6) |
| 22 | 4 / 18 / 0 | 0 | 1408, 224 | window lurker |
| 23..31 | 12 / 6 / 0 | 0 | (448,208) (592,336) (928,144) (960,304) (1056,144) (1136,416) (1168,144) (1288,128) (1360,432) | wall torch (tile animation, 5.8) |
| 32 | 12 / 6 / 1 | 0 | 1248, 200 | breakable scenery (5.8) |
| 33 | 12 / 6 / 2 | 0 | 1120, 232 | breakable scenery (5.8) |
| 34 | 4 / 2 / 0 | 0 | 720, 464 | falling chandelier (5.2) |
| 35 | 4 / 2 / 1 | 0 | 944, 208 | falling chandelier (longer drop) |
| 36 | 4 / 2 / 0 | 0 | 1184, 304 | falling chandelier |
| 37 | 12 / 8 / 1 | 6 | 1352, 96 | shop door (8.2) |
| 38 | 6 / 2 / 0 | 0 | 1728, 61 | **the wagon** (6.1) |

### 3.2 Stage event table (0x9AD8[1] = 0x9B2E, 10 events)
Loaded by 0x9A92 into the event pool (stage1_enemies.md 2.2). Fields: kind /
variant, parameters (+12), endX (+06), y (+08), startX (+0A), spawnY (+0C).
VERIFIED (ROM; every event seen in run B).

| Ev | kind / var | par | startX | endX | y / spawnY | Meaning (section) |
|---|---|---|---|---|---|---|
| 0 | 0 / 0 | 0 | 0 | 288 | 88 / 88 | edge soldiers while camera X in [0, 288) (4.1) |
| 1 | 0 / 0 | 0 | 832 | 1536 | 88 / 88 | edge soldiers while camera X in [832, 1536) |
| 2 | 0 / 2 | 0 | 0 | 1120 | 88 / 88 | soldier pairs every 240 frames while camera X < 1120 (4.1) |
| 3 | 14 / 2 | 0 | 1320 | 1320 | 200 / 200 | spearman from a door at (1320,200), respawning (5.6) |
| 4 | 15 / 0 | 0 | 1536 | 1536 | 168 / 168 | hawks from chase distance 12800 (5.11) |
| 5 | 20 / 0 | 0 | 0 | 1536 | 92 / 0 | mounted archers entering from the left, distance windows (5.9) |
| 6 | 20 / 1 | 0 | 0 | 1920 | 92 / 0 | mounted archers entering from the right (5.9) |
| 7 | 21 / 0 | 0 | 0 | 1536 | 92 / 0 | boarding soldiers, distance 4608..12544 (5.10) |
| 8 | 1 / 2 | 0x0202 | 1504 | 1504 | 56 / 56 | hound, distance 5632..12800 (5.13) |
| 9 | 1 / 2 | 0x0200 | 1504 | 1504 | 56 / 56 | hound, distance 17152..28160 (5.13) |

Event kind routines (table 0x381FA): 0 0x3833C, 1 0x38698, 14 0x38C14, 15 0x38D82,
20 0x38FFE, 21 0x391EE. Family population bytes FF8F40..FF8F5F are cleared by the
stage reset (2EDE). VERIFIED.

### 3.3 Roster

| Family | Class / kind / var | Count | Where | Spawned by | Energy | Score | Drop |
|---|---|---|---|---|---|---|---|
| Soldier (edge, pairs) | 4 / 0 / 0 | unlimited | camera-relative, x < 1488 | events 0, 1, 2 | 1 | 50 | 1 gold |
| Soldier placed / ambush | 4 / 0 / 4, 8 | 5 + 2 | 416..1376 | table | 1 | 50 | 100 (14/16) / 10 gold |
| Archer | 4 / 3 / 2 | 2 | 608, 1360 | table | 4 | 50 | 10 / 100 gold |
| Crate archer | 4 / 16 / 0, 2 | 2 | 1176, 1240 | table | 1 | 200 | 1/10/100/200 gold |
| Window crossbowman | 4 / 17 / 0 | up to 5 (one per window) | windows | class 12 kind 10 | 1 | 200 | 1/10/100/200 gold |
| Window lurker | 4 / 18 / 0 | 2 | 1248, 1408 | table | 1 | 200 | 1/10/100/200 gold |
| Spearman | 4 / 14 / 8, 2 | 1 + respawns | 1464; 1320 | table; event 3 | 4 | 200 | 100 (v8) / 10 (v2) gold |
| Falling chandelier + flames | 4 / 2; 10 / 12 | 3 | 720, 944, 1184 | table | - | - | - |
| Returning spear | 10 / 7 | per throw | - | spearman | - | - | - |
| Arrows | 10 / 0 / 0, 1 | per shot | - | archers, riders | - | - | - |
| Mounted archer | 4 / 20 / 0, 1 | many | chase strip | events 5, 6 | 2 | 100 | 10 (7/8) / 100 gold |
| Boarding soldier | 4 / 21 / 0 | one at a time | chase strip | event 7 | 2 | 100 | 10 (7/8) / 100 gold |
| Hawk + rock | 4 / 15 / 0; 10 / 9 | up to cap | chase strip | event 4 | 2 | 200 | set 4: 100 + 200 + 200 gold |
| Hound | 4 / 1 / 2 | one at a time | chase strip | events 8, 9 | 2 | 100 | 10 / 100 gold |
| Horses | 12 / 15 | one per rider | chase strip | riders' init | - | - | - |
| Chests | 16 / 4 | 3 | 352..784 | table | 6 | - | 8.1 |

---

## 4. Families shared with stage 1: stage-2 differences

### 4.1 Soldier (class 4 kind 0)
- Variant 0 wake-up in stage index 1 (0x100A8): after placing itself at
  camera X - 32 / + 416 it is **removed at once if x > 1488**, if |x - 744| < 24,
  or if |x - 1176| < 24 and y < 88 (no entries through the tower and the doors).
  VERIFIED (disassembly; trace: 2-frame births at 1503..1745).
- Edge event (variant 0, events 0/1) in stage 2 also retires when FF8395 is set
  or player X > 1536; a spawn x >= 1536 is moved 212 px left with facing 0.
  VERIFIED (0x38390, 0x384A6).
- **Event 2 (variant 2, 0x384F6)**: while camera X in [startX 0, endX 1120): a
  240-frame timer; at expiry two variant-0 soldiers (no population cap): the first
  with random facing at y = camera Y + 144, the second with the opposite facing
  at y = player Y + 23 (camera Y + 112 while the player-gate word FF988E is set);
  x is then set by the variant-0 wake-up (side by facing). The timer is held
  (reset) while FF9802, FF9803 or FF789A is non-zero; retires on FF8395 or
  FF83D0. VERIFIED (disassembly; trace pairs every ~240 frames, e.g. keys
  1340/1470, 1518/1760) / UNKNOWN (what FF9802/FF9803/FF789A are).
- Variants 4 and 8 as stage 1. VERIFIED (trace: (792,168) dropped in at 3242).

### 4.2 Archer (kind 3 variant 2) — no stage-specific code (0x125D0..0x13BD2).
VERIFIED.

### 4.3 Spearman (kind 14) — variants 8 and 2: see 5.6.

### 4.4 Beast (kind 1) — variant 2 is the chase hound: see 5.13.

---

## 5. New families (numbered 5.2-5.14; soldier changes are 4.1)

### 5.2 Falling chandelier (class 4 kind 2, 0x1237A), variants 0 and 1, and its flames

A burning candle chandelier hanging from a ceiling beam (`vid/v004100`). Placed:
var 0 (720,464), var 1 (944,208), var 0 (1184,304).

| Item | Value | Tag |
|---|---|---|
| Shots | cannot be hit (target-class byte 0x3A07A[2] = 0xFF); no energy, score or drop | VERIFIED |
| Contact | never hurts (0x3A740[2] = 0xFF); only its flames hurt | VERIFIED |
| Init | colour override +0x10 = 19, enable +0x12; root **0x12582** (hold, 7 pieces, palette 19; box (18,36,20,32) / (18,36,24,48)) | VERIFIED |
| Flicker | every 8 frames (clock & 7 == 0) the override toggles palette 19 <-> 20 (flames glow); in every state | VERIFIED |
| Trigger (phase 0) | `|playerX - x| < 96` and `y - H - playerY` in [-32, +32], H = 64 (var 0) or 128 (var 1): the player is under it | VERIFIED (trace: var 1 at (944,208) fell at key 4080 with the player at (911,99)) |
| Delay (phase 2) | 0x12438[rand & 3] = 1, 4, 2, 6 frames | VERIFIED |
| Fall (phase 4) | speed += 32 per frame to 1792 (7 px/frame), y -= speed; floor test 0x3C48 each frame | VERIFIED (y 207 -> 72 in 46 frames) |
| Crash | class-12 kind 0 **variant 4** burst, sounds **0x3A** and **0xA1**, root **0x1259E** (8 ticks, 8 pieces) -> **0x125B8** (wreck, hold, 5 pieces), timer 16 | VERIFIED |
| Fire spread | after 16 frames one flame (class 10 kind 12) at x; after 8 more two at x-16 / x+16; after 8 more two at x-32 / x+32; then the wreck is removed | VERIFIED (trace flames born 4144 at 944; 4152 at 928/960; 4160 at 912/976) |
| Time stop | no pause test: it runs even while FF83D2 is set | SOURCE (`actor4_falling_trigger.h` note) |

**Flame (class 10 kind 12; kind 4 is the same routine 0x24152)**: spawn sound
**0x37**, root **0x241E4** (10 nodes x 4 ticks, a 1-piece node then 1x2 blocks,
last held), palette **6**, box **(6,12,12,14)**. Hurts for **36** frames, then
not collidable (+0 = 2) for **8**, then removed; removed earlier off camera
(2162, every 16 frames). Each frame x += FF83E4 (the wagon's last x step; 0 in the
fort, so flames on the wagon ride along). Variant 0 pauses while FF83D2 is set.
Damage handler: kind 12 plain box; kind 4 honours the protection item
(0x3AABC[4] = 6, combat.md 2.1). VERIFIED (disassembly; trace lifetimes 46
frames). This also closes stage1_enemies.md open question 5 (the sorcerer's ground
fire is this routine as kind 4).

Dump keys: 4100 (hanging), 4130 (crash), 4150-4160 (flames).

### 5.3 Crate archer (class 4 kind 16, 0x1995E), variants 0 and 2

A green archer who ducks behind a crate and pops up to shoot along the floor
(`vid/v004420`, crate at x 1176). Placed: var 2 (1176,88), var 0 (1240,400); both
variants run the same code.

| Item | Value | Tag |
|---|---|---|
| Energy | **1** (+2A/+2B = 1 at wake) | VERIFIED |
| Score | index 4 (**200**) | VERIFIED |
| Drop | set `rand & 3`: 0 = 1 gold, 1 = 10, 2 = 100, 3 = 200 (one pickup each) | VERIFIED (0x19CC2) |
| Palette | 17 | VERIFIED |
| Wake (state 0) | every 16 frames (clock + slot): `|y - playerY| <= 30` -> state 2, energy 1, **hidden and not collidable** (+0 = 2) | VERIFIED (key 966) |
| Phase 0 hidden | every 16 frames: face the player; if `|x - playerX| < 160` and bit (rand & 15) of word 0x19A76[row] is set (row = rank high >> 5; set bits per row 3, 5, 7, 9, 10, 10, 11, 11 of 16) -> collidable, timer 18, pop-up root table **0x19CDE** | VERIFIED (table) / SOURCE (row index) |
| Phase 2 | 18 frames, then aim root table **0x19D32**, timer 20 | VERIFIED |
| Phase 4 fire | after 20 frames: sound **0x34** (only when on screen, 786C); **arrow** (class 10 kind 0) direction 4 (facing right) or 12 (left) at (x +- 16, y + 14); recoil root table 0x19D72, timer 40 | VERIFIED (arrow (1160,102) dir 12 at key 4412) |
| Phase 6 | 40 frames, then duck root table **0x19DAA**, timer 18 | VERIFIED |
| Phase 8 | 18 frames, then hidden (+0 = 2), empty root **0x19E7E**, timer 16, phase 0 | VERIFIED |
| Time stop | phases skipped while FF83D2 is set | VERIFIED |
| Death | +0 = 2, sound **0x32**; flies up: vertical speed 1024, -64 per frame to -1920; horizontal 1.5 px/frame (table 0x19E8A) with facing = the player's facing; fall root table **0x19DFE**; from the apex every 16 frames off-camera removal; on landing (3B0A) root table **0x19E02** for 10 frames, sound **0x33**, burst class 12 kind 0 variant 5, score + drop, removed | VERIFIED (key 4532 hit -> 4582 removed) |

Poses (palette 17): pop-up 0x19CDE[f] (2x1 block 6, 3 pieces 6, 2x2 block hold;
box (16,32,8,16), last (16,32,4,8)); aim 0x19D32[f] (16 ticks, 10 pieces, box
(24,48,12,24)); recoil 0x19D72[f] (16 ticks, 8 pieces, (24,48,12,24)); duck
0x19DAA[f] (pop-up reversed); fall 0x19DFE[f] (4x4, 4x3, 4x2 blocks: 2, 2, 16,
hold; (18,36,20,32)/(18,36,24,48)); landing 0x19E02[f] (4x1 hold); hidden 0x19E7E
(empty). The aim and recoil chains' second nodes run into the next table: held
by code.

### 5.4 Window shutter (class 12 kind 10, 0x35D26) and window crossbowman (class 4 kind 17, 0x19E92)

**Shutter**: placed at (640,408), (896,408), (992,408), (992,168), (1120,168);
draw list 2, root **0x35DFE** (5 nodes x 6 ticks, last held, palette 24: the
shutters open). Every 8 frames (clock + slot): if the player is at most 96 px
below it (`y - playerY` in [0, 95]) and `|x - playerX| < 128`, it spawns **one**
kind-17 variant 0 at its own position (+0E = 2), animates 30 frames and then only
retires (x < camera min X, every 16 frames). One crossbowman per window, once.
VERIFIED (disassembly; trace births at (640,408) key 3734, (992,168) 4076,
(1120,168) 4306; the windows at 896/992,408 never triggered).

**Crossbowman** (green soldier with a crossbow in the window, `vid/v003780`,
`v004420`): energy **1**, score index 4 (**200**), drop set `rand & 3` (1/10/100/200
gold), palette **17**. VERIFIED.

| Phase | Behaviour | Tag |
|---|---|---|
| wake | variant 0: +0 = 2, root **0x1A1E4** (rising: 16 ticks, then 0x1A1FA hold), active at once. (Variants 2/4, unused here, wait until on screen and start in phase 4.) | VERIFIED |
| 0 | timer 40 | VERIFIED |
| 2 | 40 frames rising (not collidable), then +0 = 1, timer 1 | VERIFIED (3736 -> 3776, 4076 -> 4116) |
| 4 aim | when the timer runs out: aim = rounded 16-direction toward the player (20D2); allowed only if 0x19F7E[aim] = 1: directions **4, 5, 6, 7, 9, 10, 11, 12** (sideways and downward diagonals); facing = aim, pose 0x1A214[aim]; then if the player is in range (0..95 px below, `|dx| < 128`) and bit (rand & 15) of 0x1A002[row] (set bits per row 4, 8, 8, 8, 10, 11, 12, 16) -> phase 6, timer 36; else retry in 16 frames | VERIFIED (facing followed the player 10/9/7/6/5/4) |
| 6 | 36 frames, then sound **0x34** (on screen) and an **arrow** (class 10 kind 0, 4 px/frame) with direction = aim from (x,y) + 0x1A070[aim]: dirs 0-4 (+12,-3), 5 (+8,-14), 6 (0,-4), 7-8 (-6,-8), 9 (+6,-8), 10 (0,-4), 11 (-8,-14), 12-15 (-12,-3); timer 10 | VERIFIED (arrows dir 5..9 from (987..1004, 153..162)) |
| 8 | 10 frames, then phase 4 with timer 16 | VERIFIED |
| retire | every 32 frames if x < camera min X (21C6, 2.5) | VERIFIED |
| death | +0 = 2, sound **0x32**, root **0x1A4B0** (3x2 block 8 ticks, hold) 16 frames; sound **0x33**, burst variant 5, 10 frames; score 200 + drop; removed | VERIFIED (4788 -> 4816) |

Aim poses 0x1A214[0..15] (palette 17, box (24,48,12,24)): dirs 0-4 -> 0x1A234,
5 -> 0x1A28C, 6 -> 0x1A2D6, 7-8 -> 0x1A324, 9 -> 0x1A372, 10 -> 0x1A3C0, 11 ->
0x1A40E, 12-15 -> 0x1A458; each 16 ticks, 20 ticks (shot), hold. Rising 0x1A1E4
box (18,36,20,32)/(18,36,24,48).

### 5.5 Window lurker (class 4 kind 18, 0x1A4E2)

A dark creature that leans out of a window and pulls back (`vid/v005050`, window
at x 1248). Placed at (1248,112) and (1408,224).

| Item | Value | Tag |
|---|---|---|
| Energy / score / drop | **1** / index 4 (**200**) / set rand & 3 | VERIFIED |
| Palette / box | 24 / **(2,4,32,33)** on every node (a narrow column x +-2, y-32..y+1) | VERIFIED (node walk) |
| Wake | every 16 frames: `|dx| < 96` and `|dy| < 48` -> draw list 2, energy 1, root **0x1A728**, sound **0xA0**, timer 12 | VERIFIED (keys 5044, 6146; sound run t5041, 6143) |
| Phase 0 | 12 frames (not collidable), then +0 = 1, timer 80 | VERIFIED |
| Phase 2 | 80 frames out (hurts, hittable), then +0 = 2, timer 1 | VERIFIED (6158 -> 6238) |
| Phase 4 | every 16 frames: player in wake range and 0x1A5D0[rand & 15] (4/16) -> +0 = 1, root **0x1A77A**, timer 36, back to phase 2 | VERIFIED (table) / SOURCE (not seen) |
| Retire | every 64 frames x < camera min X | VERIFIED (removed 6578/6580 after boarding) |
| Death | +0 = 2, sound **0x32** + a falling debris piece (class 12 kind 9, 0x1A6D4: root 0x1A7E6 palette 18, falls -32/frame for 24 frames, removed); next frame sound **0x33** + burst variant 5; score + drop; root **0x1A7CC** (hold: the empty window) until retired | VERIFIED (5064 -> 5068; debris 5066..5092) |

Chain 0x1A728: 3x2 block 6, 6 pieces 6, 7 pieces 6, 7 pieces 32, then
0x1A77A..0x1A7C0 (6 ticks each) to 0x1A7CC hold (6 pieces).

### 5.6 Spearman variants 8 and 2 (class 4 kind 14) and the returning spear

Same AI as stage 1 after activation (stage1_enemies.md 5.8, box (24,48,32,48)),
palette 18, energy **4**, score 200. Differences:
- **Variant 2** (event 3, at (1320,200)) and **variant 8** (placed at (1464,384))
  use the "emerge" entry 0x177B6: phase 0 wait until on screen (`x-camX <= 448`,
  `y-camY <= 276`); phase 2 floor probe; phase 4 wait until the player is within
  `|dy| < 48` and `|dx| < 192`; phase 6 spawn class 12 kind 0 **variant 7** (door
  effect) at its position, timer 30; phase 8: 30 frames, then root **0x18B78**
  (emerge: 10, 10, 4x4 block hold, palette 18); phase 10: 30 frames animating
  (paused by FF83D2); phase 12: activate (energy 4, root table 0x186A4).
  VERIFIED (trace slot 45: 6132 ph8, 6162 ph10, 6192 ph12, 6194 active).
- Variant 8 never woke in run B (the player never came near y 384 there).
- Drop: 0x18544[variant >> 1]: set **2** (100 gold) for variant 0, set **1** (10
  gold) for variant 2, set 2 for variants 4 and up. VERIFIED.
- **Event 3** (event kind 14, 0x38C14): every 8 frames, when `endX - camX <= 384`
  and `y - camY <= 256` -> spawn kind 14 variant 2 at (1320, 200), +0 = 2; the
  next one only after the child has died and a delay 0x38D72[rank >> 12] = 240
  (ranks 0-5), 210 (6-8), 180, 150, 120, 90, 60, 60, 30 frames. Retires when FF8395
  is set. VERIFIED (disassembly) / SOURCE (the death-counted population).
- Throw: spear spawned at (x + 32 facing right / x - 32 left, y + 8), sound
  **0x8B** (sound run t6272).
- **Returning spear (class 10 kind 7, 0x2469C)**: 8 frames of wind-up, then x
  speed +-1536 (6 px/frame) away from the thrower, changing by -+24 per frame (it
  stops after 64 frames and comes back); the first wall or clash (0x3CDC /
  0x75C8) sets the speed to -+1024 toward the thrower; removed when back at the
  thrower (0x75C8 test) or off camera (every 8 frames); FF83D0 removes it. Root
  table 0x247D4[f]: 4 nodes x 2 ticks (spinning, palette 18), box (8,16,8,16);
  plain damage handler. VERIFIED (disassembly; trace: born 6274 at (1397,215),
  gone 6330) / SOURCE (meaning of 0x75C8).

### 5.8 Scenery animators (class 12 kind 6, 0x32FB4)
- **Variant 0, wall torch** (9 placed, 3.1): no sprite; while within camera
  x-16..x+400 and y-16..y+272 it rewrites a 2x2 block of scroll2 cells at its map
  position (lookup 0x43658) every **7** frames, cycling 3 frames (2.6). Native:
  an animated 2x2-cell tile set on plane A. VERIFIED.
- **Variants 1 (1248,200) and 2 (1120,232), breakable scenery**: while FF83DD is
  clear, write the intact cells every frame (tables 0x3327C var 1 / 0x3325C var 2,
  2x2 cells at offset (-32,-8)); when the player comes within +-16 px both ways:
  write the broken cells (0x3323C / 0x3321C), spawn `variant` bursts (class 12
  kind 0 variant 4, offsets 0x33102) and `variant` falling pieces (class 12 kind 6
  variant 3 or 4: gravity 40/frame to 1792, removed on landing; roots 0x33322 /
  0x33346, palette 24 / 18). Removed when FF83DD is set. VERIFIED (disassembly) /
  UNKNOWN (what it depicts, whether it hurts; never triggered in run B).

### 5.9 Mounted archer (class 4 kind 20, 0x1B268), variants 0 and 1

Green-armoured archer on a brown horse shooting along the road (`vid/v007000`,
`v008900`). Every chase rule (strip, road speed, FF83D0 end) is in 2.4 and 6.

| Item | Value | Tag |
|---|---|---|
| Energy | **2** | VERIFIED (0x1B2AC; trace hp 2) |
| Score | index 2 = **100** | VERIFIED (0x1B93A) |
| Drop | set 0x1B954[rand & 7] = 1,1,1,1,2,1,1,1 (10 gold 7/8, 100 gold 1/8) | VERIFIED |
| Boxes | touch **(6,12,24,44)** (attack-high pose 0x1BC00: (12,24,24,44)); shot **(10,20,24,44)** | VERIFIED (node walk) |
| Palette / lane | 17 / +0E = 2 | VERIFIED |
| Motion | table 0x1B2EE: facing 0 **+384** (1.5 px/frame right), facing 1 **-128** (0.5 px/frame left) | VERIFIED |
| Sounds | 0x34 arrow, 0x32 hit, 0x33 FF83D0 explosion | VERIFIED |
| Horse | a separate class-12 kind 15 actor (routine 0x30116) spawned by the rider's init (0x49E6 var 0 / 0x4A0A var 2) at the rider's position, linked by +0x26; scenery, never collides | VERIFIED (each kind-20/21 birth has a kind-15 horse birth at the same key and position) |

Spawn: event 5 (left side, x 1536, facing 0; variant from tables, often 1) and
event 6 (variant 0 from the right, x 1920, facing 1). VERIFIED (births).

**Variant 0** (0x1B27A): waits for FF83DD; energy 2, saved side +40 = facing,
root 0x1B9BE. State 2 each frame: FF83D0 -> explode; energy changed -> state 4;
FF83D2 -> frozen; else:
- Phase 0 (enter): move by the motion table until `x - camX` is in [80, 304];
  then turn, phase 2, stay timer **768**, aggression thresholds at 512/256/0
  (aggression +44 = 0 -> 1 -> 2).
- Phase 2 (ride alongside): every 16 frames a 1/2 chance to turn; keep-out:
  leaving [cam+16, cam+368] snaps it to the nearer edge and turns it; entering
  the wagon band [cam+112, cam+272] snaps it to cam+112 (left rider) or cam+272
  (right rider) and turns it. Every 16 frames roll 0x1B408[(rankHi >> 4)*8 +
  rand & 7] (2/8 for rank rows 0..10, then 3..7/8) -> attack. VERIFIED (tables) /
  SOURCE (structure).
- Attack: left rider: 0x1B4E6[aggr + rand & 15] (~4/16) -> high shot (root
  0x1BB14, 48 frames, arrow at y+24), else low shot (root 0x1BA00, 56 frames,
  arrow y+10). Right rider: root 0x1BC00, 48 frames, arrow y+10. At timer **16**:
  sound **0x34**, arrow class 10 kind 0 **variant 0** at x+16 direction **4**
  (left rider) or x-16 direction **12** (right rider). VERIFIED (trace arrows at y
  102 heading 12).
- Phase 4 (leave after 768 frames): rides back out the way it came; removed when
  `x - cam + 32 > 448`.
- Hit (2 -> 1): sound **0x32**, 15 frames not collidable (root 0x1BD20), back to
  phase 2. SOURCE.
- Death: sound 0x32, thrown off away from its side (3 px/frame, motion 0xED088),
  vertical speed 0 then +32/frame to 1792 downward, root 0x1BD20; on the road
  (0x3C48): root 0x1BD64 (held), drop, score 100, clears FF8F55 (left) / FF8F56
  (right) so its event may spawn again; the body slides left at the road speed
  until off screen. The horse runs on. VERIFIED (code; deaths 9050, 9292 with
  pickups).

**Variant 1** (0x1B638), one-shot archer from the left: waits FF83DD, rides
right at 1.5 px/frame until x >= cam+112, turns; wanders; every 16 frames, unless
inside [cam+128, cam+256], chance 0x1B75C[rand & 7] = 4/8 to attack, high
(0x1B79C[rand & 7] 4/8: root 0x1BB14, 48 frames, arrow y+24) or low (root
0x1BA00, 56 frames, arrow y+16); at timer 16 sound 0x34 and an arrow class 10
kind 0 **variant 1** (pose table 0x2391E, palette 23) at x+16 direction **4**;
then leaves (removed when `x - cam > 384`). Exactly one arrow per rider.
VERIFIED (arrow variant 1 at x 1668/1666 facing 4, keys 10906, 11298).

Roots (palette 17): 0x1B9BE ride (2 x 8, loop), 0x1BA00 low shot (6 x 8, then
127), 0x1BB14 high shot (5 x 8, then 127), 0x1BC00 right-side shot (5 x 8, hold),
0x1BD20 hit/fall (7, 7, hold), 0x1BD64 lying (hold). Riders' and hound's tick
counts follow the road speed: helper 0x4972/0x49AE takes 0x4996[FF83DA >> 8] =
10, 9, 8, 7, 6, 5, 4 ticks. SOURCE.

### 5.10 Boarding soldier (class 4 kind 21, 0x1BD74)

Armoured soldier with a mace on a horse who rides up, jumps onto the wagon,
fights on the deck and jumps back (`vid/v007620` jumping, `v007800` on the deck).

| Item | Value | Tag |
|---|---|---|
| Energy | **2** | VERIFIED |
| Score | **100** | VERIFIED (0x1C5A6) |
| Drop | 0x1C6C2[rand & 7] = 1,2,1,1,1,1,1,1 | VERIFIED |
| Boxes | touch and shot **(16,32,24,48)** on all body poses | VERIFIED |
| Palette | **16** (draw list 2) | VERIFIED |
| Motion tables | 0x1BDF2 ride (+768 / -512), 0x1BDFA leap (+-1024), 0x1BE02 (+-512), 0x1BE0A (+-256), 0x1BE12 (0, +-256 vertical) | VERIFIED |
| Sounds | **0x90** jump aboard (sound run t7575, 8532, 8721), 0x32 hit, 0x33 explosion | VERIFIED |
| Flags | FF8EB4 = someone is on (or boarding) the wagon; FF8F57 = a kind-21 is alive (event gate) | VERIFIED |

Behaviour (SOURCE: read from 0x1BD74..0x1C790; partly trace-checked):
1. Waits FF83DD; rides in from x 1536 at 3 px/frame until x >= cam+32; 768-frame
   stay.
2. Action pick 0x1BF02[(rankHi & 0xF0) + rand & 15] = 2, 4 or 6 (4 more often at
   high rank); if 4 and FF8EB4 is set, re-pick from 0x1C002[rand & 7] (2 or 6).
   - **2, strike from the saddle**: ride toward the player; within 88 px: if the
     player is grounded (FF988E) wait 8 frames, else surge for 32 frames (motion
     0x1BDFA, root 0x1C7D8, attack flag +3C from frame 16).
   - **4, board**: when `|playerX - x| <= 112`: lane 0, sound 0x90, jump
     (vertical 1024, -32/frame, root 0x1C848), fall (root 0x1C890, +32/frame to
     1536); landing on the deck (platform test 0x743A): moves with the wagon (x +=
     FF83E4), turns toward the player every 64 frames, stays within +-56 px of the
     wagon centre FF83DE, poses 0x1C940/0x1C948/0x1C950 with waits
     0x1C21C[rand & 3] = 64, 128, 192, 255; within 16 x 16 of the player: leap
     attack (motion 0x1BE0A, speed 768 - 48/frame, root 0x1C968). Missing the
     deck: falls to the road and dies (5).
   - **6, return**: jumps back to the saddle (root 0x1C8B6, 24 frames), y = 92.
3. After 768 frames it rides out left; removed when x < cam-32.
4. Hit with energy left: sound 0x32, 8 frames not collidable (root 0x1C960[f]
   on the deck, else 0x1C91A), resumes.
5. Death: score 100, sound 0x32, thrown off (vertical 512, gravity 48, away
   from the player); on the road: drop, burst class 12 kind 0 variant 4 + sound
   0x33, FF8F57 -= 1; the body slides left at road speed. Leaving clears FF8EB4
   if it had boarded.
Trace: four boarders 7486..8846. Roots (palette 16): 0x1C790 ride (2, loop),
0x1C7D8 strike (3), 0x1C848 jump (2), 0x1C890 fall, 0x1C86A landed, 0x1C8B6
dismount (3), 0x1C91A thrown, deck poses 0x1C940/48/50/58/60/68/70/78 (facing
pairs via 0x1C4C0).

### 5.11 Hawk with a rock (class 4 kind 15 variant 0, 0x18BD8) and the rock (class 10 kind 9)

Brown bird of prey crossing the top of the screen with a rock, dropping it on
the player, then swooping low (`vid/v008900`, `v009160`).

| Item | Value | Tag |
|---|---|---|
| Energy | **2** (variant 1, unused here: 3) | VERIFIED |
| Score | index 4 = **200** | VERIFIED (0x1956C) |
| Drop | set **4** = [v5, v6, v6] (100 + 200 + 200 gold) | VERIFIED (0x19578) |
| Boxes | flight 0x196B4/0x1971E touch (9,18,8,12), (10,20,1,17), (20,32,5,10); dive poses 0x19766[dir] touch (9,18,8,16)/(6,12,8,16), shot (9,18,10,20)..(10,20,10,20); hit 0x1990A touch (18,36,20,32), shot (18,36,24,48) | VERIFIED |
| Palette | **31** (variant 1: 22) | VERIFIED |
| Sounds | 0x32 hit, 0x33 death | VERIFIED |

Behaviour (variant 0, code 0x18DE2.. and slot 38 keys 8852..9364):
1. Born at (1536, 168) by event 4, energy 2, facing 4, root 0x196B4, y + random
   offset (0x18E30); speed 0x190F0[rand & 15] and leg time 0x190E0[rand & 15]
   (trace: 1, then 1.5 px/frame, ~120-frame legs); bobs +-4 px (+-8/frame,
   0x19294/0x192A8). Carries a **rock** (class 10 kind 9) at (x-16, y-24).
2. Releases the rock when over the player (`|playerX + 16 - x - 10| < 20`, table
   0x1914A). VERIFIED (rock released at x 1881 y 139 with the bird at ~1897,163).
3. Swoop: 16-direction poses 0x19766[dir] turning 4 -> 0/14 -> 12, dives low
   over the road, flies left 2 px/frame for 64 frames (not collidable, +0 = 2,
   while low), climbs back up and right at ~4 px/frame and leaves; removed off
   camera (2162, every 16 frames). VERIFIED (keys 9156..9364).
4. Hit: sound 0x32, root 0x1990A 10 frames; death: falls (-40/frame), on the road
   burst variant 5, sound 0x33, score 200, drop set 4, FF8F4F -= 1. SOURCE.

**Rock** (0x24A04): chain 0x24B12 (4 x 4 ticks then hold, palette **31**), box
**(6,12,6,12)**. Carried (+38 = 1) it rides with the bird. Released: falls
-32/frame without cap; on terrain: 20-frame break, removed; on a surface (0x3B0A:
the road line, y 48) -> variant 2: lies there and slides left at road speed until
off screen. Hurts the player (1 HP) on contact. VERIFIED (code; trace var 0 -> 2
at y 48) / UNKNOWN (which surfaces 0x3B0A accepts).

### 5.12 Wagon-leap trigger (class 16 kind 3, 0x36CD0) and the scripted leap
- Placed at (1544,400) (the tower top). When `|playerX - x| <= 8`, player Y in
  [y-32, y+256] and FF8362 is set: player **life state 12** (scripted; player
  record +02.. cleared), once. VERIFIED (disassembly) / never triggered in run B.
- Player life state 12 (0xD5A8, SOURCE): FF8396 = 0, sound **0x08FB** (music
  fade), fall to the floor (gravity 24/frame to 1792); walk right 2 px/frame for
  16 frames; then leap: vertical 1088 up, gravity from +0x34, horizontal 2
  px/frame toward x **1728** (clamped), music word **3** (0x03 + 0x01FA); on
  landing on the wagon (38F6) -> normal play (state 2), clock set to **3:00**,
  sound **0x2B**. So the intended route is: climb the tower, step on the trigger,
  Madmartigan jumps down and runs/leaps onto the wagon. The player can also board
  by jumping from the balcony (run B), which skips the music and clock change.
  UNKNOWN: where FF8362 is set (also read by the stage-1 arrival code 0x42968;
  2F00 copies FF8363 into it).

### 5.13 Hound (class 4 kind 1 variant 2, beast routine 0x115EE)

Blue wolf-like hound with a red collar running beside the wagon and leaping onto
it (`vid/v008900`, `v009160`); same art as the stage-1 beast (palette 3).
- Energy **2**, score 100 and the beast's drop set and hit/death code
  (stage1_enemies.md 5.2); box on every pose **(18,36,16,25)** (class-4 tables).
  VERIFIED (code 0x11714, node walk) / score and drop not trace-checked (the hound
  survived run B).
- Variant 2 (0x1162C -> 0x11714 / 0x11B0E / 0x11DF2 / 0x11F46): spawns at (1504, 56)
  (left of the strip, on the road), motion 0xED068 (1 px/frame), tick rate tied to
  the road speed (0x4972), root r1 0x11FCC (run). Every 16 frames, when x is within
  +-16 of cam+120: waits 0x11BAC[(rand + slot) & 3] = 20, 40, 60 or 80 frames, then
  **leaps**: vertical 1024 decreasing 48/frame (root r11 0x11FF4), falls +48/frame
  to 1536 (root r12 0x11FF8); on the wagon deck (0x3B0A) -> runs on the deck; on
  the road (0x743A) -> runs (root r13 0x11FFC) for 0x11C6A[rand & 14] = 300, 60,
  360, 240, 240, 240, 180, 240 frames, then repeats. Removed every 64 frames if x <
  camera min X. SOURCE (code) / VERIFIED (one hound 7912..11498 at y 56, leaping).
- FF83D0 -> explodes (0x4908), no reward. Death restores the event quota FF8F41.
  VERIFIED (code).

**Hound events 8 and 9** (0x388F6 / 0x387DC): wait FF83DD, quota FF8F41 = 1.
Event 8: distance [5632, 12800): timer 0x38878[0x38858[rand & 15]] (16..80
frames); on expiry with quota spawn at (1504, 56), quota -1; retires at 12800.
Event 9: distance [17152, 20480) with table 0x38858, [20480, 28160) with table
0x38868; retires at 28160. VERIFIED (code; trace).

### 5.14 Chase spawn events (stage table entries 4..7)
All read FF8396 (distance) and population flags; first spawns match run B
(kind 20 var 1 at distance 1032, kind 21 at 4617, kind 15 at 12813). VERIFIED.

| Ev | Kind / var | Routine | Rule |
|---|---|---|---|
| 4 | 15 / 0 hawk | 0x38D94 | every 8 frames wait until distance >= **12800**; clear FF8F4F; spawn at (1536, 168), FF8F4F += 1, timer 0x38E08[rankHi >> 4] = 240,224,224,224,224,208,192,176,160,144,128,112,96,80,64,48; next spawn when the timer has run out **and** FF8F4F < cap 0x38E58[rankHi >> 4] = 1,1,2,3,4,4,5,5,5,5,6,6,7,7,8,9; retires on FF83D0 |
| 5 | 20 riders from the left | 0x39010 | distance windows (0x3904A): [1024, 4352), [12800, 14848), [14848, 17152), [24320, 27648). In a window, when its timer runs out and FF8F55 is clear: spawn kind 20 at (1536, 92) facing 0, variant 0x390E4[rand & 15] (8/16 variant 1) or, beyond distance 2480, 0x390F4[rand & 15] (10/16 variant 1); FF8F55 = 1. Timer 0x39118[(rankHi >> 3) & 30] = 256, 180, 140, 128, 128, 128, 118, 30, 20, 10, 10, ... frames |
| 6 | 20 riders from the right | 0x39138 | windows 0x39172: [8704, 18176), [22016, 27392); spawns kind 20 **variant 0** facing 1 at (1920, 92), flag FF8F56, same timer table |
| 7 | 21 boarders | 0x391EE | from distance > 4608 (clears FF8EB4) to > 12544: when the timer runs out and FF8F57 is clear, spawn kind 21 at (1536, 92) facing 0, FF8F57 = 1 |

With ~6 px/frame, distance 1024 is reached ~380 frames after boarding and 28672
(the end) ~5,100 frames after boarding (run B: 6524 -> 11496). VERIFIED.

---

## 6. The wagon chase (stage climax; there is no boss)

### 6.1 Wagon (class 6 kind 2, 0x2F740) and its parts
Placed at (1728, 61) on the road below the tower. Body root **0x302CC** (14
pieces, palette **26**, hold). It is a platform (class 6): the player stands on
its deck at y + 32. VERIFIED (trace; render `vid/v008000`).

State 0 (init) spawns its parts (class 12, linked by +0x26; scenery, no collision):

| Part | Class 12 kind / var | Offset | Root | Tag |
|---|---|---|---|---|
| rear wheel | 11 / 0 | (+24, -5) | 0x302F6 (4 x 8 ticks loop, 3x2 blocks, palette 26) | SOURCE |
| front wheel | 11 / 2 | (-37, -5) | 0x30302 (same chain, other phase) | SOURCE |
| horse pulling the wagon | 14 | (+112, +11) | 0x30900 / 0x30400 (6-7 nodes x 8 ticks, 32 pieces, palettes 28/29) | SOURCE |
| driver (Willow, palette 0) | 13 | (+56, +24) | 0x30368 (3 x 8 ticks loop, 3x2 blocks, palette **0**) | SOURCE |

It then clears FF83DD, stores FF83DE/FF83E0 = its position (the wagon centre
used by boarders), clears FF8F0A. Each frame it publishes its x/y step
(FF83E4 = x step: flames and boarders on the deck move with it).

Phases (state 2, +3):

| Phase | Behaviour | Tag |
|---|---|---|
| 0 | wait until stood on (+24 set by the platform pass); then **FF83DD += 1** (camera mode 8, 2.4; fort actors retire, 2.5) | VERIFIED (key 6524) |
| 2 | accelerate: FF83DA += 4 per frame until 1536 (6 px/frame); then **FF8395 = 1** (checkpoint 9 armed, sky controller on), FF83DA = 1536, phase 4, timer 64. If FF8395 was already set (restart) the speed is 1536 at once | VERIFIED (keys 6526..6906) |
| 4 | ride: sub-machine +4 drifts the wagon across the screen at 0.5 px/frame (x += -+128 << 8): right for 64 frames (32 in later loops), pause 128, left 16, pause 512, left 16, pause 320, right 160, pause 96, left 160, pause 256, repeat (0x2F946..0x2FA34); sub-machine +5 jolts it vertically: random wait 0x2FA4C[rand & 7] = 30, 120, 1, 60, 8, 40, 30, 20 frames (paused while FF83D7 is set), then 32 steps of table 0x2FA90 (+-1..2 px bumps). End test every frame: when FF8398 >= max X - 2048 (= 30208, distance **28672**): **FF83E2 = 1** (clock stop), **FF83D0 = 1** (every chase enemy explodes, no reward), phase 6 | VERIFIED (code; FF83D0 at key 11496, virtual 30219) |
| 6.0 | measure the distance to the screen centre (camX + 192) | SOURCE |
| 6.2 | move 1 px/frame to the screen centre | SOURCE |
| 6.4 | wait 32 frames; **FF83E2 = 2**: the clock stops (1.4); a player standing in normal play is put into scripted life state 16 by the contact pass (0x39C2A; what state 16 does: UNKNOWN); the wheels and the horse react to FF83E2 (0x2FC46, 0x2FD10, 0x2FF7E: they come loose, `vid/v011600`) | VERIFIED (trace: wagon reached x 1728 and FF83E2 = 2 at key 11616) / SOURCE (part reactions) |
| 6.6 | the wagon breaks: y falls with speed +48/frame up to 1536 until it meets the floor (3C48); then FF8F0A = 1 and a class-12 kind 20 (dust/crash effect, 0x33A56) at its position | VERIFIED (render `vid/v011800`: wagon wrecked, Willow sitting on the remains) |
| 6.8 | road decelerates: FF83DA -= 5 per frame to 0; then timer 128 | VERIFIED (trace) |
| 6.10 | after 128 frames: **FF83C3 = 2** -> stage clear (7) | VERIFIED (key 12064) |

Native note: the wagon, horse, wheels and driver are one composite at a fixed
screen place (x 1728 +- drift); the player and enemies are in screen-strip
coordinates; the road and trees move with FF83DA.

### 6.2 What the task asked for and is not there
- **Mid-boss**: none. No kind-7 or other encounter actor, no camera stop with
  FF83D0/FF8470 in the fort. VERIFIED (object and event tables; run B).
- **Stage boss**: none. FF83CC (boss bar) stays 0; no class-18 record; the stage
  ends from the wagon script (6.1 phase 6.10). The chase's last 5,000 frames
  with riders, boarders, hawks and the hound play the role of the climax.
  VERIFIED.

---

## 7. Stage clear, results and story (run C, natural, no input)

### 7.1 Clear and results
Same mode-4 sequence as stage1_boss.md 6.1/6.2 with stage-2 values:

| t (run C) | Step | Event | Tag |
|---|---|---|---|
| 1400 | 0..6 | FF83C3 = 2 (injected here; in play set by the wagon, 6.1): sound **0x10**; results text **"STAGE2"** (message **0x0844**) and "CLEAR POINTS"; scripted input stream 0xA626[1] = 0x47E8E (player walks out; the game keeps running behind the text) | VERIFIED (run S: 0x10 at the clear) |
| 1404..1814 | 8 | count: TIME s x **200** (s = remaining seconds) and LIFE 8*(HP+armour) x **200**; sound **0x30** each update; run C: time 3:24 -> 204 x 200 = **40,800**, life 24 x 200 = **4,800**, total **45,600** (410 updates = 40800/100 + 2) | VERIFIED (render `vidC/v001820`) |
| 1814..2002 | 10..12 | 120 updates + fade (176 + 12) | VERIFIED |
| 2003 | 14 | sounds 0xF4, **0x08FB**; FF8343 = 0xA7B0[1] = 1; story backdrop: scroll2 **grid 7** at (x 0, y 0) (table 0xAC46[1]; stage 1 used (0, 256)) = **Cherlindrea** (the forest spirit, a pale-haired woman in white robes); FF8E85 = page count 0xA8AE[1] = **1** (two pages); stage 2 only: two palette controllers (kind 0) cycling scroll2 palettes **4** (colours 0xEA120) and **5** (0xEA1C0), +3B = 4, +3A = 8, +3C = 2 (her glow) | VERIFIED (render) / SOURCE (controller fields) |
| 2003..2055 | 16 | after 60 updates type page message (style 0x031B: font 0x7000+ASCII, 3 frames/char, destination 0x1B = scroll1 0x910160: column 2, rows 24-27); per-character 0x2F suppressed | VERIFIED |
| 2055 | 18 | page 1 done -> 60 updates -> clear rows 24-27 (7D70 x 2) and, for stage 2 only, run the palette controllers (0x0BC4); page count -1 -> step 16 again | VERIFIED (run C: 2457 back to step 16) |
| 2457..2458 | 16/18 | page 2 | VERIFIED |
| ~2757 | 18 end | fade, sound off (784C), stage index -> 2, 2E1C, 2D80, 2F8A, 381BC; **stage-3 map scene** (FF8004 = 0); stage 3 music word 4 at its play start | VERIFIED (run C stage word 2 at v2759; run S 0x04 + 0x01FA) |

Story pages (message records 0x70000 + m*168, shift 0):
- page 1, message **0x2D** (0x71D88): "I AM A SPIRIT OF THE FOREST. / THE CHILD YOU
  ARE SEARCHING FOR IS THE / QUEEN TO BE OF THE KINGDOM OF PEACE, / ELORA DANAN."
  (`&` = new row) — render `vidC/v002420`;
- page 2, message **0x2E** (0x71E30): "IF YOU WANT TO KNOW ABOUT ELORA, GO SEE THE
  WITCH FIN RAZIEL." — render `vidC/v002720`.
VERIFIED (ROM strings + renders). Then stage 3 ("FIN RAZIEL'S ISLAND", message
0x0667) follows.

### 7.2 Notes for the native game
- Results: reuse the stage-1 results screen with stage number 2 (factor 200).
- Story page: plane-B picture of scroll2 grid 7 at (0,0) (Cherlindrea) with the
  two palette cycles; text in the band (cut 3).

---

## 8. Shops, items, hidden objects

### 8.1 Chests (class 16 kind 4, energy 6, stage1_enemies.md 5.4)
Chest index = entry direction: 0 (544,160), 1 (352,384), 2 (784,160). Rewards
0x37EA6 + 0x34 (stage 2) + set*10 + chest*2 (kind 5 = gold object of that
variant, kind 6 = heart); set = FF83A4 (rotates per new game):

| Set | chest 0 (544,160) | 1 (352,384) | 2 (784,160) |
|---|---|---|---|
| 0 | gold v0 (1) | gold v6 (200) | heart |
| 1 | gold v6 | gold v0 | heart |
| 2 | heart | gold v6 | gold v0 |
| 3 | gold v6 | heart | gold v0 |
VERIFIED (ROM). Run B: chests seen at 838 (set as the debug start left it).

### 8.2 Shop
Shop door class 12 kind 8 variant 1, direction 6, at **(1352, 96)** (the "SHOP"
sign in `vid/v005600`), as stage 1's doors (`actor12_shop.h`); shop contents and
behaviour are out of scope (shop spec). VERIFIED (table).

### 8.3 Hidden object
None: stage 2's table has no class 16 kind 9 (stage 3's has one). VERIFIED.

---

## 9. Sounds in stage 2 (run S unless noted; `jsr 0x786A`)

| Word | Where |
|---|---|
| 0xF0, 0xF7, 0x12 / 0xF0, 0xF7 | world-map scene start / end |
| 0x02 + 0x01FA | stage music (play set-up, checkpoints 0-8) |
| 0x03 + 0x01FA | wagon music (scripted leap, checkpoint 9) — SOURCE |
| 0x08FB | music fade at the scripted leap (SOURCE); also before the story page |
| 0x2F | title letters (26) |
| 0x34 | arrows (archers, crate archers, crossbowmen, mounted archers) |
| 0x37 | flame appears |
| 0x3A + 0xA1 | chandelier crash (t4126) |
| 0xA0 | window lurker appears (t5041, 6143) |
| 0x8B | spear throw (t6272) |
| 0x90 | boarding soldier jumps aboard (t7575, 8532, 8721) |
| 0x32 / 0x33 | enemy hit / death burst |
| 0x2D | pickup |
| 0x2B | scripted leap landing (SOURCE) |
| 0x10 | stage clear (t12062) |
| 0x30 | results count tick |
| 0xF4, 0x08FB | story page |
Player (Madmartigan) sounds also present in run S (0x26, 0x2A, 0x2B, 0x31, 0x39,
0x3B, 0xF4...): belong to a Madmartigan spec. VERIFIED (sound word log).

---

## 10. Dump files for the pose / plane packer

All under `S/` = `/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage2/`.
`.cps` = lockstep VIDDUMP (0x80 bytes CPS-A/B registers + video RAM 0x900000..0x92FFFF,
live palettes included); render with `S/render.sh DIR KEY...` (calls `cpsframe.py`
with `CPSFULL=1`); rendered PNGs are in `S/png/`.

| File | Shows |
|---|---|
| `vidA/v003100.cps` | world map for stage 2 (Madmartigan figure above marker II, marker I cleared) |
| `vidA/v003334.cps` | stage start: "STAGE2 / CHERLINDREA'S FOREST" title over the start screen; HUD; Madmartigan |
| `vid/v000900.cps` | start screen (title being typed), forest edge and the fort wall |
| `vid/v002000.cps` | fort entrance: stone wall, window, wall torch, chest on a balcony, archer |
| `vid/v003000.cps` | fort ground floor: galleries, doors, ladder, barrels, chests |
| `vid/v003780.cps` | low window crossbowman rising (shutter open) |
| `vid/v004100.cps`, `v004130.cps`, `v004160.cps` | chandelier hanging / crash / flames; window crossbowmen |
| `vid/v004420.cps`, `v004540.cps` | crate archer firing / hit; two window crossbowmen |
| `vid/v004790.cps` | crossbowman death |
| `vid/v005050.cps`, `v005060.cps` | window lurker out / killed |
| `vid/v005600.cps` | shop door ("SHOP" sign), galleries, soldiers |
| `vid/v006160.cps`, `v006200.cps` | spearman emerging from a door, tower |
| `vid/v006300.cps` | tower balcony over the forest road (outside view, day sky) |
| `vid/v006530.cps` | boarding: wagon, horse, wheels, driver, camera at 1536 |
| `vid/v007000.cps` | chase, day sky; mounted archer (left) shooting |
| `vid/v007520.cps`, `v007620.cps`, `v007800.cps` | boarding soldier riding in / jumping / on the deck |
| `vid/v008000.cps`, `v008200.cps` | riders and hound beside the wagon |
| `vid/v008900.cps` | hawk with rock, archers both sides, hound |
| `vid/v009160.cps` | hawk swoop, hound on the road |
| `vid/v011490.cps` | chase end (FF83D0 just set), dusk sky |
| `vid/v011600.cps`, `v011800.cps` | wagon moving to centre / wrecked, Willow on the remains |
| `vid/v012000.cps`, `v012060.cps` | stopped scene just before the clear |
| `vidC/v001820.cps` | results: STAGE2 CLEAR POINTS, TIME 204 x200, LIFE 24 x200 |
| `vidC/v002420.cps` | story page 1 (Cherlindrea portrait, palette cycles) |
| `vidC/v002720.cps` | story page 2 |
| `vidC/v002900.cps` | stage-3 world map (marker III) |
| `vid/v*.cps` every 10 keys 838..12064 | every moment of stage 2 (sky palettes 0..9 can be read at distance multiples of 3072: virtual X = 1536 + 3072 s) |

RAM dumps (`ram/rKEY.bin`, 64 KB work RAM, every 2 keys) hold every actor record
(pools as stage1_enemies.md 3) for frame-by-frame comparison; `births.txt` lists
every actor's lifetime, position and energy; `flags_b.txt` the camera and stage
flags.

---

## 11. Open questions (UNKNOWN), where to look

1. **Madmartigan** (player form 4): moves, sword reach and power, jump, damage,
   animation roots — needed before stage 2 can be played. Player code reads +0x20
   (form) everywhere; start from the player dispatch and the form-4 branches
   (e.g. root tables near 0xE194/0xE428 used by the leap script), traces in `S/ram/`
   (player record FF9846).
2. Natural route to the wagon: record a scripted trace (no nudges) that climbs the
   tower to (1544,400) to see life state 12, music 0x03 and the 3:00 clock; check
   where FF8362 is set.
3. Collision ids 12-22 and 44-46 (fort only): descriptors at 0x4712 (stairs?).
4. Player life state 16 (set by the contact pass when FF83E2 = 2 at the chase
   end): what Madmartigan does there (0x39C2A).
5. Class 12 kind 6 variants 1/2 (breakable scenery) art and effect; event-2 hold
   flags FF9802/FF9803/FF789A; spear catch test 0x75C8; surface test 0x3B0A;
   kind-18 narrow box; kind-21 deck combat details; hawk tables 0x190E0/0x190F0/
   0x1914A; horse (class 12 kind 15, 0x30116) after its rider dies.
6. Presentation (user decision): cut 1 says plane B is static per area; the chase
   relies on the road (plane A) scrolling at 6 px/frame and the trees (scroll3)
   at 3 px/frame. Propose: plane A road loop + plane B scrolling as an exception
   for the chase, with the 10 sky palette steps of 2.6.
7. Where FF8395 is cleared at the next stage/new game.
8. Restart inside the chase (checkpoint 9) is from code only; trace a death on the
   wagon.
