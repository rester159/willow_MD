# Willow (arcade) for Sega Genesis / Mega Drive: release V0.8 (build 2026-11-11a)

**File:** `dist/willow-v0.8.bin` (a copy of `game/willow-native-m2-20261111a.bin`)
**Size:** 5,242,880 bytes (5 MiB / 40 Mbit, the agreed cartridge size)
**SHA-1:** 374f6afb04a9ecb5c28906607723467ac37c8498
**Status:** published at the user's request ("create new v / merge, push, deploy to git. create bin, push to itch. Publish New devlog", 2026-11-11). It is test build 10a with only the version text changed (2 bytes differ: the digit and the header checksum).
**Not confirmed on hardware.** No build since 06d has been run on a console: every check of this build is on emulators. The carts' mapper, boot and save changes in it are untested on real carts.

A native Genesis game built from the arcade program's behaviour (not the
arcade code running on the Genesis).

## What is in it

- All six stages, bosses, shops, story pages, the ending, continue and
  name entry, behaving as the arcade does.
- Opening screen with the arcade's WILLOW title, ARCADE / HOME, credit
  RESTER159, 2026 and V0.8.
- Arcade mode: attract loop, coin play, DIP switches, high scores.
- Home mode: START GAME, 2 PLAYERS, OPTIONS (including CREDITS, default 3,
  music and CONTROLS); left idle, the attract's story pages and the Home
  ranking (any button returns); Start pauses; NEW GAME PLUS after a
  one-credit clear; a debug menu behind Up, Up, Down, Down, C.
- High scores and options saved in SRAM, Home and Arcade kept apart.
- Two-player co-op (Home only): one player is Willow and the other
  Madmartigan (players pick), drop-in at any time, separate energy, lives,
  gold and score, shared credits, the camera between the players, enemies
  aiming at the nearer player, stage 4's sled and snowball for two.
- Music and sound effects adapted to the Genesis sound chips.
- Art pass (user request 2026-11-04): every stage's scenery colours refitted
  (smoother colours, no square patches), stage 1's intro and story text
  fixed, stage 1's first part and stage 3's meadow with 15 colours a line,
  the stage 1 boss in the arcade's colours. Gameplay unchanged.

## Fixes since V0.7 (08c)

- Stage 2 shopkeeper (player report "hard to reach"): stage 2's shop is the
  arcade's second door type. As in the arcade, nothing shows at the door:
  stand at it and hold Up (not Left or Right) and the shop opens after a
  short wait, the player not hittable meanwhile. Before, it used stage 1's
  touch rule, so a player on the gallery above the door could not reach it,
  and a shopkeeper figure floated in the doorway (10a). The door-opening
  animation is not included yet.
- Stage 1 earth (player suggestion): the path's upper band is brown, apart
  from the olive stone (56 tiles, 861 pixels; colours unchanged). The lower
  band stays olive: its colour line has no free entry (10a).
- Not done, by the user's decision: scenery in front of the characters on
  stages 1 and 3. It covers the bottom HUD bar, and every way of keeping the
  bar shown cost speed (bridge collapse 62 -> 48-54 frames drawn per 100).

## Fixes since V0.6 (06o)

Bugs (player reports on V0.6):
- Scenery holes on every stage filled from the arcade's maps: stage 2's
  missing platforms, stage 3's tree island, stage 5's tower, stage 1's canopy
  top (06p). The platform after stage 1's first shopkeeper is probably the
  same fault, but that exact spot has not been checked.
- Stage 1 boss: the head and paw are drawn on the body's shown pose (they
  floated off it, "a disembodied limb") (07a).
- Stage 1 sorceress: her head is whole when she appears (no white strip);
  while she holds the screen the top HUD rows are on the window plane (07b).
- Stage 2: handrails, balcony fronts and the big pillar are in front of the
  characters, as in the arcade (684 cells; the Genesis chooses per 8x8 cell,
  the arcade per pixel) (07b).
- Story picture (Bavmorda): no lighter-blue squares; the sky and rock colours
  are the arcade's (07b).
- Shop picture: no grey strips in the boxes, the curtain's red stripes back
  (07b).

Speed (exact: the game's behaviour is unchanged):
- Stage 1's bridge collapse after the sorceress: frames drawn per 100 in its
  two worst 100-frame windows 61/47 (V0.6 code) -> 81/62; the window after
  -> 97 (07b, 08a, 08b).
- Frames drawn per 100 worst / median, V0.6 -> V0.7: stage 6 corridor 66/99
  -> 82/100; 2P stage 1 44/98 -> 56/100; 2P stage 6 corridor 41/53 -> 48/57;
  stage 1 route 100/100 kept; stage 5 interior top worst 25 unchanged.

## Fixes since 05w

- 05x: the six world maps rebuilt from the arcade (grey mountains, olive
  grass, textured forests); High Aldwin's robe white and grey (was green);
  the dialogue box's greys as the arcade's.
- 05y: stage 1's two cave doorways black, as the arcade (were teal with a
  brown edge); the water band at the top of stage 1's waterfall smooth
  (the tile merge had stamped it into repeating blocks).
- 05z: no grey band behind the story text and stage titles: the text is
  drawn as sprites over the scene (user's change to approved cut 3).
- 06a: High Aldwin's face and beard in skin and cream colours (was green).
- 06c: the stage titles as the arcade's: its own serif letters, white with
  grey shading and the black drop shadow, centred mid-screen, the arcade's
  timing (5 updates after the name on stages 2-6). Replaces 06b's red.
- 06d: the HUD shows during stages 2-6's titles, as the arcade; stage 1's
  story text mid-screen at the arcade's rows.

## Fixes since 06d (V0.5)

Cartridges:
- Header "SEGA SSF": the bank mapper is switched on by the EverDrives (06d
  never paged stage 1's boss body, stage 6 or the ending on them, and did not
  boot on a Mega EverDrive PRO). Both mapper protocols are supported, found at
  boot; a cart that pages nothing gets a warning (06e, 06g).
- Boot puts the mapper in its power-on state; Reset in stage 1 or 6 no longer
  shows a false warning (06g). The game's own graphics DMA holds the Z80 and
  starts as SGDK's does, two risks on real consoles (06f).

Bugs:
- Pause stops the stage clock, in 1P and 2P; a long pause no longer kills the
  hero on resuming (06h).
- The hole in the scenery at the base of stage 1's waterfall is gone (06h).
- World map and story pictures no longer depend on free heap memory: the
  likely cause of the corrupted map after a death, not proven on hardware
  (06k); the map's set-up after a death is safer (06i).
- The dark bar flickering across the top of stage 1 is gone (06f).
- The far layer no longer breaks up behind the HUD: the top HUD rows are
  sprites, so the background scrolls cleanly to the top of the screen, as in
  the arcade; same look, 1P and 2P (06k, 06l).
- The shop's YES/NO cursor is the arcade's size (was twice as tall) (06n).

Speed:
- Frames drawn per 100, worst / median, 06i (06d not re-measured) to V0.6: stage 5
  interior top 25/59 -> 25/69; stage 6 corridor 54/97 -> 66/99; 2P stage 1
  37/91 -> 44/98; 2P stage 6 corridor 40/50 -> 41/53. Stage 1 route 100/100
  kept. Game speed unchanged (06l, 06m).

## Checks (headless only)

- Movement traces against the arcade: 24 of 24 match.
- Magic checks: 825 of 825 frames match in every field.
- Frames drawn per 100 (worst / median, updates per 100 worst), measured
  on 10a (this build differs only in the version text). One player: stage 1
  route 100/100 (100); bridge collapse 61/100 (100); stage 5 interior top
  30/98 (96, the accepted spearman crowd); stage 6 point 2 82/100 (100). Two
  players: stage 1 53/100 (95); stage 5 point 10 98/100; stage 5 interior
  top 25/25 (93); stage 6 point 2 49/57 (99). As V0.7 within run-to-run
  noise.

## Open issues and what is needed to close them

- No result on a real console or cart for any build since 06d. Needed: V0.8
  run on an EverDrive MD v2.6 and on a Mega EverDrive PRO (and other carts
  people have), with the console model, cart model and firmware version; does
  it boot, any warning on boot, is the top of stage 1 clean, does the stage 1
  boss look right, do stage 6 and the ending play, do high scores survive a
  power cycle.
- Stage 1 bridge collapse: still uneven (about 61 of 100 frames drawn at the
  worst moment). Each update plus drawing there is about 15% over one frame;
  it needs its own speed round. Nothing needed from players.
- Stage 5 interior top dips to about 25-30 frames drawn per 100 at its worst
  (the accepted spearman crowd); two players are slower at the heaviest spots.
- Corrupted world map after a death (BUG 5): not reproduced; two likely
  causes removed (06i, 06k). Needed: whether it still happens on V0.8, with
  the stage, emulator or cart, and a screenshot.
- Stage 3 "scrolls wildly then gets stuck": not reproduced. Needed: where in
  stage 3, what the player did just before, emulator or cart and version; a
  save state if possible.
- Random tiles on the stage 1 boss in the attract demo: not reproduced; it
  matches an emulator or cart that does not switch banks for a 5 MiB ROM
  ("SEGA SSF" header). Needed: the emulator and version.
- Stage 2 shop door: the door-opening animation is not shown yet.
- Stage 1 earth: the lower band of the path is still olive.
- Suggestion not taken up: fewer enemies in stage 2's fight (a change to the
  arcade's content; needs the user's decision).
- Small: the HUD's GOLD and TIME read blue in the shop; the story picture's
  robe shades are one green step off across cells.

## Known differences

- Not tested on real hardware, and the extended SSF save RAM of the Mega
  EverDrive X and PRO has no emulator here: saves on those carts are unproven.
- In the top 16 lines the HUD's sprites use part of the 20-sprites and 320-
  pixels-a-line limits (not in stage 6's corridors, where those rows stay on
  plane B): in a crowd up there the last sprites can lose pixels on those
  lines (stage 1 route 11 of 1,184 frames).
- The shop's cursor corners sit over the first and last letters; in the
  arcade the letters are probably on top (letter tiles here are opaque).
- A pose or new actor can show one frame late in 1P when the sprite cache is
  full late in a frame (retained-pose rule).

- Stage 1's cloud band above the far picture is plain sky: there is no
  video memory left for it (accepted by the user, 2026-11-04).
- Two-player co-op runs slower than one player: typically 30-50 frames
  drawn per 100, and at the heaviest spots (stage 2 fort, stage 5 points
  6-10, stage 6 point 5) about 25-35 per 100, with a brief slow-down to
  90-93 updates per 100 there.
- Approved presentation adaptations: native HUD, fixed colours per stage
  (close to the arcade), shared hero colours.
- While a stage title is up, the name's line uses up to 304 of the 320
  sprite pixels a scanline allows: a sprite crossing those 16 lines (the
  hero, if standing there) can lose parts until the title clears.
- High Aldwin uses Willow's colour line (for his skin tones): a hit on
  Willow on the first screen of stage 1 would flash him with her.

## User decisions reflected

Full list with dates in `AGENTS.md`. Highlights: 5 MiB cartridge; native
audio adaptation; parallax kept on every stage; stage 5's spearman crowd
kept as is ("Keep 04k"); console test waived; front end as specified;
"Players pick who", "Drop-in any time", "Everything separate", "Midpoint +
leash", "Other plays on", "Keep aiming", "Yes, shared colours", the 2P HUD
"Yes, as shown"; Home credits shared in 2P ("Yes, all of Home"); stage 4
in 2P (both on the sled, alternate coins, either player brakes, the other
hero lands beside the snowball); 2P heaviest spots "Draw every 4th update";
art pass "B, the blend"; story text and titles "Text as sprites"; titles
"like in the arcade"; "Yes, match both" (HUD during titles, story
mid-screen); "Yes, release 06d"; "HUD as sprites"; "Fix the costs first"; "Get route to 100"; publish V0.6, V0.7 and V0.8 without a cart test; "Drop stages 1/3 front".
