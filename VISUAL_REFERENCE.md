# Arcade visual reference, 2026-10-03

Reference: https://www.youtube.com/watch?v=BwJPTSXwrBg

Reviewed decoded video frames across the longplay at the timestamps below.
This is a sampled visual review, not a claim that every animation or encounter
has been implemented or checked frame by frame.

| Time | Scene and color targets | Visible sprite references |
| --- | --- | --- |
| 1:45–1:58 | Layered green forest, neutral olive-gray rocks, brown wood, blue gorge | Red-haired Willow in blue/green, dark armored swordsman, blue-purple quadruped, blue/gold bag |
| 4:25 | Warm brown castle masonry, wood balconies, orange flame | Cyan sword arc, gold coin |
| 8:51 | Cool gray-blue cliffs, cyan sky, distant blue trees, yellow-green grass | Willow casting, gold chest |
| 13:16 | Navy backdrop, bare gray trees, pale coastal rocks, blue water and white waves | Blue orbs, yellow hit flash |
| 17:42 | Black caves, brown/tan rock spires and stalactites | Silver/gold Madmartigan, gold coins |
| 22:07 | Snowy blue mountains, white ground, timber village fence | Horse/rider, Madmartigan; stage clear |
| 26:33 | Green-gray foreground blocks, slate recesses, arches, pale spikes | Purple bats, green armored enemy, blue-white magic |
| 30:59 | Purple-gray masonry, deep blue windows, dark gray spikes | Madmartigan, green archer, pale arrow |
| 35:24 | Navy sky, blue-gray clouds, lavender towers, brown doors | Flying red-caped armored opponent |
| 39:50 | Purple-blue interior, lavender-gray platforms, chains | Willow standing |
| 42:40 | Dark dome, blue/purple columns, gray altar, gold candles | White/cyan electrical swirl, Willow |

Use the supplied original graphics and CPS palette words as numerical sources.
Video compression/scaling is a visual cross-check, not a replacement palette.
Do not invent missing animation frames or replace original characters with
custom art. The companion and Sorsha trial remain separate optional additions.

## Applied in this checkpoint

- Willow uses original standing block 0x0033, instead of a walking frame at rest.
- Solo magic uses original palette 8 in PAL1, preserving cyan/white and orange
  effects. With the companion present it uses the existing PAL0 approximation
  because PAL1 belongs to the companion. This compromise remains visible.
- Pause text shares Willow's palette and no longer whites out the effects slot.
- Three Sorsha practice backdrops now fit source RGB888 directly to two Genesis
  palettes using complete 8x8 tiles. Geometry remains custom practice crops.
- `tools/cps1_stage_preview.py` now defaults to the normal selector table with
  mode-specific foreground grid dimensions and upward map-Y correction.
  **Correction:** the other table is Scroll3 parallax, not an attract route.
  The preview now selects by layer, and rejects unvalidated later Scroll3 modes.

## Reproducible reference outputs

Run `python tools/visual_reference.py` with Pillow installed. It writes
`build/visual-reference/scenery-source.png`, `characters-source.png`, and
`maps.json`. The scenery sheet samples three chunks from each of eight map
selectors; these are **raw source chunks, not complete composited stages**.
Later maps still need live checks of palette changes, independent layer scroll,
entry coordinates and stage-selector relationships. Only normal map 0 has
been validated against live camera and tile RAM. Black/odd-colored sample
regions must not be treated as intended final appearance.

The character sheet decodes original Willow, Madmartigan, and armored-soldier
blocks. It is an art reference, not evidence of integrated enemy gameplay.

The remaining work includes the other characters/enemies and all bosses,
combat/damage, campaign progression, dynamic scenery/palettes, shops and audio.

## Superseding code-trace correction

The earlier `scenery-source.png` composite assumes the same selectors and camera
coordinates for both layers. That assumption is wrong. Treat that composite as
withdrawn; use `cps1_captured_background.py` with matching live captures instead.
The original character tile strips remain valid as isolated art references.
The native prototype's background compositor also needs this correction.
