/* Stage 2's windows (specs/stage2.md 5.4, 5.5): the shutter (class 12
 * kind 10, 0x35D26) that opens once and releases a crossbowman (class 4
 * kind 17, 0x19E92), and the window lurker (class 4 kind 18, 0x1A4E2) with
 * the debris it drops when killed (class 12 kind 9, 0x1A6D4).
 *
 * Chains (tools/pack_stage2.sh): shutter 0 open; crossbow 0 rising, 1-8 aim
 * (aim, shot, hold) by direction group, 9 death; lurker 0 out (to the empty
 * window), 1 out again, 2 the empty window; debris 0. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"

extern u16 aim16(s16 dx, s16 dy);

static bool below(Actor *a, u16 dx_max)       /* the player 0..95 px below, |dx| < dx_max */
{
    s16 dy = AY(a) - (s16)(player.y >> 8), dx = (s16)(player.x >> 8) - AX(a);
    return (u16)dy <= 95 && (u16)(dx + dx_max - 1) <= 2 * dx_max - 2;
}

static void retire(Actor *a, u16 every)
{
    if (!(game_frame & (every - 1)) && AX(a) < player.cam_min_x) actor_free(a);
}

/* ---- Shutter: once, when the player is below, open and release one
 * crossbowman. */
void shutter_update(Actor *a)
{
    if (!a->state)
    {
        a->state = 2;
        a->active = 0;                      /* closed: the window in the scenery */
        return;
    }
    if (a->state == 2)
    {
        if (((game_frame + (a - actors)) & 7) || !below(a, 128)) return;
        {
            Actor *c = actor_new(F_CROSSBOW);
            if (c) { c->x = a->x; c->y = a->y; c->variant = 0; }
        }
        a->state = 4;
        a->active = 2;
        a->timer = 30;
        actor_anim(a, 0);
        return;
    }
    if (a->timer) { a->timer--; actor_step(a); }
    retire(a, 16);
}

/* ---- Crossbowman. */
enum { P_RISE, P_AIM, P_SHOT, P_AFTER };
static const u16 cb_bits[8] = { 0x2442, 0x5555, 0x5555, 0x5555, 0x6D76, 0xDDBA, 0x77BB, 0xFFFF };   /* 0x1A002 */
static const u8 cb_chain[16] = { 1, 1, 1, 1, 1, 2, 3, 4, 4, 5, 6, 7, 8, 8, 8, 8 };              /* 0x1A214 */
static const s8 cb_off[16][2] = {                                                            /* 0x1A070 */
    { 12, -3 }, { 12, -3 }, { 12, -3 }, { 12, -3 }, { 12, -3 }, { 8, -14 }, { 0, -4 }, { -6, -8 },
    { -6, -8 }, { 6, -8 }, { 0, -4 }, { -8, -14 }, { -12, -3 }, { -12, -3 }, { -12, -3 }, { -12, -3 }
};

void crossbow_update(Actor *a)
{
    switch (a->state)
    {
    case 0:
        /* Stage 6 places them (variant 4, stage6.md 4.2): each waits in its
         * window until the player is below, as stage 2's shutter does
         * (approximation: the variant's own wait is not traced). */
        if (a->variant == 4 && (((game_frame + (a - actors)) & 7) || !below(a, 128))) { a->active = 0; return; }
        a->state = 2;
        a->active = 2;                      /* rising: not hittable for 40 frames */
        a->hp = a->last_hp = 1;
        a->phase = P_RISE;
        a->timer = 40;
        actor_anim(a, 0);
        return;
    case 4:                                 /* hit: 16 frames, then the burst */
        actor_step(a);
        if (--a->timer) return;
        sound_post(SND_EXPLODE);
        effect(F_BURST, AX(a), AY(a), 5);
        a->state = 6;
        a->timer = 10;
        a->active = 0;
        return;
    case 6:
        if (--a->timer) return;
        award(4);
        drop(a, rnd() & 3);
        actor_free(a);
        return;
    }
    if (encounter_clear) { sound_post(SND_EXPLODE); effect(F_BURST, AX(a), AY(a), 5); actor_free(a); return; }
    if (a->hp != a->last_hp)
    {
        sound_post(SND_ENEMY_HIT);
        a->state = 4;
        a->active = 2;
        a->timer = 16;
        actor_anim(a, 9);
        return;
    }
    actor_step(a);
    switch (a->phase)
    {
    case P_RISE:
        if (--a->timer) break;
        a->active = 1;
        a->phase = P_AIM;
        a->timer = 1;
        break;
    case P_AIM:
        if (--a->timer) break;
        a->timer = 16;                      /* retry */
        {
            u16 d = aim16((s16)(player.x >> 8) - AX(a), (s16)(player.y >> 8) - AY(a));
            if (!(0x1EF0 >> d & 1)) break;  /* 0x19F7E: directions 4-7, 9-12 */
            actor_anim(a, cb_chain[d]);     /* faces the aim: pose 0x1A214[aim] */
            a->sub = d;
            if (below(a, 128) && (cb_bits[(rank >> 13) & 7] >> (rnd() & 15) & 1))
            {
                a->phase = P_SHOT;
                a->timer = 36;
            }
        }
        break;
    case P_SHOT:
        if (--a->timer) break;
        if ((u16)(AX(a) - CAM_X) <= 384) sound_post(SND_ARROW);
        {
            Actor *s = actor_new(F_ARROW);
            if (s)
            {
                s->x = a->x + ((s32)cb_off[a->sub][0] << 8);
                s->y = a->y + ((s32)cb_off[a->sub][1] << 8);
                s->sub = a->sub;
            }
        }
        a->phase = P_AFTER;
        a->timer = 10;
        break;
    default:
        if (--a->timer) break;
        a->phase = P_AIM;
        a->timer = 16;
        break;
    }
    if (!(game_frame & 31) && AX(a) < player.cam_min_x) actor_free(a);
}

/* ---- Window lurker: leans out when the player is near, hits and is hit
 * for 80 frames, pulls back; again at random (4/16 every 16 frames). */
static const u8 lurk_again[16] = { 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0 };   /* 0x1A5D0 */

static bool lurk_near(Actor *a)
{
    s16 dx = (s16)(player.x >> 8) - AX(a), dy = (s16)(player.y >> 8) - AY(a);
    return (u16)(dx + 95) <= 190 && (u16)(dy + 47) <= 94;
}

void lurker_update(Actor *a)
{
    switch (a->state)
    {
    case 0:                                 /* in the window, unseen */
        a->active = 0;
        if (((game_frame + (a - actors)) & 15) || !lurk_near(a)) return;
        a->state = 2;
        a->active = 2;
        a->hp = a->last_hp = 1;
        a->phase = 0;
        a->timer = 12;
        sound_post(0xA0);
        actor_anim(a, 0);
        return;
    case 4:                                 /* killed: the empty window until retired */
        if (a->timer && !--a->timer)
        {
            sound_post(SND_EXPLODE);
            effect(F_BURST, AX(a), AY(a), 5);
            award(4);
            drop(a, rnd() & 3);
        }
        if (!(game_frame & 63) && AX(a) < player.cam_min_x) actor_free(a);
        return;
    }
    if (a->hp != a->last_hp)
    {
        Actor *d = actor_new(F_DEBRIS);
        if (d) { d->x = a->x; d->y = a->y; }
        sound_post(SND_ENEMY_HIT);
        a->state = 4;
        a->active = 2;
        a->timer = 1;
        actor_anim(a, 2);
        return;
    }
    actor_step(a);
    switch (a->phase)
    {
    case 0:
        if (--a->timer) break;
        a->active = 1;
        a->phase = 2;
        a->timer = 80;
        break;
    case 2:
        if (--a->timer) break;
        a->active = 2;
        a->phase = 4;
        break;
    default:
        if (((game_frame + (a - actors)) & 15) || !lurk_near(a) || !lurk_again[rnd() & 15]) break;
        a->active = 1;
        a->phase = 2;
        a->timer = 36;
        actor_anim(a, 1);
        break;
    }
    if (!(game_frame & 63) && AX(a) < player.cam_min_x) actor_free(a);
}

/* Debris: falls 32 a frame (8.8) for 24 frames. */
void debris_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 2; a->timer = 24; actor_anim(a, 0); return; }
    a->y -= 32 * (25 - a->timer);
    actor_step(a);
    if (!--a->timer) actor_free(a);
}
