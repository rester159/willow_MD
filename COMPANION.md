# Two-player Willow companion

## Art revision, 2026-10-03

The first companion did not match Willow's sprite style: it had realistic body
proportions and too much fine detail, then lost readability when reduced to
30 pixels tall. The revised eight-pose sheet uses the extracted standing Willow
block `0x0033` as its main reference: 36-pixel height, larger head and fists,
broader silhouette, short legs, and simplified flat shading. This is custom
experimental art, not an original arcade character or a source-faithful feature.

The new source is `assets/companion/sheet-v2.png`; the old `sheet.png` is retained.
The importer matches RGB888 against named original CPS palette words before
Genesis RGB333 conversion. Skin/brown/red ramps come from sprite palette 0,
with neutral fur grays and a dark brown from palette 1. The manifest records
the exact group/pen references and original words. A per-pose drawing pivot
preserves the entire punch inside the existing 48×48 canvas in both directions;
it does not change movement, timing, or collision behavior.

The revised prototype is `out/experimental/companion-v2/willow-companion-v2.bin`.
That directory also contains the old/new/Willow comparison, sprite sheet, and
headless gameplay capture. The faithful translation work in `native/intro_probe`
is unchanged. Release packaging and the 3,601-frame two-player headless smoke
test pass, including palette-slot preservation. All imported poses have been
checked for clipping; source sprites, existing arcade captures, and actual
Genesis output were visually compared. A fresh fetch of the requested longplay
section (1:40–1:58) was denied with HTTP 403, so this revision does not claim a
new longplay review.

## Prototype controls and scope

The character on the left of the supplied photo is now a playable controller-2
companion in the existing Willow Stage 0. His eight poses preserve the fur cap,
long hair, face paint, fur vest, and red-brown clothing. The final 48×48 sprite
canvas uses fifteen colors selected from the game's Genesis-converted palette,
with a character height close to Willow's.

Press **Start on controller 2** at the title or during Willow gameplay to join.
Controller 1 continues to control Willow. Both players use **D-pad to move,
A to jump, and B or C to attack**. The companion's attack is a punch. Once
joined, either controller's Start pauses/resumes the game. Configure both
Genesis gamepads in the emulator before playing.

Both characters have independent positions and animation state. The horizontal
camera follows their midpoint, and a 240-pixel separation limit keeps the party
on screen. The companion recovers near Willow after a fall. Because this
prototype has no vertical camera, companion spawn/landing rules avoid deep
source floors below the visible viewport. Willow's stage reset also returns
the companion to the starting area.

The other Willow stages and Stage 0 enemy combat are not implemented in this
checkout. This adds co-op movement and attack animations to the playable stage;
it does not add campaign progression, enemy damage, or friendly fire. Sorsha's
separate sword trial remains accessible with controller 1 A + Start at title.

Build with `make release SGDK_ROOT=.tools/sgdk-build`. The ROM is `out/rom.bin`;
this pass is also saved as `out/release/willow-2p.bin`. Generated art is in
`assets/companion/sheet-v2.png`; `tools/companion_assets.py` imports it into four
hardware sprite blocks per pose, using the same Pillow environment as Sorsha.
The import manifest and preview are `build/companion_assets.json` and
`build/companion-sheet-preview.png`.

`src/companion_control.h` holds the independent controller and floor handling;
`src/companion_render.h` reserves a separate VRAM region and palette. Sprite
updates are queued for VBlank. Integration is in `src/playable_stage0.inc`.

Verification includes the release build, native cartridge audit, sanitizer
checks for movement/jumping/punching/tether/pit recovery, and Genesis Plus GX
two-controller captures for joining, independent controls, shared camera,
pause/resume, and the original single-player entry.

Run the controller checks with:

```sh
cc -Wall -Wextra -Werror -fsanitize=address,undefined \
  tools/test_companion.c -o build/test_companion
build/test_companion
```
