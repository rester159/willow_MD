/* Stage HUD (specs/hud.md; approved cut 2: native HUD from game values,
 * original font, Genesis layout).
 *
 * The arcade's 48-column text layer is folded into 40 columns: the left
 * group keeps its columns, the middle group moves 1 left, the right group 7
 * left (TIME 6, to keep a gap after the gold). Top rows: sprites, with the
 * priority bit, first in the table, so the HUD is over the foreground and
 * other sprites as the arcade's text layer is (see top_put). Bottom rows: on
 * plane B, the arcade's opaque band (charge bar, items). The reserve lives are a transparent overlay inside
 * the scene, so they are sprites. Every element is redrawn only when its
 * value changes, as in the arcade (4A34, 4E32, 4F18, 6284).
 *
 * Glyph tiles are coloured offline for the stage's CRAM (tools/hudpack.py);
 * each tile carries the line it was coloured for. */
#include <genesis.h>
#include "stagedata.h"
#include "hud.h"
#include "stage.h"
#include "sound.h"
#include "player.h"
#include "coop.h"
#include "frontend.h"
#include "actor.h"
#include "shot.h"
#include "../data/hud/hud_tiles.h"
#include "../data/stage1/layout.h"

extern const u8 hud_tiles[], hud2_tiles[], hud3_tiles[], hud4_tiles[], hud5_tiles[], hud6_tiles[];
extern const u8 hud_partial[], hud2_partial[], hud3_partial[], hud4_partial[], hud5_partial[], hud6_partial[];   /* partial-fill tiles [palette][fill 0..6][top, bottom] */
extern const u8 hud_partline[], hud2_partline[], hud3_partline[], hud4_partline[], hud5_partline[], hud6_partline[];  /* CRAM line per partial palette */
/* The stage's colouring of the HUD (tools/hudpack.py; stage 2 keeps stage
 * 1's lines, LINES_FROM, so the HT_ constants serve both). */
static const u8 *ht_tiles = hud_tiles, *ht_partial = hud_partial, *ht_partline = hud_partline;

static u8 sword_shown = 0xFF;   /* sword level whose icon is in VRAM */

void hud_stage(u16 stage)
{
    sword_shown = 0xFF;
    ht_tiles = stage == 5 ? hud6_tiles : stage == 4 ? hud5_tiles : stage == 3 ? hud4_tiles : stage == 2 ? hud3_tiles : stage ? hud2_tiles : hud_tiles;
    ht_partial = stage == 5 ? hud6_partial : stage == 4 ? hud5_partial : stage == 3 ? hud4_partial : stage == 2 ? hud3_partial : stage ? hud2_partial : hud_partial;
    ht_partline = stage == 5 ? hud6_partline : stage == 4 ? hud5_partline : stage == 3 ? hud4_partline : stage == 2 ? hud3_partline : stage ? hud2_partline : hud_partline;
}

Hud hud;
extern u16 game_frame;
u8 hud_dirty;
static u16 vbase;

#define W(ht) (0x8000 | ((ht) & 0x6000) | (vbase + ((ht) & 0x1FFF)))

#define HUD_SKY sd->sky       /* CRAM entry of the stage's sky (stagepack) */
u8 hud_keep_backdrop;      /* set: hud_init leaves the backdrop (a reload with the display off) */
#define ROW_BAND_T 26           /* plane B rows of the bottom band */
#define ROW_BAND_B 27

/* Charge bar (4FBE): cells per magic level, and each cell's colour group
 * (table 0x4C36 minus palette 0x11, plus the level). */
static const u8 bar_cells[7] = { 18, 21, 23, 24, 24, 24, 24 };
static const u8 bar_group[24] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6 };
static const u16 bar_tiles[13] = {
    HT_BAR0_ET, HT_BAR1_ET, HT_BAR2_ET, HT_BAR3_ET, HT_BAR4_ET, HT_BAR5_ET, HT_BAR6_ET,
    HT_BAR7_ET, HT_BAR8_ET, HT_BAR9_ET, HT_BARA_ET, HT_BARB_ET, HT_BARC_ET
};

/* Item slots (4CDA, 4DD0): the item each slot shows, the count it needs
 * and its icon. */
static const u8 slot_item[9] = { 10, 8, 9, 7, 3, 3, 11, 11, 11 };
static const u8 slot_need[9] = { 1, 1, 1, 1, 1, 2, 1, 2, 3 };
static const u16 icon_tiles[6] = { HT_ICON0_TL, HT_ICON1_TL, HT_ICON2_TL, HT_ICON3_TL, HT_ICON4_TL, HT_ICON5_TL };

/* VRAM holds the HUD's tiles up to HT_RESIDENT: the charge bar's seven
 * colour groups sit in the slots of groups 0-6 and are reloaded with groups
 * level..level+6 when the magic level changes; the item icons have a block
 * of their own (hud_icons). */
static u8 bar_lvl;
static u16 icon_base;
#define BW(grp, off) (0x8000 | (bar_tiles[grp] & 0x6000) | (vbase + (bar_tiles[(grp) - bar_lvl] & 0x1FFF) + (off)))
#define IW(ht) (0x8000 | ((ht) & 0x6000) | (icon_base + ((ht) & 0x1FFF) - (HT_ICON0_TL & 0x1FFF)))

/* The bar's colour level: Willow's magic level, or Madmartigan's sword
 * level (4B0E, hud.md 3.6); his bar is always 18 cells. */
static u16 bar_level(void) { return player.form ? sword_level : hud.magic; }
static u16 bar_len(void) { return player.form ? 18 : bar_cells[hud.magic]; }

static void bar_load(void)
{
    u16 i;
    if (bar_level() == bar_lvl) return;
    bar_lvl = bar_level();
    for (i = 0; i < 7; i++)
        DMA_queueDma(DMA_VRAM, (void *)(ht_tiles + (u32)(bar_tiles[bar_lvl + i] & 0x1FFF) * 32),
                     (vbase + (bar_tiles[i] & 0x1FFF)) * 32, 4 * 16, 2);
}

void hud_icons(u16 vram)                    /* the item icons' block (24 tiles), then the sword icon (8) */
{
    icon_base = vram;
    sword_shown = 0xFF;
    VDP_loadTileData((const u32 *)(ht_tiles + (u32)(HT_ICON0_TL & 0x1FFF) * 32), vram, 24, DMA);
}
static const u8 slot_icon[9] = { 0, 1, 2, 3, 4, 4, 5, 5, 5 };
#define ITEM_COL 26
#define ITEM_SLOTS 7            /* columns 26-39; the arcade has room for 9 */

/* The HUD's rows (0, 1, 26, 27) live in plane B's spare ring rows
 * (src/stage.c): keep their glyphs, the stage lays them out. */
static void top_put(u16 r, u16 x, u16 n);

static void row(u16 y, u16 x, u16 *w, u16 n)
{
    u16 k = y < 2 ? y : y - 24;
    memcpy(stage_hudrow[k] + x, w, n * 2);  /* top rows: 0 = nothing there */
    if (k < 2) top_put(k, x, n);
    stage_hudput(k, x, n);
}

/* ---- The top rows as sprites (user decision 2026-11-05): plane B then shows
 * the far layer to the top of the screen. Each cell the HUD can fill has its
 * own VRAM tile holding a copy of the glyph (top_slot), so runs of cells with
 * consecutive tiles are single sprites up to 4 cells wide; empty cells cost
 * nothing. A sprite has one CRAM line, so a run also ends where the line
 * changes. Rebuilt only when the shape changes; the list stays at the
 * table's start, so the HUD is over every other sprite and first in each
 * line's budget. The slots: the yellow and red glyphs' and gems' own VRAM
 * (only these rows use them, the copies come from ROM), the old window text
 * band (unused since the story text became sprites) with the 12 tiles before
 * it, and 2 of the 4 by the scroll table: the pose cache only loses two small
 * ranges a pose rarely fits. */
#define TOP_SPRITES 40
static u16 top_base;                 /* set: the slots are mapped */
static u8 top_plane;                 /* the rows are on plane B here (src/stage.c hud_plane) */
static u16 top_slot[2][40];          /* (row, column) -> VRAM tile; 0: never used */
static u16 top_shown[2][40];         /* the words whose pixels are in VRAM */
static VDPSprite top_sat[TOP_SPRITES];
static u8 top_n, top_dirty;

/* Columns that need a slot, as [from, to) pairs: 1P's. 1UP and 2UP (columns
 * 1-3, 29-31) use the blue glyphs' own tiles (slot 0: "UP" is consecutive
 * there), which only costs a sprite each. */
static void top_put(u16 r, u16 x, u16 n);

void hud_top_vram(void)
{
    static const u8 cols[2][6] = { { 4, 12, 18, 28, 32, 40 }, { 1, 22, 23, 29, 30, 39 } };
    const u16 from[3] = { vbase + (HT_YEL_0 & 0x07FF), HUD_TOP_LOW, HUD_TOP_LOW2 };
    const u16 to[3] = { vbase + (HT_BOSS_CELL & 0x07FF), HUD_TOP_LOW + HUD_TOP_LOWN, HUD_TOP_LOW2 + HUD_TOP_LOW2N };
    u16 r, k, c, t = 0, slot = from[0];
    top_base = 1;
    memset(top_shown, 0, sizeof top_shown);
    memset(top_slot, 0, sizeof top_slot);
    for (r = 0; r < 2; r++)
        for (k = 0; k < 6; k += 2)
            for (c = cols[r][k]; c < cols[r][k + 1]; c++)
            {
                if (slot == to[t]) slot = from[++t];
                top_slot[r][c] = slot++;
            }
    top_n = 0;
    top_dirty = 1;
    top_put(0, 0, 40);
    top_put(1, 0, 40);
}


/* Changed cells' tiles, copied by the CPU in VBlank: a SGDK queue entry each
 * cost ~600 cycles of frame time (a score change is several digits). */
static const u32 *top_src[80];
static u16 top_dst[80];
static u16 top_np;

void hud_vblank(void)
{
    vu32 *ctrl = (vu32 *)VDP_CTRL_PORT, *data = (vu32 *)VDP_DATA_PORT;
    u16 i, ints;
    if (!top_np) return;
    ints = SYS_getAndSetInterruptMaskLevel(7);
    for (i = 0; i < top_np; i++)
    {
        const u32 *s = top_src[i];
        *ctrl = VDP_WRITE_VRAM_ADDR((u32)top_dst[i]);
        *data = s[0]; *data = s[1]; *data = s[2]; *data = s[3];
        *data = s[4]; *data = s[5]; *data = s[6]; *data = s[7];
    }
    top_np = 0;
    SYS_setInterruptMaskLevel(ints);
}

static void top_put(u16 r, u16 x, u16 n)
{
    u16 *o = top_shown[r] + x;
    const u16 *g = stage_hudrow[r] + x;
    const u16 *slot = top_slot[r] + x;
    if (!top_base || top_plane) return;
    for (; n; n--, o++, g++, slot++)
    {
        u16 w = *g;
        if (w == *o) continue;
        if (*slot && ((w ^ *o) & 0x07FF))
        {
            if (w && top_np < 80) { top_src[top_np] = (const u32 *)(ht_tiles + (u32)((w & 0x07FF) - vbase) * 32); top_dst[top_np++] = *slot * 32; }
        }
        /* A new glyph in a cell that stays filled, in the same line, needs
         * no new sprites (nearly every change: rebuilding costs ~10 lines). */
        if (!w || !*o || ((w ^ *o) & 0x6000)) top_dirty = 1;
        *o = w;
    }
}

/* Sprites or plane B: on plane B the glyphs' own tiles are back in VRAM. */
void hud_top_mode(bool plane)
{
    hud_top_mode_tm(plane, DMA);
}

/* tm DMA_QUEUE: the plane mode's tiles in the coming VBlank (in play: an
 * immediate 1 KB DMA in the frame stalls the CPU ~57 lines, a late frame
 * that starts main.c's deadline rule). */
void hud_top_mode_tm(bool plane, u8 tm)
{
    if (plane == top_plane) return;
    top_plane = plane;
    top_np = 0;
    memset(top_shown, 0, sizeof top_shown);
    top_dirty = 1;
    if (plane)
        VDP_loadTileData((const u32 *)(ht_tiles + (u32)(HT_YEL_0 & 0x07FF) * 32), vbase + (HT_YEL_0 & 0x07FF), (HT_BOSS_CELL & 0x07FF) - (HT_YEL_0 & 0x07FF), tm);
    else { top_put(0, 0, 40); top_put(1, 0, 40); }
}

#define TOP_TILE(c) (top_slot[r][c] ? top_slot[r][c] : w[c] & 0x07FF)

static void top_build(void)
{
    u16 r;
    top_n = 0;
    top_dirty = 0;
    for (r = 0; r < 2; r++)
    {
        u16 c = 0;
        const u16 *w = top_shown[r];
        while (c < 40)
        {
            u16 line = w[c] & 0x6000, len = 1, t;
            VDPSprite *s;
            if (!w[c]) { c++; continue; }
            t = TOP_TILE(c);
            while (len < 4 && c + len < 40 && w[c + len] && (w[c + len] & 0x6000) == line && TOP_TILE(c + len) == t + len) len++;
            s = top_sat + top_n++;
            s->y = 128 + r * 8;
            s->size_link = (len - 1) << 10;
            s->attribut = 0x8000 | line | t;
            s->x = 128 + c * 8;
            c += len;
        }
    }
}

/* n BCD digits of v (from the top), leading zeros blank, the last digit
 * always shown (56E4). */
static void put_bcd(u16 *w, u32 v, u16 n, u16 digit0)
{
    bool lead = TRUE;
    s16 i;
    for (i = n - 1; i >= 0; i--)
    {
        u16 d = (v >> (i * 4)) & 15;
        if (d || !i) lead = FALSE;
        *w++ = lead ? 0 : W(digit0 + d);
    }
}

/* ---- Two players (co-op, Home; specs/coop.md 4.7, AGENTS.md "2P HUD"):
 * scores at the top corners and TIME between them (HI hidden); each
 * player's gems and gold on its half of row 1 (left: pad 1); at the bottom
 * each player's bar, shortened, with a few of its item icons (Willow: 12
 * cells, 3 icons; Madmartigan: 8 cells, his sword, 2 icons). The right
 * half's bar colours have a set of their own (vbase2: 7 groups x 4 tiles,
 * then its fill slot), carved off the pose cache when the second player
 * joins. */
#define SIDE_COL(s) ((s) * 20)
static u16 vbase2;
static u8 bar_lvl2 = 0xFF;
#define BW2(grp, off) (0x8000 | (bar_tiles[grp] & 0x6000) | (vbase2 + ((grp) - bar_lvl2) * 4 + (off)))

static u16 side_level(const HudSide *h) { return h->form ? sword_level : hud.magic; }
static u16 side_len(const HudSide *h) { return h->form ? 18 : bar_cells[hud.magic]; }
static u16 side_cells(const HudSide *h) { return h->form ? 8 : 12; }

#define COLD __attribute__((noinline, optimize("Os")))   /* co-op only: small, out of 1P's paths */

static COLD void bar_load2(u16 lv)
{
    u16 i;
    if (lv == bar_lvl2) return;
    bar_lvl2 = lv;
    for (i = 0; i < 7; i++)
        DMA_queueDma(DMA_VRAM, (void *)(ht_tiles + (u32)(bar_tiles[lv + i] & 0x1FFF) * 32), (vbase2 + i * 4) * 32, 4 * 16, 2);
}

/* v: 30 tiles for the bar, then 6 for the cells only the 2P layout fills. */
void hud_2p_vram(u16 v)
{
    vbase2 = v;
    bar_lvl2 = 0xFF;
    v += 30;
    top_slot[0][15] = v; top_slot[0][16] = v + 1; top_slot[0][17] = v + 2;
    top_slot[1][0] = v + 3; top_slot[1][22] = v + 4; top_slot[1][29] = v + 5;
}

/* A side's bar group word (set A: the left side, the 1P slots; set B: the right). */
static COLD u16 side_bw(u16 s, u16 grp, u16 off)
{
    if (!s) return BW(grp, off);
    return BW2(grp, off);
}

static COLD void side_bar_load(u16 s, const HudSide *h)
{
    u16 lv = side_level(h), i;
    if (s) { bar_load2(lv); return; }
    if (lv == bar_lvl) return;
    bar_lvl = lv;
    for (i = 0; i < 7; i++)
        DMA_queueDma(DMA_VRAM, (void *)(ht_tiles + (u32)(bar_tiles[bar_lvl + i] & 0x1FFF) * 32),
                     (vbase + (bar_tiles[i] & 0x1FFF)) * 32, 4 * 16, 2);
}

static COLD void draw_side_band(u16 s)
{
    HudSide h;
    u16 t[20], b[20], k, n, len, lv, x = 0, icons, slot;
    coop_side(s, &h);
    side_bar_load(s, &h);
    n = side_cells(&h); len = side_len(&h); lv = side_level(&h);
    t[x] = W(HT_CAP_LT); b[x] = W(HT_CAP_LB); x++;
    for (k = 0; k < n; k++, x++)
    {
        u16 grp = bar_group[k * len / n] + lv;
        t[x] = side_bw(s, grp, 0); b[x] = side_bw(s, grp, 1);
    }
    t[x] = W(HT_CAP_RT); b[x] = W(HT_CAP_RB); x++;
    if (h.form)
    {
        static const u16 sword_tiles[5] = { HT_SWORD0_0T, HT_SWORD1_0T, HT_SWORD2_0T, HT_SWORD3_0T, HT_SWORD4_0T };
        u16 ht = sword_tiles[sword_level], at = 0x8000 | (ht & 0x6000) | (icon_base + 24);
        if (sword_shown != sword_level)
        {
            sword_shown = sword_level;
            DMA_queueDma(DMA_VRAM, (void *)(ht_tiles + (u32)(ht & 0x1FFF) * 32), (icon_base + 24) * 32, 8 * 16, 2);
        }
        t[x] = W(HT_CAP_LT); b[x] = W(HT_CAP_LB); x++;
        for (k = 0; k < 4; k++, x++) { t[x] = at + k * 2; b[x] = at + k * 2 + 1; }
        t[x] = W(HT_CAP_RT); b[x] = W(HT_CAP_RB); x++;
    }
    icons = (20 - x) / 2;
    for (slot = 0; slot < 9 && icons; slot++)
        if (h.items[slot_item[slot]] >= slot_need[slot])
        {
            u16 g = icon_tiles[slot_icon[slot]];
            t[x] = IW(g); b[x] = IW(g + 1);
            t[x + 1] = IW(g + 2); b[x + 1] = IW(g + 3);
            x += 2; icons--;
        }
    for (; x < 20; x++) t[x] = b[x] = W(HT_BAND_BLANK);
    row(ROW_BAND_T, SIDE_COL(s), t, 20);
    row(ROW_BAND_B, SIDE_COL(s), b, 20);
}

/* A charge step on the side whose record is in the globals (magic_meter
 * runs in each player's record): the 1P cell rule, scaled to the shorter bar. */
static COLD void charge_2p(u16 h)
{
    HudSide hs;
    u16 s = coop_side_now(), n, len, d, g, f, t[2], b[2], pal, k = 0, lv;
    coop_side(s, &hs);
    n = side_cells(&hs); len = side_len(&hs); lv = side_level(&hs);
    d = h * n / len;                        /* eighths of a shown cell */
    g = d >> 3; f = d & 7;
    if (g >= n) return;
    if (g)
    {
        pal = bar_group[(g - 1) * len / n] + lv;
        t[0] = side_bw(s, pal, 2); b[0] = side_bw(s, pal, 3);
        k = 1;
    }
    pal = bar_group[g * len / n] + lv;
    if (f == 7) { t[k] = side_bw(s, pal, 2); b[k] = side_bw(s, pal, 3); }
    else
    {
        u16 slot = s ? vbase2 + 28 : vbase + (HT_PART_T & 0x1FFF), line = ht_partline[pal] << 13;
        DMA_queueDma(DMA_VRAM, (void *)(ht_partial + ((u32)(pal * 7 + f) * 2) * 32), slot * 32, 32, 2);
        t[k] = 0x8000 | line | slot;
        b[k] = 0x8000 | line | (slot + 1);
    }
    k++;
    row(ROW_BAND_T, SIDE_COL(s) + 1 + g + 1 - k, t, k);
    row(ROW_BAND_B, SIDE_COL(s) + 1 + g + 1 - k, b, k);
}

static COLD void draw_2p(u8 dirty)
{
    u16 w[8], s;
    HudSide h;
    for (s = 0; s < 2; s++)
    {
        coop_side(s, &h);
        if (dirty & HUD_SCORE) { put_bcd(w, h.score, 8, HT_BLUE_0); row(0, s ? 32 : 4, w, 8); }
        if (dirty & HUD_VITAL)
        {
            u16 v[9], i, n = 0;
            for (i = 0; i < h.hp && n < 8; i++) v[n++] = W(HT_GEM_FULL);
            for (i = h.hp; i < h.max_hp && n < 8; i++) v[n++] = W(HT_GEM_EMPTY);
            for (i = 0; i < h.armor && n < 8; i++) v[n++] = W(HT_ARMOR_CELL);
            while (n < 8) v[n++] = 0;
            row(1, SIDE_COL(s), v, 8);
        }
        if (dirty & HUD_GOLD) { put_bcd(w, h.gold, 6, HT_YEL_0); row(1, SIDE_COL(s) + 12, w, 6); }   /* then 2 columns apart */
        if (dirty & (HUD_BAR | HUD_ITEMS)) draw_side_band(s);
    }
    if (dirty & HUD_TIME)
    {
        w[0] = 0;
        put_bcd(w, hud.min, 2, HT_RED_0);
        w[2] = W(HT_RED_COLON);
        w[3] = W(HT_RED_0 + (hud.sec >> 4));
        w[4] = W(HT_RED_0 + (hud.sec & 15));
        row(0, 20, w, 5);
    }
}

static void draw_score(void)
{
    u16 w[8];
    put_bcd(w, hud.score, 8, HT_BLUE_0);
    row(0, 4, w, 8);
    put_bcd(w, hud.score2, 8, HT_BLUE_0);
    row(0, 32, w, 8);
}

static void draw_hi(void)
{
    u16 w[8];
    put_bcd(w, hud.hi, 8, HT_BLUE_0);
    row(0, 20, w, 8);
}

static void draw_vital(void)
{
    u16 w[12], i, n = 0;
    for (i = 0; i < hud.hp && n < 12; i++) w[n++] = W(HT_GEM_FULL);
    for (i = hud.hp; i < hud.max_hp && n < 12; i++) w[n++] = W(HT_GEM_EMPTY);
    for (i = 0; i < hud.armor && n < 12; i++) w[n++] = W(HT_ARMOR_CELL);
    while (n < 12) w[n++] = 0;
    row(1, 6, w, 12);
}

static void draw_gold(void)
{
    u16 w[6];
    put_bcd(w, hud.gold, 6, HT_YEL_0);
    row(1, 23, w, 6);
}

static void draw_time(void)
{
    u16 w[5];
    put_bcd(w, hud.min, 2, HT_RED_0);
    w[2] = W(HT_RED_COLON);
    w[3] = W(HT_RED_0 + (hud.sec >> 4));
    w[4] = W(HT_RED_0 + (hud.sec & 15));
    row(1, 34, w, 5);
}

static void draw_bar(void)
{
    u16 t[26], b[26], k, n = bar_len(), lv = bar_level();
    bar_load();
    t[0] = W(HT_CAP_LT); b[0] = W(HT_CAP_LB);
    for (k = 0; k < n; k++)
    {
        u16 grp = bar_group[k] + lv;
        t[1 + k] = BW(grp, 0); b[1 + k] = BW(grp, 1);
    }
    t[1 + n] = W(HT_CAP_RT); b[1 + n] = W(HT_CAP_RB);
    for (k = n + 2; k < 26; k++) t[k] = b[k] = W(HT_BAND_BLANK);
    if (player.form)
    {
        /* Madmartigan (5040): a second bracket holding the sword icon of his
         * sword level (4 x 2 glyphs) after the bar. */
        static const u16 sword_tiles[5] = { HT_SWORD0_0T, HT_SWORD1_0T, HT_SWORD2_0T, HT_SWORD3_0T, HT_SWORD4_0T };
        u16 ht = sword_tiles[sword_level], at = 0x8000 | (ht & 0x6000) | (icon_base + 24);
        if (sword_shown != sword_level)
        {
            sword_shown = sword_level;
            DMA_queueDma(DMA_VRAM, (void *)(ht_tiles + (u32)(ht & 0x1FFF) * 32), (icon_base + 24) * 32, 8 * 16, 2);
        }
        t[20] = W(HT_CAP_LT); b[20] = W(HT_CAP_LB);
        for (k = 0; k < 4; k++) { t[21 + k] = at + k * 2; b[21 + k] = at + k * 2 + 1; }
        t[25] = W(HT_CAP_RT); b[25] = W(HT_CAP_RB);
    }
    row(ROW_BAND_T, 0, t, 26);
    row(ROW_BAND_B, 0, b, 26);
}

static void draw_items(void)
{
    u16 t[ITEM_SLOTS * 2], b[ITEM_SLOTS * 2], s, n = 0;
    for (s = 0; s < 9 && n < ITEM_SLOTS; s++)
        if (hud.items[slot_item[s]] >= slot_need[s])
        {
            u16 g = icon_tiles[slot_icon[s]];  /* TL, BL, TR, BR */
            t[n * 2] = IW(g); b[n * 2] = IW(g + 1);
            t[n * 2 + 1] = IW(g + 2); b[n * 2 + 1] = IW(g + 3);
            n++;
        }
    for (s = n * 2; s < ITEM_SLOTS * 2; s++) t[s] = b[s] = W(HT_BAND_BLANK);
    row(ROW_BAND_T, ITEM_COL, t, ITEM_SLOTS * 2);
    row(ROW_BAND_B, ITEM_COL, b, ITEM_SLOTS * 2);
}

/* One charge meter step (4B62): the cell for level h fills to (h & 7) / 8
 * and the cell before it shows full. The filling cell's tiles are streamed
 * into one slot pair. */
void hud_charge(u16 h)
{
    u16 g = h >> 3, f = h & 7, t[2], b[2], pal, n = 0;
    if (two_players) { charge_2p(h); return; }
    if (g >= bar_len()) return;
    if (g)
    {
        pal = bar_group[g - 1] + bar_level();
        t[0] = BW(pal, 2); b[0] = BW(pal, 3);
        n = 1;
    }
    pal = bar_group[g] + bar_level();
    if (f == 7) { t[n] = BW(pal, 2); b[n] = BW(pal, 3); }
    else
    {
        u16 slot = vbase + (HT_PART_T & 0x1FFF), line = ht_partline[pal] << 13;
        DMA_queueDma(DMA_VRAM, (void *)(ht_partial + ((u32)(pal * 7 + f) * 2) * 32), slot * 32, 32, 2);
        t[n] = 0x8000 | line | slot;
        b[n] = 0x8000 | line | (slot + 1);
    }
    n++;
    row(ROW_BAND_T, 2 + g - n, t, n);   /* queued, so it lands with the tiles */
    row(ROW_BAND_B, 2 + g - n, b, n);
}

/* A centred message on plane B row y (TIME OVER, CONTINUE ...), the arcade
 * font at normal size (approved cut 3: text on its own rows); NULL clears
 * the row back to the backdrop. */
static s16 find(const char *set, char c)
{
    s16 i;
    for (i = 0; set[i]; i++) if (set[i] == c) return i;
    return -1;
}

void hud_message(const char *text, u16 y)
{
    static const char blue[] = "UPHIVTAL";
    static const u16 blue_t[] = { HT_BLUE_U, HT_BLUE_P, HT_BLUE_H, HT_BLUE_I, HT_BLUE_V, HT_BLUE_T, HT_BLUE_A, HT_BLUE_L };
    static const char msg[] = "MEORCNGSKBWYDFJQXZ.,'!?-";
    u16 w[40], i, n = 0;
    for (i = 0; i < 40; i++) w[i] = stage_bword(i, y);
    if (text)
    {
        const char *c;
        for (c = text; *c; c++) n++;
        for (i = 0, c = text; *c; c++, i++)
        {
            s16 k;
            u16 t = 0;
            if (*c >= '0' && *c <= '9') t = W(HT_BLUE_0 + *c - '0');
            else if (*c == '=') t = W(HT_BLUE_EQ);
            else if ((k = find(blue, *c)) >= 0) t = W(blue_t[k]);
            else if ((k = find(msg, *c)) >= 0) t = W(HT_MSG_M + k);
            if (t) w[20 - n / 2 + i] = t;
        }
    }
    stage_bput(0, y, w, 40);
}

/* Text from column x of plane B row y, n characters (typed text); the
 * rest of the row keeps the backdrop. */
u8 hud_blank_rows;          /* hud_text rows have a blank background, not the backdrop */
const u16 *hud_row_words;   /* non-NULL: hud_text rows keep these words (40 x 28) + hud_row_add */
u16 hud_row_add;

/* Glyph word of ASCII 32..95 (0: none), built once per HUD base. */
static u16 glyph_of[64], glyph_base = 0xFFFF;

static void glyphs_build(void)
{
    static const char blue[] = "UPHIVTAL";
    static const u16 blue_t[] = { HT_BLUE_U, HT_BLUE_P, HT_BLUE_H, HT_BLUE_I, HT_BLUE_V, HT_BLUE_T, HT_BLUE_A, HT_BLUE_L };
    static const char msg[] = "MEORCNGSKBWYDFJQXZ.,'!?-";
    u16 c;
    for (c = 32; c < 96; c++)
    {
        s16 k;
        u16 t = 0;
        if (c >= '0' && c <= '9') t = W(HT_BLUE_0 + c - '0');
        else if (c == '=') t = W(HT_BLUE_EQ);
        else if ((k = find(blue, c)) >= 0) t = W(blue_t[k]);
        else if ((k = find(msg, c)) >= 0) t = W(HT_MSG_M + k);
        glyph_of[c - 32] = t;
    }
    glyph_base = vbase;
}

u16 hud_glyph(char ch)
{
    u8 c = ch - 32;
    if (glyph_base != vbase) glyphs_build();
    return c < 64 ? glyph_of[c] : 0;
}

void hud_text(const char *text, u16 n, u16 x, u16 y)
{
    u16 w[40], i;
    if (glyph_base != vbase) glyphs_build();
    if (hud_row_words) { const u16 *m = hud_row_words + y * 40; for (i = 0; i < 40; i++) w[i] = m[i] + hud_row_add; }
    else if (hud_blank_rows) memset(w, 0, sizeof w);
    else stage_brow(y, w);
    for (i = 0; i < n && text[i] && x + i < 40; i++)
    {
        u8 c = text[i] - 32;
        if (c < 64 && glyph_of[c]) w[x + i] = glyph_of[c];
    }
    stage_bput(0, y, w, 40);
}

/* Boss energy (4AC6, hud.md 3.9): "ENEMY" and ceil(energy / 8) cells on
 * row 2; energy < 0 clears the row. */
void hud_boss(s16 energy)
{
    u16 w[40], i, n;
    for (i = 0; i < 40; i++) w[i] = stage_bword(i, 2);
    if (energy >= 0)
    {
        static const u16 label[5] = { HT_MSG_E, HT_MSG_N, HT_MSG_E, HT_MSG_M, HT_MSG_Y };
        for (i = 0; i < 5; i++) w[1 + i] = W(label[i]);
        n = (energy + 7) >> 3;
        for (i = 0; i < n && i < 23; i++) w[7 + i] = W(HT_BOSS_CELL);
    }
    stage_bput(0, 2, w, 40);
}

/* A HUD glyph's tile (HT_ constant) copied to VRAM tile v (sprites: stage
 * 5's boss bars); its CRAM line bits. */
u16 hud_glyph_copy(u16 ht, u16 v)
{
    DMA_queueDma(DMA_VRAM, (void *)(ht_tiles + (u32)(ht & 0x1FFF) * 32), v * 32, 16, 2);
    return ht & 0x6000;
}

u16 hud_init(u16 vram_base)
{
    /* Static labels (text string 12). */
    static const u16 top[] = { 1, HT_BLUE_1, HT_BLUE_U, HT_BLUE_P, 0,
                               18, HT_BLUE_H, HT_BLUE_I, 0,
                               29, HT_BLUE_2, HT_BLUE_U, HT_BLUE_P, 0, 0xFFFF };
    static const u16 bottom[] = { 1, HT_BLUE_V, HT_BLUE_I, HT_BLUE_T, HT_BLUE_A, HT_BLUE_L, 0,
                                  18, HT_YEL_G, HT_YEL_O, HT_YEL_L, HT_YEL_D, 0,
                                  30, HT_RED_T, HT_RED_I, HT_RED_M, HT_RED_E, 0, 0xFFFF };
    u16 loaded;

    vbase = vram_base;
    /* The tiles up to HT_RESIDENT; the bar's groups follow the magic level
     * (bar_load), the item icons are in their own block (hud_icons). */
    loaded = HT_RESIDENT & 0x1FFF;
    VDP_loadTileData((const u32 *)ht_tiles, vbase, loaded, DMA);
    bar_lvl = 0;
    bar_load();
    if (!hud_keep_backdrop) VDP_setBackgroundColor(HUD_SKY);
    (void)top; (void)bottom;
    hud_layout();
    return vbase + loaded;
}

/* The labels of the top rows for one or two players, then every element. */
COLD void hud_layout(void)
{
    static const u16 top[] = { 1, HT_BLUE_1, HT_BLUE_U, HT_BLUE_P, 0,
                               18, HT_BLUE_H, HT_BLUE_I, 0,
                               29, HT_BLUE_2, HT_BLUE_U, HT_BLUE_P, 0, 0xFFFF };
    static const u16 bottom[] = { 1, HT_BLUE_V, HT_BLUE_I, HT_BLUE_T, HT_BLUE_A, HT_BLUE_L, 0,
                                  18, HT_YEL_G, HT_YEL_O, HT_YEL_L, HT_YEL_D, 0,
                                  30, HT_RED_T, HT_RED_I, HT_RED_M, HT_RED_E, 0, 0xFFFF };
    static const u16 top2[] = { 1, HT_BLUE_1, HT_BLUE_U, HT_BLUE_P, 0,
                                15, HT_RED_T, HT_RED_I, HT_RED_M, HT_RED_E, 0,
                                29, HT_BLUE_2, HT_BLUE_U, HT_BLUE_P, 0, 0xFFFF };
    static const u16 bottom2[] = { 8, HT_YEL_G, HT_YEL_O, HT_YEL_L, HT_YEL_D, 0,
                                   28, HT_YEL_G, HT_YEL_O, HT_YEL_L, HT_YEL_D, 0, 0xFFFF };
    const u16 *labels[2] = { two_players ? top2 : top, two_players ? bottom2 : bottom };
    u16 r;
    memset(stage_hudrow, 0, sizeof stage_hudrow);
    memset(top_shown, 0, sizeof top_shown);        /* the glyphs' colours may be a new stage's */
    top_np = 0;
    for (r = 0; r < 2; r++)
    {
        const u16 *l = labels[r];
        while (*l != 0xFFFF)
        {
            u16 x = *l++;
            for (; *l; l++) stage_hudrow[r][x++] = W(*l);
            l++;
        }
        top_put(r, 0, 40);
        stage_hudput(r, 0, 40);
    }
    hud_dirty = 0xFF;
    hud_update();
}

void hud_update(void)
{
    /* Clock (4EA2): one second per 60 updates, from the first unlocked
     * update; stops at 0:00 (the time-out itself is not ported yet). */
    if (!hud.clock_lock && !fe.paused && (hud.min | hud.sec))
    {
        if (hud.div) hud.div--;
        else
        {
            hud.div = 59;
            if (hud.sec & 15) hud.sec--;
            else if (hud.sec) hud.sec -= 7;            /* x0 -> (x-1)9 */
            else { hud.sec = 0x59; hud.min--; }        /* minutes < 10 */
            hud_dirty |= HUD_TIME;
        }
    }
    /* Under 0:10 the warning sound every 32 frames (4EA2). */
    if (!hud.clock_lock && !fe.paused && !hud.min && hud.sec < 0x10 && !(game_frame & 31)) sound_post(SND_TIME_WARN);
    if (!hud_dirty) return;
    if (two_players) { draw_2p(hud_dirty); hud_dirty = 0; return; }
    if (hud_dirty & HUD_SCORE) draw_score();
    if (hud_dirty & HUD_HI) draw_hi();
    if (hud_dirty & HUD_VITAL) draw_vital();
    if (hud_dirty & HUD_GOLD) draw_gold();
    if (hud_dirty & HUD_TIME) draw_time();
    if (hud_dirty & HUD_BAR) draw_bar();
    if (hud_dirty & HUD_ITEMS) draw_items();
    hud_dirty = 0;
}

/* Reserve lives, "P= n" (56C8): arcade columns 41-44 of row 26, folded to
 * columns 34-37 at y 192, over the scene. */
/* Co-op: each player's reserve lives under its half. Written as the
 * sprite table's words directly (the struct's bit fields cost ~1,200
 * cycles a call). */
static u16 lives_draw(VDPSprite *sat, u16 first, u8 lives, u16 col)
{
    u16 v = lives ? lives - 1 : 0, x = 128 + col * 8, k = first;   /* BCD lives - 1 */
    VDPSprite *s = sat + first;
    if ((v & 15) == 15) v -= 6;                     /* BCD borrow */
#define LIVES_PUT(t) { s->y = 128 + 192; s->size_link = ++k; s->attribut = W(t); s->x = x; s++; }
    LIVES_PUT(HT_BLUE_P); x += 8;
    LIVES_PUT(HT_BLUE_EQ); x += 8;
    if (v >> 4) LIVES_PUT(HT_BLUE_0 + (v >> 4));
    x += 8;
    LIVES_PUT(HT_BLUE_0 + (v & 15));
#undef LIVES_PUT
    return k - first;
}

static u16 hud_draw_2p(VDPSprite *sat, u16 first)
{
    u16 now = coop_side_now(), n;
    n = lives_draw(sat, first, now == 0 ? hud.lives : coop_other_lives(), 2);   /* coop_side's lives, without the record copy */
    return n + lives_draw(sat, first + n, now == 1 ? hud.lives : coop_other_lives(), 34);
}

/* Co-op: the out player's prompt, "1P START" / "2P START" blinking at the
 * bottom left while Home credits remain (Start on its pad continues). */
static COLD u16 out_prompt(VDPSprite *sat, u16 first)
{
    static const u16 g[8] = { 0, HT_BLUE_P, 0, HT_MSG_S, HT_BLUE_T, HT_BLUE_A, HT_MSG_M + 3, HT_BLUE_T };
    u16 i, n = 0;
    if (!home_credits || (game_frame & 16)) return 0;
    for (i = 0; i < 8; i++)
    {
        VDPSprite *s;
        u16 t = i ? g[i] : HT_BLUE_0 + 2 - coop_lead_pad;   /* the out player's pad */
        if (!t) continue;
        s = sat + first + n;
        s->y = 128 + 192;
        s->size = SPRITE_SIZE(1, 1);
        s->attribut = W(t);
        s->x = 128 + (2 + i) * 8;
        s->link = first + n + 1;
        n++;
    }
    return n;
}

u8 hud_sat_ok;              /* sat[0..] still holds the top rows' sprites (main.c clears it when it writes them over) */

u16 hud_draw(VDPSprite *sat, u16 first)
{
    u16 n;
    if (top_dirty) { top_build(); hud_sat_ok = 0; }
    n = top_plane ? 0 : top_n;
    if (first || !hud_sat_ok)
    {
        /* Only when the list changes: every frame it cost ~4 lines. */
        const u32 *a = (const u32 *)top_sat;
        u32 *o = (u32 *)(sat + first);
        u16 i;
        for (i = 0; i < n; i++)
        {
            *o++ = *a++ | (first + i + 1);  /* y, size and link */
            *o++ = *a++;                    /* attributes, x */
        }
        hud_sat_ok = !first;
    }
    if (two_players) return n + hud_draw_2p(sat, first + n);
    if (coop_state == CO_OUT) n += out_prompt(sat, first + n);
    return n + lives_draw(sat, first + n, hud.lives, 34);
}
