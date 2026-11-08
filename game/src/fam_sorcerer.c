/* Sorcerer mid-boss (class 4 kind 7 variant 2) and its shots (class 10
 * kinds 1, 2); specs/stage1_enemies.md 5.6, arcade routines 1432C (entry
 * 14340, appearance 14440, flight 144D2, attack 14532, blink 148EA,
 * tracking 14B04, wobble 14B82), its shots 23D76, 23DC0 and ground fire
 * 23E76 (flames: src/fam_chandelier.c). Approximations: the white hit
 * flash is a blink (cut 4: fixed sprite lines); after a hit's invulnerability ends while it
 * waits hidden it stays hidden and not hittable (the arcade makes it
 * hittable unseen); the tracking countdowns stop at -1 instead of wrapping
 * after 32768 frames. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"
#include "game.h"
#include "dirmath.h"
#include "stage.h"
#ifndef EXP_NOWIN
#define EXP_NOWIN 0             /* measurement builds: 1 keeps the sprite HUD at her entry */
#endif

/* Chains (tools/pack_stage1.sh): appear 0 (0x14FE8), 1 (0x15004); hover 2
 * + facing (0x15026); cast 6 + facing (0x1502E); flight 10 + aim
 * (0x151AE). Facing 0 left, 1 right, 2 down-left, 3 down-right; aim 0..15
 * clockwise from up. */
#ifndef SORC_HP
#define SORC_HP 12                          /* test builds: -DSORC_HP=n (reach the bridge collapse quickly) */
#endif
#define C_APPEAR 0
#define C_APPEAR2 1
#define C_HOVER 2
#define C_CAST 6
#define C_FLY 10

/* phase: appearance (hidden, then two poses), flight, attack substates
 * (source 0..8), blink. */
enum { P_APPEAR0, P_APPEAR2, P_APPEAR4, P_FLY0, P_FLY, P_WAIT, P_ARM, P_HOVER, P_CAST, P_FIRED, P_BLINK };
/* flags */
#define COL 1                               /* collidable (source byte 0 = 1) */
#define BACK_WAIT 2                         /* the blink returns to P_WAIT, else P_ARM */
#define RIGHT_SET 4                         /* flight table 0x14FA8, else 0x14F68 */
/* v[0], v[1]: the player position last tracked; v[2], v[3]: tracking
 * countdowns; vx, vy: tracking steps (8.8); v[4]: wobble entry; v[5]: its
 * timer. sub: blink frames. */

static const s16 relocate_off[4][2] = { { 160, 0 }, { -160, 0 }, { 144, 56 }, { -144, 56 } };   /* 0x14CE0 */
static const u8 hover_time[16] = { 32, 48, 40, 32, 32, 32, 24, 22, 20, 18, 16, 14, 12, 8, 8, 8 };   /* 0x14622 */
static const u8 cast_roll[256] = {                                                       /* 0x14642 */
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
    0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1,
    1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0,
    0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0,
    0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0,
    1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1,
    1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1,
    1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1,
    1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const u8 fire_pick[16] = { 2, 1, 2, 3, 2, 1, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3 };       /* 0x14842 */
static const u8 retarget[16] = { 255, 255, 255, 255, 1, 3, 255, 255, 255, 255, 255, 2, 0, 255, 255, 255 };   /* 0x14C2C */
static const s16 sparkle_off[16] = { 2, -2, 0, -1, 6, -6, 10, -10, 4, -4, 8, -8, 16, -16, 24, -24 };       /* 0x14D34 */
/* Motion per frame (8.8). Hover per facing 0x14F58; flight per aim, from
 * the left of the player 0x14FA8, else 0x14F68; wobble 0xED020. */
static const s16 hover_v[4][2] = { { -128, 0 }, { 128, 0 }, { -118, -48 }, { 118, -48 } };
static const s16 fly_left_v[16][2] = { { -384, 0 }, { -354, 146 }, { -271, 271 }, { -146, 354 }, { 0, 384 }, { 146, 354 },
    { 271, 271 }, { 354, 146 }, { 384, 0 }, { 354, -146 }, { 271, -271 }, { 146, -354 }, { 0, -384 }, { -146, -354 },
    { -271, -271 }, { -354, -146 } };
static const s16 fly_right_v[16][2] = { { 384, 0 }, { 354, -146 }, { 271, -271 }, { 146, -354 }, { 0, -384 }, { -146, -354 },
    { -271, -271 }, { -354, -146 }, { -384, 0 }, { -354, 146 }, { -271, 271 }, { -146, 354 }, { 0, 384 }, { 146, 354 },
    { 271, 271 }, { 354, 146 } };
static const s16 wobble_v[16][2] = { { 0, 80 }, { 30, 73 }, { 56, 56 }, { 73, 30 }, { 80, 0 }, { 73, -30 }, { 56, -56 },
    { 30, -73 }, { 0, -80 }, { -30, -73 }, { -56, -56 }, { -73, -30 }, { -80, 0 }, { -73, 30 }, { -56, 56 }, { -30, 73 } };
/* Shot motion per facing (8.8): slow 0x?, fast; left, right, down-left, down-right. */
static const s16 slow_v[4][2] = { { -512, 0 }, { 512, 0 }, { -472, -195 }, { 472, -195 } };
static const s16 fast_v[4][2] = { { -1024, 0 }, { 1024, 0 }, { -945, -391 }, { 945, -391 } };

#define PX (player.x >> 8)
#define PY (player.y >> 8)
#define RANK_ROW ((rank >> 12) & 15)

static u16 aim_at_player(Actor *a)
{
    return ((angle128(PX - AX(a), PY - AY(a)) * 2 + 8) >> 4) & 15;
}

static void remember_player(Actor *a)
{
    a->v[0] = PX;
    a->v[1] = PY;
}

/* Ground fire (class 10 kind 3, 0x23E76): an emitter 40 px to one side of
 * the player, stepping 16 px toward him (0x23ED2). */
static void ground_fire(s16 side)
{
    Actor *g = actor_new(F_GROUNDFIRE);
    if (!g) return;
    g->x = (s32)(PX + 40 * side) << 8;
    g->y = (s32)PY << 8;
    g->v[0] = -16 * side;
}

/* 14852: kind 1 slow fireball, 2 fast squiggle, 3 ground fire from both
 * sides (a slow fireball while the player is dying). */
static void fire(Actor *a)
{
    u16 kind = fire_pick[rnd() & 15];
    Actor *s;
    if (kind == 3 && player.dead) kind = 1;
    if (kind == 3)
    {
        ground_fire(-1);                    /* variant 4: from the left */
        ground_fire(1);                     /* variant 0: from the right */
        return;
    }
    s = actor_new(kind == 2 ? F_SQUIGGLE : F_FIREBALL);
    if (!s) return;
    s->x = a->x;
    s->y = a->y;
    s->facing = a->facing & 3;
    s->variant = kind == 2;
}

/* Blink 8 times (3 frames each) not hittable, then go on (14D54 / 148EA). */
static void blink(Actor *a, bool back_to_wait)
{
    a->flags = (a->flags & ~(COL | BACK_WAIT)) | (back_to_wait ? BACK_WAIT : 0);
    a->phase = P_BLINK;
    a->sub = 0;
    remember_player(a);
}

/* Every 8 frames per axis: the player's move since the last look is
 * covered at 2 px a frame; a still vertical skips that frame's steps. Then
 * the random wobble (a new vector every 3 frames). */
static void track(Actor *a)
{
    if (!((game_frame + (a - actors)) & 7))
    {
        if (player.dead) goto wobble;
        s16 dx = PX - a->v[0], dy = PY - a->v[1];
        if (dx)
        {
            a->vx = dx < 0 ? -512 : 512;
            a->v[2] = (dx < 0 ? -dx : dx) >> 1;
            a->v[0] = PX;
        }
        if (!dy) goto wobble;
        a->vy = dy < 0 ? -512 : 512;
        a->v[3] = (dy < 0 ? -dy : dy) >> 1;
        a->v[1] = PY;
    }
    if (a->v[2] >= 0 && --a->v[2] >= 0) a->x += a->vx;
    if (a->v[3] >= 0 && --a->v[3] >= 0) a->y += a->vy;
wobble:
    if (!--a->v[5])
    {
        a->v[5] = 3;
        a->v[4] = rnd() & 15;
    }
    a->x += wobble_v[a->v[4]][0];
    a->y += wobble_v[a->v[4]][1];
}

static void hover_move(Actor *a)
{
    a->x += hover_v[a->facing & 3][0];
    a->y += hover_v[a->facing & 3][1];
}

/* 14A6E. Returns TRUE when it died (freed). */
static bool damage(Actor *a)
{
    if (a->t2)
    {
        if (--a->t2) return FALSE;
        a->flags |= COL;
    }
    if (a->hp == a->last_hp) return FALSE;
    a->last_hp = a->hp;
    a->flags &= ~COL;
    a->t2 = 28;
    if (a->hp)
    {
        sound_post(SND_ENEMY_HIT);
        return FALSE;
    }
    {
        Actor *p;
        u16 i;
        for (i = 0; i < 2; i++)
            if ((p = actor_new(F_PLATFORM)) != NULL)
            {
                p->x = (s32)(i ? 2192 : 2048) << 8;
                p->y = (s32)80 << 8;
            }
    }
    sound_post(SND_EXPLODE);
    effect(F_BURST, AX(a), AY(a), 4);
    bridge_collapse[0] = 1;
    surface_wake();
    game_open_gate();
    midboss_active = 0;
    stage_hud_window(FALSE);
    award(6);
    drop(a, 5);
    actor_free(a);
    return TRUE;
}

void sorcerer_update(Actor *a)
{
    bool shown = TRUE;
    if (!a->state)
    {
        /* wait for the player at the camera stop (every 16 frames) */
        a->active = 0;
        if ((game_frame + (a - actors)) & 15) return;
        if ((u16)(AX(a) - CAM_X) > 384 || (u16)(AY(a) - CAM_Y) > 256 || !arrival) return;
        midboss_active = 1;
        if (!EXP_NOWIN) stage_hud_window(TRUE);   /* the camera holds: the top HUD rows off the sprites (her entry is at the top) */
        sound_post(SND_SORCERER);
        a->state = 2;
        a->hp = a->last_hp = SORC_HP;
        a->flags = 0;
        a->v[5] = 1;
        a->v[2] = a->v[3] = 0;
        remember_player(a);
        a->phase = P_APPEAR0;
        a->timer = 20;
        actor_anim(a, C_APPEAR);
        return;
    }
#ifdef HIT_AT
    if (game_frame == HIT_AT && a->hp > 1) a->hp--;   /* test builds: a hit at a set frame */
#endif
#ifdef SORC_KILL_AT
    if (game_frame == SORC_KILL_AT) a->hp = 0;        /* test builds: dies at a set frame (the bridge collapse) */
#endif
    switch (a->phase)
    {
    case P_APPEAR0:                         /* hidden, sparkles */
        shown = FALSE;
        if (--a->timer)
        {
            effect(F_SPARKLE, AX(a) + sparkle_off[rnd() & 15], AY(a) + sparkle_off[rnd() & 15], 0);
            break;
        }
        a->phase = P_APPEAR2;
        a->timer = 24;
        actor_anim(a, C_APPEAR);
        break;
    case P_APPEAR2:
        if (--a->timer)
        {
            effect(F_SPARKLE, AX(a) + sparkle_off[rnd() & 15], AY(a) + sparkle_off[rnd() & 15], 0);
            actor_step(a);
            break;
        }
        a->phase = P_APPEAR4;
        a->timer = 20;
        actor_anim(a, C_APPEAR2);
        break;
    case P_APPEAR4:
        if (--a->timer) break;
        a->flags |= COL;
        a->timer = 8;
        a->phase = P_FLY0;
        if (AX(a) < PX) a->flags |= RIGHT_SET;
        remember_player(a);
        a->facing = aim_at_player(a);
        actor_anim(a, C_FLY + a->facing);
        break;
    case P_BLINK:                           /* hidden first, toggles every 3 frames */
        shown = a->sub >= 3 && ((a->sub / 3) & 1);
        if (++a->sub > 24)
        {
            a->flags |= COL;
            a->phase = (a->flags & BACK_WAIT) ? P_WAIT : P_ARM;
            shown = FALSE;
        }
        break;
    default:                                /* flight and attack: tracking and damage */
        switch (a->phase)
        {
        case P_FLY0:
            remember_player(a);
            a->v[2] = a->v[3] = 0;
            a->vx = a->vy = 0;
            a->phase = P_FLY;
            break;
        case P_FLY:
            actor_step(a);
            {
                const s16 *v = ((a->flags & RIGHT_SET) ? fly_right_v : fly_left_v)[a->facing & 15];
                a->x += v[0];
                a->y += v[1];
            }
            if (--a->timer) break;
            a->timer = 8;
            {
                u16 aim = aim_at_player(a);
                if (aim == a->facing) break;
                if (retarget[aim] == 255)
                {
                    a->facing = aim;
                    actor_anim(a, C_FLY + aim);
                    break;
                }
                a->facing = retarget[aim];
                a->phase = P_ARM;
                actor_anim(a, C_HOVER + a->facing);
            }
            break;
        case P_WAIT:                        /* hidden, then reappear by the player */
            shown = FALSE;
            if (--a->timer) break;
            if (player.dead) { a->timer = 1; break; }
            remember_player(a);
            a->v[2] = a->v[3] = 0;
            a->facing ^= 2;
            {
                /* 14D54: by the camera centre unless the player is near it */
                u16 centre = CAM_X + 192;
                if ((u16)(centre - PX + 60) >= 120)
                {
                    if (centre > (u16)PX) a->facing &= 2;
                    else if (!(a->facing & 1)) a->facing ^= 1;
                }
            }
            a->x = (s32)(PX + relocate_off[a->facing & 3][0]) << 8;
            a->y = (s32)(PY + relocate_off[a->facing & 3][1]) << 8;
            actor_anim(a, C_HOVER + (a->facing & 3));
            blink(a, FALSE);
            a->flags &= ~COL;
            break;
        case P_ARM:
            a->flags |= COL;
            a->phase = P_HOVER;
            a->timer = hover_time[RANK_ROW];
            break;
        case P_HOVER:
            if (!--a->timer)
            {
                if (cast_roll[(RANK_ROW << 4) + (rnd() & 15)])
                {
                    a->phase = P_CAST;
                    a->timer = 14;
                    actor_anim(a, C_CAST + (a->facing & 3));
                    break;
                }
                a->timer = hover_time[RANK_ROW];
            }
            /* out of view or close to the player: blink away */
            if ((u16)(AX(a) - CAM_X) > 400 || (u16)(AY(a) - CAM_Y) > 272 || (u16)(AX(a) - PX + 80) < 160)
            {
                a->timer = 30;
                blink(a, TRUE);
            }
            actor_step(a);
            hover_move(a);
            break;
        case P_CAST:
            if (!--a->timer) { fire(a); a->phase = P_FIRED; a->timer = 10; }
            actor_step(a);
            break;
        case P_FIRED:
            if (!--a->timer) { a->phase = P_ARM; actor_anim(a, C_HOVER + (a->facing & 3)); }
            actor_step(a);
            break;
        }
        track(a);
        if (damage(a)) return;
        if (a->phase == P_WAIT || a->phase == P_BLINK) shown = FALSE;   /* waiting, or a blink begins */
        /* The hit flash (14D8A: palette job 21/22, white / normal every 3
         * frames): it shares a sprite line, so it blinks instead (as
         * Bavmorda's, src/fam_bav.c). */
        if (a->t2 && ((a->t2 / 3) & 1)) shown = FALSE;
        break;
    }
    a->active = !shown ? 0 : (a->flags & COL) ? 1 : 2;
}

/* Slow fireball (kind 1, 2 px a frame) and fast squiggle (kind 2, 4 px a
 * frame) along the cast pose's direction; removed off camera. */
void fireball_update(Actor *a)
{
    const s16 *v = (a->fam == F_SQUIGGLE ? fast_v : slow_v)[a->facing & 3];
    if (!a->state)
    {
        a->state = 2;
        a->active = 1;
        actor_anim(a, a->facing & 3);
        return;
    }
    actor_step(a);
    a->x += v[0];
    a->y += v[1];
    if (!(game_frame & 7) && ((u16)(AX(a) - CAM_X + 32) > 448 || (u16)(AY(a) - CAM_Y + 32) > 320))
        actor_free(a);
}

/* Ground-fire emitter: its marker shows 16 frames; then, over ground (a
 * standable surface, or a floor cell 16 px below), three flames 24 frames
 * apart, stepping v[0] px each; else it goes. Never hurts. */
void groundfire_update(Actor *a)
{
    s16 top;
    if (!a->state)
    {
        a->state = 2;
        a->active = 2;
        a->timer = 16;
        a->sub = 0;
        actor_anim(a, 0);
        return;
    }
    if (!a->sub)
    {
        if (--a->timer) { actor_step(a); return; }
        if (!surface_under(AX(a), AY(a) - 16, &top) && !(terrain_flags(AX(a), AY(a) - 16) & 8)) { actor_free(a); return; }
        a->active = 0;                      /* the marker goes; the flames remain */
        a->sub = 4;
        a->timer = 1;
    }
    if (--a->timer) return;
    if (!--a->sub) { actor_free(a); return; }
    {
        Actor *f = actor_new(F_FLAME);
        if (f) { f->x = a->x; f->y = a->y; }
    }
    a->x += (s32)a->v[0] << 8;
    a->timer = 24;
}
