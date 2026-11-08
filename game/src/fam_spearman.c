/* The axe thrower ("spearman", class 4 kind 14 variant 0), its returning
 * axe (class 10 kind 7) and the scripted beast at the boss-arena entrance
 * (class 4 kind 1 variant 3); specs/stage1_spearman.md, arcade routines
 * 0x176F6, 0x2469C, 0x11F4E. Deviation: an axe that misses its catch is
 * removed once well off camera (the arcade lets it fly on while its owner
 * lives, 6.4). */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"
#include "game.h"

#define SND_AXE 0x8B
#define SND_HIT 0x32

/* Chains (tools/pack_stage1.sh), + facing (0 right, 1 left). */
enum { C_WALK = 0, C_STAND = 2, C_HURT = 4, C_HURT_AXE = 6, C_CHOP = 8, C_THROW = 10,
       C_STRIKE = 12, C_FOLLOW = 14, C_CATCH = 16 };
enum { P_WALK = 0, P_CHOP = 2, P_ADVANCE = 4, P_THROW = 6, P_HOME = 8, P_STEP = 12, P_TURN = 16, P_SLOPE = 22 };

#define FACE(a) ((a)->facing & 1)
#define HOME_X(a) ((a)->v[0])
#define STAND_I(a) ((a)->v[1])
#define AXE_OUT(a) ((a)->v[2])
#define AHEAD_UP(a) ((a)->v[3])     /* +1F: ground rises ahead */
#define SAVED(a) ((a)->v[4])        /* phase | sub << 8 while hurt */
#define WALK_V 320
#define ADV_V 512

/* Action rows A..E (0x181CA): 4 rank blocks x 16. */
static const u8 rows[5][64] = {
    { 2,2,2,6,2,2,2,2,2,2,2,2,2,2,2,2, 2,6,2,6,2,2,2,2,2,2,2,2,2,2,6,2, 2,2,2,6,2,2,2,2,2,6,2,2,6,2,6,2, 6,6,2,6,6,2,6,2,2,2,6,2,2,6,2,6 },
    { 2,4,6,2,4,6,2,4,6,2,4,6,2,4,6,6, 2,6,6,2,4,6,6,6,6,6,4,6,2,4,6,6, 6,4,6,2,4,6,6,4,6,6,4,6,6,4,6,6, 6,4,6,6,4,6,6,4,6,6,4,6,6,4,6,6 },
    { 6,4,6,4,4,6,6,4,6,4,4,6,6,4,6,6, 6,4,6,4,4,6,6,4,6,6,4,6,6,6,6,6, 6,4,6,6,6,6,6,6,6,6,4,6,6,4,6,6, 6,6,6,4,6,6,6,6,6,4,6,6,6,6,6,6 },
    { 6,4,6,4,4,6,6,4,6,4,4,6,4,4,6,4, 6,4,6,4,4,6,6,4,6,6,4,6,4,6,6,4, 6,6,6,4,6,6,6,4,6,6,4,6,4,4,6,6, 6,6,6,4,4,6,6,6,6,6,6,6,4,6,6,6 },
    { 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8 },
};
static const u8 row_of[16] = { 0, 0, 0, 1, 2, 2, 2, 2, 3, 3, 4, 4, 4, 4, 4, 4 };
static const u8 advance_time[16] = { 16, 16, 32, 32, 48, 48, 64, 64, 80, 80, 96, 96, 112, 112, 128, 128 };
static const u8 stand_face[4] = { 0, 1, 1, 0 };

static void chain(Actor *a, u16 c)
{
    actor_anim(a, c + FACE(a));
}

static void set_phase(Actor *a, u16 p)
{
    a->phase = p;
    a->sub = 0;
}

/* Memo of the probes below (exact: their answers depend only on the cells'
 * columns, the probe height and the terrain, unless a floor cell is a slope,
 * which is never memoized). Spearmen standing or walking on flat ground ask
 * the same thing update after update. Kept in v[5] and pad (zeroed by
 * actor_new: nothing valid). */
#define FROZEN(a) ((a)->pad[0])              /* +4E: frozen far above or below the camera (0x17F6C) */
#define M_FY(a) ((a)->v[5])                  /* feet: probe y */
#define M_FC(a) (*(s16 *)&(a)->pad[1])       /* feet: centre column */
#define M_TY(a) (*(s16 *)&(a)->pad[3])       /* terrain: probe y */
#define M_TC(a) (*(s16 *)&(a)->pad[5])       /* terrain: column 16 ahead */
#define M_EP(a) ((a)->pad[7])                /* terrain_epoch of both */
#define M_FI(a) ((a)->pad[8])                /* feet: 0x80 valid, 3 front column - centre + 1, 4 ahead up, 8 on floor */
#define M_FDY(a) ((s8)(a)->pad[9])           /* feet: floor top - probe y */
#define M_TI(a) ((a)->pad[10])               /* terrain: 0x80 valid, 7 column 40 ahead - column 16 + 2, 0x30 outcome */
_Static_assert(__builtin_offsetof(Actor, pad) == 53, "memo words must be even");

#ifdef FREEZE_COUNT
u16 frozen_n;                          /* measurement: frozen spearman updates */
#endif
#ifdef TMEMO_CHECK
u16 tmemo_hits, tmemo_bad;             /* measurement: memo hits, and those the probes contradict */
#endif

static void memo_epoch(Actor *a)
{
    if (M_EP(a) != terrain_epoch) { M_EP(a) = terrain_epoch; M_FI(a) = 0; M_TI(a) = 0; }
}

/* Feet probe (0x3B0A selector 5): front foot sets AHEAD_UP; the centre foot
 * snaps y to the floor surface. TRUE when on floor. */
static bool feet(Actor *a)
{
    s16 x = AX(a), y = AY(a) - 32, c = x >> 4, fx = x + (FACE(a) ? -12 : 12), top;
    u8 k = (fx >> 4) - c + 1, slope = 0, up, on;
    memo_epoch(a);
#ifdef NO_TMEMO
    if (0)
#else
    if ((M_FI(a) & 0x83) == (0x80 | k) && M_FY(a) == y && M_FC(a) == c)
#endif
    {
#ifdef TMEMO_CHECK
        {
            extern u16 tmemo_hits, tmemo_bad;
            u8 sl = 0; u16 u = terrain_floor_s(fx, y, &top, &sl), o = terrain_floor_s(x, y, &top, &sl);
            tmemo_hits++;
            if (sl || u != ((M_FI(a) >> 2) & 1) || o != ((M_FI(a) >> 3) & 1) || (o && top - y != M_FDY(a))) tmemo_bad++;
        }
#endif
        AHEAD_UP(a) = (M_FI(a) >> 2) & 1;
        if (!(M_FI(a) & 8)) return FALSE;
        a->y = (s32)(y + M_FDY(a) + 32) << 8;
        return TRUE;
    }
    up = terrain_floor_s(fx, y, &top, &slope);
    AHEAD_UP(a) = up;
    on = terrain_floor_s(x, y, &top, &slope);
    M_FI(a) = 0;
    if (!slope)
    {
        M_FY(a) = y; M_FC(a) = c;
        M_FI(a) = 0x80 | k | (up << 2) | (on << 3);
        a->pad[9] = on ? top - y : 0;
    }
    if (!on) return FALSE;
    a->y = (s32)(top + 32) << 8;
    return TRUE;
}

static bool cell(s16 x, s16 y)
{
    return (terrain_flags(x, y) & 8) != 0;
}

static void gravity(Actor *a)
{
    if (a->vy <= -1920) a->y -= 7 << 8;
    else { a->vy -= 40; a->y += a->vy; }
}

static void move(Actor *a)
{
    a->x += a->vx;
}

/* Terrain check (0x18368) after a walking step: 0 nothing, else the new
 * phase (1 P_SLOPE, 2 P_TURN, 3 P_STEP). */
static u8 terrain_ahead(s16 x, s16 y, s16 d)
{
    s16 x2 = x + 40 * d;
    if (cell(x + 16 * d, y - 40)) return terrain_shape(x + 16 * d, y - 40) & 0x80 ? 0 : 1;
    if (cell(x2, y + 24)) return 2;
    if (cell(x2, y - 24)) return terrain_shape(x2, y - 24) & 0x80 ? 0 : 1;
    return cell(x2, y - 56) ? 3 : 2;
}

static void terrain(Actor *a)
{
    static const u8 to[4] = { 0, P_SLOPE, P_TURN, P_STEP };
    s16 x = AX(a), y = AY(a), d = FACE(a) ? -1 : 1, c = (x + 16 * d) >> 4;
    u8 k = ((x + 40 * d) >> 4) - c + 2, r;
    feet(a);
    if (AHEAD_UP(a)) return;
#ifdef NO_TMEMO
    if (0) r = 0;
#else
    if ((M_TI(a) & 0x87) == (0x80 | k) && M_TY(a) == y && M_TC(a) == c)
    {
        r = (M_TI(a) >> 4) & 3;
#ifdef TMEMO_CHECK
        { extern u16 tmemo_hits, tmemo_bad; tmemo_hits++; if (r != terrain_ahead(x, y, d)) tmemo_bad++; }
#endif
    }
#endif
    else
    {
        r = terrain_ahead(x, y, d);
        M_TY(a) = y; M_TC(a) = c;
        M_TI(a) = 0x80 | k | (r << 4);
    }
    if (r) set_phase(a, to[r]);
}

static bool in_window(Actor *a)
{
    return (u16)(AY(a) - (s16)(player.y >> 8) + 32) < 64;
}

static void face_player(Actor *a)
{
    a->facing = AX(a) > (s16)(player.x >> 8);
}

static void by_distance(Actor *a, s16 d)
{
    u16 block = ((rank >> 8) >> 2) & 0x30;
    set_phase(a, rows[row_of[(d >> 4) & 15]][block + (rnd() & 15)]);
}

static void choose(Actor *a)
{
    s16 dx;
    if (!in_window(a)) { set_phase(a, P_HOME); return; }
    face_player(a);
    dx = (s16)(player.x >> 8) - AX(a);
    by_distance(a, dx < 0 ? -dx : dx);
}

static void near(Actor *a)
{
    s16 dx = AX(a) - (s16)(player.x >> 8);
    if (in_window(a) && dx >= -160 && dx < 160) choose(a);
}

static void ahead(Actor *a)
{
    s16 d = FACE(a) ? AX(a) - (s16)(player.x >> 8) : (s16)(player.x >> 8) - AX(a);
    if (in_window(a) && d >= 0 && d < 160) by_distance(a, d);
}

static void walk_motion(Actor *a, s16 v)
{
    a->vx = FACE(a) ? -v : v;
}

static Actor *axe_of(Actor *a)
{
    Actor *x = a->link ? actors + a->link - 1 : NULL;
    return x && x->fam == F_AXE ? x : NULL;
}

static void throw_axe(Actor *a)
{
    Actor *x = actor_new(F_AXE);
    if (!x) return;
    x->facing = FACE(a);
    x->x = a->x + ((s32)(FACE(a) ? -32 : 32) << 8);
    x->y = a->y + (8 << 8);
    x->link = a - actors + 1;
    a->link = x - actors + 1;
    sound_post(SND_AXE);
}

static void phase_step(Actor *a)
{
    switch (a->phase)
    {
    case P_WALK:
        switch (a->sub)
        {
        case 0: a->sub = 2; a->timer = 40; face_player(a); walk_motion(a, WALK_V); chain(a, C_WALK); break;
        case 2:
            if (!--a->timer) { a->sub = 4; a->timer = 40; chain(a, C_STAND); break; }
            ahead(a);
            if (a->phase != P_WALK) break;
            actor_step(a); move(a); a->y -= 2 << 8; terrain(a);
            break;
        case 4:
            if (!--a->timer)
            {
                a->facing = stand_face[STAND_I(a)];
                STAND_I(a) = (STAND_I(a) + 1) & 3;
                a->sub = 0;
                near(a);
                break;
            }
            actor_step(a); a->y -= 2 << 8; feet(a);
            break;
        }
        break;
    case P_CHOP:
        switch (a->sub)
        {
        case 0: a->sub = 2; a->timer = 45; face_player(a); chain(a, C_CHOP); break;
        case 2:
            actor_step(a);
            if (!--a->timer) { a->sub = 4; a->timer = 15; sound_post(SND_AXE); chain(a, C_STRIKE); }
            break;
        case 4: if (!--a->timer) choose(a); break;
        }
        break;
    case P_ADVANCE:
        switch (a->sub)
        {
        case 0:
            a->sub = 2; a->timer = advance_time[rnd() & 15]; face_player(a);
            walk_motion(a, ADV_V); chain(a, C_WALK);
            break;
        case 2:
            if (!--a->timer) { choose(a); break; }
            actor_step(a); actor_step(a);
            if (!((game_frame + (a - actors)) & 15))
            {
                u8 f = AX(a) >= (s16)(player.x >> 8);
                if (f != FACE(a)) { a->facing = f; walk_motion(a, ADV_V); chain(a, C_WALK); }
            }
            move(a); a->y -= 2 << 8; terrain(a);
            break;
        }
        break;
    case P_THROW:
        switch (a->sub)
        {
        case 0: a->sub = 2; a->timer = 42; face_player(a); chain(a, C_THROW); break;
        case 2: actor_step(a); if (!--a->timer) a->sub = 4; break;
        case 4: throw_axe(a); a->sub = 6; AXE_OUT(a) = 1; a->timer = 12; chain(a, C_FOLLOW); break;
        case 6: actor_step(a); if (!--a->timer) { a->sub = 8; a->timer = 200; } break;
        case 8:
            {
                Actor *x = axe_of(a);
                bool catch = !--a->timer;
                if (!catch && x)
                {
                    s16 d = AX(a) - AX(x);
                    if (x->v[2] || (d >= -64 && d < 64)) { x->v[2] = 1; catch = TRUE; }
                }
                if (catch) { a->sub = 10; AXE_OUT(a) = 0; a->timer = 24; chain(a, C_CATCH); }
            }
            break;
        case 10: actor_step(a); if (!--a->timer) choose(a); break;
        }
        a->y -= 2 << 8; feet(a);
        break;
    case P_HOME:
        switch (a->sub)
        {
        case 0:
            a->sub = 2; a->facing = HOME_X(a) >= AX(a) ? 0 : 1; a->timer = 60;
            walk_motion(a, WALK_V); chain(a, C_WALK);
            break;
        case 2:
            {
                s16 d = AX(a) - HOME_X(a);
                if (d >= -10 && d < 10) { a->phase = P_WALK; a->sub = 2; STAND_I(a) = 0; a->timer = 1; break; }
            }
            if (!--a->timer) { a->sub = 4; a->timer = 30; chain(a, C_STAND); break; }
            move(a); actor_step(a); ahead(a);
            if (a->phase == P_HOME) terrain(a);
            break;
        case 4:
            if (!--a->timer) { a->sub = 0; near(a); break; }
            actor_step(a); feet(a);
            break;
        }
        break;
    case P_STEP:
        if (a->sub == 0) { a->vy = -512; a->sub = 2; }
        if (a->sub == 2)
        {
            if (feet(a)) { gravity(a); move(a); actor_step(a); }
            else a->sub = 4;
        }
        else
        {
            if (!feet(a)) { gravity(a); move(a); actor_step(a); }
            else choose(a);
        }
        break;
    case P_TURN:
        switch (a->sub)
        {
        case 0: a->facing ^= 1; a->timer = 4; a->sub = 2; chain(a, C_STAND); break;
        case 2: if (!--a->timer) { a->sub = 4; a->timer = 10; walk_motion(a, WALK_V); chain(a, C_WALK); } break;
        case 4:
            if (!--a->timer) { set_phase(a, P_HOME); break; }
            actor_step(a); move(a); feet(a);
            break;
        }
        break;
    case P_SLOPE:
        if (AHEAD_UP(a)) { choose(a); break; }
        actor_step(a); move(a);
        if (!feet(a)) set_phase(a, P_TURN);
        break;
    }
}

void spearman_update(Actor *a)
{
    switch (a->state)
    {
    case 0:                                         /* wake-up (0x17728) */
        a->active = 0;
        if ((u16)(AX(a) - CAM_X) > 448 || (u16)(AY(a) - CAM_Y) > 276) return;
        feet(a);
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = (a->variant == 10 || a->variant == 12 || a->variant == 16) ? 8 : 4;   /* the cart pushers' brutes (stage6.md 4.4), stage 5's corridor entrant (stage5.md 4.3): 8 */
        HOME_X(a) = AX(a);
        STAND_I(a) = AXE_OUT(a) = 0;
        set_phase(a, P_WALK);
        chain(a, C_STAND);
        return;
    case 2:
        if (!FROZEN(a)) phase_step(a);
        if (!((game_frame + (a - actors)) & 31) && AX(a) < (stage_index == 4 ? (s16)(player.cam_min_x - 128) : player.cam_min_x)) { actor_free(a); return; }   /* 0x17922: stage 5 keeps 128 more */
        {
            /* Freeze (0x17F6C, every variant but the raft's 6): y outside
             * [camera Y - 128, camera Y + 384] (unsigned lower, signed upper
             * bound) skips the phase code from the next update on, and the
             * off-camera test removes it outside x - camera X in [-48, 432]
             * or y - camera Y in [-32, 288]. */
            u16 lower = CAM_Y - 128, y = AY(a);
            if (a->variant == 6 || (s16)lower < 0 || (y >= lower && (s16)(lower + 512) >= (s16)y)) FROZEN(a) = 0;
            else
            {
                FROZEN(a) = 1;
#ifdef FREEZE_COUNT
                { extern u16 frozen_n; frozen_n++; }
#endif
                if ((u16)(AX(a) - CAM_X + 48) > 480 || (u16)(AY(a) - CAM_Y + 32) > 320) { actor_free(a); return; }
            }
        }
        if (encounter_clear)
        {
            sound_post(SND_EXPLODE);
            effect(F_BURST, AX(a), AY(a), 5);
            actor_free(a);
            return;
        }
        if (a->hp != a->last_hp)
        {
            a->last_hp = a->hp;
            a->active = 2;
            SAVED(a) = a->phase | (a->sub << 8);
            a->state = 4;
            a->phase = 0;
        }
        return;
    case 4:                                         /* hurt / death (0x1844C) */
        if (a->phase == 0)
        {
            sound_post(SND_HIT);
            a->phase = 2;
            a->t2 = 10;
            a->timer = (u8)(a->timer - 10) >= 128 ? 1 : a->timer - 10;
            chain(a, AXE_OUT(a) ? C_HURT_AXE : C_HURT);
            return;
        }
        if (a->phase == 2)
        {
            if (--a->t2) return;
            if (!a->hp)
            {
                a->phase = 4;
                sound_post(SND_EXPLODE);
                effect(F_BURST, AX(a), AY(a), 5);
                return;
            }
            a->state = 2;
            a->active = 1;
            if (AXE_OUT(a)) { a->phase = SAVED(a) & 0xFF; a->sub = SAVED(a) >> 8; chain(a, C_FOLLOW); }
            else { chain(a, C_STAND); choose(a); }
            return;
        }
        award(4);                                   /* 200 */
        drop(a, a->variant == 2 ? 1 : 2);        /* 0x18544: set 1 for stage 2's door variant 2 */
        actor_free(a);
        return;
    }
}

/* The axe (0x2469C): decelerates, turns round and comes back; caught by
 * its owner within 64 px. */
void axe_update(Actor *a)
{
    Actor *owner = a->link ? actors + a->link - 1 : NULL;
    bool owner_here = owner && owner->fam == F_SPEARMAN;
    if (!a->state)
    {
        a->state = 2;
        a->active = 1;
        a->timer = 8;
        a->vx = FACE(a) ? -1536 : 1536;
        a->v[0] = FACE(a) ? 24 : -24;
        actor_anim(a, FACE(a));
        return;
    }
    if (encounter_clear) { effect(F_BURST, AX(a), AY(a), 4); actor_free(a); return; }
    if (a->sub == 0)                                /* launch: 7 moves, then none */
    {
        if (--a->timer) { a->vx += a->v[0]; a->x += a->vx; actor_step(a); }
        else a->sub = 2;
        return;
    }
    if (!owner_here) a->sub = 4;
    if (a->sub == 2 && a->v[2]) { if (owner_here) owner->link = 0; actor_free(a); return; }
    if (a->sub == 4 && !((game_frame + (a - actors)) & 7) &&
        ((u16)(AX(a) - CAM_X + 48) > 480 || (u16)(AY(a) - CAM_Y + 32) > 320)) { actor_free(a); return; }
    a->vx += a->v[0];
    a->x += a->vx;
    actor_step(a);
    if (!a->v[1])                                   /* one rebound off a wall */
    {
        u8 f = terrain_flags(AX(a), AY(a) - 8);
        if (f & (a->vx > 0 ? 1 : 2)) { a->v[1] = 1; a->vx = a->vx > 0 ? -1024 : 1024; }
    }
    /* Deviation: a missed axe goes once well off camera. */
    if (!((game_frame + (a - actors)) & 15) && (u16)(AX(a) - CAM_X + 256) > 896) actor_free(a);
}

/* Scripted beast at the arena entrance (0x11F4E): walks 142 px when the
 * camera reaches the arena, stands until the boss body spawns its head,
 * then vanishes. Never collidable. */
extern u8 boss_seen_flag(void);
extern u8 boss_head_spawned;

void arena_beast_update(Actor *a)
{
    switch (a->state)
    {
    case 0:
        if (boss_seen_flag()) { actor_free(a); return; }
        a->active = 2;
        a->state = 2;
        a->phase = 0;
        a->timer = 72;
        actor_anim(a, FACE(a));
        return;
    case 2:
        if (a->phase == 0)
        {
            if ((u16)(AX(a) - CAM_X + 32) <= 448 && (u16)(AY(a) - CAM_Y) <= 272) a->phase = 2;
            return;
        }
        if (a->phase == 2)
        {
            if (!--a->timer) { a->phase = 4; actor_anim(a, 2 + FACE(a)); return; }
            a->x += FACE(a) ? -512 : 512;
            actor_step(a);
            return;
        }
        if (boss_head_spawned) actor_free(a);
        return;
    }
}
