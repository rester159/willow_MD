# Stage HUD behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), main program `build/reference_maincpu.bin`
(sha256 0b66efe8...). Behaviour spec for the native Genesis rewrite
(NATIVE_ENGINE_PLAN.md sections 2 and 6; approved cut 2 in AGENTS.md: a native
HUD drawn from game values, original font, Genesis layout). Arcade addresses
are citations only. Same evidence tags as `player.md`:
**VERIFIED** (ROM/disassembly read here plus the transliteration in
`native/engine/src`), **SOURCE** (one source), **UNKNOWN** (not established).

RAM below is arcade work RAM (`a5` = FF8000). It tells an offline tool or a
trace where to read a value; the native game keeps its own plain variables
(section 6).

---

## 1. Arcade text layer (scroll1) basics

| Item | Value | Tag |
|---|---|---|
| Layer | CPS scroll1, 8x8 cells, map at 0x910000. Cell (col,row) word pair (code, attribute) at `0x910000 + col*0x80 + row*4` (column-major, 32 rows per column) | VERIFIED (routines 4F54, 5BAA; B1 `s1text.h` decodes the same) |
| Visible area | columns 0..47 (384 px), rows 2..29 (224 lines). Screen pixel = (col*8, (row-2)*8). Rows 30-31 are off-screen | VERIFIED (B1 s1text row/column mapping displays correctly) |
| Glyph code | `0x7000 + ASCII` for text (digits 0x7030-0x7039 and 0x7000-0x7009 are the same pixels; numbers use 0x7000+digit). Blank in the top rows = 0x7020 (space); blank in the bottom bar = 0x7120 | VERIFIED (string printer 5BAA adds 0x7000; number printer 56E4; tiles decoded and compared) |
| Attribute | low 5 bits = scroll1 palette 0..31; bits 5/6 flips (unused by the HUD) | VERIFIED |
| Palettes | 32 scroll1 palettes, ROM **0xE6EA0** (32 x 16 words, CPS 0x0RGB, OR 0xF000 on load), loaded to palette RAM 0x91C400 (routines 76C/91E). Fixed for play; an alternate set 0xE76A0 is loaded by 0x94E (non-play scene). Pen 15 = transparent | VERIFIED |
| Font graphics | CPS GFX region assembled by `tools/cps1_gfx.py assemble_region(willow.zip)`; glyph code c at byte offset `c*64` (8x8 scroll1 layout: two 8x8 halves of a 16x8 cell, `decode_tile(gfx, c, 8, layout)` with layout = column parity). Codes 0x7000..0x7B7F hold all text and HUD art. `build/.../b1/mks1atlas.py` already extracts exactly this range | VERIFIED (decoded here: '0' 0x7030, 'A' 0x7041, heart cells, bars) |

Text palettes used by the HUD (CPS 0x0RGB, pens 2,3,4,6,7 are the glyph shades,
pen 7/8 outline/white):
- palette 0: light blue text (0A9F, 0BAF, 098F, 087F, dark 0408), pen 8 = white 0FFF;
- palette 2: yellow (0FD0, 0FF0, 0FB0, 0F90, dark 0B00);
- palette 3: orange-red (0F50, 0F60, 0E40, 0D30, dark 0700);
- palette 0x10: vitality/boss cells: full = orange/red (pens 4-6 = 0F94, 0D50, 0C00),
  empty interior = black 0111, armour = green (0C/0D/0E = 07F7, 00A0, 0060),
  boss bar = blue (0A/0B = 006D, 00AF, 07 = 000C), frame greys 0999/0666;
- palettes 0x11..0x1D: charge bar fill colours (section 3.6); 0x13/0x14 inventory icons.
VERIFIED (ROM dump of 0xE6EA0).

## 2. Layout (arcade cells)

Static labels are text string 12 of the string table **0x5C7E** (format: x, row*4,
attribute, ASCII bytes, `*` = next segment, 0 = end), drawn by text job 12 when
the HUD is (re)built (4F54 queues it). VERIFIED (decoded):

| Label | Cell | Palette |
|---|---|---|
| `1UP` | (1,2) | 0 |
| `HI` | (19,2) | 0 |
| `2UP` | (36,2) | 0 |
| `VITAL` | (1,3) | 0 |
| `GOLD` | (19,3) | 2 |
| `TIME` | (36,3) | 3 |
| `:` | (42,3) | 3 |
| `P=` | (41,26) | 0 |
| `ENEMY` (string 14, only with a boss) | (1,4) | 0 |

```
row 2 : 1UP 12345678   HI 00055000      2UP 00000000
row 3 : VITAL [][][]    GOLD 000000      TIME 3:00
row 4 : ENEMY ====== (boss only)
row 5 :       ====== (second boss bar, stage 5 only)
row 26:                                          P=2
rows 28-29: |=======charge bar=======|  [inventory icons x9]
```

## 3. Elements

### 3.1 Score (1UP / 2UP)
- Meaning: player score, 8 BCD digits (4 bytes). RAM **FF834A** (current player);
  the other player's saved score is **FF82CA** (copy of the player block at FF82C2).
  VERIFIED (6284, 6300).
- Drawn at (4,2) for player 1 and (39,2) for player 2 (FF8292 = current player is
  P2 swaps the two), 8 cells, leading zeros blank, last digit always shown
  ("       0" at zero). Palette 0. VERIFIED (62A0..62FA, 56E4/5720).
- Changes when an award is added (6274 queued / 6284 direct, BCD add). On BCD
  overflow past 99999999 the add is discarded (score stays). The award table is
  0x624E (word offsets to 4-byte BCD amounts): 50, 100, 200, 500, 1000, 2000,
  5000, 6000, 8000, 10000, 20000, 30000, 100000, 1000000 (indices 0..26 step 2).
  Queued awards are suppressed while the demo sound gate FF8293 is set. VERIFIED.
- The attribute's high byte is inherited from the caller's register (only the
  low byte is set to 0): priority bits only, colours are palette 0. SOURCE
  (score.h comment); treat as palette 0.

### 3.2 High score (HI)
- RAM **FFF03E** (4 BCD bytes), drawn at (21,2), palette 0, same digit rules.
  Updated and redrawn on any score add that makes score >= high score.
  Initialised from the ranking table: default top entry **55000** (ROM table
  0x183C: 10 entries, 55000 down to 10000, names "CAP"/"COM"). VERIFIED (1466..1482, 62BA).

### 3.3 Vitality (hearts)
- RAM: HP **FF8362**, max HP **FF8363**, armour **FF8364**. VERIFIED (4A34).
- Drawn from (6,3) rightwards (12 cells cleared with blank 0x7020 first):
  HP cells of **0x7784** (full, orange), then (maxHp-HP) cells of **0x7783**
  (empty), then armour cells of **0x7793** (green). Palette 0x10. VERIFIED.
  (The "hearts" are square gem cells, not heart shapes.)
- Range: maxHp 2..5 at start (DIP), raised by the shop to at most 6; HP 0..maxHp;
  armour 0..6 (three armour items worth 1, 2, 3). SOURCE (shop.h grants).
- Changes: each hit (player.md 3.10) costs 1 armour point if any, else 1 HP;
  shop heals (+1 or +2, capped at maxHp) and max-HP purchase; continue and new
  game set HP = maxHp (2F00). Redraw on every change (4A34). SOURCE / VERIFIED.

### 3.4 Gold
- RAM **FF8368** (4 BCD bytes); bytes 1..3 (6 digits) shown at (24,3), (26,3),
  (28,3), palette 2 (yellow), leading zeros blank, last digit always shown.
  VERIFIED (4E32).
- Changes: pickups add an amount from table **0x4E70** (4-byte BCD, index*4):
  1, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 (4E12). An add that
  overflows 8 digits is discarded. Shop purchases subtract. VERIFIED / SOURCE.

### 3.5 Time
- RAM **FF83C6** minutes (BCD), **FF83C7** seconds (BCD), **FF83C8** frame divider
  (59..0). Drawn: minutes 2 digits at (40,3) (leading zero blank), seconds 2
  digits at (43,3) (both shown), palette 3; `:` is the static label. VERIFIED (4EA2..4F40).
- Ticks once per 60 updates (divider reload 59). Stops while FF83D2 (magic
  freeze, magic.md spell 10), FF83E9 or FF83CF (scene locks) is set, or while
  FF83E2 >= 2. Clamped at 0:00. Below 0:10 (word < 0x0010) a warning sound
  **0x43** plays every 32 frames (when the shared frame counter & 31 == 0),
  unless FFF99E is set. VERIFIED (4EA2).
- Start value per stage checkpoint from the table **0x304C** (stage index ->
  8-byte records x, y, time BCD word, music): stage 1 = **3:00** at every
  checkpoint; stage 2 3:30 (3:00 last); stage 3 4:30 (4:00 last); stage 4 5:30;
  stage 5 5:30/5:00/4:30; stage 6 uses 0x3560 by section. Reloaded on each
  (re)start at a checkpoint (2E9C). VERIFIED (ROM dump).
- At 0:00 the play loop (9536/9606) restores the player palette, plays sound
  0x14 and switches the game mode (FF8008 = 10, FF800A = 80, then 6496). Whether this costs a life:
  UNKNOWN (follow mode 10 / 6496).
- The remaining time also feeds the stage-clear time bonus (A994..AA86). SOURCE.

### 3.6 Magic charge meter (bottom bar)
- RAM: meter **FF8376** (16-bit, high byte = charge level H), active flag
  **FF83CA**, Willow magic level **FF8366** (0..6), Madmartigan sword level
  **FF8367** (0..4), form flag FF9866 (player +0x20). Behaviour of the value is
  in player.md 3.6 and magic.md 1. VERIFIED (4B0E..4BF8).
- Empty bar (4FBE, on reset/level change/release): left cap **0x7780/0x7790**
  at (1,28)/(1,29) palette 0x11; then N empty cells **0x7781/0x7791** from
  column 2, N = 18, 21, 23, 24, 24, 24, 24 for levels 0..6 (table 0x4FB6 + 1);
  right cap **0x7782/0x7792** palette 0x11 at column 20, 23, 25, 26, 26, 26, 26
  (table 0x5080). So the bar is longer at higher magic levels. VERIFIED.
- Cell k (column 2+k) palette = table **0x4C36**[k] + magic level, with 0x4C36 =
  11 11 11 11 11 12 12 12 12 12 13 13 13 13 14 14 14 14 15 15 15 16 16 17 (hex):
  the bar's colour shifts along its length and with the level. VERIFIED.
- While charging, each update: group = H>>3, partial = H&7; cell (2+group)
  gets fill glyph pair **0x77A0+partial / 0x77B0+partial** (top/bottom), and
  cell (1+group) is set to the full pair 0x77A7/0x77B7. Fill rises 1/8 cell per
  H step. At the cap the value stops and the bar stops changing. VERIFIED (4B62..4BE2).
- Release/stop: sound **0xF4**, value 0, flag cleared, bar redrawn empty. VERIFIED (4AF2).
- FF0A3C is set to ((H & 8) ^ 8) every step (8 on reset); no reader was found:
  UNKNOWN purpose (likely a blink); not needed for the native HUD.
- Madmartigan form: 18 cells (palette base from sword level), cap at 20, a
  second left cap at 21, a 4x2 sword icon at 22..25 (glyphs **0x77C0+...**,
  table 0x4C8A + level*16, palette 0x4C84[level] = 11,11,11,12,12,12), right
  cap at 26 (5040, 4C4E). SOURCE (later stages only).

### 3.7 Items (inventory icons)
- RAM: item counts **FF8352..FF835D** (12 bytes). Icons at rows 28-29 from
  column 27, 9 slots of 2x2 cells, slot n at column 27+2n; area cleared to
  0x7120 (20 columns) first. VERIFIED (4CE4, 4D6A, 4DA6).
- Slot -> item (table 0x4CDA): 0:item10, 1:item8, 2:item9, 3:item7 (shown when
  the count is non-zero); 4,5: item3 (one icon per unit, max 2); 6,7,8: item11
  (one per unit, max 3). VERIFIED.
- Icon glyphs (table 0x4DE2 + offset 0x4DD0[slot], order top-left, bottom-left,
  top-right, bottom-right): slot 0 0x7786/96/87/97, slot 1 0x7788.., slot 2
  0x778A.., slot 3 0x77A8/B8/A9/B9, slots 4-5 0x778C/9C/8D/9D, slots 6-8
  0x77AA/BA/AB/BB. Palette 0x14 for slots 0-2, 0x13 for the rest (0x4DDA). VERIFIED.
- Meaning (from shop.h grants; SOURCE): items 10/8/9 = armour worth 1/2/3
  points (consumed in order 10, 8, 9 when hit); item 11 = protection charges
  (1..3; a hit with one spends it and gives 128 frames of invulnerability);
  item 3 = bought up to 2 (use UNKNOWN); item 7 = single (use UNKNOWN); items
  0, 4, 5, 6 are purchase counters (extra life, heal, max HP, heal+2), not shown.
  Item names: UNKNOWN (shop dialogue text, table 0x340E6 / messages).

### 3.8 Lives
- RAM **FF8342** (BCD). Display: `P=` then **lives-1** (reserve lives) as 2
  digits at (43,26), leading zero blank, palette 0 (56C8). VERIFIED.
- Changes: -1 on life loss (BCD), +1 per extra-life shop item (no change on BCD
  carry). Continue resets to the DIP value (2CEE). Score-based extends:
  a bonus threshold FF834E is loaded from FF828A at continue; its use is
  UNKNOWN (look for compares against FF834E). SOURCE.
- Row 26 is inside the play view (screen y 192); B1 moved it to the top row.

### 3.9 Boss energy (ENEMY)
- RAM **FF83CC** (boss energy, byte), second bar **FF83EC** (stage-5 boss).
  Bars at (6,4) and (6,5): ceil(energy/8) cells of **0x7794** (blue), 23 cells
  cleared with 0x7020 first. `ENEMY` label (string 14) at (1,4). Drawn by bosses
  when they start/take damage (4AC6, 4AA8; e.g. 163FA sets 32). VERIFIED.

### 3.10 Credits / player 2
- `CREDIT` (string 0) and `  COIN` (string 16) at (37,29) belong to attract and
  coin-up screens; not part of the play HUD. VERIFIED (strings).
- Two-player alternating play: the current player's block (first 128 bytes, FF8342..FF83C1) is
  exchanged with the save at FF82C2 on a life loss when the other player has
  lives; FF8292 toggles and the 1UP/2UP score positions swap. Only one player
  is on screen at a time. SOURCE (life_loss.h, player_status.h).

## 4. When the HUD is redrawn
- Full rebuild (4F44 -> 4F54 + 4F98): clear rows 28-30, labels (string 12),
  bar caps, time, items, empty charge bar, Madmartigan parts, vitality, lives,
  scores, gold. Done at stage start, after shops, and after the spell-9 flash
  (FF8349 = 1 handled by 66B0, which also redraws boss bars). VERIFIED.
- Single elements are redrawn by the routine that changes the value (score
  6284, gold 4E12, time 4F18, vitality 4A34, charge 4B0E, items 4CE4). VERIFIED.
- Spell 9 flash: FF8349 = 0xFF fills rows 2..27 (all 48 columns) with the solid
  white glyph **0x7025** palette 0 for 3 frames, then the whole HUD is rebuilt.
  VERIFIED (66B0, 6714; magic.md).

## 5. New-game and continue values, DIP switches

New game (2D18, then continue 2CEE): VERIFIED unless noted.
| Value | Start |
|---|---|
| Score | 0 |
| Lives | DIP "lives": 1, 2, 3 or 4 (table 0x1BEE indexed by inverted DSW-C bits 0-1) |
| Max HP and HP | DIP "vitality": 2, 3, 4 or 5 (table 0x1BF2, inverted DSW-C bits 2-3) |
| Armour, items | 0 |
| Gold | 0 |
| Magic level (Willow) | 0; sword level 0 |
| Time | stage checkpoint value (3:00 for stage 1) |
| High score | 55000 until beaten (ranking defaults) |
| Shop prices FF8378.. | 0400, 0800, 0250, 1000, 1500, 0500 (BCD; NG+ doubles, AGENTS.md) |

- DIP decode (1B96): DSW-C (inverted) bit 4 -> FF8295, bit 5 demo sound,
  bit 6 continue allowed (FF8287), bit 7 debug (FF828F: stage select from DSW-B,
  max levels on a held input). DSW-B (1B46): bits 0-2 difficulty (table 0x1B3E),
  bits 3-4 Nando speed (FFF9A4), bit 7 "magic continue" (FFF9A6, magic.md 6).
  MAME's factory defaults: UNKNOWN here (check cps1.cpp willow); B1 `port.c`
  `make_dsw` has the menu-to-switch mapping.
- Continue (2CEE): lives = DIP value, score 0, HP = maxHp; magic level, gold,
  items and armour are **kept**. SOURCE (only those fields are written).

## 6. Native state the HUD needs (plain variables)

Per player (swap the whole set for alternating 2-player):
`score` (u32 BCD or binary + 8-digit render), `lives` (u8), `hp`, `maxHp`,
`armor` (u8), `gold` (u32, 6 digits shown), `items[12]` (u8),
`magicLevel` (0..6), `swordLevel` (0..4), `form` (Willow/Madmartigan).
Global: `highScore` (u32), `timeMin`, `timeSec`, `timeDiv` (u8), `clockFrozen`
flags, `charge` (u16 meter) and `charging` (u8), `bossEnergy[2]` (u8) and
`bossBarVisible`, `currentPlayer` (0/1), `otherScore`.
Each change marks only its HUD element dirty (as the arcade does); no
per-frame full redraw is needed. The flash (spell 9) and HUD rebuild are events.

Genesis layout: the arcade uses 48 columns; the Genesis screen has 40
(320 px). Cut 2 allows a Genesis layout; keep the element order, glyphs and
palettes (fixed text colours approximate palettes 0/2/3/0x10/0x11..0x1D).
B1 (`s1text.h` `s1_col`) removed the gaps between groups: top rows
x<19 kept, 19..35 -> x-4, >=36 -> x-8; bottom bar x<24 kept, 24..31 dropped,
>=32 -> x-8. A native layout may choose differently.

## 7. Open questions (UNKNOWN)
1. Outcome of time-out (mode 10 path at 956E): life loss or game over?
2. Item names and the use of items 3 and 7 (shop text / item routines).
3. Score extend rule (FF834E bonus threshold).
4. FF0A3C purpose (charge meter step writes it; no reader found).
5. Arcade DIP factory defaults (for the Arcade-mode default settings).
6. Not trace-checked: exact redraw timing of each element relative to the
   frame; capture with a headless trace of scroll1 writes if it matters.
