# Arcade front end (boot, attract, coins, start, continue, game over, rankings, DIP switches): behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin` (sha256
0b66efe8...; US set: FBI screen, Lucasfilm/Capcom U.S.A. notices). Behaviour
spec for the native Genesis rewrite's **Arcade mode** (AGENTS.md "Home/Arcade
frontend": Arcade keeps the original credit-based experience and attract loop
with a DIP-switch menu). Arcade addresses are citations only. It builds on
`hud.md` (HUD, lives display, DIP decode 1B96/1B46, new-game/continue values),
`intro.md` (Start -> world map -> forest story -> stage 1) and the rank notes in
`stage1_enemies.md` 2.3; those are not repeated.

Evidence tags (as the other specs):
- **VERIFIED**: two independent sources agree (disassembly read for this file
  plus a headless arcade trace, or plus the transliteration in `native/engine/src`).
- **SOURCE**: one source only (usually disassembly read here).
- **UNKNOWN**: not established; says where to look.

RAM is arcade work RAM (`a5` = FF8000; `a5@(-n)` lands in FF0xxx/FF8xxx-n).
"t" = harness script frame = frames since the program's first VBlank (one
60 Hz frame; the front end runs one update per frame).

---

## 0. Traces used (headless lockstep, reference program; no GUI)

Harness: a private copy of `build/.../perf/arcade/b1/lockstep.c`
(`S/lockstep_fe.c`, binary `S/lockstep_fe3`; the shared `out/lockstep.keys`
was not touched), core `perf/gpgx-cps/genesis_plus_gx_libretro.dylib`,
`OVERCLOCK=5000`, `SYNC_BIN=b1/out/sync.bin`, mask `b1/out/mask.bin`, runner
`S/run.sh NAME FRAMES`. Additions in the copy:
`SCRIPT=abs KEYS="from-to:keys;..."` over absolute t (upper case = P1 R L D U
A B, lower case = system byte: `c` coin 1, `d` coin 2, `v` service, `s` 1P
start, `t` 2P start), `DSWA/DSWB/DSWC` (hex, raw port bytes), `POKE=t:addr=val,...`
(work-RAM bytes at t), `SNDQ=1` (every word entering the sound ring FF8172,
write index FF801E, logged with t), BYTELOG lines also print the dump key `k`.
All runs: default DIP bytes 0xFF (all switches off: 1 coin 1 credit, upright
1 player, rank level 4, Nando normal, magic continue off, 2 lives, vitality 3,
demo sound on, continue on, no debug).

`S/` = `/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/frontend/`

| Run | Input | Covers | Output |
|---|---|---|---|
| A (attract) | `SCRIPT=idle`, 19000 frames (5.3 min) | power-on, 3.3 attract loops, both demos, rankings | `S/a.log` (BYTELOG 8000-800D, credits, stage...), `S/vidA/` (every 15 keys), `S/ramA/` (every 60), `S/asnd.log` (sound words), `S/cal.log` (key<->t) |
| C1 | coin t1000, coin t1100, coin-2 t1200, 1P start t1400, Right held from t2700 (walks into enemies) | credit screen, start, 2 life losses, continue screen with 2 credits ignored, game over, credit screen again (to t16000) | `S/c1.log`, `S/vidC1/` (every 10) |
| C2 | as C1 + Start t4300 | continue with a credit, second continue screen (1 credit) times out | `S/c2.log`, `S/vidC2/` |
| C3 | coin t1000, start t1400, Right from 2700, Start t4250 (no credit), coin t4300, Start t4400, button 1 at t6100 | start without credit, coin during countdown, continue, countdown skip, game over to attract | `S/c3.log`, `S/vidC3/` (every 4 from 4200) |
| C4 | 2 coins, **2P start** t1400, Right from 2700 | alternating two-player game to game over | `S/c4.log`, `S/vidC4/` |
| N1 | as C1 to t4050, score poked to 60000 at t4050 (`POKE`, after the last point award), no input after | qualification, name entry idle timeout, ranking, continue | `S/n1.log`, `S/vidN1/` (every 5 from 4000) |
| N2 | as N1 + name input `S/n2.keys` (W, X, rub-out, I, L) | letter entry, rub-out, auto-finish, stored name | `S/n2.log`, `S/vidN2/` (every 2), `S/ramN2/` (every 10) |
| T | coin, start, clock poked to 0:05 at t2600 | time over | `S/tout.log`, `S/vidT/` |
| B1 / B2 | coin t1000 / idle | per-frame blink of PRESS START BUTTON / INSERT COIN | `S/vidB1/` (1028-1157), `S/vidB2/` (3170-3429) |
| CMAX / CJAM | 11 coins 30 frames apart / coin held 200 frames, 2-frame and 1-frame pulses | credit cap, lockout, coin debounce | `S/cmax.log`, `S/cjam.log` |

Caveats: `OVERCLOCK=5000` removes CPU slowdown, so these are the program's
logical frame counts (all front-end timers count updates; the demo play could
run slower on real hardware). The harness skips the board's video-RAM self-test
branch (0x696), so the power-on test duration is not real. The name-entry score
was poked (no natural run reached 10,000 points); everything after the poke is
the program's own behaviour. Dump keys equal t+2 except across the attract demos,
which overwrite the frame counter (section 14).

---

## 1. Control structure

| Item | Value | Tag |
|---|---|---|
| Tasks | Fixed task slots: boot/attract task at slot 1 (FF0020), credit task slot 2 (FF0040), game task slot 3 (FF0060); each owns FF8000 (state), FF8004 (mode), FF8008 (step), FF800A (timer), FF8006 (second timer) while it runs | VERIFIED (code 15CC calls; port.c reads the same slots) |
| Boot task | 0x7FDA, states 0..A | VERIFIED |
| Attract task | 0x80D0: FF8000 0 init, 2 fade, 4 scenes (FF8004 0 opening, 2 character panels, 4 logo/notices, 6 demo, 8 interlude), 6 fade, 8 ranking (0x907E), 10 fade, 12 restart | VERIFIED |
| Credit task | 0x90E8: fade, then states 0 (0x91E2), 2 (0x920E), 4 (0x9228 wait for Start) | VERIFIED |
| Game task | 0x92D2 / dispatcher 0x93BE: FF8000 0 init (0x93D6), 2 play (0x93FE; FF8004 0 scene manager, 2 play, 4 stage clear, 6 life loss), 4 score qualification (0xB954), 6 name entry (0xB9B8), 8 continue / game over (0x9748) | VERIFIED |
| Attract flag | FF8293 = 1 from boot until a game starts (cleared 0x81CE / 0x92B4); gates demo sound, demo input and coin display | VERIFIED |
| Continue flag | FF799F = 1 while the continue screen runs | VERIFIED |
| Credits | FF82A2 (BCD, 0..9); FF82A3 = credits last shown | VERIFIED |
| Players | FF82A5: 0 one player, 1 alternating two players; FF8292 current player (0 P1, 1 P2) | VERIFIED |
| Inputs | FF8058 = system byte (inverted CPS 800018: bit0 coin 1, bit1 coin 2, bit2 service, bit4 1P start, bit5 2P start, bit6 test), FF8059.. previous frames; FF8060/FF8062 P1/P2 ports; FF806A = the input the game uses (0x1F88), FF806B previous | VERIFIED |
| Test switch | system bit 6 at any time -> service/test menu (0x932E -> 0x45A8C); not part of Arcade mode | SOURCE |

---

## 2. Power-on

| t | Event | Tag |
|---|---|---|
| (before t0) | Reset (0x13FC): sound ring cleared, DIPs decoded (1AC2, 1B46, 1B96), sound F0/F7, ranking table copied from ROM (section 9), RAM/VRAM self-test text `WORK RAM OK / SCROLL 1 OK / SCROLL 2 OK / SCROLL 3 OK / OBJ RAM OK` (dump `vidA/v000015`) | VERIFIED |
| 0-56 | Boot task: random seed FF8284 = 0x2472 (state 0), DIPs re-read, FF8293 = 1 (state 2), short fade (event 0x1002 speed 2), 1 update, fade out | VERIFIED |
| 70 | Boot ends; with test switch or debug DIP -> test menu, else attract task | VERIFIED |
| 79-84 | Attract init (0x821E: palettes 0x0AC4) and fade-out wait | VERIFIED |
| **86** | Attract opening begins (section 3) | VERIFIED |

The power-on test screen is board diagnostics; a native Arcade mode can show
it briefly or skip it (presentation). Correction to `intro.md` 8: its "15-330
self test / 345-825 Bavmorda" are not t values; in t the opening runs 86-829
(same timings in that agent's own `att.log`).

---

## 3. Attract loop (no credit)

Loop: opening -> character panels -> notices/logo -> demo -> interlude ->
ranking -> (back to opening). No title-music, no "how to play" screen, no
separate Capcom animation, and **the forest story intro of `intro.md` is not
in the attract loop**: it only runs after Start (and on player 2's first turn,
section 6). Strings `WARNING ... OUTSIDE OF THE COUNTRY OF JAPAN` (string 3)
and `PRESENTED BY` (string 22) exist but were not shown in 3.3 loops. VERIFIED (run A).

### 3.1 Timeline (run A, first loop; FF8004/FF8008 steps)

| t | Steps | Content | Tag |
|---|---|---|---|
| 86-266 | 0/2 (180) | Cave scene (scroll layers, rocky cave) fades in; Willow holding the baby stands centre (`v000090`) | VERIFIED |
| 266-326 | 0/4 (60) | Bavmorda's image fades in above (`v000270`) | VERIFIED |
| 326-612 | 0/6 | Lightning palette cycles; red text typed (message 0x34, style 0x1607) `SHE WANTS TO DESTROY THE HOLY BABY` / `ELORA DANAN!` (`v000405`) | VERIFIED |
| 612-780 | 0/8 | second message (0x35, style 0x1608) `THE EVIL QUEEN BAVMORDA` (`v000705`) | VERIFIED |
| 780-829 | 0/A | fade to black | VERIFIED |
| 830-1221 | 2/2 | black screen, light-blue text typed at the right: `HE WAS SELECTED / AS A PROTECTOR OF / ELORA DANAN / AGAINST QUEEN / BAVMORDA.` then `A PEACE-LOVING MAN.` (`v001005`, `v001305`) | VERIFIED |
| 1221-1500 | 2/4..A | framed Willow portrait appears at the left; name `WILLOW` (yellow) types (`v001410`) | VERIFIED |
| 1500-1550 | 2/C | fade | VERIFIED |
| 1550-2147 | 2/E..18 | `HE JOINS WILLOW'S / FIGHT AGAINST / GENERAL KAEL, / A FATAL RIVAL.` `A BRAVE WARRIOR.`, framed Madmartigan portrait at the right, name `MADMARTIGAN` (`v001905`, `v002160`) | VERIFIED |
| 2147-2196 | 2/1A | fade | VERIFIED |
| 2197-2923 | 4/2..4 | yellow text: `A SUPER GAME FROM CAPCOM ... / COMING THIS SUMMER!` (2197-2509), `A MAGNIFICENT FANTASY FILLED WITH LOVE, / FRIENDSHIP AND ADVENTURE!` (2509-2923) (`v002505`, `v002910`) | VERIFIED |
| 2923-2972 | 4/6 | fade | VERIFIED |
| 2973-3073 | 4/8..E | WILLOW logo: white flash, then dark outline fading up (`v002985`, `v003045`) | VERIFIED |
| 3073 | 4/10 | logo in full colour, **sound 0x81** (`v003090`) | VERIFIED |
| 3081-3176 | 4/12..14 | Lucasfilm logo fades in at the lower right (`v003165`) | VERIFIED |
| 3177-3357 | 4/18 (180) | `INSERT COIN` (19,16) palette 2, blinking **16 frames on / 16 off** (event 0x0801, 0x0110; FREE PLAY variant 0x0809); Lucasfilm notice (string 10, rows 20-28) (`v003345`) | VERIFIED (B2: on 3180-3195, off 3196-3211, ...) |
| 3357-3404 | 4/1A | notice cleared | VERIFIED |
| 3404-3584 | 4/1E (180) | Capcom notice (string 11, rows 20-26) + CAPCOM logo; INSERT COIN still blinking (`v003570`) | VERIFIED |
| 3584-3633 | 4/20 | fade | VERIFIED |
| 3639-3759 | 4/24 (120) | FBI seal, `"WINNERS DON'T USE DRUGS"` / `WILLIAM S. SESSIONS, DIRECTOR, FBI` (`v003750`) | VERIFIED |
| 3759-3808 | 4/26 | fade | VERIFIED |
| 3809-4710 | 6/2..4 | **demo play, 900 frames** (3.2) | VERIFIED |
| 4710-4951 | 6/6..8 | stage title typed over the frozen demo: message 0x0640+.. (3.2), 180-frame hold, sounds **0x08FB, 0xF4** | VERIFIED |
| 4951-5001 | 8/2 | fade | VERIFIED |
| 5002-5348 | 8/6..8 | interlude: Eborsisk picture (map grid 7 layers, camera (0..640, 768)) and text `THE STORY HAS ALREADY BEGUN / FOR ALL BRAVE PLAYERS...` (message 0x063E style 0x1615, then 0x063F style 0x1611) (`v005550`) | VERIFIED |
| 5348-5476 | 8/A (128) | Eborsisk breathes fire (actor allocated by 0x2A1A, +0x20 = 8, +0x21 = 1, at (728,904); 0x9034) (`v005745`, `v005790`) | VERIFIED |
| 5477-5527 | state 6 | fade | VERIFIED |
| 5527-5709 | state 8 | **ranking** (section 11): drawn at once (text job 0x0402), fade-in, **180-frame hold**, no music, no skip (`v006000`) | VERIFIED |
| 5710-5759 | state 10 | fade | VERIFIED |
| 5760 | state 12 -> 0 | loop restarts; opening at 5768 (no power-on part) | VERIFIED |

Loop length: 5,726 frames with the stage-2 demo, 5,682 with the stage-1 demo
(loop 1 t5760-11486, loop 2 11486-17168); ~95 s. Attract sounds in the whole
loop: only 0x81 (logo), the demo music/effects and the title ticks 0x2F, with
the 0x08FB/0xF4 fades. VERIFIED (`S/asnd.log`).

### 3.2 Demo play (attract FF8004 = 6, 0x8CB4)
- Two demos alternate, selector FF0E5A (0/1, toggled each demo): loop 1 stage
  1, loop 2 stage 2, loop 3 stage 1, ... VERIFIED (trace: FF8345 = 0/1/0).
- Set-up (0x8CDA): the frame counter FF8018 and random seed FF8284 are saved
  and replaced from table **0x8CCA** (8 bytes per demo: frame word, seed word,
  input-table pointer): demo 0 = (0x7938, 0xF697, **0x4800E**), demo 1 =
  (0x4912, 0xA649, **0x4820E**); the run is therefore deterministic. New-game
  values (2DE6, 2648, 2CCE), then HP = max HP = **5**, FF8365 = 5. VERIFIED (code + trace).
  - Demo 0 (stage index 0): FF838A = FF838B = 1, magic level FF8366 = 1,
    camera limit FF838E = 0x0D00, checkpoint FF8394 = 6: **the stage-1 boss
    arena**, clock 1:00 (`vidA/v003975`, `v004245`). Music word 1 (0x0001,
    0x01FA). VERIFIED.
  - Demo 1 (stage index 1): FF8395 = 1 (wagon at full speed), FF8398 = 0x10C0,
    FF838E = 0x7E00: **the stage-2 wagon chase** as Madmartigan, clock 3:00
    (`v010110`, `v010500`). Music word 3. VERIFIED.
- Then stage reset 2DFE, HUD 4F44, stage objects 4035E, stage events 9A92,
  fade-in (0x1002 speed 8), `INSERT COIN` blinking as on the logo screen
  (event 0x0801/0x0110 from 0x8D3E). VERIFIED (code identical to 3.1's call; dump).
- Each update: timer FF800A from **900** down; input 0x1F88, clock 4EA2, actors
  9EF2, collisions 39A52, rank 3807C, stage 3B524. Ends when the timer reaches 0
  or FF83C2 is set (player life lost). VERIFIED (both demos ran 900 frames).
- **Demo input = recorded tables**, read by 0x1F88 while FF8293 is set: entries
  of 4 bytes (unused byte, input byte as FF806A: bit0 right, 1 left, 2 down,
  3 up, 4 button 1 attack, 5 button 2 jump; 16-bit duration in frames). The
  first entry is taken at the first update; an entry with duration 0 is skipped.
  Demo 0 starts `R8 -52 R21 -2 A15 -23 B11 -10 A6 ...` (107 entries, ends with
  a 65,535-frame idle entry); demo 1 `-22 R19 -3 L43 -1 D5 DA16 D9 ...` (83
  entries to 900 frames). Decoder: `S/demo_inputs.py`. VERIFIED (ROM + replay).
- Points awarded in the demo are suppressed (hud.md 3.1, FF8293). The P2
  port's button 2 held (FF8062 == 0x20) at demo set-up draws operator statistics
  on row 27 (0x572C: credits taken, plays, average play time). SOURCE.
- End (0x8E14): sound 0xF4, frame counter and seed restored, message
  0x0600|table 0x8E50[stage] (stage 1: **0x0640** `STAGE1`, stage 2 0x0655
  `STAGE2`) style 0x1422, then 0x0600|0x8E8E[stage] (0x0641 `CROSSROAD`,
  0x0666 `CHERLINDREA'S FOREST`) style 0x8E94[stage] (0x1423/0x1424), 180 frames,
  sounds 0x08FB + 0xF4. VERIFIED (`vidA/v005205`, `v005340`).

---

## 4. Coins and credits

| Item | Value | Tag |
|---|---|---|
| Coin inputs | coin 1 (A) system bit 0, coin 2 (B) bit 1, service bit 2. Read every VBlank (1C50, from the VBlank handler 0x1570) except while FF82A1 is set (service menu) | VERIFIED |
| Debounce | per slot a 4-frame history (1BFA): a coin starts when the input was off 2 frames then on 2 frames; it is **counted when released** (history off-off after on-on); held longer than **120 frames** it is discarded (jam) | VERIFIED (CJAM: 2-frame pulse counted at release+1, 1-frame pulse ignored, 200-frame hold ignored) |
| Ratio | slot record FF8272 (A) / FF827A (B): +4 coins needed, +5 credits given, from table **0x1B2E** by inverted DSW-A bits 0-2 / 3-5 (section 12). Coins count up in +2; when count >= needed, credits += given (BCD), count = 0 (1CE2) | VERIFIED (code) / trace with 1C1C only |
| Service | service switch (same debounce) adds 1 credit | SOURCE |
| Max credits | **9**; adds above 9 are clamped (1D12). Coin lockout outputs FF8296 bits 2-3 cleared while credits >= 9 (1E72), set again below | VERIFIED (CMAX: 11 coins -> 9; FF8296 0x0C -> 0x00 at 9) |
| Coin meters | FF8296 bits 0/1 pulse per counted coin (1D26); hardware only | SOURCE |
| Coin sound | **0x3B** for every counted coin, even above 9 credits and during play (1D6A -> 78D8, not gated by demo sound) | VERIFIED (11 sounds in CMAX) |
| Display refresh | after a coin, text job 0x0400 (credit display) only while the attract flag FF8293 or the continue flag FF799F is set; in play credits change silently | VERIFIED |
| Coin in attract | the attract task's end-of-update check (0x811E) sees credits > 0: all its state is cleared and the **credit task** starts the same frame (coin at t1000-1005, counted t1007, attract gone t1007) | VERIFIED |
| Coin on the credit screen | legend and count redrawn when FF82A2 changes (0x916C) | VERIFIED |
| Statistics | every counted coin also adds 1 (BCD) to FF0A30/31, an operator counter (shown only by the demo statistics) | SOURCE |

Credit display (text job 0x0400 -> 0x55AE): `CREDIT` (string 0) at **(37,29)**,
palette 0, count as 2 digits at **(44,29)-(45,29)**, leading zero blank
("CREDIT  0" with no credit). In the 2-coins-to-start mode the label is `  COIN`
(string 16). Shown on the credit screen and the continue screen, never in play.
VERIFIED (dumps `vidC1/v001030`, `vidC3/v004320`, play frame `vidC1/v003350`).

---

## 5. Credit screen and Start

### 5.1 Credit screen (credit task 0x90E8)
1. On entry: sound **0x08FB** (music fade), fade out (event 0x1000, speed 1),
   wait; layers cleared, stage-0 palettes, text scroll reset; sound **0xF4**.
   18 frames (t1007-1025). VERIFIED.
2. State 0 (0x91E2): FF82A3 = credits; normal coin mode: blinking text job
   **0x0806 / 0x0208**: `PRESS START BUTTON` (string 6) at (17,15) palette 0,
   **8 frames on / 8 off** (B1: on 1028-1035, off 1036-1043, ...); then the
   legend (string by credits, 0x9182): credits 1 -> string 7 `1 PLAYER ONLY`,
   2..9 -> string 8 `1 OR 2 PLAYERS` (both (19,16) palette 0), free play ->
   string 9 `  FREE PLAY` (palette 2). VERIFIED (`vidC1/v001030`, `v001110`).
   2-coins-to-start mode (FF79A0): no PRESS START BUTTON; legend 0x91DA by
   credits 0/1 -> string 17 `INSERT ADDITIONAL COIN`, 2 -> 18 `PUSH 1P START`,
   3 -> 19 `PUSH 1P START OR / 2P INSERT ADDITIONAL COIN`, >= 4 -> 20
   `PUSH 1P OR 2P START` (all at (15,15) / (13,16)). SOURCE.
3. State 2: credit display (0x0400), fade in (0x1ECC speed 1).
4. State 4 (0x9228): wait for Start. **No timeout**: with credits the screen
   stays until Start (C1: t4896-16000, after a game over with 2 credits). VERIFIED.
   No music on this screen. VERIFIED.

### 5.2 Start rules (0x9228)
- Only **rising edges** (FF8058 & ~FF8059 & 0x30) in credit-screen state 4.
- 2P start (bit 5) is tested first: needs credits >= 2 (>= 4 with FF79A0)
  -> credits -= 2 (4), FF82A5 = 1. If 2P cannot start, a 1P edge in the same
  frame is tested: needs >= 1 (>= 2 with FF79A0) -> credits -= 1 (2), FF82A5 = 0.
  BCD subtraction (`sbcd`). VERIFIED (C1: 3 -> 2 at t1400; C4: 2P start 2 -> 0) /
  FF79A0 branch SOURCE.
- Free play (FF82A4): any start edge starts and sets credits to 0. Also from
  the attract loop directly (0x8186), without a credit screen. SOURCE.
- Then (0x9282): FF82A5 set, all task state cleared, FF8293 = FF799F = 0,
  game task 0x92D2 started: fade out (0x1ECC), game init at **t1423** (23
  frames after Start): sounds 0xF0, 0xF7, new game (2CCE), world map with
  music 0x12 at 1424 ... (intro.md 3). VERIFIED.
- Start without credits: ignored everywhere (attract, continue). VERIFIED (C3 t4250).
- New game (2D18) also counts games in FF79A2 (0..3, wraps) and copies it to
  FF83A4; purpose UNKNOWN (look for readers of FF83A4).

---

## 6. Game start, life loss and two players

- One player: Start -> world map -> forest story -> STAGE1 (intro.md). After a
  life loss (FF8004 = 6, 0x96BE): lives - 1 (BCD), rank -13 (0x38134), then
  FF8000 = 2/FF8004 = 0: **world map (128-frame hold) and stage restart at the
  checkpoint, without the story** (C1: life lost t3397, map 3399-3582, play
  3639 with the checkpoint time). VERIFIED.
- Lives reach 0 (0x96FA): FF8000 = 4 -> **score qualification first**, then
  name entry if qualified, **then** the continue screen (sections 10, 7). The
  ranking is decided before continuing; a continue resets the score to 0
  (2CEE, hud.md 5), so later qualification uses the new score. VERIFIED (N1/N2
  order: qualify t4070, name entry 4071, ranking 4645, continue 4875).
- Time over (FF83C6 = 0:00 in play, 0x956E): sound **0x14**, label 2
  `TIME OVER` (big letters, (15,12), palette 15), 80 frames, then the normal
  life loss (sounds 0xF4, 0x10FB). Answers hud.md open question 1. VERIFIED
  (run T: t2864-2944, lives 2 -> 1 at 3001, map, restart with 3:00). The debug
  no-damage switch FF799E disables the time over (stage1_shops_companion.md).
- **Alternating two players** (FF82A5 = 1): both players start with the DIP
  lives (FF8342 and the saved block FF82C2). On every life loss, if the other
  player has lives, the 128-byte player blocks FF8342.. and FF82C2.. are
  exchanged (0x9714), FF8292 toggles, the 1UP/2UP score positions swap; in
  cabinet setting "cocktail" the screen flips (FF8294 bit 7). Player 2's first
  turn plays the whole forest story intro again (C4: t3399-4537). When one
  player's lives reach 0 that player gets qualification/name entry and the
  continue screen; after it (continued or not) play passes to the other player
  if they still have lives (0x98C2). VERIFIED (C4: P1 out at 5924, P1 continue
  screen 5930-6450, game over, P2 resumes at 6738; P2 out 7408 -> game over 8222 -> attract).
- Which controls P2 uses (0x1F94): P1's port in cabinet "upright 1 player"
  (DSW-A bits 6-7 off), else the P2 port. VERIFIED (code) / C4 played P2 on the P1 port.

---

## 7. Continue screen (game task FF8000 = 8, 0x9748)

### 7.1 Sequence (C1, t from the life loss that empties the lives)

| t | Mode | Event | Tag |
|---|---|---|---|
| 4069 | | sounds 0xF0, 0xF7; lives 0 | VERIFIED |
| 4070-4071 | qualify | no qualification (score 0 < 10,000) | VERIFIED |
| 4071 | 8/0 (0x9762) | pools and cameras cleared. If the ending flag FF8388 or the "no continue" DIP (FF8287) is set -> straight to GAME OVER (7.3). Else FF799F = 1, label 1 **`CONTINUE`** (big letters, cells (17,8), palette 15), credit display, figure spawned (0x32710: class-12 kind 3 variants 1 and 2 at world (168,80) and (216,72): an old bearded man with a staff lying on the ground, animated, `vidC1/v004100`, `v004500`), timer 3 | VERIFIED |
| 4075 | 8/4 | countdown **9** drawn (big digit, cells (23,11), palette 11, glyphs 0x7B00..: table 0x9A7E), music **0x15**, fade in (task waits) | VERIFIED |
| 4179, then every 52 | 8/4 | sound **0x42**, countdown - 1 (BCD) and redraw. Digit 9 is shown **104** frames (fade + 52), 8..1 52 frames each | VERIFIED |
| 4595 | 8/6 | countdown 0 shown; timer **64** | VERIFIED |
| 4659 | 8/8 step 0 | fade out (0x0C14), wait | VERIFIED |
| 4707 | 8/8 step 2 | label 0 **`GAME OVER`** (big, cells (15,12), palette 15, two-cell gap), music **0x14**, fade in (0x1014); 120 frames (`v004720`) | VERIFIED |
| 4883 | -> 0x992C | FF799F = 0, fade out; sounds 0xF0, 0xF7; statistics; attract task restarted | VERIFIED |
| 4892 | | credits remain -> credit screen at once (0x08FB, 0xF4; `v004910`); with no credits -> attract opening (C3: 6388, opening 6404) | VERIFIED |

Whole continue window: 104 + 8 x 52 = **520 frames** from the 9 to the 0, plus
52 frames of grace (below).

### 7.2 Credits and buttons during the countdown (0x9998, 0x99D8)
- Each update in modes 4 and 6 (while the 64-frame timer is >= 12): if
  credits > 0 (or free play) and **1P or 2P Start is held** (level, FF8058 &
  0x30, not an edge) -> continue (7.4). VERIFIED (C2 t4300, C3 t4400; the
  held-level test is SOURCE).
- Start with no credit: nothing. Insert a coin during the countdown: sound
  0x3B, `CREDIT` count redrawn, the countdown keeps running (not reset); Start
  then continues. VERIFIED (C3: start t4250 ignored, coin 4300 -> credit 1 at
  4307, countdown 6 -> 5 continued, Start 4400 accepted).
- While the countdown is **below 8**, a new press of button 1 or 2 (attack or
  jump) sets countdown = 1 and timer = 1: the next update ticks to 0 (sound
  0x42) and the 64-frame grace starts: a fast way to give up. Ignored at 9 and 8.
  VERIFIED (C3: button 1 at t6100 with 7 showing -> 0 at 6100, GAME OVER 6164).
- After 0 the remaining 64-frame timer accepts Start for its first 52 frames
  (timer >= 12) and then goes to game over. SOURCE (code 0x9834; not traced).

### 7.3 Game over without a continue offer
Continue DIP off (FF8287 = 1) or the ending (FF8388): mode 8 step 4 (0x9900):
120 frames of `GAME OVER` with music 0x14 (no music after the ending), then as
7.1 from 4883. SOURCE.

### 7.4 Continue accepted (0x9998 -> mode 12, 0x9A18)
- Credits - 1 (BCD; not in free play), **FF839B + 1** (number of continues,
  saturates at 255; it is the term subtracted from the rank at each stage start,
  0x381A0, stage1_enemies.md 2.3), credit display, FF8004 = 12, **120 frames**
  with the figure still animating; no sound. VERIFIED (C2: t4300 -> 4419, credits
  2 -> 1, FF839B 0 -> 1).
- Then play time added to the statistics, **2CEE** (hud.md 5: lives = DIP,
  score 0, HP = max, rank re-based, extend threshold reloaded; magic, gold,
  items kept), FF799F = 0, fade out (0x98BA). VERIFIED (C2: lives 2, HP 3 at 4419).
- Resume: two players with the other player alive -> other player; else
  FF8000 = 2/FF8004 = 0: world map (music 0x12) and restart of the current
  stage at its checkpoint with the checkpoint time (C2: map 4476, play 4793,
  3:00). VERIFIED.
- Music: the continue music 0x15 is replaced by the map music 0x12. VERIFIED.

---

## 8. Game over and the return to attract (0x992C)
- FF8000/8004/8008/800C/8010 = 0, FF8294 = FF8292 = 0, coin lockout re-armed
  (1E72), sounds 0xF0, 0xF7. VERIFIED.
- Operator statistics (only shown by the demo statistics): play-time totals
  FF0E64 (long), count FF0E6C, plays FF0E68; average FF0E6A = total / count
  (`divu`). SOURCE.
- Attract task 0x80D0 started at state 0: with credits -> credit screen next
  frame (5.1); without -> attract opening (no power-on part, no ranking first).
  VERIFIED (C1, C3).

---

## 9. High-score table

| Item | Value | Tag |
|---|---|---|
| ROM default | **0x183C**: 10 x 8 bytes = 4-byte BCD score + 3 letters + space: 55000 CAP, 50000 COM, 45000 CAP, 40000 COM, 35000 CAP, 30000 COM, 25000 CAP, 20000 COM, 15000 CAP, 10000 COM | VERIFIED |
| RAM copy | **FFEFC6** (80 bytes), copied at reset only (0x147A); pointer list FFF016 (10 longs to the entries); HUD high score **FFF03E** = first entry's score (0x1482) | VERIFIED (RAM dumps) |
| Persistence | none on the arcade board (no battery): lost at power-off. The port keeps Arcade and Home tables separately in SRAM (AGENTS.md; B1 `port.c` loads the table at boot and saves after a game) | SOURCE |
| Qualification (0xB954) | at the moment a player's lives reach 0: unsigned 32-bit compare of each entry (from the top) with the score FF834A; the new score goes in at the **first entry whose score is <= the player's score** (a tie ranks the new score **above** the old one); entries below move down one, the 10th is dropped; the new entry's name stays the old bytes until name entry writes 4 spaces. Score below the 10th: no entry, name entry skipped (FF8000 += 4) | VERIFIED (N1/N2: 60000 placed TOP, 55000 CAP moved to 2ND, 10000 COM dropped) |
| Rank index | FF7996 (fp+0x1A of the record FF797C) = 9 - position (9 = TOP); the name pointer FF7988 = entry + 4 | VERIFIED (RAM N2) |
| HUD HI | not changed by qualification or name entry; it follows score adds during play (hud.md 3.2) | VERIFIED (N2: FFF03E stayed 55000 after a poked 60000) |

---

## 10. Name entry (game task FF8000 = 6, 0xB9B8; record FF797C)

### 10.1 Screen (set-up 0xB9E0, string 15, 0xBE60)
```
row  6:                     PLAYER 1                 (20,6) "PLAYER" pal 3, digit (27,6) pal 3
rows 8-9:             6 0 0 0 0 PTS                  big digits from (14,8) pal 9, "PTS" (30,9) pal 3
row 12:        A B C D E F G H I J K L M N O P Q R   cells (7+2c, 12), pal 0
row 14:        S T U V W X Y Z & ' ( ) * + . - RB ED cells (7+2c, 14), pal 0
row 17:                 PUT YOUR NAME 20             (16,17) pal 3, timer (30,17)-(31,17) pal 2
row 20:                     PLAYER  1                (20,20) pal 3, digit (28,20) pal 3
row 23:                      WIL                     name cells (21..23,23), letters pal 2
```
- Letter set (0xBAC8, 36 bytes, glyph 0x7000 + byte):
  `ABCDEFGHIJKLMNOPQRSTUVWXYZ&'()*+.-{|`; `{` (0x707B) shows as a small "RB"
  (rub-out) glyph, `|` (0x707C) as "ED" (end). VERIFIED (ROM + `vidN1/v004120`).
- Player digit '1'/'2' from FF8292. Score: big-digit glyph table **0xBEE4**
  (0 -> 0x7B22, 1..8 -> 0x7B00..0x7B0E, 9 -> 0x7B20; each 2x2 cells), leading
  zeros skipped (the first non-zero digit goes to column 14). VERIFIED.
- Screen cleared to black; no scenery. VERIFIED.

### 10.2 Start (step 2, 0xBA54)
Cursor on **ED** (column 17, row 1), name cell (21,23) shows ED, name bytes =
4 spaces, timer **0x20** (BCD 20), sub-timer 48, blink timer 5, letters left 3,
music **0x18** (both branches of the rank test post 0x18), fade in. VERIFIED
(N1: 0x18 at t4073).

### 10.3 Input (each update, 0xBAF6 -> BD60 -> BDF4)
- Buttons: a new press (edge) of **button 1 or 2** selects the letter under
  the cursor:
  - a letter: written at the next name cell (21+n, 23) palette 2 and stored as
    ASCII; letters left - 1; at 0 (3 letters) entry ends.
  - **RB**: if at least one letter was entered, the last one is erased (screen
    cell blanked, byte back to space) and letters left + 1; else nothing.
  - **ED**: entry ends at once (fewer than 3 letters allowed; the rest stay spaces).
  VERIFIED (N2: W, X, RB, I, L -> "WIL " stored, entry ended on the 3rd letter at t4440).
- Directions: a new press moves at once; a direction held without a new press
  repeats every **10 frames**. Right (bit 0) before left: column + 1 (17 wraps
  to 0) / - 1 (0 wraps to 17); up/down each toggle the row (two rows). A
  horizontal and a vertical move can happen in the same update. The old cell's
  palette is reset to 0. VERIFIED (N2 cursor path) / repeat SOURCE.
- Highlight (BD60): every 5 frames the selected letter's palette toggles 0/3,
  and the selected letter (code and palette) is copied into the next name cell
  as a preview. VERIFIED (`vidN2/v004214`: X flashing in the grid and at (22,23)).
- Idle: with no button press and no direction held an idle counter counts up;
  at **420 frames** (7 s) the entry ends. Any press or held direction resets it.
  VERIFIED (N1: start 4073 + fade, end at 4546).
- Timer (BDF4): every **48 frames** the BCD timer - 1, shown at (30,17); when it
  is 0 at a tick the entry ends (name cell palette 9). From 20: 20 x 48 + 48 =
  **1,008 frames**. It only matters while the player keeps moving (idle ends
  first otherwise). VERIFIED (N1: 20 -> 12 over the idle wait) / expiry SOURCE.

### 10.4 End and ranking display (0xBCC2..0xBD4E; the "0xBD04" music)
1. 42 frames, fade out (1E88).
2. Text job **0x0402** (ranking, FF82A6 busy, drawn in one frame), music
   **0x16** (0xBD04: same word for every rank), fade in. VERIFIED (N1 t4645).
3. Up to **120 frames**, ended early by any P1 input held (FF806A non-zero);
   meanwhile the new row (25 cells from its rank label, row table 0xBDCC)
   cycles palettes 0..3 every 5 frames. VERIFIED (`vidN2/v004560`: blue TOP row).
4. Fade out, FF8000 = 8: continue screen (section 7). VERIFIED (N1: 4818 -> 4875).
- Saved: the 4 name bytes in the table entry (3 letters + space, unused
  positions spaces). Nothing else. VERIFIED (RAM N2).

## 11. Ranking display (string 13 + 0x565A)
`RANKING` (21,6) palette 3; rows 9, 11, ..., 27: `TOP`, `2ND`, `3RD`, `4TH`..
`9TH` at column 15, `10TH` at column 14 (palette 3); score 8 BCD digits at
columns 18-25 palette 5, **all** leading zeros blank; ` PTS` (26) palette 3;
name 4 characters at columns 31-34 palette 2. Same screen in the attract loop
(3.1, 180 frames, no music) and after name entry (10.4, music 0x16). VERIFIED
(`vidA/v006000`, `vidN1/v004760`).

---

## 12. DIP switches (raw port bytes: bit = 1 when the switch is off)

Decoded at reset (0x143E..0x1446), at boot state 2 (0x8010..0x8018) and from
the test menu (0x45D0A, 0x45D68); DSW-C bits 3/4/7 also each VBlank (0x1A8E).
Inverted copies: FF8066 (A), FF8067 (B), FF8068 (C). `port.c make_dsw` already
maps menu values to these bytes. VERIFIED (code) unless noted.

| Switch | Raw bits | RAM / read at | Effect |
|---|---|---|---|
| Coin A | DSW-A 0-2 | FF8272+4/5 via table 0x1B2E (1AC2) | raw 7 1C1C (default), 6 1C2C, 5 1C3C, 4 1C4C, 3 2C1C, 2 3C1C, 1 4C1C, 0 2C1C (with the next row) |
| Coin B | DSW-A 3-5 | FF827A+4/5 | same values |
| 2 coins start / 1 continue | Coin A and Coin B both raw 0 | FF79A0 (1AF6) | every coin = 1 credit, 1P start costs 2, 2P 4, continue 1, `  COIN` label, legends 17-20 (5.1). SOURCE |
| Cabinet | DSW-A 6-7 | FF828E (1B02) | raw 3 (default): upright, P2 plays on P1 controls; raw 2 (bit 6 on): P2 own controls; raw 0/1: P2 own controls and screen flipped on P2's turns (0x9722). MAME labels: UNKNOWN (likely upright 1P / upright 2P / cocktail) |
| Difficulty | DSW-B 0-2 | FF8291 (1B46, table 0x1B3E) | new-game rank high byte (0x381B0) and the stage-start floor (0x38146): raw 4 0x00, 5 0x10, 6 0x20, **7 0x30 (default)**, 3 0x40, 2 0x50, 1 0x60, 0 0x70. port.c menu levels 0-7 = raw 4,5,6,7,3,2,1,0 (easiest to hardest). Nothing else reads FF8291 (only 0x3814E-0x381D4 and the test menu) |
| Nando speed | DSW-B 3-4 | FFF9A4 (1B54) | "Nando" = 難度, difficulty: the per-update rank growth (0x380E8): raw 0x18 **normal** (+1 per update, default), 0x10 slow (+1 every other update), 0x08 fast (+2), 0x00 very fast (+3); saturates at 0xFFFF (stage1_enemies.md 2.3, combat.md) |
| (debug stage) | DSW-B 5-7 | read only with debug on (2D66) | start stage index = inverted bits 5-7 at a new game |
| Magic continue | DSW-B 7 | FFF9A6 (1B62) | raw 0 = on: when a stage is entered after a clear (0xA956 -> 2E1C) magic/sword levels are raised to stage minimums (magic.md 6) |
| Lives | DSW-C 0-1 | FF8288 (1B96, table 0x1BEE) | raw 3 **2 lives (default)**, 2 1, 1 3, 0 4; loaded at new game and continue (2CEE) |
| Vitality | DSW-C 2-3 | FF79A3 (1BAE, table 0x1BF2) | raw 0xC **3 (default)**, 8 4, 4 5, 0 2: max HP at new game (2D60) |
| Flip | DSW-C 4 | FF8295 = 0x80 when raw 0 (1BBE, 1A98) | video flip (18D0); not needed |
| Demo sound | DSW-C 5 | FF8290 (1BCC) | raw 0 = off: while FF8293 (attract) is set, sound posts through 0x786A are dropped (0x7818); control words F0/F4/F7/FF (0x7844) and the coin sound still play. Default raw 1 = sound on (run A heard 0x81 and the demo music) |
| Continue | DSW-C 6 | FF8287 (1BDA) | raw 0 = no continue: straight to GAME OVER (0x9770, 7.3). Default raw 1 = continue allowed |
| Test / debug | DSW-C 7 | FF828F (1BE8, 1AAA each VBlank) | raw 0 at boot: test menu instead of attract (0x80A6). Turned on after boot: debug: stage select (above), DSW-C bit 3 -> FF799E no damage and no time over, DSW-C bit 1 freezes the game task (0x92E2) and attract (0x80E4), DSW-C bit 2 slow motion (sleep ((DSW-B & 0x1E) >> 1) + 2 frames per update, 0x9304), free-play test 1B12 (FF82A4 = 1 when Coin A raw 0 and Coin B raw 7), max magic/sword on a held input (0xBF04). Not for Arcade mode |

- **Free play** (FF82A4, legend `FREE PLAY`, start without coins) is only ever
  set by 1B12, which runs only with the debug switch on: a normally configured
  board has no free play. SOURCE (only 1B20/1B28 write FF82A4).
- Factory/MAME default positions: UNKNOWN here (MAME cps1.cpp `willow` ports;
  all-off = the values marked default above, which is what the traces used).

---

## 13. Sounds of the front end (sound ring words; `S/*.log` SND lines)

| Word | When | Tag |
|---|---|---|
| 0x81 | attract: logo turns to full colour | VERIFIED |
| 0x0001 + 0x01FA | demo 0 music (music word 1) | VERIFIED |
| 0x0003 + 0x01FA | demo 1 music (wagon chase) | VERIFIED |
| 0x2F | each typed title letter (demo end, intro) | VERIFIED |
| 0x08FB, 0xF4 | music fade + reset: entering the credit screen, end of demo titles | VERIFIED |
| 0x3B | each counted coin (any time) | VERIFIED |
| 0xF0, 0xF7 | stop all: game init, life loss, lives out, game over exit | VERIFIED |
| 0x12 | world map (new game, restarts, after continue) | VERIFIED |
| 0x18 | name entry music | VERIFIED |
| 0x16 | ranking after name entry | VERIFIED |
| 0x15 | continue screen music | VERIFIED |
| 0x42 | each countdown step | VERIFIED |
| 0x14 | GAME OVER and TIME OVER | VERIFIED |
| 0x43 | clock below 0:10, every 32 frames (hud.md 3.5) | VERIFIED (run T) |

The attract scenes, credit screen and attract ranking have no music.

---

## 14. Dump list (`.cps` = lockstep VIDDUMP: 0x80 bytes CPS-A/B registers + video RAM 0x900000-0x92FFFF with live palettes)

Render: `S/render.sh DIR KEY...` (cpsframe.py, `CPSFULL=1`, 384x224) or
`S/mont.sh DIR OUT KEY...` (2-column PNG). Text cells: `S/cells.py DIR from to
"c,r ..."`. Key = t+2, except run A: t+152 for t 3809-4709 (demo 0), t+381 for
4710-9490, t+566 for 9491-10391 (demo 1), t+760 for 10392-15216, t+776 for
15217-16117, t+883 after (`S/cal.log`).

| File | Shows |
|---|---|
| `vidA/v000015` | power-on test text |
| `vidA/v000090`, `v000270` | opening: cave with Willow and the baby; Bavmorda appearing |
| `vidA/v000405`, `v000705` | Bavmorda texts typing |
| `vidA/v001005`, `v001305`, `v001410` | Willow panel: text, `A PEACE-LOVING MAN.`, portrait + WILLOW |
| `vidA/v001905`, `v002160` | Madmartigan panel |
| `vidA/v002505`, `v002910` | `A SUPER GAME FROM CAPCOM ...` texts |
| `vidA/v002985`, `v003045`, `v003090`, `v003165` | logo: white flash, outline, colour, Lucasfilm logo |
| `vidA/v003345` | logo + INSERT COIN + Lucasfilm notice |
| `vidA/v003570` | logo + Capcom notice + CAPCOM logo |
| `vidA/v003750` | FBI screen |
| `vidA/v003975`, `v004245`, `v004500`, `v004845` | demo 0 (stage-1 boss arena, INSERT COIN) |
| `vidA/v005205`, `v005340` | demo titles STAGE1 / CROSSROAD |
| `vidA/v005550`, `v005715`, `v005745`, `v005790` | interlude: Eborsisk, text, fire |
| `vidA/v006000` | attract ranking (default table) |
| `vidA/v010110`, `v010500`, `v010950` | demo 1 (wagon chase) |
| `vidB2/v003170`..`v003429` | every frame of the INSERT COIN blink |
| `vidB1/v001028`..`v001157` | every frame of the credit screen (PRESS START BUTTON blink) |
| `vidC1/v001030`, `v001110`, `v001220` | credit screen with 1 / 2 / 3 credits |
| `vidC1/v004100`, `v004500`, `v004620` | continue screen 9 / 2 / 0 with the figure, CREDIT 2 |
| `vidC1/v004720`, `v004800` | GAME OVER |
| `vidC1/v004910` | credit screen after a game over with credits left |
| `vidC3/v004248`..`v004320` | continue with 0 then 1 credit (coin during the countdown) |
| `vidN1/v004080`, `v004120`, `v004555` | name entry fade-in, idle, timer at 12 |
| `vidN1/v004700`, `v004760` | ranking with the new (blank-name) TOP entry |
| `vidN2/v004214`, `v004268`, `v004380`, `v004444` | name entry: X highlighted / "WX" + preview / after rub-out / "WIL" |
| `vidN2/v004560`, `v004700` | ranking, TOP row "WIL" cycling colours |
| `vidT/v002900` | TIME OVER |
| `vidC4/v*` | two-player game (every 20 from 1380) |

---

## 15. Native implementation notes (Arcade mode)
- State machine to rebuild natively: boot -> attract (5 scenes + demo +
  interlude + ranking, repeating) -> credit screen -> game -> [lives out:
  qualify -> name entry -> ranking] -> continue -> game over -> attract or
  credit screen. Timers in 3.1, 7.1 and 10 are exact update counts.
- Credits: BCD 0..9, coin debounce/jam and per-slot ratios as section 4;
  Genesis C = coin 1 (AGENTS.md), Start = 1P start; a second pad's Start can be
  the 2P start for alternating play. Coin B and service have no Genesis button
  (menu ratios still apply to coin A).
- Demo: replay the two input tables (0x4800E / 0x4820E) into the native game
  from the stated set-up (stage, checkpoint, magic 1, HP 5, seed and frame
  counter values). The native engine's own random/frame state must be seeded
  equivalently for the replay to follow the arcade; if the native simulation
  is not bit-exact, the demo will drift (verify against `vidA` frames).
- Presentation cuts (AGENTS.md) apply to these screens as to the intro: text
  on its own rows (cut 3), fixed palettes (cut 4). The 48-column layouts
  (credit text at columns 37-45, ranking columns 14-34, name grid 7-41) need
  a Genesis layout within 40 columns.
- Rankings: Arcade table saved in SRAM separately from Home (AGENTS.md); the
  arcade itself resets it at power-on.

## 16. Open questions (UNKNOWN), where to look
1. MAME factory DIP defaults and labels for Willow (cps1.cpp `willow` input
   ports), in particular the cabinet values and difficulty level names.
2. Continue grace: Start accepted during the first 52 frames after the 0
   (0x9834, timer >= 12) is from code; trace a Start ~30 frames after the 0.
3. Name-entry timer expiry and held-direction repeat (10 frames) are from code;
   trace a run that keeps moving for > 1,008 frames.
4. 2-coins-to-start mode (both coin switches raw 0) and its legends: trace
   with `DSWA=C0` and coins.
5. FF83A4 (game count mod 4): who reads it.
6. Identity of the continue-screen figure (class-12 kind 3 variants 1/2,
   0x32710; looks like the High Aldwin lying down) and of FF8365 = 5 in the demo set-up.
7. Real-hardware length of the power-on test and of the demo (CPU slowdown);
   the harness patches the video-RAM test and runs overclocked.
8. Operator statistics display (P2 button 2 at demo start, 0x572C): exact
   fields, if Arcade mode wants it.
