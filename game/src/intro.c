/* Game start and the stage-1 intro (specs/intro.md): the world map (scene
 * A), then the forest intro (scene B): the pan down from the sky, the
 * typed story, the High Aldwin's warning, the title, and the stage-start
 * gate. Timings are the arcade's (section 3), counted in updates.
 *
 * Presentation (approved cuts 1, 3, 4; tools/intropack.py): the map is one
 * plane picture with its markers baked in (their blinking is left out);
 * the sky the pan starts in is a band of rows above the stage map, drawn
 * without parallax, its tiles lent the backdrop's and the pose cache's
 * VRAM (no sprite is on screen up there) and its colours Willow's line and
 * the waterfall's cycle pens, all given back once the camera is below it;
 * the story text and the stage titles are sprites over the scene in the
 * top four rows (user, 2026-11-04: no grey band; was the window plane);
 * the dialogue box keeps the arcade's frame on plane B.
 * Holding A or B runs the scene 4 updates a frame (the arcade's
 * fast-forward runs on spare CPU, its real factor is unknown, 10.1); fades
 * keep their speed. */
#include <genesis.h>
#include "frontend.h"
#include "stagedata.h"
#include "actor.h"
#include "coop.h"
#include "player.h"
#include "hud.h"
#include "sound.h"
#include "stage.h"
#include "game.h"
#include "sprite.h"
#include "packed.h"
#include "screens.h"
#include "bank.h"
#include "quest.h"
#include "../data/stage1/layout.h"
#include "../data/intro/intro_text.h"

extern const u8 intro_map_tiles[], intro_sky_tiles[], intro_font[], intro_box_tiles[];
extern const u8 intro_font_spr[];   /* tools/fontspr.py: ground transparent, title letters in 2 x 2 sprite order */
extern const u16 intro_map_map[], intro_map_pal[], intro_sky_map[], intro_sky_pal[], intro_box_map[], intro_map_alt[];
extern const u8 intro_map2_tiles[];
extern const u16 intro_map2_map[], intro_map2_pal[], intro_map2_alt[];
extern const u8 intro_map3_tiles[], intro_map4_tiles[], intro_map5_tiles[], intro_map6_tiles[];
extern const u16 intro_map3_map[], intro_map3_pal[], intro_map3_alt[], intro_map4_map[], intro_map4_pal[], intro_map4_alt[], intro_map5_map[], intro_map5_pal[], intro_map5_alt[], intro_map6_map[], intro_map6_pal[], intro_map6_alt[];
/* The world map before a stage (4.2; stage2.md 1.2): picture, blinking cells. */
typedef struct { const u8 *tiles; const u16 *map, *pal, *alt; u16 ntiles, nalt; } WorldMap;
#define MAP2_XTILES 115
#define MAP3_XTILES 136
#define MAP4_XTILES 115
#define MAP5_XTILES 154
#define MAP6_XTILES 150
static const WorldMap maps[6] = {
    { intro_map_tiles, intro_map_map, intro_map_pal, intro_map_alt, INTRO_MAP_TILES, INTRO_MAP_ALT },
    { intro_map2_tiles, intro_map2_map, intro_map_pal, intro_map2_alt, MAP2_XTILES, INTRO_MAP2_ALT },
    { intro_map3_tiles, intro_map3_map, intro_map_pal, intro_map3_alt, MAP3_XTILES, INTRO_MAP3_ALT },
    { intro_map4_tiles, intro_map4_map, intro_map_pal, intro_map4_alt, MAP4_XTILES, INTRO_MAP4_ALT },
    { intro_map5_tiles, intro_map5_map, intro_map_pal, intro_map5_alt, MAP5_XTILES, INTRO_MAP5_ALT },
    { intro_map6_tiles, intro_map6_map, intro_map_pal, intro_map6_alt, MAP6_XTILES, INTRO_MAP6_ALT },
};

#define FONT_BASE 1664      /* the HUD's tiles (reloaded at play start) */
#define FONT_S1_TILES (INTRO_FONT_TILES < 128 ? INTRO_FONT_TILES : 128)   /* stage 1's intro: below 1792 (plane A) */
#define BAND_ROWS 4         /* screen rows 0-3: the text's rows */
#define SND_STOP 0xF0
#define SND_TYPE 0x2F
#define FF_RATE 4           /* scene updates a frame while A or B is held */

static u16 sky_vram[INTRO_SKY_TILES];
static u16 fade[64];
static u8 sky_on, backdrop_step;
static u8 fade_step;        /* forest fade-in: 16 levels, 2 updates each (job 0x1002); 0 done */

/* CRAM at level k/16 of the target (channels scaled, rounded down). */
static void fade_level(u16 k)
{
    static u16 c[64];
    u16 i;
    for (i = 0; i < 64; i++)
    {
        u16 w = fade[i];
        c[i] = ((((w >> 1) & 7) * k / 16) << 1) | ((((w >> 5) & 7) * k / 16) << 5) | ((((w >> 9) & 7) * k / 16) << 9);
    }
    PAL_setColors(0, c, 64, DMA_QUEUE);
}
static u16 step, timer, typed, pan_end;
static Actor *aldwin;

/* ---- Text over the scene (user, 2026-11-04): sprites, no band.
 * Story rows 1-2 (shown on screen rows 11-12, the arcade's scroll1 rows
 * 13-14, intro.md 6.3; user 2026-11-06 "match both"): each column a tile
 * slot, FONT_BASE + INTRO_TITLE0 + (row - 1) * 40 + column (the title
 * glyphs' VRAM: no title shows meanwhile; the title loads its glyphs after
 * the box), drawn as sprites over the runs of written characters, at most
 * four columns each (spaces cost no sprite pixels). Titles: each big
 * letter a 2 x 2 sprite (letter_tile). */
#define STORY_SLOT0 (FONT_BASE + INTRO_TITLE0)
#define STORY_ROW0 10       /* story row r on screen row STORY_ROW0 + r */
static u8 text_on;
static u8 story_col[2][40];         /* story rows 1-2: columns with a glyph */
typedef struct { u8 r, c; u16 tile; } BigLetter;
static u16 title_pal = PAL1;        /* the title letters' line (title_glyphs) */
static u16 letter_tile[64];         /* the title letters' first VRAM tile, by ASCII - 32 (0: none) */
static BigLetter big[40];
static u8 nbig;
static u8 title_hud;                /* a later stage's title: the HUD shows, the glyphs in borrowed pose-cache VRAM */

static void band_show(bool on)
{
    text_on = on;
}

static void band_clear(void)
{
    memset(story_col, 0, sizeof story_col);
    nbig = 0;
}

static u8 in_box;           /* the message types into the dialogue box (plane B), not the band */

static void band_put(u16 r, u16 c, const u8 *table, char ch)
{
    u16 g = (ch >= 32 && ch < 96) ? table[ch - 32] : 0;
    u16 slot;
    if (in_box)
    {
        static u16 w;
        w = TILE_ATTR_FULL(PAL1, 1, 0, 0, FONT_BASE + g);
        stage_bput(c, r, &w, 1);
        return;
    }
    if (r < 1 || r > 2 || c >= 40 || !g) return;
    slot = STORY_SLOT0 + (r - 1) * 40 + c;
    story_col[r - 1][c] = 1;
    DMA_queueDma(DMA_VRAM, (void *)(intro_font_spr + g * 32), slot * 32, 16, 2);
}

/* The text's sprites, first in the table (over every other sprite);
 * returns the sprites written. */
u16 intro_text_draw(VDPSprite *sat, u16 first)
{
    u16 k = first, r, i;
    VDPSprite *s = sat + first;
    if (!text_on || game_mode != GM_INTRO) return 0;
    for (r = 0; r < 2; r++)
        for (i = 0; i < 40;)
        {
            u16 w = 0;
            if (!story_col[r][i]) { i++; continue; }
            while (w < 4 && i + w < 40 && story_col[r][i + w]) w++;
            s->y = 128 + (STORY_ROW0 + 1 + r) * 8;
            s->size_link = ((w - 1) << 10) | ++k;           /* w x 1 */
            s->attribut = TILE_ATTR_FULL(PAL1, 1, 0, 0, STORY_SLOT0 + r * 40 + i);
            s->x = 128 + i * 8;
            s++;
            i += w;
        }
    for (i = 0; i < nbig; i++)
    {
        s->y = 128 + big[i].r * 8;
        s->size_link = (5 << 8) | ++k;                      /* 2 x 2 */
        s->attribut = TILE_ATTR_FULL(title_pal, 1, 0, 0, big[i].tile);
        s->x = 128 + big[i].c * 8;
        s++;
    }
    return k - first;
}

/* ---- The dialogue box (6.3): the arcade's frame on plane B rows 2-9 with
 * the priority bit (over the scenery, as the HUD), its tiles over the title
 * glyphs; it opens from the centre over 17 updates. */
#define BOX_ROW 2
#define BOX_ROWS 8

static void box_columns(u16 c0, u16 c1)    /* columns c0..c1-1 shown */
{
    static u16 w[BOX_ROWS][40];
    u16 r, c;
    for (r = 0; r < BOX_ROWS; r++)
    {
        for (c = c0; c < c1; c++)
        {
            u16 m = intro_box_map[r * 40 + c];
            w[r][c] = m == 0xFFFF ? stage_bword(c, BOX_ROW + r)
                                  : (m & 0x1800) | TILE_ATTR_FULL(PAL1, 1, 0, 0, FONT_BASE + INTRO_TITLE0 + (m & 0x7FF));
        }
        stage_bput(c0, BOX_ROW + r, w[r] + c0, c1 - c0);
    }
}

static u8 title_due;        /* stage 1: the title glyphs to load (over the box frame's tiles) */

static void box_close(void)
{
    u16 r;
    for (r = 0; r < BOX_ROWS; r++)
    {
        static u16 w[40];
        u16 c;
        for (c = 0; c < 40; c++) w[c] = stage_bword(c, BOX_ROW + r);
        stage_bput(0, BOX_ROW + r, w, 40);
    }
    /* the title glyphs go back where the frame was at the title's first
     * update (title_glyphs: the frame's words are gone by then) */
    title_due = 1;
}

/* The title letters' VRAM: stage 1 the 80 tiles from FONT_BASE +
 * INTRO_TITLE0 (below plane A at 1792), loaded after the box; a later
 * stage tiles borrowed from the pose cache (the HUD shows meanwhile), only
 * the letters its title uses (title_tiles). */

/* The title glyphs as sprite art into VRAM from dst (CPU writes, the
 * interrupts held: the display is off, or the glyphs are not shown yet).
 * As the arcade (user, 2026-11-06: "white with slight black shadow"):
 * tools/fontspr.py stores the arcade big font's pens 0-6 (palette 0x0E:
 * near-black shadow 111, greys 666, white 777 x2, 555, 444, 333) as pens
 * 1-7; each takes the nearest colour of the CRAM line that fits them best,
 * weighted by use (sprite lines 0-1: pens 1-15; scenery lines 2-3: their
 * static pens 1-9). */
static const char *const title[6][2] = { { "STAGE1", "CROSSROAD" }, { "STAGE2", "CHERLINDREA'S FOREST" },
                                          { "STAGE3", "FIN RAZIEL'S ISLAND" }, { "STAGE4", "SORSHA'S CAMP" },
                                          { "STAGE5", "TIR ASLEEN CASTLE" }, { "STAGE6", "NOCKMAAR CASTLE" } };

static const char *ttl(u16 k) { return fe.quest ? quest_title(k) : title[stage_index][k]; }   /* Sorsha's Quest: her chapters' titles */

/* Tiles the stage's title letters need (4 per distinct letter). */
static u16 title_tiles(void)
{
    u8 seen[64];
    u16 n = 0, k;
    const char *t;
    memset(seen, 0, sizeof seen);
    for (k = 0; k < 2; k++)
        for (t = ttl(k); *t; t++)
        {
            u16 c = (u8)*t - 32;
            if (c < 64 && font_title[c] && !seen[c]) { seen[c] = 1; n += 4; }
        }
    return n;
}

static void title_glyphs(u16 dst)
{
    static const u16 want[7] = { 0x0222, 0x0CCC, 0x0EEE, 0x0EEE, 0x0AAA, 0x0888, 0x0666 };   /* 111 666 777 777 555 444 333 */
    static const u8 wt[7] = { 8, 5, 2, 4, 1, 1, 1 };
    const u16 *spr = stage_sprpal();
    u8 map[16], best_map[7] = { 1, 2, 3, 4, 5, 6, 7 }, l, k, i;
    u32 best = 0xFFFFFFFF;
    u16 g, *t, ints;
    static u16 buf[16];
    for (l = 0; l < 4; l++)
    {
        const u16 *pal = l < 2 ? spr + l * 16 : sd->pal + (l - 2) * 16;
        u8 m[7], top = l < 2 ? 16 : 10;
        u32 e = 0;
        for (k = 0; k < 7; k++)
        {
            u16 bd = 0xFFFF;
            for (i = 1; i < top; i++)
            {
                u16 w = pal[i], d = 0, ch;
                for (ch = 1; ch < 12; ch += 4)
                {
                    s16 x = (s16)((w >> ch) & 7) - (s16)((want[k] >> ch) & 7);
                    d += x * x;
                }
                if (d < bd) { bd = d; m[k] = i; }
            }
            e += (u32)bd * wt[k];
        }
        if (e < best) { best = e; title_pal = l; for (k = 0; k < 7; k++) best_map[k] = m[k]; }
    }
    for (i = 0; i < 16; i++) map[i] = 0;
    for (k = 0; k < 7; k++) map[k + 1] = best_map[k];
    memset(letter_tile, 0, sizeof letter_tile);
    ints = SYS_getAndSetInterruptMaskLevel(7);
    for (l = 0; l < 2; l++)
    {
        const char *c;
        for (c = ttl(l); *c; c++)
        {
            u16 a0 = (u8)*c - 32, q;
            if (a0 >= 64 || !font_title[a0] || letter_tile[a0]) continue;
            letter_tile[a0] = dst;
            for (q = 0, g = font_title[a0]; q < 4; q++, g++, dst++)
            {
                const u8 *src = intro_font_spr + g * 32;
                t = buf;
                for (i = 0; i < 16; i++, src += 2)
                {
                    u8 a = src[0], b = src[1];
                    *t++ = (map[a >> 4] << 12) | (map[a & 15] << 8) | (map[b >> 4] << 4) | map[b & 15];
                }
                VDP_loadTileData((const u32 *)buf, dst, 1, CPU);
            }
        }
    }
    SYS_setInterruptMaskLevel(ints);
}

static void band_big(u16 r, u16 c, char ch)
{
    u16 v = (ch >= 32 && ch < 96) ? letter_tile[ch - 32] : 0;
    if (!v || nbig >= sizeof big / sizeof big[0]) return;
    big[nbig].r = r; big[nbig].c = c; big[nbig].tile = v;   /* the line: title_pal */
    nbig++;
}

/* Messages (6.3): text, band row, column, glyph table; '&' = next row. */
typedef struct { const char *text; u8 row, col; const u8 *font; } Message;
static const Message msgs[4] = {
    { "ONE YOUNG MAN STARTS ON HIS JOURNEY", 1, 1, font_story },
    { "TO SAVE A BABY SAINT FROM BAVMORDA'S&WICKED HANDS.", 1, 3, font_story },
    { "HIS NAME IS WILLOW.", 1, 8, font_story },
    { "WILLOW, BE CAREFUL. BAVMORDA'S FINGERS&PENETRATE OUTSIDE OF THE VILLA.", 4, 0, font_box },   /* in the box */
};
static u8 msg_row, msg_col;

/* One character every 3 updates with the tick (spaces too); TRUE when done. */
static bool type_step(const Message *m)
{
    char ch;
    if (timer) { timer--; return FALSE; }
    ch = m->text[typed];
    if (!ch) return TRUE;
    typed++;
    timer = 2;
    sound_post(SND_TYPE);
    if (ch == '&') { msg_row++; msg_col = m->col; return FALSE; }
    band_put(msg_row, msg_col++, m->font, ch);
    return FALSE;
}

static void type_start(const Message *m)
{
    typed = 0;
    timer = 0;
    msg_row = m->row;
    msg_col = m->col;
}

/* ---- Scenery actors (6.4): the windmill and the High Aldwin. */
u8 aldwin_turn;

void scenery_update(Actor *a)
{
    if (!a->state)
    {
        a->state = 2;
        a->active = 2;
        actor_anim(a, 0);
    }
    if (a->fam == F_ALDWIN && aldwin_turn == 1) { aldwin_turn = 2; actor_anim(a, 1); }
    actor_step(a);
    if (!((game_frame + (a - actors)) & 15) && AX(a) < CAM_X - 96) actor_free(a);
}

static void scenery(u16 fam, s16 x, s16 y)
{
    Actor *a = actor_new(fam);
    if (!a) return;
    a->x = (s32)x << 8;
    a->y = (s32)y << 8;
    if (fam == F_ALDWIN) aldwin = a;
}

/* ---- Scene A: the world map (4.1-4.3). Blocking: nothing else runs. */
static void wait_frames(u16 n)
{
    while (n--)
    {
        u16 j = JOY_readJoypad(JOY_1);
        SYS_doVBlankProcess();
        if ((j & (BUTTON_A | BUTTON_B)) && n > FF_RATE) n -= FF_RATE - 1;
    }
}

static u16 altw[40];
void intro_world_map(u16 stage)
{
    const WorldMap *m = maps + stage;
    const u16 *mm;
    u16 i;
    /* Plane B a still picture first: this also stops the H interrupt's
     * scroll splits, which must not write the VDP during the CPU palette
     * write or the DMA fill below (after a life loss they are running; only
     * the status port may be touched during a fill, no emulator shows it). */
    stage_bfixed(TRUE);
    DMA_flushQueue();                       /* what the last frame queued lands first */
    PAL_setColors(0, palette_black, 64, CPU);
    VDP_setWindowVPos(FALSE, 0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_setVerticalScroll(BG_A, 0); VDP_setVerticalScroll(BG_B, 0);
    load_packed(intro_map_tiles, 16, INTRO_MAP_TILES);   /* the shared map (map 1) */
    if (m->tiles != intro_map_tiles) load_packed(m->tiles, 16 + INTRO_MAP_TILES, m->ntiles);   /* this screen's own tiles (tools/mapshare.py) */
    mm = map_open(m->map);
    {
        static u16 w[40];
        u16 r, c;
        for (r = 0; r < 28; r++)
        {
            for (c = 0; c < 40; c++) w[c] = mm[r * 40 + c] + 16;
            VDP_setTileMapDataRow(BG_B, w, r, 0, 40, CPU);
        }
    }
    for (i = 0; i < m->nalt; i++) altw[i] = mm[m->alt[i * 2]];   /* the blinking cells' own words */
    map_close();
    VDP_fillTileMapRect(BG_A, 0, 0, 0, 64, 32);
    {
        VDPSprite s = { 0 };
        DMA_doDma(DMA_VRAM, &s, 0xAC00, 4, 2);              /* no sprites */
    }
    sound_post(SND_MUSIC_MAP);
    wait_frames(3);
    for (i = 0; i < 64; i++) fade[i] = m->pal[i];
    PAL_fadeInAll(fade, 48, FALSE);
    /* The hold: stage marker I blinks between palettes 0x12 and 0x13 every
     * 10 updates (4.2), its cells swapped with their alternates. */
    for (i = 0; i < 128; i++)
    {
        u16 j = JOY_readJoypad(JOY_1), k;
        if (i % 10 == 0)
            for (k = 0; k < m->nalt; k++)
            {
                u16 cell = m->alt[k * 2];
                u16 w = ((i / 10) & 1) ? m->alt[k * 2 + 1] : altw[k];
                VDP_setTileMapXY(BG_B, w + 16, cell % 40, cell / 40);
            }
        SYS_doVBlankProcess();
        if ((j & (BUTTON_A | BUTTON_B)) && i + FF_RATE < 128) i += FF_RATE - 1;
    }
    sound_post(SND_STOP);
    PAL_fadeOutAll(48, FALSE);
    wait_frames(7);
}

/* ---- Scene B set-up (5): the stage at the top of the pan, the sky band in
 * the backdrop's and the pose cache's VRAM. */
static void forest_setup(void)
{
    u16 lo[8], hi[8], n, r, k = 0, i, real;
    /* Sky tiles above the map: the pose cache's ranges (handed back once
     * the camera is below the map's top), then the backdrop's slots past
     * the ones rows 0-15 use (the far layer is not shown and its tiles are
     * reloaded below camera 144, after the band has left the view; 05o:
     * the cache now has 271 slots for the band's 360); rows 0-15: the
     * backdrop's first slots. */
    posecache_flush();
    for (i = 0; i < 12; i++) sky_vram[k++] = HUD_TOP_LOW + i;   /* as the cache had them: the HUD's top rows are not up yet (hud_init reloads them) */
    for (i = 0; i < 4; i++) sky_vram[k++] = HUD_TOP_LOW2 + i;
    n = posecache_ranges(lo, hi);
    for (r = 0; r < n && k < INTRO_SKY_TOP_TILES; r++)
        for (i = lo[r]; i < hi[r] && k < INTRO_SKY_TOP_TILES; i++) sky_vram[k++] = i;
    for (i = STAGE_BD_BASE + INTRO_SKY_TILES - INTRO_SKY_TOP_TILES; i < STAGE_BD_BASE + STAGE_BD_TILES && k < INTRO_SKY_TOP_TILES; i++)
        sky_vram[k++] = i;
    real = k;
    /* Not reached; a shortfall would show a wrong cloud tile, never
     * overwrite tile 0. */
    for (i = k; k < INTRO_SKY_TOP_TILES; k++) sky_vram[k] = i ? sky_vram[k - i] : STAGE_BD_BASE;
    for (i = 0; k < INTRO_SKY_TILES; i++) sky_vram[k++] = STAGE_BD_BASE + i;
    player.cam_x = 0;
    player.cam_y = 512;
    player_stand(TRUE);                     /* facing the Aldwin (0xD15A) */
    memset(stage_hudrow, 0, sizeof stage_hudrow);    /* no HUD until play (hud_init) */
    stage_far_lock(TRUE, TILE_ATTR_FULL(PAL1, 1, 0, 0, FONT_BASE));   /* still until play (the box rows are written in place); a blank band at the foot */
    stage_sky(intro_sky_map, sky_vram);
    stage_init(player.cam_x + 32, 272 - player.cam_y);
    PAL_setColors(0, palette_black, 64, CPU);   /* black until the fade-in */
    /* Sky tiles over the backdrop's (plane B stays blank: the sky colour). */
    for (i = 0; i < INTRO_SKY_TILES;)
    {
        u16 j = i + 1;
        if (i >= real && i < INTRO_SKY_TOP_TILES) { i++; continue; }   /* a shared fallback slot */
        while (j < INTRO_SKY_TILES && j != real && sky_vram[j] == sky_vram[j - 1] + 1) j++;
        VDP_loadTileData((const u32 *)(intro_sky_tiles + (u32)i * 32), sky_vram[i], j - i, DMA);
        i = j;
    }
    VDP_fillTileMapRect(BG_B, 0, 0, 0, 64, 28);    /* all 64 columns: plane B scrolls */
    VDP_setBackgroundColor(sd->sky);
    VDP_loadTileData((const u32 *)intro_font, FONT_BASE, INTRO_TITLE0, DMA);      /* the box's glyphs (plane B) */
    title_hud = 0;                          /* the title glyphs: after the box (title_due) */
    title_due = 0;
    band_clear();
    sky_on = 2;
    backdrop_step = 0;
    /* Colours: the sky's in line 0 and the cycle pens. */
    for (i = 0; i < 16; i++) fade[i] = intro_sky_pal[i];
    for (i = 0; i < 16; i++) fade[16 + i] = sd->sprpal[16 + i];
    for (i = 0; i < 32; i++) fade[32 + i] = sd->pal[i];
    for (i = 0; i < 6; i++) { fade[32 + 10 + i] = intro_sky_pal[16 + i]; fade[48 + 10 + i] = intro_sky_pal[22 + i]; }
}

/* ---- Script (section 3, 6.3), one call per scene update. */
enum {
    S_FADE_IN, S_WAIT63, S_MSG1, S_GAP1, S_MSG2, S_GAP2, S_MSG3, S_GAP3, S_PANWAIT,
    S_BOX_OPEN, S_MSG4, S_GAP4, S_STAGE1, S_CROSSROAD, S_GAP5, S_DONE, S_SCENE4
};

static void pan(void)
{
    if (player.cam_y > 0)
    {
        player.cam_y -= 2;
        if (!player.cam_y) pan_end = 1;
    }
    else if (pan_end && ++pan_end == 223) aldwin_turn = 1;   /* 222 updates after the pan */
}

static void scene_update(void)
{
    if (step >= S_MSG1 && stage_index == 0) pan();       /* stage 1's intro only (a later stage's title keeps its camera) */
    switch (step)
    {
    case S_WAIT63:
        if (--timer) return;
        scenery(F_WINDMILL, 218, 136);
        scenery(F_ALDWIN, 160, 80);
        band_show(TRUE);
        type_start(msgs + 0);
        step = S_MSG1;
        return;
    case S_MSG1: if (type_step(msgs + 0)) { step = S_GAP1; timer = 29; } return;
    case S_GAP1: if (--timer) return; band_clear(); type_start(msgs + 1); step = S_MSG2; return;
    case S_MSG2: if (type_step(msgs + 1)) { step = S_GAP2; timer = 30; } return;
    case S_GAP2: if (--timer) return; band_clear(); type_start(msgs + 2); step = S_MSG3; return;
    case S_MSG3: if (type_step(msgs + 2)) { step = S_GAP3; timer = 24; } return;
    case S_GAP3: if (--timer) return; step = S_PANWAIT; return;
    case S_PANWAIT:
        if (player.cam_y) return;
        band_clear();
        band_show(FALSE);
        DMA_queueDma(DMA_VRAM, (void *)intro_box_tiles, (FONT_BASE + INTRO_TITLE0) * 32, INTRO_BOX_TILES * 16, 2);
        step = S_BOX_OPEN;
        timer = 17;
        return;
    case S_BOX_OPEN:                        /* centre columns 20/27 outwards (cropped: 16..23) */
        {
            u16 half = (17 - timer + 1) * 20 / 17;
            box_columns(20 - half, 20 + half);
        }
        if (--timer) return;
        in_box = 1;
        type_start(msgs + 3);
        step = S_MSG4;
        return;
    case S_MSG4: if (type_step(msgs + 3)) { step = S_GAP4; timer = 120; } return;
    case S_GAP4:
        if (--timer) return;
        in_box = 0;
        box_close();
        band_clear();
        band_show(TRUE);
        step = S_STAGE1;
        typed = 0;
        timer = 0;
        return;
    case S_STAGE1:                          /* big letters, 4 updates each */
    case S_CROSSROAD:
        {
            static const u8 col[6][2] = { { 14, 10 }, { 14, 0 }, { 14, 1 }, { 14, 7 }, { 14, 3 }, { 14, 5 } };   /* stage3.md 1.3, stage4.md 1.4, stage5.md 1.4, stage6.md 1.4 (48-column grid less 4) */
            const char *t = ttl(step - S_STAGE1);
            if (title_due) { title_due = 0; title_glyphs(FONT_BASE + INTRO_TITLE0); }   /* stage 1: below 1792 (plane A), 80 tiles */
            if (timer) { timer--; return; }
            if (!t[typed]) { step++; typed = 0; if (step == S_GAP5) timer = stage_index ? 5 : 30; return; }   /* stage2.md 1.3: 5 updates, stage 1 30 (intro.md 6.3) */
            sound_post(SND_TYPE);
            band_big(step == S_STAGE1 ? 11 : 14, (fe.quest ? 20 - strlen(t) : col[stage_index][step - S_STAGE1]) + typed * 2, t[typed]);   /* the arcade's rows 13-14 and 16-17 (screen y 88, 112) */
            typed++;
            timer = 3;
        }
        return;
    case S_GAP5:
        if (--timer) return;
        step = S_DONE;
        return;
    case S_SCENE4:                          /* stage 4's intro (stage4.md 1.3): the horse group, the walk-in */
        actors_update();
        if (timer == 256 - 125) player_script_x = 176;
        if (timer <= 256 - 22 && timer > 256 - 22 - 192 && !((256 - 22 - timer) & 31)) sound_post(0x39);   /* hoofbeats, 6 */
        if (--timer) return;
        step = S_STAGE1;
        typed = 0;
        timer = 9;
        return;
    }
}

/* ---- Entry points. */
/* The next stage after a clear (stage2.md 1.1-1.3): its world map, the
 * stage, then its title typed on the band (cut 3) before the HUD. */
void stage_enter_next(void)
{
    u16 i;
    if (fe.quest) quest_map(TRUE);          /* Sorsha's Quest: her map and the chapter's page */
    else intro_world_map(stage_index + 1);
    if (stage_index + 1 == 4) { extern u8 stage5_form; stage5_form = fe.quest ? 1 : two_players ? player.form : select_screen(); }   /* the quest: Sorsha (Madmartigan's numbers) */   /* stage5.md 1.3; co-op: no choice (both heroes play) */
    VDP_setBackgroundColor(32);             /* a blanked display shows the backdrop entry: scenery line 2 entry 0, black in every stage */
    VDP_setEnable(FALSE);                   /* the stage loads unseen (stage_init sets its colours at once) */
    memset(stage_hudrow, 0, sizeof stage_hudrow);    /* no HUD until play (hud_init): the old rows name tiles the new stage overwrites */
    game_next_stage();
    game_mode = GM_INTRO;
    hud.clock_lock = 1;
    aldwin = NULL;
    sky_on = 0;
    in_box = 0;
    PAL_setColors(0, palette_black, 64, CPU);
    /* The HUD from the start, as the arcade (play sub 0 builds it before the
     * title, stage2.md 1.1); the title's glyphs then go in VRAM borrowed
     * from the pose cache until play (posecache_give). Without room: the
     * old way, glyphs in the HUD's VRAM and the HUD at play start. */
    {
        u16 v = posecache_take(title_tiles());
        title_hud = v != 0;
        if (title_hud)
        {
            hud_init(FONT_BASE);
            hud_dirty = 0xFF;
            title_glyphs(v);
        }
        else title_glyphs(FONT_BASE + 1);
    }
    band_clear();
    band_show(TRUE);
    game_stage_music();
    for (i = 0; i < 32; i++) fade[i] = stage_sprpal()[i];
    for (i = 0; i < 32; i++) fade[32 + i] = sd->pal[i];
    SYS_doVBlankProcess();
    if (stage_index == 3 && !fe.quest)
    {
        /* Stage 4: the horse with Willow and a soldier gallops by, Madmartigan runs in after it. */
        u16 k;
        for (k = 0; k < 3; k++)
        {
            Actor *a = actor_new(F_INTROHORSE);
            if (a) { a->variant = k; a->x = (s32)-96 << 8; a->y = (s32)(k == 2 ? 4220 : 4200) << 8; }
        }
        player.x = (s32)-32 << 8;
        player_script(22);
        player_script_x = -32;
        step = S_SCENE4;
        timer = 256;
    }
    else
    {
        step = S_STAGE1;
        typed = 0;
        timer = 9;
    }
    /* The planes as the first frame of play lays them (the far layer's rows
     * included: its ring moves one row a frame), landed before the colours
     * fade in, with the scroll values that go with them. */
    for (i = 0; i < 16; i++)
    {
        stage_scroll(player.cam_x + 32, 272 - player.cam_y);
        stage_commit();                     /* its scroll values for the HUD split */
        SYS_doVBlankProcess();
    }
    { extern void stage_split(bool on); stage_split(TRUE); }   /* the HUD splits from the first frame (main.c keeps them): the empty HUD rows show the far layer over the top 16 lines */
    DMA_flushQueue();
    PAL_setColors(0, palette_black, 64, CPU);   /* queued colour writes landed: black again */
    VDP_setBackgroundColor(sd->sky);
    VDP_setEnable(TRUE);
    PAL_fadeInAll(fade, 30, FALSE);
}

/* A game from the menu: the intro, or the DEBUG page's stage. Stages 2-6:
 * the stage data advanced to the one before, then the usual hand-over
 * (world map, stage 5's select, title); game_next_stage applies the start
 * point. */
void game_begin(void)
{
    if (fe.quest) { quest_start(); return; }   /* src/quest.c */
    if (!fe.debug || !fe.debug_stage) { intro_start(); return; }
    PAL_setColors(0, palette_black, 64, CPU);   /* the stages streamed on the way stay dark */
    game_new();
    while (stage_index + 1 < fe.debug_stage) game_next_stage();
    SYS_doVBlankProcess();                      /* the streamed columns land, then the planes are cleared: */
    VDP_clearPlane(BG_A, TRUE);                 /* the map's band shows what is on them (after a clear, the */
    VDP_clearPlane(BG_B, TRUE);                 /* story screen has emptied them) */
    VDP_resetSprites(); VDP_updateSprites(1, DMA);
    stage_enter_next();
}

void intro_start(void)
{
    game_new();
    game_mode = GM_INTRO;
    hud.clock_lock = 1;
    aldwin = NULL;
    aldwin_turn = 0;
    pan_end = 0;
    in_box = 0;
    intro_world_map(0);
    VDP_setBackgroundColor(32);             /* the forest loads unseen: a blanked display shows black */
    VDP_setEnable(FALSE);
    forest_setup();
    DMA_flushQueue();
    PAL_setColors(0, palette_black, 64, CPU);
    VDP_setEnable(TRUE);
    sound_post(SND_MUSIC_STAGE1);           /* t964: stage music in the dark */
    wait_frames(7);
    step = S_WAIT63;
    timer = 63;
    fade_step = 1;                          /* fade-in, not waited for (6.2) */
}

/* A later stage's title shows the HUD (main.c draws its sprites too). */
bool intro_hud(void)
{
    return game_mode == GM_INTRO && title_hud;
}

/* Hide Willow while the sky shows (her colours are lent to it). */
bool intro_hides_player(void)
{
    return game_mode == GM_INTRO && sky_on;
}


void intro_update(u16 held)
{
    u16 n = (held & (BUTTON_A | BUTTON_B)) && step != S_DONE ? FF_RATE : 1;
    if (fade_step)
    {
        if (!(fade_step & 1)) fade_level(fade_step >> 1);
        if (++fade_step > 32) { fade_level(16); fade_step = 0; }
    }
    while (n-- && step != S_DONE) scene_update();
    actors_update();
    /* The camera below the rows above the map (view top at map row 0): the
     * pose cache's VRAM back (the windmill comes into view at 217). Below
     * map row 16 (camera 144): the backdrop, Willow's line and the cycle
     * pens back, before she comes into view (about 105). */
    if (sky_on == 2 && player.cam_y <= 272) { posecache_flush(); sky_on = 1; }
    if (sky_on == 1 && player.cam_y <= 144 && !fade_step)
    {
        if (backdrop_step == 0)
        {
            stage_sky(NULL, NULL);
            PAL_setColors(0, stage_sprpal(), 16, DMA_QUEUE);
            PAL_setColors(32 + 10, sd->pal + 10, 6, DMA_QUEUE);
            PAL_setColors(48 + 10, sd->pal + 26, 6, DMA_QUEUE);
        }
        stage_backdrop(backdrop_step);
        if (++backdrop_step == STAGE_BACKDROP_STEPS) sky_on = 0;
    }
    if (step == S_DONE && !sky_on)
    {
        /* Play (section 7): text cleared, the HUD, the stage-start gate.
         * The text's sprites leave one frame before the HUD's tiles take
         * their VRAM (the last sprite table would show HUD glyphs). */
        if (text_on) { band_show(FALSE); return; }
        if (title_hud) { title_hud = 0; posecache_give(); }   /* the HUD is up already */
        else { hud_init(FONT_BASE); hud_dirty = 0xFF; }
        hud.clock_lock = 0;
        stage_far_lock(FALSE, 0);
        game_play_start();
    }
}
