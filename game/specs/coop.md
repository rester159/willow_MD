# Two-player co-op (Home mode): design proposal

Status: APPROVED by the user 2026-11-03 ("Approve as proposed" for sections
2-4; colours 4.2 and the HUD 4.7 still shown as pictures before they are
final). Next step: the speed pass (section 5), done by the session that owns
game/src ("Main session does it"). Decisions already
taken are marked **DECIDED** with the user's words (also in AGENTS.md).
Nothing here is implemented. Numbers marked MEASURED come from the ROM, the
data files or headless runs; ESTIMATE marks the rest.

## 0. What the arcade does (evidence)

- The arcade has **no simultaneous two-player game**. "1 OR 2 PLAYERS" is an
  **alternating** game (FF82A5 = 1): one player on screen; on every life loss
  the 128-byte player blocks FF8342.. / FF82C2.. are exchanged and the other
  player continues at their own checkpoint (0x9714, 0x98C2;
  specs/arcade_frontend.md 6, specs/combat.md 6). VERIFIED (disassembly and
  trace C4).
- The native Arcade mode shows the credit screen text but does not yet run
  the alternating game (src/screens.c; main.c turns pad 2 off). Building the
  alternating game is an Arcade-fidelity item, separate from this plan.
- The arcade's hidden companion (class 16 kind 0, 0x36D7E) is one character
  (same routine and art) found once per game in each of stages 1, 3 and 4
  (FF83A5 bits 0-2). It is a computer-controlled helper, never hurt.
  specs/stage1_shops_companion.md 2, stage3.md 8.3, stage4.md 8.3.
- Player hero per stage (form table 0x2F72): 1 Willow, 2 Madmartigan, 3
  Willow, 4 Madmartigan, 5 chosen on a select screen, 6 Madmartigan (areas
  0-4) then Willow (areas 5-6).

## 1. Decisions so far

| Topic | Decision | Words |
|---|---|---|
| Mode | Home only; Arcade keeps the arcade's alternating game | "2P mode is only for Home" |
| Heroes | One player is Willow, the other Madmartigan, for the whole game; the players choose who is who | "what if in 2 player, one person is always willow, the other is always madmartigan, and thats it"; then "Players pick who" |
| Join / leave | Drop-in any time: Start on pad 2 joins a Home game; a player out of lives comes back with a continue | "Drop-in any time" |
| Sharing | Everything separate: own energy, lives, gold, score | "Everything separate" |
| Camera | Follows the point between the players; the screen edge holds them together | "Midpoint + leash" |
| A player out | The other keeps playing; game over when both are out | "Other plays on" |

The hidden companion stays the arcade's computer-controlled helper in
stages 1, 3 and 4 (it follows the nearer player; section 4.6).

## 2. Proposed rules (to confirm)

### 2.1 Start, pick and drop-in
- Home menu: `1 PLAYER` / `2 PLAYERS`. 2 PLAYERS opens a pick screen built
  from stage 5's select screen ("LET'S SELECT PLAYERS.", already ported in
  src/screens.c): each pad moves its cursor; the two must pick different
  heroes; Start on both (or a timeout) begins.
- 1-player Home is unchanged: the arcade's hero for each stage.
- Drop-in during a 1-player game: Start on pad 2 shows a short "PLAYER 2
  JOIN" prompt and pauses; pad 2 gets the hero **not** being played at that
  moment; from then on both heroes are fixed for the rest of the game, even
  in stages where 1P would switch (5, 6). P2 appears beside P1 (on the floor
  under P1's x, else at P1), blinking invulnerable for 2 s.
- Join is refused during scripted moments (section 3) and shops; the prompt
  waits for the next free moment.
- If one player declines the continue (section 2.4) the game goes on as one
  player who keeps their hero; pad 2 (or 1) may join again later.

### 2.2 Separate resources
Per player: energy (and armour), lives, gold, score, items, magic level and
acorns (Willow), sword level (Madmartigan), checkpoint-independent status.
The two native player records already hold most of this (the arcade's
128-byte player block layout: FF8342..FF83C1). Shared: the stage timer, rank,
the stage's own state (chests opened, companion found, boss).
- Pickups (gold, food, items) go to the player who touches them.
- Chests: the player whose shot or sword opens it receives the reward.
- Score: each player's own kills and pickups.
- Extra lives from score: per player.
- High scores: each player qualifies separately at game over (Home ranking).

### 2.3 Shops
A shop door opens for the player who enters it; the game freezes as in 1P
(the other player waits). The shop sells from that player's gold; items the
hero cannot use show the arcade's "YOU CAN'T USE IT." (Madmartigan's swords
for Willow; Willow's magic items for Madmartigan). Each shop visit is per
player (both may enter in turn).

### 2.4 Life loss, continue, game over
- A life lost by one player: that player respawns beside the partner after
  the death animation (no checkpoint restart, no world map), 2 s invulnerable.
  Pits: same (respawn beside the partner).
- If both are dead at the same moment, or the stage timer runs out: both lose
  a life and the stage restarts at the checkpoint, as 1P (world map hold).
- Out of lives: that player gets the continue countdown drawn small in their
  HUD half while the other plays on (no full-screen continue screen); Start
  on their pad continues (Home continues are free): full energy, DIP lives,
  score 0 (as the arcade's continue). Declining (countdown 0) leaves a
  1-player game; that pad can drop in again later.
- Both out with no continue taken: the normal Home game over.
- New Game Plus: a co-op clear unlocks NG+ only if neither player continued
  (same rule as 1P: one credit, life losses allowed). PROPOSAL.

### 2.5 Camera (DECIDED: midpoint + leash)
- The camera's target is the midpoint of the two players instead of the
  player; every stage's existing camera rules (locks, arenas, scroll limits,
  vertical rules, auto-scroll) are applied to that target unchanged.
- Leash: a player cannot move past the screen edge (16 px margin) while the
  other holds the camera; walking stops there (no damage). In auto-scrolling
  parts (stage 2 wagon, stage 4 sled) the screen pushes both as it pushes P1.
- Vertical: the midpoint as well; if the players are more than one screen
  height apart (falls in stages 3, 4, 5, 6), the lower/stuck one is moved to
  the other after 2 s off screen (blink-in), as a pit respawn without the
  life loss. PROPOSAL.
- Boss arenas that lock the camera lock it for both; a player outside when
  it locks is moved inside.

### 2.6 Enemies and difficulty
- Enemies that aim at "the player" aim at the **nearer** player (horizontal
  distance, ties to P1), chosen where the arcade code reads the player's
  position for a decision (turn, shoot, chase); a cached choice per enemy
  refreshed every 8 updates to bound the cost.
- Enemy contact and projectiles test both players.
- Both players' shots, swords and spells hit enemies as in 1P.
- Difficulty unchanged (enemy energy, spawns, rank) in the first version;
  a later option may raise boss energy for two players. PROPOSAL.
- No friendly fire.

## 3. Stage scripts that assume one hero (each needs a rule)

| Stage | Script | Proposed rule |
|---|---|---|
| 1 | intro walk-in, story pages | P2 walks in behind P1 (hero's own walk) |
| 2 | wagon boarding leap and wagon chase (stage2.md 5.12, 0x2F740) | both ride the wagon; each boards by the scripted leap when they reach x 1728 (or is placed on it when the chase starts) |
| 2 | Madmartigan's road death (madmartigan.md 5.5) | applies to whichever player falls on the road |
| 3 | lake raft/boat, swell | both stand on the boat; a player in the water respawns on the boat |
| 4 | horses walk-in intro, Sorsha's camp scene, shield carry, rolling snowball, sled (stage4.md) | intro: both walk in; carried/snowball/sled: both ride or are carried side by side; Sorsha targets the nearer player |
| 5 | select screen | skipped in 2P (heroes already fixed) |
| 6 | Madmartigan -> Willow switch at the area 5 door (0x2D92E) | no switch in 2P; both continue |
| 6 | Bavmorda finale, ending | ending pages unchanged; the transformation uses Willow (if Willow is out of lives at that moment, the scene still plays) |

Each row needs reading the matching arcade script before coding; the
list is from the specs, not yet a full audit.

## 4. Presentation and budgets

### 4.1 Art
No new art: both heroes' poses already exist (Willow 70 poses / 1,449 tiles,
largest 36 tiles, 4 sprites; Madmartigan 70 / 1,184 tiles, largest 43, 6
sprites; MEASURED data/willow, data/mad layout.h). Both are reachable in every
stage today (stages 5 and 6 switch forms).

### 4.2 Colours (the main cost)
CRAM: line 0 is the player, line 1 every other sprite plus HUD glyphs, lines
2-3 the scenery. Willow uses 15 colours, Madmartigan 14; together **26
distinct** (only 0x000/0x222, 0x8EE, 0xEEE shared). MEASURED (pal.bin).
Options:
- (a) **Joint fit into line 0 for 2P only**: merge to 15 colours, weighted by
  pixel use: every merged colour stays within about 1.4 Genesis steps of its
  original (10 pairs merged, 60% of hero pixels change by about one step).
  MEASURED (offline merge on the tiles). 1P keeps exact colours if 2P uses a
  second, re-indexed copy of one hero's tiles: Madmartigan's 37,888 bytes
  (MEASURED), which is too large for the fixed ROM area's 68,744 free bytes
  together with the code; unless placed in a banked area.
- (b) Joint fit used in **both** 1P and 2P (both heroes re-indexed in place,
  no extra ROM): 1P heroes also lose about one step on 60% of pixels.
- (c) Exact 1P with a shared index order (each line-0 slot holds one Willow
  and one Madmartigan colour, re-indexed in place, no extra ROM): 1P stays
  exact, but 2P pairs some unlike colours (MEASURED best pairing: 3 pairs
  differ by 4-6 steps, e.g. Willow's blue cloak against Madmartigan's grey).
- (d) Take P2's colours from line 1: every stage's enemy fit loses colours.
  Not recommended.
Recommendation: (a) with the copy in banked ROM if section 4.5 allows,
else (b); decided by pictures shown to the user.

### 4.3 VRAM and sprites
- A second player pose slot of **43 tiles** (Madmartigan's largest),
  taken from the pose cache (src/vram.c: the player keeps 1424-1466; the
  rest is shared first fit). MEASURED pose cache and largest working set
  (pose tiles drawn in one frame): stage 1 271 / 229 (42 spare); stage 5
  315 / 227-279 (36-88 spare); stage 6 373 / 312 (61 spare). 43 tiles more
  than stage 1's and stage 5's spare: more refused enemy draws there unless
  VRAM is found (e.g. a smaller P2 slot with split uploads, or HUD/backdrop
  tiles moved).
- Pose uploads: MEASURED for P1 740-1,030 bytes per pose change (largest
  39 tiles), 109-286 bytes per drawn frame; P2 adds up to 1,376 bytes in a
  VBlank: ESTIMATE within the VBlank DMA budget (~7 KB).
- Hardware sprites: +4 (Willow) or +6 (Madmartigan) of the 80-entry table,
  plus the reserve-lives overlay for P2. The worst known moment (stage 5
  interior top, 13 spearmen, ~65 enemy sprites) needs checking for the 80
  limit and 20-per-line drop-out: PENDING.

### 4.4 CPU (MEASURED, 2026-11-03; game/coop/)
A measurement build (game/coop/p2_proxy.diff, applied to a scratch copy of
src/ from build 04s) adds each update what a second player as busy as P1
would cost: P1's player update + meter/shots/spells/sparks again (timed and
burned), a second full collision pass (results discarded; an upper bound),
and a second player_draw into an unused sprite table plus its pose upload.
Gameplay checked unchanged (RAM checksums). Headless, Genesis Plus GX.

Cost of the second player per update (scanlines; 1 line = 488 cycles; an
NTSC frame is 262 lines):

| Spot | P2 update | collisions | drawing | total median / worst | P1's own player update |
|---|---|---|---|---|---|
| Stage 1 route | 38.1 | 6.0 | 4.3 | 50.0 / 56.2 | 28.8 |
| Stage 5 interior top (s5 script) | 35.0 | 7.8 | 4.1 | 45.1 / 58.5 | 22.9 |
| Stage 5 point 10, hold right | 29.3 | 5.6 | 4.2 | 39.2 / 43.8 | 19.8 |
| Stage 6 area 2 | 36.4 | 9.9 | 4.6 | 50.3 / 55.9 | 27.7 |

Frames drawn per 100 (median / worst window); updates stay 100 / 99 in
every run (full game speed kept):

| Spot | 1 player today | 2 players |
|---|---|---|
| Stage 1 route | 100 / 59 | **50 / 41** |
| Stage 5 interior top | 81 / 40 | **37 / 22** |
| Stage 5 point 10 | 100 / 59 | **49 / 36** |
| Stage 6 area 2 | 79 / 50 | **49 / 42** |

So a second player costs about 15-19% of a frame (39-50 lines), and the
shown frame rate roughly halves (60 -> 30 fps typical; worst windows 13-25
fps). Not covered: enemies choosing between two players, the 2P camera, a
second HUD (P1's costs ~13 lines when it redraws), P2's sprites taking real
table slots. Timer note: SGDK's VDP_getAdjustedVCounter reads 16 for the
whole VBlank; the proxy timed with vtimer + the raw counter (3% high when
checked against a 48,800-cycle loop).

The player update itself (20-29 lines for P1) is the largest piece and has
not been optimised yet; making it cheaper helps 1P and 2P. Options are in
section 5.

### 4.5 ROM
MEASURED from out/rom.out (build 04s):
- Fixed area (banks 0-7, 0x000000-0x3FFFFF): code and data end at 0x3EF378:
  **68,744 bytes free**. New 2P code goes here.
- Stage-6 bank region (0x400000-0x4FFFFF, mapped at 0x300000 by the SSF2
  mapper during stage 6): ends at 0x4E9D12: **90,862 bytes free**.
- Total free in the 5 MiB image: 159,606 bytes.
ESTIMATE of new code and data: player record x2, input, pick screen, HUD
halves, targeting, camera, rules: 8-14 KB. Option 4.2 (a) adds 37,888 bytes
(or ~20 KB compressed if the pose upload can decompress; not measured).
- Work RAM: .data + .bss 40,050 bytes of 64 KB; a second player record and
  state adds ~1 KB.

### 4.6 The hidden companion with two players
It follows and throws for the nearer player; its throw triggers on either
player's attack press; the Quest-Stone it spends comes from the player who
touched it. PROPOSAL.

### 4.7 HUD
The arcade's HUD already has a 2UP score position (39,2) (hud.md 3.1).
Proposal: top rows: P1 score/energy/gold on the left, P2 on the right, TIME
centred; bottom band: each player's own bar (Willow's magic charge bar,
Madmartigan's 18-cell sword bar) and items in halves; reserve lives per
player. A mock-up is to be shown before coding.

## 5. Frame-rate options (for the user)
The 1-player standard is full game speed and about 48 fps drawn at worst.
Measured, two players draw about 30 fps typical and 13-25 fps in the worst
windows (4.4). Choices:
- (A) **Speed pass first**: optimise the player update (20-29 lines today),
  collisions against two players and drawing (sgdk-68000-optimization) before
  and during 2P work; aim 2P at 30+ fps worst; 1P gains too. Needs
  measurement before promising a figure.
- (B) **Accept a lower 2P frame rate**: full game speed, about 30 fps
  typical in 2P (Home only); 1P unchanged.
- (C) Both: (A) then decide with numbers.

**DECIDED** (2026-11-03): "Speed pass first" (A); the 2P frame-rate
figure is decided afterwards with the new measurements.

Speed pass round 1 (build 2026-11-05a, by the main session; M1_NOTES "Build
2026-11-05a"): bridge-log drawing 11.6% -> 7.1% of the stage 1 route,
the player's surface probe column-indexed 3.1% -> 1.6%, stage 5 spawner fix.
The player update itself barely changed (~12k cycles an update in the foot
and wall terrain probes); next step proposed there: shared probe results per
update (a data change). Frames drawn per 100 (median / worst):

| Spot | 1P before | 1P 05a | 2P proxy before | 2P proxy 05a |
|---|---|---|---|---|
| Stage 1 route | 100 / 59 | 100 / 100 | 50 / 41 | 77 / 49 |
| Stage 5 point 10 (hold right) | 100 / 59 | 100 / 98 | 49 / 36 | 98 / 63 |
| Stage 6 area 2 | 79 / 50 | 81 / 52 | 49 / 42 | 48 / 46 |

Stage 5 interior top (s5 script) not re-measured in round 1.

Speed pass round 2 (build 2026-11-05b; M1_NOTES "Build 2026-11-05b"): the
player's probes share a 4-column cache; almost no gain (stage 6 area 2
player_update 9.38% -> 9.31% of the frame): the probes' own work, not the
lookup, is the cost; probes are ~3% of the frame. Frames drawn per 100
(median / worst):

| Spot | 1P | 2P proxy |
|---|---|---|
| Stage 1 route | 100 / 100 | 71 / 49 |
| Stage 5 point 10, hold right | 100 / 98 | 100 / 64 |
| Stage 5 interior top (new script: walk right, attack every 8, jumps; reaches the boss arena; not comparable with 81/40, 37/22) | 67 / 29 | 41 / 16 |
| Stage 6 area 2 | 82 / 52 | 49 / 45 |

Interior top self time: actor loop 7.7%, actor drawing 5.5%, game_update
with collisions 4.7%, stage5_events 3.5%, shots 6.2% (three functions),
player update 1.9% self / 7.7% inclusive. More 2P headroom needs
structural work (fewer actor dispatches, cheaper collision pass, cheaper

Speed pass round 3 (build 2026-11-05c; M1_NOTES 05c): shot_test rejects
targets outside the union of live shot boxes (exact; idle chests 8.2 -> 4.5
lines); frame rate within noise. Frames drawn per 100 (median / worst):

| Spot | 1P | 2P proxy |
|---|---|---|
| Stage 1 route | 100 / 100 | 68 / 49 |
| Stage 5 point 10 | 100 / 98 | 80 / 60 |
| Stage 5 interior top (05b script) | 60 / 31 | 40 / 13 |
| Stage 6 area 2 | 82 / 52 | 48 / 45 |

Interior-top dip, lines per update: family updates ~45 (spearmen 17.7, bats
8.1, archers 7.1), update loop 17, family draws ~27 plus ~26 draw loop and
surfaces, VBlank callback ~4. Cost is spread over many actors (1-2k cycles
each to update, ~1k to draw), each path already tuned: no exact change with
a large gain is left. More 2P speed needs processing fewer actors (an
approximation for the user to allow) or accepting these rates.
shots).

Still to measure: sprite-table peak with P2's sprites at stage 5's
interior top (80-entry cap); colour options 4.2 as pictures.

## 6. Implementation order (after approval)
1. Speed pass (section 5) by the session that owns game/src and the build
   (willow-b1-native-rewrite), as the user chose; then re-measure 2P with
   game/coop/p2_proxy.diff and decide the 2P frame rate with the user.
2. Player record x2 (player.c is a singleton today), pad 2 input, P2 draw,
   collisions vs both, nearer-player targeting: stage 1 first; measure.
3. Camera midpoint + leash; life loss / respawn / continue rules; HUD.
4. Pick screen and drop-in.
5. Stage scripts table (section 3), stage by stage, measured each time.
6. Gates: the existing trace and magic gates must pass unchanged in 1P and
   Arcade (2P code inactive there).

## 7. 2P approximations (DECIDED 2026-11-03: "2P-only approximations")
Home 2P only, active at busy moments: far-off-screen enemies updated at a
lower rate; resident poses reused instead of refreshed (Rolling Thunder
approach, AGENTS.md). Not allowed: removing enemies or spawns, changing
difficulty. Each approximation is listed here with frames drawn per 100
before/after. 1P and Arcade stay exact (approximation code inactive).

Measured by the main session (2P proxy, frames drawn per 100, median /
worst): enemies >64 px off view at half rate: stage 1 65/47, stage 5
interior top 41/16, stage 6 area 2 48/38 (no gain: the enemies at these
spots are on screen; removed). Enemies >128 px from the player at half rate
even on screen (-DEXP_NEARHALF, measurement only, beyond "keep play the
same"): 94/49, 45/15, 49/42. At the interior top and stage 6 the cost is
drawing (~100 lines), the two player updates and stage events. The 2P flag
exists: `u8 two_players` (src/actor.c, actor.h), 0 in 1P; the co-op code
sets it; -DTWO_PLAYERS_TEST sets it in test builds.
**DECIDED** (2026-11-03): "Accept, build co-op": co-op is built with these
2P rates; a cheaper drawing path may come later as its own project.

## 8. Implementation status (2026-11-03)
Code is written by the co-op session in game/coop/work and handed to the
main session as patches (game/coop/patches/; user: "Co-op session,
patches"). Joining is compiled in only with -DCOOP until the feature is done.
- m1 (applied): second player record and swap (src/coop.c), pad 2 read
  directly, join beside the lead, both update/draw/fight, hits vs both,
  score/gold/pickups per player, camera midpoint + screen-edge leash, pull
  to the lead after 2 s too far apart, respawn at the partner's last safe
  ground, lead out -> the other leads, both down -> checkpoint restart.
- m2 (applied): shared line 0 (coop/tools/line0.py), stage_sprpal(), hurt
  blink instead of the palette flash in 2P.
- m3 (applied): the 2P HUD (section 4.7 as approved; no VITAL label in 2P).
- m4 (sent): nearer-player targeting; Home "2 PLAYERS" + pick screen; the
  lead is always the stage's hero (pads keep their heroes; a player left
  alone plays the stage's hero as in 1P); the second player sits out the
  lead's scripted parts; stage 5's select skipped in 2P; P2 shopping (it
  becomes the lead); time over for both; continues count for NG+.
- m5 (sent): Home CREDITS option (0-9, default 3; every Home continue, 1P
  or 2P, uses one; shared in 2P; user 2026-11-03), the out player's blinking
  "1P/2P START" prompt. Aiming at the nearer player kept ("Keep aiming").
- Measured 2P after m4 (real), frames drawn per 100 median / worst: stage 1
  route 52/35, stage 5 interior top 35-40/5-11, stage 6 area 2 48/42.
  Without targeting: stage 5 top 44/15. 2P sits at the frame pacer's edge,
  so small changes swing these by 5-10; idle cycles are the steadier measure.
- ROM: the -DCOOP build leaves 6,460 bytes in banks 0-7 (main data of
  2026-11-03 evening).
- m6 (sent): stage 4's sled / snowball (section 9); fixes from the
  end-to-end runs (P2 VRAM from the smallest pose-cache range: the shop's
  sprite art; join retries when VRAM is held; camera on the lead during its
  scripts). End-to-end: 65 start points + a stage 1-6 + ending chain, all
  2P to the end, game speed kept (coop/e2e.py; numbers in coop-m6.md).
- Still to do: switch -DCOOP on in the release (user's call).

## 9. Stage 4's sled and snowball in 2P (DECIDED 2026-11-03)
- Both players ride the shield sled (life state 10, drawn by the shield's
  own poses, which show both riders).
- Sled gold "Alternate": the 86 ride coins go to each player in turn.
- Brake "Either player": Left on either pad slows the sled.
- Snowball "Lands beside it": the snowball carries the stage hero as in the
  arcade; the other hero is thrown off, lands on the floor beside the
  snowball and walks on freely.

