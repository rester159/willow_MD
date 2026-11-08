/* Stage 2's falling chandelier (class 4 kind 2, 0x1237A) and its flames
 * (class 10 kind 12, 0x24152); specs/stage2.md 5.2.
 *
 * The chandelier is never hit and never hurts; it falls when the player is
 * under it, crashes and spreads five flames that hurt for 36 frames.
 * Chains (tools/pack_stage2.sh "chandelier"): 0/1 hanging in palette 19 / 20,
 * 2/3 crash -> wreck in 19 / 20 (the code flips them every 8 frames). */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"
#include "game.h"

enum { P_WAIT, P_DELAY, P_FALL, P_WRECK };
static const u8 delay[4] = { 1, 4, 2, 6 };          /* 0x12438 */

/* Actor fields: phase, timer, v[0] crashed (chain base 2), v[1] spread step. */

static void flicker(Actor *a)
{
    if (!(game_frame & 7)) actor_anim(a, a->v[0] + ((game_frame >> 3) & 1));
}

static void flame_at(s16 x, s16 y)
{
    Actor *f = actor_new(F_FLAME);
    if (!f) return;
    f->x = (s32)x << 8;
    f->y = (s32)y << 8;
}

void chandelier_update(Actor *a)
{
    s16 px = player.x >> 8, py = player.y >> 8;
    if (!a->state)
    {
        a->state = 2;
        a->active = 2;                      /* drawn only: no hits either way */
        a->phase = P_WAIT;
        a->v[0] = 0;
        actor_anim(a, 0);
        return;
    }
    flicker(a);
    switch (a->phase)
    {
    case P_WAIT:                            /* the player under it (H 64, variant 1: 128) */
    {
        s16 d = AY(a) - (a->variant ? 128 : 64) - py;
        if ((u16)(px - AX(a) + 95) <= 190 && (u16)(d + 32) <= 64)
        {
            a->phase = P_DELAY;
            a->timer = delay[rnd() & 3];
        }
        break;
    }
    case P_DELAY:
        if (!--a->timer) { a->phase = P_FALL; a->vy = 0; }
        break;
    case P_FALL:
    {
        s16 top;
        if (a->vy < 1792) a->vy += 32;
        a->y -= a->vy;
        if (terrain_floor(AX(a), AY(a), &top))
        {
            a->y = (s32)top << 8;
            effect(F_BURST, AX(a), AY(a), 4);
            sound_post(0x3A);
            sound_post(0xA1);
            a->v[0] = 2;
            actor_anim(a, 2 + ((game_frame >> 3) & 1));
            a->phase = P_WRECK;
            a->timer = 16;
            a->v[1] = 0;
        }
        break;
    }
    default:                                /* fire spreads: x, then +-16, then +-32 */
        actor_step(a);
        if (--a->timer) break;
        switch (a->v[1]++)
        {
        case 0: flame_at(AX(a), AY(a)); a->timer = 8; break;
        case 1: flame_at(AX(a) - 16, AY(a)); flame_at(AX(a) + 16, AY(a)); a->timer = 8; break;
        default: flame_at(AX(a) - 32, AY(a)); flame_at(AX(a) + 32, AY(a)); actor_free(a); break;
        }
        break;
    }
}

/* A flame (also the sorcerer's ground fire, class 10 kind 4: the same
 * routine): hurts for 36 frames, then 8 frames harmless; rides the wagon's
 * step (0 in the fort); removed off camera every 16 frames. */
void flame_update(Actor *a)
{
    extern s16 wagon_step;
    if (!a->state)
    {
        a->state = 2;
        a->active = 1;
        a->timer = 44;
        sound_post(0x37);
        actor_anim(a, 0);
        return;
    }
    if (stage_index == 1) a->x += wagon_step;   /* stage 1's ground-fire flames stand still */
    actor_step(a);
    if (a->timer == 9) a->active = 2;
    if (!--a->timer) { actor_free(a); return; }
    if (!(game_frame & 15) && ((u16)(AX(a) - CAM_X + 32) > 448 || (u16)(AY(a) - CAM_Y + 32) > 320)) actor_free(a);
}
