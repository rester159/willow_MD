/* The port's front end (AGENTS.md "Home/Arcade frontend and saves"): a
 * Home / Arcade selector with the credit line, each mode's menu (options or
 * DIP switches, high scores, controls, music select), settings and high
 * scores kept per mode in save RAM, Home's NEW GAME PLUS once unlocked.
 * Port additions, not arcade behaviour; the arcade's own attract, coins and
 * name entry belong to Arcade mode (specs/arcade_frontend.md).
 *
 * Save RAM (src/cart.c: standard SSF2's odd bytes at 0x200001, or extended
 * SSF's bank 31): paged in for each access, interrupts masked and the Z80
 * held (the ROM there is hidden meanwhile): two copies of one
 * record, each with a sequence number and a checksum; a save writes the
 * older copy, a load takes the newest valid one, so an interrupted write
 * leaves the previous record. Text: the arcade's font (tools/fontpack.py),
 * tiles at VRAM 16 (the stage pool: the intro reloads it), plane A. */
#include <genesis.h>
#include "coop.h"
#include "screens.h"
#include "frontend.h"
#include "sound.h"
#include "stage.h"
#include "cart.h"

extern const u8 font_tiles[];
extern const u16 font_pal[];

#define FONT 16                     /* VRAM tile of ' ' */
#define LOGO 128                    /* the opening screen's title tiles */
#define LOGO_TILES 181
extern const u8 logo_tiles[];
extern const u16 logo_map[], logo_pal[];
#define REC_MAGIC 0x574C            /* 'WL' */
#define REC_VERSION 1
#define REC_SIZE (sizeof(SaveRec))
#define SLOT(k) ((u32)(k) * 256)

typedef struct
{
    u16 magic;
    u8 version, pad;
    u16 seq;
    Settings set[2];
    u8 scores[2][10][8];            /* BCD score (4 bytes), 3 letters, space */
    u8 plus;                        /* Home NEW GAME PLUS unlocked */
    u8 credits;                     /* Home credits (continues) + 1; 0: the default, 3 (older saves) */
    u16 sum;
} SaveRec;

FrontEnd fe;
static SaveRec rec;
static u8 save_failed;

static const Settings defaults[2] = {
    { 3, 3, 4, 1, 1, 1, 0, 4, 4 },     /* Home: lives 3 */
    { 3, 2, 3, 1, 1, 1, 1, 4, 4 },     /* Arcade: the arcade's defaults (lives 2, vitality 3: the traces) */
};
static const u8 rom_scores[10][8] = {   /* ROM 0x183C */
    { 0, 5, 0x50, 0, 'C', 'A', 'P', ' ' }, { 0, 5, 0, 0, 'C', 'O', 'M', ' ' },
    { 0, 4, 0x50, 0, 'C', 'A', 'P', ' ' }, { 0, 4, 0, 0, 'C', 'O', 'M', ' ' },
    { 0, 3, 0x50, 0, 'C', 'A', 'P', ' ' }, { 0, 3, 0, 0, 'C', 'O', 'M', ' ' },
    { 0, 2, 0x50, 0, 'C', 'A', 'P', ' ' }, { 0, 2, 0, 0, 'C', 'O', 'M', ' ' },
    { 0, 1, 0x50, 0, 'C', 'A', 'P', ' ' }, { 0, 1, 0, 0, 'C', 'O', 'M', ' ' },
};

/* ---- Save RAM. */
static u16 rec_sum(const SaveRec *r)
{
    const u8 *p = (const u8 *)r;
    u16 i, h = 0x5A5A;
    for (i = 0; i < REC_SIZE - 2; i++) h = (u16)((h << 1 | h >> 15) ^ p[i]);
    return h;
}

static bool settings_ok(const Settings *s)
{
    return s->difficulty < 8 && s->lives >= 1 && s->lives <= 4 && s->vitality >= 2 && s->vitality <= 5 &&
           s->cont < 2 && s->nando < 4 && s->magic < 2 && s->demo < 2 && s->coinA < 8 && s->coinB < 8;
}

static bool slot_read(u16 k, SaveRec *r)
{
    u8 *p = (u8 *)r;
    u16 i;
    cart_sram_begin(FALSE);                     /* the ROM there is hidden meanwhile (src/cart.c) */
    for (i = 0; i < REC_SIZE; i++) p[i] = cart_sram_read(SLOT(k) + i);
    cart_sram_end();
    return r->magic == REC_MAGIC && r->version == REC_VERSION && r->sum == rec_sum(r) &&
           settings_ok(&r->set[0]) && settings_ok(&r->set[1]);
}

static void defaults_all(void)
{
    memset(&rec, 0, sizeof rec);
    rec.magic = REC_MAGIC;
    rec.version = REC_VERSION;
    rec.set[0] = defaults[0];
    rec.set[1] = defaults[1];
    memcpy(rec.scores[0], rom_scores, sizeof rom_scores);
    memcpy(rec.scores[1], rom_scores, sizeof rom_scores);
}

void frontend_load(void)
{
    static SaveRec b;
    bool a_ok = slot_read(0, &rec), b_ok = slot_read(1, &b);
    if (b_ok && (!a_ok || (s16)(b.seq - rec.seq) > 0)) rec = b;
    else if (!a_ok) defaults_all();
}

void frontend_save(void)
{
    static SaveRec check;
    u8 *p = (u8 *)&rec;
    u16 i, k;
    rec.seq++;
    k = rec.seq & 1;                    /* the other copy than the newest record's (seq alternates parity) */
    rec.sum = rec_sum(&rec);
    cart_sram_begin(TRUE);
    for (i = 0; i < REC_SIZE; i++) cart_sram_write(SLOT(k) + i, p[i]);
    cart_sram_end();
    save_failed = !slot_read(k, &check) || check.seq != rec.seq;
}

Settings *frontend_settings(void) { return &rec.set[fe.mode]; }
/* Home credits (user, 2026-11-03): continues per game, 0..9, default 3;
 * shared by both players in co-op. */
u8 home_credits;
u8 frontend_credits(void) { return rec.credits ? rec.credits - 1 : 3; }
bool frontend_plus(void) { return rec.plus; }

void frontend_unlock_plus(void)
{
    if (rec.plus) return;
    rec.plus = 1;
    frontend_save();
}

const u8 *frontend_scores(u16 mode) { return rec.scores[mode][0]; }

/* A score's place in the mode's table (0..9), 10 if it does not qualify. */
u16 frontend_rank(u32 bcd)
{
    if (fe.debug) return 10;                /* DEBUG runs: nothing ranked or saved */
    u16 i;
    for (i = 0; i < 10; i++)
    {
        const u8 *e = rec.scores[fe.mode][i];
        u32 v = ((u32)e[0] << 24) | ((u32)e[1] << 16) | ((u32)e[2] << 8) | e[3];
        if (bcd > v) return i;
    }
    return 10;
}

void frontend_insert(u16 place, u32 bcd, const char *name)
{
    u8 (*t)[8] = rec.scores[fe.mode];
    if (place >= 10) return;
    {
        s16 k;
        for (k = 8; k >= (s16)place; k--) memcpy(t[k + 1], t[k], 8);   /* down one, from the bottom */
    }
    t[place][0] = bcd >> 24; t[place][1] = bcd >> 16; t[place][2] = bcd >> 8; t[place][3] = bcd;
    memcpy(t[place] + 4, name, 3);
    t[place][7] = ' ';
    frontend_save();
}

u32 frontend_best(void)
{
    const u8 *e = rec.scores[fe.mode][0];
    return ((u32)e[0] << 24) | ((u32)e[1] << 16) | ((u32)e[2] << 8) | e[3];
}

/* ---- Text on plane A: a page is built in RAM (scr) and sent in the
 * next VBlank as a whole, so no half-drawn or blank frame shows. */
static u16 *scr;                            /* 28 x 40 words (plane A), then 11 x 40 (the logo); heap, while the menu runs */

static void text(u16 x, u16 y, const char *s, bool hi)
{
    u16 *w = scr + y * 40 + x, n = 0;
    while (*s && x + n < 40)
    {
        u8 c = (u8)*s++;
        if (c >= 'a' && c <= 'z') c -= 32;
        w[n++] = TILE_ATTR_FULL(hi ? PAL1 : PAL0, 1, 0, 0, FONT + (c >= 32 && c < 96 ? c - 32 : 0));
    }
}

static void clear(void)
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
}

/* A page redraw: the text buffer emptied; plane B's logo rows written
 * or cleared only when a logo page starts or ends. Whole-plane DMA fills
 * here ran into the shown frame and blanked it (a black frame on every
 * cursor move, up to 05p). */
static u8 logo_shown;
static bool clear_page(bool logo)
{
    bool fresh = logo && !logo_shown;
    memset(scr, 0, 28 * 40 * 2);
    if (logo_shown && !logo)                    /* the logo's rows, in the same VBlank as the page */
    {
        static const u16 blank[40];
        u16 r;
        for (r = 0; r < 11; r++) VDP_setTileMapDataRow(BG_B, blank, r, 0, 40, DMA_QUEUE);
    }
    logo_shown = logo;
    return fresh;
}

/* ---- Pages (as the B1 build's menus, port.c). */
enum { P_SELECT, P_HOME, P_ARCADE, P_OPTIONS, P_DIPS, P_CONTROLS, P_SCORES, P_MUSIC, P_DEBUG, P_PAGES };
static u8 pg, row, dirty, track;

static const struct { u8 cmd; char name[13]; } tracks[] = {
    { SND_MUSIC_STAGE1, "STAGE 1" }, { SND_MUSIC_BOSS, "STAGE 1 BOSS" }, { SND_MUSIC_STAGE2, "STAGE 2" },
    { SND_MUSIC_CHASE, "THE CHASE" }, { SND_MUSIC_SHOP, "SHOP" }, { SND_MUSIC_MAP, "WORLD MAP" },
    { SND_MUSIC_CLEAR, "STAGE CLEAR" }, { SND_MUSIC_CONTINUE, "CONTINUE" }, { SND_MUSIC_OVER, "GAME OVER" },
};
#define NTRACKS (sizeof tracks / sizeof tracks[0])

#if defined(COOP) || defined(COOP_TEST)
#define COOP_ROW 1                          /* Home's 2 PLAYERS row (co-op, src/coop.c) */
#else
#define COOP_ROW 0
#endif

static u16 rows(void)
{
    static const u8 n[P_PAGES] = { 2, 2, 4, 7, 9, 0, 0, 2, 5 };   /* no BACK rows: B backs out (user) */
    return n[pg] + (pg == P_HOME ? rec.plus + COOP_ROW + 1 : 0);   /* + SORSHA'S QUEST */
}

/* Home/Arcade page row -> its action: 0 start, 1 options, 2 scores, 3
 * controls, 4 back, 5 NEW GAME PLUS, 6 two players. Home rows: START GAME,
 * [2 PLAYERS], [NEW GAME PLUS], OPTIONS (B goes back; its ranking shows
 * when the page sits idle; CONTROLS is in OPTIONS). Arcade: all five in order. */
static u16 home_row(u16 r)
{
    if (pg != P_HOME || !r) return r;
    if (COOP_ROW) { if (r == 1) return 6; r--; }
    if (rec.plus) { if (r == 1) return 5; r--; }
    if (r == 1) return 7;                   /* SORSHA'S QUEST */
    return r - 1;
}

static u16 idle_wait;                       /* frames without a press on the Home page */
#define IDLE_ATTRACT 600

static void page(u16 p) { pg = p; row = 0; dirty = 1; idle_wait = 0; }

static u8 cyc(u8 v, u16 first, u16 count, s16 d) { return first + (v - first + count + d) % count; }

static void put_number(char *p, u16 v) { p[0] = '0' + v; p[1] = 0; }

static void draw(void)
{
    static const char *const heads[P_PAGES] = { "SELECT MODE", "HOME", "ARCADE", "OPTIONS", "DIP SWITCHES",
                                                "CONTROLS", "HIGH SCORES", "MUSIC SELECT", "DEBUG" };
    static const char *const common[6] = { "DIFFICULTY", "LIVES", "VITALITY", "CONTINUE", "NANDO SPEED", "MAGIC CONTINUE" };
    const char *labels[10] = { 0 };
    u16 count = rows(), arcade = fe.mode == FE_ARCADE, n, first, last;
    bool logo = clear_page(pg == P_SELECT || pg == P_HOME);
    if (pg == P_SELECT || pg == P_HOME)
    {
        /* The opening screen and Home: the arcade's title (tools/logopack.py,
         * plane B, lines 2-3); the opening screen adds the modes, the credit
         * and the version. */
        u16 *w = scr + 28 * 40, k;                  /* sent with the page, in the same VBlank */
        for (k = 0; logo && k < 11 * 40; k++) w[k] = logo_map[k] ? logo_map[k] + LOGO : 0;
        if (logo) VDP_setTileMapDataRect(BG_B, w, 0, 0, 40, 11, 40, DMA_QUEUE);
        if (pg == P_SELECT) { labels[0] = "ARCADE"; labels[1] = "HOME"; }
    }
    if (pg == P_SELECT)
    {
        /* Credit centred and version at the right on row 25 (lines
         * 200-207), inside the picture on displays that crop the edges. */
        text(12, 25, "RESTER159, 2026", FALSE);
        text(35, 25, "V0.9", FALSE);
    }
    else if (pg != P_HOME)                      /* Home: the big logo above */
    {
        if (pg != P_OPTIONS) text(17, 2, "WILLOW", TRUE);   /* Options: its heading only (user) */
        text(20 - (strlen(heads[pg]) >> 1), 5, heads[pg], TRUE);
    }
    if (pg == P_DEBUG)
    {
        char st[2], cp[3];
        labels[0] = "STAGE"; labels[1] = fe.debug_stage == 5 ? "AREA" : "START POINT"; labels[2] = "INVINCIBLE";
        labels[3] = "START GAME"; labels[4] = "HERO";
        put_number(st, fe.debug_stage + 1);
        cp[0] = fe.debug_cp >= 10 ? '1' : '0' + fe.debug_cp; cp[1] = fe.debug_cp >= 10 ? '0' + fe.debug_cp - 10 : 0; cp[2] = 0;
        text(25, 8, st, TRUE);
        text(25, 10, cp, TRUE);
        text(25, 12, fe.invincible ? "ON" : "OFF", TRUE);
        text(25, 16, fe.sorsha ? "SORSHA" : "STAGE'S", TRUE);
        text(6, 19, "LEFT/RIGHT CHANGE", FALSE);
        text(6, 21, "SCORES ARE NOT SAVED", FALSE);
    }
    else if (pg == P_HOME || pg == P_ARCADE)
    {
        labels[0] = arcade ? "ENTER ARCADE" : "START GAME";
        labels[1] = arcade ? "DIP SWITCHES" : "OPTIONS";
        labels[2] = "HIGH SCORES"; labels[3] = "CONTROLS";
        if (!arcade) { labels[1] = "SORSHA'S QUEST"; labels[2] = "OPTIONS"; labels[3] = 0; }   /* Home: START GAME, SORSHA'S QUEST, OPTIONS (B goes back) */
        if (!arcade && rec.plus) { for (n = 5; n > 1; n--) labels[n] = labels[n - 1]; labels[1] = "NEW GAME PLUS"; }
        if (!arcade && COOP_ROW) { for (n = 6; n > 1; n--) labels[n] = labels[n - 1]; labels[1] = "2 PLAYERS"; }
    }
    else if (pg == P_OPTIONS || pg == P_DIPS)
    {
        for (n = 0; n < 6; n++) labels[n] = common[n];
        if (pg == P_OPTIONS) { labels[3] = "CREDITS"; labels[5] = "MUSIC SELECT"; labels[6] = "CONTROLS"; }   /* B goes back */
        else { labels[6] = "DEMO SOUND"; labels[7] = "COIN A"; labels[8] = "COIN B"; }
    }
    else if (pg == P_MUSIC)
    {
        labels[0] = "PLAY"; labels[1] = "STOP";
        text(8, 15, "<", TRUE); text(10, 15, tracks[track].name, TRUE); text(11 + strlen(tracks[track].name), 15, ">", TRUE);
        text(6, 18, "LEFT/RIGHT CHANGE TRACK", FALSE);
    }
    else if (pg == P_SCORES)
    {
        for (n = 0; n < 10; n++)
        {
            const u8 *p = rec.scores[fe.mode][n];
            char line[20], *q = line;
            u16 k, lead = 1;
            *q++ = n == 9 ? '1' : ' '; *q++ = '0' + (n + 1) % 10; *q++ = ' '; *q++ = ' ';
            for (k = 0; k < 8; k++)
            {
                u16 v = p[k >> 1] >> ((k & 1) ? 0 : 4) & 15;
                if (v || k == 7) lead = 0;
                *q++ = lead ? ' ' : '0' + v;
            }
            *q++ = ' '; *q++ = ' ';
            for (k = 4; k < 7; k++) *q++ = p[k];
            *q = 0;
            text(10, 7 + n, line, n == 0);
        }
    }
    else if (pg == P_CONTROLS)
    {
        text(6, 11, "D-PAD   MOVE / SELECT", FALSE);
        text(6, 13, "A       ATTACK / SELECT", FALSE);
        text(6, 15, "B       JUMP / BACK", FALSE);
        if (arcade) text(6, 17, "C       INSERT COIN", FALSE);
        text(6, 19, arcade ? "START   START / CONTINUE" : "START   PAUSE / CONTINUE", FALSE);
    }
    /* Seven rows at a time, the selected one scrolled into view. */
    {
        u16 win = 7;
        first = row >= win ? row - win + 1 : 0;
        last = count < first + win ? count : first + win;
    }
    for (n = first; n < last; n++)
    {
        u16 y = pg == P_SCORES ? 19 : (pg == P_SELECT ? 14 : pg == P_HOME ? 13 : 8) + (n - first) * 2;
        u16 x = pg == P_SELECT || pg == P_HOME ? 20 - (strlen(labels[n]) >> 1) : 6;   /* under the logo: centred */
        text(x, y, labels[n], n == row);
        if (n == row) text(x - 2, y, ">", TRUE);
    }
    if (pg == P_OPTIONS || pg == P_DIPS)
    {
        static const char *const speed[4] = { "SLOW", "NORMAL", "FAST", "VERY FAST" };
        static const char *const coins[8] = { "4C 1C", "3C 1C", "2C 1C", "2C 1C*", "1C 1C", "1C 2C", "1C 3C", "1C 4C" };
        const Settings *s = &rec.set[fe.mode];
        char d[2], l[2], v[2];
        const char *values[9];
        u16 nv = pg == P_DIPS ? 9 : 5;
        put_number(d, s->difficulty + 1); put_number(l, s->lives); put_number(v, s->vitality);
        values[0] = d; values[1] = l; values[2] = v; values[3] = s->cont ? "ON" : "OFF";
        if (pg == P_OPTIONS) { static char c[2]; put_number(c, frontend_credits()); values[3] = c; }   /* Home: CREDITS */
        values[4] = speed[s->nando]; values[5] = s->magic ? "ON" : "OFF"; values[6] = s->demo ? "ON" : "OFF";
        values[7] = coins[s->coinA]; values[8] = coins[s->coinB];
        for (n = first; n < last && n < nv; n++) text(25, 8 + (n - first) * 2, values[n], TRUE);
        if (pg == P_DIPS) text(6, 22, first ? "* ONE COIN TO CONTINUE" : "DOWN FOR COIN SETTINGS", FALSE);
    }
    if (save_failed) text(12, 22, "SAVE UNAVAILABLE", TRUE);
    text(9, pg == P_SELECT ? 22 : 24, "A/START SELECT   B BACK", FALSE);
    VDP_setTileMapDataRect(BG_A, scr, 0, 0, 40, 28, 40, DMA_QUEUE);   /* in the next VBlank */
}

/* One menu step on the newly pressed buttons; TRUE: start the game. */
static bool step(u16 edge)
{
    u16 parent = fe.mode == FE_ARCADE ? P_ARCADE : P_HOME, ok = edge & (BUTTON_A | BUTTON_START);
    u16 adjust = edge & (BUTTON_LEFT | BUTTON_RIGHT);
    s16 d = (edge & BUTTON_LEFT) ? -1 : 1;
    if (fe.mode == FE_HOME && pg == P_CONTROLS) parent = P_OPTIONS;   /* Home: CONTROLS is in OPTIONS */
    if (pg == P_HOME && edge)
    {
        /* The debug menu's secret combo on the Home page: Up, Up, Down,
         * Down, C. */
        static const u16 combo[5] = { BUTTON_UP, BUTTON_UP, BUTTON_DOWN, BUTTON_DOWN, BUTTON_C };
        static u8 got;
        got = (edge & combo[got]) ? got + 1 : (edge & BUTTON_UP) ? 1 : 0;
        if (got == 5) { got = 0; fe.debug = 1; page(P_DEBUG); return FALSE; }
    }
    if (edge & BUTTON_B)
    {
        if (pg == P_SELECT) return FALSE;
        if (pg == P_DEBUG) { fe.debug = 0; fe.sorsha = 0; page(P_HOME); return FALSE; }
        if (pg == P_MUSIC) { XGM2_stop(); page(P_OPTIONS); return FALSE; }
        if (pg == P_OPTIONS || pg == P_DIPS) frontend_save();
        page(pg == P_HOME || pg == P_ARCADE ? P_SELECT : parent);
        return FALSE;
    }
    if ((edge & (BUTTON_UP | BUTTON_DOWN)) && rows()) { row = cyc(row, 0, rows(), (edge & BUTTON_UP) ? -1 : 1); dirty = 1; return FALSE; }
    if (pg == P_SELECT && ok)
    {
        fe.debug = 0;
        fe.invincible = 0;
        fe.sorsha = 0;
        fe.quest = 0;
        fe.mode = row == 0 ? FE_ARCADE : FE_HOME; page(fe.mode == FE_ARCADE ? P_ARCADE : P_HOME);
        return FALSE;
    }
    if (pg == P_DEBUG)
    {
        /* Start points per stage index: stage 1 its start; 2-5 the
         * checkpoint records (src/game.c); stage 6 its areas. */
        static const u8 points[6] = { 1, 10, 14, 14, 19, 7 };
        if (row == 3) { if (ok) { fe.plus = 0; fe.quest = 0; return TRUE; } return FALSE; }
        if (!adjust && !ok) return FALSE;
        if (row == 0) { fe.debug_stage = cyc(fe.debug_stage, 0, 6, d); fe.debug_cp = 0; }
        else if (row == 1) fe.debug_cp = cyc(fe.debug_cp, 0, points[fe.debug_stage], d);
        else if (row == 4) fe.sorsha ^= 1;
        else fe.invincible ^= 1;
        dirty = 1;
        return FALSE;
    }
    if ((pg == P_HOME || pg == P_ARCADE) && ok)
    {
        u16 r = home_row(row);
        if (r == 0 || r == 5) { fe.plus = r == 5; fe.quest = 0; coop_pick = 0; return TRUE; }
        if (r == 6) { fe.plus = 0; fe.quest = 0; coop_pick = 1; return TRUE; }
        if (r == 7) { fe.plus = 0; fe.quest = 1; coop_pick = 0; return TRUE; }   /* Sorsha's Quest (src/quest.c) */   /* co-op: the pick screen, then the game */
        page(r == 1 ? (fe.mode == FE_ARCADE ? P_DIPS : P_OPTIONS) : r == 2 ? P_SCORES : r == 3 ? P_CONTROLS : P_SELECT);
        return FALSE;
    }
    if (pg == P_OPTIONS || pg == P_DIPS)
    {
        Settings *s = &rec.set[fe.mode];
        if (pg == P_OPTIONS && row == 5) { if (ok) page(P_MUSIC); return FALSE; }
        if (pg == P_OPTIONS && row == 6) { if (ok) page(P_CONTROLS); return FALSE; }
        if (!adjust && !ok) return FALSE;
        switch (row)
        {
        case 0: s->difficulty = cyc(s->difficulty, 0, 8, d); break;
        case 1: s->lives = cyc(s->lives, 1, 4, d); break;
        case 2: s->vitality = cyc(s->vitality, 2, 4, d); break;
        case 3: if (pg == P_OPTIONS) rec.credits = 1 + cyc(frontend_credits(), 0, 10, d); else s->cont ^= 1; break;
        case 4: s->nando = cyc(s->nando, 0, 4, d); break;
        case 5: s->magic ^= 1; break;
        case 6: s->demo ^= 1; break;
        case 7: s->coinA = cyc(s->coinA, 0, 8, d); break;
        case 8: s->coinB = cyc(s->coinB, 0, 8, d); break;
        }
        dirty = 1;
        return FALSE;
    }
    if (pg == P_MUSIC)
    {
        if (adjust) { track = cyc(track, 0, NTRACKS, d); dirty = 1; }
        if (ok)
        {
            if (row == 0) sound_post(tracks[track].cmd);
            else XGM2_stop();
        }
        return FALSE;
    }
    if ((pg == P_CONTROLS || pg == P_SCORES) && ok) page(parent);
    return FALSE;
}

/* The menu until a game starts (fe.mode, fe.plus). From power-on the
 * selector; after a game, the mode's own page. */
static void menu_setup(void)
{
    PAL_setColors(0, palette_black, 64, CPU);
    SYS_doVBlankProcess();                      /* the stage's pending column and tile writes land before the clear */
    stage_split(FALSE);
    VDP_setWindowVPos(FALSE, 0);
    VDP_setHorizontalScroll(BG_A, 0); VDP_setHorizontalScroll(BG_B, 0);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setVerticalScroll(BG_A, 0); VDP_setVerticalScroll(BG_B, 0);
    VDP_resetSprites(); VDP_updateSprites(1, DMA);
    VDP_loadTileData((const u32 *)font_tiles, FONT, 64, DMA);
    PAL_setColors(0, font_pal, 32, DMA);
    VDP_loadTileData((const u32 *)logo_tiles, LOGO, LOGO_TILES, DMA);
    PAL_setColors(32, logo_pal, 32, DMA);
    PAL_setColor(0, 0);
    VDP_setBackgroundColor(0);
    clear();                                    /* the stage's planes, while the colours are black */
    logo_shown = 0;
    scr = MEM_alloc((28 + 11) * 40 * 2);
}

void frontend_menu(bool first)
{
    u16 prev;
    menu_setup();
    page(first ? P_SELECT : fe.debug ? P_DEBUG : fe.mode == FE_ARCADE ? P_ARCADE : P_HOME);
    prev = JOY_readJoypad(JOY_1);                /* a button held from the game is not a press */
    for (;;)
    {
        u16 held, still = 0;
        if (dirty) { dirty = 0; draw(); VDP_setEnable(TRUE); }
        SYS_doVBlankProcess();
        held = JOY_readJoypad(JOY_1);
        if (held & ~prev) still = 0;
        else if (pg == P_HOME) still = idle_wait + 1;
        idle_wait = still;
        /* Home sitting idle 10 s runs the arcade attract loop's story
         * pictures with the Home ranking (no demo play); any button
         * returns to the Home page. */
        if (idle_wait >= IDLE_ATTRACT)
        {
            MEM_free(scr);                      /* the pictures' unpacking needs the heap */
            scr = NULL;
            attract_home();
            menu_setup();
            page(P_HOME);
            prev = JOY_readJoypad(JOY_1);       /* the returning press is not a menu press */
            continue;
        }
        if (step(held & ~prev)) break;
        prev = held;
    }
    XGM2_stop();
    MEM_free(scr);                              /* the last page went out in the loop's last VBlank */
    scr = NULL;
    clear();
    { extern u8 coop_partner; coop_partner = 1;   /* Madmartigan unless the pick screen chooses Sorsha */
#ifdef COOP_SORSHA
      coop_partner = 2;                         /* measurement builds: Willow + Sorsha */
#endif
    }
    if (coop_pick) coop_pick = coop_select_screen() + 1;   /* co-op: pad 1's hero */
    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);   /* the stage's mode (stage_init sets it again) */
    fe.start_held = 1;                          /* the launching press is not a pause */
}

