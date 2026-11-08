/* Stage 2's wagon and the chase (specs/stage2.md 2.4, 2.6, 6.1; class 6
 * kind 2, 0x2F740): a platform on the road below the tower. Stood on, it
 * starts the chase: the camera holds at 1536 (the actors' strip) while a
 * virtual camera scrolls the road and the far layer at the wagon's speed;
 * the sky darkens with the distance; at distance 28672 the wagon rolls to
 * the centre, breaks down, the road stops and the stage is clear.
 *
 * Parts (scenery, drawn as actors of their own, following the body):
 * rear / front wheel (chains 0 / 1 of the wagon set), the driver, the horse. */
#include <genesis.h>
#include "frontend.h"
#include "stagedata.h"
#include "actor.h"
#include "player.h"
#include "hud.h"
#include "game.h"
#include "stage.h"
#include "sound.h"

#define CHASE_X 1536            /* the actors' camera during the chase */
#define END_X 30208             /* virtual camera at the end (max X 32256 - 2048) */

s16 wagon_step;                 /* FF83E4: the wagon's x step this frame (8.8): flames on its deck ride along */
s32 chase_vx;                   /* FF8398: the virtual camera (8.8); 0 = no chase */
u16 chase_speed;                /* FF83DA: road speed (8.8) */
u8 chase_end;                   /* FF83E2: 1 the end run, 2 the clock stops */
u8 chase_armed;                 /* FF8395: the wagon reached full speed (a restart resumes the chase) */
static u8 sky_step;

static const s16 part_dx[4] = { 24, -37, 56, 112 };
static const s16 part_dy[4] = { -5, -5, 24, 11 };
static const u8 part_fam[4] = { F_WHEEL, F_WHEEL, F_DRIVER, F_HORSE };

/* The drift (0x2F946): +-0.5 px/frame for these many frames, pauses between. */
static const s16 drift[] = { 64, -128, -16, -512, -16, -320, 160, -96, -160, -256 };   /* >0 right, <0 left/pause */
static const u8 drift_move[] = { 1, 0, 2, 0, 2, 0, 1, 0, 2, 0 };                       /* 0 pause, 1 right, 2 left */
static const s8 jolt[32] = { 0, 0, 2, 1, 0, 0, 0, 0, -2, -1, 0, -1, -1, 0, 0, 1,
                             1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const u8 jolt_wait[8] = { 30, 120, 1, 60, 8, 40, 30, 20 };

/* Actor fields: phase, timer (phase timer), v[0] drift step, v[1] drift
 * frames left, v[2] jolt wait, v[3] jolt index, v[4] loop count, v[5] the
 * resting height (the jolts are bumps from it). */

static void move_by(Actor *a, s16 dx, s16 dy)          /* 8.8: the body, and the player on its deck */
{
    bool on = !player.airborne && (u16)((s16)(player.x >> 8) - AX(a) + 80) <= 160 && (s16)(player.y >> 8) == AY(a) + 32;
    a->x += dx;
    wagon_step += dx;
    a->y += dy;
    if (on) { player.x += dx; player.y += dy; }
}

void wagon_update(Actor *a)
{
    wagon_step = 0;
    switch (a->state)
    {
    case 0:
    {
        u16 k;
        a->state = 2;
        a->active = 1;
        a->pose = 0;
        a->phase = 0;
        a->v[5] = AY(a);                    /* resting height */
        if (!chase_armed) chase_vx = 0;     /* a restart keeps the resumed distance (game.c) */
        chase_speed = 0;
        chase_end = 0;
        sky_step = 0;
        for (k = 0; k < 4; k++)
        {
            Actor *p = actor_new(part_fam[k]);
            if (!p) continue;
            p->link = (a - actors) + 1;
            p->v[0] = part_dx[k];
            p->v[1] = part_dy[k];
            p->variant = k;
            p->x = a->x + ((s32)part_dx[k] << 8);
            p->y = a->y + ((s32)part_dy[k] << 8);
        }
        return;
    }
    }
    if (chase_vx)
    {
        u16 s;
        chase_vx += chase_speed;
        if ((chase_vx >> 8) >= 1760) stage_chase_lend();   /* once (posecache_lend ignores repeats) */
        s = (u16)((chase_vx >> 8) - CHASE_X) / 3072;
        if (s > 9) s = 9;
        if (s != sky_step) { sky_step = s; stage_far_step(s); }
    }
    switch (a->phase)
    {
    case 0:                             /* wait until stood on, then the chase (2.4) */
        if (!(a->flags & STOOD)) return;
        a->flags &= ~STOOD;
        if (chase_armed)
        {
            /* A restart (FF8395): at full speed at once, from the resumed distance. */
            stage_chase(TRUE);
            actors_retire_below(CHASE_X);
            chase_speed = 1536;
            a->phase = 4; a->v[0] = 0; a->v[1] = 64; a->v[2] = jolt_wait[rnd() & 7];
            return;
        }
        chase_vx = (s32)CHASE_X << 8;
#ifdef CHASE_AT
        chase_vx = (s32)(CHASE_AT) << 8;    /* test builds: the chase from this virtual distance */
#endif
        player.cam_min_x = player.cam_max_x = CHASE_X;
        stage_chase(TRUE);
        actors_retire_below(CHASE_X);   /* the fort behind is cleared (2.5) */
        a->phase = 2;
        return;
    case 2:                             /* accelerate to 6 px/frame */
        chase_speed += 4;
        if (chase_speed >= 1536) { chase_speed = 1536; chase_armed = 1; a->phase = 4; a->v[0] = 0; a->v[1] = 64; a->v[2] = jolt_wait[rnd() & 7]; }
        return;
    case 4:                             /* ride: drift and jolts */
#ifndef INVINCIBLE
        /* 0x2FE5A: Madmartigan down on the road dies there (2.9). */
        if (player.form && !player.dead && !fe.invincible && (s16)(player.y >> 8) <= 64) player_road_death();
#endif
        if (drift_move[a->v[0]]) move_by(a, drift_move[a->v[0]] == 1 ? 128 : -128, 0);
        if (!--a->v[1])
        {
            if (++a->v[0] == sizeof drift_move) { a->v[0] = 0; a->v[4]++; }
            a->v[1] = drift[a->v[0]] < 0 ? -drift[a->v[0]] : drift[a->v[0]];
            if (a->v[0] == 0 && a->v[4]) a->v[1] = 32;
        }
        if (a->v[2]) a->v[2]--;
        else
        {
            move_by(a, 0, (((s32)(a->v[5] + jolt[a->v[3]]) << 8) - a->y));     /* a bump from its resting height */
            if (++a->v[3] == 32) { a->v[3] = 0; a->v[2] = jolt_wait[rnd() & 7]; }
        }
        if ((chase_vx >> 8) >= END_X)
        {
            chase_end = 1;
            hud.clock_lock = 1;
            a->phase = 6;
        }
        return;
    case 6:                             /* to the screen centre at 1 px/frame */
    {
        s16 c = CHASE_X + 192 - AX(a);
        if (c) { move_by(a, c > 0 ? 256 : -256, 0); return; }
        a->phase = 8;
        a->timer = 32;
        return;
    }
    case 8:
        if (--a->timer) return;
        chase_end = 2;
        if (!player.airborne && !player.dead && player.form) player_script(16);   /* 0x39C2A: the chase-end walk */
        a->phase = 10;
        a->vy = 0;
        return;
    case 10:                            /* breaks down onto the road */
    {
        s16 top;
        if (a->vy < 1536) a->vy += 48;
        a->y -= a->vy;
        if (terrain_floor(AX(a), AY(a), &top)) { a->y = (s32)top << 8; a->phase = 12; }
        return;
    }
    case 12:                            /* the road stops */
        if (chase_speed > 5) { chase_speed -= 5; return; }
        chase_speed = 0;
        a->phase = 14;
        a->timer = 128;
        return;
    case 14:
        if (--a->timer) return;
        a->phase = 16;
        game_stage_clear();
        return;
    }
}

/* Wheels, driver, horse: on the body at their offsets, animating. The
 * horse's hooves (sound 0x39) every 64 updates while the chase runs. When
 * the clock stops at the end (FF83E2 = 2) the wheels and the horse come
 * loose (stage2.md 6.4): a wheel drops to its resting height and rolls back
 * from 1 px an update, + 0.25 px each to 6 (0x2FD10); the horse gallops on
 * at 2 px an update, hooves every 32 (0x3007E); both go off camera (0x2162). */
void wagon_part_update(Actor *a)
{
    Actor *w = actors + a->link - 1;
    if (!a->state)
    {
        a->state = 2;
        a->active = 2;
        actor_anim(a, a->variant == 1 ? 1 : 0);
    }
    if (a->phase)
    {
        if (a->fam == F_HORSE)
        {
            if (!(game_frame & 31)) sound_post(0x39);
            actor_step(a);
            a->x += 2 << 8;
        }
        else
        {
            actor_step(a);
            a->x -= a->vx;
            a->vx = a->vx + 64 > 1536 ? 1536 : a->vx + 64;
        }
        if ((u16)(AX(a) - CAM_X + 48) > 480 || (u16)(AY(a) - CAM_Y + 32) > 320) actor_free(a);
        return;
    }
    if (!a->link || w->fam != F_WAGON) { actor_free(a); return; }
    /* The wheels (0x2FC3A): when the end run starts (FF83E2 set), a burst
     * (class 12 kind 0 variant 5, 0x492C) at each, once. */
    if (chase_end && a->fam == F_WHEEL && !(a->flags & 1))
    {
        a->flags |= 1;
        effect(F_BURST, AX(a), AY(a), 5);
    }
    if (chase_end == 2 && a->fam != F_DRIVER)
    {
        a->phase = 2;
        if (a->fam == F_WHEEL) { a->y = (s32)(w->v[5] + a->v[1]) << 8; a->vx = 256; }
        return;
    }
    if (a->fam == F_HORSE && chase_vx && !(game_frame & 63)) sound_post(0x39);
    a->x = w->x + ((s32)a->v[0] << 8);
    a->y = w->y + ((s32)a->v[1] << 8);
    actor_step(a);
}
