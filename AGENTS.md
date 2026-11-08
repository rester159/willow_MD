## Ground-up native engine (user decision, 2026-11-01)

After builds t/u the user said smooth full speed without glitches is
required ("if I don't have smoothness and full speed, no glitches this game
is dead"), rejected steady pacing, and chose "Plan ground-up engine" after
measurements showed the B1 architecture (arcade program on the 68000 plus a
presenter) cannot hold 60 even with a 2x CPU overclock. The plan is
NATIVE_ENGINE_PLAN.md: a new Genesis-native game (sgdk-native-doctrine), B1
kept only as the research/trace instrument. The fidelity contract in that
plan's section 2 is a proposal until the user agrees.

## Parallax restored (user decision, 2026-11-02)

Asked "i think we are missing one level of parallax. would it be possible
to add it back?", shown measured options, the user chose option (b) ("can we
try b"): the native engine streams the arcade's real far layer (scroll3) on
plane B, scrolled horizontally at the arcade's far-layer rate (half the
camera; far x = view x / 2 + 16, measured on stage-1 dumps). This supersedes
cut 1 (static backdrop) for the native engine. The far layer's tiles are
merged to fit VRAM next to the foreground (stage 1: 364 far tiles, foreground
merged to a 686-slot pool); colours share the scenery lines. Plane B's HUD
rows keep scroll 0 (per-row scroll), so the far layer keeps one vertical
position (no vertical parallax until the HUD leaves plane B).

Then (2026-11-02) the user asked: "for all levles, try and retain all level of
parallax". Every stage keeps all of its arcade scroll layers moving at their
arcade rates (far layer, any row/column scroll bands, special scroll effects),
horizontally and vertically where the arcade moves them. Where a Genesis limit
forces a compromise (e.g. the HUD on plane B blocking vertical far movement),
remove the limit first (move the HUD) rather than dropping a layer's motion;
report any remaining difference plainly. Stage 1 (build h, 2026-11-02): the far layer moves
vertically too (HUD rows in plane B's spare ring rows, shown by mid-frame
scroll splits); the arcade's cloud band above the far picture's top is plain
sky (no VRAM or CRAM room; measured in game/M1_NOTES.md).

## Stage 5 interior-top approximations (user decision, 2026-11-03)

Asked whether stage 5's interior top at about 30 fps dipping to 20 (full
game speed, exact behaviour) is acceptable or may differ slightly from the
arcade, the user chose "Allow small difference" (relayed by the main
session; earlier, asked directly: "Allow small approximations"). Scope:
stage 5's interior top only, aiming for about 48+ fps at its worst, full
game speed kept. Allowed: small, local behaviour approximations that keep
the play the same (e.g. far-from-player spearmen/bats updated at a lower
rate, cheaper wall checks). Not allowed: removing enemies, spawns or
content, or changing difficulty. Each approximation is listed in
game/M1_NOTES.md with frames drawn per 100 before and after.

## Stage 5 opening spearman crowd (user decision, 2026-11-03)

Offered simpler distant spearmen, slower distant spearmen, or keeping build
04k, the user chose "Keep 04k as is" (relayed by the main session, then
confirmed directly: "Keep 04k"). The
brief dip to 32-42 frames drawn per 100 during the ~600-frame stretch after
the interior-top checkpoint (13 spearmen at once) is accepted; no
approximations there. The speed pass is done. The console test of 04k is
user-owned and pending; it is not a blocker.

## Approved presentation cuts (user decision, 2026-10-31)

After playtesting the B1 cartridge ("a huge mess"), the user said it will
never be 1:1 and asked for powerful cuts, aesthetics first and gameplay
last; then approved cuts 1-4 ("do 1-4"):
1. No parallax: the far layer (scroll3) is not scrolled; plane B shows a
   static backdrop per stage area.
2. Native HUD drawn from game values in the original font, Genesis layout,
   instead of trapping every arcade text write.
3. Story/intro text on its own rows with a plain background, not over the
   scene.
4. Fixed palettes per stage; scenery tiles pre-coloured offline; sprites
   get fixed palette lines per stage (colours approximate).
These override the colour-fidelity and no-content-change wording above for
presentation only. Game logic stays the original program, lockstep
identical. After the next playtest (perf "still terrible", sprites flash
colours, flat blue instead of sky) the user approved ("ok go ahead"): fixed
sprite palettes per stage, a static backdrop picture per area instead of
the flat colour, the native HUD, and cut 6: 30 fps display (all drawing
every other VBlank; game logic every frame). Then (text screens still cut off; VRAM
full) the user approved cut 5 ("Approve cut 5"): the foreground (scroll2)
tile dictionary, similar tiles merged per stage to free VRAM. Not approved:
any gameplay cut (7). After playing the 20 fps test build (2026-10-31) the
user said "its not smooth, but much better than anything I have seen
before": cut 6 is now 20 fps (all drawing every third VBlank,
`-DB1_THIRDDISPLAY`, default); game logic still every frame. Enemy colours
use per-group pen maps with pre-coloured ROM blocks (user: "colors are
mostly right"). After build n ("jittery"), presentation is game-locked
(`-DB1_GAMELOCK`, default): a frame every 3 game updates (at most 6
VBlanks), about 18 fps; still cut 6, game logic every frame. After build p ("still
stuttery"; options measured) the user chose option (b): "Try (b) camera
60 Hz". Part of cut 6: `-DB1_VSCROLL` (default) moves plane A's scroll and
shifts the shown sprites with the camera every VBlank between
game-locked presentations (new sprite tables and cells stay game-locked);
at the drawn window's edge the view stops at that edge until the next presentation.
After build q (scrolling "not steady") the shown camera is smoothed
(`-DB1_SMOOTHCAM`, default, part of cut 6): it moves every VBlank at the
game camera's averaged speed, at most 8 px behind it, never ahead, and
snaps on jumps; scenery and world sprites use it, the HUD does not; game
logic never sees it. After build s (characters still jittery against the
gliding camera) the user chose "Sprites every 2 updates": game-locked
presentation every 2 game updates (`-DGL_N=2`, default; at most 4
VBlanks), ~23-27 sprite frames/s, at a cost of ~1-2.5 updates/s. Then (still
"stuttery", "we need something drastic") the user chose "Try steady
pacing": test builds hold the program's VBlank handler during play (as
Home pause does) so it ticks on a fixed schedule (`-DB1_PACE`, PACE_N /
PACE_D; u: 2/3 = 40 updates/s, 20 fps display; u-half: 1/2 = 30, 30 fps).
This is a speed change (uniform slow motion), offered for playtest, not
yet accepted as final.

## Retained-art integration order (user authorization, 2026-10-22)

The user explicitly answered **“Yes—integrate retained artwork now”** to:
“May I integrate your approved Rolling Thunder retained-art approach now to
finish the hardware renderer, then verify the complete linked ROM fits 5 MiB
before FPS tuning?” This permits that renderer integration before the storage
gate below. Simulation, collision, animation clocks, audio timing and current
positions must keep advancing; only fresh artwork refreshes may be reduced.
Verify the complete linked cartridge against 5 MiB before subsequent FPS tuning.
This does not authorize whole-display skipping, content removal, or treating
a component-only fit as complete-game proof.

## Cartridge capacity (latest user correction, 2026-10-18)

The delivered cartridge must be at most **5 MiB / 40 Mbit (5,242,880 bytes)**,
including code, graphics, audio, tables, headers and padding. The user's latest
message explicitly says “5mb not 4”; use the already-confirmed binary convention.
This supersedes the intervening 4 MiB requirement. Bank switching is allowed
within the total; it does not grant additional storage. Check actual final-image
bytes before delivery. Full-game storage proof must precede performance work.
Preserve research builds, but do not deliver them as the finished game or use
component-only fit as proof of full-game fit.

## Native audio adaptation (user authorization, 2026-10-18)

Preserve music, cue timing and gameplay sounds; adapt instruments to Genesis.
The user explicitly selected this over requiring exact arcade sound.
Native engine (2026-11-01): the user accepted ("Accept both") music note
onsets rounded to 60 Hz frames (mean 4.1 ms, max 8.3 ms) and the arcade's
FM sound effects played as PCM samples rendered from the original chip. Retain
source logical tracks and command semantics; verify timing and audible output.
Instrument adaptation does not authorize dropping cues or gameplay content.

For busy scenes, the user explicitly requested the Rolling Thunder approach:
reduce visible animation refresh work and reuse resident artwork while updating
current positions/visibility. Keep source simulation, collision, animation clocks,
attacks and timers intact. Report simulation, geometry presentations and fresh
animation poses separately. Do not count a frozen display as fresh movement.
Revalidate lifecycle, palette, visibility and residency before reusing artwork;
fall back to current art on incompatible transitions. No permission to remove
actors, stages or gameplay content was given.

## Performance fallback order (user clarification, 2026-10-19)

After the full-game storage gate passes, first reduce fresh animation-artwork
refreshes using the Rolling Thunder retained-pose/reprojection approach. Keep
current positions, camera, facing, visibility and ordering updated; source
simulation, collision, animation clocks, attacks, timers and audio timing keep
advancing normally. This first step is not whole-display frame skipping.
Only if measured busy-scene performance remains insufficient after this step,
use whole-display frame skipping as the second fallback. Do not skip simulation
updates or remove actors/content. Report simulation updates, geometry
presentations, fresh animation poses and skipped presentations separately.
Reference implementation reviewed: Rolling Thunder src/main.c,
src/campaign_sprite_reprojection.c and inc/rt_frame_pacer.h; findings in PORTING.md.

## Storage-first gate (user correction, 2026-10-18)

The user requires full-game storage proof before any more performance work.
The earlier 5 MiB lossless source-content model is research only and does not
prove the complete native game fits. Keep that distinction explicit and do not
resume performance work based only on the diagnostic fit. Current evidence:
`build/full-game-storage-5mib-layout-v2/layout.json`,
`build/full-game-storage-5mib-v3/capacity-decision.json` and PORTING.md. The newer
layout contains measured code/resources and passes headless storage checks,
but its campaign code is not bound to the final runtime. It is not a complete
or playable cartridge and does not open the performance gate.

Latest component evidence (still not a playable game or full-fit proof):
`build/native-world/audio-integration-status-20261018.json`. Native FM/PSG/PCM
output works in headless checks. The graphics-only pack removes 248,973 bytes
of archival ADPCM duplicated by the PCM bank; all graphics and all 40 valid
samples remain. The 4 MiB art/PCM test image omits the unfinished campaign link.
Do not confuse it with a complete cartridge. Live campaign audio clock/strobe
integration remains unverified; the audio state comparison fixture advances a
controlled test clock and deliberately includes expensive observation work.

# Willow visual fidelity

## Front-end requirements (user, 2026-11-03, relayed by the main session)

"also make sure we have
* opening screen: arcade, home, credits (with arcade title) - v0.5 bottom right
* Arcade screen, attract, dipswitches high scores, SRAM saves for high scores and otions
* Home screen: Home, Options, Debug menu with secret combo"
and "in home: start to pause". As built (04z): the opening screen shows the
arcade's WILLOW title, ARCADE and HOME, the credit `RESTER159, 2026` and
`V0.5` bottom right (the arcade font has no lower case; V0.6 from build 06o); the Arcade page
enters the arcade (attract loop until a coin), DIP switches, high scores;
high scores and options are saved per mode in SRAM (two checksummed copies);
the Home page has START GAME, OPTIONS, HIGH SCORES, CONTROLS, and the Debug
menu opens only with the secret combo Up, Up, Down, Down, C on the Home page;
Start pauses and resumes Home play (not the launching press).
Layout (user, 2026-11-05, relayed: "need center text / bottom: retser159,
2026 / bottom right v0.5"; RESTER159 kept): from build 05n ARCADE / HOME
are centred, the credit is centred on row 25 and V0.5 sits at the right of
row 25, inside the visible picture.
Home page (user, 2026-11-05, relayed: "in home * add big logo * take out
high scores (they would appear if you wait long enough, like in the
arcade- then press any button and you go back to the main home screen) *
cotnrols are insiode of options"): from 05o the Home page shows the big
logo; its ranking appears after 10 s idle (any button returns); CONTROLS
is in OPTIONS. Cloud band: no answer given, plain sky kept (option a).
Then (05p, user: "take out BACK from home page. if you press B you will go
back"): no BACK row on Home or Home OPTIONS; B backs out.
Then (05q, user: "even in optoins. no need for a back in the menu"): no
BACK row on any port menu (Home, Options, Controls, Music, Arcade, DIP
switches, High scores, Debug); B backs out everywhere; the "B BACK" hint
stays. User: "in options, take willow title out": the Options page shows
only its OPTIONS heading.
Then (05r, user, relayed: "can you add the attract sequence to home as well
(not gameplay, the images with the story)"): Home sitting idle 10 s runs
the arcade attract loop's story pages in the arcade's order and timing,
with the Home ranking in the ranking's place; no demo play, no INSERT
COIN; any button returns to the Home page (it replaces the 05o idle
ranking). Then the user, shown the logo/notice, Capcom credit and FBI
pages: "take this oiut of the attract": the Home loop is the cave, Willow,
Madmartigan, the yellow lines, the Eborsisk, the Home ranking, repeating.
The Arcade attract loop is unchanged (all its pages and the demo).
Asked directly (2026-11-04): Arcade attract "Home only (Recommended)": the
Arcade keeps the arcade's full loop. Stage 1 cloud band: "Accept plain sky
(Recommended)": plain sky is the accepted final state.
Then (2026-11-04), asked directly whether build 05r is the finished port,
the user chose "Accept 05r as final": game/willow-native-m2-20261105r.bin
(SHA-1 429393dde81abb759cfa50cd299c688b23358b0b) is the accepted release.

## Hardware test of 06d (user report, 2026-11-05)

Relayed by the main session, the user's words: "Boots on 2012 Everdrive MD
v2.6 on Sega Genesis Model 1 VA3, but does not boot with MEDPRO. Shows
graphical corruption at top layer of screen for the background graphics.
First boss is graphically bugged." Configurations (tester: the user;
ROM dist/willow-v0.5.bin = 06d, SHA-1 dc487b504ee13f08b88a3f1c09e2e8036ced9ac0):

| Console | Cart / firmware | Result |
|---|---|---|
| Genesis Model 1, VA3 board | EverDrive MD v2.6 (2012) | boots; background corruption on the screen's top layer; stage 1 boss graphics bugged |
| Genesis Model 1, VA3 board | Mega EverDrive PRO | does not boot |

Cause found (no hardware here: from the image and the carts' documented
behaviour): 06d's header said "SEGA MEGA DRIVE" with a 5 MiB ROM. The
EverDrives switch their bank mapper on only for a "SEGA SSF" header (the
headless core enables it by ROM size, so no headless check showed it):
the v2.6 never paged banks 8-9 (stage 1's boss body, stage 6, the
ending), and the PRO would not start the image. With "SEGA SSF" the X/PRO
carts use Krikzz's extended SSF (16-bit registers, unlock bit, save RAM =
bank 31) instead of the Sega mapper's byte registers and 0xA130F1 save
switch, so 06e (src/cart.c) detects the protocol at boot and drives banks
and save RAM in either; a cart that pages nothing gets a boot warning.
The top-layer corruption is most likely the same fault (stage 1 reads
from slots 6-7 / the stray mapper writes); unconfirmed until the retest.
Test build 06e (SHA-1 3ada213c25dd53a23bdb9376533fbb6894211892) is for
the user to try on both carts; dist/ stays 06d until the user confirms on
hardware. Physical results so far apply only to the rows above.
Then (2026-11-05, relayed: "on the first level, the background has
corruption at the top"; build and machine not stated): reproduced headless on
06e in Genesis Plus GX and PicoDrive as a dark bar at line 16 (late H
interrupt showing the bottom HUD's ring row). Test build 06f (SHA-1
ef0707e046bed7bb61fa64e6929357729f941a7a) fixes that and also holds the Z80 and starts the
transfer from RAM in the game's own plane DMA (hardware-only risks, as SGDK's
DMA does). Not yet tried on hardware; dist/ stays 06d.
Those reports are about the itch upload ("the version we posted on itch"),
presumably 06d, so they say nothing about 06e. Test build 06g (SHA-1
77bf06bb634675d448847c019e830118ae54b247) adds a boot-time mapper reset and
one mapper write sequence for both protocols; it supersedes 06e and 06f for
the carts. Not yet tried on hardware; dist/ stays 06d.
Then (relayed player report, BUG 3 pause clock, BUG 4 hole at the waterfall
base): test build 06h (SHA-1 5669439d6aeb7c260705a5abf03dd465ab99d1d5) fixes
both and supersedes 06g for testing; dist/ stays 06d.
Test build 06i (SHA-1 440c28ffbbe6c2cde302a0c89da2b67906a9f0c7) adds a safer
after-death map set-up (BUG 5 not reproduced) and supersedes 06h. The HUD-row
far-layer stepping (itch comment) is diagnosed; its fix is the user's choice,
not built. dist/ stays 06d.

## HUD as sprites (user decision, 2026-11-05)

The far layer in the top HUD rows stepped in whole tiles behind the HUD (itch
comment: the graphics behind it "dynamically corrupt"). Offered empty rows,
a black bar or HUD as sprites, the user chose "HUD as sprites (Recommended)"
(relayed by the main session, asked directly there). Both 1P and 2P HUDs:
same look, positions and fixed-to-screen behaviour; plane B carries the far
layer to the top as in the arcade. If it costs too much, stop and ask: the
empty rows are the fallback, the user's call.
Built as test build 06k (SHA-1 3e6c049376ce95e5cc94b5a213214a8c124da7f4,
M1_NOTES.md), which also stops packed pictures depending on the heap (a likely
cause of BUG 5); not yet tried on hardware; dist/ stays 06d.
Then, asked whether to test 06k on the carts next, the user chose "Fix the
costs first" (relayed by the main session): the sprite HUD's costs are worked
on before any cart test (stage 1 route's 78 frames drawn per 100, stage 6
area 2's top lines over the sprite limits, then a full re-measure). A target
that needs a changed HUD look, or costs enemies or content, goes back to the
user.
Then test build 06l (SHA-1 1c2e3334509edfaee0f96b8257d9bac05480d242,
M1_NOTES.md): top HUD rows back on plane B in stage 6's corridors, sprites
elsewhere; pacing changes; stage 1 route 84/100 frames drawn per 100 (not
back to 100). Not yet tried on hardware; dist/ stays 06d.
Then, asked whether to test 06l on the carts next, the user chose "Get route
to 100" (relayed by the main session): the 1P stage 1 route back to 100 frames
drawn per 100 before any cart test, no other row worse; a cheaper pose cache,
or spreading or holding pose loads under the retained-pose rule. If 100 needs
a changed HUD look or costs content, stop and give the user the options.
Test build 06m (SHA-1 535ba18df4860ff49d34fb9521a637b76d0dd0ae, M1_NOTES.md):
the route is 100/100 with updates 100, no other row worse beyond noise. Not
yet tried on hardware; dist/ stays 06d.
Test build 06n (SHA-1 a3fd167f39388ec942c04fd0d491f7db40793883) also fixes the shop's YES/NO cursor height
(user: "seems too big"); it supersedes 06m for testing.
Then (2026-11-06, relayed by the main session) the user asked "can you change
the v in the game and post on itch, with a devlog with improvements?": V0.6,
build 06o (SHA-1 a55fe7edcf4cbe78d2448ec5acb0cc50ed1a6eae, 06n with only the
version text changed), is in dist/willow-v0.6.bin. The user's request to post
replaces "dist/ stays 06d until confirmed on hardware" for this build; there
is no cart result for any build since 06d, and dist/RELEASE.md says so.
Test build 06p (SHA-1 eaa1071b2af2db358fff87e3e6d6d5a0febdd936, M1_NOTES.md)
fills the scenery holes on all stages (player report on stage 2, relayed);
old art unchanged. Not released: dist/ stays V0.6 until the user approves.
Then (2026-11-08) the user asked: "increase the version number, push to Git,
create the bin, post it on itch ... create the devlog with what's been
resolved, what's the open issues, and what we need to know to close those open
issues": V0.7, build 08c (SHA-1 ec097c205116c37f8cdc3b16f7bfb0ef7360af87, 08b
with only the version text changed), is in dist/willow-v0.7.bin. No cart
result exists for any build since 06d; dist/RELEASE.md says so.

## Story text without the grey band (user decision, 2026-11-04)

Relayed by the main session: asked about "can we take out the grey box and
make it transparent", offered text as sprites, a transparent window (flat
sky colour) or keeping the band, the user chose "Text as sprites
(Recommended)". This changes approved cut 3 (story/intro text on its own
rows with a plain background): the story text and the stage title cards
are drawn as sprites over the scene, which shows fully behind them; no grey
band. Within the per-line sprite and VRAM limits, gates passing, headless
checks only; dist/ unchanged until the user approves the build.

## Home/Arcade frontend and saves (user request, 2026-10-21)

Boot to a Home/Arcade selector using the original main-screen font and the
credit `RESTER159, 2026` on the initial mode selector only (user, 2026-11-03:
"take out 'ported by'"; it read `PORTED BY RESTER159, 2026` before). Use A
for attack and B for jump; Arcade C inserts a coin on each press and Start
starts/continues play. During Home gameplay P1 Start toggles pause/resume,
without allowing the held launch press to pause immediately. Arcade retains the original credit-based
experience and attract loop, with an accessible DIP-switch menu. Home has no
attract loop and returns to its menu after game over; provide options including
music selection. Use the other native ports as interface references, preserving
Willow's source-derived arcade behavior behind the port menus. Persist high
scores in SRAM with a versioned, interrupted-write-tolerant format. Keep Home
and Arcade rankings/settings separate. Include all added code/resources in the
final 5 MiB proof; reserve save SRAM separately from old actor-storage probes.
Validate actual native menu rendering/font use and mode handoffs headlessly.

Keep colors as close to the arcade as Genesis hardware permits. Use the original
CPS palette words as the numeric source; do not apply a blanket yellow/green tint
or discard RGB precision before palette fitting. Preserve leaf shading and the
separate neutral stone, brown wood, blue water/sky, and stage-specific lighting.

Use the user's arcade longplay extensively for visual checks, including the
matching stage and camera area: https://www.youtube.com/watch?v=BwJPTSXwrBg .
Reference timestamps and the current color-conversion design are in PORTING.md.
Compare generated art and actual emulator output; keep both players' palette
slots available when improving backgrounds. A successful build alone is not a
visual check.

## Arcade behavior requirement (user correction, 2026-10-03)

The requested product is a native, behaviorally faithful arcade port. Derive
logic, timing, state transitions, animation, and camera rules from the original
arcade program and data. Use video only for visual/color comparison. Do not
invent behavior, substitute a prototype, or call approximations a completed
port. Verify translated routines against matching arcade traces. Keep custom
Sorsha/companion experiments separate from the arcade-faithful path. Preserve
existing work, but do not integrate it into that path without source evidence.

## Native renderer direction (user correction, 2026-10-06)

Replace costly arcade-style rendering adapters with native data structures and
prepare Genesis render data ahead of time. Keep the original arcade code and
traces as the authority for behavior, animation, ordering, and latch timing.
Prefer reusable sprite templates, compact native instances, resolved tile words,
and updates to changed resources over re-decoding complete CPS banks every frame.
Precomputation must not bake in captured positions or assume a captured object
list remains valid when source animation, visibility, palettes, or residency
changes. Keep static preparation proofs distinct from live game integration.

## Performance acceptance (user correction, 2026-10-07)

The user accepts measured gameplay above 50 FPS. Perfect 60 FPS is no longer a
completion blocker. Keep the arcade behavior requirement and normal-speed
emulation; measure actual source update bodies and presentation separately.
The original main-loop sleep count is not an update/FPS counter. Opening-route
measurements do not establish performance for other stages or heavier combat.

Latest user correction (2026-10-10): 48 FPS at the worst gameplay moments is
acceptable, with arcade-speed behavior preserved. The earlier 48.15 processing
figure was milliseconds, not FPS; it does not prove this acceptance threshold.

## RetroArch launch preference

Always launch windowed and in the background without stealing focus. Use the
launcher with macOS Launch Services `open -g`; never activate or raise the app
or send UI keys after launching unless the user explicitly requests it. Prefer
headless validation. Do not restart the running game merely to acknowledge this
preference. Keep normal-speed approximately 60 FPS playback with VSync.

User correction (2026-11-03, relayed by the main session): "why do you open
so many retroarch windows, just do one headless". Do not launch RetroArch or
any emulator GUI for builds, not even test builds: every check is headless.
Launch only when the user explicitly asks, and then a single window. Build
summaries name the newest build file instead.

Latest user preference supersedes unsolicited launch checks: do not launch or
interact with RetroArch during development. Use headless libretro tests. Only
launch it when the user explicitly asks, then keep it windowed and do not take
focus. Do not use an app screenshot/input operation that activates it.

This restriction also applies to MAME and any other emulator or preview window:
do not start a GUI, switch applications, or briefly enter fullscreen for a test.
Keep development checks headless. The GUI launcher requires the explicit
`--launch` flag as an additional guard against accidental invocation; that flag
does not replace the requirement for an explicit user launch request.

MAME reference-capture recipes are blocked on macOS, even with `-video none`
and SDL dummy drivers, to avoid any native application startup taking focus.
User (2026-11-03, relayed by the main session): "stop openin mame". Never
start MAME in any form (not with -video none, not with a Lua script) nor any
emulator GUI; only the headless libretro tools, no window. A check that needs
MAME is skipped and reported as skipped.
Use existing captured traces and headless libretro checks on this machine.

## Per-level palettes (user correction, 2026-10-21)

Use a separate, source-derived background palette configuration for each of the six levels, with matching native tile color indices. Do not reuse the stage-zero fitted palette across the campaign. Preserve source-driven scene changes, fades and palette cycles, and keep both character/text palette banks separate from background banks. Current gameplay scenery still uses the shared fixed fit and overlaps object/text banks; its captured colors are rejected. Fix the resource mapping and live palette publication together, then check actual in-engine output for every level. Count the new resources in the complete5MiB image.

## Home New Game Plus (user request, 2026-10-21)

A completed Home campaign on one credit (no continues; losing lives within the
initial credit is allowed) unlocks NEW GAME PLUS in the Home main menu. Persist
that unlock in SRAM. Ordinary game-over, a continued clear, and Arcade clears
must not grant it. Starting a normal game must restore ordinary rules. NG+ is
explicitly authorized Home customization: more enemy energy and higher shop
costs. Initial tuning is double effective enemy energy and double prices.
Only add more enemies after full-cartridge storage and busy-scene hardware
headroom are proven; extra spawns are conditional, not yet enabled. Keep the
usual source arcade rules and content in Arcade mode.

## Console test waived; polish list (user decision, 2026-11-03)

Relayed by the main session, the user said: "dont worry about my test. do
the polish, take care of the small pieces". The console hardware test is no
longer a pending item: do not wait on it or list it as open. Work through
the remaining polish and simplified pieces from the arcade code: stage 4
intro scene (horses walk-in), the tower doorway wipe, the wagon-chase part at
0x2FC3A that bursts (0x492C), the missing short enemy poses (0x113FA,
0x11432, 0x1144E), ground fire (done in 04r), stage 4 sled slope classes,
more simultaneous explosions if budget allows, the boss white hit flash
(blinks in 04s), water shimmer cycles, stage 1 cloud band, torch-fringe
colours and area 6 storm colours where a better fit exists. Keep gates
passing and full speed; report frames drawn per 100 if a piece costs speed.

## 2-player co-op (user decisions, 2026-11-03)

Co-op is being designed in its own session (game/specs/coop.md). The user
said: "2P mode is only for Home". Arcade mode keeps the arcade's own
behaviour (alternating turns, one player on screen). Asked "what if in 2
player, one person is always willow, the other is always madmartigan, and
thats it", then offered Willow fixed as P1 or a choice, the user chose
"Players pick who": in 2P one player is Willow and the other Madmartigan for
the whole game, the players choosing who is who.
Then (2026-11-03) the user chose: "Drop-in any time" (Start on pad 2 joins a
Home game; a player out of lives can come back with a continue);
"Everything separate" (own energy, lives, gold and score per player);
"Midpoint + leash" (the camera follows the point between the players, the
screen edge holds them together); "Other plays on" (when one player is out,
the other keeps playing; game over when both are out).
Measured (game/specs/coop.md 4.4): a second player costs 39-50 scanlines an
update and halves frames drawn (about 30 fps typical, 13-25 at the worst).
Offered a speed pass first or accepting about 30 fps in 2P, the user chose
"Speed pass first": optimise the player update, collisions and drawing (1P
gains too), then decide the 2P frame rate with new numbers.
Then the user chose "Main session does it" (the speed pass is done by the
session that owns game/src and the build, not in parallel) and "Approve as
proposed" for the rest of game/specs/coop.md sections 2-4 (pick screen,
respawn beside the partner, continue in the HUD, nearer-player targeting,
stage script rules, split HUD); colours and the HUD are still shown to the
user as pictures before they are final.
After round 1 (build 05a: 2P proxy stage 1 77/49, stage 6 area 2 48/46),
the user chose "Yes, round 2": the main session does a second speed pass
(the player's terrain probes, shared probe results per update) before any
co-op code.
After round 2 (build 05b: 2P proxy stage 5 interior top 41/16, stage 6
area 2 49/45; the remaining cost is the actor loop, drawing, collisions and
shots), the user chose "Rework first": a structural round 3 (actor
run/draw, hit checks, shots) by the main session before co-op code.

## Structural speed round 3 before co-op (user decision, 2026-11-03)

Asked whether to rework how actors are run and drawn before co-op coding or
start co-op at the current rates, the user chose "Rework first" (relayed by
the main session). Scope: fewer objects processed per update where exact, a
cheaper hit-check pass, cheaper shots, cheaper actor sprite emission. Exact
behaviour, gates passing, headless only. Re-measure 1P and the 2P proxy at
stage 1 route, stage 5 point 10, stage 5 interior top and stage 6 area 2.
Co-op coding waits for this.

## Two-player approximations (user decision, 2026-11-03)

Asked whether, for two players, distant enemies may be updated less often or
the current rates accepted, the user chose "Allow approximations" (relayed by
the main session). Scope: 2-player co-op play only (Home mode); 1P stays
exact and unchanged (gated on the 2P flag `two_players`, src/actor.c, set by
the co-op code). Allowed: small local approximations that keep play the same,
e.g. far-from-both-players enemies updated at a lower rate, cheaper drawing
for distant actors. Not allowed: removing enemies, spawns or content, or
changing difficulty. Target ~48 fps at worst in 2P at stage 1 route, stage 5
interior top and stage 6 area 2. Each approximation is listed in
game/M1_NOTES.md with frames drawn per 100 before/after.
After round 3 (build 05c: exact speed-ups exhausted; 2P proxy stage 5
interior top 40/13, stage 6 area 2 48/45), the user chose "2P-only
approximations": in Home 2P only, at busy moments, far-off-screen enemies
may update at a lower rate and resident poses may be reused instead of
refreshed (the Rolling Thunder approach). Never remove enemies or spawns,
never change difficulty; list each approximation with its measured gain
(frames drawn per 100 before/after). 1P and Arcade stay exact.
The allowed approximations gave no real gain (the busy spots' enemies are
on screen; drawing is the cost). Offered accepting the 2P rates, a drawing
redesign first, or a steady 30 fps in 2P, the user chose "Accept, build
co-op": co-op is built now with the measured 2P rates (Home only, full game
speed; about stage 1 68/49, stage 5 interior top 40/13, stage 6 area 2
48/45 frames drawn per 100). A cheaper drawing path may come later as its
own engine project.
Who writes it (2026-11-03, relayed by the main session): "Co-op session,
patches": the co-op session writes the co-op code in a separate copy
(game/coop/work), builds and tests it headless there, and hands finished
patches (game/coop/patches/, against the main session's newest source) with
test results to the main session to apply; rebased onto its latest source
before each handoff. 1P stays exact (gates 24/24, magic clean).
Colours (2026-11-03): shown pictures of today's heroes, 1P under a shared
line-0 layout (a few near-identical shades about one step off: ~34% of
Willow's pixels, ~24% of Madmartigan's) and 2P with both heroes fitted into
one 15-colour line, the user chose "Yes, shared colours": both heroes use
the shared line-0 layout in 1P and 2P (keeping 1P exact needed ~84 KB of
second tile copies, more than the free ROM).
2P HUD (2026-11-03): shown a mock-up (scores at the top corners, TIME
centred with HI hidden in 2P, each player's VITAL and GOLD on its half, the
two bars at the bottom), the user chose "Yes, as shown" and, for the item
icons, "Shorter bars, a few icons": each player's bar is shortened to fit
3-4 of that player's item icons beside it.
After patch m4 (2P frames drawn per 100: stage 1 route 52/35, stage 5
interior top 35-40/5-11, stage 6 area 2 48/42; aiming at the lead only gave
44/15 at the stage 5 spot), the user chose "Keep aiming": enemies keep
aiming at the nearer player.
Credits (2026-11-03): the user said "in 2P payers share credits. number of
credit is 3 by default in home but can be changed in options", then, asked
whether 1P Home is limited too, chose "Yes, all of Home": Home gets a
CREDITS option (default 3); every Home continue (1P or 2P) uses one; in 2P
both players draw from the same pool. (Supersedes "Home continues free".)
2P heaviest spots (2026-11-03, relayed by the main session): offered (A)
full speed with the picture nearly frozen at the heaviest 2P spots (stage 2
fort, stage 5 points 6-10, stage 6 point 5: worst 1-13 frames drawn per
100) or (B) at least every 4th update drawn (worst 24-34 drawn, updates
90-93 per 100 there: a brief ~10% slow-down), the user chose "Draw every 4th
update": in Home 2P only (FORCE_DRAW_2P, src/main.c). 1P and Arcade unchanged.
Stage 4 in 2P (2026-11-03, relayed by the main session): both players are
held on the shield sled (life state 10; the arcade's shield poses show both
riders). Sled gold: "Alternate" (the 86 ride coins go to each player in
turn; the total stays the arcade's). Brake: "Either player" (Left on either
pad slows the sled). Snowball: "Lands beside it" (the snowball carries the
stage hero as in the arcade; the other hero is thrown off, lands on the
floor beside it and walks on freely). 1P stays exact.

## Colour and edge art pass (user request, 2026-11-04)

Relayed by the main session with 8 screenshots (game/out/audit/
user_art_feedback_20261104): "I need you to take a pass at coloring. Right
now bc of the color reduciton many scenese are very blotchy. we need to
smooth out the colors so that scene look good even with fewer colors. this
is an art pass. besides bad colors, there are a lot of 'square' edges.
those need to go. across all levels and screens". Stay within VRAM/CRAM and
5 MiB, gates passing, gameplay exact, per-level palettes and both hero lines
kept; before/after pictures per stage at the screenshots' spots.
Shown stage 1 options (game/out/audit/artpass/s1_*_options.png), the user
chose "B, the blend": scenery colours fitted by tools/palopt (perceived
error, hue weighted, tile-border seams), a light global tile merge, then
merges only where the streamed VRAM pool needs them (tools/poolmerge).
Then (stages 2-6, the fort and cave "unacceptable" in 05u, the camp snow in
05v) fixed through 05w; the user: "everything looks great outside of this"
(the camp, fixed in 05w), screens reviewed, and "Yes, release 05w": 05w
(SHA-1 a5269c28e83903ac78b8e87844ea91fdad71f776) is the release, copied to
dist/willow-v0.5.bin with dist/RELEASE.md.
Then (2026-11-04..06, relayed by the main session) the follow-up fixes
05x-06d (world maps, High Aldwin's colours, cave doorways, the waterfall
band, text as sprites, arcade-style titles, the HUD during titles, the
story mid-screen); asked whether to release 06d, the user chose "Yes,
release 06d": game/willow-native-m2-20261106d.bin (SHA-1
dc487b504ee13f08b88a3f1c09e2e8036ced9ac0, 5,242,880 bytes) is the
accepted release, copied to dist/willow-v0.5.bin with dist/RELEASE.md.
