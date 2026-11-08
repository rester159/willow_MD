/* Stage 2's crate archer (class 4 kind 16, 0x1995E; specs/stage2.md 5.3):
 * hidden behind a crate, it pops up at random, aims, shoots an arrow along
 * the floor and ducks again. Energy 1, score 200, drop set rand & 3.
 *
 * Chains (tools/pack_stage2.sh "cratearcher"), right / left: 0 pop-up, 2 aim,
 * 4 recoil, 6 duck, 8 fall (dying), 10 landing. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"

enum { C_POP = 0, C_AIM = 2, C_RECOIL = 4, C_DUCK = 6, C_FALL = 8, C_LAND = 10 };
enum { P_HIDDEN, P_POP, P_AIM, P_RECOIL, P_DUCK };

/* Pop-up chance per rank row (0x19A76): bit (rand & 15) set. */
static const u16 pop_bits[8] = { 0x8108, 0x4445, 0x5551, 0xA95B, 0xAD5B, 0xAD5B, 0x57BB, 0xCDBD };

static void chain(Actor *a, u16 c) { actor_anim(a, c + (a->facing & 1)); }
static void face_player(Actor *a) { a->facing = (player.x >> 8) < AX(a); }

void cratearcher_update(Actor *a)
{
    u16 slot = a - actors;
    switch (a->state)
    {
    case 0:                                 /* dormant until the player is at its height */
        a->active = 0;
        if ((game_frame + slot) & 15) return;
        if ((u16)((player.y >> 8) - AY(a) + 30) > 60) return;
        a->state = 2;
        a->hp = a->last_hp = 1;
        a->phase = P_HIDDEN;
        return;
    case 4:                                 /* dying: up, then down to the floor */
        a->x += a->facing ? -384 : 384;
        a->y += a->vy;
        if (a->vy > -1920) a->vy -= 64;
        if (a->vy < 0 && actor_ground(a, 0, -8))
        {
            a->state = 6;
            a->timer = 10;
            chain(a, C_LAND);
        }
        else if (a->vy < 0 && !(game_frame & 15) && (u16)(AY(a) - CAM_Y + 64) > 352) actor_free(a);
        return;
    case 6:
        if (--a->timer) return;
        sound_post(SND_EXPLODE);
        effect(F_BURST, AX(a), AY(a), 5);
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
        a->vy = 1024;
        a->facing = player.facing & 1;      /* flies the way the player faces */
        chain(a, C_FALL);
        return;
    }
    if (a->phase != P_HIDDEN) actor_step(a);
    switch (a->phase)
    {
    case P_HIDDEN:
        if ((game_frame + slot) & 15) break;
        face_player(a);
        if ((u16)((player.x >> 8) - AX(a) + 159) <= 318 && (pop_bits[(rank >> 13) & 7] >> (rnd() & 15) & 1))
        {
            a->active = 1;
            a->phase = P_POP;
            a->timer = 18;
            chain(a, C_POP);
        }
        break;
    case P_POP:
        if (--a->timer) break;
        a->phase = P_AIM;
        a->timer = 20;
        chain(a, C_AIM);
        break;
    case P_AIM:
        if (--a->timer) break;
        if ((u16)(AX(a) - CAM_X) <= 384) sound_post(SND_ARROW);
        {
            Actor *s = actor_new(F_ARROW);
            if (s)
            {
                s->x = a->x + ((a->facing ? -16 : 16) << 8);
                s->y = a->y + (14 << 8);
                s->sub = a->facing ? 12 : 4;
            }
        }
        a->phase = P_RECOIL;
        a->timer = 40;
        chain(a, C_RECOIL);
        break;
    case P_RECOIL:
        if (--a->timer) break;
        a->phase = P_DUCK;
        a->timer = 18;
        chain(a, C_DUCK);
        break;
    default:
        if (--a->timer) break;
        a->active = 0;                      /* hidden again (empty pose 0x19E7E) */
        a->phase = P_HIDDEN;
        break;
    }
}
