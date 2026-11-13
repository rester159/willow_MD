/* Shops (specs/stage1_shops_companion.md 1): the shopkeeper at a shop door,
 * the shop screen (tools/shoppack.py: the arcade's curtains, boxes and the
 * owl's window recomposed to 320 px on plane B; item icons, cursor and
 * bought markers as sprites; the arcade font on plane B rows over the text
 * box), the menu, purchases and the way back into the stage.
 *
 * Native layout (1.4: the 384-px screen does not fit): names on row 20,
 * descriptions and the owl's lines on rows 22-23 from column 1 (re-wrapped
 * at 38 characters), the price's big G at columns 22-23 and six digits
 * from column 24, the big YES / NO on rows 22-23 at x 96 / 208. */
#include <genesis.h>
#include "stagedata.h"
#include "game.h"
#include "actor.h"
#include "coop.h"
#include "player.h"
#include "hud.h"
#include "magic.h"
#include "shot.h"
#include "sound.h"
#include "stage.h"
#include "packed.h"
#include "shop.h"
#include "frontend.h"
#include "../data/shop/shop.h"

extern const u8 shop_tiles[], shop_spr[];
extern const u16 shop_map[], shop_alt[], shop_pal[];   /* shop_map: aPLib-packed (map_keep) */
static const u16 *smap;                     /* the shop's unpacked map words */
static u16 fade_pal[64];             /* the stage's colours, for the way back */

#define SHOP_VRAM 16                /* background tiles: the stage pool and far layer's VRAM */
#define SND_EXIT 0x10FB
#define SND_TYPE 0x2F

u8 shop_used;                       /* FF839A: bit per door, cleared at new game and stage clear */
u8 shop_safe;                       /* FF9896: updates of post-shop safety left */
static u16 price[6];                /* FF8378: persistent prices of kinds 2..7 (BCD) */

/* ---- Items (ROM tables 0x3479A, 0x3439E, 0x34874; names and
 * descriptions 1.9, re-wrapped at 38 columns). */
static const u8 item_kind[28] = { 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 2, 1, 3, 0, 5, 6, 7, 4, 0, 0, 0, 0 };
static const u32 item_price[28] = {
    0x4000, 0x50, 0x10000, 0x8000, 0x850, 0x4500, 0, 0, 0, 0x6000, 0x85000, 0x10000, 0, 0x15000,
    0x2000, 0x500, 0x200, 0, 0x400, 0x1000, 0x1000, 0x1500, 0x500, 0x250, 0x200, 0x2000, 0, 0
};
static const u32 oneup_price[3] = { 0x5000, 0x25000, 0x80000 };
static const u32 pack_price[3] = { 0x2000, 0x5000, 0x10000 };
static const u8 shop_items[18][12] = {
    { 0, 9, 10, 11, 15, 16, 17, 18, 24, 25, 1, 6 }, { 2, 3, 4, 5, 15, 16, 17, 18, 13, 14, 1, 6 },
    { 7, 8, 12, 15, 15, 16, 17, 18, 19, 23, 1, 6 }, { 20, 21, 22, 20, 15, 16, 17, 18, 21, 22, 1, 6 },
    { 0, 0, 23, 21, 16, 18, 17, 1, 22, 20, 7, 6 }, { 0, 0, 23, 15, 16, 18, 17, 1, 22, 20, 7, 6 },
    { 0, 0, 23, 15, 16, 18, 17, 1, 22, 20, 7, 6 }, { 0, 0, 23, 21, 16, 18, 17, 1, 22, 20, 7, 6 },
    { 0, 0, 23, 21, 16, 18, 17, 1, 22, 20, 7, 6 }, { 0, 0, 23, 21, 16, 18, 17, 1, 22, 20, 7, 6 },
    { 0, 0, 23, 15, 16, 18, 17, 1, 22, 20, 7, 6 }, { 0, 0, 23, 21, 16, 18, 17, 1, 22, 20, 7, 6 },
    { 0, 0, 23, 21, 16, 18, 17, 1, 22, 20, 7, 6 }, { 0, 0, 23, 21, 16, 18, 17, 1, 22, 20, 7, 6 },
    { 0, 0, 23, 21, 16, 18, 17, 1, 22, 20, 7, 6 }, { 0, 0, 23, 21, 16, 18, 17, 1, 22, 20, 7, 6 },
    { 0, 0, 23, 26, 16, 18, 27, 1, 26, 26, 26, 6 }, { 0, 0, 23, 26, 16, 18, 27, 1, 26, 26, 26, 6 },
};
static const u8 magic_item[7] = { 0, 4, 14, 5, 3, 2, 13 };     /* 0x33E0A */
static const u8 sword_item[5] = { 0, 25, 0, 9, 11 };           /* 0x33E12 */
static const u8 info1[18] = { 0x51, 0x51, 0x51, 0x51, 0x51, 0x52, 0x60, 0x62, 0x64, 0x70, 0x72, 0x74, 0x76, 0x78, 0x7A, 0x78, 0x78, 0x7A };
static const u8 info2[18] = { 0, 0, 0, 0, 0, 0x53, 0x61, 0x63, 0x65, 0x71, 0x73, 0x75, 0x77, 0x79, 0x7B, 0x79, 0x79, 0x7B };   /* 0x33FF8 */

static const char *const item_name[28] = {
    "DRAGON SWORD", "INFORMATION", "EXPLOSIVE MAGIC", "TIME MAGIC", "MOONLIGHT MAGIC", "GOLD MAGIC",
    "EXIT", "1 UP", "1 UP", "BATTLE BLADE", "", "ULTIMATE SWORD", "", "CRYSTAL MAGIC", "TORNADO MAGIC",
    "QUEST-STONE", "HOLY WATER", "LIFE PACK", "HOLY LIQUOR", "", "CHARM RING", "CHARM BRACELET",
    "CHARM PENDANT", "MAGIC BRACELET", "", "BATTLE SWORD", "", "",
};
static const char *const item_desc[28] = {
    "ATTACK POWER INCREASES.", "I WILL TEACH YOU SOMETHING/INTERESTING.", "IT WILL BLAST THE ENEMY.",
    "TIME CAN BE STOPPED.", "THE LIGHT OF THE MAGIC WILL RIP/THE ENEMY APART.",
    "IT WILL CHANGE THE ENEMY INTO GOLD.", "ARE YOU GOING ALREADY?", "ONE PLAYER IS ADDED.",
    "ONE PLAYER IS ADDED.", "ATTACK POWER INCREASES.", "", "THE PROOF OF A TRUE WARRIOR.", "",
    "THE ULTIMATE MAGIC.", "IT WILL ENGULF THE ENEMY IN A/TORNADO.", "?...",
    "YOU CAN RECOVER 1 POINT OF LIFE.", "LIFE IS EXTENDED ONE POINT.",
    "YOU CAN RECOVER 2 POINTS OF LIFE.", "", "ALLOWS YOU TO AVOID 2 POINTS OF/ENEMY ATTACK.",
    "ALLOWS YOU TO AVOID 3 POINTS OF/ENEMY ATTACK.", "ALLOWS YOU TO AVOID 1 POINT OF/ENEMY ATTACK.",
    "IT WILL SAVE YOU FROM ENEMY MAGIC.", "", "ATTACK POWER INCREASES.", "", "",
};
#define MSG_WELCOME "WELCOME. MAY I HELP YOU?"
#define MSG_THANKS  "THANK YOU. DO YOU NEED ANYTHING ELSE."
#define MSG_FULL    "IT LOOKS LIKE YOU CAN'T HOLD ANY/MORE."
#define MSG_OUT     "WE HAVE RUN OUT OF GOODS."
#define MSG_GOLD    "YOU DON'T HAVE ENOUGH GOLD."
#define MSG_CANT    "YOU CAN'T USE IT."
#define MSG_BYE     "WELL THEN, TAKE CARE."

/* INFORMATION's lines (messages 0x51-0x53, 0x60-0x7B; ROM 0x71A98..: from
 * 0x52 on two 80-byte texts and 8 bytes a pair, 0x71AF0 + (id - 0x52) / 2 x
 * 168, + 80 for the second). '&' breaks a line; the arcade wraps at 40
 * columns, the shop's rows here hold 39 (word wrap). */
static const char *info_text(u8 id)
{
    switch (id)
    {
    case 0x51: return "FIRST, WE MUST MEET WITH THE WARRIOR.";
    case 0x52: return "THE QUEST-STONE IS A LEGENDARY ROCK.";
    case 0x53: return "THE PERSON WHO HOLDS IT WILL BE BLESSED/WITH GOOD LUCK.";
    case 0x60: return "THIS TAVERN IS SURROUNDED.";
    case 0x61: return "YOU SHOULD GET ON THE WAGON AND/RUN AWAY.";
    case 0x62: return "THE SERGEANT OF THE NOCKMAAR TROOPS/WILL HAVE UNGUARDED MOMENTS";
    case 0x63: return "WHEN ATTACKING.";
    case 0x64: return "I HAVE HEARD THAT A WITCH LIVES ON/FIN RAZIEL ISLAND.";
    case 0x65: return "DO YOU HAVE THE MAGIC BRACELET?";
    case 0x70: return "A DRAGON LIVES IN THE BACK OF THE CAVE.";
    case 0x71: return "BEWARE OF HIS FLAMES.";
    case 0x72: return "USE A SLED TO GO DOWN THE MOUNTAIN.";
    case 0x73: return "AVOID THE BOULDER BY JUMPING.";
    case 0x74: return "LOOK FOR THE BROWNIE.";
    case 0x75: return "HE SHOULD BE A STRONG ALLY.";
    case 0x76: return "THE TWO HEADED DRAGON IS A FEARFUL/CREATURE.";
    case 0x77: return "ARE YOU CAPABLE OF EXPLOSIVE MAGIC?";
    case 0x78: return "THE CORRIDOR IS A MAZE.";
    case 0x79: return "LOOK FOR THE TRUE DOOR.";
    case 0x7A: return "BEWARE OF BAVMORDA'S MAGIC.";
    case 0x7B: return "WITH HER MAGIC,SHE CAN ALSO BLOW LIFE/INTO OBJECTS.";
    }
    return "";
}

/* ---- Visit state (FF0Fxx). */
enum { S_ENTER, S_WAIT, S_ASK, S_MENU, S_INFO, S_LEAVE, S_DONE };
static u8 step, which, col, row, markers, owl_mode, owl_frame, owl_shown, owl_delay, owl_timer, info_next;
static u16 bought, timer;
static u8 items[12];
static u32 snap[6], snap_1up, snap_pack;
static s8 yes_x;                    /* YES/NO cursor: 0 YES, 1 NO, -1 hidden */
static u8 marker_slot[12];
static u16 spr_base;                /* VRAM of the 12 icons, then the 8 pieces */
static VDPSprite icon_spr[48];
static u16 icons_n;

/* Typed text (style 0x1300 / 0x1301): 3 updates a character, sound 0x2F. */
static const char *type_text;
static u8 type_row, type_at, type_tick;

static void text_clear(u16 r) { hud_text("", 0, 0, r); }

static void type_start(const char *t, u16 r)
{
    type_text = t; type_row = r; type_at = 0; type_tick = 1;
}

static bool typing(void) { return type_text != NULL; }

static void type_update(void)
{
    const char *t = type_text, *p;
    if (!t || --type_tick) return;
    type_tick = 3;
    type_at++;
    sound_post(SND_TYPE);
    for (p = t; *p && *p != '/'; p++) ;
    if (type_at <= p - t) hud_text(t, type_at, 1, type_row);
    else hud_text(p + 1, type_at - (p - t) - 1, 1, type_row + 1);
    if (!t[type_at]) type_text = NULL;
}

/* ---- Prices (BCD, 4 bytes). */
static u32 bcd_add(u32 a, u32 b)
{
    u32 r = 0, c = 0;
    u16 i;
    for (i = 0; i < 32; i += 4)
    {
        u32 d = ((a >> i) & 15) + ((b >> i) & 15) + c;
        c = d > 9;
        if (c) d -= 10;
        r |= d << i;
    }
    return r;
}

static bool bcd_sub(u32 *a, u32 b)                  /* FALSE on a borrow: *a unchanged */
{
    u32 r = 0;
    s16 c = 0;
    u16 i;
    for (i = 0; i < 32; i += 4)
    {
        s16 d = (s16)((*a >> i) & 15) - (s16)((b >> i) & 15) - c;
        c = d < 0;
        if (c) d += 10;
        r |= (u32)d << i;
    }
    if (c) return FALSE;
    *a = r;
    return TRUE;
}

static u32 item_cost_arcade(u16 item)
{
    u16 k = item_kind[item];
    if (k == 1) return item == 17 ? snap_pack : snap_1up;
    if (k >= 2) return snap[k - 2];
    return item_price[item];
}

/* NEW GAME PLUS (Home, AGENTS.md): prices doubled. */
static u32 item_cost(u16 item)
{
    u32 v = item_cost_arcade(item);
    return fe.plus ? bcd_add(v, v) : v;
}

static void glyph(u16 g, u16 x, u16 y)              /* a 2x2 big glyph on rows y, y+1 */
{
    u16 w[2];
    w[0] = shop_glyph[g][0] + SHOP_VRAM; w[1] = shop_glyph[g][1] + SHOP_VRAM;
    stage_bput(x, y, w, 2);
    w[0] = shop_glyph[g][2] + SHOP_VRAM; w[1] = shop_glyph[g][3] + SHOP_VRAM;
    stage_bput(x, y + 1, w, 2);
}

static void put_chars(u16 *w, const char *t, const char *end)
{
    u16 x = 1;
    for (; t < end && *t && x < 40; t++, x++) { u16 g = hud_glyph(*t); if (g) w[x] = g; }
}

/* Name, description, price (0x346E0, 0x348FC): rows 20-23 composed and
 * written once each. */
static void show_item(void)
{
    static u16 w[4][40];
    u16 s = (row * 4 + col) >> 1, item = items[s], i, r;
    u32 v = item == 6 ? 0 : item_cost(item);
    bool lead = TRUE;
    const char *d = item_desc[item], *p = d;
    type_text = NULL;
    for (r = 0; r < 4; r++) stage_brow(20 + r, w[r]);
    put_chars(w[0], item_name[item], item_name[item] + 40);
    while (*p && *p != '/') p++;
    put_chars(w[2], d, p);
    if (*p) put_chars(w[3], p + 1, p + 41);
    for (i = 0; i < 7; i++)
    {
        u16 g = 0, x = 22 + i * 2;
        if (i)
        {
            u16 dg = (v >> ((6 - i) * 4)) & 15;
            if (item == 6) break;
            if (dg || i == 6) lead = FALSE;
            if (lead) continue;
            g = 1 + dg;
        }
        w[0][x] = shop_glyph[g][0] + SHOP_VRAM; w[0][x + 1] = shop_glyph[g][1] + SHOP_VRAM;
        w[1][x] = shop_glyph[g][2] + SHOP_VRAM; w[1][x + 1] = shop_glyph[g][3] + SHOP_VRAM;
    }
    for (r = 0; r < 4; r++) stage_bput(0, 20 + r, w[r], 40);
}

/* ---- The owl (0x34AD4): blink, or talk after a purchase attempt. */
static void owl_show(u16 f)
{
    static u16 w[40];
    u16 k = 0;
    while (k < SHOP_OWL_CELLS)
    {
        u16 r = shop_owl_cell[k] / 40, c0 = shop_owl_cell[k] % 40, n = 0;
        while (k < SHOP_OWL_CELLS && shop_owl_cell[k] / 40 == r)
        {
            u16 c = shop_owl_cell[k] % 40;
            while (c0 + n < c) { w[n] = smap[r * 40 + c0 + n] + SHOP_VRAM; n++; }
            w[n++] = (f ? shop_alt[(f - 1) * SHOP_OWL_CELLS + k] : smap[r * 40 + c]) + SHOP_VRAM;
            k++;
        }
        stage_bput(c0, r, w, n);
    }
}

static void owl_talk(void) { owl_mode = 1; owl_timer = 63; owl_delay = 7; owl_frame = 0; }

static void owl_update(void)
{
    if (!owl_timer--)
    {
        owl_timer = 62; owl_mode = 0; owl_delay = 7; owl_frame = 0;
    }
    else if (!--owl_delay)
    {
        if (owl_mode) { owl_frame ^= 1; owl_delay = 10; }
        else { owl_frame ^= 2; owl_delay = owl_frame ? 20 : 40; }
    }
    if (owl_frame != owl_shown) owl_show(owl_shown = owl_frame);
}

/* ---- Purchases (0x3436E). */
static const char *buy(u16 s)
{
    u16 item = items[s], k = item_kind[item];
    u32 cost;
    if (item == 10 || item == 12 || item == 24 || item == 26 || item == 27) return NULL;
    if (item != 23 && (bought & (1 << s))) return MSG_OUT;
    owl_talk();
    switch (item)                                   /* capacity, then character */
    {
    case 23: if (hud.items[11] + 1 > 3) return MSG_FULL; break;
    case 21: if (hud.items[9]) return MSG_FULL; break;
    case 20: if (hud.items[8]) return MSG_FULL; break;
    case 22: if (hud.items[10]) return MSG_FULL; break;
    case 16: if (hud.hp + 1 > hud.max_hp) return MSG_FULL; break;
    case 18: if (hud.hp + 1 > hud.max_hp) return MSG_FULL; break;
    case 17: if (hud.max_hp + 1 > 6) return MSG_FULL; break;
    case 15: if (hud.items[3] >= 2) return MSG_FULL; break;
    case 25: case 0: case 9: case 11: if (!player.form) return MSG_CANT; break;          /* Madmartigan's swords */
    case 4: case 14: case 5: case 3: case 2: case 13: if (player.form) return MSG_CANT; break;   /* Willow's magic */
    }
    cost = item_cost(item);
    if (!bcd_sub(&hud.gold, cost)) return MSG_GOLD;
    if (k >= 2) price[k - 2] = bcd_add(price[k - 2], item_price[item]);
    hud_dirty |= HUD_GOLD;
    bought |= 1 << s;
    switch (item)
    {
    case 4: case 14: case 5: case 3: case 2: case 13:
        {
            u16 l;
            for (l = 1; l < 7 && magic_item[l] != item; l++) ;
            magic.level = hud.magic = l;
            hud_dirty |= HUD_BAR;
        }
        break;
    case 25: case 0: case 9: case 11:
        {
            u16 l;
            for (l = 1; l < 5 && sword_item[l] != item; l++) ;
            sword_level = l;                        /* FF8367; sword HUD, charge reset */
            magic.hold = 0;
            hud_dirty |= HUD_BAR;
        }
        break;
    case 23: hud.items[11]++; hud_dirty |= HUD_ITEMS; if (hud.items[11] < 3) bought &= ~(1 << s); break;
    case 21: hud.items[9] = 3; hud.armor += 3; hud_dirty |= HUD_ITEMS | HUD_VITAL; break;
    case 20: hud.items[8] = 2; hud.armor += 2; hud_dirty |= HUD_ITEMS | HUD_VITAL; break;
    case 22: hud.items[10] = 1; hud.armor += 1; hud_dirty |= HUD_ITEMS | HUD_VITAL; break;
    case 16: hud.hp++; hud.items[4]++; hud_dirty |= HUD_VITAL; break;
    case 18: hud.hp += hud.hp + 2 <= hud.max_hp ? 2 : 1; hud.items[6]++; hud_dirty |= HUD_VITAL; break;
    case 17: hud.max_hp++; hud.items[5]++; hud_dirty |= HUD_VITAL; break;
    case 7: case 8:
        hud.items[0]++;
        if (hud.lives != 0x99) hud.lives = (hud.lives & 15) == 9 ? hud.lives + 7 : hud.lives + 1;
        break;
    case 15: hud.items[3]++; hud_dirty |= HUD_ITEMS; break;
    case 1:
        text_clear(22); text_clear(23);
        type_start(info_text(info1[which]), 22);
        info_next = info2[which];
        step = S_INFO;
        timer = 0;
        break;
    }
    if (bought & (1 << s)) marker_slot[markers++] = s;
    player.hp_cache = hud.hp;                       /* no hurt reaction for the shop's changes */
    player.armor_cache = hud.armor;
    return item == 1 ? NULL : MSG_THANKS;
}

/* ---- Entering and leaving. */
void shop_new_game(void)
{
    shop_used = 0;
    price[0] = 0x400; price[1] = 0x800; price[2] = 0x250; price[3] = 0x1000; price[4] = 0x1500; price[5] = 0x500;
}

void shop_enter(u16 index)
{
    which = index;
    step = S_ENTER;
    game_mode = GM_SHOP;
    magic.hold = 0;                                 /* a charge in progress is lost */
    magic.meter = 0;
    sound_post(SND_CHARGE_END);
    sound_post(SND_EXIT);
}

static void build_screen(void)
{
    u16 lo[8], hi[8], n, i, best = 0, s;
    smap = map_keep(shop_map);
    stage_bpicture(smap, SHOP_VRAM);         /* first: the scroll splits stop (the plane fill below must not be interrupted) */
    VDP_clearPlane(BG_A, TRUE);
    load_packed(shop_tiles, SHOP_VRAM, SHOP_TILES);   /* aPLib (tools/pack_compress.sh) */
    /* Sprite art: the pose cache's largest range (forgotten on the way out). */
    n = posecache_ranges(lo, hi);
    for (i = 1; i < n; i++) if (hi[i] - lo[i] > hi[best] - lo[best]) best = i;
    spr_base = lo[best];
    posecache_flush();
    for (s = 0; s < 12; s++)
        VDP_loadTileData((const u32 *)(shop_spr + (u32)items[s] * 512), spr_base + s * 16, 16, DMA);
    VDP_loadTileData((const u32 *)(shop_spr + 28 * 512), spr_base + 192, 32, DMA);
    hud_dirty = 0xFF;
    icons_n = 0;
    VDP_setBackgroundColor(0);
}

static void sprites_off(void)                       /* the old sprite table must not show through a blocking fade */
{
    *(vu32 *)VDP_CTRL_PORT = VDP_WRITE_VRAM_ADDR(0xAC00);
    *(vu32 *)VDP_DATA_PORT = 0;
    *(vu32 *)VDP_DATA_PORT = 0;
}

static void restore_stage(void)
{
    sprites_off();
    stage_bpicture(NULL, 0);
    stage_restart(player.cam_x + 32, 272 - player.cam_y);
    posecache_flush();
    shot_init(magic.level);
    hud_dirty = 0xFF;
    VDP_setBackgroundColor(sd->sky);              /* colours: faded in from fade_pal */
}

/* Once per update in GM_SHOP. TRUE when the visit is over. */
bool shop_update(u16 held, u16 pressed)
{
    u16 s;
    (void)held;
    hud.clock_lock = 0;                             /* the clock runs in the shop (1.8) */
    if (step >= S_ASK && step <= S_LEAVE && !hud.min && !hud.sec)
    {
        /* Time up inside the shop (0x956E): back to the stage at once,
         * where play runs the normal time-up path. */
        PAL_fadeOutAll(30, FALSE);
        restore_stage();
        step = S_DONE;
        timer = 1;
    }
    type_update();
    switch (step)
    {
    case S_ENTER:                                   /* 1.3 entry 1-5 */
        PAL_getColors(0, fade_pal, 64);
        PAL_fadeOutAll(59, FALSE);
        sound_post(SND_CHARGE_END);
        sound_post(SND_MUSIC_SHOP);
        bought = 0; markers = 0;
        for (s = 0; s < 6; s++) snap[s] = price[s];
        snap_1up = oneup_price[hud.items[0] < 2 ? hud.items[0] : 2];
        snap_pack = pack_price[hud.items[5] < 2 ? hud.items[5] : 2];
        memcpy(items, shop_items[which], 12);
        items[0] = magic_item[magic.level + 1 < 6 ? magic.level + 1 : 6];
        items[1] = sword_item[sword_level + 1 < 4 ? sword_level + 1 : 4];
        if (items[10] == 7 && player.form) items[10] = 8;    /* 1 UP of the current character */
        if (magic.level >= 6) { bought |= 1; marker_slot[markers++] = 0; }
        if (sword_level >= 4) { bought |= 2; marker_slot[markers++] = 1; }
        build_screen();
        step = S_WAIT;
        timer = 32;
        return FALSE;
    case S_WAIT:
        if (--timer) return FALSE;
        col = row = 0;
        yes_x = 0;
        owl_mode = 0; owl_frame = owl_shown = 0; owl_delay = 7; owl_timer = 62;
        type_start(MSG_WELCOME, 20);
        glyph(11, 12, 22); glyph(12, 14, 22); glyph(13, 16, 22);      /* YES */
        glyph(14, 26, 22); glyph(15, 30, 22);                          /* NO */
        hud_update();
        {
            static u16 pal[64];
            memcpy(pal, shop_pal, sizeof pal);
            SYS_doVBlankProcess();
            PAL_fadeInAll(pal, 30, FALSE);
        }
        step = S_ASK;
        return FALSE;
    case S_ASK:                                     /* YES / NO (0x33E7C) */
        owl_update();
        if (pressed & BUTTON_RIGHT) yes_x = 1;
        else if (pressed & BUTTON_LEFT) yes_x = 0;
        else if (pressed & (BUTTON_A | BUTTON_B))
        {
            type_text = NULL;
            text_clear(20); text_clear(21); text_clear(22); text_clear(23);
            if (yes_x) goto leave;
            yes_x = -1;
            show_item();
            step = S_MENU;
        }
        return FALSE;
    case S_MENU:                                    /* 0x34272: one action, right left down up button */
        owl_update();
        if (pressed & BUTTON_RIGHT) col = (col + 2) & 6;
        else if (pressed & BUTTON_LEFT) col = (col - 2) & 6;
        else if (pressed & BUTTON_DOWN) row = row >= 4 ? 0 : row + 2;
        else if (pressed & BUTTON_UP) row = row ? row - 2 : 4;
        else if (pressed & (BUTTON_A | BUTTON_B))
        {
            const char *m;
            s = (row * 4 + col) >> 1;
            if (items[s] == 6) goto leave;
            m = buy(s);
            if (m) { text_clear(22); text_clear(23); type_start(m, 22); }
            return FALSE;
        }
        else return FALSE;
        show_item();
        return FALSE;
    case S_INFO:                                    /* 0x33F84 */
        owl_update();
        if (typing()) return FALSE;
        if (!info_next) { step = S_MENU; return FALSE; }
        if (++timer < 31) return FALSE;
        text_clear(22); text_clear(23);
        type_start(info_text(info_next), 22);
        info_next = 0;
        timer = 0;
        return FALSE;
    case S_LEAVE:                                   /* 0x33F42 */
        if (typing()) return FALSE;
        if (--timer) return FALSE;
        sound_post(SND_EXIT);
        PAL_fadeOutAll(30, FALSE);
        restore_stage();
        step = S_DONE;
        timer = 2;
        return FALSE;
    case S_DONE:
        if (--timer) return FALSE;
        shop_safe = 96;                             /* 0x965E */
        player.vuln = 2;
        game_stage_music();                         /* the stage's music again */
        SYS_doVBlankProcess();
        PAL_fadeInAll(fade_pal, 30, FALSE);
        PAL_setColors(0, stage_sprpal(), 32, DMA);
        return TRUE;
    }
    return FALSE;
leave:
    text_clear(20); text_clear(21); text_clear(22); text_clear(23);
    yes_x = -1;
    col = 4; row = 6;                               /* the item cursor hidden */
    type_start(MSG_BYE, 22);
    timer = 63;
    step = S_LEAVE;
    return FALSE;
}

/* The shop's sprites: cursor, bought markers, icons (four 16x16 pieces
 * each, every piece on its own CRAM line); then the HUD's lives. */
static u16 piece(VDPSprite *sp, u16 tile, u16 line, s16 x, s16 y, u16 link)
{
    sp->y = 128 + y;
    sp->size = SPRITE_SIZE(2, 2);
    sp->attribut = TILE_ATTR_FULL(line, 0, 0, 0, tile);
    sp->x = 128 + x;
    sp->link = link;
    return 1;
}
#define PIECE(k, x, y) piece(sat + first + n, spr_base + 192 + (k) * 4, shop_piece_line[k], x, y, first + n + 1)

bool shop_screen(void) { return step != S_DONE; }    /* FALSE: the stage is back (fading in) */

u16 shop_draw(VDPSprite *sat, u16 first)
{
    u16 n = 0, s, m, q;
    if (step == S_ENTER || step == S_WAIT || step == S_DONE) return 0;
    if (step == S_MENU || step == S_INFO)
    {
        s16 x = shop_icon_x[col >> 1], y = shop_icon_y[row >> 1];
        n += PIECE(0, x, y); n += PIECE(1, x + 16, y); n += PIECE(2, x, y + 16); n += PIECE(3, x + 16, y + 16);
    }
    else if (yes_x >= 0 && step == S_ASK)
    {
        s16 x = yes_x ? 208 : 96;
        /* All four at one y (0x34A10): the upper and lower corners of a 16-px bracket. */
        n += PIECE(0, x, 176); n += PIECE(2, x, 176); n += PIECE(1, x + 32, 176); n += PIECE(3, x + 32, 176);
    }
    for (m = 0; m < markers; m++)
    {
        s16 x = shop_icon_x[marker_slot[m] & 3], y = shop_icon_y[marker_slot[m] >> 2];
        n += PIECE(4, x, y); n += PIECE(5, x + 16, y); n += PIECE(6, x, y + 16); n += PIECE(7, x + 16, y + 16);
    }
    if (!icons_n)                                   /* the icons never move: prepared once a visit */
        for (s = 0; s < 12; s++)
        {
            if (items[s] == 26) continue;
            for (q = 0; q < 4; q++)
                icons_n += piece(icon_spr + icons_n, spr_base + s * 16 + q * 4, shop_icon_line[items[s]][q],
                                 shop_icon_x[s & 3] + (q & 1) * 16, shop_icon_y[s >> 2] + (q >> 1) * 16, 0);
        }
    memcpy(sat + first + n, icon_spr, icons_n * sizeof(VDPSprite));
    for (q = 0; q < icons_n; q++, n++) sat[first + n].link = first + n + 1;
    return n + hud_draw(sat, first + n);
}

/* ---- The shopkeeper at a shop door (class 12 kind 8, 0x32BE6).
 * Door type (variant low nibble) 0, stage 1: the shopkeeper stands at the
 * door and touching him (+-12 x, +-14 y) opens the shop. Type 1, stage 2's
 * door (stage1_shops_companion.md 1.1): nothing shows; standing at it
 * (+-16 x, +-32 y) with Up held and neither Left nor Right starts a 30-update
 * countdown with the player not hittable, then the shop. The arcade shows
 * the door opening then (root 0x32CAC, not packed: the door stays as drawn). */
void shopkeeper_update(Actor *a)
{
    s16 px = player.x >> 8, py = player.y >> 8;
    bool type1 = (a->variant & 15) == 1;
    if (!a->state)
    {
        if (shop_used & (1 << (a->variant >> 4))) { actor_free(a); return; }
        a->state = 2;
        a->active = type1 ? 0 : 2;
        a->pose = 0;
        a->timer = 0;
    }
    if (a->timer)
    {
        player.vuln = 2;
        if (!--a->timer) { shop_enter(a->facing); actor_free(a); }
        return;
    }
    if (!hud.hp || game_mode != GM_PLAY) return;
    if (type1)
    {
        u16 held = player.last_held;
        if ((u16)(px - AX(a) + 16) <= 32 && (u16)(py - AY(a) + 32) <= 64 &&
            (held & BUTTON_UP) && !(held & (BUTTON_LEFT | BUTTON_RIGHT)))
        {
            shop_used |= 1 << (a->variant >> 4);
            player.vuln = 2;
            a->timer = 30;
        }
        return;
    }
    if ((u16)(px - AX(a) + 12) <= 24 && (u16)(py - AY(a) + 14) <= 28)
    {
        shop_used |= 1 << (a->variant >> 4);
        shop_enter(a->facing);
        actor_free(a);
    }
}
