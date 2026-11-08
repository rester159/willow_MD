/* Stage 2's door spearmen (class 4 kind 14 variants 8 and 2, entry 0x177B6;
 * specs/stage2.md 5.6): they wait behind a door until the player comes near,
 * the door opens (class 12 kind 0 variant 7) and they step out; then they are
 * the stage-1 axe thrower (src/fam_spearman.c). Event 3 (0x38C14) sends a new
 * variant 2 from the door at (1320, 200) after each one dies.
 *
 * Chain (tools/pack_stage2.sh "emerge"): 0 stepping out. */
#include <genesis.h>
#include "actor.h"
#include "player.h"

enum { P_WAIT, P_NEAR, P_DOOR, P_OUT };

void emerge_update(Actor *a)
{
    switch (a->phase)
    {
    case P_WAIT:                            /* on screen, then a foot on the floor */
        a->active = 0;
        if ((u16)(AX(a) - CAM_X) > 448 || (u16)(AY(a) - CAM_Y) > 276) return;
        a->state = 2;
        actor_ground(a, 0, -16);
        a->phase = P_NEAR;
        return;
    case P_NEAR:                            /* the player within 48 / 192 */
        if ((u16)((player.y >> 8) - AY(a) + 47) > 94 || (u16)((player.x >> 8) - AX(a) + 191) > 382) return;
        effect(F_DOORFX, AX(a), AY(a), 7);
        a->phase = P_DOOR;
        a->timer = 30;
        return;
    case P_DOOR:
        if (--a->timer) return;
        a->active = 2;
        a->phase = P_OUT;
        a->timer = 30;
        actor_anim(a, 0);
        return;
    default:
        actor_step(a);
        if (--a->timer) return;
        a->fam = F_SPEARMAN;                /* active: the axe thrower's code from its wake-up */
        a->state = 0;
        a->phase = 0;
        return;
    }
}

static u16 door_wait;
static const u8 door_delay[16] = { 240, 240, 240, 240, 240, 240, 210, 210, 210, 180, 150, 120, 90, 60, 60, 30 };   /* 0x38D72 */

void door_event_reset(void) { door_wait = 0; }

/* Event 3: every 8 frames, the door on screen and no door spearman alive. */
void door_event(void)
{
    u16 i;
    if (game_frame & 7) return;
    if ((u16)(1320 - CAM_X) > 384 || (u16)(200 - CAM_Y) > 256) return;
    for (i = pool_lo[C_ENEMY]; i < pool_lo[C_ENEMY + 1]; i++)
        if ((actors[i].fam == F_SPEARMAN || actors[i].fam == F_EMERGE) && actors[i].variant == 2 && !actors[i].spawn)
        {
            door_wait = door_delay[rank >> 12];
            return;
        }
    if (door_wait > 8) { door_wait -= 8; return; }
    {
        Actor *a = actor_new(F_EMERGE);
        if (!a) return;
        a->variant = 2;
        a->x = (s32)1320 << 8;
        a->y = (s32)200 << 8;
    }
}
