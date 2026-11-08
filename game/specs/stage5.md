# Stage 5 (Tir Asleen Castle): behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin`. Behaviour
spec for the native Genesis rewrite (NATIVE_ENGINE_PLAN.md sections 2 and 6),
organised like `stage4.md`, which it continues. Arcade addresses are citations
only. "Stage 5" = arcade stage index 4 (FF8344 = 4). The player is **chosen by
the player** on a select screen before the stage: **Willow** (form 0, the
default) or **Madmartigan** (form 4); `player.md` / `magic.md` or
`madmartigan.md` apply, `combat.md` for both. Section 12 lists what the player
code needs beyond those specs.

Evidence tags (as the stage-1..4 specs):
- **VERIFIED**: two independent sources agree (disassembly read for this file plus
  a headless arcade trace, or plus the ROM data and a render).
- **SOURCE**: one source only (usually disassembly read here, not trace-checked).
- **UNKNOWN**: not established; says where to look.

Units: world X right, world **Y up**; speeds are signed **8.8 px/frame** (256 =
1 px/frame); one update = one 60 Hz frame. Hit box = (xOffset, width, yOffset,
height), box = [x-xOffset, x-xOffset+width] x [y-yOffset, y-yOffset+height].
16-direction values: 0 up, 4 right, 8 down, 12 left.

**Corrections for earlier specs and notes found while writing this one.**
1. The form table 0x2F72 (Willow for indices 0/2/4) is **not** the stage-5 form.
   0x2F08 uses it only for indices 0-3 and, for index 4, only while **FF83A3** is
   clear (before the select screen). Once FF83A3 is set (0xA1B8), the form is
   **FF8386** as chosen on the select screen (1.3), copied to the player record
   (+0x20 = FF9866), and **y += 8** when the form is not 0 (Madmartigan).
   VERIFIED (disassembly 0x2F08..0x2F64; run A form 0 by timeout, run M form 4
   chosen with Right, start y 72 / 80).
2. Scroll1 (text) palettes: the loader 0x0978 adds **((index >> 1) * 0x400)** to
   0xE6EA0 (0x09C0..0x09D6). stage3.md 2.6 and stage4.md 2.6 give 0xE6EA0 for
   indices 2 and 3; both use **0xE72A0**; index 4 uses **0xE76A0**. SOURCE
   (disassembly; not compared with the dumps' text palettes).
3. stage4.md 1.1 / 7.1 ("stage 5: music word 10, title 23 letters") is right, but
   the music word is posted at the play set-up after the select screen, not at the
   map scene; the select screen itself types its prompt with 21 x sound 0x2F
   (1.1). VERIFIED (runs AS/CS/S).
4. The class-4 event routine table is **0x381FA** (word offsets indexed by event
   kind; kinds 25/26/27 go through `bra` stubs at 0x3823E..0x38246, kind 31 is
   inline at 0x3824A). Earlier specs cite the routines but not the table.
   VERIFIED (ROM; stage-4 routines 0x393B6 / 0x39328 resolve through it).
5. The boss's breath and death flags are **FF0EFC / FF0EFE** (`a5@(-28932)`,
   `a5@(-28930)` with a5 = FF8000), not FF8EFC / FF8EFE as an earlier draft of
   this spec said; the flame list is at FF0EC4. VERIFIED (RAM run W: FF0EFC = 1
   during each breath).
6. Earlier drafts of this spec called class 4 kind 27 a "doorway" and class 6
   kind 8 a "door leaf"; they are a **cart pusher** (a soldier) and the **spiked
   cart** he pushes (5.3). The stage-5 "gate" (class 12 kind 35) is the tower
   doorway's **interior wipe** (5.6). VERIFIED (run K renders).

**Content summary.** Stage 5 is the approach to and inside of **Tir Asleen
castle** at sunset. After a **player select screen** ("LET'S SELECT PLAYERS.":
Willow or Madmartigan, 10-second countdown) it runs in four parts:
(A) **outer walls** (x 0..~2300, y 0..~520): a path along the moss-green curtain
wall, then up the ruined stone tower and wall slopes; **trolls** (brown hairy
brutes, class 4 kind 26) walk, leap and climb out of tower windows and throw
green orbs; soldiers, an archer, a chest, a shop door; (B) **the gap**
(x ~2300..4100): the wall breaks off over open sky (a pit), crossed on
**falling stone pillars** (class 6 kind 7) while the camera only moves right;
an upper **corridor route** (y 512..767) entered through a tower doorway at
(2576, 640) (an interior "wipe", 5.6), where soldiers push **spiked carts** at
the player and spearmen come from behind; (C) **castle interior** (x ~4100..5100): dark
halls with balconies, spike rails, wall torches, a flock of **bats**, archers,
spearmen, three chests and a second shop door; (D) the **red-sky battlements**
(x ~5100..6150): four trolls, then the boss **Eborsisk**, a two-headed
serpent-dragon (class 18 kind 6) rising from below the bridge: one neck on the
far layer (scroll3), one neck as sprites, both heads as sprites, each head with
127 energy and its own bar row. No mid-boss. Clear,
results (factor 500), one story page with **General Kael**. VERIFIED (traces
below; disassembly; renders).

## 0. Traces used (headless lockstep, reference program; no GUI)

Harness `build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/lockstep.keys ref`
as stage3.md 0 / stage4.md 0 (`lockstep.keys ref CORE ROM out/mask.bin REC
FRAMES`, CORE = `.../perf/gpgx-cps/genesis_plus_gx_libretro.dylib`,
`OVERCLOCK=5000`), started by `S/run.sh NAME FRAMES` with the settings in the
environment. Scratch dir
`/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage5/`
(below `S/`). Keys = program VBlank count; script time "t" runs ~3 frames behind.

| Run | Settings | Covers | Output |
|---|---|---|---|
| A (natural hand-over) | `SCRIPT=stand STAGE=3 INVINCIBLE=1 ENDAT=1400`, 7000 frames, `RAMEVERY=4`, `VIDEVERY=6 VIDFROM=3400`, `CAMLOG=1` | stage-4 story end, **stage-5 map, select screen (timeout -> Willow), title, play start** | `S/a.log`, `S/ramA/`, `S/vidA/` |
| AS (sound) | as A plus `DBGPC=7824 DBGN=1000000` | sound words of run A | `S/as.log`, `S/snd_as.txt` |
| B (whole stage, Willow) | `SCRIPT=random STAGE=4 INVINCIBLE=1 GATEASSIST=900 NUDGE=600 KILLBOSS=1`, 26000 frames, `RAMEVERY=2`, `VIDEVERY=10`, `CAMLOG=1 CAMEVERY=10` | debug start (select at 836, Willow), play 1164, arena 15908, boss 17018, clear (FF83C3 = 2) 18468, stage word 5 at 18476, into stage 6 | `S/b.log`, `S/ram/`, `S/vid/`, `S/births.txt` (actor lifetimes 996..18480), `S/flags_b.txt` |
| S (sound) | as B plus `DBGPC=7824` (same trajectory) | sound words of run B | `S/s.log`, `S/snd_s.txt` (key ~ column + 510) |
| M (Madmartigan) | `SCRIPT=keys STAGE=4 INVINCIBLE=1 GATEASSIST=900 NUDGE=600 KILLBOSS=1`, `KEYS` = `S/keysM.txt` (Right taps on the select screen, A to confirm, then walk right with attack/jump), 22000 frames, `RAMEVERY=4`, `VIDEVERY=20` | **select Madmartigan** (form 4 at key 1064), whole stage as Madmartigan, boss 7836, clear, stage 6 | `S/m.log`, `S/ramM/`, `S/vidM/`, `S/births_m.txt` |
| C (stage-5 clear) | `SCRIPT=stand STAGE=4 INVINCIBLE=1 ENDAT=1700`, 6000 frames, `RAMEVERY=4`, `VIDEVERY=6 VIDFROM=1700` | results, story page, stage-6 map and start | `S/c.log`, `S/ramC/`, `S/vidC/` |
| CS (sound) | as C plus `DBGPC=7824` | sound words of run C | `S/cs.log`, `S/snd_cs.txt` |
| W (boss, Willow, natural) | as B (same trajectory, verified identical camera log to key 6500) but **no KILLBOSS**, `DRVTO=17000` (assist drivers off from t 17000), `SPLICE=16330 SCRIPT2=bot` (from t 17030, key ~17033, a closed-loop bot: stand 96 px right of the live head, face left, tap A, jump when the head is > 40 px above), 24000 frames, `RAMEVERY=1 RAMFROM=17000`, `VIDEVERY=10 VIDFROM=17000` | wake 17018, **natural hits on both heads** with Willow's level-0 shots (head A 127 -> 104, head B 127 -> 118 by 24000), breaths, all motion per update | `S/boss2/w.log`, `S/boss2/ramW/` (every update), `S/boss2/vidW/`, `S/boss2/w.pkl` (series) |
| MM (boss, Madmartigan, natural) | as M (keys `S/keysM.txt`) without KILLBOSS, `DRVTO=7840 SPLICE=7140 SCRIPT2=bot BOTDX=40 BOTJY=30`, 16000 frames, `RAMEVERY=2 RAMFROM=7800`, `VIDEVERY=20 VIDFROM=7800` | wake 7836, sword hits on both heads, **head A killed naturally at 14878** (death 14878..15060), head B 127 -> 34 by 16000 | `S/boss2/mm.log`, `S/boss2/ramMM/`, `S/boss2/vidMM/` |
| K (upper corridor) | `SCRIPT=random STAGE=4 INVINCIBLE=1 INVFROM=1700 GATEASSIST=900 NUDGE=600`, `POKE="1250-1252:83c6=0000;1253-1700:8370=0a00,8372=0280"` (clock 0 -> TIME OVER; last safe position (2560, 640) -> **restart at checkpoint 4**), `SPLICE=1100 SCRIPT2=keys` (walk right, A every other 8 frames, B 16 of 96), 7600 frames, `RAMEVERY=2 RAMFROM=1200`, `VIDEVERY=6 VIDFROM=1600` | restart at (2560, 640) key ~1508, **gate wipe 1516..1670**, corridor carts and spearmen 2126..7000, corridor exit into the interior (x 4072 at 7000) | `S/boss2/k.log`, `S/boss2/ramK/`, `S/boss2/vidK/` |

Helper scripts in `S/` (copied from `../stage4/` and `../stage3/`): `modes.py`,
`flags.py`, `births.py`, `slot.py` / `bslot.py`, `roots2.py` (pose tables:
`S/roots.txt`), `montage.py` / `render.sh` (`.cps` -> PNG in `S/png/`; montages
`mA1`, `mA2` (map, select, title), `m1`..`m5` (run B), `mc` (results, story)),
`regs.py`, `objpal.py`, `palscan.py` (live palette changes between dumps),
`sndseq.py`, `unwrap.py`, `rom.py`; disassembly listings `S/*.dis` (`sel.dis`
select screen, `k26.dis` troll, `k27.dis` cart pusher, `c6k7.dis` pillar,
`c12k35.dis` gate, `c10k15.dis` orb, `c10k20.dis` boss fire, `boss.dis`,
`ev14.dis`, `ev26.dis`), `S/tables.txt` (object and event tables), foreground
map `S/map/s5.ppm` / `s5_seg.png` (rows 5-7 of grid 4).

Runs W, MM and K use an agent copy of the harness, `S/boss2/ls` (source
`S/boss2/ls.c` = `lockstep.c` plus: `SPLICE`/`SCRIPT2`/`KEYS2`/bot (`BOTTGT`,
`BOTDX`, `BOTJY`), `RAMFROM`, `DRVTO`, `POKE`, `INVFROM`), started by
`S/boss2/run.sh NAME FRAMES` with `NOREC=1`. New helpers: `S/bossrec.py`
(class-18 records per dump), `S/hits.py` (head energy / +0 changes),
`S/series.py` (per-update pickle), `S/roots3.py` (pose chains from explicit
roots; output `S/boss2/roots_boss.txt`), renders in `S/boss2/png/` (`gate.png`,
`door.png`, `vidW-v017860.png`, `vidW-v018500.png`).

Harness notes: ENDAT must fall in play (FF8004 = 2); at t 1400 a debug start is
still on the select screen, so run C uses 1700. In runs B and M the player is
invincible: falling into the gap bounces him back up at y 0 instead of a pit
death. Run B never took the upper corridor (kind 27 cart pushers and the kind 35
gate never came on screen); run M neither (run K does, by a checkpoint restart).
**Neither run B nor M damaged a boss head** naturally (runs W and MM do): both heads were zeroed by `KILLBOSS` (run B t17324 head var 3, t18224
head var 2; run M t8404, t9304); everything after those keys is the normal death
path started artificially. The arena trolls died naturally in both runs. A NUDGE
moved Madmartigan from x 192 to 240 at t1325 (not a game rule).

---

## 1. Stage entry after stage 4

### 1.1 Hand-over timeline (run A, keys)

| Key | Mode (FF8004 / FF8008) | Event | Tag |
|---|---|---|---|
| ~3480..3534 | stage-4 clear step 18 end | fade, stage index -> **4** (stage word 4 at v3534), rank update (1.8) | VERIFIED (stage4.md 7.1) |
| 3536 | mode 0 step 2..4 | **world-map scene** (1.2): sounds **0xF0, 0xF7, 0x12** | VERIFIED |
| 3540..3716 | step 4 | fade-in, 128-update hold | VERIFIED |
| 3716..3770 | step 6 | sounds **0xF0, 0xF7**; fade-out; set-up 0xA0D0: stage index 4 and **FF83A3 clear -> 0xA1B8** (select screen) | VERIFIED |
| 3772 | step **32** (0xA49A) | 0xA1B8: FF83A3 = 1, timer 7, FF0E6E (choice made) = 0, three class-12 actors (1.3); step 32: FF8348 = 1, prompt text, name labels, "TIME" | VERIFIED |
| 3776 | step 34 -> **36** (0xA542) | countdown 320 updates (10 counts) while the cursor reads the pad (1.3) | VERIFIED |
| 4145 | step **38** (0xA5A0) | choice made (timeout here): 63 updates, then 6770 (window cleared), mode 2 play sub 0 | VERIFIED |
| 4208 | play sub 0 | stage reset 2DFE (clock **5:30**, HP = max, **music word 10**: sounds **0x0A, 0x01FA**), HUD, stage objects 4035E, stage events 9A92; form from FF8386 (0x2F08, correction 1) | VERIFIED |
| 4264 | sub 2 | fade-in (job 0x1002) | VERIFIED |
| 4276..4300 | subs 4 / 6 / 8 | title (1.4): 23 x sound **0x2F** | VERIFIED |
| 4372 | play step 2 | text cleared; clock runs (5:30 -> 5:29) | VERIFIED |
| 4388 | | player life state 2 (control) | VERIFIED |

Map scene to select: 236 updates; select (no input) to play set-up: 436; play
set-up to control: 180. Buttons fast-forward the map and title (FF799C); the
select screen clears FF799C every update (it cannot be skipped, only decided).
VERIFIED (run B: random buttons decided the select in 100 updates, 838 -> 936).

The select screen's step dispatch: mode-0 step table **0x9F90** (steps 26/28/30
= stage-4 intro, **32 0xA49A, 34 0xA50E, 36 0xA542, 38 0xA5A0**). SOURCE (ROM
table) / VERIFIED (step values in RAM).

**Restarts** (after a life loss): FF83A3 stays set, so 0xA0D0 skips the select
screen (0xA0E4) and the chosen form is kept (0x2F08 reads FF8386). Where FF83A3
is cleared for a new game: UNKNOWN (not in 2DFE; as FF83A0 in stage4.md 1.1).
FF83A3 lies in the per-player block FF8342..FF83C1 (exchanged in two-player
games, combat.md), so each player probably chooses once: UNKNOWN (no two-player
trace). SOURCE.

### 1.2 World-map scene for stage 5
As stage4.md 1.2 except: markers I..IV cleared (palette 0x14), **marker V**
blinks; **both heroes** stand side by side on the map: Willow and Madmartigan
above marker V (the castle at the right of the map), Madmartigan raising his
sword. VERIFIED (render `vidA/v003610`, `v003706`, montage `png/mA1.png`) /
UNKNOWN (which table entry 0x35268[4] / 0x35280[4] places the pair; read them
for the native map scene).

### 1.3 Select screen (0xA1B8, steps 32..38; renders `vidA/v003778`..`v004142`, `png/mA1.png`)
A black screen with the dialogue window at the top, a large "TIME" with a
two-digit countdown under it, and the two heroes standing over their names
(`WILLOW UFGOOD` left, `MADMARTIGAN` right, orange text).

| Item | Behaviour | Tag |
|---|---|---|
| Actors (0xA1B8) | class 12 **kind 32** (0x3611E) cursor at (64, 40); **kind 34** (0x362CA) Willow figure at (112, 72), +21 = 0, root **0xD14A**; **kind 33** (0x3626E) Madmartigan figure at (272, 80), +21 = 4, root **0xE26C** | VERIFIED (disassembly; births run B) |
| Prompt | step 32: text **"LET'S SELECT PLAYERS."** (string at ROM 0x7FB8, message 0x0A19 style 0x0306 = dialogue window, one sound **0x2F** per character: 21) | VERIFIED (ROM; render `vidA/v003814`; run CS 21 x 0x2F) |
| Names | messages 0x0271 / 0x0273 (styles 0x262D / 0x262E): `WILLOW UFGOOD` (ROM 0x71290), `MADMARTIGAN` (0x712E0) | VERIFIED (ROM; render) |
| "TIME" | big codes 06C6 06A0 06A8 0688 at scroll1 0x910930; digits at 0x910D30 (0x67C8 style 14): digit = ((timer - 1) >> 5) + 1, i.e. **10 .. 1**, 32 updates each; "00" at timeout | VERIFIED (code 0xA5BE; renders TIME10 / 06 / 02 / 00) |
| Cursor input (kind 32) | waits until the prompt is typed (FF7891 clear) + 32 updates; then each update, on a **new** press (FF806A & ~FF806B): **Right** (bit 0): cursor x 224, FF9866 = FF8386 = **4 (Madmartigan)**; **Left** (bit 1): cursor x 64, both = **0 (Willow)**; **button 1 or 2** (bits 4/5): confirm (FF0E6E = 1); cursor root **0x36206** (palette 8, 2 nodes x 4, a blinking frame) | VERIFIED (disassembly 0x3618A..0x361DC; run M: Right taps -> form 4 at key 1064) |
| Countdown | step 36 decrements the 320-update timer while no choice is made; at 0: "00" and the **current** choice is taken (default Willow) | VERIFIED (run A: timeout -> form 0) |
| On confirm | the unchosen figure removes itself (state 6), the chosen one stays 63 updates; FF0E6E = 1; if Madmartigan: player y += 8; erase pass 0x7DAE over the unchosen name (scroll1 0x910200 / 0x910C00); step 38: 63 updates, window cleared, play | VERIFIED (run M: y 72 -> 80; render `vidA/v004142`: only Willow left) |
| Sounds | 21 x 0x2F for the prompt; no music change here (the stage music starts at play set-up) | VERIFIED (run CS) |

Observed duration: run A step 36 lasted 369 updates (3776 -> 4145): the 320-update
countdown starts only once the prompt is typed. SOURCE (which flag holds the
timer during typing: likely FF8E58 / FF7891; not traced).

Native: a dedicated select scene (cut 3: the prompt in the text band). Default
Willow; Left/Right move the choice, A/B confirm; 10 x 32-update countdown.

### 1.4 Title text (play sub-steps 4, 6, 8)
- Sub 4: message **0x0658** (0x94C8[4] = 0x58): `STAGE5` (record 0x71CE0: four
  0x000E words, codes 06C4 06C6 0680 068C 0688 **0B08**). VERIFIED (ROM; render
  `vidA/v004300`).
- Sub 6: message **0x0669** (0x9506[4] = 0x69), style **0x1420** (0x950C[4]):
  `TIR ASLEEN CASTLE` (record **0x72274**: four 0x0000 words, then 17 big codes:
  T 06C6, I 06A0, R 06C2, space 01A6, A 0680, S 06C4, L 06A6, E 0688, N **06AA**,
  C 0684), drawn on the row under `STAGE5`, starting near column 5. Then sub 8 and
  play step 2 as stage 4. VERIFIED (ROM; render `vidA/v004366`; 23 x 0x2F = 6 +
  17).
- Cut 3 applies.

### 1.5 Music and clock
- **Music word 10** (0x0A then 0x01FA) at the play set-up for checkpoints 0-17;
  checkpoint 18 (the arena) starts with **music word 11** (1.7). VERIFIED (ROM;
  runs AS/S/CS: 0x0A + 0x01FA right before the title's 0x2F run).
- **Boss music 0x0B**: posted by event 13 when the player passes x **5584** (3.2,
  6.2). VERIFIED (run S: 0x0B at column 14938 = key ~15450, the event's spawn key).
- No music triggers (class 16 kind 8) in this stage; the shop (if entered) fades
  and restores the music as stage4.md 1.5. SOURCE (table; no shop in the runs).
- Clock: **5:30** at checkpoints 0-1, **5:00** at 2, **4:30** at 3-5, **4:00** at
  6-9, **3:30** at 10-12, **3:20** at 13, **3:10** at 14, **3:00** at 15, **2:45** at
  16-17, **2:30** at 18 (records 0x342C). Normal ticks; warning 0x43 under 0:16.
  It stops for good at the second head's death (FF83E9, 6.7). Run B: 5:30 at 1164,
  1:24 at the arena (15908), 0:45 at the boss's death. VERIFIED (ROM + trace).

### 1.6 Start position, player form
- Start **(192, 72)** for Willow, **(192, 80)** for Madmartigan (y + 8,
  correction 1), facing right, camera **(0, 0)**; checkpoint-0 record (192, 72);
  initial last-safe position 0x2FA2[4] = (192, 80). VERIFIED (runs A, C, M).
- Form: FF8386 from the select screen (0 or 4) from play sub 0 on; the stage
  content (objects, events, boss) is the same for both forms. VERIFIED (runs B/M:
  identical object and event tables, same boss trigger chain).

### 1.7 Checkpoints (restart table)
Resolver **0x33A4** (0x2F66[4] = word 0x043E), records **0x342C** (0x304C + word
0x3E0; 8 bytes: x, y, time BCD, music word). VERIFIED (ROM).

| c | x | y | time | music | camera min X | Where |
|---|---|---|---|---|---|---|
| 0 | 192 | 72 | 5:30 | 10 | 0 | start, outer wall |
| 1 | 1120 | 96 | 5:30 | 10 | 608 | tower foot |
| 2 | 1376 | 416 | 5:00 | 10 | 864 | tower / wall slope |
| 3 | 2240 | 416 | 4:30 | 10 | 1728 | wall top before the gap |
| 4 | 2560 | 640 | 4:30 | 10 | 2048 | upper corridor entrance (the gate, 5.6) |
| 5 | 2944 | 640 | 4:30 | 10 | 2432 | upper corridor |
| 6 | 3200 | 640 | 4:00 | 10 | 2688 | upper corridor |
| 7 | 3456 | 640 | 4:00 | 10 | 2944 | upper corridor |
| 8 | 3712 | 640 | 4:00 | 10 | 3200 | upper corridor |
| 9 | 3968 | 640 | 4:00 | 10 | 3456 | corridor end |
| 10 | 4160 | 608 | 3:30 | 10 | 3648 | interior, top |
| 11 | 4576 | 544 | 3:30 | 10 | 4064 | interior |
| 12 | 4512 | 384 | 3:30 | 10 | 4000 | interior |
| 13 | 4752 | 368 | 3:20 | 10 | 4240 | interior |
| 14 | 4448 | 79 | 3:10 | 10 | 3936 | interior, bottom |
| 15 | 4608 | 79 | 3:00 | 10 | 4096 | interior, bottom |
| 16 | 4880 | 128 | 2:45 | 10 | 4368 | interior exit |
| 17 | 5056 | 128 | 2:45 | 10 | 4544 | battlements |
| 18 | **5472** | 128 | **2:30** | **11** | **5376** | arena (record x word 0x9500: bit 15 -> x 0x1500 = 5376, camera min X = x & 0xFF80 = 5376, then **+96 for stage index 4** (0x303C) -> x 5472) |

Normal records: camera min X = x - 512 (0 if negative) (0x301A). VERIFIED (ROM).

Selection from the last safe position (FF8370 / FF8372, high bytes of x and y):
i = (x >> 8) * 4 + r, r = (y >> 8) (0 if the byte is negative) & 31, c = byte
table **0x33CC**[i] (96 bytes, x 0..6143); the index only ever increases. Note the
mixing: i = 4 * xBlock + yBlock lets yBlock 0..7 spill into the next x blocks'
entries (the table is laid out for it). VERIFIED (ROM, disassembly 0x33A4).

| x >> 8 | 0-2 | 3 | 4 | 5 | 6-7 | 8-9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21-23 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| bytes [r=0..3] | 0 0 0 0 | 0 1 1 1 | 1 1 1 1 | 1 2 2 2 | 2 2 2 2 | 3 3 3 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 14 12 10 10 | 15 13 11 11 | 16 | 17 | 18 |

(Index i = 4*xb + yb: e.g. x 4352..4607 (xb 17) at y 256..511 (yb 1) reads
i = 69 = entry 17[1] = **12**; at y 512..767 (yb 2) i = 70 -> 17[2] = 10; with
yb >= 4 the read continues into the next x block's entries.) VERIFIED (ROM).

Restart after event 13 (the arena event): **FF83A6** set -> the event spawns no
trolls and bumps **FF83EE** at once (3.2), so the boss wakes without the troll
wave. SOURCE (0x39750).

### 1.8 Difficulty rank
Stage start (0x38146): excess over FF8291 halved, plus **0x18** (0x3819A[4]),
minus the death-count term (stage4.md 1.8). Run A: 0x3058 at the end of stage 4
-> **0x4858** at the stage-5 map. Run B (debug start 0x3000): 0x3005 at play
start, 0x699D at the arena, 0x72AB at the boss's death (~1 per update).
VERIFIED (traces).

---

## 2. Map, layers, camera, collision, palettes

### 2.1 Layers, scroll rates and stage size
- **Foreground (scroll2)**: grid **4** (0x2DBF8[4] = **0x2E824**), mode
  **1** (0x2DC1C[4] = 1): **8 block rows x 32 columns** (stagemap.py SHAPE 1 =
  (8, 32) is right), 256x256-px blocks of 16x16-px cells at ROM 0x80000 + b*0x400.
  Row r covers world y (7-r)*256 .. +255. Used blocks:

  | Row | World y | Columns: blocks | Part |
  |---|---|---|---|
  | 7 | 0..255 | 0-8: 32 33 34 35 36 37 41 41 41; 9-15 empty; 16-23: 87 88 89 56 91 92 93 94 | outer wall path (start); the gap (empty); interior bottom, battlements and arena (x 4096..6143) |
  | 6 | 256..511 | 3-10: 38 39 40 42 43 44 45 50; 15-20: 52 60 85 86 60 90 | tower and wall slopes; interior middle |
  | 5 | 512..767 | 7-19: 51 46 47 48 55 55 56 56 56 57 58 59 60 | upper corridor (x 1792..5119) |
  | 1 | 1536..1791 | 0-10: 53 54 55 55 56 56 56 57 58 59 51 | not reached in play (camera Y stays <= ~375): purpose UNKNOWN |

  Play area x 0..6143 (camera max X 5760 + 384), y 0..767. Empty blocks below
  the corridor between x ~2304 and 4095 form the **gap** (open sky, the far layer
  shows through; below it nothing: black). VERIFIED (ROM grid dump; assembled map
  `S/map/s5_seg.png`; renders).
- **Far layer (scroll3)**: grid 0x2EB24[4] = **0x2EC4C**, mode 0 (16 x 16 blocks
  of 256 px, 32x32 tiles at 0xC0000 + b*0x100); only **columns 0-7** (far x
  0..2047) are filled:
  row 15 (far y 0..255) **23 25 27 23 25 27 23 25**, row 14 (256..511) **22 24 26
  22 24 26 22 24**, row 13 (512..767) **28** x 8. Columns 8-15 are block 0 (empty).
  A sunset sky with clouds and distant mountains; the three column blocks differ
  (e.g. more purple cloud near some), period **768 px** of far x. VERIFIED (ROM;
  renders `vid/v001200`, `v008400`..`v011000`).
- **Scroll rates (every frame)**: scroll2 = camera (1:1); scroll3 = **camera / 2
  in x and in y** (far X = camX >> 1, far Y = camY >> 1): shift table **0x42FC4**
  bytes 1, 1, 1, 3, **1**, 1. Over run B's play frames 7,405 of 7,959 RAM dumps
  have FF8488 = FF8408 >> 1 and FF848C = FF840C >> 1; the exceptions are all after
  the arena lock (below). Run M: 1,470 / 1,475. scroll1 (HUD/text) fixed.
  VERIFIED (disassembly table; RAM).
- **Arena**: from the arena lock (key 15908, camera mode 14) the far layer stops
  at **(2783, 0)** (camX 5566 >> 1), i.e. far x in the empty columns: the arena's
  red sky is **scroll2** (blocks 91-94). At the boss's wake the far layer is
  cleared (0x6666 fills scroll3 with tile 0x0A00) and becomes the **boss's body
  layer** (6.3), scrolled by the boss, and the layer order changes. VERIFIED
  (RAM; `vid/v017400` scroll3-only render `png/p3_017400.png`).
- **No row, line or column scroll** anywhere: CPS-A video control 0x800122 =
  **0x003E** in every stage-5 dump (1,738 in run B, 551 in run A). Layer order
  (0x800170) **0x12F8** in play (scroll3 behind scroll2), **0x12C0** for one dump
  at the select set-up and the play set-up, **0x13B8** from the boss's wake
  (scroll2 and scroll3 swapped: **scroll3 in front of scroll2**, sprites and
  scroll1 on top). VERIFIED (`S/regs.py vid 1100 18470`; `boss.dis` 0x2B794: FF804A
  = (FF804A & 63) | 0x13B8).
- **Special scroll effects**: (a) the camera only moves right through the gap
  (2.3); (b) the arena lock and the boss's own far-layer motion (6.3); (c) the
  gate tile animation (5.6) and wall torches (5.7) rewrite scroll2 cells. No
  palette effects (2.6).
- **Native (parallax rule, AGENTS.md 2026-11-02)**: plane A = foreground at the
  camera; plane B = the far layer at (camX / 2, camY / 2) in **both** axes during
  play. The far picture is 2048 px wide (8 blocks, a 768-px period that does not
  divide 2048: stream columns) and needs only rows 15 and 14 (far y 0..~420 for
  camera Y <= ~375 plus 224 visible): 512 px tall. At the arena lock plane B
  stops and is later reused for the boss body; the boss body must draw **above**
  plane A (plane B tiles with the priority bit, plane A low priority) while the
  layer order is 0x13B8.
- **Areas** (renders, run B keys):

  | Camera X | Area | Key frames |
  |---|---|---|
  | 0..~600 | moss-green curtain wall with a gate, dirt path, tall grass and rocks; sunset sky | `vid/v001200`, `v002500` |
  | ~600..1300 | ruined round tower; trolls drop out of its window holes; chest (1472,176) on a wooden balcony | `vid/v003500`, `v005000` |
  | ~1300..2300 | wall slopes up to the wall top (camera Y up to ~370); trolls, archer, white studded wall | `vid/v006000`, `v007000` |
  | ~2300..4100 | the gap: open sky over a void; falling stone pillars (camera one-way) | `vid/v008400`..`v011000` |
  | upper corridor y 512..767 | green stone hall with dark recesses, entered through the white tower's doorway (5.6) | `S/boss2/vidK/v002122.cps`..`v006994.cps`, `S/map/s5_seg.png` |
  | ~3700..5100 | castle interior: dark stone halls, balconies with spike rails, torches, bats, spearmen, archers, chests, shop door (4816,648) | `vid/v012000`..`v014000` |
  | ~5100..5760 | battlements under a red sky; arena (camera locked at 5376..5760) | `vid/v015000`, `v015900`, `v017400` |

### 2.2 Terrain (scroll2 grid 4, collision ids)
Collision id = attribute bits 10-15 of the 16x16 cell; descriptors at 0x4712 +
id*8 (player.md 6). Ids by count over grid 4 (all rows): see `stagemap.py` output
`S/map/s5.cells` (rows 5-7). Not tabulated here: UNKNOWN (run the colpack /
stagemap census; check for ladder, hazard (39) and spike ids on the interior
spike rails and the 1:2 / 1:4 wall slopes). The gap has no floor cells (the
pillars, 5.4, are the only footing there). SOURCE (map render) / UNKNOWN (ids).

### 2.3 Camera
- Limits at stage start (0x2DCA / 0x2DBE / 0x2DD8 [4]): max X **0x1680 =
  5760**, min Y **0**, max Y **0x800 = 2048**. VERIFIED (ROM; RAM FF846A = 5760,
  FF845C = 0, FF8434 = 2048 from key 1100).
- X follows the player at player X - 192; moving right raises min X to camera X -
  **768** (run B: min X = camera - 770 on the outer wall). Y follows the player
  (camera Y 0..~375 in run B). VERIFIED (trace).
- **Stage-5 camera hook 0x42E2C** (table 0x42D74[4]): while camera X is in
  **[2368, 3528)**, min X (FF8466) = camera X: **no scrolling back over the gap
  and the corridor**. VERIFIED (disassembly; run B min X = camera X from 2389 at
  key 8500 to 3526 at 10900, then constant).
- **Event 13** (x 5584, 3.2) sets camera min X FF8466 = **5376** (and FF838C).
  VERIFIED (RAM 15450).
- **Arena lock**: player X >= max X **5760** (key 15908, player 5761, camera
  5566): FF83D0 = 1, FF8470 = 1, camera mode **14**, player life state **14** for
  one update (no arrival walk, as the stage-4 dragon arena); the camera keeps
  following inside **[5376, 5760]** and reaches 5760 (key ~16500). VERIFIED
  (trace 15908..16500).
- The boss sets camera mode byte FF8405 = 2 at its wake; the camera stays at 5760.
  Camera Y reads 65534 (-2) during the boss: it is the boss's **screen shake**
  (6.4 bob step 2: camera Y alternates -2 / 0 every update for 16 updates, once
  per 48-update bob cycle). VERIFIED (disassembly 0x2BB94; run W RAM).

### 2.4 Pit
The gap (x ~2304..4095 below the corridor) is a pit: falling below the bottom
(world y ~0, camera min Y 0) is a pit death (combat.md 3.2). VERIFIED (run B:
the invincible player bounced back up at y 0 several times between keys 8400 and
11000) / SOURCE (death path, invincibility masks it).

### 2.5 Actor retirement
The general "x < camera min X" rule (21C6). The camera's one-way section (2.3)
retires actors left behind in the gap. SOURCE.

### 2.6 Palettes
- Loaded at stage set-up (0x0978, stage index 4): objects 0-15 **0xE5080**,
  objects 16-31 **0xE5C80** (0xE5480 + 0x800), scroll2 (32 palettes) **0xE8D20**
  (0xE7D20 + 0x1000), scroll1 text **0xE76A0** (correction 2), scroll3 16
  palettes **0xEAD00** (0xA12[4]) into scroll3 palettes **16-31** (0xA2E[4] = 1).
  SOURCE (disassembly).
- **No palette controllers**: table 0x410EC[4] = **0x411D0** holds count -1
  (0xFFFF); and **no palette changes at all during play**: `S/palscan.py vid
  1100 18470` finds no change in any of the 128 palettes between consecutive
  dumps (every 10 updates) until the stage-6 load (key 18470). The sunset /
  purple / red skies are different picture blocks (far blocks 22-27, scroll2
  blocks 91-94), not palette effects. VERIFIED (ROM; dumps).
- Boss hit flashes: none seen (no natural hits); the boss's hit path (6.5) only
  redraws the bar and plays a sound, no palette job. SOURCE.
- Story page: no controller (7.2).

---

## 3. Placed objects and spawn events

### 3.1 Stage object table (0x4034A[4] = 0x409D0, 57 entries)
Entry bytes class, variant, kind, direction, x, y (loader 0x40374). All present
at key 1056 in run B. VERIFIED (ROM; births).

| # | class / kind / variant | dir | x, y | What it is (section) |
|---|---|---|---|---|
| 0 | 12 / **35** / 0 | 0 | 2576, 640 | tower doorway: corridor interior wipe (5.6) |
| 1 | 12 / 8 / 0 | 11 | 1008, 296 | shop door (8.2) |
| 2 | 12 / 8 / 16 | 12 | 4816, 648 | shop door, interior top (8.2) |
| 3-6 | 16 / 4 / 0 | 0..3 | (4720,240) (4416,320) (4816,480) (1472,176) | chests 0-3 (8.1) |
| 7-12 | 4 / **27** / 4 | 0 | (3176,656) (3488,656) (3632,656) (3792,656) (3936,656) (4064,656) | corridor cart pushers (5.3) |
| 13-21 | 4 / **26** / 0 | 1,1,1,0,0,1,1,1,1 | (1024,104) (768,104) (1456,104) (1120,296) (1024,376) (1328,424) (1616,424) (1792,504) (2336,503) | trolls (5.1) |
| 22, 25 | 6 / **7** / 2 | 0 | (2840,672) (3256,672) | standing stone pillars (5.4) |
| 23, 24 | 6 / **7** / 0 | 0 | (3000,832) (3096,832) | falling stone pillars (5.4) |
| 26 | **18 / 6 / 0** | 0 | 2992, 240 | **boss Eborsisk** (controller record; its coordinates are far-layer coordinates) (6) |
| 27-37 | 12 / 6 / 5 | 0 | (4432,592) (4528,592) (4576,592) (4720,528) (4656,480) (4368,368) (4512,304) (4560,208) (4432,96) (4560,96) (4688,96) | wall torches (5.7) |
| 38-46 | 4 / 0 / 4 | 0 | (592,80) (768,104) (1072,104) (1264,104) (1328,424) (1408,424) (1488,424) (2240,424) (2336,424) | placed soldiers (4.1) |
| 47 | 4 / 3 / 2 | 1 | 2336, 504 | archer (4.2) |
| 48-50 | 4 / 3 / 2 | 1 | (4608,88) (4688,88) (4768,88) | archers, interior bottom |
| 51-56 | 4 / 14 / 0 | 1 | (4632,464) (4504,400) (4592,304) (4520,176) (4944,144) (5248,144) | spearmen (4.3) |

No hidden object (class 16 kind 9) and no music trigger in this stage. VERIFIED
(ROM).

### 3.2 Stage event table (0x9AD8 + word 0x02D2 = 0x9DAA, 14 events)
Fields: +0E class, kind, variant, +0F, par (+12), endX (+06), y (+08), startX
(+0A), spawnY (+0C). Routines through the class-4 event table 0x381FA
(correction 4). VERIFIED (ROM; every event present at key 1056 in run B).

| Ev | +0E / kind / var | +0F | par | +06 | +08 | +0A | +0C | Meaning (section) |
|---|---|---|---|---|---|---|---|---|
| 0 | 4 / **31** / 0 | 0 | 0 | 3968 | 272 | 3968 | 272 | bat flock (0x3824A): when player X > **4160**, place the bats (4.4) |
| 1 | 4 / **14** / 16 | 0 | 512 | 2752 | 656 | 4096 | 656 | spearman entrant, camera X 2752..4096 in the corridor (4.3) |
| 2-5 | 4 / **14** / 14 | 1 | 0 | 4496 / 4640 / 4784 / 4784 | 560 / 560 / 496 / 384 | = +06 | = +08 | respawning spearman spawners in the interior (4.3) |
| 6-12 | 4 / **26** / 0..6 | 0 | 0 | 904, 1392, 1072, 1336, 1808, 1928, 2048 | 112, 192, 368, 432, 512, 512, 512 | 904, 1416, 920, 1336, 1848, 1928, 2008 | 192, 240, 384, 352, 432, 432, 432 | troll windows (5.2) |
| 13 | 4 / **26** / 7 | 0 | 0 | 5584 | 135 | 5584 | 135 | arena event: four trolls, camera min X 5376, boss music (6.2) |

### 3.3 Roster

| Family | Class / kind / var | Count | Where | Spawned by | Energy | Score | Drop |
|---|---|---|---|---|---|---|---|
| Soldier (placed) | 4 / 0 / 4 | 9 | outer wall, wall top | table | 1 | 50 | stage1_enemies.md 5.1 |
| Archer | 4 / 3 / 2 | 4 | wall top, interior bottom | table | as stage 1 | as stage 1 | as stage 1 |
| Arrow | 10 / 0 | per shot | - | archers | - | - | - |
| Spearman + returning spear | 4 / 14 / 0 (10 from cart pushers, 14 / 16 from events); 10 / 7 | 6 placed | interior, battlements, corridor | table, cart pushers, events 1-5 | 4 (variants 10 / 16: **8**, run K) | 200 | 100 gold |
| Bat | 4 / 31 / 0 | up to 33 | interior | event 0 | 1 | 100 | set 1 |
| **Troll** | 4 / 26 / 0 (placed), 1-7 (windows), 8-9 (arena) | 9 placed, 7 windows, 4 arena | outer wall to battlements | table, events 6-13 | **10** | **500** | set 1 or 2 |
| **Troll orb** | 10 / 15 | per throw | - | trolls | - | - | - |
| **Cart pusher + spiked cart** | 4 / 27 / 4 + 6 / 8 | 6 | upper corridor | table | pusher not hittable; cart **30** | - | - |
| **Stone pillar** | 6 / 7 / 0, 2 | 4 | the gap | table | 16 | none | none |
| **Tower doorway wipe** | 12 / 35 | 1 | (2576, 640) | table | - | - | - |
| Wall torch | 12 / 6 / 5 | 11 | interior | table | - | - | - |
| Chests | 16 / 4 | 4 | tower balcony, interior | table | 6 | - | 8.1 |
| **Eborsisk** (scroll3 neck/controller, sprite neck, 2 heads) | 18 / 6 / 0..3 | 1 | arena | table + event 13 | 127 per head (sprite neck 1) | 10,000 per head | set 9 (14,000 gold) |
| Boss fire chain | 10 / 20 / 0..10 | 6, 9 or 11 per breath | - | heads | - | - | - |
| Select-screen actors | 12 / 32, 33, 34 | 1 each | select screen | 0xA1B8 | - | - | - |

Census over run B (births.txt): bursts class 12 kind 0 x 205, pickups class 14
x 37, troll orbs class 10 kind 15 x 47, boss fire class 10 kind 20 x 47 (5
breaths), arrows class 10 kind 0 x 18, spears class 10 kind 7 x 2, bats x 30
(placed at once at key 11950), trolls: 9 placed + 3 window spawns + 4 arena.
VERIFIED.

---

## 4. Families shared with stages 1-4: stage-5 differences

### 4.1 Soldiers (class 4 kind 0 variant 4, placed)
Nine placed variant-4 soldiers: drop in when on screen, search, then as
stage1_enemies.md 5.1 (energy 1, score 50, palette 16). The level-change
routine runs normally (the stage-4 exception, stage4.md correction 4, is index 3
only). VERIFIED (births: born 1056, killed 2418..6612) / SOURCE (behaviour).

### 4.2 Archer (class 4 kind 3 variant 2)
(2336, 504) on the wall top and three at the interior bottom (4608..4768, 88),
facing left. As stage1_enemies.md 5.9 (arrows class 10 kind 0, sound **0x34**).
VERIFIED (18 arrows: 7018..8352 from the wall-top archer, 12730..13720 interior).
Run S has 67 x 0x34, more than the arrows: the other source is UNKNOWN.

### 4.3 Spearman (class 4 kind 14)
- Variant 0: six placed in the interior and on the battlements, facing left; as
  stage3.md 4.1 (returning spear class 10 kind 7, sound 0x8B: 1 throw in run S).
  VERIFIED (births).
- **Variant 14** (events 2-5, mode A of event kind 14, 0x38C24): every 8 updates,
  when the spawner (x +06, y +08) is on screen or just right of it (0 <= x -
  camX <= 384 and 0 <= y - camY <= 256): spawn kind 14 variant 14 at (+0A, +0C)
  facing +0F (1 = left), linked to the event (+26); after it dies (+16 cleared by
  the child), wait 0x38D72[rank >> 4] = 240, 240, 240, 240, 240, 240, 210, 210,
  210, 180, 150, 120, 90, 60, 60, 30 updates, then spawn again: **endless
  spearmen** from these four balconies while in view. SOURCE (disassembly); never
  triggered in runs B/M (the players stayed low, y 79..247, and the spawners are
  at y 384..560).
- **Variant 16** (event 1, mode B 0x38CCC, par high byte 2): every 8 updates,
  while 2752 <= camX <= 4096 and 656 - camY <= 256, and no variant-16 spearman
  of this event alive: spawn kind 14 variant 16; camX > 4096 retires the event.
  The spawner writes x = FF840C (camera Y) - 16, but the spearman appears at
  **x = camera X (the left screen edge)**, y 656, **facing right** (+0F = 0), so
  it walks in **from behind** the player; energy **8**, palette 18. Run K: born
  at key 2150 (camera X 2752, the first update the camera reached 2752) and 3030
  (x 3085 = camera X); after its death the next comes once the previous is gone.
  VERIFIED (run K RAM) / SOURCE (spawner rule).
- **Variant 10**: the soldier who pushed a spiked cart (5.3) turns into a
  spearman variant 10 at his own position, facing his facing (left), energy **8**,
  when the cart is destroyed or the player is behind him; from then on as the
  ordinary spearman (pokes, hit reaction about 12 updates (state 4 phase 2),
  energy -1 per level-0 shot, death at 0). VERIFIED (run K: keys 2592, 3424,
  3784).

### 4.4 Bats (class 4 kind 31, stage4.md 5.2) and the flock event
Event 0 (0x3824A): once the player X > **4160**: for each of **33** positions in
table **0x382B8**: skip it if x < camera min X (FF838C) or if it is inside the
box |x - playerX| <= 128 and |y - playerY| <= 100 (roughly on screen); else place
a class 4 kind 31 bat there (+0 = 1); then the event ends. Positions (x, y):
(4608,688) (4640,688) (4672,688) (4704,688) (4736,608) (4768,608) (4512,504)
(4544,504) (4576,504) (4608,504) (4640,504) (4672,504) (4608,408) (4640,408)
(4672,408) (4480,344) (4512,344) (4544,344) (4704,328) (4736,328) (4768,328)
(4544,248) (4576,248) (4608,248) (4640,248) (4448,120) (4480,120) (4512,120)
(4544,120) (4576,120) (4608,120) (4640,120) (4672,120). VERIFIED (ROM; run B: 30
bats born at key 11950, player (4164, 438)). Behaviour as stage4.md 5.2 (wake
within 96 x / 112 y of the player, swoop, energy 1, score 100, drop set 1,
palette **22**). Run B: most bats never woke and lived until the clear, so the
stage-4 "x > 4096 off screen" retirement does not remove them here: UNKNOWN
(check 0x22292's retire test for the stage index).

### 4.5 Shared effects
Bursts class 12 kind 0 variants 4/5, gold pickups class 14 kind 0
(stage1_enemies.md 6). VERIFIED (births).

---

## 5. New families

Pose tables for the packer: `S/roots.txt` (from `roots2.py` with the right
geometry base: class 4 0x3AC4A, class 6 0x3B1AA, class 10 0x3B0D2, class 18
0x3B07A). Boxes are (xOffset, width, yOffset, height).

### 5.1 Troll (class 4 kind 26, 0x1FCE6), placed variant 0
Brown, hairy, ape-like brutes with grey faces (`vid/v002700`..`v003150`,
`v004990`, `v016700`): they walk at the player, leap, grapple and throw green orbs.

| Item | Value | Tag |
|---|---|---|
| Energy | **10** (+2A/+2B) | VERIFIED (trace hp 10) |
| Score | 0x1806 = **500** | VERIFIED (score +500 at 17016) |
| Drop | 0x20D5C[rand & 15] = 1, 2, 1, 2, 1, 2, 2, 1, 2, 1, 2, 1, 2, 1, 1, 2: set **1** (10 gold) or **2** (100 gold), 50/50 | SOURCE |
| Palette | **24** | VERIFIED (walk; `objpal.py vid/v016700.cps`) |
| Init (0x1FDAA) | probe +13 = 9, +10 = 25, motion **0xED070**; aggression timer +39 from rank table 0x1FDFC (rows of 4 by rank >> 5, rand & 3; row 0: 176, 255, 224, 176; row 1: 176, 160, 160, 144; ...) | SOURCE |
| Variant dispatch (+21) | 0 -> 0x1FD08 (placed), **1-7 -> 0x20698** (window entrants, 5.2), **8-9 -> 0x1FD1C** (arena, 6.2) | SOURCE |
| Hit | energy changed: +0 = 2, sound **0x9B**, knock-back pose; if already in the hurt phase: 18 updates (+3C) | VERIFIED (86 x 0x9B in run S) |
| Death | sound **0x9C**, +0 = 2, thrown up (y speed **768**, gravity -48/update to -1792), on landing (3B0A) sound **0x33**, burst (0x4908), score 500, drop; window/arena counters decremented (FF0F5C / FF0F87) | VERIFIED (6 x 0x9C; 0x33) / SOURCE (arc) |
| Throw | class 10 kind 15 orb (5.5) from three code paths (0x204A6, 0x2053E, 0x20B80); sound **0x9D** | VERIFIED (47 orbs, 47 x 0x9D in run S) |
| Leap | jump speed **1664** (6.5 px/update) in three paths (0x201CE, 0x20402, 0x20AE6) | SOURCE |
| Phases | walk (0x20D88 loop), crouch/land (0x20E64 / 0x20E68), leap (0x20F1C, 0x20F60), grapple / hold (0x2102C, 0x2115A, 0x21338), hurt (0x21464), death (0x20D84) | SOURCE (pose usage) / UNKNOWN (phase table, ranges, timings: trace a troll fight with the slot tool) |
| Boxes | stand / walk (10,20,24,40); upright (6,12,24,46..48); grapple (14,28,8,32); low (16,32,6,12) / (16,32,16,22) | VERIFIED (walk) |

**No grapple.** Run B (every 2 updates) shows no player state change near any
troll in the 0x2102C / 0x2115A / 0x21270 / 0x21338 poses: those are the **window
trolls' wall-clinging** poses. Window trolls (variants 1-7, 5.2) appear at the
window (pose 0x21104 / 0x21138 / 0x2109C / 0x210DC), then cling to the wall at
the window's x (0x2102C 4 x 10 loop) and **move up and down the wall** (run B
slot 33 variant 1 at x 904: y 192 -> 175 -> 138 -> 126 -> 174 ...), throwing orbs
from the wall (pose 0x21338 / 0x213D0 at each throw), until they **drop** (phase
2.4: 0x2115E, landing 0x20E7C) and become a walking variant-0 troll (run B: slot
33 variant 1 -> 0 at key 4276, variant 2 at 5064, variant 5 at 5900..5952). The
render `vid/v003080` ("a troll on top of Willow") is a wall troll above Willow.
VERIFIED (run B RAM).

Phases seen on a placed troll (run B slot 43, keys 1056..3654; +2.+3.+4):
| Phase | What | Timing / motion | Tag |
|---|---|---|---|
| 2.0.x | walk toward the player (0x20D88 loop) | | VERIFIED |
| 2.2 / 2.4 (state 2) | **hopping**: crouch (pose 0x20E68 = 0x20ED2/0x20E7C) then a leap (0x20EF2..., y +43 at the top), **20 updates per half**, about **60 px forward per 40-update hop** toward the player | run B 3200..3280: x 1030 -> 970 -> 910 -> 850 -> 790 | VERIFIED |
| 2.8 | **throw**: pose 0x20D84[f] (0x211B8 / 0x21214; the root table earlier labelled "death fall"), orb (5.5) | 32 updates (2.8.2 -> 2.0.2) | VERIFIED |
| 2.10, 2.6 | other attack / turn steps (0x21214 / 0x20ED2) | | UNKNOWN (detail) |
| 4.2 (state 2) | **knock-back** after a hit: pose **0x21464[side]** (0x21468 / 0x214AC by +30 = the player's side), pushed ~42 px away over 28-30 updates, then walk | run B 3168 -> 3196: x 992 -> 1034 | VERIFIED |
| 4.6 | death throw (same 0x21464 poses, y speed 768) | | VERIFIED |
Placed trolls stay asleep (+1 = 0) until the player is within 224 px in x and
112 in y (0x1FE48: |x - playerX + 224| <= 448, |y - playerY + 112| <= 224),
then count themselves in FF0F5C (`a5@(-28836)`) and face the player. SOURCE
(disassembly) / VERIFIED (run B: slot 43 woke at 3146, player 807 vs troll 1024).
Phase-table decode (which attack is chosen when, the +39 aggression timer):
UNKNOWN (k26.dis 0x2015C..0x2069x not read).

**Roots** (palette 24, `S/roots.txt`): 0x20D84[f] (0x211B8 / 0x21214, 4 nodes,
death fall), 0x20D88[f] (0x20D8C / 0x20DF8, 4 x 10 walk loop), 0x20E64[f]
(0x20E70 / 0x20EC6), 0x20E68[f] (0x20E7C / 0x20ED2), 0x20F1C[f] (0x20F20 /
0x20F40), 0x20F60[f] (0x20F64 / 0x20FC8, 4 x 6 loop), 0x2102C (0x21030, 4 x 10
loop, both facings), 0x2115A (0x2115E, 3 nodes), 0x21270[f] (0x21274 / 0x212D6),
0x21338[f] (0x2133C / 0x213D0), 0x21464 (knock-back / death throw, indexed by
+30 = side: 0x21468 / 0x214AC, 2 nodes 6 then hold, 11 pieces, box (6,12,24,46);
`S/boss2/roots_boss.txt`). VERIFIED (walk; run B nodes 0x214AC / 0x21468 during
hits).

### 5.2 Troll windows (event kind 26 variants 0-6, 0x395D0)
The tower and wall windows from which trolls climb out (`vid/v003080`: a troll
climbing out of a window hole).

| Item | Value | Tag |
|---|---|---|
| Arm | first update: timer +14 = **32**, FF0F80[var] = 0, window box from 0x39614 + var*8: (a, b, c, d) = (0x90,0x120,0x40,0x80), (0x78,0xF0,0x40,0x80), (0x60,0xC0,0x80,0x100), (0x90,0x120,0x40,0x80), (0x50,0xA0,0x40,0x80), (0x30,0x40,0x40,0x80), (0x50,0x60,0x40,0x80) | SOURCE |
| Test (when FF83D2 clear) | alive count FF0F5C < cap 0x39668[rank >> 4] = 1, 1, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5; this window's troll not alive (FF0F80[var] = 0); player in the window box: 0 <= (x +06 - playerX) + a <= b and 0 <= (y +08 - playerY) + c <= d; then timer -1; at 0: spawn | SOURCE |
| Spawn | class 4 kind 26 **variant var + 1** at (+0A, +0C) (the window), FF0F80[var] += 1, FF0F5C += 1; next timer 0x3971C[(rank & 0xE0) >> 3 + rand & 3] (rows of 4 bytes; e.g. rank row 0: 176, 240, 255, 192) | SOURCE |
| Retire | every 64 updates (clock + slot) the off-camera test 21C6 | SOURCE |

Run B: windows 6, 7, 10 spawned trolls (variants 1, 2, 5) at keys 3072, 4984,
5832. VERIFIED (births). Window positions (x, y) = (904,192), (1416,240),
(920,384), (1336,352), (1848,432), (1928,432), (2008,432).

### 5.3 Corridor cart pushers (class 4 kind 27 variant 4, 0x214FA) and spiked carts (class 6 kind 8, 0x21AE2)
Six green-armoured soldiers in the upper corridor (y 656, x 3176, 3488, 3632,
3792, 3936, 4064) each **push a wheeled cart with a spiked front** toward the
player (render `S/boss2/png/door.png`: `vidK` 2122 (corridor), 2194 (pusher and
cart arriving; shots hitting the cart), 2410 (cart against Willow), 2584 (cart
destroyed), 2596 / 2662 (the pusher, now a spearman, fights)).

| Item | Value | Tag |
|---|---|---|
| Wake (pusher) | when 0 <= x - camX <= 448, 0 <= y - camY <= 256 and FF83EA clear: facing forced **left** (+0F = 1; the old facing kept in +36), create the cart (class 6 kind 8, linked both ways: cart +26 = pusher, pusher +38 = cart), pusher **+0 = 2** (cannot be hit while pushing), timer 64, motion 0xED068, pose 0x21704 (push loop) | VERIFIED (run K: pusher at 3176 woke at key 2126, camera 2732) / SOURCE |
| Push | the pusher walks left at **1 px/update** (run K 2126 -> 2190: x 3175 -> 3113); every 64 updates rand & 15 indexes 0x21624 = 0,1,0,0,1,0,0,0,0,0,1,0,0,0,1,0: on 1 (4 in 16) it **stops for 64 updates** (+31 = 1, pose 0x21700 hold), then pushes on | VERIFIED (stops at 2190..2254, 2510..2574, 2902..2966, 3158..3222) |
| Cart position | every update the cart sits at pusher x **- 48** (facing left; + 48 facing right), same y | VERIFIED (3175 / 3128 ... 3097 / 3049) |
| Cart | class 6 (a solid obstacle, combat.md 2.1: pushes the player 2 px/update, hurts on contact rules of class 6), energy **30**, palette **19**, box (16,40,32,64); its wheels turn (+30 & 3 selects the pose in the 4-node loop) while the pusher moves; blocked by the player, the pair stops advancing (run K: player held at x 3013..3029 while the cart sat at 3049..3065) | VERIFIED (run K) / SOURCE |
| Cart hit | energy changed: +0 = 3 and a flash (+12 toggled every 3 updates) for **10** updates, then +0 = 1; below 10 energy the cracked pose set; Willow's level-0 shots took 1 each (run K: 30 hits 2146..2572) | VERIFIED |
| Cart destroyed | energy 0: phase 4: burst (0x492C), 15 updates, then the cart's +32 is set and it is removed (2AD2) | VERIFIED (run K 2574 -> 2590) |
| Pusher ends | when the cart's +32 is set, or the player is behind him (0x21652: player x > pusher x while he faces left), or x < camera min X + 128: phase 4: a **spearman variant 10** at his position and facing (4.3), the pusher removed | VERIFIED (run K: 2590 -> spearman 2592; 3422 -> 3424; 3782 -> 3784) |
| Palette / box | pusher **18** (the spearman palette), (88,176,32,62) | VERIFIED (walk) |

Roots: pusher 0x21700[f] (0x21708 / 0x21734, held), 0x21704[f] (0x21760 /
0x217F2, 4 x 16 loop) (`S/roots.txt`); cart: first pose 0x21CD4[f] (0x21D04 /
0x21DA0), intact 0x21CD8[f][+30 & 3] (0x21D04, 0x21D2A, 0x21D50, 0x21D76 /
0x21DA0..0x21E12), cracked 0x21CF0[f][+30 & 3] (0x21E3C..0x21EAE /
0x21ED8..0x21F4A); each a 4-node x 8 loop, 14 pieces, palette 19
(`S/boss2/roots_boss.txt`). VERIFIED (walk; run K nodes).
Native: an obstacle-pushing pair: a non-hittable pusher at 1 px/update with
random 64-update stops, the cart 48 px ahead (energy 30, solid, spiked), the
pusher becoming a variant-10 spearman.

### 5.4 Stone pillars (class 6 kind 7, 0x318E0)
Stone columns with spiked bottoms over the gap (`vid/v009900`: a pillar dropping
from the top of the screen). They are platforms (class 6) and destructible.

| Item | Value | Tag |
|---|---|---|
| Common | energy **16** (+2A/+2B), probe 10, +10 = 20, palette **19**, box (8,16,48,96) on every pose | VERIFIED (walk) / SOURCE |
| Variant 0 (falling), (3000,832) (3096,832) | every 4 updates: when |x - playerX| < 48: delay 0x31980[rand & 15] = 1, 16, 32, 16, 16, 32, 16, 32, 48, 16, 32, 48, 32, 16, 32, 1 updates; then **falls**: y speed +32/update up to **1792 (7 px/update)**; on a floor (3C48): sound **0xA8**, then stays (phase 6 = rts) | VERIFIED (run B: pillar x 3000 at y 212 -> 142, 10 updates; 2 x 0xA8) / SOURCE (delays) |
| Variant 2 (standing), (2840,672) (3256,672) | taller pose 0x31C60; stands still until destroyed | SOURCE |
| Hit | energy changed: 16-update flash (+30, +0 = 3, toggling +12 every 3), sound **0x32**; below 8 energy: cracked poses 0x31C44 (var 0) / 0x31CDA (var 2) | SOURCE |
| Destroyed | sound **0x33**, ten debris class 12 kind 0 **variant 9** at (x - 15..+16 random, y - 40 + 8 i); after 8 updates it **respawns**: var 0 at y 832, var 2 at y **768** and falls (phase 2) | SOURCE |
| Score / drop | none | SOURCE |

Roots (palette 19): 0x31BF4 (var 0 intact), 0x31C10 (var 0 hit flash), 0x31C44
(var 0 cracked), 0x31C60 (var 2), 0x31C8A (var 2 flash, 2 x 4), 0x31CDA (var 2
cracked). VERIFIED (walk). Native: platforms the player stands on (class 6 rules
as the stage-3 sinking pillars), falling with the above timings.

### 5.5 Troll orb (class 10 kind 15, 0x25A50)
A small green ball (`vid/v003500`). Lifetime **64** updates (+22), motion table
**0xEE4E4**, flies in direction +0F; every second update: wall test (3DDC, probe
from the direction) -> burst; every other update: hit test against the player's
shots (0x75C8, +24) -> deflected/destroyed (pose 0x25B22 table by direction); on
expiry state 6 (removed). Contact hurts (1 HP). **Aim**: +0F is a **64-step
direction** toward the player at the throw (0 up, 16 right, 32 down, 48 left;
Y up); motion 0xEE4E4 holds 64 (x, y) speeds of magnitude **1024 (4 px/update)**
(entry 0 = (0, 1024), 8 = (723, 723), 16 = (1024, 0), 48 = (-1024, 0)). Run B
orbs: player right (dx 98, dy -2) -> 16; left (dx -133) -> 48; down-left (dx -69,
dy -87) -> 40; up-right (dx 124, dy 68) -> 13. VERIFIED (ROM table + 25 orbs of
run B). Roots: 0x25B6E (6 x 4, looping
from the third node, palette **24**, box (3,6,3,6)); 0x25C6A (impact, 7 then
hold, box (8,16,8,16)); 0x261CA (palette 8). VERIFIED (47 orbs in run B, walk) /
SOURCE (motion).

### 5.6 Tower doorway wipe ("gate", class 12 kind 35, 0x32DDC) at (2576, 640)
The upper corridor is entered through a doorway in the **white stone tower** at
the top of a stair (outside view). When the player stands in it, the corridor
**interior is revealed by an expanding rectangle** (a wipe) that replaces the
tower picture around the doorway (render `S/boss2/png/gate.png`: `vidK` 1612
(tower, doorway, stage title), 1624, 1636, 1648, 1660, 1672 (corridor)).

| Step | Behaviour | Tag |
|---|---|---|
| Trigger | player within \|dx\| <= 16 and \|dy\| <= 32 of (2576, 640): **FF83D0 = 1**, **FF83EA = 1** (player frozen; cart pushers do not wake), the object moves to (camX + 176, camY + 128) | VERIFIED (run K: restart at (2560, 640), trigger at key 1516, object (2544, 640)) |
| Frame 0 | at once: a 2 x 2 cell block from 0x32F80's pointer (0x8D6DC) written at scroll2 VRAM **0x9197DC** (CPS scroll2 scan: +4 bytes = one cell down, +64 bytes = one column right) | VERIFIED (renders) / SOURCE (addresses) |
| Frames 1-6 | every **8** updates: table **0x32F7C** entry i = (columns - 1, rows, source pointer): (1,3,0x8D658), (5,5,0x8D5D4), (9,7,0x8D550), (13,9,0x8D4CC), (17,11,0x8D448), (21,13,0x8D404); each frame writes the **outline** of a rectangle one cell larger on every side (two full edge columns of `rows + 1` cells, and the top and bottom two cells of each middle column), the write address stepping **-132 bytes** (2 columns left, 1 cell up; frame 6 differs: one-column edges, +64) | VERIFIED (run K frames at 1516, 1622, 1630, 1638, 1646, 1654, 1662) / SOURCE (layout) |
| End | frame 7 (8 updates after frame 6): FF83D0 = FF83EA = 0, camera min X FF838C = **2368**, object removed | VERIFIED (run K 1670) |
The cell data come from a stored interior block in ROM (0x8D404..0x8D6E3, scroll2
cell words, the same layout); after the wipe, scrolling right streams the corridor
from the stage map as usual. The wipe's frame timer only runs during play (run K
held frame 1 from 1516 until play resumed at ~1614). Native: a centred, growing
rectangle of plane-A cells (7 frames, 8 updates apart, ~26 x 14 cells at the
end) copied from a prepared interior block, with the player frozen; then normal
streaming. Where the player comes from in a natural run (the stair to the tower
door): not traced (run K started at checkpoint 4).

### 5.7 Wall torches (class 12 kind 6 variant 5, 0x333FA)
Eleven torches in the interior. As the stage-2 wall torch (stage2.md 5.8): no
sprite; while on screen it rewrites a 2x2 block of scroll2 cells every **7**
updates, cycling 3 frames (cells 0x0BA6..0x0BBF); removed when FF0FA8 differs
from its +24. VERIFIED (disassembly; torches visible in `vid/v013000`).

### 5.8 Boss fire (class 10 kind 20, 0x26A1C)
See 6.5 (chain count, positions, timing, roots).

---

## 6. Boss: Eborsisk (class 18 kind 6, 0x2B74C)

### 6.1 Records and roles
- Table entry 26: class 18 kind 6 **variant 0** at (2992, 240) (far-layer
  coordinates), created at stage load (class 18 skips the distance filter).
  Phase 0 waits for **FF83EE** (set when the last arena troll dies, 6.2).
  VERIFIED (run B 17018, run M 7836, run W 17018, run MM 7836).
- At the wake it allocates three more class-18 records (0x2A68), sorts the four
  by address and gives them variants **0, 3, 1, 2** in ascending address order
  (bytes 0x2B86C read backwards), links each to the previous (+40) and then sets
  var 1's +40 = var 0 (0x2B85C). VERIFIED (run W: slots in that order).
  Variant dispatch 0x2B75A:

| Var | Role | Energy | Drawn as | Position | Tag |
|---|---|---|---|---|---|
| 0 | controller and **scroll3 neck + head picture** ("far neck") | 0 (never hit: no sprite, no box) | scroll3 tiles (6.4), 4 pictures by +0F | its x, y stay (2992, 240); it owns the **anchor** (+48, +4A) | VERIFIED |
| 1 | **sprite neck** | 1 (6.6) | sprite, root table 0x2C9B4[+0F] (69..74 pieces, palette 26, box (40,136,0,96)) | = var 0's anchor every update (0x2BFFE) | VERIFIED |
| 2 | **head A**, on the sprite neck | 127 -> **FF83CC**, bar row 1 | sprite, 0x2CC2E[facing][mouth] (31..34 pieces, box (40,80,32,64)), live palette **28** (+10) | var 1 position + 0x2C1F0[var 1 +0F] = (-112,+112), (-166,+93), (-221,+74), (-272,+56) | VERIFIED (run W) |
| 3 | **head B**, on the scroll3 neck | 127 -> **FF83EC**, bar row 2 | sprite, same tables, live palette **27** | anchor + 0x2C548[var 0 +0F] = (-152,+112), (-206,+92), (-261,+74), (-312,+56) | VERIFIED (run W) |

Both necks grow from the same anchor; the sprite neck leans less (head A 40 px
to the right of head B at the same pose). Renders: `S/boss2/png/vidW-v017860.png`
(scroll3 neck at pose 3, head B at the far left, the sprite neck and head A in
front), `vidW-v018500.png` (both heads breathing at Willow; two bar rows).

### 6.2 Arena and the troll wave (event 13, 0x3973C)
| Key (run B) | Event | Tag |
|---|---|---|
| 15450 | event 13: player X >= **5584**: four class 4 kind 26 trolls: **variant 9** (+22 delay 1), **8** (16), **9** (192), **8** (224 updates), y 135; FF0F87 = count; FF83A6 = 1; camera min X FF8466 = **5376**; sound **0x0B** (boss music). Each arena troll appears after its delay at **camera X** (variant 9: left edge) or **camera X + 384** (variant 8: right edge) | VERIFIED (births; RAM; sound log) |
| 15908 | arena lock (2.3): player X >= 5760, FF83D0 = 1, mode 14, camera [5376, 5760] | VERIFIED |
| 16748..17018 | the four trolls die (natural in runs B and M); each arena troll's removal decrements FF0F87; at 0: **FF83EE = 1** | VERIFIED |
| 17018 | the boss wakes (6.3) | VERIFIED |

If no troll could be allocated, FF83EE is set at once; on a restart with FF83A6
set no trolls come (1.7). SOURCE.

### 6.3 Wake and rise (variant 0, 0x2B788 / 0x2B888 / 0x2B920)
- Scroll3 cleared (0x6666, every cell tile 0x0A00), layer word = (FF804A & 63)
  | **0x13B8** (scroll3 in front of scroll2), FF8405 = 2, far scroll X = camX -
  5376 + **2496** (= 2880), far Y = **256**; FF0EFC = FF0EFE = 0; records as
  6.1, each +3 = 2, +0E = 2, anchor (+48, +4A) = (6040, 104). Then **FF83CE = 1**
  (bar), **FF83D0 = 1**, **FF838B = 1**. VERIFIED (RAM 17018..17020: cc = 127,
  ce = 1, a8b = 1; regs layer 0x13B8 from 17020).
- Var 0 init (0x2B888): x, y = (2992, 240); +40 = the scroll3 tilemap address
  of (2992, 240) (0x43D4E); +4C = camera X; +22 = 0x97, +23 = 0x1C; pose +0F =
  **1**; action +50 = -1; anchor = (6040, **-152**); bob speed +2C = 512; draws
  picture 1. Head A init: +10 = 28, energy 127, FF83CC = 127, bar row 1 (0x4AC6).
  Head B init: +10 = 27, energy 127, FF83EC = 127, bar row 2 (0x4AA8). Neck init:
  energy 1, action -1, pose 0. VERIFIED (RAM) / SOURCE.
- **Rise** (sub 0, 0x2B920): every update anchor y + 1 and far Y - 1 until far
  Y goes negative: then anchor y = 104, far Y = 0, sub 2. Run W: 17020 -> 17278
  (**258 updates**; anchor y -152 -> 104, far Y 256 -> 0). The heads ride up
  with their necks; nobody attacks during the rise; the scroll3 push (6.6) is
  off (sub 0). VERIFIED.

### 6.4 Motion: anchor, sway, bob, far scroll, pictures
- **Far scroll is pinned to the anchor**: each update far X (FF8488) += the
  camera X change (0x2BB0C) and moves opposite to the anchor's x steps, far Y
  (FF848C) moves opposite to the anchor's y; scroll3 registers FF8036 / FF8038 =
  far X / far Y & 2047. Hence **far X = camX - anchorX + 3160** (within 2 px,
  camera sampling) and **far Y = 104 - anchorY**: a scroll3 picture pixel P
  appears at world x = P + anchorX - 3160. VERIFIED (run W per update, e.g.
  17424: camera 5692, anchor (5895, 104), far (2959, 0)).
- **Sway** (0x2BA9E, state +5), skipped while **FF0EFC** (a breath) is set:
  state 0: anchor x - 1 per update until it falls below **5768**, then state 2:
  + 1 per update until above **6040**, then state 0 again (a 272-px swing at
  1 px/update). VERIFIED (run W: 6040 -> 5895 by 17424; paused 17424..17460
  while head A breathed).
- **Bob** (0x2BB2E, state +53; runs every update in subs 2/4/6), a **48-update
  cycle**: state 0: speed +2C (512 at the start) - 32 per update, far Y long +=
  speed << 8 (8.8 px), anchor y follows (-1 px per px): the picture and the
  anchor drop **15 px** over 16 updates; state 2 (16 updates): **screen shake**:
  +5A = +-2 negated every update and added to camera Y (FF840C) and anchor y
  (camera Y alternates -2 / 0, 2.3), then the anchor y is restored; state 4:
  speed 0 -> 512 by +32 per update, far Y back down 15 px (anchor y back to
  104). VERIFIED (run W: bob states at 17278, 17295, 17311, 17327 ...; anchor y
  104 <-> 89; camera Y 65534 in state 2).
- **Pictures (var 0 +0F, 0..3)**: 0x2C732 redraws the scroll3 neck whenever the
  pose changes: tile list **0x2C7B4**[+0F] (word offsets) -> 0x2C7BC (pose 0),
  0x2C83A (1), 0x2C8B8 (2), 0x2C936 (3), 63 words each; tile code = word +
  **0x0A00** (word 0 gives 0x0A00, the blank tile 0x6666 clears with),
  attribute word **0x001F**; written from the base +40 in **8 rows** of 32x32
  tiles (row r one tile row below row r - 1) holding **4, 8, 10, 10, 9, 6, 8, 8**
  tiles (0x2C79C + 1), starting at column **0, -2, -4, -4, -3, +1, +1, +1**
  relative to the base column (row start offsets 0x2C7A4 = 0, -60, -120, -116,
  -80, 52, 56, 60 bytes in the CPS scroll3 scan: +4 bytes = one tile down, +32
  bytes = one tile right), each row left to right (+32 bytes). Pose 0 = coiled
  upright (rest of the sprite neck's twin), 3 = fully stretched left. SOURCE
  (disassembly) / VERIFIED (four pictures in the renders: dump keys in 10).
- Var 0 rests at pose **1**, the sprite neck at pose **0**.

### 6.5 Attacks: decisions, stretch, fire breath
Decisions (var 0 every **59** updates (+22 reloaded with 59), 0x2B956; var 1 every **64**, 0x2BC80;
only in their idle sub 2 with action -1):
- Distance band d1: var 1: 0x2BD12[|playerX - anchorX| >> 4] (0 for < 128 px,
  16 for 128..223, 32 for 224..319, **48** beyond 320); var 0: 0x2BA00[|playerX
  - 2992| >> 4] but its x stays 2992 (far-layer coordinates), so d1 is **always
  48**. VERIFIED (code; run W bands).
- Action = table[rank row][d1 + (rand & 15)], table **0x2BD6E** (Willow) or
  **0x2BE76** (Madmartigan: FF9866 != 0), rank row = word index (FF8346 >> 5) &
  6 (4 rows of 64 bytes; dumped in `S/boss2/boss_tables.txt`): **-1** nothing, **0/1/2**
  breath with fire-chain type 0/1/2, **3** stretch without fire. Over all 256
  entries: Willow -1 x28, 0 x82, 1 x61, 2 x20, 3 x65; Madmartigan -1 x56, 0 x2,
  1 x42, 2 x90, 3 x66 (against Madmartigan the boss idles more and breathes the
  short 6-flame chains; against Willow the long 11- and 9-flame ones). SOURCE
  (ROM).
- Breath actions: the neck drops to pose 0 (var 0 redraws picture 0); reach
  +51 = 0x2BD26[rand & 7] = 1, 0, 1, 0, 0, 2, 1, 0. Reach 0: breathe at once
  (pose 0). Else the neck stretches **one pose per tick** (59 updates for var 0,
  64 for var 1; +0F 0 -> 1 -> 2) and sets the breath flag (+54, action copied to
  +5C) when +0F == reach; then it retracts one pose per tick to its rest pose.
  Action 3: stretch to pose 3, then retract; the head's mouth follows the pose
  (0x2C104[pose] = 2, 1, 1, 0) but no fire. VERIFIED (run W: e.g. var 0 action 3
  at 17724: poses 1, 2 (17783), 3 (17842), 2 (17901), 1 (17960), 0 (18019), idle
  18078; var 1 action 1 breath at 17147, 17403, 17915) / SOURCE (tables).
- **Breath** (head sub 4 / 6, 0x2C108 / 0x2C1A4), started by its neck's flag:
  mouth +2E 0 -> 1 -> 2 at 10-update steps; at mouth 2: **sound 0x8D**,
  **FF0EFC = 1** (sway stops), the fire chain (class 10 kind 20, +2E = action =
  chain type, +0F = head facing, +0E = 0 for head A / 2 for head B) at the head
  + 0x2C198[facing] = (-32,-24), (-16,-32), (0,-40); mouth stays 2 for **20**
  updates, 1 for 10, then 0 and idle; the neck's flag is cleared: **50 updates**
  per breath. VERIFIED (run W: head A 17147 (mouth 0), 17157 (1), 17167 (2, chain
  at 17168), 17187 (1), 17197 idle).
- Head facing (+0F 0..2: 2 = mouth down-left, 0 = level left): set every 64
  updates while idle and at the breath from the direction to the player (0x20D2:
  sector (dir + 8) >> 4: 11 -> 1, above 11 -> 0, below 11 -> 2). Runs W and MM:
  always 2 (player below). SOURCE / VERIFIED (2 only).
- **Fire chain** (0x26A1C): flame count 0x26B96[type] + 1 = **11 / 9 / 6** for
  type 0 / 1 / 2 (fewer if the pool is short, FF980A); all flames created in the
  same update at fixed positions = mouth + 0x26B0C[facing] offsets:
  facing 0 (level): (0,0) (-8,0) (-16,0) (-32,0) (-56,0) (-80,0) (-112,0) (-144,0)
  (-176,0) (-216,0) (-264,0); facing 1: (0,0) (-8,-4) (-16,-8) (-32,-16) (-56,-28)
  (-80,-40) (-112,-56) (-144,-72) (-176,-88) (-216,-108) (-256,-120); facing 2
  (steep): (0,0) (-8,-8) (-16,-16) (-32,-16) (-48,-32) (-64,-48) (-88,-56)
  (-112,-80) (-136,-104) (-168,-120) (-200,-152). Each flame waits hidden (+0 =
  2) until the previous one starts burning, then 4 updates (sub 2) and burns
  **24 updates** (+0 = 1: hurts on contact; 7 nodes x 4 of its root), then is
  removed: flame i burns from about spawn + 7 + 4i to spawn + 31 + 4i (the
  stream grows away from the mouth one flame per 4 updates, and dies back the
  same way). The last flame clears
  FF0EFC when it starts burning. Roots 0x26C30[i]: flames 0-2 0x26C46, 3-5
  0x26C9E, 6-8 0x26CF6, 9-10 0x26D4E (palette 8, boxes growing from (2,4,8,9) to
  (16,26,8,24)). VERIFIED (run W 17168..17230 per update; run B chains) /
  SOURCE (count table).

### 6.6 Being hit, shields, the push wall
- All four records are boss damage class 1 (combat.md 7.1: kind 6): a shot or
  sword overlapping a record with **+0 == 1** subtracts its power and sets +0 =
  2; a record with +0 == 2 **deflects** the shot. VERIFIED (runs W, MM).
- **Sprite neck = shield**: var 1 (energy 1, box (40,136,0,96)) takes the
  first hit that reaches it, energy 0, and its routine never resets +0: it stays
  **+0 = 2 for the rest of the fight and deflects every shot** that overlaps it.
  VERIFIED (run W key 17214, run MM 8024).
- **Heads**: vulnerable whenever +0 == 1 (no armoured phases; breathing,
  stretching or idle). An energy change (0x2C562 / 0x2C602): its bar row redrawn,
  **sound 0x99**, **+5D = 48**: +0 = 2 for 48 updates, then +0 = 1 (hittable the
  next update; run W hits every 49-50 updates at best), and a **palette flash
  job** (0x2C86, pool FFF042): data 0x2CF50 (head A, palette 28) / 0x2CF10
  (head B, palette 27), +3B = 13 steps every +3A = 4 updates: a near-white set
  (0x0FFF...) alternating with the normal colours (0x2CF70 / 0x2CF30). Damage
  per hit = the attack's power: **1** for Willow's level-0 shot and for
  Madmartigan's level-0 sword (runs W, MM). The heads hang at y ~145..216 (the
  player stands at 127 on the bridge): the player must jump or let a head dip.
  VERIFIED (run W / MM hits; palscan o27 / o28 after hits) / SOURCE (job data).
- **Bars**: two rows under ENEMY: FF83CC (head A) at scroll1 0x910310 (0x4AC6)
  and FF83EC (head B) at 0x910314 (0x4AA8), length (energy - 1) >> 3 cells (tile
  0x7794): **each bar follows its own head**; a dead head's row is empty. VERIFIED
  (render `vidW-v018500`: two rows; run MM: FF83CC 0 after head A died, FF83EC
  counting down).
- **Push wall** (0x2C63A, var 0, subs 2..6, not with service invincibility
  799E): if player x > anchorX + 0x2C68A[var 0 pose][(playerY >> 5) & 7] (pose 0:
  -128, -128, -96, -64, -64, -96, -96, -96; pose 1: -128, -128, -96, -96, -96,
  -128, -128, -128; pose 2: -128 x4, -192 x4; pose 3: -128 x3, -192, -256, -256,
  -128, -128): player x -= 4 and, if the player is +0 == 1, **1 damage**
  (3A66C). Every 8 updates (0x2C6CA) class-8 player shots of the kinds marked in
  0x2C728 (1, 2, 3, 5, 7, 8) beyond the same line are ended (+0 = 2, state
  0x0402). The scroll3 body is a hurting wall on the right. SOURCE.

### 6.7 Death
| Step | Behaviour | Tag |
|---|---|---|
| First head (FF0EFE = 0 -> 1) | +22 = **180** updates: every 7 updates an explosion class 12 kind 0 variant 4 at the head + 0x2C352[i & 7] ((0,-8), (40,8), (-40,-32), (24,-32), (-24,8), (24,-40), (-40,0), (22,-12)), sound **0x33**; then **+10,000** (0x180C twice) and the head is removed; its neck stays (the sprite neck keeps moving and keeps deflecting shots) | VERIFIED (run MM: head A energy 0 at **14878** naturally (Madmartigan), removed 15060; run B head var 3 at 17324 -> 17508) |
| Second head (FF0EFE = 1) | **FF83E9 = 1** (clock stop), reward list **9** at (camX + 288, head y): **15 pickups** = 10 x variant 7 (500), 4 x variant 8 (1000), 1 x variant 10 (5000) = **14,000 gold**; FF83CE = FF83D0 = FF838B = 0, **FF83CF = 1**, FF0EFE = 2 | VERIFIED (run B 18226..18228; births of 15 class-14 pickups) |
| Collapse | +22 = **240** updates: every 3 updates two explosions (offsets 0x2C470 / 0x2C352), sound 0x33; for the last 120 updates the body anchor sinks **1 px/update** in x and y | VERIFIED (explosion bursts 18230..18460) |
| End | **FF83C3 = 2** (results, no exit timer), **+10,000** | VERIFIED (FF83C3 = 2 and score +10,000 at 18468) |

Total for the boss: 20,000 points and 14,000 gold. Either head may die first.
Run B: second head zeroed at 18226 -> results 18468 = 242 updates. VERIFIED.

### 6.8 What is not there; native notes
No dialogue; no camera glide; the boss music starts at event 13, not at the
wake; no armour phases or vulnerability windows beyond the 48-update post-hit
blink. VERIFIED (traces).
Native: plane B (the far layer) shows the scroll3 neck picture (4 prepared
pictures of 8 x 32-px tile rows; scrolled by anchor-relative X/Y every update
so it moves rigidly with the anchor); the sprite neck and both heads are
sprites (palettes 26, 28, 27); per-update anchor rules: rise (258 updates),
sway (+-1 px, 5768..6040, paused by a breath), bob (48-update cycle with a 16-
update 2-px camera shake); decisions every 59 (far neck, band 48) / 64 (sprite
neck) updates from the per-form rank tables; 50-update breaths with 6..11-flame
chains; 1 damage per level-0 hit, 48-update blink, two bars; the sprite neck
becomes a permanent shot-deflecting shield after its first hit; the far body
pushes and hurts the player right of its line.

---

## 7. Stage clear, results and story (run C, natural, no input)

### 7.1 Clear and results
Run C injected FF83C3 = 2 at t 1700 (play, TIME 5:27).

| Key (run C) | Step | Event | Tag |
|---|---|---|---|
| 1704 | 4 | results text **"STAGE5 / CLEAR POINTS"** | VERIFIED (render `vidC/v001712`) |
| 1708..3344 | 8 | count: TIME s x **500** and LIFE 8*(HP+armour) x **500**, sound **0x30** each update (1,636 in run CS); run C: TIME **327 x 500**, LIFE **24 x 500 = 12,000** | VERIFIED (render `vidC/v003338`: TIME327x500 counting, LIFE 24x500=12000; **factor 500 confirmed**) |
| 3344..3532 | 10..12 | 120 updates + fade | VERIFIED |
| 3532 | 16 | sounds **0xF4, 0x08FB**; story backdrop scroll2 at **(2560, 256)** (0xAC46[4]): **General Kael** in his skull helmet with spiked pauldrons, on black; page count 0xA8AE[4] = **0** (one page); no palette controller | VERIFIED (render `vidC/v003542`) |
| 3584..~3990 | 18 | the page typed (style 0x031B, 3 frames/char, rows 24-27, purple text), held, fade | VERIFIED (renders `vidC/v003758`, `v003812`, `v003968`) |
| 4004 | map | stage index -> **5**; stage-6 map scene (marker VI) | VERIFIED (render `vidC/v004100`) |

Story page (table 0xA8EA + 4*4 = bytes **46 00 00 00**; record 0x70000 + m * 168,
ASCII, `&` = line break, `/` = end):
- message **0x46** (ROM **0x72DF0**): "IT IS AMAZING HOW ALL OF YOU WERE ABLE&TO
  COME THIS FAR WITH YOUR POWERS.&HOWEVER, YOUR LIFE ENDS HERE."
VERIFIED (ROM string + renders).

### 7.2 Notes for the native game
- Results: the shared results screen with stage number 5 and **factor 500**.
- Story: plane-B picture of the story grid at (2560, 256) (the same story grid as
  stage4.md 7.2's (1536, 256)), no palette cycle, one page in the text band (cut
  3), 40 columns with `&` breaks.
- Then stage 6 (index 5): its map, music word 12 (0x0C + 0x01FA), its own title.

---

## 8. Shops, items, hidden objects

### 8.1 Chests (class 16 kind 4, energy 6)
Rewards: table 0x37EA6 + word for index 4 = **0x37F52**: 4 sets x 5 entries
(kind, variant); kind 5 = gold object (0 = 1, 5 = 100, 6 = 200 gold), kind 6 =
heart; set = FF83A4; chest index = the table direction byte. VERIFIED (ROM).

| Set | chest 0 (4720,240) | 1 (4416,320) | 2 (4816,480) | 3 (1472,176) |
|---|---|---|---|---|
| 0 | heart | gold v0 (1) | gold v5 (100) | gold v6 (200) |
| 1 | gold v0 | gold v5 | gold v6 | heart |
| 2 | gold v5 | gold v6 | heart | gold v0 |
| 3 | gold v6 | heart | gold v0 | gold v5 |

Chest 3 is on the tower balcony (`vid/v004990`), the others in the interior.

### 8.2 Shop doors (class 12 kind 8)
(1008, 296) variant 0 direction 11 (tower foot) and (4816, 648) variant 16
direction 12 (interior top, next to chest 2). Contents: shop spec (direction =
shop list). Not entered in runs B/M (no 0x10FB / 0x1B in run S before the clear).
UNKNOWN (inventory for lists 11 and 12; how the door looks: render a run that
goes in).

### 8.3 Hidden object, music triggers
None in this stage. VERIFIED (object table).

---

## 9. Sounds in stage 5 (run S unless noted; `jsr 0x786A`)

| Word | Where |
|---|---|
| 0xF0, 0xF7, 0x12 / 0xF0, 0xF7 | world-map scene start / end (runs AS/CS) |
| 0x2F | select prompt (21), title (23) (runs CS/AS) |
| 0x0A + 0x01FA | stage music (play set-up) |
| 0x0B | boss music (event 13, x 5584) |
| 0x9B | troll hit (86) |
| 0x9C | troll death cry (6) |
| 0x9D | troll throws an orb (47) |
| 0x32 / 0x33 | hit / death burst (79 / 164) |
| 0x34 | arrows (67; only 18 arrows were shot: other source UNKNOWN) |
| 0x8B | spear throw (1) |
| 0xA8 | stone pillar lands (2) |
| 0x8D | boss fire breath (5; one per breath, at mouth 2, 6.5) |
| 0x99 | boss head hit (0x2C58E, on every energy change of either head; SOURCE: runs W / MM logged no sound words) |
| 0x2D | pickup (33) |
| 0x10 | stage-clear jingle (1, at the boss's end, run S) |
| 0x30 | results count tick (1,636 in run CS) |
| 0xF4, 0x08FB | story page |
| 0x0C + 0x01FA | stage-6 music (after the stage-6 map) |
| 0x43 | clock warning under 0:16 (not heard) |

Player sounds (0x20 x641, 0xF4 charge x425, 0x26, 0x2B, 0x31, 0x3B, 0x2A):
player.md / magic.md / madmartigan.md. One 0x10FB in run S belongs to stage 6.
VERIFIED (sound word log; order).

---

## 10. Dump files for the pose / plane / far-layer packers

All under `S/` = `/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/stage5/`.
`.cps` = lockstep VIDDUMP (0x80 bytes CPS-A/B registers + video RAM, live
palettes included); render with `S/render.sh DIR KEY...` or `S/montage.py OUT.png
DIR KEY...` (scroll3 only: `CPSPLANE3=1`, sprites only: `CPSOBJONLY=1` with
cpsframe.py). Run B dumps every 10 keys (1100..22400), run A every 6 from 3400
(keys = 3400 + 6n), run C every 6 from 1700 (keys = 1700 + 6n, i.e. = 2 mod 6),
run M every 20 (Madmartigan). RAM dumps `ram/rKEY.bin` (every 2), `ramA`,
`ramC`, `ramM` (every 4).

No palette changes during play (2.6): any play dump gives every live palette.

Scenes, planes and far-layer pictures:

| File | Shows |
|---|---|
| `vidA/v003610.cps`, `v003706.cps` | world map for stage 5 (both heroes at marker V) |
| `vidA/v003778.cps`, `v003814.cps`, `v003850.cps`, `v003970.cps`, `v004090.cps`, `v004142.cps` | select screen: empty window, prompt typing, both heroes and names, TIME 10/06/02/00, Willow chosen |
| `vidA/v004300.cps`, `v004366.cps` | title STAGE5 / TIR ASLEEN CASTLE |
| `vid/v001200.cps`, `v002500.cps` | outer wall; far blocks 23/25 (far y 0) |
| `vid/v005500.cps`, `v005900.cps`, `v007500.cps` | wall slopes, camera Y ~300..370: far rows 15 and 14 (blocks 22..27) |
| `vid/v008400.cps`, `v009300.cps`, `v010300.cps`, `v010500.cps` | the gap: far layer fully visible (blocks 23/25/27, 22/24/26), black void below |
| `vid/v012000.cps`..`v014000.cps` | interior (far layer hidden; torches animating) |
| `vid/v015000.cps`, `v015900.cps` | battlements, red sky (scroll2 blocks 91-94), arena |
| `vid/v017100.cps`..`v018460.cps` | boss fight (layer 0x13B8); scroll3-only renders `png/p3_017200.png`, `p3_017400.png` |
| `vidC/v001712.cps`, `v003338.cps` | results STAGE5 CLEAR POINTS, TIME327x500, LIFE 24x500 |
| `vidC/v003542.cps`, `v003968.cps` | story backdrop (Kael) and the full page |
| `vidC/v004100.cps` | stage-6 world map |
| `S/map/s5.ppm` | the assembled foreground rows 5-7 (6144 x 768, arcade colours) |

Families (pose packer, run B `vid/vKEY.cps` unless noted):

| Key | Shows (palette) |
|---|---|
| vidA 3850..4142 | select-screen Willow (0) and Madmartigan figures, cursor frame (8) |
| 1200..2500 | placed soldiers (16) |
| 2700..3150, 4990..5100 | trolls (24) walking, leaping, out of windows; grapple at 3080 |
| 3220..3530, 5700..6700 | troll orbs (24) |
| 4990 | chest 3 on the tower balcony |
| 7020..8350 | archer on the wall top (17), arrows |
| 9570..9580, 9900..9920; vidM 4860..4920 | stone pillars (19) falling |
| 12000..13850 | bats (22) hanging/flying; torches |
| 12400..14600 | interior spearmen (18), archers (17) |
| 15460..17010 | arena trolls (24) entering from both edges |
| 17020..18220 | Eborsisk: sprite neck/head (26, 27, 28), scroll3 neck/head; fire chains at 17170..17230, 17430..17480, 17680..17740, 17940..17990, 18190..18260 |
| 17330..17500, 18230..18460 | head death explosions, gold shower (18228..), body sinking |

Natural boss fight (run W `S/boss2/vidW/vKEY.cps`, every 10 keys 17000..23990;
run MM `S/boss2/vidMM/`, every 20 keys 7800..15980):

| Key | Shows |
|---|---|
| vidW 17020..17270 | the rise (far Y 256 -> 0), both necks and heads coming up |
| vidW 17490..17540 (scroll3 picture **0**), 17280..17330 (**1**, rest), 17790..17840 (**2**), 17850..17900 (**3**) | the four scroll3 neck pictures (var 0 +0F); scroll3-only renders with `CPSPLANE3=1` |
| vidW 17280.. (neck pose 0), 18180..18230 (**1**), 18240..18290 (**2**), 18300..18350 (**3**) | the four sprite-neck poses (var 1 +0F, root table 0x2C9B4) |
| vidW 17280.. (mouth 0), 17420 / 17450 / 17670 (head A mouth **1**), 17430 / 17440 / 17680 (head A mouth **2**), 17560 / 17590 (head B mouth 1), 17570 / 17580 / 18160 (head B mouth 2) | head poses, facing 2 (facings 0 / 1 never occurred: player always below) |
| vidW 17170..17230 | head A's 9-flame chain growing and dying back (type 1, facing 2) |
| vidW 18220..18520 | hits: palette 28 / 27 flashing (white set), both bar rows going down |
| vidW 17300.., e.g. 17450 | camera shake (camera Y -2) during the bob |
| vidMM 14880..15060 | head A's natural death (explosions), the sprite neck staying |

Upper corridor (run K `S/boss2/vidK/vKEY.cps`, every 6 keys 1600..7594):

| Key | Shows |
|---|---|
| vidK 1600..1612 | white tower, the doorway with Willow (checkpoint 4 restart), stage title |
| vidK 1618..1672 | the interior wipe, frames 1..7 (`S/boss2/png/gate.png`) |
| vidK 2122..2200 | the corridor (green stone, dark recesses), first cart pusher and spiked cart arriving |
| vidK 2400..2580 | cart against the player, cart flashing (hits), cracked cart (energy < 10) |
| vidK 2584..2596 | cart burst; pusher turning into a spearman variant 10 |
| vidK 2150.., 3030.. | spearman variant 16 entering at the left edge |
| vidK 2900..3430, 3500..3790 | second and third pushers |
| vidK 6900..7100 | corridor end and the drop into the interior top (x ~4072) |

Not in any dump (never on screen): the shop doors' interiors, spearman variant
14 (interior balconies). Corridor pushers, carts, the wipe and spearman variants
10 / 16: run K above. Pose roots for the boss, flames, cart, debris and the
troll's 0x21464: `S/boss2/roots_boss.txt`.

---

## 11. Open questions (UNKNOWN), where to look

Priority for the native stage:
1. **Boss, remaining**: resolved by runs W / MM (section 6): roles, motion,
   decisions, breaths, chains, hits, bars, shield, push wall, natural head
   death. Still open: the second head's natural death in a natural fight (both
   runs ran out of frames; the KILLBOSS path of runs B / M covers it), head
   facings 0 / 1 (player above / level with the heads; 0x20D2 sector rule
   SOURCE only), and the scroll3 picture layout checked cell by cell against a
   dump's scroll3 map (6.4 is from the code).
2. **Upper corridor, remaining**: resolved by run K (5.3, 5.6, 4.3). Still open:
   how the player reaches the tower doorway at (2576, 640) in a natural run (the
   stair outside; run K restarted at checkpoint 4) and whether the corridor or
   the pillar gap below is the intended route (both lead to the interior top);
   the class-6 contact damage of the spiked cart (the player was invincible).
3. **Troll details, remaining**: no grapple exists (5.1: wall-clinging window
   trolls); orb aim VERIFIED (5.5); hop distance and knock-back VERIFIED. Still
   open: the phase-selection table and the +39 aggression timer (k26.dis
   0x2015C..0x2069x), the window trolls' climb speed and drop rule, the arena
   trolls' entry walk.
4. **Pillars as platforms**: confirm the player can stand on falling/landed
   pillars and that var 2 stays until destroyed; where var 0 lands in the gap
   (run B: y ~142..303); whether a pillar falling on the player hurts (spiked
   bottom).
5. **Collision ids** of grid 4 (spike rails in the interior, slopes, ledges):
   census with `stagemap.py 4 5 3` / `colpack.py`.
6. **Select screen**: two-player behaviour (does each player choose?), where
   FF83A3 is cleared for a new game, the map-scene hero pair table entries
   (0x35268[4] / 0x35280[4]), the exact flag that holds the countdown while the
   prompt types.
7. **Grid 4 row 1** (blocks 51..59 at y 1536..1791): unused in play? (maybe the
   stage-6 or ending picture).
8. Bats not retiring at x > 4096 here (stage4.md 5.2 rule): read 0x22292's
   retire test.
9. (Resolved: camera Y -2 is the boss's bob shake, 6.4.)
10. Shop inventories for door directions 11 and 12.

---

## 12. The player: what stage 5 adds beyond player.md / magic.md / madmartigan.md

1. **The select screen chooses the form** for stages 5 (and the start of 6):
   FF8386 = 0 (Willow, default) or 4 (Madmartigan), set by Left / Right on the
   select screen, confirmed by a button or the 10-count timeout (1.3). The player
   record takes the form at play set-up (0x2F08) and **Madmartigan starts 8 px
   higher** (y 80 vs 72). The native game needs both player controllers available
   in this stage (Willow's magic and Madmartigan's sword), with the same level
   data. VERIFIED.
2. **FF83EA (player freeze)** while the tower-doorway wipe runs (5.6, about 56
   updates) and checked by the cart pushers (5.3). VERIFIED (run K).
3. **Platforms**: falling stone pillars (class 6 kind 7) over a pit, and the
   one-way camera there (2.3): the player must ride/jump pillars that drop when he
   comes within 48 px. Both forms can destroy them (energy 16) and they respawn.
   SOURCE.
4. **No troll grapple** (5.1): nothing holds the player; trolls hurt by
   contact and orbs only. VERIFIED (run B).
7. **Boss contact rules**: the scroll3 body pushes the player 4 px left and
   hurts him right of its line (6.6); flames hurt only while burning; head and
   neck sprites hurt on contact (class-18 body, combat.md 2.1). SOURCE.
5. Restart in the arena (checkpoint 18): x 5472 (bit-15 record + 96), camera min
   X 5376, boss music 11, no troll wave if FF83A6 is set (1.7, 6.2). SOURCE.
6. Run M showed the stage-6 start keeps form 4 and later switches the form to 0
   (FF8386 0 at key ~12312 in stage 6): the stage-6 spec must cover that hand-over.
   VERIFIED (run M RAM) / UNKNOWN (cause).
