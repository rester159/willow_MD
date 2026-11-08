# Native engine, milestone M0: notes (2026-11-01)

Plan: ../NATIVE_ENGINE_PLAN.md. Status: in progress.

## Toolchain (done)

- SGDK 2.11, the copy rebuilt for this compiler: `../build/sgdk-native`
  (`inc/genesis.h`: `SGDK_VERSION 2.11`; `lib/libmd.a`; no-LTO makefile).
- m68k-elf-gcc 16.1.0 (Homebrew); Java: openjdk 21 for rescomp/sizebnd.
- Build: from `game/`:
  `make -f ../build/sgdk-native/makefile_nolto.gen SGDK=../build/sgdk-native JAVA=/opt/homebrew/opt/openjdk@21/bin/java release`
- Headless check: B1's `menutest` with the RetroArch Genesis Plus GX core,
  `DUMP=<frame> DIR=<dir>`; the M0 boot ROM shows its text at frame 60.

## Audit of native/engine (earlier native attempt)

Reusable as-is (Genesis-native, prepared data):
`native/engine/src/sprite_residency.h`, `pattern_cache.h`,
`scenery_residency.h`, `scenery_stream.h`, `scenery_vdp.h`, `port_save.h`,
`port_save_sgdk.h`, `port_menu.h`, `high_scores.h`, `name_entry.h`,
`storage_lzss.h`; tools `build_tile_dictionary.py`, `build_sprite_trimmed.py`,
`pack_sprite_patterns.py`.

Rework before reuse: `sprite_templates.h` (arcade attribute layout and
"carry" quirks), the offline cutters/converters `build_sprite_blocks.py`,
`build_scenery*.py`, `build_world.py`.

Reference only (arcade transliterations under native names): `player_*.h`,
`actor*_*.h`, `gameplay_*.h`, `audio_runtime.h`, `game_audio.h`,
`campaign.*`, top-level `src/main.c`, `src/willow_arcade_*`.
`audio_output.h` / `audio_genesis.h` only behind a new native music format.

Earlier measured numbers there are partial (no whole-game FPS).

## Peak per screen (arcade frame dumps; 8x8 tiles flip-canonical, by palette group)

| Stage | Foreground | Far layer | Sprite tiles | Sprite pieces (16x16) | Colours |
|---|---|---|---|---|---|
| 1 | 848 | 909 | 146 | 66 | 136 |
| 2 | 520 | 477 | 409 | 175 | 171 |
| 3 | 644 | 774 | 150 | 56 | 122 |
| 4 | 311 | 464 | 136 | 84 | 95 |
| 5 | 497 | 788 | 214 | 93 | 118 |
| 6 | 594 | 633 | 213 | 194 | 106 |

Sources: stage 1 55 dumps (every 150 frames), stage 2/5/6 60-74 dumps,
stages 3/4 6 dumps plus a 60-VBlank stage 4 window (sparse: treat 3/4 as
lower bounds). Tool: `game/tools/peaks.py` (run from game/).

Consequences for the design:
- Foreground (<= ~850 tiles a screen) fits VRAM only with the far layer as
  a compact backdrop (B1: <= 96-192 tiles) and sprites streamed.
- Up to 194 visible 16x16 pieces: pieces must be merged offline into larger
  Genesis sprites (up to 32x32) per pose to stay under 80 sprites and the
  per-line limit.
- 95-171 colours a screen vs 61: fixed per-stage palette lines (approved).

## Stage 1 camera

Camera per VBlank logged for the whole of stage 1 (B1 reference run): the
foreground scroll position restarts in each section (x 0..0x360, y
0x200..0x300), i.e. the stage is built from several map sections.

## Gameplay foreground map format (decoded)

The gameplay foreground is a grid of 256x256-pixel blocks: grid k (pointer
table 2DBF8, shape by mode table 2DC1C) names a block per grid cell; block
b's 16x16 cells are at ROM 0x80000 + b * 0x400, column-major, 4 bytes a cell.
Stage 1 is grid 0, rows 14-15: 15 x 2 blocks = 3840 x 512 px (blocks 1-24;
the blocks B1's cell model held during play are exactly those of row 15).
The game's world camera (FF8408/FF840C) runs continuously along the stage
(x 0x1A..0x7A0+); the hardware scroll registers restart per section.
Animated tiles and doors are run-time overrides on top (handled natively
later). Tool: `game/tools/stagemap.py GRID ROW0 ROWS OUT [palette dump]`;
stage 1 output `game/data/stage1_fg.cells` and a full preview (checked:
forest start, rocks, both waterfalls, cliffs, end tower).

Stage 1 foreground as Genesis data (`game/tools/fgstats.py`):
- 5,342 unique 8x8 tiles (flip-canonical, by palette group), 23 palette
  groups;
- peak 974 tiles in any 352x240 window (x 2496, y 224: the waterfall
  cliffs); 1,241 in a full-height 352-px strip, so tiles are streamed by the
  visible window, not by full columns.

## Sprites

`native/engine/tools/build_player_sprite_art.py` still works: Willow's 140
animation poses -> 1,530 16x16 pieces (9 or 6 a pose), 624 unique 16x16
patterns, 93,674 bytes. Merging each pose's 16x16 grid into Genesis sprites
(up to 32x32) gives 4 sprites for a 3x3 pose and 2 for 3x2: 2.5-3x fewer,
which brings the worst screen (stage 6, 194 pieces) to roughly 60-80
hardware sprites. The merge step and the enemy cutter come with M2.

## VRAM plan (per stage, from these numbers)

| Use | Tiles |
|---|---|
| Plane A 64x64 (512x512 px: a stage's full height) | 256 |
| Plane B 64x32 backdrop map | 128 |
| Window plane (HUD) | 128 |
| SAT, horizontal scroll | 52 |
| Foreground pool (peak 974 + streaming margin) | ~1,050 |
| Backdrop picture | ~150 |
| Sprites (resident + streamed poses) | ~240 |
| HUD glyphs | ~40 |
| Total | ~2,044 of 2,048 |

Tight but it fits; the foreground pool is the item to watch per stage.

## M0 status

Done: toolchain, audit, per-stage peaks, foreground map format, stage 1
foreground converter and measurements, Willow pose cutting, VRAM plan.
Moved to M1/M2: Genesis tile/plane emission with streaming slots (M1),
sprite merge step and enemy cutter (M2), stages 2-6 grid rows (as each
stage is started).
