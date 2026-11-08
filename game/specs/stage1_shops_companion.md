# Stage 1 shops and the hidden companion: behaviour specification

Arcade: Willow (Capcom CPS-1, 1989), `build/reference_maincpu.bin` (sha256
0b66efe8...). Behaviour spec for the native Genesis rewrite (NATIVE_ENGINE_PLAN.md
sections 2 and 6). Continues `stage1_enemies.md` (sections 5.5 hidden object,
5.12 shop doors), `hud.md` (gold, items, lives), `magic.md` (magic level) and
`combat.md` (shot collision). Arcade addresses are citations only.

Evidence tags (as stage1_enemies.md):
- **VERIFIED**: two independent sources agree (ROM bytes / disassembly read for
  this file plus the transliteration in `native/engine/src/*.h` or `src/*.h`, or
  plus a captured arcade trace).
- **SOURCE**: one source only (usually the transliteration, which PORTING.md says
  was differential-tested against the original routines).
- **UNKNOWN**: not established; says where to look next.

Units as player.md: world X right, world **Y up**; speeds signed **8.8
px/frame** (256 = 1 px/frame); one update = one 60 Hz frame. Hit box =
(xOffset, width, yOffset, height). RAM: `a5` = FF8000. Player record FF9846
(+0 active: 1 collidable / 2 not, +6 x, +0A y, +0F facing, +20 form 0 = Willow /
1 = Madmartigan). Inputs (FF806A current, FF806B previous): bit 0 right, 1 left,
2 down, 3 up, 4 button 1 (attack, "A"), 5 button 2 (jump, "B").

Text cells: scroll1 cell (col,row) -> CPS screen pixel (col*8, (row-2)*8) on the
384-wide line (intro.md 6.3). CPS objects: screen = (x-64, y-16). CPS screen is
384 wide; the Genesis 320 view (the B1 crop drops 32 px at each side) cuts the
outer item columns, so the native shop needs its own 320-wide layout (section
1.4).

Traces (headless lockstep, reference program, scratch only; nothing was
launched): a private copy of the harness, `ls_shop` built from `lockstep.c` in
the scratch dir with three additions (`POKES=t:addr:val;...` work-RAM byte
writes, `RANDUNTIL=p` = SCRIPT=random before p then KEYS, `NUDGEUNTIL=t`), run as
`run.sh` with `STAGE=0 INVINCIBLE=1 KILLBOSS=1 GATEASSIST=900 NUDGE=600`.
- **A** (`keysA.txt`, `logA.txt`, video `vidA/`): random play to t5060, then
  Up+B+Right jump onto the shop ledge, touch at **t5799**; in the shop: Right
  (NO), Left (YES), A; Down + A on HOLY WATER (HP full), Information bought (105
  -> 55 gold), Information again, Battle Sword, EXIT. Natural gold (105).
- **B** (`keysB.txt`, `logB.txt`, `vidB/`): same entry, **gold poked to 50000 at
  t5900** (assist), then 9 purchases (magic, 3 magic bracelets, life pack, 1UP,
  charm ring, charm pendant, holy water) and EXIT.
- **C** (`keysC.txt`, `logC.txt`, `vidC/`): random to t4160, then jump shots at
  the hidden object (placed at 1248,128 in this run); **its energy poked to 1 at
  t4150** (assist, saves 19 hits); companion followed to t5700 with walking,
  standing, jumps and attack presses.
- Sound queue: ring FF8172 (64 words), write index FF801E (0x7818/0x7824),
  logged every frame (`logS.txt`, `logSB.txt`, `logSC.txt`).
Scratch: `/private/tmp/claude-501/-Users-rester159-genesis-ports--capcom-willow/53b11e69-00ff-4aeb-998a-a81a07ed3d73/scratchpad/agents/shop/`
(`msg.py` = message decoder, `items.py` = shop table dump, `png/` = rendered
frames, see section 4).

---

## 1. Shops

### 1.1 The shop door (class 12 kind 8), ROM 0x32BE6

Stage table entries: (1872, 311) variant **0** direction **4**; (2688, 199)
variant **16** direction **5** (stage1_enemies.md 2.1). Direction = **shop
index** (0..17, selects the item list and dialogue). Variant low nibble = door
type (0, 1, 2; stage 1 uses only 0), high nibble = bit number in the "shop used"
byte **FF839A** (stage 1: bit 0 and bit 1). VERIFIED (disassembly 0x32BE6..0x32C9A,
`actor12_shop.h`, trace).

| Item | Value | Tag |
|---|---|---|
| Visible actor | The **shopkeeper** (blue owl-hooded figure) standing at the door position. Root **0x32C9C** set with 0x1F40: one node, packed **3x3 block** of 16x16 tiles (48x48), code **0x0AC6**, CPS object palette **12** (attr 0x220C), held. Drawn every frame (0x2244) while waiting | VERIFIED (ROM node, trace frame a5790) |
| Init (state 0) | if bit (variant>>4) of FF839A is set: removed at once (shop already used); else state 2 with the pose | VERIFIED (0x32C14) |
| Trigger (variant 0) | every frame: `(u16)(playerX - x + 12) <= 24` and `(u16)(playerY - y + 14) <= 28` (player x in [x-12, x+12], y in [y-14, y+14]) **and** HP FF8362 != 0. No button needed, the player just walks/jumps into him | VERIFIED (0x32C40..0x32C68; trace: touched at (1863, 303) while landing from a jump) |
| On trigger | player record +0 = **2** (not collidable); set the FF839A bit; FF0F0E = shop index (+0F); **FF83C5 = 1** (event busy); actor phase -> wait. The shopkeeper sprite is no longer drawn | VERIFIED (0x32C6A..0x32C86, trace t5799) |
| Wait | while FF83C5 != 0 nothing; when it clears the actor is released (state 4/6 -> 0x2C54) | VERIFIED (0x32C90) |
| Variant 1 (not in stage 1) | larger box (|dx| <= 16, |dy| <= 32), needs **Up held without Left/Right**, plays root **0x32CAC** (4x3 block, palette 24, 3 nodes of 8 ticks then held) for a 30-frame countdown before the event starts | SOURCE (`actor12_shop.h`) / ROM nodes VERIFIED |
| Reset of FF839A | cleared at **new game** (0x2D18 memory clear, and 0x2D8C via 0x2D80) and at **stage clear** (0xA95A -> 0x2D80). Not cleared by a life loss or a continue: each shop is usable **once per stage** | VERIFIED (all writers found by search) |

### 1.2 Main-state routing around the shop (0x9530..0x968E)
- Play state (FF8008 = 2, 0x9530): when FF83C5 is set, `0x95B6`: clear FF83D0,
  FF8008 = 4, sound **0xF4** (0x785E), sound **0x10FB**. VERIFIED (code + trace
  t5800).
- Event state (FF8008 = 4, 0x9600): each update runs only 0x1F88, the clock
  **0x4EA2** and the shop routine **0x33BD6**. Actors, collisions, spawners and
  the camera do **not** run: the stage is frozen behind the shop. If the time
  FF83C6 reaches 0 (and the debug switch FF799E is clear) the normal time-up
  path 0x956E runs even inside the shop. VERIFIED (disassembly).
- When FF83C5 clears: FF8008 = 6 (0x9622): 0x6614 clears both CPS object banks;
  the two scroll controllers 0x42602/0x42F48 rebuild the stage view until FF8471
  is clear; FF8008 = 8 with a 2-update countdown; then (0x965E) player +0 = 2,
  **FF9896 = 96** (post-shop invulnerability, player not collidable for 96
  updates), FF9894 = 4, FF987C = 0, FF9895 = 0; stage music restart (0x7878:
  command **table 0x789E[stage]** then **0x01FA**; stage 1 = **0x0000, 0x01FA**);
  fade-in 0x1EAA; FF8008 = 2. VERIFIED (code + trace A: busy clear t7536, music
  t7543, play t7596, player +0 back to 1 at t7691 = 95 updates later).

### 1.3 The shop routine 0x33BD6 (state FF800C, step FF8010, timer FF8012)

Shop work RAM (FF0Fxx = a5-0x70xx): FF0F0E shop index, FF0F10 column (0,2,4,6),
FF0F12 row (0,2,4), FF0F14 bought bits (bit = selection index), FF0F0F marker
count, FF0F16..FF0F20 price snapshot, FF0F22..FF0F2D the 12 item numbers, FF0F2E
/ FF0F32 1UP / life-pack price snapshot, FF0F36/37 dialogue state/delay, FF0F38..
FF0F3B owl blink. VERIFIED (disassembly).

**Entry (state 0 step 0, 0x33BF8)**, the update after the trigger:
1. Clear FF9885, FF8376 (charge meter) and FF83CA (charge active): a charge in
   progress is lost. Marker count and bought bits = 0. VERIFIED.
2. Snapshot the six persistent prices FF8378..FF8382 to FF0F16.. (the prices
   shown and charged during this visit). Snapshot the 1UP price = 0x348E4
   [min(FF8352,2)] and the life-pack price = 0x348F0[min(FF8357,2)]. VERIFIED.
3. Fade-out job 0x1E88 (job 0x0C00, speed 3, the game waits for it), sound
   **0xF4** (0x785E), FF8343 = 0, palette load **0x0B28**, music **0x1B** then
   **0x02FA** (0x78C8). Trace: fade-out t5801..t5860 (59 frames, update counter
   frozen), sounds F4 / 1B / 2FA at t5860. VERIFIED.
4. Step 2: **32 updates** wait (t5860..t5891). VERIFIED (trace).
5. Step 4 (0x33C8E): layer control FF804A = **0x13B8**; scroll2 camera FF8402:
   map directory long at **0x2DBF8+28**, x **2048**, y 0, load map chunks **5**
   and **9** (0x548C); scroll3 camera FF8482: **0x2EB24+28**, x **512**, y 0,
   chunks 5 and 9 (0x5532). Copy the 12 item numbers of this shop from table
   **0x340E6** + shop*12; replace item 0 with the next magic level and item 1
   with the next sword level, item 10 with the 1UP of the current character
   (section 1.6). VERIFIED (disassembly 0x33C8E..0x33E04; tables dumped).
6. State 2 (0x33E18): draw the 12 item icons (0x341DA), rebuild the HUD
   (0x4F44), YES/NO cursor (0x349F8), message **0x004D** style 0x1301
   ("WELCOME. MAY I HELP YOU?") and message **0x0650** style 0x131C (big
   "YES    NO"), fade-in 0x1EAA. Trace: reveal at t5892, text typed from t5892.
   VERIFIED.

**YES/NO (step 0, 0x33E7C)**: newly pressed bits (`~prev & cur`), priority
right, left, buttons. Right -> cursor on NO (column 2), Left -> YES (column 0);
no sound. Button 1 or 2: on YES -> clear text rows 22 and 24 (0x7D80), draw the
big **G** symbol (code **0x768C** = 0x7000 + word 0x33E06, attr **9**) at cells
(28..29, 22..23), show the selected item's name/description (0x346E0), its
price (0x348FC) and the item cursor (0x34998); step 2. On NO -> leave (below).
The player can press while the welcome text is still typing. VERIFIED (code,
trace A: t6000 R, t6060 L, t6120 A).

**Item menu (step 2, 0x33EF2 / input 0x34272)**:
- Edge-triggered, one action per update, priority **right, left, down, up,
  button**: right `column = (column+2) & 6`, left `(column-2) & 6` (both wrap),
  down `row += 2`, past 4 -> 0, up `row -= 2`, below 0 -> 4. Any move redraws
  name, description and price. No cursor sound. VERIFIED (code + trace).
- Selection index = `(row*4 + column) / 2`: row 0 = items 0..3, row 2 = 4..7,
  row 4 = 8..11; column 0/2 = left pair, 4/6 = right pair. VERIFIED.
- Button 1 or 2 = buy (0x3436E, section 1.7). EXIT (item 6) sets the timer
  FF8012 = 0; the next update sees it and leaves. VERIFIED.

**Information dialogue (step 6, 0x33F84)**: after buying item 1, clear row 24,
message `0x33FE6[shop]` style 0x1300; if `0x33FF8[shop]` is 0 return to the
menu at once; else wait for the text to finish, then **31 more updates**, clear
row 24, show the second message, back to the menu. Input is ignored meanwhile.
VERIFIED (code; trace A shop 4: single message).

**Leaving (0x33EF8, step 4)**: clear rows 22 and 24; cursor "column 4" (YES/NO
cursor x = 0: hidden); FF0E58 = 1; message **0x0020** style 0x1300 ("WELL
THEN, TAKE CARE."); timer FF8012 = **63**. Then (0x33F42) wait until the text is
finished (FF0E58 = 0), count the 63 updates, cancel the two text tasks (slots
0xC0, 0xE0), sound **0x10FB**, fade-out 0x1E88, FF800C = FF8010 = 0, **FF83C5 =
0**, camera records restore (0x2DE6), HUD rebuild (0x4F44), stage palettes
(0x0978). Then the main-state return of 1.2. Trace A: EXIT pressed t7350, text
t7351..t7411, sound 10FB t7477, busy clear t7536, play t7596. Trace B: EXIT
t8600 -> busy clear t8786 (same 186 frames). VERIFIED.

**Where play resumes**: the player record is untouched (same x, y, facing:
trace (1863,303) before and after); the view is rebuilt around it; the
shopkeeper is gone (FF839A bit); the clock kept running (1.8); the player is
invulnerable/not collidable for 96 updates after the fade-in. VERIFIED (trace
frames s7550, b8990).

**Owl animation (0x34AD4, every shop update while the menu runs)**: blink timer
FF0F38 restarts every **63** updates (then: mode = normal, delay 7, frame 0, no
draw that update). Normal mode: when the delay runs out, frame ^= 2, delay = 40
if the new frame is 0, else 20 -> per 63 updates: frame 0 for 7, frame 2 for 20,
frame 0 for 36. After any purchase attempt (0x3436E sets mode = 1, timer 63,
delay 7, frame 0): frame ^= 1 every **10** updates (talking) until the 63-timer
ends. Each update writes the 2x2 scroll3 cells at CPS 0x917AA8 (+0, +4, +0x20,
+0x24; attr 2) from table **0x34B34** + frame*8: frame 0 = 0x0AF9 0x0AFD 0x0AFA
0x0AFE, frame 1 = 0x0ACA 0x0ACE 0x0ACB 0x0ACF, frame 2 = 0x0AC8 0x0ACC 0x0AC9
0x0ACD (native: layer 1 cells x 21..22, y 58..59). VERIFIED (disassembly
0x34A9C..0x34B32 = `shop.h willowNativeShopAnimate`).

### 1.4 Screen layout (CPS coordinates, trace frames s5900 .. b8580)

| Element | Where | Source | Tag |
|---|---|---|---|
| Background | full-screen red curtains, 12 framed icon boxes (2x3 left, 2x3 right), centre window with the shopkeeper portrait (owl in blue hood at a desk with candles), text box at the bottom. Scroll2 = map chunks 5 and 9 of directory 0x2DBF8+28 at camera (2048,0); scroll3 = chunks 5 and 9 of 0x2EB24+28 at (512,0); layer control 0x13B8. Offline extraction: `native/engine/tools/build_shop.py` (`scene-tiles.bin`) | VERIFIED (code + frames) |
| HUD | stays visible (rows 0-3 and the bottom bar), rebuilt at entry; gold updates on purchase | VERIFIED (frames) |
| Item icons | scroll2 cells (col,row) from table **0x340B6** (CPS 0x918000 addresses): slots 0-3 (1,51) (4,51) (18,51) (21,51); 4-7 (1,53) (4,53) (18,53) (21,53); 8-11 (1,56) (4,56) (18,56) (21,56). Icon = 2 columns x 2 or 3 rows (height table **0x341BE**: non-zero = 3) of tiles `code + x + y*8`, code table **0x34046**[item], attribute table **0x3407E**[item] (routine 0x341DA) | VERIFIED (ROM tables, `build_shop.py`, frames) |
| Item cursor | 4 CPS objects, palette **8**, codes **0x000F 0x001F 0x002F 0x003F** (table 0x34982) at (x, y) (x+16, y) (x, y+16) (x+16, y+16); x = table 0x3498A[column/2] = **80, 128, 352, 400**, y = 0x34992[row/2] = **48, 88, 128** (screen x 16/64/288/336, y 32/72/112): a yellow frame around the icon (0x34998) | VERIFIED |
| Bought marker | 4 objects, palette 8, codes **0x006F 0x004F 0x007F 0x005F** (table 0x3497A), same geometry over the item, a red X; object slots 8+4n in purchase order (0x34A4A); also sets the bought bit | VERIFIED (code + frames s6620, b8580) |
| YES/NO cursor | 4 objects, palette 8, cursor codes in order 0,2 at (x,192) and 1,3 at (x+32,192), x = table **0x349F2**[column/2] = **176** (YES), **288** (NO), 0 when leaving: yellow brackets around the word | VERIFIED |
| Name | message `0x0200 | 0x34758[item]`, style **0x2101**: cell (4, **22**), one row, drawn at once (handler 2, no sound) | VERIFIED |
| Description | message `0x0200 | 0x3473C[item]`, style **0x1100**: cell (4, **24**), two rows, at once | VERIFIED |
| Price | big `G` at cells (28..29, 22..23), attr 9; 6 BCD digits at cells (30+2n .. 31+2n, 22..23), n = 0..5, 2x2 glyph `d, d+1, d+16, d+17` with d = table **0x6822**[digit] (0 = 0x7B22, 1..8 = 0x7B00..0x7B0E step 2, 9 = 0x7B20), attr **9**, leading zeros blank (EXIT shows only the G) | VERIFIED (code, frames) |
| Shop messages | style 0x1300: cell (4, **24**), 2 rows, 3 frames per character, sound 0x2F per character; 0x1301: (4, 22) | VERIFIED |
| Text palette | scroll1 palette **5** for all shop strings (record attribute words), 0x0E for the big YES/NO | VERIFIED (ROM records) |
| Row clears | 0x7D80(row): rows row, row+1, cols 4..43 = 0x71A6 attr 0; 0x7DA8: 20 columns | VERIFIED (PORTING "Selected-item messages") |

Native layout note: at 384 px the outer icons are at screen x 16 and 336..368;
the Genesis view is 320 wide, so the icon grid, price and YES/NO positions must
be laid out natively (e.g. compress the column spacing); keep the 2x3 + 2x3 grid
and the cursor-to-index mapping above.

### 1.5 Palettes
- 0x0B28 at entry: FF804C..FF8052 = 0, then CPS palette RAM **0x91C800** <- ROM
  **0xE9920** (0x400 bytes; 0xE9D20 if FF8343 is set, which entry clears first)
  and **0x91CC00** <- ROM **0xEB700** (0x400 bytes): palette groups 64..127
  (scroll2 + scroll3). Object and scroll1 palettes stay the stage's (cursor and
  marker use object palette 8, text palettes 5, 9, 0x0E, HUD unchanged).
  VERIFIED (disassembly; `shop_scene.h` writes groups 64+i from the same 0x800
  bytes).
- Exit: 0x0978 reloads the stage palettes. VERIFIED.
- Shopkeeper door sprite in the stage: object palette 12 (stage palette).

### 1.6 Item lists of the stage-1 shops (table 0x340E6, 18 shops x 12 bytes)

Item table per item n (28 items): kind **0x3479A**[n], "repeatable" flag
**0x3439E**[n], height 0x341BE[n], price long **0x34874**+n*4, icon code
0x34046, icon attribute 0x3407E, description message 0x3473C, name message
0x34758. VERIFIED (ROM dump `items.py`).

Slot replacement at entry (0x33D8C..0x33E00): slot 0 = **0x33E0A**[min(FF8366+1,
6)] (Willow magic: levels 0..5 -> items 4, 14, 5, 3, 2, 13; at level 6 item 13
shown with the bought marker); slot 1 = **0x33E12**[min(FF8367+1, 4)]
(Madmartigan sword: levels 0..3 -> items 25, 0, 9, 11; at level 4 item 11
marked); slot 10 = item **8** if player +20 (form) != 0 else item **7** (shops
16, 17: item 26 = empty). VERIFIED.

| Slot | Shop 4: door (1872,311) | Shop 5: door (2688,199) |
|---|---|---|
| 0 | next magic (item 4 MOONLIGHT MAGIC at level 0) | same |
| 1 | next sword (item 25 BATTLE SWORD at level 0) | same |
| 2 | 23 MAGIC BRACELET | 23 MAGIC BRACELET |
| 3 | 21 CHARM BRACELET | **15 QUEST-STONE** |
| 4 | 16 HOLY WATER | 16 HOLY WATER |
| 5 | 18 HOLY LIQUOR | 18 HOLY LIQUOR |
| 6 | 17 LIFE PACK | 17 LIFE PACK |
| 7 | 1 INFORMATION | 1 INFORMATION |
| 8 | 22 CHARM PENDANT | 22 CHARM PENDANT |
| 9 | 20 CHARM RING | 20 CHARM RING |
| 10 | 7 1 UP (Willow) / 8 (Madmartigan) | same |
| 11 | 6 EXIT | 6 EXIT |

Raw rows: shop 4 = 0,0,23,21,16,18,17,1,22,20,7,6; shop 5 = 0,0,23,15,16,18,17,
1,22,20,7,6. VERIFIED (ROM; trace A/B screen shows shop 4 exactly). Shop 5 was
not entered in a trace (it is past the mid-boss gate).

### 1.7 Items: price, effect, messages (0x3436E + the 28 handlers)

Prices are BCD gold. Kind 0: fixed long at 0x34874+n*4. Kind 1: by a count
(table 0x348E4). Kinds 2..7: persistent price word **FF8378 + (kind-2)*2**,
initial values at new game (0x2D28) **400, 800, 250, 1000, 1500, 500**; each
purchase adds the item's increase word (low word of 0x34874+n*4) with BCD add;
the visit's displayed and charged price is the entry snapshot. VERIFIED (code
+ trace B: magic bracelet charged 250 three times while FF837C went 0250 ->
0500 -> 0750 -> 1000).

| Item | Name | Price (gold) | Requirement / cap ("full" = message 26) | Effect | Tag |
|---|---|---|---|---|---|
| 4, 14, 5, 3, 2, 13 | MOONLIGHT / TORNADO / GOLD / TIME / EXPLOSIVE / CRYSTAL MAGIC | 850 / 2000 / 4500 / 8000 / 10000 / 15000 | Willow only (form 0) else message 30 | FF8366 (magic level) = 1 / 2 / 3 / 4 / 5 / 6 (absolute; also the shot level), marker, charge bar redrawn (0x4FBE) | VERIFIED (trace B: 850, level 0 -> 1) |
| 25, 0, 9, 11 | BATTLE SWORD / DRAGON SWORD / BATTLE BLADE / ULTIMATE SWORD | 2000 / 4000 / 6000 / 10000 | Madmartigan only (form 1) else message 30 | FF8367 = 1 / 2 / 3 / 4, marker, sword HUD (0x5040), charge reset | VERIFIED (trace A: Willow -> "YOU CAN'T USE IT.") |
| 23 | MAGIC BRACELET | kind 4: 250 (+250 per purchase) | FF835D + 1 <= 3 | FF835D += 1 (protection charge, hud.md 3.7); HUD icon slot FF835D+5; marker only when it reaches 3; **repeatable** within a visit (flag 1) | VERIFIED (trace B) |
| 21 | CHARM BRACELET | kind 6: 1500 (+1500) | FF835B == 0 | FF835B = 3, armour FF8364 += 3, icon slot 2, marker | SOURCE (same code path as 20/22, VERIFIED) |
| 20 | CHARM RING | kind 5: 1000 (+1000) | FF835A == 0 | FF835A = 2, armour += 2, icon slot 1, marker | VERIFIED (trace B: armour 0 -> 2, price 1000 -> 2000) |
| 22 | CHARM PENDANT | kind 7: 500 (+500) | FF835C == 0 | FF835C = 1, armour += 1, icon slot 0, marker | VERIFIED (trace B: armour 2 -> 3) |
| 16 | HOLY WATER | kind 2: 400 (+200) | HP+1 <= max HP | HP (FF8362) += 1, FF8356 += 1, HP bar redrawn, marker | VERIFIED (trace B: HP 3 -> 4; trace A: HP full -> message 26) |
| 18 | HOLY LIQUOR | kind 3: 800 (+400) | HP+2 <= max, else HP+1 <= max, else full | HP += 2 (or +1), FF8358 += 1, marker | SOURCE (trace B: refused at full HP) |
| 17 | LIFE PACK | kind 1: **2000 / 5000 / 10000** by FF8357 = 0 / 1 / >=2 | max HP+1 <= **6** | max HP FF8363 += 1 (HP itself unchanged), FF8357 += 1, marker | VERIFIED (trace B: max 3 -> 4, 2000) |
| 7 / 8 | 1 UP | kind 1: **5000 / 25000 / 80000** by FF8352 = 0 / 1 / >=2 | none | FF8352 += 1, marker, lives FF8342 += 1 (BCD; unchanged on carry), queued job 0x0404 (0x1F14) | VERIFIED (trace B: lives 2 -> 3, 5000) |
| 15 | QUEST-STONE (shop 5) | 500 | FF8355 < 2 | FF8355 += 1, icon slot FF8355+3 (4 or 5), marker. **Use: makes the hidden object visible (2.1) and one stone is spent when the companion joins (2.2)** | SOURCE (handler) / VERIFIED (readers 0x375AA, 0x36E70) |
| 1 | INFORMATION | 50 | none | marker, dialogue step (1.3): shop 4 message **0x51**; shop 5 **0x52** then **0x53** | VERIFIED (trace A: 105 -> 55, dialogue) |
| 6 | EXIT | (0, only "G" shown) | | leave (1.3); its description shows while selected | VERIFIED |
| 10, 12, 24, 26, 27 | (unused entries) | | | confirm does nothing (no payment, no message) | SOURCE |

Purchase order of checks (0x3436E): (1) if the item is not repeatable (flag
0x3439E = 0) and its bought bit is set -> message **27** "WE HAVE RUN OUT OF
GOODS." (each non-repeatable slot can be bought **once per visit**); (2) owl
"talking" mode on; (3) capacity check -> message **26**; (4) character check ->
message **30**; (5) payment: subtract the 4-byte BCD price from gold **FF8368..
FF836B** with SBCD (low byte first); on a borrow the old value is restored and
message **28** "YOU DON'T HAVE ENOUGH GOLD." (nothing else changes); (6) on
success: persistent price increase (kinds 2..7), HUD gold redraw (6 digits at
hud cell (24,3)), the effect, then message **25** "THANK YOU. DO YOU NEED
ANYTHING ELSE.". Messages use style 0x1300 after clearing row 24. No sound
except the typing 0x2F. VERIFIED (disassembly order in PORTING "Item
confirmation", `shop.h`; trace A/B messages, gold and no other sounds).

Bought bits are per visit (cleared at entry); the next visit offers the item
again (with the raised price for kinds 2..7, the next tier for kind 1, the next
level for magic/sword). VERIFIED.

**Home mode NG+** (AGENTS.md): all nonzero prices doubled at display and payment
(`willowNativeShopRunPrice`); arcade mode unchanged. Mention only.

### 1.8 Clock, music, sounds
- **The clock runs in the shop** (0x4EA2 is called every event update): trace A
  1:40 at entry, 1:14 when play resumed (26 s in 1677 frames); it stands still
  only during the blocking fades (updates frozen). Time-up inside the shop runs
  the normal time-up path. VERIFIED (code + trace).
- Sound commands (queue words, trace A): entry **0x00F4, 0x10FB** (t5800); after
  the fade-out **0x00F4, 0x001B, 0x02FA** (shop music 0x1B, t5860); **0x002F**
  per typed character (welcome, YES/NO letters, purchase answers, information,
  farewell; item names/descriptions are instant and silent); exit **0x10FB**
  (t7477); after return **0x0000, 0x01FA** (stage-1 music restart, t7543). No
  cursor, purchase or 1UP sounds. VERIFIED (sound-queue log).

### 1.9 Strings (message records, decoder `msg.py`: record 0x70000 + (id >> layout)*168, layout = style >> 12, stream = record + 8 + 0x7984 table, palette word = record + 0x798C table, destination table 0x7A1E)

| Word / style | Text (exact) | Stream ROM | Cell |
|---|---|---|---|
| 0x004D / 0x1301 | `WELCOME. MAY I HELP YOU?` | 0x71948 | (4,22) |
| 0x0650 / 0x131C | big `YES` + `NO` (codes 0x76E0 0x7688 0x76C4, 4 blanks 0x71A6, 0x76AA, blank, 0x76AC) | 0x71A48 | (14,24) |
| 0x0019 / 0x1300 (25) | `THANK YOU. DO YOU NEED ANYTHING ELSE.` | 0x70838 | (4,24) |
| 0x001A (26) | `IT LOOKS LIKE YOU CAN'T HOLD ANY MORE.` | 0x70890 | (4,24) |
| 0x001B (27) | `WE HAVE RUN OUT OF GOODS.` | 0x708E0 | (4,24) |
| 0x001C (28) | `YOU DON'T HAVE ENOUGH GOLD.` | 0x70938 | (4,24) |
| 0x001E (30) | `YOU CAN'T USE IT.` | 0x709E0 | (4,24) |
| 0x0020 (32) | `WELL THEN, TAKE CARE.` | 0x70A88 | (4,24) |
| 0x0051 | `FIRST, WE MUST MEET WITH THE WARRIOR.` (shop 4 information) | 0x71A98 | (4,24) |
| 0x0052 | `THE QUEST-STONE IS A LEGENDARY ROCK.` (shop 5, first) | 0x71AF0 | (4,24) |
| 0x0053 | `THE PERSON WHO HOLDS IT WILL BE BLESSED WITH GOOD LUCK.` (shop 5, second; wraps after 40 characters) | 0x71B40 | (4,24) |

Item names (style 0x2101, cell (4,22)) / descriptions (style 0x1100, cell
(4,24), `/` = next row):

| Item | Name (word, stream) | Description (word, stream) |
|---|---|---|
| 4 | MOONLIGHT MAGIC (0x246, 0x70B80) | THE LIGHT OF THE MAGIC WILL RIP / THE ENEMY APART. (0x204, 0x70158) |
| 14 | TORNADO MAGIC (0x250, 0x70D28) | IT WILL ENGULF THE ENEMY IN A TORNADO. (0x20D, 0x70448) |
| 5 | GOLD MAGIC (0x247, 0x70BA8) | IT WILL CHANGE THE ENEMY INTO GOLD. (0x205, 0x701A8) |
| 3 | TIME MAGIC (0x245, 0x70B58) | TIME CAN BE STOPPED. (0x203, 0x70100) |
| 2 | EXPLOSIVE MAGIC (0x244, 0x70B30) | IT WILL BLAST THE ENEMY. (0x202, 0x700B0) |
| 13 | CRYSTAL MAGIC (0x24F, 0x70CF8) | THE ULTIMATE MAGIC. (0x20C, 0x703F8) |
| 25 | BATTLE SWORD (0x25B, 0x70EF0) | ATTACK POWER INCREASES. (0x218, 0x707E8) |
| 0 | DRAGON SWORD (0x242, 0x70AD8) | ATTACK POWER INCREASES. (0x200, 0x70008) |
| 9 | BATTLE BLADE (0x24B, 0x70C50) | ATTACK POWER INCREASES. (0x208, 0x702A8) |
| 11 | ULTIMATE SWORD (0x24D, 0x70CA8) | THE PROOF OF A TRUE WARRIOR. (0x20A, 0x70350) |
| 23 | MAGIC BRACELET (0x259, 0x70EA0) | IT WILL SAVE YOU FROM ENEMY MAGIC. (0x216, 0x70740) |
| 21 | CHARM BRACELET (0x257, 0x70E48) | ALLOWS YOU TO AVOID 3 POINTS OF ENEMY / ATTACK. (0x214, 0x70698) |
| 15 | QUEST-STONE (0x251, 0x70D50) | ?... (0x20E, 0x704A0) |
| 16 | HOLY WATER (0x252, 0x70D78) | YOU CAN RECOVER 1 POINT OF LIFE. (0x20F, 0x704F0) |
| 18 | HOLY LIQUOR (0x254, 0x70DD0) | YOU CAN RECOVER 2 POINTS OF LIFE. (0x211, 0x70598) |
| 17 | LIFE PACK (0x253, 0x70DA0) | LIFE IS EXTENDED ONE POINT. (0x210, 0x70548) |
| 1 | INFORMATION (0x243, 0x70B00) | I WILL TEACH YOU SOMETHING INTERESTING. (0x201, 0x70058) |
| 22 | CHARM PENDANT (0x258, 0x70E78) | ALLOWS YOU TO AVOID 1 POINT OF ENEMY / ATTACK. (0x215, 0x706E8) |
| 20 | CHARM RING (0x256, 0x70E20) | ALLOWS YOU TO AVOID 2 POINTS OF ENEMY / ATTACK. (0x213, 0x70640) |
| 7 / 8 | 1 UP (0x249 / 0x24A, 0x70C00 / 0x70C28) | ONE PLAYER IS ADDED. (0x207, 0x70250) |
| 6 | EXIT (0x248, 0x70BD8) | ARE YOU GOING ALREADY? (0x206, 0x70200) |

All VERIFIED (decoder reproduces the intro strings of intro.md exactly; shop
strings seen in frames s5900..b8580).

---

## 2. The hidden companion

### 2.1 The hidden object (class 16 kind 9, 0x374FA) - summary
As stage1_enemies.md 5.5: once per game (bit 0 of **FF83A5**, cleared only by
the new-game clear 0x2D18), placed at random at **(896,160)** 6/16,
**(1248,128)** 5/16 or **(1312,224)** 5/16 (tables 0x37542 / 0x37532); hit box
(10,20,16,20), energy **20**; every player-shot hit: sound 0x32 and 16 frames
not hittable. **Visible only if FF8355 (Quest-Stones) != 0**: then it shows root
**0x3784E** (the companion's own blinking pose, 2.4) - so the Quest-Stone
reveals a little figure hiding in the forest; without a stone it is invisible
(empty pose 0x375C4) but can still be shot. When energy reaches 0 (no sound
on that hit): allocate a class-16 record (0x2C38): +0 = 1, kind (+20) = **0**,
x, y = the object's position; set the FF83A5 bit; the object is removed.
Companion projectiles also count as shots for it. VERIFIED (disassembly
0x3754E..0x37654; trace C: killed at t4199, companion record at (1248,128)
the same update).

### 2.2 Who it is
A small **Brownie-like warrior** (grey helmet/cap, brown hair and cloak, about
24 px tall, carrying a spear) who follows Willow and throws **spears/arrows**
(long brown shaft, green fletching). Identity in the film's terms (Franjean /
Rool) is not named anywhere in the program: UNKNOWN (visual only; frames
`zoom_c4205.png`, `zoom_c5665.png`). It never takes damage, never hurts by
touch and never collides with enemies; only its projectile interacts. It has
no hit box of its own. VERIFIED (no damage path in 0x36D7E; class 16 is not in
the 39A52 collision schedule).

### 2.3 Behaviour (class 16 kind 0, routine **0x36D7E**; transliteration
`native/engine/src/stage_companion.h`, `src/willow_arcade_actor16_zero.h`,
differential-tested 65,536 cases per PORTING 2026-10-04)

Record fields (class-16 pool FFEAC6, 20 x 64 bytes): +2 state, +3 phase, +5
action, +6 x, +0A y, +0F facing (0 right, 1 left), +18 motion pointer (table
**0xED060**), +22 timer, +24 floor contact, +2C vertical speed (8.8), +3D
"player near" latch. "Cadence" = frame counter FF8019 + (19 - slot). Player
"standing" = player facing byte bit 1 (FF9855 & 2). Motion table 0xED060 (x
speed per facing, y 0): motion 0 = ±128 (0.5 px/f), 1 = ±256 (1), 2 = ±384
(1.5), 3 = ±512 (2), 4 = ±640 (2.5). Every active update ends with: if
**FF83DC** != 0, x += 1 (stage drift; meaning of FF83DC UNKNOWN), then draw
(0x2244, only while inside the camera window). VERIFIED (ROM table, code
0x36EC8..0x36EDE) / rest SOURCE.

**State 0: freed, waiting for the player**
| Phase | Behaviour | Tag |
|---|---|---|
| 0 (spawn) | contact 0, speed 0, motion pointer 0xED070 (motion 2), facing right, root **0x3784E** (blinking: visible 10, empty 8 frames) | VERIFIED (0x36DA8..0x36DD0) |
| 2 (fall) | only while on screen: if it has floor contact -> phase 4, speed 768, root **0x37868**. Else: proximity test (below), y -= speed, speed += 32 up to **1664** (6.5 px/f), floor test 0x3C48, platform landing 0x7232 | VERIFIED (0x36DD4..0x36E1A) |
| 4 (bounce) | only while on screen: animate, y += speed, speed -= 48 down to -1792; when falling and the floor is hit: speed 768 (3 px/f up) and root 0x37868 again -> it hops in place. Proximity test: player x in [x-24, x+24] and player y in [y-28, y+20] sets the latch (`(u16)(px-x+24) <= 48`, `(u16)(py-y+28) <= 48`) | VERIFIED (0x36E20..0x36EC6) |
| join | when latched: if **FF8355 != 0**, FF8355 -= 1 and the HUD item icons are redrawn (0x4CE4) (one Quest-Stone is spent); state 2, phase 0, facing = player facing & 1, walk root table **0x37656** | VERIFIED (0x36E6A..0x36E96; trace C: join at t4250, player 22 px left of it) |

**State 2: companion (phase dispatch table 0x36EE2: 0 0x36F2E, 2 0x370A0, 4
0x37152, 6 0x37256, 8 0x37302, 10 0x373AA)**

Phase 0, **follow**:
- Action 0 (player walking): if the player is standing -> action 2, motion 1.
  Else face the player's facing (new walk root on change), move with motion 2
  (1.5 px/f) and advance the walk animation **twice** per update.
- Action 2 (player standing): every 32 cadence frames, if farther than 40 px
  from the player (`(u16)(px - x + 40) > 80`), face him; every 8 cadence frames,
  if the player walks again -> action 0, motion 2, facing = player facing; else
  keep walking slowly (motion 1, 1 px/f, animation once per update). Result:
  it paces around a standing player.
- Every 16 cadence frames: farther than **72 px** -> phase **2** (3/4, random
  0x1D8E & 3 != 0) or phase **8** (1/4).
- If the player stands on ground and the vertical distance is outside
  [-48, +48] (`(u16)(py - y + 48) > 96`) -> phase **4** (leap to him).
- Every other update: probe the ground 3 px below (y -= 3, floor 0x3C48, then
  platforms 0x7232); if there is none, undo, face the player and phase **6**
  (hop/fall).
- A newly pressed attack button (bit 4) -> phase **10** (throw).

Phase 2, **run to the player**: face him, motion **4** (2.5 px/f), walk
animation **3** steps per update; ground probe 4 px below each update (none ->
fall: y -= 4, phase 6); every 32 cadence frames re-face; when within 40 px ->
back to follow (motion 2).

Phase 4, **leap to the player** (vector jump): vertical speed 1024 (4 px/f up),
vx = (px - x) * 4, vy = (py - y) * 4 (8.8: covers the distance in 64 updates),
facing = sign of vx, jump root **0x3765A** table. Rising: x += vx, y += vy, y +=
speed, speed -= 32 until negative; falling: y -= speed, speed += 32 up to 1536;
the floor is not tested while speed < 896; on landing back to follow (walk
root).

Phase 6, **hop / fall**: motion 3 (2 px/f) if farther than 64 px, else motion
0 (0.5 px/f); speed 512 (2 px/f up); same rise/fall as phase 4 (floor tested
at once when falling); on landing -> follow.

Phase 8, **dash** (1/4 of the "too far" cases): motion 4, timer 32, face the
player, root table **0x3765E** (8 small poses of 4 ticks, then a held pose: a
roll/somersault); 32 updates of running with the ground probe, then 6 updates
of animation; then run (phase 2) if still farther than 40 px, else follow.

Phase 10, **ground throw**: root table **0x37662** (wind-up, held) for **4**
updates, then spawn one projectile (2.5) at the companion's own (x, y) and
facing, root table **0x37666** (throw, held) for **5** updates, then follow.
Trace C: attack pressed t5650 -> phase 10 at t5650, throw at t5655, follow at
t5660. VERIFIED (trace + 0x373AA..0x37428).

Air throw (checked every update in phases 4 and 6, 0x3742A): if attack is
newly pressed and the companion is within 96 px of the player on both axes:
2 updates later spawn a projectile, root table **0x3766A** (air throw) for 5
updates, then the jump root again. VERIFIED (disassembly 0x3743C..0x374BE).

Lifetime: the companion never removes itself (states 4/6 are only a release
path, 0x374C2 -> 0x2C54, and nothing in 0x36D7E sets them). It is never
removed when off screen; it simply is not drawn. It follows the player for the
rest of the stage. What clears it (next stage load, life loss, continue): 
UNKNOWN - check the class-16 pool clears at stage load (0x40374 / 0x9A92
callers) and the life-loss restart. VERIFIED (no self-release) / UNKNOWN (pool
clear).

Sounds: none of its own (no 0x786A call in 0x36D7E; the projectile kind 2 has
no spawn sound). Its hits on enemies give the enemies' normal hit sounds (0x32,
0x33). VERIFIED (code + sound log trace C).

### 2.4 Animation roots (format: stage1_enemies.md section 3; all CPS object palette **4**, flip bit 0x20 for facing left)

| Use | Root (right / left) | Nodes (ticks) | Tag |
|---|---|---|---|
| Appear / hidden-object reveal (blink) | **0x3784E** | 0x3784E 2x2 block code 0x08E2 (10), 0x3785A empty (8), loop | VERIFIED |
| Bounce | **0x37868** | 0x37868 (5), 0x37874 held (2x2 blocks, code 0x08C4..) | VERIFIED |
| Walk, set 0 (table 0x37656) | 0x3766E / 0x376A2 | 4 nodes x 8 ticks, loop (2x2 blocks, first code 0x0884) | VERIFIED |
| Jump, set 1 (0x3765A) | 0x376D6 / 0x376F2 | (4), then held | VERIFIED |
| Dash/roll, set 2 (0x3765E) | 0x3770E / 0x3777E | 8 nodes x 4 ticks (2 pieces each), then a held 2x2 node (0x3776E / 0x377DE) | VERIFIED |
| Wind-up, set 3 (0x37662) | 0x377EE / 0x3780E | held | VERIFIED |
| Throw, set 4 (0x37666) | 0x377FE / 0x3781E | held, 3x2 block (attr 0x1204), code 0x08A4 | VERIFIED |
| Air throw, set 5 (0x3766A) | 0x3782E / 0x3783E | held | VERIFIED |
| Empty (hidden object without stone) | 0x375C4 | terminal, do not follow | VERIFIED |
| Shopkeeper door | 0x32C9C (variant 0/2), 0x32CAC (variant 1) | see 1.1 | VERIFIED |

The animation is advanced by 0x1F4C once per call; the code calls it 2x (follow,
walking) or 3x (run) per update as noted. Extraction lists: `build_stage_objects.py`
(roots 0x3784E, 0x37868, tables 0x37656..0x3766A) and
`build_companion_projectile.py`.

### 2.5 The projectile (class 8 kind **2**, routine **0xEDFA**; `companion_projectile.h`, `src/willow_arcade_actor8_two_three.h`)

| Item | Value | Tag |
|---|---|---|
| Spawn | 0x2B04 allocates from the **player-shot pool** (class 8, FFC2C6, 20 slots, shared with Willow's shots and spells); if full nothing is thrown. +0 = 1, kind 2, facing and x, y = the companion's (no offset). Does not count against Willow's shot limit | VERIFIED (0x37478..0x37496) |
| Init | power **1**, hit box **(8,16,4,8)**, floor probe selector 4, timer 20, motion **0xED0A4**: ±**1152** (4.5 px/f), y 0; root table **0xF3D4** -> 0xF3D8 / 0xF3E8: one node, 2x1 block (32x16) code **0x0880**, palette 4, held | VERIFIED (ROM + `two_three.h`) |
| Flight | 20 updates straight (4.5 px/f); then still moving horizontally it falls: gravity +32 per update up to **1792** (7 px/f), y -= gravity; floor test each update: hitting the floor removes it at once (no effect). Every 4 frames: removed if outside the camera window [-32, +416] x [-16, +272] | SOURCE |
| Hits | tested as a player shot (combat.md steps 4/5) against enemies, surfaces, boss parts; chests and the hidden object test the shot pool themselves. On a hit (state 4): impact root table **0xEFB4** (0xF354 / 0xF394: 4 nodes x 3 ticks + held, 16x16 code 0x0748, palette 8) for 15 updates, or table **0xEFB8** (0xF2EC / 0xF320: 4 x 3 ticks, 2x2, palette 8) for 12 updates when the collision sets phase 2, then removed | SOURCE (`two_three.h`) / nodes VERIFIED |
| Sound | none | VERIFIED |

### 2.6 Native notes
- The companion is a stage object: spawn it from the hidden-object death, keep
  it in its own slot (one per game per FF83A5 bit), update after the player.
- The "Quest-Stone" item (FF8355, hud.md "item 3") is now identified: bought in
  shop 5 (500 gold, max 2), reveals the hidden object and is consumed (one) when
  the companion joins. Resolves stage1_enemies.md open question 8 and the hud.md
  "item 3 use UNKNOWN".

---

## 3. Open questions (UNKNOWN)
1. What removes the companion (stage end, life loss, continue): class-16 pool
   clears; trace a death with the companion present (drop `INVINCIBLE`).
2. FF83DC (companion/actor drift +1 px): writer and stage use.
3. Shop 5 was not entered in a trace (needs the mid-boss killed naturally);
   its flow is the same code with shop index 5.
4. Exact fade timing of 0x1E88/0x1EAA (job 0x0C00/0x0C10, speed 3): trace shows
   ~59 frames out and ~53 frames in with the game frozen; the per-step palette
   math is in 0x509C/0x582C (intro.md 6.2).
5. Whether the stage music command 0x0000 + 0x01FA restarts the track from the
   beginning (audio.md parameter-latch question).
6. Queued job 0x0404 after a 1UP purchase (0x1F14): presumably the lives
   display refresh; read the 0x1F14 dispatch.

## 4. Saved dumps (scratch `.../agents/shop/png/`, rendered with cpsframe.py, CPSFULL=1, 384x224)

| File | Shows |
|---|---|
| a5790.png | stage: Willow jumping onto the ledge next to the shopkeeper (door sprite) at (1872,311), 9 frames before the touch |
| s5900.png | shop screen fading in: 12 icons, owl portrait, "WE..." typing, YES cursor |
| a6570.png | complete welcome text and big YES / NO with the cursor on YES |
| s6130.png | menu: MOONLIGHT MAGIC selected (cursor frame on slot 0), description, G 850 |
| s6350.png | HOLY WATER selected, answer "IT LOOKS LIKE YOU CAN'T HOLD A(NY MORE)" typing |
| s6620.png | Information bought: red X marker on slot 7, gold 55, information text typing |
| s7100.png | BATTLE SWORD selected with Willow: "YOU CAN'T USE IT" |
| s7420.png | farewell "WELL THEN, TAKE CARE." |
| s7550.png | back in the stage fading in, same position, shopkeeper gone |
| b6250.png | magic bought: marker on slot 0, "THANK YOU. DO YO(U...)", gold 49150 |
| b8580.png | after 9 purchases: markers on slots 0,2,4,6,8,9,10, EXIT selected (price area shows only G), armour icons in the HUD bar |
| b8990.png | stage after the second visit: HUD with 4 vitality cells, armour/charm icons, P=2 |
| c4205.png, zoom_c4205.png | the companion appearing at (1248,128) after the hidden object is destroyed (Willow's shot burst next to it) |
| c4300.png, zoom_c4300.png | companion joining among soldiers |
| c5665.png, zoom_c5665.png | companion throwing its spear (projectile in flight) next to Willow |
