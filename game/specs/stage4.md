# Stage 4 (Sorsha's Camp): behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin`. Behaviour
spec for the native Genesis rewrite (NATIVE_ENGINE_PLAN.md sections 2 and 6),
organised like `stage3.md`, which it continues. Arcade addresses are citations
only. "Stage 4" = arcade stage index 3 (FF8344 = 3). The player is
**Madmartigan** (form 4) for the whole stage: `madmartigan.md` and `combat.md`
apply; section 12 lists what this stage adds for him.

Evidence tags (as the stage-1/2/3 specs):
- **VERIFIED**: two independent sources agree (disassembly read for this file plus
  a headless arcade trace, or plus the ROM data and a render).
- **SOURCE**: one source only (usually disassembly read here, not trace-checked).
- **UNKNOWN**: not established; says where to look.

Units: world X right, world **Y up**; speeds are signed **8.8 px/frame** (256 =
1 px/frame); one update = one 60 Hz frame. Hit box = (xOffset, width, yOffset,
height), box = [x-xOffset, x-xOffset+width] x [y-yOffset, y-yOffset+height].
16-direction values: 0 up, 4 right, 8 down, 12 left; the flying mid-boss uses
128 directions (0 up, 32 right, 64 down, 96 left).

**Corrections for earlier specs and notes found while writing this one.**
1. The stage-4 intro is routine **0xA14A** (scene steps FF8008 = **26** then
   **30**; step 28 at 0xA452 runs inside the same update), not 0xA1B8: set-up
   0xA0D0 sends stage index **3** to 0xA14A (while FF83A0 is clear) and index
   **4** to 0xA1B8 (stage 5, while FF83A3 is clear). stage3.md 1.1 has this
   right; its 7.1 row "(FF8008 = 26/30, 0xA1B8)" names the wrong routine.
   VERIFIED (disassembly 0xA0D0..0xA1B8; run A: steps 26 -> 30 at keys
   3344 -> 3352).
2. `game/tools/stagemap.py` maps grid **mode 2** to 4 x 64 blocks; the lookup
   0x43722 uses `((~y) & 0x1F00) >> 2 | (x >> 8) & 63`, i.e. **32 rows x 64
   columns** (grid 0x2E024..0x2E823 is 2048 bytes). Stage 4 is the only mode-2
   grid; the tool must use (32, 64) before it can assemble this stage
   (not changed here: outside this task). VERIFIED (disassembly; grid size).
3. stage3.md 1.3 cites ROM 0x72178 for message 0x0667; title records are
   0x70000 + low byte * 84, so 0x0667 is at **0x721CC** (0x72178 is 0x0666).
   VERIFIED (ROM decode).
4. stage1_enemies.md 5.1 "change level": the routine 0x10C9C is skipped when the
   player-gate word FF988E is set **or** when the stage index is **3** (0x10CA4):
   **soldiers never jump up or drop to the player's level in stage 4**.
   VERIFIED (disassembly).

**Content summary.** Stage 4 is a long Madmartigan stage in four parts:
(A) **snow slopes** (x 0..~2600, world y 4096..~4900): a downhill snowfield in
falling snow, soldiers chasing from behind, a bomb trooper dropping in from the
right, growing **rolling snowballs**, swooping hawks, chests and a shop door at
the cave mouth; (B) the **ice cave** (x ~2600..~4600, y ~4600..5300): hanging
**bats**, a **mid-boss: a golden fire-breathing dragon** (class 18 kind 5, three
parts) in a locked arena, chests, a hidden object, a second shop; (C) **Sorsha's
camp** (x ~4600..6900, y 5424): tents that release spear throwers, soldiers,
archers, spearmen, then the **boss Sorsha** (class 18 kind 3), a sword fighter
who parries; after her defeat ("I LOST. KAEL HAS TAKEN THE BABY.") (D) the
**shield sled ride**: Madmartigan and Willow slide down the mountain on a
shield (class 6 kind 4) from (7232, 5424) to the bottom (x ~15800, y 73) over
86 gold pickups, crash into a snowball and roll into a village where a short
scene ends the stage. VERIFIED (traces below; disassembly).

## 0. Traces used (headless lockstep, reference program; no GUI)

Harness `build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/lockstep.keys ref`
as stage3.md 0 (`lockstep.keys ref CORE ROM out/mask.bin REC FRAMES`, CORE =
`.../perf/gpgx-cps/genesis_plus_gx_libretro.dylib`, `OVERCLOCK=5000`), started by
`S/run.sh NAME FRAMES` with the settings in the environment. Scratch dir
`/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage4/`
(below `S/`). Keys = program VBlank count; script time "t" runs ~3 frames behind.

| Run | Settings | Covers | Output |
|---|---|---|---|
| A (natural hand-over) | `SCRIPT=stand STAGE=2 INVINCIBLE=1 ENDAT=1400`, 5600 frames, `RAMEVERY=4`, `VIDEVERY=6 VIDFROM=2900`, `CAMLOG=1` | stage-3 results and story, **stage-4 map, intro scene, title, play start** | `S/a.log`, `S/ramA/`, `S/vidA/` |
| AS (sound) | as A plus `DBGPC=7824 DBGN=1000000` | sound words of run A | `S/as.log`, `S/snd_as.txt` (key = column + 1283) |
| B (whole stage) | `SCRIPT=random STAGE=3 INVINCIBLE=1 GATEASSIST=900 NUDGE=600 KILLBOSS=1`, 22000 frames, `RAMEVERY=2`, `VIDEVERY=10`, `CAMLOG=1 CAMEVERY=10` | stage 4 from the debug start (intro fast-forwarded by the random buttons, play at key 840) to the clear (FF83C3 = 2 at 21490, stage word 4 at 21495) and into stage 5 | `S/b.log`, `S/ram/`, `S/vid/`, `S/births.txt` (actor lifetimes 836..21500), `S/flags_b.txt` (stage flags) |
| S (sound) | as B plus `DBGPC=7824` (same trajectory) | sound words of run B | `S/s.log`, `S/snd_s.txt`, `S/snd_s_abs.txt` |
| C (stage-4 clear) | `SCRIPT=stand STAGE=3 INVINCIBLE=1 ENDAT=1400`, 4600 frames, `RAMEVERY=4`, `VIDEVERY=6 VIDFROM=1400` | stage-4 results, the two story pages, stage-5 map and start | `S/c.log`, `S/ramC/`, `S/vidC/` |
| CS (sound) | as C plus `DBGPC=7824` | sound words of run C | `S/cs.log`, `S/snd_cs.txt` |

Helper scripts in `S/`: `modes.py` (mode/step timeline), `flags.py`, `births.py`,
`slot.py` / `bslot.py` (one record's state changes), `walk.py` / `../stage3/roots2.py`
(node walkers: `S/roots.txt` holds every new family's pose tables),
`montage.py` and `render.sh` (`.cps` -> PNG in `S/png/`), `regs.py` (CPS
video/layer registers per dump), `objpal.py` (object palettes in a dump),
`sndseq.py` (condensed sound sequence), disassembly listings `S/*.dis`,
tables `S/tables.txt` (object and event tables), `S/descent_pickups.txt`.

Harness notes: as stage3.md 0. In run S the DBGPC key column drifts over long
silent stretches: real key is about column + 520 early and ~250 later at the
end of the stage; order and counts are reliable, keys within ~30 only before
key ~19000. Run B's player is invincible and random-driven with a 48-px nudge
every 600 frames when the camera stalls (none after key 13735); the mid-boss
took 1 natural hit and was then killed by `KILLBOSS` (t12658); Sorsha took 16
natural sword hits (96 -> 80) and was then killed by `KILLBOSS` (t19038).
Everything after those keys is the normal death path started artificially.
The shield ride ran naturally (71 of 86 pickups collected). Run B entered the
shop at the cave mouth (keys 7238..9546).

---

## 1. Stage entry after stage 3

### 1.1 Hand-over timeline (run A, keys)

| Key | Mode (FF8004 / FF8008) | Event | Tag |
|---|---|---|---|
| ~2900..3105 | clear step 18 | stage-3 story ends (stage3.md 7.1); fade; stage index -> **3** (stage word 3 at v3105), rank update (1.7) | VERIFIED |
| 3108 | mode 0 step 2..4 | **world-map scene** (1.2): sounds **0xF0, 0xF7, 0x12** | VERIFIED |
| 3108..3288 | step 4 | fade-in, 128-update hold | VERIFIED |
| 3288..3344 | step 6 | sounds **0xF0, 0xF7**; fade-out; set-up 0xA0D0: stage index 3 and FF83A0 clear -> **intro 0xA14A** | VERIFIED |
| 3344 | step 26 | 0xA14A: FF83A0 = 1, FF8348 = 0, stage reset 2DFE (clock **5:30**, HP = max, **music word 7**: sounds **0x07, 0x01FA** at 3346), player life state **18** (timed scene) at (448, 4176), camera and far layer at (192, 4096), class-12 **kind 7** (intro horse group, 1.3) at (-96, 4200), HUD 4F44; timer 7 | VERIFIED |
| 3352 | step 26 -> 28 -> 30 | fade-in job 0x1002 speed 2 starts (step 26 end); step 28 (0xA452, same update): FF8348 = 1, player x = **-32**, timer **256**; step 30 (0xA46E) runs actors (9EF2) and the draw (3B536) for 256 updates | VERIFIED |
| 3352..3608 | step 30 | intro scene (1.3) | VERIFIED |
| 3608 | mode 2 sub 4 | step 30 end: play mode, **sub-step 4** (the title, skipping play sub 0/2: the stage was reset at 3344), stage objects 4035E, stage events 9A92 | VERIFIED |
| 3611..3684 | subs 4 / 6 / 8 | title (1.4): 19 x sound **0x2F**, 4 updates apart | VERIFIED |
| 3692 | play step 2 | text cleared (679C); clock runs (5:30 -> 5:29) | VERIFIED |
| 3700 | | player life state 2 (control); the scripted pose from the intro is kept until then | VERIFIED |

Map scene to intro: 236 updates; intro to play: 264; play to control: 92.
Buttons fast-forward the scenes (FF799C; run B's random input crossed the
whole intro in 4 keys, 836 -> 840). VERIFIED.

**Restarts** (after a life loss): FF83A0 stays set, so 0xA0D0 skips the intro
(0xA0FC) and FF8348 = 1 sends it straight to play set-up 0xA220 (map scene ->
play sub 0, as stage 3). Where FF83A0 is cleared for the next game: UNKNOWN
(not in 2DFE; probably the new-game clear). SOURCE.

### 1.2 World-map scene for stage 4
As stage3.md 1.2 except: markers I..III palette **0x14** (cleared), marker IV
blinks 0x12/0x13, V..VI 0x12; the hero figure is **Madmartigan**: position
table 0x35268[3] = world **(2840, 112)** (above marker IV, in the mountains at
the lower right of the map), root 0x35280[3] -> **0x353E0** (the stage-2 chain:
he raises his sword once and holds the pose, palette 0x16). VERIFIED (ROM;
render `vidA/v003200`).

### 1.3 Intro scene (scene step 30, 256 updates; renders `vidA/v003410`, `v003500`, `v003584`)
A horse with two riders gallops across the snowfield from left to right while
Madmartigan runs in after them from the left edge.

| Actor | Behaviour | Tag |
|---|---|---|
| class 12 **kind 7** (0x34BDE), the horse | at (-96, 4200); spawns class 12 **kind 14 var 2** and **kind 13 var 2** linked to it (+26); moves **+3 px per update** for 192 updates (x -96 -> 480), then 32 updates still, then removed (key 3580). Root **0x1C790** (2 nodes x 8 ticks, 12 pieces, palette **16**) | VERIFIED |
| class 12 kind 13 var 2 (0x2FDC8 area), front rider | follows the horse; root **0x30390** (2x2 block, palette **0**, 6 ticks: Willow's colours, a small red-haired figure) | VERIFIED (walk; render) |
| class 12 kind 14 var 2, rear rider | follows the horse at y - 20; root **0x30400** (6 nodes x 8 ticks, 32 pieces, palette **28**: a helmeted soldier) | VERIFIED |
| sound **0x39** | 6 times, every **32** updates from key 3374 (hoofbeats) | VERIFIED (run AS) |
| player (state 18, Madmartigan's 0xD8CA) | x = -32: phase 4 waits 125 updates (node 0xE1A4), phase 6 walks right **2 px/update** (walk nodes 0xE1A4..0xE1E0, 4 ticks each) from key 3484 to x **176** at 3588, then phase 8 stands (node 0xE25C) until play state 2 at 3700 | VERIFIED |

Native: a scripted scene: three sprites moving at 3 px/update, the player's
walk-in, sound 0x39 every 32 updates; camera fixed at (0, 4096) (it moves from
192 to 0 at step 28). Cut 3 does not apply (no text).

### 1.4 Title text (play sub-steps 4, 6, 8)
- Sub 4: message **0x0657** (0x94C8[3] = 0x57), style 0x1412: `STAGE4`
  (codes 06C4 06C6 0680 068C 0688 **0B06**), columns 18-29, rows 13-14.
  VERIFIED (ROM record 0x71C8C; render `vidA/v003650`).
- Sub 6: message **0x0668** (0x9506[3] = 0x68), style **0x1420** (0x950C[3];
  destination 0x20): `SORSHA'S CAMP` (13 big characters; the record starts with
  four 0x000E words), drawn from column 10, rows 16-17; timer 5. Sub 8: when
  typed, 5 updates, text cleared, play step 2. Big codes: S 06C4, O **06AC**, R
  06C2, H 068E, A 0680, ' 0FEE, space 01A6, C **0684**, M **06A8**, P **06AE**.
  VERIFIED (ROM 0x72220; render `vidA/v003680`; 19 x 0x2F = 6 + 13).
- Cut 3 applies (title on its own rows, plain background).

### 1.5 Music and clock
- **Music word 7** (0x07 then 0x01FA) at the intro / play set-up for
  checkpoints 0-5 and 9-13; checkpoints 6-8 (inside the cave) start with
  **music word 9** (records 0x3334, 1.7). VERIFIED (ROM; runs AS/S).
- In play the music follows two **music triggers** (class 16 kind 8, 8.4):
  (2656, 4912) variant 0 posts **0x09** (cave music) when the player passes x
  2656; (4352, 5264) variant 1 posts **0x07** (stage music again) at x 4352.
  The shop fades the music (0x10FB), plays 0x1B + 0x02FA, and on exit re-posts
  the current music. The ride starts with sound **0x08** (6.13); no separate
  boss music for the dragon or for Sorsha. VERIFIED (run S order: 0x10FB, 0x1B
  0x02FA, shop, 0x10FB, 0x09 x2 + 0x01FA at the cave mouth; 0x07 again near the
  cave exit; 0x08 at the ride start).
- Clock: **5:30** at checkpoints 0-4, **5:00** at 5-6, **4:30** at 7-9, **4:00**
  at 10-12, **3:30** at 13 (records 0x3334). Normal 1 s ticks (hud.md 3.5);
  under 0:16 a warning sound **0x43** every 32 updates (0x4EBA). The clock
  routine 0x4EA2 stops while FF83D2, **FF83E9** or **FF83CF** is set or FF83E2
  >= 2: it stops at Sorsha's defeat (FF83E9 for her line, then FF83CF) and stays
  stopped through the whole ride and the end scene. Run B: 5:30 at 840, 0:29 at
  Sorsha's defeat, still 0:29 at the clear. VERIFIED (disassembly + trace).

### 1.6 Start position, player form
- Player start after the intro: **(176, 4176)** (walked in, 1.3), facing right.
  The checkpoint-0 record (128, 4176) is used for restarts; initial last-safe
  position 0x2FA2[3] = (192, 4224). Camera starts at **(0, 4096)** (camera min Y
  is 4096 at the stage start, 2.3). VERIFIED (traces A and B).
- Form: 0x2F72[3*4] = **4**: Madmartigan (FF8386 = 4 from key 3344).
  VERIFIED.

### 1.7 Checkpoints (restart table)
Resolver **0x3230** (0x2F66[3]), records **0x3334** (8 bytes: x, y, time BCD,
music word). VERIFIED (ROM).

| c | x | y | time | music | camera min X | Where |
|---|---|---|---|---|---|---|
| 0 | 128 | 4176 | 5:30 | 7 | 0 | slope top (start) |
| 1 | 544 | 4208 | 5:30 | 7 | 32 | slope |
| 2 | 1056 | 4384 | 5:30 | 7 | 544 | slope |
| 3 | 1568 | 4520 | 5:30 | 7 | 1056 | slope, snowballs |
| 4 | 2080 | 4752 | 5:30 | 7 | 1568 | lower slope |
| 5 | 2592 | 4888 | 5:00 | 7 | 2080 | cave mouth (shop) |
| 6 | 3104 | 4664 | 5:00 | **9** | 2592 | cave |
| 7 | 3872 | 4664 | 4:30 | 9 | 3360 | after the dragon arena |
| 8 | 4048 | 4952 | 4:30 | 9 | 3536 | lower cave |
| 9 | 4192 | 5240 | 4:30 | 7 | 3680 | lower cave (by the second music trigger) |
| 10 | 4896 | 5432 | 4:00 | 7 | 4384 | camp |
| 11 | 5408 | 5432 | 4:00 | 7 | 4896 | camp |
| 12 | 5920 | 5432 | 4:00 | 7 | 5408 | camp |
| 13 | **6576** | 5432 | **3:30** | 7 | **6528** | Sorsha's arena (record x word 0x99B0: bit 15 set -> x = 0x19B0, camera min X = x & 0xFF80) |

Selection from the last safe position (FF8370/FF8372, recorded while grounded,
combat.md 3.5): i = (x >> 8) * 8 + r, r = ((y >> 8) - 16, 0 if negative) & 7,
c = byte table **0x325C**[i] (27 x 8 bytes, x 0..6911); the index only ever
increases:

| x | 0-511 | 512-1023 | 1024-1535 | 1536-2047 | 2048-2559 | 2560-3071 | 3072-3839 | 3840-4607 | 4608-4863 | 4864-5375 | 5376-5887 | 5888-6399 | 6400-6911 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| c | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 (y < 4864) / 8 (y 4864..5119) / 9 (y >= 5120) | 9 | 10 | 11 | 12 | 13 |

VERIFIED (ROM). The ride (x >= 6912) is beyond the table: the read runs into
the records at 0x3334 (byte 0x00, then 0x80, 0x10, ...). The player is not
grounded on terrain during the ride (state 10, 6.13), so whether a safe position
is ever recorded there: UNKNOWN; the native game should keep c = 13 for x >=
6912 (no restart point on the descent). SOURCE.

Restarts after the mid-boss: FF838A (set when the dragon dies) makes the dragon
remove itself at load (0x2ACC0) and makes the camera skip the arena lock
(0x429F0); the max-X raise to 6528 is done by the stage-4 camera hook only
while FF838A is clear (0x42DDA), so how a restart past the arena gets camera
max X 6528: UNKNOWN (trace a death after the dragon). Sorsha removes herself at
load when **FF83A1** is set (0x283DA). SOURCE.

### 1.8 Difficulty rank
Stage start (0x38146): excess over FF8291 halved, plus **0x18** for stage index
3 (byte table 0x3819A[3]), minus the death-count term. Run A (stage 3 debug
start): 0x3142 at the end of stage 3 -> **0x4842** at the stage-4 start. Run B
(debug start 0x3000): 0x47EC at the cave mouth (7044), 0x53E0 at the dragon's
first hit, 0x69C6 at Sorsha's arrival, 0x76AE at the clear (~1 per update).
VERIFIED (traces A/B).

---

## 2. Map, layers, camera, collision, palettes

### 2.1 Layers, scroll rates and stage size
- **Foreground (scroll2)**: grid **3** (0x2DBF8[3] = 0x2E024), **mode 2**: 32
  block rows of 64 columns; index = (((~worldY) >> 8) & 31) * 64 + ((x >> 8) &
  63) (lookup 0x43722), 256x256-px blocks of 16x16-px cells at ROM 0x80000 +
  b*0x400. Used blocks (row r covers world y (31-r)*256 .. +255):

  | Row | World y | Columns: blocks | Part |
  |---|---|---|---|
  | 15 | 4096..4351 | 0-7: 144 145 146 147 148 149 149 149 | slope top (start) |
  | 14 | 4352..4607 | 3-8: 150 151 152 153 148 149; 10-16: 185 x7; 17: 149 | slope, cave roof |
  | 13 | 4608..4863 | 6-17: 150 154 155 156 157 158 159 160 161 162 163 149 | lower slope, cave upper level |
  | 12 | 4864..5119 | 9-17: 201 164 165 187 187 167 168 169 149 | cave mouth, cave floor |
  | 11 | 5120..5375 | 13 181, 14 166, 16-17 170 171, 18 148, 19-27 178 x9, 28 149, 29-31 182 183 184 | lower cave, cave exit, camp backdrop rows |
  | 10 | 5376..5631 | 17-28: 150 172 173 174 175 176 174 175 176 177 179 180 | camp ground (x 4352..7423) |
  | 12..18 | 3328..5119 | 29-41: 149 / 186 188 189 / 190 191 / 196 195 / 192 193 194 195 | descent start |
  | 19..31 | 0..3327 | diagonal of 149 149 + 190 191 / 192..195 / 196 195, columns 39..59 | the descent slope |
  | 31 | 0..255 | 57-63: 149 149 197 198 199 200 202 | the village at the bottom (end scene) |

  Width 64 x 256 = 16384 px, height 32 x 256 = 8192 px. VERIFIED (ROM grid
  dump; renders in 2.2).
- **Far layer (scroll3)**: grid 0x2EB24[3] = **0x2EF4C**, mode 0 (16 x 16 blocks
  of 256 px, 32x32 tiles at 0xC0000 + b*0x100); every row repeats **one block in
  all 16 columns** (the picture repeats every 256 px horizontally):
  row 15 (far y 0..255) block **64**, row 14 (256..511) **62**, row 13 (512..767)
  **59**, row 12 (768..1023) **63**. VERIFIED (ROM).
- **Scroll rates (every frame)**: scroll2 = camera (1:1); scroll3 = **camera / 8
  in x and in y** (far X = camX >> 3, far Y = camY >> 3): the per-stage shift
  table **0x42FC4** = 1, 1, 1, **3**, 1, 1 (0x42FE0: far = camera >> shift). Over
  run B's play frames (9,097 RAM dumps) FF8488 = FF8408 >> 3 and FF848C =
  FF840C >> 3 every time. Far Y ranges 512..670 in parts A-C (camera Y
  4096..5360) and 670 -> 0 during the descent; far X 0..~2000. scroll1
  (HUD/text) fixed. VERIFIED (disassembly; RAM).
- **No row, line or column scroll** anywhere in the stage: CPS-A video control
  0x800122 = **0x003E** (row-scroll bit 0 clear) in all 2,067 stage-4 dumps;
  layer order (0x800170) **0x12F8** in play, 0x12C0 at the start, the shop
  transition and the end-scene cut, 0x13B8 / 0x1380 in the shop. VERIFIED
  (`S/regs.py vid 836 21500`).
- **Special scroll effects**: (a) **falling snow** in three depth layers
  (sprites, 2.7) over part A; (b) the **mid-boss arena lock**: camera X confined
  to [3392, 3584] (2.3); (c) the **boss arena glide** to 6528 and lock (6.10);
  (d) the **descent**: the camera follows the sled diagonally down 5,400 px of
  world y, so the far layer moves vertically from y 670 to 0 at 1/8 rate,
  showing all four far blocks (63/59 near the top, 62, then 64 at the bottom).
  No palette cycles (2.6).
- **Native (parallax rule, AGENTS.md 2026-11-02)**: plane A = foreground at the
  camera; plane B = the far layer at (camX / 8, camY / 8) in **both** axes. The
  far picture is 256 px wide (one block per row) and 1024 px tall (four
  blocks), so plane B can hold a 256-px-wide strip of each far block stacked
  vertically and wrap horizontally; the HUD must not pin plane B's vertical
  scroll (vertical far motion is required, especially on the descent).
- **Areas** (renders, run B keys):

  | Camera X | Area | Key frames |
  |---|---|---|
  | 0..~550 | snowfield at the top (camera Y 4096), falling snow, distant snowy mountains (far block 59), soldiers from the left | `vid/v001000`, `v002000` |
  | ~550..1550 | slope begins to fall away to the right; bomb trooper from the right; dead frosted trees | `vid/v003000`, `v004000` |
  | ~1400..2400 | steeper slopes, huge rolling snowballs, hawks | `vid/v005000`, `v006000` |
  | ~2300..2660 | cave mouth in a rock cliff (camera min Y 4592 from 7044); shop door (2656, 4888); chest (2832, 4782) | `vid/v007000`, `v009600`, `v009700` |
  | ~2660..3392 | ice cave: black interior with stalagmites, rock arches, bats | `vid/v010200`, `v010500`, `v011300` |
  | 3392..3584 (locked) | dragon arena under a rock arch | `vid/v011400`..`v012900` |
  | ~3584..4600 | lower cave: chests, hidden object, spearman, archer, second shop door (3920, 4888) | `vid/v013000`, `v013600` |
  | ~4600..4740 | cave exit: snow wall, then the open mountainside (camera min Y 5360 from 14616) | `vid/v013900`, `v014200`, `v014500` |
  | ~4600..6528 | Sorsha's camp: grey tents on snow under a cloudy sky (far blocks 63/59) | `vid/v014700`, `v016000`, `v017000` |
  | 6528 (locked) | Sorsha's arena: a large round hide tent with shields | `vid/v018100`, `v018300`, `v018700` |
  | 6528..15970 | the shield ride down the mountain | `vid/v019550`, `v019700`, `v020200`, `v020900` |
  | 15776..16384 | bottom: snowball crash and roll; village with thatched huts and fences | `vid/v021000`, `v021320`, `v021480` |

### 2.2 Terrain (scroll2 grid 3, collision ids)
Collision id = attribute bits 10-15 of the 16x16 cell; descriptors 8 bytes at
0x4712 + id*8 (player.md 6). Ids present in grid 3 (cells): 2 (42), 4 (2), 5
(374), 6 (8), 8 (41), 9 (64), 10 (46), 12 (7), 13 (33), 14 (7), **20** (1), 21
(36), 22 (37), **23** (250), 24-27 (25 each), 28-31 (23 each), **37** (46), 44
(66), 45 (66), 46 (1466). VERIFIED (ROM scan, `S/` script in the transcript of
this task; reproducible with `stagemap.py` once corrected, correction 2).

- New against stages 1-3: **20** (one cell at (4368, 5248), descriptor `08 00
  .. 83`: no floor shape, byte 5 = 0x83; at the second music trigger in the
  lower cave: purpose UNKNOWN) and **23** (250 cells along the descent, x
  8448..15248, y 128..4880: descriptor `08 40 .. 03`, floor shape 0x40, byte 5
  0x03: the descent slope surface the shield follows). VERIFIED (ROM).
- The snow slopes use the stage-3 1:4 slopes **24-27 / 28-31** and the 1:2 slopes
  **44/45**; 46 is solid fill (floor continues upward). VERIFIED (ROM).
- **37** (`88 8f`: ledge material + floor, thin platform) occurs 46 times:
  Madmartigan can drop through these (madmartigan.md 2.7) and climb onto them
  (2.8). Where they are: in the cave levels (rock shelves). SOURCE (ROM scan;
  positions not tabulated: use the corrected `stagemap.py`).
- No hazard tiles (id 39) and no water (id 32). VERIFIED (ROM scan).

### 2.3 Camera
- Limits at stage start (0x2DCA / 0x2DBE / 0x2DD8 [3]): max X **0x0E00 =
  3584**, min Y **0x1000 = 4096**, max Y **0x1F00 = 7936**. VERIFIED (ROM; RAM
  FF846A = 3584, FF845C = 4096 at key 838).
- X follows the player at player X - 192 (player.md 7); moving right raises min
  X to camera X - 768. VERIFIED (trace min X = cam - 770 on the slopes).
- **Camera min Y raises** (one-way floors for the camera, so the view does not
  drop below the terrain): 4096 -> **4592** at camera X 2304 (key 7044, cave
  mouth), -> **5360** at camera X ~4609 (key 14616, cave exit), -> **0** at the
  ride start (key 19486). Camera Y is therefore 4096 on the top slope, 4592+
  in the cave and **5360** (fixed, the ground at y 5424 is below) in the camp.
  The stage-4 vertical camera hook 0x42D16 sets vertical sub-mode 10 at camera
  X 6912..7039 (ride start) and 12 at camera X 4608..4735 (cave exit, when the
  floor table value is above the camera). VERIFIED (trace) / SOURCE (which
  code writes 4592 / 5360: the floor-table path 0x42E66, table not decoded).
- **Mid-boss lock** (camera mode 0 -> 2 -> 14, 0x42942 / 0x429E8): when the
  player X reaches max X **3584** (key 11318, player 3586, camera 3391): FF83D0
  = 1 and (FF8362 set) player life state 14, which ends at once because FF8470
  is already set (life state 2 again two keys later: no arrival walk here); for
  stage index 3 with FF838A clear the camera goes to mode
  **14** with **FF8470 = 1** and **min X = 3584 - 192 = 3392**: the camera keeps
  following inside **[3392, 3584]** (no glide). VERIFIED (trace 11318..11520:
  min X 3392, camera 3394..3494..3460).
- **Stage-4 camera hook 0x42DDA** (per-stage table 0x42D74[3]): (1) FF838A clear
  and **FF83E7** set (the dragon has died): max X = **6528**, min X = camera X,
  mode 12, FF8470 = 0, **FF838A = 1** (key 12870); (2) FF838B clear and
  **FF83CF** set (Sorsha defeated): max X = **16000**, min X = camera X, mode
  12, FF838B = 1 (key 19358). VERIFIED (disassembly + trace).
- **Boss arrival**: player X >= 6528 (key 18042): FF83D0 = 1, player life state
  **14** (arrival walk), camera mode 2 glides **+2 px/update** to 6528 (6333 ->
  6528 by key 18140), then mode 4 with FF8470 (locked at 6528; Y 5360).
  VERIFIED.
- Ride and end: max X 16000 then **16384** (mode 6 at key 21300, the end scene);
  min Y 0. VERIFIED.

### 2.4 Pit
None in practice: the playfield is solid snow/rock below the player everywhere
(camera min Y keeps the view above the terrain). Falling below camera min Y would
still be a pit death (combat.md 3.2). SOURCE.

### 2.5 Actor retirement
The general "x < camera min X" rule (21C6) as stage 1; the bats retire when off
screen with x > 4096 (5.2). No stage-4 branch in 21C6. SOURCE.

### 2.6 Palettes
- Loaded at stage set-up (0x0978, stage index 3): objects 0-15 **0xE5080**,
  objects 16-31 **0xE5A80** (0xE5480 + 0x600), scroll2 (32 palettes) **0xE8920**
  (0xE7D20 + 0xC00), scroll1 text 0xE6EA0, scroll3 16 palettes **0xEB300**
  into scroll3 palettes **16-31** (0xA2E[3] = 1). SOURCE (disassembly; the
  dumps hold the live palettes).
- **No palette controllers**: table 0x410EC[3] = 0x411CE holds count -1, and no
  camera-started controller exists for index 3; the only palette jobs in the
  whole run are boss jobs (dragon on object palette **26**: fade-in 0xE6CE0, hit
  0xE6D60; Sorsha on palette **28**: hit 0xE6CA0). VERIFIED (ROM; RAM pool
  FFF042 scanned 836..21496). No sky change, no cycles: the native stage uses
  fixed palette lines.
- Story page: no controller (7.2).

### 2.7 Falling snow (event slots 0-23: class-12 events kind 23 -> class 12 kind 23, 0x3565E)
- 24 events (+0E = 12, kind 23), variants 0, 1, 2 repeating, each with an x
  offset (+12) of -8, 0, 16, 40, 64, 88, 104, 120, 136, 152, 168, 184, 200,
  216, 232, 248, 280, 288, 312, 320, 352, 368, 376, 392 (across the 384-px
  screen). Active while **camera X + 384 <= 2704** (endX), i.e. camera X <=
  2320 (part A). Event 0x3997C: every 64 updates re-check; a timer counts down;
  at 0 spawn a flake at **(camera X + offset, camera Y + 240)** (just above the
  top edge) and reload the timer from 0x399F8[variant * 8 + rand & 7]: variant
  0: 32, 40, 48, 40, 48, 32, 48, 40; variant 1: 80, 88, 96, 88, 88, 80, 96,
  96; variant 2: 160, 176, 168, 160, 176, 168, 176, 160. Out of range:
  re-check every 16 updates, delay 0x39A3E[rand & 15] = 1, 60, 120, 180
  (repeating). SOURCE (disassembly) / VERIFIED (run B: 1,438 flakes born
  922..7094, none later).
- **Flake** (class 12 kind 23): falls straight down at **2 px/update**
  (variant 0), **1** (variant 1), **0.5** (variant 2) (0x356BC); lane +0E =
  variant * 2; pose by variant and rand & 3 (root table 0x356A4: 4 one-piece
  poses per variant, palette **27**, attribute 0x8000); removed below camera Y
  - 16. A three-depth snowfall: native as sprites (or one sprite layer per
  depth). SOURCE / VERIFIED (counts per variant 743/431/264).

---

## 3. Placed objects and spawn events

### 3.1 Stage object table (0x4034A[3] = 0x4080E, 56 entries)
Entry bytes class, variant, kind, direction, x, y (loader 0x40374; the class-18
records skip the distance filter, so both bosses exist from the stage start).
VERIFIED (ROM; all present at key 840 in run B).

| # | class / kind / variant | dir | x, y | What it is (section) |
|---|---|---|---|---|
| 0 | 16 / 9 / 2 | 0 | (0, 0) -> random place | hidden object (8.3) |
| 1 | 16 / 8 / 0 | 0 | 2656, 4912 | music trigger: cave music 9 (8.4) |
| 2 | 16 / 8 / 1 | 0 | 4352, 5264 | music trigger: stage music 7 (8.4) |
| 3-6 | 4 / 15 / 2 | 0 | (1888,4448) (2144,4448) (2336,4864) (2400,4864) | swooping hawks (5.4) |
| 7-11 | 16 / 4 / 0 | 0..4 | (2832,4782) (4160,4656) (3744,5104) (4208,5104) (3776,5104) | chests 0-4 (8.1) |
| 12 | 12 / 8 / 0 | 9 | 2656, 4888 | shop door at the cave mouth (8.2) |
| 13 | 12 / 8 / 16 | 10 | 3920, 4888 | shop door after the dragon (8.2) |
| 14 | **18 / 3 / 0** | 1 | 6784, 5440 | **Sorsha** (6.8) |
| 15 | **18 / 5 / 0** | 0 | 3840, 4816 | **dragon mid-boss** (6.1) |
| 16 | 6 / 4 / 0 | 0 | 7232, 5424 | the shield sled (6.13) |
| 17-26 | 4 / 30 / 0 | 0 | (1808,4640) (1936,4704) (1968,4736) (2128,4768) (2192,4784) (2256,4800) (2384,4864) (4464,5312) (4528,5360) (4656,5408) | rolling snowballs (5.1) |
| 27-45 | 4 / 31 / 0 | 0 | (2880,4960) (2912,4928) (2944,4896) (2976,4864) (3040,4800) (3072,4736) (3104,4720) (3168,4720) (4144,4744) (4160,4752) (4176,4748) (4032,4908) (4064,4902) (4096,4904) (4128,4982) (4160,4980) (4192,4988) (4064,5068) (4096,5066) | cave bats (5.2) |
| 46 | 4 / 3 / 2 | 1 | 2368, 4832 | archer (4.2) |
| 47 | 4 / 14 / 0 | 1 | 4192, 5128 | spearman (4.3) |
| 48 | 4 / 3 / 2 | 1 | 4192, 5256 | archer |
| 49, 50 | 4 / 14 / 0 | 1 | (5952,5424) (6208,5424) | spearmen in the camp |
| 51-53 | 4 / 3 / 2 | 1 | (4864,5424) (5072,5424) (5216,5424) | archers in the camp |
| 54 | 16 / 7 / 0 | 0 | 14936, 480 | ride crash marker (6.13) |
| 55 | 12 / 27 / 0 | 0 | 15808, 144 | end-scene actor (6.14) |

### 3.2 Stage event table (0x9AD8[3] = 0x9BF6, 31 events)
Fields: +0E class, kind, variant, +0F, par (+12), endX (+06), y (+08), startX
(+0A), spawnY (+0C). VERIFIED (ROM; every event present in run B).

| Ev | +0E / kind / var | startX | endX | y / spawnY | Meaning (section) |
|---|---|---|---|---|---|
| 0-23 | 12 / 23 / 0,1,2 | 0 | 2704 | 0 | falling snow, par = x offset -8..392 (2.7) |
| 24 | 4 / 32 / 0 | 1536 (+0A) | 544 (+06) | 0 | bomb trooper from the right, camera X 544..1536 (5.3) |
| 25 | 4 / 0 / **4** | 0 | 1280 | 0 | soldiers from the left on the top slope (4.1) |
| 26 | 4 / 0 / 0 | 4608 | 5376 | 5424 / 5424 | edge soldiers in the camp (4.1) |
| 27 | 4 / 0 / 0 | 6400 | 5888 | 5424 / 5424 | edge soldiers, **never spawns** (starts at 6400 but retires at >= 5888 in the same update) |
| 28-30 | 4 / 25 / 0 | - | 5512 / 5752 / 6280 (x) | 5424 | tent spawners (5.5) |

Event 24's fields are used as: wait while camera X < **544**, retire when
camera X > **1536** (0x39328 reads the constants 544 and 1536 directly).
SOURCE (code) / VERIFIED (one trooper at key 2668, event retired 5376).

### 3.3 Roster

| Family | Class / kind / var | Count | Where | Spawned by | Energy | Score | Drop |
|---|---|---|---|---|---|---|---|
| Soldier (edge entrant) | 4 / 0 / 0 | ~12 on the slope, ~10 in the camp | x 0..1280; camp 4608..5376 | events 25, 26 | 1 | 50 | 1 gold |
| Archer | 4 / 3 / 2 | 5 | cave, camp | table | 4 | as stage 1 | as stage 1 |
| Arrow | 10 / 0 | per shot | - | archers | - | - | - |
| Spearman + returning spear | 4 / 14 / 0; 10 / 7 | 3 | lower cave, camp | table | 4 | 200 | 100 gold |
| Swooping hawk | 4 / 15 / 2 | 4 | slopes | table | 2 | UNKNOWN | UNKNOWN |
| Rolling snowball | 4 / 30 / 0 -> 1-3 | 10 | slopes, cave exit | table | 1 | 100 | none |
| Bat | 4 / 31 / 0 | 19 | cave | table | 1 | 100 | set 1 (10 gold) |
| Bomb trooper (stage 4) | 4 / 32 / 0 | 1 at a time | camera X 544..1536 | event 24 | 10 | 500 | set 0x22A14 |
| Rolling bomb + shards | 10 / 10 / 8 | per throw | - | trooper | - | - | - |
| Tent spear thrower | 4 / 25 / 0 | up to 6 per tent (cap by rank) | camp | events 28-30 | 6 | 500 | set 0x1F5C4 |
| Thrown spear | 10 / 14 / 0, 2 | per throw | - | tent spear throwers | - | - | - |
| **Dragon** (body, neck, head) | 18 / 5 / 0, 2, 4 | 1 | arena x 3392..3968 | table | 80 | 10,000 | 15 x 100 gold |
| Dragon fire | 10 / 19 / 0 (+ 7 x var 2) | per breath | - | head | - | - | - |
| **Sorsha** | 18 / 3 / 0 | 1 | arena 6528 | table | 96 | 5,000 | none |
| Sword-slash box | 10 / 18 | per slash | - | Sorsha | - | - | - |
| Parry spark | 12 / 28 | per parry | - | Sorsha | - | - | - |
| Shield sled | 6 / 4 | 1 | 7232.. | table | - | - | - |
| Ride gold | 14 / 1 / 6 (200), 7 (500) | 86 | descent | shield (0x2D550) | - | - | gold |
| Chests | 16 / 4 | 5 | cave | table | 6 | - | 8.1 |
| Hidden object | 16 / 9 / 2 | 1 | one of 3 places | table | 20 | - | companion |
| Music triggers | 16 / 8 / 0, 1 | 2 | 2656, 4352 | table | - | - | - |
| Intro horse and riders | 12 / 7; 12 / 13, 14 var 2 | 1 each | intro | 0xA14A | - | - | - |
| Snowflakes | 12 / 23 / 0-2 | ~1,400 | part A | events 0-23 | - | - | - |

Projectile census over run B (births.txt): class 10 kind 14 (thrown spears)
39, kind 19 (fire) 24, kind 18 (Sorsha slashes) 5, kind 0 (arrows) 7, kind 7
(spear) 1. VERIFIED.

---

## 4. Families shared with stages 1-3: stage-4 differences

### 4.1 Soldiers (class 4 kind 0 variant 0) and their events
- As stage1_enemies.md 5.1 (energy 1, score 50, palette 16), with the stage-4
  difference that **"change level" never runs** (correction 4): soldiers do not
  jump up or drop down to the player's height. VERIFIED (disassembly).
- **Event 25** (kind 0 **variant 4**, routine 0x385CE; stage 4 only): active
  while camera X < **1280**; when FF0F40 (edge soldiers alive) < cap
  0x3862A[rank >> 4] = 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4: after a
  delay 0x3863A[rand & 15] = 110, 158, 222, 94, 158, 94, 110, 158, 94, 158, 110,
  94, 110, 158, 222, 110 updates, spawn a variant-0 soldier facing 0 at
  **(camera X - 16, camera Y + 80)**, FF0F40 += 1. The soldier's own wake-up then
  places it at camera X - 32 facing right: **soldiers run in from behind
  (left)**, downhill after the player. Retires at camera X >= 1280 or FF83D0.
  VERIFIED (run B: 12 soldiers x 34..869, keys 1080..3884, entering at the left
  edge) / SOURCE (tables).
- **Event 26** (variant 0, the stage-1 edge event 0x3834E): camera X 4608..5376,
  spawn y 5424, mostly from the right (0x384D4: 14/16). VERIFIED (run B: 10
  soldiers 14810..15788). Event 27 never spawns (3.2).

### 4.2 Archer (class 4 kind 3 variant 2)
Placed at (2368,4832), (4192,5256) and three in the camp (4864..5216, 5424),
all facing left. As stage1_enemies.md 5.9 (arrows class 10 kind 0, sound
**0x34**: 7 in run S). No stage-index-3 branch found. SOURCE / VERIFIED
(arrows 15032..15592).

### 4.3 Spearman (class 4 kind 14 variant 0)
(4192,5128) in the lower cave and (5952,5424), (6208,5424) in the camp, facing
left. As stage3.md 4.1 (returning spear class 10 kind 7, sound 0x8B: one throw
in run S). SOURCE.

### 4.4 Shared effects
Bursts class 12 kind 0 variants 4/5, gold pickups class 14 kind 0
(stage1_enemies.md 6). VERIFIED (births).

---

## 5. New families

Pose tables for the packer: `S/roots.txt` (from `../stage3/roots2.py` with the
right geometry base: class 4 0x3AC4A/0x3AE62, class 10 0x3B0D2, class 18
0x3B07A, class 6 0x3B1AA). Boxes below are (touch) / (shot) where they differ.

### 5.1 Rolling snowball (class 4 kind 30, 0x21F74)
A snowball that wakes when it comes on screen, rolls left down the slope
growing in three steps, then bounces along (`vid/v005000`: the large one).

| Item | Value | Tag |
|---|---|---|
| Energy | **1** (any hit) | VERIFIED (trace hp 1) |
| Score | 0x1802 = index 2 = **100**; no drop | SOURCE |
| Palette | **27** | VERIFIED (walk) |
| Init | probe +13 = 8, facing 0, variant (+21) = **1**, speed +2C = 0, timer 20, velocity table 0x21FE0, root 0x22178; waits until on screen (+1) | VERIFIED (all 10 at key 840 with variant 1) |
| Retire | off screen and the player is right of it (playerX >= x) -> removed | SOURCE |
| Phase 0 (roll and grow) | each update: x -= speed, speed += 16 up to **1024 (4 px/update)**; velocity (0, -3 px) and floor follow (3B0A); every **20** updates the next size: root 0x22170[variant] (v1 0x22194 2x2 block box (16,32,8,16); v2 0x221B0 3x2 (18,36,16,32); v3 0x221CC 4x3 (20,40,18,36)), variant += 1; after variant 3: phase 2 | VERIFIED (trace: var 1 -> 2 -> 3 at 20-update steps, x falling ~2 px/update) |
| Phase 2 (bounce) | vertical speed 768 (or 256, 50/50), velocity table 0x21FE4 (-4 px/update x), speed -= 48/update to -1024; on landing (+24): 16 updates on velocity 0x21FE8 (-4, -3), phase 4 -> new bounce | SOURCE |
| Death | sound **0x32**, root 0x2222C (snow burst: 8, 8, hold; palette 27), 24 updates, score, removed | SOURCE |
| Time stop | phases skipped while FF83D2 | SOURCE |

### 5.2 Bat (class 4 kind 31, 0x22292)
Purple bats hanging from the cave roof (`vid/v010200`, `v010500`).

| Item | Value | Tag |
|---|---|---|
| Energy | **1** (+2A/+2B) | VERIFIED |
| Score / drop | 0x1802 = **100** / set **1** (one 10-gold pickup) | SOURCE |
| Palette | **22** | VERIFIED (walk; `objpal.py vid/v010500.cps`) |
| Boxes | hanging (5,10,4,8)/(7,14,7,14) first node, then (8,16,6,12)/(12,24,8,16) | VERIFIED (walk) |
| Motion | 16-direction tables: 0xED364 (2 px), 0xED3E4 (2.5 px), 0xED2E4 (1 px) | VERIFIED (ROM) |
| Wake | hanging (root 0x225E4, 6 nodes x 7 ticks): when `|playerX - x| <= 96` and `|y - playerY| <= 112`: timer 0x2232A[rand & 15] = 32, 64, 96, -, 32, 64, 96, 64, 32, 64, 96, 64, 64, 64, 96, - ("-" = wait 255 then re-check) -> after it: 42 updates, then active | SOURCE |
| Active phase 0 | 48 updates flapping (root 0x22630, 4 nodes x 3 ticks) dropping toward the player (facing 7/8/9 by the angle) | SOURCE |
| Phase 2 (chase) | every 24 updates steer toward the player with a random offset (0x2245C: -1, -2, 1, 0, 2, 0, -1, 0, 2, 3, 0, -2, 0, -3, -1, 0 steps; "-1" entries switch to hovering at 1 px), 2 px/update; when level with the player (`|dy| <= 32`, `|dx| <= 16`): dash horizontally (2 or 2.5 px), then phase 6 | SOURCE |
| Phase 4 (hover) | 1 px/update, every 8 updates up or down at random; back to chasing after its timer | SOURCE |
| Phase 6 / 8 (climb) | rise to playerY + 0x223D2[rand & 6] = 80, 96, 64 or 56 px accelerating +48/update (max 8 px), then decelerate (-48) and hover (64 updates) | SOURCE |
| Death | sound **0x33**, score 100, drop set 1, burst class 12 kind 0 variant 4, removed | SOURCE |
| Retire | off screen and x > 4096 -> removed | SOURCE / VERIFIED (bats x > 4096 removed 13064..13336; earlier bats lived to 9746..11158) |

### 5.3 Stage-4 bomb trooper (class 4 kind 32, 0x22664) and event 24
The kind-23 bomb trooper's art and bombs (stage3.md 5.1/5.2) with a stage-4
routine: one trooper at a time drops in from above the right edge.

| Item | Value | Tag |
|---|---|---|
| Event 24 (0x39328) | wait until camera X >= 544; while camera X <= 1536: every 60 updates, if FF0F62 is clear: spawn kind 32 at **(camera X + 416, camera Y + 288)** (right of the screen, 64 px above its top), FF0F62 = 1; camera X > 1536 -> retire | VERIFIED (spawn key 2668 at (962, 4405), camera (548, 4117)) |
| Energy / palette | **10** / override **5**; motion 0xED078 (2 px/update) | VERIFIED (hp 10) |
| Fall in | y -= 3/update until on a floor (+24) | SOURCE |
| Approach | when `|playerX - x| <= 160`: a throw count +2E = 0x2274A[rand & 7] = 4, 2, 4, 3, 2, 3, 2, 2; ground-throw pose (kind 23 table 0x1DC72) | SOURCE |
| Throw | at timer 16: **class 10 kind 10 variant 8** (the rolling bomb with shards, stage3.md 5.2) at (x -+ 16, y + 8); between throws walks (0x1DD4A) facing the player, turning at slope edges; x clamped <= **2304** | SOURCE |
| Hit / death | as kind 23 (15-update flash 14/5, sound 0x32); death sound 0x33, drop 0x22A14[rand & 7] = 2, 1, 2, 3, 2, 4, 2, 2, score **500** (0x1806), burst; FF0F62 cleared on removal | SOURCE |
| Poses | kind 23 tables 0x1DD4A (run), 0x1DC72 (ground throw), 0x1DB9A (stand); palette 5 | SOURCE |

### 5.4 Swooping hawk (class 4 kind 15 variant 2, 0x18C02)
Placed at (1888,4448), (2144,4448), (2336,4864), (2400,4864).

| Item | Value | Tag |
|---|---|---|
| Wake | every 16 updates: when x - camera X < 192: energy **2**, motion 0xED364; jumps to **camera + (96, 256)** (above the top edge), facing 10, intangible (+0 = 2), root 0x1994A, and glides down-left at 2 px/update until x <= camera X; waits **90** updates off screen | SOURCE |
| Swoop | re-enters at **camera + (-16, 152)** (left edge) and flies a fixed path 0x18D62 of (direction, active, lane, updates) steps: (5,1,0,24), (4,1,0,4), (3,1,0,24), (2,1,2,3), (1,1,2,3), (15,2,2,3), (14,2,2,3), (13,2,2,3), (12,2,..) ...: a dive and climb across the screen (motion 0xED464); then leaves; every 16 updates off-camera removal | SOURCE |
| Palette / boxes | **31**; flight (9,18,8,12)-(9,18,8,16), hit poses (18,36,20,32) | VERIFIED (walk) |
| Roots | 0x196B4, 0x1971E (direct), 0x19766[facing] (flight, 0x19786 / 0x1979E), 0x1990A, 0x19922, 0x1993E, 0x1994A | VERIFIED (walk) |
| Score / drop / death | not decoded (shares the kind-15 death path, stage2.md 5.11) | UNKNOWN |

Run B: the four hawks died between keys 5540 and 6480 (two drops of gold
variants 5/6 at 5540). VERIFIED.

### 5.5 Tent spear thrower (class 4 kind 25, 0x1F29E) and the tent events 28-30
Green-armoured soldiers with long spears who step out of the camp tents
(`vid/v016000`, `v017000`).

**Tent event** (class-4 event kind 25, 0x393B6; x 5512, 5752, 6280, y 5424):
every 16 updates, when x - camera X < 480: arm with **6** spawns; then: delay
(first 0x39440[rand & 15] = 48, 16, 32, 0, 32, 16, 0, 32, 16, 0, 32, 16, 0, 16,
32, 0; later 0x39450 = 160, 96, 96, 128, 64, 160, 64, 128, 128, 64, 160, 64,
96, 128, 96, 160 updates); at 0: only if the player is **not** within 48 px of
the tent and the tent is within camera [-96, +480]: if FF0F5B (alive) < cap
0x39460[rank >> 4] = 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 8, 8: spawn kind
25 at the tent, FF0F5B += 1, one spawn used; after 6, retire. FF83D0 retires
it. VERIFIED (run B: spawns 15118, 15184, 16776; events retired at 18046) /
SOURCE (tables).

| Item | Value | Tag |
|---|---|---|
| Energy / palette | **6** / **30**; motion 0xED070 (1.5 px/update) | VERIFIED (walk) / SOURCE |
| Emerge | phase 6: lane 2, intangible, facing the player, x += 16 (or -8), 32 updates root 0x1F7C6 (stepping out), then normal | SOURCE |
| Walk | approaches a target offset (+3C = 0x1F306[rand & 30] = -32, 0, -16, 20, 32, -8, -24, 16, 24, 16, 10, 0, -16, 32, 0, -32) from the player; re-evaluates every 32 updates | SOURCE |
| Attack choice | every 40 updates by distance (< 64, < 88, < 120, >= 120) from 0x1F710: 0 none, **2** / **4** spear throw (class 10 kind 14 variant 0 / 2), **6** lunge (3 px/update, 3 updates, root 0x1F7B6) | SOURCE |
| Throw | 28 updates wind-up (root 0x1F7BA), then the spear, 14 updates | SOURCE |
| Hit / death | 16-update flash, sound 0x32; death sound **0x33**, drop 0x1F5C4[rand & 7] = 2, 1, 2, 3, 2, 4, 2, 2, score **500** (0x1806); FF0F5B -= 1; FF83D0 -> no reward | SOURCE |
| Boxes | standing (8,32,32,56); throw (4,40,32,40)/(24,32,32,48) | VERIFIED (walk) |
| Roots | 0x1F7AE[f] stand (0x1F7CA / 0x1F7F4), 0x1F7B6[f] lunge (0x1F92A / 0x1F9B0, 4 x 5 loop), 0x1F7BA[f] throw high (0x1FA36 / 0x1FAA2: 8, 20, hold), 0x1F7BE[f] throw low (0x1FB0E / 0x1FB86), 0x1F7C6[f] emerge (0x1FC4E / 0x1FC9A, 4 x 8) | VERIFIED (walk) |

**Thrown spear (class 10 kind 14, 0x25740)**: from (x +- 16, y); x speed
**512** (variant 0, 2 px/update) or **384** (variant 2, 1.5 px/update) by
facing, up speed **960** decreasing 32/update (apex after 30 updates), falls
until the floor; then stuck 38 updates (harmless, +0 = 2) and removed; FF83D0
-> removed. Root 0x25898[f] (0x259A8 / 0x259FC, palette 30, box (8,16,8,16)).
SOURCE / VERIFIED (39 spears 15222..18042).

The dragon and Sorsha are in section 6; the shield, the ride gold and the
end-scene objects in 6.13 / 6.14.

---

## 6. Mid-boss (the dragon) and boss (Sorsha)

### 6.1 Dragon record and parts (class 18 kind 5, 0x2AC80)
- Table entry 15: class 18 kind 5 variant 0 at **(3840, 4816)**, created at
  stage load. Its first update: if **FF838A** is set (beaten before) it is
  removed; else it allocates two more class-18 records and links them:
  variant **2 (neck)** (+26 = body) and variant **4 (head)** (+26 = body, +28 =
  neck), both +0 = 2 (never collidable). Then body init: **energy 80**
  (FF83CC/FF83CD = 80 from key 922, bar hidden until the fight), bob as the
  stage-3 boss (0x28050: +2C 64, quarters of 32 updates), attack timer +37 = 60,
  root **0x2B4E0** (4 nodes x 10 ticks, palette **26**, 41/39/24/25 pieces,
  box **(24,48,42,58)**). VERIFIED (trace: three c18 records at key 840, cc = 80
  at 922; walk).
- Wake: when on screen and `|x - playerX| <= 192` -> state 2. VERIFIED (11330).

### 6.2 Arena
Camera lock (2.3): camera X in [3392, 3584], camera Y 4592, FF83D0 and FF8470
set when the player reaches x 3584 (key 11318). The dragon flies under the rock
arch (`vid/v011400`, `v012000`); waypoint y base **4663** (6.4). VERIFIED.

### 6.3 Intro (state 2 phase 0, 0x2AE1C)
| Sub | Behaviour | Tag |
|---|---|---|
| 0 | timer 128, **FF83E6 = 1** (encounter flag) | VERIFIED (11338) |
| 2 | 128 updates; palette job kind 1: colours **0xE6CE0** -> object palette 26, 6 steps x 6 updates (fade-in) | VERIFIED (pool job 11466) |
| 4 | 64 updates; then **+0 = 1** (collidable), phase 2 with the first waypoint | VERIFIED (+0 = 1 at ~11546) |

### 6.4 Flight (phases 2, 4, 6; 0x2AE04)
- Speed table by rank (0x2AF0A, rank byte & 0xE0 >> 3): **0xEF1E4 = 2 px**
  (ranks 0-5), 0xEF3E4 = 2.5 (6-11)... (rows 0..7: 2, 2, 2, 2.5, 2.5, 2.5, 3,
  3 px/update); 128 directions (+0F). VERIFIED (trace 60 px per 30 updates at
  rank ~0x4F).
- **Waypoints** (0x2B05C): side s = rand & 1: target x = player X + (-128 /
  +128), y = **4663 + 152**; later targets from 0x2B09E: by the time counter
  +66 (< 1280, < 2560, >= 2560 updates: three rows) and rand & 3, offsets
  (dx, dy): e.g. row 0 (-128, 104), (-72, 140), (-72, 140), (0, 152); mirrored
  for the other side. SOURCE.
- Phase 2: steer every 4 updates toward the target ((angle + 1) >> 1 of the
  256-step angle), x clamped to [camera min X + 32, camera X + 352] (+6B set
  when clamped); within the rank radius 0x2AEDA (8/16 px; 10/20 at high rank)
  -> hover 0x2AF9E[rand & 3] = 32, 64, 96, 128 updates, then phase 4 to the
  next waypoint. Phase 4: steer every 2 updates; within +-14 px -> phase 6.
  VERIFIED (trace: phase 2 -> 4 -> 6 cycle 11548..11888) / SOURCE (tables).
- **Phase 6 (breath)**: +6A = 1 for **64** updates (the head breathes), then
  phase 2 with a new waypoint. VERIFIED (11888..11948, 12158..12216).
- Neck (variant 2, 0x2B310): follows the body; every 8 updates while the body is
  not breathing it turns one step (0..6) toward angle ((angle to player + 8) >>
  4) - 5 clamped to 0..6; root table **0x2B60E**[step] (7 entries, palette 26).
  Head (variant 4, 0x2B3BE): position = body + 0x2B4C4[neck step]; root table
  **0x2B660**[step] (normal) or **0x2B66E**[step] (breathing), palette 26.
  SOURCE (walk: one node per step).

### 6.5 Attack: fire breath (class 10 kind 19, 0x267E6)
- When the body enters phase 6, the head (sub 0): sound **0x99**, one class 10
  kind 19 **variant 0** at head + 0x2B45A[step] with direction = neck step + 5
  (16-direction: 5..11, i.e. down-right through down-left). VERIFIED (3 x 0x99
  in run S; fire births 11884, ~12150, 12418).
- Variant 0 (emitter): spawns **7** variant-2 flames, one per update, at its
  position, index +30 = 0..6. Each flame: start delay 0x2686A[i] = 1, 4, 7, 10,
  14, 18, 22 updates; then flies at **6 px/update** (0xED624) in the breath
  direction until it hits a floor (3C48), then rolls along the floor at **5
  px/update** (0xED5A4) left or right (alternating by index) for **48** updates,
  then a 12-update end animation and removed. All stop when FF83E7 is set (the
  dragon died). Roots: i 0-3 0x269CC (2x2 block, 3 nodes x 4, palette **13**,
  box (12,24,11,20)), i 4-6 0x269F4 (box (14,28,13,24)). Contact hurts (1 HP).
  VERIFIED (24 kind-19 records: 3 variant 0 + 21 variant 2) / SOURCE (speeds).

### 6.6 Hit reaction and death
| Item | Value | Tag |
|---|---|---|
| Hit | energy changed: FF83CC = energy, bar redrawn, +0 = 2 for **40** updates, sound **0x32**, palette job 0xE6D60 -> palette 26 (13 steps x 3) | VERIFIED (80 -> 79 at 12472; job 12472) |
| Death (state 4, 0x2B136) | sound **0x9A**, probe 10, palette job (0xE6D60, 7 steps x 3); falls 1 px/update until the floor (3C48); then **96** updates: every 16 an explosion class 12 kind 0 variant 4 at body + 0x2B23C[i] (0,-8), (40,8), (-40,-32), (24,-32), (-24,8), (24,-40), (-40,0), (22,-12), sound **0x33** | VERIFIED (death 12660, 0x9A then 0x33 bursts) |
| Reward | score **0x180C twice = 10,000** (1F14 and 1F12), **FF83E6 = 0, FF83D0 = 0, FF83E7 = 1**, reward list **13** = **15 x 100-gold** pickups (class 14 kind 0 variant 5) at its position | VERIFIED (score 2450 -> 12450 at 12880; 15 pickups at 12870) |
| After | camera hook: max X 6528, FF838A = 1 (2.3); the parts follow the body's state 6 and are removed | VERIFIED (12870) |

Run B: death 12660 -> reward 12870 = 210 updates. VERIFIED.

### 6.7 What is not there (mid-boss)
No dialogue, no music change, no camera glide. The bar FF83CC shows 80 while
the dragon is alive; Sorsha's init overwrites it with 96 (6.8). VERIFIED.

### 6.8 Sorsha (class 18 kind 3, 0x283AC)
A red-haired swordswoman in blue armour (`vid/v018100`, `v018300`).

| Item | Value | Tag |
|---|---|---|
| Wake | every 2 updates: when x - camera X <= 384: (FF83A1 set -> removed) FF83CE += 1, FF838B += 1, **energy 96** (+2A/+2B, FF83CC), bar drawn at once (4AC6), motion 0xED080, probe 5, faces the player (0x18088), root 0x2925E | VERIFIED (hp 96 at 18078) |
| Palette | object palette **28** (hit job 0xE6CA0, 13 steps x 3) | VERIFIED |
| Box | **(8,16,32,48)** on every node (class-18 table 0x3B07A) | VERIFIED (walk) |
| Clamp | x kept within camera X + 16 .. camera X + 368 | SOURCE |
| Score / drop | **5000** (0x180C) / none | VERIFIED (14650 -> 19650; no pickups) |
| Damage to her | normal sword hits (madmartigan.md 5); 17-update invulnerability per hit (+36), sound 0x32 | VERIFIED (96 -> 80 over 16 hits, ~18 updates apart) |

**Decision (0x28A34)**: distance class d = `|dx| >> 5` (even 0..14, 14 when >=
256) selects a table 0x28AA2[d] of rank rows ((rank >> 3) & 30), then rand & 15
picks the phase (+3). VERIFIED (trace phases 4, 6, 8, 12 cycling) / SOURCE
(table contents not tabulated: extract 0x28AA2.. for the native data).

| Phase | Behaviour | Tag |
|---|---|---|
| 0 intro | 90 updates, then root 0x2925A 24 updates | VERIFIED (18078..18192) |
| 2 | root 0x2925E 16 updates, then decide | VERIFIED |
| 4 walk in | timer 0x28528 by distance, walk root 0x29262 toward the player (0x20B4) | VERIFIED (18224..18254) |
| 6 walk back | timer 0x285EC (10 or 21), velocity 0x2996A, walk away | VERIFIED (18496..18508) |
| 8 sword | combo type +5 from 0x28652 / 0x28662 by distance (0 / 2 / 4): type 0: wind-up 16 (0x2926E), **slash** sound **0x96** + slash box (class 10 kind 18) 16 updates (0x29272); type 2: wind-up 16 (0x2927E), **thrust** sound **0x97**, 8 updates (0x29282); type 4: high thrust (0x29286) sound 0x97; the box (0x29000 test) hits the player on contact; after a combo: 7/16 chance (0x287B2) to chain another, else decide | VERIFIED (types 0/2/4 in trace; 0x96 x5, 0x97 x11 in run S) |
| 10 jump attack | crouch 20 (0x29266), leap up speed **1152**, gravity 40 (cap -1920, then 7 px/update) moving toward the player; near the player in the air: aerial slash (0x29276, sound 0x96); on landing 24 updates (0x29266), decide | SOURCE |
| 12 guard | 16 updates root 0x2925E, decide | VERIFIED |
| 14 (state 4) backstep / parry | see below | VERIFIED |

**Slash box (class 10 kind 18, 0x266E2)**: lives **4** updates at Sorsha +
(+-32, +4) by facing, root 0x26756[f] (palette 6, box (1,25,16,24)); touch
damage 1 HP. SOURCE / VERIFIED (5 records in run B).

**Parry (0x28E74)**: when she is not attacking (+3A, +39 clear), the player is
within 128 px, she faces the player and the player faces her: each update she
scans the player's sword/shot pool (FFC2C6, 20 x 64) for a live record within
dx +-96, dy +-32; with chance 0x28F32 (6/16 for a plain swing, 9/16 for
variant != 0, i.e. charged) she goes to **state 4 sub 14**: +0 = 2 (immune),
backstep (x speed -+1024 decelerating 128/update, 5 + 25 updates, roots
0x2928A / 0x2928E), and with 50% a **sword clash**: sound **0x28** and a class 12
kind 28 spark at her +-16 (12 updates, root 0xF2EC); then decide. VERIFIED
(trace: state 4 sub 14 at 18646 and 18680 with kind-28 sparks; 0x28 x2) /
SOURCE (chances).

**Defeat (state 4, 0x290A8)**:
| Step | Behaviour | Tag |
|---|---|---|
| 0 | energy 0: bar 0, sound **0x98**, then sound 0x32; root **0x29292** (5 nodes: kneeling) 80 updates | VERIFIED (19040, 0x98) |
| 2 | 30 updates | VERIFIED |
| 4 | score **5000** | VERIFIED |
| 6 | **FF83E9 = 1** (clock stop), FF83EA = 1 (player freeze), FF8E58 = 1, message **0x0443** style 0x0306 (dialogue window, 3 frames/letter, 31 x sound 0x2F): **"I LOST." / "KAEL HAS TAKEN THE BABY."** (`&` = line break) | VERIFIED (ROM 0x72BF8; render `vid/v019200`; 0x2F x31) |
| 8 | when the text is done + 96 updates: FF83E9 = FF83EA = 0, **player life state 22** (script walk), window cleared (6770), FF83CE = FF838B = FF83D0 = 0, **FF83CF += 1**, **FF83A1 += 1** -> camera hook: max X 16000 (2.3) | VERIFIED (19358) |
| 10 | every 16 updates off-camera removal (she stays kneeling until the camera leaves) | SOURCE |

Cut 3 applies to her line (dialogue on its own rows).

**Sorsha roots** (palette 28, box (8,16,32,48); `S/roots.txt`): 0x2925A[f]
(0x2929A / 0x29386) intro, 0x2925E[f] (0x294AC / 0x2950A) stand, 0x29262[f]
(0x29300 / 0x293EC, 4-node walk loop), 0x29266[f] crouch, 0x2926A[f] jump,
0x2926E[f] (0x2952E / 0x29592) slash wind-up, 0x29272[f] slash, 0x29276[f]
air slash wind-up, 0x2927A[f] air slash, 0x2927E[f] thrust wind-up, 0x29282[f]
thrust, 0x29286[f] high thrust, 0x2928A[f] / 0x2928E[f] backstep, 0x29292[f]
(0x2983A / 0x298D2, 5 nodes) defeated. VERIFIED (walk; trace nodes 0x29702,
0x29722, 0x29748, 0x29794, 0x297D8, 0x29818).

### 6.9 Boss checkpoint
Checkpoint 13 (6576, 5432), 3:30, music 7, camera min X 6528 (1.7). Restarting
there puts the player at x >= 6528: the arrival (2.3) fires at once. SOURCE.

### 6.10 Arrival (keys, run B)
| Key | Event | Tag |
|---|---|---|
| 18042 | player X >= max X 6528 with FF8362: FF83D0 = 1 (camp soldiers explode: three 0x33), player life state **14** (arrival walk), camera mode 2 | VERIFIED |
| 18042..18140 | camera +2 px/update to 6528; Sorsha wakes at 18078 (camera 6407) | VERIFIED |
| 18140 | camera mode 4, FF8470 = 1 (locked) | VERIFIED |
| 18150.. | player state 14 continues (sub 4, 6) then control | VERIFIED |

No dialogue before the fight and no boss music. VERIFIED.

### 6.11 Ride start (after Sorsha)
Player life state **22** walks Madmartigan right (2 px/update) from x ~6894 to
the shield at **7232** (keys 19358..19528); camera mode 12 follows (max 16000,
min 6528). VERIFIED.

### 6.12 Ride: what the player sees
`vid/v019550` (on the summit), `v019700`, `v020200`, `v020900`: Madmartigan and
Willow sit on a round shield sliding down a steep snow slope, gold coins on the
way, "200" labels when collected; the far layer goes from cloudy sky to snowy
ranges to a forested mountain. VERIFIED (renders).

### 6.13 The shield sled (class 6 kind 4, 0x30ECE) and the ride gold
| Step | Behaviour | Tag |
|---|---|---|
| init | probe 1, accel +30 = **24**, +32 = 1, +34 = 8, max speed +38 = 1024, min +3A = 512, speed +2C = 0; root **0x3129E** (palette 25/24) | SOURCE |
| phase 0 | wait until player X >= 7232: FF0F06 = 0 (pickup counter), **player life state 10** (fixed hold, 0xD576: the player record keeps pose 0xD5A0 and is **not drawn**; the shield's poses include both riders), sound **0x08**, **0x2D550**: places the **86 ride pickups** (below), root 0x312BE 32 updates | VERIFIED (19528: state 10, 0x08; no palette-1 object in `vid/v019700`) |
| phase 2 | root table 0x312DE[slope] (4 entries: 0x312E6, 0x3132A, 0x31346, 0x31396; 2 nodes x 8, palette **24**) | VERIFIED (walk) |
| phase 4 ride | each update: speed += 24 toward max (+38) unless **Left** is held (FF806A bit 1: speed -16/update down to min +3A) or airborne (-8 to min); x += speed; y -= speed >> slope shift (+3C); terrain follow (3CB0) with gravity +24 to 1792 when off the surface; slope class s = +2E & 15 from the surface sets max 0x31136[s] = **1152, 1408, 1536, 1792** (4.5..7 px/update), min 0x3113E = 1024, 1280, 1408, 1408, shift 0x31146 = 0, 2, 1, 0 and the root; the player position = the shield position every update | SOURCE / VERIFIED (trace: ~5 px/update average, 7232 -> 14936 in ~1360 updates) |
| jump | on the surface and a new press of **button 2** (jump): sound **0x9F**, up speed 0x310EA[s] = 896, 1024, 1152, 1280, gravity 64/update (cap 1792) until it lands | VERIFIED (11 x 0x9F in run S) / SOURCE |
| spray | every 8 updates a class 12 kind 26 snow spray at (x, y - 16) linked to it (kind 26 spawns kind-31 particles) | VERIFIED (87 kind-26, ~590 kind-31 records 19568..20902) |
| end | when a class 16 **kind 7** marker (3.1 #54, x 14936) has x <= shield x: phase 6: sound **0x9E**, class 12 **kind 25** (the snowball, 6.14) at the shield; the shield flies on (x + 6/update, y speed 1280 -16/update to 512) and leaves the screen (2162); if no class-12 slot: FF83C3 = 2 at once | VERIFIED (crash 20890, 0x9E) |

**Ride gold** (0x2D586: count 86, entries kind<<8|variant, x, y): **81 x class
14 kind 1 variant 6 (200 gold)** and **5 x variant 7 (500 gold)** at (9072,4368),
(9600,3824), (9824,3616), (11520,2928), (11584,2880). Static pickups along the
slope from (7840, 5216) to (14368, 1056) (full list `S/descent_pickups.txt`).
Each pickup collected adds 1 to **FF0F06**; if the class-14 pool is full the
rest are queued in FF0F04 / FF0F00. VERIFIED (ROM; run B: 71 collected, FF0F06
0 -> 71 over 19696..20804).

**All 86 collected**: the snowball (6.14) spawns a class-14 **kind 10** at
**(15712, 64)** when FF0F06 = 86 (0x357FE): a hidden bonus at the bottom. What
kind 10 is (an item? a large treasure?): UNKNOWN (not reached).

### 6.14 Crash, roll and end scene
| Key (run B) | Event | Tag |
|---|---|---|
| 20890 | class 12 kind 25 (0x357BC) at the shield: FF0F3C = 0; the player is carried (player position = its position every update); 5 updates on velocity 0x35826 (+6 px x), then a fall (y speed -96/update) to the floor, then **rolls** (velocity 0x35946) 40 updates root 0xE5A8, then root 0x35A2A (4x3 block snowball, palette 27) rolling right until x >= **15776** | VERIFIED (render `vid/v021000`: Madmartigan inside a rolling snowball) |
| ~21080 | x >= 15776: sound **0xA0** (the snowball breaks), root 0x35A5E, 90 updates, then **FF0F3C = 1** | VERIFIED (FF0F3C = 1 at 21170) |
| 21170..21210 | class 12 kind 27 (table #55 at (15808, 144), woke when within camera + 416, root 0x35B90): on FF0F3C = 1 it moves 16 updates (0x359F8), root 0x35BA0, falls 24 updates, then **FF0F3C = 2** | VERIFIED (21210) |
| 21210..21242 | kind 25: root 0x35B70 32 updates, then **player life state 20** (exit scene) | VERIFIED (21242) |
| 21300 | cut (layer 0x12C0 one dump): player at **(16162, 73)**, camera (15970, 0), max X 16384 (mode 6) | VERIFIED |
| 21306 | the player's state 20 spawns class 12 **kind 29** at (16256, 80) (a horse, root 0x35C82, palette **29**), which spawns **kind 30** at (16256, 96) (its rider, root 0x35CB8, 4 nodes x 10 then hold, palette **20**) | VERIFIED (`vid/v021320`, `v021480`: Madmartigan facing a mounted figure in a snowy village) |
| 21306..21490 | kind 30: waits while FFF891 is set, 40 updates, 90 updates, then **FF83C3 = 2** (results) | VERIFIED (FF83C3 = 2 at 21490) |

So natural play: Sorsha's defeat -> line -> walk -> ride (~1360 updates at the
average speed) -> crash -> roll -> end scene ~600 updates -> results. There is
no stage-exit timer (class 16 kind 1) and no FF83C3 = 1 step in stage 4: the
sequence sets FF83C3 = 2 directly. VERIFIED (flags_b: c3 0 -> 2 at 21490).

---

## 7. Stage clear, results and story (run C, natural, no input)

### 7.1 Clear and results
Run C injected FF83C3 = 2 at t 1400 (in play at the stage start, so TIME = 5:28).

| Key (run C) | Step | Event | Tag |
|---|---|---|---|
| 1404 | 4 | results text **"STAGE4 / CLEAR POINTS"** | VERIFIED (render `vidC/v001406`) |
| 1408..2720 | 8 | count: TIME s x **400** and LIFE 8*(HP+armour) x **400**, sound **0x30** each update (1,313 in run CS); run C: TIME **328 x 400 = 131,200**, LIFE **24 x 400 = 9,600** | VERIFIED (render `vidC/v002702`: TIME328x400, LIFE 24x400; factor 400 confirmed) |
| 2720..2908 | 10..12 | 120 updates + fade | VERIFIED |
| 2908 | 14 | sounds **0xF4, 0x08FB**; palette set 0xA7B0[3] = 1; story backdrop scroll2 **grid 7** at **(1536, 256)** (0xAC46[3]): a grey-bearded, scarred warrior with a round shield on his back, on black; page counter **FF0E85** = 0xA8AE[3] = **1** (two pages); no palette controller | VERIFIED (render `vidC/v002924`) |
| 2964..3208 | 18 | page 1 typed (style 0x031B, 3 frames/char, column 2, rows 24-27, 40 columns, wrap at the row end) | VERIFIED |
| 3268..3420 | 18 | page 2 | VERIFIED |
| ~3480..3534 | 18 end | fade, sound off, stage index -> 4 (stage word 4 at v3534) | VERIFIED |
| 3536.. | map | stage-5 map scene (marker V), then stage 5 (music word 10: 0x0A + 0x01FA, title 23 letters) | VERIFIED (render `vidC/v003650`; run CS) |

Story pages (table 0xA8EA + 3*4 = bytes 45 44 00 00, indexed by FF0E85 counting
down [1], [0]; message records 0x70000 + m * 168, ASCII, `&` = line break):
- page 1, message **0x44** (0x72CA0): "UNFORTUNATELY, THE KINGDOM OF GALLADOORN"
  "THAT I WAS PROTECTING WAS DEFEATED BY&KAEL." (the first line is exactly 40
  characters and wraps) — render `vidC/v003206`;
- page 2, message **0x45** (0x72D48): "YOU GUYS ARE OUR ONLY HOPE.&WE ARE
  COUNTING ON YOU." — render `vidC/v003416`.
VERIFIED (ROM strings + renders).

### 7.2 Notes for the native game
- Results: the shared results screen with stage number 4 and **factor 400**.
- Story: plane-B picture of scroll2 grid 7 at (1536, 256), no palette cycle, two
  pages in the text band (cut 3), 40 columns with wrap and `&` breaks.
- Then stage 5 (index 4): its own intro 0xA1B8 (not in this spec).

---

## 8. Shops, items, hidden objects

### 8.1 Chests (class 16 kind 4, energy 6)
Rewards 0x37EA6 + 0x84 (stage 3 offset) = **0x37F2A**: 4 sets x 5 chests x
(kind, variant); kind 5 = gold object of that variant (0 = 1, 5 = 100, 6 = 200
gold), kind 6 = heart; set = FF83A4. VERIFIED (ROM).

| Set | chest 0 (2832,4782) | 1 (4160,4656) | 2 (3744,5104) | 3 (4208,5104) | 4 (3776,5104) |
|---|---|---|---|---|---|
| 0 | gold v6 (200) | gold v0 (1) | heart | gold v5 (100) | gold v6 |
| 1 | gold v0 | gold v5 | heart | gold v6 | gold v6 |
| 2 | gold v0 | gold v6 | heart | gold v5 | gold v6 |
| 3 | gold v5 | gold v6 | heart | gold v0 | gold v6 |

Chest 0 is at the cave mouth, the others in the lower cave after the dragon.

### 8.2 Shop doors (class 12 kind 8)
(2656, 4888) variant 0 direction 9 (the cave mouth, same x as the cave-music
trigger) and (3920, 4888) variant 16 direction 10 (just right of the dragon
arena). Run B used the first one at keys 7238..9546 (play step 4; layer word
0x13B8; music 0x10FB, 0x1B + 0x02FA; on exit 0x10FB and the cave music 0x09:
the trigger fired as the player came out at x 2656). Contents: shop spec. The
clock keeps running in the shop (3:44 -> 3:09). VERIFIED (trace, sound log).

### 8.3 Hidden object (class 16 kind 9 variant 2)
As stage3.md 8.3 with variant 2: removed if **bit 2** of FF83A5 is set. Position
0x37542[3] = -28 -> table **0x37526** + 0x37532[rand & 15] (0: 6/16, 8: 5/16, 4:
5/16): **(3280, 4704)** 6/16, **(4016, 4832)** 5/16, **(3712, 5136)** 5/16 — in
the cave. Box (10,20,16,20), energy 20, visible only with item FF8355; at
energy 0: the companion (class 16 kind 0), bit 2 of FF83A5 set. VERIFIED (ROM;
run B placed it at (3712, 5136)).

### 8.4 Music triggers (class 16 kind 8, 0x36D2C)
Phase 0: if its x <= player X at load it is skipped (retired); else phase 2: when
player X >= its x: post sound **0x09** (variant 0) or **0x07** (variant 1) once
(the music engine follows with 0x01FA) and retire. (2656, 4912) var 0, (4352,
5264) var 1. VERIFIED (disassembly; sound log order).

---

## 9. Sounds in stage 4 (run S unless noted; `jsr 0x786A`)

| Word | Where |
|---|---|
| 0xF0, 0xF7, 0x12 / 0xF0, 0xF7 | world-map scene start / end (run AS) |
| 0x07 + 0x01FA | stage music (intro set-up; cave-exit trigger) |
| 0x09 + 0x01FA | cave music (trigger at x 2656, checkpoints 6-8) |
| 0x39 | intro hoofbeats, 6 x every 32 updates (run AS) |
| 0x2F | title (19), shop text, Sorsha's line (31) |
| 0x10FB / 0x1B + 0x02FA | shop: music fade / shop music |
| 0x32 / 0x33 | hit / death burst (81 / 56) |
| 0x34 | arrows (7) |
| 0x8B | spear throw (1) |
| 0x99 | dragon fire breath (3) |
| 0x9A | dragon death, then 0x33 bursts |
| 0x96 / 0x97 | Sorsha slash (5) / thrust (11) |
| 0x28 | Sorsha's parry clash (2) |
| 0x98 | Sorsha defeated |
| 0x08 | ride start |
| 0x9F | shield jump (11) |
| 0x9E | ride crash |
| 0xA0 | snowball breaks at the bottom |
| 0x2D | pickup (113, most on the ride) |
| 0x43 | clock warning under 0:16 (not heard: the clock stopped at 0:29) |
| 0x30 | results count tick (1,313, run CS) |
| 0xF4, 0x08FB | story page |
| 0x0A + 0x01FA, 0x2F x23 | stage-5 music and title (run CS) |

Player sounds also present (0x26 sword x530, 0x2A, 0x2B, 0x31, 0x3B, 0xF4
charge reset): madmartigan.md 6. One **0x10** (stage clear jingle) is posted
just before the results count (about key 21490); stage 4 has no FF83C3 = 1
step, so its owner here is presumably the results mode start: VERIFIED (sound
word log) / UNKNOWN (owner).

---

## 10. Dump files for the pose / plane / far-layer packers

All under `S/` = `/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage4/`.
`.cps` = lockstep VIDDUMP (0x80 bytes CPS-A/B registers + video RAM, live
palettes included); render with `S/render.sh DIR KEY...` or several at once
with `S/montage.py OUT.png DIR KEY...` (into `S/png/`; montages m1..m10, mc
already rendered). Run B dumps every 10 keys (840..21990), run A every 6 from
2900, run C every 6 from 1400. RAM dumps (`ram/rKEY.bin`, every 2 keys) hold
every actor record for frame-exact node pointers.

Scenes, planes and far-layer pictures:

| File | Shows |
|---|---|
| `vidA/v003200.cps` | world map for stage 4 (Madmartigan at marker IV; I-III cleared) |
| `vidA/v003410.cps`, `v003500.cps` | intro: horse with Willow and a soldier (palettes 16 / 0 / 28) |
| `vidA/v003584.cps`, `v003650.cps`, `v003680.cps` | Madmartigan walked in; title STAGE4 / SORSHA'S CAMP typing |
| `vid/v001000.cps`, `v002000.cps` | top snowfield; far block 59 at far y 512 (camera Y 4096) |
| `vid/v007000.cps`, `v009600.cps` | cave mouth; far y ~574 |
| `vid/v010200.cps`..`v011300.cps` | cave interior (far layer hidden behind scroll2) |
| `vid/v014500.cps`, `v014700.cps`, `v016000.cps` | cave exit and camp: far blocks 63 (clouds) and 59 at far y 670 |
| `vid/v019550.cps`, `v019700.cps` | ride top, far y ~640 |
| `vid/v020200.cps` | ride middle, far y ~380 (block 62) |
| `vid/v020900.cps`, `v021000.cps` | ride bottom, far y ~100..0 (block 64: forested mountain) |
| `vid/v021320.cps`, `v021480.cps` | end scene, village (scroll2 blocks 197-202), far y 0 |
| `vid/v007330.cps`..`v009540.cps` | shop interior (layer 0x13B8) |
| `vidC/v002702.cps` | results STAGE4 CLEAR POINTS, TIME328x400, LIFE 24x400 |
| `vidC/v002924.cps`, `v003206.cps`, `v003416.cps` | story backdrop and pages 1-2 |
| `vidC/v003650.cps` | stage-5 world map |

The far layer has no palette changes: any of the dumps above gives its palettes
(scroll3 palettes 16-31). All four far blocks are visible somewhere in
`vid/v019550..v021000` (descent) plus the camp dumps.

Families (pose packer, run B `vid/vKEY.cps` unless noted):

| Key | Shows (palette) |
|---|---|
| 1000..4000 | soldiers (16) from the left, snowflakes (27) |
| 2670..3000 | stage-4 bomb trooper (5) falling in / throwing; rolling bomb (5), shards (24) |
| 4950..5050, 5400..5450 | snowballs growing (27): v2 4950, v3 4970..5042 |
| 5000..6480 | swooping hawks (31) |
| 7230..7330 | shop door at the cave mouth |
| 9750..11150 | bats (22) hanging and flying (`v010500`: two bats) |
| 11400..11540 | dragon fading in (26) |
| 11600..12650 | dragon flying; fire breath at 11890..11950, 12160..12220, 12420..12480 (flames 13) |
| 12660..12870 | dragon death explosions, gold shower (15 x 100) |
| 13000..13600 | lower cave: chests, spearman (18), archer (17) |
| 15000..18040 | camp: edge soldiers (16), tent spear throwers (30), thrown spears (30), archers (17), spearmen (18) |
| 18100..19040 | Sorsha (28): walk, slash 18270..18290, thrust 18314..18330, parry backstep 18646, 18680 (spark), slash boxes 18422..19030 |
| 19040..19350 | Sorsha defeated pose, dialogue window (`v019200`) |
| 19550..20890 | the shield with both riders (24/25), snow spray, ride gold and "200" labels |
| 20900..21240 | snowball carrying Madmartigan (27), break-up |
| 21300..21490 | end scene: horse (29), rider (20), Madmartigan |

---

## 11. Open questions (UNKNOWN), where to look

Priority for the native stage:
1. **Natural boss fights**: the dragon was killed by KILLBOSS after one hit and
   Sorsha after 16 hits; record scripted-keys runs (no invincibility after the
   arena) to time a natural dragon fight (fire-breath frequency, waypoint table
   0x2B09E rows) and Sorsha's full decision tables (0x28AA2.., 0x28528,
   0x285EC, 0x28652 / 0x28662) with her jump attack (phase 10, not seen).
2. **Ride details**: does anything on the slope hurt or stop the sled (the
   class-12 kind 26/31 records are spray only?); is the player vulnerable in
   state 10; what does Left really do (FF806A bit 1 braking: confirm with a
   held-Left run); jump arc numbers; the class-14 **kind 10** bonus for all 86
   pickups (force FF0F06 = 86 with a POKE or a perfect run) and what it is.
3. **Restarts**: a death after the dragon (how camera max X 6528 is restored
   with FF838A set); a death on the camp; checkpoint index on the ride (table
   overrun, 1.7); FF83A0 clearing for a new game.
4. **Hawk variant 2**: score, drop and the full path table 0x18D62; death path.
5. **Camera min-Y floors** (4592 / 5360): the code writing FF845C in stage 4
   (floor table read at 0x42E66 when +67 is set) and the vertical sub-modes 10
   / 12 (0x42D16).
6. Collision id **20** at (4368, 5248) and the positions of the ledge cells
   (id 37) for Madmartigan's climb/drop (use `stagemap.py` with the mode-2 fix,
   correction 2).
7. Bats: the wake timer entries marked "-" (0xFF) and the variant-1 bat seen at
   key 13094 (+21 = 1 is the re-check flag, not a variant: confirm).
8. The end-scene figures (class 12 kind 29/30: a horseman; kind 27 at (15808,
   144)) — who they are visually is clear only from the renders; FFF891 (what
   kind 30 waits on).
9. Sound 0x10 owner at the results start; whether the shop exit always re-posts
   the trigger music (run B posted 0x09 twice).

---

## 12. Madmartigan: what stage 4 adds beyond madmartigan.md

1. **Life state 18 (timed scene, 0xD8CA)** is used by the intro (1.3): x = -32,
   125-update wait, walk 2 px/update to x 176 (walk nodes 0xE1A4..0xE1E0), stand
   (0xE25C) until play; the native intro needs this scripted walk. VERIFIED.
2. **Life state 14 (arrival walk)** at Sorsha's arena while the camera glides
   (6.10); at the dragon arena it is set for one update only (FF8470 is
   already set by camera mode 14). VERIFIED.
3. **Life state 22 (script walk)** after Sorsha's line: walks right to the
   shield at 7232 without input (6.11). VERIFIED.
4. **Life state 10 (fixed hold, pose 0xD5A0)** for the whole ride: the player is
   not drawn, the shield (class 6 kind 4) moves him (player x/y = shield x/y
   every update) and reads the pad itself (Left = brake, button 2 = jump).
   **New input semantics** for the ride; HUD and gold work as usual. VERIFIED
   (state, invisibility) / SOURCE (input).
5. The class-12 kind 25 snowball carries the player after the crash (position
   written every update), then **life state 20 (exit scene)** moves him to
   (16162, 73) and spawns the end-scene horseman (6.14). VERIFIED.
6. **Sword vs a parrying boss**: Sorsha scans the sword/shot pool and backsteps
   with a clash (6.8); the sword hit itself is the normal class-18 policy
   (madmartigan.md open question 5 still applies for the exact handler).
7. Terrain: 1:4 and 1:2 snow slopes (ids 24-31, 44, 45), ledge material (37) in
   the cave: Madmartigan's slope walking, drop-through and climb are all needed
   here; the new id 23 is ride-only (the player is in state 10 there).
8. The soldiers' level change is disabled in this stage (correction 4), which
   changes how they chase him on the slopes.
