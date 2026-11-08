# Game start and stage-1 intro behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), main program `build/reference_maincpu.bin`
(sha256 0b66efe8...). Behaviour spec for the native Genesis rewrite
(NATIVE_ENGINE_PLAN.md sections 2 and 6). It covers everything from pressing
Start with a credit up to the first frame the player controls Willow in stage 1,
plus a short outline of the attract loop (section 8). Arcade addresses are
citations only. Same evidence tags as `player.md`:
**VERIFIED** (ROM/disassembly read here plus a headless arcade trace, or plus
the transliteration in `native/engine/src`), **SOURCE** (one source),
**UNKNOWN** (not established; says where to look).

Approved presentation cuts that apply (AGENTS.md): cut 3 (story/intro text on
its own rows with a plain background, not over the scene), cut 1 (no parallax:
plane B static per area), cut 4 (fixed palettes per stage, pre-coloured art),
cut 6 (display rate). Section 9 says where each one bites.

RAM below is arcade work RAM (`a5` = FF8000; negative `a5` offsets land in
FF0xxx, e.g. `a5@(-28750)` = FF0FB2, `a5@(31132)` = FFF99C). It only tells a
trace where to look; the native game keeps its own plain variables.

---

## 1. Evidence and how the timings were measured

- Disassembly of the scene manager 0x9F70..0xA5E4, the game task 0x92D2..0x9420,
  the credit task 0x90E8..0x9300, the message printer 0x78F0..0x7F40, the fade
  jobs 0x1E54..0x1F2E / 0x509C / 0x582C, palette loaders 0x0978 / 0x0AC4, class-12
  actors 0x2F39E (kind table 0x2F3C4), animation walkers 0x1F30/0x1F40/0x1F4C.
- Headless lockstep traces (no GUI), harness
  `build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/lockstep.keys ref`,
  `OVERCLOCK=5000`, natural intro (no `STAGE`, debug DIPs off):
  - `SCRIPT=stand`: coin held t600-605, Start held **t700-705**, then no input.
    BYTELOG of FF8000-FF800D, 83CF/83E9/83E2, cameras, player, clock; video dump
    every 6 frames; separate run with `DBGPC=7824` (every word entering the sound
    queue).
  - `SCRIPT=idle` (attract only, 4500 frames), `SCRIPT=keys KEYS="0-1700:A"`
    (attack held through the intro).
  "t" = harness script frame (frames since the program's first VBlank). Video
  dump keys (program VBlank count) track t to within a frame or two here. Frame numbers below are from the natural run
  and are exact to about 1 frame (sound words are queued during the frame
  before the BYTELOG line that shows the state change).
- Scratch output (rendered with `cpsframe.py`, `CPSFULL=1`, 384x224):
  `/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/intro/frames/`
  best frames: `intro-v000804.png` (world map), `intro-v001002.png` (sky, top of
  pan), `intro-v001110.png` (pan + first text), `intro-v001302.png` (forest,
  pan ended), `intro-v001380.png` ("HIS NAME IS WILLOW."), `intro-v001440.png`
  (dialogue box opening), `intro-v001650.png` (box text complete),
  `intro-v001830.png` (STAGE1 / CROSSROAD typing), `intro-v001884.png` (play,
  HUD, clock 2:59), `attract-v*.png` (attract scenes). Raw dumps: `nat/`,
  `att/`; logs `nat.log`, `snd.log`, `att.log`, `skip.log`.
- The earlier MAME-based intro research (`build/arcade-intro-*.json`,
  `build/arcade_intro_text.json`, `tools/native_intro_*.py`,
  `native/engine/src/campaign_scene.h`, `opening_entry.h`) agrees with
  everything below where they overlap (`arcade-intro-sequence-parity.json`:
  manager 898 frames).

## 2. Control structure (what drives the sequence)

| Item | Value | Tag |
|---|---|---|
| Top state | FF8000 (owned by whichever task is running: attract 0x80D0, credit 0x9146, game 0x93C0) | VERIFIED |
| Game mode | FF8004 (game task, 0x93FE): 0 = scene manager (intro), 2 = play, 4/6 other | VERIFIED |
| Scene step / timer / sub | FF8008 step (even), FF800A timer, FF800C sub. Scene manager entry 0x9F70, step table 0x9F90 (steps 0..38) | VERIFIED |
| Update rate | The game task (0x92D2) runs one update then sleeps 1 frame: one scene step per 60 Hz frame | VERIFIED |
| Message busy | FF82A6 = 1 while a text message is still typing; the printer clears it at the end (0x797A) | VERIFIED (trace) |
| Fast-forward flag | FFF99C: set every update while P1 button 1 or 2 is held (FF806A & 0x30, 0x9F70); cleared by most steps | VERIFIED |
| Stage-visited flag | FF8348: 0 during the intro, set to 1 when play starts (0xA42C) | VERIFIED |

Fast-forward (the only intro "skip"): with FFF99C set, the scheduler's idle
loop (0x1DDC) counts down every sleeping task again on each idle pass, so the
game task, the message printer and timers run several times per 60 Hz frame.
The speed-up is whatever CPU is spare: on the overclocked harness the
128-frame map hold vanished and the whole 898-frame forest intro took 3 frames
(fades that the game task waits for, e.g. the map fade-in, still took their
48 frames). There is no Start-button skip and no hard skip. The real-hardware
speed-up factor is **UNKNOWN** (needs an unoverclocked run, `OVERCLOCK` near the
CPS 10 MHz / Genesis 7.67 MHz ratio, or the `ffHist` statistics of lockstep).
VERIFIED (code + skip trace). Native: see section 9.

## 3. Timeline (natural run, no input after Start)

| t | Frames | Scene / event | Tag |
|---|---|---|---|
| 600-605 | | coin: sound **0x3B**, then **0x08FB** (t607), **0xF4** (t625); credit screen | VERIFIED |
| 700 | | Start pressed. Credit task (0x9228) takes the credit(s), starts the game task | VERIFIED |
| 700-722 | 23 | credit screen fades out (fade job 0x0C00, 1 frame/step, 0x92D4) | VERIFIED |
| 723 | 1 | game init (0x93D6): sound **0xF0, 0xF7** (stop all), clear pools, new-game values (2CCE), clear scroll1/2/3 | VERIFIED |
| 724 | | scene step 0: world map set up, music **0x12** | VERIFIED |
| 724-727 | 3 | step 2 timer 3 (black) | VERIFIED |
| 727-~775 | ~48 | map fades in (job 0x0C10, 3 frames/step); scene waits | VERIFIED |
| ~779-907 | 128 | step 4: map held, markers animate | VERIFIED |
| 907 | | sound **0xF0, 0xF7**; fade out (job 0x0C00, 3 frames/step) | VERIFIED |
| 907-~957 | ~50 | map fades to black; scene waits | VERIFIED |
| ~957-964 | ~7 | step 6: stage-1 setup (section 5) | VERIFIED |
| 964 | | stage reset: clock **3:00**, music words **0x0000, 0x01FA**, then **0x0013, 0x02FA** | VERIFIED |
| 964-971 | 7 | step 8: cameras settle, still black | VERIFIED |
| 971-~1002 | ~32 | fade in (job 0x1002, 2 frames/step); step 10 timer 63 | VERIFIED |
| 1034 | | message 1 starts, windmill and Aldwin spawn, pan starts | VERIFIED |
| 1035-1290 | 256 | camera pans down 2 px/frame (y 512 -> 0) | VERIFIED |
| 1034-1136 | 103 | "ONE YOUNG MAN STARTS ON HIS JOURNEY" types (35 chars x 3) | VERIFIED |
| 1168-1312 | 147 | "TO SAVE A BABY SAINT FROM BAVMORDA'S / WICKED HANDS." (49 x 3) | VERIFIED |
| 1346-1400 | 57 | "HIS NAME IS WILLOW." (19 x 3) | VERIFIED |
| 1427-1443 | ~17 | dialogue box opens | VERIFIED |
| 1444-1648 | 205 | box text "WILLOW, BE CAREFUL. ..." (69 x 3) | VERIFIED |
| 1512 | | Aldwin changes pose (222 frames after the pan ended) | VERIFIED (code) |
| 1771 | | text layer cleared; "STAGE1" types (6 big letters x 4) | VERIFIED |
| 1796-1828 | | "CROSSROAD" types (9 x 4) | VERIFIED |
| 1862 | | text cleared, play mode (FF8004 = 2), HUD built, stage spawns start | VERIFIED |
| 1863 | | clock 3:00 -> 2:59 (first play update) | VERIFIED |
| 1862-1879 | 17 | player gate: Willow turns right, 16-frame wait | VERIFIED |
| **1879** | | **player state 2: first frame under player control** | VERIFIED |

Start to control: 1179 frames (19.65 s). World map: ~236 frames from game
init to black. Forest intro (stage reset to play): **898 frames**.

## 4. Scene A: world map (scene steps 0-6)

### 4.1 Set-up (step 0, 0x9FB8)
- Clears camera records (2DE6) and actor pools (2648), FF8343 = 0, loads the
  map palettes (0x0AC4), queues sound **0x12** (0x9FCE). VERIFIED.
- Layer control word FF804A = (old & 0x3F) | 0x12C0; CPS layer control register
  read back as **0x12F8** in the dump (same as in play). VERIFIED.
- Near camera (scroll2) record FF8402: map grid **7** (pointer table 0x2DBF8
  entry 7 = 0x2EA24, mode 0), x = **2560**, y = 0. Far camera (scroll3) record
  FF8482: grid 7 of 0x2EB24 (= 0x2F24C), x = **1024**, y = 0. Both drawn whole
  at once (548C / 5532 twice). No scrolling afterwards. VERIFIED.
- Spawns 7 class-12 actors (0x350E2): kind **21** sub 0..5 (stage markers I-VI)
  and kind **22** (Willow's figure). VERIFIED.

### 4.2 What is on screen (`intro-v000804.png`)
- scroll3 = the painted world map (land, castles, lakes). scroll2 = the gold
  picture frame and the compass rose (top left), pen 15 elsewhere. VERIFIED
  (rendered grid 7 with `game/tools/stagemap.py 7 15 1`, blocks 29/30 at
  x 2560-3071).
- No scroll1 text. VERIFIED (dump decode).
- Stage markers (kind 21, handler 0x3512E). World positions from table
  **0x351A2** (x,y per stage index), roots from offset table **0x351BA**
  (via 1F40), each a single held node (ticks 0xFFFF), attr **0x0112** (block
  2x1 tiles, palette 0x12):

  | Marker | World (x,y) | Root | Code | Tag |
  |---|---|---|---|---|
  | I | (2604,104) | 0x35316 | 0x11D3 | VERIFIED |
  | II | (2664,160) | 0x35326 | 0x11E3 | VERIFIED |
  | III | (2736,112) | 0x35336 | 0x11F3 | VERIFIED |
  | IV | (2840,88) | 0x35346 | 0x11D5 | VERIFIED |
  | V | (2864,176) | 0x35356 | 0x11F5 | VERIFIED |
  | VI | (2792,200) | 0x35366 | 0x11E5 | VERIFIED |

  Palette rule (record +0x10 palette override, +0x12 enable): stages already
  cleared (index < FF8344) use palette **0x14**; the current stage blinks
  between palettes **0x12 and 0x13 every 10 frames**; later stages keep 0x12.
  VERIFIED (code 0x35166..0x35204; dump: marker I attr 0x0112 at t804/810,
  0x0113 at t816).
- Willow's map figure (kind 22, handler 0x3520A): position by stage from table
  **0x35268** (stage 1: world (2604,128), just above marker I), root by stage
  from offset table **0x35280** (stages 1/3: 0x35376; 2/4: 0x353E0; 5/6:
  0x35410). Stage-1 chain 0x35376: node 0x35376 (attr 0x1115: block 2x2,
  palette **0x15**, code 0x2202, 42 ticks) then alternating 8-tick nodes
  (piece-list node 0x2202/2214/2205/2215 and block 0x2206) looping back to
  0x35376 after a 0xFFFF node (0x353D0). The figure blinks/animates in place.
  VERIFIED (chain read; dump record c2202 a1115). The +0x21 test at 0x35222
  reads `a4` (the last allocated slot), not the actor's own record: a source
  quirk; in this spawn order it takes the "position by stage" path. SOURCE.
- Animation node format (all actors here): +0 piece count (bit 15: long pieces),
  +2 attr (low 5 bits = object palette; high byte non-zero: one CPS block of
  16x16 tiles, x size = bits 8-11 + 1, y size = bits 12-15 + 1, tile code +
  column + 16*row, followed by one code word; else a list of `count` piece
  words), +4 selector (bit 15: next node pointer is a long that follows),
  +8 ticks (0xFFFF = hold), +10 data. Walkers: 1F30 (root = table[+0x0F] offset),
  1F40 (root = a0), 1F4C (tick; next node). VERIFIED (1F30..1F86).

### 4.3 Timing and exit
- Step 2: 3 frames, then fade-in job **0x0C10** (speed 3: ~48 frames, measured
  brightness flat from t774), the scene task waits for it (1EAA). Step 4: 128
  frames hold. Then sound **0xF0, 0xF7** (784C) and fade-out job **0x0C00**
  (speed 3; also clears the text layer, 0x582C), waited for (1E88). VERIFIED.
- Holding button 1/2 fast-forwards the hold (section 2). VERIFIED.
- Whether the map is also shown on other entries to the scene manager
  (continue, life loss): **UNKNOWN** (every entry at step 0 runs it; find
  who resets FF8008 to 0 with FF8004 = 0).

## 5. Stage-1 set-up (step 6, 0xA0D0)

Order (VERIFIED, also `opening-boundary.json`): clear cameras 2DE6, clear
actor pools 2648, stage palettes **0978**, player start **2F08**, then the
branch: stage 4 / stage 3 have their own intros (0xA1B8, 0xA14A); if FF8348
set or stage != 0, go straight to play (0xA220, map only); else (new game,
stage 1): stage reset **2DFE** (clock from table 0x304C = 3:00, divider 0,
HP = max HP, queues music 0x0000 + 0x01FA), then:
- camera target FF0FB0/FF0FB2 = **(192, 640)**, near and far camera positions
  = **(192, 512)**, timer 7, step 8;
- **78B8**: queues **0x0013, 0x02FA** (stage music; it keeps playing into
  play, no new music command at play start). VERIFIED (snd trace t964).

Player start: Willow at **(192, 72)** world, player state 0 (stage-start gate),
action 2 waiting for FF8348; facing 3 (standing, facing **left** toward the
Aldwin) because FF8348 = 0 at the gate reset (0xBFA8). Pose root table
**0xD07E**, entry 3 = node **0xD15A** (code 0x0033, attr 0x2220: 3x3 block,
x-flip, palette 0, hold). VERIFIED (code, chain, MAME record code 51 attr
0x2220, trace).

## 6. Scene B: forest intro (steps 8-24)

### 6.1 Layers and camera
- Backgrounds are the real stage-1 maps: scroll2 grid 0 (0x2DBF8 entry 0,
  mode 0, 16x16 tiles from blocks at ROM 0x80000 + b*0x400) and scroll3 grid 0
  (0x2EB24 entry 0, 32x32 tiles, blocks at 0xC0000 + b*0x100). Lookup and
  render: `tools/cps1_source_layers.py` (`SourceLayers(stage=0)`), assembly:
  `game/tools/stagemap.py 0 ...`. VERIFIED (addresses), derived ranges:
  scroll2 block rows 13-15 (world y 0..736), scroll3 block rows 14-15.
- Camera (trace): near (scroll2) x = **0**, y from **512** to **0**; far
  (scroll3) x = 0, y = half the near y (**256 -> 0**). The near x is set to
  192 at t964 and the camera controller puts it at 0 (player x - 192) by t970.
  VERIFIED.
- Top of the pan (`intro-v001002.png`): blue sky and clouds with a mountain
  range and a castle silhouette on scroll3, cloud banks on scroll2. Bottom
  (`intro-v001302.png`): the Nelwyn village forest with the hut (left), the
  windmill, the Aldwin and Willow. VERIFIED (renders).
- Pan rule (step 12-18): each frame, while near camera y != 0, the target y
  (FF0FB2) -= 2; the camera follows (42602 near, 42F48 far). Result: exactly
  2 px/frame, t1035-1290. VERIFIED.
- Layer control 0x12F8; priority masks for stage 1 from table 0x0A52 (0000,
  003E, 00FE, 001E; read back in the dump). VERIFIED.

### 6.2 Fades
- t964-971 black (step 8 timer 7). At timer end: job **0x1002** with speed 2
  (palette brightness fade-in, 509C; 16 steps of 2 frames, full at ~t1002);
  not waited for. VERIFIED (code, brightness of renders).
- No fade at the end: the title text is cleared and play starts on the same
  picture. VERIFIED.

### 6.3 Text (scroll1)
Message printer: `jsr 15CC` with a0 = 0x78F0, d1 = message word (high byte =
handler), d2 = style (high byte: high nibble = record index shift, low nibble
= frames per character; low byte: destination index into table 0x7A1E).
Records at ROM **0x70000 + (index >> shift) * 168**. Per character: sound
**0x2F** (also for spaces; suppressed when FF8293 or FF83C3 is set), draw,
sleep. `&` = next row (same start column), `/` = end. Handler 0 draws glyph
0x7000 + ASCII, handler 4 draws 0x7100 + ASCII inside a box, handler 6 draws
2x2 big letters (cell codes c, c+1 / c+0x10, c+0x11, c = 0x7000 + word).
VERIFIED (0x78F0..0x7B44, 0x7DB8..0x7E2A, 0x7F12; strings
`build/arcade_intro_text.json`, re-decoded from the dumps).

Cell (col,row) -> CPS screen pixel (col*8, (row-2)*8) on the 384-wide line.

| # | Word / style | Text (exact) | Cells | Palette | Rate | Tag |
|---|---|---|---|---|---|---|
| 1 | 0x002E / 0x1302 | `ONE YOUNG MAN STARTS ON HIS JOURNEY` | col 5, row 13 | scroll1 pal 3 (red-orange) | 3 f/char | VERIFIED |
| 2 | 0x002F / 0x1303 | `TO SAVE A BABY SAINT FROM BAVMORDA'S` / `WICKED HANDS.` | col 7, rows 13-14 | 3 | 3 | VERIFIED |
| 3 | 0x0031 / 0x1305 | `HIS NAME IS WILLOW.` | col 12, row 13 | 3 | 3 | VERIFIED |
| 4 | 0x0419 / 0x0306 | `WILLOW, BE CAREFUL. BAVMORDA'S FINGERS` / `PENETRATE OUTSIDE OF THE VILLA.` | col 4, rows 6-7, in the box | 0 (light blue) | 3 | VERIFIED |
| 5 | 0x0640 / 0x1412 | `STAGE1` (big) | cols 18-29, rows 13-14 | 0x0E | 4 f/letter | VERIFIED |
| 6 | 0x0641 / 0x1413 | `CROSSROAD` (big, 9 letters, no S) | cols 14-31, rows 16-17 | 0x0E | 4 | VERIFIED |

Records: messages 1-3 in 0x70F18 / 0x70FC0, message 4 in 0x71068, 5-6 in
0x71500 (record + 8 / + 88 = the two strings). Big-letter codes (word + 0x7000):
A 0x7680, C 0x7684, D 0x7686, E 0x7688, G 0x768C, O 0x76AC, R 0x76C2,
S 0x76C4, T 0x76C6, digit 1 0x7B00. VERIFIED (dump cells t1860).

Clearing: 7D80(row) writes the blank glyph **0x71A6** (attr 0) to rows row and
row+1, columns 4..43. Before message 2: rows 13-14; before 3: rows 13-14 and
16-17; before 4: rows 13-14. 6642 clears the whole text layer to 0x7020
before message 5 and at play start. VERIFIED.

Dialogue box (handler 4, 0x7B46): a framed panel on scroll1 rows **4-11**,
columns 0-47, frame glyphs 0x7A0C..0x7ABB palette **0x1F** (mirrored on the
right), interior dark; it opens horizontally from the centre (columns 20/27)
in 5 steps 4 frames apart (column tables 0x7C50 / 0x7C64, widths 0x7C78,
glyph offsets 0x7C7E), then the text types inside. Removed by the 6642 clear.
VERIFIED (dump cells, code; step widths SOURCE).

Gaps between messages (scene timers, counted only after the previous message
finished, except step 16): 29 frames (1 -> 2), 30 (2 -> 3), 24 then wait for
the pan to have ended (3 -> 4), 120 (4 -> clear + 5), none (5 -> 6), 30
(6 -> play). VERIFIED (0xA2C4..0xA41C, trace).

### 6.4 Actors
All three are drawn in world space and scroll up with the pan; none moves.

| Actor | Spawn (step 10, t1034) | Animation | Palette | Tag |
|---|---|---|---|---|
| Windmill (class 12 kind **1**, 0x3257E) | world (218,136), active byte 2, draw list 2 (record +0x0E = 2) | root **0x325BC** (1F40): 0x0760 (block 6x5) 10 ticks -> 0x0766 (5x5) 10 -> 0x076B (5x5) 10 -> loop. Turning sails, 30-frame cycle | object **0x1F** | VERIFIED |
| High Aldwin (class 12 kind **3** sub 0, 0x326A6) | world (160,80), active byte 1 | root **0x32AA2**: 0x0B00 10 ticks -> 0x0B03 6 -> 0x0B06 6 -> back to 0x0B03 (3x3 blocks). When the near camera y reaches 0, waits **222** frames, then root **0x32ACA**: 0x0B03 8 ticks -> 0x0B09 held (turns/gestures). Then normal off-screen removal (2162) | object **0x0A** | VERIFIED |
| Willow (player, state 0) | world (192,72) from 2F08 | stand left, node 0xD15A (code 0x0033, x-flipped 3x3) | object **0** | VERIFIED |

Both scene actors stay in the stage after play starts and are removed only by
their off-screen tests (windmill: 21C6 every 16 frames, stage 1 = x < FF838C;
Aldwin: 2162, beyond the camera margin). VERIFIED (code), the screenshot at
t1884 shows both.

### 6.5 Sound
- Music 0x13 (with 0x0000/0x01FA and 0x02FA words, see `audio.md` section 2 for
  the parameter latch) starts at t964 in the dark and continues into play.
- One **0x2F** per typed character (232 in total). Nothing else.
- Semantics of the 0x0000 + 0x01FA pair (checkpoint music word from 0x304C)
  next to 0x13: **UNKNOWN** (audio.md open item on 0x01FA/0x02FA).
VERIFIED (snd trace).

## 7. How stage 1 begins (end of step 24, 0xA41C)

At the end of the 30-frame wait after `CROSSROAD`:
1. 6642: text layer cleared.
2. FF8348 = 1; FF8004 = 2 (play), FF8008 = 2, FF800C = 0; FFF99C cleared.
3. 4F44: full HUD build (hud.md 4); 4035E: stage actor spawns; 9A92: stage
   events. Camera stays at (0,0). VERIFIED.
4. Player gate (0xBFE8): FF8348 now set, so Willow's facing becomes 2
   (standing right, root 0xD14A, code 0x0033 unflipped), a 16-frame timer
   runs, then player state 2 (normal play, action 0) at **t1879**: the first
   frame input moves Willow. VERIFIED (code, trace 9848 = 2 at t1879, render).

### 7.1 Why the clock does not run during the intro
- The clock routine **4EA2** (hud.md 3.5) is called only from the play loops
  (callers 0x8DF8, 0x9552, 0x9618; the latter two in play mode 0x9416..). The
  intro runs in FF8004 = 0 (scene manager), so 4EA2 is simply never called.
  VERIFIED.
- FF83CF, FF83E9 and FF83E2 stay **0** for the whole intro (trace). They are
  in-play locks written by actor scripts (FF83CF at 0x270DE, 0x27F0E, 0x2916A,
  0x2A4F8, 0x2C2A0; FF83E9 at many cutscene/boss sites, e.g. 0x14990,
  0x1670C, 0x1DEDC; FF83E2 at 0x2F91E/0x2FB4E). They do not hold the stage-1
  start clock. VERIFIED (trace), writers' meanings **UNKNOWN**.
- Stage reset 2DFE loads 3:00 with divider 0 at t964; 4EA2 does
  `divider -= 1` and ticks on borrow, so the very first play update (t1862/
  1863) shows **2:59**, then every 60 updates. The "~900 frames held" is
  exactly the forest intro length (898 frames). VERIFIED.

## 8. Attract loop (no credit) - outline only

Entirely different content from the game-start intro; it is Arcade-mode only
(AGENTS.md: Home has no attract loop). Attract task 0x80D0, FF8000 = 4,
sub-mode FF8004; timings from the idle trace (t from the program start):

| t | FF8004 | Content | Tag |
|---|---|---|---|
| 15-330 | (boot) | RAM/VRAM self-test text (`WORK RAM OK` ... `SCROLL 3 OK`) | VERIFIED |
| 345-825 | 0 | Bavmorda scene: Queen Bavmorda over a cave, lightning palette cycles; red text rows 14-18: `THE EVIL QUEEN BAVMORDA` / `SHE WANTS TO DESTROY THE HOLY BABY` / `ELORA DANAN!` | VERIFIED (dumps) |
| 829-2196 | 2 | character panels with framed portraits: `HE WAS SELECTED / AS A PROTECTOR OF / ELORA DANAN / AGAINST QUEEN / BAVMORDA.` `A PEACE-LOVING MAN.` `WILLOW UFGOOD`; then `HE JOINS WILLOW'S / FIGHT AGAINST / GENERAL KAEL, / A FATAL RIVAL.` `A BRAVE WARRIOR.` `MADMARTIGAN` | VERIFIED |
| 2196-3808 | 4 | `A SUPER GAME FROM CAPCOM ...` `COMING THIS SUMMER!` `A MAGNIFICENT FANTASY FILLED WITH LOVE,` `FRIENDSHIP AND ADVENTURE!`; WILLOW logo with `INSERT COIN` and the Lucasfilm/Capcom copyright lines; FBI seal `WINNERS DON'T USE DRUGS` / `WILLIAM S.SESSIONS, DIRECTOR,FBI` | VERIFIED |
| 3809- | 6 | demo play: stage 1 boss area (player x 3392), clock 1:00, timer 900 frames, HUD and `INSERT COIN` | VERIFIED |

The previous transliteration covers these scenes (`native/engine/src/
game_attract_task.h`, `_opening.h`, `_pan.h`, `_panel.h`, `_demo.h`,
`_scores.h`, `_interlude.h`); high-score table and loop restart follow the
demo (FF8000 states 6-12). Exact durations, music and all strings of the
attract loop: SOURCE/UNKNOWN beyond the table; to be specified separately if
Arcade mode needs it. Coin during attract: sound 0x3B; the credit screen shows
`PRESS START BUTTON` (cell 17,15) and `1 PLAYER ONLY` / `1 OR 2 PLAYERS` /
`FREE PLAY` (cell 19,16), `CREDIT n` (37,29) (string table near 0x5E19).
VERIFIED (ROM strings, dump).

## 9. Native implementation notes

Behaviour to reproduce: the order, durations and per-frame rates above, the
music/sound commands at the same frames, the actors and their animation
timing, the player gate, the clock not running until play.

Presentation (proposals; the user decides where marked):
- **Cut 3 (text on its own rows):** story messages 1-3 and the title use
  the Genesis window plane: a plain black band (e.g. 4 rows at the bottom,
  rows 24-27 of 28) with the original font and palettes 3 / 0x0E; messages keep
  their typing rate and the 0x2F tick. Message 4's box is the arcade's own
  panel (already a plain background); draw it in the band too, or as the
  original 8-row panel at the top. 38 characters per line fit 40 columns. The
  big-letter title (18 cells wide) also fits. Decision needed: band position
  and whether the box keeps its frame art.
- **Cut 1 (no parallax) vs the pan:** the pan moves scroll2 at 2 px and
  scroll3 at 1 px per frame for 256 frames. Options: (a) plane B is a
  pre-composed tall picture of the scroll3 sky/mountains (384 x 480 source,
  cropped to 320) scrolled at half speed only during the intro (a parallax
  exception for 256 frames); (b) plane B static (show the forest backdrop)
  and only plane A pans; (c) pre-compose both layers into plane A for the
  pan. Decision needed. Plane A must hold scroll2 block rows 13-15 (world y
  0..736) for the pan.
- **Cut 4:** fixed palettes: scroll2 0xE7D20 (stage 1), scroll3 0xEAB00
  (16 palettes), text 0xE6EA0, objects 0xE5080 (0-15) and 0xE5480 (16-31).
  Fades: 16-step CRAM brightness ramps (VDP colours scaled), speeds 3/3/2
  frames per step as in section 3.
- **Fast-forward:** native equivalent proposal: while A or B is held, run 4
  scene updates per frame (messages, timers, pan) but keep fades and music
  real-time. The arcade factor is UNKNOWN (section 2); keep this a proposal.
- **Width:** the CPS line is 384 px, Genesis 320: with the usual 32 px crop on
  each side all text (columns 4..43) and actors fit; the box frame's outer
  columns are cut.

Data a native build needs (offline tools write it; nothing of the arcade
program runs):
1. World map: scroll3 grid 7 at x 1024..1407, y 0..223 and scroll2 grid 7 at
   x 2560..2943 (frame + compass), 0x2DBF8/0x2EB24 entry 7; palettes scroll2
   0xE9920 (32), scroll3 0xEB700 (32), objects 16-31 0xE5280; priority masks
   0x0A82 (FF8343 = 0).
2. Marker sprites: 6 codes (0x11D3/E3/F3/D5/F5/E5, 2x1 blocks) and palettes
   0x12/0x13/0x14 (ROM 0xE5280 + (p-16)*32); positions 0x351A2. Willow map
   figure chain 0x35376 (codes 0x2202..0x2215, palette 0x15), position 0x35268.
3. Stage-1 backgrounds for the pan: scroll2 grid 0 rows 13-15, scroll3 grid 0
   rows 14-15 (`game/tools/stagemap.py`, `tools/cps1_source_layers.py`;
   existing stage-1 plane data in `game/data/stage1` covers play only).
4. Sprites: windmill blocks 0x0760 (6x5), 0x0766, 0x076B (5x5), palette 0x1F
   (ROM 0xE5480 + 15*32 = 0xE5660); Aldwin blocks 0x0B00/03/06/09 (3x3),
   palette 0x0A (0xE5080 + 10*32 = 0xE5120); Willow 0x0033 (3x3) palette 0
   (player.md). Graphics region from `tools/cps1_gfx.py assemble_region`.
5. Text: the 6 strings, their cells, palettes 0/3/0x0E/0x1F (scroll1 palette
   ROM 0xE6EA0), font 0x7000+ASCII (hud.md), box font 0x7100+ASCII, big
   letters (A C D E G O R S T 1: codes above, 2x2 cells), box frame glyphs
   0x7A0C-0x7ABB and the opening tables 0x7C50-0x7C7E.
6. Script table: per step its duration/condition, spawns, clears, messages,
   sounds (section 3, 6.3 gaps), pan rule, gate rule.
7. Sound: 0x12 (map music), 0xF0/0xF7 (stop), 0x0013 + 0x02FA (stage music,
   with 0x0000 + 0x01FA), 0x2F (text tick); mapped by the audio path.

## 10. Open questions (UNKNOWN)
1. Real-hardware fast-forward factor while button 1/2 is held (section 2).
2. Whether the world map / forest intro repeat after a continue or life loss
   (who resets FF8008/FF8004 to 0; FF8348 is set after the first intro).
3. Meaning of the 0x0000 + 0x01FA music words queued with 0x0013 + 0x02FA.
4. Exact fade curves of jobs 0x0C00/0x0C10/0x1002 (brightness levels per step);
   frame counts are measured, the per-step colour maths is in 0x509C/0x582C.
5. Two-player start (FF82A5 = 1): assumed identical intro, not traced.
6. Purpose of the windmill's "active = 2" vs Aldwin's 1 (pool byte +0).
7. Attract loop details beyond section 8 (durations of each panel, music,
   high-score table), if Arcade mode is to reproduce it.
