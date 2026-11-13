/* The arcade's own screens between plays (specs/arcade_frontend.md 4-11):
 * coins and credits, the credit screen, the name entry and ranking, the
 * continue screen and GAME OVER. Used by both modes (Home continues and
 * starts without credits).
 *
 * Layout: the arcade's text grid is 48 x 30 cells with rows 2-29 shown; the
 * Genesis shows 40 x 28: arcade cell (c, r) -> (c - 4, r - 2). Text on plane
 * A over black, the arcade's font (tools/fontpack.py: small glyphs at VRAM
 * 16, big 2x2 glyphs after them) and its fixed text palettes, four per
 * screen as CRAM lines. The continue screen's figures (class 12 kind 3,
 * 0x32710) are sprites on line 3. */
#include <genesis.h>
#include "coop.h"
#include "screens.h"
#include "frontend.h"
#include "hud.h"
#include "sound.h"
#include "stage.h"
#include "packed.h"
#include "player.h"
#include "s5spr.h"
#include "sprite.h"
#include "../data/contfig/contfig.h"

extern const u8 font_tiles[], font_big[];
extern const u16 font_textpal[];

#define SMALL 16                    /* VRAM tile of glyph ' ' */
#define BIG (SMALL + 66)            /* big glyphs: A-Z, 0-9, 4 tiles each */
#define X(c) ((c) - 4)
#define Y(r) ((r) - 2)

/* ---- Coins and credits (section 4). */
u8 credits;                         /* FF82A2, BCD 0..9 */
static u8 coin_hist, coin_count;
static u16 coin_held;

/* Per Coin A menu value (port.c order): coins needed, credits given. */
static const u8 coin_need[8] = { 4, 3, 2, 2, 1, 1, 1, 1 }, coin_give[8] = { 1, 1, 1, 1, 1, 2, 3, 4 };

/* Each frame in Arcade mode, from the pad's C: a coin is counted when the
 * button is released after 2+ frames down; held 120+ frames it is a jam. */
bool coin_update(u16 held)
{
    u8 on = (held & BUTTON_C) != 0;
    bool counted = FALSE;
    if (fe.mode != FE_ARCADE) return FALSE;
    coin_hist = (coin_hist << 1 | on) & 15;
    if (on) { if (coin_held < 0xFFFF) coin_held++; }
    else
    {
        if (coin_held >= 2 && coin_held < 120)
        {
            const Settings *s = frontend_settings();
            sound_post(0x3B);
            if (++coin_count >= coin_need[s->coinA])
            {
                u16 c = (credits >> 4) * 10 + (credits & 15) + coin_give[s->coinA];
                coin_count = 0;
                if (c > 9) c = 9;
                credits = c;                    /* single digit: BCD = binary */
            }
            counted = TRUE;
        }
        coin_held = 0;
    }
    return counted;
}

/* ---- Screen helpers. */
static void lines(u16 a, u16 b, u16 c, u16 d)
{
    PAL_setColors(0, font_textpal + a * 16, 16, DMA);
    PAL_setColors(16, font_textpal + b * 16, 16, DMA);
    PAL_setColors(32, font_textpal + c * 16, 16, DMA);
    PAL_setColors(48, font_textpal + d * 16, 16, DMA);
    PAL_setColor(0, 0);
}

static void put(u16 x, u16 y, const char *s, u16 line)
{
    u16 w[40], n = 0;
    while (*s && x + n < 40)
    {
        u8 c = (u8)*s++;
        u16 g = c == '{' ? 64 : c == '|' ? 65 : (c >= 32 && c < 96 ? c - 32 : 0);
        w[n++] = TILE_ATTR_FULL(line, 1, 0, 0, SMALL + g);
    }
    VDP_setTileMapDataRow(BG_A, w, y, x, n, CPU);
}

static void big(u16 x, u16 y, char ch, u16 line)
{
    u16 g = ch >= 'A' && ch <= 'Z' ? ch - 'A' : ch >= '0' && ch <= '9' ? 26 + ch - '0' : 0xFFFF, t;
    if (g == 0xFFFF) { VDP_fillTileMapRect(BG_A, 0, x, y, 2, 2); return; }
    t = BIG + g * 4;
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(line, 1, 0, 0, t), x, y);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(line, 1, 0, 0, t + 1), x, y + 1);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(line, 1, 0, 0, t + 2), x + 1, y);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(line, 1, 0, 0, t + 3), x + 1, y + 1);
}

static void big_text(u16 x, u16 y, const char *s, u16 line)
{
    for (; *s; s++, x += 2) if (*s != ' ') big(x, y, *s, line);
}

static void begin(void)
{
    PAL_setColors(0, palette_black, 64, CPU);
    SYS_doVBlankProcess();          /* the stage's pending column and tile writes land before the clear */
    stage_split(FALSE);
    VDP_setWindowVPos(FALSE, 0);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A, 0); VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0); VDP_setVerticalScroll(BG_B, 0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_resetSprites(); VDP_updateSprites(1, DMA);
    VDP_loadTileData((const u32 *)font_tiles, SMALL, 66, DMA);
    VDP_loadTileData((const u32 *)font_big, BIG, 144, DMA);
    VDP_setBackgroundColor(0);
}

static u16 fade_to[64];
static void fade_in(u16 a, u16 b, u16 c, u16 d)
{
    memcpy(fade_to, font_textpal + a * 16, 32); memcpy(fade_to + 16, font_textpal + b * 16, 32);
    memcpy(fade_to + 32, font_textpal + c * 16, 32); memcpy(fade_to + 48, font_textpal + d * 16, 32);
    fade_to[0] = 0;
    PAL_fadeInAll(fade_to, 16, FALSE);
}

static void credit_display(void)
{
    char t[3];
    u8 c = fe.mode == FE_ARCADE ? credits : home_credits;   /* Home: its credits (continues) */
    t[0] = (c >> 4) ? '0' + (c >> 4) : ' ';
    t[1] = '0' + (c & 15);
    t[2] = 0;
    put(31, Y(29), "CREDIT", 0);            /* arcade (37..45, 29): inside 40 columns */
    put(38, Y(29), t, 0);
}

static u16 pad_prev;
static u16 pad_edge(u16 *held)
{
    u16 h = JOY_readJoypad(JOY_1), e = h & ~pad_prev;
    pad_prev = h;
    if (held) *held = h;
    if (coin_update(h)) credit_display();
    return e;
}

static void frames(u16 n) { while (n--) { SYS_doVBlankProcess(); pad_edge(NULL); } }

/* ---- Credit screen (5.1): until Start with a credit (no timeout). */
void credit_screen(void)
{
    u16 shown = 0xFF, t = 0;
    sound_post(SND_STOP);
    begin();
    lines(0, 2, 0, 0);
    PAL_setColors(0, font_textpal, 32, DMA);
    PAL_setColor(0, 0);
    pad_prev = JOY_readJoypad(JOY_1);
    for (;;)
    {
        u16 e;
        if (credits != shown)
        {
            shown = credits;
            put(X(19), Y(16), credits == 1 ? "1 PLAYER ONLY " : "1 OR 2 PLAYERS", 0);
            credit_display();
        }
        put(X(17), Y(15), (t & 8) ? "                  " : "PRESS START BUTTON", 0);   /* 8 on, 8 off */
        t++;
        SYS_doVBlankProcess();
        e = pad_edge(NULL);
        if ((e & BUTTON_START) && credits) { credits--; break; }
    }
    PAL_fadeOutAll(16, FALSE);
    VDP_clearPlane(BG_A, TRUE);
}

/* ---- Ranking (11): the mode's table; row `hot` (0..9, or 10) cycles the
 * text palettes 0..3 every 5 frames. */
static void ranking_rows(u16 hot)
{
    static const char *const label[10] = { "TOP", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH", "8TH", "9TH", "10TH" };
    const u8 *tab = frontend_scores(fe.mode);
    u16 n, k;
    put(X(21), Y(6), "RANKING", 0);
    for (n = 0; n < 10; n++)
    {
        const u8 *e = tab + n * 8;
        char sc[9], nm[5];
        u16 lead = 1, row = Y(9 + n * 2), ln = n == hot ? 3 : 0;
        for (k = 0; k < 8; k++)
        {
            u16 v = e[k >> 1] >> ((k & 1) ? 0 : 4) & 15;
            if (v) lead = 0;
            sc[k] = lead ? ' ' : '0' + v;
        }
        sc[8] = 0;
        for (k = 0; k < 4; k++) nm[k] = e[4 + k];
        nm[4] = 0;
        put(X(n == 9 ? 14 : 15), row, label[n], ln ? 3 : 0);
        put(X(18), row, sc, ln ? 3 : 1);
        put(X(26), row, " PTS", ln ? 3 : 0);
        put(X(31), row, nm, ln ? 3 : 2);
    }
}

/* The attract's ranking (3.1): 180 frames, no music. */
void ranking_screen(void)
{
    begin();
    ranking_rows(10);
    fade_in(3, 5, 2, 0);
    frames(180);
    PAL_fadeOutAll(16, FALSE);
}

/* ---- Name entry (10): `place` 0..9 in the mode's table (its entry already
 * holds the score). */
static void name_entry(u16 place, u32 score)
{
    static const char set[37] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ&'()*+.-{|";
    char name[5] = "    ";
    u16 col = 17, row = 1, left = 3, timer = 0x20, sub = 48, blink = 5, idle = 0, rep = 0, hi = 0, k;
    begin();
    lines(0, 3, 2, 9);
    put(X(20), Y(6), "PLAYER", 1); put(X(27), Y(6), "1", 1);
    {
        u16 x = X(14), lead = 1;
        for (k = 0; k < 8; k++)
        {
            u16 v = score >> (28 - 4 * k) & 15;
            if (v || k == 7) lead = 0;
            if (!lead) { big(x, Y(8), '0' + v, 3); x += 2; }
        }
        put(X(30), Y(9), "PTS", 1);
    }
    for (k = 0; k < 36; k++) { char s[2] = { set[k], 0 }; put(X(7 + 2 * (k % 18)), Y(12 + 2 * (k / 18)), s, 0); }
    put(X(16), Y(17), "PUT YOUR NAME", 1);
    put(X(20), Y(20), "PLAYER  1", 1);
    sound_post(SND_MUSIC_NAME);
    pad_prev = JOY_readJoypad(JOY_1);
    fade_in(0, 3, 2, 9);
    for (;;)
    {
        u16 held, e, n = 3 - left;
        char t[3] = { '0' + (timer >> 4), '0' + (timer & 15), 0 }, cur[2] = { set[row * 18 + col], 0 };
        put(X(30), Y(17), t, 2);
        if (left) put(X(21 + n), Y(23), cur, 2);                /* preview of the selected letter */
        if (!--blink) { blink = 5; hi ^= 1; }
        put(X(7 + 2 * col), Y(12 + 2 * row), cur, hi ? 1 : 0);
        SYS_doVBlankProcess();
        e = pad_edge(&held);
        if (e & (BUTTON_A | BUTTON_B))
        {
            char c = set[row * 18 + col];
            idle = 0;
            if (c == '|') break;                                 /* ED */
            if (c == '{')                                        /* RB */
            {
                if (left < 3) { left++; name[2 - left] = ' '; put(X(21 + 3 - left), Y(23), "  ", 2); }
                continue;
            }
            name[n] = c;
            put(X(21 + n), Y(23), cur, 2);
            if (!--left) break;
            continue;
        }
        if (held & (BUTTON_LEFT | BUTTON_RIGHT | BUTTON_UP | BUTTON_DOWN))
        {
            u16 go = e & (BUTTON_LEFT | BUTTON_RIGHT | BUTTON_UP | BUTTON_DOWN);
            idle = 0;
            if (!go && ++rep >= 10) { go = held; rep = 0; }
            if (go) rep = 0;
            if (go)
            {
                put(X(7 + 2 * col), Y(12 + 2 * row), cur, 0);
                if (go & BUTTON_RIGHT) col = col == 17 ? 0 : col + 1;
                else if (go & BUTTON_LEFT) col = col ? col - 1 : 17;
                if (go & (BUTTON_UP | BUTTON_DOWN)) row ^= 1;
            }
        }
        else if (++idle >= 420) break;
        if (!--sub)
        {
            sub = 48;
            if (!timer) break;
            timer = (timer & 15) ? timer - 1 : timer - 7;        /* BCD */
        }
    }
    frames(42);
    PAL_fadeOutAll(16, FALSE);
    frontend_insert(place, score, name);
    /* The ranking after the entry (10.4): music 0x16, up to 120 frames. */
    begin();
    ranking_rows(place);
    sound_post(SND_MUSIC_RANK);
    fade_in(3, 5, 2, 0);
    for (k = 0; k < 120; k++)
    {
        u16 held;
        PAL_setColors(48, font_textpal + ((k / 5) & 3) * 16, 16, DMA);
        PAL_setColor(48, 0);
        SYS_doVBlankProcess();
        pad_edge(&held);
        if (held & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START | BUTTON_DIR)) break;
    }
    PAL_fadeOutAll(16, FALSE);
}

/* Lives out (6): qualification and name entry first. A tie ranks above. */
void scores_screens(void)
{
    u32 score = hud.score;
    u16 place = frontend_rank(score);
    if (place < 10) name_entry(place, score);
}

/* ---- The continue screen's figures (class 12 kind 3; 0x32710, 0x3275E,
 * 0x328CA, 0x32A46; tools/contpack.py): the old man lying at (168, 80)
 * (shown after a 16-frame burst), the figure by form at (216, 72). On a
 * continue the old man rises 38 frames throwing sparks (0x328C0 / 0x328C4)
 * and bursts away; then the figure, 43 frames (Madmartigan 53), throws
 * sparks (0x329E6 / 0x329FE), then sparkles (0xFB90) until the screen ends.
 * On GAME OVER the old man takes his last pose. Screen x = x - 32, y = 239
 * - y; line 3 holds the form's fitted colours. */
extern const u8 contfig_poses[], contfig_anim[], contfig_tiles0[], contfig_tiles1[];
extern const u16 contfig_pal0[], contfig_pal1[];
extern u16 rnd(void);
#define CF_VRAM (BIG + 144)
#define CF_SPARKS 32

typedef struct { Anim a; s16 x, y; u8 on, life; } CfObj;
static CfObj cf_old, cf_fig, cf_burst[2], cf_spark[CF_SPARKS];
static u8 cf_form, cf_mode, cf_old_wait, cf_old_t, cf_old_sub, cf_fig_t, cf_fig_sub, cf_r15, cf_r48;
static const u8 cf_spark_life[4] = { 9, 16, 7, 24 };                                 /* 0x32A7C */
static const u8 cf_spark_chain[4] = { CF_SPARK0, CF_SPARK1, CF_SPARK2, CF_SPARK3 };   /* 0x32A80 */
static const u8 old_sx[4] = { 168, 176, 184, 192 }, old_sy[6] = { 88, 80, 72, 64, 56, 48 };   /* 0x328C0, 0x328C4 */
static const u8 fig_sx[24] = { 168, 176, 184, 192, 172, 180, 188, 196, 176, 184, 192, 200,
                               180, 188, 196, 204, 184, 192, 200, 208, 188, 196, 204, 212 };   /* 0x329E6 */
static const u8 fig_sy[6] = { 48, 56, 64, 72, 80, 88 };                              /* 0x329FE */
static const s8 fig_around[16][2] = { { -2, 20 }, { 2, 18 }, { -4, 16 }, { 4, 14 }, { -8, 12 }, { 8, 10 }, { -10, 8 }, { 10, 6 },
                                      { -12, 4 }, { 12, 2 }, { 0, 0 }, { -10, -2 }, { 10, -4 }, { -8, -6 }, { 8, -8 }, { -5, -8 } };   /* 0xFB90 */

static void cf_set(CfObj *o, u16 chain, s16 x, s16 y)
{
    anim_start(&o->a, contfig_anim + chain);
    o->x = x; o->y = y; o->on = 1;
}

static void cf_burst_at(s16 x, s16 y)
{
    CfObj *b = &cf_burst[cf_burst[0].on ? 1 : 0];
    cf_set(b, CF_BURST, x, y);
    b->life = 32;                                           /* 0x32260[5] */
}

static void cf_spark_at(s16 x, s16 y)
{
    u16 i, c;
    for (i = 0; i < CF_SPARKS && cf_spark[i].on; i++) ;
    c = rnd() & 3;
    if (i == CF_SPARKS) return;
    cf_set(&cf_spark[i], cf_spark_chain[c], x, y);
    cf_spark[i].life = cf_spark_life[c];
}

/* The tile set and line 3 for the form; the old man waits 16 frames in its burst. */
static void cf_begin(void)
{
    u16 i;
    cf_form = player.form ? 1 : 0;
    load_packed(cf_form ? contfig_tiles1 : contfig_tiles0, CF_VRAM, CONTFIG_TILES);
    memset(cf_spark, 0, sizeof cf_spark);
    for (i = 0; i < 2; i++) cf_burst[i].on = 0;
    cf_set(&cf_old, CF_OLD_LIE, 168, 80);
    cf_old.on = 0;
    cf_old_wait = 16; cf_old_sub = 0;
    cf_burst_at(168, 80);
    cf_set(&cf_fig, cf_form ? CF_MAD : CF_WIL, 216, 72);
    cf_fig_sub = 0;
    cf_mode = 0;
}

/* One frame: mode 0 waiting, 1 continuing (FF8004 = 12), 2 GAME OVER (8). */
static void cf_frame(u16 mode)
{
    VDPSprite *sat = vdpSpriteCache;
    CfObj *list[4 + CF_SPARKS];
    u16 n = 0, k, i;
    if (cf_old_wait) { if (!--cf_old_wait) cf_old.on = 1; }
    else if (cf_old.on)
    {
        if (!cf_old_sub)
        {
            if (mode == 1) { cf_old_sub = 4; cf_r15 = 0; cf_r48 = 6; cf_old_t = 0; cf_set(&cf_old, CF_OLD_RISE, 168, 80); }
            else if (mode == 2) { cf_old_sub = 2; cf_set(&cf_old, CF_OLD_OVER, 168, 80); }
            else anim_step(&cf_old.a);
        }
        else if (cf_old_sub == 2) anim_step(&cf_old.a);
        else if (++cf_old_t >= 38) { cf_old.on = 0; cf_burst_at(168, 80); }
        else
        {
            u16 r = rnd(), x = old_sx[(r >> 4) & 3] + (r & 3);
            anim_step(&cf_old.a);
            r = rnd();
            cf_spark_at(x, old_sy[cf_r15] + (r & 7));
            if (!--cf_r48) { cf_r48 = 6; if (++cf_r15 > 5) cf_r15 = 5; }
        }
    }
    /* The figure: still until 38 frames into a continue. */
    if (mode == 1)
    {
        if (!cf_fig_sub) { cf_fig_sub = 2; cf_fig_t = 38; }
        else if (cf_fig_sub == 2 && !--cf_fig_t)
        {
            cf_fig_sub = 4; cf_r15 = 0; cf_r48 = 6; cf_fig_t = cf_form ? 53 : 43;
            cf_set(&cf_fig, cf_form ? CF_MAD_GO : CF_WIL_GO, 216, 72);
        }
        else if (cf_fig_sub == 4)
        {
            if (!--cf_fig_t) cf_fig_sub = 6;
            else
            {
                u16 r = rnd(), x = fig_sx[cf_r15 * 4 + ((r >> 4) & 3)] + (r & 3);
                anim_step(&cf_fig.a);
                r = rnd();
                cf_spark_at(x, fig_sy[cf_r15] + (r & 3));
                if (!--cf_r48) { cf_r48 = 6; if (++cf_r15 > 5) cf_r15 = 5; }
            }
        }
        else if (cf_fig_sub == 6)
        {
            const s8 *o = fig_around[rnd() & 15];
            cf_spark_at(216 + o[0], 72 + o[1]);
        }
    }
    for (i = 0; i < 2; i++)
        if (cf_burst[i].on) { anim_step(&cf_burst[i].a); if (!--cf_burst[i].life) cf_burst[i].on = 0; }
    for (i = 0; i < CF_SPARKS; i++)
        if (cf_spark[i].on) { anim_step(&cf_spark[i].a); if (!--cf_spark[i].life) cf_spark[i].on = 0; }
    /* Drawn: sparks and bursts over the figures. */
    k = 0;
    for (i = 0; i < CF_SPARKS; i++) if (cf_spark[i].on) list[k++] = &cf_spark[i];
    for (i = 0; i < 2; i++) if (cf_burst[i].on) list[k++] = &cf_burst[i];
    if (cf_old.on) list[k++] = &cf_old;
    if (cf_fig.on) list[k++] = &cf_fig;
    for (i = 0; i < k; i++)
    {
        const u8 *d = contfig_poses + anim_pose(&list[i]->a) * 6;
        const u8 *e = contfig_poses + CONTFIG_POSES * 6 + ((d[4] << 8) | d[5]) * 6;
        u16 t0 = (d[0] << 8) | d[1], ns = d[3], j;
        s16 sx = list[i]->x - 32, sy = 239 - list[i]->y;
        for (j = 0; j < ns && n < 80; j++, e += 6)
        {
            VDPSprite *v = sat + n;
            v->y = 128 + sy + (s16)((e[0] << 8) | e[1]);
            v->size = e[2] & 15;
            v->attribut = TILE_ATTR_FULL(PAL3, 1, 0, e[2] >> 7, CF_VRAM + t0 + e[3]);
            v->x = 128 + sx + (s16)((e[4] << 8) | e[5]);
            v->link = n + 1;
            n++;
        }
    }
    if (!n) { sat[0].y = 0; n = 1; }
    sat[n - 1].link = 0;
    VDP_updateSprites(n, DMA_QUEUE);
}

/* n frames of the figures. */
static void cf_frames(u16 n, u16 mode)
{
    while (n--) { cf_frame(mode); SYS_doVBlankProcess(); pad_edge(NULL); }
}

/* Line 3: the figures' colours (fade_in's fourth text line is unused here). */
static void cf_fade_in(u16 a, u16 b, u16 c)
{
    memcpy(fade_to, font_textpal + a * 16, 32); memcpy(fade_to + 16, font_textpal + b * 16, 32);
    memcpy(fade_to + 32, font_textpal + c * 16, 32); memcpy(fade_to + 48, cf_form ? contfig_pal1 : contfig_pal0, 32);
    fade_to[0] = 0;
    PAL_fadeInAll(fade_to, 16, TRUE);
}

/* ---- Continue (7): TRUE when the player continues. */
static void game_over(void)
{
    begin();
    big_text(X(15), Y(12), "GAME  OVER", 1);
    sound_post(SND_MUSIC_OVER);
    fade_in(0, 15, 0, 0);
    frames(120);
    PAL_fadeOutAll(16, FALSE);
}

/* After the ending (stage6.md 7.5): GAME OVER without its music (FF8388). */
void screens_game_over(void)
{
    begin();
    big_text(X(15), Y(12), "GAME  OVER", 1);
    fade_in(0, 15, 0, 0);
    frames(120);
    PAL_fadeOutAll(16, FALSE);
}

bool continue_screen(void)
{
    u16 digit = 9, t, grace = 0;
    char d[2] = { '9', 0 };
    if (fe.mode == FE_HOME ? !home_credits : !frontend_settings()->cont) { game_over(); return FALSE; }   /* Home: no credits left */
    begin();
    cf_begin();
    big_text(X(17), Y(8), "CONTINUE", 1);
    credit_display();
    big(X(23), Y(11), '9', 2);
    sound_post(SND_MUSIC_CONTINUE);
    pad_prev = JOY_readJoypad(JOY_1);
    cf_fade_in(0, 15, 11);
    t = 104;                                    /* 9 shows 104 frames counting its fade-in */
    for (;;)
    {
        u16 held, e;
        cf_frame(0);
        SYS_doVBlankProcess();
        e = pad_edge(&held);
        if ((held & BUTTON_START) && (fe.mode != FE_ARCADE || credits) && (digit || grace >= 12)) break;   /* level, not edge */
        if (digit && digit < 8 && (e & (BUTTON_A | BUTTON_B))) { digit = 1; t = 1; }
        if (digit)
        {
            if (--t) continue;
            digit--;
            sound_post(0x42);
            d[0] = '0' + digit;
            big(X(23), Y(11), d[0], 2);
            t = 52;
            if (!digit) grace = 64;
            continue;
        }
        if (!--grace)
        {
            /* GAME OVER over the figures (the old man's last pose). */
            VDP_clearPlane(BG_A, TRUE);
            big_text(X(15), Y(12), "GAME  OVER", 1);
            sound_post(SND_MUSIC_OVER);
            cf_frames(120, 2);
            PAL_fadeOutAll(16, FALSE);
            VDP_resetSprites(); VDP_updateSprites(1, DMA);
            return FALSE;
        }
    }
    /* Continue (7.4): a credit (Arcade), 120 frames, then the restart. */
    if (fe.mode == FE_ARCADE) { credits--; credit_display(); }
    else { home_credits--; credit_display(); }
    fe.continued = 1;
    cf_frames(120, 1);
    PAL_fadeOutAll(16, FALSE);
    VDP_resetSprites(); VDP_updateSprites(1, DMA);
    return TRUE;
}

/* ---- Stage 5's player select (specs/stage5.md 1.3, 0xA1B8): the prompt
 * typed (sound 0x2F a letter), then a 10-count of 32 updates each from
 * 32 updates after the prompt; Left picks Willow, Right Madmartigan (a new
 * press), A or B confirms; at 0 the current choice (Willow by default). The
 * chosen figure stays 63 updates, the other leaves. Text in the arcade's
 * orange text palette (0x0E); the figures on lines 2 / 3 in their own
 * colours, the cursor on the stage's sprite line 1. Arcade positions: cursor
 * (64 / 224, 40), Willow (112, 72), Madmartigan (272, 80); screen x = x - 32,
 * y = 239 - y. Returns the form (0 Willow, 1 Madmartigan). */
extern const u16 willow_pal[], mad_pal[], s5_sprpal[], sorsha_pal[];
extern u8 coop_partner;                     /* src/coop.c: Willow's partner in Home co-op (1 Madmartigan, 2 Sorsha) */
#define SEL_VRAM (BIG + 144)

static u16 sel_cursor(VDPSprite *sat, u16 first, s16 sx, s16 sy)
{
    const u8 *dir = s5_cursor_poses, *e = s5_cursor_poses + S5_CURSOR_POSES * 6 + ((dir[4] << 8) | dir[5]) * 6;
    u16 nspr = dir[3], i;
    for (i = 0; i < nspr; i++, e += 6)
    {
        VDPSprite *v = sat + first + i;
        v->y = 128 + sy + (s16)((e[0] << 8) | e[1]);
        v->size = e[2] & 15;
        v->attribut = TILE_ATTR_FULL(PAL1, 1, 0, e[2] >> 7, SEL_VRAM + e[3]);
        v->x = 128 + sx + (s16)((e[4] << 8) | e[5]);
        v->link = first + i + 1;
    }
    return nspr;
}

/* Stage 5's select screen; two = co-op (Home 2 PLAYERS, specs/coop.md 2.1):
 * both heroes stay shown, "1P" / "2P" over them (pad 1 picks its hero with
 * Left / Right, pad 2 plays the other), A, B or Start on either pad confirms. */
static u16 select_run(bool two)
{
    static const char prompt[] = "LET'S SELECT PLAYERS.";
    u16 choice = 0, typed = 0, wait = 3, timer = 320, hold = 0, chosen = 0, t = 0, n, last2 = 0xFFFF, shown = 2, partner = 1;
    char d[3] = "10";
    begin();
    {
        const u8 *dir = s5_cursor_poses;
        VDP_loadTileData((const u32 *)(s5_cursor_tiles + ((dir[0] << 8) | dir[1]) * 32), SEL_VRAM, dir[2], DMA);
    }
    memcpy(fade_to, font_textpal + 14 * 16, 32);
    memcpy(fade_to + 16, s5_sprpal + 16, 32);
    memcpy(fade_to + 32, willow_pal, 32);
    memcpy(fade_to + 48, mad_pal, 32);
    fade_to[0] = 0;
    big_text(16, 6, "TIME", 0);
    big_text(18, 9, d, 0);
    put(2, 23, "WILLOW UFGOOD", 0);
    put(25, 23, "MADMARTIGAN", 0);
    if (two) put(6, 25, "UP/DOWN  MADMARTIGAN OR SORSHA", 0);
    pad_prev = JOY_readJoypad(JOY_1);
    for (;;)
    {
        u16 e = pad_edge(NULL), e2 = 0;
        VDPSprite *sat = vdpSpriteCache;
        if (two) { u16 p2 = coop_pad2(); e2 = p2 & ~last2; last2 = p2; }
        n = 0;
        if (two || !chosen || choice == 0) n += player_pose_put(0, player_form_stand(0), sat, n, SEL_VRAM + 64, 80, 167, PAL2);
        if (two || !chosen || choice == 1) n += player_pose_put(partner, player_form_stand(partner), sat, n, SEL_VRAM + 160, 240, 159, PAL3);
        if (!chosen && (t & 4)) n += sel_cursor(sat, n, choice ? 192 : 32, 199);
        if (!n) { sat[0].y = 0; sat[0].link = 0; n = 1; }
        sat[n - 1].link = 0;
        VDP_updateSprites(n, DMA_QUEUE);
        SYS_doVBlankProcess();
        if (t == 1) PAL_fadeInAll(fade_to, 16, TRUE);
        t++;
        if (two && shown != choice)
        {
            shown = choice;
            put(9, 15, choice ? "2P" : "1P", 0);
            put(30, 15, choice ? "1P" : "2P", 0);
        }
        if (chosen) { if (++hold >= 63) break; continue; }
        if (prompt[typed])
        {
            if (--wait) continue;
            wait = 3;
            put(9 + typed, 2, (char[]){ prompt[typed], 0 }, 0);
            sound_post(0x2F);
            if (!prompt[++typed]) wait = 32;
            continue;
        }
        if (wait) { wait--; continue; }
        if (two && (e & (BUTTON_UP | BUTTON_DOWN)))   /* Home co-op: Willow's partner, Madmartigan or Sorsha (user) */
        {
            partner ^= 3;
            put(25, 23, partner == 2 ? "SORSHA     " : "MADMARTIGAN", 0);
            PAL_setColors(48, partner == 2 ? sorsha_pal : mad_pal, 16, DMA_QUEUE);
        }
        if (e & BUTTON_RIGHT) choice = 1;
        if (e & BUTTON_LEFT) choice = 0;
        if (e & (BUTTON_A | BUTTON_B)) { chosen = 1; continue; }
        if (two && ((e | e2) & (BUTTON_A | BUTTON_B | BUTTON_START))) { chosen = 1; continue; }
        if (!--timer) { big_text(18, 9, "00", 0); chosen = 1; continue; }
        {
            u16 k = ((timer - 1) >> 5) + 1;
            d[0] = k >= 10 ? '1' : '0'; d[1] = '0' + k % 10;
            if (!((timer - 1) & 31) || timer == 319) big_text(18, 9, d, 0);
        }
    }
    PAL_fadeOutAll(16, FALSE);
    VDP_resetSprites(); VDP_updateSprites(1, DMA);
    if (two) coop_partner = partner;
    return choice;
}

u16 select_screen(void) { return select_run(FALSE); }
u16 coop_select_screen(void) { return select_run(TRUE); }

/* ---- The attract loop (3.1): pictures (tools/attractpack.py: plane B, CRAM
 * lines 1-3), typed text and INSERT COIN on plane A (line 0), the ranking.
 * It ends when a coin brings a credit (-> the credit screen) or on A+B+C+Start
 * (-> the port's menu: TRUE). Not shown: the cave's lightning cycle,
 * Bavmorda fading in over the cave, the logo's white flash, the Eborsisk's
 * fire breath, the demo play (3.2). */
#include "../data/attract/attract.h"
#define PIC (BIG + 144)                     /* picture tiles after the fonts */
#define DECL(n) extern const u8 n##_tiles[]; extern const u16 n##_map[], n##_pal[]
DECL(att_cave); DECL(att_willow); DECL(att_mad); DECL(att_logo); DECL(att_capcom); DECL(att_fbi); DECL(att_dragon);
static u8 att_exit;                         /* 1: a credit, 2: back to the menu */
static u16 att_blink;                       /* INSERT COIN (0: off) */
static u8 att_home;                         /* Home's idle loop: no coins, no demo; any button ends it (3) */
static volatile u16 att_seen;               /* Home: buttons seen held by the VInt read (a picture unpacks for ~35 frames) */
static void att_vint(void)
{
    JOY_update();
    att_seen |= JOY_readJoypad(JOY_1);
}

static bool att_wait(u16 n)
{
    while (n--)
    {
        u16 held, edge, before = pad_prev, seen;
        SYS_doVBlankProcess();
        edge = pad_edge(&held);
        seen = att_seen; att_seen = 0;
        if (att_home) { if (edge || (seen & ~before)) att_exit = 3; }
        else if (credits) att_exit = 1;
        if ((held & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START)) == (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START)) att_exit = 2;
        if (att_blink)                      /* 16 on, 16 off */
        {
            if (!(att_blink & 15)) put(15, 14, (att_blink & 16) ? "           " : "INSERT COIN", 0);
            att_blink++;
        }
        if (att_exit) return FALSE;
    }
    return TRUE;
}

/* Home: fades run in the background so a press during one still returns
 * at once (the Arcade's fades block, as before). */
static void att_fadewait(void)
{
    while (PAL_isDoingFade())
        if (!att_wait(1)) { PAL_interruptFade(); return; }
}

static void att_picture(const u8 *tiles, u16 ntiles, const u16 *map)
{
    static u16 w[40];
    u16 r, c;
    load_packed(tiles, PIC, ntiles);       /* aPLib (tools/pack_compress.sh) */
    map = map_open(map);
    for (r = 0; r < 28; r++)
    {
        for (c = 0; c < 40; c++) w[c] = map[r * 40 + c] + PIC;
        VDP_setTileMapDataRow(BG_B, w, r, 0, 40, CPU);
    }
    map_close();
}

/* Fade in a picture's lines 1-3 (line 0: text palette tp; FBI: all four). */
static void att_show(const u16 *pal, u16 tp, bool all)
{
    memcpy(fade_to, pal, 128);
    if (!all) memcpy(fade_to, font_textpal + tp * 16, 32);
    fade_to[0] = 0;
    PAL_fadeInAll(fade_to, 16, att_home);
    if (att_home) att_fadewait();
}

static u16 att_line;                        /* the CRAM line text is typed on */
static bool att_type(const char *const *t, const u8 *x, const u8 *y, u16 n, u16 tick)
{
    u16 i, k;
    for (i = 0; i < n; i++)
        for (k = 1; t[i][k - 1]; k++)
        {
            char s[2] = { t[i][k - 1], 0 };
            put(x[i] + k - 1, y[i], s, att_line);
            if (!att_wait(tick)) return FALSE;
        }
    return TRUE;
}

static bool att_fade(void)
{
    PAL_fadeOutAll(16, att_home);
    if (att_home) att_fadewait();
    if (att_exit) return FALSE;
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    att_blink = 0;
    return att_wait(33) && !att_exit;
}

u16 attract_loop(u16 from)
{
    static const char *const she[3] = { "SHE WANTS TO DESTROY THE HOLY BABY", "ELORA DANAN!", "THE EVIL QUEEN BAVMORDA" };
    static const u8 she_x[3] = { 2, 2, 2 }, she_y[3] = { 12, 14, 12 };
    static const char *const wil[7] = { "HE WAS SELECTED", "AS A PROTECTOR OF", "ELORA DANAN", "AGAINST QUEEN", "BAVMORDA.", "A PEACE-LOVING MAN.", "WILLOW" };
    static const u8 wil_x[7] = { 22, 22, 22, 22, 22, 21, 25 }, wil_y[7] = { 4, 5, 6, 7, 8, 14, 16 };
    static const char *const mad[6] = { "HE JOINS WILLOW'S", "FIGHT AGAINST", "GENERAL KAEL,", "A FATAL RIVAL.", "A BRAVE WARRIOR.", "MADMARTIGAN" };
    static const u8 mad_x[6] = { 1, 1, 1, 1, 2, 4 }, mad_y[6] = { 4, 5, 6, 7, 16, 18 };
    static const char *const sup[4] = { "A SUPER GAME FROM CAPCOM ...", "COMING THIS SUMMER!",
                                        "A MAGNIFICENT FANTASY FILLED WITH LOVE,", "FRIENDSHIP AND ADVENTURE!" };
    static const u8 sup_x[4] = { 6, 10, 0, 7 }, sup_y[4] = { 12, 14, 12, 14 };
    static const char *const dra[2] = { "THE STORY HAS ALREADY BEGUN", "FOR ALL BRAVE PLAYERS..." };
    static const u8 dra_x[2] = { 2, 2 }, dra_y[2] = { 18, 20 };
    att_exit = 0;
    att_line = 0;
    att_blink = 0;
    for (;;)
    {
        begin();
        if (!att_home) pad_prev = JOY_readJoypad(JOY_1);   /* Home: a press made while a picture loads still counts */
        if (from) { from = 0; goto interlude; }
        /* The cave (t 86-829): red text, 4 frames a letter. */
        att_picture(att_cave_tiles, ATT_CAVE_TILES, att_cave_map);
        att_show(att_cave_pal, 3, FALSE);
        if (!att_wait(240) || !att_type(she, she_x, she_y, 2, 4) || !att_wait(120)) break;
        VDP_clearPlane(BG_A, TRUE);
        if (!att_type(she + 2, she_x + 2, she_y + 2, 1, 4) || !att_wait(100) || !att_fade()) break;
        /* Willow (t 830-1550): light-blue text, then the portrait and his name. */
        PAL_setColors(0, font_textpal, 16, DMA); PAL_setColor(0, 0);
        if (!att_type(wil, wil_x, wil_y, 6, 4) || !att_wait(60)) break;
        att_picture(att_willow_tiles, ATT_WILLOW_TILES, att_willow_map);
        att_show(att_willow_pal, 0, FALSE);
        PAL_setColors(16, font_textpal + 2 * 16, 16, DMA);   /* line 1: the name, yellow */
        att_line = 1;
        if (!att_type(wil + 6, wil_x + 6, wil_y + 6, 1, 4)) break;
        att_line = 0;
        if (!att_wait(180) || !att_fade()) break;
        /* Madmartigan (t 1550-2196). */
        PAL_setColors(0, font_textpal, 16, DMA); PAL_setColor(0, 0);
        if (!att_type(mad, mad_x, mad_y, 5, 4) || !att_wait(60)) break;
        att_picture(att_mad_tiles, ATT_MAD_TILES, att_mad_map);
        att_show(att_mad_pal, 0, FALSE);
        PAL_setColors(16, font_textpal + 2 * 16, 16, DMA);
        att_line = 1;
        if (!att_type(mad + 5, mad_x + 5, mad_y + 5, 1, 4)) break;
        att_line = 0;
        if (!att_wait(180) || !att_fade()) break;
        /* Yellow lines (t 2197-2972). */
        PAL_setColors(0, font_textpal + 2 * 16, 16, DMA); PAL_setColor(0, 0);
        if (!att_type(sup, sup_x, sup_y, 2, 4) || !att_wait(120)) break;
        VDP_clearPlane(BG_A, TRUE);
        if (!att_type(sup + 2, sup_x + 2, sup_y + 2, 2, 4) || !att_wait(120) || !att_fade()) break;
        /* The logo, Lucasfilm's notice, INSERT COIN (t 2973-3584), the
         * Capcom credits and the FBI's line: Arcade only (Home: user, "take
         * this out of the attract"). */
        if (att_home) goto interlude;
        att_picture(att_logo_tiles, ATT_LOGO_TILES, att_logo_map);
        att_show(att_logo_pal, 2, FALSE);
        sound_post(0x81);
        att_blink = 16;
        if (!att_wait(280)) break;
        att_picture(att_capcom_tiles, ATT_CAPCOM_TILES, att_capcom_map);
        PAL_setColors(16, att_capcom_pal + 16, 48, DMA);
        if (!att_wait(180) || !att_fade()) break;
        /* The FBI's line (t 3639-3808). */
        att_picture(att_fbi_tiles, ATT_FBI_TILES, att_fbi_map);
        att_show(att_fbi_pal, 0, TRUE);
        if (!att_wait(120) || !att_fade()) break;
        /* The demo play (3.2): the game runs it (src/main.c), then comes back here. */
        PAL_fadeOutAll(16, FALSE);
        return ATT_DEMO;
interlude:
        /* The interlude (t 5002-5527). */
        att_picture(att_dragon_tiles, ATT_DRAGON_TILES, att_dragon_map);
        att_show(att_dragon_pal, 0, FALSE);
        if (!att_type(dra, dra_x, dra_y, 2, 4) || !att_wait(200) || !att_fade()) break;
        /* The ranking (180 frames); Home: its own table, a button ends it. */
        if (att_home)
        {
            begin();
            ranking_rows(10);
            memcpy(fade_to, font_textpal + 3 * 16, 32); memcpy(fade_to + 16, font_textpal + 5 * 16, 32);
            memcpy(fade_to + 32, font_textpal + 2 * 16, 32); memcpy(fade_to + 48, font_textpal, 32);
            fade_to[0] = 0;
            PAL_fadeInAll(fade_to, 16, TRUE);   /* as fade_in(3, 5, 2, 0), in the background */
            att_fadewait();
            if (att_exit || !att_wait(180)) break;
            PAL_fadeOutAll(16, TRUE);
            att_fadewait();
            if (att_exit) break;
            continue;
        }
        ranking_screen();
        if (credits) att_exit = 1;
        if (att_exit) break;
    }
    PAL_fadeOutAll(16, FALSE);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    return att_exit == 2 ? ATT_MENU : ATT_CREDIT;
}

/* The demo's end (3.2, 0x8E14): the stage's title typed over the frozen
 * play, 180 frames. */
void attract_demo_title(u16 stage)
{
    static const char *const t[2][2] = { { "STAGE1", "CROSSROAD" }, { "STAGE2", "CHERLINDREA'S FOREST" } };
    sound_post(SND_STOP);
    hud_message(t[stage][0], 10);           /* the HUD's message rows (the play's tiles stay) */
    hud_message(t[stage][1], 12);
    att_exit = 0;
    att_blink = 0;
    att_wait(180);
    hud_message(NULL, 10);
    hud_message(NULL, 12);
}

/* Home sitting idle: the attract loop's story pictures in the arcade's
 * order and timing (the cave, Willow, Madmartigan, the yellow lines, the
 * Eborsisk), the Home ranking in the arcade ranking's place; no logo/notice,
 * Capcom credits, FBI screen or demo play; until any button. */
void attract_home(void)
{
    att_home = 1;
    pad_prev = JOY_readJoypad(JOY_1);
    att_seen = 0;
    SYS_setVIntCallback(att_vint);          /* the next stage's load sets its own (stage_init) */
    attract_loop(0);
    SYS_setVIntCallback(NULL);
    att_home = 0;
    att_exit = 0;
}

bool attract_exit_menu(void) { return att_exit == 2; }

/* ---- Sorsha's Quest story pages (src/quest.c, out/audit/quest/PLAN.md):
 * a picture with the page's lines typed under it, 4 updates a letter (A, B
 * or C: the rest at once; then any press goes on). Pictures marked NEW in
 * the plan are the user's pictures (pages 1, 6, 7; banks 8-9 are in for
 * pages 1 and 7, stage 2-5 data for page 6). */
#include "../data/intro/story3.h"
#include "../data/intro/story4.h"
#include "../data/intro/story6.h"
extern const u8 story3_tiles[], story4_tiles[], story6_tiles[];
extern const u16 story3_map[], story3_pal[], story4_map[], story4_pal[], story6_map[], story6_pal[];
extern const u8 qpic1_tiles[], qpic6_tiles[], qpic7_tiles[];
extern const u16 qpic1_map[], qpic6_map[], qpic7_map[], qpic1_pal[], qpic6_pal[], qpic7_pal[];
#include "../data/quest/quest.h"
void quest_page(u16 n)
{
    static const char *const text[8][4] = {
        { "QUEEN BAVMORDA FEARS A CHILD", "WHO WILL END HER REIGN.", "SHE SENDS HER DAUGHTER SORSHA", "TO FIND IT." },
        { "SORSHA RIDES OUT OF NOCKMAAR", "AT THE HEAD OF HER MOTHER'S", "ARMY.", NULL },
        { "A DAIKINI AND A NELWYN HAVE", "TAKEN THE CHILD TO THE", "CROSSROADS FORT.", NULL },
        { "THE CHILD IS HIDDEN ON", "FIN RAZIEL'S ISLAND.", "SORSHA CROSSES THE LAKE.", NULL },
        { "AT THE SNOW CAMP A PRISONER", "CALLS HER NAME AND SPEAKS OF", "LOVE. SHE DRAWS HER SWORD.", NULL },
        { "AT TIR ASLEEN THE CURSE FALLS.", "SORSHA TURNS HER SWORD", "AGAINST HER MOTHER'S BEAST.", NULL },
        { "WITH WILLOW AND MADMARTIGAN", "SHE RIDES BACK TO NOCKMAAR", "TO FACE HER MOTHER.", NULL },
        { "BAVMORDA IS GONE.", "THE CHILD IS SAFE.", "SORSHA IS FREE.", NULL },
    };
    static const struct { const u8 *tiles; const u16 *map, *pal; u16 ntiles; u8 line, tp, row, step; } pic[8] = {
        { att_cave_tiles, att_cave_map, att_cave_pal, ATT_CAVE_TILES, 0, 3, 19, 2 },
        { story6_tiles, story6_map, story6_pal, STORY6_TILES, 1, 0, 22, 1 },
        { att_willow_tiles, att_willow_map, att_willow_pal, ATT_WILLOW_TILES, 0, 0, 22, 2 },
        { story3_tiles, story3_map, story3_pal, STORY3_TILES, 1, 0, 22, 1 },
        { att_mad_tiles, att_mad_map, att_mad_pal, ATT_MAD_TILES, 0, 0, 19, 2 },
        { att_dragon_tiles, att_dragon_map, att_dragon_pal, ATT_DRAGON_TILES, 0, 0, 19, 2 },
        { story4_tiles, story4_map, story4_pal, STORY4_TILES, 1, 0, 22, 1 },
        { story6_tiles, story6_map, story6_pal, STORY6_TILES, 1, 0, 22, 1 },
    };
    u16 i, k, fast = 0, last = JOY_readJoypad(JOY_1);
    extern u8 game_hide_sprites;
    game_hide_sprites = 1;                  /* the play's sprites stay off (main.c) */
    /* The user's pictures (tools/questpic.py): 18 x 17 cells with the gold
     * frame, lines 1-3; text on line 0 below. */
    static const u8 *const qtiles[3] = { qpic1_tiles, qpic6_tiles, qpic7_tiles };
    static const u16 *const qmap[3] = { qpic1_map, qpic6_map, qpic7_map }, *const qpal[3] = { qpic1_pal, qpic6_pal, qpic7_pal };
    static const u16 qn[3] = { QPIC1_TILES, QPIC6_TILES, QPIC7_TILES };
    s16 q = n == 1 ? 0 : n == 6 ? 1 : n == 7 ? 2 : -1;
    u16 row0 = q >= 0 ? 20 : pic[n].row, step = q >= 0 ? 2 : pic[n].step, line = q >= 0 ? 0 : pic[n].line;
    PAL_fadeOutAll(16, FALSE);
    begin();
    if (q >= 0)
    {
        const u16 *m;
        u16 r, c, w[18];
        load_packed(qtiles[q], PIC, qn[q]);
        m = map_open(qmap[q]);
        for (r = 0; r < 17; r++)
        {
            for (c = 0; c < 18; c++) w[c] = m[r * 18 + c] + PIC;
            VDP_setTileMapDataRow(BG_B, w, r + 1, 11, 18, CPU);
        }
        map_close();
        memcpy(fade_to, qpal[q], 128);
        memcpy(fade_to, font_textpal, 32);
    }
    else
    {
        att_picture(pic[n].tiles, pic[n].ntiles, pic[n].map);
        memcpy(fade_to, pic[n].pal, 128);
        memcpy(fade_to + pic[n].line * 16, font_textpal + pic[n].tp * 16, 32);
    }
    fade_to[0] = 0;
    PAL_fadeInAll(fade_to, 16, FALSE);
    for (i = 0; i < 4 && text[n][i]; i++)
    {
        const char *t = text[n][i];
        u16 x = 20 - (strlen(t) + 1) / 2, y = row0 + i * step;
        for (k = 1; t[k - 1]; k++)
        {
            char s[2] = { t[k - 1], 0 };
            put(x + k - 1, y, s, line);
            if (fast) continue;
            {
                u16 w = 4;
                while (w--)
                {
                    u16 h = JOY_readJoypad(JOY_1);
                    SYS_doVBlankProcess();
                    if (h & ~last & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START)) fast = 1;
                    last = h;
                }
                if (s[0] != ' ') sound_post(0x2F);
            }
        }
    }
    for (i = 0; i < 300; i++)               /* about 5 s, or a press */
    {
        u16 h = JOY_readJoypad(JOY_1);
        SYS_doVBlankProcess();
        if (i > 20 && (h & ~last & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START))) break;
        last = h;
    }
    PAL_fadeOutAll(16, FALSE);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    game_hide_sprites = 0;
}
