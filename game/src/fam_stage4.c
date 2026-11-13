/* Stage 4's families and rules (specs/stage4.md): the camera's limits and
 * locks, the music triggers, the falling snow, the events (soldiers from
 * behind, the bomb trooper from the right, the camp's tents), the rolling
 * snowballs, the bats, the swooping hawks, the tent spear throwers, the
 * dragon (mid-boss), Sorsha (boss), the shield ride, the crash and the end
 * scene. Native y = arcade y (world_top 8191).
 *
 * Approximations (M1_NOTES): the bats' chase is a straight steer with the
 * table's random offsets; the dragon's fade-in
 * and the hit flashes are not shown (approved cut 4: fixed palettes);
 * snowflakes are capped at 24. */
#include <genesis.h>
#include "actor.h"
#include "coop.h"
#include "player.h"
#include "sound.h"
#include "hud.h"
#include "game.h"
#include "shot.h"
#include "dirmath.h"
#include "s4spr.h"
#include "quest.h"
#include "frontend.h"
#include "stagedata.h"

#define FACE(a) ((a)->facing & 1)
#define DIR(a) (FACE(a) ? -1 : 1)
#define SND_HIT 0x32
#define SND_BOOM 0x33

extern u8 scene_lock;
u8 dragon_beaten;           /* FF838A (stage 4): the dragon is beaten */
u8 sorsha_beaten;           /* FF83A1 */
static u8 arena1, arena2, ride_on, trooper4_alive, tent_alive, music_at;
/* Home-only Sorsha (AGENTS.md): no shield sled; after the camp she walks
 * down its slope (stage 4's soldiers and hawks on the way, the ride's
 * coins as pickups) to the village scene. */
static u8 walk_on, walk_hawks;
static u16 walk_end;
static bool sorsha_hero(void) { extern u8 coop_partner; return player.form == 2 || (two_players && coop_partner == 2); }

static u16 ride_gold_next;

static void chain(Actor *a, u16 c) { actor_anim(a, c + FACE(a)); }
static void face_player(Actor *a) { a->facing = (player.x >> 8) < AX(a); }
static bool off_cam(Actor *a)
{
    return (u16)(AX(a) - CAM_X + 64) > 448 || (u16)(AY(a) - CAM_Y + 96) > 416;
}

/* A hit: TRUE when the energy changed (the caller handles 0); not
 * collidable for `immune` frames (v[5]). */
static bool hit(Actor *a, u16 immune)
{
    if (a->hp == a->last_hp) return FALSE;
    a->last_hp = a->hp;
    if (a->hp) { sound_post(SND_HIT); a->active = 2; a->v[5] = immune; }
    return TRUE;
}
static void immune_tick(Actor *a) { if (a->v[5] && !--a->v[5]) a->active = 1; }

static void boom(Actor *a, u16 score, s16 set, u16 burst)
{
    sound_post(SND_BOOM);
    effect(F_BURST, AX(a), AY(a), burst);
    if (score != 0xFF) award(score);
    if (set >= 0) drop(a, set);
    actor_free(a);
}

/* ---- Camera (2.3) and music triggers (8.4). */
void stage4_camera(void)
{
    player.cam_max_y = 7936;
    player.cam_min_y = checkpoint >= 10 ? 5360 : checkpoint >= 5 ? 4592 : 4096;
    if (player.cam_y < player.cam_min_y) player.cam_y = player.cam_min_y;
    if (checkpoint >= 7) dragon_beaten = 1;    /* checkpoints past the arena (1.7: how the arcade restores max X there is unknown) */
    player.cam_max_x = sorsha_beaten ? 16000 : dragon_beaten ? 6528 : 3584;
    if (sorsha_beaten && sorsha_hero()) { walk_on = 1; player.cam_min_y = 0; }   /* Sorsha: the walk down (a restart after the camp) */
}

void spray_clear(void);

void stage4_reset(void)
{
    extern void flakes_clear(void);
    arena1 = arena2 = ride_on = walk_on = walk_hawks = 0;
    walk_end = 0;
    trooper4_alive = tent_alive = 0;
    edge_alive = 0;
    music_at = checkpoint >= 9 ? 2 : checkpoint >= 6 ? 1 : 0;
    ride_gold_next = 0;
    encounter_clear = 0;
    midboss_active = 0;
    flakes_clear();
    spray_clear();
#ifdef S4_RIDE
    dragon_beaten = sorsha_beaten = 1;      /* test builds: the ride from checkpoint 13 */
    player.cam_max_x = 16000;
#endif
}

static void stage4_flags(void)
{
    s16 px = player.x >> 8;
    if (CAM_X >= 2304 && player.cam_min_y < 4592 && !ride_on && !walk_on) player.cam_min_y = 4592;
    if (CAM_X >= 4609 && player.cam_min_y < 5360 && !ride_on && !walk_on) player.cam_min_y = 5360;
    /* Music triggers (class 16 kind 8): the cave at x 2656, the stage again at 4352. */
    if (music_at == 0 && px >= 2656) { music_at = 1; sound_post(SND_MUSIC_CAVE); sound_post(0x01FA); }
    if (music_at == 1 && px >= 4352) { music_at = 2; sound_post(SND_MUSIC_STAGE4); sound_post(0x01FA); }
    /* The dragon's arena (2.3): at x 3584 the camera keeps inside [3392, 3584]. */
    if (!dragon_beaten && !arena1 && px >= 3584) { arena1 = 1; encounter_clear = 1; player.cam_min_x = 3392; }
    /* Sorsha's arena (6.10): at x 6528 the camera glides 2 px an update to 6528. */
    if (dragon_beaten && !sorsha_beaten && !arena2 && px >= 6528) { arena2 = 1; encounter_clear = 1; }
    if (arena2 && !sorsha_beaten && CAM_X < 6528)
    {
        player.cam_x = CAM_X + 2 > 6528 ? 6528 : CAM_X + 2;
        player.cam_min_x = player.cam_x;
    }
}

/* ---- Falling snow (2.7): 24 sources across the screen, three depths
 * (2, 1, 0.5 px a frame); a particle list, not actors (up to ~65 flakes in
 * the arcade; 24 here). */
#define FLAKES 24
static struct { s16 x, y; u8 v, pose; } flk[FLAKES];
static u8 nflk;
static u8 flk_timer[24];
static const s16 flk_dx[24] = { -8, 0, 16, 40, 64, 88, 104, 120, 136, 152, 168, 184, 200, 216, 232, 248, 280, 288, 312, 320, 352, 368, 376, 392 };
static const u8 flk_wait[3][8] = { { 32, 40, 48, 40, 48, 32, 48, 40 }, { 80, 88, 96, 88, 88, 80, 96, 96 }, { 160, 176, 168, 160, 176, 168, 176, 160 } };
static u8 hint_s4flake[S4_FLAKE_POSES];
static PoseSet ps_flake = { s4_flake_poses, s4_flake_tiles, s4_flake_anim, S4_FLAKE_POSES, 0, 0, NULL, hint_s4flake, 0, s4_flake_sat, 0 };

void flakes_clear(void) { u16 i; nflk = 0; for (i = 0; i < 24; i++) flk_timer[i] = 1 + i * 3; }

static void flakes_update(void)
{
    u16 i;
    for (i = 0; i < nflk;)
    {
        flk[i].y -= flk[i].v == 0 ? 2 : flk[i].v == 1 ? 1 : (game_frame & 1);
        if (flk[i].y < CAM_Y - 16) flk[i] = flk[--nflk];
        else i++;
    }
    if (CAM_X > 2320) return;
    for (i = 0; i < 24; i++)
    {
        u16 v = i % 3;
        if (--flk_timer[i]) continue;
        flk_timer[i] = flk_wait[v][rnd() & 7];
        if (nflk < FLAKES)
        {
            flk[nflk].x = CAM_X + flk_dx[i] - 32;
            flk[nflk].y = CAM_Y + 240;
            flk[nflk].v = v;
            flk[nflk].pose = v * 4 + (rnd() & 3);
            nflk++;
        }
    }
}

/* Resident art (families_init, before the pose cache takes the sprite
 * VRAM): 11 tiles; each flake is one sprite written here directly. */
static u16 vram_flake[S4_FLAKE_POSES];
void flakes_init(void)
{
    ps_flake.vram_of = vram_flake;
    poseset_resident(&ps_flake);
}

u16 flakes_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y)
{
    u16 i, k = first;
    s16 ox = 128 - view_x, oy = 128 + 511 - view_y;
    for (i = 0; i < nflk && k < 76; i++)
    {
        u16 pose = flk[i].pose;                 /* chain = pose: one pose each (tools/pack_stage4.sh "flake") */
        const u8 *d = ps_flake.poses + pose * 6;
        const s16 *q = ps_flake.sat + (((d[4] << 8) | d[5]) << 2);
        VDPSprite *o = sat + k++;
        o->y = oy - flk[i].y + q[0];
        o->size_link = q[1] | k;
        o->attribut = q[2] + vram_flake[pose];
        o->x = ox + flk[i].x + q[3];
    }
    return k - first;
}

/* ---- The sled's snow spray (6.13 "spray"; class 12 kinds 26 / 31, routines
 * 0x360A0 / 0x35E5C): every 8 ride updates an emitter, which for 16 updates
 * sends a particle every 2. A particle takes, from the game's random numbers
 * in the arcade's order: a lifetime and chain (0x35E70: 14, 20, 26, 32
 * updates; chains 0x35E74), a direction pair and flip, a starting offset
 * (0x35ECE: 0, 4, 8, 12 px); it then moves at its direction's speed
 * (0x35F60, 1/256 px) from the sled plus 0x35F4C[class], following the
 * sled. A particle list with resident art, as the falling snow.
 * The class is the sled's slope class (its +2E, 0x3112C), taken & 3 here
 * (classes above 3 come only from unrelated descriptors). */
#define SPRAYS 32
static struct { s32 ox, oy; u8 life, dir, chain, frame, tick, flip, cls; } spr[SPRAYS];   /* offsets 16.16 px */
static u8 nspr, spray_emit, spray_left, spray_wait;
static Actor *spray_sled;
static const u8 spray_life[4] = { 14, 20, 26, 32 };
static const s16 spray_vel[8][2] = { { -640, 0 }, { -627, 124 }, { -590, 244 }, { -531, 355 }, { -452, 452 }, { -355, 531 }, { -244, 590 }, { -124, 627 } };
static const s16 spray_off[4][2] = { { -12, -16 }, { -12, -16 }, { -8, -16 }, { -8, -16 } };
static u8 spray_class, spray_emit_class;   /* the sled's slope class now / at the emitter's birth (& 3) */
static u8 hint_s4spray[S4_SPRAY_POSES];
static PoseSet ps_spray = { s4_spray_poses, s4_spray_tiles, s4_spray_anim, S4_SPRAY_POSES, 0, 0, NULL, hint_s4spray, 0, s4_spray_sat, 0 };
static u16 vram_spray[S4_SPRAY_POSES];

void spray_clear(void) { nspr = 0; spray_left = 0; spray_emit = 0; spray_sled = NULL; }

void spray_init(void)
{
    ps_spray.vram_of = vram_spray;
    poseset_resident(&ps_spray);
}

/* The sled's ride update: an emitter every 8 updates (kind 26 at the sled). */
static void spray_ride(Actor *sled)
{
    spray_sled = sled;
    if (++spray_emit < 8) return;
    spray_emit = 0;
    rnd();                                  /* the emitter's variant (0x36116), not read */
    spray_emit_class = spray_class;         /* its +46: the sled's class then (0x3112C) */
    spray_left = 8;
    spray_wait = 1;
}

static void spray_update(void)
{
    u16 i;
    for (i = 0; i < nspr;)
    {
        const u8 *c;
        if (!--spr[i].life) { spr[i] = spr[--nspr]; continue; }
        spr[i].ox += (s32)spray_vel[spr[i].dir][0] << 8;
        spr[i].oy += (s32)spray_vel[spr[i].dir][1] << 8;
        c = ps_spray.anim;
        { u16 k = spr[i].chain; while (k--) c += 2 + 2 * c[0]; }
        if (c[2 + 2 * spr[i].frame] != 0xFF && !--spr[i].tick && spr[i].frame + 1 < c[0])
        {
            spr[i].frame++;
            spr[i].tick = c[2 + 2 * spr[i].frame];
        }
        i++;
    }
    if (spray_left && !--spray_wait)
    {
        spray_wait = 2;
        spray_left--;
        if (nspr < SPRAYS)
        {
            u16 k = rnd() & 3, h = rnd() & 1;
            const u8 *c = ps_spray.anim;
            spr[nspr].life = spray_life[k];
            spr[nspr].chain = k;
            { u16 j = k; while (j--) c += 2 + 2 * c[0]; }
            spr[nspr].frame = 0;
            spr[nspr].tick = c[2];
            spr[nspr].cls = spray_emit_class;
            spr[nspr].dir = h + spray_emit_class * 2;
            spr[nspr].flip = h;
            spr[nspr].ox = (s32)((rnd() & 3) * 4) << 16;
            spr[nspr].oy = 0;
            nspr++;
        }
    }
}

u16 spray_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y)
{
    u16 i, k = first;
    s16 ox, oy;
    if (!spray_sled || !nspr) return 0;
    ox = 128 - view_x + AX(spray_sled);
    oy = 128 + 511 - view_y - AY(spray_sled);
    for (i = 0; i < nspr && k < 76; i++)
    {
        const u8 *c = ps_spray.anim, *d;
        const s16 *q;
        VDPSprite *o;
        u16 pose, j = spr[i].chain;
        while (j--) c += 2 + 2 * c[0];
        pose = c[1 + 2 * spr[i].frame];
        d = ps_spray.poses + pose * 6;
        q = ps_spray.sat + (((d[4] << 8) | d[5]) << 2);
        o = sat + k++;
        o->y = oy - spray_off[spr[i].cls][1] - (s16)(spr[i].oy >> 16) + q[0];
        o->size_link = q[1] | k;
        o->attribut = (q[2] + vram_spray[pose]) ^ (spr[i].flip ? 0x0800 : 0);
        o->x = ox + spray_off[spr[i].cls][0] + (s16)(spr[i].ox >> 16) + (spr[i].flip ? -q[3] - (((q[1] >> 10) & 3) + 1) * 8 : q[3]);
    }
    return k - first;
}

/* ---- Events (3.2): soldiers from behind (25), the camp's edge soldiers
 * (26), the bomb trooper (24), the tents (28-30). */
static const u8 ev25_cap[16] = { 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4 };
static const u8 ev25_wait[16] = { 110, 158, 222, 94, 158, 94, 110, 158, 94, 158, 110, 94, 110, 158, 222, 110 };
static const u8 tent_first[16] = { 48, 16, 32, 0, 32, 16, 0, 32, 16, 0, 32, 16, 0, 16, 32, 0 };
static const u8 tent_next[16] = { 160, 96, 96, 128, 64, 160, 64, 128, 128, 64, 160, 64, 96, 128, 96, 160 };
static const u8 tent_cap[16] = { 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 8, 8 };
static const s16 tent_x[3] = { 5512, 5752, 6280 };
static u16 ev25_timer, ev24_timer, ev26_timer;
static u8 tent_left[3], tent_armed[3];
static u16 tent_timer[3];

static void stage4_spawn_soldier(s16 x, s16 y, u16 facing)
{
    Actor *s = actor_new(F_SOLDIER);
    if (!s) return;
    s->variant = 0;
    s->facing = facing;
    s->x = (s32)x << 8;
    s->y = (s32)y << 8;
    s->spawn = 0;
    s->flags = 0;
    edge_alive++;                           /* FF0F40: the soldier's removal lowers it */
}

static void ride_gold_ahead(void);
/* Sorsha's walk down the ride's slope (Home only). Soldiers as the camp's
 * event 26 (same caps and waits, mostly ahead; dropped in from the top), a swooping hawk every 1024 px, the ride's coins, and at the
 * bottom the village scene (as the crash's, src: crash_update 10-12). */
static u16 walk_soldiers(void)
{
    u16 i, n = 0;
    for (i = pool_lo[C_ENEMY]; i < pool_hi[C_ENEMY]; i++) if (actors[i].fam == F_SOLDIER) n++;
    return n;
}
/* Sorsha's Quest, chapter IV (src/quest.c): the same walk the other way,
 * from the village up the slope to the camp; the ride's coins from its
 * bottom end, a hawk every 1024 px, the soldiers mostly ahead (left). The
 * chapter's end at the camp is src/quest.c's. */
static const s16 ride_gold[86][3];          /* below */
static void walk_up_update(void)
{
    u16 r = rank >> 12;
    if (!walk_hawks) { walk_hawks = (CAM_X >> 10) + 1; ride_gold_next = 85; }
    while (ride_gold_next != 0xFFFF && ride_gold[ride_gold_next][1] > CAM_X - 80)
    {
        Actor *g = actor_new(F_REWARD);
        if (!g) break;
        g->x = (s32)ride_gold[ride_gold_next][1] << 8;
        g->y = (s32)ride_gold[ride_gold_next][2] << 8;
        g->variant = ride_gold[ride_gold_next][0] & 15;
        ride_gold_next = ride_gold_next ? ride_gold_next - 1 : 0xFFFF;
    }
    if (CAM_X > 7600 && walk_soldiers() < ev25_cap[r])
    {
        if (!ev26_timer) ev26_timer = ev25_wait[rnd() & 15];
        else if (!--ev26_timer)
        {
            u16 ahead = (rnd() & 15) < 14;
            s16 x = ahead ? CAM_X + 32 + (rnd() & 63) : CAM_X + 224 + (rnd() & 31);
            Actor *o = actor_new(F_SOLDIER);
            if (o) { o->variant = 1; o->facing = !ahead; o->x = (s32)x << 8; o->y = (s32)(CAM_Y + 232) << 8; }
        }
    }
    if ((u16)(CAM_X >> 10) + 1 < walk_hawks && CAM_X > 7800)
    {
        Actor *h = actor_new(F_HAWK4);
        walk_hawks = (CAM_X >> 10) + 1;
        if (h) { h->x = (s32)(CAM_X + 160) << 8; h->y = (s32)(CAM_Y + 256) << 8; h->variant = 2; }
    }
}

static void walk_update(void)
{
    u16 r = rank >> 12;
    s16 px = player.x >> 8;
    if (qrev) { walk_up_update(); return; }
    if (walk_end)
    {
        if (--walk_end == 130)
        {
            Actor *h = actor_new(F_ENDING);
            if (h) { h->x = (s32)16256 << 8; h->y = (s32)80 << 8; h->variant = 2; }
            h = actor_new(F_ENDING);
            if (h) { h->x = (s32)16256 << 8; h->y = (s32)96 << 8; h->variant = 3; }
            player_script(22);
            player_script_x = 16162;
            player.cam_max_x = 16384;
        }
        if (!walk_end) game_stage_clear();
        return;
    }
    if (px >= 15776) { sound_post(0xA0); walk_end = 72 + 130; encounter_clear = 1; return; }
    ride_gold_ahead();
    if (CAM_X < 15200 && walk_soldiers() < ev25_cap[r])
    {
        if (!ev26_timer) ev26_timer = ev25_wait[rnd() & 15];
        else if (!--ev26_timer)
        {
            /* dropped in from the screen's top (as event 25's soldiers):
             * off-screen sides are far above or below a 45-degree slope */
            u16 right = (rnd() & 15) < 14;
            s16 x = right ? CAM_X + 240 + (rnd() & 63) : CAM_X + 96 + (rnd() & 31);
            Actor *o = actor_new(F_SOLDIER);
            if (o)                          /* variant 1: wakes where placed (variant 0 moves to the screen's edge) */
            {
                o->variant = 1; o->facing = right;
                o->x = (s32)x << 8; o->y = (s32)(CAM_Y + 232) << 8;
            }
        }
    }
    if ((u16)(CAM_X >> 10) > walk_hawks && CAM_X < 15000)
    {
        Actor *h = actor_new(F_HAWK4);
        walk_hawks = CAM_X >> 10;
        if (h) { h->x = (s32)(CAM_X + 160) << 8; h->y = (s32)(CAM_Y + 256) << 8; h->variant = 2; }
    }
}

void stage4_events(void)
{
    u16 r = rank >> 12, k;
    stage4_flags();
    flakes_update();
    spray_update();
    if (walk_on) { walk_update(); return; }
    if (encounter_clear) return;
    /* Event 25: while camera x < 1280. */
    if (CAM_X < 1280 && edge_alive < ev25_cap[r])
    {
        if (!ev25_timer) ev25_timer = ev25_wait[rnd() & 15];
        else if (!--ev25_timer) stage4_spawn_soldier(CAM_X - 16, CAM_Y + 80, 0);
    }
    /* Event 26: the camp, camera x 4608..5376, mostly from the right. */
    if (CAM_X >= 4608 && CAM_X < 5376 && edge_alive < ev25_cap[r])
    {
        if (!ev26_timer) ev26_timer = ev25_wait[rnd() & 15];
        else if (!--ev26_timer)
        {
            u16 right = (rnd() & 15) < 14;
            stage4_spawn_soldier(right ? CAM_X + 416 : CAM_X - 32, 5424, right);
        }
    }
    /* Event 24: one bomb trooper at a time from above the right edge. */
    if (CAM_X >= 544 && CAM_X <= 1536 && !trooper4_alive && ++ev24_timer >= 60)
    {
        Actor *t = actor_new(F_TROOPER);
        ev24_timer = 0;
        if (t)
        {
            t->variant = 2;
            t->facing = 1;
            t->x = (s32)(CAM_X + 416) << 8;
            t->y = (s32)(CAM_Y + 288) << 8;
            trooper4_alive = 1;
        }
    }
    /* Events 28-30: the tents. */
    for (k = 0; k < 3; k++)
    {
        s16 tx = tent_x[k];
        if (!tent_armed[k])
        {
            if (!(game_frame & 15) && tx - CAM_X < 480) { tent_armed[k] = 1; tent_left[k] = 6; tent_timer[k] = 1 + tent_first[rnd() & 15]; }
            continue;
        }
        if (!tent_left[k] || --tent_timer[k]) continue;
        tent_timer[k] = 1;
        if ((u16)((player.x >> 8) - tx + 48) <= 96 || (u16)(tx - CAM_X + 96) > 576 || tent_alive >= tent_cap[r]) continue;
        {
            Actor *t = actor_new(F_TENT);
            if (!t) continue;
            t->x = (s32)tx << 8;
            t->y = (s32)5424 << 8;
            tent_alive++;
            tent_left[k]--;
            tent_timer[k] = tent_next[rnd() & 15];
        }
    }
}

void stage4_events_clear(void)
{
    u16 k;
    ev25_timer = ev24_timer = ev26_timer = 0;
    for (k = 0; k < 3; k++) tent_armed[k] = tent_left[k] = 0;
}

void trooper4_gone(void) { trooper4_alive = 0; }

/* ---- Rolling snowball (5.1, class 4 kind 30). Chains: 0-3 by size (pose
 * table 0x22170), 4 rolling (small), 5 the burst. v[0] speed. */
static const s8 ball_foot[4] = { 0, 8, 16, 18 };

void snowball_update(Actor *a)
{
    s16 top;
    if (!a->state)
    {
        a->state = 2;
        a->active = 0;
        a->hp = a->last_hp = 1;
        a->variant = 1;
        a->timer = 20;
        actor_anim(a, 4);
        return;
    }
    if (a->state == 4) { actor_step(a); if (!--a->timer) actor_free(a); return; }
    if (!a->active)
    {
        if ((u16)(AX(a) - CAM_X) < 384 && (u16)(AY(a) - CAM_Y) < 240) { a->active = 1; a->phase = 0; a->v[0] = 0; }
        return;
    }
    if (a->hp != a->last_hp && !a->hp)
    {
        sound_post(SND_HIT);
        award(2);
        a->state = 4; a->active = 2; a->timer = 24;
        actor_anim(a, 5);
        return;
    }
    if (encounter_clear || (off_cam(a) && (player.x >> 8) >= AX(a))) { actor_free(a); return; }
    actor_step(a);
    if (a->phase == 0)
    {
        if (a->v[0] < 1024) a->v[0] += 16;
        a->x -= a->v[0];
        a->y -= 3 << 8;
        actor_ground(a, 0, -ball_foot[a->variant]);
        if (!--a->timer)
        {
            a->timer = 20;
            if (a->variant < 3) { a->variant++; actor_anim(a, a->variant); }
            else { a->phase = 2; a->vy = (rnd() & 1) ? 768 : 256; }
        }
        return;
    }
    /* Bouncing, 4 px a frame. */
    a->x -= 1024;
    a->y += a->vy;
    a->vy = a->vy - 48 < -1024 ? -1024 : a->vy - 48;
    if (a->vy < 0 && terrain_floor(AX(a), AY(a) - ball_foot[3], &top))
    {
        a->y = (s32)(top + ball_foot[3]) << 8;
        a->vy = (rnd() & 1) ? 768 : 256;
    }
}

/* ---- Bat (5.2, class 4 kind 31). Chains: 0 hanging, 1 flying. Phases:
 * 0 hanging, 1 waking, 2 chasing, 3 climbing, 4 hovering. */
static const u8 bat_wait[16] = { 32, 64, 96, 0, 32, 64, 96, 64, 32, 64, 96, 64, 64, 64, 96, 0 };
static const s8 bat_off[16] = { -1, -2, 1, 0, 2, 0, -1, 0, 2, 3, 0, -2, 0, -3, -1, 0 };
static const u8 bat_rise[4] = { 80, 96, 64, 56 };

void bat_update(Actor *a)
{
    s16 dx = (player.x >> 8) - AX(a), dy = (player.y >> 8) + 24 - AY(a);
    if (!a->state)
    {
        a->state = 2; a->active = 1; a->hp = a->last_hp = 1; a->phase = 0;
        actor_anim(a, 0);
        return;
    }
    if (a->hp != a->last_hp && !a->hp) { boom(a, 2, 1, 4); return; }
    if (encounter_clear) { boom(a, 0xFF, -1, 5); return; }
    if (!(game_frame & 7) && stage_index == 3 && off_cam(a) && AX(a) > 4096) { actor_free(a); return; }   /* stage 5 keeps them (stage5.md 4.4) */
    actor_step(a);
    switch (a->phase)
    {
    case 0:
        if ((u16)(dx + 96) <= 192 && (u16)(-dy + 24 + 112) <= 224)
        {
            u16 w = bat_wait[rnd() & 15];
            if (!w) return;
            a->phase = 1; a->timer = w + 42;
        }
        return;
    case 1:
        if (--a->timer) return;
        a->phase = 2; a->timer = 1;
        actor_anim(a, 1);
        return;
    case 2:                                 /* steer every 24 frames, 2 px a frame */
        if (!--a->timer)
        {
            a->timer = 24;
            a->v[0] = (angle128(dx, dy) + bat_off[rnd() & 15] * 8) & 127;
            dir_v(a->v[0], 512, &a->vx, &a->vy);
        }
        a->x += a->vx; a->y += a->vy;
        if ((u16)(dy + 32) <= 64 && (u16)(dx + 16) <= 32)
        {
            a->phase = 3;
            a->v[1] = (player.y >> 8) + bat_rise[rnd() & 3];
            a->vy = 0;
        }
        return;
    case 3:                                 /* climb, then hover */
        if (a->vy < 2048) a->vy += 48;
        a->y += a->vy;
        if (AY(a) >= a->v[1]) { a->phase = 4; a->timer = 64; }
        return;
    case 4:
        if (!(a->timer & 7)) a->vy = (rnd() & 1) ? 256 : -256;
        a->y += a->vy;
        a->x += dx < 0 ? -256 : 256;
        if (!--a->timer) { a->phase = 2; a->timer = 1; }
        return;
    }
}

/* ---- Swooping hawk (5.4, class 4 kind 15 variant 2). Chains: 0 glide (h196B4),
 * 1 flap (h1971E), 2/3 flight by facing, 4-7 hit poses. The path 0x18D62:
 * (16-direction, active, frames). */
static const u8 swoop[9][3] = { { 5, 1, 24 }, { 4, 1, 4 }, { 3, 1, 24 }, { 2, 1, 3 }, { 1, 1, 3 }, { 15, 2, 3 }, { 14, 2, 3 }, { 13, 2, 3 }, { 12, 2, 255 } };

void hawk4_update(Actor *a)
{
    if (!a->state)
    {
        a->state = 2; a->active = 0; a->phase = 0;
        actor_anim(a, 0);
        return;
    }
    if (a->hp != a->last_hp && !a->hp) { boom(a, 4, 4, 5); return; }
    if (encounter_clear && a->phase) { boom(a, 0xFF, -1, 5); return; }
    actor_step(a);
    switch (a->phase)
    {
    case 0:                                 /* wake: to the top right, glide out left */
        if ((game_frame & 15) || AX(a) - CAM_X >= 192) return;
        a->hp = a->last_hp = 2;
        a->x = (s32)(CAM_X + 96) << 8;
        a->y = (s32)(CAM_Y + 256) << 8;
        a->active = 2;
        a->phase = 1;
        actor_anim(a, 2 + 1);
        return;
    case 1:
        a->x -= 362; a->y -= 362;
        if (AX(a) <= CAM_X) { a->phase = 2; a->timer = 90; }
        return;
    case 2:
        if (--a->timer) return;
        a->x = (s32)(CAM_X - 16) << 8;
        a->y = (s32)(CAM_Y + 152) << 8;
        a->phase = 3; a->sub = 0; a->timer = 0;
        return;
    case 3:                                 /* the swoop */
        if (!a->timer)
        {
            if (a->sub >= 9) { actor_free(a); return; }
            a->timer = swoop[a->sub][2];
            a->v[0] = swoop[a->sub][0] << 3;
            a->active = swoop[a->sub][1] == 1 ? 1 : 1;
            a->facing = (a->v[0] & 127) > 64;
            chain(a, 2);
            a->sub++;
        }
        a->timer--;
        {
            s16 vx, vy;
            dir_v(a->v[0], 640, &vx, &vy);
            a->x += vx; a->y += vy;
        }
        if (!(game_frame & 15) && off_cam(a) && a->sub >= 9) actor_free(a);
        return;
    }
}

/* ---- Tent spear thrower (5.5, class 4 kind 25). Chains (+facing): 0
 * stand, 2 lunge, 4 throw high, 6 throw low, 8 emerge. v[0] target offset,
 * v[1] attack timer. Its spear (class 10 kind 14): F_TSPEAR, chains 0/1. */
enum { TC_STAND = 0, TC_LUNGE = 2, TC_HIGH = 4, TC_LOW = 6, TC_EMERGE = 8 };
static const s8 tent_off[16] = { -32, 0, -16, 20, 32, -8, -24, 16, 24, 16, 10, 0, -16, 32, 0, -32 };
static const u8 tent_drop[8] = { 2, 1, 2, 3, 2, 4, 2, 2 };

void tent_update(Actor *a)
{
    s16 px = player.x >> 8, dx = px - AX(a), adx = dx < 0 ? -dx : dx;
    if (!a->state)
    {
        a->state = 2; a->active = 2; a->hp = a->last_hp = 6;
        face_player(a);
        a->x += (FACE(a) ? -8 : 16) << 8;
        a->phase = 0; a->timer = 32;
        chain(a, TC_EMERGE);
        return;
    }
    if (encounter_clear) { tent_alive--; boom(a, 0xFF, -1, 5); return; }
    if (hit(a, 16) && !a->hp) { tent_alive--; boom(a, 6, tent_drop[rnd() & 7], 5); return; }
    immune_tick(a);
    actor_step(a);
    switch (a->phase)
    {
    case 0:
        if (--a->timer) return;
        a->active = 1; a->phase = 1; a->v[1] = 40;
        a->v[0] = tent_off[(rnd() >> 1) & 15];
        chain(a, TC_STAND);
        return;
    case 1:                                 /* approach the player + offset */
        {
            s16 t = px + a->v[0] - AX(a);
            if (t > 4) a->x += 384; else if (t < -4) a->x -= 384;
            face_player(a);
            actor_ground(a, 0, -32);
            if (!(game_frame & 31)) a->v[0] = tent_off[(rnd() >> 1) & 15];
        }
        if (--a->v[1]) return;
        a->v[1] = 40;
        {
            u16 c = adx < 64 ? 3 : adx < 88 ? 1 : adx < 120 ? 2 : 1 + (rnd() & 1);
            if (c == 3) { a->phase = 3; a->timer = 3; chain(a, TC_LUNGE); }
            else { a->phase = 2; a->timer = 28 + 14; a->sub = c; chain(a, c == 1 ? TC_HIGH : TC_LOW); }
        }
        return;
    case 2:                                 /* the throw */
        if (a->timer == 14)
        {
            Actor *s = actor_new(F_TSPEAR);
            if (s)
            {
                s->facing = FACE(a);
                s->variant = a->sub == 1 ? 0 : 2;
                s->x = a->x + ((s32)(FACE(a) ? -16 : 16) << 8);
                s->y = a->y + (24 << 8);
            }
            sound_post(0x8B);
        }
        if (--a->timer) return;
        a->phase = 1; chain(a, TC_STAND);
        return;
    case 3:                                 /* lunge 3 px a frame */
        a->x += DIR(a) * 768;
        if (--a->timer) return;
        a->phase = 1; chain(a, TC_STAND);
        return;
    }
}

void tspear_update(Actor *a)
{
    s16 top;
    if (!a->state)
    {
        a->state = 2; a->active = 1;
        a->vx = (a->variant ? 384 : 512) * DIR(a);
        a->vy = 960;
        chain(a, 0);
        return;
    }
    if (encounter_clear) { actor_free(a); return; }
    if (a->state == 4) { if (!--a->timer) actor_free(a); return; }
    a->x += a->vx; a->y += a->vy;
    a->vy = a->vy - 32 < -1792 ? -1792 : a->vy - 32;
    if (a->vy < 0 && terrain_floor(AX(a), AY(a) - 4, &top)) { a->state = 4; a->active = 2; a->timer = 38; }
    else if (off_cam(a)) actor_free(a);
}

/* ---- The dragon (6.1-6.6, class 18 kind 5): body (F_DRAGON), neck and
 * head drawn as parts (F_DRAGONP: variant 2 neck, 4 head). Chains: 0 body,
 * 1-7 neck by step, 8-14 head by step, 15-21 head breathing. Body v[0]
 * heading, v[1] target x, v[2] target y, v[3] bob, sub = neck step,
 * flags: breathing. Fire: F_FIRE (chains 0 small, 1 large). */
static const s8 head_off[7][2] = { { 40, -16 }, { 40, -16 }, { 19, -28 }, { 8, -32 }, { -19, -28 }, { -40, -16 }, { -40, -16 } };
static const s8 fire_off[7][2] = { { 16, -10 }, { 14, -14 }, { 10, -16 }, { 4, -16 }, { -10, -16 }, { -14, -14 }, { -16, -10 } };
static const s8 dragon_boom[8][2] = { { 0, -8 }, { 40, 8 }, { -40, -32 }, { 24, -32 }, { -24, 8 }, { 24, -40 }, { -40, 0 }, { 22, -12 } };
static const u8 dragon_hover[4] = { 32, 64, 96, 128 };
/* Waypoint offsets (0x2B09E, the left side; the right side mirrored) by the
 * fight's time (< 1280, < 2560, later updates: lower and lower). */
static const s16 dragon_way[3][4][2] = {
    { { -128, 104 }, { -72, 140 }, { -72, 140 }, { 0, 152 } },
    { { -128, 48 }, { -96, 88 }, { -56, 112 }, { 0, 120 } },
    { { -96, 40 }, { -72, 64 }, { -40, 80 }, { 0, 88 } },
};
static u16 dragon_time;
static Actor *dragon_part[2];
enum { DG_WAIT, DG_INTRO, DG_FLY, DG_HOVER, DG_TO, DG_BREATH, DG_DIE, DG_REWARD };

static void dragon_target(Actor *a)
{
    u16 row = dragon_time < 1280 ? 0 : dragon_time < 2560 ? 1 : 2;
    const s16 *w = dragon_way[row][rnd() & 3];
    s16 side = (rnd() & 1) ? -1 : 1;
    a->v[1] = (player.x >> 8) + side * w[0];
    a->v[2] = 4663 + w[1];
    if (a->v[1] < player.cam_min_x + 32) a->v[1] = player.cam_min_x + 32;
    if (a->v[1] > CAM_X + 352) a->v[1] = CAM_X + 352;
}

static void dragon_parts(Actor *a)
{
    u16 k;
    for (k = 0; k < 2; k++)
    {
        Actor *p = dragon_part[k];
        if (!p || p->fam != F_DRAGONP) continue;
        if (k == 0) { p->x = a->x; p->y = a->y; actor_anim(p, 1 + a->sub); }
        else
        {
            p->x = a->x + ((s32)head_off[a->sub][0] << 8);
            p->y = a->y + ((s32)head_off[a->sub][1] << 8);
            actor_anim(p, (a->flags & 1 ? 15 : 8) + a->sub);
        }
        p->active = a->active ? 2 : 0;
    }
}

void dragon_update(Actor *a)
{
    s16 px = player.x >> 8;
    if (!a->state)
    {
        u16 k;
        if (dragon_beaten) { actor_free(a); return; }
        a->state = 2; a->active = 0; a->phase = DG_WAIT; a->sub = 3;
        actor_anim(a, 0);
        for (k = 0; k < 2; k++)
        {
            Actor *p = actor_new(F_DRAGONP);
            dragon_part[k] = p;
            if (p) { p->variant = 2 + 2 * k; p->state = 2; p->active = 0; p->x = a->x; p->y = a->y; }
        }
        return;
    }
    switch (a->phase)
    {
    case DG_WAIT:
        if ((u16)(AX(a) - CAM_X) > 384 || (u16)(px - AX(a) + 192) > 384) return;
        a->phase = DG_INTRO; a->timer = 192;
        midboss_active = 1;
        a->active = 2;
        a->hp = a->last_hp = 80;
        hud_boss(80);
        break;
    case DG_INTRO:
        if (--a->timer) break;
        a->active = 1;
        a->phase = DG_FLY;
        dragon_time = 0;
        a->v[1] = px + ((rnd() & 1) ? 128 : -128);   /* the first target (0x2B05C): beside the player, 4663 + 152 */
        a->v[2] = 4663 + 152;
        break;
    case DG_DIE:
        if (!terrain_floor(AX(a), AY(a) - 42, &(s16){0})) a->y -= 256;
        else if (a->timer)
        {
            if (!(a->timer & 15))
            {
                const s8 *o = dragon_boom[(a->timer >> 4) & 7];
                effect(F_BURST, AX(a) + o[0], AY(a) + o[1], 4);
                sound_post(SND_BOOM);
            }
            if (!--a->timer) a->phase = DG_REWARD;
        }
        dragon_parts(a);
        return;
    case DG_REWARD:
        award(12); award(12);                   /* 10,000 */
        drop(a, 13);                            /* 15 x 100 gold */
        midboss_active = 0;
        encounter_clear = 0;
        dragon_beaten = 1;
        hud_boss(-1);
        player.cam_max_x = 6528;
        player.cam_min_x = CAM_X;
        {
            u16 k;
            for (k = 0; k < 2; k++) if (dragon_part[k] && dragon_part[k]->fam == F_DRAGONP) actor_free(dragon_part[k]);
        }
        actor_free(a);
        return;
    }
    if (a->phase >= DG_FLY && a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        hud_boss(a->hp);
        if (!a->hp)
        {
            sound_post(0x9A);
            a->phase = DG_DIE; a->active = 2; a->timer = 96;
            a->flags = 0;
            return;
        }
        sound_post(SND_HIT);
        a->active = 2; a->v[5] = 40;
    }
    if (a->v[5] && !--a->v[5]) a->active = 1;
    if (dragon_time < 0xFFFF) dragon_time++;
    actor_step(a);
    /* Bob: +-4 px, 128 updates. */
    {
        u16 t = ++a->v[3] & 127;
        a->y += (t < 32 || t >= 96) ? 64 : -64;
    }
    /* The neck turns a step every 8 updates toward the player (not while breathing). */
    if (!(game_frame & 7) && !(a->flags & 1))
    {
        u16 ang = (angle128(px - AX(a), (player.y >> 8) - AY(a)) * 2 + 8) >> 4;
        s16 want = (s16)ang - 5;
        if (want < 0) want = 0;
        if (want > 6) want = 6;
        if (a->sub < want) a->sub++; else if (a->sub > want) a->sub--;
    }
    switch (a->phase)
    {
    case DG_FLY: case DG_TO:
        if (!(game_frame & (a->phase == DG_FLY ? 3 : 1)))
        {
            u16 want = angle128(a->v[1] - AX(a), a->v[2] - AY(a));
            s16 d = (s16)((want - a->v[0]) & 127);
            if (d) a->v[0] = (a->v[0] + (d < 64 ? 1 : 127)) & 127;
            if (d > 8 && d < 120) a->v[0] = (a->v[0] + (d < 64 ? 1 : 127)) & 127;
        }
        {
            s16 vx, vy;
            dir_v(a->v[0], 512, &vx, &vy);
            a->x += vx; a->y += vy;
        }
        if (AX(a) < player.cam_min_x + 32) a->x = (s32)(player.cam_min_x + 32) << 8;
        if (AX(a) > CAM_X + 352) a->x = (s32)(CAM_X + 352) << 8;
        if ((u16)(AX(a) - a->v[1] + 14) <= 28 && (u16)(AY(a) - a->v[2] + 14) <= 28)
        {
            if (a->phase == DG_FLY) { a->phase = DG_HOVER; a->timer = dragon_hover[rnd() & 3]; }
            else { a->phase = DG_BREATH; a->timer = 64; a->flags |= 1; }
        }
        break;
    case DG_HOVER:
        if (--a->timer) break;
        a->phase = DG_TO;
        dragon_target(a);
        break;
    case DG_BREATH:
        if (a->timer == 64)
        {
            Actor *f = actor_new(F_FIRE);
            sound_post(0x99);
            if (f)
            {
                s16 hx = AX(a) + head_off[a->sub][0] + fire_off[a->sub][0], hy = AY(a) + head_off[a->sub][1] + fire_off[a->sub][1];
                f->x = (s32)hx << 8; f->y = (s32)hy << 8;
                f->variant = 0;
                f->sub = a->sub + 5;            /* 16-direction: 5..11 */
            }
        }
        if (--a->timer) break;
        a->flags &= ~1;
        a->phase = DG_FLY;
        dragon_target(a);
        break;
    }
    dragon_parts(a);
}

void dragonp_update(Actor *a) { (void)a; }

/* Fire (6.5): variant 0 the emitter (seven flames, one an update), 2 a
 * flame: after its delay 6 px an update in the breath's direction to the
 * floor, then 5 px along it for 48 updates, then 12 updates and removed. */
static const u8 fire_delay[7] = { 1, 4, 7, 10, 14, 18, 22 };

void fire_update(Actor *a)
{
    s16 top;
    if (dragon_beaten) { actor_free(a); return; }
    if (a->variant == 0)
    {
        if (!a->state) { a->state = 2; a->active = 0; a->t2 = 0; return; }
        if (a->t2 < 7)
        {
            Actor *f = actor_new(F_FIRE);
            if (f) { f->variant = 2; f->x = a->x; f->y = a->y; f->sub = a->sub; f->v[0] = a->t2; f->timer = fire_delay[a->t2]; f->facing = a->t2 & 1; }
            a->t2++;
            return;
        }
        actor_free(a);
        return;
    }
    if (!a->state) { a->state = 2; a->active = 0; actor_anim(a, a->v[0] >= 4); return; }
    actor_step(a);
    if (a->state == 2)
    {
        if (--a->timer) return;
        a->state = 4; a->active = 1;
        dir_v(a->sub << 3, 1536, &a->vx, &a->vy);
        return;
    }
    if (a->state == 4)
    {
        a->x += a->vx; a->y += a->vy;
        if (terrain_floor(AX(a), AY(a) - 10, &top)) { a->y = (s32)(top + 10) << 8; a->state = 6; a->timer = 48; a->vx = FACE(a) ? -1280 : 1280; }
        else if (off_cam(a)) actor_free(a);
        return;
    }
    if (a->state == 6)
    {
        a->x += a->vx;
        actor_ground(a, 0, -10);
        if (!--a->timer) { a->state = 8; a->timer = 12; a->active = 2; }
        return;
    }
    if (!--a->timer) actor_free(a);
}

/* ---- Sorsha (6.8, class 18 kind 3). Chains (+facing): 0 intro, 2 stand,
 * 4 walk, 6 crouch, 8 jump, 10 slash wind-up, 12 slash, 14 air wind-up,
 * 16 air slash, 18 thrust wind-up, 20 thrust, 22 high thrust, 24/26
 * backstep, 28 defeated. Phases below; v[0] combo step. */
enum { SC_INTRO = 0, SC_STAND = 2, SC_WALK = 4, SC_CROUCH = 6, SC_JUMP = 8, SC_SWIND = 10, SC_SLASH = 12,
       SC_AWIND = 14, SC_ASLASH = 16, SC_TWIND = 18, SC_THRUST = 20, SC_HIGH = 22, SC_BACK = 24, SC_BACK2 = 26, SC_DOWN = 28 };
enum { SP_WAIT, SP_INTRO, SP_DECIDE, SP_IN, SP_OUT, SP_SWORD, SP_JUMP, SP_GUARD, SP_PARRY, SP_DEFEAT, SP_TALK, SP_GONE };
static const char *const sorsha_text[2] = { "I LOST.", "KAEL HAS TAKEN THE BABY." };
static const char *const rival_text[2] = { "YOU FIGHT WELL, SORSHA.", "MY SWORD AND MY HEART ARE YOURS." };   /* Sorsha's Quest: Madmartigan */
#define RIVAL (fe.quest)                     /* Sorsha's Quest chapter IV: the boss is Madmartigan (src/families.c families_rival) */

static void slash_box(Actor *a, s16 dx, s16 dy)
{
    Actor *s = actor_new(F_SLASH4);
    if (!s) return;
    s->x = a->x + ((s32)(FACE(a) ? -dx : dx) << 8);
    s->y = a->y + ((s32)dy << 8);
    s->facing = FACE(a);
}

void slash4_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 1; a->timer = 4; return; }
    if (!--a->timer) actor_free(a);
}

#include "sorsha_table.h"

static void sorsha_decide(Actor *a)
{
    s16 dx = (player.x >> 8) - AX(a), adx = dx < 0 ? -dx : dx;
    u16 d = adx >= 256 ? 7 : adx >> 5 >> 1, ph = sorsha_lists[sorsha_pick[d][rank >> 12]][rnd() & 15];
    face_player(a);
    switch (ph)
    {
    case 4:                                 /* walk in (timer by distance, 0x28528) */
        a->phase = SP_IN; a->timer = adx > 160 ? 48 : 24; chain(a, SC_WALK); return;
    case 6:                                 /* walk back (0x285EC: 10 or 21) */
        a->phase = SP_OUT; a->timer = (rnd() & 1) ? 10 : 21; chain(a, SC_WALK); return;
    case 8:                                 /* sword: combo type by distance (0x28652 / 0x28662) */
        a->phase = SP_SWORD; a->v[0] = adx < 64 ? 0 : adx < 112 ? 2 : 4; a->sub = 0; a->timer = 16;
        chain(a, a->v[0] == 0 ? SC_SWIND : SC_TWIND); return;
    case 10:
        a->phase = SP_JUMP; a->sub = 0; a->timer = 20; chain(a, SC_CROUCH); return;
    default:                                /* 2: stand 16 updates, decide again */
        a->phase = SP_GUARD; a->timer = 16; chain(a, SC_STAND); return;
    }
}

void sorsha_update(Actor *a)
{
    s16 dx = (player.x >> 8) - AX(a), adx = dx < 0 ? -dx : dx;
    if (!a->state)
    {
        if (sorsha_beaten && !RIVAL) { actor_free(a); return; }
        a->state = 2; a->active = 0; a->phase = SP_WAIT;
        return;
    }
    if (a->phase == SP_WAIT)
    {
        if (AX(a) - CAM_X > 384) return;
        if (RIVAL)
        {
            /* her way up ends at the camp: the camera stops at the arcade's
             * arena (6528) and his colours take stage 4's sprite line 1 */
            if (CAM_X > 6528) return;
            player.cam_min_x = player.cam_max_x = 6528;
            PAL_setColors(16, sd->sprpal, 16, DMA_QUEUE);
            sound_post(SND_MUSIC_BOSS);
        }
        a->active = 1;
        a->hp = a->last_hp = 96;
        hud_boss(96);
        face_player(a);
        a->phase = SP_INTRO; a->timer = 90 + 24;
        chain(a, SC_INTRO);
        return;
    }
    if (a->phase < SP_DEFEAT && a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        hud_boss(a->hp);
        if (!a->hp)
        {
            sound_post(0x98);
            a->phase = SP_DEFEAT; a->active = 2; a->timer = 80 + 30;
            chain(a, SC_DOWN);
            return;
        }
        sound_post(SND_HIT);
        a->active = 2; a->v[5] = 17;
    }
    if (a->v[5] && !--a->v[5]) a->active = 1;
    actor_step(a);
    if (AX(a) < CAM_X + 16) a->x = (s32)(CAM_X + 16) << 8;
    if (AX(a) > CAM_X + 368) a->x = (s32)(CAM_X + 368) << 8;
    /* The parry (0x28E74): a sword swing within reach while she faces the player. */
    if (a->phase == SP_DECIDE || a->phase == SP_IN || a->phase == SP_OUT || a->phase == SP_GUARD)
    {
        bool faces = FACE(a) == (dx < 0), pfaces = player.facing == (dx >= 0);
        if (adx < 128 && faces && pfaces && shot_sword_live() && (rnd() & 15) < 6)
        {
            a->phase = SP_PARRY; a->active = 2; a->timer = 30;
            a->vx = FACE(a) ? 1024 : -1024;
            chain(a, SC_BACK);
            if (rnd() & 1)
            {
                Actor *k = actor_new(F_CLASH);
                sound_post(0x28);
                if (k) { k->x = a->x + ((s32)(FACE(a) ? -16 : 16) << 8); k->y = a->y + (24 << 8); }
            }
            return;
        }
    }
    switch (a->phase)
    {
    case SP_INTRO:
        if (a->timer == 24) chain(a, SC_STAND);
        if (!--a->timer) sorsha_decide(a);
        return;
    case SP_DECIDE:
        sorsha_decide(a);
        return;
    case SP_IN: case SP_OUT:
        a->x += (a->phase == SP_IN ? DIR(a) : -DIR(a)) * 384;
        actor_ground(a, 0, -32);
        if (!--a->timer || (a->phase == SP_IN && adx < 48)) a->phase = SP_DECIDE;
        return;
    case SP_GUARD:
        if (!--a->timer) a->phase = SP_DECIDE;
        return;
    case SP_SWORD:
        if (--a->timer) return;
        if (a->sub == 0)
        {
            a->sub = 1;
            if (a->v[0] == 0) { sound_post(0x96); chain(a, SC_SLASH); slash_box(a, 32, 4); a->timer = 16; }
            else { sound_post(0x97); chain(a, a->v[0] == 2 ? SC_THRUST : SC_HIGH); slash_box(a, 36, a->v[0] == 2 ? 4 : 20); a->timer = 8; }
            return;
        }
        if ((rnd() & 15) < 7 && adx < 120) { a->sub = 0; a->timer = 16; chain(a, a->v[0] == 0 ? SC_SWIND : SC_TWIND); return; }
        chain(a, SC_STAND);
        a->phase = SP_DECIDE;
        return;
    case SP_JUMP:
        if (a->sub == 0) { if (--a->timer) return; a->sub = 1; a->vy = 1152; chain(a, SC_JUMP); return; }
        if (a->sub == 1)
        {
            a->x += DIR(a) * 512;
            a->y += a->vy;
            a->vy = a->vy - 40 < -1920 ? -1920 : a->vy - 40;
            if (adx < 48 && !a->t2) { a->t2 = 1; sound_post(0x96); chain(a, SC_ASLASH); slash_box(a, 32, 4); }
            if (a->vy < 0 && actor_ground(a, 0, -32)) { a->sub = 2; a->timer = 24; a->t2 = 0; chain(a, SC_CROUCH); }
            return;
        }
        if (!--a->timer) a->phase = SP_DECIDE;
        return;
    case SP_PARRY:
        a->x += a->vx;
        if (a->vx > 0) a->vx = a->vx > 128 ? a->vx - 128 : 0; else a->vx = a->vx < -128 ? a->vx + 128 : 0;
        if (a->timer == 25) chain(a, SC_BACK2);
        if (--a->timer) return;
        a->active = 1;
        a->phase = SP_DECIDE;
        return;
    case SP_DEFEAT:
        if (--a->timer) return;
        award(12);                          /* 5000 */
        scene_lock = 1;
        player_script(22);
        player_script_x = player.x >> 8;    /* frozen while she speaks */
        a->phase = SP_TALK; a->sub = 0; a->v[1] = 0; a->timer = 30;
        return;
    case SP_TALK:                           /* her line (cut 3: the HUD's rows), 3 updates a letter */
        if (--a->timer) return;
        a->timer = 3;
        if (a->sub == 2)
        {
            hud_message(NULL, 4); hud_message(NULL, 6);
            scene_lock = 0;
            if (RIVAL)
            {
                PAL_setColors(16, sd->sprpal + 16, 16, DMA_QUEUE);   /* line 1 the stage's again */
                hud_boss(-1);
                player.script = 0;
                a->phase = SP_GONE;
                game_stage_clear();             /* chapter IV's end (src/quest.c) */
                return;
            }
            sorsha_beaten = 1;
            encounter_clear = 0;
            hud_boss(-1);
            player.cam_max_x = 16000;
            player.cam_min_x = CAM_X;
            player_script_x = 7232;         /* the walk to the shield (6.11) */
            if (sorsha_hero())              /* Home Sorsha: no sled; she walks down the slope */
            {
                player.script = 0;
                player.cam_min_y = 0;
                walk_on = 1;
                sound_post(SND_MUSIC_RIDE);
            }
            a->phase = SP_GONE;
            return;
        }
        {
            const char *t = (RIVAL ? rival_text : sorsha_text)[a->sub];
            a->v[1]++;
            hud_text(t, a->v[1], 2, 4 + a->sub * 2);
            if (t[a->v[1] - 1] != ' ') sound_post(0x2F);
            if (!t[a->v[1]]) { a->sub++; a->v[1] = 0; a->timer = a->sub == 2 ? 96 : 12; }
        }
        return;
    case SP_GONE:
        if (!(game_frame & 15) && off_cam(a)) actor_free(a);
        return;
    }
}

void clash_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 2; a->timer = 12; actor_anim(a, 0); return; }
    actor_step(a);
    if (!--a->timer) actor_free(a);
}

/* ---- The shield ride (6.13). Chains: 0 rest (r3129E), 1 mounting
 * (r312BE), 2-5 riding by slope class. The 86 ride pickups (ROM 0x2D586)
 * are placed as the ride reaches them (the pickup pool holds 20). */
static const s16 ride_gold[86][3] = {
#include "ride_gold.h"
};

/* The ride's tables by slope class (0x31136 top speed, 0x3113E lowest,
 * 0x310EA jump; 16 entries as the ROM runs on: a descriptor's class byte
 * can be any of 0..15). */
#ifdef DBG_RIDE
u16 dbg_cls[5];                        /* measurement: ride updates per slope class */
u8 dbg_id0[64];                        /* measurement: collision ids under the sled when class 0 */
s16 dbg_ride[4];
#endif
static const u16 ride_max[16] = { 1152, 1408, 1536, 1792, 1024, 1280, 1408, 1408, 0, 2, 1, 0, 12334, 44, 18624, 57728 };
static const u16 ride_min[16] = { 1024, 1280, 1408, 1408, 0, 2, 1, 0, 12334, 44, 18624, 57728, 53678, 6, 18990, 62 };
static const u16 ride_shift[16] = { 0, 2, 1, 0, 12334, 44, 18624, 57728, 53678, 6, 18990, 62, 26390, 12334, 44, 12846 };   /* 0x31146 */
static const s16 ride_jump[16] = { 896, 1024, 1152, 1280, 20152, 8012, 4141, 25, 512, 7, 26164, 20152, 11164, 26414, 6524, 1 };

static void ride_gold_ahead(void)
{
    while (ride_gold_next < 86 && ride_gold[ride_gold_next][1] < CAM_X + 400)
    {
        Actor *g = actor_new(F_REWARD);
        if (!g) break;
        g->x = (s32)ride_gold[ride_gold_next][1] << 8;
        g->y = (s32)ride_gold[ride_gold_next][2] << 8;
        g->variant = ride_gold[ride_gold_next][0] & 15;
        if (two_players) g->pad[2] = 1 + (ride_gold_next & 1);   /* co-op: the ride's coins alternate between the players */
        ride_gold_next++;
    }
}

void shield_update(Actor *a)
{
    s16 top;
    switch (a->state)
    {
    case 0:
        if (sorsha_hero()) { actor_free(a); return; }   /* Home Sorsha: no sled (AGENTS.md) */
        a->state = 2; a->active = 2; a->v[0] = 0; a->vy = 0;
        actor_anim(a, 0);
        return;
    case 2:                                 /* wait for Madmartigan */
        if (!sorsha_beaten || (player.x >> 8) < AX(a)) return;
        player_script(10);
        sound_post(SND_MUSIC_RIDE);
        ride_on = 1;
        player.cam_min_y = 0;
        player.cam_max_x = 16000;
        a->state = 4; a->timer = 32;
        actor_anim(a, 1);
        return;
    case 4:
        actor_step(a);
        if (!--a->timer) { a->state = 6; actor_anim(a, 2); a->v[1] = 0; a->v[2] = 1024; a->v[3] = 512; a->v[4] = 0; }   /* 0x30EF0: max 1024, min 512, class 0 */
        break;
    case 6:                                 /* the ride */
        {
            bool on = terrain_floor(AX(a), AY(a) - 1, &top);
            u16 held = player.last_held, s = on ? terrain_slope_class(AX(a), AY(a)) : 0;   /* the arcade's probe (0, -16) from its origin: the surface here */
            actor_step(a);
            /* The slope class (+2E: the floor probe 0x4200 copies descriptor
             * byte 5 on a slope, 0 on flat floor or none): a change sets the
             * top and lowest speeds and the pose (0x310B8). */
            if (s != a->v[1])
            {
                a->v[1] = s;
                a->v[2] = ride_max[s];
                a->v[3] = ride_min[s];
                a->v[4] = ride_shift[s] & 15;       /* +3C (a shift: the 68000 uses its low bits) */
                if (s < 4) actor_anim(a, 2 + s);   /* 0x312DE[class] (tools/pack_stage4.sh "shield") */
            }
            spray_class = s & 3;
#ifdef DBG_RIDE
            { extern u16 dbg_cls[5]; dbg_cls[s < 4 ? s : 4]++; }
            { extern s16 dbg_ride[4]; s16 t2; dbg_ride[0] = on; dbg_ride[1] = s; dbg_ride[2] = a->vy; dbg_ride[3] = terrain_floor(AX(a), AY(a) + 24, &t2) || 1 ? AY(a) - top : 0; }
            { extern u8 dbg_id0[64]; if (on && !s) { u16 t = terrain_id(AX(a), AY(a) - 1); if (dbg_id0[t] < 255) dbg_id0[t]++; } }
#endif
            if ((held | coop_brake()) & BUTTON_LEFT) a->v[0]   /* (co-op: either pad brakes) */ = a->v[0] - 16 < a->v[3] ? a->v[3] : a->v[0] - 16;
            else if (!on) a->v[0] = a->v[0] - 8 < a->v[3] ? a->v[3] : a->v[0] - 8;
            else a->v[0] = a->v[0] + 24 > a->v[2] ? a->v[2] : a->v[0] + 24;
            a->x += a->v[0];
            if (on && a->vy <= 0)
            {
                a->vy = 0;
                a->y = (s32)top << 8;
                if (player.last_held & ~a->t2 & BUTTON_B) { sound_post(0x9F); a->vy = ride_jump[s]; a->y += 1 << 8; }   /* 0x310EA[class] */
                else
                {
                    /* The class's descent (y -= speed >> shift), then follow
                     * the surface: a few px a frame. */
                    u16 k;
                    a->y -= (s32)(a->v[0] >> a->v[4]);
                    for (k = 0; k < 8 && !terrain_floor(AX(a), AY(a) - 2, &top); k++) a->y -= 1 << 8;
                    if (terrain_floor(AX(a), AY(a) - 2, &top)) a->y = (s32)top << 8;
                }
            }
            else
            {
                a->y += a->vy;
                a->vy = a->vy - 64 < -1792 ? -1792 : a->vy - 64;
                if (a->vy < 0 && terrain_floor(AX(a), AY(a), &top)) { a->y = (s32)top << 8; a->vy = 0; }
            }
            a->t2 = player.last_held;
            player.x = a->x; player.y = a->y;
            spray_ride(a);
            ride_gold_ahead();                      /* the pickups ahead */
            if (AX(a) >= 14936)
            {
                Actor *c = actor_new(F_CRASH);
                sound_post(0x9E);
                if (c) { c->x = a->x; c->y = a->y; }
                a->state = 8; a->vy = 1280;
            }
        }
        return;
    case 8:                                 /* flies on and away */
        a->x += 6 << 8;
        a->y += a->vy;
        if (a->vy > 512) a->vy -= 16;
        if (off_cam(a)) actor_free(a);
        return;
    }
}

/* ---- The crash and the end scene (6.14). The snowball carries
 * Madmartigan (chains 0 rolling, 1 breaking, 2 last); then the village: the
 * horse and its rider (F_ENDING chains 2, 3), the results. */
void crash_update(Actor *a)
{
    s16 top;
    if (!a->state) { a->state = 2; a->active = 2; a->timer = 5; a->vy = 0; actor_anim(a, 0); return; }
    actor_step(a);
    switch (a->state)
    {
    case 2:
        a->x += 6 << 8;
        if (!--a->timer) { a->state = 4; coop_throw_off(AX(a), AY(a)); }   /* co-op: the other hero is thrown off */
        break;
    case 4:                                 /* fall, then roll right */
        if (terrain_floor(AX(a), AY(a), &top)) { a->y = (s32)top << 8; a->state = 6; }
        else { a->vy -= 96; a->y += a->vy; }
        break;
    case 6:
        a->x += 3 << 8;
        actor_ground(a, 0, -1);
        if (AX(a) >= 15776) { sound_post(0xA0); a->state = 8; a->timer = 90; actor_anim(a, 1); }
        break;
    case 8:
        if (--a->timer) break;
        a->state = 10; a->timer = 72;
        actor_anim(a, 2);
        break;
    case 10:                                /* the village: the horse and its rider arrive */
        if (--a->timer) break;
        {
            Actor *h = actor_new(F_ENDING);
            if (h) { h->x = (s32)16256 << 8; h->y = (s32)80 << 8; h->variant = 2; }
            h = actor_new(F_ENDING);
            if (h) { h->x = (s32)16256 << 8; h->y = (s32)96 << 8; h->variant = 3; }
        }
        player_script(22);
        player.x = (s32)16162 << 8; player.y = (s32)73 << 8;
        player_script_x = 16162;
        player.cam_max_x = 16384;
        a->state = 12; a->timer = 40 + 90;
        a->active = 0;
        break;
    case 12:
        if (--a->timer) break;
        actor_free(a);
        game_stage_clear();
        return;
    }
    if (a->state < 12) { player.x = a->x; player.y = a->y; }
}

void ending_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 2; actor_anim(a, a->variant); return; }
    actor_step(a);
}

/* ---- Bomb trooper's stage-4 variant (5.3) hooks: src/fam_stage3.c runs
 * the trooper; variant 2 falls in and clears this on its removal. */

/* ---- The intro's horse group (1.3): the horse (chain 0), Willow (1) and
 * the soldier (2); +3 px an update for 192 updates, 32 still, gone. */
void introhorse_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 2; a->timer = 224; actor_anim(a, a->variant); return; }
    actor_step(a);
    if (a->timer > 32) a->x += 3 << 8;
    if (!--a->timer) actor_free(a);
}
