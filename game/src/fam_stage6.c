/* Stage 6, Nockmaar Castle (specs/stage6.md): the areas, their restart
 * points and camera windows, the doors between them, and which families
 * play here.
 *
 * Every area is its own scenery set (src/stagedata.c stage6_data[area]) in
 * its own native coordinates: native x = arcade x - XOFF, native y = arcade
 * y - YOFF (tools/pack_stage6_areas.py); the tables below are in those
 * coordinates. The data lives in the SSF2 banks above 4 MB (src/bank.c):
 * nothing of stages 2-5's data (.stagedata) may be read here. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "hud.h"
#include "sound.h"
#include "stagedata.h"
#include "bank.h"
#include "../data/stage6/a6/far6.h"

u8 area6;                   /* FF8374: the area being played */
const StageObject *stage6_objects_of(u16 a);   /* src/game.c (the area tables) */
u16 stage6_count_of(u16 a);
u16 doors6_used;            /* FF8384: false doors already taken (bit per variant; gone for the game) */
u8 kael_beaten;             /* FF83E7: Kael is beaten (the area-4 exit opens) */

/* Restart records (1.7, 0x3560 + word[area]): x, y, time (BCD m:ss),
 * camera min x; native coordinates. Record x with bit 15 (areas 4, 6):
 * camera min x = x & 0xFF80; else x - 512 (>= 0). */
static const s16 rec0[8][4] = {
    { 192, 80, 0x200, 0 }, { 544, 80, 0x200, 32 }, { 800, 80, 0x200, 288 }, { 1056, 120, 0x200, 544 },
    { 1312, 80, 0x200, 800 }, { 1568, 120, 0x200, 1056 }, { 1824, 80, 0x200, 1312 }, { 2080, 80, 0x200, 1568 },
};
static const s16 rec1[5][4] = {
    { 192, 80, 0x200, 0 }, { 544, 80, 0x200, 32 }, { 736, 80, 0x200, 224 }, { 1408, 80, 0x200, 896 }, { 2080, 80, 0x200, 1568 },
};
static const s16 rec2[9][4] = {
    { 192, 80, 0x200, 0 }, { 224, 80, 0x200, 0 }, { 480, 80, 0x200, 0 }, { 736, 80, 0x200, 224 }, { 992, 80, 0x200, 480 },
    { 1248, 80, 0x200, 736 }, { 1504, 152, 0x200, 992 }, { 1760, 152, 0x200, 1248 }, { 2016, 152, 0x200, 1504 },
};
static const s16 rec3[9][4] = {
    { 192, 88, 0x200, 0 }, { 224, 88, 0x200, 0 }, { 480, 88, 0x200, 0 }, { 736, 88, 0x200, 224 }, { 992, 88, 0x200, 480 },
    { 1248, 88, 0x200, 736 }, { 1504, 88, 0x200, 992 }, { 1760, 88, 0x200, 1248 }, { 2016, 88, 0x200, 1504 },
};
static const s16 rec4[1][4] = { { 32, 80, 0x200, 0 } };
static const s16 rec5[9][4] = {
    { 352, 48, 0x300, 0 }, { 672, 272, 0x300, 160 }, { 352, 528, 0x230, 0 }, { 672, 784, 0x230, 160 },
    { 352, 1040, 0x230, 0 }, { 672, 1296, 0x200, 160 }, { 592, 1552, 0x200, 80 }, { 432, 1808, 0x130, 0 },
    { 608, 2144, 0x130, 96 },
};
static const s16 rec6[1][4] = { { 32, 72, 0x200, 0 } };
static const s16 (*const recs[7])[4] = { rec0, rec1, rec2, rec3, rec4, rec5, rec6 };
/* Index tables (0x356E + word[area]): by x >> 8 (areas 0-3) or y >> 8 (area 5). */
static const u8 cpi0[10] = { 0, 0, 1, 2, 3, 4, 5, 6, 7, 7 };
static const u8 cpi1[10] = { 0, 0, 1, 2, 2, 3, 3, 3, 4, 4 };
static const u8 cpi2[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 8 };
static const u8 cpi5[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 8 };
static const u8 *const cpis[7] = { cpi0, cpi1, cpi2, cpi2, NULL, cpi5, NULL };

u16 stage6_checkpoint(s16 x, s16 y)
{
    const u8 *t = cpis[area6];
    s16 k = area6 == 5 ? y >> 8 : x >> 8;
    if (!t) return 0;
    if (k < 0) k = 0;
    if (k > 9) k = 9;
    return t[k];
}

const s16 *stage6_rec(u16 cp) { return recs[area6][cp]; }

/* Music per area (1.7 records): 0x0C castle, 0x0E Kael, 0x0D tower, 0x0F Bavmorda. */
u16 stage6_music(void)
{
    static const u8 music[7] = { SND_MUSIC_STAGE6, SND_MUSIC_STAGE6, SND_MUSIC_STAGE6, SND_MUSIC_STAGE6, SND_MUSIC_KAEL, SND_MUSIC_TOWER6, SND_MUSIC_BAV };
    return music[area6];
}

/* Camera windows (2.3), native. Areas 0-3 scroll right to 1960 at a fixed
 * y; area 4 and 6 are fixed rooms; area 5 (the tower) climbs y 0..2048,
 * clamped to the tower's four block columns (the arcade's 384-px screen
 * could show wrapped columns; it never does in natural play). */
void stage6_camera(void)
{
    player.cam_min_y = player.cam_max_y = 0;
    switch (area6)
    {
    case 4: case 6:
        player.cam_min_x = player.cam_max_x = 0;
        break;
    case 5:
        player.cam_max_x = 1024 - 352;
        player.cam_max_y = 2048;
        break;
    default:
        player.cam_max_x = 1960;
        break;
    }
}

/* The scenery set of the current area (the bank must be in). */
void stage6_select(void)
{
    if (!bank_s6) bank_stage6(TRUE);
    sd = &stage6_data[area6];
    terrain_changed();
}

void stage6_new_game(void)
{
    extern void kael_new_game(void);
    extern void stage6_events_new(void);
    area6 = 0; doors6_used = 0; kael_beaten = 0;
    stage6_events_new();
    kael_new_game();
    { extern void bav_new_game(void); bav_new_game(); }
}

/* Families placed in stage 6 (3.1): only those whose art the stage loads
 * (src/families.c families_stage(5)); the rest are added with their art. */
u16 stage6_family(const StageObject *o)
{
    if (o->cls == 4)
        switch (o->kind)
        {
        case 0: return F_SOLDIER;
        case 3: return F_ARCHER;
        case 14: return F_SPEARMAN;
        case 17: return F_CROSSBOW;
        case 19: return F_KNIGHT;      /* mace knights (stage6_families.md 1) */
        case 27: return F_DOORWAY;     /* cart pushers (6.1) */
        case 24: return F_KAEL;
        case 28: return F_BRAZIER6;
        case 33: return F_MORTAR6;     /* the tower (stage6_families.md 4, 5) */
        case 34: return F_TROOP6;    /* the altar brazier (stage6_bavmorda.md 3.5) */
        }
    if (o->cls == 14 && o->kind == 2) return F_DOOR6;
    if (o->cls == 16 && o->kind == 4) return F_CHEST;
    if (o->cls == 6) return o->kind == 9 ? F_STONE6 : o->kind == 10 ? F_CRUSH6 : F_NONE;   /* stones, crushers (stage6_families.md 2, 3) */
    if (o->cls == 12 && o->kind == 8) return F_SHOP;      /* shop doors (8.2): stock by the dir byte */
    if (o->cls == 12 && o->kind == 40) return F_CANDLE6;
    if (o->cls == 18 && o->kind == 4) return F_BAV;       /* Queen Bavmorda */
    return F_NONE;
}

/* ---- Doors (2.5: class 14 kind 2, routine 0x2D78E). A door is a trigger:
 * the player within x +-12, y -14..+14 of it, grounded, holding Up without
 * Left/Right, freezes (FF83E9/EA) for 30 updates; then the area changes
 * (src/game.c game_area6: fade, the new area's scenery and objects, the
 * start position 0x2D994[v]). A false door, once taken, is gone for the
 * game (FF8384). Variant 10 (Kael's bridge) opens on its own after Kael. */
static const u8 door_to[12] = { 1, 2, 1, 3, 1, 2, 2, 4, 1, 3, 5, 6 };          /* 0x2D974 */
static const s16 door_start[12][2] = {                                          /* 0x2D994, native */
    { 64, 80 }, { 64, 80 }, { 64, 80 }, { 64, 168 }, { 64, 80 }, { 64, 80 },
    { 64, 80 }, { 32, 80 }, { 64, 80 }, { 64, 168 }, { 320, 48 }, { 40, 72 },
};
static const u8 door_true[12] = { 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1 };        /* 0x2DA4C */
u8 door6_go;                /* the door taken + 1: src/game.c changes the area */

void door6_update(Actor *a)
{
    s16 dx, dy;
    if (!a->state)
    {
        a->state = 1;
        if (!door_true[a->variant] && (doors6_used & (1 << a->variant))) { actor_free(a); return; }
        a->pose = 0;
    }
    if (a->state == 2)
    {
        if (++a->timer >= 30) { door6_go = a->variant + 1; a->state = 3; }
        return;
    }
    if (a->state != 1 || door6_go) return;
    if (a->variant == 10)
    {
        if (kael_beaten) { gate_freeze = 1; a->state = 2; a->timer = 0; }
        return;
    }
    dx = (player.x >> 8) - (a->x >> 8);
    dy = (player.y >> 8) - (a->y >> 8);
    if (dx < -12 || dx > 12 || dy < -14 || dy > 14 || player.airborne || player.dead) return;
    if ((player.last_held & (BUTTON_UP | BUTTON_LEFT | BUTTON_RIGHT)) != BUTTON_UP) return;
    gate_freeze = 1;
    a->state = 2;
    a->timer = 0;
}

/* The area a door leads to, its start position; marks a false door used. */
u16 door6_take(u16 v, s16 *x, s16 *y)
{
    if (!door_true[v]) doors6_used |= 1 << v;
    *x = door_start[v][0];
    *y = door_start[v][1];
    return door_to[v];
}

/* ---- Events (stage6.md 3.2; stage6_families.md 6.3): event 0 (area 0's
 * edge soldiers) runs in src/game.c's shared loop; here the archer pair
 * waiting on area 0's floor (1, 2), the cart pushers sent from 256 px
 * behind the player in area 3 (3-6) and the tower's respawning brute (7;
 * event 8 lies outside every area). Native coordinates of their areas. */
static const s16 cart_ev[4][4] = {      /* endX, y, startX, budget */
    { 624, 176, 368, 100 }, { 640, 96, 384, 192 }, { 928, 176, 672, 192 }, { 1568, 176, 1312, 160 },
};
static u8 ev6_done;                     /* bits: events retired (1-2: 1, 3-6: 2..5) */
static u8 brute6;                       /* the tower brute's slot + 1 */

void stage6_events_reset(void) { brute6 = 0; }
void stage6_events_new(void) { ev6_done = 0; brute6 = 0; }

void stage6_animators(const StageObject *tab, u16 n);
void stage6_events(void)
{
    u16 k;
    /* Every area's scenery ends below 750 and the torch / chain tiles use
     * 760-935: the pose cache borrows 940-1041 (taken back by every
     * stage_init / stage_restart, lent again here). */
    { extern void posecache_lend(u16 lo, u16 hi); posecache_lend(940, 1042); }
#ifndef EXP_NOANIM
    stage6_animators(stage6_objects_of(area6), stage6_count_of(area6));
#endif
    if (area6 == 6)
    {
        /* The far layer behind the columns (stage6.md 2.1, 2.6): it sways
         * +-1 px an update between far x 512 and 576, reversing every 64
         * (camera hook 0x43150; src/stage.c stage_far_dx), and its
         * lightning shows only while controllers 1-2 light scroll3
         * palettes 28/29. Natively the bolts are on the sprite line
         * (tools/far6.py; the chamber holds both scenery lines) and their
         * tiles are swapped with blanks: lit 3 x 9 updates of a 57-update
         * loop (the controllers' 19 steps x 3; the lit share as sampled in
         * run D7, the step pattern approximated). */
        extern s16 stage_far_dx;
        extern u8 anim6_loaded;
        static u8 sway_t, sway_dir, storm_t, storm_step, storm_reload;
        if (++sway_t >= 64) { sway_t = 0; sway_dir ^= 1; }
        stage_far_dx += sway_dir ? -1 : 1;
        if (stage_far_dx < 0) stage_far_dx = 0;
        if (stage_far_dx > 64) stage_far_dx = 64;
        if (!anim6_loaded)
        {
            extern void stage_far_all(void);
            stage_far_all();                /* the whole picture resident: the sway streams nothing */
            anim6_loaded = 1; storm_t = 1; storm_step = 18;   /* (re-laid after a stage_init / restart) */
            storm_reload = 1;
        }
        if (!--storm_t)
        {
            /* Sequences 19 / 20 (tools/far6.py): a step every 3 updates;
             * the state's bolt cells, the backdrop black or the dim glow. */
            static const u16 blank[FAR6_CELLS];
            extern const u8 far6_cells[];
            extern const u16 far6_words[];
            extern void stage_far_cells(const u8 *cells, const u16 *w, const u16 *was, u16 n);
            static const u16 *shown;            /* the words laid out last (NULL: unknown, after a reload) */
            const u16 *now;
            u8 st;
            storm_t = 3;
            if (++storm_step >= 19) storm_step = 0;
            st = far6_step[storm_step];
            now = st == 255 ? blank : far6_words + st * FAR6_CELLS;
            if (storm_reload) { shown = NULL; storm_reload = 0; }   /* the whole set after a reload */
            stage_far_cells(far6_cells, now, shown, FAR6_CELLS);
            shown = now;
            PAL_setColor(0, far6_glow[storm_step] ? FAR6_GLOW : 0);
        }

        /* The chamber's grey lightning flash (stage6_bavmorda.md 7: scroll2
         * palettes 13-16, a 117-update loop, 3-update grey frames at ~3, 63,
         * 99, 108): here both scenery lines, lightened to grey. */
        static u8 loop;
        static const u16 *shown;
        u16 t = loop;
        bool grey = (t >= 3 && t < 6) || (t >= 63 && t < 66) || (t >= 99 && t < 102) || (t >= 108 && t < 111);
        if (++loop >= 117) loop = 0;
        if (grey && shown != (const u16 *)1)
        {
            static u16 g[32];
            for (k = 0; k < 32; k++)
            {
                u16 w = sd->pal[k], l = (((w >> 1) & 7) + ((w >> 5) & 7) + ((w >> 9) & 7) + 9) / 3;
                if (l > 7) l = 7;
                g[k] = k & 15 ? (l << 1) | (l << 5) | (l << 9) : w;
            }
            PAL_setColors(32, g, 32, DMA_QUEUE);
            shown = (const u16 *)1;
        }
        else if (!grey && shown == (const u16 *)1) { PAL_setColors(32, sd->pal, 32, DMA_QUEUE); shown = NULL; }
    }
    if (area6 == 0 && !(ev6_done & 1))
    {
        static const s16 at[4] = { 1120, 1248, 1632, 1760 };
        ev6_done |= 1;
        for (k = 0; k < 4; k++)
        {
            Actor *a = actor_new(F_ARCHER);
            if (!a) break;
            a->variant = 2; a->facing = 1;
            a->x = (s32)at[k] << 8; a->y = (s32)80 << 8;
            a->flags = SLEEP;
        }
    }
    if (area6 == 3 && !(game_frame & 15))
        for (k = 0; k < 4; k++)
        {
            const s16 *e = cart_ev[k];
            s16 d = e[0] - (s16)(player.x >> 8);
            if (ev6_done & (2 << k)) continue;
            if ((u16)(e[0] - CAM_X) > 384 || (u16)(e[1] - CAM_Y) > 256) continue;
            if (d <= -10 || d >= 10) continue;
            ev6_done |= 2 << k;
            {
                Actor *a = actor_new(F_DOORWAY);
                if (!a) continue;
                a->variant = 2; a->facing = e[3];
                a->x = (s32)e[2] << 8; a->y = (s32)e[1] << 8;
            }
        }
    if (area6 == 5 && !(game_frame & 31))
    {
        Actor *b = brute6 ? actors + brute6 - 1 : NULL;
        if (b && (b->fam != F_SPEARMAN || b->variant != 14)) b = NULL;
        if (!b && (u16)(736 - CAM_X + 64) <= 512 && (u16)(192 - CAM_Y + 32) <= 320)
        {
            b = actor_new(F_SPEARMAN);
            if (b) { b->variant = 14; b->facing = 1; b->x = (s32)736 << 8; b->y = (s32)192 << 8; brute6 = (u8)(b - actors) + 1; }
        }
        else if (!b) brute6 = 0;
    }
}

/* ---- Wall torches (class 12 kind 6 variant 6, 2 x 2 16-px cells, 3
 * frames) and area 2's chandeliers (variant 7, 3 x 2, 2 frames), every 7
 * updates while near the view (stage6_families.md 8). A tile has one CRAM
 * line: the plane keeps a static wall block on the scenery lines (its
 * flame pixels dark) and the flames are sprites on line 1 (tools/
 * torchpack6.py; tiles from ANIM_VRAM, free in every area). All of an
 * area's animators start at its load, so they share one clock. */
#include "../data/stage6/a0/anim.h"
#include "../data/stage6/a1/anim.h"
#include "../data/stage6/a2/anim.h"
#include "../data/stage6/a3/anim.h"
#define ANIM_VRAM 760
extern const u16 s6a0_anim_words[], s6a1_anim_words[], s6a2_anim_words[], s6a3_anim_words[];
extern const u8 s6a0_anim_tiles[], s6a1_anim_tiles[], s6a2_anim_tiles[], s6a3_anim_tiles[];
static const u16 *const anim_words[4] = { s6a0_anim_words, s6a1_anim_words, s6a2_anim_words, s6a3_anim_words };
static const u8 *const anim_tiles[4] = { s6a0_anim_tiles, s6a1_anim_tiles, s6a2_anim_tiles, s6a3_anim_tiles };
static const u8 anim_ntiles[4] = { A0_TILES, A1_TILES, A2_TILES, A3_TILES };
static const u8 anim_torch[4] = { A0_TORCH_SPR, A1_TORCH_SPR, A2_TORCH_SPR, A3_TORCH_SPR };
static const u8 anim_chand[4] = { A0_CHAND_SPR, A1_CHAND_SPR, A2_CHAND_SPR, A3_CHAND_SPR };
extern u8 anim6_loaded;
extern void stage_cells_put(s16 c, s16 r, const u16 *w, u16 cols, u16 rows, u16 base);
static u8 anim_timer = 7, anim_frame;

static bool anim_near(const StageObject *o) { return (u16)(o->x - CAM_X + 24) <= 432 && (u16)(o->y - CAM_Y + 24) <= 304; }

void stage6_animators(const StageObject *tab, u16 n)
{
    u16 i;
    if (area6 > 3 || --anim_timer) return;
    anim_timer = 7;
    anim_frame++;
    for (i = 0; i < n; i++)
    {
        const StageObject *o = tab + i;
        u16 v = o->variant;
        if (o->cls != 12 || o->kind != 6 || (v != 6 && v != 7) || !anim_near(o)) continue;
        if (!anim6_loaded) { VDP_loadTileData((const u32 *)anim_tiles[area6], ANIM_VRAM, anim_ntiles[area6], DMA); anim6_loaded = 1; }
        else if (anim_frame & 3) continue;  /* the wall block is static: re-laid every 28 updates (after column loads) */
        stage_cells_put((o->x >> 4) * 2, ((s16)(sd->world_top - o->y) >> 4) * 2,
                        anim_words[area6] + (v == 7 ? 16 : 0), v == 7 ? 6 : 4, 4, ANIM_VRAM);
    }
}

/* The flames (main.c's draw hook): a 32 x 32 sprite a torch, 32 + 16 a
 * chandelier; the area's animators listed once (they are a fixed set). */
static const StageObject *torch_list[16];
static u8 torch_n, torch_area = 0xFF;
u16 torches6_draw(VDPSprite *sat, u16 first, u16 max, s16 view_x, s16 view_y)
{
    u16 i, k = first;
    if (area6 > 3 || !anim6_loaded) return 0;
    if (torch_area != area6)
    {
        const StageObject *tab = stage6_objects_of(area6);
        u16 n = stage6_count_of(area6);
        torch_n = 0; torch_area = area6;
        for (i = 0; i < n && torch_n < 16; i++)
            if (tab[i].cls == 12 && tab[i].kind == 6 && (tab[i].variant == 6 || tab[i].variant == 7)) torch_list[torch_n++] = tab + i;
    }
    for (i = 0; i < torch_n && k + 2 <= max; i++)
    {
        const StageObject *o = torch_list[i];
        u16 v = o->variant, t;
        s16 sx, sy;
        if (!anim_near(o)) continue;
        sx = (o->x & ~15) - view_x + 128;
        sy = (((s16)(sd->world_top - o->y) >> 4) << 4) - (view_y + sd->world_top - 511) + 128;
        if (v == 6) t = ANIM_VRAM + anim_torch[area6] + (anim_frame % 3) * 16;
        else t = ANIM_VRAM + anim_chand[area6] + (anim_frame & 1) * 24;
        sat[k].y = sy; sat[k].size_link = (15 << 8) | (k + 1); sat[k].attribut = 0x8000 | (1 << 13) | t; sat[k].x = sx; k++;
        if (v == 7) { sat[k].y = sy; sat[k].size_link = (7 << 8) | (k + 1); sat[k].attribut = 0x8000 | (1 << 13) | (t + 16); sat[k].x = sx + 32; k++; }
    }
    return k - first;
}
