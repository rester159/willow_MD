# Combat and life cycle specification

Arcade: Willow (Capcom CPS-1, 1989), main program `build/reference_maincpu.bin`
(sha256 0b66efe8...). Behaviour spec for the native Genesis rewrite
(NATIVE_ENGINE_PLAN.md sections 2 and 6). Continues `player.md` 3.10, `hud.md`
and `magic.md` 3. Arcade addresses are citations only.

Evidence tags (as in `player.md`):
- **VERIFIED**: two independent sources agree (disassembly read here plus the
  transliteration in `native/engine/src`, or plus a headless arcade trace).
- **SOURCE**: one source only (usually the disassembly read for this spec).
- **UNKNOWN**: not established; says where to look next.

Units as player.md: world Y increases **upward**; speeds are 8.8 px/frame;
hit box = (xOffset, width, yOffset, height), box = [x-xOffset, x-xOffset+width]
x [y-yOffset, y-yOffset+height]. One update = one 60 Hz arcade frame.
RAM: `a5` = FF8000; player record FF9846 (+0 active, +2 state, +3 action,
+6 x, +0A y, +0F facing, +14 animation node). "Frame counter" = FF8019 (the
game-update counter, low byte).

Traces used (headless lockstep harness, `.../perf/arcade/b1`, reference run,
default DIP switches = 2 lives, 3 HP, continue allowed; scratch logs only):
- **D**: `SCRIPT=die` (walk right, never attack): two hits, two pit deaths,
  continue countdown, game over.
- **G**: `SCRIPT=stand` (no input): time runs out at 0:00.
- **E**: `SCRIPT=stand` with the stage clear injected (results tally).
- **C/H**: `SCRIPT=random INVINCIBLE=1 KILLBOSS=1` (boss and stage end).

---

## 1. Collision schedule (routine 39A52, once per update, after all actors moved)

The arcade does not test every pair every frame. Publication (2244) puts each
on-screen actor (x in [camX-32, camX+416), y-camY in [0,272]) into a per-class
list ("lane", chosen by actor +0x0E). Then 39A52 runs, in order: VERIFIED
(disassembly 39A52..39AC8; transliteration notes PORTING.md 2026-10-04)

| Step | Pairs | When |
|---|---|---|
| 1 | (stage index >= 4 only) actor obstacle pushes 3B3EC | every frame |
| 2 | player vs class-6 surfaces, lanes 0 and 1 (39ADE, 39B42: platforms, obstacles, solids) | every frame |
| 3 | class-6 vs class-4 actors (3B25C, surfaces carrying enemies) | every frame |
| 4 | **player shots** (class 8, lane 0) vs: class-6 lane 0 (39F0C), class-4 lane 0 (39FD4), class-18 lane 1 (3A452) | frame & 3 = 0 or 2 |
| 5 | player shots vs: class-4 lane 1 (39FFA), class-4 lane 2 (39FAC), class-18 lane 0 (3A42C) | frame & 3 = 1 or 3 |
| 6 | **enemy projectiles** (class 10) vs player: lane 0 (3AA3C) on frames 0/2, lane 1 (3AA50) on frames 1/3 | every frame, alternating lane |
| 7 | **enemy bodies** vs player: class 4 lanes 0, 1, 2 (3A6AA), then class 18 lanes 0, 1 (3A8E2) | only frame & 3 = 3 |

- Steps 6 and 7 are skipped entirely while FF799E (service "invincible" switch,
  DSW-C bit 3 with the debug bit) is set. VERIFIED.
- A player-damage pass only runs while the player record +0 == 1
  ("vulnerable"); it is re-checked between lanes. The first contact that does
  damage ends the pass (d7 cleared). VERIFIED.
- Consequence for the native game: an enemy body overlapping Willow hurts at
  most once per 4 frames and only on frames where FF8019 & 3 == 3; enemy
  projectiles are tested every other frame per lane; shots test each enemy
  lane every other frame. Keep this schedule (it changes contact timing by up
  to 3 frames), or record the deviation. SOURCE (timing consequence derived).

### 1.1 The box test (shared by every pass)
For an attacker A and a target T with boxes from their current pose:
`(uint16)(A.x - T.x + A.xOff + T.xOff) <= A.w + T.w` and
`(uint16)(A.y - T.y + A.yOff + T.yOff) <= A.h + T.h` (unsigned, inclusive).
This is exactly "the two boxes overlap or touch". VERIFIED (3A782, 3A10C;
`combat_contact.h willowNativeCombatOverlap`).

### 1.2 Geometry tables (pose node +6 = geometry index; entry = 4 signed words at base + index*8)
| Use | Base | Tag |
|---|---|---|
| Willow / Madmartigan hurt box (player.md section 5) | 0x3B1C2 | VERIFIED |
| Class-4 enemy body, when touching the player | 0x3AC4A | VERIFIED |
| Class-4 enemy, when hit by a player shot | 0x3AE62 | VERIFIED (39FD4) |
| Class-10 enemy projectile (vs player) | 0x3B0D2 | VERIFIED (3AA3C) |
| Class-18 boss (vs player and vs shots) | 0x3B07A | VERIFIED (3A8E2, 3A4BC) |
| Class-6 surface (vs shots) | 0x3B1AA | VERIFIED (39F72) |
| Player shot (class 8) | fixed per spawn into +0x30..+0x36 (player.md 3.5, magic.md) | VERIFIED |

The body and shot boxes of an enemy can differ (two tables). Precompute both
per pose offline (`native/engine/tools/build_combat_data.py` already does).

## 2. How Willow is hurt

### 2.1 Damage sources and handlers
The handler is chosen per target kind from a byte table (0xFF = never hurts).

| Source | Selector table (kind -> handler) | Handlers | Tag |
|---|---|---|---|
| class-4 enemy body (3A6AA) | 0x3A740 (56 kinds); kind 2 never hurts | 3A782 plain box -> damage (all kinds except:) kind 13 -> 3A812 (protection item honoured), 21 -> 3A7B8 (extra rectangle when +0x3C set), 22 -> 3A808 (only when target byte +0x2E allows), 25 -> 3A848 (per-variant byte boxes 0x3AA1C/0x3A8C2, mirrored by facing) | VERIFIED |
| class-18 boss body (3A8E2) | 0x3A95A: kinds 0, 2..6 -> 3A96E plain box; kinds 1, 7.. never | 3A96E; 3A9A4 (+0x7E set: byte box 0x3AA1C) | VERIFIED |
| class-10 enemy projectile (3AA3C/3AA50) | 0x3AABC (40 kinds) | 3AAF0 plain (most kinds); kinds 1, 4, 11, 23 -> 3ABBA (protection honoured); kind 2 -> 3AB26 (protection honoured, then **stun**, 2.5); kind 8 -> 3AB6A (variant 0 only; also sets FF0EB6[owner's variant] = 1, "attack connected"); kind 10 -> 3AC30 (only variants 1..5); kind 15 -> 3ABF0 (ends the projectile: its state = 4, then damage) | VERIFIED |
| class-6 obstacle / solid surface (39D1E, 39DA6, 39E74) | 0x39B1A | push the player 2 px/frame away; damage only when \|dx\| < 16 (obstacle), < 32 (short solid) or when facing into a tall solid; never while FF799E | VERIFIED (`obstacle_contact.h` + disasm 39D1E) |
| hazard tile (collision id 39) | - | C2B8 probe, 2.4 | VERIFIED |
| pit | - | instant removal, 3.2 | VERIFIED (trace D) |

**Every hit costs exactly 1 point** regardless of the enemy or projectile
(no per-enemy damage value exists). VERIFIED (3A66C has no amount; trace D:
3 -> 2 -> 1).

### 2.2 The damage routine (3A66C; protected variant 3A642)
1. 3A642 only: if item 11 (FF835D, "protection" charges) > 0: item 11 -= 1,
   player +0 = 2, special-invulnerability timer (+0x50) = **128**, flash
   counter (+0x4E) = 1, flash phase (+0x4F) = 0, hurt timer (+0x36) = 0, redraw
   the item icons. **No HP/armour loss, no sound.** Done.
2. Player +0 = 2 (not vulnerable; blocks every later pass this frame).
3. If armour (FF8364) > 0: armour -= 1, and one armour item is consumed:
   item 10 (FF835C) if non-zero, else item 8 (FF835A), else item 9 (FF835B).
   Otherwise HP (FF8362) -= 1.
VERIFIED (disasm 3A642..3A6A8; `obstacle_contact.h willowNativeContactDamage`,
`combat_contact.h willowNativeCombatProtection`).

Note: one armour item is removed at the first armour hit even when it was
worth 2 or 3 points (the counts are per item, armour is a separate number).
SOURCE (as coded; the shop spec should confirm what items 8/9/10 add).

### 2.3 Hurt reaction (player health step C170, runs inside the player update)
Order inside one update: the player handler runs first, collision (section 1)
later in the same update, so the reaction below starts on the **next** update.

| Item | Value | Tag |
|---|---|---|
| Detection | compares HP with a cached copy (FF8365) and armour with FF8389; a change starts the reaction | VERIFIED |
| Sound | **0x82** (Willow), 0x84 (Madmartigan) | VERIFIED (disasm C1A8; audio.md: OKI sample 2) |
| HUD | vitality redraw (4A34); armour change also redraws items (4CE4) | VERIFIED |
| Hurt timer | +0x36 = **128**, -1 per update; while non-zero the player stays +0 = 2; at 0: +0 = 1 and the normal palette is restored | VERIFIED (trace D: hit at frame 2285, timer 0x80 at 2286, vulnerable again at 2414, i.e. 129 updates after the hit frame) |
| Flashing | palette swap every 4 updates, starting with the flash palette on the first reaction frame: flash = ROM **0xE5000** (Willow; +0x20 Madmartigan), normal = **0xE5080** (+0x20). 16 CPS words written into object palette 0 (Willow) / 1 (Madmartigan) at 0x91C000 | VERIFIED (disasm C1BE..C26C) |
| Knockback | **none**: position, speed, action and pose are untouched; Willow keeps walking/jumping/attacking normally | VERIFIED (trace D: x keeps +2/frame through both hits; no write to the player's motion in 3A66C/C170) |
| Other checks while hurt | hazard tiles and HP-change detection are skipped while +0x36 or +0x50 runs | VERIFIED |

Flash colours (CPS 0x0RGB): 0xE5000 = 0F00 0FFA 0FF7 0FC6 0F84 0F44 0FCC 0F44
0F84 0FC6 0FFA 0FF7 0FC6 0FC6 0FF7 0000 (red/yellow tint); protection flash
0xE5040 = 000F 0AFF 07FF 06CF 048F 044F 0CCF 044F 048F 06CF 0AFF 07FF 06CF
06CF 07FF 0000 (blue tint); normal 0xE5080 = 0222 0FD8 0C95 0A73 0851 0500
0EEE 0800 0A00 0E50 009E 006B 0037 0075 00D7 0000. VERIFIED (ROM dump).
Native: with fixed per-stage sprite palettes (approved cut 4) use one spare
palette line or a CRAM swap of Willow's line for 4 of every 8 frames.

### 2.4 Special invulnerability (+0x50)
Set by the protection item (128 updates) and by spell 10 (time stop, 255 each
frame, magic.md). While non-zero: -1 per update, the protection flash palette
(0xE5040) alternates with normal every 4 updates; at 0: +0 = 1, flash cleared,
normal palette. VERIFIED (C272..C2B6).

### 2.5 Stun (class-10 kind-2 projectiles only)
3AB26: after the (protected) damage call, when it reports a result, the player
is put into state 4 action 4 ("recover"): 30 updates of the recover poses
(player.md poses 16-17 R / 18-19 L, root table 0xD08E indexed by facing & 1),
no control, then back to normal ground action. Which stage-1 projectile is
kind 2: see `stage1_enemies.md`. SOURCE (disasm 3AB26, CF2E..CF66; the
"result" of the protected path (D0 after 4CE4) is UNKNOWN in detail).

### 2.6 Hazard tiles (collision id 39)
Each update while not hurt: probe the map at the player's x and, on even
frames, y+16 (y+0 when crouched); on odd frames y-8 (Madmartigan y-16). A cell
with collision id 39 calls 3A66C (unprotected damage). Not while FF799E.
Whether stage 1 contains id 39 cells: UNKNOWN (scan stage 0 scroll2
attributes). VERIFIED for the rule (C2B8; player.md 3.10).

## 3. Death

### 3.1 HP reaches 0
In the health step, HP <= 0 (signed) instead of the hurt reaction: player
+0 = 2, state (+2) = 4 (dying), action 0, HP = 0, vitality redrawn. No 0x82.
Dying (CED4), each update also runs the pit check and the screen clamp:
1. Action 0: clear hold counter (+0x31), sound **0x83**, timer **40**, facing =
   (facing & 1) ^ 1 (the moving bit is cleared and the side flips), death
   animation from root table **0xD436** (poses 60-64 R / 65-69 L, 8 ticks
   each, then hold).
2. Action 2: timer -1; at 0 -> state 6.
Madmartigan: timer 60 and extra sound 0x85 (later stages). VERIFIED (disasm
CED4..CF2C; `player_death.h`).

### 3.2 Pit
Each update (also while dying): if y <= FF845C (signed; the camera's lower
limit) the player goes **directly** to state 6: no death animation, no 0x83.
If FF799E (service invincibility) is set the player instead bounces up at
3584 (14 px/frame) in the jump state; this is the "rebound" flag of player.md
open question 3 (a service switch, not gameplay). VERIFIED (C33E; trace D:
y 2 -> -3 at frame 2518, state 2 -> 6 in the same update, no 0x83).
Stage 1 pits seen in trace D near x = 0x588..0x5C1 and x = 0x7C1..0x7D5.

### 3.3 Removed (state 6, CF6A) and the life-loss hand-off
One update: clear hold counter, player +0..+5 = 0, charge meter FF8376 = 0,
**FF83C2 = 1** (death flag), restore the normal palette. The play loop (9530)
then, next update: clears FF83C2, sets top mode FF8004 = 6 (life loss), posts
sound 0xF4 (785E) then **0x10FB**, and fades out (1E88: job 0x0C00, waits for
the fade). VERIFIED (disasm 9598; trace D: FF83C2 at 2519, mode 6 at 2520).

### 3.4 Life loss (FF8004 = 6, 96BE)
| Step | Action | Tag |
|---|---|---|
| 0 | lives FF8342 -= 1 (BCD); rank (FF8346 high byte, section 9) -= 13, floor 0; sounds 0xF0, 0xF7 | VERIFIED (trace D: lives 2 -> 1 at 2577, 57 updates after the death flag = fade time) |
| 2 | if lives == 0: top mode FF8000 += 2 -> game over path (section 5). Else if 2-player alternating (FF82A5) and the other player has lives (FF82C2): swap players (section 6). Then FF8004 = 0: stage (re)start | VERIFIED |

### 3.5 Restart at a checkpoint
What happens on FF8004 = 0 after a life loss (and after time-out, continue):

1. Stage-entry presentation (FF8004 = 0 steps 0..6, 9F70): sound **0x12**,
   the world-map picture with Willow's marker (camera 0x0A00 scene) for 3 +
   128 updates, fade in/out; ~240 updates in total. On the very first entry
   of stage 1 the intro story follows (other spec); on a restart (FF8348 set)
   play starts directly. VERIFIED (trace D: life loss 2578 -> play 2819;
   trace G: 12742 -> 12983; frame dump shows the map).
2. Resets (2DFE, 2DE6, 2EDE, 2648, 2F08): play flags FF83C2..FF8401 cleared
   (includes stage-clear, boss bar FF83CC, scene locks FF83CF/E9, time-stop
   FF83D2); camera block FF8402..FF8501 cleared; FF0F40.. cleared; **all actor
   pools re-initialised** (every enemy, projectile, pickup and effect is
   removed; they respawn from the stage spawn schedule as the camera moves);
   player record rebuilt in state 0 (stage-start gate, player.md 3).
   SOURCE (disasm; the spawn schedule's re-arming is the enemies/stage spec's
   concern).
3. **Checkpoint** (2FBA): the arcade records the last safe position every
   update while HP > 0 and the player is grounded (+0x24): FF8370 = x,
   FF8372 = y (38084). On restart: i = (x >> 8) * 2 + ((y >> 8) & 1) (y
   treated as 0 if negative), c = table **0x3058**[i] (stage 1); the
   checkpoint index FF8394 only ever increases (c > current); c = 4 is only
   allowed when FF838A is set (the camera gate at X 1952 has opened, i.e. the
   guardian there was beaten and FF83E7 set; camera event 42D82), else 3.
   Stage-1 table by x (y low / y high):

   | x | 0-767 | 768-1023 | 1024-1279 | 1280-1791 | 1792-2047 | 2048-2303 | 2304-2815 | 2816-3327 | >= 3328 |
   |---|---|---|---|---|---|---|---|---|---|
   | c | 0 | 1 | 1 / 2 | 2 | 2 / 3 | 3 | 4 | 5 | 6 |

   Checkpoint records **0x3078** + c*8 = (x, y, time BCD, music word):
   | c | x | y | time | music | camera min X |
   |---|---|---|---|---|---|
   | 0 | 192 | 72 | 3:00 | 0 | 0 |
   | 1 | 928 | 72 | 3:00 | 0 | 416 |
   | 2 | 1440 | 176 | 3:00 | 0 | 928 |
   | 3 | 1920 | 304 | 2:00 | 0 | 1408 |
   | 4 | 2400 | 208 | 2:00 | 0 | 1888 |
   | 5 | 2912 | 72 | 2:00 | 0 | 2400 |
   | 6 | 3392 | 72 | **1:00** | 1 | 3328 (x has bit 15 set: min X = x & 0xFF80) |

   Camera min X = x - 512 (>= 0) unless bit 15 (FF838C). The camera starts
   centred on the player (player.md 7). The time (FF83C6) and the music
   (788A: post the music word, then 0x01FA) come from the record.
   VERIFIED (ROM tables; trace D: respawn at (0x5A0, 0xB0) = checkpoint 2,
   camera 0x4E0/0x30, time 3:00). Correction to `hud.md` 3.5: stage 1 is
   **not** 3:00 at every checkpoint (3:00, 3:00, 3:00, 2:00, 2:00, 2:00, 1:00).
4. HP = max HP (2F00). Kept: armour, items, gold, score, magic level, form.
   Cleared: charge meter. VERIFIED (trace D: HP 1 -> 3 on respawn).
5. The checkpoint index survives continues (only a new game, 263C, and stage
   advance, 2F8A, clear it). SOURCE.

## 4. Time-out at 0:00 (resolves hud.md open question 1)
When the clock word FF83C6 == 0 during play (9536, 9606; skipped while
FF799E): restore the player palette, sound **0x14**, clear FF83C2, game step
FF8008 = 10 with timer **80**, print big text "TIME OVER" (6496 message 2:
column 15, row 12, 2x2-cell glyphs, attribute 0x0F). For 80 updates nothing
runs (the screen is frozen: no actors, no player). Then FF8004 = 6, sound
0xF4 then **0x10FB**, fade-out: **it is a normal life loss** (section 3.4):
-1 life, restart at the checkpoint with that checkpoint's time; game over if
it was the last life. VERIFIED (disasm 956E/9690; trace G: 0:00 at 12604,
life loss at 12684 = 80 updates, lives 2 -> 1, play again at 12983 with 3:00).
Warning sound before that: 0x43 every 32 frames below 0:10 (hud.md 3.5).

## 5. Game over, continue, name entry

### 5.1 Order (top mode FF8000)
Lives reach 0 -> FF8000 = 4 (B954): if the score beats one of the 10 ranking
entries (FF6FC6.., 8 bytes each) -> FF8000 = 6 name entry (B9B8, frontend
spec); else FF8000 = 8 (continue screen) directly. So a qualifying score is
entered at game over, before the continue offer; where name entry hands
over (presumably to FF8000 = 8) is UNKNOWN. SOURCE (disasm B954..B9B6; name
entry not traced).

### 5.2 Continue screen (FF8000 = 8, 9748)
| Step | Behaviour | Tag |
|---|---|---|
| 0 | if continues are disabled (FF8287, DSW-C bit 6 inverted, non-zero) or after the ending (FF8388): go to game over (step 8 sub 4). Else: big text "CONTINUE" (6496 message 1: column 17, row 8, attribute 0x0F), a scene of actors from 0x32710 (fallen Willow), 3-update delay | VERIFIED (trace D frame dump) |
| 2 | digit = **9**, timer 52, draw the digit (2x2 cells at column 23, row 11, palette 0x0B; glyph = 0x7000 + table 0x9A7E[digit] = 0B22 for 0, 0B00, 0B02 .. 0B0E for 1..8, 0B20 for 9; cells TL g, TR g+1, BL g+16, BR g+17), sound **0x15** (continue music) | VERIFIED |
| 4 | every 52 updates: sound **0x42**, digit -= 1 (BCD), redraw; at 0 -> step 6 (64 updates). Once the digit is below 8, pressing button 1 or 2 sets timer 1 and digit 1 (skips to the end). Start (P1/P2, FF8058 bits 0x30) with a credit (FF82A2 > 0) or free play (FF82A4): continues-used FF839B += 1 (max 255), credits -= 1 (BCD), job 0x0400 (probably the credit display); -> step 12 | VERIFIED (trace D: digit 9 at 3255 then -1 every 52 updates; step 6 at 3775) |
| 6 | 64 updates (Start still accepted while >= 12 left) -> step 8 | VERIFIED |
| 8 | game over: fade (0x0C14), then big text "GAME OVER" (message 0: column 15, row 12, attribute 0x0F), sound **0x14**, 120 updates; then if 2-player and the other player has lives -> swap and resume (FF8000 = 2); else back to attract (992C: modes cleared, play statistics updated) | VERIFIED (trace D: 3839 game over, 4063 attract) |
| 12 | 120 updates, then continue reset (2CEE) and resume play: FF8000 = 2 -> stage (re)start at the kept checkpoint (section 3.5) | SOURCE |

Continue reset (2CEE): lives = DIP lives (FF8288), score = 0, extend state
cleared, FF839C (play-time counter) and FF83A3 cleared, rank adjusted (9.3),
HP = max HP. Kept: magic level, gold, items, armour (hud.md 5). VERIFIED.

### 5.3 Home / Arcade (AGENTS.md)
- Arcade mode: the original credit flow above (C inserts a coin, Start
  continues). Keep the DIP "allow continue" switch.
- Home mode: no credits; proposal: the same CONTINUE countdown, Start
  continues without a coin; after GAME OVER return to the Home menu (no
  attract). A continued run never unlocks NG+ (only a one-credit clear does);
  losing lives within the first credit is allowed. Port decision, not arcade
  behaviour.

## 6. Two-player alternation
Only one player is on screen. On a life loss (96EC) or at game over (98C2),
when FF82A5 (alternating 2-player game) is set and the other player's lives
(FF82C2) are non-zero: FF8292 ^= 1 (current player), on a cocktail cabinet
(FF828E bit 1) FF8294 ^= 0x80 (flip screen), and the 128-byte player blocks
FF8342..FF83C1 and FF82C2..FF8341 are exchanged (lives, score, gold, items,
HP, armour, magic level, checkpoint, rank, ...). The other player then
restarts at **their own** checkpoint. VERIFIED (disasm 9714..973C;
`life_loss.h`, `player_status.h`). Home mode may drop 2-player (port decision).

## 7. Player shots vs enemies (normal shots; magic.md 3 covers the spells)

### 7.1 Which targets
Shots (class 8) in lane 0 against the lanes listed in section 1 (each enemy
lane every other frame). A target must have +0 == 1 (class 4/18) or bit 0 of
+0 set (class 6). The target's damage class comes from its kind:

| Target | Kind -> class table | Classes | Tag |
|---|---|---|---|
| class-4 enemy | 0x3A07A (48 kinds) | class 0 for all kinds except: 19 -> 1 (shielded), 13 -> 2, 28 -> 3; kinds 2 and 5 cannot be hit | VERIFIED |
| class-18 boss | 0x3A4E6 | kind 0 -> 0, kinds 2, 3, 5, 6 -> 1, 4 -> 2, 1 and 7+ cannot be hit | VERIFIED |
| class-6 surface | 0x39F8C | kinds 7, 8 -> 0, 10 -> 3; others cannot be hit | VERIFIED |

### 7.2 Policy for a normal shot (kind 1; table 0x3A0AA row 1, boss 0x3A4F6 row 4)
| Target class | Policy | Effect | Tag |
|---|---|---|---|
| enemy 0 | REACT (3A16A) | boxes overlap -> if target +0 != 2: target +0 = 2, energy (+0x2A) -= shot power (+0x2A), floor 0; the shot stops: +0 = 2, state 4 phase 0 (impact). If the target was already +0 == 2: shot state 4 phase 2 (deflect), no damage | VERIFIED (disasm 3A16A..3A1DC) |
| enemy 1 | SHIELD | if the target's shield is closed and the shot comes from the shielded side: deflect; else as REACT (see combat_contact.h) | SOURCE |
| enemy 2 | REACT_BASE | REACT, only against variant 0 (+0x21 == 0) | VERIFIED |
| enemy 3 | DEFLECT | always deflected | SOURCE |
| boss 0 | BOSS_DEFLECT (3A5B8) | overlap -> shot deflected (state 4 phase 2), no damage | VERIFIED |
| boss 1, 2 | REACT | as above | SOURCE |

- One shot damages at most one target, once (the lane scan stops at the
  first hit). Power = magic level + 1 (player.md 3.5). VERIFIED.
- Shot end states (normal shot, EDA8): phase 0 (hit) 15 updates of root
  table 0xEFB4 (by facing); phase 2 (deflected or wall) 12 updates of 0xEFB8;
  the shot stops moving and no longer collides; then it is removed and the
  live-shot count (FF799D) decreases. VERIFIED (disasm EDB8..EDF6).
- Target +0 == 2 is the enemy's "just hit" state; **the enemy's own routine**
  notices the energy change (usually by comparing +0x2A with a copy in +0x2B),
  plays its hit sound and reaction, and returns +0 to 1 when it is ready to be
  hit again. Energy 0 -> its death routine, rewards. Per-enemy energy, hit
  reaction, recovery time, rewards: `stage1_enemies.md`; stage-1 boss:
  `stage1_boss.md`. SOURCE.
- Score awards are posted as job **0x18xx** (1F14 queue -> 626A -> 6274):
  xx = index into the award table 0x624E (hud.md 3.1: 0x00 = 50, 0x02 = 100,
  0x04 = 200, 0x06 = 500, 0x08 = 1000, 0x0A = 2000, 0x0C = 5000, ...). Queued
  awards are dropped while the demo sound gate FF8293 is set. VERIFIED (boss:
  job 0x180A, trace H score +2000).
- Kill rewards (gold/item pickups) use 2564: reward list (table 0x2544 + word
  offsets) of class-14 pickups spawned at a point; per enemy. SOURCE.

### 7.3 NG+ (Home only, AGENTS.md)
Effective enemy energy is doubled (two energy points per source point,
`combat_contact.h` energyHalf scheme) and shop prices doubled. The boss bar
should keep showing the source-scale value (ceil(energy/2)). Arcade mode
never uses this. Port rule, not arcade behaviour.

## 8. Player damage numbers that do not exist
- No per-enemy damage, no knockback, no hit-stop, no screen shake, no
  invulnerability on respawn beyond the stage-start gate. VERIFIED (traces D,
  G; no code found).

## 9. Difficulty rank (FF8346, word; the high byte is the rank)
Enemy and boss timers read the rank's high byte (e.g. stage-1 boss attack
delay, stage1_boss.md). Rules: VERIFIED (disasm 380E8, 38134, 38146, 381BC)
- Start: DIP difficulty table 0x1B3E (DSW-B bits 0-2): 0x30, 0x20, 0x10, 0x00,
  0x40, 0x50, 0x60, 0x70 (high byte; stored in FF8291 as the base).
- Every play update (3807C): the word += 1 (DSW-B "Nando speed" 0), += 1 on
  even frames only (1), += 2 (2), += 3 (3), saturating at 0xFFFF. So the rank
  rises by 1 every 256 updates at setting 0 (about 14 per minute).
- Life loss: rank -= 13 (floor 0).
- Continue: if rank >= base: rank = base + (rank - base) / 2; rank +=
  stage table 0x3819A[stage] (0, 8, 16, 24, 24, 8; saturating 255); rank -=
  0x381A0[min(continuesUsed, 30) / 2] (8, 16, 24, ... 120; floor 0); low byte
  cleared.
- Stage advance: rank -= 4 (floor 0); rank = max(rank, base + 0x3819A[stage]).

## 10. Score extends (hud.md open question 3)
**None in this revision.** The extend routine 37FDA (compares the score's top
word with the threshold FF834E taken from the table at pointer FF828A; first,
second, then every N) is never called: no branch, jump or pointer to it exists
in the program, and FF828A is never written (0 in every capture). Lives only
change by life loss, the shop's extra-life item and continue. VERIFIED
(byte search of the whole ROM; traces show no extra life at any score).
The routine's data path (threshold FF834E, state FF8289) can be ignored.

## 11. Sounds used here (jsr 786A with d0)
| Cmd | When |
|---|---|
| 0x82 / 0x84 | hurt (Willow / Madmartigan) |
| 0x83 (+0x85 Madmartigan) | HP death |
| 0xF4 then 0x10FB | death/time-out hand-off to life loss (0xF4 also stops the charge sound) |
| 0xF0, 0xF7 | life loss step 0 |
| 0x14 | TIME OVER, GAME OVER |
| 0x15 | continue screen music |
| 0x42 | continue countdown tick |
| 0x43 | clock warning below 0:10 (every 32 frames) |
| 0x12 | stage (re)entry map scene |
| 0x10 | stage clear (unless final stage) |
| music word of the checkpoint record, then 0x01FA | play music on (re)start |
SOURCE (call sites in the disassembly; what each command sounds like:
audio.md).

## 12. Native state
Player: `vulnerable` (+0: 1 normal, 2 not hittable), `hurtTimer` (u8, 128),
`specialTimer` (u8, protection 128 / time stop), `flashCount`, `flashPhase`,
`cachedHp`, `cachedArmor`, `lastSafeX/Y`. Game: `checkpoint` (u8), `deathFlag`,
`rank` (u16), `continuesUsed`, `lives` per player, `playMode` (play, time-over
80-update freeze, life loss, restart presentation, game over / continue).
Data to extract offline: tables 0x3058, 0x3078, 0x3A740, 0x3A95A, 0x3AABC,
0x3A07A, 0x3A4E6, 0x39F8C, 0x3A0AA/0x3A4F6 rows, the five geometry tables,
palettes 0xE5000/40/80, 0x1B3E, 0x3819A, 0x381A0, big-text messages 0x648E,
digit table 0x9A7E.

## 13. Open questions (UNKNOWN)
1. Which stage-1 enemies/projectiles use the special handlers (kind 2 stun,
   protection-honouring kinds): cross-check with `stage1_enemies.md`.
2. Whether stage 1 has hazard tiles (id 39) or class-6 obstacles that hurt.
3. The exact return value of the protected path (3A642 -> 4CE4) that decides
   the kind-2 stun when a protection charge was used.
4. Name entry (B9B8) and the attract return (992C) belong to the frontend spec.
5. Not trace-checked: protection item, armour consumption, 2-player swap,
   continue taken with a credit, rank changes.
