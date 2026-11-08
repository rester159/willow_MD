/* Soldier (class 4 kind 0; specs/stage1_enemies.md 5.1, arcade routine
 * 10054 as the specification). Variants: 0 enters from a screen edge
 * (stage event), 4 placed, 8 ambush from above (becomes 4).
 * The ledge climb (phase 4, 0x105DE) is never entered (no phase change leads
 * to it); a level change up is the jump (phase 2), down the dive (phase 6).
 * The jump's wait (0x113FA, per facing) and the dive's crouch and hop
 * (0x11432, 0x1144E, set directly: one pose for both facings) are chains
 * 26-29. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"
#include "game.h"

/* Chains (tools/pack_stage1.sh order): facing 0 / 1. */
#define CH_WALK 0
#define CH_RISE 2       /* look around (search, variant-4 wake) */
#define CH_GUARD 4
#define CH_TURN 6
#define CH_FALL 8
#define CH_CROUCH 14
#define CH_DEATH 18
#define CH_DEATH_LAND 20
#define CH_LEAP 10      /* 0x11466 */
#define CH_LEAP_LAND 12 /* 0x11482 */
#define CH_AMBUSH 22    /* 0x1145A, held: falling */
#define CH_LAND 23      /* 0x1149E, held */
#define CH_JUMP 24
#define CH_JWAIT 26     /* 0x113FA + facing */
#define CH_DCROUCH 28   /* 0x11432, held */
#define CH_HOP 29       /* 0x1144E, held */

enum { P_WALK = 0, P_JUMP = 2, P_DIVE = 6, P_SEARCH = 8, P_FALL = 14, P_TURN = 16, P_DOWN = 20 };

static const u8 stop_delay[8] = { 16, 24, 32, 32, 40, 48, 56, 64 };      /* 0x10420 */
static const u8 search_pause[4] = { 32, 64, 96, 128 };                  /* 0x101F2 */
static const u8 retry_walk[16] = { 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 };   /* 9/16 (0x104A8) */
static const u8 change_level[16] = { 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0 }; /* 6/16 (0x103E4) */
static const u8 drop_100[16] = { 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2 };     /* 0x1114A: 14/16 set 2 */

#define FACE(a) ((a)->facing & 1)
#define DIR(a) (FACE(a) ? -1 : 1)

static void face_player(Actor *a)
{
    a->facing = (player.x >> 8) < AX(a);
}

static void set_chain(Actor *a, u16 ch)
{
    actor_anim(a, ch + (ch < CH_AMBUSH || ch == CH_JWAIT ? FACE(a) : 0));
}

static bool grounded(Actor *a)
{
    /* The arcade's floor test 0x3B0A with probe set 0 (table 0x3B66): one
     * probe at the forward foot, (+-8, -25) from the origin. */
    return actor_ground(a, DIR(a) * 8, -25);
}

static void walk_start(Actor *a, u16 frames)
{
    a->phase = P_WALK;
    a->timer = frames;
    a->sub = 0;
    set_chain(a, CH_WALK);
}

/* Gravity (speed -40 a frame down to -1920); TRUE on landing. */
static bool fall(Actor *a)
{
    a->y += a->vy;
    if (a->vy > -1920) a->vy -= 40;
    return a->vy < 0 && grounded(a);
}

static void die(Actor *a)
{
    sound_post(SND_ENEMY_HIT);
    a->state = 4;
    a->active = 2;
    a->facing = (player.x >> 8) > AX(a);          /* flies away from the player */
    a->vx = FACE(a) ? -384 : 384;
    a->vy = 1024;
    set_chain(a, CH_DEATH);
}

static void remove(Actor *a)
{
    if (a->variant == 0 && edge_alive) edge_alive--;
    actor_free(a);
}

static void explode(Actor *a)
{
    sound_post(SND_EXPLODE);
    effect(F_BURST, AX(a), AY(a), 5);
    remove(a);
}

static void walk_step(Actor *a)
{
    s16 speed = a->variant == 0 ? (FACE(a) ? 512 : 640) : 512;
    s16 top;
    a->x += FACE(a) ? -speed : speed;
    a->y -= 768;
    if (grounded(a)) return;
    /* No floor under the forward foot: step down if there is floor below
     * ahead, otherwise look around. */
    a->y += 768;
    if (terrain_floor(AX(a) + DIR(a) * 16, AY(a) - 33, &top) && !(terrain_flags(AX(a) + DIR(a) * 16, AY(a) - 33) & 0x80))
    {
        a->phase = P_DOWN;
        a->vy = 0;
    }
    else
    {
        a->x -= FACE(a) ? -speed : speed;
        a->phase = P_SEARCH;
        a->timer = search_pause[rnd() & 3];
        set_chain(a, CH_RISE);
    }
}

/* Gravity as 0x1074C: speed -40 then y += speed; at -1920, 7 px a frame. */
static void dive_gravity(Actor *a)
{
    if (a->vy <= -1920) a->y -= 7 << 8;
    else { a->vy -= 40; a->y += a->vy; }
}

static void sink(Actor *a)
{
    sound_post(SND_SINK);
    effect(F_SPLASH, AX(a), AY(a), 0);
    remove(a);
}

/* After a jump or dive: walk (facing the player) on floor, else search. */
static void land_end(Actor *a)
{
    if (grounded(a)) { face_player(a); walk_start(a, 60); }
    else { a->phase = P_SEARCH; a->timer = search_pause[rnd() & 3]; set_chain(a, CH_RISE); }
}

static void jump_step(Actor *a)
{
    switch (a->sub)
    {
    case 0:
        if (!--a->timer) { a->sub = 2; a->timer = 32; a->y -= 1 << 8; set_chain(a, CH_CROUCH); }
        break;
    case 2:
        if (!--a->timer) { a->sub = 4; a->vy = 1408; a->y += 12 << 8; set_chain(a, CH_JUMP); }
        break;
    case 4:                                         /* rising: no sideways motion */
        if (a->vy < 0) { a->sub = 6; break; }
        a->vy -= 48; a->y += a->vy;
        break;
    case 6:
        a->vy -= 48; a->y += a->vy;
        if (grounded(a)) { a->sub = 8; a->timer = 10; set_chain(a, CH_CROUCH); }
        break;
    case 8:
        if (!--a->timer) land_end(a);
        break;
    }
}

/* The leap's motion (0x107AC): gravity, and 2.25 px a frame (0x115D6)
 * until the first wall (probe (+-2, -25)). */
static void dive_move(Actor *a)
{
    dive_gravity(a);
    if (a->t2) return;
    a->x += FACE(a) ? -576 : 576;
    if (terrain_wall(AX(a) + (FACE(a) ? -2 : 2), AY(a) - 25, FACE(a))) a->t2 = 1;
}

static void dive_step(Actor *a, s16 px, s16 py)
{
    switch (a->sub)
    {
    case 0:
        if (!--a->timer) { a->sub = 2; a->timer = 8; set_chain(a, CH_DCROUCH); }   /* 0x11432 */
        break;
    case 2:
        if (!--a->timer) { a->vy = 384; a->sub = 4; set_chain(a, CH_HOP); }        /* 0x1144E */
        else actor_step_i(a);
        break;
    case 4:
        if (a->vy >= 0) { dive_gravity(a); break; }
        a->sub = 6; a->timer = 24; set_chain(a, CH_AMBUSH);
        break;
    case 6:
        if (!--a->timer) a->sub = 8;
        else dive_gravity(a);
        break;
    case 8:
        dive_gravity(a);
        if ((u16)(AY(a) - py + 34) < 68 && (u16)(AX(a) - px + 28) < 56) { a->sub = 12; break; }   /* the player near: leap */
        if (terrain_id(AX(a), AY(a)) == 32) { sink(a); break; }
        if (a->vy < 0 && grounded(a)) { a->sub = 10; a->timer = 18; set_chain(a, CH_LAND); }
        break;
    case 10:
        if (--a->timer) { actor_step_i(a); break; }
        land_end(a);
        break;
    case 12:                                        /* the leap: 3.5 px up, the player's facing, 35 frames */
        a->sub = 14; a->vy = 896; a->facing = player.facing & 1; a->timer = 35; a->t2 = 0;
        set_chain(a, CH_LEAP);
        break;
    case 14:
        if (!--a->timer) { a->sub = 16; break; }
        dive_move(a);
        break;
    case 16:
        dive_move(a);
        if (terrain_id(AX(a), AY(a)) == 32) { sink(a); break; }
        if (a->vy < 0 && grounded(a)) { a->sub = 18; a->timer = 8; set_chain(a, CH_LEAP_LAND); break; }
        if (!((game_frame + (u16)(a - actors)) & 15) &&
            ((u16)(AX(a) - CAM_X + 48) > 480 || (u16)(AY(a) - CAM_Y + 32) > 320)) remove(a);
        break;
    case 18:
        if (!--a->timer) land_end(a);
        break;
    }
}

static void ai(Actor *a)
{
    s16 px = player.x >> 8, py = player.y >> 8, dx = px - AX(a), dy = py - AY(a);
    u16 slot = a - actors;
    switch (a->phase)
    {
    case P_WALK:
        actor_step_i(a);
        walk_step(a);
        if (a->phase != P_WALK) break;
        if (a->sub)                                 /* stopping near the player */
        {
            if (!--a->sub)
            {
                face_player(a);
                if ((dy > 25 || dy < -25) && retry_walk[rnd() & 15]) { a->phase = P_TURN; a->timer = 28; a->t2 = P_SEARCH; set_chain(a, CH_GUARD); }
                else walk_start(a, 60);
            }
            break;
        }
        if (!((game_frame + slot) & 15) && dx < 28 && dx > -28) a->sub = stop_delay[rnd() & 7];
        if (!--a->timer)
        {
            a->timer = 60;
            if ((dy > 25 || dy < -25) && stage_index != 3 && change_level[rnd() & 15])   /* stage 4: never (stage4.md correction 4) */
            {
                face_player(a);
                a->sub = 0; a->timer = 16;          /* 0x10C9C: phase 2 or 6, 16 updates' wait */
                if (dy > 0) { a->phase = P_JUMP; set_chain(a, CH_JWAIT); }
                else { a->phase = P_DIVE; set_chain(a, CH_FALL); }
            }
            else if ((dx < 0) != FACE(a)) { a->phase = P_TURN; a->timer = 8; a->t2 = P_WALK; set_chain(a, CH_TURN); }
        }
        break;
    case P_JUMP:                                    /* 0x104E6: wait, crouch, rise straight up at 5.5 px */
        jump_step(a);
        break;
    case P_DIVE:                                    /* 0x10680: crouch, hop, fall; a leap at a near player */
        dive_step(a, px, py);
        break;
    case P_FALL:
    case P_DOWN:
        actor_step_i(a);
        if (a->phase == P_DOWN) a->x += FACE(a) ? -512 : 512;
        if (fall(a))
        {
            if (a->phase == P_DOWN) face_player(a);
            walk_start(a, 60);
        }
        if (terrain_id(AX(a), AY(a)) == 32)           /* into the water */
        {
            sound_post(SND_SINK);
            effect(F_SPLASH, AX(a), AY(a), 0);
            remove(a);
        }
        break;
    case P_SEARCH:
        actor_step_i(a);
        if (dy < 30 && dy > -30 && (dx < 0) != FACE(a)) { a->phase = P_TURN; a->timer = 8; a->t2 = P_WALK; set_chain(a, CH_TURN); break; }
        if (!--a->timer) { face_player(a); walk_start(a, 60); }
        break;
    case P_TURN:
        actor_step_i(a);
        if (!--a->timer)
        {
            if (a->t2 == P_WALK) { a->facing ^= 1; walk_start(a, 60); }
            else { a->phase = P_SEARCH; a->timer = search_pause[rnd() & 3]; set_chain(a, CH_RISE); }
        }
        break;
    }
}

void soldier_update(Actor *a)
{
    u16 slot = a - actors;
    switch (a->state)
    {
    case 0:                                         /* dormant: wake-up rule per variant */
        a->active = 0;
        if (a->variant == 0)
        {
            a->x = (s32)(a->facing ? CAM_X + 416 : CAM_X - 32) << 8;
            if (stage_index)
            {
                /* Stage 2 (0x100A8): a spawn past the tower moves 212 px left
                 * facing right; none through the tower or the doors. */
                if (AX(a) >= 1536) { a->x -= 212 << 8; a->facing = 0; }
                if (AX(a) > 1488 || (u16)(AX(a) - 744 + 23) <= 46 || ((u16)(AX(a) - 1176 + 23) <= 46 && AY(a) < 88))
                { remove(a); return; }
            }
            a->state = 1;
            a->hp = a->last_hp = 1;
        }
        else if (!((game_frame + slot) & 3))
        {
            s16 sx = AX(a) - CAM_X, sy = AY(a) - CAM_Y;
            if (a->variant == 8)
            {
                s16 dx = (player.x >> 8) - AX(a), dy = (player.y >> 8) - AY(a);
                if (dx < 48 && dx > -48 && dy < 48 && dy > -48)
                {
                    a->y = (s32)(CAM_Y + 264) << 8;
                    a->variant = 4;
                    a->state = 2; a->active = 1;
                    a->hp = a->last_hp = 1;
                    a->phase = P_DIVE; a->sub = 6; a->timer = 40; a->vy = -1;   /* 0x10054: the dive's falling step, 40 updates */
                    set_chain(a, CH_AMBUSH);
                }
            }
            else if ((u16)sx <= 416 && (u16)sy <= 256)
            {
                a->state = 1;
                a->hp = a->last_hp = 1;
            }
        }
        return;
    case 1:                                         /* land: 7 px a frame down to a floor */
        a->y -= 7 << 8;
        if (AY(a) < CAM_Y - 64) { remove(a); return; }
        if (grounded(a))
        {
            {
                s16 t;
                if (terrain_floor(AX(a), AY(a) + 8, &t)) { remove(a); return; }   /* body inside the ground */
            }
            a->state = 2;
            a->active = 1;
            face_player(a);
            if (a->variant == 0) walk_start(a, 60);
            else { a->phase = P_SEARCH; a->timer = search_pause[rnd() & 3]; set_chain(a, CH_RISE); }
        }
        return;
    case 2:
        if (encounter_clear) { explode(a); return; }
        if (a->hp != a->last_hp) { die(a); return; }
        ai(a);
        if (!((game_frame + slot) & 15) && AX(a) < player.cam_min_x) { remove(a); return; }
        {
            s16 sx = AX(a) - CAM_X, sy = AY(a) - CAM_Y;
            if (sx < -48 || sx > 432 || sy < -32 || sy > 288) remove(a);
        }
        return;
    case 4:                                         /* dying: arc away, burst on landing */
        actor_step_i(a);
        a->x += a->vx;
        a->y += a->vy;
        a->vy -= 64;
        if (a->vy < 0)
        {
            if (terrain_id(AX(a), AY(a)) == 32)
            {
                sound_post(SND_SINK);
                effect(F_SPLASH, AX(a), AY(a), 0);
                remove(a);
                return;
            }
            if (grounded(a) || AY(a) < CAM_Y - 32)
            {
                sound_post(SND_EXPLODE);
                effect(F_BURST, AX(a), AY(a), 5);
                a->y += 6 << 8;
                a->state = 6;
                a->timer = 10;
                set_chain(a, CH_DEATH_LAND);
            }
        }
        return;
    case 6:
        if (!--a->timer)
        {
            award(0);
            drop(a, a->variant == 0 ? 0 : drop_100[rnd() & 15]);
            remove(a);
        }
        return;
    }
}
