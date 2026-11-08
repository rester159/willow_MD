# Willow steady-state renderer redesign (45+ FPS plan)

Status: design, 2026-10-24. Evidence is in PORTING.md ("FPS work session",
"MILESTONE (v47 ...)", "Gameplay inclusive profile"). Candidate v47 is the
current baseline (`build/native-world/cartridge-output-binding-v1/candidate-v47`).

## Why incremental fixes cannot reach 45 FPS

Measured Home stage-1 gameplay (v47, normal boot, headless): ~10,200 emulated
frames per source update. Budget at 45 updates/s on NTSC is ~1.3 frames
(~170k 68000 cycles) per update, including audio (~12% of every frame).

Per update today (inclusive, gameplay):

| Work | Share | Nature |
| --- | --- | --- |
| Text/object composite prepare (pixel roles + k-means fit) | 61% | full-screen CPU re-render |
| Reservation: staging + composite tile render | ~20% | re-decodes every changed tile |
| VideoRetain VRAM compares | ~14% | hardware readback |
| Audio (interleaved) | ~12% of frame time | real-time driver |
| Source game logic (actors, poses) | ~2% | the actual game |

Source logic is cheap. The renderer re-derives the whole picture from source
pixels and from VRAM readback on every update.

## Target architecture

Every update must cost O(changes), never O(screen) or O(cache size):

1. **Palette projection (cached).** Source object/text palette groups map to
   Genesis sprite pages 2/3 through a projection recomputed only when source
   palette content changes (stage load, fades, palette jobs). Clustering runs
   over palette entries (<=64 groups x 15 pens), never over screen pixels.
   Colours are approximations of the arcade (2 x 15 pens), as the current
   composite fit already is; the projection weights by pen usage.
2. **Projected actor tiles.** Actor rows publish native sprites whose tiles are
   keyed by (source pattern tile, group, projection epoch) and decoded once
   with the pen remap. Unchanged poses reuse resident VRAM tiles; SAT entries
   take page 30/31 from the projection. The composite stays as a fallback for
   cases the projection cannot represent (none expected in gameplay).
3. **Allocation invariant instead of readback.** New tiles never overwrite VRAM
   that the displayed frame references (front/back slot ownership), so tile
   uploads are always safe ahead of the atomic SAT/CRAM/scroll switch. Remove
   per-update FrontCapture/Retain/StagePrepare readback scans.
4. **Indexed caches.** Pattern lookups must be O(1) (hash index) or bounded by
   small candidate sets; no linear scans of 1,360 slots per acquire.
5. **Bounded transfers.** One VBlank publication per update with per-update
   DMA under the measured window profile; overflow spreads tile uploads ahead.

Rolling Thunder retained-pose refresh (authorized) and whole-display skipping
(second fallback) apply only after these, per AGENTS.md.

## RAM constraint

Work RAM is full (game storage 61,184 bytes; stack watermark 3,138 bytes;
~310 bytes between deepest stack and BSS end in v45-v47). Each step must free
or reuse RAM before adding state. Candidates: projection state replaces
per-update composite state; hash index must reuse existing bucket/slot bits.

## Order of work

1. Projection module + host unit tests (this document's step 1).
2. Projected actor tile key + decode + SAT routing; integrate behind the
   existing composite fallback; Home gameplay benchmark.
3. Allocation invariant; remove readback scans; benchmark.
4. Cache index; transfer budgeting; benchmark.
5. Retained-pose reduction only if still short.

Every step: exact-equivalence or visual-regression evidence, full transfer
regression, 5 MiB link check, Home normal-boot controller test.

## Decision (user, 2026-10-24)

The user chose **"New gameplay renderer"**: write a new Genesis-native
sprite/text renderer for gameplay (per-stage/cached sprite palettes, tiles
uploaded once and reused, no per-frame VRAM readback), keeping the source game
logic and keeping the existing renderer for intro/story scenes. Retrofitting
the existing renderer's packed caches was rejected as likely to plateau.

## New gameplay renderer: concrete design (v1)

Source facts (candidate-v48): `willowNativeGameSpriteOutputEmit(family,x,y,
pattern,style)` emits one CPS 16x16 object piece; `willowNativeSpriteRowsEmit`
expands it into at most two 16x8 rows (ROM entry: dx, dy, width, row key).
Stage-1 gameplay emits ~48 pieces (96 rows), 3 object palette groups.
Free sprite VRAM outside scenery (tiles >=1360, excluding plane/SAT/scroll
tables): ~411 tiles in fragments, i.e. ~100 resident 16x16 pieces.

v1 renderer, gameplay scenes only:
1. Capture emitted pieces (x, y, catalog key, style) into a piece list that
   reuses the old renderer's row storage (unused while v1 is active).
2. Palette projection (sprite_palette_projection.h) over in-use object and
   HUD text groups -> CRAM pages 2/3; recomputed only on source palette change.
3. Piece cache: key (catalog entry, projected group slot, projection epoch)
   -> 4-tile VRAM block. Decode once from ROM row words with pen remap. Front/
   back reference counts so blocks visible in the displayed SAT are never
   reused; LRU among unreferenced blocks.
4. Uploads go ahead of the atomic switch (blocks are never visible before
   their SAT), each in guarded transfer windows; SAT + CRAM + scroll in one
   atomic window.
5. One SAT entry (16x16, hardware flip) per piece; order = source order.
   Over 80 pieces or 20 per line: deterministic rotation (documented flicker)
   rather than CPU recomposition.
6. Sprite/scenery priority: v1 uses Genesis per-tile priority instead of the
   CPU row-coverage trim; differences to be measured against the longplay.
7. HUD/text in gameplay: window plane (top) with glyph tiles, palette page
   from the projection; remaining text via the old path until replaced.
Integration: a display mode switch at gameplay entry; full redraw at the
switch (stage start), old renderer for intro/story scenes.
Testing: host prototype of steps 2-5 comparing piece output against the old
renderer's composite pixels on captured frames; then Home normal-boot runs.

## Implementation status (candidate-v48, 2026-10-24)

Code (all under `build/native-world/cartridge-output-binding-v1/candidate-v48/native`):

- `engine/src/sprite_palette_projection.h`: projection over in-use object
  groups (all 15 pens) and visible text groups (glyph pen uses); colours read
  through callbacks, workspace borrowed (no stack arrays).
- `engine/src/gameplay_pieces.h`: 100-block VRAM cache (tiles 1360-1535,
  1664-1791, 1920-1951, 1972-1983, 1985-2044), 128-entry open-addressing
  index, front/back/resident bits, LRU among unreferenced blocks; sprite keys
  exact, text-run keys 30-bit FNV + 16-bit independent check (46 bits).
- `engine/src/gameplay_renderer.h`: emit capture (same clipping as the old
  output), text runs (<=4 cells, one row, one page, start at an opaque cell;
  rescanned only when the surface revision, text scroll or layer word
  changes), projection epochs (pen-map changes only), resolve, compose,
  SAT (text first, then pieces in emission order, 80 entries max).
- `engine/src/game_display.h`: mode switch at Begin (enter needs an empty old
  residency; shop scenes and the ending composite keep the old renderer),
  native emit/prepare/commit branches, clear handling.
- `cartridge/platform_output.c`: skips sprite Retain in native mode and
  uploads pending blocks through `willowCartridgeVideoFrontStage` (refuses
  any tile visible in the captured physical front) before the grant.
- `cartridge/video_stage.c`, `video_scene_prepare.c`, `video_scene.c`:
  native SAT prepared under the reservation and published in the atomic grant.

RAM: sprite store 3,014/3,072 bytes over the idle `display.rows`; text store
914/960 over the idle `display.frames`; renderer controller ~60 bytes.
Link: 5,230,902 bytes (12,000 under 5 MiB).

Known v1 limits (to measure, not hidden): more than 80 SAT entries or
20 sprites/320 px on a line drop at the hardware; more than 48 text runs drop
(counted in `dropped`); per-line rotation is not implemented yet.
