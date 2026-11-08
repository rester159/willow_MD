# Stage 6 (Nockmaar Castle) and the ending: behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin`. Behaviour
spec for the native Genesis rewrite (NATIVE_ENGINE_PLAN.md sections 2 and 6),
organised like `stage4.md`, which it continues. Arcade addresses are citations
only. "Stage 6" = arcade stage index 5 (FF8344 = 5), the final stage. It is the
only stage built from **seven areas** (sub-stage index **FF8374** = 0..6) joined
by doors, and the only stage that **changes the player's form inside the
stage**: areas 0-4 are played as **Madmartigan** (form 4, `madmartigan.md`),
areas 5-6 as **Willow** (form 0, `player.md`, `magic.md`). It ends with the
game's ending and staff roll (section 7) instead of a results screen.

Evidence tags (as the stage-1..4 specs):
- **VERIFIED**: two independent sources agree (disassembly read for this file plus
  a headless arcade trace, or plus the ROM data and a render).
- **SOURCE**: one source only (usually disassembly read here, not trace-checked).
- **UNKNOWN**: not established; says where to look.

Units: world X right, world **Y up**; speeds are signed **8.8 px/frame** (256 =
1 px/frame); one update = one 60 Hz frame. Hit box = (xOffset, width, yOffset,
height), box = [x-xOffset, x-xOffset+width] x [y-yOffset, y-yOffset+height].

**Corrections for earlier specs and notes found while writing this one.**
1. The per-stage tables at **0x2F66** (checkpoint resolver), **0x9AD8** (events)
   and **0x35280** (map hero roots) hold **word offsets**, not longs or
   absolute pointers: entry = table + word[stage]. stage4.md quotes the right
   targets; anyone reading them as longs gets garbage for index 5. 0x4034A
   (object tables) holds longs but has **no entry for index 5**: the loader
   0x4035E branches to **0x40322** for stage 5 (section 3.1). VERIFIED (ROM).
2. Form table 0x2F72 (4 bytes per stage) gives **4** (Madmartigan) for index 5,
   but 0x2F08 only uses it for stage indices < 4; for index 5 it applies only in
   areas 0-4 (FF8374 < 5); from area 5 the current FF8386 is kept, and the area-4
   exit door sets **FF8386 = 0 (Willow)** (0x2D92E). So stage 6 is **not** a
   Madmartigan-only stage. VERIFIED (disassembly 0x2F08..0x2F64, 0x2D92E; RAM
   FF8386 4 -> 0 at the area-5 entry in runs R/D4/D4b).
3. The clear sequence (FF8004 = 4) checks **FF83A2** at step 4 (0xA682): when
   set it goes to **step 20 (0xB11A, the ending)** and never shows the results,
   the clear bonus or a story page for stage 6. FF83A2 is set by Bavmorda's
   wake-up (0x29A3E), i.e. at the area-6 entry, not at her death. The story-page
   end (0xA93E) leaves the stage index at 5 instead of advancing. VERIFIED
   (disassembly; runs R/D7: FF83A2 = 1 at key 3274, ending at 3672/5674).
4. The CPS row-scroll bit is never set in stage 6 or the ending (video control
   0x800122 = 0x003E in every dump of every run). VERIFIED (`S/regs.py`).
5. Gold pickup class 14 kind 0 **variant 10** appears (Kael's reward); its value
   is not in the earlier specs (variants 0 = 1, 5 = 100, 6 = 200, 7 = 500, 8 =
   1000 gold). UNKNOWN (pickups table 0x2D314[10]).

**Content summary.** Stage 6 is Nockmaar Castle, seven areas stacked in one
16 x 16 block map (2.1), each with its own camera window, clock reload and
music; doors (class 14 kind 2) move between areas:
- **Area 0** (y 0..255, x 0..2240) as Madmartigan: a rocky ledge and a
  drawbridge under a stormy night sky, then the castle wall with window
  crossbowmen and archers, then torch-lit corridors with soldiers and a chest.
  One door at the end (x 2240).
- **Areas 1-3** ("THE CORRIDOR IS A MAZE. LOOK FOR THE TRUE DOOR."): corridors
  (y 768.., 1536.., 2304..) with window crossbowmen, flipping stepping stones
  over **spikes** (area 1), chained **spike crushers** that drop from the
  ceiling and armoured mace knights (area 2), and green axe-throwing brutes
  pushing **spiked carts** (area 3). Each ends with two to four doors: one true
  door leads on, the others send the player back to the start of area 1, 2 or
  3 (false doors vanish once used).
- **Area 4** (y 3072, x 0..383, camera fixed): a bridge between two towers under
  the night sky: the mid-boss **General Kael** (class 4 kind 24) with a dialogue.
  His defeat shows **Bavmorda's picture and threat** (story picture + text), then
  the player becomes **Willow**.
- **Area 5** (x 3072..4095, y 0..2303): Willow **climbs a tower** (vertical
  scrolling, spiral stairs) past window crossbowmen, knights, brutes, bomb
  troopers and chests to the door at the top (3584, 2144).
- **Area 6** (y 3072, x 1024, camera fixed): Bavmorda's chamber (a domed altar
  room, candelabras, a swaying far layer): the final boss **Queen Bavmorda**
  (class 18 kind 4), dialogue, fight. Her defeat starts **the ending** (7): her
  whirlwind, the staff roll over six character portraits, Nockmaar castle
  sinking into smoke as the sky turns to day, the closing texts, "PRESENTED BY
  CAPCOM / LUCASFILM", then score ranking, name entry, GAME OVER and the attract
  mode.
VERIFIED (traces below; disassembly; renders in `S/png/`).

## 0. Traces used (headless lockstep, reference program; no GUI)

Harness `build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/lockstep.keys ref`
as stage4.md 0 (`OVERCLOCK=5000`, `RAMEVERY/RAMOUT`, `VIDDUMP/VIDEVERY/VIDFROM`,
`CAMLOG`, `DBGPC=7824` for sound words), started by `S/run.sh NAME FRAMES`.
Some runs use a **private copy** `S/lockstep6` built from `S/lockstep6.c`
(the shared harness and its binary were not touched; runner `S/run6.sh`), with
two additions: `ROUTEUNTIL=k` (the harness's own `SCRIPT=route` portal route for
stage index 5 is followed until waypoint k, then the `random` script plays) and
`POKE=t:addr=val,...;...` (work-RAM bytes at script time t). The `route` script
itself is in the shared harness (`lockstep.c` 602ff.): it teleports the player
next to the true door of each area and holds Up, and zeroes the HP of the
section boss and of the final boss (record found from the boss bar).

Scratch dir
`/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage6/`
(below `S/`). Keys = program VBlank count; script time "t" runs ~3 frames behind.

| Run | Settings | Covers | Output |
|---|---|---|---|
| A2 (hand-over) | `SCRIPT=stand STAGE=4 INVINCIBLE=1 ENDAT=1700`, 7000 frames, `RAMEVERY=4`, `VIDEVERY=6 VIDFROM=3000` | stage-5 results and story, **stage-6 map scene, title, play start** | `S/a2.log`, `S/ramA2/`, `S/vidA2/` |
| AS2 | as A2 + `DBGPC=7824` | its sound words | `S/as2.log`, `S/snd_as2.txt` |
| B (natural areas 0-1) | `SCRIPT=random STAGE=5 INVINCIBLE=1 GATEASSIST=900 NUDGE=600 KILLBOSS=1`, 26000 frames, `RAMEVERY=2`, `VIDEVERY=10` | area 0 (keys 938..8726, the clock ran out at 8250), area 1 (8726..15013, shop 10278..10670); **after a gate assist at t15013 it jumped to area 5 (artefact, see notes)** | `S/b.log`, `S/ram/`, `S/vid/`, `S/births_b.txt` |
| S | as B + `DBGPC=7824` | sound words of B | `S/s.log`, `S/snd_s.txt` |
| R (route + ending) | `SCRIPT=route STAGE=5 INVINCIBLE=1`, 30000 frames, `RAMEVERY=2`, `VIDEVERY=6` | every area by its true door (area 1 1530, 2 1682, 3 1836, 4 1988; Kael zeroed t2287; area 5 3116, 6 3268), Bavmorda zeroed at t3567 (key 3670) before her fight, **the whole ending with no input** (3672..9824), ranking/name entry, GAME OVER, attract | `S/r.log`, `S/ramR/`, `S/vidR/`, `S/births_r_end.txt` |
| RS | as R + `DBGPC=7824` | sound words of R | `S/rs.log`, `S/snd_rs.txt` |
| D2 | private harness, `SCRIPT=route ROUTEUNTIL=2`, random from area 2 (key 1682), `GATEASSIST=900 NUDGE=600 KILLBOSS=1`, 12000 frames | **area 2 natural** (1682..9240, then area 3 by its own door) | `S/d2.log`, `S/ramD2/`, `S/vidD2/` |
| D3b | `ROUTEUNTIL=3`, random from area 3 (1836), `GATEASSIST=99999` (no assist), 14000 frames | **area 3 natural** | `S/d3b.log`, `S/ramD3b/`, `S/vidD3b/` |
| D4b | `ROUTEUNTIL=4`, random from area 4 (1988), `GATEASSIST=3600 KILLBOSS=1`, 7000 frames, `VIDEVERY=6` | **Kael natural** for 3200 updates (128 -> 117), killed at t5590; death, reward, Bavmorda picture, area 5 | `S/d4b.log`, `S/ramD4b/`, `S/vidD4b/`, `S/kael_states.txt` |
| D4bs | as D4b + `DBGPC=7824` | sounds of Kael | `S/d4bs.log`, `S/snd_d4bs.txt` |
| D6 | `ROUTEUNTIL=6`, random from area 5 (3116), 14000 frames | **area 5 natural, lower tower only** (y 47..341) | `S/d6.log`, `S/ramD6/`, `S/vidD6/` |
| D6b | as D6 + `POKE` teleports to the area-5 checkpoints every 900 t | **unusable after t3700**: the teleports put Willow inside walls and she left the tower's columns (map wrap, 2.1) | `S/ramD6b/`, `S/vidD6b/` |
| D7 | `ROUTEUNTIL=7`, random from area 6 (3268), `GATEASSIST=2400 KILLBOSS=1`, 16000 frames, `VIDEVERY=6` | **Bavmorda natural** for 2000 updates (180 -> 171), killed at t5670 (key 5672); **the whole ending** (5674..11436) under random input; name entry, GAME OVER, attract, a new game | `S/d7.log`, `S/ramD7/`, `S/vidD7/`, `S/bav_states.txt` |
| D7s | as D7 + `DBGPC=7824` | sounds of Bavmorda and the ending | `S/d7s.log`, `S/snd_d7s.txt` |
| (C, CS, A, AS) | `ENDAT=1400` on stage 5 / stage 4 | **discarded**: an injected clear on stage index 5 stops at ending sub-step 4 forever (FF83F6 is set only by Bavmorda's whirlwind, 7.2); run A injected before play | `S/c.log`, `S/ramC/` |

Helper scripts in `S/`: `areas.py` (area/mode timeline), `modes.py`, `flags.py`,
`births.py`, `live.py` (actors of one dump), `prof.py` -> `S/prof.txt` (every
family seen: count, HP, palettes, nodes), `farchk.py` (far-layer rate check per
area), `mkroots.sh` -> **`S/roots.txt`** (pose root tables), `montage.py` /
`render.sh` (renders in `S/png/`), `regs.py`, `dr.py` (address-range listing of
`S/full.dis`, the disassembly of 0..0x48000), `unwrap.py`, `sndseq.py`.

Harness notes. The DBGPC sound logs drift in long silent stretches (the ending,
the credits); their order and counts are reliable, keys are not: the ending's
sound keys below come from the code (sub-step starts) plus the RAM traces.
Offsets used: run RS column + ~765 at Bavmorda's death; D7s + 766. The
`GATEASSIST` driver bumps **FF83E7** when the camera is still for N frames; in
stage 6 FF83E7 is the **Kael-defeated flag that opens the area-4 exit**
(2D864), so an assist anywhere sends the player to area 5 (B at t15013, an
early D3 at t2737): those parts are artefacts. All bosses were killed by the
harness after a natural phase; everything after the kill is the normal death
path started artificially.

---

## 1. Stage entry after stage 5

### 1.1 Hand-over timeline (run A2, keys)

| Key | Mode (FF8004 / FF8008) | Event | Tag |
|---|---|---|---|
| ~3584..4003 | clear step 18 | stage-5 story page (stage 5's spec); fade; stage index -> **5** (stage word 5 at v4003), rank update (1.8) | VERIFIED |
| 4004 | mode 0 step 2..4 | **world-map scene** (1.2): sounds **0xF0, 0xF7, 0x12** | VERIFIED |
| 4008..4188 | step 4 | fade-in and the 128-update hold | VERIFIED |
| 4188..4244 | step 6 | sounds 0xF0, 0xF7; fade-out; set-up 0xA0D0: stage index 5 has **no intro scene** (only indices 3 and 4 do), FF8348 set -> play set-up 0xA220 | VERIFIED |
| 4244 | play sub 2 | stage reset 2DFE: clock **2:00**, HP = max, **music word 0x0C** (sounds **0x0C, 0x01FA**), form 4, player (192, 80) facing right, camera (0, 0), HUD, stage objects 4035E (common + area-0 table, 3.1), events 9A92 | VERIFIED |
| 4248..4344 | subs 4 / 6 / 8 | title (1.4): 21 x sound **0x2F**, 4 updates apart | VERIFIED |
| 4344 | play step 2 | text cleared; clock runs (2:00 -> 1:59) | VERIFIED |
| 4356..4360 | | player life state 2 (control) | VERIFIED |

Map scene to play set-up: 240 updates; play set-up to control: ~116. Buttons
fast-forward the scenes (FF799C). VERIFIED.

### 1.2 World-map scene for stage 6
As stage4.md 1.2 except: markers I..V palette 0x14 (cleared), marker **VI**
(class 12 kind 21 variant 5 at (2792, 200)) blinks; the hero figure (class 12
kind 22) at **(2792, 176)** (0x35268[5]) uses root **0x35410** (0x35280 +
word[5]; the same chain as stage 5): **Willow and Madmartigan together**,
palette **21**, beside the castle of Nockmaar at the top of the map. VERIFIED
(ROM; RAM `ramA2/r004104`; render `vidA2/v004104`).

### 1.3 Intro scene
None (set-up 0xA0D0 starts a scene only for indices 3 and 4). VERIFIED.

### 1.4 Title text (play sub-steps 4, 6, 8)
- Sub 4: message **0x0659** (0x94C8[5] = 0x59), style 0x1412: `STAGE6`
  (codes 06C4 06C6 0680 068C 0688 **0B0A**). VERIFIED (ROM record 0x71D34; render
  `vidA2/v004302`).
- Sub 6: message **0x066A** (0x9506[5] = 0x6A), style **0x1421** (0x950C[5]):
  `NOCKMAAR CASTLE` (15 big characters after four 0x000E words; big codes N
  **06AA**, O 06AC, C 0684, K **06A4**, M 06A8, A 0680, R 06C2, space 01A6, S
  06C4, T 06C6, L **06A6**, E 0688). 6 + 15 = 21 x 0x2F. VERIFIED (ROM 0x722C8;
  render `vidA2/v004302` mid-typing; run AS2).
- Cut 3 applies (title on its own rows, plain background).

### 1.5 Music and clock
- Music per **area** (table **0x78AA**[FF8374], read by the clock/music reload
  0x2ED2 for stage index 5): areas 0-3 **0x0C**, area 4 **0x0E**, area 5
  **0x0D**, area 6 **0x0F** (each followed by 0x01FA). Every door transition
  fades the music (**0x10FB**) and the new area's music is posted when the area
  starts. Kael also posts music word 0x0E when he wakes (0x1DEBE); Bavmorda
  posts 0x0F at her wake-up (0x29A44). VERIFIED (ROM; runs RS/D4bs/D7s:
  0x10FB, 0x0C at each corridor door; 0x0E x2 at area 4; 0x10FB, 0x0D at area
  5; 0x10FB, 0x0F x2 at area 6).
- **Ending music 0x1D** (with 0x01FA) starts while Bavmorda's whirlwind is on
  screen (7.2). VERIFIED (RS/D7s order) / UNKNOWN (the code that posts it; not
  found as an immediate 0x1D near 786A: probably through 788A with a table).
- Clock: **2:00** in areas 0-4 and 6, **3:00** in area 5 (checkpoint records,
  1.7). It is **reloaded at every area change** (door phase 8 calls 2E9C, 8
  updates after the new area appears), but **not** when Kael's death moves the
  player to area 5 by the automatic path (the clock keeps running until the
  area-5 door scene ends, then reloads 3:00). Normal 1 s ticks; stops while
  FF83E9/FF83CF/FF83D2 (dialogues, door transitions, boss defeats). Run B let
  the clock reach **0:00** in area 0 at key 8250 (invincibility DIP on: no
  time-out death happened; the clock stayed at 0:00 until the next area).
  VERIFIED (traces) / UNKNOWN (the time-out rule without the DIP: combat.md).

### 1.6 Start position, player form
- Player start: **(192, 80)** facing right, camera **(0, 0)**, far layer (0, 0).
  VERIFIED (A2, B, R).
- Form: Madmartigan (FF8386 = 4) in areas 0-4; **Willow (FF8386 = 0)** from the
  area-4 exit (correction 2). The area-4 exit door (variant 10) also resets the
  player record (0x2D92E: FF1866 = 0, FF8386 = 0, player +2 = 2, +3 = 0, +4 =
  0, +0F = 2 (facing), FF1876 = 2, pose 0xD14A). Whether Willow keeps Madmartigan's HP,
  items or magic: the HP and gold carry over (HUD unchanged in `vidD4b`);
  magic level / bracelets: UNKNOWN (player record fields not compared).
  VERIFIED (RAM, renders) / SOURCE (field list).

### 1.7 Checkpoints (restart table)
Resolver **0x34C4** (0x2F66 + word[5] = 0x2F66 + 0x55E). Unlike every other
stage it is **per area**: jump table 0x34D2[area] chooses the index source:
areas 0-3 the last-safe **x >> 8** (FF8370), area 5 the last-safe **y >> 8**
(FF8372), areas 4 and 6 the constant 0. The byte table is 0x356E + word[area],
the records (8 bytes: x, y, time BCD, music) 0x3560 + word[area]; the index
only increases (FF8394). Record x with bit 15 set: x & 0x7FFF, camera min X = x
& 0xFF80; else camera min X = x - 512 (>= 0). VERIFIED (ROM; disassembly).

| Area | Index source | Byte table (entries 0..9) | Records (c: x, y, time, music) |
|---|---|---|---|
| 0 | x >> 8 | 0x357C: 0 0 1 2 3 4 5 6 7 7 | 0x35A4: 0 (192,80) 1 (544,80) 2 (800,80) 3 (1056,120) 4 (1312,80) 5 (1568,120) 6 (1824,80) 7 (2080,80); all 2:00, 0x0C |
| 1 | x >> 8 | 0x3586: 0 0 1 2 2 3 3 3 4 4 | 0x35E4: 0 (192,848) 1 (544,848) 2 (736,848) 3 (1408,848) 4 (2080,848); 2:00, 0x0C |
| 2 | x >> 8 | 0x3590: 0 1 2 3 4 5 6 7 8 8 | 0x360C: (192,1616) (224,1616) (480,1616) (736,1616) (992,1616) (1248,1616) (1504,1688) (1760,1688) (2016,1688); 2:00, 0x0C |
| 3 | x >> 8 | 0x3590 (shared with area 2) | 0x3654: (192,2392) (224,2392) (480,2392) ... (2016,2392) every 256 from 224; 2:00, 0x0C |
| 4 | 0 | - | 0x369C: **(32 \| 0x8000, 3152)**, 2:00, **0x0E** |
| 5 | y >> 8 | 0x359A: 0 1 2 3 4 5 6 7 8 8 | 0x36A4: 0 (3424,48) **3:00**; 1 (3744,272) 3:00; 2 (3424,528) **2:30**; 3 (3744,784) 2:30; 4 (3424,1040) 2:30; 5 (3744,1296) **2:00**; 6 (3664,1552) 2:00; 7 (3504,1808) **1:30**; 8 (3680,2144) 1:30; music **0x0D** |
| 6 | 0 | - | 0x36EC: **(1056 \| 0x8000, 3144)**, 2:00, **0x0F** |

Note the area-5 times fall with height (3:00 at the bottom, 1:30 near the top)
and the stage-wide music word in the record matches the area table 0x78AA.
Restart behaviour after a life loss in stage 6 (which area, whether the area-0
start or the current area's checkpoint): SOURCE (the resolver keeps FF8374) /
UNKNOWN (no death was traced: all runs used the invincibility DIP).

### 1.8 Difficulty rank
Stage start (0x38146): as stage4.md 1.8 with **+0x18** for index 5
(0x3819A[5]). Run A2: 0x3089 at the stage-5 clear -> **0x4889** at the stage-6
start. Rank keeps rising ~1 per update during play (B: 0x3000 debug start ->
0x4EE1 by key 9240). VERIFIED.

---

## 2. Map, layers, camera, collision, palettes

### 2.1 Layers, areas and stage size
- **Foreground (scroll2)**: grid **5** (0x2DBF8[5] = **0x2E924**), **mode 0**:
  16 x 16 blocks of 256 px (16 x 16 cells of 16 px, ROM 0x80000 + b*0x400);
  index = row * 16 + ((x >> 8) & 15), row = 15 - (y >> 8) (Y up). The map is
  **4096 x 4096 px** and **wraps** horizontally (column = (x >> 8) & 15): the
  areas are separate windows of this map, kept apart by the camera limits (2.3).
  VERIFIED (ROM; `stagemap.py` mode 0 handles it).

  | Area | World rect | Grid rows : columns -> blocks | Picture |
  |---|---|---|---|
  | 0 | x 0..2559, y 0..255 (+ ceiling row y 256..511) | row 15: 211 212 213 204 203 204 208 215 216 217; row 14 cols 2-9: 255 | ledge + drawbridge (x 0..~520), castle wall, corridors (`vid/v001000`..`v008400`) |
  | 1 | y 768..1023 | row 12: 218 214 95 219 220 221 222 253 223 217 (row 11: 255 ceiling) | corridor, windows, spikes and flipping stones, doors (`vid/v009000`..`v014000`) |
  | 2 | y 1536..1791 | row 9: 224 225 207 226 225 227 228 229 230 254 (row 8: 255) | crusher hall, mace knights, stairs (`vidD2/v001800`..`v009100`) |
  | 3 | y 2304..2559 | row 6: 231 232 233 233 234 233 233 234 235 217 (row 5: 255) | two-level corridor with spiked carts (`vidD3b/v001900`..`v011000`) |
  | 4 | x 0..511, y 3072..3327 | row 3 cols 0-1: 236 210 | Kael's bridge (`vidD4b/v002004`) |
  | 6 | x 1024..1535, y 3072..3327 | row 3 cols 4-5: 237 209 | Bavmorda's chamber (`vidD7/v003450`) |
  | 5 | x 3072..4095, y 0..2303 | rows 15..7, cols 12-15: walls 240/205 (col 12), 238/206 (col 15); interior col 13-14: 239 241 / 242 243 / 244 245 / 246 247 / 82 83 / 248 249 / 84 250 / 248 79 / 251 252 | the tower (`vidD6/`, `vidR/v003120`..`v003260`) |

  Block 255 (rows 14, 11, 8, 5) is the ceiling strip above each corridor.
  VERIFIED (ROM grid dump) / renders.
- **Far layer (scroll3)**: grid 0x2EB24[5] = **0x2F04C**, mode 0, blocks of 256
  px (32x32 tiles at 0xC0000 + b*0x100). Used blocks: rows 15..12 (far y
  0..1023), columns 0-5: 80-85 / 86-91 / 92-97 / 98 99 x3 (row 12) and row 11
  cols 0-5: 100 101 x3; columns 6-8 rows 15..10: 110 111 112 (x4) / 113 114 115
  / 116 117 118; rows 9-8 cols 0-3: 102 103 106 107 / 104 105 108 109.
  The far picture per area (far = camera / 2): area 0 far y 0 -> row 15 (80-85,
  the storm clouds over the sea at night); area 1 far y 384 -> row 14 (86-91);
  area 2 far y 768 -> row 12 (98 99); area 3 far y 1152 -> row 11 (100 101); area 4
  far (0, 1536) -> row 9 cols 0-1 (102 103: night clouds over the bridge); area 6
  far (512, 1536) -> row 9 cols 2-3 (106 107, the view behind the chamber's
  columns); area 5 far x 1536..2048, y 0..1024 -> cols 6-8 rows 15..10 (110-118,
  the sky beside the tower). Inside the corridors the far layer is mostly hidden
  by scroll2 (it shows through windows). VERIFIED (ROM; renders) / SOURCE (which
  far blocks show through which windows).
- **Scroll rates**: scroll2 = camera (1:1); scroll3 = **camera / 2 in x and y**
  (0x42FC4[5] = 1). Checked in every play dump of runs B, R, D2, D3b, D4, D6,
  D7 (37,000 dumps): far X = camX >> 1 and far Y = camY >> 1 in areas 0-5 always
  (the only exceptions are the shop/story-picture frames, camera (2048, 256)).
  Vertical far motion happens in area 5 (camera Y 0..2048 -> far Y 0..1024).
  VERIFIED.
- **Special scroll effect (area 6)**: the far X is **not** camera / 2: the
  camera hook 0x4310C (stage 5, area 6 -> 0x43150) moves far X by **+-1 px per
  update, reversing every 64 updates** (+72 counter, +73 direction): a triangle
  wave far X = 512 .. 576 (period 128 updates) while the camera stays at (1024,
  3072). Far Y stays 1536. VERIFIED (D7: 3268..5672 every 2 keys: 511, 513 ...
  575, 573 ... 511) / SOURCE (code).
- **No row, line or column scroll** anywhere (correction 4). Layer order word
  0x800170 = **0x12F8** in play, 0x12C0 during fades and door transitions,
  0x13B8 in shops, 0x12D0 for one dump at the ending's portrait/scene switch.
  VERIFIED.
- **Native (parallax rule, AGENTS.md 2026-11-02)**: plane A = foreground at the
  camera; plane B = far layer at (camX / 2, camY / 2), both axes, with the area-6
  sway as a separate far-X animation. The far picture used per area is small
  (one or two block rows); vertical far motion is needed in area 5 (1024 px of
  far picture for 2048 px of camera), so the HUD must not pin plane B's
  vertical scroll there.

### 2.2 Terrain (scroll2 grid 5, collision ids)
Collision id = attribute bits 10-15 of each 16x16 cell; descriptors 8 bytes at
0x4712 + id*8 (player.md 6). Ids per area (cell counts over the area's blocks):

| Area | Ids (count) |
|---|---|
| 0 | 2 (127), 3 (1), 4 (4), 5 (8), 6 (4), 8 (19), 9 (175), 10 (8), 13 (112), **37** (48) |
| 1 | 1 (2), 2 (68), 3 (2), 4 (2), 5 (68), 6 (2), 8 (16), 9 (92), 10 (4), 13 (144), **39** (144) |
| 2 | 1 (6), 2 (51), 3 (8), 4 (4), 5 (61), 6 (5), 8 (25), 9 (452), 10 (23), 12 (10), 13 (123), 14 (11), 21 (4), 22 (5), 37 (26), 46 (9) |
| 3 | 5 (144), 8 (11), 9 (559), 13 (144), 37 (120) |
| 4 | 2 (24), 5 (12), 9 (24) |
| 6 | 2 (24), 9 (24) |
| 5 | 5 (24), 8 (144), 9 (288), 10 (144), 21 (8), 22 (8), 37 (206), 44 (8), 45 (8), 46 (25) |

VERIFIED (ROM scan).
- **39 = hazard (spikes)**: 144 cells along the floor of area 1 (the spike pit
  crossed on the flipping stones, 5.6): touching it costs HP through 3A66C
  (combat.md 2.6; madmartigan.md probe offsets). VERIFIED (ROM; render
  `vid/v009800`, the spike strip).
- **37** (ledge material + floor, thin platform: Madmartigan can drop through
  and climb, madmartigan.md 2.7/2.8): balconies in areas 0, 2, 3 and the tower
  stairs/landings in area 5 (206 cells). SOURCE (positions not tabulated: use
  `stagemap.py 5 ...`).
- 21/22 (stage-3 stairs/slope ids with byte 5 = 0x82), 44/45 (1:2 slopes), 46
  (solid fill): the tower's spiral stairs (area 5) and the area-2 staircase.
  SOURCE.
- 9 (`0f92fdfd12`) is the common solid floor/wall; 13 / 8 / 10 walls and
  ceilings as in earlier stages. No water (32). VERIFIED (ROM).

### 2.3 Camera
- Stage-start limits (0x2DCA / 0x2DBE / 0x2DD8 [5]): max X **0x07A8 = 1960**,
  min Y 0, max Y 0. VERIFIED (ROM; RAM).
- **Per-area window** (set by the door transition 0x2D8B6.., RAM FF838C min X,
  FF846A/FF838E max X, FF845C/FF8390 min Y, FF8434/FF8392 max Y):

  | Area | min X | max X | Y | Notes | Tag |
  |---|---|---|---|---|---|
  | 0 | follows (camera - 768 rule) | **1960** | **0** fixed | horizontal | VERIFIED |
  | 1 | follows | 1960 | **768** fixed | | VERIFIED |
  | 2 | follows | 1960 | **1536** fixed | | VERIFIED |
  | 3 | follows | 1960 | **2304** fixed | | VERIFIED |
  | 4 | 0 | **0** | **3072** | camera fixed at (0, 3072) | VERIFIED |
  | 5 | follows (0 at the entry) | **4096** | **0 .. 2048** | vertical scrolling; FF8392 = 0x0800 (0x2D9DE[5]) | VERIFIED (RAM) |
  | 6 | **1024** | **1024** | **3072** | camera fixed at (1024, 3072) | VERIFIED |

  Y: min Y = start y & 0xFF00 (0x2D8E0, FF8390); max Y = 0x2D9DE[area] = 0,
  0x300, 0x600, 0x900, 0xC00, 0x800, 0xC00 (FF8392); min X = 0 except **1024**
  for door 11 (0x2D8EC, area 6).
  X follows the player at player X - 192 (player.md 7); camera mode 14 (bit 2
  set) is the normal mode in every area. VERIFIED.
- Area 5: X in [3072-ish .. 4096] is wider than the tower (x 3072..4095 is four
  block columns, the screen is 384 wide): with max X 4096 the camera can show
  x up to 4480, i.e. **wrapped columns 0-1** of other rows. In natural play the
  tower walls keep Willow inside (camera X 3152..3633 in run D6); the
  arcade does not clamp further. Native: clamp the area-5 camera to the tower
  (UNKNOWN whether the arcade ever shows the wrap; D6b's teleports did).
- No arena glide or lock code: areas 4 and 6 are fixed windows; no camera
  hook raises limits during the stage. VERIFIED (flags in D4b/D7: limits never
  change inside an area).

### 2.4 Pit
None: every area has a solid floor (the area-1 spike strip is a hazard, not a
pit). SOURCE.

### 2.5 Doors (class 14 kind 2, routine 0x2D78E; table "common" 3.1)
Twelve doors are created at the stage load (common object table) and live in
every area (they are drawn only when on screen; palette **27**, root 0x2DB16,
open/close nodes 0x2DB60 / 0x2DBAA).

| Variant | Position | In area | Leads to (0x2D974[v]) | Start position (0x2D994[v]) | True / false (0x2DA4C[v]) |
|---|---|---|---|---|---|
| 0 | (2240, 72) | 0 | **1** | (64, 848) | true |
| 1 | (2112, 840) | 1 | **2** | (64, 1616) | true |
| 2 | (2240, 840) | 1 | 1 (restart area 1) | (64, 848) | false |
| 3 | (2048, 1680) | 2 | **3** | (64, 2472) | true |
| 4 | (2144, 1680) | 2 | 1 | (64, 848) | false |
| 5 | (2240, 1680) | 2 | 2 (restart) | (64, 1616) | false |
| 6 | (2144, 2464) | 3 | 2 | (64, 1616) | false |
| 7 | (2240, 2464) | 3 | **4** | (32, 3152) | true |
| 8 | (2144, 2384) | 3 | 1 | (64, 848) | false |
| 9 | (2240, 2384) | 3 | 3 (restart) | (64, 2472) | false |
| 10 | (352, 3152) | 4 | **5** | (3392, 48) | automatic after Kael |
| 11 | (3584, 2144) | 5 | **6** | (1064, 3144) | true |

Behaviour (SOURCE, VERIFIED for the paths the route and D2 runs took):
1. Phase 0 (0x2D7EA): if 0x2DA4C[v] = 0 and bit v of **FF8384** is set, the door
   is removed (a used false door is gone for the rest of the game).
2. Waiting: when the player is within **x +-12, y -14..+14** of the door, holds
   **Up** without Left/Right, and is grounded (FF8362): FF83E9 = FF83EA = 1
   (clock stop, player freeze), player state forced (FF1846 = 2, FF1896 = 0,
   FF187C = 1), 30 updates.
3. Then (0x2D89A): music fade **0x10FB**, screen fade, FF8374 = new area, player
   and camera to the start position, FF8390 = y & 0xFF00, FF838C = 0 (1024 for
   variant 11), FF838E from 0x2D9C8/0x2D9D6, FF8392 = 0x2D9DE[area]; variant
   10 also makes the player Willow (1.6); HUD redraw 4F44.
4. After 5 updates: 263C and **0x4032A (the new area's object table, 3.1)**;
   after 8 more: clock and music reload 2E9C (1.5); a false door sets bit v of
   FF8384; FF83E9/FF83EA cleared.
- Variant 10 (area 4) does not wait for Up: it waits for **FF83E7** (Kael
  defeated), then freezes the player (FF83E9/EA/EB), and after its timer shows
  the **Bavmorda story picture** (phase 10, 0x2DA5A: scroll2 grid 7 at (2048,
  256), the far layer hidden, palette set 1 (FF8343), message **0x48** style
  0x031B: "I WILL NOW SEND ELORA TO THE WORLD OF&DARKNESS. HA, HA, HA,...&THEN
  THE WORLD WILL BE MINE.", **256 updates**, buttons fast-forward (799C)), then
  the area change to area 5 and a full stage set-up 0x978. VERIFIED (D4b keys
  5910 -> 6080; render `vidD4b/v006036`).
- Run D2 took the true area-2 door naturally (key 9240); the route took every
  true door; no false door was used in any run (so FF8384 and the restarts are
  SOURCE).
- Native: a door is a trigger rectangle with Up, a 30-update freeze, the fade,
  and a table lookup; false doors are a saved bit set.

### 2.6 Palettes
- Loaded at stage set-up (0x0978) as earlier stages (object palettes, scroll2
  32 palettes, scroll3 16 palettes into 16-31 per 0xA2E[5] = 1). The dumps hold
  the live palettes. SOURCE.
- **Seven permanent palette controllers** (table 0x410EC[5] = **0x411D2**, count
  7, 18-byte entries: type/kind nibbles, destination, steps, frames, colour
  table, four window words), created by the object loader at the stage start
  and **alive in every area** (RAM pool FFF042: all seven in every play dump of
  areas 0-6):

  | # | Entry | Destination (live +30) | Colour table | Steps x frames | Window words | Tag |
  |---|---|---|---|---|---|---|
  | 0 | `30 1B 05 08` | scroll3 palette **27** (0x91CF60) | 0xECDA0 | 6 x 8 | 0, 0x0B00, 0x0300, 0x0600 | VERIFIED (RAM) |
  | 1 | `31 1C 13 03` | scroll3 palette **28** (0x91CF80) | 0xECE60 | 20 x 3 | 0x0300, 0x0600, 0x0C00, 0x0F00 | VERIFIED |
  | 2 | `31 1D 14 03` | scroll3 palette **29** (0x91CFA0) | 0xECF40 | 21 x 3 | same | VERIFIED |
  | 3 | `21 0D 15 03` | scroll2 palette **13** (0x91C9A0) | 0xEAA00 | 22 x 3 | same | VERIFIED |
  | 4 | `21 0E 15 03` | scroll2 palette **14** | 0xEAA40 | 22 x 3 | same | VERIFIED |
  | 5 | `21 0F 15 03` | scroll2 palette **15** | 0xEAA80 | 22 x 3 | same | VERIFIED |
  | 6 | `21 10 15 03` | scroll2 palette **16** | 0xEAAC0 | 22 x 3 | same | VERIFIED |

  Their meaning by area: scroll3 27-29 are far-layer (sky/storm) palettes and
  scroll2 13-16 are scenery (torch/window light) palettes. How the controller
  "type" nibbles 2 and 3 run (continuous cycle, random flicker, the window
  words): UNKNOWN (read the controller tick for kinds 2/3; stage3.md
  correction 2 documents kind 1). Native: keep all seven as palette cycles on
  the fixed per-stage lines (cut 4 allows fixed palettes, but the cycles are
  part of the arcade look: flickering torches and storm sky).
- Boss hit jobs: Kael **0xE6E20** (area 4), Bavmorda **0xE6E60** -> object
  palette **29** (22/23 steps x 3). VERIFIED (pool jobs in D4b/D7).
- **Bavmorda's lightning flashes** turn the whole room grey for a few frames
  (`vidD7/v003672`, `v004200`; also the first ending frames `vidR/v004200`):
  done by switching the palette set, not by the job pool. UNKNOWN (which
  routine; look for FF8343 / 0xAC4 calls in 0x29972..0x2AC80).

---

## 3. Placed objects and spawn events

### 3.1 Object tables (loader 0x4035E -> 0x40322 for stage index 5)
Two tables per load: the **common table 0x40B9A** (12 entries, always) and the
**area table** 0x4033C + word[FF8374] (offsets 0x08C0, 0x098A, 0x0A74, 0x0B3E,
0x0C40, 0x0C4A, 0x0D2C). Entries are 8 bytes: class, variant, kind,
direction, x, y (as stage4.md 3.1). The loader runs at the stage start and on
every area change (0x4032A), so each area's objects appear when it is entered.
VERIFIED (ROM; births in every run at the area-entry keys).

**Common (0x40B9A)**: the twelve doors, class 14 kind 2 variants 0-11 at the
positions of 2.5. VERIFIED.

**Area 0 (0x40BFC, 25 entries)**

| # | class / kind / var | dir | x, y | What it is |
|---|---|---|---|---|
| 0-9 | 12 / 6 / 6 | 0 | (800,160) (928,160) (1312,160) (1440,160) (1824,160) (1952,160) (1568,160) (2080,160) (1696,160) (2208,160) | wall-torch animators (stage2.md 5.8) |
| 10 | 16 / 4 / 0 | 0 | (1376, 160) | chest 0 (8.1) |
| 11-14 | 4 / 17 / 4 | 0 | (600,128) (712,128) (1112,128) (1224,128) | window crossbowmen (4.2) |
| 15-20 | 4 / 0 / 4 | 0 | (832,168) (976,168) (1344,168) (1488,168) (1856,168) (1968,168) | soldiers on the upper ledges (4.1) |
| 21-23 | 4 / 0 / 4 | 0 | (1376,80) (1488,80) (1904,80) | soldiers on the floor |
| 24 | 4 / 3 / 2 | 0 | (2224, 80) | archer by the exit door |

**Area 1 (0x40CC6, 29 entries)**

| # | class / kind / var | dir | x, y | What it is |
|---|---|---|---|---|
| 0-3 | 12 / 6 / 6 | 0 | (64,928) (2096,928) (1472,928) (2224,928) | torches |
| 4-12 | 4 / 17 / 4 | 0 | (344,896) (456,896) (600,896) (712,896) (872,912) (1352,960) (1768,928) (1896,928) (2024,928) | window crossbowmen |
| 13 | 12 / 8 / 32 | 16 | (736, 848) | shop door (8.2) |
| 14-27 | 6 / 9 / 0..13 | 0, 1, 1, 1, 1, 1, 0, 255 x7 | (784,848) (880,848) (1024,816) (1120,912) (1184,816) (1280,912) (1536,816) (1600,848) (1664,816) (1728,848) (1792,816) (1856,848) (1920,816) (1984,848) | flipping stepping stones over the spikes (5.6) |
| 28 | 4 / 0 / 4 | 0 | (208, 848) | soldier |

**Area 2 (0x40DB0, 25 entries)**

| # | class / kind / var | dir | x, y | What it is |
|---|---|---|---|---|
| 0 | 12 / 6 / 6 | 0 | (160, 1664) | torch |
| 1 | 16 / 4 / 0 | 1 | (592, 1680) | chest 1 |
| 2, 3 | 4 / 19 / 1, 2 | 1 | (912,1624) (912,1696) | mace knights (4.3) |
| 4-6 | 12 / 6 / 7 | 0 | (448,1696) (1216,1696) (1376,1696) | chandelier animators (variant 7) |
| 7-16 | 6 / 10 / 0,0,0,0,1,0,2,2,2,2 | 2,0,2,0,0,2,0,0,0,0 | (280,1704) (408,1752) (536,1704) (664,1752) (1048,1752) (1176,1704) (1560,1752) (1688,1752) (1816,1752) (1944,1752) | spike crushers (5.7) |
| 17-19 | 4 / 0 / 4 | 0 | (1296,1616) (1504,1688) (1752,1688) | soldiers |
| 20 | 4 / 3 / 2 | 1 | (1880, 1688) | archer |
| 21-24 | 4 / 14 / 0 | 1 | (2064,1696) (344,1624) (472,1624) (600,1624) | axe brutes (4.4) |

**Area 3 (0x40E7A, 32 entries)**

| # | class / kind / var | dir | x, y | What it is |
|---|---|---|---|---|
| 0-14 | 12 / 6 / 6 | 0 | (32,2400) (944,2400) (160,2480) (1184,2448) (288,2448) (1456,2480) (432,2480) (1456,2400) (432,2400) (1712,2480) (688,2480) (1712,2400) (688,2400) (1952,2448) (944,2480) | torches |
| 15 | 16 / 4 / 0 | 2 | (2288, 2384) | chest 2 |
| 16, 17 | 4 / 19 / 3, 4 | 1 | (2208,2480) (2208,2400) | mace knights guarding the doors |
| 18 | 12 / 8 / 0 | 13 | (32, 2392) | shop door (8.2) |
| 19-27 | 4 / 27 / 0 | 128, 64, 96, 96, 64, 64, 160, 64, 96 | (760,2480) (1120,2480) (1720,2480) (568,2400) (792,2400) (1088,2400) (1520,2400) (1688,2400) (1880,2400) | spiked-cart spawners (5.4; "dir" = parameter) |
| 28-31 | 4 / 14 / 0 | 1 | (200,2400) (1216,2400) (1824,2480) (1984,2400) | axe brutes |

**Area 4 (0x40F7C)**: one entry, **class 4 kind 24 variant 0, dir 1, (304,
3160): General Kael** (6.1).

**Area 5 (0x40F86, 28 entries)**

| # | class / kind / var | dir | x, y | What it is |
|---|---|---|---|---|
| 0, 1 | 12 / 8 / 48, 16 | 17, 14 | (3488,2152) (3808,520) | shop doors |
| 2, 3 | 16 / 4 / 0 | 3, 4 | (3344,576) (3344,1168) | chests 3, 4 |
| 4-9 | 4 / 19 / 5..10 | 1,1,1,0,0,0 | (3776,416) (3776,800) (3776,1312) (3392,528) (3392,1056) (3504,2160) | knights |
| 10-12 | 4 / 34 / 0, 1, 2 | 0 | (3648,1344) (3520,1600) (3648,1856) | bomb troopers (5.3) |
| 13-21 | 4 / 17 / 4 | 1,1,1,0,1,0,0,1,0 | (3584,208) (3520,144) (3520,432) (3648,432) (3520,560) (3648,720) (3648,976) (3520,1072) (3648,1232) | window crossbowmen |
| 22-25 | 4 / 33 / 0 | 0 | (3536,1376) (3536,1888) (3632,1632) (3464,2080) | stationary mortars (5.2) |
| 26, 27 | 4 / 0 / 4 | 0, 1 | (3728,184) (3456,408) | soldiers |

**Area 6 (0x41068, 6 entries)**

| # | class / kind / var | dir | x, y | What it is |
|---|---|---|---|---|
| 0 | 4 / 28 / 0 | 0 | (1376, 3152) | the altar brazier (5.5) |
| 1 | **18 / 4 / 0** | 1 | (1216, 3160) | **Bavmorda** (6.5) |
| 2-5 | 12 / 40 / 1..4 | 1 | (1088,3160) (1168,3160) (1264,3160) (1344,3160) | candelabras (6.7) |

All VERIFIED (ROM; every entry appears in RAM at its area entry).

### 3.2 Event table (0x9AD8 + word[5] = 0x9E70, 9 events)
Fields as stage4.md 3.2: +0E class, kind, variant, +0F, par (+12), endX (+06),
y (+08), startX (+0A), spawnY (+0C). Events are created at the stage start and
persist across areas (they test their own windows). VERIFIED (ROM; RAM
FF8502.. in run B from key 838).

| Ev | +0E / kind / var | +0F | endX (+06) | y (+08) | startX (+0A) | spawnY (+0C) | Meaning |
|---|---|---|---|---|---|---|---|
| 0 | 4 / 0 / 0 | 0 | 544 | 80 | 64 | 80 | edge soldiers in area 0, camera X 64..544 (4.1) |
| 1 | 4 / 3 / 0 | 1 | 1120 | 80 | 1120 | 80 | archer pair at x 1120 (area 0): spawns class 4 kind 3 **variants 10 and 12** at (1120, 80) and (1248, 80) at once (key 844), then retires |
| 2 | 4 / 3 / 0 | 1 | 1632 | 80 | 1632 | 80 | archer pair at (1632, 80) var 12 and (1760, 80) var 10 |
| 3-6 | 4 / 27 / 0 | 100, 192, 192, 160 | 624, 640, 928, 1568 | 2480, 2400, 2480, 2480 | 368, 384, 672, 1312 | as y | spiked-cart spawner events in area 3 (5.4) |
| 7, 8 | 4 / 14 / 14 | 1 | 3808, 4784 | 192, 384 | 3808, 4784 | 192, 384 | axe-brute respawners: ev 7 keeps one kind-14 **variant 14** brute at (3808, 192) in the lower tower (B: born 15614, 16238, 21478 after each death) |

Event 8's x 4784 lies outside every area (map column 2 by wrap): it never
spawns in natural play. SOURCE / VERIFIED (no birth from it in any run).

### 3.3 Roster

| Family | Class / kind / var | Count | Area | Spawned by | Energy | Score | Drop |
|---|---|---|---|---|---|---|---|
| Soldier | 4 / 0 / 4 (and 0 / 0 event) | 13 placed + edge entrants | 0, 1, 2, 5 | table, ev 0 | 1 | 50 | 1 gold |
| Archer | 4 / 3 / 2, 8, 10, 12 | 2 placed + 4 event | 0, 2 | table, ev 1-2 | 4 | as stage 1 | as stage 1 |
| Arrow | 10 / 0 | per shot | - | archers, crossbowmen | - | - | - |
| Window crossbowman | 4 / 17 / 4 | 22 | 0, 1, 5 | table | 1 | stage2.md 5.4 | stage2.md 5.4 |
| Axe brute (+ returning axe) | 4 / 14 / 0, 10, 12, 14; 10 / 7 | 8 placed + cart pushers + ev 7 | 2, 3, 5 | table, kind-27 spawners, ev 7 | 4 (var 0, 14), **8** (var 10, 12) | 200 | 100 gold |
| Mace knight | 4 / 19 / 1-10 | 10 | 2, 3, 5 | table | 48 | 2000 | 3 x 500 gold |
| Flail shards / knight projectiles | 10 / 8 / 0-7 | per attack | 5 | knights var 5-10 | - | - | - |
| Spiked-cart spawner | 4 / 27 / 0 (and 2) | 9 placed + 4 events | 3 | table, ev 3-6 | - | - | - |
| Spiked cart | 6 / 8 / 0 (routine of 4/29) | per spawner | 3 | kind 27 | **30** | UNKNOWN | UNKNOWN |
| Bomb trooper (tower) | 4 / 34 / 0-2 | 3 | 5 | table | 24 | UNKNOWN | UNKNOWN |
| Mortar | 4 / 33 / 0 + 12 / 37 part | 4 | 5 | table | 24 | UNKNOWN | UNKNOWN |
| Flipping stones | 6 / 9 / 0-13 | 14 | 1 | table | - | - | - |
| Spike crushers | 6 / 10 / 0-2 | 10 | 2 | table | - | - | - |
| Chandeliers / torches | 12 / 6 / 6, 7; 12 / 36 / 0-2 | many | 0-3 | table, kind 6 | - | - | - |
| **General Kael** | 4 / 24 / 0 | 1 | 4 | table | **128** | **10,000** | 10 x 500 + 4 x 1000 + 1 x var 10 gold |
| Kael's sword box | 10 / 13 | per slash | 4 | Kael | - | - | - |
| Parry spark | 12 / 28 | per parry | 4 | Kael | - | - | - |
| **Bavmorda** | 18 / 4 / 0 | 1 | 6 | table | **180** | **1,000,000** | none |
| Her spells | 10 / 16, 21, 22, 23; 12 / 17, 39, 41 | per cast | 6 | Bavmorda | - | - | - |
| Altar brazier | 4 / 28 / 0 | 1 | 6 | table | - | - | - |
| Candelabras | 12 / 40 / 1-4 | 4 | 6 | table | - | - | - |
| Ending smoke | 12 / 38 / 0-27 | 28 | ending | 0xB566 | - | - | - |
| Chests | 16 / 4 | 5 | 0, 2, 3, 5 | table | 6 | - | 8.1 |
| Shop doors | 12 / 8 / 0, 16, 32, 48 | 4 | 1, 3, 5 | table | - | - | - |
| Doors | 14 / 2 / 0-11 | 12 | all | common table | - | - | - |

Energies VERIFIED (RAM +2A in `S/prof.txt`); scores of shared families from
the earlier specs (SOURCE for this stage); Kael and Bavmorda scores VERIFIED
(score 0 -> 10,000 -> 1,010,000 in D4b/D7).

---

## 4. Families shared with stages 1-5: stage-6 differences

### 4.1 Soldiers (class 4 kind 0)
- Placed variant 4 soldiers (areas 0, 1, 2, 5) on ledges and floors: the
  variant-4 placed behaviour of stage4.md 4.1 / stage1_enemies.md 5.1 (energy 1,
  score 50, palette 16). "Change level" is **not** disabled in stage 6 (the
  0x10CA4 test is for index 3 only). SOURCE.
- **Event 0** (variant 0, the stage-1 edge event 0x3834E): camera X 64..544,
  spawn y 80: soldiers run in at the screen edges in the first part of area 0.
  VERIFIED (B: 18 variant-0 soldiers keys 1230..3018; event retired at 3018).

### 4.2 Window crossbowman (class 4 kind 17 variant 4, 0x19E92)
As stage2.md 5.4 (window shutter and crossbowman; energy 1, palette **17**,
arrows class 10 kind 0, sound **0x34**). 22 of them in areas 0, 1 and 5; area
0's appear in the castle wall's windows, area 1's in the corridor windows, the
tower's along the stairs. VERIFIED (B: 21 records; 128 x 0x34 in run S;
renders `vid/v002500`, `v009800`).

### 4.3 Mace knight (class 4 kind 19, 0x1A7F2)
The stage-3 lance knight family (stage3.md 5.3: energy **48**, score 2000, 3 x
500 gold, sounds **0x91** thrust / **0x92** hit, palette **15**). In stage 6
the art is an armoured knight with a ball-and-chain (`vidD2/v006000`):
- Variants **1, 2** (area 2, (912, 1624/1696)): active patrol/attack (nodes
  0x1AEA8, 0x1AF40, 0x1B052 ...); variant 2 took 3 hits in run R.
- Variants **3, 4** (area 3, beside the doors at x 2208) and **6, 7, 8, 9, 10**
  (tower): seen only in the waiting pose (node 0x1AEA8 or 0x1AE14) in the runs:
  they wake when the player comes near (not reached).
- Variant **5** (tower, (3776, 416)) attacked with **class 10 kind 8 variants
  0-7** (8 linked records, palette 15, nodes 0x249B8 / 0x249E0: the flail's
  chain and ball, swung as a chain of segments) in run B. VERIFIED (B keys
  17718..).
Variant-specific behaviour (which variants patrol, which wait, the flail
chain motion): UNKNOWN (variant branches in 0x1A7F2; stage3.md only covers
variant 0).

### 4.4 Axe brute (class 4 kind 14, 0x176F6) and the returning axe (class 10 kind 7)
The green brutes of `vidD2/v001800` and `vidD3b/` are the family stage3.md 4.1 /
stage4.md 4.3 call "spearman" (palette **18**, returning weapon class 10 kind
7, palette 18, sound **0x8B** on the throw).
- Variant 0 (placed, areas 2, 3): energy **4**. VERIFIED.
- Variants **10 and 12** (energy **8**): spawned by the spiked-cart spawners in
  area 3 (5.4): they push the carts. VERIFIED (D2/D3b RAM: hp 8, pal 18; nodes
  0x18618 / 0x185D2).
- Variant **14** (energy 4): spawned by events 7/8 in the tower, respawning.
  VERIFIED (B).
Variant-specific behaviour: UNKNOWN beyond what the renders show (variants
10/12 walk behind a cart, then attack once it is destroyed).

### 4.5 Archer (class 4 kind 3)
Variants 2 (placed), 8 (area 2/route dumps), 10 and 12 (events 1/2, area 0):
all energy 4, palette 17, arrows 0x34. Variants 10/12 appear on the floor of
area 0 at x 1120..1760 at the stage start and wait for the player. SOURCE
(variant branches) / VERIFIED (records).

### 4.6 Shared effects
Bursts class 12 kind 0 variant 5, gold pickups class 14 kind 0 (variants 0, 2,
5, 6, 7, 8, 10), chests class 16 kind 4, wall torches class 12 kind 6 variant 6
(no sprite: they rewrite scroll2 cells, stage2.md 5.8) and **variant 7** (area 2
chandeliers, same mechanism). VERIFIED (births) / SOURCE (variant 7 art).

---

## 5. New families

Pose tables for the packer: **`S/roots.txt`** (roots2.py over `S/full.dis`
with the right box base: class 4 0x3AC4A, class 18 0x3B07A, class 6 0x3B1AA,
class 10/12/14 0x3B0D2), followed by the **observed node pointers per family
and variant** with palettes and dump counts (from every stage-6 RAM dump), for
the families whose roots are set by other loaders (Bavmorda, kind 33, the
knights' and brutes' variants, the doors).

### 5.1 Overview of what was and was not traced naturally
Natural (random input, RAM every 2 keys): areas 0-3, Kael (3200 updates),
Bavmorda (2000 updates), lower tower. Not reached naturally: the upper tower
(y > 350: mortars, bomb troopers, the tower knights' attacks, the tower
chests and shops). Their entries below are SOURCE from code and table data.

### 5.2 Mortar (class 4 kind 33, 0x22A72)
Init (0x22A96): energy **24** (+2A/+2B), +12 = 1, +10 = 5, probe +13 = 5,
facing 1, timer +22 = 40, motion 0x22B1A; root 0x22D88 (nodes 0x22D90 /
0x22E08, palette **5**); spawns a **class 12 kind 37** part at x +-4 (palette
**26**, root 0x36472, box (8,16,8,16)). Stops while FF83D2 (time stop). Its
attack (a lobbed shot every few seconds when the player is within range, 0x22CAC
range test, rand & 7 cadence): SOURCE (partially read) / UNKNOWN (projectile
kind, score, drop). Four in the tower at (3536,1376) (3536,1888) (3632,1632)
(3464,2080). VERIFIED (records, hp 24, palettes).

### 5.3 Tower bomb trooper (class 4 kind 34, 0x22F38)
Energy **24**, palette **5**; uses the stage-3 bomb trooper's tables (stand
0x1DB9A, ground throw 0x1DC72, run 0x1DD4A) plus its own 0x233A6 (4-node loop)
and 0x233AA (2 nodes), box (12,24,24,40). Variants 0-2 at (3648,1344) (3520,1600)
(3648,1856). Behaviour as stage3.md 5.1 / stage4.md 5.3 with tower terrain:
UNKNOWN in detail (not reached). VERIFIED (records) / SOURCE (roots).

### 5.4 Spiked-cart spawner (class 4 kind 27, 0x214FA) and spiked cart (class 6 kind 8 = routine 0x21AE2)
Area 3's hazard: a spiked wall on a wheeled cart pushed toward the player by one
or two green brutes (`vidD3b/v003000`..`v011000`).
- Spawner: no sprite of its own while waiting (node 0); "dir" byte (+0F) =
  activation parameter (64, 96, 128, 160, 192; events 3-6 use 100/192/192/160).
  When the camera reaches it, it spawns a **class 6 kind 8** cart and **class 4
  kind 14 variant 10 / 12** pushers (energy 8). Roots (palette **18**): 0x21700
  (still), 0x21704 (4-node loop), box **(88,176,32,62)** — a 176-px wide
  spiked wall drawn by the spawner itself when active (variant 2, nodes
  0x21708..0x217F2). VERIFIED (D2/D3b records) / SOURCE (timing, cap).
- Cart (class 6 kind 8): energy **30** (hp 30 -> 15 seen as it is hit), palette
  **19**, 4-node rolling loops 0x21D04 (facing 0) / 0x21DA0 (facing 1), plus
  damage stages 0x21D2A..0x21F4A (16 nodes observed: the cart breaks up as its
  energy falls), box (14,24,16,48); contact hurts. VERIFIED (records) / UNKNOWN
  (speed, what happens at 0 energy, score).

### 5.5 Altar brazier (class 4 kind 28, 0x21884)
At (1376, 3152) in Bavmorda's chamber, palette **30**, roots 0x21A70 (still),
0x21A80, 0x21A90 (4 nodes), box (14,28,16,32); sound **0x8F** at 0x218FC (also
heard during the fight: 6 x 0x8F in D7s). Probably the object Bavmorda's
"blow life into objects" magic animates ("WITH HER MAGIC, SHE CAN ALSO BLOW
LIFE INTO OBJECTS", shop message 0x3D). It stayed in its still node in D7 and
was removed with Bavmorda's death. SOURCE / UNKNOWN (behaviour).

### 5.6 Flipping stepping stones (class 6 kind 9, 0x31D04)
Fourteen stone slabs over the area-1 spike strip at x 784..1984 (y 816/848/912).
They do not move; each alternates between a **flat (standable)** node and an
**edge-on** node: roots 0x31FC2, 0x31FCE, 0x31FDA, 0x31FE6 (palette **28**,
box (8,16,48,96)). Phase 0 (the "dir" byte 0 / non-zero picks the start state:
0x31E9C flat or 0x31F04 edge-on); per-variant timers from **0x31D68** (all
0x78 = 120 for the first entries), 0x31D88, 0x31DA8; in run B they flipped
(phases 4/6, 8-update node steps 0x31FE6 -> 0x31FDA -> 0x31FC2 -> 0x31FCE) when
the player stood on or near them (keys 10702..10956). SOURCE (timers) /
VERIFIED (states, nodes) / UNKNOWN (whether they flip on a timer or on
contact; read 0x31D18..0x31FC0).

### 5.7 Spike crushers (class 6 kind 10, 0x31FF4)
Ten spiked stone blocks on chains in area 2 (`vidD2/v001800`, `v003000`):
root **0x32222** (palette **26**, box (16,32,16,32)); each drops from the
ceiling and rises again: y range **1573..1704** or **1573..1752** (variants 0/1,
by the "dir" byte 0/2: a short or a long drop) and **1645..1752** (variant 2).
Contact hurts. Cycle timing: UNKNOWN (read 0x31FF4..0x3222E; the D2 dumps hold
every 2 keys of their motion). VERIFIED (ranges).

### 5.8 Chandeliers and scenery (class 12 kind 36, 0x362F0)
Variants 0, 1, 2 (palette **26**, roots 0x363EE, 0x3640C, 0x3641E): hanging
candle chandeliers and chains in areas 2/3, created by the kind-6 variant-7
animators. Decoration only. SOURCE.

---

## 6. Mid-boss (General Kael) and final boss (Queen Bavmorda)

### 6.1 Kael (class 4 kind 24, 0x1DE3E): record and wake-up
- Area-4 table entry: (304, 3160), facing left. Init (0x1DE6A): energy **128**
  (+2A/+2B), probe 5, motion 0xED080, root table 0x1ECF6 (stand), palette **24**
  (nodes in `S/roots.txt`, box (16,28,32,56) standing). VERIFIED (D4b hp 128).
- Wake when on screen (+1): if **FF838A** is set (already beaten) it skips the
  scene; else music word **0x0E**, FF838A = FF83E6 = FF83D0 = FF83E9 = 1,
  dialogue message **0x0447** style 0x0306 (3 frames/letter, 0x2F per letter):
  **"THE TIME HAS FINALLY COME TO FIGHT.&I HAVE BEEN WAITING.&THE QUEEN WILL
  ELIMINATE THE BABY SOON."** (ROM 0x72E98), then **96 updates** after the text;
  player frozen (FF83EA) and clock stopped meanwhile; then the bar (FF83CC =
  **127**, the bar's maximum) and the fight. Run D4b: 1994..2388 (394 updates).
  VERIFIED (render `vidD4b/v002202`; ROM; flags).
- Arena: area 4, camera fixed (0, 3072); the bridge floor y 3152; Kael ranges
  x ~190..400, leaps to y ~3290. Cut 3 applies to the dialogue.

### 6.2 Kael's fight (state 2, phases)
Phase timeline of run D4b (`S/kael_states.txt`, 195 phase changes over 3200
updates):

| Phase | Behaviour (trace) | Tag |
|---|---|---|
| 14 | walk toward / away from the player (subs 2/4/6: 10, ~60, 30 updates; x changes ~90 px), stand root 0x1ECF6, walk loop 0x1ECFA (4 nodes) | VERIFIED |
| 8 | **leap**: subs 2 (10 updates crouch), 4 (~38 updates rising: y 3160 -> ~3286, +126 px), 6 (8), 8 (30 updates hanging at the top) | VERIFIED |
| 10 | **aerial attack** from the top: subs 2 (26-28), 4 (14: dips to y ~3292), 6 (14-20: falls to ~3253); sound **0x91** | VERIFIED (timing) / SOURCE (sound) |
| 12 | **descending slash**: subs 2..8 (16, 6, 10, 30 updates): moves ~60 px toward the player while landing at y 3160 | VERIFIED |
| 4 | short stand (16 updates) between attacks | VERIFIED |
| 6 | hit reaction / guard (30 + 16 updates); **parry**: two class 12 kind 28 sparks and sound **0x28** in D4b (as Sorsha's clash, stage4.md 6.8) | VERIFIED |
| 16 | 8 occurrences: UNKNOWN (probably a ground thrust) | UNKNOWN |

- **Sword box (class 10 kind 13, 0x25604)**: 26 records in D4b, each **~26
  updates**, at Kael's sword (x +-20..60, y 3160..3242), palette **25**
  (nodes 0x256A4..0x2572A); touch damage. Sound **0xA2** with each slash (12 in
  D4bs). VERIFIED (records, count) / SOURCE (damage).
- Natural damage taken: 11 hits in 3200 updates from random play (about one
  per 290 updates); each hit 1 HP (sword) with ~20-30 updates of invulnerability.
- Decision tables (distance/rank -> phase), leap targets and speeds: UNKNOWN
  (read 0x1DF40..0x1E700 and the tables after 0x1ECF6; the D4b dumps hold every
  2 keys).

### 6.3 Kael's defeat
| Step (D4b keys) | Behaviour | Tag |
|---|---|---|
| 5594 | energy 0 (assist): state 2 phase 6 continues 16 updates, then **state 4** | VERIFIED |
| 5610..5670 | state 4 phase 6 (60): **flashes white** (render `vidD4b/v005622`), sound **0x95** (0x1E6F0) | VERIFIED |
| 5670..5730 | phase 8 (60): collapses (roots 0x1ED22 / 0x1ED26, box (18,36,20,32)) | VERIFIED |
| 5730..5910 | phase 10 (180): lies on the bridge; at 5730 the **reward**: score **+10,000** and the gold shower **10 x variant 7 (500) + 4 x variant 8 (1000) + 1 x variant 10** pickups at his position (`vidD4b/v005802`) | VERIFIED (score; R and D4b births) |
| 5910 | state 6, removed; **FF83E7 = 1**, FF83D0 = FF83E6 = 0 | VERIFIED |
| 5910..6080 | the area-4 door (variant 10) freezes the player, then the **Bavmorda picture** with message 0x48 (2.5; 256 updates without input, fast-forwarded in D4b by random buttons), then area 5 as **Willow** (form 0) at (3392, 48), music 0x0D, clock reload 3:00 | VERIFIED |

### 6.4 Bavmorda (class 18 kind 4, 0x29972): record and wake-up
- Area-6 table entry: (1216, 3160), facing left. Init (0x299B0): +0 = 2
  (intangible), probe 3, **energy 180** (+2A/+2B, FF83CC/FF83CD), FF83F4 = 0,
  FF83F5 = 0, motion **0xED068**, attack order from 0x29A10[rand & 3]
  (byte strings `04 03 02 FF`, `01 04 02 03 FF`, `03 02 04 01 FF`, `02 03 01
  04 FF`: the four attack sequences she cycles through), root table **0x2A6DA**
  (4-node idle loop, palette **29**, box **(16,32,32,52)**). VERIFIED (D7 hp
  180, node 0x2A76E) / SOURCE (sequence meaning).
- Wake (0x29A2C, on the first update with +1): FF83CE = 1; once (FF838B):
  FF838B = 1, **FF83A2 = 1** (the final-boss flag that turns the clear into the
  ending, correction 3), music **0x0F**, FF83E9 = 1, dialogue **0x0449** style
  0x0306: **"IT LOOKS LIKE YOU MADE IT IN TIME.&DO YOU THINK YOU CAN DESTROY
  ME WITH&THAT MAGIC TOY OF YOURS."** (ROM 0x72FE8), then **96 updates** after
  the text (player frozen, clock stopped), then the bar (4AC6) and the fight
  (state 2, timer 30). D7: 3274..3670 (396 updates); a sound **0x8F**
  (thunder/laugh, posted by the brazier 0x218FC or 1058) during the text.
  VERIFIED (render `vidD7/v003450`; ROM; RS order 0x2F x32, 0x8F, 0x2F x62).
- Arena: area 6, camera fixed (1024, 3072), floor y 3160 (she stands at y 3160
  and flies up to ~3270). The far layer sways (2.1). Cut 3 applies to the text.

### 6.5 Bavmorda's fight (state 2)
Phase timeline of run D7 (`S/bav_states.txt`, 2000 updates of random play by
Willow, 9 hits):

| Phase | Behaviour (trace) | Spawns / sounds | Tag |
|---|---|---|---|
| 0 | 30 updates idle after the dialogue | - | VERIFIED |
| 2 | **walk / glide** along the floor toward or away from the player (subs 2/4: 24..140 updates, ~1 px/update; x 1080..1345) | - | VERIFIED |
| 4 | **star burst**: sub 0 spawns a class 12 **kind 39** casting glow (60 updates, palette 0/8, nodes 0x261CA/0x261E2) and **8 x class 10 kind 16** sparks at her hand (palette **8**, root 0x261F0, 4-node loop, box (4,8,4,8)/(5,10,5,10)), which fly out and **home in**, living 250..420 updates; subs 2/4 ~70..130 updates | **0xA6** cast (0x29F2E / 0x2A05A), **0x37** spark x3 | VERIFIED (births 3822, 4..8 sparks) |
| 6 | **rise / fly**: from the floor up to y ~3260 and across the room (x 1345 -> 1315 -> 1179 -> 1088), 50..80 updates per move | - | VERIFIED |
| 8 | **energy ball** from the air: sub 0 class 12 **kind 17 variant 1** charge glow (80 updates, palette 27, nodes 0x36C2A..0x36C64), sub 2/4 (20 + 40 updates) a class 10 **kind 21** ball (palette 8/13, root 0x26224, box (10,20,10,20); 8-node burst root 0x2624C, palette 13, box (14,28,8,30)) thrown at the player, ~80..90 updates life | **0xA9** (0x29E54) | VERIFIED |
| 10 | **lightning stream**: sub 0 class 12 **kind 41** (palette 8, 4 nodes 0x365E8.., 80 updates) then **9 x class 10 kind 22** (variant 0 head, 1, 2 x7: a chain of bolt segments every 2-4 updates, palette 8, roots 0x26214 / 0x26330 / 0x2633C, box (6,12,8,16)), 20..200 updates; the grey screen flash (2.6) | (flash) | VERIFIED |
| (any) | **candelabras come alive**: a candelabra (class 12 kind 40) is removed and **3 x class 10 kind 23 variant 1** flames appear at its place (x -32, 0, +32; y 3143): walking fires (palette **6**, 8-node loop 0x26656 -> 0x2666E, end 0x266BA, box (6,12,12,14)), ~250 updates (D7: candelabra var 4 at 4586, var 1 at 4824) | - | VERIFIED |

- Decision: the attack order table 0x29A10 (above), the per-rank delay table
  **0x29AD8** (rank byte & 0xE0 >> 3 + rand & 3 -> +33) and a random wait table
  **0x29B16** (rand & 15 -> 320, 640, 960 updates) drive which phase comes next.
  SOURCE (tables) / UNKNOWN (exact mapping of sequence codes 1-4 to phases
  4/6/8/10).
- She is hit by Willow's magic / sword shots normally (1 HP each in D7: 180 ->
  171 over 9 hits); invulnerability per hit and her hit flash: palette job
  **0xE6E60** -> object palette **29** (22 steps x 3). VERIFIED.
- Contact damage from her body, the sparks, the ball, the bolts and the flames:
  SOURCE (all class-10 records are enemy shots).

### 6.6 Bavmorda's defeat (state 4) and her whirlwind
| Step | Behaviour | Tag |
|---|---|---|
| energy 0 (0x2A4DC) | sound **0x20FB** (music fade), palette fade job 0x181A, **FF83C3 = 2** (stage clear), FF83E9 = 1, **FF83CF = 1**, +0 = 2, FF83CC = 0 (bar cleared), score **+1,000,000** (D7: 10,000 -> 1,010,000) | VERIFIED (D7 key 5672; R 3670) |
| same update | the play mode sees FF83C3 = 2 -> clear mode (FF8004 = 4) step 0 -> 4 -> **20** (FF83A2 set): **the ending starts at once** (key 5674); actors keep running in the ending (0x9EF2) | VERIFIED |
| state 4 phases 0-2 (0x2A518..) | her last pose: x/y target (1376, 3152) = the altar, phase 12, +31 = 0xFF, +68 = 1, +69 = 9; palette job 0xE6E60 (23 steps) -> she fades into the altar | SOURCE / VERIFIED (render `vidR/v003744`: a blue whirlwind beside the altar, she is gone) |
| whirlwind (0x2A596..0x2A6D6) | root **0x2AC04** (6 nodes, loop -> 0x2AC58, palettes 8/29, box (40,136,0,96)): sound **0x95**; speed +46 = **384** max, +48 = **4** accel/update, +4A = **16** rise; it sweeps left and right across the chamber (phases 2..8: accelerate, decelerate, reverse), each pass faster (+384 max, +4 accel, +16 rise); first reversal: **FF83F6 = 1** (the ending's staff roll starts); final pass: **FF83F6 = 2** (it leaves upward) | VERIFIED (R: FF83F6 1 at 4158, 2 at 4742; renders `vidR/v003900`..`v004500`) / SOURCE (constants) |
| | the whirlwind record is removed at ending sub-step 6 (R key 4742; D7 6356) | VERIFIED |

There is no results screen, no "STAGE CLEAR" text, no clear bonus and no story
page after Bavmorda: see 7.

---

## 7. The ending (clear mode step 20, 0xB11A)

### 7.1 Entry and rules
- Reached only through FF83A2 (set when Bavmorda wakes) and FF83C3 = 2 (her
  defeat): clear step 4 (0xA682) jumps to **step 20**. Every update of step 20
  clears FF799C first (0xB11A), so **buttons do not fast-forward the ending**;
  it has no input at all (D7 ran it under random input with the same
  structure; its timing differs only where the whirlwind's passes differ).
  VERIFIED (R with no input 3672..9824 = 6152 updates; D7 random 5674..11436
  = 5762).
- Sub-steps (FF800C), routine table 0xB12A (20 entries):

| Sub | Routine | R key | D7 key | What happens | Tag |
|---|---|---|---|---|---|
| 0 | 0xB152 | 3672 | 5674 | palette fade job **0x0C14** speed 4 (scene darkens), FF7891 = 1; actors and draw keep running | VERIFIED |
| 2 | 0xB174 | 3674 | 5676 | wait for the fade; init the roll (FF0E88 line = 0, FF0E90 = 0, FF0E8A portrait = 0, scroll1 x/y 0, FF0E8C = 0, FF0E89 = 8), 0xAF4, fade-in job **0x1014** speed 4 | VERIFIED |
| 4 | 0xB1B8 | 3736 | 5736 | the **whirlwind** plays in the chamber (6.6); when FF83F6 = 1 the **staff roll** (7.3) starts scrolling over the scene (R 4158) | VERIFIED |
| 6 | 0xB210 | 4742 | 6356 | FF83F6 = 2: scroll1 rows cleared (0xB3FA: both scroll1 pages filled with space 0x7020), fade jobs **0x0C12, 0x0C16, 0x0C18** speed 16 (the chamber fades out); 0xBC4 each update until FF83F7 > 2; roll continues | VERIFIED |
| 8 | 0xB248 | 4984, 5398, 5812, 6226, 6640, 7054 | 6598, 7012, 7426, 7840, 8254, 8668 | **portrait n** (FF0E8A = n, 0..5): palette set 1 (FF8343), scroll2 grid 7 at **0xAC62[n]**, fade-in **0x1016** speed 4, **352 updates** | VERIFIED |
| 10 | 0xB268 | 5336, 5750, 6164, 6578, 6992, 7406 | 6950, ... 9020 | fade-out **0x0C16** speed 4 (~62 updates), n += 1; after n = 5: FF0E90 bit 0 | VERIFIED |
| 12 | 0xB48E | 7468 | 9082 | both roll and portraits done (FF0E90 = 3): 15 more updates of scroll, then fade | VERIFIED |
| 14 | 0xB4B2 | 7484 | 9096 | palette set 0, 0x2648; **castle scene** set-up: scroll2 grid 7 at **(512, 256)**, scroll3 grid **0x2EB24[7] = 0x2F24C** at **(1536, 0)**, timer 31 | VERIFIED |
| 16 | 0xB566 | 7540 | 9152 | after 31: FF8411 = 0, FF8424 = 3, timer 319; spawns the **smoke** controller class 12 **kind 38** (y 256) and two palette jobs (0xE7C00 -> palette 17, 12 steps x 24 updates; 0xEC0E0 -> palette 12, 12 x 24): **the sunset sky brightens to day** | VERIFIED (RAM jobs) / SOURCE (targets) |
| 18 / 20 | 0xB604 / 0xB608 | 7622 | 9236 | 319 updates: the **castle sinks**: scroll2 Y += **0.5 px/update** (camera Y 257 -> 415, 158 px) with a horizontal **shake** (+-0.5 px, direction flips every 3 updates); class 12 kind 38 variants 0-27 = **28 smoke clouds** (palette **23**, roots 0x3685E.., 7-node puffs) billow up from the castle's base | VERIFIED (renders `vidR/v007680`, `v007800`, `v007878`) |
| 22 | 0xB690 | 7940 | 9554 | sound **0x86**; FF8E58 = 1; message **0x4A** style 0x0329 typed: **"WILLOW AND MADMARTIGAN FOUGHT BRAVELY TO&DEFEAT THE EVIL QUEEN, BAVMORDA."** (ROM 0x73090; the first line is exactly 40 characters and wraps); then **200** updates | VERIFIED (render `vidR/v007950` "WILL...") |
| 24 | 0xB6DC | 8356 | 9970 | sound **0x87**, palette jobs 6, 8 (7D80); message **0x4B** style 0x032A: **"DARKNESS HAS GIVEN WAY TO A WORLD OF&PEACE AND HAPPINESS."** (ROM 0x73138); then 63 updates | VERIFIED (render `vidR/v008592`) |
| 26 | 0xB710 | 8588 | 10200 | message **0x4C** style 0x032B (a single space: clears the text rows); 63 updates | VERIFIED |
| 28 | 0xB75E | 8654 | 10266 | sound **0x88**, palette jobs 6, 8, 10; message **0x7C** style 0x1325 (84-byte record 0x728B0): **"A GREAT MAGICIAN."** | VERIFIED (render `vidR/v008712`) |
| 30 | 0xB786 | 8706 | 10318 | message **0x7D** style 0x1326: **"WILLOW UFGOOD"** (same row, red name colour) | VERIFIED (render `vidR/v008754`) |
| 32 | 0xB7AE | 8746 | 10358 | message **0x7E** style 0x1327: **"A BRAVE WARRIOR."** (next row) | VERIFIED |
| 34 | 0xB7DC | 8794 | 10408 | message **0x7F** style 0x1328: **"MADMARTIGAN"**; then **351** updates | VERIFIED (render `vidR/v008802`) |
| 36 | 0xB828 | 9178 | 10792 | sound **0x89**, palette jobs 6, 10; message **0x4D** style 0x032C: **"THEIR NAMES WILL LIVE ON FOREVER."** (ROM 0x73288); then **256** updates | VERIFIED (render `vidR/v009540`) |
| 38 | 0xB89C | 9534 | 11146 | scroll1 Y = 256, fade job **0x0016** speed 256, **the closing card**: scroll3 grid 7 at **(0, 512)** (a portrait of **Willow**) and the scroll1 logo block 0xB8D8: tile codes 0x7180..0x71F9 in six 9-11-tile columns at scroll1 0x910D40.. (palettes 0x14 / 0x13): **"PRESENTED BY / CAPCOM / LUCASFILM Ltd."**; **127 updates**; then sound **0x10FB** (music fade), fade-out | VERIFIED (renders `vidR/v009600`..`v009816`) |
| end | 0xB8A4 | 9824 | 11436 | **FF8342 (lives) = 1, FF8388 = 1** (game finished), FF8004 = **6** (the life-loss path), FF8008 = FF800C = 0, 0x2648 | VERIFIED |

The text rows: messages with style 0x03xx use the 168-byte story records
(0x70000 + m * 168, ASCII, `&` = line break, 40 columns, 3 frames per
character, destination 0x29..0x2C = the upper text rows: `vidR/v008364`: row
y ~16 from the top); styles 0x13xx use the 84-byte records (0x70000 + m * 84)
in the big-font colour pair (first part purple, names red). Each typed
character posts no 0x2F here (no letter sounds were logged in the ending).
VERIFIED (sound logs: no 0x2F between 0x86 and 0x89).

### 7.2 The whirlwind and FF83F6
FF83F6 is written only by Bavmorda's whirlwind (0x2A6BC: 1, 0x2A692: 2). Native:
the ending's sub-step 4 waits for the whirlwind's first reversal before the
roll starts, and sub-step 6 waits for its exit. An injected clear (harness
ENDAT) never sets it and stops the ending at sub 4 forever (runs C/CS). R: 1 at
key 4158 (422 updates after sub 4 began), 2 at 4742; D7: sub 6 at 6356 (620
after sub 4). VERIFIED.

### 7.3 Staff roll (credits)
- Text: **74 lines x 16 characters** at **0xAC7A** (ASCII; tile = 0x7000 +
  character), palette per line from **0xB3AA** (palette **3** for the five
  headings, **0** for the names). VERIFIED (ROM; renders `vidR/v004992`..`v007062`).
- Scroll: scroll1 is scrolled up by **0.25 px per update** (FF0E8C -= 0x4000 per
  update; scroll1 Y = its high word & 511); whenever bit 3 of the scroll changes
  (every 32 updates) the next line is written into the scroll1 row just below
  the screen (0xB2D4: column 0..15 of the left half, rows from 0x910000 by the
  scroll position) and the row 16 rows below is cleared to spaces; after line
  73 (FF0E88 > 0x49) FF0E90 bit 1 is set. 74 lines x 32 updates = **2368
  updates** (R: line 8 at 4384, 16 at 4640 ... 74 at 6496). The credits stay on
  the left part of the screen (16 columns, x 0..127 of the scroll1 page; the
  portraits are drawn on the right). VERIFIED.
- The full text (blank lines shown as `-`; heading lines in palette 3):

  ```
  0-4    -
  5      "    PLANNING    "   (heading)
  6-7    -
  8      "  KAWANORIDER   "
  9      -
  10     " DERU DERU ITOH "
  11-13  -
  14     "   PROGRAMING   "   (heading; spelled with one M)
  15-16  -
  17     " PROFESSOR ARAI "
  18     -
  19     "  QUEEN TOMITA  "
  20     -
  21     "    YOKOYAN     "
  22     -
  23     "     UEYAN      "
  24-26  -
  27     "CHARACTER DESIGN"   (heading)
  28-29  -
  30     "  FRUNKY KAZU   "
  31     -
  32     "BAKUHATSU YOKOZO"
  33     -
  34     "    TERUKUN     "
  35     -
  36     "   HIRAMACHO    "
  37     -
  38     "    KURIBOW     "
  39     -
  40     " PUTTSUN MIDORI "
  41     -
  42     "   SADAKICHI    "
  43     -
  44     "      OYUU      "
  45     -
  46     "     RINMA      "
  47     -
  48     "    OKACHAN     "
  49-51  -
  52     " MUSIC COMPOSE  "   (heading)
  53-54  -
  55     "  OGERETU KUN   "
  56-58  -
  59     "  HARD DESIGN   "   (heading)
  60-61  -
  62     "    KUCCHAN     "
  63-65  -
  66     "   DIRECTION    "   (heading)
  67-68  -
  69     " KIHAJI OKAMOTO "
  70-72  -
  73     "  PRESENTED BY  "   (heading)
  ```
  VERIFIED (ROM 0xAC7A dump; renders). (Line 73 scrolls in last; the closing
  card repeats "PRESENTED BY" with the logos, 7.1 sub 38.)
- **Portraits** behind the roll (sub-steps 8/10): scroll2 **grid 7**
  (0x2DBF8[7] = 0x2EA24, the story-picture grid) at 0xAC62[n]:

  | n | Position | Picture (render) | Tag |
  |---|---|---|---|
  | 0 | (0, 256) | Madmartigan, smiling, with his sword (`vidR/v004992`) | VERIFIED |
  | 1 | (1024, 256) | Fin Raziel, the old sorceress with her staff (`v005406`) | VERIFIED |
  | 2 | (1536, 256) | the scarred grey-bearded warrior with a shield (the stage-4 story picture) (`v005820`) | VERIFIED |
  | 3 | (2560, 256) | General Kael in his skull helmet (`v006234`) | VERIFIED |
  | 4 | (2048, 256) | Queen Bavmorda (`v006648`; also the area-4 door picture) | VERIFIED |
  | 5 | (2944, 256) | the baby Elora (`v007062`) | VERIFIED |

  Each: fade-in (4), 352 updates shown, fade-out (~62): ~414 updates per
  portrait; the first starts when sub 6 ends (R 4984), the last ends at 7468.
  The roll and the portraits run in parallel; the ending continues when both
  are done (FF0E90 = 3). VERIFIED.

### 7.4 Music and sounds of the ending
Order (runs RS and D7s; keys from the sub-step starts): **0x20FB** music fade
and **0x95** at Bavmorda's defeat; **0x1D + 0x01FA** the ending music (during
sub 4, while the whirlwind sweeps; RS col 3066 ~ key 3830); **0x86** (sub 22),
**0x87** (sub 24), **0x88** (sub 28), **0x89** (sub 36): one chime each as the
closing texts appear; **0x10FB** at the end of the closing card. D7s also logs
**0x3B** x3 at the music start (the whirlwind?). No 0x2F letter sounds.
VERIFIED (order and counts) / UNKNOWN (who posts 0x1D; 0x3B's owner).

### 7.5 After the ending
Run R (no input), keys:

| Key | Mode (FF8000 / 8004 / 8008) | Event | Tag |
|---|---|---|---|
| 9824 | game task 4 | the ending set FF8004 = 6 with lives = 1: the life-loss path (0x96BE) subtracts a life -> 0 -> FF8000 = 4: **score qualification** (arcade_frontend.md 9) | VERIFIED |
| 9826..10570 | 6 / 2..6 | **name entry** (arcade_frontend.md 10): sounds 0xF0 0xF7, music **0x18**; "PLAYER 1 / 1010000 PTS / PUT YOUR NAME 20" (`vidR/v009834`, `v010200`); then the ranking (music **0x16**) | VERIFIED |
| 10626..10854 | 8 / 8 / 4..6 | the continue/game-over task (0x9748): **FF8388 set -> no continue offer and no game-over music 0x14**; "GAME OVER" (`vidR/v010632`) for **120** updates | VERIFIED (render; code 0x976A, 0x990A) |
| 10854 | 0 | back to the **attract mode** (opening, demo, rankings) | VERIFIED |

- High score: the ending does not award any clear bonus; the score is the play
  score (R 1,010,000 = Kael 10,000 + Bavmorda 1,000,000 with every other
  enemy skipped by the route). The HI display follows the score as usual.
  VERIFIED.
- **One-credit clear**: the arcade has no special handling: no extra screen,
  no flag outside FF8388 (which only marks "game finished"), the same name
  entry and GAME OVER whatever the number of continues used. The Home-mode
  **New Game Plus unlock** (AGENTS.md 2026-10-21: a Home clear on one credit) is
  a native addition hooked on this point (the moment the ending sets FF8388),
  with the continue count tracked by the native front end. VERIFIED (code
  0x96BE..0x9790 and 0x98EE..0x9920 read: only FF8342, FF8388, FF8287 are
  tested).
- What the second player sees (2P game): UNKNOWN (only 1P traced; the ending
  sets FF8342 for the current player only).

### 7.6 Native notes for the ending
- A scripted scene: no gameplay update except the whirlwind and the smoke
  actors; three backgrounds (the chamber with the whirlwind; the six portraits
  on plane B or A with the text roll on the other plane; the castle scene with
  the sinking plane, the smoke sprites and the sky palette fades; the closing
  card with Willow's portrait and the logos).
- The roll needs a vertically scrolling text plane (0.25 px/update) beside a
  static portrait: on the Genesis, the window plane or plane A with per-column
  limits cannot scroll; use plane A for the roll (only the left 128 px carry
  text) and plane B for the portrait at fixed scroll. Portraits are 6 story
  pictures from grid 7 (already used by the story pages: storypack2.py).
- The castle scene's horizontal shake (+-0.5 px every 3 updates) and 0.5
  px/update vertical motion are whole-plane scrolls (no line scroll needed).
- Cut 3 applies to the closing texts (they already use their own rows over
  plain sky).

---

## 8. Shops, chests, hidden objects

### 8.1 Chests (class 16 kind 4, energy 6)
Rewards **0x37EA6 + 0xD4 = 0x37F7A**: 4 sets (FF83A4) x 5 chests (index = the
table "dir" byte) x (kind, variant); kind 5 = gold of that variant (0 = 1, 5 =
100, 6 = 200), kind 6 = heart. VERIFIED (ROM; code 0x37B02).

| Set | chest 0 (1376,160) area 0 | 1 (592,1680) area 2 | 2 (2288,2384) area 3 | 3 (3344,576) area 5 | 4 (3344,1168) area 5 |
|---|---|---|---|---|---|
| 0 | gold v0 (1) | gold v5 (100) | gold v6 (200) | gold v0 | heart |
| 1 | gold v5 | gold v6 | gold v0 | heart | gold v0 |
| 2 | gold v6 | gold v0 | heart | gold v0 | gold v5 |
| 3 | gold v0 | heart | gold v0 | gold v5 | gold v6 |

Chest 0 opened in run B (energy 6 -> 4 seen); chests 3/4 in the tower were not
reached. VERIFIED (records).

### 8.2 Shop doors (class 12 kind 8, palette 12, node 0x32C9C)
| Variant | dir | Position | Area | Tag |
|---|---|---|---|---|
| 32 | 16 | (736, 848) | 1 (corridor door before the spikes) | VERIFIED (B entered it: keys 10278..10670, play steps 4/6/8, layer 0x13B8, 0x10FB / 0x1B + 0x02FA / exit) |
| 0 | 13 | (32, 2392) | 3 (the start of area 3; a small blue-robed shopkeeper figure stands there, `vidD3b/v001900`) | VERIFIED (record) |
| 16 | 14 | (3808, 520) | 5 (tower) | SOURCE |
| 48 | 17 | (3488, 2152) | 5 (top of the tower, beside the door to Bavmorda) | SOURCE |

The "dir" byte selects the shop's stock/info set (stage1_shops_companion.md).
Stage-6 information messages: **0x3C "THE CORRIDOR IS A MAZE./ LOOK FOR THE
TRUE DOOR./"** and **0x3D "BEWARE OF BAVMORDA'S MAGIC./ WITH HER MAGIC,SHE CAN
ALSO BLOW LIFE&INTO OBJECTS./"** (84-byte records 0x72760 / 0x72808). Which
shop sells what, and which info message each shop gives: UNKNOWN (shop spec
tables by dir 13, 14, 16, 17). VERIFIED (strings) / UNKNOWN (mapping).

### 8.3 Hidden objects
None: no class 16 kind 9 in any stage-6 table. The companion (brownie) items do
not appear. VERIFIED (ROM tables).

---

## 9. Sounds in stage 6 (`jsr 0x786A`; runs S, RS, D4bs, D7s, AS2)

| Word | Where |
|---|---|
| 0xF0, 0xF7, 0x12 / 0xF0, 0xF7 | world-map scene start / end |
| 0x0C + 0x01FA | stage music, areas 0-3 (stage start, every corridor door) |
| 0x0E + 0x01FA | area 4 / Kael (door and Kael's wake-up: 2x) |
| 0x0D + 0x01FA | area 5 (tower) |
| 0x0F + 0x01FA | area 6 / Bavmorda (2x) |
| 0x1D + 0x01FA | ending music |
| 0x10FB | music fade: every door, the shop, the end of the ending |
| 0x20FB | music fade at Bavmorda's defeat |
| 0x2F | title (21), Kael's line (94), Bavmorda's line (32 + 62), the area-4 door picture text (89), shop texts |
| 0x1B + 0x02FA | shop music |
| 0x34 | arrows (crossbowmen, archers; 128 in run S) |
| 0x8B | returning axe throw (brutes) |
| 0x91 | knight thrust (stage3.md; 22 in run S) and Kael's aerial attack (0x1E842); the knight hit sound 0x92 was not logged |
| 0xA2 | Kael's slash (0x1E8D2; 12 in D4bs) |
| 0x28 | Kael's parry clash |
| 0x95 | Kael's death (0x1E6F0); Bavmorda's whirlwind (0x2A5B2) |
| 0x8F | the altar brazier / thunder during Bavmorda's dialogue and fight (6 in D7s) |
| 0xA6 | Bavmorda casts the star burst (2) |
| 0xA9 | Bavmorda's energy ball (2) |
| 0x37 | her sparks (7) |
| 0x86, 0x87, 0x88, 0x89 | ending text chimes (7.1) |
| 0x18 / 0x16 | name entry / ranking music after the ending |
| 0x32 / 0x33 | hit / death burst |
| 0x2D | pickup |
| 0x43 | clock warning under 0:16: **not logged** although run B's clock ran down to 0:00 in area 0 (UNKNOWN: another call path than 786A, or no warning in this stage) |

Player sounds also present: Madmartigan 0x26 sword, 0x2A, 0x2B, 0x31, 0x3B,
0xF4 (madmartigan.md 6); Willow 0x20 magic shot, 0x44 charge, 0x21, 0x3E,
0x8D, 0x81, 0x90 (player.md, magic.md; RS/D7s after the area-5 entry).
VERIFIED (sound word logs) / SOURCE (owners not read here: 0x3B, 0x8F's
second owner 0x1058).

---

## 10. Dump files for the pose / plane / far-layer / ending packers

All under `S/` = `/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage6/`.
`.cps` = VIDDUMP (0x80 bytes CPS-A/B registers + video RAM, live palettes);
render with `S/render.sh DIR KEY...` or `S/montage.py OUT.png DIR KEY...` (into
`S/png/`; montages already rendered: `h1` hand-over, `a0`-`a3` areas 0-3, `k1`,
`k2` Kael, `t1` tower, `f1`, `f2` Bavmorda, `e1`-`e6` the ending). Vid keys:
`vid/`, `vidD2/`, `vidD3b/`, `vidD6/` every 10; `vidR/`, `vidD4b/`, `vidD7/`
every 6; `vidA2/` every 6 from 3000.

Scenes, planes and far-layer pictures:

| File | Shows |
|---|---|
| `vidA2/v004104.cps` | world map for stage 6 (marker VI, Willow + Madmartigan at the castle) |
| `vidA2/v004302.cps` | title STAGE6 / NOCKMAA(R CASTLE) typing over area 0 |
| `vid/v001000.cps` | area 0 start: ledge, drawbridge, **storm far layer (far blocks 80-85)** |
| `vid/v002500.cps`, `v004000.cps`, `v005500.cps`, `v008400.cps` | area 0: castle wall with windows, corridors, chest, exit door |
| `vid/v009000.cps`, `v009800.cps`, `v011000.cps`, `v014000.cps` | area 1: windows, spike strip and stones, the two end doors |
| `vid/v010300.cps`..`v010600.cps` | shop interior (area-1 shop) |
| `vidD2/v001800.cps`, `v003000.cps`, `v006000.cps`, `v009100.cps` | area 2: crushers, knights, staircase, the three end doors |
| `vidD3b/v001900.cps`, `v005000.cps`, `v011000.cps` | area 3: shop figure, spiked carts with brutes |
| `vidD4b/v002004.cps`, `v002202.cps` | area 4: Kael's bridge (**far blocks 102/103**) and his dialogue |
| `vidD4b/v006036.cps` | the Bavmorda story picture with message 0x48 |
| `vidD6/` (3200..13000), `vidR/v003120`..`v003260` | area 5, lower tower; R crosses the top |
| `vidD7/v003276.cps` | area 6 fading in (Bavmorda, candelabras) |
| `vidD7/v003450.cps` | Bavmorda's dialogue; the swaying far layer (blocks 106/107) |
| `vidR/v003678.cps`..`v004746.cps` | ending: Bavmorda's defeat flash, whirlwind, grey flash, roll starting |
| `vidR/v004992.cps`, `v005406`, `v005820`, `v006234`, `v006648`, `v007062` | the six portraits with the roll (n = 0..5) |
| `vidR/v007542.cps`, `v007632.cps` | castle at sunset (scroll2 grid 7 at (512,256), scroll3 0x2F24C at (1536,0)) |
| `vidR/v007680.cps`, `v007800.cps`, `v007878.cps` | castle sinking into smoke, sky brightening |
| `vidR/v007950.cps`, `v008364`, `v008592`, `v008712`, `v008754`, `v008802`, `v009540` | the closing texts (7.1) |
| `vidR/v009600.cps`, `v009720.cps` | the closing card (Willow portrait, PRESENTED BY / CAPCOM / LUCASFILM Ltd.) |
| `vidR/v009834.cps`, `v010200.cps`, `v010632.cps` | name entry after the ending; GAME OVER |

Families (pose packer; key in the run's dump folder):

| Run / keys | Shows (palette) |
|---|---|
| `vid` 1000..3000 | edge soldiers (16), Madmartigan |
| `vid` 2500, 9000..14000 | window crossbowmen (17) and arrows (17) |
| `vid` 846..5600 | archers var 10/12 (17) in area 0 |
| `vidD2` 1800..3000 | axe brutes var 0 (18), returning axes (18) |
| `vidD2` 1800..9000 | spike crushers (26), chandeliers (26) |
| `vidD2` 4500..6000 | mace knights var 1/2 (15) |
| `vidD3b` 2000..11000, `vidD2` 9246..12000 | spiked carts (19), pusher brutes var 10/12 (18), cart walls (18) |
| `vid` 8730..15000 | flipping stones (28) |
| `vid` 17718.. (B, tower) | knight var 5 flail chain class 10 kind 8 (15) |
| `vidD4b` 2390..5590 | Kael (24): walk, leap 2672..2758, air attack, descending slash 2980..3042, sword boxes (25), parry sparks 4424 |
| `vidD4b` 5610..5910 | Kael's flash, collapse, gold shower |
| `vidD7` 3700..5670 | Bavmorda (29): glide, stars 3822..4244 (kind 39 glow, kind 16 sparks, 8), lightning 4272..4356 and 4684..4784 (kind 41, kind 22, 8), energy ball 4444..4556 and 4576..4674 (kind 17 glow 27, kind 21 ball 8/13), candelabra flames 4584..4836 (kind 23, 6) |
| `vidR` 3680..4746 | whirlwind (8/29) |
| `vidR` 7626..7900 | smoke clouds class 12 kind 38 (23) |

Far-layer pictures for the far-layer packer: all far blocks of 2.1 are visible
in the area dumps listed (areas 0-4, 6) and in `vidD6/` (tower, far blocks
110-118); the palette controllers (2.6) change scroll3 palettes 27-29: take
several dumps across a 60-update window for the cycle frames.

---

## 11. Open questions (UNKNOWN), where to look

Priority for the native stage:
1. **Upper tower (area 5, y 350..2300)**: mortars (kind 33) and their shots,
   tower bomb troopers (kind 34), knights 6-10, tower chests and both tower
   shops: none was reached naturally. Next step: a private-harness run that
   teleports Willow **only to standable checkpoint positions** (the D6b
   teleports landed inside walls) or a keyed route up the stairs.
2. **Boss decision tables**: Kael (0x1DF40..0x1E700; phase 16; the leap and
   slash geometry) and Bavmorda (sequence codes 0x29A10 -> phases; tables
   0x29AD8, 0x29B16; the brazier's role). Longer natural fights (GATEASSIST
   larger, Madmartigan/Willow scripted keys) would give the phase statistics.
3. **Restarts**: a death in each area (where does the player restart: same
   area checkpoint (1.7) or area 0?), a time-out without the invincibility DIP,
   and whether FF838A/FF838B make Kael/Bavmorda skip their dialogue after a
   restart (code says yes).
4. False doors: FF8384 persistence (per credit? per game?), and the exact
   door-trigger rectangle and Up rule; run a test through doors 2, 4-6, 8, 9.
5. Palette controller types 2/3 (2.6) and Bavmorda's grey flash mechanism.
6. Flipping stones' trigger rule (timer vs contact) and crusher cycle timing
   (0x31D04.., 0x31FF4..).
7. Shops: stock per dir (13, 14, 16, 17) and which shop gives info 0x3C/0x3D.
8. Gold variant 10's value (Kael's reward), the spiked cart's score/drop, the
   mortar/bomb trooper scores.
9. The ending music owner (0x1D) and 0x3B in the ending; the 2-player ending.
10. Willow's state on the area-5 entry: magic level, bracelets, sword items
    from Madmartigan's part (the player record reset at 0x2D92E).

---

## 12. The player: what stage 6 adds beyond madmartigan.md / player.md

1. **Form switch inside a stage**: areas 0-4 Madmartigan, areas 5-6 Willow
   (correction 2). The switch happens during the area-4 door scene (the player
   is frozen and the screen shows the Bavmorda picture); the native player
   object must reinitialise its form, pose (0xD14A), facing and state (1.6)
   while keeping HP, gold and score. The HUD form indicator changes with it.
   VERIFIED.
2. **Doors**: Up (without Left/Right) while grounded within x +-12 / y +-14 of
   a door: a 30-update freeze, then the area change (2.5). New input semantics
   for Madmartigan (Up normally climbs, madmartigan.md 2.8) and for Willow
   (area 5's door). VERIFIED (route runs) / SOURCE (rectangle).
3. Freezes and clock stops: FF83E9/FF83EA during both boss dialogues (96
   updates after the text), doors, the Bavmorda picture, and from Bavmorda's
   defeat to the end of the game. VERIFIED.
4. Terrain: **hazard tiles (id 39)** in area 1 (combat.md 2.6), thin ledges
   (37) for Madmartigan's drop/climb (areas 0, 2, 3) and for Willow on the tower
   stairs (area 5), stairs/slopes 21/22/44/45 in the tower. SOURCE.
5. Sword vs a parrying boss again (Kael, as Sorsha in stage 4). VERIFIED (two
   parry sparks).
6. Willow vs Bavmorda: her hits are counted 1 HP per shot (9 hits = 9 HP in
   D7), so charged magic damage values matter for the fight length
   (magic.md). SOURCE.
7. After the ending the player's lives are forced to 1 and the life-loss path
   runs (7.5): the native game should go straight to qualification/name entry
   (Arcade) or the Home menu flow, without the continue screen. VERIFIED.
