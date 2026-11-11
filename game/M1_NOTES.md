# Native engine, milestone M1: notes (2026-11-01)

Plan: ../NATIVE_ENGINE_PLAN.md (M1: stage 1 opening, Willow walk, jump,
attack, magic, HUD, music; gate: 60 updates/s, no glitches, movement
matches arcade traces). Status: in progress.

## Done

Build and checks (from `game/`):
- `make -f ../build/sgdk-native/makefile_nolto.gen SGDK=../build/sgdk-native JAVA=/opt/homebrew/opt/openjdk@21/bin/java release`
- `tools/headless.sh out/rom.bin FRAMES "f,f" OUTDIR "KEYS"`: stock Genesis
  Plus GX, no window, frames as PPM.

### Stage 1 foreground and backdrop (`tools/stagepack.py`, `src/stage.c`)
- Two scenery palette lines (CRAM lines 2-3) fitted offline by k-means over
  8x8 tiles; each tile takes the better line. Foreground and backdrop share
  them. Checked against the arcade map: forest, rocks, waterfalls.
- Map: 480 x 64 cells (3840 x 512 px); plane A 64x64 holds the full height,
  scrolled horizontally; columns stream in as the window moves.
- VRAM slots planned offline by interval colouring along the arcade's camera
  path (`B tN` world camera log, view = x+32 .. +320, map y 272 - cam y,
  48 px vertical margin): a tile's runs get slots so that no two runs sharing
  a slot are ever on screen together. Runtime only copies the listed tiles.
- Pool: 1,152 slots unmerged; 941 with approved cut 5 (foreground tiles
  merged within error 128, `FG_MERGE=128`: no visible change in side-by-side
  frames). Backdrop: static picture (cut 1), 813 distinct 8x8 cells merged
  to 384 tiles (`BD_TILES=384`, k-medoids in `tools/tilemerge.c`, mirrored
  matches allowed). 160 tiles was visibly mosaic; 384 with mirroring is
  close to the unmerged picture. VRAM: tiles 16-956 pool, 957-1340
  backdrop, 1424-1535 Willow and shot, 1648+ debug font.
- Not reducible by hiding: 920 of the 1,120 backdrop cells show through the
  foreground somewhere even in the forest opening.
- Streamer: at most 2 columns a frame; after a camera jump the view stays
  inside the loaded columns and catches up (never shows unloaded tiles).
- Checked headless along the arcade's whole stage 1 camera path (`CAMTEST`
  build, `CFLAGS_EXTRA=-DCAMTEST ./build.sh`): no wrong tiles, no missed
  VBlank, at most 71 tiles loaded in one frame. `tools/streamcheck.py`
  compares frames with the offline picture: only the emulator's DAC scale
  (7 -> 238) and the test build's Willow sprite differ.
- `./build.sh` always reassembles `src/data.s` (make does not track its
  `.incbin` files: a stale data object once crashed the ROM).

### Willow (`src/player.c`, `tools/posepack.py`, `specs/player.md`)
- Poses: the 70 Willow animation nodes drawn offline and cut into Genesis
  sprites (<= 32x32 over non-empty cells): at most 36 tiles and 4 sprites a
  pose; tiles DMA'd only on a pose change. CPS-1 flipped multi-tile blocks
  draw their tiles in reverse order (fixed in the packer).
- Collision: per 16x16 cell id (arcade attribute bits 10-15) and the 64
  descriptors (ROM 0x4712), `tools/colpack.py`. Stage 1 has floors, solid
  blocks, walls and thin platforms (ids 36-38).
- Walk 2 px/frame, ground push and foot snap, edge fall, jump 1088 / 1408
  (up), gravity 56, max fall 1792, air steering, crouch, ground / crouch /
  air attack (10 frames), head and wall probes, camera X = player - 192.
- Arcade timing rules found by trace comparison: a jump or attack press is
  acted on in the next update; on a press frame and on the frame crouch is
  released the pose does not change; landing while walking starts the walk
  cycle one tick in, landing standing keeps the jump pose one update; in the
  air no direction sets the standing bit.
- **Trace check**: `tools/tracecmp.py` against the original program at full
  speed (lockstep `ref` on the CPS core, `out/lockstep.keys` with
  `SCRIPT=keys`, the B1 harness's new scripted-input mode). Script: walk
  100 frames, jump, up-jump with steering, attack, crouch, crouch attack,
  walk left, jump left. 565 of 565 frames match in x, y, facing, pose and
  camera X.
  (The B1 cartridge cannot serve as reference: it runs the arcade program at
  ~80 % speed, so the same input frames give fewer updates.)

### Shot (`src/shot.c`)
- Arcade routine EC84 as the specification: speed 1024 + 64 v (table
  0xED0C), up to 4 alive, 64-frame life, removed off-screen (checked every
  4th frame) or at a wall; spawns on the 5th attack frame at y+8 (standing)
  or y+0 (crouching). Art: animation chains from the root table 0xEFBC
  (7 variants x 2 facings, 4 poses x 3 frames), palette group 8.

### Frame time
Game work (input, Willow, shots, streaming, sprite table) ends at scanline
16 in a median frame and 71 at worst (a frame that loads a new map column),
of 224 before VBlank; no missed VBlank in any run. (The optional debug row,
`-DDEBUG_ROW=1`, costs ~30 lines by itself.) The per-tile DMA queue entries are the column cost:
to optimise before M3 (contiguous per-column tile blocks, or CPU copy in
VBlank).

Playtest build: `game/willow-native-m1-20261101a.bin` (512 KiB; boots
straight into stage 1 at the start point; A attack, B jump, up+B high jump,
down crouch).

### Second pass (2026-11-01, after "continue with the native rewrite")
- **Slide, drop-through, ledge climb** written from the arcade handlers
  (CDC2, C8F2, CCF0; ledge probe 0x3AF2 / routine 3FA2; climb offsets
  0xCD74), stepping Willow's animation through the arcade node graph
  (ticks / next, exported by posepack as `nodes.bin`).
- **Arcade rules found by trace comparison** (all now in `src/player.c`):
  a foot exactly on a floor surface counts as contact; flat floors of
  height >= 9 ignore a first-foot probe in their lowest 7 px; wall probes
  ignore a cell when the probe is below its ceiling or above its floor, and
  push out exactly to the wall face; in the jump the horizontal step comes
  after the vertical move and the ledge test (routine CA1C runs after the
  handler), so all probes see the previous x; the ledge is tested after
  the vertical move, not on a landing frame; landing keeps the jump pose for
  that update, and a walk entered with the facing unchanged (after a
  landing or climb) shows its first pose one tick short; crouching with a
  direction only turns (facing 2/3); an attack press during a slide shows
  the crouch pose at once; the crouch flag ends with the slide.
- **Vertical camera** (42B1C / 42BAC): rise tracking, acceleration 64 a
  frame to 1792 in 16.16, snap within 130, airborne follow only past 200
  at 2 px a frame; the airborne flag is set and cleared where the arcade
  does (jump 1, falling 2, landing / ledge grab 0, climb 1 -> 0). Stage 1
  has no minimum-Y table (42E66 is off for it).
- **Traces** (`tools/tracecheck.sh`, data in `tests/traces/`): basic 565
  frames, slide 505, route 718 (walk with 34 jumps / up-jumps, ledge grab,
  two climbs, camera rising): every frame matches in x, y, facing, pose,
  camera x and camera y. The route reference continues to 2,405 frames
  (`route-full.ref.txt`), but from frame 713 the arcade's enemies act on
  Willow (a 2 px push with no terrain under it), so the rest waits for M2.
  `tools/tracediff.sh NAME FROM TO` prints both sides.
- **Streaming cost**: tiles for new columns are now copied by the CPU in
  VBlank (`stage_vblank`) instead of one SGDK DMA queue entry per tile
  (~600 cycles of frame time each): worst gameplay frame ended at scanline
  71, now 41-45.
- **Audio**: arcade sound program run offline (`tools/audio/opmlog`), its
  YM2151 writes converted to YM2612 + PSG VGM (`opm2opn.py`), music packed
  by SGDK's XGM2; effects are PCM rendered from the original YM2151 logs
  with ymfm. In the game: stage 1 music (command 0x13), shot (0x20) and
  landing (0x2B) sounds, `src/sound.c`, `res/audio.res`. Headless output
  level ~5,000 RMS from the second second on; Z80 load ~30 %.
- **SGDK fix**: GCC 16 compiled `Z80_upload`'s byte loop as
  `move.b (a0)+,(0,a0,d0.l)`; on a 68000 the destination sees the
  incremented a0, so every byte landed one address too high and no Z80
  sound driver ran (XGM and XGM2 both silent). `build/sgdk-native/src/
  z80_ctrl.c` now uses explicit `move.b (a1)+,(a0)+` in Z80_upload and
  Z80_download, rebuilt into `lib/libmd.a` (originals kept as `.orig`).
- Frame time with audio: game work ends by scanline 45 in every gameplay
  frame (median 16); the one start-up frame that loads the stage and
  starts the music reaches 111. No missed VBlank.

Playtest build: `game/willow-native-m1-20261101b.bin` (music, shot and
landing sounds, slide (down+B), drop-through, ledge climb (hold up)).

### HUD (2026-11-01, third pass; `src/hud.c`, `tools/hudpack.py`)
- The user accepted both audio approximations ("Accept both"): music onsets
  on 60 Hz frames (mean 4.1 ms, max 8.3 ms) and the FM effects as PCM.
- Glyphs: every HUD glyph (font, vitality cells, charge bar pieces for text
  palettes 0x11-0x1D, item icons) decoded from the arcade graphics with its
  fixed text palette (ROM 0xE6EA0) and coloured offline for stage 1's CRAM:
  each glyph group takes the CRAM line that reproduces it best (blue text
  and armour line 2, yellow text line 1, red text / gems / bar mostly line
  0). 135 resident tiles at 1648-1782, plus 182 partial-fill tiles in ROM
  for the charging cell (streamed into one slot pair).
- Layout (cut 2): the 48-column arcade layout folded to 40 columns (left
  group as is, middle group 1 left, right group 7 left, TIME 6 left). All
  on plane B (static backdrop, cut 1) with the priority bit: the top rows'
  glyphs replace backdrop cells, their transparent pixels show the
  foreground or the background colour, set to the stage's sky (CRAM 38),
  and blank cells get their backdrop word back, so the text sits over the
  canopy as in the arcade (a window-plane HUD hid the foreground). The
  bottom rows are the arcade's opaque band (charge bar, items); the reserve
  lives ("P= n") are 1x1 sprites at y 192 over the scene. Willow and shot
  sprites now use low priority so the HUD covers them, as the arcade's text
  layer does.
- Items: shown icons are packed from column 26, at most 7 (the arcade has
  9 fixed slots; all 9 at once needs every armour, item 7, 2x item 3 and 3+
  protection charges).
- Values: start values from the arcade trace (lives 2 -> "P= 1", vitality
  3/3, time 3:00, high score 55000). The clock ticks from the first
  unlocked update (3:00 -> 2:59 at once, then every 60 updates, as the
  arcade at t1863); the arcade holds it ~900 frames during the opening
  scene, which the native build does not have yet. Redraws only on change.
- Checked: `-DHUDTEST` build (every element away from its start value,
  magic level 6, 9 items) captured headless; traces still 565/505/718.
- Frame time (frame-end scanline, gameplay, `-DHUDOFF` for comparison):
  basic max 46, slide 54, route 62 (59 without the HUD), median 16; start-up
  frame 183. The earlier "45" was the basic script; the route's streaming
  frames were already at 59. No missed VBlank: 60 updates/s.

Playtest build: `game/willow-native-m1-20261101c.bin` (adds the HUD).

### Charge magic (2026-11-01, fourth pass; `src/magic.c`, `src/shot.c`, `src/vram.c`)
- Arcade routines read for the rules: C610 (the handlers' attack-button
  check), C6C0 (sparks), 4B0E (meter step), CB24 (spell selection), EC84 /
  ED7C (normal shot), 3744 (shot wall test), FB7C (spark actor).
- Hold counter (to 127; sound 0x31 at 16, 0x44 every 16 frames at 127),
  meter (+0x11F a frame to 0x8FFF at level 0), release with a hold of 16+
  = a new attack whose spell comes from the meter's level at release (taps
  cast the level's own variant). The counter only runs in the ground, slide
  and air handlers (frozen during the attack itself); drop and climb run the
  sparks only. HUD: the cell at level h shows a white line h & 7 eighths
  long, the one before it full (the arcade's "fill" is that white line);
  the filling cell's tiles are streamed into one slot pair, both queued for
  the same VBlank. Reset redraws the empty bar.
- Shots: variant per shot (the spell, 0-6); the arcade's order: the spawn
  update only sets up, then life (64), bounds every 4th update, move,
  animate, wall test; out of life / off screen removes it after that update;
  a wall starts the 12-update impact animation (0xEFB8, now packed) in place.
- Sparks: 16 offsets standing / crouched (0xFB90 / 0xFBD0), four animations
  and lifetimes (0xFC64 / 0xFC60), cadence 0xC706 by hold / 16. Positions
  random (own generator: decoration, fidelity contract).
- Spells 7-12 need magic level 1+, which comes from shops (not in the stage
  1 opening): not implemented yet; they need sprite VRAM planned with M2's
  enemies (below).
- Trace (`tests/traces/charge.*`, `tools/magiccheck.sh`): taps, holds of 20,
  50, 90 and 150 frames, a crouched hold and a hold through a jump. Willow
  813/813 frames; hold counter, meter, meter-active flag and release level
  825/825; shots (variant, x, y) 778/825 strictly, 825/825 with
  `OPTS=--tolerant`, which leaves out two accepted differences: (1) shots
  already past the screen edge are removed a few frames apart (the arcade's
  every-4th-update bounds test runs on a phase set by its frame counter and
  pool slot), (2) x 1 px apart: the arcade's spawn copies only the whole
  pixel of x and keeps the pool slot's old sub-pixel fraction.
- Sound: 0x31 / 0x44 as PCM on channel 3, stopped by 0xF4 on release (the
  music plays on, as in the arcade); shot on channel 1, landing on 2.
  Checked headless: channel 3 busy exactly during each charge.
- Packer: `MIRROR=1` makes a pose that is a horizontal mirror of an earlier
  one reuse its tiles with the sprites' H-flip (shot tiles 734 -> 436);
  identical poses share tiles (sparks 120 -> 24 tiles); "rADDR" packs one
  chain from a node; an empty node keeps one piece slot (fixes the chain walk).
- **Sprite VRAM** (`src/vram.c`): after the planes, the backdrop (384 tiles)
  and the HUD, sprite tiles live in gaps: 1345-1375, 1396-1407, 1408-1423
  (window nametable, unused outside the debug row), 1424-1535 (Willow 36 at
  1424) and the HUD's end to 1791; first fit. Plane B's nametable is
  0xC000-0xCDFF (tiles 1536-1647): a first version put shot tiles there and
  corrupted two backdrop cells (found in the frame comparison, fixed). At
  level 0: shots 122, sparks 24, 46 tiles left. The HUD now loads only the
  bar colours level 0 uses (87 tiles; all 135 with a higher level or items).
  M2's enemies will need the plan in NATIVE_ENGINE_PLAN.md (~240 sprite
  tiles) revisited: the backdrop took 384 instead of ~150.
- Frame time: `tools/frametime.sh` measures the work in scanlines with the
  raw V counter. SGDK's adjusted counter reads 16 anywhere in VBlank, so the
  earlier "median 16" meant "finished inside VBlank" and the HUD-pass
  figures (46/54/62) were in that adjusted scale. Now: work per update
  median 29 lines, max 45 (basic), 49 (slide), 48 (route), 74 (charge, with
  sparks and the bar), of the 262-line NTSC frame; it starts in VBlank and
  ends by display line 50. 60 updates/s.

Playtest build: `game/willow-native-m1-20261101d.bin` (charge magic: hold A,
release for a stronger shot; sparks; charge bar and sounds).

### Waterfall palette cycle and sounds (2026-11-01, fifth pass)
- Arcade cycle (`tools/cyclescan.py` over 64 per-frame dumps ->
  `data/stage1/cycle.json`): 39 scroll2 entries of groups 9, 12, 13, 14 and
  16 change every 8 frames, period 6 states (48 frames). Two six-colour
  families: blue water (groups 9/14/16 pens 1-6) and white foam (groups
  12/13 pens 1-6), each pen a phase of its family; 6 more pens run their own
  sequences (under 5,000 of ~135,000 cycling pixels in the stage).
- Native (`tools/stagepack.py` with `CYCLE=data/stage1/cycle.json`): six
  reserved CRAM entries per family, pens 10-15 of line 2 (water) and line 3
  (foam), one per phase; odd pens take the nearest phase of their group's
  family; tiles with cycling pixels are pinned to their family's line; the
  other colours are fitted to pens 1-9 of each line (was 15). `stage_cycle()`
  queues the next state's 12 words every 8 updates (`data/stage1/cycle.bin`).
  Fit error 0.90M -> 1.64M; side-by-side frames: the forest keeps its look
  (the rocks come out greyer, nearer the arcade's than the earlier bluish
  fit), the waterfall shows its four colours. Pool 939, backdrop 383 tiles.
  The HUD glyphs avoid pens 10-15 of the scenery lines (`hudpack.py`
  `STATIC=9`); the background colour comes from the pack (`STAGE_SKY`).
- Phase: the native cycle starts at play start (the arcade's phase depends
  on its opening scene, not built yet).
- Checks: the CAMTEST frames at the waterfall change every 8 frames;
  `tools/streamcheck.py` now compares Genesis levels (the preview's 7 is 252,
  the emulator's 238, which the old tolerance missed), skips the HUD rows
  and accepts cycle colours on both sides: along the camera path only the
  test build's Willow sprite and the lives sprites differ (127-858 pixels).
  Its coarse alignment can pick a view 10 px off near the waterfall (frame
  7800: 15,330 at x 2745, 857 at the right x 2735).
- Sounds: slide 0x2A and time warning 0x43 (every 32 frames under 0:10,
  4EA2) rendered from the arcade's sound program (`tools/audio/run.sh`) and
  played on PCM channel 2; checked headless (the warning from frame 10,268,
  when the clock passes 0:10).
- Traces 4/4 and charge state unchanged; work per update median 29-40
  lines, max 78 (charge) of 262.

Playtest build: `game/willow-native-m1-20261101e.bin` (waterfall cycle,
slide and time-warning sounds).

## Open for M1
- Charged spells 7-12 (magic level 1+, from shops; specs/magic.md) with the
  sprite VRAM plan for M2.
- Remaining sounds: hurt and death (with M2's enemies), the other stage
  cues; the shot's muzzle effect (class 2 kind 1).
- HUD: time-out outcome
  (specs/hud.md open question 1), score/gold awards (M2 enemies, pickups).
- Intro scene; the two misassembled variant-6 shot frames.

## Stage 1 end to end: boss, stage clear, intro (2026-11-02)

User request: "finish level 1 and intro end to end. then let's test".
Playtest build: `game/willow-native-m2-20261102.bin` (1,179,648 bytes).

### Boss arena (`src/fam_boss.c`, specs/stage1_boss.md)
- Sage (class 4 kind 7 variant 0): wakes at the arena camera (3328, then
  camera min locked there), boss music 0x01, clock stopped, the warning
  typed 3 updates a character (tick 0x2F) on plane B rows 4-8, 90 updates,
  pose 0x150FE, body after 38, leaves after 27. On a restart in the arena
  (boss already seen) the body comes at once without its rise.
- Body: a plane B picture (`tools/bosspack.py`): every pose (rise 4, loop
  8, collapse 4) composited offline into a 27 x 14 region at the fixed
  arena camera, cells with body pixels carry the priority bit (all sprites
  get it while the body shows). Tiles stream into foreground pool slots no
  arena column uses (two buffers of 228, 64 tiles a frame); the region's
  words are resolved offline per buffer (`maps.bin`) and DMA'd straight
  from ROM. Logic per spec 3: loop ticks, attack choice table by player
  column, rank timer, pending command, hold until the head/paw clears it.
- Head (32 energy, ENEMY bar on plane B row 2), fire breath 0x8D: 9 flames
  9..1 at 0x164DE offsets, 6 updates apart, live 24 + 4. Hit recovery 16
  (17 while breathing). Paw: shown at controls 2 and 6, claw swipe on
  command 0x80, absorbs shots. Hit flash palettes approximated by a blink.
- Death: sound 0x8E, scene lock, 40 bursts 6 apart (0x33), head and paw go
  at 192, collapse, 2000 points, 15 pickups (set 13), camera max 3456,
  stage exit after 200.
- Stage clear (spec 6.1, 6.2): 0x10, scripted walk right 94 updates,
  STAGE1 / CLEAR POINTS / TIME and LIFE rows counted +100 an update (0x30),
  total to the score, 120 updates, fade, story page (text only, plain black
  screen: the portrait is not in this build), then back to the intro.
- Not in this build: the scripted beast at the arena entrance (kind 1
  variant 3), the placed kind-6 object at 3776 (the body comes from the
  sage).

### Intro (`src/intro.c`, `tools/intropack.py`, specs/intro.md)
- World map: the arcade frame (dump t804) as one plane B picture, 572
  tiles, its own 4 CRAM lines; markers and Willow's figure baked (no
  blinking). Music 0x12, 3 + 48 fade + 128 hold + 48 fade + 7 (blocking).
- Forest: the real stage planes at camera (0, 512) panning 2 px an update.
  The sky above the map (30 rows) and the empty cells of map rows 0-15 (the
  far mountains and castle) come from a band rendered offline from the
  ROM's scroll2/scroll3 grids (cut 1: no parallax, the far layer taken at
  the camera showing each row mid-screen). Its tiles borrow VRAM: rows
  above the map use the pose cache's ranges (400 tiles; no sprite is on
  screen while they show; handed back at camera 272), rows 0-15 the
  backdrop's 192 slots (restored with plane B's words at camera 144).
  Colours: CRAM line 0 (Willow, off screen until ~105) fitted to the
  clouds, pens 10-15 of lines 2-3 (the waterfall cycle) to the mountains;
  both restored at camera 144. Plane B is blank (sky colour) meanwhile.
- Text (cut 3): window plane rows 0-3 (0xB000-0xB1FF, tiles 1408-1423 now
  kept out of `vram_alloc`) as a plain dark band; opaque glyphs (line 1,
  pen 1 background) loaded over the HUD's tiles, which `hud_init` reloads
  at play start. Messages, gaps and rates as spec 6.3; the dialogue box is
  the band (no frame or opening animation, the 17 updates kept); STAGE1 /
  CROSSROAD in big letters, 4 updates each.
- Windmill (0x325BC) and High Aldwin (0x32AA2, then 0x32ACA 222 updates
  after the pan) are sprites (`data/s1spr/windmill`, `aldwin`, colours
  from the forest dump through the stage sprite fit). Willow stands facing
  left, then the stage-start gate: facing right, 16 updates, control.
- Fade-in: done by hand, 16 levels x 2 updates (SGDK's async
  `PAL_fadeInAll` did not finish reliably here).
- Holding A or B runs the scene 4 updates a frame (fades excepted): a
  proposal, the arcade's real factor is unknown (spec 10.1).
- Measured: forest intro 898 updates as the arcade; no dropped frames
  after the (blocking) map scene.

### Fixes found on the way
- DMA queue: SGDK's default is 80 entries; a restart queues two windows of
  columns and overflowed it (garbage columns). Now 160.
- `actors_update` shrank a class's bound over an actor spawned past it
  during the same pass (the boss body was never updated).
- Clearing plane B with `VDP_clearPlane` writes 64 x 32 and wipes the HUD
  glyph tiles at 0xCE00; clear rows 0-27 only.
- `pose_draw` could write past the 80-entry sprite table.
- The idle-time spin (`frametime.sh` idle numbers) is now opt-in
  (`-DIDLE_MEASURE`); it made every frame take two VBlanks. Dropped-frame
  counts are measured on normal builds. Build e was not affected (60 Hz
  clock checked).
- Pose cache: a miss evicts every stale entry at once and searches once
  (one gap search per eviction cost ~40,000 cycles).

### Performance (dropped frames, headless)
- Bridge logs: resident art drawn inline, settled logs sleep (`IDLE`) until
  a foot or the collapse wakes them, cheaper foot probe; sparkles, gold and
  splashes resident. Route trace: 642 -> 25 of 2509 dropped (all at the
  sorcerer's entrance, ~0.5 s); basic/slide/charge 0.
- Boss fight: 22 of 2909 (was ~10% before the offline maps and the cache
  fix).
- Traces 24/24 fields and magic 825/825 unchanged. Test builds: `-DNO_INTRO`
  (traces), `-DSTART_CP=6` (arena start), `-DBOSS_HP=n`, `-DINVINCIBLE`.

### Open
- Sorcerer entrance still drops ~25 frames; the map markers' blinking; the
  story portrait; the arena's scripted beast; spearman, apparition, shops,
  companion; boss fur colour (grey for light blue); area backdrops.

## Stage 1 polish (2026-11-02, builds b and c)

- Trees (user: "can we fix these trees"): the backdrop's 192 tiles and its
  share of the scenery palette are now weighted by how often each backdrop
  cell shows through the foreground along the arcade camera path
  (`tools/stagepack.py`, `BD_FIT=6`): the teal forest and cyan sky are back,
  the flat dark blocks gone; the foreground lost a little yellow-green.
  `tools/regen_stage1.sh SCRATCH 6` rebuilds every pack that depends on the
  stage palette (planes, sprite fit and HUD, boss body, intro).
- Frame dips: bridge logs and sorcerer sparkles drawn in tight loops
  (sparkles are a particle list, not actors); pose cache free space kept as
  extents (a miss evicts only until a run fits); pad 2 not polled. Route
  0 dropped frames (was 25), boss fight 0-1.
- Boss: the fur takes the scenery's cycle pens 10-15 while it shows (the
  waterfall cycle waits); the hit flash swaps them with colours mapped from
  the arcade's flash palette (0xE6A60): only the fur flashes, its claws and
  horns use shared pens. The head no longer blinks.
- Warning box: the arcade's frame (text layer of dump t1446) on plane B rows
  2-9 with priority, opening from the centre over 17 updates; its 34 tiles
  load over the title glyphs while it shows.
- World map: stage marker I blinks (palettes 0x12/0x13, 10 updates).
- Story page: Madmartigan's portrait (dump E t2890, lines 0, 2, 3 fitted),
  text typed at rows 22-24 as the arcade's rows 24-26.
- Builds: `willow-native-m2-20261102b.bin` (trees), `...c.bin` (all above).

## Parallax: the arcade far layer on plane B (2026-11-02, build d)

User chose option (b) (AGENTS.md "Parallax restored").
- `tools/farlayer.py`: the stage's scroll3 as one 2120 x 224 picture (ROM
  grids, palettes from a dump). Measured on four arcade dumps: far pixel at
  screen (x, y) = picture (x + cam_x / 2 + 32, y + 132 - cam_y / 2); the game
  keeps cam_y = 0's rows.
- `tools/stagepack.py`: a picture wider than the screen is packed as the far
  layer, its map column-major (265 x 28); `BD_TILES=364`, `FG_MERGE=320`
  (pool 686 slots, was 858; foreground visually unchanged). Palette weight
  of a far cell = how often it shows through the foreground x `BD_FIT` (6),
  no minimum (a minimum of 1 per cell paled the foreground's greens).
  `tools/regen_stage1.sh` runs the whole chain.
- `src/stage.c`: far columns streamed into plane B rows 2-25 as plane A's
  are; `HSCROLL_TILE` (per 8-px row): rows 0-1 and 26-27 (HUD) at 0, the
  rest at -(view x / 2 + 16); tables queued only when the view moved.
  `stage_bword` / `stage_bput` (`_rom`) place plane B text and pictures
  where the far scroll puts them; `stage_bfixed` for full-screen pictures
  (world map, story page). The scroll table now spans 0xA800-0xAB7F (vram
  range 0 shrank to 1372-1375). DMA queue 256 entries.
- Boss body re-composited against the far layer at the arena (far x 1696,
  a whole column); its map rows DMA'd from ROM; the map swap takes a frame
  of its own.
- Measured: far layer vs the exact arcade layer, mean squared error 1.50 per
  pixel (3-bit channels), 43% of pixels exact (the old static backdrop vs
  its source: 2.29, 24%). Camera-matched frames against eight arcade dumps
  (start to waterfall) line up; dropped frames 0 on every trace script and
  the boss fight.
- Not yet: vertical far movement (needs the HUD off plane B); the arena's
  night palette (the arcade darkens everything at the boss); the straw hut
  at the stage start (an object, not in this build).
- Fix (build e, user screenshot of a checkered sky on the high ledges): moving
  the scroll table to row mode shrank the pose cache to 392 tiles, below the
  400 the intro's cloud band borrowed; the shortfall was mapped to VRAM tile
  0, the blank tile every empty foreground cell uses. The band is now packed
  for 360 tiles, and a shortfall reuses a loaded slot instead of tile 0.
  Checked: tile 0 stays blank through intro and play.
- Fixes (build f, user screenshots): (1) the sorcerer drawn in pieces: for
  a flipped node built from separate 16x16 pieces, `tools/posepack.py`
  flipped the pieces' pixels but not their positions (x -> -x - 16). The
  sorcerer's facing-right poses, and some of the archer's, arrow's and
  beast's, had pieces on the wrong side since they were first packed;
  `tools/regen_stage1.sh` now repacks the families too. (2) The HUD's top
  rows kept a fixed sky strip over the scrolling far layer (treetops cut
  off): they now show the far layer, re-laid when it moves a whole column
  (one row a frame on the next two frames), around the HUD glyphs
  (`stage_hudrow`). Route: 2 dropped frames (seven soldiers and a column
  load on one frame), fight 0.

### Shops (build g, specs/stage1_shops_companion.md 1)
- Shopkeeper at both stage-1 doors (class 12 kind 8, root 0x32C9C, family
  `shopkeeper`); touching him (box +-12 x, +-14 y, HP > 0) opens the shop
  once per stage (`shop_used`, cleared at new game).
- Screen: `tools/shoppack.py` from the arcade shop dump (vidA v006130):
  scroll2 shelf (icon cells blanked) under scroll3, recomposed from 384 to
  320 px (above the text box only plain curtain columns are dropped, so all
  12 boxes and the owl's window keep their pixels; the box loses 64 interior
  columns). 327 tiles at VRAM 16 (the stage pool / far layer, idle in the
  shop), plane B via `stage_bpicture` (HUD glyphs kept, hud_text writes over
  the picture). Lines 2-3 fitted to the background, line 1 the HUD's, line 0
  to the icon colours the others serve worst; each icon is four 16x16 sprites
  on their best line (icon palette table 0x3407E is words). Owl blink/talk
  frames as alternate cells (0x34AD4 timing); big G / digits / YES NO glyphs.
- Flow per 1.2-1.8: fade 59, music 0x1B (converted, loops from tick 282),
  32 updates, welcome typed, YES/NO, menu (right/left wrap, down/up wrap),
  purchases with the arcade's check order, persistent kind 2-7 prices (BCD,
  entry snapshot), markers, information dialogue, farewell, 63 updates, fade,
  stage rebuilt, 96 updates not hittable, stage music. The clock runs; time-up
  inside the shop returns to the stage, where TIME OVER runs.
- Deviations: text re-wrapped at 38 columns from column 1 (320 px); text in
  the HUD font colour (arcade palette 5); the 1UP/sword slots assume Willow
  (Madmartigan is not in this build); fades block updates (the arcade's
  entry fade also freezes them).
- Measured: shop visit 0 dropped frames outside the fades (show_item composes
  rows 20-23 once each; hud_text uses a glyph table); traces 4/4 x 6 fields
  match; route 2, spearman area 4 (was 16), fight 1 dropped frames.

### Vertical far-layer parallax (build h; AGENTS.md "for all levels ... all level of parallax")
- The far layer now moves vertically at the arcade's rate: screen line L shows
  far picture row (L + far_ty) / 8, far_ty = STAGE_FAR_TOP - floor(cam_y / 2).
  Checked against the arcade: in all 46 reference dumps scroll3 y =
  1408 + ceil(scroll2 y / 2), the same law; best-match offsets of the arcade's
  own scroll3 against the far picture at cam_y 15 / 140 / 175 / 239 agree
  with the formula within 3 px (sampling granularity).
- Plane B is a 32-row ring (picture row r -> plane row r & 31); 26 far rows
  resident (lines 16-207); the HUD's four rows (screen rows 0-1, 26-27) live
  in the next ring rows and are shown by two mid-frame vertical scroll splits
  (VInt: lines 0-15; H interrupts at lines 15 and 207; three a frame). The
  foreground stays visible behind the top HUD as before. HUD tiles moved from
  0xCE00 to 0xD000 (plane B needs all 32 rows): 16 fewer sprite tiles.
- The splits are off whenever the main thread streams to the VDP by CPU (they
  would corrupt it): fixed pictures (map, shop, story), the intro, the
  foreground reload; SGDK DMA fills are not interrupt-safe either, so pictures
  are set before plane A is cleared.
- Per-frame nametable and scroll-table writes (both planes' columns and rows,
  the HUD's rows, the hscroll tables) are a list copied by the CPU in VBlank
  (stage.c vq), not SGDK DMA queue entries (several hundred cycles each);
  a full list flushes in order. Moving the HUD rows reuses their laid-out words.
- Not shown: the arcade's cloud band above the far picture's top (seen only
  from the highest cliffs) - plain sky colour there. Measured: a 368-row far
  picture needs ~100 more VRAM tiles (the sprite cache has none to spare:
  100 fewer tiles -> refused pose draws) and its whites/blues take scenery
  pens from the greens and browns (main-band colour error 4.2 -> 8+). The
  tools support it (FAR_TOP, FAR_POOL streaming in stagepack.py) for stages
  with room.
- Measured (headless): traces 24/24 fields; dropped frames route trace 1,
  full route run 4 of 2600 (1 without the feature), spearman area 9 of 1700
  (7), boss fight 4 of 2200 (4); idle-time p1 route 17.3k cycles (19.1k
  without), spearman worst frame 16% idle.
- Build i: the per-frame plane and scroll-table writes (both planes, HUD
  rows, plane B text and the boss body) are one ordered list sent by DMA in
  VBlank with the VDP registers set directly (stage.c vq; a full list goes
  at once, in order). Mixing it with SGDK's queue put old far words over new
  ones on restarts and shop text under its row clears, so plane B writes all
  use it. Profiles of deterministic idle builds: the vertical far layer and
  splits cost under 1% of a frame.
- HUD: the charge bar's 28 tiles are reloaded with the magic level's colour
  groups; the item icons (24 tiles) came from the far layer (339 tiles now,
  BD_TILES=340) - taking them from sprite VRAM brought back refused pose draws.
- Object pool 12 (was 8: chests, two shop doors and the hidden object filled
  it); pickups 20.

### Hidden companion (build i, specs/stage1_shops_companion.md 2)
- The hidden object now places itself at stage start (it slept at x 0 and
  never woke after a checkpoint restart). Destroyed, it frees the companion
  (`fam_companion.c`): it hops until Willow comes near (a Quest-Stone is
  spent), then follows / paces / runs / leaps / hops / dashes as 2.3, and
  throws a spear (shot.c: 4.5 px/f for 20 updates, then falls; power 1; the
  shots' impact animations) on a new attack press, also in the air.
- Deviations: falling out of the stage (a pit) puts it back at Willow's side
  (arcade behaviour not established); a life loss removes it (UNKNOWN in the
  arcade); FF83DC drift not ported.
- Checked headlessly (-DTEST_HIDDEN=1060 -DTEST_HIDDEN_Y=84, START_CP=1):
  appears, joins, follows, throws.

## Stage 2 (in progress, 2026-11-02)
Spec: specs/stage2.md; Madmartigan: specs/madmartigan.md (being written).
- Far layer: same law as stage 1 (scroll3 y = 1408 + ceil(scroll2 y / 2),
  x = scroll2 x / 2 - 32; checked on the stage-2 dumps); grid 1, repeats every
  4096 px (StageData.far_wrap), FAR_TOP 136 (the camera's full vertical range;
  the sky dedupes), 924 distinct cells merged to 339.
- Fort map: grid-1 cells 0..127 (data/stage2_fg.cells, x 0..2047), pool 396
  slots (fits stage 1's 687), packed with the far layer at stage 1's base
  (POOL_SLOTS=687 for future packs); collision = same descriptors as stage 1.
  `-DSTAGE2_VIEW` test start: the fort renders (Willow standing in).
- Plan: per-stage sprite art (shared families keep poses/anims/boxes, take
  stage-2 tiles and palette; new families' full art), game.c per-stage tables
  (objects 3.1, events 3.2, checkpoints 1.6, clock 3:30, music 2), the fort
  families (chandelier, crate archer, shutters/crossbowmen, lurkers, spearman
  v8/v2 + returning spear, torches, breakables, shop 6), Madmartigan, the wagon
  chase (camera mode 8, road loop on plane A, far layer scrolling at half the
  virtual X - the user's "all parallax" settles open question 6 - and the ten
  dusk palette steps), chase families (mounted archers + horses, boarding
  soldiers, hawks + rocks, hound), clear / results / story, and the link from
  stage 1's clear (map scene, title).
- Done (2026-11-02, test builds): stage 1's story leads to stage 2 (its
  world map with marker II blinking and Madmartigan's figure, the stage
  switch, "STAGE2 / CHERLINDREA'S FOREST" typed, music 0x02); score, lives,
  gold and items carry over; stage-2 checkpoints (1.6). Placed soldiers,
  archers, chests, gold in stage 2's colours (data/s2spr, tools/pack_stage2.sh;
  shared families keep their poses, the fit makes their tiles and lines;
  sprites may borrow line 2 only). HUD recoloured per stage
  (data/hud2, LINES_FROM keeps the HT_ constants).
- The chase (fam_wagon.c, stage.c chase mode, tools/chasepack.py): the fort
  on line 2 (stagepack SPLIT_LINES), the far layer the stage's only line-3
  user, clustered so its ten dusk steps are palettes (the whole stage uses
  the same far tiles: nothing reloads when boarding); road blocks by the
  ROM sequence in the 290 pool slots the fort never uses. Boarding fixes the
  actors' camera at 1536, the virtual camera scrolls the road and (at half)
  the far layer; drift, jolts (bumps from the resting height), the end run,
  the breakdown, the road stopping, results x200. Camera y held at 0.
  Music 0x03 converted (posted by the tower leap, not yet ported).
- Not yet: Madmartigan (specs/madmartigan.md), the fort's own families
  (chandeliers, crate archers, shutters/crossbowmen, lurkers, door
  spearmen, torches, breakables), events (edge soldiers, pairs), the
  tower trigger and leap, chase enemies (riders + horses, boarders, hawks
  + rocks, hound), Cherlindrea's story page, the shopkeeper's stage-2
  colours, a restart inside the chase.
- Madmartigan plan (specs/madmartigan.md): a form of src/player.c, not a
  second player. Art: posepack nodes 70..139 of the player-hold manifest
  (palette 1, a stage-2 dump) -> data/mad; pose ids by semantic table per
  form (walk 0-5/6-11, crouch 12/13, stand 14/15, attack 16/18, follow 17/19,
  crouch sword 20/22 + follow 21/23, air sword 24/26 + follow 25/27, jump
  28/29, drop 30-33/34-37, climb 38-42/43-47, slide 48-49/50-51, recover
  52/53, road death 54-57, death 58-63/64-69 = manifest id - 70). Probes:
  feet (+-12,-24), walls as Willow's, head +24/+8; collision radius 24.
  Sword: 12 updates of pose (4 attack + 8 follow), a sword object in shot.c
  (spawned the update after the press, visible after 4 with sound 0x26,
  images 0/1 four updates each at player + (+-48, +16 / 0 crouch / +24 air),
  box 0xE78C by level and facing in the arcade's unsigned test, power
  0xE7DC[level*4 + tier], not consumed; cuts arrows and rocks). Charge:
  meter +0x11F a held frame to 0x8FFF, release -> a second swing with tier
  0xDE0A[(L+1)>>3] and the extension (4 px an update for tier x 4 updates,
  a spark each update). Slide: 1792 - 64/update, 28 updates; recover 6, no
  jump. Hurt 0x84, no knockback; death 60 updates on 10-tick nodes, 0x85.

### Madmartigan (build j, 2026-11-02)
- `src/player.c`: a `Form` table (art, pose numbers, probes, swing length)
  selected by `player_form()` (kept by `player_init`); `game_next_stage`
  picks form 1 (`-DS2_FORM=0` test builds play stage 2 as Willow),
  `game_new` form 0. Willow's path is unchanged (traces 24/24 fields).
- Madmartigan's own code: ground/air/crouch swing (12 updates; the pose
  follows at the 5th; a jump press cancels it; landing mid-swing finishes on
  the ground; the air-follow pose stays until landing or a turn), slide
  (0xE02A: 28 updates / 101 px, 6 recovery, no jump), drop (0xDD04: 6 + 12
  updates without a floor test), climb (0xDF40 spring 1344 - 56; measured
  65 px onto the stage-2 gallery at x 684), ledge test (+-8,+40) with the
  ceiling pre-test behind the head; the head-hit flag is the latest
  update's head probe (39FE runs every update) - a sticky flag looped
  grab/abort under the gallery's lip. Hurt 0x84 / death 0x85 (60 updates),
  hurt flash 0xE5020, meter +0x11F to 0x8FFF, tier 0xDE0A (`magic_tier`).
- Sword (`src/shot.c`): spawned with the swing pose, shown and hitting 4
  updates later (sound 0x26), images 0/1 for 4 each at player + (+-48, +16 /
  0 / +24) by posture at spawn, box 0xE78C, power 0xE7DC by `sword_level` and
  tier, one target per update, not used up; tier x 4 extension updates at
  4 px each. Cuts arrows (`arrow_break`). Art: data/s2spr/sword (12 poses:
  levels 0-1, 2-3, 4), coloured with 0xEA98 row 0.
- Player VRAM slot 36 -> 43 tiles (his largest pose); mirrored left poses
  (H-flip bit in the entry) now honoured by `player_draw`.
- Audio: sword 0x26 (FM, rendered), OKI 0x84 / 0x85 added (`tools/audio/run.sh`).
- Measured: stage-2 route 2,300 frames (walk, swings every 16, jumps,
  climb): dropped frames 3 (the Willow stand-in 9).
- Not yet: the sword's colour steps and sparks during the extension (5.3,
  5.5), the road death (2.9), the HUD's Madmartigan form (sword icon, 18-cell
  bar), sword levels in the shop, the scripted wagon leap / chase-end walk
  (2.10). Sword pieces were fitted half to line 1, half to scenery line 2
  (colour approximate).

### Stage 2 complete pass (build k, 2026-11-02)
- Madmartigan's HUD: 18-cell bar coloured by sword level, the second
  bracket with the 4x2 sword icon (glyphs 0x77C0.., `hudpack.py` HT_SWORDn,
  8 tiles streamed after the item icons). Shop: his swords (levels 1-4; Willow
  "YOU CAN'T USE IT"), Willow's magic refused to him, slot 10 his 1 UP, the
  stage's music after the shop. `pack_stage2.sh` re-runs the stage-2 HUD
  pack after every sprite fit (its glyphs share sprite line 1).
- Tower trigger (1544, 400) -> scripted leap (player life state 12: fall,
  16-frame walk, leap toward 1728, music 0x03, clock 3:00); chase end walk
  (state 16) to the screen centre. Road death (wagon 0x2FE5A): 96 updates
  on the road-death poses, carried off by the road. The wagon deck places
  both forms' origin at y + 32 (Madmartigan's foot probe asks at Willow's
  height).
- Chase restart (1.6): a death after full speed resumes at the last
  4096-px mark of the virtual camera (1728 at first), checkpoint 9, 3:00,
  music 0x03, the wagon at full speed when boarded. The sprite table is
  blanked before a restart's blocking fade-in (stale sprites over reused
  VRAM showed as garbage).
- Fort: events 0-2 (edge soldiers with stage 2's entry rules, soldier
  pairs every 240 frames), falling chandeliers and their five flames, crate
  archers, window shutters releasing crossbowmen (aim by 16 directions,
  allowed 4-7/9-12), window lurkers (+ debris), door spearmen (variant 8
  placed, variant 2 from event 3 after each death, door effect, then the
  axe thrower's code), the axe in stage 2's colours.
- Chase (fam_chase.c): mounted archers (variants 0/1, keep-out band,
  aggression, one-arrow variant 1), their horses (root 0x30400, all on one
  shared pose), boarding soldiers (strike / board the deck / return to the
  saddle, leap attack, thrown off), hawks carrying rocks (released over the
  player, swoop), rocks (on the road they slide away), the hound (leaps onto
  the deck), events 4-9 with the ROM tables (distance windows, rank
  timers, caps).
- Story page: Cherlindrea (scroll2 grid 7, `tools/storypack2.py`), messages
  0x2D (4 lines) and 0x2E (one 61-character line, broken at a word for 40
  columns); then the run ends (stage 3 not built).
- Sounds added: 0x37 flame, 0x3A + 0xA1 chandelier crash, 0x8B axe/spear
  throw (stage 1's was missing too), 0x90 boarder, 0xA0 lurker.
- Pose packer: per-root palette override `ADDR@G` (chandelier flicker 19/20),
  `SPR_COST` (pieces vs tiles).
- Performance work for the chase: the fort's pool tiles (VRAM 16..412) are
  lent to the sprite cache once the road fills the view (refused draws 3027
  -> 374 frames of 6000); pose loads capped at 96 tiles while lent (a VBlank
  overrun delayed the line-15 HUD split: 12 of 401 frames at 160, 0 of 801 at
  96); horses in one shared slot; HUD rows over the wrapping far layer
  without per-word division (a third of a frame each); road columns copied
  from words that carry their VRAM base; a 68000 sprite-copy loop.
  Chase drops (S2_CP=9, swings every 30): 2372 -> 292 of 5800, worst 31 per
  100 frames when riders, a boarder, hawks and the hound are all on screen.
  Per-update work there: p50 100k, p99 121k cycles (enemies not drawn: 88k;
  no enemies: 71k). Not solved yet.
- Stage 1 unchanged: traces 24/24 fields; route 9 of 2509 dropped
  (invincible, as before), spearman area 3, boss fight 6.
- Not yet: wall torches and breakable scenery (5.8, need plane-A tile
  animation), the sword's colour steps and sparks (5.3, 5.5), rider arrow
  variant 1's own art, horses reacting to the chase end, Cherlindrea's glow
  cycles, stage 3+.

### Front end and saves (2026-11-02, AGENTS.md "Home/Arcade frontend")
- `src/frontend.c`: power-on selector (HOME / ARCADE, "PORTED BY RESTER159,
  2026"), each mode's page (Home: START GAME, NEW GAME PLUS once unlocked,
  OPTIONS, HIGH SCORES, CONTROLS; Arcade: ENTER ARCADE, DIP SWITCHES, ...),
  options / DIP switches (difficulty, lives, vitality, continue, Nando speed,
  magic continue, demo sound, coin A/B), music select (the converted tracks),
  high scores per mode, controls; the arcade's text font
  (`tools/fontpack.py`, glyphs 0x7000 + ASCII, text palettes 0 and 2).
- Settings reach the game: lives, vitality, starting rank = difficulty x
  0x1000 (0x1B3E; default 3 -> 0x3000 as before), continue off -> GAME OVER
  at once, magic continue (magic >= 1 entering stage 2, 2E1C). Test builds
  (NO_INTRO) use Arcade's defaults (the traces' DIP values).
- Save RAM: two copies of one record (magic, version, sequence, both
  modes' settings and top-10 tables, NG+ flag, checksum) at SRAM 0 / 256; a
  save writes the copy the newest record is not in; a load takes the newest
  valid copy. Paged in by 0xA130F1 for each access, interrupts masked (the
  ROM is over 2 MB now). The ROM header is a plain "SEGA MEGA DRIVE" one
  (`src/boot/rom_head.c`): SGDK's ENABLE_BANK_SWITCH wrote "SEGA SSF", which
  makes emulators / EverDrives treat 0xA130F1 as the extended-SSF bank-0
  register: the save RAM switch remapped the running code (the "hang").
- Home: Start pauses / resumes (not the launching press), the music pauses;
  a run's end (game over, or the end of the built stages) -> a qualifying
  score's entry screen (provisional: three letters) -> the menu. NEW GAME PLUS:
  enemy energy doubled when set, shop prices doubled; unlock hook
  `frontend_unlock_plus()` (to call at the final ending on one credit).
- Arcade: for now the same flow; the arcade's attract, coins, credits and
  name entry come from specs/arcade_frontend.md (in research).
- Checked headlessly: menus, option change saved and reloaded across runs,
  Home start -> intro -> play, pause / resume.

### Charged spells 7-12, soldiers' footing (2026-11-02)
- `src/spell.c` (specs/magic.md 4): piercing bolt (7), rising bolt (8),
  flash (9: white screen 3 frames, enemies on screen with energy <= 8 die
  for 9 frames), time stop (10: 128 frames, ordinary enemies, enemy shots and
  moving surfaces frozen, clock stopped, Willow not hittable, plane A rows
  waved by 0x6A46 >> 4 per 8-px row - the arcade's per line), two fire
  walkers (11), blast (12: 160x160 box, power 20, cancels arrows, fireballs,
  rocks; 8 radial pieces). Art `spell` family (pack_stage1.sh, palettes 8, 9,
  13); sounds 0x21-0x25. Collisions: pierce takes min(power, energy) and the
  bolt goes on with what is left; once = the existing hit-state rule.
  Approximations: the flash's "admitted" = on screen; bosses not frozen by
  time stop; boss parts other than the body take spells like enemies.
  Sprite fit: `sprpal.py` takes `DIR=wN` weights (the spells at weight 1 made
  the enemies' total error slightly lower: 419k vs 429k).
- User (screenshot): a soldier's feet on the very edge of a rock. The
  arcade's floor test 0x3B0A probes one point at the forward foot from table
  0x3B66 by probe set (+0x13): soldier set 0 = (+-8, -25), archer set 3 =
  (+-12, -24) (set 1 = (+-8, -16) once hit), beast set 4 = (0, -17),
  spearman set 5 = (+-12, -32). Ours used (+-8, -16) for all three: soldiers
  stood 9 px low, archers 8 px. Now as the arcade (rendered arcade frame
  v002800 vs ours on the same rock: feet a few px inside the top face).

### Arcade screens between plays, life-loss world map (build m, 2026-11-02)
- `src/screens.c` (specs/arcade_frontend.md 4-11), both modes:
  - Life loss (6): sound F0, fade, the world map (128-frame hold, music
    0x12), the stage reloaded at its checkpoint (`game_reload`: art, planes,
    HUD, stage music), no story. Was a fade back in place.
  - Lives out: qualification (a tie ranks above) -> the arcade's name entry
    (letter grid with RB/ED, cursor starts on ED, timer 20 every 48 frames,
    idle 420, highlight every 5, repeat 10, music 0x18) -> ranking (music
    0x16, new row cycling, 120 frames, any input ends) -> CONTINUE (big
    letters, 9 for 104 frames then 52 each with sound 0x42, A/B below 8 skips
    to 0, Start held continues, 64-frame grace, music 0x15) -> GAME OVER (120
    frames, music 0x14). A continue: lives = DIP, score 0, energy full, the
    continue count kept (FF839B), world map, checkpoint. Home continues free;
    Arcade takes a credit.
  - Time over: music 0x14 added (label stays the HUD message).
  - Arcade: C inserts a coin at any time (counted on release, 2..119 frames
    held; 120+ = jam), Coin A ratio, credits 0..9, sound 0x3B; the credit
    screen (PRESS START BUTTON 8/8 blink, 1 PLAYER ONLY / 1 OR 2 PLAYERS,
    CREDIT n) before each Arcade game. After game over: credits left -> the
    credit screen, none -> the Arcade menu page (stands in for the attract
    loop, not built yet).
- Fonts: `tools/fontpack.py` now also writes the big 2x2 letters and digits
  (`big.bin`) and the arcade's 32 text palettes (`textpal.bin`); each screen
  loads the four it uses as CRAM lines with the arcade's palette numbers.
- Layout: arcade cell (c, r) -> (c - 4, r - 2); CREDIT moved to columns
  31-38 (the arcade's 37-45 are off a 40-column screen).
- Not shown: the old man lying on the continue screen (class 12 kind 3).
  Replaced: the provisional 3-letter entry screen in frontend.c.
- Checked headlessly (poked lives, score, clock): time over -> world map ->
  restart; lives out with 60000 -> name entry (ED) -> ranking (TOP 60000;
  matches the arcade's v004760 frame) -> countdown -> GAME OVER -> Home menu;
  Start during the countdown -> world map -> play with 3 lives; Arcade coin
  -> credit 1 -> Start -> game. Traces 24/24, magic 825/825 (shots 778
  strict, as before).

### Stage 3: Fin Raziel's island (build n, 2026-11-02; specs/stage3.md)
- Data (tools): `stagemap.py` grid 2 rows 5-6 -> data/stage3_fg.cells;
  `farlayer.py` FAR_WIDTH=3264 FAR_TOP=144 FAR_YOFF=128 grid 2 (new FAR_YOFF:
  the stage's camera starts at y 256); `stagepack.py` POOL_SLOTS=687 FAR_TOP=144
  BD_FIT=6 FG_MERGE=320 BD_TILES=339 with the camera of run B (RAM dumps, cam y
  - 256); `colpack.py` (slopes 24-31 need nothing new); `stagetable.py`
  YOFF=256 NAME=stage3_objects; `intropack.py` MAP3 (world map, marker III,
  Willow by the lake; the title font gained Z and 3 in place);
  `tools/pack_stage3.sh` (25 sprite families, the fit, HUD recolour data/hud3,
  Fin Raziel's page via `storypack2.py` STORY=3, the storm states);
  `tools/audio/run_s3.sh` (music 0x04, 0x05, 0x06, 0x16, 0x18; effects 0x3E,
  0x8F, 0x91-0x95).
- Native y = arcade y - 256 throughout stage 3 (checkpoints, objects, water
  line 52, decks 60, boss waypoints 96..208).
- Engine: stage_index 2 everywhere (checkpoints 0x31C0 / 0x3190, music by
  checkpoint, results x300, title "STAGE3 / FIN RAZIEL'S ISLAND", story
  pages); Willow's shots, sparks and spells recoloured per stage
  (shot_art/spark_art/spell_art); `src/fam_stage3.c`: bomb troopers + bombs /
  shards / bursts, lance knight + 8-segment lance (FF83E6 encounter, no lock),
  sinking pillars (stood on: 1.6 px/frame), Willow's boat (follows her x,
  clamps x >= 2832, joins a restart on the lake, drawn first so the per-line
  sprite limit cannot drop it), enemy rafts with archer / spearman crews
  (quota one per side, escort rule, sink when both are gone), leaping fish
  (3 leap types, dive, surface), hawk variant 1 + rock, the three events,
  the sea swell (view offset +-10.5 px, 172 frames, from camera x 3872), the
  storm (camera x 3456..6272: day -> night 48 frames, lightning sequences,
  thunder 0x8F every 300, back to day after the boss; `tools/stormpack.py`
  maps scroll3 palettes 16-17's 8 steps onto the 9 scenery CRAM entries their
  pixels own - palette 17 changes too, the spec says it does not);
  `src/fam_boss3.c`: the arena (x 5760: FF83D0, camera glide 2 px), the
  dialogue (cut 3 rows), waypoint paths A/B with speed phases, bolt (32
  directions, 5 px), cast / summon glows, 4 x 6-segment serpents on the 24 ROM
  paths (bodies touch-only), 40-update invulnerability, death (16 bursts,
  5000, 15 gold), the clear 200 updates later.
- Approximations / not yet: the water shimmer cycles (scroll2 10/11, scroll3
  18); the apparition event; the hidden object's stage-3 places (not placed);
  the knight's reward platform; the ending figure and the arena object (the
  clear follows the exit timer); trooper choice by rank row 0 only; raft crews
  on fixed attack timers; bob as a triangle; hit flashes; the fish spray.
- Checked headlessly: start, cliffs, sea pillars, islet knight, shore shop,
  boat ride with rafts / fish / storm / swell, arena dialogue and fight, the
  clear, results, Fin Raziel's page, stage 2 -> 3 hand-over (forced clear),
  life loss on the lake -> world map -> restart on the boat. Traces 24/24,
  magic 825/825.
- Measured (dropped frames per 100, INVINCIBLE, walking right + shooting):
  meadow with troopers ~4 (one spike of 26 at the start); lake 0-8, with the
  swell + fish + rafts 20-37 (fish drawing and the swell are the cost; to fix).
- Build o (2026-11-02): the water shimmer (tools/stagepack.py CYCLE accepts a
  six-entry family plus a three-entry one; data/stage3/cycle.json from ROM
  0xEA7A0 / 0xEA880; the storm writes pens 1-9 only), the hidden object's
  stage-3 places (FF83A5 bit 1) with the companion in stage-3 colours, stage-3
  chest rewards (0x37EA6 + 0x5C), dropped gold that falls into the lake
  removed, the boat drawn only on screen (sprite x wraps).
- Speed (all stages): spell_test / spell_cuts / spell_update / spell_draw and
  shot_test exit at once when nothing is live (they scanned 20 / 8 slots for
  every enemy and projectile); the boat/raft bob without libgcc divides; the
  boat's tilt re-rooted only when it changes; the hidden object tests shots on
  alternate frames. Stage 3 drops per 100 frames (walk + shoot): meadow 0
  after the start (4-7 in the first 300), lake 0-6 (was 20-37), arena 0.
  Chase work unchanged (p50 97k, p99 117k).

### Arcade attract loop (build p, 2026-11-02; specs/arcade_frontend.md 3)
- `tools/attractpack.py`: seven pictures from run A's dumps (cave + Bavmorda +
  Willow, the two framed portraits, the WILLOW logo with Lucasfilm's notice,
  the logo with Capcom's notice, the FBI seal, the Eborsisk), composed at 384
  pixels and resampled to 320 (the logo and the FBI line fill the arcade's
  width), CRAM lines 1-3 (the portraits 2-3), line 0 the typed text's.
- `src/screens.c` attract_loop: the opening's red lines, the panels' typed
  text and yellow names, the yellow lines, the logo (sound 0x81) with INSERT
  COIN blinking 16/16, the FBI line, the demo, the interlude's lines, the
  ranking (180), again. A coin -> the credit screen; A+B+C+Start -> the
  port's menu (a port addition: the arcade has no way out).
- Demo play (3.2): stage 1's boss arena (checkpoint 6, gate open, the boss
  seen, magic 1, energy 5/5, 1:00, music word 1) and stage 2's chase
  (Madmartigan, chase armed at the virtual x 0x10C0 -> the 4096 mark, 3:00)
  alternate; the recorded input tables (ROM 0x4800E / 0x4820E,
  src/demo_inputs.h) replace the pad for 900 frames or until a life is lost;
  no score; demo sound DIP honoured; the stage's title over the frozen play
  for 180 frames (the HUD's message rows), then the interlude. The native
  game is not the arcade's bit-exact simulation, so the replay drifts from
  the arcade's (as the spec expected).
- Not shown: the cave's lightning cycle and Bavmorda fading in over the cave
  (one picture), the logo's white flash, the Eborsisk's fire, the
  power-on self-test text.
- Checked headlessly: two full loops (both demos), a coin in the attract and
  in a demo -> credit -> Start -> the game. Traces 24/24, magic 825/825.
- Build q (2026-11-02): Nando speed (rank growth: slow +1 every other update,
  normal +1, fast +2, very fast +3; 0x380E8); stage 3's ending figure (Fin
  Raziel as a possum: runs in at 2 px from camera x + 416 once the clear
  starts, holds 40, the burst 24, stands 120, then the results; palettes
  29-31 from dump v016250). Traces 24/24.

### Stage 4: Sorsha's camp (build r, 2026-11-02; specs/stage4.md)
- Tall stages: StageData gains world_top (511; stage 4 8191), far_shift (1;
  stage 4 3: the far layer at camera / 8), pool, bd_base, bd_count (stage 4:
  pool 281 slots, far 743 tiles at 297..1039). The engine keeps a virtual view
  y (272 - camera y) for sprites; stage.c converts it to the map (+ world_top
  - 511), RMAX = the map's last band. Map and collision are trimmed columns
  (tools/maptrim.py: u32 offset, u16 first row, u16 rows; --col for the ids;
  stage 4: 4.2 MB -> 153 KB map, 512 KB -> 22 KB collision; stages 1-3 also
  trimmed: -117 KB). terrain_id() is the one collision lookup (actor.c; the
  player's and shots' use it). Player camera y limits are per stage
  (cam_min_y, cam_max_y; the pit is below cam_min_y).
- Tools: stagemap.py mode 2 = 32 x 64 blocks; stagepack.py WORLD_TOP,
  FAR_SHIFT, FAR_WRAP (a 256-px far picture repeating), tall-map preview at
  1/4; farlayer.py grid 3 FAR_WIDTH=256 FAR_TOP=670; titleglyphs.py appends
  big title glyphs (Z 3 M P 4); the intro font loads only the title glyphs
  for later stages' titles (title_off) so it stays below plane A; MAP4 world
  map; pack_stage4.sh (27 families); run_s4.sh (music 0x07, 0x08, 0x09;
  effects 0x28, 0x39, 0x96-0x9A, 0x9E, 0x9F); storypack2.py STORY=4.
- src/fam_stage4.c: camera limits (min y 4096 -> 4592 at camera x 2304 ->
  5360 at 4609 -> 0 on the ride; max x 3584 / 6528 / 16000), the dragon arena
  (camera kept in [3392, 3584]), Sorsha's arrival glide, music triggers (2656
  cave, 4352 stage), falling snow (24 sources, three depths, resident art, 24
  flakes at most), events 24 (bomb trooper from the right), 25 (soldiers from
  behind), 26 (camp edge soldiers), 28-30 (tents), snowballs (grow three
  sizes, then bounce), bats, swooping hawks (the ROM path 0x18D62), tent
  spear throwers and spears, the dragon (body, neck, head by step, fire
  emitter + 7 flames, bursts, 10,000 + 15 x 100 gold), Sorsha (walk, sword
  combos, jump slash, parry backstep + clash, defeat, her line, the walk to
  the shield), the shield ride (Left brakes, B jumps, the 86 ride pickups
  placed as the ride reaches them), the crash snowball, the village end scene
  (horse, rider) and the results (x400, two Galladoorn pages).
- Soldiers never change level in stage 4 (stage4.md correction 4).
- Storage: the OKI voices are stored at 6,650 Hz and played at XGM2's half
  rate (the chip's own rate is ~7.6 kHz); tools/audio/trim_sfx.py cuts the
  FM effect renders to their arcade length + 0.5 s (their 3-s release tails):
  3.86 MB -> 3.51 MB of ROM content.
- Not yet: the stage-4 intro scene (horse group, walk-in; the stage starts
  at its checkpoint after the map and the title), Sorsha's ROM decision table
  (a distance rule here), the dragon's later waypoint rows, the slope classes
  of the ride (one speed class), the snow spray, the hidden all-pickups
  bonus, the camera too low during the title typing (48 px).
- Measured (dropped frames per 100, walk + shoot): slopes 0-1 after the
  start (the snow at 1 a frame through pose_draw cost ~29% before the
  resident path), cave 0-7, camp 0-6, the ride up to 28.
- Build s (2026-11-02): the stage-4 intro scene (the horse with Willow and a
  soldier gallops by at 3 px, Madmartigan walks in from x -32 to 176 from
  update 125, hoofbeats 0x39 every 32 updates; the title after 256 updates);
  the camera no longer drifts during a later stage's title (stage 1's intro
  pan ran for every title: stage 4's camera starts at 4096); aPLib-packed
  scene pictures in 256-tile chunks (tools/pack_compress.sh, src/packed.c:
  world maps, story pages, the shop, the attract pictures: 237 KB -> 115 KB).
  ROM content 3.40 MB. Traces 24/24, magic 825/825.

### Metatile maps, Sorsha's table, dragon rows (2026-11-02)
- Plane-A maps are 16x16 metatiles (tools/maptrim.py --meta ROWS: a
  dictionary of unique 2x2 word blocks, per 16-px column the trimmed
  metatile indices; data/stageN/map_meta.bin; src/stage.c map_word() and
  put_column(), which builds a column two rows a metatile). Maps 1-4:
  234 KB -> 102 KB (stage 4 153 KB -> 56 KB). Checked: every cell of the
  four maps decodes to map.bin's word; the CAMTEST stage-1 path, frames
  1500-1640, every new frame equals an old-format frame pixel for pixel.
  Dropped frames per 100 (stage 4 walk + shoot) equal to the old reader's.
  ROM content 3.25 MB.
- Sorsha's decision now uses the ROM table 0x28AA2 (src/sorsha_table.h: 34
  lists of 16 phases by distance class and rank row, picked by rand & 15)
  instead of the distance rule; the dragon's later waypoint rows (three rows
  by time in the arena: < 1280, < 2560, later; 4 waypoints a row picked by
  rand & 3).
- Fixed: the falling-snow source offsets were s8 (values up to 392 wrapped):
  the flakes now fall across the whole screen width.
- Magic check: shots strictly 760/825 (was 778); every difference is a shot
  already past the screen's right edge (accepted difference 1); --tolerant
  825/825. It predates the metatile change (same with the old reader).
- Build t (willow-native-m2-20261102t.bin, 3,407,872 bytes, content
  3,277,297): the above, and the Home/Arcade selector no longer shows stage 1
  on its right half (the stage set-up's queued column writes landed after
  the menu was drawn; builds p-s had it; the menu now waits one VBlank before
  clearing, as screens.c does). Checked headless: menu, Home start -> story
  -> play, the Arcade page. Launched.

### Stage 5: Tir Asleen castle (build u, 2026-11-03; specs/stage5.md)
- Data: `stagemap.py 4 5 3` (grid 4 rows 5-7, world y 0..767: WORLD_TOP 767,
  native y = arcade y); `farlayer.py` FAR_WIDTH=3232 FAR_TOP=216 grid 4 (far
  layer at camera / 2 both ways); `stagepack.py` BD_TILES=520 BD_FIT=3 (no
  foreground merging: pool 493 + far 454 tiles; merging showed as striped
  walls) with run B's camera plus a synthetic sweep over the upper corridor
  (never visited in the runs); `colpack.py`, `maptrim.py --meta 96 / --col
  48`; `stagetable.py 4`; `tools/pack_stage5.sh` (22 families; what shows in
  the boss fight pinned to sprite line 1); `tools/audio/run_s5.sh` (music
  0x0A, 0x0B; OKI 0x9B-0x9D, 0xA8); MAP5 world map; title glyph 5 (big font
  0x7B08: titleglyphs.py codes are 0x7000 + the font code).
- Player select (1.3, src/screens.c select_screen): the prompt typed, the
  10-count (32 updates a count from 32 after the prompt), Left / Right pick,
  A / B confirm, Willow by default; the chosen figure stays 63 updates.
  Madmartigan starts 8 px higher; sprite line 0 is the chosen form's
  (s5_sprpal_live); restarts keep the form.
- src/fam_stage5.c: camera (one-way over x 2368..3528, the arena lock at
  5760 in [5376, 5760]); events: troll windows (6-12, caps and timers from
  ROM), the bat flock (0, 33 places), the balcony spearmen (2-5, endless,
  waits by rank), the corridor entrant (1: from the right edge; the arcade
  reads camera Y there), the arena (13: four trolls from the screen edges,
  boss music 0x0B); trolls (sleep until within 224 x 112, hops of ~60 px,
  32-update throws aimed in 64 directions at 4 px, knock-back by side, death
  throw; window trolls cling to the wall, move and throw, then drop and
  walk); troll orbs; stone pillars (standable, fall within 48 px after the
  ROM delays, energy 16, cracked, respawn); cart pushers and spiked carts
  (5.3: 1 px an update, random 64-update stops, cart 30 energy pushes the
  player, the pusher becomes a spearman); the tower doorway (5.6: player
  held 64 updates, enemies explode, camera min 2368; its wipe picture is not
  drawn, but its effect on collision is: the tower's wall column at x 2592
  reads as the corridor's afterwards, gate5_open); hazard tiles (collision
  id 39, combat.md 2.6) in player.c for every stage.
- Eborsisk (6; src/fam_boss5.c, tools/bosspack5.py): both necks hang on one
  anchor, so they are one plane-B picture per pair of poses (16 pictures, 48
  x 32 cells, CRAM line 3, high priority; every sprite high while it shows),
  placed through the far layer's machinery (stage_far_pic / stage_far_at:
  the HUD's ring rows and splits keep working); the arena re-tiled on line 2
  into the far layer's VRAM at the wake so the necks get the pool (3,328
  distinct neck tiles merged to 625 with tools/tilemerge); a pose change
  re-lays only the changed rows of each column (tools/bosspack5.py
  neck_relay.bin), 200 words an update. Rise 258 updates, sway 5768..6040,
  bob 48 updates (the 2-px shake not shown), decisions every 59 / 64 updates
  from the per-form ROM tables (src/boss5_tables.h), breaths of 50 updates
  with 11 / 9 / 6-flame chains (a group's flames share one VRAM slot and
  pose), heads 127 each (1 a hit, 48 updates deflecting), the sprite neck a
  shield after its first hit, the push wall, two bars as sprites, the deaths
  (180 / 240 updates of explosions, at most 6 at once; the arcade ~13), 14,000
  gold, 2 x 10,000, results x500, Kael's page. Test flag BOSS5_HP.
- Engine (all stages): the VBlank interrupt latches the HUD split's scroll
  values only for a finished frame (stage_commit -> frame_ready): a frame
  that overran no longer mixes this frame's ring rows with next frame's
  values (the HUD's rows showed mid-screen every 8 frames during vertical far
  scrolling); the plane write list holds 192 entries with a shared 2,048-word
  copy pool (less RAM than before); the pose cache's per-frame budget counts
  the plane words queued; 16 x 16 multiplies in the far layer (the 32-bit
  routine cost a stage-1 frame: traces were 20/24 until fixed); put_row reads
  metatiles two columns at a time; bats resident (stages 4, 5).
- Not yet / approximations: the doorway's wipe picture, the interior wall
  torches' animation, bat flock as a roost list (hanging bats drawn without
  actors, at most 6 awake at once, wakes tested every other update), the
  boss's camera shake and white hit flash (a blink), the push wall's
  shot cut, debris of broken pillars (bursts).
- Measured (dropped frames per 100, walk + shoot, INVINCIBLE): start 0-19
  (the tower troll fights), corridor 0, interior bottom 0-12, battlements 0,
  Eborsisk's fight 0-4, his death and collapse 8-44 (gold shower and
  explosions), the interior top (balcony spearmen + bats + archers) ~40-50:
  the engine's per-actor cost needs a pass there.
- ROM content 3.96 MB (4 MB = 4,194,304: 234 KB left; stage 6 will need bank
  switching or cuts).
- Build u: willow-native-m2-20261103u.bin (4,063,232 bytes padded).

## Stage 6 scenery (2026-11-03)

`tools/pack_stage6_areas.py S6DIR [AREA...]` packs the seven areas of
specs/stage6.md 2.1 as separate scenery sets `data/stage6/aN`, each in its
own native coordinates (native x = arcade x - XOFF, y = arcade y - YOFF), so
the engine reads every area as an ordinary stage. Grid 5 rows/columns per
area (stagemap, cropped), far layer at half the camera from FAR_X0/FAR_YOFF,
cameras from the research RAM dumps filtered by the area's fixed camera y
(tower: x range). Results (foreground tiles / backdrop tiles / bytes):
a0 449/460/55,174; a1 330/34/38,658; a2 366/161/44,772; a3 243/1/32,756;
a4 153/238/18,878; a5 (tower, 1024 x 2304 px) 659/159/75,416; a6 597/1/31,658.
Total 297,312 bytes uncompressed, for the SSF2 banks above 4 MB.
Area 3's far blocks (100/101, far y 1024..1407) are fully transparent in the
ROM: no far picture there, as in the arcade. Area 6's far layer (106/107)
is lightning seen behind the chamber's columns, lit only by palette
controllers 1-2 (scroll3 palettes 28/29, mostly black): packed dark for now;
TODO: pack it with pen identity on its own line and run the controllers'
colour tables (0xECE60 / 0xECF40) as a CRAM cycle, with the +-64 px sway.

## Stage 6 engine (2026-11-03, in progress)

- Data: src/s6data.s (section .s6, banks 8-9 via bank_stage6) holds the
  seven areas, s6_sprpal and the HUD recolour (data/hud6); sprites
  src/s6spr.s (tools/pack_stage6.sh, all on line 1: the scenery lines change
  by area). stagedata.c stage6_data[7]; src/stage6_objects.h
  (tools/stagetable6.py: area tables + the twelve doors, native coords).
- src/fam_stage6.c: area6 (FF8374), per-area restart records and index
  tables (1.7), camera windows (2.3; tower clamped to its four columns:
  cam x <= 672), music per area, doors (2.5) as triggers (F_DOOR6, no
  sprite yet), false doors in doors6_used. game.c game_area6: fade,
  scenery set, objects, door start position; variant 10 makes Willow.
- Nothing of .stagedata (stages 2-5) may be read while the banks are in:
  stage 6 places only families whose art families_stage(5) redirects
  (soldier, archer, spearman, chest, gold, burst, axe) until the others
  get stage-6 art.
- Test flags: -DSTAGE6_VIEW (with -DNO_INTRO), -DS6_AREA=n, -DS6_CP=n.
- Verified headless: area 0 scrolls with its soldiers, door 0 -> area 1
  (fade, new area, entry position). ROM 4,980,736 bytes.
- (2026-11-03, cont.) Kael (src/fam_kael.c, specs/stage6_kael.md): wake
  dialogue (cut 3 rows 4/6/8), decision tables, ledge jump, drop, hop,
  leap, parry + push, sword waves (F_KSLASH), defeat (score 10,000, drop
  set 9), exit door 10 -> Bavmorda's picture (story6, message 0x48, 256
  updates) -> area 5 as Willow. Parry spark (12/28) not drawn yet (needs
  stage-6 art). Test: -DS6_AREA=4 -DKAEL_HP=n.
- Music 0x0C-0x0F, 0x1D (tools/audio/run_s6.sh; 0x0C and 0x0E loops forced,
  also 0x07 in run_s4.sh); OKI 0xA2, 0xA6, 0xA9, 0x86-0x89.
- World map 6 (intro_map6, .s6; intropack MAP6=v004098,v004104), title
  NOCKMAAR CASTLE (glyphs 6=7B0A, K=76A4), stage_enter_next banks in first.
- Shops work in stage 6 (F_SHOP; art in .rodata). Story pictures 4/5 moved
  to .rodata (the ending can show them with the banks in).
- Ending data: tools/endpack.py (castle scene: castle = scroll2 palette 14,
  sky = scroll3 palette 12, pen-exact lines; 23 palette states for the
  sunset->day; closing card); portraits data/ending/storye0..e5 (storypack2
  from the ending's own frames, line 1 = s6 sprite line 1 for the roll text).
- Specs: specs/stage6_kael.md, stage6_families.md (supersede parts of
  stage6.md; read their section 0/9 corrections), stage6_bavmorda.md (pending).

### Build v (2026-11-03): the whole game, stage 6 and the ending

`game/willow-native-m2-20261103v.bin` (5,111,808 bytes; content 5,105,726,
137,154 under 5 MiB). Complete linked cartridge: stages 1-6, the ending,
Home/Arcade front end, saves. Gates: tracecheck 24/24, magiccheck 825/825.
Stage 6 dropped updates per 100 (headless, Willow/Mad attacking): area 0
0, area 2 0-15 (knights + crushers + brutes; >= 51 updates/s), Kael 0,
Bavmorda 0-1.

- Bavmorda (src/fam_bav.c, specs/stage6_bavmorda.md): wake dialogue, walk,
  star burst (8 sparks: wait, spiral, orbit, aimed once), take-off, figure-8
  glide with the lightning columns (attack order = candelabra codes) and
  the energy ball, candelabras -> 3 flames, the final phase at energy <= 24
  (altar strike, brazier wakes and hops/fires, she hovers and bobs), 68-
  update invulnerability, defeat (+1,000,000) -> the ending. Test flags
  -DBAV_HP=n, -DBAV_NOFINAL. Known gap: the hit flash palette job (white
  blink) is not drawn.
- The ending (src/ending.c, blocking, run R's timing): the whirlwind as the
  spec (speed quirk at the first reversal, whirl_f6 1/2 drive the roll and
  the chamber fade), the staff roll on plane B (shifted 4 columns), six
  portraits on plane A (data/ending/storye0..5 from the ending's fully
  faded-in frames), the castle scene (hills = scroll1 palette 17 on plane B
  with priority, line 0; castle line 2; sky line 3; 16 palette states),
  smoke, the closing texts and card; then the name entry, GAME OVER without
  music, NEW GAME PLUS unlock for a Home clear with no continue. Test flag
  -DENDING_TEST (with -DSTAGE6_VIEW -DS6_AREA=6).
- Stage-6 families: mace knights = stage 3's routine with the per-variant
  patrol windows (0x1AC50; only while near the screen), the flail = the
  lance; stones (two collision cells id 37 while flat: actor.c patch
  table), crushers (C_SURFACE, standable, side hurt), cart pushers
  (budget = dir byte, variant 2 pushes right, the brute after is variant
  10/12 with 8 energy; the cart stays as an obstacle in stage 6),
  crossbowmen wait in their windows.
- Storage lesson: banks 0-7 hold the common data and stages 2-5 (.stagedata
  linked last); the linker now asserts __stage_data_end + .data <= 0x400000
  (an overflow had silently overwritten the start of the stored stage-6
  banks: area 0's far tiles). Stage 6's music (0x0C-0x0F, 0x1D) is .incbin'ed
  .xgc in .s6 (it plays only while the banks are in; game_stage_reset stops
  the sound before switching back). After changing data a .s file
  .incbin's, touch the .s file (make tracks only the source).
- stage_init now loads a blank tile 0 and clears plane A (rows a short map
  never writes kept the previous scene).
- Mapper / hardware: the SSF2 (Sega mapper) registers are written with the
  plain "SEGA MEGA DRIVE" header (bank_stage6); GPGX honours them and the
  SRAM saves are unchanged with the >4 MB image. EverDrive support for SSF2
  banking with this header is NOT verified on hardware (Mega EverDrive
  Pro/X7 document SSF2 mapping; older EverDrives may need the "SEGA SSF"
  header or not support >4 MB). Pending a hardware check.
- Open (stage 6): tower mortars (4/33) and bomb troopers (4/34) not placed
  yet; events 0-8 (edge soldiers, archer pairs, cart pushers from behind,
  tower brute respawners) not run; wall torches / chandeliers (cell
  animation) static; crusher chains and the riding archer (dir 2) absent;
  Kael's parry spark not drawn; area 6's lightning far layer packed dark
  (palette controllers not run); the room's grey flash cycle absent.
  Stage 5's cart (same routine) still breaks when its pusher leaves
  (stage6_families.md 6.2 says it stays): to fix with a stage-5 check.

### Build w (2026-11-03): stage 6 completed

`game/willow-native-m2-20261103w.bin` (5,242,880-byte file = exactly 5 MiB
padded; content 5,125,696, 117,184 free). Gates: tracecheck 24/24,
magiccheck 825/825. Stage 6 worst drops per 100: area 2 17 (~50 updates/s),
tower 0-26 at its first busy stretch.
- Events (fam_stage6.c stage6_events): event 0 edge soldiers (area 0, shared
  loop), the archer pairs (placed on area 0's floor as variant 2: the
  arcade's variants 10/12 wait there; their own branch not traced),
  area 3's cart pushers from 256 px behind (variant 2, budget), the tower
  brute respawner (variant 14 at (736, 192)).
- Tower: mortars (bob 40 px, bomb drop, 500 points, drop table) and bomb
  troopers (ground behaviour then the ladder at x 512, lobbed bombs);
  stage 3's bomb art packed for stage 6. Both from code only (never traced).
- Kael's parry spark (spell chain 15, root 0xF2EC); Kael and Bavmorda blink
  while immune (their hit palette jobs: they share sprite line 1).
- Torches/chandeliers: tools/torchpack6.py, frames at VRAM 760 written by
  one shared clock (stage6_animators; not actors: the effect pool is 24).
  Crusher chains: resident tiles at VRAM 840, drawn by chains6_draw (main.c
  hook; through the pose cache they cost 30+ drops per 100).
- Area 6's grey flash: both scenery lines lightened for 3 updates at 3, 63,
  99, 108 of a 117-update loop.
- Stage 5's cart now stays as an obstacle when its pusher leaves (same
  routine as stage 6, stage6_families.md 6.2); removed when far behind.
- Remaining differences (reported, not fixed): area 6's far layer
  (lightning behind the columns, scroll3 palettes 28/29 cycling) needs two
  CRAM lines the chamber's scenery already uses: it stays dark (its +-64
  px sway is then invisible). Torch flame colours come from each area's
  scenery lines (dull). The crushers' riding archer (dir 2, its variant-8
  logic untraced) is absent. SSF2 banking on EverDrive hardware unverified.

### Mapper and hardware compatibility (2026-11-03)
- Scheme: a 5 MiB image (file padded to 5,242,880). Banks 0-7 (0x000000-
  0x3FFFFF) are an ordinary 4 MB cartridge: the common data and stages 1-5
  (the linker asserts they end below 0x400000). Banks 8-9 (stored
  0x400000-0x4FFFFF) hold stage 6, the ending and stage 6's music; while
  they play, the Sega (SSF2) mapper's slot registers 0xA130FD / 0xA130FF
  map banks 8/9 into slots 6/7 (0x300000-0x3FFFFF), and back to 6/7 at the
  next new game (sound stopped first: the Z80 reads music through the
  mapper). Slots 0-5 are never touched.
- The header stays "SEGA MEGA DRIVE" (not "SEGA SSF"): with "SEGA SSF"
  0xA130F1 becomes an extended register remapping slot 0 (the running
  code), which collides with the SRAM enable that the saves use. This is
  the convention of the original Super Street Fighter II cartridge (a
  standard header, mapper selected by the >4 MB size), which mappers and
  flash carts emulate by size.
- Evidence: headless runs on two independent emulators' mapper code,
  Genesis Plus GX and PicoDrive (libretro cores): the stage 5 -> 6 hand-
  over (world map, title, play), every stage-6 area, both bosses and the
  ending (incl. the music in the banks) display correctly; SRAM saves
  behave the same with the >4 MB image (GPGX, 188 bytes as before).
- NOT verified: real hardware with a flash cart. Expected to need a cart
  with Sega-mapper (SSF2) support for >4 MB images (e.g. a Mega EverDrive
  X/Pro series); older flash carts limited to 4 MB cannot run stage 6.
  Without the mapper the game up to stage 5 still works (banks 0-7 are a
  plain 4 MB layout), but entering stage 6 would show garbage. To check on
  a console: boot, play to stage 6 (or use a -DS6_ENTER test build), save a
  high score, power-cycle, confirm the score.

### Build x (2026-11-03)
`game/willow-native-m2-20261103x.bin` (content 5,142,132; 100,748 free).
Gates 24/24, 825/825. Added: the crusher-riding archers (stage6_families.md
11: dormant until on screen, ride at crusher y + 43 with the landing hop,
fire every 56 updates while within 144 px of the camera's y, hurt 10,
death flight away from the player, 50 points, 10/100 gold); torch tiles
may use the stage's sprite line 1 (the flames' oranges). Area 6's storm
far layer was tried lit on the chamber's lines (dark green, barely
visible, and it cost two HUD glyph cells): reverted to dark; it is the one
remaining CRAM-limit difference in stage 6. PicoDrive also runs the banked
stage 6 and the ending (see "Mapper and hardware compatibility").

### Build z (2026-11-03)
`game/willow-native-m2-20261103z.bin` (content 5,142,132; 100,748 free).
Gates 24/24, 825/825. Stage 6 worst dropped updates per 100 (headless,
attacking, invincible): area 0 10, area 2 16, area 3 9, area 4 0, tower 5,
area 6 6 (all >= 50 updates/s); area 1's apparent drops are its walk-in
shop screen (game mode 7), none from x 1408 on.
- Performance: the shots-vs-enemies pass skips enemies far outside the
  view (no shot or spell reaches them): the tower start went from 30 to
  5. Stage 6 lends VRAM 880-1041 to the pose cache (posecache_lend in
  stage6_events; returned by every stage_init/restart). The riding
  archers hold the draw's first node (approved Rolling Thunder approach).
  Chain sprites use entries resolved once (chain_prepare). The stone
  collision overrides have a bounding-box reject.
- Remaining differences: area 6's storm far layer dark (CRAM lines); torch
  flame tiles mix lines per 8x8 cell (a few fringe pixels); riding
  archers' draw animation held; EverDrive/console runs unverified.

### Build 2026-11-04a (superseded: the storm is now ROM-exact on the sprite line only, see "Current status")
`game/willow-native-m2-20261104a.bin` (content 5,148,292; 94,588 free).
Gates 24/24, 825/825; stage 6 worst drops per 100: area 2 17, area 6 6.
- Area 6's storm far layer is back: the bolts on the sprite line 1
  (tools/far6.py; the lit sky colour transparent, the chamber keeps both
  scenery lines), shown in 3 x 9-update flashes of a 57-update loop (the
  controllers' steps approximated from run D7), tiles swapped in 30-tile
  slices (a bigger VBlank transfer delays the HUD split). The +-64 px sway
  is per-row scroll on plane-B rows 9-25 only (rows 2-8 hold the boss bar
  and the dialogue) with the whole 55-column picture loaded once
  (stage_far_all: streaming during the sway rewrote the bar's label).
- Torches: the plane keeps a static wall block on the scenery lines (flame
  pixels darkened); the flames are 32x32 sprites on line 1 (chandeliers
  48 px), drawn last (torches6_draw). No fringe; the wall block re-laid
  every 28 updates.
- The riding archers play their full draw animation again (no cost
  difference measured once the other fixes were in).
- Remaining: bolt edges partly greenish (line 1's nearest colours);
  console / EverDrive runs unverified.

### Build 2026-11-04b (superseded: the storm is now ROM-exact on the sprite line only, see "Current status")
`game/willow-native-m2-20261104b.bin` (content 5,152,004; 90,876 free).
Gates 24/24, 825/825. Stage 6 worst dropped updates per 100 (headless,
attacking): area 0 19, area 1 0 (past the shop), area 2 15, area 3 3,
area 4 0, tower 4, area 6 20 (all >= 48 updates/s).
- Area 6's storm is now pen-exact (tools/far6.py): sequences 19 / 20 (ROM
  0x0E4E / 0x0E62, 19 steps x 3 updates) over the colour tables 0xECE60 /
  0xECF40 (full brightness, as the job writes them): the bolt's segments
  lit in turn (frames 2-5), the whole bolt (frame 1), the dim blue sky
  glow (frames 1, 6) as the backdrop (CRAM 0, the area's sky entry). The
  15 lit states' 75 tiles are resident together; a step rewrites only the
  far cells whose word changes (stage_far_cells). Each tile takes sprite
  line 1 (cyans, white) or Willow's line 0 (deep blues), whichever fits.
- The torch flame draw walks a cached list of the area's animators.
- Hardware: still to be run on a console / flash cart (see "Mapper and
  hardware compatibility" for the scheme and the test procedure).

### Build 2026-11-04c
`game/willow-native-m2-20261104c.bin`. Gates 24/24, 825/825. Stage 6's
sprite line 1 is now fitted with the storm's colours too (tools/sprpal.py
EXTRA, tools/pack_stage6.sh: deep blue, blue, cyan, light weights so
Bavmorda's purple keeps its entry); area 6's storm tiles use line 1 only
(Willow's line no longer borrowed: her hurt flash leaves the storm alone).
Everything that copies line 1 was repacked (sprites, HUD 6, torches, the
Bavmorda picture, the ending portraits).

### Current status (2026-11-04c; supersedes the earlier build notes above)
- Whole game in `game/willow-native-m2-20261104c.bin`: stages 1-6, Kael,
  Bavmorda, the ending, name entry, GAME OVER, Home NEW GAME PLUS unlock.
- Area 6's storm: pen-exact from the ROM (sequences 19/20 at 0x0E4E /
  0x0E62, colour tables 0xECE60 / 0xECF40), drawn on stage 6's sprite line
  (fitted to include its colours); no approximated steps, no borrowed
  Willow line (the 04a/04b notes describe superseded versions).
- Remaining by design (approved cut 4, colours per stage fixed and
  approximate): the storm's colours are the nearest of the sprite line's
  15 (deep blue (0,0,7) shows as (0,4,7)).
- Open: a run on a real console with a Sega-mapper flash cart (see
  "Mapper and hardware compatibility"); both emulators pass.

### Build 2026-11-04d: speed-up pass (started; the user chose "After stage 6")
`game/willow-native-m2-20261104d.bin`. Gates 24/24, 825/825.
- Whole-display frame skipping (AGENTS.md's second fallback, main.c
  catch_up): when a shown frame ran past its VBlank (vtimer), up to two
  updates follow without drawing or waiting; every update still runs. Not
  after blocking screens (more than 2 owed: nothing is caught up).
  -DNO_CATCHUP turns it off. Measure with $S/ups.sh (game updates per 100
  frames; the drops metric no longer equals lost updates).
- terrain lookups inlined (tid).
- Busiest windows, game updates per 100 frames (before -> after; 60 Hz =
  100, the 48 acceptance = 80), headless, walking and attacking:
  stage 1 route 50 -> 93, stage 3 lake 63 -> 75, stage 5 interior top
  50 -> 69, stage 5 start 59 -> 86, stage 5 boss 83 -> 87, stage 6 area 2
  79 -> 85, stage 6 area 6 79 -> 80; stages 2 and 4 already 93-96.
- Still under 80: stage 5's interior top (8-9 spearmen: their AI and
  terrain probes ~16% of the time) and stage 3's lake. Next: the per-actor
  cost (spearman probes once per update, bats), per the doctrine a data/
  structure change rather than more local tuning.

### Build 2026-11-04e: frame skipping keeps full speed
`game/willow-native-m2-20261104e.bin`. Gates 24/24, 825/825.
- The catch-up (main.c) now keeps a running debt: VBlanks (vtimer) minus
  updates run, at most 2, carried across shown frames; each interval runs
  that many updates without drawing. Before, a shown frame just past its
  VBlank left one update owed, the next interval paid it, and the debt was
  forgotten even though that interval also took 2 VBlanks: 3 updates per 4
  VBlanks. Reset outside play, while paused and after blocking screens.
  Every update still runs (input, simulation, clock); only drawing is
  skipped (AGENTS.md's second fallback).
- Game updates per 100 frames, worst 100-frame window (04d -> 04e; 100 =
  full speed): stage 1 route 93 -> 99, stage 3 lake 75 -> 99, stage 5
  interior top 69 -> 93 (median 99-100), stage 5 start 86 -> 100, stage 5
  boss 87 -> 99, stage 6 area 2 85 -> 99, area 6 80 -> 99, stages 2 and 4
  99. No spot is under the 80 acceptance.
- The cost is the shown frame rate (frames drawn per 100, median): stage 1
  route 100, stage 6 area 6 84, stage 3 lake 54, stage 5 interior top 33
  (20 fps there: one update costs ~0.7 frame of CPU and drawing ~0.33, so
  a skipped+shown pair does not fit in 2 frames; 30 fps there needs about
  10-15% less CPU per update). Tried and reverted: a terrain cell memo
  (slower: 66), and policies that skip one update while owing 2 (median 50
  shown but the game fell to 78-83 updates).
- Profile at the interior top (1000 updates): spearmen 23% inclusive
  (9 per update), terrain_floor 9.5% (16.6 probes per update), the actor
  loop 8.5%, the shots-vs-enemies loop ~10% (game_update self), VBlank
  waiting 16.5%.

### Build 2026-11-04f: frame skipping by deadline, spearman probe memo
`game/willow-native-m2-20261104f.bin`. Gates 24/24, 825/825.
- Frame skipping (main.c) replaces 04e's debt rule. Update n is due by the
  n-th VBlank since play went on (vtimer). An update already late is not
  drawn; within 2 s of falling behind ("heavy"), an update on time whose
  drawing (measured length + 8 lines, decaying maximum) would end after
  the coming VBlank is not drawn either: the next update runs at once and
  is drawn for its own VBlank. More than 3 late is forgiven (blocking
  screens). Every update runs; only drawing is skipped. 04e's rule kept a
  debt of 2 that a busy scene could never pay back, so it showed every
  third frame; measured at the interior top: logic ~150 lines an update,
  drawing ~90, so an update plus a drawn update fit in two frames.
  Without the heavy limit, light scenes skipped frames that would have
  made it and one stage 1 trace shifted by an update (frame 417).
- Spearman probes memoized (fam_spearman.c feet/terrain): the answers are
  kept per actor against the probe columns, height and terrain_epoch
  (actor.c; bumped on stage/area/gate/stone-cell changes); a floor slope
  is never memoized. -DTMEMO_CHECK recomputes every hit: 15,274 hits
  (stage 5 interior top, stage 6 area 2), 0 contradicted. -DNO_TMEMO
  disables it. Spearmen 22.6% -> 16.6% of the time at the interior top.
- Updates per 100 frames (worst window; 100 = full speed) / frames shown
  per 100 (median, worst): stage 1 route 99 / 100, 49; stage 2 100 / 100,
  66; stage 3 lake 99 / 62, 49; stage 4 99 / 96, 58; stage 5 start 100 /
  100, 47; stage 5 interior top 99 / 55, 33 (04e: 93 / 33, 31); stage 5
  boss 99 / 100, 50; stage 6 area 2 99 / 70, 47; area 6 98 / 90, 49.
  Every spot keeps full game speed; the worst spot now shows 30 fps
  (every second frame) most of the time.

### Build 2026-11-04g: spearman freeze and stage 5 retirement (arcade)
`game/willow-native-m2-20261104g.bin`. Gates 24/24, 825/825.
- The arcade's spearman freeze (0x17F6C, specs/stage1_spearman.md, the
  transliteration src/willow_arcade_actor4_fourteen_post.h) was missing:
  every variant but 6 (raft), when camera Y - 128 >= 0 and y is outside
  [camera Y - 128 (unsigned), camera Y + 384 (signed)], skips its phase code
  from the next update on (+4E, pad[0]) and is removed outside the
  off-camera box (x - camera X in [-48, 432], y - camera Y in [-32, 288]).
  Order as 0x17922: phase unless frozen, retirement, freeze test.
- Stage 5's retirement bound (0x17922, FF8344 == 4): x < camera min X - 128
  (was camera min X, the other stages' rule).
- Speed: no gain. The main session approved a structural off-screen pass on
  condition that behaviour stays exact. At the interior top every spearman
  is inside the arcade's active band (frozen updates in the test run: 8), so
  the arcade updates them fully and nothing can be skipped exactly; the
  remaining cost is spread (Willow's wall probes, stage 5 events, bats,
  animation steps, 3-5% each). Interior top: updates 98-100 per 100, shown
  median ~50 (30 fps), worst windows ~31-35 (every third frame).
- -DFREEZE_COUNT counts frozen updates (measurement).

### Build 2026-11-04h: far enemies at half rate in stage 5 (approximation)
`game/willow-native-m2-20261104h.bin`. Gates 24/24, 825/825.
- User decision (2026-11-03, asked directly): for stage 5's interior-top
  dips, "Allow small approximations".
- DEVIATION (actor.c actors_update, -DNO_FARHALF turns it off): in stage 5
  only, an active (state 2) spearman, bat or archer more than 64 px outside
  the view (x - camera X outside [-64, 384) or y - camera Y outside
  [-64, 320)) is updated every second frame (slot parity), so off screen it
  moves, animates and counts down at half speed, and a far archer shoots
  half as often. Near and on-screen enemies, Willow, bosses, spawners and
  every other stage are unchanged.
- CPU per update at the interior top (fixed-pace measurement build,
  work.sh): median 67,178 -> 58,055 cycles (-14%), 90th percentile 92,889
  -> 89,383, 99th 105,217 -> 102,955. Frames shown per 100 at the interior
  top: median ~50 -> 53-65 (the game's path, and so this number, varies
  between runs); worst windows unchanged at ~33: the peaks are drawn
  frames while the camera scrolls (stage_scroll ~60 lines: map rows,
  columns, far layer) with the bats and spearmen on screen, which the
  approximation does not touch. Updates per 100: 99-100 everywhere.
- Other stage 5 spots: start 100 / shown median 100; boss 99 / 100.

### Build 2026-11-04i: stage 5 spearmen in the shared pose cache
`game/willow-native-m2-20261104i.bin`. Gates 24/24, 825/825.
- Found while profiling the interior top: the pose cache there has one
  VRAM range of 287 tiles (1057..1344), and spearmen used per-actor slots
  of 54 tiles, so only about four got a slot; the others' draws were
  refused (no keep-last-pose fallback in slot mode) and they blinked out.
  On-screen draws refused at the interior top (-DDBG_UNSHOWN, frames
  0..2000): slots 123 of 3,126 (3.9%); shared cache 20 of 3,520 (0.6%).
  Other spots: stage 5 start 0, stage 6 area 2 3 of 6,605, stage 1 route
  0, stage 3 lake 15 of 4,818.
- families_stage: stage 5's spearmen use the shared cache (slot_tiles 0;
  keyed by tile data, so spearmen in the same pose share one copy, and a
  pose that cannot load shows the last one). -DSPEAR_SLOTS5 restores slots.
  Other stages keep slots.
- CPU per update at the interior top (work.sh): median 58,055 -> 46,179,
  90th percentile 89,383 -> 81,165 (fewer loads: shared poses). Updates
  per 100: 99-100 at stage 5 start, interior top, boss. Frames shown at
  the interior top: median ~50-53, worst ~30-33 (unchanged within run
  noise; the worst moments are drawn frames during camera scrolling).
- Headless frames 700 and 1000 at the interior top: spearmen drawn
  correctly (pose, facing, palette).

### Stage 5 interior top: the 48 fps goal (2026-11-03, after 04i)
User decision ("Allow small difference", AGENTS.md): small local
approximations at the interior top, aiming for ~48+ fps at its worst, full
speed kept, no enemy/spawn/difficulty change. Measured (prof sections,
every update drawn, frames 400-1200, scanlines per update; a frame is 262,
~225 usable before VBlank work): logic ~164 (enemies 89, collisions and
the rest 37, stage events 14, shots 8, HUD 13) + drawing ~103 = ~267. To
draw every frame the total must drop to ~225, i.e. ~40 lines; to reach 80
of 100 frames drawn at worst, about the same.
Tried (each measured, none kept):
- Light steps for far patrolling spearmen (skip terrain probes and the
  choice test every second update): spearman update 2,324 -> 2,247 cycles
  (-3%).
- Half rate for spearmen/bats/archers far from Willow (> 160 px across or
  > 96 px up/down), on screen too: enemies 89 -> 86 lines; frames shown
  unchanged (median 49-50, worst 30-34).
- Sound off (diagnostic only): enemies 89 -> 83, drawing 103 -> 94.
Conclusion: the cost is in the enemies near Willow (the ones that fight)
and in drawing the scene; neither can be cut by a small local
approximation without changing the play. Reaching ~48 fps at worst needs
a larger change (e.g. cheaper actor/draw data paths for stage 5, or fewer
sprites drawn), which goes beyond the approved scope. 04i stays current.

### Build 2026-11-04j: collision pre-filter (exact)
`game/willow-native-m2-20261104j.bin`. Gates 24/24, 825/825.
- Collisions cost ~36 scanlines an update at stage 5's interior top (timed
  with -DPROF_NOCOLL); the shots-vs-enemies pass ~26 of them.
- box_hit and actor_box are inline (actor.h): 37 -> 32 lines.
- Pre-filter (game.c collisions, shot_bounds / spell_bounds): once an
  update, the union of every box shot_test and spell_test can hit (the
  sword while swinging, shots, spears, spells); an enemy whose shot box
  misses it is skipped (no test could hit it). Off while spell 9's blast
  runs (it hits by view position). 32 -> 14 lines. -DNO_SHOTBOUND turns it
  off; -DSHOTBOUND_CHECK tests every skipped enemy against every source:
  9,377 skipped across stages 2-6 runs, 0 that a test would have hit.
- Frames drawn per 100 (median / worst; updates 99-100 everywhere):
  stage 5 interior top 50 / 30 -> 65-80 / 44 (second script 100 / 30: the
  worst windows are the first ~600 frames, the bat flock in flight);
  stage 1 route 100 / 56; stage 3 lake 68 / 50; stage 5 start 100 / 51;
  boss 100 / 51; stage 6 area 2 98 / 56 (was 70 / 47); area 6 91 / 54.
- Measurement note: gprof's per-function self times inside the actor loop
  disagree with scanline timings; decisions here use scanline sections
  (secs.sh) and frames drawn. Stubbing a family changes the game's path,
  so per-family costs are rough (spearman updates ~31 lines, spearman
  drawing ~30, bat updates ~17).

### Build 2026-11-04k: roost pre-filter (exact); the opening stretch
`game/willow-native-m2-20261104k.bin`. Gates 24/24, 825/825.
- roost_update (hanging bats) uses the same shot_bounds union before
  shot_test (exact pre-filter).
- Interior top, frames drawn per 100 over 3,000 frames (median / worst):
  script s5 85 / 42, script s5x2 100 / 32. The worst windows are the first
  ~600 frames after the checkpoint: 13 spearmen active at once (4,130
  spearman updates in 312 updates), enemy updates ~100 lines, drawing ~99
  (65 sprites of spearmen; ~95 cycles a hardware sprite to emit, plus pose
  cache lookups). After that stretch most windows draw 100 of 100.
- Remaining gap to ~48 fps at worst is that opening crowd; small local
  approximations do not reach it (light steps -3%, far-from-player half
  rate within noise). Options need the user: a cheaper representation for
  the crowd (e.g. fewer sprite pieces per spearman far from Willow, or a
  lower rate for on-screen spearmen far from Willow, visible as slower
  walking).

### Build 2026-11-04l: DEBUG menu with stage select (user request)
`game/willow-native-m2-20261104l.bin`. Gates 24/24, 825/825. User: "add a
debug menu with stage select" (after "i cant test level 5 from your builds
bc it starts from the beginning all the time").
- SELECT MODE has a third choice, DEBUG (frontend.c P_DEBUG): STAGE 1-6;
  START POINT (stage 1: its start; stages 2-5: checkpoint 0-9 / 0-13 /
  0-13 / 0-18, the records in game.c); AREA 0-6 for stage 6; INVINCIBLE
  ON/OFF (runtime fe.invincible: no damage, pits bounce, no chase road
  death); START GAME; BACK.
- game_begin (intro.c): stage 1 is the normal intro; stages 2-6 advance the
  stage data, clear both planes and the sprites (the map's band shows what
  is on them; after a real clear the story screen has emptied them), then
  the usual hand-over: world map, stage 5's player select, title.
  game_next_stage applies the start point (as the test builds' S*_CP /
  S6_AREA).
- DEBUG runs play with Home settings; nothing is ranked or saved
  (frontend_rank returns none) and NEW GAME PLUS is not unlocked; after a
  game over the menu returns to the DEBUG page.
- Headless: stage 2 start (world map, then play), stage 3 point 3, stage 4
  point 2, stage 5 point 10 (map, player select, the interior top), stage 6
  area 5 (the tower): all start in play. Some start points (e.g. inside a
  boss arena) may need flags a real run would have set; it is a debug tool.

### Build 2026-11-04m: torches, sled spray, sword sparks, hidden object
`game/willow-native-m2-20261104m.bin`. Gates 24/24, 825/825. From an audit
of the notes' open items against the code (the most visible small ones).
- Stage 5's eleven interior wall torches (class 12 kind 6 variant 5,
  0x333FA, stage5.md 5.7) animate: while within camera x-16..x+400,
  y-16..y+272 each counts 7 updates and writes the next of three frames
  (table 0x333E2; scroll2 entries run down a column: top-left, bottom-left,
  top-right, bottom-right) into its 2 x 2 16-px cells. tools/torchpack5.py:
  pen colours read from data/stage5_fg.ppm at the torches (100% consistent),
  each 8x8 tile on scenery line 2 or 3 (flame pens weighted x4: line 2 has
  the yellows), 35 tiles at VRAM 963 (stage 5's scenery ends at 963; the
  rest, 998-1041, is lent to the pose cache in stage 5). A list in
  fam_stage5.c (as actors they cost ~9 scanlines an update; the list ~4).
- Stage 4's sled spray (6.13; class 12 kinds 26 / 31, 0x360A0 / 0x35E5C):
  every 8 ride updates an emitter sends a particle every 2 updates for 16;
  each takes lifetime and chain (0x35E70 / 0x35E74), direction pair and
  flip, start offset (0x35ECE) from the game's random numbers in the
  arcade's order and moves at 0x35F60 from the sled + 0x35F4C, following
  it. Art: data/s4spr/spray (palette 27, the falling snow's colours; on
  scenery line 2, exact). Approximation: the direction set by slope class
  is class 2 (the port's ride has one class).
- Madmartigan's sword extension sparks (madmartigan.md 5.3; class 2 kind
  2): one an update at the sword + 0xFF28[r & 15] (x mirrored facing
  left), the charge sparks' lifetimes and chains. Stages 2 and 4 get spark
  art in their colours (data/s2spr/spark, data/s4spr/spark). The sword's
  colour steps (5.5) stay off: its pieces share fixed lines (approved cut
  4).
- Hidden object: stage 4's own places (0x37526) and found bit 2 (it used
  stage 1's); the pick table 0x37532 in the arcade's order for every stage
  (byte offsets: 0, 8, 4, 4, 0, ...; the port had it sorted, same odds but
  a different place per random number); the found bits cleared by a new
  game (0x2D18; the port never cleared them).
- tools/sprpal.py KEEP_LINE1=PAL recolours added families for a stage's
  existing fit (line 1 and every other family unchanged).
- Frames drawn per 100 (median / worst): stage 5 interior top 69 / 40
  (was ~51 / 34: the lent VRAM); torch area (checkpoint 11) 41 / 21 (46 /
  24 without torches); stage 4 ride 94 / 89. Updates 99-100.
- Still open from the audit (M1 list): stage 2 wagon wheels / horse coming
  loose at the chase end (0x2FC46, 0x2FD10, 0x2FF7E: needs disassembly),
  stage 1 boss arena darkening, the sorcerer's free flight and ground fire,
  stage 3 boss flashes, the continue screen's old man, shop INFORMATION
  texts in stages 2-6, the dragon's fade-in, Eborsisk's camera shake.

### Build 2026-11-04n: chase end, boss shake, shop texts, hooves
`game/willow-native-m2-20261104n.bin`. Gates 24/24, 825/825.
- Stage 2's chase end (stage2.md 6.4; disassembled 0x2FD10, 0x3007E): when
  the clock stops (FF83E2 = 2) the wheels drop to their resting height and
  roll back from 1 px an update, + 0.25 px each to 6; the horse gallops on
  at 2 px an update; both leave the camera (0x2162). Not yet: the part at
  0x2FC3A that bursts (0x492C) once FF83E2 is set (not identified among the
  port's parts). Test flag -DCHASE_AT=x starts the chase at virtual x.
- The horse's hooves (sound 0x39, sfx_hoof): every 64 updates while the
  chase runs (0x2FF6C), every 32 once loose (0x3007E) (the port never
  played it).
- Eborsisk's bob shake (stage5.md 6.4): 16 of every 48 updates the view
  alternates 2 px / 0 (swell_y, the view offset stage 3 uses). Deviation:
  the necks and heads shake with the view (the arcade moves the anchor with
  the camera: there the boss holds still while the arena shakes).
- Shops' INFORMATION in stages 2-6: messages 0x60-0x7B from the ROM (from
  0x52 on, two 80-byte texts and 8 bytes a pair: 0x71AF0 + (id - 0x52) / 2
  x 168, + 80 for the second), the second line's table 0x33FF8 complete;
  rows of 39 characters (the arcade wraps at 40): word wrap.
- Recorded, not done: the dragon's fade-in (stage4.md 6.3: a fade of object
  palette 26; its colours are on fixed shared lines, approved cut 4); the
  continue screen's figures (0x32710: the old man and a figure by the
  chosen form, then sparks on continuing; their colours need the continue
  screen's arcade dump, not on this machine).
- Frames drawn per 100 (median / worst): stage 5 boss 100 / 54; stage 2
  route 100 / 65. Updates 99-100.

### Build 2026-11-04o: stair fix (user report), soldiers, trooper, stage 5
`game/willow-native-m2-20261104o.bin`. Gates 24/24, 825/825.
- User report: "in level 5 in the beginning when climbing the stairs going
  left, the character falls through them". The stairs on the tower face
  (x 1122..1278, floor y 270..192, then the platform at 271) are solid in
  the collision data (scanned per x, -DDBG_FLOORSCAN). The player's foot
  probe lacked player.md 6's rule (0x3E4C): a floor shape with bits 6 and 7
  set means the floor continues into the cell above; after the snap the
  probe runs once more at the corrected height. Without it Willow stood on
  the full cell under the slope (y 240) while the stairs rose on, then fell
  where that cell ended (x 1190). Now she climbs to the platform (y 287).
  Every stage's slopes use this; the arcade traces pass.
- Stage 1 soldiers (stage1_enemies.md 5.1; transliteration actor4_zero_
  jump.h / dive.h): the ledge climb (phase 4) is never entered (no phase
  change leads to it); a level change up is the jump: 16 updates' wait,
  crouch 32 (y - 1), rise at 5.5 px with no sideways motion, landing pose
  10; down is the dive: 16 + 8 updates, a 1.5 px hop, the falling pose 24
  updates, and when the player is near (|dy| < 34, |dx| < 28) a leap (3.5
  px up, the player's facing, 2.25 px a frame until a wall, 35 updates);
  the ambush (variant 8) falls into the dive's falling step (40 updates,
  speed -1). Approximation: three short poses (0x113FA, 0x11432, 0x1144E)
  are not in the packed art: crouch / rise stand in. terrain_wall (actor.c):
  the player's wall test at a point, without the push.
- Stage 3's bomb trooper: the choice table by rank row (0x1D560 / 0x1D660,
  rank high byte & 0xF0), c > 15 -> 4 without a random number (the port
  used row 0 with sorted thresholds).
- Stage 5: the push wall ends Willow's shots past it every 8 updates
  (0x2C6CA; her spells not: their class-8 kinds not mapped); the corridor
  entrant is variant 16 at the left screen edge facing right, energy 8
  (4.3; the port had the right edge, facing left, energy 4).
- Pose loads a frame: LOAD_BUDGET 160 -> 96 tiles (the lent budget already
  used in the chase, stages 5 and 6). With the soldiers' arcade dive the
  stage 1 route trace hit a VBlank overrun at frame 646 (76 pose tiles + 315
  queued words + 15 stage tiles: the next frame started at line 46, a frame
  was skipped); the chase had measured overruns at 160 (12 of 401 frames).
  Refused draws on the stage 1 route 39 -> 35; stages 5-6 unchanged.
- Frames drawn per 100 (median / worst; updates 99-100): stage 1 route 100 /
  57; stage 3 lake 68 / 50; stage 5 interior top 73 / 44; stage 6 area 2
  79 / 50 (runs vary with the path).
- Not done: stage 4's ride slope classes (the class source, +2E of the
  shield, not found yet: 0x3CB0 is only a probe helper).

### Build 2026-11-04p: stage 4 ride slope classes
`game/willow-native-m2-20261104p.bin`. Gates 24/24, 825/825.
- The sled's slope class (+2E): the shared floor probe 0x4200 copies byte
  5 of the collision descriptor on a slope (0 on flat floor or none; a full
  cell with shape bits 6+7 probes again above, 0x429E). The sled probes at
  (0, -16) from its origin (selector +13 = 1, table 0x3C74): the surface
  point here. A class change (0x310B8) sets the top speed 0x31136, lowest
  0x3113E, descent shift 0x31146 (y -= speed >> shift each surface update)
  and the pose 0x312DE[class]; the jump speed is 0x310EA[class]; the spray
  emitter takes the class (0x3112C). Start (0x30EF0): top 1024, lowest
  512, class 0. Tables 16 entries as the ROM runs on (a descriptor's byte 5
  can exceed 3). terrain_slope_class (actor.c).
- Ride x 7232 -> 14936 without jumps: 1,321 updates (the spec's arcade
  trace ~1,360 with the player's own input; the port had a fixed top speed,
  class 2's 1536, before). Classes over the ride: 201 / 238 / 322 / 528
  updates for 0 / 1 / 2 / 3.

### Stage 1 boss "arena darkening": not a darkening (2026-11-03, research)
The boss's init (0x26E28) starts palette job kind 1 (runner 0x0BC4 ->
0x0C0A): source 0xE69C0, palette +38 = 3, page +3C = 0, 6 steps (+3B = 5,
the step wraps after 5) every 6 updates. The runner writes 0x91C000 +
page x 1024 + palette x 32: **object palette 3**, keeping each colour's
brightness nibble. The six steps: red 0x0D00 x15, near-black 0x0111, red,
near-black, red, then a blue-white / gold set (0x0FA8, 0x099F ... 0x0F78),
repeating while the job lives. So it flickers whichever sprites use
object palette 3; the scenery does not darken (the early note at "Not yet:
... the arena's night palette" was a guess). In the stage 1 dumps object
palette 3 is rare (codes 0x800-0x920, a few frames); which sprite shows it
at the boss is not known (the dumps hold video RAM only). Not implemented.

### Build 2026-11-04q: sorcerer flight, arcade random generator, coin height, bank space
`game/willow-native-m2-20261104q.bin`. Gates 24/24, magic clean.
- Coins resting on ledges (user screenshot, stage 1: a coin on a rock's
  front lip, its lower half over the rock face). Wrong in ours. The arcade's
  bouncing pickup (class 14, 2CFC2) lands with the generic actor foot probe
  (40A0) at offset table 0x3C74 entry r[19] = 0 (set at spawn) = (0, -8):
  the probe is 8 px below the coin's origin and the probe point is put on
  the surface, so the origin rests 8 px above the floor. Ours probed at the
  origin and put the origin on the surface: every landed coin sat 8 px too
  low. Fixed in src/families.c gold_update (probe at y - 8, rest at
  surface + 8). Evidence is the arcade code and table (no MAME frame:
  GUI/MAME capture is blocked on this machine).
- Random numbers: rnd() (src/actor.c) used an even multiplier (0x41C6), so
  its state collapsed to a fixed value about 540 frames into play: every
  random choice since builds began was a constant afterwards (e.g. the
  sorcerer never cast at rank row 3; wobble always one direction). Replaced
  with the arcade's generator (0x1D8E: 16-bit shift register, bit 15 = bit
  1 xor bit 9 of the old value, low byte returned; boot value 0x2472 from
  FF8284). Call order still differs from the arcade (fidelity contract).
- Sorcerer (src/fam_sorcerer.c) rewritten to the arcade structure: hidden
  sparkle appearance (20 frames), two appearance poses (24 + 20), flight on
  16 aim directions (0x14F68 / 0x14FA8, re-aim every 8 frames, 0x14C2C
  turns into the attack), hover 0x14F58 with delay 0x14622 and cast roll
  0x14642, cast pose 0x1502E, shot pick 0x14842, out-of-range/close blink
  (8 x 3 frames) -> 30 hidden frames -> relocation by the player (0x14CE0,
  camera-centre facing rule 14D54) -> blink -> hover; tracking (every 8
  frames, 2 px steps) and the random wobble (0xED020). Art: 0x151AE now
  packed with all 16 entries (pack_stage1.sh; sprite palettes unchanged).
  Approximations: ground fire (kind 3) is a slow fireball; hit flash cut 4.
- Bank space: with this build the release overflowed banks 0-7 by 3,254
  bytes. tools/gen_spr_data.py now stores a stage 2-5 sprite file that is
  byte-identical to an earlier stage's once, its symbol an alias
  (tools/spr_alias_sN.ld, included by tools/md_willow.ld): 173 files, 76 KB
  freed; banks 0-7 now end at 0x3EE354 (~72 KB spare).
- Frames drawn per 100 in play (median / worst; updates 99-100): stage 1
  route 100 / 60; stage 5 interior top (debug menu, stage 5 point 10) 100 /
  59 with either generator; stage 6 area 2 76 / 50 (old generator 72 / 50).
- Spot runs: STAGE*_VIEW test builds need -DNO_INTRO with them (without it
  they boot to the menu); the stage 5/6 figures above came through the
  DEBUG menu (stage, point) on the release build.

### Build 2026-11-04r: the sorcerer's ground fire
`game/willow-native-m2-20261104r.bin`. Gates 24/24, magic clean.
- The sorcerer's kind-3 attack (2/16 of casts; a slow fireball stand-in
  until now) is the arcade's ground fire (0x14852, emitter 0x23E76): two
  emitters at the player's height, 40 px to each side (0x23ED2), each
  showing its marker (0x240FE) for 16 frames; then, if a surface or a
  floor cell is 16 px below (the arcade's support test 0x72A8 and the
  descriptor floor flag), three flames 24 frames apart, stepping 16 px
  toward the player; with no ground (e.g. the player in mid-jump) they
  vanish, as in the arcade. Flames are class 10 kind 4, the same routine
  and art (0x24152, 0x241E4) as stage 2's chandelier flames (one
  flame_update; stage 1's do not ride the wagon step).
- Art: pack_stage1.sh `groundfire` (r240FE) and `flame` (r241E4), from a new
  headless arcade dump (lockstep ref, SCRIPT=walk, VIDDUMP frame 2795; it
  reproduces the stored sorcerer data byte for byte). Recoloured with
  KEEP_LINE1 against stage 1's existing fit (fit_stage1_sprites.sh): sprite
  palette and every other family unchanged. The flames' colours are the
  arcade palette group 6 (blue-white), as for stage 2.
- surface_under (src/fam_surface.c): the surface test without marking the
  surface stood on (surface_floor's side effect sags the logs).
- Frames drawn per 100 in play (median / worst; updates 99-100): stage 1
  route 100 / 59; standing at the sorcerer through repeated ground fire
  100 / 57.
- Remaining sorcerer approximation: hits shown by invulnerability instead
  of the palette flash (cut 4).

### Build 2026-11-04s: hit flashes as blinks
`game/willow-native-m2-20261104s.bin`. Gates 24/24, magic clean.
- The arcade's white hit flashes are palette jobs (white / normal every 3
  updates). Sprite lines are fixed per stage (cut 4), so, as Bavmorda's
  already did (src/fam_bav.c), they are shown as blinks on the same rhythm:
  - stage 1 sorcerer (14D8A, job 21/22): blinks through its 28 invulnerable
    frames (hidden first);
  - stage 3 boss (27E96, job 0xE6B20, 13 toggles): blinks through its 40
    invulnerable updates; its death flash (7 toggles) blinks the first 21
    updates of the death.
  Checked headlessly with test flags `-DHIT_AT=frame` (one hit at that game
  frame; sorcerer and stage 3 boss) and `-DDIE_AT=frame` (stage 3 boss):
  sorcerer hidden/shown in 3-frame runs for its 28 frames, then hittable
  (a hit during its relocation blink counts when the blink ends, as in the
  arcade); stage 3 boss (STAGE3_VIEW S3_CP=13, fight from game frame 413)
  3-update runs through 40 updates, then hittable; its death 4 hidden runs
  in 21 updates, then shown through the explosions.

### Build 2026-11-04t: the continue screen's figures
`game/willow-native-m2-20261104t.bin`. Gates 24/24, magic clean.
- The figures on the continue screen (class 12 kind 3; 0x32710, 0x3275E,
  0x328CA, 0x32A46), from a new headless arcade dump of that screen
  (lockstep ref SCRIPT=die, VIDDUMP frame 3420): the old man (168, 80)
  shown after a 16-frame burst, the figure by form (216, 72), Willow or
  Madmartigan. On a continue the old man rises 38 frames throwing sparks
  (0x328C0 / 0x328C4) and bursts away; the figure waits 38 frames, gets up
  over 43 (Madmartigan 53) frames throwing sparks (0x329E6 / 0x329FE), then
  sparkles (0xFB90). Declined: GAME OVER over the figures, the old man in
  his last pose (the arcade's screen; the port used to switch to a blank
  GAME OVER screen).
- Art: tools/contpack.py (12 chains incl. 4 sparks and the burst), one
  15-colour fit per form on CRAM line 3 (36-37 colours in; the old man's
  robe a little browner), tiles aPLib-packed (pack_compress.sh: 2 x 28 KB ->
  9.4 / 10.5 KB), loaded at tile 226 for the screen only (the world map and
  game_reload rebuild VRAM after it).
- Checked headlessly against the arcade frame (positions identical): poked
  1 life and a 2-second clock (FF4B5D-5F) for a time over -> continue ->
  GAME OVER, and coin + Start for the continue sequence.
- Banks 0-7 end at 0x3F4F54 (~45 KB spare).

### Build 2026-11-04u: the soldiers' three short poses
`game/willow-native-m2-20261104u.bin`. Gates 24/24, magic clean; stage 1
route 100 / 59 frames drawn per 100 (unchanged).
- The jump's wait (0x113FA, per facing), the dive's crouch (0x11432) and
  hop (0x1144E) replace the stand-ins (src/fam_soldier.c chains 26-29).
  0x11432 was already packed (pose 42, the 0x11416 chain's third node); the
  others are 4 new poses, 28 tiles. As in the arcade (0x104E6 never steps
  the jump's images) the wait holds its first node.
- Art for stages 1, 2, 4, 5 and 6 without their (lost) arcade dumps: the
  soldier repacked once (pens do not depend on the dump; new roots appended
  so every existing pose, tile, chain and box stays byte-identical), each
  stage's stored soldier palette put back, recoloured with KEEP_LINE1
  against the stage's kept fit and its own fit settings: palettes, every
  other family and every existing soldier byte unchanged (checked).
  pack_stage1/2/4/5/6.sh list the new roots.
- Seen headlessly: jumps (stages 2, 5, 6: wait 73 -> crouch -> rise) and a
  dive (stage 6: 0x11416 crouch -> 42 -> hop 76 -> falling -> landing).

### Build 2026-11-04v: the wagon wheels' burst
`game/willow-native-m2-20261104v.bin`. Gates 24/24, magic clean.
- The chase-end part at 0x2FC3A is the wagon wheels' own routine (0x2FBE6,
  variants 0 / 1; its next state 0x2FD10 is the drop and roll-back built in
  04n): when the end run starts (FF83E2 non-zero) each wheel spawns a burst
  (class 12 kind 0 variant 5, 0x492C) at its position, once
  (src/fam_wagon.c). Seen headlessly (STAGE2_VIEW S2_CP=9 CHASE_AT=29500):
  two bursts at the wheels at the end run's first update.

### Build 2026-11-04w: stage 5's doorway wipe picture
`game/willow-native-m2-20261104w.bin`. Gates 24/24, magic clean.
- The tower doorway's wipe (5.6, 0x32DDC) now draws its picture: the
  arcade's writes replayed offline (tools/wipepack5.py: frame 0 a 2 x 2
  block at scroll2 0x9197DC, rings from table 0x32F7C, the write address
  2 columns left and 1 cell up a frame, frame 6 one-column edges), mapped
  to the world by the arcade's scroll2 address routine 0x43658 (column =
  (x >> 4) & 63, row = (~y >> 4) & 63): 24 x 14 cells over x 2368-2751,
  y 528-751, 186 tiles (each 8 x 8 on scenery line 2 or 3; pen colours from
  the same cells in the assembled foreground). Timing from the routine:
  frame 0 at once, 1-6 every 8 updates, the end 8 later (56 updates; the
  port held 64 before).
- VRAM: the tiles are borrowed off the top of the pose cache's largest
  range (posecache_take / posecache_give, src/sprite.c) from the trigger
  until the camera passes x 2752 (the corridor's camera minimum follows the
  camera, so the picture never returns); stage_init gives them back too.
- Deviation: two of the block's cells have arcade priority group 1; all
  are written without the priority bit.
- Found while measuring (not caused by this build, same with the wipe off,
  -DNO_WIPE5): stage 5's upper corridor from checkpoint 4, walking right
  into the carts, spearmen and pushers, draws 33 median / 12 worst frames
  per 100 (updates 98-100). Not measured before; to be profiled.

### Build 2026-11-04x: stage 5 spawners' one-at-a-time rule (bug fix)
`game/willow-native-m2-20261104x.bin`. Gates 24/24, magic clean.
- Bug: stage 5's event spawners (the corridor entrant, variant 16, and the
  four interior balconies) keep one spearman each (spec 4.3: the next only
  after it has gone). The port tagged its spearmen in pad[0], which the
  spearman itself uses as its FROZEN flag (+4E): once that changed the
  spawner thought its spearman gone and spawned another every 8 updates.
  The tag now lives in bits 1-7 of `ng` (bit 0 stays NEW GAME PLUS's
  doubled-energy flag).
- Corridor from checkpoint 4 (walk right, attack every 8 frames): 18
  spearmen at once -> 1; frames drawn per 100 30 / 4 -> 83 / 53 (median /
  worst; updates 99-100). Interior top from point 10 (walk right, attack,
  jumps): at most 12 spearmen at once -> 5; 67 / 31 -> 70 / 32 (its worst
  windows have another cause). The "13 spearmen at once" of the accepted
  04k opening-crowd dip came partly from this bug.

### Build 2026-11-04y: Eborsisk's collapse explosions
`game/willow-native-m2-20261104y.bin`. Gates 24/24, magic clean.
- The collapse (second head's end, 240 updates) now spawns the arcade's
  two explosions every 3 updates, one from each position table (0x2C470 /
  0x2C352; before: one, the tables in turn), at most 7 at once (was 6; the
  arcade ~13). Measured, frames drawn per 100 in the collapse's worst
  window (debug start 18, -DDIE_AT=200: head i dies 200 + 300 i updates
  after it is up): cap 6 88, 7 85, 8 75, 10 69, 13 60: 7 is the most that
  keeps 80 (48 fps).
- Unchanged: the first head's death window draws 60 per 100 with any cap
  (36% of it waiting for VBlank: video transfers, not CPU; the stage 5
  boss's known worst, 04n).
- The polish list's stage 4 intro (horse group, walk-in, hoofbeats) has
  been in since build s; water shimmer since build o; stage 6 torch fringe
  and the area 6 storm since 04a-04c. The stage 1 cloud band stays plain
  sky (measured trade-off, AGENTS.md: ~100 more VRAM tiles than the sprite
  cache can spare and a worse scenery colour fit).

### Build 2026-11-04z: front-end requirements
`game/willow-native-m2-20261104z.bin`. Gates 24/24, magic clean. No
emulator window launched (AGENTS.md: headless only).
- Audit against the user's list (AGENTS.md "Front-end requirements"):
  already there: the Arcade page (enter arcade -> attract loop until a coin,
  DIP switches, high scores), SRAM saves of scores and options per mode (two
  checksummed, versioned copies), the Home page (start, options with music
  select, high scores, controls), Start pause in Home play (not the
  launching press). Added: the opening screen shows the arcade's WILLOW
  title (tools/logopack.py: the attract logo's rows 0-10, its line-1 tiles
  moved to line 3's free pens so the menu text keeps lines 0-1), rows
  ARCADE / HOME, the credit `RESTER159, 2026` ("take out 'ported by'") and
  `V0.5` bottom right; the Debug menu moved off the opening screen behind a
  secret combo on the Home page: Up, Up, Down, Down, C (its BACK returns to
  Home).
- Checked headlessly: opening screen, Arcade page, DIP switches, high
  scores, the attract loop after ENTER ARCADE, Home page, the combo -> Debug,
  Options change -> SRAM -> power cycle (SRAM file) -> kept, Home start with
  Start held 40 frames (no pause), Start -> PAUSE (player and clock still),
  Start -> resumes.

### Build 2026-11-05a: speed pass before co-op (first round)
`game/willow-native-m2-20261105a.bin`. Gates 24/24, magic clean.
- The bridge logs' drawing (logs_draw, 11.6% of the stage 1 route's time
  before): each log pose's one sprite resolved once (logs_prepare), logs on
  their own tight path (x from v[1], no 32-bit shift, the output pointer
  kept): 7.1%.
- The player's surface probe: logs indexed by 16-px column of their x
  (log_at, set when a log starts; a stale entry fails the slot check); the
  pool scan only looks for the other surfaces: surface_floor 3.1% -> 1.6%.
- Tried and reverted: an 8-entry cell cache in terrain_id (the probes do
  not revisit cells enough: 2.1% -> 3.0%).
- Frames drawn per 100 (median / worst; updates 99-100), the co-op
  session's matrix (game/coop/matrix.out) before -> now:
  1P stage 1 route 100 / 59 -> 100 / 100; stage 5 point 10 (debug menu,
  hold right) 100 / 59 -> 100 / 98 (with 04x's spawner fix); stage 6 area 2
  79 / 50 -> 81 / 52.
  2P proxy (p2_proxy.diff in a scratch copy, the instrumentation hunk
  re-applied by hand): stage 1 route 50 / 41 -> 77 / 49; stage 5 point 10
  98 / 63; stage 6 area 2 49 / 42 -> 48 / 46.
- Not yet cheaper: the player update itself (foot / wall probes through
  terrain_id; P1 ~ 12k cycles an update). Next per the doctrine: a data
  change (probe results shared between the player's probes of one update)
  rather than more local tuning.

### Build 2026-11-05b: speed pass round 2 (the player's terrain probes)
`game/willow-native-m2-20261105b.bin`. Gates 24/24, magic clean.
- The player's probes (wall, foot, head, ledge, hazard) read collision ids
  through a 4-column cache in src/player.c (cell_id): each column's header
  (terrain_column, src/actor.c) is kept until the terrain epoch or the
  stage changes, so a probe is a row read. Columns with tid()'s special
  cases (stage 6's patched cells, stage 5's opened doorway column) still go
  through terrain_id. Results identical (gates).
- Gain small: stage 6 area 2 player_update 9.38% -> 9.31%, wall 2.99% ->
  2.75%. The lookup was not the bulk: the probes' own work is.
- Frames drawn per 100 (median / worst; updates 96-100):

  | spot | 1P | 2P proxy |
  |---|---|---|
  | stage 1 route | 100 / 100 | 71 / 49 |
  | stage 5 point 10 (debug menu, hold right) | 100 / 98 | 100 / 64 |
  | stage 5 interior top (S5_CP=10, walk + attack every 8 + jumps; the original s5 script was lost) | 67 / 29 | 41 / 16 |
  | stage 6 area 2 (debug menu, hold right) | 82 / 52 | 49 / 45 |

- Where the time is (stage 5 interior top, self): the actor loop 7.7%
  (dispatch over many actors), actors_draw 5.5%, game_update 4.7% (the
  collisions inlined), stage5_events 3.5%, the shots 2.7 + 1.9 + 1.6%, the
  player update 1.9% self (7.7% inclusive). A second player's cost is
  mostly its update, collisions and drawing; the probes are ~3% of the
  frame. Further 2P headroom needs the structural items (fewer actor
  dispatches, a cheaper collision pass, shots), not the probes.
- Stage 5 interior top, 1P, per 100-frame window with the attack-heavy
  script: 46 / 31 / 29 / 41 frames drawn at frames 400-800 (the stretch
  after point 10 the user accepted in "Keep 04k" at 32-42 with gentler
  input), 50-100 elsewhere. CPU-bound (450 updates, 173 drawn); no single
  family dominates (actor loop, drawing, collisions, stage5_events, bats,
  shots). Not reopened without the user.
- Real-time section timing (the wprof profiler charges interrupt time to
  whatever runs, so it understated drawing): prof[] V-counter stamps per
  update, test flag -DNO_CATCHUP; -DSECS2 adds stamps inside the drawing
  (profd[]). Stage 5 interior top dip (frames 450-900), scanlines per
  update, median / p90: player 22 / 27, shots + magic 14 / 17, stage
  events 22 / 43, actors 84 / 96, collisions and the rest 21 / 30,
  drawing 102 / 140 (magic + player + shots + spells 24 / 35, the roost
  12 / 15, actors_draw 53 / 88; drawing starts near line 158, so it usually
  spans the VBlank and holds its interrupt's transfers). The structural
  costs: per-actor drawing through the pose cache (~1,300 cycles an actor)
  and the per-actor update loop; changing either is the engine redesign
  awaiting the user's choice (2026-11-03).

### Build 2026-11-05c: structural round 3 (measurements; small exact change)
`game/willow-native-m2-20261105c.bin`. Gates 24/24, magic clean.
- shot_test rejects a box outside the union of the live shot, spear and
  sword boxes, kept from the end of shot_update (a new shot, spear or swing
  drops it until then): exact. Chests' draw-independent shot tests 8.2 ->
  4.5 lines an update at the interior top; frame rate change within noise.
- Measurement tools (test builds): -DFAMTIME (real time per family's
  update, interrupts held, V/H counter: H wraps before V steps, V's VBlank
  jump dropped), -DDRAWTIME (per family's pose draw), -DVBLTIME (the
  VBlank callback), -DSECS_NOINT, -DEXP_NOCALL (the actor loop without the
  families). Stage 5 interior top dip, lines per update: family updates
  ~45 (spearmen 17.7, bats 8.1, archers 7.1, chests 4.5, gold 3.0), the
  update loop itself 17, family draws ~27 (spearmen 13.9, chests 6.0,
  archers 6.0) plus the draw loop and surfaces ~26, VBlank callback ~4.
  The cost is spread over many actors at ~1-2k cycles each to update and
  ~1k each to draw, each path already tuned (pose cache hit ~20
  instructions, sprite copy in assembly).
- Frames drawn per 100 (median / worst):

  | spot | 1P | 2P proxy |
  |---|---|---|
  | stage 1 route | 100 / 100 | 68 / 49 |
  | stage 5 point 10 | 100 / 98 | 80 / 60 |
  | stage 5 interior top (05b script) | 60 / 31 | 40 / 13 |
  | stage 6 area 2 | 82 / 52 | 48 / 45 |

- Conclusion: no single exact change left with a large gain; two players
  at the interior top or stage 6 area 2 need either fewer actors processed
  (approximations the user would have to allow) or accepting the rates.

### Build 2026-11-05d: the 2P flag; two-player approximations measured
`game/willow-native-m2-20261105d.bin`. Gates 24/24, magic clean (1P exact).
- `two_players` (src/actor.c, actor.h): 0 in 1P; the co-op code sets it;
  -DTWO_PLAYERS_TEST sets it in test builds. The 2P approximations (AGENTS.md
  "Two-player approximations") are gated on it.
- Tried, 2P proxy, frames drawn per 100 (median / worst), before -> after:
  1. Active enemies more than 64 px outside the view updated every second
     frame: stage 1 route 68 / 49 -> 65 / 47, stage 5 interior top 40 / 13
     -> 41 / 16, stage 6 area 2 48 / 45 -> 48 / 38: no gain (the enemies
     at these spots are on screen); removed.
  2. (Measurement only, -DEXP_NEARHALF; beyond "keep play the same")
     enemies more than 128 px from the player updated every second frame:
     stage 1 route 94 / 49, interior top 45 / 15, stage 6 area 2 49 / 42.
- Conclusion: the target (~48 fps at worst in 2P) is not reachable with
  enemy update rates: at the interior top and stage 6 area 2 the cost is
  the drawing (~100 lines), two player updates and the stage events; the
  interior top is 31 at worst already in 1P. Options for the user: a steady
  30 fps presentation in 2P, cheaper per-actor drawing, or the rates.

### Build 2026-11-05f: imagery audit; stage-load glitches fixed
`game/willow-native-m2-20261105f.bin`. Gates 24/24, magic clean.
- Audit for the user ("do you have all the imagery between levels, the
  main logo, attract mode for arcade, sram saves"): pictures and index in
  game/out/audit/ (INDEX.md). Every between-level screen of the six stages
  is present (map with marker, title, results, story page with portrait,
  stage 4's intro, stage 5's select), the ending and staff roll, the opening
  screen, the full attract loop, the continue screen, name entry and SRAM
  persistence of scores and options in both modes.
- Fixed: stages loaded with the display on, so half-loaded tiles showed for
  a frame between the world map and the stage (stage_enter_next), after a
  lost life (game_reload) and before stage 1's intro (intro_start): now the
  display is off (backdrop CRAM 32, black in every stage) while loading, the
  DMA queue is flushed, the colours blacked, and the first frame's planes
  and HUD-split scroll values laid (stage_scroll + stage_commit) before the
  fade-in; hud_init leaves the backdrop alone during a reload
  (hud_keep_backdrop).
- Test flag -DTEST_CLEAR_AT=n now counts n updates into each stage's play
  (it compared the absolute game frame).
- The two HUD rows during a stage's fade-in: clean in 05f (measured: no
  dotted pattern in the bottom rows through the stage 2 fade-in).

### Build 2026-11-05g: co-op patches m1 and m2 applied
`game/willow-native-m2-20261105g.bin`. Gates 24/24, magic clean (1P).
- From the co-op session (game/coop/patches/coop-m1.diff, coop-m2.diff;
  notes beside them; applied from game/src with patch -p1): src/coop.c/.h
  and hooks; joining is off unless built with -DCOOP (release behaviour
  unchanged). m2: both heroes share sprite line 0 (user: "Yes, shared
  colours"); data from `python3 coop/tools/line0.py data` (run once; rerun
  after regenerating data/willow or data/mad). 1P colours: near-identical
  shades merged (~1 step). The -DCOOP build compiles.

### Build 2026-11-05h: co-op patch m3 (2P HUD); portrait audit

- coop/patches/coop-m3.diff applied from src (hud.c/h, coop.c/h; backups of
  the four files in the session scratch). Gates on the gate build
  (-DNO_INTRO -DINVINCIBLE): tracecheck 24/24, magiccheck 825/825 in every
  field. 1P stage 1 route (route100.keys): frames drawn per 100 worst 100,
  median 100 (unchanged). 2P figures are the co-op session's (coop-m3.md):
  stage 1 route 84/43, stage 5 interior top 48/15, stage 6 area 2 49/45.
- Headless 2P check (-DCOOP -DCOOP_TEST, pad 2 Start): 1UP/2UP at the
  corners, TIME centred, gems and GOLD per half, split bottom bars
  (Madmartigan's with the sword), "P= n" under each half.
- Portrait audit (user: "there are portaits likie masnerigan"): every
  portrait the arcade draws is shown by ours; table in out/audit/INDEX.md,
  new captures 68 (Bavmorda after Kael) and 82 (shop owl). 0xAC46 entries 5
  and 6 are never indexed by the arcade.

### Build 2026-11-05i: co-op patch m4 (targeting, 2 PLAYERS start, lead = stage hero)
`game/willow-native-m2-20261105i.bin`. coop/patches/coop-m4.diff applied
from src (notes coop-m4.md). Gates (-DNO_INTRO -DINVINCIBLE): tracecheck
24/24, magiccheck 825/825 in every field. -DCOOP and release link.
- 1P frames drawn per 100 (worst / median): stage 1 route 74 / 100 (two
  windows at frames ~900-1100 read 82 and 74; 100 in 05h), stage 5
  interior top (S5_CP=10, s5top.keys) 26 / 66, stage 6 point 2 52 / 73,
  stage 4 point 0 82 / 99, stage 3 point 0 65 / 98.
- The stage 1 dip is the pacer's deadline edge (coop-m3.md): one update
  there finishes a few hundred cycles late, and that starts the 2 s window
  in which on-time updates whose drawing would cross VBlank are skipped.
  Tried: enter that window only on a second late update within 1 s. Stage
  1 route 100/100 and stage 4 worst 87, but stage 3 median 98 -> 93 (more
  late updates without the window); reverted.
- Also tried (05i): the 2 s window shortened to 1 s. Stage 1 route worst
  74 -> 82, stage 6 point 2 52/73 -> 59/78, stage 3 median 98 -> 95;
  reverted (no clear win). Per-family time over the stage 1 dip (frames
  940-1100, -DFAMTIME): soldier 8.3 lines an update, beast 7.4, gold 3.6,
  hidden object 1.6, shop door 0.8; section medians player 26, shots+magic
  8, actors 46, drawing 62, identical before and after m4 (the dip is a few
  hundred cycles at the deadline edge, not a new cost).

### Build 2026-11-05j: co-op patch m5 (Home credits, the out player's prompt)
`game/willow-native-m2-20261105j.bin`. coop/patches/coop-m5.diff applied
(notes coop-m5.md; user rule in AGENTS.md "Credits": Home CREDITS 0-9,
default 3, every Home continue uses one, shared in 2P; saved in the save
record's spare byte, same version). Gates 24/24, magiccheck 825/825.
1P frames drawn per 100 (worst / median): stage 1 route 100 / 100 (the
05i dip is gone: the edge update now lands on time), stage 5 interior top
26 / 61, stage 6 point 2 51 / 73, stage 4 point 0 82 / 99, stage 3 point 0
66 / 94. The medians move by a few with each build at the pacer's edge
(05i: 66, 73, 99, 98) without any change in the measured section costs.

### Build 2026-11-05k: co-op in the release; m6; fade-in glitches; pacer
`game/willow-native-m2-20261105k.bin` (5,242,880 bytes; banks 0-7 end
0x3FEAC4, 5,436 bytes free). Gates with co-op compiled in: tracecheck
24/24, magiccheck 825/825.
- Co-op on in every build: build.sh adds -DCOOP unless CFLAGS_EXTRA has
  -DNOCOOP (or already -DCOOP). The Home menu shows 2 PLAYERS.
- coop/patches/coop-m6.diff applied (pose-cache carve for P2, join retry,
  camera during scripts, stage 4 sled/snowball in 2P). 1P stage 4 ride
  exact against the pre-co-op source (-DUPD_INPUT, S4_CP 11 and 13: 0
  differing updates of ~2,890).
- 1P exactness since co-op: the frame-timed scripts give different paths
  when updates land on different frames (input is by frame), so they do
  not compare builds. Test hook -DUPD_INPUT (main.c): input by update
  (Right, A every 37, B every 151). Pre-co-op source vs now, stage 5 point
  10: the player and HUD identical for 2,354 updates (a few one-update
  differences are snapshots taken mid-update). Same path, frames drawn per
  100 worst / median: stage 5 top 44 / 100 before co-op, 42 / 100 now;
  stage 6 area 2 52 / 99; stage 4 start 82 / 100 without the co-op code,
  75 / 100 with it (one window, frames ~1200-1300; section times identical:
  the deadline edge). The 05j "26" at the interior top was the s5top
  script's path, not the code (04k's 85 / 42 was script s5).
- Pacer (main.c): after SKIP_MAX (3) presents skipped in a row, an update
  at most one late is drawn. 1P unchanged or better (stage 5 top worst 26
  -> 28 on s5top). FORCE_DRAW_2P (0 by default, user decision pending): in
  co-op, draw after 3 skips however late. Measured (2P, e2e.py): worst
  windows 1-13 -> 24-42 frames drawn per 100, updates 90-93 per 100 there.
- Stage fade-in glitches (the "dotted band"): (1) the hand-over into stages
  2-6 kept the old HUD rows, whose words name tiles the new stage
  overwrites (garbage in the bottom HUD rows during the fade): cleared as
  the intro does. (2) One frame at the fade's end with plane B at the
  wrong scroll: the main loop turned the HUD split off mid-frame on the
  title's first frame; now stage_split_scene() turns it off before the
  fade with plane B at the scene's value. (3) One frame at play start: the
  split turned on mid-frame with a stale band counter wrote the bottom
  band's value; split_set parks the counter until the VBlank's reset. The
  settle loop before a fade now scrolls and commits each of 16 frames (the
  far ring moves one row a frame). Chain run (TEST_CLEAR_AT=200, every 2nd
  frame, stages 1-5 and stage 5's story): 0 one-frame glitches (4 before).
- Stage 2 torches: the map's priority bits read once per torch
  (stage_cells_pri), not 16 map reads per frame change (8.0 -> 6.6 lines an
  update under FAMTIME at the fort).
- Glitch scans of 05k (headless, every 2nd frame): the DEBUG starts of
  stages 4, 5, 6 (world map, select, title, fade-in, play start): 0
  one-frame glitches (stage 5's one flag is the select cursor's blink). A
  forced life loss (hud.hp poked to 0) in stage 2 and stage 5, through the
  world map and the reload: stage 5 0; stage 2 flags a fort wall object's
  regular 2-frame change every 15 frames (05j the same), not a load glitch.

### Build 2026-11-05l: 2P draws at least every 4th update (user decision)
`game/willow-native-m2-20261105l.bin` (5,242,880 bytes; banks 0-7 5,436
free). User: "Draw every 4th update" (AGENTS.md, 2P section):
FORCE_DRAW_2P = 1 (main.c): in co-op, after 3 skipped presents the next
update is drawn however late. 2P worst windows, frames drawn per 100
(updates per 100), before -> after (coop/e2e.py, -DCOOP_AUTOJOIN): stage 2
fort points 4-5 1 (99, 96) -> 25, 34 (90, 92); stage 5 points 6-10 12, 5,
8, 6, 4 -> 25, 25, 25, 25, 24 (99, 93, 90, 93, 92); stage 6 point 5 13 ->
25 (100). Medians unchanged within path variation (2P 25-48, the fort 100
outside its heavy windows). 1P unchanged (two_players 0): stage 1 route
100 / 100; gates 24/24, magiccheck 825/825.

### Build 2026-11-05m: cheaper sprite emission (exact); final ROM
`game/willow-native-m2-20261105m.bin` (5,242,880 bytes; banks 0-7 5,180
free). Gates 24/24, magiccheck 825/825.
- Where 2P stage 5 time goes (PC profile, point 10, tools/gprof): drawing
  ~13% of all cycles (~20% of the busy time); the second player's update
  (coop_frame, with exchange_all 4.5%) and the actor updates are larger.
- Drawing changes, all exact (the linked sprite list identical every update
  against the previous code: 1P stage 5 point 10, 2,185 updates; 2P point 8,
  2,044; NO_CATCHUP + UPD_INPUT): the HUD's "P= n" sprites written as words
  (hud_draw 2.24% -> 0.96% of cycles; 2P no longer copies both HUD records
  for the lives); the heroes' sprite words written directly (no bit-field
  read-modify-writes); pose_draw_inline takes a hinted cache entry itself
  (pc_ent / pc_frame, src/sprite.h) instead of calling cache_get.
- 2P stage 5, frames drawn per 100 median / worst (updates per 100 worst),
  05l -> 05m (e2e.py, frame-timed: paths vary a little): point 6 43.5/25
  -> 46.5/30 (99); 7 47.5/25 -> 48/35 (99); 8 43/25 -> 44.5/24 (93); 9 31/25
  -> 39.5/25 (98); 10 26/24 -> 31.5/25 (91). 1P: stage 1 route 100/100;
  stage 5 top same path 42/100 (unchanged).
- Measurement hooks added (off by default): DRAWSECT (main.c: drawing
  timed with interrupts held), PROFA (actor.c: per-class draw stamps).

### Build 2026-11-05n: opening screen layout (user feedback)
`game/willow-native-m2-20261105n.bin` (5,242,880 bytes; banks 0-7 4,924
bytes free; SHA-1 0d12c493dfec1aa665805949ce20daa65fe888be). From a
RetroArch screenshot the user asked: "need center text / bottom: rester159,
2026 / bottom right v0.5". src/frontend.c, opening page only: ARCADE / HOME
centred under the logo (cursor two columns left of the item); the
A/START SELECT B BACK hint centred on row 22; the credit RESTER159, 2026
centred on row 25 and V0.5 at the right of row 25 (columns 35-38, lines
200-207), so both stay visible on displays that crop the bottom edge rows
(before: rows 26 and 27). Other pages unchanged. Headless capture:
out/audit/01_opening_screen_05n.png. Gates 24/24, magic 825/825; game code
unchanged otherwise.

### Build 2026-11-05o: Home page changes; stage 1 intro sky fixed
`game/willow-native-m2-20261105o.bin` (5,242,880 bytes; banks 0-7 3,900
bytes free; SHA-1 8ce1dc18d52757807e0886ff0c9b2493aedc9425). User feedback
after playing 05m: "in home * add big logo * take out high scores (they
would appear if you wait long enough, like in the arcade- then press any
button and you go back to the main home screen) * cotnrols are insiode of
options ... glitchy sky when lvl 1 stars".
- Home page (src/frontend.c): the big WILLOW logo as on the opening screen,
  rows centred under it: START GAME, [2 PLAYERS], [NEW GAME PLUS], OPTIONS,
  BACK. HIGH SCORES left the menu: after 10 s idle on the Home page the
  Home ranking shows (PRESS A BUTTON), any button returns to Home, and
  after 10 s more it returns by itself (no attract play in Home). CONTROLS
  moved into OPTIONS (row 7, before BACK; BACK from it returns to OPTIONS);
  OPTIONS shows all eight rows. The Arcade page is unchanged.
- Stage 1 intro sky (Home and Arcade): the clouds of the pan's top band
  were a mosaic of wrong tiles. The band's 360 tiles go into the pose
  cache's VRAM, which now has only 271 slots there (measured), so 89 tiles
  fell back to repeated slots; also a new cache range starting below the
  previous one was skipped by the loader. Fix (src/intro.c forest_setup):
  the shortfall goes into the backdrop's slots past the 192 the band's
  lower rows use (stage 1: 339 slots; plane B is blank in the pan and the
  backdrop is reloaded below camera 144, after the band has left the
  view); the loader skips only the true fallback slots. Headless: the sky
  is clean (out/audit/05o_stage1_sky.png, before:
  05n_stage1_sky_glitch.png); one-frame glitch scan 0 on Home (frames
  300-1700, even and odd) and Arcade (500-1900) stage 1 starts.
- Gates 24/24, magic 825/825. Pictures: out/audit/05o_home.png,
  05o_options.png, 05o_home_idle_scores.png, 05o_stage1_start_sheet.png.

### Build 2026-11-05p: no BACK rows on Home and Options
`game/willow-native-m2-20261105p.bin` (SHA-1
248684da2e958e33a8f39cc08be35298be0da721; banks 0-7 3,900 bytes free).
User: "take out BACK from home page. if you press B you will go back".
Home lists START GAME, [2 PLAYERS], [NEW GAME PLUS], OPTIONS; Home OPTIONS
ends at CONTROLS (B saves and returns, as before). The B BACK hint stays.
Arcade page and DIP switches unchanged. Headless: B from OPTIONS -> Home,
B from Home -> opening screen. Gates 24/24, magic 825/825. Pictures:
out/audit/05p_home.png, 05p_options.png.

### Build 2026-11-05q: no BACK rows in any menu; Options title; menu flash
`game/willow-native-m2-20261105q.bin` (SHA-1
9f1e225d1cdda2e1f6792c701b77b9d8b7c12bb0; banks 0-7 5,180 bytes free).
User: "even in optoins. no need for a back in the menu" and "in options,
take willow title out" (relayed).
- No BACK row on any port menu (Home, Options, Controls, Music select,
  Arcade, DIP switches, High scores, Debug); B backs out everywhere (DIP
  switches and Options save on B, as before). The A/START SELECT B BACK
  hint stays. Controls and High scores have no rows (A or B returns).
- Options page: the small WILLOW title is gone; the OPTIONS heading stays.
- Found while checking: every menu redraw (each cursor move, value change
  or page change) showed one black frame, since 05m and before: the redraw
  cleared both whole planes with DMA fills that ran into the shown frame.
  Now a page is built in a heap buffer (2.6 KB, only while the menu runs)
  and sent with the logo rows in the next VBlank. Headless frame by frame:
  no blank or half-drawn frame on cursor moves or page changes (opening ->
  Home -> Options -> Home -> opening -> Arcade -> DIP switches -> Arcade).
- Headless: every page captured (out/audit/05q_*.png); a Home game left to
  game over returns to Home and its idle ranking. Gates 24/24, magic
  825/825.

### Build 2026-11-05r: the attract's story pages on Home
`game/willow-native-m2-20261105r.bin` (SHA-1
429393dde81abb759cfa50cd299c688b23358b0b; banks 0-7 9,020 bytes free).
Why more free than 05q (5,180): 05q rebuilt from its saved sources
(SHA-1 matches) and the two symbol maps compared. Code ends 3,856 bytes
earlier (_etext 0x157A80 -> 0x156B70), all in src/screens.c: in 05q GCC
copied the attract's wait loop (att_wait) into each of its callers
(att_type's two clones 1,008/1,020 bytes, att_fade 798, attract_demo_title
840); in 05r att_wait is larger (the Home checks) and is called instead
(254/352/166/104 bytes). Music, graphics and data addresses are unchanged. User (relayed): "can you add the attract sequence to
home as well (not gameplay, the images with the story)", then, about the
logo/notice, Capcom credit and FBI pages: "take this oiut of the attract".
- Home sitting idle 10 s (600 frames) runs the arcade attract loop's own
  code (src/screens.c attract_loop, Home flag): the cave with Bavmorda's
  typed lines, Willow, Madmartigan, the yellow lines, then the Eborsisk
  interlude and the ranking (the Home table), in the arcade's order and
  timing, repeating. Left out on Home: the logo/notice page, the Capcom
  credit page, the FBI page, the demo play, INSERT COIN. It replaces the
  05o idle ranking page.
- Any button returns to the Home page (the press does not also act on the
  menu). The Home loop's fades run in the background and the pad is also
  read in the vertical interrupt (a full-screen picture unpacks for about
  35 frames), so a 3-frame tap anywhere in the loop returns: 82 tap
  timings tested headless, none missed. The interrupt callback is cleared
  on exit; the next stage load sets its own.
- Arcade unchanged: its attract still shows every page and the demo
  (headless capture).
- Headless: out/audit/05r_home_attract_sheet.png (frames 700-5200, every
  100); B and Start return; a game started after the attract plays
  normally (map, stage 1, HUD split). Gates 24/24, magic 825/825.
- User decisions (asked directly, 2026-11-04): the Arcade attract stays
  complete ("Home only"); stage 1's cloud band stays plain sky ("Accept
  plain sky").
- Accepted (asked directly, 2026-11-04): "Accept 05r as final"; 05r is
  the finished port.

### Build 2026-11-05s (test, art pass part 1): stage 1 and stage 3 colours
`game/willow-native-m2-20261105s.bin` (SHA-1
06bb8935b30cc0235847d1064883d13c1529cf29; banks 0-7 802 bytes free). Not
the release: the art pass continues (stages 2, 4, 5, 6, screens). User
request and choice ("B, the blend") in AGENTS.md; pictures in
out/audit/artpass/ (*_before_after.png: 05r left, 05s right).
- Causes found: each scenery tile picked one of two 15-colour lines alone
  (the same arcade colour shown differently in neighbouring tiles: square
  patches), the cut-5 merge ignored edges, the far layer's colours lost to
  the foreground, and the stage 1 story font was packed against an older
  sprite line 1 (red/green stripes).
- Tools (opt-in; old packs reproduce): tools/palopt.c (scenery lines by
  local search over all 512 colours: OKLab error, hue x4 (CHROMA), tile
  border gradients (LAMBDA), worst-area weighting (POW), restarts);
  tools/poolmerge.c (merge only where the streamed pool needs it);
  stagepack.py PALOPT, FG_POOL, CYCLE_FROM, VIS_FROM_CACHE, quantise in
  OKLab; tilemerge.c PERCEPT/BORDER/CHROMA.
- Stage 1: lines by palopt; merge FG_MERGE=160 then FG_POOL=680 (option B);
  pool 687 (POOL_SLOTS kept), far 338 tiles. New: the waterfall's palette
  cycle starts at camera x 1040 (cycle.bin word 72, src/stage.c
  stage_cycle; stage_cycle_pal for fades, stage_cycle_reset on reload):
  left of it pens 10-15 of lines 2-3 are ordinary colours, so the stage's
  first 132 columns (intro, house, canopy) have 15 colours a line. Checked
  headless: the pond and waterfall cycle as before when they come on screen.
- Stage 1 sprites refitted to the new scenery lines keeping sprite line 1
  (KEEP_LINE1); the spell and High Aldwin pinned to line 1
  (fit_stage1_sprites.sh), so stage 3 still shares the spell art and
  Aldwin keeps his orange cape. HUD, boss body (bosspack with a regenerated
  combat dump v010600) and the intro sky (intropack with regenerated
  SCRIPT=stand dumps; PERCEPT merge) rebuilt; the story font's glyphs
  rebuilt against the current sprite line 1 (orange, as the arcade).
- Stage 3: lines by palopt with POW=2 (the meadow keeps its yellow front
  grass and teal back grass); same merge, 1740 fg / 315 far tiles; sprites
  (KEEP_LINE1), HUD and storm rebuilt; cycle.bin word 72 = 0.
- Arcade dumps were gone; tools/... lockstep ref regenerates them (walk,
  stand, combat scripts; palettes match the cached ones).
- Gameplay unchanged: same frames old/new on the same input; gates 24/24,
  magic 825/825.
- Open: a few square patches remain (canopy above the house); the
  dialogue box was not repacked (its dump frame not found).

### Build 2026-11-05t (test, art pass part 2): squares, intro sky, stage 3 meadow
`game/willow-native-m2-20261105t.bin` (SHA-1
77b7b4c776e21bcf127ac2459954322a26edd48a). User, shown 05s: "Adjust first"
-> "Remove square patches", "Intro sky clouds", "Stage 3 meadow".
Pictures: out/audit/artpass/t_*.png (05r | 05s | 05t).
- Squares: they came from the pool-aware merge, which ran first over all
  4847 tiles (1467 merges, most at the stage start, the busiest window).
  Now the light global merge runs first (GLOBAL_FIRST, FG_MERGE=300 with
  DCW=4: a merge that shifts a tile's mean tone costs 64x4 more, in
  tilemerge.c and poolmerge.c), then the pool merge only where still
  needed (WPOW=0.5). Stage 1: 2886 fg tiles (05s 2957), pool 687, far 335.
- Intro sky: keeps all 15 colours of lines 2-3 fixed (SKY_FIXED15: stage 1's
  pens 10-15 are scenery colours at the stage start, on screen during the
  pan) and gets 410 tiles above the map (TOP_BUDGET; the pose cache's 271 +
  the backdrop's spare slots; intro_text.h INTRO_SKY_TOP_TILES 410). The far
  forest band under the clouds lost its brown/teal blotches; the clouds
  change little (543 distinct tiles for 410 slots; no more VRAM at that
  moment: the stage start's window uses 675 of the 687 pool slots). The
  castle on the mountain is simpler (dark box).
- Stage 3: palette cycle from camera x 792 (the meadow and the first part:
  15 colours a line), BOOST=0:45:4 (the opening screen weighs 4x), POW=2,
  the same merge as stage 1 (FG_MERGE=420): 1660 fg / 323 far tiles. The
  meadow keeps teal back grass and yellow-olive front grass, no flat band.
  Water cycles normally from the later start points. The HUD bar takes a
  teal shade (hudpack follows the stage lines).
- Gates 24/24, magic 825/825.

### Build 2026-11-05v (test, art pass part 3): stages 2, 4, 5, 6
`game/willow-native-m2-20261105v.bin` (SHA-1
dad57185eb7117e2e1b38959d5d11808f15a5c28; banks 0-7 69,898 bytes free).
User "Yes, continue" (method applied to every stage), then of the stage 2
fort and stage 4 cave pictures: "these 2 are unacceptable".
Pictures: out/audit/artpass/u_*.png (05r left).
- Stage 1's boss body (74 KB) moved to banks 8-9 (section .s6, src/data.s);
  the banks are mapped in for all of stage 1 (game_stage_reset) and out at
  the step to stage 2 (game_next_stage): stage 1 reads nothing else from
  0x300000 up. Banks 0-7 went from 3.9 KB to 78 KB free. The stage 1 boss
  now shows the arcade's blue fur (05r's body was packed against an older
  sprite fit: pink and patchy).
- Stage 1's cycle switch (camera x 1040) costs no frames: the cycle keeps
  its clock in both modes and the switch is written on a cycle frame (an
  extra queued write moved heavy frames late: 77 drawn per 100 on the
  route; now 100).
- HUDs: every stage's HUD keeps 05r's line per glyph group (LINES_FROM;
  fit_stage1_sprites.sh too): a refit had moved stage 1's white glyphs to a
  scenery line and they showed hollow past the cycle switch.
- Stage 2: the fort (line 2) unmerged except where the pool needs it
  (FG_POOL=392: 851 tiles; 05r merged the wall into one repeating texture);
  line 3 and the far layer stay the chase's; road and torch tiles remapped
  to the new line 2; sprites (burst, flame pinned to line 1) and HUD refitted.
- Stage 4: DARK_MAP=111:000 (the cave's near-black fill rounded to grey
  level 1 and showed as boxes), BD_UNIFORM (no camera log: far cells weigh
  alike, else the high cloud rows were merged flat), FG_MERGE=320 DCW=4 +
  FG_POOL=276, PIN=000,000; 1582 fg / 738 far tiles.
- Stage 5: FG_MERGE=60 DCW=4, POOL_SLOTS=493, far 447 (<= 454 for the boss
  arena); 1623 fg tiles (05r 1857); the far sky is the arcade's purple (05r
  grey with red blocks). torchpack5/wipepack5 rerun.
- Stage 6: all seven areas refitted (pools and far counts as 05r; area 6
  keeps its storm far layer); torch and chandelier wall tiles remapped.
- Frames drawn per 100 (1P, worst / median): stage 1 route 100/100 (05r
  100/100); stage 5 point 10 98/100 (98/100); stage 6 area 2 52/90 (52/88);
  stage 5 interior top 25/59 (05r 28/66; the same CPU sections, a scene at
  the budget's edge: with 05r's scenery and the rest new it reads 24/74).
- Gates 24/24, magic 825/825.
- Not done yet: between-level screens (world maps, story portraits), the
  dialogue box, the intro sky's cloud tiles (VRAM-bound).

### Build 2026-11-05w (test): stage 4 snow
User on 05v: "everything looks great outside of this" (the stage 4 camp).
The snow showed flat white tiles beside lavender-dithered ones (square
patches): the arcade's two snow whites (7,7,7) and (6,6,7) were both kept and
merged tiles mixed them. DARK_MAP=111:000,667:777 for stage 4 (one snow
white, as 05r showed it); 1599 fg / 739 far tiles. Picture
out/audit/artpass/w_s4_camp_before_after.png. Gates 24/24, magic 825/825.

### Build 2026-11-05x (test): world maps, High Aldwin, dialogue box
`game/willow-native-m2-20261105x.bin` (SHA-1
13393d7b7b3aa207270d3092ed927c214723fbab). Not released (dist/ holds 05w).
User (relayed): "can we fix this too?" (the world map), then "can you also
fix the old man's color (in the beginning, he is green) / can we take out
the grey box and make it transparent / and this text box xolors red and
whute are so weird". Pictures out/audit/artpass/x_*.png (05w left).
- World maps (all six): rebuilt from new headless arcade dumps (lockstep
  SCRIPT=stand, STAGE=n for the maps before stages 2-6): the brightest frame
  of the fade (769/774) as the picture; the alternate cells (marker blink,
  figures) from a later frame given the base frame's palette (no fade
  difference). Fitted with tools/palopt over the picture's four lines
  (LINES=4; intropack PICOPT=1, picopt()): perceived error, hue x4, tile
  borders. 623-652 tiles each (budget 1049), 13-34 alternate cells. The
  arcade's grey mountains, olive grass and textured forests are back (they
  were lavender, brown and flat teal).
- High Aldwin: his own sprpal pass (fit_stage1_sprites.sh) with all 15
  scenery pens (he shows only at the stage start, where pens 10-15 are
  scenery colours) and a perceived distance (sprpal SPR_PERCEPT=1,
  SPR_CHROMA=1): white robe, neutral grey shading, red cape (05w: line 1
  only, its grey-green 343 made him green). Sprite line 1 unchanged.
- Dialogue box: its arcade greys (text palette 17: 444/555/666/333, read
  from ROM; the box's dump frame cannot be made: the harness's stand script
  no longer plays the intro) had become line 1's green-grey 343 and dark
  green 021; now slate 122 and near-black 111 (box pens 6 -> 14, 7 -> 1,
  applied to box_tiles.bin after intropack). Red 600 and white 777 stay:
  the palette's own red 700 and white 777.
- Grey band behind the story text: not changed yet; options sent to the
  main session (the window plane cannot show plane A through it; the text
  would have to become sprites).
- Gates 24/24, magic 825/825.

### Build 2026-11-05y (test): stage 1 doorway, waterfall-top water band
`game/willow-native-m2-20261105y.bin` (SHA-1
99adfc230bce4fb33fc2f273fb928014dc751a61). Relayed requests: the teal
doorway ("can this be all brown? or black?", 17_doorway_teal.png) and the
water band blocks (18_water_band_blocks.png). Pictures
out/audit/artpass/y_*.png (05x left, arcade first where present).
- Doorway (two caves, x ~1900 and ~2500): the arcade fills them with
  near-black 111 (about 17 of 255), which the fit drew as line 0's slate
  122 with a brown edge. stagepack DARK_MAP=111:000 makes it black; the
  doorway tiles take line 1, which already has an opaque black.
- Water band at the waterfall top: the light tile merge (FG_MERGE) saw every
  cycling pixel as nearly one colour, so different water tiles merged into
  repeating stamps (also in 05r). With CYC_KEEP=2 the cycling tiles merge
  only among themselves, each family phase seen as its own colour, in both
  the global merge and tools/poolmerge. The band now matches the source
  wave shapes (checked against a lockstep arcade frame on the bridge).
- Palette: all 30 scenery colours pinned to 05x's (tools/palopt PIN now
  takes "rgb+rgb+...,..." per line), so pal.bin, cycle.bin, the far layer,
  sprites, HUD and the intro sky are byte-identical; only foreground tiles,
  map, loads and the boss body's free pool slots changed. Trials that
  refitted the lines (pinning black, dark olive or the tower brown) turned
  ~4,500 rock pixels teal or the end tower olive; rejected.
- Pool merges: 246 (05x 171), the extra mostly in the rocks next to the
  waterfall; looked at, blockiness about the same as 05x there.
  Foreground 2,943 tiles (+57, +1.8 KB); banks 0-7 end 0x3F3336.
- Gates 24/24, magic 825/825. Stage 1 route (3000 frames, 26 windows):
  100/100 updates and frames drawn per 100, worst and median (05x 100/100).

### Build 2026-11-05z (test): story text and stage titles as sprites
`game/willow-native-m2-20261105z.bin` (SHA-1
848accd42d757ed7e2c57a8bbfd1ced9c349a910), on top of 05y. User's choice
"Text as sprites (Recommended)" (AGENTS.md, changes approved cut 3): no
grey band; the scene shows fully behind the text. Pictures
out/audit/artpass/z_*.png (05y left).
- Font: tools/fontspr.py writes data/intro/font_spr.bin (148 glyphs,
  4.7 KB): the band's ground pen 1 made transparent, each big title letter
  stored in 2 x 2 sprite tile order.
- Story (stage 1, rows 1-2): each column a VRAM slot in the title glyphs'
  area (FONT_BASE + 48 + row * 40 + column; no title shows meanwhile,
  box_close puts the title glyphs back); drawn as 4 x 1 sprites over the
  groups of four columns in use: at most 10 sprites, 320 px per line.
  Message 4 still types into the arcade's dialogue box on plane B.
- Titles (all stages): one 2 x 2 sprite per letter (at most 19 on a line).
  The letters' pens 2, 6, 15 are stage 1's line-1 white, grey-green and
  light colours; other stages' sprite lines hold other colours there
  (stage 6: black, stage 5: orange), so at load each pen takes the
  nearest colour in the sprite line (0 or 1) that fits best
  (intro.c title_glyphs). Stage 1 unchanged (exact match in line 1).
- Sprites go first in the table (intro_text_draw, main.c), so on those
  lines they win over the scene's sprites.
- The window band also hid the screen's top 16 lines during a later
  stage's title, where plane B holds the HUD's ring rows; the title now
  keeps the HUD splits on (main.c, intro.c stage_enter_next), so those
  rows show the far layer as between the HUD's items in play. Stage 1's
  intro keeps splits off as before.
- Left as is: during the fade-in of stage 5's title, a 32 x 16 px corner
  at the top right shows the backdrop colour (the far picture's edge) a
  shade off the sky until the fade ends. Stage 3's white title over its
  white clouds is hard to read (the font's own colours, now over the
  scene).
- Gates 24/24, magic 825/825; gameplay code unchanged (the intro only).

### Build 2026-11-06a (test): High Aldwin's face
`game/willow-native-m2-20261106a.bin` (SHA-1
957658c6b59f103665f506151fe0aa1141d4a696), on top of 05z (which has the
story text and titles as sprites). Relayed: "the face is green" (05x).
Pictures out/audit/artpass/a_aldwin_05x_06a.png (05w, 05z, 06a) and
a_aldwin_source.png (the arcade's colours).
- His skin, beard and robe shades (arcade 765, 653, 543, 432: warm cream
  and tan) had no match in the scenery lines or sprite line 1, so they
  took olive greens. tools/sprpal.py ALLOW_LINE0=1 (his own pass in
  fit_stage1_sprites.sh) lets his pieces use Willow's line 0, which has
  774/653/431 skin tones: all 15 pieces chose it. Face skin-coloured,
  beard white, robe cream/tan, as the arcade's.
- Caveat: line 0 flashes when Willow is hurt. He shows in the intro (no
  damage) and for the first screen of play until the camera leaves him;
  a hit there would flash him with her.
- Gates 24/24, magic 825/825.

### Build 2026-11-06b (test): red stage titles with an outline
`game/willow-native-m2-20261106b.bin` (SHA-1 838c50110c06dac76abc7ed03515bce2a6c71a6d), on top of
06a. Relayed: "maybe red, because otherwise you can't see" (white titles
lost on stage 3's sky and stage 4's snow, weak on 2 and 6). Pictures
out/audit/artpass/b_titles_closeup_06a_06b.png (title rows, 06a left) and
b_titles_06a_06b.png (whole frames).
- All six title cards (STAGE n and the name) are red like the story text:
  fill 720, shading 500, with a one-pixel dark outline (all eight
  neighbours) added by tools/fontspr.py in the letters' free pen 1. Red
  alone would vanish on stage 5's orange sunset, so the outline is on for
  every stage.
- Colours come from spare entries of the sprite lines (no new CRAM): the
  font stores stage 1's line-1 pens (13 red 720, 11 600, 3 741, 1 111);
  other stages pick the nearest in line 0 or 1 (intro.c title_glyphs),
  which on stages 2-5 is Willow's 730/500/111: a slightly more orange red.
- Gates 24/24, magic 825/825.

### Build 2026-11-06c (test): stage titles as the arcade's
`game/willow-native-m2-20261106c.bin` (SHA-1
8c64594f8b1084c9471d0ef784025b768e9794fa), on top of 06b (replaces its red
titles). User: "can we do this like in the arcade? white with slight black
shadow" (reference 19_arcade_stage_title.png). Pictures
out/audit/artpass/c_titles_closeup_06b_06c_arcade.png (06b, 06c, arcade
lockstep frames for stages 2-6; stage 1's arcade picture is the user's 19)
and c_titles_06b_06c_arcade.png (whole frames).
- Glyphs: re-decoded from the arcade big font (tools/fontspr.py: cells c,
  c+1 / c+0x10, c+0x11, codes from intro.md 6.3 and stage2-6.md), keeping
  all of palette 0x0E's pens: the near-black drop shadow (pen 0, below and
  right), white 777, greys 666/555/444/333. The old extraction had merged
  the shadow into the band's ground (both mapped to pen 1), so 05z-06b had
  no shadow; 06b's synthetic outline is gone.
- Colours: at load (intro.c title_glyphs) each of the 7 pens takes the
  nearest colour in whichever CRAM line serves them best (sprite lines
  0-1, or scenery lines 2-3's static pens 1-9); no new CRAM. All six read
  white with grey lower shading and the dark shadow, as the arcade.
- Size and place: 16 x 16 letters (2 x 2 sprites), STAGE n on the arcade's
  rows 13-14, the name on rows 16-17 (screen y 88 and 112), centred as
  the arcade's columns (col - 4 on the 320-wide screen).
- Timing: 4 updates a character (spaces included), the name right after
  STAGE n; then 30 updates (stage 1, intro.md 6.3) or 5 (stages 2-6,
  stage2-6.md 1.3/1.4) before play: 06b held every title 30.
- Stage 1: the glyphs load at the title's first update (CPU writes,
  interrupts held), after the dialogue box's frame tiles are gone.
- The text's sprites leave one frame before the HUD's tiles load at play
  start (one frame of HUD glyphs in the letters' place, caught on stage 5).
- Limits: the name line has up to 19 letters, 304 of the 320 sprite
  pixels a line; during the title other sprites crossing screen lines
  112-127 may lose parts (the text is first in the table). VRAM: the title
  glyphs' existing 80 / 100 tiles.
- Differences left: the arcade shows the HUD during stages 2-6's titles
  (ours shows it at play start, as before); the arcade's story text (stage
  1) is also mid-screen (row 13), ours stays in rows 1-2.
- Gates 24/24, magic 825/825.

### Build 2026-11-06d (test): HUD during titles, story text mid-screen
`game/willow-native-m2-20261106d.bin` (SHA-1
dc487b504ee13f08b88a3f1c09e2e8036ced9ac0), on top of 06c. User: "Yes,
match both" (the two arcade differences left in 06c). Picture
out/audit/artpass/d_hud_story_06c_06d_arcade.png (06c, 06d, arcade
lockstep frames for stages 2-6; no arcade frame of the stage 1 intro).
- HUD during stages 2-6's titles (and stage 4's horse scene before it), as
  the arcade, whose play sub 0 builds the HUD (4F44) before the title
  (stage2.md 1.1): stage_enter_next runs hud_init right after the stage
  loads; main.c draws the HUD's sprites ("P= n") after the text in the
  intro mode. The clock stays held until play, as before (the arcade's
  first tick is at play step 2). The title letters move out of the HUD's
  VRAM into tiles borrowed from the pose cache (posecache_take, only the
  letters the title uses: 4 tiles each, at most 64), given back at play
  start. Stage 1's intro keeps its HUD at play start (arcade: intro.md 7).
- Story text (stage 1, messages 1-3) on screen rows 11-12 (y 88, 96): the
  arcade's scroll1 rows 13-14 (intro.md 6.3), columns as before (arcade
  column - 4). Sprites now cover only the runs of typed characters, at
  most 4 wide (spaces cost no sprite pixels): message 2's first row is 10
  sprites, 248 px of the 320 a line, leaving room for the windmill and
  actors on those lines.
- Gates 24/24, magic 825/825.

### Release 2026-11-06d
User (relayed by the main session): "Yes, release 06d".
`game/willow-native-m2-20261106d.bin` copied to `dist/willow-v0.5.bin`
(SHA-1 dc487b504ee13f08b88a3f1c09e2e8036ced9ac0, 5,242,880 bytes);
dist/ holds only it and RELEASE.md. dist/RELEASE.md and game/RELEASE.md
list the fixes since 05w (05x, 05y, 05z, 06a, 06c, 06d; 06b's red titles
were replaced by 06c).

### Build 2026-11-06e (hardware test): SSF mapper for real flash carts
`game/willow-native-m2-20261106e.bin` (SHA-1
3ada213c25dd53a23bdb9376533fbb6894211892, SHA-256
d1b3282e094d53e7763313a3ac87bfaf759ed946eddffe9d79c24f82886f742b,
5,242,880 bytes), on top of 06d. For the user's hardware retest; dist/
stays 06d.
Report (2026-11-05, Genesis Model 1 VA3): EverDrive MD v2.6 (2012) boots
06d with "graphical corruption at top layer of screen for the background
graphics" and "First boss is graphically bugged"; Mega EverDrive PRO does
not boot it.
Causes found:
- 06d's header was "SEGA MEGA DRIVE" for a 5 MiB image. Genesis Plus GX
  (our headless core) turns the Sega/SSF2 mapper on for any ROM over
  4 MiB, but the EverDrives page banks only with a "SEGA SSF" header; on
  the v2.6 the writes paging banks 8-9 into slots 6-7 did nothing, so
  stage 1's boss body (in .s6 since 05w) read stage 2-5 data: the bugged
  boss. Stage 6 and the ending would fail the same way. The PRO not
  booting fits the same header (a >4 MiB image without SSF).
- The top-layer background corruption was not reproduced headlessly; the
  likeliest cause is the same mapper fault (stage 1 maps banks 8-9 for
  its whole length; a cart that decodes the mapper writes loosely may
  also switch save RAM over the upper ROM). Audit for real-VDP-only
  causes (sgdk-dma-z80): scroll splits write VSRAM only with the main
  thread's VDP access safe, queued DMA uses SGDK's checked calls (128 KiB
  boundary), samples sit below 0x200000 (no Z80 reads from switched
  slots), bank switches happen at stage changes only. Nothing found that
  an emulator would hide; retest needed.
- The header checksum was wrong (0x4CF2 stored; sizebnd's did not match
  the image): build.sh now writes the sum over the whole image.
Fix (src/cart.c, src/cart.h; bank.c and frontend.c use it):
- Header "SEGA SSF" (rom_head.c; -DHDR_MD keeps the old one for tests).
- cart_detect at boot: a probe run from RAM (interrupts and Z80 held)
  writes 0x8001 to 0xA130F0: Krikzz extended SSF (Mega EverDrive X3/X5/
  X7/PRO) shows bank 1 in slot 0, standard SSF2 (Sega mapper) only
  switches save RAM on; then 0x8000 restores either. build.sh checks bank
  1's word at 0x100 differs from the header's.
- Banks: extended SSF unlocks (0xA130F0 = 0x8000, bit 15) and writes
  words to 0xA130FC/0xA130FE; standard SSF2 keeps the byte registers
  0xA130FD/0xA130FF. Boot also checks banks 8-9 page in (slot 6's first
  long changes and returns); if not, a warning screen names the problem.
- Save RAM: standard SSF2 as before (0xA130F1, odd bytes 0x200001);
  extended SSF maps bank 31 into slot 4 (0xA130F8 = 0x1F), writes with
  bit 13 set, word accesses from 0x240000 (the X series' half; the PRO
  has all 512 KB), then ROM bank 4 back. Z80 bus held during access.
Headless checks: the SSF build runs the core's extended-SSF mapper
(detected EXT, banks ok), the -DHDR_MD build its Sega mapper (detected
STD, banks ok); stage 1's boss and stage 6 draw correctly on both; gates
24/24, magic 825/825 on the SSF build. Not testable headlessly: extended
SSF save RAM (the core does not emulate bank 31) and every real cart.
Hardware matrix: v2.6 and PRO rows in AGENTS.md; both pending the 06e
retest.

### Build 2026-11-06f (hardware test): the bar at line 16, DMA as SGDK does it
User report (relayed): "on the first level, the background has corruption at
the top"; build and machine not stated. Reproduced headless in both Genesis
Plus GX and PicoDrive on 06e, climbing stage 1 (frames 1400-1900 of the route:
9 of 501 frames on GX, 4 on PicoDrive, none on 06f; picture
out/audit/artpass/f_line16_06e_06f.png).
- Cause: the H interrupt that ends the HUD's top band (after line 15) lands
  late: it waits for the running instruction, and for the VBlank writes when
  they overrun (measured: writes end as late as line 40 on the climb, the
  switch then at line 41, 12 late of 3400 frames). The frame's lines until the
  switch use the top band's scroll, and in plane B's ring the next row after
  the top HUD rows was the bottom HUD: a dark bar from the left edge at
  line 16, at most a few lines high, flickering. Real hardware's
  timing may make it more frequent; there is no measurement of that.
- Fix (src/stage.c hud_prow): the top HUD rows sit just before the resident
  far rows, the bottom ones just after, so a late switch shows the far layer
  itself, at most 7 px off (behind plane A). The spare ring rows moved; ring
  writes unchanged. Stage 6's storm cells (ring rows 10-25) stay clear of the
  HUD rows at its far position. The H interrupt writes the scroll before
  the counter register (a few cycles sooner).
- Hardware-only, not reproducible on any emulator here, both in the
  game's own plane DMA (vq_dma, used for scenery rows and columns, the far layer, the
  scroll tables), where SGDK's own DMA does better: (1) the Z80 was running
  through it; XGM2 reads the ROM continuously and SGDK's HALT_Z80_ON_DMA
  note says a Z80 bus access as a DMA starts can corrupt the transfer on
  some consoles; now held for the flush. (2) The word that starts the DMA was
  half of a long register write; now written from RAM as DMA_doDma does.
  Either could explain background-only corruption on the VA3 (sprites go
  through SGDK's queue and were protected). Unconfirmed until the retest.
- Not fixed: the same late switch at the bottom (after line 207) can leave
  a partial line 208 showing the far layer over the HUD band's top line.
  The VBlank writes still overrun on heavy streaming frames.
- Checks: gates 24/24, magic 825/825, stages 2-6 sampled frames identical to
  06e. SHA-1 ef0707e046bed7bb61fa64e6929357729f941a7a. dist/ stays 06d.

### Build 2026-11-06g (hardware test): boot path for the flash carts
Asked (relayed) to go over the Mega EverDrive PRO path. The user's PRO crash
and stage 1 reports are about the itch upload, presumably dist/ 06d, so they
say nothing about 06e-06g. 06g = 06f plus:
- src/boot/sega.s: first thing at every reset (Z80 halted), word writes put
  the mapper in its power-on state: 0xA130F0 = 0x8000 (extended SSF unlocked
  before any other register, standard save RAM off), slots 1-7 = banks 1-7.
  Before, nothing set slots 1-5, and the boot relied on the firmware (.data
  is copied from slot 7 at a cold start); the probe's first write also had
  to unlock and page at once.
- Reset during stage 1 or 6 left banks 8-9 in, so cart_detect saw no change
  and showed the "does not page" warning for 5 s; slots 6-7 are now reset first.
  Not run headless (the harness has no reset).
- src/cart.c slots67: one word-write sequence for both protocols (low bytes
  reach the standard registers). A wrong protocol detection can now only
  cost the saves, not the banks.
Read and found sound: the probe (runs from RAM, interrupts and Z80 held,
bank 1 word differs, build.sh checks), the warning screen (ordinary reads
and VDP), the save path (callers in slot 0, RAM buffers, Z80 and interrupts
held), header (SEGA SSF, ROM end 0x4FFFFF, RA F820 0x200001-0x20FFFF, JUE,
whole-image checksum).
Still unverified without the carts: extended SSF itself, and save RAM at bank 31
offset 0x40000 (no emulator here has it). The diagnostic build is held until the
user has tried the current test build.
Checks: gates 24/24, magic 825/825; stages 1 and 6 frames identical to 06f
under both the SSF and -DHDR_MD headers. SHA-1
77bf06bb634675d448847c019e830118ae54b247. dist/ stays 06d.

### Build 2026-11-06h (hardware test): pause clock, waterfall base
SHA-1 5669439d6aeb7c260705a5abf03dd465ab99d1d5, on top of 06g. Player report,
relayed (BUG 3, BUG 4; photo out/audit/user_art_feedback_20261104/
20_player_bug3_bug4_waterfall.webp, a console, probably 06d). Pictures:
out/audit/artpass/h_waterfall_base_arcade_06d_06g_06h.png (arcade map, 06d,
06g, 06h), h_pause_clock_06g_06h.png.
- BUG 3, pause: hud_update ran the stage clock and the time warning every
  loop even paused, so time ran out and the hero died on resuming. Both now
  stop with fe.paused (Home only: Arcade never sets it), and frames /
  game_frame hold too. Test: 1,200 paused frames are pixel-identical in 1P
  and 2P on 06h (the TIME digits change on 06g); drop-in was already gated
  on !fe.paused and coop_frame does not run paused.
- BUG 4, waterfall base: a data fault, on every build since the map was
  packed, not streaming or timing. stagepack keeps only the rows the recorded
  arcade camera path showed, +- 48 px. That route crosses the base high up
  (camera y 175+), so cells of columns 133-146 (16-px), rows 24-31, were
  dropped; falling in and walking back left shows the far layer through a
  rectangle. Fix: stagepack VIS_EXTRA=256:300:40:63 (recipe
  scratchpad art/v10.sh = the 05y recipe + VIS_EXTRA), 47 more tiles, pool
  peak and merges unchanged, pal/far/cycle byte-identical; slots renumber,
  so map_meta and data/boss1 were regenerated. Reproduced headless by poking
  the hero to x 2400, y 96; camera x 2138; 06d and 06g show the hole, 06h
  matches the arcade map.
- Audit of the other stages (estimate from standable cells, not proof):
  cells outside the packed rows, possibly reachable: stage 2 columns 19-100
  rows 0-11; stage 3 columns 19-55 rows 4-13; stage 5 columns 52-86 and
  329-335 rows 16-29; stage 6 areas 0-3 rows 7-13, area 5. Stage 4 not
  assessed (slope camera). Most are above the route and may need no fix; none
  has been shown on screen. Stage 1's other span (columns 23-47, rows 6-13) has no
  footing near it.
- Gates 24/24, magic 825/825. dist/ stays 06d.

### Build 2026-11-06i (hardware test): after-death map set-up; HUD rows diagnosed
SHA-1 440c28ffbbe6c2cde302a0c89da2b67906a9f0c7, on top of 06h.
- BUG 5 (relayed: after a death "a mountain of corrupted gibberish" where the
  map should be, on a real console, probably 06d): NOT reproduced. A forced
  life loss in all six stages shows a clean map on 06d, on 06d with the bank
  writes disabled (patched copy: stage 6 itself crashes there, the known
  header fault, but its map is clean), and on 06h and 06i
  (out/audit/artpass/i_after_death_map_stages1-6_06i.png). No map data is above
  0x300000 (highest: stage 5's at 0x2B7BF0), so banks 8-9 cannot affect it.
  One hardware-only hazard found and fixed: on the path after a death the
  scroll splits were still on through the CPU palette write and plane A's DMA
  fill, and the H interrupt writes the VDP (illegal during a fill). intro_world_map
  now stops them first. Cause not proven. The same order remains at other
  VDP_clearPlane sites, not audited one by one.
- HUD rows (itch comment, ROBOT-RANDOM, 21_player_hud_top_corruption.webp;
  "dynamically corrupt" as the hero walks): reproduced on GX and PicoDrive in
  ordinary walking. Not timing. Screen rows 0-1 are plane B words with
  horizontal scroll 0 so the HUD stands still, and the far layer in them is
  re-laid in whole tiles (far_hud_col = far_x >> 3), row 0 one frame and row
  1 the next. So it steps 8 px while the far layer below glides, up to 7 px off,
  with a seam at line 16; a fast camera keeps re-arming row 0 and starves row 1
  (a wrong far row in row 1: i_hud_rows_stale_fast_camera.png). Picture
  i_hud_rows_far_seam_06h.png. Vertically the band is also tile-aligned.
  The design decision is the user's (not changed): see the report's options.
- Writes: SGDK's DMA queue runs first (palettes with it), then the stage's
  tile copies and plane DMA, from VBlank start; overruns reach line 40 on the
  climb in GX. Nothing drops transfers except MAX_PENDING 128 tile loads a
  presented frame (load_list skips the rest silently; not seen, not measured).

### Build 2026-11-06k (hardware test): the top HUD rows as sprites, packed pictures off the heap
`game/willow-native-m2-20261106k.bin`, SHA-1
3e6c049376ce95e5cc94b5a213214a8c124da7f4, on top of 06i (06j was never handed out: it had the fault below). User: "HUD as
sprites (Recommended)" (AGENTS.md). Pictures out/audit/artpass/k_*.png.
- src/hud.c: rows 0-1 are sprites, first in the table with the priority bit.
  Each cell has a VRAM tile with a copy of the glyph (top_slot); runs of filled
  cells with consecutive tiles and one CRAM line are single sprites up to 4
  wide; empty cells cost nothing. The list is rebuilt only when the shape
  changes and stays in sat[] between frames (hud_sat_ok); changed tiles are
  copied by the CPU in VBlank (hud_vblank). Every-frame copying cost ~4 lines,
  a rebuild ~10, a SGDK queue entry per digit ~600 cycles.
- Over a fixed picture (shop, story) the rows stay on plane B as before.
- src/stage.c: plane B holds 28 far rows, lines 0-207; only the bottom band
  has a split (one H interrupt after line 207). The top split and all
  re-laying of far words behind the HUD are gone. far_row steps along the row
  (a division per word cost 17 lines once those rows were real picture rows).
- VRAM: 62 1P cells: the yellow, red and gem glyphs' 32 tiles (only these
  rows used them), 1396-1423 (28: the old window text band, unused since the
  story text became sprites, and 12 from the cache) and 1372-1373. 1UP and 2UP use the
  blue glyphs' own tiles. The 2P-only 6 cells come with the 2P carve (36, was
  30). 80 tiles off the cache's largest range was measured far worse.
- Frames drawn per 100 worst/median, 06i -> 06k (updates per 100 unchanged):
  1P stage 1 route 100/100 -> 78/100; stage 5 interior top 25/59 -> 27/56;
  stage 6 area 2 54/97 -> 51/100. 2P from the debug page: stage 1 37/91 ->
  38/94; stage 5 point 10 99/100 -> 98/100 (not the interior top's crowd: the
  S5_CP=10 script is 1P only); stage 6 area 2 40/50 -> 40/49. The route's 78:
  a late frame at x ~1776 (a pose-load burst, 229 lines against 213) starts the
  2-s deadline rule; it was already 5 lines from the limit.
- Top 16 lines, sprites/line and pixels/line maxima (limits 20 and 320):
  1P stage 1 29, 456 (8 of 1364 frames over); stage 5 16, 320 (0); stage 6
  area 2 22, 544 (772 of 802); 2P stage 1 19, 352 (1 of 1456); stage 5 point
  10 13, 200 (0); stage 6 area 2 21, 416 (375 of 692). Over the limit the
  sprites last in the table lose pixels in those lines only (the HUD never).
  The table reached 80 entries on 81 frames there (1P), 18 (2P); the old
  build's count was not measured.
- A bug of this work, caught by looking at frames (gates compare logic, not
  pictures): the link written into the wrong word ended the list after one
  sprite. Fixed; speed re-measured after.
- BUG 5 mechanism found: load_packed took an 8 KB heap block and returned
  silently without it. The HUD's new RAM (1.1 KB) left the heap short, and the
  first 06j build showed exactly the player's corrupted world map after a
  death: the picture's words over the stage's tiles, in all six stages. Now it
  unpacks into SGDK's DMA buffer (8 KB, queue flushed first), so the heap no
  longer matters: clean in all six stages. Whether 06d failed the same way on
  the console is unproven: 06d has the block on every path tested here,
  including Home's idle attract; the only other heap user is the menu's
  scr, with a margin under ~3 KB.
- 06i's hazard: the H interrupt remains (line 207), so the fix stays needed.
- Not viewed: the shop's return to play; tested: intro, titles via debug
  start, after-death map in six stages, pause in 1P and 2P.
- Gates 24/24, magic 825/825; dist/ stays 06d.

### Build 2026-11-06l (hardware test): the sprite HUD's costs
`game/willow-native-m2-20261106l.bin`, SHA-1
1c2e3334509edfaee0f96b8257d9bac05480d242, on top of 06k. User: "Fix the costs
first" (AGENTS.md). All numbers on this source; pictures
out/audit/artpass/l_*.png. Tools moved to tools/perf/ (the session scratch
folder was wiped: its arcade dumps and camera log are gone, so re-packing
stage 1 needs them re-made; data/stage1/decode.cache keeps the rows).
- Stage 6's corridors (areas 0-3): the top HUD rows are back on plane B (rows
  0-1, scroll 0, hud_plane in src/stage.c, hud_top_mode in src/hud.c). Fixed
  camera height; far rows 0-1 are one tile in areas 0 and 3, and behind opaque
  plane A in 1 and 2 (rows 34-35 have no empty cells; not checked per
  pixel), so nothing can step behind them. Same look. Other scenes: sprites.
- Over fixed pictures the rows are on plane B too: hud_top_mode puts the
  yellow/red/gem glyphs' tiles back (06k would have shown wrong glyphs there).
- Frame pacing: draw_est rises half way to a long frame (was all the way); one
  frame late sets the deadline rule for 30 frames, 2 or more late for 120 (was
  120 for any); pose loads stop past line 196 in a frame (those actors keep
  the pose in VRAM one more frame, the Rolling Thunder rule).
- Frames drawn per 100 worst/median, 06i -> 06l (updates worst): 1P stage 1
  route 100/100 -> 84/100 (100); stage 5 interior top 25/59 -> 25/76 (98);
  stage 6 debug point 2 54/97 -> 66/99 (99). 2P: stage 1 37/91 -> 43/98 (95);
  stage 5 point 10 99/100 -> 98/100 (100); stage 6 debug point 2 40/50 ->
  41/53 (99); stage 5 interior top, first measurement, 25/25 (94). The route
  is not back to 100: one frame misses VBlank by 5 lines around x 1776 (58
  pose tiles and 2 cache misses); 84 is 50 fps for that window.
- Top 16 lines (max sprites/line, max px/line, frames over a limit, table
  max): 1P route 33, 560, 11 of 1191, 70; stage 5 top 15, 336, 2 of 1256, 60;
  stage 6 14, 368, 26 of 964, 72; 2P stage 1 19, 352, 1 of 1442, 54; stage 5
  point 10 13, 200, 0 of 823, 43; stage 5 top 17, 296, 0 of 761, 64; stage 6
  10, 272, 0 of 729, 63. The 80-entry cap: 0 frames in all (06k 81 and 18;
  06i 0 of 3200, read from its RAM). 100 stepped stage 6 frames: 2 with a
  one-frame blink, 3 pixels at most.
- Fixed on the way, found by looking at frames: labels missing on NO_INTRO
  starts (hud_top_vram now lays the rows), no top HUD after a debug start
  (the mode now follows stage_init's b_fixed = 0).
- Debug "stage 6 area 2" in earlier notes is debug point 2, in area 0.
- Checked on this source: after-death map in six stages, pause 1,200 frames
  in 1P and 2P (pixel-identical), shop return and story page (test hooks not
  in the ROM). Gates 24/24, magic 825/825. dist/ stays 06d.

### Build 2026-11-06m (hardware test): stage 1 route back to 100
`game/willow-native-m2-20261106m.bin`, SHA-1
535ba18df4860ff49d34fb9521a637b76d0dd0ae, on top of 06l. User: "Get route to
100" (AGENTS.md). Pictures out/audit/artpass/m_*.png.
- Cause, measured: the late frame's extra 40 lines were two pose-cache
  evictions (9 and 11 lines in room()) plus their loads (10), late in a frame
  already ending near line 190. A cheaper hash or fewer tiles was not needed.
- Fix (src/sprite.c room): with no free room, past line 150, in 1P outside
  stage 5 and outside the deadline rule, the eviction waits one frame: the
  actor keeps the pose in VRAM, a new actor shows a frame later. Never two
  frames in a row. Retained-pose rule (AGENTS.md).
- Measured off in stage 5 and 2P: there it moved frames drawn up a little but
  updates worst down 1-2, so those rows stay as 06l.
- Frames drawn per 100 worst/median (updates worst), 06i / 06l / 06m: 1P route
  100/100 (100) / 84/100 (100) / 100/100 (100); stage 5 interior top 25/59
  (98) / 25/76 (98) / 25/69 (99); stage 6 point 2 54/97 (99) / 66/99 (99) /
  66/99 (99). 2P: stage 1 37/91 (95) / 43/98 (95) / 44/98 (95); stage 5 point
  10 99/100 (100) / 98/100 (100) / 98/100 (100); stage 6 point 2 40/50 (99) /
  41/53 (99) / 41/53 (99); interior top -- / 25/25 (94) / 25/25 (94). Stage 5
  runs identical logic to 06l here: 76 -> 69 is timing noise from code
  layout, which also moved updates 98 -> 99 (seen across builds: 67-84).
- Top 16 lines (max sprites, max px, frames over, table max): route 33, 560,
  11 of 1184, 70; stage 5 top 15, 336, 2 of 1247, 60; stage 6 14, 368, 26 of
  959, 72; 2P stage 1 19, 352, 1 of 1442, 54; point 10 13, 200, 0 of 832, 43;
  stage 5 top 18, 272, 0 of 758, 65; stage 6 10, 272, 0 of 728, 62. Cap 80: 0.
- Actors refused on screen on the route (within a 16-px margin), frames: 06l
  22, 06m 24; 22 of them are one run from frame 863, already in 06l, not
  looked into.
- Viewed on this source: shop and return, story page, after-death map in six
  stages, pause 1,200 frames in 1P and 2P (pixel-identical). Gates 24/24,
  magic 825/825; dist/ stays 06d.

### Build 2026-11-06n (hardware test): shop YES/NO cursor height
`game/willow-native-m2-20261106n.bin`, SHA-1
a3fd167f39388ec942c04fd0d491f7db40793883, on top of 06m. User, looking at the
YES/NO cursor: "is this accurate? seems too big". It was wrong: the arcade
(0x349FE) draws all four pieces at y 192, so the upper and lower corners make
a 16-px bracket round the 16-px letters; the port put the lower pair 16 px
down (32 px tall), since the first shop build. Fixed in src/shop.c. The corners
are over the letters here; the arcade's text layer is probably over its
sprites, but the shop's letter tiles are opaque, so the priority bit hides the
bracket altogether (tried, reverted); no arcade shop frame was rendered. Picture
out/audit/artpass/n_shop_yes_no_cursor.png (old, YES, NO). Shop drawing only.
Gates 24/24, magic 825/825; speed tables not re-run (no change outside the
shop). dist/ stays 06d.

### Build 2026-11-06o: V0.6, published without a cart result
`game/willow-native-m2-20261106o.bin`, SHA-1
a55fe7edcf4cbe78d2448ec5acb0cc50ed1a6eae, 5,242,880 bytes: 06n with the
opening screen's V0.5 changed to V0.6 (2 bytes differ, the digit and the
checksum). The user asked to post it on itch with a devlog; no cart result
exists for any build since 06d. dist/willow-v0.6.bin replaces v0.5 (06d is
kept as game/willow-native-m2-20261106d.bin). Gates 24/24, magic 825/825;
opening screen viewed.

### Build 2026-11-06p (test): scenery holes, all stages
`game/willow-native-m2-20261106p.bin`, SHA-1
eaa1071b2af2db358fff87e3e6d6d5a0febdd936, on top of 06o. Player report
(22_player_stage2_missing_platforms.png, Kega Fusion): platforms missing on
stage 2. Same class as BUG 4: packs kept only rows on the recorded camera path
+- 48 px. Reproduced on V0.6 headlessly by placing hero and camera; compared
with the arcade's cells: out/audit/artpass/p_holes_arcade_v06_06p.png (arcade,
V0.6, 06p: stage 2 fort top, stage 3 tree island, stage 5 tower, stage 1
canopy top).
- Nothing needed remaking: each decode.cache holds its arcade palette dump and
  rows, and willow.zip the graphics. tools/perf/stage_repack.py, area6.py (the
  art pass's scripts, recovered) with tools/perf/r.sh's recipes reproduce
  tiles, map and loads byte for byte on stages 1-5 and area 5.
- tools/addcells.py (new): a whole-map trial pack (stagepack FREEZE_CACHE,
  VIS_EXTRA, poolmerge kept tiles) gives only the cells that were empty; they
  are added to the data with every old word, tile, load entry and slot
  unchanged, so art, boss and arena data hold. New runs take free slots;
  nearby runs of a tile grow (load entries move); with no slot a cell takes the
  nearest tile live at its column.
- Added (8x8 cells; new tiles; cells on a nearest tile): stage 1 693, 60, 304;
  stage 2 2133, 45, 357; stage 3 523, 6, 38; stage 4 13439, 85, 402 (rows
  within 112 px of the camera path, not the whole map: the whole map needs
  ~29 KB of map data, more than is free); stage 5 1998, 60, 87; area 5 20370,
  3, 266. Areas 0-4 and 6 have a fixed camera height: no holes. Stage 2's
  torches moved to the end of the road's slots so the fort can use the rest
  (not past the leap's columns).
- ROM: banks 0-7 ~13 KB free (was ~37 KB).
- Frames drawn per 100 worst/median (updates worst): 1P route 100/100 (100);
  stage 5 top 29/84 (96); stage 6 point 2 66/99 (99). 2P stage 1 44/98 (95);
  point 10 98/100 (100); interior top 25/25 (94); stage 6 40/53 (98). Top-16
  lines over a limit: route 11 of 1184, stage 5 2, stage 6 26, 2P 0, 0, 1, 0.
  Cap 80: 0. Gates 24/24, magic 825/825. After-death maps and pause viewed;
  shop and story not re-viewed. dist/ stays V0.6.

### Build 2026-11-07a (test): stage 1 boss head and paw on the body; player report triage
`game/willow-native-m2-20261107a.bin`, on top of 06p. Player reports (V0.6) and
the user's own: triage below; one fix in this build.
- Fixed: the stage 1 boss's head and paw follow the body's control at once,
  while the body picture (plane B, 64 tiles a frame plus a frame for the map)
  shows the new pose a few frames later; the head was drawn off the body
  (a gap at the neck, "a disembodied limb"). They are now drawn against the
  pose shown (fam_boss.c boss_draw_adjust, from actors_draw); positions, boxes
  and timing stay the control's. Arena frames 884-950 before/after.
- Test flags added (defaults unchanged): -DSORC_HP=n (sorcerer energy),
  -DSORC_KILL_AT=frame (the sorcerer dies then: the bridge collapse).
- Found, not fixed yet:
  - Sorceress (stage 1 mid-boss) entry: her head missing for ~3 frames with a
    white strip; frames 1192 and 1196 of the route are over the top-16-line
    sprite limits (the top HUD rows as sprites are first in each line's
    budget). The bridge logs crossing the top lines (1054-1132) too.
  - Bridge collapse after the mid-boss: 61 and 47 frames drawn per 100
    (updates 100): game_update ~0.4 frame an update (actors_update self 13.8%,
    log_update 11.4%: 30 logs, gold, splashes, soldiers). The route measure
    never reached it (the script stays at the mid-boss).
  - Stage 2 handrails behind the hero: stagepack.py never sets the plane
    priority bit; the arcade's layer order 0x12F8 puts scroll2 mask pens
    (0x003E/00FE/001E by attr bits 7-8) over the objects.
  - Bavmorda story picture: lighter-blue 8x8 squares around her and on the
    rocks (GX); shop picture squares to compare with the arcade.
- Not reproduced (GX, PicoDrive): stage 3 "scrolls wildly then stuck" (stage
  3 plays through with up-jumps; the lake and boss arena reached); stage 1 boss
  random tiles in the attract demo (clean on GX; matches the bank 8-9 symptom
  of an emulator or cart without the SSF mapper).
- Arcade behaviour: chests take 6 hits with the starting sword (energy 6,
  power 1, specs/stage2.md 8.1, madmartigan.md 5.4).
- Gates 24/24, magic 825/825. dist/ stays V0.6.

### Build 2026-11-07b (test)
`game/willow-native-m2-20261107b.bin`, SHA-1
20a40c84b6f6fefb8aed8e278e9135899fc2b70d, on top of 07a. dist/ stays V0.6.
- Sorceress entry (user's choice "HUD on plane B there"): while she holds
  the camera the top HUD rows are on the window plane (stage.c
  stage_hud_window, nametable 0xB000), sprites everywhere else; her head is
  whole on entry; back to the sprite HUD when she dies. The switch's 32 HUD
  tiles go in the VBlank queue (hud_top_mode_tm, DMA_QUEUE): loaded at once
  mid-frame they stalled ~57 lines, one late frame started the 2 s deadline
  rule and the route fell to 93 drawn per 100 at 1100-1200; now 100.
- Stage 2 handrails, balcony fronts, the big pillar: tools/maskprio.py sets
  the plane-A priority bit on cells whose arcade scroll2 mask pens (CPS-B
  80016E/16C/16A/168 = 0, 003E, 00FE, 001E in every gameplay dump; attr bits
  7-8) are at least half the cell's opaque pixels: 684 cells. The Genesis
  chooses per 8x8 cell; the arcade per pixel. Stage 1 (1,139 cells) and
  stage 3 (4,400, mostly water) would change too: not applied (user's call).
- Story picture (att_cave, attract and Home loop): CRAM line 2 had no navy
  (sky drawn 0x600) and lines 1-2 drew the rock teal 0xA60 where line 3 has
  0x880: three palette words, no tile changed. Robe shades 0x422/0x442
  still differ by one green step across cells.
- Shop: line 1 (the fixed HUD line) drew black as 0x222 and the reds as
  0x028; lines 2-3 had one red, so the arcade's curtain stripes were flat.
  tools/shopfix.py: line 2 slots 9/E, line 3 B/D (unused by the owl frames
  and glyphs) take 0x00C/0x00A; 513 non-owl cells re-fitted to
  data/shop/preview.ppm (the arcade source), black on index 0 (black
  backdrop); error 111,350 -> 34,483; tiles 327 -> 481. Pre-existing, not
  touched: the HUD's GOLD/TIME read blue in the shop.
- Bridge collapse (exact speed-ups): log fast path while a collapse counts
  down, actor_step without its two calls, surface_find walks only non-log
  surface slots (actor.c surf_other). Frames drawn per 100 at 1200-1300 /
  1300-1400: 61/47 -> 64/48 (updates 100). The scene needs ~290 lines a
  drawn frame (update ~140, scroll ~40, draw ~75-100, VBlank) against ~262;
  ~19% of the time is spent waiting with draws skipped (irregular 1-2
  updates a draw). Not solved; options to the user.
- Test flags: -DSHOP_AT=frame (the shop opens then); -DEXP_NOWIN=1
  (measurement: the sprite HUD kept at her entry).
- Frames drawn per 100 worst/median (updates worst), 06m -> 07b: 1P route
  100/100 (100) -> 100/100 (100); stage 5 interior top 25/69 (99) -> 25/84
  (96); stage 6 point 2 66/99 (99) -> 66/99 (99). 2P: stage 1 44/98 (95) ->
  51/96 (95); stage 5 point 10 98/100 -> 98/100; interior top 25/25 (94) ->
  25/25 (94); stage 6 point 2 41/53 -> 43/51 (99). Top 16 lines: route over
  frames 5 of 1784 (was 11 of 1184). Gates 24/24, magic 825/825.

### Build 2026-11-08a (test): speed project, round 1
`game/willow-native-m2-20261108a.bin`, SHA-1
3ec500741fe8034347042edc40e98bdc822aba18, on top of 07b (user: "Deeper speed
project", for the stage 1 bridge collapse). dist/ stays V0.6.
- Exact speed-ups (gates 24/24, magic 825/825):
  - bridge logs: an inline fast path in the actor loop (actor.c log_quick:
    a log counting down to its fall; a falling log on the 3 frames in 4
    without the water test); the draw walks a cached list of log slots in a
    68000 loop (families.c logs_draw), the other surfaces after them (order
    between logs and platforms changes; they never overlap);
  - surface_find and the other-surface draw walk only the non-log surface
    slots (actor.c surf_other), skipping clear bits 16/8/4 at a time;
  - shot_test's first rejection inline (shot.h; no side effects);
  - actor_step without its two calls; inlined (actor_step_i) in soldiers,
    beasts and gold;
  - actor_ground caches each actor's last 16x16 collision cell id (arrays
    by slot; dropped on a new actor, actor_clear, an epoch wrap or another
    stage's data).
- Pacing (presentation only; game logic every frame as before):
  - SGDK's VDP_getAdjustedVCounter is the raw line + 32 (16 in VBlank): the
    deadline rule's 224 was raw line 192. Now PRED_LINE 256 (raw 224) for
    decisions up to raw 224 (PRED_FROM), estimate decay 1/4 (EST_DECAY 2),
    and two estimates: after a drawn update and after a skipped one (the
    latter carries two updates' scrolling and pose loads).
  - Late start (main.c): a frame queued in VBlank lines 224-240 (raw V
    counter + VBlank flag; 229-234 taken as the later pass) goes in that
    VBlank when its work fits: measured ~14 lines with nothing queued plus a
    line per ~170 bytes; kept 17 + bytes / 150, done by line 259. -DLATE_CHECK
    counts late starts whose VBlank work ended in the picture: 0 (stage 6
    area 2, the bridge). A looser budget (line 240, 190 bytes a line) gained
    more but spilled to line 28-43 in 2-7 frames: not used.
- Frames drawn per 100 worst/median (updates worst), 07b -> 08a: 1P route
  100/100 -> 100/100; bridge collapse windows 1200-1300 / 1300-1400 64/48 ->
  77/50 (run to run 69-84 / 50-62); stage 5 interior top 25/84 (96) ->
  25/92 (96); stage 6 point 2 66/99 -> 82/99; 2P stage 1 51/96 -> 53/100;
  stage 5 point 10 98/100 -> 98/100; interior top 25/25 -> 25/25; stage 6
  point 2 43/51 -> 48/55.
- Where 1300-1400 goes (gprof, a drawn frame ~ two updates): an update ~76
  lines of actors (soldiers 12, gold 5, logs 5, splashes 4, chests 4, beast
  3, spearman 3, the loop ~15) and ~24 of player probes; a drawing ~70 lines,
  of which pose loading ~25 (slot_get, room ~6,800 cycles a call, the DMA
  queue), logs ~13; scrolling ~30 a drawing; the VBlank work ~14 lines fixed.
- Measurement flags: -DNO_CATCHUP (every update drawn: updates per 100 show
  the overrun; the recorded input then drifts from ~1400), -DLATE_CHECK,
  -DPRED_LINE/PRED_FROM/EST_DECAY/EST_MARGIN/LATE_LINE/LATE_RATE/LATE_FIXED,
  -DDBG_ITER2 (actor loop slots by class: empty/skipped/run). final.sh has a
  bridge collapse row.

### Build 2026-11-08b (test): speed project, round 2
`game/willow-native-m2-20261108b.bin`, SHA-1
90797264e3f0c6c3def30f3e1305ded385808de2, on top of 08a (user: "Yes, round
2"). dist/ stays V0.6. Gates 24/24, magic 825/825.
- Exact speed-ups:
  - actor loop: a lean path (actor.c actors_update_lean) for one player, no
    time-stop spell, not NEW GAME PLUS, not stage 5 (its half-rate spearmen):
    the same order and skip rules without the per-actor tests that cannot
    apply (~4 lines an update at the bridge; -DNO_LEANLOOP for the old loop);
  - pose cache: room() skips its free-entry search when every entry is in use
    (sprite.c nkeys); the slot families' hinted slot already holding the pose
    is tested inline before slot_get (posedraw.h).
- Tried, off by default: update ahead (main.c, -DAHEAD_ON): in a busy
  stretch the next update runs in the wait before the VBlank and is drawn
  after it. Mixed: stage 5 interior top worst 25 -> 33 but median 89 -> 85,
  stage 6 point 2 82 -> 87, 2P stage 1 56/100 -> 53/99, bridge unchanged; and
  that update reads the pad a frame earlier.
- Frames drawn per 100 worst/median, 08a -> 08b: 1P route 100/100 -> 100/100;
  bridge collapse windows 1200-1300 / 1300-1400 77/50 -> 81/62, the window
  after 91 -> 97; stage 5 interior top 25/92 -> 25/89 (run to run); stage 6
  point 2 82/99 -> 82/100; 2P stage 1 53/100 -> 56/100; 2P stage 5 point 10
  98/100; 2P interior top 25/25; 2P stage 6 point 2 48/55 -> 48/57.
- Findings (1300-1400): an update ~110-130 lines (actor loop ~26 of its own,
  ~200 cycles a slot over ~63 slots; soldiers ~4 lines each; player ~24), a
  drawing ~120 (actors_draw ~68 with pose loading, scrolling ~29), the VBlank
  work 20-60 lines (-DVBSTEPS): the stage's writes after SGDK's queue often
  end at picture line 0-21. One update + one drawing + its VBlank is ~260-280
  lines against 262; a pacing model (update 118, drawing 140, VBlank 32 ->
  ~51 drawn) gives ~76 drawn only below ~245 lines in all.
- Measurement flags: -DDBG_LOADS (pose tiles loaded by pose set),
  -DVBSTEPS (raw V counter at the VBlank callback's start and end and after
  the process), LATE_CHECK now also vb_q / vb_s (SGDK queue, stage bytes).

### Build 2026-11-08c: V0.7, published without a cart result
`game/willow-native-m2-20261108c.bin`, SHA-1
ec097c205116c37f8cdc3b16f7bfb0ef7360af87, 5,242,880 bytes: 08b with only the
opening screen's version text V0.6 -> V0.7 (2 bytes differ: the digit and the
header checksum). The user asked (2026-11-08) to raise the version, push to
Git, post on itch with a devlog of what is resolved, what is open and what is
needed to close it. Copied to dist/willow-v0.7.bin (replaces v0.6; 06o is kept
as game/willow-native-m2-20261106o.bin), dist/RELEASE.md rewritten, devlog
text in dist/DEVLOG-v0.7.md. Gates on this source: 24/24, magic 825/825.
No cart result exists for any build since 06d.

### Build 2026-11-10a (test): stage 2 shop door, stage 1 earth
`game/willow-native-m2-20261110a.bin`, SHA-1
9994ca00a919df0faa2785e6da2d391190ef27e2, on top of 08c (V0.7). dist/ stays
V0.7. Gates 24/24, magic 825/825.
- Stage 2 shopkeeper "hard to reach" (player report): stage 2's door is door
  type 1 (variant 1, stage1_shops_companion.md 1.1), which src/shop.c treated
  as type 0. Now as the arcade: nothing shows at the door; standing within
  +-16 x, +-32 y with Up held and neither Left nor Right starts a 30-update
  countdown (player not hittable), then the shop. Before, a player on the
  gallery (y 80, door y 96) was outside the +-14 box and a shopkeeper figure
  floated in the doorway. Not done: the door-opening animation (root
  0x32CAC, not packed).
- Stage 1 earth contrast (player suggestion): tools/earthfix.py, applied to
  data/stage1/tiles.bin (rerun on freshly packed files only). The path
  (arcade group 4 pens 10-14, browns) shared olive 0x466 with the stone; the
  path's pixels in line-3 tiles now use line 3's 0x468 (56 tiles, 861
  pixels). Line-2 path tiles stay olive: line 2 has no brown and no free
  entry (the far layer uses pen 6), and moving them to line 3 recoloured the
  bushes. The image differs from 08c only inside stage 1's tiles.
- Tried and dropped (user: "Drop stages 1/3 front"): scenery in front of the
  characters on stages 1 and 3 (tools/maskprio.py; stage 3 grass group 1 at
  FRONT_MIN 0.9, water excluded: per-cell priority hid the raft). Priority
  cells cover the bottom HUD band (plane B). Keeping the band shown cost
  speed every way tried: window rows 26-27 (8 pose-cache tiles: bridge
  62 -> 48), rewriting the band's plane rows without the bit (route 100 ->
  89, bridge 54). Stage 1 and 3 maps are back to 08c's.
- Bridge collapse: not improved. Profile 1300-1400: VDP_waitVBlank 22%
  (update ~120 + drawing ~140 + VBlank ~30 lines > 262: each drawn frame
  takes two VBlanks). Every frame drawn there needs ~15% less work.
- Frames drawn per 100 worst/median (updates worst): 1P route 100/100 (100);
  bridge 61/100 (100); stage 5 top 30/98 (96); stage 6 a2 82/100 (100); 2P
  stage 1 53/100 (95); 2P stage 5 p10 98/100; 2P stage 5 top 25/25 (93); 2P
  stage 6 a2 49/57 (99). As 08b within run-to-run noise.

### Build 2026-11-11a: V0.8, published without a cart result
`game/willow-native-m2-20261111a.bin`, SHA-1
374f6afb04a9ecb5c28906607723467ac37c8498, 5,242,880 bytes: 10a with only the
opening screen's version text V0.7 -> V0.8 (2 bytes differ: the digit and the
header checksum). The user asked (2026-11-11): "create new v / merge, push,
deploy to git. create bin, push to itch. Publish New devlog". Copied to
dist/willow-v0.8.bin (replaces v0.7; 08c is kept as
game/willow-native-m2-20261108c.bin), dist/RELEASE.md rewritten, devlog text
in dist/DEVLOG-v0.8.md. Gates and speed as 10a. No cart result exists for any
build since 06d.
