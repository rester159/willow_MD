# Sorsha playable prototype

The release ROM now includes **Sorsha: Sword Trial**, entered by holding **A
while pressing Start on the title screen**. Start by itself runs the existing
Willow Stage 0 game.

This is a first playable character pass. This checkout has no completed
Madmartigan stage controller or campaign transitions to extend. Sorsha therefore
has a separate sparring mode using scenery extracted from source stage indices
1, 3, and 5 (shown as scenes 2, 4, and 6). These are composed practice arenas
with a flat floor, not completed ports of those levels. Madmartigan's exact
attack timings, enemies, bosses, scrolling, and level collision remain to be
ported before campaign integration is possible.

## Controls

| Input | Action |
| --- | --- |
| Left / Right | Walk and face that direction |
| A | Sword attack |
| B | Jump; press A in the air for an aerial attack |
| Down | Crouch; Down + A performs a low slash |
| C | Guard against an opponent in front while grounded |
| Start | Pause / resume; retry after defeat; next scene after victory |
| Left / Right while paused | Select one of the three practice scenes |
| A + C while paused | Leave the trial and enter Willow Stage 0 |

Defeat three opponents to clear a scene. Opponents have three hit points,
Sorsha has five, and an incoming attack is announced before it lands.
Each sword swing can damage an opponent once. Damage grants temporary
invulnerability. Trial combat currently has no sound effects.

## Art and implementation

- `assets/sorsha/sheet.png` is the generated source artwork: auburn hair,
  charcoal armor, shoulder plates, and a silver sword, based on the supplied
  portrait and an extracted arcade sprite reference.
- `assets/sorsha/layout.json` records the import rectangles, common scale,
  and foot anchors. The sixteen poses cover idle, four walk frames, crouch,
  jump, fall, four standing sword frames, low/air strikes, hurt, and victory.
- `tools/sorsha_assets.py` compiles the sheet to 80×64, 4bpp frames with 15
  opaque colors plus transparency. Every character palette entry is selected
  from the original game's sprite colors after Genesis RGB333 conversion.
  The extended canvas preserves the sword at full extension. Left-facing
  poses use hardware flips around the same body anchor.
- The art is an approximation of the arcade style, not original Capcom art
  or a claim of an exact pixel-for-pixel match. The scenery uses two palettes
  selected from source scene colors to fit Genesis hardware limits.
- `src/sorsha_combat.h` contains the independent controller;
  `src/sorsha_mode.h` contains the Genesis renderer, controls, and scene UI.
  These do not replace the Willow controller.

## Build and verification

Run `make release SGDK_ROOT=.tools/sgdk-build` to produce `out/rom.bin` and
`out/release/rom.bin`. Asset regeneration needs Pillow. The local
`.tools/sorsha-python` environment contains it; elsewhere set `SORSHA_PYTHON`
to a Python interpreter with Pillow installed. Generated previews and the
palette/pose manifest are under `build/sorsha-*` and `build/sorsha_assets.json`.

Combat checks:

```sh
cc -Wall -Wextra -Werror -fsanitize=address,undefined \
  tools/test_sorsha_combat.c -o build/test_sorsha_combat
build/test_sorsha_combat
```

This pass is also saved as `out/release/willow-sorsha.bin` for immediate play.
The release build and native ROM audit pass. Combat checks cover movement,
jump landing, one hit per swing, facing, directional guarding, invulnerability,
low/air attack poses, and victory. Genesis Plus GX emulator smoke tests cover
title entry, combat, scene selection, pause, defeat/retry, and returning to Willow. The
unchanged Start-only entry was also checked. This is not a full campaign or
physical-console playtest.

`tools/verify_sorsha_rom.c` is a local Genesis Plus GX/libretro capture harness,
using the existing `build/libretro.h`. Compile it with `cc -w`, then pass the
core library, ROM, and optional scenario: 0 for combat, 1 for scene menus and
returning to Willow, 2 for the original Start-only path, or 3 for defeat/retry. It writes screenshots
under `build/` and requires a core exposing the same debug symbols as the
existing `build/rom_probe.c` harness.
