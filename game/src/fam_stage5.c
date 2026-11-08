/* Stage 5's families and rules (specs/stage5.md): the camera (one-way over
 * the gap, the arena), the events (troll windows, the bat flock, the
 * spearman balconies and the corridor entrant, the arena's troll wave), the
 * trolls and their orbs, the stone pillars over the gap and the corridor
 * doorways. Native y = arcade y (world_top 767).
 *
 * Approximations (M1_NOTES): the troll's phase timings and ranges (walk,
 * leap at close range, throw at long range, from the timers the spec gives;
 * no grapple), the corridor entrant's x (the arcade reads camera Y there,
 * spec 4.3: here the right edge), the pillar debris (bursts), the gate and
 * the wall torches (not in this build). */
#include <genesis.h>
#include "actor.h"
#include "sprite.h"
#include "stage.h"
#include "player.h"
#include "sound.h"
#include "hud.h"
#include "game.h"
#include "shot.h"
#include "dirmath.h"
#include "spell.h"
#include "s5spr.h"

#define FACE(a) ((a)->facing & 1)
#define DIR(a) (FACE(a) ? -1 : 1)
#define SND_HIT 0x32
#define SND_BOOM 0x33
#define TAG(a) ((a)->ng >> 1)       /* the event that spawned it (1 + event), 0: placed (the spearman's pad bytes are its own) */

u8 boss5_wake;              /* FF83EE: the arena's trolls are gone */
u8 boss5_dead;              /* FF83CF */
static u8 arena_done;       /* FF83A6: the arena event ran (a restart skips the wave) */
static u8 arena_left;       /* FF0F87 */
static u8 trolls_out;       /* FF0F5C: window trolls alive */
static u8 window_out[7];    /* FF0F80[var] */
static u8 bats_placed, arena_on, locked;
static u8 nroost;                   /* hanging bats (roost_update) */

static void chain(Actor *a, u16 c) { actor_anim(a, c + FACE(a)); }
static void face_player(Actor *a) { a->facing = (player.x >> 8) < AX(a); }
static bool off_cam(Actor *a)
{
    return (u16)(AX(a) - CAM_X + 64) > 512 || (u16)(AY(a) - CAM_Y + 96) > 416;
}

/* ---- Camera (2.3). */
void stage5_camera(void)
{
    player.cam_min_y = 0;
    player.cam_max_y = 2048;
    player.cam_max_x = 5760;
    if (checkpoint == 18) player.cam_min_x = 5376;
}

/* The interior's wall torches (class 12 kind 6 variant 5, 0x333FA; 5.7):
 * eleven animators; while one is within camera x-16..x+400, y-16..y+272 it
 * counts 7 updates and writes the next of three frames (0, 1, 2, from the
 * first write) into its 2 x 2 16-px cells (4 x 4 plane-A cells; tools/
 * torchpack5.py, tiles at TORCH5_VRAM: the scenery ends at 963). A list
 * here, not actors (11 actors cost ~9 scanlines an update at checkpoint 11). */
#include "../data/stage5/torch5.h"
#define TORCH5_VRAM 963
#define TORCHES5 11
extern const u16 torch5_words[];
extern const u8 torch5_tiles[];
extern u8 torch5_loaded;
static const s16 torch5_at[TORCHES5][2] = {
    { 4432, 592 }, { 4528, 592 }, { 4576, 592 }, { 4720, 528 }, { 4656, 480 }, { 4368, 368 },
    { 4512, 304 }, { 4560, 208 }, { 4432, 96 }, { 4560, 96 }, { 4688, 96 } };   /* src/stage5_objects.h */
static u8 torch5_timer[TORCHES5], torch5_frame[TORCHES5], torch5_pri_ok[TORCHES5];
static u16 torch5_pri[TORCHES5];

static void torches5_reset(void)
{
    u16 i;
    for (i = 0; i < TORCHES5; i++) { torch5_timer[i] = 7; torch5_frame[i] = 0; }
}

static void torches5_update(void)
{
    u16 i;
    for (i = 0; i < TORCHES5; i++)
    {
        s16 c, r;
        if ((u16)(torch5_at[i][0] - CAM_X + 16) > 416 || (u16)(torch5_at[i][1] - CAM_Y + 16) > 288) continue;
        if (--torch5_timer[i]) continue;
        torch5_timer[i] = 7;
        c = (torch5_at[i][0] >> 4) * 2; r = ((767 - torch5_at[i][1]) >> 4) * 2;
        if (!torch5_loaded) { VDP_loadTileData((const u32 *)torch5_tiles, TORCH5_VRAM, TORCH5_TILES, DMA_QUEUE); torch5_loaded = 1; }
        if (!torch5_pri_ok[i]) { torch5_pri[i] = stage_cells_pri(c, r, 4, 4); torch5_pri_ok[i] = 1; }   /* the map's priority bits, read once */
        stage_cells_put_pri(c, r, torch5_words + torch5_frame[i] * 16, 4, 4, TORCH5_VRAM, torch5_pri[i]);
        if (++torch5_frame[i] > 2) torch5_frame[i] = 0;
    }
}

#ifdef DBG_FLOORSCAN
s16 dbg_floor[2][320];                 /* measurement: per x 1000..1319 (step 1), the first two floor tops below y 420 */
static void floor_scan(void)
{
    u16 i;
    for (i = 0; i < 320; i++)
    {
        s16 x = 1000 + i, y, top, n = 0;
        dbg_floor[0][i] = dbg_floor[1][i] = 0;
        for (y = 300; y > 60 && n < 2; y -= 4)
            if (terrain_floor(x, y, &top)) { dbg_floor[n][i] = top; n++; y = top - 20; }
    }
}
#endif

void stage5_reset(void)
{
    torches5_reset();
    trolls_out = arena_left = 0;
    gate_freeze = 0;
    gate5_open = checkpoint >= 5;
    terrain_changed();               /* restarts past the doorway find it open */
    memset(window_out, 0, sizeof window_out);
    bats_placed = arena_on = locked = 0;
    nroost = 0;
    boss5_wake = boss5_dead = 0;
    encounter_clear = 0;
    midboss_active = 0;
    if (checkpoint < 18) arena_done = 0;
}

void stage5_new_game(void) { arena_done = 0; }

/* ---- Events (3.2). Spawners: the spearman balconies (2-5, variant 14),
 * the corridor entrant (1, variant 16), the troll windows (6-12), the bat
 * flock (0), the arena (13). */
typedef struct { s16 x, y, sx, sy; } Spot;
static const Spot balcony[4] = { { 4496, 560, 4496, 560 }, { 4640, 560, 4640, 560 }, { 4784, 496, 4784, 496 }, { 4784, 384, 4784, 384 } };
static const Spot window5[7] = {        /* +06 / +08 the test point, +0A / +0C the window */
    { 904, 112, 904, 192 }, { 1392, 192, 1416, 240 }, { 1072, 368, 920, 384 }, { 1336, 432, 1336, 352 },
    { 1808, 512, 1848, 432 }, { 1928, 512, 1928, 432 }, { 2048, 512, 2008, 432 } };
static const u16 window_box[7][4] = {
    { 144, 288, 64, 128 }, { 120, 240, 64, 128 }, { 96, 192, 128, 256 }, { 144, 288, 64, 128 },
    { 80, 160, 64, 128 }, { 48, 64, 64, 128 }, { 80, 96, 64, 128 } };
static const u8 window_cap[16] = { 1, 1, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5 };
static const u8 window_next[32] = { 176, 240, 255, 192, 192, 224, 223, 176, 192, 224, 207, 160, 160, 208, 192, 160,
                                    160, 208, 192, 160, 144, 192, 184, 144, 152, 192, 176, 144, 136, 192, 160, 128 };
static const u8 spear_wait[16] = { 240, 240, 240, 240, 240, 240, 210, 210, 210, 180, 150, 120, 90, 60, 60, 30 };
static const s16 bat_spots[33][2] = {
    { 4608, 688 }, { 4640, 688 }, { 4672, 688 }, { 4704, 688 }, { 4736, 608 }, { 4768, 608 }, { 4512, 504 },
    { 4544, 504 }, { 4576, 504 }, { 4608, 504 }, { 4640, 504 }, { 4672, 504 }, { 4608, 408 }, { 4640, 408 },
    { 4672, 408 }, { 4480, 344 }, { 4512, 344 }, { 4544, 344 }, { 4704, 328 }, { 4736, 328 }, { 4768, 328 },
    { 4544, 248 }, { 4576, 248 }, { 4608, 248 }, { 4640, 248 }, { 4448, 120 }, { 4480, 120 }, { 4512, 120 },
    { 4544, 120 }, { 4576, 120 }, { 4608, 120 }, { 4640, 120 }, { 4672, 120 } };
static u16 balcony_timer[4], window_timer[7], entrant_timer;

/* ---- The flock's bats while they hang (4.4): ~20 in view at once, too many
 * for actors; a list drawn from the resident art (src/families.c), shot
 * every 4th update, made actors (F_BAT) when they wake (bat_update's test:
 * within 96 x / 112 y of the player). Hanging bats do not hurt on contact
 * here (they wake first). */
#define ROOST 33
#ifndef BAT_CAP
#define BAT_CAP 6
#endif
static struct { s16 x, y; } roost[ROOST];
extern const s16 s5_bat_boxes[];
extern const PoseSet *bat_poseset(void);

static void roost_update(void)
{
    s16 px = player.x >> 8, py = player.y >> 8;
    u16 i, awake = (game_frame & 15) ? 0xFF : actor_count(F_BAT);
    s16 su[4];
    bool shots_on = shot_bounds(su);        /* the union of what shot_test can hit (exact pre-filter) */
    const s16 *bb = s5_bat_boxes + 4;
    if (awake == 0xFF) awake = BAT_CAP;     /* wakes are tested every 16 updates (actor_count is a scan) */
    for (i = game_frame & 1; i < nroost; )  /* half the list an update */
    {
        s16 x = roost[i].x, y = roost[i].y, dx = px - x, dy = py + 24 - y;
        bool gone = FALSE;
        if ((u16)(dx + 352) > 704) { i += 2; continue; }   /* far off: neither awake nor in view */
        if ((u16)(dx + 96) <= 192 && (u16)(-dy + 24 + 112) <= 224 && awake < BAT_CAP)   /* at most BAT_CAP in flight (the arcade: all that wake) */
        {
            Actor *a = actor_new(F_BAT);
            if (!a) { i += 2; continue; }
            a->x = (s32)x << 8; a->y = (s32)y << 8;
            awake++;
            gone = TRUE;
        }
        else if (shots_on && !((game_frame + i) & 2) && (u16)(x - CAM_X + 16) < 352 && (u16)(y - CAM_Y + 16) < 288 &&
                 x - bb[0] <= su[1] && x - bb[0] + bb[1] >= su[0] && y - bb[2] <= su[3] && y - bb[2] + bb[3] >= su[2] &&
                 shot_test(x, y, bb, FALSE))
        {
            Actor t;                            /* the bat's death as the actor's (stage4.md 5.2: 100, drop set 1) */
            t.x = (s32)x << 8; t.y = (s32)y << 8;
            sound_post(SND_BOOM);
            effect(F_BURST, x, y, 4);
            award(2);
            drop(&t, 1);
            gone = TRUE;
        }
        if (gone) { roost[i] = roost[--nroost]; continue; }
        i += 2;
    }
}

u16 roost_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y)
{
    const PoseSet *ps = bat_poseset();
    u16 i, k = first, pose = (game_frame >> 3) % 6;   /* the hanging loop (chain 0: poses 0-5) */
    const u8 *d = ps->poses + pose * 6;
    const s16 *q = ps->sat + (((d[4] << 8) | d[5]) << 2);
    s16 ox = 128 - view_x, oy = 128 + 511 - view_y;
    for (i = 0; i < nroost && k < 76; i++)
    {
        s16 sx = roost[i].x - view_x, sy = 511 - roost[i].y - view_y;
        VDPSprite *o;
        if ((u16)(sx + 16) > 352 || (u16)(sy + 16) > 256) continue;
        o = sat + k++;
        o->y = oy - roost[i].y + q[0];
        o->size_link = q[1] | k;
        o->attribut = q[2] + ps->vram_of[pose];
        o->x = ox + roost[i].x + q[3];
    }
    return k - first;
}
static Actor *balcony_child[4], *entrant;
static u8 window_armed;

void stage5_events_clear(void)
{
    memset(balcony_timer, 0, sizeof balcony_timer);
    memset(balcony_child, 0, sizeof balcony_child);
    entrant = NULL;
    entrant_timer = 0;
    window_armed = 0;
}

/* Alive and still the actor this spawner made. */
static bool child_alive(Actor *c, u16 tag) { return c && c->fam == F_SPEARMAN && TAG(c) == tag; }

static Actor *spearman_at(s16 x, s16 y, u16 facing, u16 tag)
{
    Actor *a = actor_new(F_SPEARMAN);
    if (!a) return NULL;
    a->variant = 0;
    a->facing = facing;
    a->x = (s32)x << 8;
    a->y = (s32)y << 8;
    a->ng = (a->ng & 1) | (tag << 1);
    return a;
}

static Actor *troll_at(s16 x, s16 y, u16 variant)
{
    Actor *a = actor_new(F_TROLL);
    if (!a) return NULL;
    a->variant = variant;
    a->x = (s32)x << 8;
    a->y = (s32)y << 8;
    face_player(a);
    return a;
}

static void stage5_flags(void)
{
    s16 px = player.x >> 8;
    /* One way over the gap and the corridor (0x42E2C). */
    if (CAM_X >= 2368 && CAM_X < 3528 && player.cam_min_x < CAM_X) player.cam_min_x = CAM_X;
    /* The arena lock (2.3): from player x 5760 the camera stays in [5376, 5760]. */
    if (!locked && px >= 5760) { locked = 1; arrival = 1; }
}

void stage5_events(void)
{
    /* Stage 5's scenery ends at 963 and its torches use 963-997 (src/
     * fam_torch.c): the pose cache borrows 998-1041 (taken back by every
     * stage_init / stage_restart, lent again here). */
    { extern void posecache_lend(u16 lo, u16 hi); posecache_lend(998, 1042); }
    torches5_update();
#ifdef TEST_PX
    { static u8 put; if (++put == 20) { player.x = (s32)TEST_PX << 8; player.y = (s32)TEST_PY << 8; } }   /* test builds: Willow placed */
#endif
#ifdef DBG_FLOORSCAN
    { static u8 done; if (!done) { done = 1; floor_scan(); } }
#endif
    s16 px = player.x >> 8, py = player.y >> 8;
    u16 i, rk = (rank >> 12) & 15;
    stage5_flags();
    if (nroost) roost_update();
    /* Event 0: the bat flock, once, when the player passes x 4160 (4.4). */
#ifdef EXP_NOBATS
    bats_placed = 1;                        /* measurement builds */
#endif
    if (!bats_placed && px > 4160)
    {
        bats_placed = 1;
        for (i = 0; i < 33; i++)
        {
            s16 x = bat_spots[i][0], y = bat_spots[i][1];
            if (x < player.cam_min_x) continue;
            if ((u16)(x - px + 128) <= 256 && (u16)(y - py + 100) <= 200) continue;
            if (nroost >= ROOST) break;
            roost[nroost].x = x; roost[nroost].y = y; nroost++;
        }
    }
    if (game_frame & 7) goto windows;
#ifdef EXP_NOSPEAR
    goto windows;                           /* measurement builds */
#endif
    /* Events 2-5: the interior balconies' endless spearmen (variant 14). */
    for (i = 0; i < 4; i++)
    {
        const Spot *s = balcony + i;
        if (child_alive(balcony_child[i], 3 + i)) continue;
        if (balcony_child[i]) { balcony_child[i] = NULL; balcony_timer[i] = spear_wait[rk]; }
        if (balcony_timer[i]) { balcony_timer[i] = balcony_timer[i] > 8 ? balcony_timer[i] - 8 : 0; continue; }
        if ((u16)(s->x - CAM_X) > 384 || (u16)(s->y - CAM_Y) > 256) continue;
        balcony_child[i] = spearman_at(s->sx, s->sy, 1, 3 + i);
    }
    /* Event 1: the corridor entrant (variant 16) while the camera is in the corridor. */
    if (CAM_X >= 2752 && CAM_X <= 4096 && (u16)(656 - CAM_Y) <= 256 && !child_alive(entrant, 2))
    {
        /* Variant 16 at the left screen edge, facing right: it walks in
         * from behind the player (4.3, run K: x = camera X), energy 8. */
        entrant = spearman_at(CAM_X, 656, 0, 2);
        if (entrant) entrant->variant = 16;
    }
windows:
    /* Events 6-12: the troll windows (5.2). */
    if (!window_armed) { window_armed = 1; for (i = 0; i < 7; i++) window_timer[i] = 32; }
    if (!time_stop && px < 2208)            /* past the last window's box (x 2048 + 160) nothing comes */
        for (i = 0; i < 7; i++)
        {
            const Spot *s = window5 + i;
            const u16 *b = window_box[i];
            if (trolls_out >= window_cap[rk] || window_out[i]) continue;
            if ((u16)(s->x - px + b[0]) > b[1] || (u16)(s->y - py + b[2]) > b[3]) continue;
            if (--window_timer[i]) continue;
            window_timer[i] = window_next[((rank >> 8) & 0xE0) >> 3 | (rnd() & 3)];
            if (troll_at(s->sx, s->sy, i + 1)) { window_out[i]++; trolls_out++; }   /* the troll's v[2] / v[3] keep the counts */
        }
    /* Event 13: the arena (6.2), at player x 5584. */
    if (!arena_on && px >= 5584)
    {
        static const u8 var[4] = { 9, 8, 9, 8 };
        static const u8 delay[4] = { 1, 16, 192, 224 };
        arena_on = 1;
        player.cam_min_x = 5376;
        sound_post(SND_MUSIC_BOSS5);
        if (arena_done) { boss5_wake = 1; return; }
        arena_done = 1;
        arena_left = 0;
        for (i = 0; i < 4; i++)
        {
            Actor *a = troll_at(0, 135, var[i]);
            if (!a) continue;
            a->active = 0;
            a->timer = delay[i];
            arena_left++;
        }
        if (!arena_left) boss5_wake = 1;
    }
}

/* ---- Trolls (5.1, class 4 kind 26). Chains: 0/1 throw, 2/3 walk, 4/5
 * crouch, 6/7 land, 8/9 leap up, 10/11 leap, 12 on the wall, 13 dropping
 * off it, 14/15 and 16/17 throws from the wall, 18/19 knocked back (by the
 * player's side; also the death throw). Variants: 0 placed, 1-7 out of
 * window var - 1 (they cling to the wall at the window, move up and down
 * it throwing orbs, then drop and walk), 8/9 the arena's from the right /
 * left edge. Placed trolls sleep until the player is within 224 x 112 px,
 * then count among the live trolls. Run B: hops of ~60 px (20 updates
 * crouched, 20 in the air, 43 px high), 32-update throws, knock-back ~42 px
 * over 28 updates. The phase table is not decoded (5.1): here a walk until
 * the aggression timer (0x1FDFC), then a hop toward the player or a throw. */
enum { TR_SLEEP, TR_WAIT, TR_WALK, TR_CROUCH, TR_AIR, TR_LAND, TR_THROW, TR_KNOCK, TR_DEAD, TR_WALL, TR_WTHROW, TR_DROP };
static const u8 troll_aggr[32] = { 176, 255, 224, 176, 176, 160, 160, 144, 160, 112, 144, 144, 160, 96, 128, 128,
                                   160, 80, 112, 128, 144, 64, 96, 112, 144, 48, 80, 96, 128, 32, 64, 96 };
static const u8 troll_drop[16] = { 1, 2, 1, 2, 1, 2, 2, 1, 2, 1, 2, 1, 2, 1, 1, 2 };

static void troll_gone(Actor *a)
{
    if (a->v[2] && trolls_out) trolls_out--;                     /* counted (FF0F5C) */
    if (a->v[3] && window_out[a->v[3] - 1]) window_out[a->v[3] - 1]--;   /* its window (FF0F80) */
    if (a->variant >= 8 && arena_left && !--arena_left) boss5_wake = 1;
}

static void troll_timer(Actor *a)
{
    a->t2 = troll_aggr[(((rank >> 8) >> 5) << 2) | (rnd() & 3)];
}

/* An orb toward the player: a 64-step direction (5.5), 4 px an update. */
static void troll_orb(Actor *a, s16 dy)
{
    Actor *o = actor_new(F_ORB);
    s16 dx;
    if (!o) return;
    sound_post(0x9D);
    o->x = a->x + (DIR(a) * 16 << 8);
    o->y = a->y + ((s32)dy << 8);
    dx = (player.x >> 8) - AX(o);
    o->v[0] = (angle128(dx, (player.y >> 8) - AY(o)) + 1) & 126;
    dir_v(o->v[0], 1024, &o->vx, &o->vy);
}

static void troll_walk(Actor *a) { a->phase = TR_WALK; chain(a, 2); }

static void troll_hop(Actor *a)
{
    face_player(a);
    a->phase = TR_CROUCH;
    a->timer = 20;
    chain(a, 6);
}

void troll_update(Actor *a)
{
    s16 dx = (player.x >> 8) - AX(a), dy = (player.y >> 8) - AY(a);
    if (!a->state)
    {
        a->state = 2;
        a->hp = a->last_hp = 10;
        troll_timer(a);
        if (a->variant >= 8) { a->phase = TR_WAIT; a->active = 0; a->v[2] = 0; return; }
        if (a->variant)
        {
            a->v[3] = a->variant;               /* its window */
            a->v[2] = 1;
            a->v[4] = AY(a);                    /* the window's y */
            a->phase = TR_WALL;
            a->timer = 160 + (rnd() & 127);     /* on the wall before it drops */
            a->sub = 0;
            face_player(a);
            actor_anim(a, 12);
            return;
        }
        a->phase = TR_SLEEP;
        chain(a, 2);
        return;
    }
    if (a->phase != TR_DEAD && a->hp != a->last_hp)
    {
        u16 side = dx > 0;                      /* the player's side (+30) */
        a->last_hp = a->hp;
        if (!a->hp)
        {
            sound_post(0x9C);
            a->phase = TR_DEAD;
            a->active = 2;
            a->vy = 768;
            a->vx = side ? -256 : 256;
            actor_anim(a, 18 + side);
            return;
        }
        sound_post(0x9B);
        if (a->phase != TR_AIR && a->phase != TR_WALL && a->phase != TR_WTHROW && a->phase != TR_DROP)
        {
            a->phase = TR_KNOCK;
            a->timer = 28;
            a->vx = side ? -384 : 384;          /* ~42 px over 28 updates */
            actor_anim(a, 18 + side);
        }
    }
    if (a->phase != TR_WAIT && a->phase != TR_DEAD) a->active = 1;    /* hittable again after a hit */
    if (a->phase > TR_WAIT && a->phase != TR_DEAD && !(game_frame & 63) && off_cam(a) && a->variant < 8)
    {
        troll_gone(a);
        actor_free(a);
        return;
    }
    actor_step(a);
    switch (a->phase)
    {
    case TR_SLEEP:                      /* 0x1FE48: |dx| <= 224, |dy| <= 112 */
        if ((u16)(dx + 224) > 448 || (u16)(dy + 112) > 224) return;
        a->v[2] = 1;
        trolls_out++;
        face_player(a);
        troll_walk(a);
        return;
    case TR_WAIT:                       /* the arena's: in after their delay at a screen edge */
        if (--a->timer) return;
        a->x = (s32)(a->variant == 9 ? CAM_X : CAM_X + 384) << 8;
        a->facing = a->variant == 8;
        a->active = 1;
        troll_walk(a);
        return;
    case TR_WALK:
        if ((dx < 0) != FACE(a) && (u16)(dx + 8) > 16) { a->facing ^= 1; chain(a, 2); }
        a->x += DIR(a) * 384;
        if (!actor_ground(a, 0, -24)) { a->phase = TR_AIR; a->vy = 0; a->vx = DIR(a) * 384; chain(a, 10); return; }
        if (--a->t2) return;
        troll_timer(a);
        if (rnd() & 1) troll_hop(a);
        else { face_player(a); a->phase = TR_THROW; a->timer = 32; chain(a, 0); }
        return;
    case TR_CROUCH:                     /* 20 updates, then a 20-update leap of ~60 px, 43 high */
        if (--a->timer) return;
        a->phase = TR_AIR;
        a->vy = 2200;
        a->vx = DIR(a) * 768;
        chain(a, 8);
        return;
    case TR_AIR:
        a->x += a->vx;
        a->y += a->vy;
        a->vy -= 220;
        if (a->vy < -1792) a->vy = -1792;
        if (a->vy < 0 && a->anim.chain == poseset_chain(families[F_TROLL]->ps, 8 + FACE(a))) chain(a, 10);
        if (a->vy < 0 && actor_ground(a, 0, -24)) { a->phase = TR_LAND; a->timer = 8; chain(a, 6); a->vx = 0; }
        if (AY(a) < -64) { troll_gone(a); actor_free(a); }
        return;
    case TR_LAND:
        if (--a->timer) return;
        if ((u16)(dx + 96) > 192 && (rnd() & 3)) { troll_hop(a); return; }   /* hops on toward the player */
        troll_walk(a);
        return;
    case TR_THROW:
        if (a->timer == 16) troll_orb(a, 16);
        if (--a->timer) return;
        troll_walk(a);
        return;
    case TR_KNOCK:
        a->x += a->vx;
        actor_ground(a, 0, -24);
        if (--a->timer) return;
        troll_walk(a);
        return;
    case TR_WALL:                       /* on the wall at the window: up and down, throws */
        if (!--a->timer) { a->phase = TR_DROP; a->vy = 0; actor_anim(a, 13); return; }
        if (!a->sub)
        {
            face_player(a);
            if (rnd() & 1) { a->phase = TR_WTHROW; a->t2 = 32; actor_anim(a, 16 + FACE(a)); return; }
            a->sub = 32;
            a->vy = (AY(a) > a->v[4] + 16) ? -256 : (AY(a) < a->v[4] - 48) ? 256 : ((rnd() & 1) ? 256 : -256);
        }
        a->sub--;
        a->y += a->vy;
        return;
    case TR_WTHROW:
        if (a->t2 == 16) troll_orb(a, 24);
        if (--a->t2) { a->timer = a->timer > 1 ? a->timer - 1 : 1; return; }
        a->phase = TR_WALL;
        a->sub = 0;
        actor_anim(a, 12);
        return;
    case TR_DROP:                       /* off the wall: falls, lands, walks (variant 0 from here) */
        a->y += a->vy;
        if (a->vy > -1792) a->vy -= 48;
        if (actor_ground(a, 0, -24))
        {
            if (a->v[3] && window_out[a->v[3] - 1]) window_out[a->v[3] - 1]--;
            a->v[3] = 0;
            a->variant = 0;
            a->phase = TR_LAND; a->timer = 8; chain(a, 6);
        }
        if (AY(a) < -64) { troll_gone(a); actor_free(a); }
        return;
    case TR_DEAD:                       /* thrown up, then a burst on landing (3B0A) */
        a->x += a->vx;
        a->y += a->vy;
        if (a->vy > -1792) a->vy -= 48;
        if ((a->vy < 0 && actor_ground(a, 0, -24)) || AY(a) < CAM_Y - 32)
        {
            sound_post(SND_BOOM);
            effect(F_BURST, AX(a), AY(a), 4);
            award(6);                   /* 0x1806: 500 */
            drop(a, troll_drop[rnd() & 15]);
            troll_gone(a);
            actor_free(a);
        }
        return;
    }
}

/* ---- Troll orbs (5.5, class 10 kind 15): 64 updates, 4 px an update; a
 * wall ends them in their impact. Chains: 0 flight, 1 impact. */
void orb_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->timer = 64; actor_anim(a, 0); return; }
    actor_step(a);
    if (a->state == 4) { a->active = 2; if (anim_done(&a->anim)) actor_free(a); return; }
    a->x += a->vx;
    a->y += a->vy;
    if (!--a->timer) { actor_free(a); return; }
    if ((game_frame & 1) && terrain_id(AX(a), AY(a)) && terrain_flags(AX(a), AY(a)))
    {
        a->state = 4;
        actor_anim(a, 1);
        return;
    }
    if (!(game_frame & 1) && shot_test(AX(a), AY(a), actor_box(a, 1), TRUE)) { a->state = 4; actor_anim(a, 1); }
}

/* ---- Stone pillars (5.4, class 6 kind 7): standable; variant 0 falls when
 * the player comes within 48 px (after 1..48 updates), variant 2 stands;
 * energy 16 (cracked below 8); destroyed, it comes back 8 updates later
 * (variant 0 at y 832, variant 2 falling from y 768). Chains: 0 intact, 1
 * flash, 2 cracked (variant 0); 3, 4, 5 (variant 2). */
static const u8 pillar_delay[16] = { 1, 16, 32, 16, 16, 32, 16, 32, 48, 16, 32, 48, 32, 16, 32, 1 };
static const s16 pillar_box[4] = { 8, 16, 48, 96 };
enum { PL_WAIT, PL_DELAY, PL_FALL, PL_REST, PL_GONE };

static void pillar_pose(Actor *a)
{
    u16 base = a->variant == 2 ? 3 : 0;
    actor_anim(a, base + (a->v[3] ? 1 : a->hp < 8 ? 2 : 0));
}

void stone_update(Actor *a)
{
    u16 p;
    if (!a->state)
    {
        a->state = 2;
        a->v[1] = AX(a);
        a->v[2] = AY(a);
        a->hp = 16;
        a->phase = a->variant == 2 ? PL_REST : PL_WAIT;
        pillar_pose(a);
        return;
    }
    a->flags &= ~STOOD;
    if (a->phase == PL_GONE)
    {
        if (--a->timer) return;
        a->hp = 16;
        a->active = 1;
        a->v[3] = 0;
        a->y = (s32)(a->variant == 2 ? 768 : 832) << 8;
        a->phase = a->variant == 2 ? PL_FALL : PL_WAIT;
        a->vy = 0;
        pillar_pose(a);
        return;
    }
    if (a->v[3] && !--a->v[3]) pillar_pose(a);
    p = shot_test(AX(a), AY(a), pillar_box, FALSE);
    if (p)
    {
        a->hp = a->hp > p ? a->hp - p : 0;
        if (!a->hp)
        {
            u16 i;
            sound_post(SND_BOOM);
            for (i = 0; i < 10; i++) effect(F_BURST, AX(a) - 15 + (rnd() & 31), AY(a) - 40 + 8 * i, 5);
            a->phase = PL_GONE;
            a->active = 0;
            a->timer = 8;
            return;
        }
        sound_post(SND_HIT);
        a->v[3] = 16;
        pillar_pose(a);
    }
    actor_step(a);
    switch (a->phase)
    {
    case PL_WAIT:
        if ((game_frame & 3) || (u16)((player.x >> 8) - AX(a) + 47) > 94) return;
        a->phase = PL_DELAY;
        a->timer = pillar_delay[rnd() & 15];
        return;
    case PL_DELAY:
        if (--a->timer) return;
        a->phase = PL_FALL;
        a->vy = 0;
        return;
    case PL_FALL:
        if (a->vy > -1792) a->vy -= 32;
        a->y += a->vy;
        if (actor_ground(a, 0, -48)) { sound_post(0xA8); a->phase = PL_REST; return; }
        if (AY(a) < -128) { a->phase = PL_GONE; a->active = 0; a->timer = 8; }
        return;
    }
}

/* ---- The tower doorway (5.6, class 12 kind 35 at (2576, 640); 0x32DDC):
 * with the player in it (|dx| <= 16, |dy| <= 32) the corridor's interior is
 * wiped in over the tower (frame 0 at once, frames 1-6 every 8 updates,
 * the end 8 updates later) while the player is held and ordinary enemies
 * explode (FF83D0); then the camera's minimum is 2368. The picture (ROM
 * 0x8D404..; tools/wipepack5.py: 24 x 14 16-px cells over x 2368-2751, y
 * 528-751) is written into plane A from tiles borrowed off the pose cache
 * until the camera has passed it (the corridor's camera minimum follows
 * the camera, so it never returns). Deviation: two of its cells have
 * arcade priority group 1; all are written without the priority bit. */
#include "../data/stage5/wipe5.h"
extern const u8 wipe5_tiles[], wipe5_frame[];
extern const u16 wipe5_words[];
static u16 wipe5_base;      /* the borrowed tiles (0: none) */

static void wipe5_put(u16 f)
{
    static u16 w[4];
    u16 cx, cy;
    for (cy = 0; cy < WIPE5_H; cy++)
        for (cx = 0; cx < WIPE5_W; cx++)
        {
            const u16 *s = wipe5_words + cy * 2 * WIPE5_W * 2 + cx * 2;
            if (wipe5_frame[cy * WIPE5_W + cx] != f) continue;
            w[0] = s[0]; w[1] = s[1]; w[2] = s[WIPE5_W * 2]; w[3] = s[WIPE5_W * 2 + 1];
            stage_cells_put_pri(WIPE5_C + cx * 2, WIPE5_R + cy * 2, w, 2, 2, wipe5_base, 0);
        }
}

void gate5_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 0; if (gate5_open) actor_free(a); return; }
    if (a->state == 2)
    {
        if ((u16)((player.x >> 8) - AX(a) + 16) > 32 || (u16)((player.y >> 8) - AY(a) + 32) > 64) return;
        a->state = 4;
        a->timer = 8;
        a->sub = 1;
        gate_freeze = 1;
        encounter_clear = 1;
#ifndef NO_WIPE5
        if ((wipe5_base = posecache_take(WIPE5_TILES)) != 0)
        {
            VDP_loadTileData((const u32 *)wipe5_tiles, wipe5_base, WIPE5_TILES, DMA);
            wipe5_put(0);
        }
#endif
        return;
    }
    if (a->state == 6)                      /* the picture shows until the camera has passed it */
    {
        if (CAM_X > 2752) { posecache_give(); wipe5_base = 0; actor_free(a); }
        return;
    }
    if (--a->timer) return;
    a->timer = 8;
    if (a->sub < 7)
    {
        if (wipe5_base) wipe5_put(a->sub);
        a->sub++;
        return;
    }
    gate_freeze = 0;
    encounter_clear = 0;
    gate5_open = 1;
    terrain_changed();
    if (player.cam_min_x < 2368) player.cam_min_x = 2368;
    a->state = 6;
}

/* ---- Corridor cart pushers (5.3, class 4 kind 27 variant 4) and their
 * spiked carts (class 6 kind 8). In view (and not while the doorway wipe
 * runs) a pusher faces left and pushes a cart 48 px ahead of him at 1 px an
 * update, stopping for 64 updates when rand & 15 picks one of 0x21624's 4 in
 * 16 (every 64 updates); he cannot be hit. The cart (energy 30, cracked
 * below 10) blocks and pushes the player 2 px an update and hurts on
 * contact; broken (burst, 15 updates later gone), or with the player behind
 * the pusher, or left of the camera's minimum + 128, the pusher becomes a
 * spearman (variant 10). Pusher chains: 0/1 held, 2/3 pushing; cart chains:
 * 0/1 by facing, 2/3 cracked. */
static const u8 cart_stop[16] = { 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0 };
u8 gate_freeze;             /* FF83EA: the doorway wipe runs */

static void pusher_end(Actor *a)
{
    Actor *c = a->link ? actors + a->link - 1 : NULL;
    if (c && c->fam == F_CART && c->link == (u8)(a - actors) + 1) c->link = 0;
    {
        Actor *b = spearman_at(AX(a), AY(a), a->facing, 0);
        if (b && stage_index == 5) b->variant = a->variant == 2 ? 12 : 10;   /* stage 6: the brute after the push (energy 8; stage6_families.md 6.1) */
    }
    actor_free(a);
}

void doorway_update(Actor *a)
{
    Actor *c = a->link ? actors + a->link - 1 : NULL;
    if (c && (c->fam != F_CART || c->link != (u8)(a - actors) + 1)) c = NULL;
    switch (a->state)
    {
    case 0:
        a->state = 2;
        a->active = 0;
        return;
    case 2:                             /* asleep until in view */
        if (gate_freeze || (u16)(AX(a) - CAM_X) > 448 || (u16)(AY(a) - CAM_Y) > 256) return;
        a->state = 4;
        a->active = 2;                  /* not hittable while pushing */
        a->v[3] = a->facing;            /* stage 6: the dir byte is the push budget (px) */
        a->facing = a->variant != 2;    /* variant 2 (stage 6's events) pushes right */
        a->timer = 64;
        c = actor_new(F_CART);
        if (c)
        {
            c->x = a->x + ((s32)(a->facing ? -48 : 48) << 8); c->y = a->y; c->facing = a->facing;
            c->link = (u8)(a - actors) + 1;
            a->link = (u8)(c - actors) + 1;
        }
        chain(a, 2);
        return;
    }
    if (!c || (a->facing ? (player.x >> 8) > AX(a) : (player.x >> 8) <= AX(a)) || AX(a) < player.cam_min_x + 128) { pusher_end(a); return; }
    actor_step(a);
    if (a->state == 6)                  /* stopped */
    {
        if (--a->timer) return;
        a->state = 4; a->timer = 64; chain(a, 2);
        return;
    }
    if (!--a->timer)
    {
        a->timer = 64;
        if (cart_stop[rnd() & 15]) { a->state = 6; chain(a, 0); return; }
    }
    if (!c->v[0])                       /* the cart not blocked by the player */
    {
        a->x += a->facing ? -256 : 256;
        c->x = a->x + ((s32)(a->facing ? -48 : 48) << 8);
        if (a->variant != 4 && !--a->v[3]) pusher_end(a);   /* stage 6: the budget spent */
    }
}

void cart_update(Actor *a)
{
    const s16 *pb, *cb;
    s16 px = player.x >> 8;
    if (!a->state) { a->state = 2; a->hp = a->last_hp = 30; actor_anim(a, FACE(a)); return; }
    if (a->state == 4)                  /* broken: 15 updates */
    {
        if (!--a->timer) actor_free(a);
        return;
    }
    if (a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        if (!a->hp)
        {
            sound_post(SND_BOOM);
            effect(F_BURST, AX(a), AY(a), 4);
            a->state = 4; a->active = 0; a->timer = 15;
            if (a->link) { Actor *p = actors + a->link - 1; if (p->fam == F_DOORWAY) p->link = 0; }
            return;
        }
        sound_post(SND_HIT);
        a->v[5] = 10;
        if (a->hp < 10 && a->v[1] != 2) { a->v[1] = 2; actor_anim(a, 2 + FACE(a)); }
    }
    if (a->v[5] && !--a->v[5]) a->active = 1;
    /* Its pusher gone, the cart stays as an obstacle (stage6_families.md
     * 6.2; the same routine in stage 5) until broken or left behind. */
    if (!a->link && !(game_frame & 31) && AX(a) < player.cam_min_x - 64) { actor_free(a); return; }
    /* The player against the cart: pushed 2 px an update, the pair held. */
    pb = player_box();
    cb = actor_box(a, 0);
    a->v[0] = box_hit(AX(a), AY(a), cb, px, player.y >> 8, pb);
    if (a->v[0]) player.x += (px < AX(a) ? -2 : 2) << 8;
    actor_step(a);
}
