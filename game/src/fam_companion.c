/* The hidden companion (specs/stage1_shops_companion.md 2; class 16 kind 0,
 * routine 0x36D7E): freed from the hidden object, it hops in place until
 * Willow comes near (one Quest-Stone is spent if she has one), then follows
 * her for the rest of the stage and throws spears when she attacks.
 *
 * Chains (tools/pack_stage1.sh "companion"): 0 blink (0x3784E), 1 bounce
 * (0x37868), then right / left pairs: 2-3 walk (0x37656), 4-5 jump (0x3765A),
 * 6-7 roll (0x3765E), 8-9 wind-up (0x37662), 10-11 throw (0x37666), 12-13
 * air throw (0x3766A). Units as the other actors: world px, y up, 8.8. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "hud.h"
#include "shot.h"

extern u16 game_pressed;            /* src/game.c: buttons newly pressed this update */

enum { C_WALK = 2, C_JUMP = 4, C_ROLL = 6, C_WIND = 8, C_THROW = 10, C_AIR = 12 };
enum { P_FOLLOW, P_RUN = 2, P_LEAP = 4, P_HOP = 6, P_DASH = 8, P_THROW = 10 };
static const s16 motion[5] = { 128, 256, 384, 512, 640 };     /* 0xED060: x speed per motion */

/* Actor fields: state (0 freed, 2 companion), phase, sub (action / step),
 * facing, vx (horizontal 8.8 for leaps), vy (vertical speed, up +),
 * v[0] motion, v[1] latch, v[2] air throw countdown, v[3] rising flag,
 * v[4] vy of a leap, timer. */

static bool floor_at(s16 x, s16 y, s16 *top)
{
    return terrain_floor(x, y, top) || surface_floor(x, y, top);
}

static void chain(Actor *a, u16 c)          /* a right / left chain by facing (v[5]: which) */
{
    a->v[5] = c;
    actor_anim(a, c + (a->facing & 1));
}

static void face(Actor *a, u16 f)            /* turning picks the other facing's chain */
{
    if ((a->facing & 1) == (f & 1)) return;
    a->facing = f & 1;
    if (a->v[5]) chain(a, a->v[5]);
}

static s16 dx_player(Actor *a) { return (s16)(player.x >> 8) - AX(a); }
static s16 dy_player(Actor *a) { return (s16)(player.y >> 8) - AY(a); }

static void walk(Actor *a)
{
    s16 v = motion[a->v[0]];
    a->x += (a->facing & 1) ? -v : v;
}

static void to_follow(Actor *a)
{
    a->phase = P_FOLLOW;
    a->sub = 0;
    a->v[0] = 2;
    chain(a, C_WALK);
}

/* The ground d px below: TRUE (snapped) or FALSE (nothing there). */
static bool probe(Actor *a, s16 d)
{
    s16 top;
    if (!floor_at(AX(a), AY(a) - d, &top)) return FALSE;
    a->y = (s32)top << 8;
    return TRUE;
}

static void throw_spear(Actor *a)
{
    shot_spear(AX(a), AY(a), a->facing & 1);
}

/* Rise / fall of a leap or hop; TRUE on landing. */
static bool airborne(Actor *a, s16 land_from)
{
    s16 top;
    a->x += a->vx;
    if (a->v[3])
    {
        a->y += a->v[4];
        a->y += a->vy;
        a->vy -= 32;
        if (a->vy < 0) { a->v[3] = 0; a->vy = 0; }
        return FALSE;
    }
    a->y -= a->vy;
    if (a->vy < 1536) a->vy += 32;
    if (a->vy < land_from) return FALSE;
    if (!floor_at(AX(a), AY(a), &top)) return FALSE;
    a->y = (s32)top << 8;
    return TRUE;
}

static void air_throw(Actor *a)              /* 0x3742A: in leaps and hops */
{
    if (a->v[2])
    {
        if (--a->v[2] == 5) throw_spear(a);
        if (!a->v[2]) chain(a, C_JUMP);
        return;
    }
    if ((game_pressed & BUTTON_A) && (u16)(dx_player(a) + 96) <= 192 && (u16)(dy_player(a) + 96) <= 192)
    {
        a->v[2] = 7;                        /* 2 updates, then the throw pose for 5 */
        chain(a, C_AIR);
    }
}

void companion_update(Actor *a)
{
    u16 cad = game_frame + 19 - (a - actors - pool_lo[C_OBJECT]);
    s16 dx, dy;
    if (a->state == 0)
    {
        /* Freed, hopping in place until Willow comes near. */
        if (a->phase == 0)
        {
            a->vy = 0;
            a->facing = 0;
            a->active = 2;
            a->v[5] = 0;
            actor_anim(a, 0);
            a->phase = 2;
        }
        dx = dx_player(a); dy = dy_player(a);
        if ((u16)(dx + 24) <= 48 && (u16)(dy + 28) <= 48) a->v[1] = 1;
        if (a->v[1])
        {
            if (hud.items[3]) { hud.items[3]--; hud_dirty |= HUD_ITEMS; }   /* one Quest-Stone spent */
            a->state = 2;
            a->facing = player.facing & 1;
            to_follow(a);
            actor_step(a);
            return;
        }
        if (a->phase == 2)
        {
            s16 top;
            if (floor_at(AX(a), AY(a), &top)) { a->y = (s32)top << 8; a->phase = 4; a->vy = 768; actor_anim(a, 1); }
            else
            {
                a->y -= a->vy;
                if (a->vy < 1664) a->vy += 32;
            }
        }
        else
        {
            s16 top;
            a->y += a->vy;
            if (a->vy > -1792) a->vy -= 48;
            if (a->vy < 0 && floor_at(AX(a), AY(a), &top)) { a->y = (s32)top << 8; a->vy = 768; actor_anim(a, 1); }
        }
        actor_step(a);
        return;
    }

    if (AY(a) < 0)
    {
        /* Fallen out of the stage (a pit): back at Willow's side. Not from
         * the arcade (what it does there is not established). */
        a->x = player.x - ((player.facing & 1) ? -24 : 24) * 256;
        a->y = player.y;
        a->facing = player.facing & 1;
        to_follow(a);
    }
    dx = dx_player(a); dy = dy_player(a);
    switch (a->phase)
    {
    case P_FOLLOW:
        if (a->sub == 0)                    /* Willow walking */
        {
            if (player.facing & 2) { a->sub = 2; a->v[0] = 1; }
            else
            {
                face(a, player.facing);
                a->v[0] = 2;
                walk(a);
                actor_step(a);
                actor_step(a);
            }
        }
        else                                /* Willow standing: pace around her */
        {
            if (!(cad & 31) && (u16)(dx + 40) > 80) face(a, dx < 0);
            if (!(cad & 7) && !(player.facing & 2)) { a->sub = 0; a->v[0] = 2; face(a, player.facing); }
            else { a->v[0] = 1; walk(a); actor_step(a); }
        }
        if (!(cad & 15) && (u16)(dx + 72) > 144)
        {
            if (rnd() & 3) { a->phase = P_RUN; a->v[0] = 4; face(a, dx < 0); chain(a, C_WALK); }
            else { a->phase = P_DASH; a->v[0] = 4; a->timer = 32; face(a, dx < 0); chain(a, C_ROLL); }
            break;
        }
        if (!player.airborne && (u16)(dy + 48) > 96)
        {
            a->phase = P_LEAP;
            a->vy = 1024; a->v[3] = 1;
            a->vx = dx * 4; a->v[4] = dy * 4;
            a->facing = a->vx < 0;
            chain(a, C_JUMP);
            break;
        }
        if (cad & 1)
        {
            if (!probe(a, 3)) { face(a, dx < 0); a->phase = P_HOP; a->vy = 512; a->v[3] = 1; a->v[0] = (u16)(dx + 64) > 128 ? 3 : 0;
                                a->vx = (a->facing & 1) ? -motion[a->v[0]] : motion[a->v[0]]; a->v[4] = 0; chain(a, C_JUMP); break; }
        }
        if (game_pressed & BUTTON_A) { a->phase = P_THROW; a->timer = 4; chain(a, C_WIND); }
        break;
    case P_RUN:
        face(a, dx < 0);
        walk(a);
        actor_step(a); actor_step(a); actor_step(a);
        if (!probe(a, 4)) { a->y -= 4 << 8; a->phase = P_HOP; a->vy = 0; a->v[3] = 0; a->vx = 0; chain(a, C_JUMP); break; }
        if ((u16)(dx + 40) <= 80) to_follow(a);
        break;
    case P_LEAP:
    case P_HOP:
        air_throw(a);
        actor_step(a);
        if (airborne(a, a->phase == P_LEAP ? 896 : 0)) to_follow(a);
        break;
    case P_DASH:
        if (a->timer > 6)
        {
            walk(a);
            if (!probe(a, 4)) { a->y -= 4 << 8; a->phase = P_HOP; a->vy = 0; a->v[3] = 0; a->vx = 0; chain(a, C_JUMP); break; }
        }
        actor_step(a);
        if (!--a->timer)
        {
            if ((u16)(dx + 40) > 80) { a->phase = P_RUN; a->v[0] = 4; chain(a, C_WALK); }
            else to_follow(a);
        }
        break;
    case P_THROW:
        actor_step(a);
        if (a->timer && !--a->timer) { throw_spear(a); chain(a, C_THROW); a->v[2] = 5; }
        else if (!a->timer && !--a->v[2]) to_follow(a);
        break;
    }
}
