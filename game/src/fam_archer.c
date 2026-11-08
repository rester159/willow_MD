/* Archer (class 4 kind 3 variant 2) and its arrow (class 10 kind 0);
 * specs/stage1_enemies.md 5.9, arcade routines 125D0 and 2359C. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"

/* Archer chains (tools/pack_stage1.sh). */
#define C_WALK 0
#define C_STAND 4
#define C_PREPARE 6
#define C_DRAW 8
#define C_RECOVER 12
#define C_AIM 14        /* + direction 0..15 */
#define C_FIRE 30       /* + direction */
#define C_DEATH_AIR 46
#define C_DEATH_LAND 48

enum { P_WALK, P_STAND, P_PREPARE, P_DRAW, P_AIM, P_FIRED, P_HIT };

/* 16 directions (0 up, 4 right, 8 down, 12 left), 4 px a frame (0xED524). */
static const s16 dir_v[16][2] = {
    { 0, 1024 }, { 391, 945 }, { 723, 723 }, { 945, 391 }, { 1024, 0 }, { 945, -391 }, { 723, -723 }, { 391, -945 },
    { 0, -1024 }, { -391, -945 }, { -723, -723 }, { -945, -391 }, { -1024, 0 }, { -945, 391 }, { -723, 723 }, { -391, 945 }
};
/* Arrow start offset from the archer per direction (0x12E46). */
static const s8 bow_off[16][2] = {
    { 0, 8 }, { 0, 8 }, { 4, 4 }, { 6, 7 }, { 10, 4 }, { 4, 0 }, { 0, 10 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 10 }, { -4, 0 }, { -10, 4 }, { -6, 7 }, { -4, 4 }, { 0, 8 }
};

/* 16-direction aim from (dx, dy) (y up), by the angle's tangent. */
u16 aim16(s16 dx, s16 dy)
{
    u16 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, o;
    /* octant step: tan 11.25 = 0.199, 33.75 = 0.668, 56.25 = 1.497, 78.75 = 5.03 */
    if (ay * 5 <= ax) o = 4;
    else if (ay * 3 <= ax * 2) o = 3;
    else if (ay * 2 <= ax * 3) o = 2;
    else if (ay <= ax * 5) o = 1;
    else o = 0;
    /* o: steps from vertical toward horizontal (0 = vertical, 4 = horizontal) */
    if (dx >= 0) return dy >= 0 ? o : 8 - o;
    return dy >= 0 ? (16 - o) & 15 : 8 + o;
}

#define FACE(a) ((a)->facing & 1)

static void chain(Actor *a, u16 c)
{
    actor_anim(a, c < C_AIM || c >= C_DEATH_AIR ? c + FACE(a) : c);
}

static bool grounded(Actor *a)
{
    /* The arcade's floor test 0x3B0A (table 0x3B66): probe set 3, the forward
     * foot at (+-12, -24); set 1, (+-8, -16), once hit and falling. */
    if (a->state == 4) return actor_ground(a, FACE(a) ? -8 : 8, -16);
    return actor_ground(a, FACE(a) ? -12 : 12, -24);
}

static void face_player(Actor *a)
{
    a->facing = (player.x >> 8) < AX(a);
}

static void walk(Actor *a)
{
    face_player(a);
    a->phase = P_WALK;
    a->timer = 60;
    chain(a, C_WALK);
}

void archer_update(Actor *a)
{
    s16 px = player.x >> 8, py = player.y >> 8, dx = px - AX(a), dy = py - AY(a);
    switch (a->state)
    {
    case 0:
        a->active = 0;
        if ((u16)(AX(a) - CAM_X) > 384 || (u16)(AY(a) - CAM_Y) > 288) return;
        a->hp = a->last_hp = 4;
        a->state = 1;
        return;
    case 1:                                         /* land */
        a->y -= 7 << 8;
        if (grounded(a)) { a->state = 2; a->active = 1; walk(a); }
        else if (AY(a) < CAM_Y - 64) actor_free(a);
        return;
    case 4:                                         /* dying: fall, burst */
        a->y += a->vy;
        a->vy -= 64;
        if (a->vy < 0 && grounded(a))
        {
            sound_post(SND_EXPLODE);
            effect(F_BURST, AX(a), AY(a), 5);
            a->state = 6;
            a->timer = 10;
            chain(a, C_DEATH_LAND);
        }
        return;
    case 6:
        if (!--a->timer)
        {
            award(0);
            drop(a, (rnd() & 15) < 14 ? 1 : 2);
            actor_free(a);
        }
        return;
    }
    if (encounter_clear) { sound_post(SND_EXPLODE); effect(F_BURST, AX(a), AY(a), 5); actor_free(a); return; }
    if (a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        sound_post(SND_ENEMY_HIT);
        if (!a->hp) { a->state = 4; a->active = 2; a->vy = 512; chain(a, C_DEATH_AIR); return; }
        a->phase = P_HIT;
        a->timer = 16;
        chain(a, C_RECOVER);
        return;
    }
    {
        s16 sx = AX(a) - CAM_X;
        if (sx < -64 || sx > 448 || AY(a) - CAM_Y > 288) return;   /* frozen off screen */
    }
    actor_step(a);
    switch (a->phase)
    {
    case P_WALK:
        a->x += FACE(a) ? -256 : 256;
        a->y -= 512;
        if (!grounded(a)) { a->y += 512; a->x -= FACE(a) ? -256 : 256; a->facing ^= 1; chain(a, C_WALK); }
        if (!--a->timer) { a->phase = P_STAND; a->timer = 56; chain(a, C_STAND); }
        /* fall through */
    case P_STAND:
        if (a->phase == P_STAND && !--a->timer) walk(a);
        if (!(game_frame & 7) && dy <= 48 && dy >= -48 && dx <= 160 && dx >= -160)
        {
            face_player(a);
            a->phase = P_PREPARE;
            a->timer = 8;
            chain(a, C_PREPARE);
        }
        break;
    case P_PREPARE:
        if (a->timer == 4) chain(a, C_DRAW);
        if (!--a->timer) { a->phase = P_DRAW; a->timer = 48; a->sub = FACE(a) ? 12 : 4; }
        break;
    case P_DRAW:
        if (!--a->timer) { a->phase = P_AIM; a->timer = 8; a->t2 = 0; chain(a, C_AIM + a->sub); }
        break;
    case P_AIM:                                     /* the bow turns a step every 8 frames */
        {
            u16 want = aim16(dx, dy);
            if (want == 0) want = FACE(a) ? 15 : 1;
            if (want == 8) want = FACE(a) ? 9 : 7;
            if (--a->timer) break;
            a->timer = 8;
            if (want != a->sub)
            {
                a->sub = (a->sub + (((want - a->sub) & 15) < 8 ? 1 : 15)) & 15;
                chain(a, C_AIM + a->sub);
                break;
            }
            if ((a->t2 += 8) < 16) break;
            sound_post(SND_ARROW);
            {
                Actor *s = actor_new(F_ARROW);
                if (s)
                {
                    s->x = a->x + ((s32)bow_off[a->sub][0] << 8);
                    s->y = a->y + ((s32)bow_off[a->sub][1] << 8);
                    s->sub = a->sub;
                }
            }
            a->phase = P_FIRED;
            a->timer = 16;
            chain(a, C_FIRE + a->sub);
        }
        break;
    case P_FIRED:
        if (!--a->timer) walk(a);
        break;
    case P_HIT:
        if (!--a->timer) { a->active = 1; walk(a); }
        break;
    }
}

/* An arrow breaks (12 frames): a floor or wall, or Madmartigan's sword. */
void arrow_break(Actor *a)
{
    a->state = 4;
    a->active = 2;
    a->timer = 12;
    actor_anim(a, 16 + a->sub);
}

/* Arrow: 4 px a frame along its direction, 64 frames; a floor or wall at
 * its tip (every 4 frames) breaks it (12 frames). */
void arrow_update(Actor *a)
{
    switch (a->state)
    {
    case 0:
        a->state = 2;
        a->timer = 64;
        a->active = 1;
        actor_anim(a, a->sub);
        return;
    case 2:
        a->x += dir_v[a->sub][0];
        a->y += dir_v[a->sub][1];
        if (!--a->timer || encounter_clear) { actor_free(a); return; }
        if (!(game_frame & 3))
        {
            s16 t, tx = AX(a) + (dir_v[a->sub][0] >> 7), ty = AY(a) + (dir_v[a->sub][1] >> 7);
            u8 f = terrain_flags(tx, ty);
            if (terrain_floor(tx, ty, &t) || (f & 3)) arrow_break(a);
        }
        if (!(game_frame & 15) && ((u16)(AX(a) - CAM_X + 32) > 448 || (u16)(AY(a) - CAM_Y + 32) > 320)) actor_free(a);
        return;
    case 4:
        actor_step(a);
        if (!--a->timer) actor_free(a);
        return;
    }
}
