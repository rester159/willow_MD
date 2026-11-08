/* Stage 1 flow (specs/stage1_enemies.md 1-2, combat.md, stage1_boss.md):
 * placed objects and spawn events, the collision passes, the waterfall
 * encounter and camera gate, life loss, checkpoints, time-out, continue and
 * game over. */
#include <genesis.h>
#include "stagedata.h"
#include "game.h"
#include "actor.h"
#include "coop.h"
#include "player.h"
#include "hud.h"
#include "shot.h"
#include "magic.h"
#include "sound.h"
#include "stage.h"
#include "bank.h"
#include "shop.h"
#include "frontend.h"
#include "spell.h"
#include "screens.h"
#include "packed.h"

#include "stage1_objects.h"
#include "stage2_objects.h"
#include "stage3_objects.h"
#include "stage4_objects.h"
#include "stage5_objects.h"
#include "stage6_objects.h"
const StageObject *stage6_objects_of(u16 a) { return stage6_objects[a]; }   /* src/fam_stage6.c */
u16 stage6_count_of(u16 a) { return stage6_count[a]; }
#include "bank.h"

/* Stage 6 (src/fam_stage6.c). */
extern u8 area6;
extern u16 stage6_family(const StageObject *o), stage6_checkpoint(s16 x, s16 y), stage6_music(void);
extern const s16 *stage6_rec(u16 cp);
extern void stage6_camera(void), stage6_select(void), stage6_new_game(void);

extern u8 chest_broken, reward_set;
extern const u8 story_tiles[];
extern const u16 story_map[], story_pal[];
extern const u8 story2_tiles[];
extern const u16 story2_map[], story2_pal[];
extern const u8 story3_tiles[];
extern const u16 story3_map[], story3_pal[];
extern const u8 story4_tiles[];
extern const u16 story4_map[], story4_pal[];
extern const u8 story5_tiles[];
extern const u16 story5_map[], story5_pal[];
#include "../data/intro/intro_text.h"
#include "../data/intro/story2.h"
#include "../data/intro/story3.h"
#include "../data/intro/story4.h"
#include "../data/intro/story5.h"
#include "../data/intro/story6.h"
extern const u8 story6_tiles[];
extern const u16 story6_map[], story6_pal[];

u8 game_mode;
u8 game_hide_sprites;       /* the story page: no sprites */
extern u16 prof[8];
u8 checkpoint;              /* FF8394: only ever increases */
static u8 lives_dip = 2;    /* DIP lives (arcade default in the traces) */
u8 game_lives_dip(void) { return lives_dip; }   /* co-op: a joining player's lives */
static u16 start_rank = 0x3000;   /* DIP difficulty (0x1B3E) */
static u16 mode_timer;
static u8 continues;         /* FF839B: continues taken this run (saturates at 255) */
u8 game_continues(void) { return continues; }
void game_count_continue(void) { if (continues < 255) continues++; }   /* co-op: an out player continues */
#define HUD_VRAM 1664       /* the HUD's tiles (main.c HUD_BASE) */
extern s32 chase_vx;

/* ---- Placed objects (ROM 0x40470) -> families. */
static u16 family_of(const StageObject *o)
{
    if (stage_index == 5) return stage6_family(o);
    switch (o->cls)
    {
    case 4:
        switch (o->kind)
        {
        case 0: return F_SOLDIER;
        case 1: return o->variant == 3 ? F_ARENA_BEAST : F_BEAST;
        case 3: return F_ARCHER;
        case 6: return F_NONE;          /* the body comes from the sage's scene */
        case 7: return o->variant == 2 ? F_SORCERER : F_SAGE;
        case 2: return F_CHANDELIER;
        case 16: return F_CRATEARCHER;
        case 17: return F_CROSSBOW;
        case 18: return F_LURKER;
        case 15: return stage_index == 3 ? F_HAWK4 : F_NONE;    /* stage 2: the chase's events; stage 4: the swooping hawks */
        case 20: case 21: return F_NONE;   /* the chase's families: events */
        case 14: return sd != &stage2_data ? F_SPEARMAN : F_EMERGE;
        case 23: return F_TROOPER;      /* stage 3 (stage3.md 5.1) */
        case 30: return F_SNOWBALL;     /* stage 4 (stage4.md 5) */
        case 31: return F_BAT;
        case 26: return F_TROLL;        /* stage 5 (stage5.md 5.1) */
        case 27: return F_DOORWAY;      /* stage 5 (5.3) */
        case 19: return F_KNIGHT;   /* stage 2: the door spearman (variant 8) emerges first */
        }
        break;
    case 18:
        if (stage_index == 2 && o->kind == 2) return F_BOSS3;   /* stage 3's boss (stage3.md 6) */
        if (stage_index == 3) return o->kind == 5 ? F_DRAGON : o->kind == 3 ? F_SORSHA : F_NONE;   /* stage 4 (6) */
        if (stage_index == 4) return o->kind == 6 ? F_EBOR : F_NONE;                              /* stage 5 (6) */
        return F_NONE;
    case 6:
        if (o->kind == 6) return o->variant < 2 ? F_PILLAR : F_NONE;   /* stage 3's sinking pillars (5.4) */
        if (o->kind == 3) return F_BOAT;                                 /* stage 3's boat (5.5) */
        if (o->kind == 4) return F_SHIELD;                               /* stage 4's shield sled (6.13) */
        if (o->kind == 7) return F_STONE;                                /* stage 5's stone pillars (5.4) */
        return o->kind == 0 ? F_LOG : o->kind == 1 ? F_PLATFORM : o->kind == 2 ? F_WAGON : F_NONE;     /* kind 2: stage 2's wagon */
    case 16:
        if (o->kind == 4) return F_CHEST;
        if (o->kind == 9) return F_HIDDEN;
        break;
    case 12:
        if (o->kind == 8) return F_SHOP;      /* shop doors */
        if (o->kind == 10) return F_SHUTTER;  /* stage 2's window shutters */
        if (o->kind == 35) return F_GATE5;    /* stage 5's tower doorway (stage5.md 5.6) */
        if (o->kind == 6 && o->variant == 0 && sd != stage_data) return F_TORCH;   /* stage 2's wall torches */
        break;
    }
    return F_NONE;
}

static void spawn_objects(void)
{
    u16 i, n;
    const StageObject *tab;
    s16 px = player.x >> 8, py = player.y >> 8;
    if (stage_index == 0) { tab = stage_objects; n = sizeof stage_objects / sizeof stage_objects[0]; }
    else if (stage_index == 1) { tab = stage2_objects; n = sizeof stage2_objects / sizeof stage2_objects[0]; }
    else if (stage_index == 2) { tab = stage3_objects; n = sizeof stage3_objects / sizeof stage3_objects[0]; }
    else if (stage_index == 3) { tab = stage4_objects; n = sizeof stage4_objects / sizeof stage4_objects[0]; }
    else if (stage_index == 4) { tab = stage5_objects; n = sizeof stage5_objects / sizeof stage5_objects[0]; }
    else { tab = stage6_objects[area6]; n = stage6_count[area6]; }
    for (i = 0; i < n; i++)
    {
        const StageObject *o = tab + i;
        u16 f = family_of(o);
        Actor *a;
        if (!f || !families[f]) continue;
#ifdef PROF_NOLOGS
        if (f == F_LOG) continue;
#endif
        /* Load filter (0x40378): enemies behind the camera's minimum or
         * close around the player are not placed. */
        if (o->cls == 4 && (o->x < player.cam_min_x ||
                            ((u16)(o->x - px + 160) <= 288 && o->y >= py - 100 && o->y <= py + 100)))
            continue;
        a = actor_new(f);
        if (!a) continue;
        a->variant = o->variant;
        a->facing = o->dir;
        a->x = (s32)o->x << 8;
        a->y = (s32)o->y << 8;
        a->spawn = i + 1;
        a->active = 0;
        a->flags = f == F_HIDDEN || f == F_BOAT || f == F_EBOR ? 0 : SLEEP;   /* dormant until the camera comes near (the hidden object places itself at once; the boat joins a restart on the lake) */
    }
}

/* ---- Spawn events (ROM 0x9AD8; specs 2.2): edge soldiers and cave beasts.
 * The anti-idle apparition (event 0) is not in this build. */
typedef struct { u8 kind, param; s16 start_x, end_x, y; u8 state, left, quota; u16 timer; } Event;
#define MAX_EVENTS 4
static Event events[MAX_EVENTS];
static const Event events_init[4] = {
    { 1, 0, 1816, 1904, 240, 0, 0, 0, 0 }, { 1, 1, 2512, 2512, 192, 0, 0, 0, 0 },
    { 0, 0, 224, 1280, 88, 0, 0, 0, 0 }, { 0, 0, 2816, 3072, 88, 0, 0, 0, 0 },
};
/* Stage 2's fort events (stage2.md 3.2: 0, 1 edge soldiers, 2 soldier
 * pairs; 3 the door spearman is its own family); the chase's (4-9) are run
 * by src/fam_chase.c. */
#define EV_PAIRS 2
#define EV_DOOR 14
static const Event events2_init[4] = {
    { 0, 0, 0, 288, 88, 0, 0, 0, 0 }, { 0, 0, 832, 1536, 88, 0, 0, 0, 0 },
    { EV_PAIRS, 0, 0, 1120, 88, 0, 0, 0, 0 }, { EV_DOOR, 0, 1320, 1320, 200, 0, 0, 0, 0 },
};
static const u8 edge_cap[16] = { 1, 2, 2, 3, 3, 3, 4, 5, 5, 6, 6, 6, 6, 7, 7, 8 };          /* 0x383C0 */
static const u8 edge_delay[16] = { 16, 128, 192, 64, 128, 48, 64, 128, 48, 128, 64, 32, 64, 128, 192, 64 };
static const u8 edge_side[16] = { 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1 };        /* 14/16 from the right */
static const u8 beast_wait[4] = { 64, 96, 128, 180 };

u8 beast_quota[2];          /* restored by the event's beast when it dies */

static void events_update(void)
{
    u16 i;
    extern s32 chase_vx;
    if (stage_index == 1) { extern void chase_events(void); chase_events(); }   /* events 4-9: src/fam_chase.c */
    if (stage_index == 2) { stage3_events(); return; }                          /* src/fam_stage3.c */
    if (stage_index == 3) { stage4_events(); return; }                          /* src/fam_stage4.c */
    if (stage_index == 4) { stage5_events(); return; }                          /* src/fam_stage5.c */
    if (stage_index == 5) { extern void stage6_events(void); stage6_events(); }   /* src/fam_stage6.c; event 0 below */
    for (i = 0; i < MAX_EVENTS; i++)
    {
        Event *e = events + i;
        if (e->state == 9) continue;
        if (stage_index == 1 && (chase_vx || (player.x >> 8) > 1536)) { e->state = 9; continue; }   /* 0x38390: retired by the chase */
        if (e->kind == EV_DOOR) { extern void door_event(void); door_event(); continue; }
        if (e->kind == EV_PAIRS && stage_index == 1)             /* soldier pairs every 240 frames (0x384F6) */
        {
            if (CAM_X < e->start_x || CAM_X >= e->end_x) continue;
            if (++e->timer < 240) continue;
            e->timer = 0;
            {
                Actor *a = actor_new(F_SOLDIER), *b;
                if (!a) continue;
                a->variant = 0;
                a->facing = rnd() & 1;
                a->y = (s32)(CAM_Y + 144) << 8;
                b = actor_new(F_SOLDIER);
                if (!b) continue;
                b->variant = 0;
                b->facing = a->facing ^ 1;
                b->y = (s32)((player.y >> 8) + 23) << 8;
            }
            continue;
        }
        if (e->kind == 0)                                   /* edge soldiers (0x38362) */
        {
            if (CAM_X < e->start_x) continue;
            if (CAM_X >= e->end_x || encounter_clear || (!(game_frame & 15) && e->end_x < player.cam_min_x)) { e->state = 9; continue; }
            if (e->timer) { if (!--e->timer) goto spawn_soldier; continue; }
            if (edge_cap[(rank >> 12) & 15] > edge_alive) e->timer = edge_delay[rnd() & 15];
            continue;
        spawn_soldier:
            {
                Actor *a = actor_new(F_SOLDIER);
                if (!a) continue;
                a->variant = 0;
                a->facing = edge_side[rnd() & 15];
                a->y = (s32)e->y << 8;
                edge_alive++;
            }
        }
        else                                                /* beasts (0x387CC) */
        {
            if (!e->state) { e->state = 1; e->left = 3; beast_quota[i] = 1; }
            if (!(game_frame & 63) && e->end_x < player.cam_min_x) { e->state = 9; continue; }
            if (e->timer) { e->timer--; continue; }
            if (!beast_quota[i] || !e->left || (game_frame & 3)) continue;
            if (e->end_x - CAM_X <= 384 && e->y - CAM_Y - 16 <= 224)
            {
                Actor *a = actor_new(F_BEAST);
                if (!a) continue;
                a->variant = 1;
                a->x = (s32)e->end_x << 8;
                a->y = (s32)e->y << 8;
                a->v[4] = e->param ? e->start_x - 40 : e->start_x - 64;   /* patrol limits */
                a->v[5] = e->param ? e->start_x + 48 : e->start_x + 64;
                a->v[3] = i + 1;                                /* quota owner */
                beast_quota[i] = 0;
                e->left--;
                e->timer = beast_wait[rnd() & 3];
                if (!e->left) e->state = 9;
            }
        }
    }
}

#ifdef SHOTBOUND_CHECK
u16 sb_rejects, sb_wrong;   /* measurement: enemies the pre-filter skipped, and those a test would have hit */
#endif

/* ---- Collision passes (combat.md 1). */
void collide_player(void);

static void collisions(void)
{
    u16 i, f = game_frame & 3;
    /* The union of every shot and spell box this update: an enemy whose box
     * misses it cannot be hit (exact: a pre-filter for the tests below). */
    s16 su[4], pu[4];
    bool shots_on = shot_bounds(su);
    u16 spells_on = spell_bounds(pu);
    if (spells_on == 1)
    {
        if (!shots_on) { su[0] = su[2] = 0x7FFF; su[1] = su[3] = -0x8000; }
        if (pu[0] < su[0]) su[0] = pu[0];
        if (pu[1] > su[1]) su[1] = pu[1];
        if (pu[2] < su[2]) su[2] = pu[2];
        if (pu[3] > su[3]) su[3] = pu[3];
    }
    if (shots_on || spells_on)
    for (i = 0; i < pool_hi[C_ENEMY]; i++)                  /* shots vs enemies (alternate frames) */
    {
        Actor *a = actors + i;
        u16 power;
        if (!a->fam || a->active != 1 || a->fam == F_SERPENTB || ((i ^ f) & 1)) continue;   /* serpent bodies: touch only (stage3.md 6.9) */
        if ((u16)(AX(a) - CAM_X + 96) > 576 || (u16)(AY(a) - CAM_Y + 96) > 512) continue;  /* far off screen: no shot or spell reaches it */
#ifndef NO_SHOTBOUND
        if (spells_on != 2)
        {
            const s16 *b = actor_box(a, 1);
            s16 ex = AX(a) - b[0], ey = AY(a) - b[2];
            if (ex > su[1] || ex + b[1] < su[0] || ey > su[3] || ey + b[3] < su[2])
            {
#ifdef SHOTBOUND_CHECK
                extern bool shot_would_hit(s16, s16, const s16 *), spell_would_hit(s16, s16, const s16 *);
                extern u16 sb_rejects, sb_wrong;
                sb_rejects++;
                if (shot_would_hit(AX(a), AY(a), b) || spell_would_hit(AX(a), AY(a), b)) sb_wrong++;
#endif
                continue;
            }
        }
#endif
        if ((power = shot_test(AX(a), AY(a), actor_box(a, 1), a->fam == F_BOSS_BODY || DEFLECTS(a))) != 0 && a->fam != F_BOSS_BODY && !DEFLECTS(a))
        {
            a->hp = a->hp > power ? a->hp - power : 0;
            a->active = 2;
            if (two_players) a->pad[0] = shot_hit_sword == 1 ? !player.form : shot_hit_sword ? 0 : player.form;   /* co-op: who hit it (who scores) */
        }
        else if (a->fam != F_BOSS_BODY && !DEFLECTS(a) && (power = spell_test(AX(a), AY(a), actor_box(a, 1), a->hp)) != 0)
        {
            a->hp = a->hp > power ? a->hp - power : 0;      /* spells 7-12 (src/spell.c) */
            a->active = 2;
            if (two_players) a->pad[0] = player.form;       /* co-op: Willow's spell */
        }
    }
    for (i = pool_lo[C_ESHOT]; i < pool_hi[C_ESHOT]; i++)  /* spell 12's blast cancels enemy projectiles (0x7102) */
    {
        Actor *a = actors + i;
        if (!a->fam || a->active != 1 || !spell_cuts(AX(a), AY(a), actor_box(a, 0))) continue;
        if (a->fam == F_ARROW) arrow_break(a);
        else if (a->fam == F_FIREBALL || a->fam == F_SQUIGGLE || a->fam == F_ROCK) actor_free(a);
    }
    collide_player();
}

/* The sword against arrows and enemies against the player: the lead's
 * record, and in co-op the second player's too (src/coop.c swaps it in). */
void collide_player(void)
{
    const s16 *pb = player_box();
    s16 px = player.x >> 8, py = player.y >> 8;
    u16 i, f = game_frame & 3;
    if (player.form)                                        /* the sword cuts arrows (madmartigan.md 5.6) */
        for (i = pool_lo[C_ESHOT]; i < pool_hi[C_ESHOT]; i++)
        {
            Actor *a = actors + i;
            if (a->fam == F_ARROW && a->active == 1 && shot_sword_cuts(AX(a), AY(a), actor_box(a, 0))) arrow_break(a);
        }
    if (player.vuln != 1) return;
    {
        static const u8 hurts[3] = { C_ESHOT, C_ENEMY, C_BOSS };
        u16 k;
        for (k = 0; k < 3; k++)
        {
            u16 c = hurts[k];
            if (c != C_ESHOT && f != 3) continue;       /* bodies: every 4th frame */
            for (i = pool_lo[c]; i < pool_hi[c]; i++)
            {
                Actor *a = actors + i;
                if (!a->fam || a->active != 1 || DEFLECTS(a)) continue;
                if ((u16)(AX(a) - px + 64) > 128 && a->fam != F_BOSS_BODY && a->fam != F_NECK5 && a->fam != F_HEAD5) continue;
                if (box_hit(AX(a), AY(a), actor_box(a, 0), px, py, pb))
                {
                    player_hurt();              /* projectiles fly on (combat.md 2.1) */
                    return;
                }
            }
        }
    }
}

/* ---- Encounter flags and camera stops (stage1_enemies.md 1). */
static u8 leap_done;        /* stage 2's wagon-leap trigger has fired */

static void stage_flags(void)
{
    if (stage_index == 1)
    {
        /* The wagon-leap trigger at the tower top (class 16 kind 3 at
         * (1544, 400), 0x36CD0; stage2.md 5.12): once. */
        s16 px = player.x >> 8, py = player.y >> 8;
        if (!leap_done && (u16)(px - 1544 + 8) <= 16 && (u16)(py - 400 + 32) <= 288 && hud.hp && !player.dead && player.form)
        {
            leap_done = 1;
            player_script(12);
        }
        return;
    }
    if (stage_index) return;
    if (!gate_open)
    {
        encounter_clear = CAM_X >= 1822;
        arrival = CAM_X >= player.cam_max_x;
    }
    else arrival = 0;
}

void game_open_gate(void)
{
    gate_open = 1;
    encounter_clear = 0;
    player.cam_max_x = 3328;
    player.cam_min_x = player.cam_x;
    player.cam_catch = 1;
}

/* ---- Checkpoints (combat.md 3.5; ROM 0x3058, 0x3078). */
/* By 128-px step of x (the ROM table is by 256 px and y bit 8; the two
 * columns where y matters are handled below). */
static const u8 cp_table[] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6 };
static const s16 cp_rec[7][4] = {   /* x, y, time (BCD m:ss), camera min x */
    { 192, 72, 0x300, 0 }, { 928, 72, 0x300, 416 }, { 1440, 176, 0x300, 928 }, { 1920, 304, 0x200, 1408 },
    { 2400, 208, 0x200, 1888 }, { 2912, 72, 0x200, 2400 }, { 3392, 72, 0x100, 3328 },
};

/* Stage 2 (stage2.md 1.6): records 0x3114 (x, y, time, camera min x =
 * x - 512), selection 0x3106 by 256-px step of x and y bit 8. */
static const s16 cp_rec2[10][4] = {
    { 192, 80, 0x330, 0 }, { 288, 80, 0x330, 0 }, { 480, 344, 0x330, 0 }, { 704, 296, 0x300, 192 },
    { 800, 80, 0x300, 288 }, { 800, 360, 0x300, 288 }, { 1056, 80, 0x300, 544 }, { 1056, 360, 0x300, 544 },
    { 1312, 80, 0x300, 800 }, { 1728, 93, 0x300, 1216 },
};
static const u8 cp_table2[14] = { 0, 0, 1, 2, 1, 3, 4, 5, 6, 7, 8, 7, 9, 9 };

/* Stage 3 (stage3.md 1.6): records 0x31C0 (x, y - 256, time, camera min x =
 * x - 512), selection 0x3190 by 256-px step of x and arcade y bit 8. */
static const s16 cp_rec3[14][4] = {
    { 192, 72, 0x430, 0 }, { 608, 288, 0x430, 96 }, { 960, 272, 0x430, 448 }, { 1456, 336, 0x400, 944 },
    { 1680, 256, 0x400, 1168 }, { 1936, 112, 0x400, 1424 }, { 2464, 112, 0x300, 1952 }, { 2880, 60, 0x300, 2368 },
    { 3392, 60, 0x300, 2880 }, { 3904, 60, 0x300, 3392 }, { 4416, 60, 0x300, 3904 }, { 4928, 60, 0x300, 4416 },
    { 5440, 60, 0x300, 4928 }, { 5952, 96, 0x200, 5440 },
};
/* Stage 4 (stage4.md 1.7): records 0x3334 (x, y, time, camera min x),
 * selection 0x325C by 256-px step of x and ((y >> 8) - 16) & 7. */
static const s16 cp_rec4[14][4] = {
    { 128, 4176, 0x530, 0 }, { 544, 4208, 0x530, 32 }, { 1056, 4384, 0x530, 544 }, { 1568, 4520, 0x530, 1056 },
    { 2080, 4752, 0x530, 1568 }, { 2592, 4888, 0x500, 2080 }, { 3104, 4664, 0x500, 2592 }, { 3872, 4664, 0x430, 3360 },
    { 4048, 4952, 0x430, 3536 }, { 4192, 5240, 0x430, 3680 }, { 4896, 5432, 0x400, 4384 }, { 5408, 5432, 0x400, 4896 },
    { 5920, 5432, 0x400, 5408 }, { 6576, 5432, 0x330, 6528 },
};
static const u8 cp_table4[27 * 8] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 8, 9, 9, 9, 9,
    7, 7, 7, 8, 9, 9, 9, 9, 7, 7, 7, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13 };
static const u8 cp_table3[48] = { 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7,
                                  8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 13, 13, 13, 13 };
/* Stage 5 (stage5.md 1.7): records 0x342C (x, y, time, camera min x = x -
 * 512; 18: the arena, x 5376 + 96, camera 5376), selection 0x33CC by
 * 4 * (x >> 8) + (y >> 8). */
static const s16 cp_rec5[19][4] = {
    { 192, 72, 0x530, 0 }, { 1120, 96, 0x530, 608 }, { 1376, 416, 0x500, 864 }, { 2240, 416, 0x430, 1728 },
    { 2560, 640, 0x430, 2048 }, { 2944, 640, 0x430, 2432 }, { 3200, 640, 0x400, 2688 }, { 3456, 640, 0x400, 2944 },
    { 3712, 640, 0x400, 3200 }, { 3968, 640, 0x400, 3456 }, { 4160, 608, 0x330, 3648 }, { 4576, 544, 0x330, 4064 },
    { 4512, 384, 0x330, 4000 }, { 4752, 368, 0x320, 4240 }, { 4448, 79, 0x310, 3936 }, { 4608, 79, 0x300, 4096 },
    { 4880, 128, 0x245, 4368 }, { 5056, 128, 0x245, 4544 }, { 5472, 128, 0x230, 5376 },
};
static const u8 cp_table5[96] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9,
    10, 10, 10, 10, 14, 12, 10, 10, 15, 13, 11, 11, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18,
    18, 18, 18, 18, 18, 18, 18, 18 };
u8 stage5_form;             /* FF8386: the select screen's choice (0 Willow, 1 Madmartigan) */
u8 stage_index;             /* FF8344: 0 stage 1, 1 stage 2 */

static u16 checkpoint_of(s16 x, s16 y)
{
    if (stage_index == 1)
    {
        u16 i = (x >> 8) * 2 + ((y >> 8) & 1);
        return cp_table2[i < 14 ? i : 13];
    }
    if (stage_index == 5) return stage6_checkpoint(x, y);
    if (stage_index == 4)
    {
        s16 r = y < 0 ? 0 : (y >> 8) & 31;
        u16 i = (x < 0 ? 0 : (x >> 8) * 4) + r;
        return i < sizeof cp_table5 ? cp_table5[i] : 18;
    }
    if (stage_index == 3)
    {
        s16 r = (y >> 8) - 16;
        u16 i = (x >> 8) * 8 + ((r < 0 ? 0 : r) & 7);
        if (x < 0) i = 0;
        return i < sizeof cp_table4 ? cp_table4[i] : 13;   /* the ride: no restart point (stage4.md 1.7) */
    }
    if (stage_index == 2)
    {
        u16 i = (x >> 8) * 2 + ((((y + 256) >> 8) - 1) & 1);
        if (x < 0) i = 0;
        return cp_table3[i < 48 ? i : 47];
    }
    {
    u16 k = x >> 7;                         /* 128-px steps, y split ignored */
    u16 c;
    if (k >= sizeof cp_table) k = sizeof cp_table - 1;
    c = cp_table[k];
    if (x >= 1024 && x < 1280 && y >= 256) c = 2;
    if (x >= 1792 && x < 2048 && y >= 256) c = 3;
    if (c == 4 && !gate_open) c = 3;
    return c;
    }
}

static u8 booted;            /* the first start: main has just streamed the stage in */
s16 s6_entry_x, s6_entry_y;  /* stage 6: the start position of the door just taken (0: the record's) */

static void stage_start(void)
{
    extern u8 chase_armed;
    extern s32 chase_vx;
    s32 resume = 0;
    const s16 *r;
    if (stage_index == 1 && chase_armed)
    {
        /* A death in the chase (stage2.md 1.6): back at the last 4096-px
         * mark of the virtual camera (1728 at the first), checkpoint 9. */
        resume = (chase_vx >> 8) & 0x7000;
        if (!resume) resume = 1728;
        checkpoint = 9;
    }
    r = stage_index == 5 ? stage6_rec(checkpoint) : stage_index == 4 ? cp_rec5[checkpoint] : stage_index == 3 ? cp_rec4[checkpoint] : stage_index == 2 ? cp_rec3[checkpoint] : stage_index ? cp_rec2[checkpoint] : cp_rec[checkpoint];
    actor_clear();
    {                                       /* stage 6's stone cells (src/actor.c) */
        extern u8 npatch; extern s16 patch_c0, patch_c1, patch_r0, patch_r1;
        npatch = 0; patch_c0 = patch_r0 = 0x7FFF; patch_c1 = patch_r1 = -1;
        terrain_changed();
    }
    boss_reset();
    memset(events, 0, sizeof events);
    if (stage_index < 2) memcpy(events, stage_index ? events2_init : events_init, sizeof events);
    else if (stage_index == 2) { stage3_events_reset(); boss3_reset(); }
    else if (stage_index == 3) { stage4_reset(); stage4_events_clear(); }
    else if (stage_index == 4) { stage5_reset(); stage5_events_clear(); boss5_reset(); }
    else
    {
        static const Event ev6 = { 0, 0, 64, 544, 80, 0, 0, 0, 0 };   /* stage6.md 3.2 event 0: area 0's edge soldiers */
        extern void stage6_events_reset(void);
        if (area6 == 0) events[0] = ev6;
        stage6_events_reset();
    }
    { extern void door_event_reset(void); extern void chase_reset(void); door_event_reset(); chase_reset(); }
    edge_alive = 0;
    encounter_clear = arrival = midboss_active = 0;
    bridge_collapse[0] = bridge_collapse[1] = 0;
    player_init(r[0], r[1]);
    player.cam_min_x = r[3];
    if (s6_entry_x) { player_init(s6_entry_x, s6_entry_y); s6_entry_x = 0; }   /* stage 6: a door's start (2.5) */
    if (gate_open) player.cam_max_x = 3328;
    if (stage_index == 1) player.cam_max_x = 32256;         /* stage 2: open (2.3) */
    if (stage_index == 2) player.cam_max_x = 5760;          /* stage 3: the arena (stage3.md 2.3) */
    if (stage_index == 3) stage4_camera();                  /* stage 4: limits by checkpoint and progress (stage4.md 2.3) */
    if (stage_index == 4) stage5_camera();                  /* stage 5 (stage5.md 2.3) */
    if (stage_index == 5) stage6_camera();                  /* stage 6: the area's window (stage6.md 2.3) */
    if (player.cam_x < r[3]) player.cam_x = r[3];
    hud.min = r[2] >> 8;
    hud.sec = r[2] & 0xFF;
    hud.div = 0;
    hud.hp = hud.max_hp;
    player.hp_cache = hud.hp;
    magic.hold = 0;
    magic.meter = 0;
    hud_dirty |= HUD_VITAL | HUD_TIME | HUD_BAR;
    shot_init(magic.level);
    spell_init();
    if (resume)
    {
        chase_vx = resume << 8;
        player.cam_x = player.cam_min_x = player.cam_max_x = 1536;
        player.cam_y = 0;
        stage_chase(TRUE);                  /* the road's columns from the start */
        sound_post(SND_MUSIC_CHASE);
        sound_post(0x01FA);
    }
    if (booted) stage_restart((resume ? (s16)resume : player.cam_x) + 32, 272 - player.cam_y);
    booted = 1;
    spawn_objects();
}

/* Stage 5's sprite line 0: the chosen form's colours (stage5.md 1.3);
 * stage 6's: Madmartigan's in areas 0-4, Willow's after (stage6.md 1.6). */
u16 s5_sprpal_live[32];      /* stage5_data.sprpal */
u16 s6_sprpal_live[32];      /* stage6_data[].sprpal */
void game_form_palette(void)
{
    extern const u16 s5_sprpal[], s6_sprpal[], willow_pal[], mad_pal[];
    if (stage_index == 5)
    {
        memcpy(s6_sprpal_live, s6_sprpal, sizeof s6_sprpal_live);
        memcpy(s6_sprpal_live, area6 < 5 ? mad_pal : willow_pal, 32);
        return;
    }
    memcpy(s5_sprpal_live, s5_sprpal, sizeof s5_sprpal_live);
    memcpy(s5_sprpal_live, stage5_form ? mad_pal : willow_pal, 32);
}

/* The next stage (stage2.md 1.1): score, lives, gold, items and magic stay;
 * checkpoint 0, the clock and full energy as a stage start. */
void game_next_stage(void)
{
    stage_index++;
    if (stage_index == 1 && bank_s6) bank_stage6(FALSE);   /* stage 2-5's data back in slots 6-7 (stage 1's boss body was in banks 8-9) */
    checkpoint = 0;
    gate5_open = 0;
    gate_open = 0;
    chest_broken = 0;
    shop_used = 0;
    leap_done = 0;
    { extern u8 chase_armed; chase_armed = 0; }
    rank = (rank >> 1) + (stage_index == 1 ? 0x800 : stage_index == 2 ? 0x1000 : 0x1800);   /* 0x38146: excess halved, + 0x3819A[stage] (approximate) */
    if (stage_index == 5) { area6 = 0; stage6_select(); }   /* the SSF2 banks in: stage 6's data (src/bank.c) */
    else sd = stage_index == 1 ? &stage2_data : stage_index == 2 ? &stage3_data : stage_index == 3 ? &stage4_data : &stage5_data;
    terrain_changed();
    if (stage_index >= 4) game_form_palette();
    families_stage(stage_index);
    hud_stage(stage_index);
    booted = 0;                             /* stage_init below streams it in; stage_start must not restart */
#ifdef S2_CP
    if (stage_index == 1) checkpoint = S2_CP;   /* test builds */
#endif
#ifdef S3_CP
    if (stage_index == 2) checkpoint = S3_CP;
#endif
#ifdef S4_CP
    if (stage_index == 3) checkpoint = S4_CP;
#endif
#ifdef S5_CP
    if (stage_index == 4) checkpoint = S5_CP;
#endif
#ifdef S6_CP
    if (stage_index == 5) checkpoint = S6_CP;
#endif
#ifdef S6_AREA
    if (stage_index == 5) { area6 = S6_AREA; stage6_select(); game_form_palette(); }   /* test builds */
#endif
    if (fe.debug && stage_index == fe.debug_stage)
    {
        /* The DEBUG page's start point: a checkpoint, or stage 6's area. */
        if (stage_index == 5) { if (fe.debug_cp) { area6 = fe.debug_cp; stage6_select(); game_form_palette(); } }
        else checkpoint = fe.debug_cp;
    }
#ifndef S2_FORM
#define S2_FORM 1                           /* test builds: 0 plays stage 2 as Willow */
#endif
    player_form(stage_index == 1 ? S2_FORM : stage_index == 3 ? 1 : stage_index == 4 ? stage5_form : stage_index == 5 ? area6 < 5 : 0);   /* Madmartigan (0x2F72: stages 2, 4, 6); stage 5: chosen (stage5.md 1.3) */
    if (frontend_settings()->magic && magic.level < 1) { magic.level = hud.magic = 1; }   /* magic continue (2E1C): stage index 1 -> magic >= 1 */
    if (stage_index == 1) player_init(cp_rec2[checkpoint][0], cp_rec2[checkpoint][1]);
    else if (stage_index == 2) player_init(cp_rec3[checkpoint][0], cp_rec3[checkpoint][1]);
    else if (stage_index == 3) player_init(cp_rec4[checkpoint][0], cp_rec4[checkpoint][1]);
    else if (stage_index == 4) player_init(cp_rec5[checkpoint][0], cp_rec5[checkpoint][1] + (stage5_form ? 8 : 0));   /* Madmartigan 8 higher (1.6) */
    else player_init(stage6_rec(checkpoint)[0], stage6_rec(checkpoint)[1]);
    if (stage_index == 3) stage4_camera();
    if (stage_index == 4) { stage5_reset(); stage5_camera(); player.cam_min_x = cp_rec5[checkpoint][3]; }
    if (stage_index == 5) { stage6_camera(); player.cam_min_x = stage6_rec(checkpoint)[3]; }
    stage_init(player.cam_x + 32, 272 - player.cam_y);
    stage_start();
    game_mode = GM_PLAY;
}

/* Stage 1's data, art and Willow again (a new game). */
void game_stage_reset(void)
{
    if (stage_index || sd != stage_data)
    {
        sd = stage_data;
        families_stage(0);
        hud_stage(0);
    }
    stage_index = 0;
    gate5_open = 0;
    terrain_changed();
    if (!bank_s6) bank_stage6(TRUE);       /* banks 8-9 in for stage 1: its boss body lives there (src/data.s); stage 1 uses nothing else from 0x300000 up */
    stage6_new_game();
    player_form(0);
    { extern u8 chase_armed; chase_armed = 0; }
}

void game_new(void)
{
    game_stage_reset();
    sword_level = 0;                        /* hud.md 5: sword level 0 */
    stage_index = 0;
    {
        /* The mode's settings (src/frontend.c; the arcade's DIP effects,
         * hud.md 5): lives, vitality, the difficulty's starting rank. */
        const Settings *s = frontend_settings();
        lives_dip = s->lives;               /* BCD, 1..4 */
        hud.max_hp = s->vitality;
        start_rank = (u16)s->difficulty << 12;   /* 0x1B3E: rank high byte = difficulty x 16 (default 3: 0x3000) */
    }
    hud.hi = frontend_best();
    coop_reset();                           /* co-op: one player until pad 2 joins */
    if (fe.mode == FE_HOME) home_credits = frontend_credits();
    hud.score = 0;
    hud.lives = lives_dip;
    hud.hp = hud.max_hp;
    hud.gold = 0;
#ifdef TEST_GOLD
    hud.gold = TEST_GOLD;               /* test builds: money for the shop */
#endif
#ifdef TEST_MAGIC
    magic.level = hud.magic = TEST_MAGIC;   /* test builds: the charged spells */
#endif
    checkpoint = 0;
    gate_open = 0;
    chest_broken = 0;
    { extern u8 hidden_found; hidden_found = 0; }   /* FF83A5: once per game (the new-game clear 0x2D18) */
    reward_set = (reward_set + 1) & 3;
    boss_new_game();
    boss3_new_game();
    shop_new_game();
    rank = start_rank;
    fe.continued = 0;
    continues = 0;
    hud_dirty = 0xFF;
#ifdef START_CP
    /* Test builds: start at a checkpoint past the waterfall gate. */
    checkpoint = START_CP;
    gate_open = 1;
    booted = 1;
#endif
    stage_start();
    game_mode = GM_PLAY;
}

/* Life loss (96BE): the restart point follows the last safe position. */
static u16 fade_pal[64];

/* The sprite table emptied (one entry, off screen): the blocking fade-in
 * after a restart would otherwise show the old sprites over reused VRAM. */
static void sat_blank(void)
{
    u16 ints = SYS_getAndSetInterruptMaskLevel(7);
    *(vu32 *)VDP_CTRL_PORT = 0x40000000 | ((u32)(0xAC00 & 0x3FFF) << 16) | (0xAC00 >> 14);
    *(vu32 *)VDP_DATA_PORT = 0;
    *(vu32 *)VDP_DATA_PORT = 0;
    SYS_setInterruptMaskLevel(ints);
}

/* The stage's music (play set-up; restarts; after the shop): stage 3 by
 * checkpoint (stage3.md 1.4: word 4, the lake 5, the boss 6). */
void game_stage_music(void)
{
    if (stage_index == 0) { sound_post(SND_MUSIC_STAGE1); return; }
    if (stage_index == 1) { sound_post(SND_MUSIC_STAGE2); sound_post(0x01FA); return; }
    if (stage_index == 3) { sound_post(checkpoint >= 6 && checkpoint <= 8 ? SND_MUSIC_CAVE : SND_MUSIC_STAGE4); sound_post(0x01FA); return; }   /* stage4.md 1.5 */
    if (stage_index == 5) { sound_post(stage6_music()); sound_post(0x01FA); return; }   /* stage6.md 1.5 */
    if (stage_index == 4) { sound_post(checkpoint == 18 ? SND_MUSIC_BOSS5 : SND_MUSIC_STAGE5); sound_post(0x01FA); return; }   /* stage5.md 1.5 */
    if (checkpoint == 13) { sound_post(SND_MUSIC_BOSS3); return; }
    sound_post(checkpoint >= 7 ? SND_MUSIC_LAKE : SND_MUSIC_STAGE3);
    sound_post(0x01FA);
}

/* The attract's demo play (arcade_frontend.md 3.2, set-up 0x8CDA): a new
 * game, then demo 0: stage 1's boss arena (checkpoint 6, gate open, boss
 * seen, magic 1, 1:00); demo 1: stage 2's chase at full speed as
 * Madmartigan (3:00). Energy 5 of 5. */
static void game_reload(void);

/* Bavmorda's picture after Kael (stage6.md 2.5, door variant 10): message
 * 0x48 typed over it, 256 updates; A or B speeds it up (799C). */
static void story6_page(void)
{
    static const char *const text[3] = { "I WILL NOW SEND ELORA TO THE WORLD OF", "DARKNESS. HA, HA, HA,...", "THEN THE WORLD WILL BE MINE." };
    u16 t, line = 0, at = 0, wait = 0;
    PAL_fadeOutAll(30, FALSE);
    sat_blank();
    actor_clear();                          /* the gold left on the bridge (the area changes after) */
    stage_bpicture(story6_map, 16);
    VDP_clearPlane(BG_A, TRUE);
    game_hide_sprites = 1;
    load_packed(story6_tiles, 16, STORY6_TILES);
    VDP_setBackgroundColor(0);
    memcpy(fade_pal, story6_pal, sizeof fade_pal);
    SYS_doVBlankProcess();
    PAL_fadeInAll(fade_pal, 30, FALSE);
    for (t = 0; t < 256; t++)
    {
        u16 n = (JOY_readJoypad(JOY_1) & (BUTTON_A | BUTTON_B)) ? 4 : 1;
        while (n--)
        {
            if (line < 3 && ++wait >= 3)
            {
                wait = 0;
                at++;
                hud_text(text[line], at, 1, 22 + line);
                if (text[line][at - 1] != ' ') sound_post(SND_TEXT);
                if (!text[line][at]) { line++; at = 0; }
            }
            if (n) t++;
        }
        SYS_doVBlankProcess();
    }
    PAL_fadeOutAll(30, FALSE);
    {
        static const char blank[] = "                                        ";
        for (t = 0; t < 3; t++) hud_text(blank, 39, 1, 22 + t);
    }
    game_hide_sprites = 0;
}

void game_demo_setup(u16 demo)
{
    extern u8 chase_armed;
    extern void boss_seen_set(void);
    game_new();
    if (demo)
    {
        game_next_stage();
        chase_armed = 1;
        chase_vx = (s32)0x10C0 << 8;
    }
    else
    {
        checkpoint = 6;
        gate_open = 1;
        boss_seen_set();
        magic.level = hud.magic = 1;
    }
    hud.max_hp = 5;
    PAL_fadeOutAll(16, FALSE);
    game_reload();
    hud.hp = hud.max_hp;
    hud.min = demo ? 3 : 1;
    hud.sec = 0;
    hud_dirty = 0xFF;
    if (!demo) sound_post(SND_MUSIC_BOSS);  /* music word 1 */
    if (!frontend_settings()->demo) sound_post(SND_STOP);   /* demo sound off (DIP) */
    game_mode = GM_PLAY;
}

/* After a blocking screen (world map, continue): the stage's art and
 * planes again, play from the checkpoint, the stage music. */
static void game_reload(void);

/* Stage 6's area change by a door (stage6.md 2.5): the music fades, the
 * screen fades; the new area's scenery set, objects and checkpoint 0; the
 * player at the door's start position (variant 10 also makes Madmartigan
 * Willow, 1.6); the clock and music of the area's record. */
static void game_area6(u16 v)
{
    extern u8 door6_go, gate_freeze;
    extern u16 door6_take(u16 v, s16 *x, s16 *y);
    extern s16 s6_entry_x, s6_entry_y;
    door6_go = 0;
    gate_freeze = 0;
    sound_post(0x10FB);
    if (v == 10) story6_page();
    else PAL_fadeOutAll(30, FALSE);
    area6 = door6_take(v, &s6_entry_x, &s6_entry_y);
    stage6_select();
    checkpoint = 0;
    if (v == 10) { player_form(0); families_stage(5); }
    game_form_palette();
    player_init(s6_entry_x, s6_entry_y);    /* the camera inside the new area before its planes are drawn */
    stage6_camera();
    game_reload();
}

static void game_reload(void)
{
    sat_blank();
    PAL_setColors(0, palette_black, 64, CPU);
    posecache_flush();
    VDP_setBackgroundColor(32);             /* a blanked display shows the backdrop entry: scenery line 2 entry 0, black in every stage */
    VDP_setEnable(FALSE);                   /* the stage loads unseen (stage_init sets its colours at once) */
    stage_init(player.cam_x + 32, 272 - player.cam_y);
    { extern u8 hud_keep_backdrop; hud_keep_backdrop = 1; hud_init(HUD_VRAM); hud_keep_backdrop = 0; }   /* the backdrop stays black until the display is on */
    booted = 1;
    stage_start();                          /* restarts at the checkpoint (chase resume included) */
    shot_init(magic.level);
    hud_dirty = 0xFF;
    hud_update();
    if (!chase_vx) game_stage_music();
    memcpy(fade_pal, stage_sprpal(), 32 * 2);         /* (co-op: line 0 shared by both heroes) */
    memcpy(fade_pal + 32, sd->pal, 32 * 2);
    stage_cycle_pal(fade_pal + 32);                   /* past the cycle's start: its pens, not the first part's */
    DMA_flushQueue();                       /* the queued colour writes land now, then black again */
    PAL_setColors(0, palette_black, 64, CPU);
    VDP_setBackgroundColor(sd->sky);
    VDP_setEnable(TRUE);
    {
        /* The planes as the first frame of play lays them, with their split
         * scroll values, landed before the colours fade in. */
        u16 i;
        for (i = 0; i < 16; i++)            /* the far ring moves one row a frame */
        {
            stage_scroll(player.cam_x + 32, 272 - player.cam_y);
            stage_commit();
            SYS_doVBlankProcess();
        }
    }
    PAL_fadeInAll(fade_pal, 30, FALSE);
}

/* Life loss (96BE, specs/arcade_frontend.md 6): the world map, then the
 * stage again at its checkpoint (no story). Lives out: the score's
 * ranking and name entry, then the continue screen (7); a continue resets
 * lives, score and energy (2CEE), magic, gold and items stay. */
static void life_lost(void)
{
    u16 c = checkpoint_of(player.safe_x, player.safe_y);
    if (c > checkpoint) checkpoint = c;
    rank = rank >= 0x0D00 ? rank - 0x0D00 : 0;
    if (hud.lives) hud.lives = (hud.lives & 15) ? hud.lives - 1 : hud.lives - 7;
    sound_post(SND_STOP);
    PAL_fadeOutAll(30, FALSE);
    sat_blank();
    hud_message(NULL, 10); hud_message(NULL, 12);
    if (!hud.lives)
    {
        scores_screens();
        if (!continue_screen()) { game_mode = GM_ENDED; return; }
        hud.lives = lives_dip;
        hud.score = 0;
        hud.hp = hud.max_hp;
        if (continues < 255) continues++;
    }
    intro_world_map(stage_index);
    game_reload();
    game_mode = GM_PLAY;
}

static void flash(void)
{
    /* Hurt flash (combat.md 2.3): Willow's line swaps to the flash colours
     * every 4 updates while the hurt timer runs. The tables are the arcade's
     * per pen, in line 0's slots shared by both heroes (game/coop/tools/
     * line0.py; a slot holding two of a hero's pens takes the first one's;
     * 0: a slot the hero does not use). In co-op both heroes share line 0:
     * the hurt player blinks instead (src/main.c, src/coop.c). */
    static const u16 flash_pal[16] = {
        0x022E, 0x0000, 0x000E, 0x022E, 0x0000, 0x04AE, 0x0000, 0x04AE,
        0x04AE, 0x0AEE, 0x0000, 0x06EE, 0x06EE, 0x0AEE, 0x0AAE, 0x0000
    };
    static const u16 mad_flash[16] = {      /* 0xE5020 (combat.md 2.3: Willow's +0x20) */
        0x0000, 0x006E, 0x000E, 0x0000, 0x048E, 0x0000, 0x048E, 0x0000,
        0x00CE, 0x0000, 0x06EE, 0x0000, 0x08EE, 0x0AEE, 0x0CEE, 0x0000
    };
    static u8 shown;
    u8 want = player.hurt_timer && !(((128 - player.hurt_timer) >> 2) & 1) && !two_players;
    if (want == shown) return;
    shown = want;
    PAL_setColors(1, want ? (player.form ? mad_flash : flash_pal) : stage_sprpal() + 1, 15, DMA_QUEUE);
}

/* ---- Stage clear (top mode 4, A5E4; specs/stage1_boss.md 6.1, 6.2). */
#define SND_CLEAR 0x10
#define SND_COUNT 0x30
#define SND_TYPE 0x2F
enum { CL_WALK, CL_RESULTS, CL_COUNT, CL_HOLD, CL_STORY, CL_STORY_HOLD };
static u8 clear_step;
static u32 bonus_time, bonus_life, count_time, count_life;
static u16 story_at;

void game_stage_clear(void)
{
    sound_post(SND_CLEAR);
    shot_init(magic.level);             /* player shots cleared (2884) */
    game_mode = GM_CLEAR;
    clear_step = CL_WALK;
    mode_timer = stage_index == 4 ? 1 : 94;     /* stage 5: results at once (FF83C3 = 2, no exit timer) */
}

/* Play begins (intro.md 7): Willow turns right and waits 16 updates. */
static u8 gate_timer;

void game_play_start(void)
{
    game_mode = GM_PLAY;
    if (player.script == 22) player.script = 0;   /* stage 4: the walk-in's stand */
    player_stand(FALSE);
    gate_timer = 16;
}

bool game_player_runs(u16 *joy)
{
    if (game_mode == GM_INTRO && player.script == 22) { *joy = 0; return TRUE; }   /* stage 4's walk-in (stage4.md 1.3) */
    if (game_mode == GM_PLAY)
    {
        if (gate_timer) { gate_timer--; return FALSE; }
        if (gate_freeze) return FALSE;      /* stage 5's doorway wipe (FF83EA) */
        return TRUE;
    }
    if (game_mode == GM_CLEAR && clear_step == CL_WALK)
    {
        *joy = BUTTON_RIGHT;            /* scripted input (stream 0x47E8E): walk out */
        return TRUE;
    }
    return FALSE;
}

static void put_number(char *t, u32 v, u16 width)
{
    char d[12];
    u16 n = 0, i;
    do { d[n++] = '0' + v % 10; v /= 10; } while (v);
    for (i = 0; i < width; i++) t[i] = i + n >= width ? d[width - 1 - i] : ' ';
    t[width] = 0;
}

static void results_rows(void)
{
    char t[40], *p;
    memcpy(t, "TIME     X100 = ", 16);
    t[10] = '1' + stage_index;
    put_number(t + 5, bonus_time / (100 * (stage_index + 1)), 3); t[8] = ' ';
    p = t + 16; put_number(p, count_time, 7);
    hud_text(t, 40, 7, 12);
    memcpy(t, "LIFE     X100 = ", 16);
    t[10] = '1' + stage_index;
    put_number(t + 5, bonus_life / (100 * (stage_index + 1)), 3); t[8] = ' ';
    p = t + 16; put_number(p, count_life, 7);
    hud_text(t, 40, 7, 15);
}

/* Story pages (rows 22..): stage 1 message 39; stage 2 messages 0x2D and
 * 0x2E (Cherlindrea, stage2.md 7.1; the second is one 61-character line in
 * the arcade's 48 columns, broken at a word here). NULL ends a page. */
static const char *const story_text[5][8] = {
    { "THANK YOU VERY MUCH FOR SAVING ME.", "TO SHOW MY APPRECIATION I WILL JOIN", "YOUR FIGHT.", NULL },
    { "I AM A SPIRIT OF THE FOREST.", "THE CHILD YOU ARE SEARCHING FOR IS THE", "QUEEN TO BE OF THE KINGDOM OF PEACE,",
      "ELORA DANAN.", NULL, "IF YOU WANT TO KNOW ABOUT ELORA, GO SEE", "THE WITCH FIN RAZIEL.", NULL },
    /* Stage 3 (stage3.md 7.1): Fin Raziel, messages 0x2F, 0x40, 0x41. */
    { "I AM RAZIEL.  ELORA WAS BORN TO DESTROY", "BAVMORDA.", NULL, "YOU CANNOT HAND ELORA OVER TO BAVMORDA.",
      "HURRY AND SAVE HER.", NULL, "I WILL GIVE YOU ALL MORE STRENGTH.", NULL },
    /* Stage 4 (stage4.md 7.1): messages 0x44, 0x45. */
    { "UNFORTUNATELY, THE KINGDOM OF GALLADOORN", "THAT I WAS PROTECTING WAS DEFEATED BY", "KAEL.", NULL,
      "YOU GUYS ARE OUR ONLY HOPE.", "WE ARE COUNTING ON YOU.", NULL, NULL },
    /* Stage 5 (stage5.md 7.1): Kael, message 0x46. */
    { "IT IS AMAZING HOW ALL OF YOU WERE ABLE", "TO COME THIS FAR WITH YOUR POWERS.", "HOWEVER, YOUR LIFE ENDS HERE.", NULL },
};
static u8 story_line, story_row;

static void clear_update(void)
{
    hud.clock_lock = 1;
    switch (clear_step)
    {
    case CL_WALK:                       /* Willow walks out, the game runs on */
        actors_update();
        if (mode_timer) mode_timer--;
        if (mode_timer || clear_wait) return;   /* stage 3: the ending figure (stage3.md 6.13) */
        {
            u16 s = ((hud.min >> 4) * 10 + (hud.min & 15)) * 60 + (hud.sec >> 4) * 10 + (hud.sec & 15);
            u16 k = 100 * (stage_index + 1);            /* stage2.md 7.1: x200; stage3.md 7.1: x300 */
            bonus_time = (u32)s * k;
            bonus_life = (u32)(8 * (hud.hp + hud.armor) & 0xFF) * k;
        }
        count_time = count_life = 0;
        { char t[7] = "STAGE1"; t[5] += stage_index; hud_message(t, 6); }
        hud_message("CLEAR POINTS", 9);
        results_rows();
        clear_step = CL_RESULTS;
        mode_timer = 3;
        return;
    case CL_RESULTS:
        if (--mode_timer) return;
        clear_step = CL_COUNT;
        return;
    case CL_COUNT:                      /* +100 an update each, sound 0x30 */
        sound_post(SND_COUNT);
        if (count_time < bonus_time) count_time += 100;
        if (count_life < bonus_life) count_life += 100;
        results_rows();
        if (count_time < bonus_time || count_life < bonus_life) return;
        {
            /* The total once into the score (6284), as BCD. */
            u32 v = bonus_time + bonus_life, bcd = 0;
            u16 sh = 0;
            while (v && sh < 32) { bcd |= (v % 10) << sh; v /= 10; sh += 4; }
            add_score(bcd);
        }
        clear_step = CL_HOLD;
        mode_timer = 120;
        return;
    case CL_HOLD:
        if (--mode_timer) return;
        PAL_getColors(0, fade_pal, 64);
        PAL_fadeOutAll(30, FALSE);
        /* The story page (6.1 steps 14-18): Madmartigan's portrait (plane
         * B picture, tools/intropack.py) and the text typed over its foot. */
        stage_bpicture(stage_index == 4 ? story5_map : stage_index == 3 ? story4_map : stage_index == 2 ? story3_map : stage_index ? story2_map : story_map, 16);   /* a still picture on plane B, under the HUD (first: the scroll splits stop) */
        VDP_clearPlane(BG_A, TRUE);
        game_hide_sprites = 1;
        if (stage_index == 4) load_packed(story5_tiles, 16, STORY5_TILES);
        else if (stage_index == 3) load_packed(story4_tiles, 16, STORY4_TILES);   /* aPLib (tools/pack_compress.sh) */
        else if (stage_index == 2) load_packed(story3_tiles, 16, STORY3_TILES);
        else if (stage_index) load_packed(story2_tiles, 16, STORY2_TILES);
        else load_packed(story_tiles, 16, STORY_TILES);
        VDP_setBackgroundColor(0);
        memcpy(fade_pal, stage_index == 4 ? story5_pal : stage_index == 3 ? story4_pal : stage_index == 2 ? story3_pal : stage_index ? story2_pal : story_pal, sizeof fade_pal);
        story_line = story_row = 0;
        SYS_doVBlankProcess();
        PAL_fadeInAll(fade_pal, 30, FALSE);
        clear_step = CL_STORY;
        mode_timer = 60;
        story_at = 0;
        return;
    case CL_STORY:                      /* 60 updates, then typed, 3 a character */
        if (--mode_timer) return;
        mode_timer = 3;
        {
            const char *t = story_text[stage_index][story_line];
            u16 k = story_at + 1;
            hud_text(t, k, 1, 22 + story_row);
            story_at = k;
            if (t[k]) return;
            story_at = 0;
            story_line++;
            story_row++;
            if (story_text[stage_index][story_line]) return;
            /* The page is done: the last one holds, an earlier one is cleared
             * after 60 updates (step 18) and the next one typed. */
            clear_step = CL_STORY_HOLD;
            mode_timer = story_line + 1 < 8 && story_text[stage_index][story_line + 1] ? 60 : 240;
        }
        return;
    case CL_STORY_HOLD:
        if (--mode_timer) return;
        if (story_line + 1 < 8 && story_text[stage_index][story_line + 1])
        {
            static const char blank[] = "                                        ";
            u16 r;
            for (r = 0; r < story_row; r++) hud_text(blank, 39, 1, 22 + r);
            story_line++;
            story_row = 0;
            clear_step = CL_STORY;
            mode_timer = 60;
            return;
        }
        PAL_fadeOutAll(30, FALSE);
        game_hide_sprites = 0;
        if (stage_index < 5) stage_enter_next();            /* stage2.md 1.1 .. stage6.md 1.1 */
        else game_mode = GM_ENDED;
        return;
    }
}

u16 game_pressed;            /* buttons newly pressed this update (the companion's throws) */

void game_update(u16 held)
{
    static u16 last_held;
    u16 pressed = held & ~last_held;
    last_held = held;
    game_pressed = pressed;
    switch (game_mode)
    {
    case GM_PLAY:
        hud.clock_lock = scene_lock || time_stop || (stage_index == 4 && boss5_dead);   /* spell 10 stops the clock (FF83D2); stage 5: the boss's end (FF83E9) */
        {
            /* Rank growth by the Nando speed setting (0x380E8, DSW-B 3-4):
             * slow +1 every other update, normal +1, fast +2, very fast +3. */
            static const u8 grow[4] = { 1, 1, 2, 3 };
            u16 nd = frontend_settings()->nando, g = nd == 0 && (game_frame & 1) ? 0 : grow[nd & 3];
            rank = rank > 0xFFFF - g ? 0xFFFF : rank + g;
        }
        stage_flags();
        events_update();
#ifdef SECS_NOINT
        __asm__ volatile ("move.w #0x2700,%sr");    /* measurement builds: the section without interrupts */
#endif
        prof[6] = *(vu16 *)0xC00008;
#ifndef PROF_NOUPD
        actors_update();
#endif
        prof[7] = *(vu16 *)0xC00008;
#ifdef SECS_NOINT
        __asm__ volatile ("move.w #0x2000,%sr");
#endif
#ifndef PROF_NOCOLL
        collisions();                       /* (co-op: the second player's in coop_frame) */
#endif
        flash();
        if (shop_safe && !--shop_safe && !player.hurt_timer && !player.dead) player.vuln = 1;   /* post-shop safety (0x965E) */
        if (player.dead == 2) { magic.hold = 0; if (!coop_life_lost()) life_lost(); break; }
        { extern u8 door6_go; if (door6_go) { game_area6(door6_go - 1); break; } }
        { extern u8 bav_end; extern void ending_run(void); if (bav_end) { bav_end = 0; ending_run(); break; } }   /* stage6.md 7 */
#ifdef SHOP_AT
        { extern void shop_enter(u16 index); if (game_frame == SHOP_AT) { shop_enter(0); break; } }   /* test builds: the shop opened at a set frame */
#endif
#ifdef TEST_CLEAR_AT
        { static u16 tca; if (++tca == TEST_CLEAR_AT) { tca = 0; game_stage_clear(); break; } }   /* test builds: a forced clear TEST_CLEAR_AT updates into each stage's play */
#endif
        if (!hud.min && !hud.sec && !hud.clock_lock)        /* time-out (956E) */
        {
            hud_message("TIME OVER", 12);
            sound_post(SND_MUSIC_OVER);
            game_mode = GM_TIME_OVER;
            mode_timer = 80;
        }
        break;
    case GM_TIME_OVER:
        hud.clock_lock = 1;
        if (!--mode_timer) { hud_message(NULL, 12); coop_both_down(); life_lost(); }
        break;
    case GM_CLEAR:
        clear_update();
        break;
    case GM_INTRO:
        intro_update(held);
        break;
    case GM_SHOP:
        if (shop_update(held, pressed)) game_mode = GM_PLAY;
        break;
    case GM_GAME_OVER:
        if (!--mode_timer) { hud_message(NULL, 12); game_mode = GM_ENDED; }   /* the run's end: scores, then the menu or the attract */
        break;
    }
}
