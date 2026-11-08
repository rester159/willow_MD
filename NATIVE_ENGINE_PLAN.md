# Willow: ground-up native engine plan (2026-11-01)

User decision, 2026-11-01: after build t/u ("if I don't have smoothness and
full speed, no glitches this game is dead"), and the finding that the B1
architecture (the arcade 68000 program running on the Genesis plus a
presenter) cannot reach a steady 60 even with a 2x CPU overclock, the user
chose "Plan ground-up engine". This document is that plan. Nothing in it is
built yet.

## 1. Goal and non-goals

Goal: a new Genesis game that plays like arcade Willow, at a steady 60
updates/s and 60 Hz camera on real hardware (Models 1-3, EverDrive), with no
visual glitches, inside 5 MiB.

Non-goals (doctrine, `sgdk-native-doctrine`):
- No arcade binary, patched or relocated, in the shipping game.
- No presenter that converts arcade sprite lists, tilemaps or palettes per
  frame.
- No routine-by-routine transliteration keeping the arcade RAM map, task
  scheduler or data contracts (the 640 `src/willow_arcade_*` files are such
  translations: research references only).
- No emulation of the arcade sound CPU in the shipping game.

B1 (the current cartridge) stays as the research instrument: lockstep
oracle, trace source, behaviour reference. It is not extended further.

## 2. Fidelity contract (to agree with the user)

Exactness is a dial. Proposed default:

Must match the arcade (verified against arcade traces, frame by frame where
it matters to the player):
- player movement, jump arcs, attack and magic timing, hit boxes, damage,
  invincibility windows, input feel;
- enemy and boss behaviour patterns, attack timings, health, spawn points
  and spawn timing;
- scoring, gold, shop prices and items, lives, time limits;
- stage layouts, scrolling routes, story text, music cues.

May be approximated (each recorded in a deviations list):
- random-number sequences (same distributions, not the same numbers);
- sub-pixel paths of debris, sparks, snow and other decoration;
- the exact count of purely decorative particles where the Genesis sprite
  limits need it (never enemies, projectiles or pickups);
- draw order between overlapping decoration;
- colours (fixed per-stage palettes, as already approved);
- parallax (static or simplified far layers, as already approved).

## 3. Budgets (NTSC frame: ~128k 68000 cycles)

Measured on B1 (stage 4, the busiest): about 100 active objects, the arcade
program alone needs ~88 % of a Genesis frame per update, presenter ~30 % more.
A native design must fit everything below at 60 Hz:

| Subsystem | Budget per frame | How |
|---|---|---|
| Game logic (100 actors) | <= 45k cycles | compact actor records, per-type native update, no dispatch tables per piece |
| Collision | <= 10k | per-team lists, coarse grid / sorted x, only overlapping pairs tested |
| Sprite output | <= 20k | pre-cut Genesis sprite pieces per pose, written straight to the SAT buffer |
| Camera, map streaming | <= 8k | pre-converted plane maps, one column/row per move |
| HUD | <= 2k | native, redrawn only on change |
| VBlank commit | ~7 KiB DMA | SAT, column/row, changed pose tiles |
| Audio | 0 on the 68000 | Z80 driver (XGM2 or equivalent), FM/PSG music, PCM only for voices |
| Spare | >= 25k | worst-case headroom |

Measured in M0 (game/M0_NOTES.md): per screen up to 848 foreground tiles
(stage 1: 974 in any 352x240 window), up to 194 visible 16x16 sprite
pieces (merged into Genesis sprites: ~60-80), 95-171 colours. VRAM plan per
stage: plane A 64x64 (256 tiles), plane B (128), window (128), tables (52),
foreground pool ~1,050, backdrop ~150, sprites ~240, HUD ~40: ~2,044 of 2,048.

Memory: 64 KiB work RAM (actor pool ~6 KiB, map/camera ~2 KiB, sprite
buffers ~1 KiB); VRAM per stage planned offline (plane tiles, resident
sprite poses, streamed poses); four palette lines (one per role: player,
enemies, scenery near, scenery far/HUD).

## 4. Architecture

- Engine: SGDK (version pinned from `native/engine/vendor/sgdk` after audit),
  m68k-elf-gcc 16.1, C with assembly only for measured hot loops.
- Actors: one pool of fixed-size records (position 16.8 fixed point,
  velocity, state, timer, hp, type); one native update function per actor
  family; families behaviour-specified from the arcade (section 6).
- Rendering: offline tools cut every arcade animation frame into Genesis
  sprite pieces (tiles pre-coloured for the stage's sprite lines, flips
  pre-resolved); at run time an actor's pose selects a prepared piece list
  that is copied to the SAT with its position. Poses resident in VRAM where
  a stage's set fits, streamed by DMA otherwise.
- Scenery: stage maps pre-converted offline to Genesis plane layout (B1's
  stage packs, tile dictionaries and backdrops are the starting data); plane
  A foreground with column streaming; plane B static/simplified backdrop;
  window plane HUD.
- Audio: convert the arcade music and effects to a Genesis driver format
  offline; the 68000 only posts events.
- Saves, menus, Home/Arcade modes, NG+: rewritten natively against the same
  rules (AGENTS.md), reusing the save format.

## 5. Reuse

Reusable (data and tools, re-targeted): B1 stage packs, scroll2 dictionaries
and backdrop pictures, per-stage palettes, the sprite atlas extraction,
`port_save.h`, menu layout rules, mkcycles.py palette cycles, the lockstep
harness and arcade traces as references.

To audit before reuse (M0): `native/engine` (earlier native engine
scaffold: player control, prepared poses, residency, adapted audio); its
README says it was never completed and parts consume arcade-shaped data.

Not reused in the game: the arcade program, the presenter, the B1 Z80 driver
(it patches the source Z80 program), `src/willow_arcade_*` translations.

## 6. Behaviour extraction method

For each actor family, in order of appearance:
1. Read the arcade routine (disassembly, `rom.dis`) and record a plain
   behaviour spec: states, timers, speeds, hit boxes, spawn rules, damage.
2. Capture reference traces from B1/the reference core (positions, states,
   hp per frame) for scripted inputs.
3. Write the native update from the spec.
4. Compare player-visible outcomes against the traces (positions over time,
   timings, damage); record any approximation.

## 7. Milestones (each ends in a playable build and measured numbers)

| M | Content | Gate |
|---|---|---|
| M0 | Audit `native/engine`; pin SGDK; offline sprite-piece cutter and stage map converter for stage 1; measure peak tiles/colours per stage | tools produce stage 1 data; budgets table filled with measured numbers |
| M1 | Stage 1 opening: scrolling foreground and backdrop, Willow walk, jump, attack, magic, HUD, music | 60 updates/s, no glitches, movement matches arcade traces |
| M2 | Stage 1 combat: first 3 enemy families, pickups, damage, death/continue | behaviour matches traces; 60 updates/s |
| M3 | **Speed gate**: stage 4 snow slope with its full object load (~100 actors) | steady 60 in the busiest scene; if not, re-plan before more content |
| M4 | Stage 1 complete incl. boss | playable end to end |
| M5-M9 | Stages 2-6, shop, story screens, ending | per stage: traces match, 60 Hz |
| M10 | Menus, saves, Arcade/Home, NG+, hardware compatibility pass | real-hardware test by the user |

Honest scale: M0-M2 are weeks of work; the whole game is months. M3 is the
go/no-go point: it proves or disproves full speed in the worst scene before
most content is written.

## 8. Risks

- Stage 4 object load may still exceed the budget natively: mitigation is
  decided at M3 (decorative particle caps, pose update rates), with the user.
- Sprite limits per scanline (20 sprites / 320 px) with many objects:
  piece cutting must minimise sprites per line; priority order for gameplay
  sprites over decoration.
- Behaviour fidelity: some arcade behaviours are subtle (boss patterns,
  RNG-driven AI); traces per family catch differences early.
- Audio conversion quality: FM conversion of the arcade score is its own
  work item.
