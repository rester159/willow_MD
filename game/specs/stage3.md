# Stage 3 (Fin Raziel's Island): behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin`. Behaviour
spec for the native Genesis rewrite (NATIVE_ENGINE_PLAN.md sections 2 and 6),
organised like `stage2.md`, which it continues. Arcade addresses are citations
only. "Stage 3" = arcade stage index 2 (FF8344 = 2). The player is **Willow**
(form 0) for the whole stage: `player.md`, `magic.md` and `combat.md` apply
unchanged; this file says "as stage 1 / as stage 2" instead of repeating shared
rules.

Evidence tags (as the stage-1/2 specs):
- **VERIFIED**: two independent sources agree (disassembly read for this file plus
  a headless arcade trace, or plus the ROM data and a render).
- **SOURCE**: one source only (usually disassembly read here, not trace-checked).
- **UNKNOWN**: not established; says where to look.

Units: world X right, world **Y up**; speeds are signed **8.8 px/frame** (256 =
1 px/frame); one update = one 60 Hz frame. Hit box = (xOffset, width, yOffset,
height), box = [x-xOffset, x-xOffset+width] x [y-yOffset, y-yOffset+height].
16-direction values: 0 up, 4 right, 8 down, 12 left (the boss uses 128
directions: 0 up, 32 right, 64 down, 96 left).

**Corrections for earlier specs found while writing this one.**
1. `a5`-relative addresses with a **negative** offset are FF0xxx, not FF8xxx
   (a5 = FF8000): the hawk population bytes are **FF0F4F / FF0F50** (a5 - 0x70B1),
   the story page counter is **FF0E85** (a5 - 0x717B). stage2.md writes FF8F4F,
   FF8F55/56/57, FF8EB4, FF8E85 and "FF8F40..FF8F5F": all of these are very
   probably FF0Fxx / FF0Exx too (check each disassembly line). VERIFIED for
   FF0F4F/FF0F50 and FF0E85 (RAM).
2. stage2.md 2.6 says no scroll2 palette cycle runs in the fort. The per-stage
   palette-controller table **0x410EC** (created by 0x4109A, called from the
   object loader 0x4035E) gives stage index 1 a **kind-1 controller on scroll2
   palette 3** (table 0xEA760, 8 steps (+3B = 7), 8 frames per step, window camera
   X 0..1536) besides the kind-7 sky controller. SOURCE (ROM table; not checked in
   the stage-2 dumps).
3. hud.md 3 gives stage 3 "4:30 (4:00 last)": the stage-3 clock is 4:30 / 4:00 /
   3:00 / 2:00 by checkpoint (1.6).

**Content summary.** Stage 3 is played as Willow in three parts:
(A) **cliffs and sea pillars** (x 0..2816): a grass meadow under grey cliffs with
ledges, bomb-throwing troopers, chests, then tall rock pillars standing in the
sea (falling in is a pit death), hawks, two sinking pillars, and a rocky islet
where an armoured **lance knight** patrols (an optional encounter: no camera
lock, no boss bar); (B) the **lake crossing** (x 2816..5808): Willow rows a small
boat that moves with his walking, escorted by enemy rafts with archers and
spearmen, leaping fish, and a **storm** (day turns to night with lightning) from
camera X 3456; (C) the **boss arena** (camera fixed at 5760): a dialogue, then a
flying white-bearded sorcerer (class 18 kind 2) with magic bolts and spirit
serpents. There is **no mid-boss**. After the boss a small figure runs in and
transforms (the stage-clear trigger), then the results and three story pages
with **Fin Raziel**. VERIFIED (traces below; disassembly).

## 0. Traces used (headless lockstep, reference program; no GUI)

Harness `build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/lockstep.keys ref`,
run from that `b1` folder as
`lockstep.keys ref CORE build/reference_maincpu.bin out/mask.bin REC FRAMES` with
CORE = `.../candidate-renderer-v1/perf/gpgx-cps/genesis_plus_gx_libretro.dylib`,
`OVERCLOCK=5000`, `RAMEVERY=n RAMOUT=dir` (64 KB work RAM per n keys),
`VIDDUMP=dir VIDEVERY=n VIDFROM=k VIDMAX=100000` (CPS video dumps), `CAMLOG=1
CAMEVERY=10`. Scratch dir
`/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage3/`
(below `S/`). Keys = program VBlank count; script time "t" runs ~3 frames behind.

| Run | Settings | Covers | Output |
|---|---|---|---|
| A (natural hand-over) | `SCRIPT=stand STAGE=1 INVINCIBLE=1 ENDAT=1400`, 3600 frames (stage-2 clear injected at t 1400) | stage-2 results and story, **stage-3 map scene, title, play start** | `S/a.log`, `S/ramA/` (every 4), `S/vidA/` (every 6 from 2600) |
| AS (sound) | as A plus `DBGPC=7824 DBGN=1000000` | sound words of run A | `S/as.log`, `S/snd_as_abs.txt` |
| B (whole stage) | `SCRIPT=random STAGE=2 INVINCIBLE=1 GATEASSIST=900 NUDGE=600 KILLBOSS=1`, stopped at key 18878 | stage 3 from the debug start (play at key 838) to the clear (FF83C3 = 1 at 16258, stage word 3 at 16315) and into stage 4 | `S/b.log` (camera every 10), `S/ram/` (every 2), `S/vid/` (every 10), `S/births.txt` (actor lifetimes 700..16400), `S/births_boss.txt`, `S/flags_b.txt` (stage flags) |
| S (sound) | as B plus `DBGPC=7824 DBGN=1000000` (same trajectory: identical nudges and boss kill) | sound words of run B | `S/s.log`, `S/snd_s_abs.txt` |
| C (stage-3 clear) | `SCRIPT=stand STAGE=2 INVINCIBLE=1 ENDAT=1400`, 4200 frames | stage-3 results, the three story pages, stage-4 map and stage-4 intro start | `S/c.log`, `S/ramC/` (every 4), `S/vidC/` (every 6 from 1400) |
| CS (sound) | as C plus `DBGPC=7824` | sound words of run C | `S/cs.log`, `S/snd_cs_abs.txt` |

Harness notes: `DBGPC` makes the idle hook return early, so RAM/video dumps and
sound logging need separate runs (done above). In the DBGPC logs the key column
is FF8018 (wraps every 256): `S/unwrap.py` makes it monotonic; real key =
column + **515** in run S (anchored on the nudge/kill lines), + **1281** in run AS
from the map scene on; run CS loses wraps in the silent story pages (use RAM
anchors). Each parallel run needs its own .rec path (the harness writes
`REC.image`).

Caveats: run B's player **never boarded the boat**: at key 8352 he entered the
shop at (2800,336), the harness nudge moved him in the shop, and from 10228 on he
repeatedly fell into the lake and was bounced back up by the invincibility DIP
(y <= 256 rebounds at 14 px/frame instead of a pit death), so the boat ride (5.5,
2.4 swell) is SOURCE only; the rafts, fish and storm ran naturally around him.
The boss lost 1 HP to a real shot and was then killed by `KILLBOSS` at t 15798;
the random input then fast-forwards the post-boss scenes (FF799C), so run C is the
reference for results and story. Run B begins with rank 0x3000 (debug start).

---

## 1. Stage entry after stage 2

### 1.1 Hand-over timeline (run A, keys)

| Key | Mode (FF8004 / FF8008) | Event | Tag |
|---|---|---|---|
| ~2518 | clear step 14 | stage-2 story pages (stage2.md 7.1): sounds 0xF4, 0x08FB | VERIFIED |
| ~2704..2759 | step 18 end | fade, sound off, stage index -> **2** (stage word 2 at v2759), checkpoint 0 and last-safe position (2F8A[2] = (192, 328)), rank update (1.7) | VERIFIED |
| 2760 | mode 0 step 2 | **world-map scene** (1.2): sounds **0xF0, 0xF7, 0x12** | VERIFIED |
| 2764..2944 | step 4 | fade-in (~52 updates), then the 128-update hold (FF800A 128 -> 0) | VERIFIED |
| 2942..2944 | step 6 | sounds **0xF0, 0xF7**; fade-out; set-up 0xA0D0: stage index 2 has **no intro** (only index 3 -> 0xA14A and index 4 -> 0xA1B8 do), FF8348 set -> straight to play (0xA220) | VERIFIED |
| 2999..3000 | play sub 0 | stage reset 2DFE (clock **4:30**, HP = max, music word **4**: sounds **0x04, 0x01FA**), HUD 4F44, stage objects 4035E (also creates the stage-3 palette controllers, 2.6), stage events 9A92 | VERIFIED |
| 3004..3008 | sub 2 | fade-in job 0x1002 speed 2 (as stage 1/2) | VERIFIED |
| 3008..3106 | subs 4 / 6 / 8 | title (1.3): one sound **0x2F** per character **including spaces**, 4 frames apart: 25 characters, keys 3009..3106 | VERIFIED |
| 3116 | play step 2 | text cleared (679C); clock runs (4:30 -> 4:29 at 3116) | VERIFIED |
| 3128 | | player state 2 (control), after the stage-start gate as stage 1 | VERIFIED |

Map scene to play set-up: **240** updates; play set-up to control: **128**
updates. Buttons fast-forward these scenes (FF799C, intro.md 2). VERIFIED.

### 1.2 World-map scene for stage 3
As intro.md 4 / stage2.md 1.2 except: markers I and II use palette **0x14**
(cleared), marker III blinks **0x12/0x13** every 10 frames, IV..VI 0x12; the hero
figure is **Willow**: position table 0x35268[2] = world **(2736, 136)** (above
marker III, by the lake on the map), root 0x35280[2] -> **0x35376** (the stage-1
chain: 2x2 block palette 0x15, 42 ticks, then alternating 8-tick nodes, looping).
VERIFIED (ROM; render `vidA/v002900`: Willow above III, I and II dark). The same
scene precedes every restart after a life loss (stage2.md 1.2).

### 1.3 Title text (play sub-steps 4, 6, 8: 0x949C, 0x94CE, 0x9518)
- Sub 4: message **0x0656** (0x94C8[2] = 0x56), style 0x1412 (big font, 4
  frames/letter): `STAGE3`, columns 18-29, rows 13-14. VERIFIED (ROM record
  0x71C38 decoded: codes 76C4 76C6 7680 768C 7688 **7B04** (= 3); render
  `vidA/v003110`).
- Sub 6: when it has finished: message **0x0667** (0x9506[2] = 0x67), style
  **0x141F** (0x950C[2]; destination 0x1F = column 5, row 16): `FIN RAZIEL'S
  ISLAND` (19 big characters: F 768A, I 76A0, N 76AA, space 71A6, R **76C2**, A
  7680, Z **76E2**, E 7688, L 76A6, ' 7FEE, S 76C4, D **7686**), columns 5-42, rows
  16-17; timer 5. Sub 8: when typed, 5 updates, text layer cleared (679C), play
  step 2. VERIFIED (ROM 0x72178; render).
- Cut 3 applies (title on its own rows, plain background), as stages 1 and 2.

### 1.4 Music and clock
- **Music word 4** (sound 0x04 then 0x01FA) at play set-up for checkpoints 0-6;
  checkpoints 7-12 (on the lake) start with **music word 5**, checkpoint 13 (boss)
  with **music word 6** (records 0x31C0, 1.6). VERIFIED (ROM; runs A/S: 0x04 +
  0x01FA at play set-up).
- In play: the boat posts **music word 5** when Willow first stands on it and
  walks right (5.5; SOURCE, not reached in run B); the boss dialogue posts sound
  **0x06** (boss music, no 0x01FA, 6.3); the shop fades the music (0x10FB), plays
  0x1B + 0x02FA and on exit 0x10FB + music word 4 again (run S keys ~8354, 8412,
  10112, 10176). VERIFIED (sound log) / SOURCE (boat).
- Clock: **4:30** at checkpoints 0-2, **4:00** at 3-5, **3:00** at 6-12, **2:00**
  at 13 (records 0x31C0). Normal 1 s ticks (hud.md 3.5); it keeps running in the
  shop (run B 2:26 -> 1:59); it stops while **FF83E9** is set (the boss dialogue,
  6.3, and from the boss's death to the clear, 6.11). Run B: 4:30 at 838, 0:41 at
  the dialogue, 0:31 at the clear. VERIFIED.

### 1.5 Start position, player form
- Player start (2F08 -> stage-3 resolver 0x3164 -> record 0): **(192, 328)**,
  stage-start gate as stage 1, facing right (FF8348 set). Camera starts at **(0,
  256)** (camera min Y is 256 in this stage, 2.3). VERIFIED (traces A and B).
- Form: 0x2F72[2*4] = **0**: Willow (FF8386 = 0; run A: 4 at the stage-2 end, 0 at
  key 3000). VERIFIED.

### 1.6 Checkpoints (restart table)
Resolver **0x3164** (no special restart path, unlike stage 2's wagon), records
**0x31C0** (8 bytes: x, y, time BCD, music word). VERIFIED (ROM).

| c | x | y | time | music | camera min X (x-512, >= 0) | Where |
|---|---|---|---|---|---|---|
| 0 | 192 | 328 | 4:30 | 4 | 0 | meadow start |
| 1 | 608 | 544 | 4:30 | 4 | 96 | cliff ledges |
| 2 | 960 | 528 | 4:30 | 4 | 448 | cliff top |
| 3 | 1456 | 592 | 4:00 | 4 | 944 | sea pillars |
| 4 | 1680 | 512 | 4:00 | 4 | 1168 | sea pillars |
| 5 | 1936 | 368 | 4:00 | 4 | 1424 | islet (behind the sinking pillars, 5.4) |
| 6 | 2464 | 368 | 3:00 | 4 | 1952 | shore |
| 7 | 2880 | 316 | 3:00 | **5** | 2368 | on the lake (boat) |
| 8 | 3392 | 316 | 3:00 | 5 | 2880 | lake |
| 9 | 3904 | 316 | 3:00 | 5 | 3392 | lake |
| 10 | 4416 | 316 | 3:00 | 5 | 3904 | lake |
| 11 | 4928 | 316 | 3:00 | 5 | 4416 | lake |
| 12 | 5440 | 316 | 3:00 | 5 | 4928 | lake |
| 13 | 5952 | 352 | **2:00** | **6** | 5440 | boss arena (6.4) |

Selection from the last safe position (FF8370/FF8372, recorded while grounded,
combat.md 3.5): i = (x >> 8) * 2 + r, r = ((y >> 8) - 1) & 1 (0 when y < 256),
c = byte table **0x3190**[i] (48 bytes); the index only ever increases:

| x | 0-511 | 512-767 | 768-1023 | 1024-1279 | 1280-1535 | 1536-1791 | 1792-2303 | 2304-2815 | 2816-3327 | 3328-3839 | 3840-4351 | 4352-4863 | 4864-5375 | 5376-5631 | 5632-6143 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| c | 0 | 0 (y < 512) / 1 (y >= 512) | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |

VERIFIED (ROM).

Notes on the lake and boss checkpoints:

- Checkpoints 7-12 put the player **on the water at y 316** = on the boat: the
  boat's init (5.5) moves the boat to the player's x when the player starts with
  x in [2832, 5776], so a restart after a death on the lake resumes on the boat,
  with music word 5 from the record. SOURCE (no death on the lake was traced).
- The "last safe position" is recorded while grounded (combat.md 3.5); standing
  on the boat deck counts as grounded? UNKNOWN (needed for c 7..12 to be
  reachable; otherwise the index would stay 6).
- **Music word 5** (sound 0x05 then 0x01FA, routine 0x788A) is posted by the
  boat when the player first stands on it and walks to its x or right of it
  (5.5 phase 2). Not heard in run B (no 0x05 in the sound log: the player never
  boarded). SOURCE. The run's sound log shows 0x04 + 0x01FA re-posted once in this
  stretch, at the shop exit (stage music restored after the shop). VERIFIED
  (order in `s.log`).

### 1.7 Difficulty rank
Stage start (0x38146): excess over FF8291 halved, plus **0x10** for stage index
2 (0x3819A[2]), minus the death-count term (0x381A0). Run A (stage 2 started
from the debug select): 0x313E at the end of stage 2 -> **0x403E** at the stage-3
start. Run B (debug start 0x3000): 0x3000 -> 0x4CE9 at the shop (8352) -> 0x5ECA at
the arena (14802) -> **0x6479** at the clear (~1 per frame). VERIFIED (traces A/B).

---

## 2. Map, layers, camera, collision, palettes

### 2.1 Layers, scroll rates and stage size
- **Foreground (scroll2)**: grid **2** (0x2DBF8[2] = 0x2DF24), **mode 1**: 8 block
  rows of 32 columns; index = (((~worldY) >> 8) & 7) * 32 + ((x >> 8) & 31)
  (lookup 0x43658 / 0x436FE), 256x256-px blocks of 16x16-px cells at ROM 0x80000 +
  b*0x400:
  - row 5 (world y **512..767**, upper cliffs): column 0 empty, blocks 119, 121,
    123, 125, 127, 129 at x 256..1791;
  - row 6 (world y **256..511**): 118, 120, 122, 124, 126, 128, 130, 131, 132, 133,
    134, 135 x4, 136, 137 x6, 138, 139 at x 0..6143 (135 and 137 are the repeated
    open-lake blocks);
  - row 7 (world y 0..255) is empty: below y 256 is the pit (2.2).
  Assemble with `game/tools/stagemap.py 2 5 2 OUT` (384 x 32 cells = 6144 x 512
  px, row 0 = world y 752..767). VERIFIED (ROM; `S/map/s3grid2.cells`).
- **Far layer (scroll3)**: grid **2** (0x2EB24[2] = 0x2EE4C), mode 0 (16 x 16
  blocks of 256 px, 32x32 tiles at 0xC0000 + b*0x100), only columns 0..12 used
  (3328 px):
  - row 14 (world y 256..511, sky/clouds and the upper far scenery): 37, 37, 39,
    41, 43, 39, 45, 51, 47, 49, 51, 47, 49;
  - row 15 (world y 0..255, mountains / sea horizon): 38, 38, 40, 42, 44, 40, 46,
    52, 48, 50, 52, 48, 50.
  Blocks 47..52 (x >= 1792, i.e. camera X >= 3584) are the storm-side scenery
  (2.6). VERIFIED (ROM).
- **Scroll rates (every frame)**: scroll2 = camera (1:1 in x and y); scroll3 =
  **camera / 2 in x and in y** (FF8488 = camX / 2, FF848C = camY / 2: cam (1721,
  256) -> far (860, 128); cam (5079, 396) -> far (2539, 198); cam (5760, 354) ->
  far (2880, 177)). Far X range 0..2880 (+384 = 3264 < 3328); far Y range 128..~272.
  scroll1 (HUD/text) fixed. VERIFIED (flags_b.txt; RAM).
- **No row, line or column scroll** anywhere in the stage: the CPS-A video
  control register (0x800122) is 0x003E (row-scroll bit 0 clear) in all 1,542
  stage-3 dumps; the layer-order word (CPS-B 0x800170) is constant 0x12F8 in
  play (0x12C0 at the start / restart, 0x13B8 in the shop). VERIFIED (dumps
  840..16250).
- **Special scroll effects**: (a) the **sea swell** during the boat ride: the
  camera Y oscillates (+-10.5 px, ~172-frame period); scroll2 moves by it and
  scroll3 by half (it follows camY / 2): 2.4, SOURCE; (b) the **arena glide**:
  camera X +2 px/update to 5760, then locked (6.2); (c) palette effects: storm
  day/night with lightning and the water shimmer (2.6). Nothing like stage 2's
  virtual camera.
- **Native (parallax rule, AGENTS.md 2026-11-02)**: plane A = foreground at the
  camera; plane B = the far layer at (camX / 2, camY / 2) in **both** axes (the
  stage needs vertical far movement: camera Y 256..~543); the swell is applied as
  a camera-Y offset to both planes and the world sprites (plane B at half). The
  HUD must not pin plane B's vertical scroll here.
- **Areas** (renders, run B keys):

  | Camera X | Area | Key frames |
  |---|---|---|
  | 0..~600 | grass meadow under a grey cliff overhang; ledge bomb trooper (360,432); start (192,328); shop door on the cliff (344,503) | `vid/v001100`, `v001500` |
  | ~400..1100 | cliff faces and ledges (upper block row y 512..767), bomb troopers, chests (816,352), (880,464) | `vid/v002460`, `v002790`, `v003410` |
  | ~1000..1700 | open sea under a cloudy sky, tall grey rock pillars standing in the water (falling in = pit), hawks | `vid/v004470`, `v004570` |
  | ~1700..1900 | two sinking rock pillars | `vid/v005600`, `v005700` |
  | ~1900..2250 | rocky islet with dead brown trees: lance knight; chest (2224,368) | `vid/v006130`, `v006180` |
  | ~2250..2816 | islet / shore: trooper (2512,384), spearman (2720,352), shop door (2800,336), the boat at (2840,316) | `vid/v007500`, `v008000`, `v008350` |
  | 2816..5808 | open lake: boat ride, enemy rafts, leaping fish; storm from camera X 3456 | `vid/v010250`..`v014500` |
  | 5567..5760, then fixed 5760 | boss arena: far shore, rocky ledge (player y 351) rising to the right, dead trees | `vid/v014800`, `v015300` |

### 2.2 Terrain and water (scroll2 grid 2, collision ids)

Assembled with `game/tools/stagemap.py 2 5 2 S/map/s3grid2` (row 0 of the
cells file = world y 752..767; 384 x 32 cells). VERIFIED (ROM scan).

| x | Collision (cell world y: id) | Meaning |
|---|---|---|
| 2448..2608 | 336: 5 | flat floor (descriptor 08 8f: floor top 15 px above the cell, player y 351) |
| 2624..2783 | slopes 28..31 (1:? descending, descriptors 08 6c/68/64/60) with 46 above | the bank slopes down to the water |
| 2784 | 304: 2 | flat floor (player y ~328 at the edge) |
| 2800 | 304: 3 (0a 88: floor + blocks moving left, wall x 15) | last land cell; **land ends at x 2816** |
| 2816..5807 | only id **32** in the cell row y 288..303 | **no floor: open water** |
| 5808 | 320: 4, 304: 8 | far-shore edge |
| 5824..6143 | 320: 5 | far shore / boss ground (player y 351) |

- Id **32** has an all-zero descriptor (no wall, floor or ceiling); it marks the
  water surface row (also at x 1184..2416 in the forest). Whether any code reads
  id 32 (splash, drift): UNKNOWN (search the terrain readers for `cmp #32`).
- **Falling into the water is a pit death**: camera min Y for stage 3 is 256
  (0x2DBE[2] = 0x0100), and y <= FF845C sends the player straight to state 6
  (combat.md 3.2). No water/swim state exists (player.md 3.11). VERIFIED
  (disassembly 0xC33E; trace: rebound at y <= 256 under the invincibility DIP).
- A splash effect (class 12 kind 0 **variant 0**, 32 frames) appears at the
  surface (y ~298..302) each time the player falls through it (76 in run B,
  e.g. key 10240 at (2935,301)); who spawns it: UNKNOWN (not the player's pit code
  0xC33E; probably a water-line probe on id 32).
- The water line where the fish swim is **y 308**; rafts and the boat ride at
  **y 316** (+-2 px bob).

**Collision ids present (grid 2)**: 2, 3, 4, 5, 6, 8, 9, 10, **24-31**, 32, 37,
44, 45, 46. New against stages 1-2: **24-27** (floor slopes rising 1:4: shape
0x20, 0x24, 0x28, 0x2C = 0/4/8/12 px + localX >> 2 over four cells, descriptor
byte 5 = 0x81) and **28-31** (the mirrored, falling 1:4 slope: shapes 0x6C..0x60,
byte 5 = 0x01). 44/45 (1:2 slopes, mirrored) and 46 (flat 16, floor continues
upward) also occur in stage 2; 37 = ledge + floor (0x88, flat 15: a thin
platform). Descriptors from 0x4712 (player.md 6); the meaning of byte 5
(0x81 / 0x01 / 0x02) is UNKNOWN (not documented in player.md). VERIFIED (ROM scan
of the grid's cells).

### 2.3 Camera
- Limits at stage start (0x2DCA / 0x2DBE / 0x2DD8 [2]): max X **0x1680 = 5760**,
  min Y **0x0100 = 256**, max Y 0x0800 (open; the map top 767 limits camera Y to
  <= 543). VERIFIED (ROM; RAM FF846A = 5760, FF845C = 256, FF8434 = 2048).
- X follows the player at player X - 192 (player.md 7); moving right raises min
  X (FF838C / FF8466) to camera X - 770 (backtracking ~768 px). Y follows the
  player as player.md 7, never below 256. No per-x Y floor table (0x42712[2] = 0;
  the post-hook table 0x42E82 entry 0x42E92 is unused). Actor retirement
  (0x21C6) is the plain "x < FF838C" rule (the stage-2 wagon branch is index 1
  only). VERIFIED (trace: min X = cam - 770; disassembly).
- At camera max X the arena arrival takes over (6.2): FF83D0, camera mode 2
  (glide), mode 4 with FF8470 (locked). VERIFIED.

### 2.4 Camera on the lake

- Horizontal: normal following (player.md 7); max X 5760 (boss arena, 6.2).
  The boat clamps the player to x >= 2832. VERIFIED (limits) / SOURCE (clamp).
- **Sea swell (special scroll effect)**: when the boat passes camera X 3872 it
  sets the camera's vertical sub-mode (+4 of the camera task FF8402, i.e.
  FF8406) to **6** with FF842E = 128: each frame the camera Y velocity (8.8) moves
  by 3 toward -128 then back toward +128 (0x42C5A), y += velocity: the whole view
  rocks up and down about **+-10.5 px with a period of ~172 frames** (simulated
  from the code). It ends at the far shore (sub-mode **8**, 0x42CA8: camera Y
  moves 3 px/frame back to 256 +-2, then sub-mode 0). Whether the camera Y
  limits (256..2048) clip the swell: UNKNOWN. SOURCE (never reached in run B).
  Native: a per-frame vertical offset applied to planes A and B and to sprites
  (the far layer keeps its rate, y/2).
- The camera post-hook table 0x42E82 entry for stage 3 (0x42E92) is the per-x
  camera-Y floor table, used only when 0x42712[stage] is set (stage index 1 only):
  no effect in stage 3. VERIFIED (disassembly 0x426DC, 0x42E66).

### 2.5 Pit
Falling below world y 256 (camera min Y) is a pit death (combat.md 3.2): the sea
around the pillars (x ~1000..1900) and the whole lake (x 2816..5807) except the
boat and rafts. There is no swimming. VERIFIED (2.2; trace rebounds under the
invincibility DIP).

### 2.6 Palettes and palette effects
- Loaded at stage set-up (0x0978, stage index 2): objects 0-15 **0xE5080**,
  objects 16-31 **0xE5880** (0xE5480 + 0x400), scroll2 (32 palettes) **0xE8520**
  (0xE7D20 + 0x800), scroll1 text **0xE6EA0**, scroll3 16 palettes **0xEB100**
  into scroll3 palettes **16-31** (0xA2E[2] = 1). SOURCE (disassembly; the dumps
  in `vid/` hold the live palettes for cross-checking).
- **Palette controllers** created at object load (0x4035E -> 0x4109A, table
  0x410EC[2] = 0x4118C, 4 records: kind | bank << 4, palette, last step, frames
  per step, colour table, camera window x0, width, y0, height):

  | # | Kind | Target | Table | Steps / rate | Window | Effect |
  |---|---|---|---|---|---|---|
  | 0 | 4 | scroll2 palette **10** | 0xEA7A0 | 6 (0..5) / 8 frames | x 0..7936 (whole stage) | water shimmer |
  | 1 | 4 | scroll2 palette **11** | 0xEA880 | 6 / 8 | whole stage | water shimmer |
  | 2 | 0 | scroll3 palette **18** | 0xEC400 | 6 / 8 | whole stage | far water shimmer |
  | 3 | 3 | storm (sub-controllers on scroll3 16 / 17) | - | sequences | camera X 3456..6272 | day -> night storm, lightning, back to day after the boss |

  VERIFIED (ROM table; RAM controller pool FFF042 + 64 n from key 900; palettes
  change in the dumps).

#### Storm: day -> night with lightning (palette controller kind 3, 0x100E)

The sky and far scenery turn to a night storm when the camera reaches x 3456
(`vid/v011000` day, `v011790` fading, `v012160` night, `v011840` lightning).

- Palette controllers live in the pool FFF042 + 64 n (kind byte +20; kind table
  0xBEA: 0 0xC0A, 1 0xD1A, 2 0xEBA, 3 0x100E, 4 0x1112, 5 0x11F0, 6 0x1224, 7
  0x1252). Each has a camera window (+06 x0, +08 width, +0A y0, +0C height,
  routine 0xCD2: active while camera X - x0 <= width and camera Y - y0 <= height,
  and FF8293, FF83C3 clear). VERIFIED (RAM at keys 900..15850).
- **Storm controller** (kind 3, record FFF102 in run B): window x0 **3456**, width
  2816, y0 0, height 768 (camera X in [3456, 6272]). Created at stage start
  by the controller table 0x410EC[2] (record 3, above). VERIFIED (RAM, ROM
  table) / SOURCE (code).
  1. On entering the window: two kind-1 sequence controllers on **scroll3
     palettes 16 and 17** (tables **0xEC200** and **0xEC300**, 32 bytes per step,
     bank +3C = 3, 8 frames per step), sequence **14** = steps 0,1,2,3,4,5: day ->
     night in 48 frames; timer 600, flash timer 63. VERIFIED (palette 16 words:
     day f0ad.. at 11770 -> f047.. 11780 -> night f004.. 11810).
  2. Each frame: thunder timer: at 0 sound **0x8F**, reload **300** (first after
     600). Flash timer: at 0 reload 0x1082[rand & 3] = 63, 47, 31, 79 frames and
     start sequence **15** (5,6,5,6,7,5: double flash) or **16** (5,6,7,5: single
     flash), 50/50. VERIFIED (12 x 0x8F in the sound log over the ~4,000-frame
     storm; flash words f057 (step 6) / f025 (step 7) at irregular 30..80-frame
     gaps).
  3. When **FF83CF** is set (the boss is beaten, key 15800): sequence **17**
     (5,4,3,2,1,0: back to day, 48 frames), then the controller ends. VERIFIED
     (day palette back at 15840..15850).
  - Step palettes (scroll3 16, first words): 0 day `00ad 0aff 08ef 06df`, 1..4
    fading, 5 night `0004 0003 0003 0003`, 6 lightning bright `0057 0046 0035
    0024`, 7 lightning dim `0025 0005 0004 0003`. Palette 17's table 0xEC300 has
    the same colours in all 8 steps (it does not visibly change). VERIFIED (ROM).
  - Only scroll3 palette 16 changes; the night look also comes from the far
    picture itself in that x range (scroll3 grid 2 blocks 47..52). Native: 8
    precomputed plane-B palette-line variants, switched by the sequences above.
- **Water cycles** (kind 4, routine 0x1112 -> core 0xC56), present all stage
  (records FFF042/082/0C2 from key 900, window always): scroll2 palettes **10**
  (table **0xEA7A0**) and **11** (**0xEA880**), scroll3 palette **18**
  (**0xEC400**): every **8** frames the next of steps 0..5 (wraps after +3B = 5):
  a 48-frame rotating shimmer of the water colours. While **FF83E3** is set, the
  two scroll2 ones are switched to kind-5 controllers with sequence 18 (step 6 =
  all-white entries in 0xEA7A0/0xEA880: a water flash); what sets FF83E3:
  UNKNOWN. VERIFIED (palettes 74/75/114 = scroll2 10/11, scroll3 18 change
  throughout run B) / SOURCE (FF83E3 path).

- Boss palette jobs on object palette 26 (fade-in, hit flash, death): 6.3, 6.5,
  6.11. Story page: no palette controller (7.2).
- Native: water shimmer = 3 cycled palette lines (two plane-A colours, one
  plane-B); storm = 8 precomputed plane-B line variants for scroll3 16 switched by
  the sequences; the boss's palette jobs on its sprite line.

---

## 3. Placed objects and spawn events

### 3.1 Stage object table (0x4034A[2] = 0x40774, 19 entries)
Loaded at stage start by 0x40374 (filters as stage1_enemies.md 2.1; class 18 is
never distance-filtered, so the boss record exists from the start, 6.1). Entry
bytes class, variant, kind, direction, x, y. VERIFIED (ROM; all 19 present at key
838 in run B with these positions).

| # | class / kind / variant | dir | x, y | What it is (section) |
|---|---|---|---|---|
| 0 | 16 / 9 / 1 | 0 | (0, 0) -> random place | hidden object (8.3) |
| 1 | 12 / 8 / 0 | 7 | 344, 503 | shop door on the cliff (8.2) |
| 2 | 12 / 8 / 16 | 8 | 2800, 336 | shop door at the shore (8.2) |
| 3 | 4 / 14 / 0 | 0 | 376, 512 | spearman (4.1) |
| 4 | 4 / 14 / 0 | 0 | 611, 447 | spearman |
| 5 | 4 / 23 / 0 | 0 | 360, 432 | bomb trooper on the overhang (5.1) |
| 6..9 | 4 / 23 / 1 | 0 | (752,368) (848,608) (1104,528) (2512,384) | bomb troopers (5.1) |
| 10 | 6 / 6 / 0 | 0 | 1776, 416 | sinking rock pillar (5.4) |
| 11 | 16 / 4 / 0 | 0 | 816, 352 | chest 0 (8.1) |
| 12 | 16 / 4 / 0 | 1 | 880, 464 | chest 1 |
| 13 | 16 / 4 / 0 | 2 | 2224, 368 | chest 2 |
| 14 | 6 / 6 / 1 | 0 | 1872, 368 | sinking rock pillar (5.4) |
| 15 | 4 / 19 / 0 | 1 | 2088, 384 | lance knight (5.3) |
| 16 | 6 / 3 / 0 | 0 | 2840, 316 | Willow's boat (5.5) |
| 17 | 4 / 14 / 0 | 1 | 2720, 352 | spearman, facing left |
| 18 | 18 / 2 / 0 | 0 | 5952, 440 | **the boss** (6) |

### 3.2 Stage event table (0x9AD8[2] = 0x9BBC, 4 events)
Loaded by 0x9A92 into the event pool (stage1_enemies.md 2.2). The event's +0E
byte is the **class** it serves: the pool dispatcher 0x380A0 calls table
0x380C2[+0E]: 4 -> 0x381EC (class-4 events by kind, table 0x381FA), **6 ->
0x39814** (class-6 events), 12 -> 0x39902. Fields: kind / variant, endX (+06), y
(+08), startX (+0A), spawnY (+0C). VERIFIED (ROM; every event seen in run B).

| Ev | +0E / kind / var | startX | endX | y / spawnY | Meaning (section) |
|---|---|---|---|---|---|
| 0 | 4 / 7 / 4 | 0 | 2720 | 0 / 0 | roaming-apparition anti-idle spawner (4.3) |
| 1 | 4 / 22 / 0 | 3840 | 3840 | 308 / 308 | leaping fish from camera X 3840 (5.8) |
| 2 | **6** / 5 / 0 | 2816 | 3824 | 316 / 316 | enemy rafts (5.6) |
| 3 | 4 / 15 / 1 | 1008 | 1696 | 0 / 0 | hawks over the sea pillars (4.2) |

Event routines: kind 7 0x38A34, kind 15 0x38D82 (variant 1 path 0x38E6C), kind 22
0x39268, class-6 events 0x39814. VERIFIED.

### 3.3 Roster

| Family | Class / kind / var | Count | Where | Spawned by | Energy | Score | Drop |
|---|---|---|---|---|---|---|---|
| Bomb trooper | 4 / 23 / 0, 1 | 5 | 360..2512 | table | 24 | 500 | 0x1D990 sets |
| Bomb + 4 shards | 10 / 10 / 0, 5-7; shards 1-4 | per throw | - | troopers | - | - | - |
| Spearman | 4 / 14 / 0 | 3 | 376, 611, 2720 | table | 4 | 200 | 100 gold |
| Returning spear | 10 / 7 | per throw | - | spearmen | - | - | - |
| Hawk (variant 1) + rock | 4 / 15 / 1; 10 / 9 | up to 18, capped | camera X 1008..1696 | event 3 | 3 | 200 | set 4 |
| Lance knight | 4 / 19 / 0 | 1 | x 1952..2208 | table | 48 | 2000 | 3 x 500 gold |
| Lance segments | 10 / 8 / 0..7 | 8 per thrust | - | knight | - | - | - |
| Sinking pillars | 6 / 6 / 0, 1 | 2 | 1776, 1872 | table | - | - | - |
| Apparition | 4 / 7 / 4 | 0..1 | anywhere x < 2720 | event 0 | 127 | - | - |
| Willow's boat | 6 / 3 / 0 | 1 | 2840.. | table | - | - | - |
| Enemy raft | 6 / 5 / 0 | one per side at a time | lake | event 2 | - | - | - |
| Raft archer | 4 / 3 / 6 | 2 per raft max | on rafts | raft | **1** | as the archer | 10 gold 14/16, 100 2/16 |
| Raft spearman | 4 / 14 / 6 | 2 per raft max | on rafts | raft | 4 | 200 | 100 gold |
| Arrows | 10 / 0 | per shot | - | raft archers | - | - | - |
| Leaping fish | 4 / 22 / 0, 2, 4 | up to cap 4..14 | lake, x >= 3840 | event 1 | 1 | 200 | 1/10/100/200 gold |
| Boss (flying sorcerer) | 18 / 2 | 1 | arena | table | 80 | 5000 | 15 pickups (4400 gold) |
| Magic bolt | 10 / 11 | per cast | - | boss | - | - | - |
| Spirit serpents | 4 / 13 / 0, 2, 4 | 4 chains x 6 per summon | arena | boss | 1 (head) | - | - |
| Cast / summon glows | 12 / 18, 12 / 17 | per attack | - | boss | - | - | - |
| Arena object | 6 / 6 / 2 | 1 | (5784, 256) | boss death | - | - | - |
| Ending figure / exit timer | 16 / 2; 16 / 1 | 1 each | arena | boss death | - | - | - |
| Knight reward | 6 / 1 / 1 | 1 | (2336, 256) | knight death | - | - | - |
| Chests | 16 / 4 | 3 | 816..2224 | table | 6 | - | 8.1 |
| Hidden object | 16 / 9 / 1 | 1 | one of 3 places | table | 20 | - | companion |

Projectile census over run B (births.txt): class 10 kind 10 (bombs and shards)
385 records, kind 8 (lance) 24, kind 7 (spears) 21, kind 0 (raft arrows) 33, kind
11 (boss bolts) 2. VERIFIED.

---

## 4. Families shared with stages 1-2: stage-3 differences

### 4.1 Spearman (class 4 kind 14 variant 0) in stage 3

Identical to stage 1 (stage1_enemies.md 5.8; box (24,48,32,48), energy 4,
palette 18, score 200, drop 0x18544[0] = set 2 = 100 gold): the routine has no
stage-3 branch (its only stage test is stage index 4, 0x17946). Placed at
(376,512), (611,447) facing right, and (2720,352) facing **left**. Throws the
returning spear (class 10 kind 7, stage2.md 5.6) with sound 0x8B (run B key
7426). The two first ones were retired at 4704/4778 (camera passed); the third
was killed at 7518 (sound 0x33). VERIFIED (trace) / SOURCE (no stage branch).

### 4.2 Hawk variant 1 (class 4 kind 15 variant 1, 0x193F6) and event 3

Brown hawk flying straight across the screen over the sea, the high ones
carrying a rock (`vid/v004570`). Same art as stage2.md 5.11.

**Event 3** (class-4 event kind 15 variant 1, routine 0x38E6C; endX 1696, startX
1008): init: FF0F50 = **18** (hawks left to spawn), timer 1 (or 10 with 6/16
chance). Every other frame: active only while camera X in **[1008, 1696)**;
then the timer runs; at 0, if FF0F4F (hawks alive) < cap 0x38EAE[rankHi >> 4] =
1,1,2,2,3,3,4,4,4,5,5,6,6,7,7,8: spawn kind 15 variant 1:
- side: x = camera X - 24 facing 0 (right), or (12/16, word 0xEBBB) x = camera X
  + 408 facing 1 (left);
- speed: motion 0xED070 (**1.5 px/frame**) or (6/16, word 0x4A92) 0xED078 (2);
- height: i = 0x38FE2[rand & 15] (i = 0: 6/16, 1: 4/16, 2: 4/16, 3: 2/16 by
  the table 2,3,0,1,0,2,1,0,0,1,2,0,2,3,0,1); y = **player Y** + 0x38FF2[i] =
  +72, +56, 0, -40; carries a rock (+3F) when i = 0 or 1.
- FF0F4F += 1, FF0F50 -= 1 (at 0 the event ends); next timer 60, or 120 (8/16,
  word 0x6276), minus 0x38FD2[rankHi >> 4] = -18, -10, 0,0,0,0,0, 10, 10, 20, 20,
  30, 40, 40, 40, 59.
VERIFIED (trace: FF0F50 18 -> 3 between keys 4442 and 5882, hawk at x = camX +
408 facing 1, y = playerY + 0 / + 56) / SOURCE (tables). Note: the population
bytes are **FF0F4F / FF0F50** (a5 - 0x70B1); stage2.md writes "FF8F4F".

**Hawk variant 1** (states 0x19402: 0 init, 2 fly, 4 hit/death, 6 remove):

| Item | Value | Tag |
|---|---|---|
| Energy / score / drop | **3** / index 4 = **200** / set **4** (100 + 200 + 200 gold) | VERIFIED (hp 3) / SOURCE (score, drop) |
| Palette | colour override **22** | VERIFIED (+10 = 22) |
| Boxes | as stage2.md 5.11 flight poses: touch = shot (9,18,8,12), (20,32,5,10), (10,20,1,17); hit pose (18,36,20,32) / shot (18,36,24,48) | VERIFIED (node walk) |
| Flight | straight line by its motion table; vertical drift 0x19956: +-0.5 px/frame, direction flipped every 40 frames (+37 counter) | SOURCE / VERIFIED (y 322 -> 317 over 90 frames) |
| Rock release | while carrying, every 4 frames: y - playerY >= 48, y <= camera Y + 256, `|playerX - (x -+ 16) + 10| <= 20` (offset -16 facing right, +16 facing left) and bit (rand & 15) of 0x1953C[(rank word >> 4) & 14] (set bits 4, 8, 8, 9, 10, 12, 16, 16): spawn class 10 kind 9 at (x -+ 16, y - 24), +38 = 1; switch to the empty-claw poses | SOURCE (no release in run B) |
| Hit | energy changed: sound 0x32, root 0x195FE (hold) 10 frames, back to flight | SOURCE |
| Death | sound 0x33, explosion (0x4908), score 200, drop set 4, FF0F4F -= 1, removed | SOURCE |
| Removal | every 8 frames outside the camera box (2162); FF0F4F -= 1 | VERIFIED (removed off the left edge, 4534/4854) |

Poses (override 22): carrying 0x195F6[f] (f0 0x196B4, f1 0x19602: 4 nodes x 6
ticks, 6-10 pieces, loop); empty claws 0x195FA[f] (0x1971E / 0x1966C: 4 x 5,
3x2 / 3x3 blocks, loop); hit 0x195FE[f] (0x1990A / 0x19916, 3x2 block, hold).
The rock: stage2.md 5.11 (falls -32/frame, breaks on terrain; on stage 3 there
is no road, so it breaks).

### 4.3 Roaming apparition (event 0: class-4 event kind 7 variant 4, 0x38A34)

As stage1_enemies.md 5.10 with stage-3 values: timer **720** frames (stage 1:
600), allowed visible-enemy sum (FF9802 + FF9803 + FF789A) **2** (stage 1: 1);
active while camera X in [0, 2720); retires when camera min X >= 2720. Held while
an apparition is out (FF0F47), FF83D0, or **FF83E6** (the lance knight's
encounter flag). Spawns kind 7 variant 4 at (playerX, playerY + 80). Cancelled
(sent to phase 12) when FF83D0 / FF83E6 is set or the visible count exceeds 2.
VERIFIED (disassembly; run B: never spawned — enemies were nearly always on
screen before key 6024, and FF83E6 stayed 1 afterwards).

### 4.4 Others
- Archer (class 4 kind 3) and spearman (kind 14) appear again only as **variant
  6 raft riders** (5.7). Returning spear (class 10 kind 7) as stage2.md 5.6.
- No soldier (kind 0), beast (kind 1) or chandelier in stage 3. VERIFIED (object
  and event tables).

---

## 5. New families

### 5.1 Bomb trooper (class 4 kind 23, 0x1D266), variants 0 and 1

Green-armoured, green-helmeted foot soldier who lobs small black bombs and hops
over the player's shots (`vid/v001100`, `v002790`). Variant 0 = the one on the
cliff overhang (360,432) that drops bombs on the meadow; variant 1 = the walking
troopers. State table 0x1D284: 0 init 0x1D28C, 2 active 0x1D336, 4 dying 0x1D97C,
6 remove (0x2A36).

| Item | Value | Tag |
|---|---|---|
| Energy | **24** (+2A/+2B, 0x1D2F0) | VERIFIED (trace hp 24 at key 844, 24 -> 9 over many hits) |
| Score | event 0x1806 = index 6 = **500** | SOURCE (0x1D9B0) |
| Drop | set 0x1D990[rand & 7] = 2, 1, 2, 3, 2, 4, 2, 2 (100 gold 5/8, 10 gold 1/8, 200 gold 1/8, 100+200+200 1/8) | SOURCE |
| Palette | colour override **5** (+12 = 1, +10 = 5) on every pose; hit flash alternates 14 / 5 | VERIFIED (node attr 0x0005; render) |
| Boxes | touch **(12,24,24,40)**, shot **(16,32,24,40)** on every node | VERIFIED (node walk, both tables) |
| Motion | 0xED078: **2 px/frame** (512) by facing (0 = right, 1 = left) | VERIFIED (trace x 342 -> 312 in 14 frames of walk) |
| Sounds | 0x32 hit, 0x33 death; bombs: 0x3E on bursting | VERIFIED (sound log: 0x3E x45 before the restart) |
| Retire | every 8 frames (clock + slot): x < camera min X (21C6) | VERIFIED (removed 5548/5550/5908/6490/11946 by retirement, none killed in run B) |
| Time stop / end | FF83D2 freezes the phases; FF83D0 -> silent death (no score, no drop) | SOURCE |

**Init (state 0)**: phase 0: window W pair (+5A, +5C) = 0x1D29E[rand & 12] =
(160,320), (176,352), (144,288) or (168,336); colour override 5; root 0x1D9EA
(walk); floor probe. Phase 2: energy 24, motion 0xED078; variant 0 -> active
phase 0 at once; variant != 0 -> root 0x1DD4A and phase 4: wait until on screen
(+1), then active **phase 6**. VERIFIED (trace var 1 (752,368): waiting 844 ->
2414).

**Variant 0 (ledge thrower)**, active phases:

| Phase | Behaviour | Tag |
|---|---|---|
| 0 watch | hit -> phase 4 (falls off the ledge). If `|playerX - x| <= 128`: timer 0x1D3C2[rand & 15] = 48, 64, 96, 64, 48, 96, 128, 64, 48, 64, 96, 64, 64, 96, 64, 96 -> phase 2. If the player is more than 128 px to the **right** -> phase 4 (drops down, root 0x1DBBE). Else animate | VERIFIED (1026, 1134, 1224, 1364 -> phase 2) |
| 2 sub 0 | count down; if `x + 96 <= playerX` -> phase 4; at 0: timer 42, throw root 0x1DADE (10, 16, 16 ticks) | VERIFIED (1090, 1182, 1320 sub 2) |
| 2 sub 2 | at timer 16: **bomb** class 10 kind 10 **variant 0** at (x - 16, y + 16), same facing; at 0: phase 0, walk root | VERIFIED (bombs born 1116, 1208, 1346, 1454) |
| 4 fall | gravity +48/frame to 1792 (7 px/frame) until floor (3B0A), then root 0x1DB9A (landing crouch) 16 frames, then phase 6 = the ground AI below | VERIFIED (hit at 1494: y 432 -> 344 by 1526, crouch, phase 6 at 1542) |

**Ground AI (variant 1 from the start; variant 0 after falling): phase 6**

| Sub | Behaviour | Tag |
|---|---|---|
| 0 decide | column c = `|playerX - x| >> 4` (c > 15 -> choice 4); else choice = 0x1D660[0x1D560[(rankHi & 0xF0) + c] + rand & 15]. Rank row 0 maps c 0-3 -> group A (13/16 "0", 3/16 "1"), c 4-6 -> group B ("1"/"2" 8/8), c 7-8 -> C ("1" 4, "2" 8, "3" 4), c 9-10 -> D ("2"/"3" 8/8), c >= 11 -> E (13/16 "4", 3/16 "3"); higher ranks shift the groups toward the player (row 14: c 0-6 -> B). Choice & 3 = 0 (0 or 4): sub 2 (walk, root 0x1DD4A). Choice 1-3: face the player, sub 4, timer 48, throw root 0x1DC72 (32 ticks then hold), bomb variant = choice + 4 | VERIFIED (tables) / SOURCE (row index) |
| 2 walk | dx = playerX - x. If `|dx| > W` (W = +5A): face the player and walk toward him. If `|dx| <= 96`: face the player, then turn and walk away. Else (96 < |dx| <= W): sub 0 next frame. Each step: move, y - 1, floor probe; **no floor ahead** -> turn back, step back, choice 0x1D748[rand & 3] = 1, 2, 3, 3 -> throw. Every 8 frames (clock + slot): if any player shot (pool FFC2C6, 20 x 64) is within dx +-80, dy +-32 -> **phase 8 (dodge jump)** | VERIFIED (trace: troopers keep 96..160 px away; jumps at 1548, 1620, 1692 after shots) |
| 4 throw | at timer 16: bomb class 10 kind 10 variant **choice + 4** (5, 6 or 7) at (x - 16 if +0F = 0, x + 16 if +0F = 1; y + 8); at 0: timer 0x1D74C[rand & 3] = 40, 20, 60, 10, root 0x1DB9A (stand), sub 6 | VERIFIED (variant 5/6/7 bombs, 1762 -> 1810) |
| 6 wait | timer, then sub 0; the dodge test (every 8 frames) also runs here | VERIFIED |

**Phase 8, dodge jump**: sub 0: 7 frames crouch (root 0x1DC0E); sub 2: vertical
speed **1280** up, root 0x1DC4E; sub 4: y += speed, speed -48/frame until < 0;
sub 6: fall +48/frame to 1792, floor test once falling faster than 1024; landed:
7 frames crouch, then phase 6 sub 0. Height ~64 px. VERIFIED (trace y 344 ->
408 -> 344, 1550..1610).

**Hit (0x1D900)**: energy changed and > 0: override 14, +0 = 2 (not
collidable) for **15** frames, flashing 14 / 5 every 4 frames, sound **0x32**;
then +0 = 1, override 5. The AI keeps running during the flash. Energy 0 ->
state 4. VERIFIED (trace act 2 for 14-16 frames per hit).

**Death (state 4)**: phase 0: sound **0x33**, drop, score 500, +0 = 2, burst
class 12 kind 0 variant 5 at its position, removed next frame. Phase 2
(FF83D0): the same without score and drop. SOURCE (no kind-23 death in run B).

Poses (palette 5; touch (12,24,24,40), shot (16,32,24,40)): walk 0x1D9EA[f]
(f0 0x1D9EE, f1 0x1DA66: 4 nodes x 10 ticks, loop); throw-from-ledge 0x1DADE[f]
(0x1DAE2 / 0x1DB3E: 10, 16, 16, loop); stand/land 0x1DB9A[f] (0x1DB9E / 0x1DBAE,
hold); ledge drop 0x1DBBE[f] (0x1DBC2 / 0x1DBE8, hold); crouch 0x1DC0E[f]
(0x1DC12 / 0x1DC30, hold); jump 0x1DC4E[f] (0x1DC52 / 0x1DC62, hold); ground
throw 0x1DC72[f] (0x1DC76 / 0x1DCBA: 32 ticks, hold); run 0x1DD4A[f] (0x1DD4E /
0x1DDC6: 4 x 8, loop). Node piece counts 9-11 (walk), 11 (throw).

### 5.2 Bomb (class 10 kind 10, 0x24B52) and its shards

Small round black bomb (1 piece, code 0x1A84/0x1A85, palette 5, `vid/v004570`);
on the ground it bursts in a yellow flash and throws 4 shards. Variant table
0x24B72: 0 -> 0x24BB2, 1-4 -> 0x24CB6 (shards), 5-7 -> 0x24DE0, 8 -> 0x24F04
(rolling bomb with shards +2E = 1; not used in stage 3's first part). Removed
when outside camera [-32, +416] x [-64, +320] (checked every 8 frames). All
hurt the player on contact (1 HP). VERIFIED (births) / SOURCE (contact).

| Variant | Motion | Tag |
|---|---|---|
| 0 (ledge drop) | falls from rest, gravity +48/frame to 1792; floor test (3B0A) and 0x75B6 each frame; on the floor: 4 shards, burst | VERIFIED (64-frame lives 1116..1180) |
| 5 / 6 / 7 (lobbed) | horizontal **3 px/frame** (0xED088) by facing; vertical up speed 0x24E30[v-5] = **1280, 1280, 768**, decreasing by 0x24E36[v-5] = **80, 56, 24** per frame (apex after 16 / 23 / 32 frames); then 20 frames of free fall (+same value per frame, cap 1792) without floor test, then a floor test every frame; on the floor: 4 shards, burst | SOURCE / VERIFIED (lives 26-90 frames) |
| burst (state 4, 0x24C62) | sound **0x3E**, 24 frames, root 0x25112[f] (8 nodes x 3 ticks, palette **13**, box (14,28,8,30)); hurts until 6 frames are left | SOURCE |
| shards 1-4 | spawned at the bomb, facing = variant & 1; up speed 0x24D40[v-1] = 768, 768, 1024, 1024, -48/frame; horizontal 3 px/frame (v1, v2; 0xED088) or 2 px/frame (v3, v4; 0xED078); then fall +48 to 1792 with floor test; removed on landing. Root 0x252DE[f] (5 nodes x 4 ticks then hold, palette **24**, 1 piece, box (6,12,6,12)) | VERIFIED (4 shards per burst, 32-36-frame lives) |

Bomb box (class-10 table 0x3B0D2): **(6,12,6,12)**; flight root 0x2503E[f]
(2 nodes x 8 ticks, loop). VERIFIED (node walk).

### 5.3 Lance knight (class 4 kind 19, 0x1A7F2) — the first-part encounter

A heavily armoured silver knight on the dead-tree islet who thrusts an
extending lance (`vid/v006130` crouched, `v006180` lance through Willow).
Placed at (2088,384) facing left. He is an **encounter**, not a mid-boss: no
camera lock, no boss bar (FF83CC is not his), the player can walk past him (run
B did: x 2247 at key 7000; the knight stays, confined to x 1952..2208, until the
stage ends).

| Item | Value | Tag |
|---|---|---|
| Energy | **48** | VERIFIED (trace 48 -> 43, five hits) |
| Score | 0x180A = index 10 = **2000** | SOURCE |
| Drop | set **6** = three 500-gold pickups | SOURCE |
| Palette | colour override **15**; hit flash 14 / 15 every 4 frames for 15 frames | VERIFIED |
| Boxes | standing / walking / standing thrust **(16,32,24,44)** (touch = shot); crouched thrust **(16,32,16,24)** | VERIFIED (node walk) |
| Motion | table 0x1A8B2: **224** (0.875 px/frame) by facing | VERIFIED (x 2087 -> 2073 in 16 frames) |
| Patrol window | 0x1AC50[variant]: centre 0x820 = **2080**, half-width 128: x is clamped to [1952, 2208] and he turns at the edges (other variants for other stages: 0x370, 0x3B0, 0x888, 0x8B8, 0xEC0, 0xD40, 0xDD0) | VERIFIED (x 2098..2208 in trace) |
| Sounds | **0x91** lance thrust, **0x92** hit (both through 786C: only when on screen), 0x33 death | VERIFIED (sound log: 0x91 x3, 0x92 x5) |
| Retirement | none (state handler ends in 0x2296, not 21C6): stays until the stage ends | VERIFIED (alive 838..16316) |

Behaviour:
- **Init (state 0)**: variant 0 and **FF838A** set (beaten before, e.g. after a
  restart) -> spawns the reward platform (below) and removes itself. Else energy
  48, override 15, motion 0x1A8B2, walk root 0x1AE10, walk counter +42 = 20.
  Phase 2: variant 0 waits until `|playerX - x| <= 160` and `|playerY - y| <=
  32`, then **FF83E6 = 1** (encounter active: suppresses the apparition, 4.3)
  and becomes active. VERIFIED (FF83E6 = 1 at key 6024, player (1933,399)).
- **Phase 0 walk**: every frame: patrol clamp (turn at an edge); if `|playerX -
  x| < 48` -> turn away, back-off timer 0x1AA0A[rand & 3] = 32, 48, 64, 80,
  phase 6; every 64 frames (clock + slot) face the player — if that turns him:
  phase 4 (turn animation 0x1B104, 40 frames); walk counter +42: at 0 a random
  facing 0x1A9E4[rand & 7] (0,1,1,0,0,0,1,1), +42 = 96 (32 if it differs from
  the saved facing). Attack timer 20: at 0, bit pick 0x1A8E2[(rankHi >> 4) * 8 +
  rand & 7] (2/8 for rank rows 0..11, then 3, 4, 5, 6 /8) -> attack, else 20
  again. VERIFIED (attacks at 6124, 6350, 7028) / SOURCE (tables).
- **Attack (phase 2, 80 frames)**: pose by 0x1AA6A[(rankHi >> 4) * 8 + rand &
  7] (crouched 4/8 at rank rows 0-3 rising to 7/8 at rows 12-15): 0 = standing
  thrust root 0x1AF3C (16, 32, 127 ticks), 1 = crouched thrust root 0x1B030
  (16, 32, 127). At timer **32**: 8 lance segments (class 10 kind 8 variants
  0..7) at (x + 72 facing right / x - 72 left, y; y - 16 when crouched), sound
  0x91. If the lance has retracted (segment 0 cleared +4A) the attack ends
  early; at 0: walk. VERIFIED (segments born 6172 at x 2063 = 2135 - 72).
- **Phase 6 back-off**: walks away while `|dx| <= 160` until the timer ends.
- **Hit**: +0 = 2 for 15 frames with the flash, sound 0x92. **Death** (state 4
  phase 2): +0 = 2, sound 0x33, drop set 6, score 2000, burst class 12 kind 0
  variant 5; variant 0 also: **spawns class 6 kind 1 variant 1 at (2336, 256)**,
  FF83E6 = 0, **FF83E7 = 1**, **FF838A = 1**; removed. SOURCE (not killed in run
  B). What class 6 kind 1 variant 1 at (2336,256) is (a platform rising from the
  water at the islet's right edge? stage 1 uses kind 1 for sinking platforms):
  UNKNOWN, see the open questions; FF83E7 is the generic gate counter the harness
  bumps.

Poses (override 15): walk 0x1AE10[f] (f0 0x1AE14, f1 0x1AEA8: 4 nodes x 10
ticks, 12-14 pieces, loop); standing thrust 0x1AF3C[f] (0x1AF40 / 0x1AFB8:
16, 32, 127; 13-16 pieces); crouched thrust 0x1B030[f] (0x1B034 / 0x1B09C: 16,
32, 127; 10-14 pieces); turn 0x1B104[f] (0x1B108 / 0x1B1B8: 8 x 4, then 127).

**Lance segments (class 10 kind 8, 0x2484C)**: 8 records start together at the
spawn point (+30 = start x), each 2 pieces (codes 0x1950), palette 15, box
**(8,16,8,16)** (0x3B0D2), root 0x249A4[v] (variant 0 = the tip, 0x249DC; 1-7
shaft 0x249B4). They move outward at **6 px/frame** (0xED0BC); segment v
reverses after travelling 0x24938[v] = 84, 72, 60, 48, 36, 24, 12, 0 px, so
the lance stretches to 84 px beyond the spawn point (156 px from the knight)
with segments 12 px apart; when the tip turns it sets FF0EB6[knight variant]
and every segment heads back; each is removed when back at its start; the tip's
return clears the knight's +4A. Life ~30 frames (6172..6202). They hurt the
player on contact. VERIFIED (trace lifetimes 16..30 frames, tip longest) /
SOURCE (contact).

### 5.4 Sinking rock pillars (class 6 kind 6, 0x31702), variants 0 and 1

Tall grey sea-stack pillars the player stands on; once stood on they sink into
the sea (`vid/v005700`). Variant 2 is the boss arena's object (6.12).

| Item | Value | Tag |
|---|---|---|
| Placed | var 0 (1776,416): root **0x3184E** (24 pieces, palette **28**, hold); var 1 (1872,368): root **0x3188C** (18 pieces) | VERIFIED |
| Skipped | when the checkpoint index FF8394 >= 5 at load, removed at once (they are behind restart point 5) | SOURCE |
| Phase 0 | wait until stood on (+24 set by the platform pass) | VERIFIED (5608, 5954) |
| Phase 2 | sink: motion 0x317AA = (0, -409): **1.6 px/frame down**, for **340** (var 0) / **280** (var 1) frames, then removed | VERIFIED (var 0 5608 -> 5948, y 416 -> -126) |
| Box (class-6 table 0x3B1AA) | (8,16,48,96) | SOURCE (walk.py '6'; how the platform pass uses it: see stage1 log bridges) |

### 5.5 Willow's boat (class 6 kind 3, routine 0x30C8A), placed at (2840, 316)

A small orange rowing boat at the shore (`vid/v008350`, `v010250` lower left).

| Item | Value | Tag |
|---|---|---|
| Init (state 0) | if player x - 2832 is in [0, 2944] (x in [2832, 5776]) -> boat x = player x (restart on the lake); +24 (stood-on) = 0, +13 = 7 (probe set), bob parameters accel +34 = 4, +36 = 4, max +30 = 64, +32 = 64, speed +2C = 64, rest y +38 = y; pose 0x30EB8 | SOURCE |
| Phase 0 | wait until stood on (+24, set by the class-6 platform pass) | SOURCE |
| Phase 2 | when player x >= boat x: **FF83DC = 1** (drift flag: player.md 3.11, Willow uses the stand pose and down+jump is a normal jump), **music word 5**, phase 4 | SOURCE |
| Phase 4 (rowing) | each frame boat x = player x, clamped >= **2832** (the player x is clamped too: he cannot go back to the shore). So the boat moves exactly with Willow's walking; there is no separate boat speed. When camera X >= **3872**: bigger bob (+34 = 8, +30 = 192), camera vertical sub-mode (FF8406) = **6**, FF842E = 128 (2.4 sea swell), phase 6 | SOURCE |
| Phase 6 | boat x = player x; each frame terrain probe 0x3D56 at boat + (32, 0) (facing right; (-32, 0) facing left, table 0x3D16/0x3D36 entry 7): when it hits floor (the far shore starts at x 5808, so at boat x ~5776): camera vertical sub-mode **8** (camera Y glides back to 256 at 3 px/frame), +34 = 4, +30 = 64, **FF83DC = 0**, FF9876 = FF9855 ^ 1, state 4 | SOURCE |
| State 4 (sink) | shared with the rafts (0x31694): 8 frames, 8 frames, then 96 frames sinking 0.25 px/frame (y -= 64 per frame), then removed (state 6 -> 0x2AD2) | SOURCE |
| Bob (every frame, 0x30DA6) | vertical spring: rise with speed +2C decelerating by +34, swap pose, fall accelerating by +36 to +30, overshoot below rest, return; poses 0x30EB8 (rest), 0x30EA0 (tilt), 0x30EAC (tilt) | VERIFIED (trace: 60-frame bob, y 314..318, keys 968..1250) |
| Poses | palette **24**; 0x30EA0 / 0x30EAC: 3x2 block (48x32), 10 ticks; 0x30EB8: 6 pieces; set by code, not chained | SOURCE |
| Platform geometry | class-6 table 0x3B1AA gives (8, 16, 48, 96) for every boat/raft pose; how the platform pass reads these four words: UNKNOWN (probably half-width 48, width 96, top offset) | SOURCE (raw) |
| Lane | +0E not set (0): combat.md pass 4 tests player shots against class-6 lane 0 — what a shot does to the boat: UNKNOWN | SOURCE |

### 5.6 Raft event (event slot 2: pool +0E = 6 -> class-6 event 0x39814, kind 5)

Fields: startX (+0A) **2816**, endX (+06) **3824**, y (+08) **316**.

| Step | Rule | Tag |
|---|---|---|
| state 0 | quotas **FF0F89 = 1** (left raft) and **FF0F8A = 1** (right raft); timer = 0x39862[rand & 15] = 32, 1, 64, 32, 16, 32, 1, 64, 32, 16, 32, 1, 32, 16, 1, 1 frames | VERIFIED (table) |
| state 2 phase 0 | wait until camera X >= 2816 | VERIFIED |
| state 2 phase 2 | camera X >= 3824 -> event retires (0x2CC2). Else, while a quota remains, count the timer down; at 0 new timer, allocate a class-6 kind-5 at y 316: if the right quota is free: facing 1, x = camera X + **440**, FF0F8A -= 1; else facing 0, x = camera X - **56**, FF0F89 -= 1 | VERIFIED (births: (2795,316) f0 and (3289,316) f1 at key 10324, camera 2853) |
| Refill | a raft that sinks gives its quota back (5.7 state 6), so at most **one raft from each side** is out at a time | VERIFIED (rafts 11452 / 12048 replaced the sunk 11422 / 12018 ones) |

Rafts alive when the event retires stay out and keep escorting the player to the
boss arena (run B: rafts born 11452 and 12048 lived until 14910).

### 5.7 Enemy raft (class 6 kind 5, routine 0x313DE) and its two riders

A wide log raft (`vid/v010400`, `v012160`, `v013500`) carrying two soldiers.

| Item | Value | Tag |
|---|---|---|
| Init phase 0 | lane +0E = 2; bob as the boat (+34 4, +36 4, +30 64, +32 64, +2C 64, +38 = y); pose **0x316F6** (6x2 block = 96x32, palette **24**, single node) | VERIFIED (node) |
| Init phase 2: crew | pick 0x3144A[rand & 15] = 0,2,4,6,4,2,0,4,0,4,4,0,4,6,4,4 -> rider kinds from words 0x3145A: 0 -> (3, 3) **two archers** (4/16), 2 -> (14, 14) **two spearmen** (2/16), 4 -> (3, 14) (8/16), 6 -> (14, 3) (2/16). Allocates two class-4 actors: kind from +3B / +3A, **variant 6**, facing = raft facing, at the raft's position, link +26 = raft, rider slot +50 = 0 / 1. If no slot is free: raft removed; if only one: +3D preset (only the +50 = 1 rider exists). Then motion table 0xED070 (facing 0 **+384**, facing 1 **-384**: 1.5 px/frame inward), timer 48 | VERIFIED (births: 10326 archer + spearman / two archers on the 10324 rafts) |
| State 2 phase 0 | 48 frames gliding in at 1.5 px/frame | VERIFIED (x 2795 -> 2867 in 48 frames) |
| State 2 phase 2 (escort) | target x = player x + 100 (right raft) or player x - 100 (left raft); moves toward it at **+448** (1.75 px/frame) when the target is to the right, **-128** (0.5 px/frame) when to the left; then clamped to x <= camera X + 384 (right raft) or x >= camera X (left raft) — so the rafts are dragged along with the camera across the whole lake | VERIFIED (left raft snapped 2867 -> 2937 = camera X at key 10376) |
| Swell | camera X >= 3872: bigger bob (+34 = 8, +30 = 192) | SOURCE |
| Both riders gone | when +3C and +3D are both set (each rider sets its flag on death or FF83D0 explosion): state 4: 8 + 8 frames, then sinks 96 frames at 0.25 px/frame, state 6: quota back (FF0F89 or FF0F8A += 1), removed | VERIFIED (riders dead 10586/11316 -> raft gone 11422 = +106 frames) |
| Collision | none of its own toward the player (lane 2); whether the player can stand on an enemy raft: UNKNOWN (class-6 platform pass by lane) | UNKNOWN |

#### Raft archer (class 4 kind 3 **variant 6**)
Same archer as stage1_enemies.md 5.9 / stage2 4.2 once active; differences:
- Init 0x126E8: state 2 **phase 14**, energy **1** (+2A/+2B = 1; the placed
  archers have 4), lane +0E = 2, +44 = raft facing, facing toward the player
  (0x12D44), shot direction +30 = facing * 8 + 4 (4 right / 12 left), pose table
  **0x131AC** (hold, 4x3 block, palette **17**, touch **(18,36,20,32)** / shot
  **(18,36,24,48)**). VERIFIED (trace hp 1, palette by node walk).
- Every frame (0x12F62) its position = raft + offset: raft facing 0: rider +50 = 0
  at (-16, +16), +50 = 1 at (+16, +16); raft facing 1 mirrored. VERIFIED (trace
  rider y = raft y + 16).
- Phase 14: sub 0 waits until `|x - playerX| < 256`; sub 2: rider +50 = 1 shoots
  at once (sound **0x34** on screen, arrow class 10 kind 0 from (x +- 10, y + 4),
  table 0x12E46), phase **16**, timer 48; rider +50 = 0 only once the raft's +3C
  is set (its partner died). Phase 16: 48 frames, then **phase 6**, the normal
  archer attack cycle (stage1_enemies.md 5.9). VERIFIED (slot 47: 10326 ph14,
  10328 ph16 + arrow, 10376 ph6; ph6 cycle ~82 frames with an arrow at 10458).
- Retirement off camera is skipped for variant 6 (0x12CFA). SOURCE.
- Death: +0 = 2, pose **0x1A4B0** (the crossbowman's death, 16 frames), sound
  **0x33**, score as the archer, drop 0x13120[rand & 15] = set 1 (10 gold)
  14/16, set 2 (100 gold) 2/16, then the raft flag (+3C if +50 else +3D); removed.
  VERIFIED (slot 47 hit 10566, 0x1A4B0 at 10568, removed 10586; gold variant 2
  at 10584; second archer 10888 -> gold variant 5).
- FF83D0 (boss arena start, 14802): sound 0x33, explodes (0x492C), no reward,
  raft flag set. VERIFIED (all riders gone at 14804).

#### Raft spearman (class 4 kind 14 **variant 6**)
Same spearman as stage1_enemies.md 5.8 / stage2 5.6 (returning spear class 10
kind 7, sound **0x8B**); differences:
- Init 0x178B0: state 2 **phase 18**, +4C = facing, lane 2, energy **4**,
  roots 0x186A4 (12-tick loop, palette **18**, box (24,48,32,48)). VERIFIED (trace
  hp 4 -> 3 -> 2 -> 1 -> 0).
- Position every frame (0x1840C) = raft + (-8, +16) / (+8, +16) by +50 and the raft
  facing (table 0x1843C / 0x18444). VERIFIED.
- Phase 18: sub 0 waits until `|x - playerX| < 160`; sub 2: +50 = 1 -> phase 6
  (normal attack cycle) at once; +50 = 0 waits for the raft's +3C. After a hit it
  resumes through **phase 20** (48 frames) -> phase 6. VERIFIED (slot 44: 10326
  ph18, 10350 ph6, hit 10430, 10466 ph20, 10514 ph6).
- No off-screen retirement while variant 6 (0x17F6C). SOURCE.
- Death: 10 frames (pose 0x187A4), drop 0x18544[3] = set **2** (100 gold), raft
  flag, removed. VERIFIED (slot 44: 10844 -> 10858, gold variant 5 at 10856).

Run B: 4 rafts, 6 archers + 4 spearmen on them (births 10324..12050).

### 5.8 Fish event (event slot 1: class-4 event kind 22, routine 0x39268)

Fields: endX (+06) **3840**, x (+0A) 3840, y (+0C) **308**.

| Step | Rule | Tag |
|---|---|---|
| state 0 | every 8 frames: when camera X >= **3840**: population FF0F58 = 0, state 2 | VERIFIED (12632) |
| state 2 | cap = 0x3929A[rank high >> 4] = 4, 6, 6, 6, 6, 6, 6, 7, 7, 8, 9, 10, 11, 12, 13, 14; if FF0F58 < cap: timer 0x392D4[rand & 15] = 64, 32, 48, 64, 80, 80, 64, 48, 48, 80, 48, 64, 32, 48, 64, 32 frames, state 4 | VERIFIED (timer 48 at 12634) |
| state 4 | at timer 0: if **FF83CE** is set the event retires; else spawn class 4 kind 22 (variant 0) at (3840, 308), FF0F58 += 1, state 2 (no slot: retry next frame) | VERIFIED (spawns 12682, 12730, ...; FF83CE set at 14900, last fish 14852) |

Run B: **10 fish** in 12682..14852 (35 birth rows in `births.txt` because the
fish rewrites its variant at every leap). Rank there ~0x56 -> cap 6.

### 5.9 Leaping fish (class 4 kind 22, routine 0x1C980), variants 0/2/4 = leap type

A big orange-gold fish (`vid/v013250`, `v014500`) that swims at the surface
(only a ripple/fin shows) and leaps out in an arc trailing blue spray.

| Item | Value | Tag |
|---|---|---|
| Energy | **1** | VERIFIED (trace hp 1) |
| Score | 0x1F14 with 0x1804 = score index 4 (**200**, as the crate archer) | SOURCE |
| Drop | 0x2508 set `rand & 3` (1 / 10 / 100 / 200 gold, as stage2 5.3) | VERIFIED (gold variants 2 and 5 at 13314, 13464, 14062, 14246) |
| Palette | **25** | VERIFIED (node walk) |
| Lane | +0E = 4 | SOURCE |
| Boxes (touch = shot) | swim 0x1D07A **(16,32,4,8)** (a thin strip at the waterline); leap and re-entry poses **(16,32,8,16)** | VERIFIED (node walk) |
| Motion | 0xED078: facing 0 **+512**, facing 1 **-512** (2 px/frame) both swimming and leaping | VERIFIED (x -18 per 8 frames in the leap) |
| Sounds | **0x38** splash on re-entry (786C, on screen only), **0x33** death | VERIFIED (0x38 x18 in run B's sound log) |

Behaviour (state 2 phases +3; the state-0 routine at 0x1C994 is never used:
state 0 jumps straight to the init at 0x1C9EC):
1. **Init**: phase 4, lane 4, +40 = y (water line 308), energy 1, facing =
   0x1CA36[rand & 15] (8/16 facing 1) or 1 while FF83D2 (time stop) is set; **x
   = camera X - 32 (facing 0, enters from the left) or camera X + 416 (facing 1,
   from the right)**; swim pose 0x1D07A. VERIFIED (12682: x 3767 = camera 3799 - 32).
2. **Phase 4** (choose): phase 6 with sub-state by distance d = |x - playerX|:
   d >= 256 -> sub 0; 128..255 -> sub 0; 80..127 -> sub 2; 48..79 -> sub 4; < 48
   -> phase 10 (dive). Table 0x1CEA6. SOURCE.
3. **Phase 6** (swim toward the player at 2 px/frame, animating): sub 0 when
   d < 144: leap type = 0x1CB80[rand & 15] (2,0,0,4,-,0,0,-,2,0,-,0,0,0,-,4; "-" =
   go to sub 2); sub 2 when d < 96: 0x1CBB8 (2,-,2,4,0,2,-,2,2,-,2,0,2,-,2,4);
   sub 4 when d < 48: 0x1CBF6 (4,0,2,4,-,4,-,4,4,-,4,2,4,0,-,4; "-" = dive, phase
   10). A chosen type is stored as the **variant** (+21) and starts phase 8.
   Every 8 frames FF83CE -> phase 12 (leave). If it has swum past the player
   (0x1CEFA) -> phase 10. VERIFIED (trace) / SOURCE (tables).
4. **Phase 8** (leap, subs +4):
   - start: +2E = 1 (airborne), vertical speed = 0x1CC9A[type] = **1792, 1792,
     1536** (7, 7, 6 px/frame up), gravity 0x1CCA0[type] = **-56, -72, -72**;
     pose list 0x1D13E[facing] (5 nodes 0x1D14C.. / 0x1D192.., 3x2 blocks, last
     held), step times 0x1CC7C[type]: **10, 8, 6** frames per pose.
   - rise and fall: every 2 frames a spray droplet (class 12 kind 0 **variant 3**)
     at (x, y) + random (+-0..10, +-0..10) (table 0x1CF6E); y += speed, speed +=
     gravity; x moves 2 px/frame. Apex ~112 px (type 0), ~87 (type 2), ~64 (type
     4) above the water. VERIFIED (y 308 -> 416 and back, type 0; 308 -> 390,
     type 2).
   - back at y <= +40: sound **0x38**, splash class 12 kind 0 **variant 0** at
     (x, y + 12), +2E = 0; then 3 frames at speed -640 / gravity +40 (dips under),
     pose 0x1D1CE (3 x 2 ticks, hold); when y is back up to +40: y = +40, pose
     0x1D232, 4 frames; then phase 10. VERIFIED (sub 10..18 timings in the slot
     traces 12748..12858).
5. **Phase 10** (submerged): every 4 frames, while the player is within 160 px
   (0x1CF8E) it stays down (only the swim ripple); when the player is farther:
   surfacing pose 0x1D0F2 (4, 4, hold; 16 frames), face the player, phase 4.
   FF83CE -> phase 12. VERIFIED (slot 46: 13008 ph10 .. 13240 surfacing, 13256 ph6).
6. **Phase 12** (leave, after FF83CE): keeps swimming; every 4 frames off-camera
   removal (0x2162). SOURCE.
7. **Hit**: energy 1 -> death (state 4): sound **0x33**, burst class 12 kind 0
   **variant 4** at its position, next frame score + drop, state 6: FF0F58 -= 1,
   removed. FF83D0 -> explode (0x492C), no reward. VERIFIED (slot 46: hit 13312,
   burst (4517,397), gold 13314).
- Off-camera retirement every 4 frames only while FF83D2 (time stop) or in phase
  12; otherwise a fish lives until killed or FF83D0. SOURCE.
- What +2E does (set only in the air): UNKNOWN (probably "hurts / can be hit
  only while leaping"; check the class-4 collision passes for +2E).

---

## 6. Boss arena and boss (the stage climax; no mid-boss)

Evidence: run B (`S/ram/`, `S/vid/`, `S/births_boss.txt`, `S/flags_b.txt`; S/ = the
scratch dir of section 0); disassembly `S/boss.dis` (0x2781C..0x283AC),
`S/k13.dis` (0x16D8C..0x176F6), `S/c6k6.dis`. Run B's player is invincible and
random-driven; the boss lost 1 HP naturally and was then killed by KILLBOSS at
t15798 (key ~15798), so everything after that key is the normal death path
started artificially. The random input fast-forwards the scenes after FF83C3 = 1
(FF799C), so the post-boss timing past key 16258 is from code only.

### 6.1 The boss record exists from the stage start

- Stage table entry 18 places **class 18 kind 2** (routine **0x2781C**) at
  **(5952, 440)**. The object loader's class filter byte for class 18
  (0x403F6[9] = 0xFF) skips the distance test, so the record is created at stage
  load (key 838) with the other placed actors. VERIFIED (trace: c18 slot 7 =
  FFF810 from key 838).
- Its init (state 0 sub 0, 0x27846) runs on the first update: **FF83CC = FF83CD
  = 80**, energy +2A/+2B = 80, +0 = 2 (not collidable), bob speed +2C = 64,
  attack timer +37 = 60, velocity table +18 = 0xEEFE4, path chosen
  (6.4), facing pose 4. **This is why FF83CC reads 80 from key 844.** The bar is
  not drawn (no 4AC6 call) and the boss is not drawn (sub 2 returns before the
  draw call 2244) until the fight starts, so nothing shows. VERIFIED (trace cc=80
  at 844; renders `vid/v014910` no ENEMY bar, `v015300` bar shown).
- Native: create the boss state at stage start (bar value 80, hidden,
  intangible) or at arena entry; only the visible behaviour below matters.

### 6.2 Arrival (camera controller mode 0, 0x42942)

| Key (run B) | Event | Tag |
|---|---|---|
| 14802 | player X (FF984C) reaches **camera max X 5760** (0x2DCA[2]) while FF8362 != 0 (3 in run B): **FF83D0 = 1**, player life state **14** (scripted arrival, player spec; it lasted 34 updates in run B), camera mode 0 -> **2** | VERIFIED |
| 14802..14804 | FF83D0: every active soldier/archer/spearman/kind-22 actor explodes without reward (three sounds **0x33**, class-12 kind 0 variant 5 bursts); event spawners retire (owned by the family sections) | VERIFIED (births 14804) |
| 14802..14900 | camera mode 2 (0x42A32): camera X += **2 px per update** until = max X **5760** (5567 -> 5760 in 97 updates); camera Y keeps following the player (Y range 256..2048) | VERIFIED |
| 14900 | camera X = 5760: **FF8470 (= camera +6E) = 1**, mode 4 (0x42A56: holds while FF8470 is set). The camera X stays at 5760 for the rest of the stage; Y still follows (256 when the player stands on the arena ledge at y 351) | VERIFIED |

Arena = screen x 5760..6144, the rocky ledge the player stands on at **y 351**
(rendered `vid/v015300`: grey rock slope rising to the right, dead trees, sea
below). The boss flies over it (waypoints x 5776..6128, y 352..464).

### 6.3 Dialogue scene (boss state 0, 0x278AA..0x27972)

| Step | Behaviour | Tag |
|---|---|---|
| sub 2, waits | every update: if FF8470 set: **FF83D0 = 1, FF83CE = 1**; if **FF838B** is already set (boss seen) go straight to sub 6 (no text, no music); else FF838B = 1, sound **0x06** (boss music; no 0x01FA follows in the sound log), **FF83E9 = 1** (scene lock: clock stops, hud.md), FF82A6 = 1 (text busy), FF8E58 = 1 (UNKNOWN), timer 96, message **0x0442** style **0x0306** (dialogue window, column 4 row 6, 4 rows; typed with one sound **0x2F** per letter): **"I HAVE BEEN WAITING FOR YOU, YOUNG MAN. NOW HAVE A TASTE OF REAL MAGIC."** | VERIFIED (ROM 0x72B50 decoded; render `vid/v015100`; sound run: 0x06 then 0x2F x letters) |
| sub 4 | wait FF82A6 clear (text done), then 96 updates; clear the window (0x6770), **FF83E9 = 0** (clock runs again); timer 8; start a palette job (kind 1, 0x2C86: colours 0xE6BA0 into object palette **26**, 14 steps, 6 updates per step: the boss fades in) | VERIFIED (trace 14900 -> 15226; clock frozen at 0:41 meanwhile) / SOURCE (job fields) |
| sub 6 | 8 updates, timer 36 | VERIFIED (15226 -> 15234) |
| sub 8 | 36 updates, drawn (fading in), still intangible | VERIFIED (15234 -> 15270) |
| state 2 sub 0 | **+0 = 1 (collidable)**, ENEMY bar drawn (4AC6, value 80 = 10 cells), fight starts | VERIFIED (key 15270) |

The same dialogue window as the stage-1 sorcerer scene (stage1_boss.md 2) with
the boss invisible behind it (`vid/v014910`: empty frame, `v015100`: text).
Cut 3 (story text on its own rows) applies to this window too.

### 6.4 Checkpoint 13 and restarts

- Checkpoint record 13 (0x31C0 + 13*8): **(5952, 352), clock 2:00, music word
  6**, selected for last-safe X 5888..6143 (byte table 0x3190[46/47] = 13).
  Camera min X = 5952 - 512 = 5440. VERIFIED (ROM).
- Restarting there puts the player at X >= 5760, so the arrival (6.2) fires at
  once (if FF8362 is set). Whether FF838B survives a life loss (then the
  dialogue and the 0x06 post are skipped, 0x27904) is UNKNOWN (same question as
  stage1_boss.md open question 2; 2D80 clears it at a stage change and the boss
  clears it at death).

**The boss.** A bald, white-bearded old sorcerer in a purple robe with a red cape, floating
and gliding around the arena (`vid/v015300`, `v015420`). One body, no parts; he
fires single magic bolts and summons serpent-like spirit chains.

### 6.5 Summary

| Item | Value | Tag |
|---|---|---|
| Energy | **80** (bar FF83CC, 10 cells) | VERIFIED |
| Damage class | 0x3A4E6[2] = boss class 1 = **REACT**: normal shots hurt it (power = magic level + 1); body touch: 0x3A95A[2] -> 3A96E plain box (1 HP to the player) | VERIFIED (table) / SOURCE (policy) |
| Box (touch and shots, class-18 table **0x3B07A**) | **(20, 40, 22, 44)** on every node: x-20..x+20, y-22..y+22 | VERIFIED (node walk) |
| Hit reaction | energy changed: FF83CC = energy, bar redrawn, **+0 = 2 for 40 updates** (invulnerable), sound **0x32**, hit-flash palette job (colours 0xE6B20 -> object palette 26, 13 steps, 3 updates per step) | VERIFIED (trace 80 -> 79 at 15498, +0 = 2 until 15538) |
| Score | job **0x180C = 5000** points | VERIFIED (score 1800 -> 6800, render `v016200`) |
| Drop | reward list **7** (0x25BE): **15 gold pickups** = 1 x variant 8 (1000) + 2 x variant 7 (500) + 12 x variant 6 (200) at its position = 4400 gold | VERIFIED (births at 16058) |
| Palette | object palette **26** | VERIFIED |
| Time stop | no FF83D2 test in the boss or its summons: they keep acting during spell 10 | SOURCE (absence) |

### 6.6 Motion

- **Velocity**: 128 directions (+0F; 0 up, 32 right, 64 down, 96 left, y up),
  table +18: **0xEEFE4 = 1.5 px/update** (384), 0xEF1E4 = 2 (512), 0xEF3E4 =
  2.5 (640), 0xEF5E4 = 3 (768); x/y += table entry each update (208C). VERIFIED
  (table; trace ~1.5 px/update).
- **Steering** (0x27E56): every second update (clock bit 0 = 0) turn +0F by
  **one step** (of 128) toward the current waypoint (shorter way). VERIFIED (trace
  +0F 96 -> 95 -> 94 ...).
- **Waypoint reached** (0x27DF8) when `x - wx` in [-10, +10] and `y - wy` in
  [-14, +14]: take the next record (x, y, phase byte, direction byte): phase ->
  +4, direction -> +0F (a snap of the heading). A record with x = 0xFFFF ends
  the path: phase word = the next word (**0x0600** = phase 6 summon) and a new
  path is drawn ((random + clock) & 1). VERIFIED (code; trace snaps 96 -> 48 ->
  72 at the waypoints).
- **Bob** (0x28050, every update, all states 2): a vertical oscillation added to
  y: speed +2C ramps 64 -> 0 -> 64 by 2 per update in four 32-update quarters
  (up, down, down, up): about +-4 px, period 128 updates. SOURCE.
- **Facing pose** (0x27AC2): every 16 updates (clock & 15 = 0) the 8-way
  direction to the player ((angle + 16) >> 5) selects root table **0x2810C**
  (re-rooted only when it changes). VERIFIED.

**Paths** (x, y, phase, direction; 0x279AA path A, 0x27A14 path B):

| # | Path A | Path B |
|---|---|---|
| 0 | 5952,464 ph0 d0 | 5952,464 ph0 d0 |
| 1 | 6024,448 ph0 d32 | 5880,448 ph0 d96 |
| 2 | 5952,424 **ph2** d80 | 5952,424 **ph2** d48 |
| 3 | 5880,448 **ph4** d96 | 6024,448 **ph4** d32 |
| 4 | 5824,416 ph0 d96 | 6080,416 ph0 d32 |
| 5 | 5952,368 ph2 d56 | 5952,368 ph2 d72 |
| 6 | 6080,416 ph4 d32 | 5824,416 ph4 d96 |
| 7 | 6128,352 ph0 d48 | 5776,352 ph0 d80 |
| 8 | 6096,448 ph0 d0 | 5808,448 ph0 d0 |
| 9 | 6128,352 **ph10** d48 | 5776,352 **ph8** d80 |
| 10 | 6080,416 ph0 d0 | 5824,416 ph0 d0 |
| 11 | 5952,368 ph2 d72 | 5952,368 ph2 d56 |
| 12 | 5824,416 ph4 d96 | 6080,416 ph4 d32 |
| 13 | 5880,448 ph0 d16 | 6024,448 ph0 d112 |
| 14 | 5952,424 ph2 d48 | 5952,424 ph2 d80 |
| 15 | 6024,448 ph4 d32 | 5880,448 ph4 d96 |
| 16 | 5952,464 ph0 d112 | 5952,464 ph0 d16 |
| end | phase 6, new path | phase 6, new path |

Path B is path A mirrored about x 5952. VERIFIED (ROM).

### 6.7 Phases (state 2 sub 2, phase +4)

| Phase | Behaviour | Tag |
|---|---|---|
| 0 cruise | speed table 1.5 px; steer to the waypoint; **attack roll** (6.8) | VERIFIED |
| 2 speed up | sub-phase: every **16** updates the speed index +1 (max 3): 2, 2.5, 3 px/update; steer; no attacks | VERIFIED (code; trace +5 / table step at 15336, 15352) |
| 4 slow down | every **14** updates the speed index -1 (min 0); steer; no attacks | VERIFIED |
| 6 / 8 / 10 summon | hover (no movement, bob and animation only): sound **0x93**, face the player, root table **0x281EE**[dir8], 30 updates; then a class-12 **kind 17** summon glow (6.10) linked to it, 28 updates; then sound **0x94** and **4 spirit serpents** (class 4 kind 13, 6.9) at its position with path index +2E = base + (random & 4) + i (i = 0..3; base 0 / 8 / 16 for phase 6 / 8 / 10); 120 updates; then phase 0, facing root again | VERIFIED (trace: glow 15756, serpents 15784 with +2E 8..11 -> phase 8, path B waypoint 9) |

### 6.8 Attack: magic bolt

- Roll (0x27D70, phase 0 only): timer +37 counts down (60 at the start); at 0:
  reload from **0x27DB0[rank >> 4]** = 100, 90, 80, 65, 60, 50, 50, 45, 30, 30, 25,
  20, 15, 10, 10, 5 updates; fire if bit (random & 15) of word **0x27DC0[rank >>
  4]** is set: 0x2453, 0x58A4, 0x4595, 0x2A95, 0x6A95, 0x6A95, 0x7FB5, 0xEEDF,
  0xEEDF, 0xFDBF, 0xFFBD, 0xFFDF, 0xF7FF, 0xFFFB, 0xBFFF, 0xFFF7 (6, 6, 6, 6, 8, 8,
  12, 13, 13, 14, 14, 15, 15, 15, 15, 15 of 16). VERIFIED (ROM) / SOURCE (rank
  index = FF8346 high byte >> 4).
- Attack (state 2 sub 4, 0x27CBE), the boss stops moving:

| Step | Behaviour | Tag |
|---|---|---|
| 0 | class-12 **kind 18** cast glow linked to it (6.10); face the player, root table **0x2832A**[dir8]; 20 updates | VERIFIED (glow 15402) |
| 2 | **bolt**: class 10 **kind 11** at its position, direction = (angle to the player + 4) >> 3 (**32 directions**); 20 updates | VERIFIED (bolt 15422, +0F 12) |
| 4 | resume the saved phase and waypoint, facing root | VERIFIED (15442) |

- **Bolt (class 10 kind 11, 0x25552)**: velocity table 0xEDB64, **5 px/update**
  (1280) in 32 directions; root **0x255D0** (4 nodes x 2 ticks, loop, 2x2 block,
  palette **27**), box (class-10 table 0x3B0D2) **(7, 14, 7, 14)**; terrain test
  (36F4) on 3 of every 4 updates: on a wall/floor it spawns class-12 kind 16
  (impact) and is removed; off-camera removal (2162) on 7 of 8 updates; damage
  handler 0x3AABC[11] = 3ABBA (protection item honoured, 1 HP). VERIFIED (trace:
  3.5 px x and y per update at direction 12, lived 15422..15474 leaving the
  screen) / SOURCE (wall handling).

### 6.9 Spirit serpent (class 4 kind 13, 0x16D8C)

Six-segment chain (head, 4 body, tail) flying a fixed path; `vid/v015790`.
| Item | Value | Tag |
|---|---|---|
| Spawn | the head (variant 0) spawns 5 more kind-13 actors at its position (0x16F76) and links them (+26 = leader); in slot order the first becomes the **tail** (variant 4, start delay 40, root table 0x175CE), the others **body** (variant 2, delays 32, 24, 16, 8; root 0x176AE); the head starts after 1 update (root table 0x174EE). If (+2E & 3) = 0 the head also spawns one class-12 kind 5 sparkle | VERIFIED (24 actors at 15784 for 4 heads; variants 0/2/4) |
| Path | 24 paths 0x16E30[+2E] (0x170BE..): (updates, 16-direction) byte pairs; each leg sets +0F and re-roots the pose; direction byte 0xFF ends: state 6, removed. Each segment follows the same path after its delay (16 px spacing) | VERIFIED (table) / SOURCE |
| Speed | table 0xED364, **2 px/update**, 16 directions | VERIFIED |
| Energy | 1 (head) | VERIFIED |
| Hittable | 0x3A07A[13] -> enemy class 2 (REACT_BASE): **only the head** (variant 0) takes shots | VERIFIED (table) |
| Touch | every segment; handler 0x3A740[13] -> 3A812 (protection item honoured) | VERIFIED (table) |
| Box | **(8, 16, 8, 16)** touch and shot, every node | VERIFIED (node walk) |
| Poses | head/tail: one 2x2-block node per direction (16, palette **20**, 8 ticks); body: 3 nodes x 4 ticks loop (2x2, palette 20) | VERIFIED (walk) |
| Death | head hit (energy changed) or **FF83CF** set (boss dead): +0 = 2, state 4; each segment waits until its leader has burst, 6 updates later a class-12 kind 0 variant 4 burst at it (head to tail). **No score, no drop, no sound** of its own | VERIFIED (all 24 died at 15800..15802 after the boss death) / SOURCE (cascade order) |

### 6.10 Effects (class 12)

- **Kind 18 (0x369E4), cast glow**: root table 0x36A74[facing] (2-node flicker,
  1 tick, palette 27) for 20 updates, then 0x36A84[facing] (held, 4-6 pieces) 10
  updates; removed (lived 32 updates). VERIFIED (trace 15402..15434).
- **Kind 17 (0x36942), summon glow** (colour override 8): root 0x36C2A (2 x 6
  ticks then a held 3x3 block, palette 27) 18 updates, 120 updates, then 0x36C4E
  (4x4 flicker, 2 ticks) 60 updates; removed (lived 140). VERIFIED (15756..15896)
  / SOURCE (segment lengths).
- **Kind 5** sparkle with each serpent group (0x33B26, not decoded).

### 6.11 Death (state 4, 0x27F08..0x2802C)

| Time | Event | Tag |
|---|---|---|
| energy 0 | **FF83E9 = 1, FF83CF = 1** (scene lock, clock stops), +0 = 2, FF83CC = 0 (bar cleared), state 4; spawns **class 6 kind 6 variant 2** (6.12) | VERIFIED (key 15800) |
| +0 | sound **0x95**; timer **256**; palette job (0xE6B20 -> palette 26, 7 steps, 3 updates/step); all serpents explode (FF83CF) | VERIFIED |
| every 16 updates | class-12 kind 0 **variant 4** explosion at boss + offset (0x28030, cycling): (0,0), (24,16), (-24,-8), (8,-8), (-8,16), (8,-24), (-24,8), (6,-4); sound **0x33** (16 explosions) | VERIFIED (sound run: 0x95 then 0x33 x 16) |
| +~40..100 | the storm ends: the stage-3 sky controller (0x103E: thunder sound **0x8F** every 300 updates, lightning flash every 0x1082[rand & 3] = 63, 47, 31, 79 updates on scroll3 palettes 16/17) sees FF83CF and switches to its clear-sky branch (0x10AA): scroll3 palettes 16/17 fade from storm to bright day sky over ~60 updates and stay; scroll2 10/11 and scroll3 18 (water) keep cycling | VERIFIED (palette diff `vid/v015800..015860`; renders `v015830` dark, `v015850` bright) / SOURCE (controller branch) |
| +256 | score **5000** (0x180C), reward list 7 (6.5); spawns **class 16 kind 2** (the ending figure, 6.13) and **class 16 kind 1** (stage-exit timer; if no slot: FF83C3 = 1 at once, 0x48EC); state 6 | VERIFIED (key 16058) |
| +257 | state 6 (0x280FA): FF83CE, FF83D0, FF838B = 0; record freed. FF83CF stays 1 | VERIFIED (flags at 16058) |
| +456 | stage-exit timer (as stage1_boss.md 6): after **200** updates **FF83C3 = 1** -> stage clear mode 4 | VERIFIED (16058 -> 16258) |

### 6.12 Arena object after the death (class 6 kind 6 variant 2, 0x317B6)

Placed at **(5784, 256)** (fixed, not the boss position), lane +0E = 2, velocity
table 0x317AE (direction 0 = up 2 px/update, 1 = down 2 px/update): rises for
32 updates (to y 320), waits for **FF83E8** (set by the ending figure, 6.13),
then sinks for 32 updates and is removed. Root **0x318C2** (8 pieces, palette
28, held), class-6 box (8, 16, 48, 96) (a narrow tall surface at the arena's
left edge). VERIFIED (trace 15800..15832: y 256 -> 318) / UNKNOWN (what it is:
it was not visible in the renders at x 5784 (screen x 24); probably an invisible
wall keeping the player in the arena during the ending; check its tiles).

### 6.13 Ending figure (class 16 kind 2, 0x37884)

| Step | Behaviour | Tag |
|---|---|---|
| init | at (camera X + 416, camera Y + 95) = (6176, 351): off screen right; root **0x3795C** (2x1 block, palette 29, 3 nodes x 4 ticks, loop: a small figure running) | VERIFIED (16058 at 6176,351) |
| 0 | waits for **FF83C3** != 0, then moves left **2 px/update** until x <= camera X + 240 (6000) | VERIFIED (x 6000 at 16260, fast-forwarded) |
| 2 | 40 updates, root 0x37984 (held small pose) | SOURCE |
| 4 | 24 updates, root **0x37994** (3x3 block, palette **31**, 8 nodes x 4 ticks: a transformation burst) | SOURCE |
| 6 | **FF83E8 = 1**; root 0x379F8 (3x3 block, palette 30, held) 120 updates | SOURCE |
| 8 | **FF83C3 = 2** (results; stage1_boss.md 6.1 step 4) | VERIFIED (run B reached phase 8 with FF83C3 = 2 at 16262, fast-forwarded) |

So natural play: FF83C3 = 1 at death + 456, results about **272 updates** later
(88 walk + 40 + 24 + 120), while mode 4 runs the player's scripted walk (input
stream 0xA626[2] = 0x47E8E). Whether the walk-out exit (D9B4..D9EA) can set
FF83C3 = 2 earlier in stage 3: UNKNOWN (record a run without button input
after the boss). Who the figure is (the next story page shows Fin Raziel):
UNKNOWN visually, never rendered on screen in run B.

### 6.14 Roots (boss, palette 26; class-18 geometry 0x3B07A)

| Table | Facing 0..7 -> root | Nodes | Use |
|---|---|---|---|
| **0x2810C** | 0, 4 -> 0x2811C; 1-3 -> 0x281A8; 5-7 -> 0x28162 | 3 nodes x 10 ticks, loop (4x4 block, 16 pieces, 4x4 block) | float / glide (front, right, left) |
| **0x281EE** | 0, 4 -> 0x281FE; 1-3 -> 0x282C6; 5-7 -> 0x28262 | 2 x 15 ticks, then held | summon (staff raised) |
| **0x2832A** | 0, 4 -> 0x2833A; 1-3 -> 0x28390; 5-7 -> 0x28374 | 20 ticks, then held | cast bolt |
All nodes box (20, 40, 22, 44). The 8-way index is (angle to the player + 16)
>> 5, 0 = up, 2 = right, 4 = down, 6 = left. VERIFIED (node walk; trace nodes
0x281A8/0x281B4/0x281DE and 0x28390/0x2839C).

### 6.15 What is not there
- **Mid-boss**: none. The lance knight (5.3) sets FF83E6 (encounter) but there is
  no camera lock and no boss bar; the player can pass him. VERIFIED (run B).
- The boss bar (FF83CC) holds 80 from key 844 but is drawn only from the fight
  start (6.1). VERIFIED.

---

## 7. Stage clear, results and story (run C, natural, no input)

### 7.1 Clear and results
In play the clear comes from the boss: the exit timer (class 16 kind 1) sets
**FF83C3 = 1** 200 updates after the boss's reward (sound **0x10**, mode 4 starts,
the player walks out on the scripted input stream 0xA626[2] = 0x47E8E), and the
ending figure sets **FF83C3 = 2** (6.13). Run C injected FF83C3 = 2 at t 1400.
Same mode-4 sequence as stage1_boss.md 6.1/6.2 with stage-3 values:

| Key (run C) | Step | Event | Tag |
|---|---|---|---|
| 1404 | 4 | results text **"STAGE3 / CLEAR POINTS"** (render) | VERIFIED |
| 1408..2200 | 8 | count: TIME s x **300** and LIFE 8*(HP+armour) x **300**, sound **0x30** each update; run C: time 4:24 -> 264 x 300 = **79,200**, life 24 x 300 = **7,200**, total **86,400**; the count takes 792 updates (= 79,200 / 100: the life count runs alongside) | VERIFIED (render `vidC/v002192`: TIME264x300, LIFE 24x300) |
| 2200..2388 | 10..12 | 120 updates + fade | VERIFIED |
| 2388 | 14 | sounds **0xF4, 0x08FB**; FF8343 = 0xA7B0[2] = 1 (palette set 0xAC4); story backdrop scroll2 **grid 7** at **(1024, 256)** (table 0xAC46[2]) = **Fin Raziel**: an old woman with long grey hair holding a twisted wooden staff, on black; page counter **FF0E85** = 0xA8AE[2] = **2** (three pages); no palette controller (only stage index 1 has them, 0xA840) | VERIFIED (render `vidC/v002438`) |
| 2392 / 2444..2588 | 16 / 18 | after ~52 updates page 1 typed (style 0x031B: font 0x7000 + ASCII, 3 frames/char, column 2, rows 24-27, 40 columns, wraps at the row end); 60 updates; rows cleared | VERIFIED |
| 2648 / 2652..2828 | 16 / 18 | page 2 | VERIFIED |
| ~2884 / 2888..2992 | 16 / 18 | page 3 | VERIFIED |
| ~3050..3105 | 18 end | fade, sound off, stage index -> 3 (stage word 3 at v3105), checkpoint 0 of stage 4 | VERIFIED |
| 3108..3288 | map | **stage-4 map scene** (Madmartigan figure at marker IV, render `vidC/v003200`); then the stage-4 intro (FF8008 = 26/30, 0xA1B8) with **music word 7** (0x07 + 0x01FA) | VERIFIED |

Story pages (table 0xA8EA + stage * 4, indexed by FF0E85 counting down: [2], [1],
[0]; message records 0x70000 + m * 168):
- page 1, message **0x2F** (0x71ED8): "I AM RAZIEL.  ELORA WAS BORN TO DESTROY
  BAVMORDA." (wraps after DESTROY) — render `vidC/v002600`;
- page 2, message **0x40** (0x72A00): "YOU CANNOT HAND ELORA OVER TO BAVMORDA.
  HURRY AND SAVE HER." (wraps after BAVMORDA.) — render `vidC/v002846`;
- page 3, message **0x41** (0x72AA8): "I WILL GIVE YOU ALL MORE STRENGTH." —
  render `vidC/v002996`.
VERIFIED (ROM strings + renders).

### 7.2 Notes for the native game
- Results: the stage-1/2 results screen with stage number 3 and **factor 300**.
- Story: plane-B picture of scroll2 grid 7 at (1024, 256) (Fin Raziel), no palette
  cycle, three pages in the text band (cut 3), 40 columns with wrap.
- Then stage 4 (index 3) has its own intro (0xA1B8): not in this spec.

---

## 8. Shops, items, hidden objects

### 8.1 Chests (class 16 kind 4, energy 6)

Rewards 0x37EA6 + 0x5C (stage 3) + set * 10 + chest * 2 (kind 5 = gold object
of that variant, kind 6 = heart); set = FF83A4:

| Set | chest 0 (816,352) | 1 (880,464) | 2 (2224,368) |
|---|---|---|---|
| 0 | gold v5 (100) | gold v0 (1) | heart |
| 1 | gold v0 | gold v5 | heart |
| 2 | heart | gold v0 | gold v5 |
| 3 | gold v5 | heart | gold v0 |

VERIFIED (ROM; run B FF83A4 = 1: chest 0 opened at key 3418 -> class 16 kind 5
variant 0 at (816,344)).

### 8.2 Shop doors (class 12 kind 8)

(344,503) variant 0 direction 7 (on the cliff above the start) and (2800,336)
variant 16 direction 8 (end of the first part). Contents: shop spec. VERIFIED
(table).

The shore shop at (2800,336) (owl shopkeeper, `vid/v008700`): run B used it at
keys 8352..10226 (play step FF8009 = 4, shop state FF800C = 2); the door record
was removed after the visit. Music: 0x10FB fade, shop music 0x1B + 0x02FA, on exit
0x10FB and stage music word 4. The clock keeps running in the shop. Short class-10
kind-10 records (2..8 frames) at (2798,352) near the door: UNKNOWN. VERIFIED
(trace, sound log).

### 8.3 Hidden object (class 16 kind 9 variant 1)

As stage1_enemies.md 5.5 with variant 1: removed if **bit 1** of FF83A5 is set
(stage 1 uses bit 0, so each stage's companion is found once per game).
Position 0x37542[2] (= 0x3751A) + 0x37532[rand & 15]: **(816,656)** 6/16,
**(848,480)** 5/16, **(1264,544)** 5/16 — in the cliffs of the first part.
Box (10,20,16,20), energy 20, visible only with item FF8355; 16-frame
invulnerability per hit (sound 0x32); at energy 0: class 16 **kind 0** (the
companion, stage1_shops_companion.md 2.3) at its position, bit 1 of FF83A5 set,
removed. VERIFIED (ROM; run B: placed at (1264,544), energy 20, never shot).

### 8.4 Knight reward
The lance knight's death spawns class 6 kind 1 variant 1 at (2336, 256) (5.3):
what it is (an item, a platform rising from the water at the islet's edge): UNKNOWN.

---

## 9. Sounds in stage 3 (run S unless noted; `jsr 0x786A`)

| Word | Where |
|---|---|
| 0xF0, 0xF7, 0x12 / 0xF0, 0xF7 | world-map scene start / end (run AS) |
| 0x04 + 0x01FA | stage music (play set-up, checkpoints 0-6; again after the shop) |
| 0x05 + 0x01FA | lake music, boat boarding and checkpoints 7-12 (SOURCE; absent in run B) |
| 0x06 | boss music at the dialogue (no 0x01FA) |
| 0x10FB / 0x1B + 0x02FA | shop: music fade / shop music |
| 0x2F | title characters (25, run AS), shop text, boss dialogue letters |
| 0x3E | bomb burst (45 in run S) |
| 0x8B | spear throw (spearmen, raft spearmen) |
| 0x91 / 0x92 | lance knight thrust / lance knight hit (on screen only) |
| 0x34 | raft archer arrow |
| 0x38 | fish splash on re-entry (18) |
| 0x8F | thunder, every 300 frames in the storm (12; first ~600 frames after entering camera X 3456) |
| 0x93 / 0x94 | boss summon start / serpents released |
| 0x95 | boss death, then 0x33 x 16 explosions |
| 0x32 / 0x33 | enemy (and boss) hit / death burst |
| 0x2D | pickup |
| 0x10 | stage clear (FF83C3 = 1, key ~16261) |
| 0x08FB, 0xF4 | music fade / story page |
| 0x30 | results count tick |
| 0x07 + 0x01FA, 0x39 | stage-4 intro music and its sound (run CS) |

Player sounds also present (0x20 magic shot x493, 0x2A, 0x2B, 0x31, 0x3B):
player.md / magic.md. Counts over the stage in run S: 0x32 77, 0x33 31, 0x34 29,
0x38 18, 0x3E 45, 0x8B 13, 0x8F 12, 0x91 3, 0x92 5, 0x93/0x94/0x95 1 each.
VERIFIED (sound word log).

---

## 10. Dump files for the pose / plane / far-layer packers

All under `S/` = `/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage3/`.
`.cps` = lockstep VIDDUMP (0x80 bytes CPS-A/B registers + video RAM
0x900000..0x92FFFF, live palettes included); render with `S/render.sh DIR KEY...`
(`cpsframe.py`, `CPSFULL=1`) into `S/png/`. Run B dumps are every 10 keys
(838..18870), run A every 6 from 2600, run C every 6 from 1400.

Scenes and planes:

| File | Shows |
|---|---|
| `vidA/v002900.cps` | world map for stage 3 (Willow above marker III; I and II cleared) |
| `vidA/v003062.cps`, `v003110.cps` | title "STAGE3 / FIN RAZIEL'S ISLAND" over the start screen; HUD |
| `vid/v001000.cps`..`v002000.cps` | meadow and cliff, far layer columns 0..1 (camY 256) |
| `vid/v004000.cps`, `v004470.cps` | sea pillars, far layer columns 1..3 |
| `vid/v006000.cps`, `v007000.cps` | islet, far layer columns 3..4 |
| `vid/v008000.cps`, `v008350.cps` | shore, boat, day far layer column 5 |
| `vid/v008700.cps` | shore shop interior (owl shopkeeper) |
| `vid/v010250.cps`, `v011000.cps` | open lake by day (scroll3 palette 16 step 0), far columns 5..7 |
| `vid/v011790.cps`, `v011840.cps`, `v012160.cps` | storm fading in / lightning (step 6) / night |
| `vid/v012700.cps`, `v014000.cps` | night lake, far columns 7..10 |
| `vid/v010330.cps`, `v010600.cps`, `v011500.cps` | camera Y ~370 (rebound): far layer upper part (row 14) |
| `vid/v014800.cps` | arena arrival glide, storm sky, far columns 11..12 |
| `vid/v015850.cps`, `v016200.cps` | arena, sky back to day (far layer day palette over the storm-side blocks) |
| `vidC/v002192.cps` | results STAGE3 CLEAR POINTS, TIME 264 x300, LIFE 24 x300 |
| `vidC/v002438.cps`, `v002600.cps`, `v002846.cps`, `v002996.cps` | story backdrop (Fin Raziel) and pages 1-3 |
| `vidC/v003200.cps` | stage-4 world map (marker IV) |

Families (pose packer): the per-family lists below (first part, lake, boss).

#### Dump keys for the pose packer (run B, `vid/vKEY.cps`)

| Key | Shows |
|---|---|
| 1100 | ledge bomb trooper (kind 23 v0) mid-throw, palette 5; cliff overhang |
| 1500 | bomb burst (palette 13) on the meadow; trooper after its first hit |
| 1560-1600 | trooper dodge jump |
| 2460, 2790 | three troopers (walk, jump), chests (palette as placed) |
| 3410 | chest 0 opening |
| 4570 | hawk variant 1 (override 22), bomb (palette 5), rock pillars, trooper on the cliff |
| 4620-4700 | hawk carrying a rock (y = playerY + 56) |
| 5600, 5700 | sinking pillars (palette 28) |
| 6130 | lance knight crouched (palette 15) |
| 6180, 6400 | lance segments extended (palette 15) |
| 6360-6380 | knight hit flash (palette 14) |
| 6970 | knight turn animation |
| 7420-7520 | spearman (palette 18), returning spear |

#### Dump keys (`S/vid/vKEY.cps`; rendered PNGs in `S/png/`)

| Key | Shows |
|---|---|
| 8000, 8350 | shore: forest edge, slope to the water, the boat at (2840,316), day sky, far mountains |
| 8700 | shop interior (owl shopkeeper), stage-3 shop |
| 10250 | open lake by day, boat at left, splash |
| 10400 | two rafts: archer + spearman crews, day (raft 6x2 block palette 24, riders palettes 17/18) |
| 11000 | raft with archer, day sky (palette 16 step 0) |
| 11790 | day -> night fade (step ~2-3) |
| 11840 | lightning flash (step 6) over the night lake, two rafts |
| 12160 | night lake, two full rafts |
| 12700 | night sky, empty water (far layer night area) |
| 13250 | fish surfacing/leaping at night (palette 25), spray |
| 13500 | rafts at night, both crews, score popup |
| 14500 | fish mid-leap with blue spray trail, rafts |
| 10330, 10600, 11500 | sky-only views (camera Y ~370 from the rebound): far layer upper part |

Every key 10230..14800 (every 10) holds the lake with live palettes; flash steps
can be read where scroll3 palette 16 starts `f057` / `f025`.

#### Dumps for the pose packer (`S/vid/`, rendered in `S/png/vid-vKEY.png`)

| Key | Shows |
|---|---|
| 14800 | arrival, camera scrolling to 5760, storm sky |
| 14910 | empty dialogue window (boss invisible) |
| 15100 | dialogue text 0x0442 |
| 15230 | arena before the boss appears |
| 15300 | boss gliding (root 0x281A8 area), ENEMY bar |
| 15420 | cast pose 0x28390 + bolt (class 10 kind 11) |
| 15700 | boss gliding left side |
| 15760 | summon pose + kind-17 glow |
| 15790 | four serpents (kind 13 chains) |
| 15810, 15830 | death explosions, storm sky |
| 15850, 15900 | explosions, sky fading to day |
| 16060, 16200, 16250 | gold shower, day sky, score 6800 |
The serpents, the bolt and the kind-17/18 glows each appear in only a few dumps
(every 10 keys); RAM dumps every 2 keys (`S/ram/r0157xx.bin`) hold their node
pointers for exact frames.

RAM dumps (`ram/rKEY.bin`, 64 KB work RAM, every 2 keys) hold every actor record
(pools as stage1_enemies.md 3) for frame-exact poses; `births.txt` lists every
actor's lifetime, position and energy; `flags_b.txt` the camera and stage flags.

---

## 11. Open questions (UNKNOWN), where to look

Priority for the native stage:
1. **The boat ride is untraced** (5.5, 2.4): record a `SCRIPT=keys` run (no
   invincibility nudges) that walks onto the boat at (2840,316) and keeps walking
   right: confirm music 5, FF83DC, the x clamp >= 2832, the swell at camera 3872
   (and whether camera Y limits clip it), docking at ~5776, the boat sinking, and
   whether standing on the boat records the last safe position (needed for
   checkpoints 7-12). Also whether the player can stand on enemy rafts (lane 2).
2. **Natural boss end** (6.11-6.13): a no-input run from the boss death to time
   FF83C3 1 -> 2, render the ending figure (class 16 kind 2: who it is; the
   following story shows Fin Raziel) and the arena object class 6 kind 6 variant 2
   (visible? solid?).
3. **Lance knight death** (5.3, 8.4): kill him in a scripted run; what class 6
   kind 1 variant 1 at (2336,256) is; kind-23 death score/drop not traced either.
4. Who spawns the water splash (class 12 kind 0 variant 0) and whether anything
   reads collision id 32; what sets FF83E3 (water flash, 2.6); descriptor byte 5
   (0x81/0x01/0x02) of the new slope ids.
5. Fish +2E (airborne) meaning for collision; hawk-1 rock release (never seen).
6. FF838B across a life loss (boss dialogue replay); FF8E58; player life state
   14 (scripted arena arrival) in stage 3; class-12 kind 5 sparkle and kind 16
   bolt impact art.
7. Class-6 platform geometry format (raw (8,16,48,96) from 0x3B1AA) for pillars,
   boat and rafts.
8. Serpent path tables 0x16E30 / 0x170BE.. (24 paths) to extract for the native
   data.
9. Check the stage-2 corrections at the top of this file (FF0Fxx addresses; the
   stage-2 scroll2 palette-3 controller).

Per-part details:

#### Open questions (first part)

1. Lance knight reward: class 6 kind 1 variant 1 at (2336,256) (routine
   0x2F65E variant 1) — what it is and whether it is needed to cross to the raft
   part (run B passed without killing the knight). Kill him in a scripted run.
2. Rock drop from hawk variant 1 never seen; the rank-bit row index uses
   `(rank word >> 4) & 14` (bits 5-7 of the rank word) — confirm.
3. Bomb variant 8 (rolling, +2E shards) user: not in stage 3's first part.
4. Kind-23 death (score 500, drop) not traced; kind 23 vs player shots during
   the 15-frame flash (+0 = 2) — check that shots pass through.
5. Class-6 kind-6 box (8,16,48,96) vs the platform pass (which part is the
   standing surface).
6. FF83E6 is never cleared if the knight is not killed: the apparition stays
   off for the rest of its range (by design?).

#### Open questions for this stretch

1. The whole boat ride is SOURCE: record a scripted run (SCRIPT=keys) that walks
   onto the boat at (2840,316) and keeps walking right: confirm music 5, FF83DC,
   the x clamp, the swell (sub-mode 6) at camera 3872, the dock at ~5776, the boat
   sinking, and whether the swell is clipped by the camera Y limits.
2. Does standing on the boat record the last safe position (checkpoints 7-12)?
   Trace a death on the lake (stop invincibility after boarding).
3. Can the player stand on enemy rafts (lane 2)? What do player shots do to the
   boat (class-6 lane 0)?
4. Who spawns the water splash (class 12 kind 0 var 0) when the player falls in;
   does anything read collision id 32?
5. What sets FF83E3 (water flash). (The storm controller comes from table
   0x410EC, 2.6.)
6. Fish +2E (airborne flag) meaning for collision; whether a swimming fish (box
   (16,32,4,8) at the waterline) can hurt or be hit.
7. Class-6 platform geometry format (raw (8,16,48,96) from 0x3B1AA).

#### Open questions (boss)

1. Natural boss death and ending without fast-forward: record a no-input end
   (e.g. a scripted-keys run after the arena, or KILLBOSS with input released) to
   time FF83C3 1 -> 2 and render the ending figure (class 16 kind 2) on screen.
2. Class 6 kind 6 variant 2 (6.12): visible or not, solid for the player or not.
3. FF838B across a life loss (dialogue replay) and FF8E58's meaning.
4. Player life state 14 at the arena (scripted arrival) in stage 3: what it
   does (it ended after 34 updates in run B, unlike stage 1's long walk).
5. Serpent path table contents beyond path 7 (0x170BE.. 24 paths) were not
   tabulated here; extract them with the (updates, direction) byte-pair format.
6. Class-12 kind 5 sparkle (0x33B26) and kind 16 bolt impact: not decoded.
