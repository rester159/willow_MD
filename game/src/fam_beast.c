/* Beast (class 4 kind 1; specs/stage1_enemies.md 5.2, arcade routine
 * 115EE). Variant 0 patrols from its placed position, variant 1 emerges
 * from a cave (stage event, src/game.c). Energy 2: the first hit knocks it
 * back, the second kills it. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"

/* Roots r (table 0x11FC8 + 4r): chains 2r (facing 0) / 2r + 1. */
#define R_EMERGE 0
#define R_CHARGE 1
#define R_WALK 2
#define R_STAND 3
#define R_SKID 4
#define R_AFTER 5
#define R_TURN 6
#define R_ROAR 7
#define R_HIT 8
#define R_DEATH 9
#define R_RECOVER 10

enum { P_EMERGE, P_WALK = 2, P_TURN = 4, P_PAUSE = 6, P_CHARGE = 8, P_OVERRUN = 10, P_ROAR = 12, P_SKID = 14 };

extern u8 beast_quota[2];

#define FACE(a) ((a)->facing & 1)
#define LO(a) ((a)->v[4])
#define HI(a) ((a)->v[5])

static void root(Actor *a, u16 r)
{
    actor_anim(a, r * 2 + FACE(a));
}

static void ground(Actor *a)
{
    a->y -= 512;
    if (!actor_ground(a, 0, -17))          /* 0x3B0A probe set 4 (table 0x3B66): (0, -17) */
    {
        a->y += 512;
        a->y -= a->vy;                      /* off a ledge: fall (+32 a frame to 1792) */
        if (a->vy < 1792) a->vy += 32;
    }
    else a->vy = 0;
}

static bool in_range(Actor *a, s16 above)
{
    s16 px = player.x >> 8, py = player.y >> 8;
    return AY(a) >= py - above && AY(a) <= py + 8 && px >= LO(a) && px <= HI(a);
}

static void walk(Actor *a)
{
    a->phase = P_WALK;
    a->timer = 120 + (rnd() & 15) * 8;      /* pause timer 0x1169E: 120..240 */
    root(a, R_WALK);
}

static void turn(Actor *a, u16 next)
{
    a->phase = P_TURN;
    a->t2 = next;
    a->sub = 16;
    root(a, R_TURN);
}

static void remove(Actor *a)
{
    if (a->variant == 1 && a->v[3] >= 1 && a->v[3] <= 2) beast_quota[a->v[3] - 1] = 1;
    actor_free(a);
}

void beast_update(Actor *a)
{
    s16 px = player.x >> 8;
    bool toward = (px < AX(a)) == FACE(a);
    switch (a->state)
    {
    case 0:                                 /* dormant until on screen */
        a->active = 0;
        if ((u16)(AX(a) - CAM_X) > 416 || (u16)(AY(a) - CAM_Y + 32) > 320) return;
        a->hp = a->last_hp = 2;
        a->state = 2;
        if (a->variant == 1) { a->phase = P_EMERGE; a->active = 2; a->timer = 24; a->facing = 1; root(a, R_EMERGE); }
        else { LO(a) = AX(a) - 128; HI(a) = AX(a) + 128; a->active = 1; walk(a); }
        return;
    case 2:
        if (a->hp != a->last_hp)
        {
            a->last_hp = a->hp;
            sound_post(SND_ENEMY_HIT);
            a->state = 4;
            a->active = 2;
            a->facing = px > AX(a);         /* slides away from the player */
            a->vx = 768;
            root(a, R_HIT);
            return;
        }
        break;
    case 4:                                 /* knocked back: slide, then recover or die */
        actor_step_i(a);
        a->x += FACE(a) ? -a->vx : a->vx;
        ground(a);
        a->vx -= 24;
        if (a->vx > 0) return;
        if (a->hp) { a->state = 6; a->timer = 6; root(a, R_RECOVER); }
        else
        {
            sound_post(SND_EXPLODE);
            effect(F_BURST, AX(a), AY(a), 4);
            a->state = 8;
            a->timer = 8;
            root(a, R_DEATH);
        }
        return;
    case 6:
        actor_step_i(a);
        if (!--a->timer) { a->state = 2; a->active = 1; walk(a); }
        return;
    case 8:
        actor_step_i(a);
        if (!--a->timer)
        {
            award(2);
            drop(a, (rnd() & 1) ? 1 : 2);
            remove(a);
        }
        return;
    }

    actor_step_i(a);
    switch (a->phase)
    {
    case P_EMERGE:
        if (!--a->timer) { a->x -= 8 << 8; a->active = 1; walk(a); }
        return;
    case P_WALK:
        a->x += FACE(a) ? -384 : 384;
        ground(a);
        if (!(game_frame & 3) && in_range(a, 40)) { if (toward) { a->phase = P_CHARGE; root(a, R_CHARGE); } else turn(a, P_CHARGE); break; }
        if ((FACE(a) && AX(a) <= LO(a)) || (!FACE(a) && AX(a) >= HI(a))) { turn(a, P_WALK); break; }
        if (!--a->timer) { a->phase = P_PAUSE; a->timer = 60; root(a, R_STAND); }
        break;
    case P_TURN:
        if (!--a->sub)
        {
            a->facing ^= 1;
            if (a->t2 == P_CHARGE) { a->phase = P_CHARGE; root(a, R_CHARGE); }
            else walk(a);
        }
        break;
    case P_PAUSE:
        if (!(game_frame & 3) && in_range(a, 40)) { if (toward) { a->phase = P_CHARGE; root(a, R_CHARGE); } else turn(a, P_CHARGE); break; }
        if (!--a->timer) { sound_post(SND_ROAR); a->phase = P_ROAR; a->timer = 58; root(a, R_ROAR); }
        break;
    case P_ROAR:
        if (!--a->timer) walk(a);
        break;
    case P_CHARGE:
        a->x += FACE(a) ? -768 : 768;
        ground(a);
        if ((FACE(a) && AX(a) <= LO(a) + 32) || (!FACE(a) && AX(a) >= HI(a) - 32)) { a->phase = P_SKID; a->vx = 768; root(a, R_SKID); break; }
        if (px - AX(a) <= 8 && AX(a) - px <= 8) { a->phase = P_OVERRUN; a->vx = 768; root(a, R_SKID); break; }
        if (!(game_frame & 3) && !in_range(a, 72)) walk(a);
        break;
    case P_SKID:
        a->x += FACE(a) ? -a->vx : a->vx;
        ground(a);
        a->vx -= 24;
        if (a->vx <= 0) turn(a, P_WALK);
        break;
    case P_OVERRUN:
        a->x += FACE(a) ? -a->vx : a->vx;
        ground(a);
        if (a->vx > 16) a->vx -= 16;
        if ((FACE(a) && AX(a) <= LO(a)) || (!FACE(a) && AX(a) >= HI(a)) || a->vx <= 16)
        {
            a->phase = P_TURN; a->t2 = P_CHARGE; a->sub = 8; root(a, R_AFTER);
        }
        break;
    }
    if (!(game_frame & 63) && AX(a) < player.cam_min_x) remove(a);
}
