/* Sorsha's Quest (Home only; AGENTS.md "Sorsha as a Home character",
 * out/audit/quest/PLAN.md). Her film story on the arcade's stages:
 *   I    Nockmaar: stage 6's corridors (areas 1, 0) right to left
 *   II   the Crossroads fort: stage 2's fort right to left (no wagon)
 *   III  Fin Raziel's island: stage 3
 *   IV   the snow camp: stage 4 from the village up the slope to the camp
 *   V    Tir Asleen: stage 5
 *   VI   Nockmaar: stage 6 from area 2 to Bavmorda
 * Reversed parts start at a late restart record facing left; the camera's
 * catch-up limit runs the other way (src/player.c), restarts step back by
 * record (src/game.c), and the part ends at its left end. Arcade and the
 * normal Home game never run this code (fe.quest is 0). */
#include <genesis.h>
#include "quest.h"
#include "actor.h"
#include "player.h"
#include "game.h"
#include "hud.h"
#include "sound.h"
#include "stage.h"
#include "stagedata.h"
#include "frontend.h"
#include "packed.h"

typedef struct { u8 stage, area, cp, rev; s16 end_x; } Part;
static const Part parts[] = {
    { 5, 1, 4, 1, 176 },        /* I: area 1 (the spikes) from its far door */
    { 5, 0, 7, 1, 176 },        /*    area 0 to the castle gate */
    { 1, 0, 8, 1, 176 },        /* II: the fort from the tower to the forest edge */
    { 2, 0, 0, 0, 0 },          /* III */
    { 3, 0, 0, 1, 0 },          /* IV: the village up the slope to the camp, Madmartigan there (src/fam_stage4.c) */
    { 4, 0, 0, 0, 0 },          /* V */
    { 5, 2, 0, 0, 0 },          /* VI: from area 2 */
};
static const u8 first_part[6] = { 0, 2, 3, 4, 5, 6 };
#define NPARTS (sizeof parts / sizeof parts[0])

u8 quest_ch;
u8 qrev;
static u8 part, part_done;

static const Part *cur(void) { return parts + part; }

/* ---- Her world map (tools/questmap.py): map 1's tiles + map 6's extras +
 * her own; chapter 1's words, the other chapters' differences, the
 * blinking disc's cells. */
extern const u8 intro_map_tiles[], intro_map6_tiles[], quest_map_xtiles[], quest_fig_tiles[], quest_fig_spr[];
extern const u16 intro_map_pal[], quest_map_map[], quest_map_diff[], quest_map_alt[];
#include "../data/quest/quest.h"            /* QMAP_XTILES (tools/questmap.py) */
#define INTRO_MAP_TILES 623                 /* data/intro/intro_text.h */
#define MAP6_XTILES 150                     /* src/intro.c */
#define FIG_VRAM (16 + INTRO_MAP_TILES + MAP6_XTILES + QMAP_XTILES)

static void map_frames(u16 n, u16 *held)
{
    while (n--)
    {
        u16 j = JOY_readJoypad(JOY_1);
        SYS_doVBlankProcess();
        if (held && (j & (BUTTON_A | BUTTON_B)) && n > 4) n -= 3;
    }
}

static void her_map(void)
{
    const u16 *mm, *d = quest_map_diff, *a = quest_map_alt;
    static u16 w[40];
    static u16 fade[64];
    u16 r, c, i, k;
    stage_bfixed(TRUE);
    DMA_flushQueue();
    PAL_setColors(0, palette_black, 64, CPU);
    VDP_setWindowVPos(FALSE, 0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_setVerticalScroll(BG_A, 0); VDP_setVerticalScroll(BG_B, 0);
    load_packed(intro_map_tiles, 16, INTRO_MAP_TILES);
    load_packed(intro_map6_tiles, 16 + INTRO_MAP_TILES, MAP6_XTILES);
    load_packed(quest_map_xtiles, 16 + INTRO_MAP_TILES + MAP6_XTILES, QMAP_XTILES);
    for (k = 2; k <= quest_ch; k++) d += 1 + d[0] * 2;   /* chapter k's differences block */
    mm = map_open(quest_map_map);
    for (r = 0; r < 28; r++)
    {
        for (c = 0; c < 40; c++) w[c] = mm[r * 40 + c] + 16;
        if (quest_ch)
            for (i = 0; i < d[0]; i++) if (d[1 + i * 2] / 40 == r) w[d[1 + i * 2] % 40] = d[2 + i * 2] + 16;
        VDP_setTileMapDataRow(BG_B, w, r, 0, 40, CPU);
    }
    map_close();
    for (k = 0; k < quest_ch; k++) a += 1 + a[0] * 3;   /* this chapter's blinking cells */
    VDP_fillTileMapRect(BG_A, 0, 0, 0, 64, 32);
    {
        /* her figure beside the chapter's disc (tools/questfig.py) */
        static const s16 at[6][2] = { { 176, 46 }, { 48, 87 }, { 120, 134 }, { 224, 158 }, { 248, 71 }, { 176, 46 } };
        static u16 sat[4 * 8];
        const s16 *e = (const s16 *)(quest_fig_spr + 2);
        u16 n = *(const u16 *)quest_fig_spr, k;
        VDP_loadTileData((const u32 *)quest_fig_tiles, FIG_VRAM, QFIG_TILES, CPU);
        for (k = 0; k < n && k < 8; k++, e += 4)
        {
            sat[k * 4] = at[quest_ch][1] + e[0] + 128;
            sat[k * 4 + 1] = e[1] | (k + 1 < n ? k + 1 : 0);
            sat[k * 4 + 2] = e[2] + FIG_VRAM;
            sat[k * 4 + 3] = at[quest_ch][0] + e[3] + 128;
        }
        DMA_doDma(DMA_VRAM, sat, 0xAC00, n * 4, 2);
    }
    sound_post(SND_MUSIC_MAP);
    map_frames(3, NULL);
    memcpy(fade, intro_map_pal, sizeof fade);
    PAL_fadeInAll(fade, 48, FALSE);
    for (i = 0; i < 128; i++)
    {
        u16 j = JOY_readJoypad(JOY_1);
        if (i % 10 == 0)
            for (k = 0; k < a[0]; k++)
            {
                const u16 *e = a + 1 + k * 3;
                VDP_setTileMapXY(BG_B, (((i / 10) & 1) ? e[2] : e[1]) + 16, e[0] % 40, e[0] / 40);
            }
        SYS_doVBlankProcess();
        if ((j & (BUTTON_A | BUTTON_B)) && i + 4 < 128) i += 3;
    }
    sound_post(SND_STOP);
    PAL_fadeOutAll(48, FALSE);
    map_frames(7, NULL);
}

void quest_map(bool page)
{
    her_map();
    if (page) quest_page(1 + quest_ch);
}

/* ---- Entering a part. */
static void enter(void)
{
    stage_index = cur()->stage - 1;         /* stage_enter_next: game_next_stage advances it */
    qrev = 0;
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_resetSprites(); VDP_updateSprites(1, DMA);
    stage_enter_next();
}

void quest_start(void)
{
    quest_ch = 0;
    part = 0;
    part_done = 0;
    qrev = 0;
#ifdef QUEST_CH
    quest_ch = QUEST_CH;                    /* test builds: start at a chapter */
    part = first_part[QUEST_CH];
#endif
    quest_page(0);                          /* the prologue */
    PAL_setColors(0, palette_black, 64, CPU);
    game_new();
    enter();
}

/* game_next_stage: before the stage streams in. */
void quest_stage_setup(void)
{
    const Part *p = cur();
    extern u8 area6;
    extern void stage6_select(void), stage6_new_game(void), game_form_palette(void);
    qrev = p->rev;
    part_done = 0;
    if (p->stage == 5)
    {
        if (quest_ch == 5) stage6_new_game();   /* chapter VI: the doors, Kael and Bavmorda afresh */
        area6 = p->area;
        stage6_select();
        game_form_palette();
    }
    if (p->stage == 3)
    {
        extern u8 dragon_beaten, sorsha_beaten;
        dragon_beaten = sorsha_beaten = 1;  /* her walk up from the village (src/fam_stage4.c) */
    }
    { extern void families_rival(bool on); families_rival(p->stage == 3); }   /* chapter IV: Madmartigan for the camp's boss */
    checkpoint = p->cp;
#ifdef QUEST_CP
    checkpoint = QUEST_CP;                  /* test builds: a forward chapter's checkpoint */
#endif
#ifdef QUEST_AREA
    if (p->stage == 5) { area6 = QUEST_AREA; stage6_select(); game_form_palette(); }   /* test builds: stage 6's area */
#endif
}

/* After the start or restart position is set (stage_start, game_next_stage). */
#define S4_START_X 16100
#define S4_START_Y 73
void quest_place(void)
{
    const Part *p = cur();
    if (!qrev) return;
    if (p->stage == 3 && checkpoint == 0)
    {
        player_init(S4_START_X, S4_START_Y);
        player.cam_max_y = 7936;
        player.cam_min_y = 0;
        player.cam_max_x = 16000;
    }
    if (p->stage == 1)
    {
        /* The fort's pillar (x 736-767) is a wall down to the floor; the
         * arcade's way past it only goes down (over its top, then the
         * shelves). Reversed, she walks past its foot: its cells from
         * the floor up to y 143 are opened (collision only). */
        extern u8 npatch, patch_id[];
        extern s16 patch_c[], patch_r[], patch_c0, patch_c1, patch_r0, patch_r1;
        u16 c, r;
        npatch = 0;
        for (c = 46; c <= 47; c++)
            for (r = 23; r <= 27; r++) { patch_c[npatch] = c; patch_r[npatch] = r; patch_id[npatch] = 0; npatch++; }
        patch_c0 = 46; patch_c1 = 47; patch_r0 = 23; patch_r1 = 27;
        terrain_changed();
    }
    player.facing = 3;                      /* standing, facing left */
    player.cam_min_x = p->stage == 3 ? 6528 : 0;
    if (player.cam_x > player.cam_max_x) player.cam_x = player.cam_max_x;
    if (player.cam_x < player.cam_min_x) player.cam_x = player.cam_min_x;
}

/* Placed objects the quest leaves out: stage 2's wagon (no chase), stage
 * 6's doors in chapter I (her way out is to the left). */
bool quest_family_off(u16 f)
{
    if (!fe.quest) return FALSE;
    if (f == F_WAGON) return TRUE;
    if (f == F_DOOR6 && quest_ch == 0) return TRUE;
    return FALSE;
}

/* Once per play update. */
void quest_update(void)
{
    const Part *p = cur();
    if (!fe.quest || !qrev || part_done || player.dead) return;
    /* Enemies left far behind (right of the camera's reach) are retired,
     * as the forward game retires those left of its minimum. */
    if (!(game_frame & 15))
    {
        u16 i;
        for (i = pool_lo[C_ENEMY]; i < pool_hi[C_ENEMY]; i++)
        {
            Actor *a = actors + i;
            if (a->fam && AX(a) > player.cam_max_x + 640) actor_free(a);
        }
    }
    if ((player.x >> 8) >= p->end_x) return;
    part_done = 1;
    if (part + 1 < (quest_ch < 5 ? first_part[quest_ch + 1] : NPARTS))
    {
        /* the next area of the same chapter (stage 6's corridors) */
        extern void game_goto_area6(u16 area, u16 cp);
        part++;
        part_done = 0;
        game_goto_area6(cur()->area, cur()->cp);
        return;
    }
    game_stage_clear();
}

/* The clear's results are over: her map, the next chapter's page, play. */
void quest_chapter_clear(void)
{
    if (quest_ch >= 5) { game_mode = GM_ENDED; return; }
    quest_ch++;
    part = first_part[quest_ch];
    enter();
}

/* Bavmorda is beaten (chapter VI): her ending page, then the scores. */
void quest_end(void)
{
    sound_post(SND_STOP);
    PAL_fadeOutAll(30, FALSE);
    quest_page(7);
    game_mode = GM_ENDED;
}

const char *quest_title(u16 line)
{
    static const char *const t[6][2] = {
        { "CHAPTER1", "NOCKMAAR CASTLE" }, { "CHAPTER2", "CROSSROADS FORT" }, { "CHAPTER3", "FIN RAZIEL'S ISLAND" },
        { "CHAPTER4", "SORSHA'S CAMP" }, { "CHAPTER5", "TIR ASLEEN CASTLE" }, { "CHAPTER6", "NOCKMAAR CASTLE" },
    };
    return t[quest_ch][line];
}
