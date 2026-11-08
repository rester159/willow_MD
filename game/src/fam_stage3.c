/* Stage 3's families (specs/stage3.md 4.2, 5): the bomb trooper and its
 * bombs, the lance knight and his lance, the sinking pillars, Willow's boat,
 * the enemy rafts and their crews, the leaping fish, hawk variant 1 and its
 * rock; and the stage's spawn events (3.2: hawks, rafts, fish).
 *
 * Native y = arcade y - 256 (the stage's map starts at y 256): water line
 * 308 -> 52, boat / raft deck 316 -> 60.
 *
 * Approximations (M1_NOTES): the boat's and rafts' spring bob is a triangle of
 * the measured amplitude (+-2 px, +-6 px in the swell); the raft crews'
 * attack cycle is a fixed timer (archers 82, spearmen 96 frames); the fish's
 * spray droplets are not drawn; hit flashes are not shown. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"
#include "shot.h"
#include "stage.h"
#include "game.h"
#include "spell.h"
#include "s3spr.h"

#define SND_HIT 0x32
#define SND_BOMB 0x3E
#define SND_THRUST 0x91
#define SND_KNIGHT_HIT 0x92
#define SND_ARROW 0x34
#define SND_SPEAR 0x8B

#define WATER_Y 52          /* 308: the fish's water line */
#define DECK_Y 60           /* 316: boat and rafts */
#define LAKE_X 2832         /* the boat's (and Willow's) left limit on the lake */
#define SWELL_X 3872        /* camera x: the bigger bob, the sea swell */

#define FACE(a) ((a)->facing & 1)
#define DIR(a) (FACE(a) ? -1 : 1)

u8 knight_beaten;           /* FF838A: the lance knight is beaten (not again after a restart) */
u8 fish_end;                /* FF83CE: the fish stop (the boss arena) */
u8 boat_drift;              /* FF83DC: Willow rides the boat */
u8 swell;                   /* the camera's sea swell (2.4): 0 off, 1 on, 2 gliding back */
s16 swell_y;                /* its view offset (px, up) */
static s16 swell_v, swell_sub; static s8 swell_dir = 1;
static u8 hawk_alive, hawk_left, raft_quota[2], fish_pop;
static s8 lance_back;       /* FF0EB6: the lance's tip has turned */

static u16 rank_row(void) { return rank >> 12; }

static void chain(Actor *a, u16 c) { actor_anim(a, c + FACE(a)); }

static void face_player(Actor *a) { a->facing = (player.x >> 8) < AX(a); }

static bool off_camera(Actor *a)
{
    return (u16)(AX(a) - CAM_X + 32) > 448 || (u16)(AY(a) - CAM_Y + 64) > 384;
}

/* A hit: TRUE when the energy changed (the caller handles death at 0). The
 * actor is not collidable for `immune` frames (v[5]). */
static bool hit(Actor *a, u16 snd, u16 immune)
{
    if (a->hp == a->last_hp) return FALSE;
    a->last_hp = a->hp;
    if (a->hp) { sound_post(snd); a->active = 2; a->v[5] = immune; }
    return TRUE;
}

static void immune_tick(Actor *a)
{
    if (a->v[5] && !--a->v[5]) a->active = 1;
}

static void die(Actor *a, u16 score, u16 set)
{
    if (a->fam == F_TROOPER && a->variant == 2) trooper4_gone();
    sound_post(SND_EXPLODE);
    effect(F_BURST, AX(a), AY(a), 5);
    if (score != 0xFF) award(score);
    if (set != 0xFF) drop(a, set);
    actor_free(a);
}

/* ---- Bomb trooper (5.1, class 4 kind 23). v[0] window W, v[1] bomb
 * variant, vy jump speed. */
enum { T_WATCH = 0, T_LEDGE = 2, T_FALL = 4, T_GROUND = 6, T_DODGE = 8, T_WAIT_SCREEN = 10 };
enum { TC_WALK = 0, TC_LTHROW = 2, TC_STAND = 4, TC_DROP = 6, TC_CROUCH = 8, TC_JUMP = 10, TC_THROW = 12, TC_RUN = 14 };
static const u16 trooper_w[4] = { 160, 176, 144, 168 };
static const u8 ledge_wait[16] = { 48, 64, 96, 64, 48, 96, 128, 64, 48, 64, 96, 64, 64, 96, 64, 96 };
static const u8 trooper_drop[8] = { 2, 1, 2, 3, 2, 4, 2, 2 };
static const u8 wait_after[4] = { 40, 20, 60, 10 };
static const u8 escape_throw[4] = { 1, 2, 3, 3 };

static void bomb_throw(Actor *a, u16 variant, s16 dx, s16 dy)
{
    Actor *b = actor_new(F_BOMB);
    if (!b) return;
    b->variant = variant;
    b->facing = FACE(a);
    b->x = a->x + ((s32)dx << 8);
    b->y = a->y + ((s32)dy << 8);
}

static bool trooper_floor(Actor *a) { return actor_ground(a, 0, -24); }

/* The choice by rank row and distance column (0x1D4EA): column c =
 * |player x - x| >> 4; c > 15 -> 4 (no random number); else group
 * 0x1D560[(rank high byte & 0xF0) + c] (an offset into 0x1D660), entry
 * random & 15. */
static const u8 trooper_group[256] = {
    0, 0, 0, 0, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 0, 0, 0, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 0, 0, 0, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 0, 0, 0, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 0, 0, 0, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 0, 0, 0, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 0, 0, 0, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 0, 0, 16, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 0, 0, 16, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 0, 16, 16, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 0, 16, 16, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 16, 16, 16, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    0, 16, 16, 16, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    16, 16, 16, 16, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    16, 16, 16, 16, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64,
    16, 16, 16, 16, 16, 16, 16, 32, 32, 48, 48, 64, 64, 64, 64, 64 };
static const u8 trooper_pick[80] = {
    0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0,
    1, 2, 1, 1, 2, 2, 1, 2, 2, 1, 2, 2, 1, 1, 2, 1,
    2, 3, 2, 1, 2, 2, 1, 3, 2, 1, 3, 2, 2, 3, 2, 1,
    3, 2, 3, 2, 2, 3, 3, 2, 3, 2, 3, 3, 2, 2, 3, 2,
    4, 4, 3, 4, 4, 4, 4, 4, 4, 3, 4, 4, 3, 4, 4, 4 };

static u16 trooper_choice(u16 c)
{
    if (c > 15) return 4;
    return trooper_pick[trooper_group[((rank >> 8) & 0xF0) + c] + (rnd() & 15)];
}

static void trooper_ground(Actor *a)
{
    s16 px = player.x >> 8, dx = px - AX(a), adx = dx < 0 ? -dx : dx;
    switch (a->sub)
    {
    case 0:
        {
            u16 c = trooper_choice(adx >> 4);
            if (!(c & 3)) { a->sub = 2; chain(a, TC_RUN); return; }
            face_player(a);
            a->sub = 4;
            a->timer = 48;
            a->v[1] = c + 4;
            chain(a, TC_THROW);
        }
        return;
    case 2:                                 /* keep 96..W away */
        if (adx > a->v[0]) a->facing = dx < 0;
        else if (adx <= 96) a->facing = dx >= 0;
        else { a->sub = 0; return; }
        a->x += DIR(a) * 512;
        if (!terrain_floor(AX(a) + DIR(a) * 12, AY(a) - 32, &(s16){0}) && !terrain_floor(AX(a) + DIR(a) * 12, AY(a) - 24, &(s16){0}))
        {
            a->x -= DIR(a) * 512;
            a->facing ^= 1;
            a->v[1] = escape_throw[rnd() & 3] + 4;
            a->sub = 4;
            a->timer = 48;
            chain(a, TC_THROW);
            return;
        }
        trooper_floor(a);
        break;
    case 4:                                 /* the throw */
        if (a->timer == 16) bomb_throw(a, a->v[1], FACE(a) ? -16 : 16, 8);
        if (--a->timer) return;
        a->timer = wait_after[rnd() & 3];
        chain(a, TC_STAND);
        a->sub = 6;
        return;
    case 6:
        if (!--a->timer) a->sub = 0;
        break;
    }
    /* The dodge (every 8 frames): a shot close by. */
    if (!((game_frame + (a - actors)) & 7) && shot_near(AX(a), AY(a), 80, 32))
    {
        a->phase = T_DODGE;
        a->sub = 0;
        a->timer = 7;
        chain(a, TC_CROUCH);
    }
}

void trooper_update(Actor *a)
{
    s16 px = player.x >> 8;
    if (!a->state)
    {
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = 24;
        a->v[0] = trooper_w[(rnd() & 12) >> 2];
        if (a->variant == 0) { a->phase = T_WATCH; chain(a, TC_WALK); }
        else if (a->variant == 2) { a->hp = a->last_hp = 10; a->phase = T_FALL; a->vy = 0; chain(a, TC_DROP); }   /* stage 4: falls in from above the right edge (stage4.md 5.3) */
        else { a->phase = T_WAIT_SCREEN; chain(a, TC_RUN); trooper_floor(a); }
        return;
    }
    if (encounter_clear) { if (a->variant == 2) trooper4_gone(); sound_post(SND_EXPLODE); effect(F_BURST, AX(a), AY(a), 5); actor_free(a); return; }
    if (hit(a, SND_HIT, 15) && !a->hp) { die(a, 6, trooper_drop[rnd() & 7]); return; }
    immune_tick(a);
    if (!((game_frame + (a - actors)) & 7) && AX(a) < player.cam_min_x) { if (a->variant == 2) trooper4_gone(); actor_free(a); return; }
    if (a->variant == 2 && AX(a) > 2304) a->x = (s32)2304 << 8;
    actor_step(a);
    switch (a->phase)
    {
    case T_WAIT_SCREEN:
        if ((u16)(AX(a) - CAM_X) < 320) { a->phase = T_GROUND; a->sub = 0; }
        return;
    case T_WATCH:                           /* on the ledge: watch, throw, drop */
        if (a->v[5]) { a->phase = T_FALL; a->vy = 0; chain(a, TC_DROP); return; }
        if (px - AX(a) > 128) { a->phase = T_FALL; a->vy = 0; chain(a, TC_DROP); return; }
        if ((u16)(px - AX(a) + 128) <= 256) { a->timer = ledge_wait[rnd() & 15]; a->phase = T_LEDGE; a->sub = 0; }
        return;
    case T_LEDGE:
        if (a->sub == 0)
        {
            if (AX(a) + 96 <= px) { a->phase = T_FALL; a->vy = 0; chain(a, TC_DROP); return; }
            if (--a->timer) return;
            a->timer = 42;
            a->sub = 2;
            chain(a, TC_LTHROW);
            return;
        }
        if (a->timer == 16) bomb_throw(a, 0, -16, 16);
        if (!--a->timer) { a->phase = T_WATCH; chain(a, TC_WALK); }
        return;
    case T_FALL:
        if (a->sub == 0)
        {
            a->vy = a->vy - 48 < -1792 ? -1792 : a->vy - 48;
            a->y += a->vy;
            if (a->vy < 0 && trooper_floor(a)) { a->sub = 2; a->timer = 16; chain(a, TC_STAND); }
            else if (AY(a) < -64) actor_free(a);
            return;
        }
        if (!--a->timer) { a->phase = T_GROUND; a->sub = 0; }
        return;
    case T_GROUND:
        trooper_ground(a);
        return;
    case T_DODGE:
        switch (a->sub)
        {
        case 0:
            if (--a->timer) return;
            a->vy = 1280;
            a->sub = 2;
            chain(a, TC_JUMP);
            return;
        case 2:                             /* up and down, the floor once falling fast */
            a->y += a->vy;
            a->vy = a->vy - 48 < -1792 ? -1792 : a->vy - 48;
            if (a->vy < -1024 && trooper_floor(a)) { a->sub = 4; a->timer = 7; chain(a, TC_CROUCH); }
            else if (AY(a) < -64) actor_free(a);
            return;
        case 4:
            if (!--a->timer) { a->phase = T_GROUND; a->sub = 0; chain(a, TC_STAND); }
            return;
        }
    }
}

/* ---- Bomb (5.2, class 10 kind 10): variant 0 dropped, 5-7 lobbed, 1-4
 * shards; state 4 the burst. v[0] frames left without the floor test. */
static const s16 lob_vy[3] = { 1280, 1280, 768 }, lob_g[3] = { 80, 56, 24 };
static const s16 shard_vy[4] = { 768, 768, 1024, 1024 };

void bomb_update(Actor *a)
{
    s16 top;
    if (!a->state)
    {
        a->state = 2;
        a->active = 1;
        if (a->variant == 8) a->variant = 5;   /* stage 4's rolling bomb (stage4.md 5.3): as variant 5 */
        if (a->variant >= 5) { a->vx = FACE(a) ? -768 : 768; a->vy = lob_vy[a->variant - 5]; a->v[1] = lob_g[a->variant - 5]; a->v[0] = -1; chain(a, 0); }
        else if (a->variant) { a->facing = a->variant & 1; a->vx = (a->variant <= 2 ? 768 : 512) * DIR(a); a->vy = shard_vy[a->variant - 1]; a->v[1] = 48; chain(a, 4); }
        else { a->vx = 0; a->vy = 0; a->v[1] = 48; chain(a, 0); }
        return;
    }
    actor_step(a);
    if (a->state == 4)                      /* the burst: hurts until 6 frames are left */
    {
        if (a->timer == 6) a->active = 2;
        if (!--a->timer) actor_free(a);
        return;
    }
    if (!(game_frame & 7) && off_camera(a)) { actor_free(a); return; }
    a->x += a->vx;
    a->y += a->vy;
    if (a->variant >= 5 && a->v[0] < 0)
    {
        a->vy -= a->v[1];
        if (a->vy <= 0) a->v[0] = 20;       /* the apex: 20 frames of fall without the floor test */
        return;
    }
    a->vy = a->vy - a->v[1] < -1792 ? -1792 : a->vy - a->v[1];
    if (a->v[0] > 0) { a->v[0]--; return; }
    if (a->vy > 0 || !terrain_floor(AX(a), AY(a) - 6, &top))
    {
        if (AY(a) < -32) actor_free(a);
        return;
    }
    if (a->variant >= 1 && a->variant <= 4) { actor_free(a); return; }
    {
        u16 v;
        for (v = 1; v <= 4; v++)
        {
            Actor *s = actor_new(F_BOMB);
            if (s) { s->variant = v; s->x = a->x; s->y = (s32)(top + 6) << 8; }
        }
    }
    sound_post(SND_BOMB);
    a->y = (s32)(top + 8) << 8;
    a->state = 4;
    a->timer = 24;
    chain(a, 2);
}

/* ---- Lance knight (5.3, class 4 kind 19) and his lance (class 10 kind 8).
 * v[0] walk counter, v[1] attack timer, v[2] crouched thrust. */
enum { K_WAIT = 0, K_WALK, K_ATTACK, K_TURN, K_BACK };
static const u8 knight_backoff[4] = { 32, 48, 64, 80 };
static const u8 knight_face[8] = { 0, 1, 1, 0, 0, 0, 1, 1 };
static const u8 lance_reach[8] = { 84, 72, 60, 48, 36, 24, 12, 0 };
/* Patrol windows by variant (0x1AC50: centre, half-width; stage6_families.md
 * 1.1): variant 0 is stage 3's, 1-10 stage 6's (arcade x; the tower's
 * variants 5-10 sit in area 5, native x = arcade x - 3072). */
static const s16 knight_win[11][2] = {
    { 0x820, 128 }, { 0x370, 48 }, { 0x3B0, 48 }, { 0x888, 40 }, { 0x8B8, 40 },
    { 0xEC0, 64 }, { 0xEC0, 64 }, { 0xEC0, 64 }, { 0xD40, 64 }, { 0xD40, 64 }, { 0xDD0, 80 },
};

/* The patrol clamp (0x1AC04): x held to the window; at an edge he turns. */
static void knight_clamp(Actor *a)
{
    u16 v = a->variant < 11 ? a->variant : 0;
    s16 c = knight_win[v][0] - (v >= 5 ? 3072 : 0), h = knight_win[v][1];
    if (AX(a) <= c - h) { a->x = (s32)(c - h) << 8; a->facing = 0; chain(a, 0); }
    if (AX(a) >= c + h) { a->x = (s32)(c + h) << 8; a->facing = 1; chain(a, 0); }
}

static void lance_spawn(Actor *a, s16 dy)
{
    u16 v;
    lance_back = 0;
    for (v = 0; v < 8; v++)
    {
        Actor *l = actor_new(F_LANCE);
        if (!l) return;
        l->variant = v;
        l->facing = FACE(a);
        l->hflip = FACE(a);
        l->x = a->x + ((s32)(FACE(a) ? -72 : 72) << 8);
        l->y = a->y + ((s32)dy << 8);
        l->v[0] = AX(l);
    }
    sound_post(SND_THRUST);
}

void knight_update(Actor *a)
{
    s16 px = player.x >> 8, py = player.y >> 8, dx = px - AX(a);
    if (!a->state)
    {
        if (knight_beaten && !a->variant) { actor_free(a); return; }
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = 48;
        a->v[0] = 20;
        a->v[1] = 20;
        a->phase = a->variant ? K_WALK : K_WAIT;   /* stage 6's (1-10): no wake (stage6_families.md 1.1) */
        chain(a, 0);
        return;
    }
    if (hit(a, SND_KNIGHT_HIT, 15) && !a->hp)
    {
        if (!a->variant) { midboss_active = 0; knight_beaten = 1; }
        die(a, 10, 6);
        return;
    }
    immune_tick(a);
    /* It thinks only while near the screen (0x1A8BA, margins 0x2296). */
    if (a->variant && ((u16)(AX(a) - CAM_X + 64) > 512 || (u16)(AY(a) - CAM_Y + 48) > 368)) return;
    actor_step(a);
    switch (a->phase)
    {
    case K_WAIT:
        if ((u16)(dx + 160) <= 320 && (u16)(py - AY(a) + 32) <= 64) { midboss_active = 1; a->phase = K_WALK; }
        return;
    case K_WALK:
        a->x += DIR(a) * 224;
        knight_clamp(a);
        if ((u16)(dx + 47) <= 94)
        {
            a->facing = dx >= 0;            /* turn away */
            a->timer = knight_backoff[rnd() & 3];
            a->phase = K_BACK;
            chain(a, 0);
            return;
        }
        if (!((game_frame + (a - actors)) & 63) && FACE(a) != (dx < 0))
        {
            a->facing ^= 1;
            a->phase = K_TURN;
            a->timer = 40;
            chain(a, 6);
            return;
        }
        if (!--a->v[0])
        {
            u16 f = knight_face[rnd() & 7];
            a->v[0] = f != FACE(a) ? 32 : 96;
            if (f != FACE(a)) { a->facing = f; chain(a, 0); }
        }
        if (!--a->v[1])
        {
            a->v[1] = 20;
            if ((rnd() & 7) < 2 + (rank_row() >= 12 ? rank_row() - 11 : 0))
            {
                a->phase = K_ATTACK;
                a->timer = 80;
                a->v[2] = (rnd() & 7) < 4;
                chain(a, a->v[2] ? 4 : 2);
            }
        }
        return;
    case K_ATTACK:
        if (a->timer == 32) lance_spawn(a, a->v[2] ? -16 : 0);
        if (!--a->timer) { a->phase = K_WALK; chain(a, 0); }
        return;
    case K_TURN:
        if (!--a->timer) { a->phase = K_WALK; chain(a, 0); }
        return;
    case K_BACK:
        a->x += DIR(a) * 224;
        knight_clamp(a);
        if (!--a->timer || (u16)(dx + 160) > 320) { a->phase = K_WALK; face_player(a); chain(a, 0); }
        return;
    }
}

void lance_update(Actor *a)
{
    s16 out;
    if (!a->state) { a->state = 2; a->active = 1; actor_anim(a, a->variant ? 1 : 0); return; }
    out = (AX(a) - a->v[0]) * DIR(a);
    if (a->state == 2)
    {
        a->x += DIR(a) * 1536;
        if (out + 6 >= lance_reach[a->variant] || lance_back)
        {
            a->state = 4;
            if (!a->variant) lance_back = 1;
        }
        return;
    }
    a->x -= DIR(a) * 1536;
    if (out <= 6) actor_free(a);
}

/* ---- Sinking pillars (5.4, class 6 kind 6 variants 0 and 1): stood on, they
 * sink 1.6 px a frame for 340 / 280 frames. Their top is 32 above them. */
void pillar_update(Actor *a)
{
    switch (a->state)
    {
    case 0:
        if (checkpoint >= 5) { actor_free(a); return; }
        a->state = 2;
        a->active = 1;
        a->v[1] = AX(a);
        actor_anim(a, a->variant);
        return;
    case 2:
        if (a->flags & STOOD) { a->state = 4; a->timer = a->variant ? 280 : 340; }
        a->flags &= ~STOOD;
        return;
    case 4:
        a->flags &= ~STOOD;
        a->y -= 409;
        if (!--a->timer) actor_free(a);
        return;
    }
}

/* ---- Bob of the boat and the rafts (0x30DA6): a triangle of +-2 px (60
 * frames), +-6 px in the swell. v[4] rest y, v[3] phase. */
static void bob(Actor *a)
{
    /* Triangle over 60 frames: -1 .. +1 in 1/15 steps (x2 or x6 px). */
    static const s8 tri[60] = { -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                                15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14 };
    s16 t = a->v[3] + 1;
    if (t >= 60) t = 0;
    a->v[3] = t;
    a->y = ((s32)a->v[4] << 8) + (CAM_X >= SWELL_X ? tri[t] * 102 : tri[t] * 34);   /* +-6 / +-2 px (8.8) */
}

/* ---- Willow's boat (5.5, class 6 kind 3): it moves with Willow once
 * boarded; at the far shore it sinks. */
void boat_update(Actor *a)
{
    s16 px = player.x >> 8;
    switch (a->state)
    {
    case 0:
        if (checkpoint >= 7 && px >= LAKE_X && px <= 5776) a->x = player.x;   /* a restart on the lake */
        a->state = 2;
        a->active = 1;
        a->v[4] = AY(a);
        a->phase = 0;
        actor_anim(a, 0);
        return;
    case 2:
        bob(a);
        a->v[1] = AX(a);
        if (a->phase == 0)
        {
            if (a->flags & STOOD) a->phase = 2;
            a->flags &= ~STOOD;
            return;
        }
        a->flags &= ~STOOD;
        if (a->phase == 2)
        {
            if (px < AX(a)) return;
            boat_drift = 1;
            sound_post(SND_MUSIC_LAKE);
            sound_post(0x01FA);
            a->phase = 4;
        }
        if (player.x < (s32)LAKE_X << 8) player.x = (s32)LAKE_X << 8;
        a->x = player.x;
        a->v[1] = AX(a);
        if (a->v[3] == 0 || a->v[3] == 30) actor_anim(a, a->v[3] ? 2 : 1);   /* tilt one way rising, the other falling */
#ifndef EXP_NOSWELL
        if (a->phase == 4 && CAM_X >= SWELL_X) { swell = 1; a->phase = 6; }
#else
        if (a->phase == 4 && CAM_X >= SWELL_X) a->phase = 6;
#endif
        if (a->phase == 6 && terrain_floor(AX(a) + 32, AY(a) + 8, &(s16){0}))
        {
            swell = 2;
            boat_drift = 0;
            a->state = 4;
            a->timer = 16 + 96;
        }
        return;
    case 4:                                 /* 16 frames, then sinking 0.25 px a frame for 96 */
        a->flags &= ~STOOD;
        if (a->timer <= 96) { a->y -= 64; a->active = 2; }
        if (!--a->timer) actor_free(a);
        return;
    }
}

/* ---- Enemy raft (5.7, class 6 kind 5) and its crew. v[0] rider slots + 1
 * (low byte rider 0, high byte rider 1), v[2] rider 0 gone, v[3] rider 1 gone. */
static const u8 crew_pick[16] = { 0, 2, 4, 6, 4, 2, 0, 4, 0, 4, 4, 0, 4, 6, 4, 4 };
static const u8 crew[8][2] = { { F_RARCHER, F_RARCHER }, { 0, 0 }, { F_RSPEAR, F_RSPEAR }, { 0, 0 },
                               { F_RARCHER, F_RSPEAR }, { 0, 0 }, { F_RSPEAR, F_RARCHER } };

void raft_update(Actor *a)
{
    s16 px = player.x >> 8;
    switch (a->state)
    {
    case 0:
        {
            u16 c = crew_pick[rnd() & 15], k;
            a->state = 2;
            a->active = 2;
            a->v[4] = AY(a);
            actor_anim(a, 0);
            for (k = 0; k < 2; k++)
            {
                Actor *r = actor_new(crew[c][k]);
                if (!r) { if (k) a->v[3] = 1; else { actor_free(a); return; } continue; }
                r->variant = 6;
                r->facing = FACE(a);
                r->sub = k;                 /* +50: its place on the raft */
                r->link = (a - actors) + 1;
                r->x = a->x; r->y = a->y;
            }
            a->vx = FACE(a) ? -384 : 384;
            a->timer = 48;
            a->phase = 0;
        }
        return;
    case 2:
        bob(a);
        if (a->v[2] && a->v[3]) { a->state = 4; a->timer = 16 + 96; return; }
        if (a->phase == 0)
        {
            a->x += a->vx;
            if (!--a->timer) a->phase = 2;
            return;
        }
        {
            s16 target = px + (FACE(a) ? 100 : -100);
            a->x += target > AX(a) ? 448 : -128;
            if (FACE(a) && AX(a) > CAM_X + 384) a->x = (s32)(CAM_X + 384) << 8;
            if (!FACE(a) && AX(a) < CAM_X) a->x = (s32)CAM_X << 8;
        }
        return;
    case 4:
        if (a->timer <= 96) a->y -= 64;
        if (--a->timer) return;
        raft_quota[FACE(a)]++;
        actor_free(a);
        return;
    }
}

/* A rider on its raft: at the raft + (+-dx, 16); FALSE when the raft is gone. */
static Actor *raft_of(Actor *a)
{
    Actor *r = a->link ? actors + a->link - 1 : NULL;
    return r && r->fam == F_RAFT ? r : NULL;
}

static void rider_gone(Actor *a)
{
    Actor *r = raft_of(a);
    if (r) { if (a->sub) r->v[2] = 1; else r->v[3] = 1; }
}

static bool rider_place(Actor *a, s16 dx)
{
    Actor *r = raft_of(a);
    if (!r) return FALSE;
    if (FACE(r)) dx = -dx;
    a->x = r->x + ((s32)(a->sub ? dx : -dx) << 8);
    a->y = r->y + (16 << 8);
    return TRUE;
}

static bool partner_gone(Actor *a)
{
    Actor *r = raft_of(a);
    return !r || (a->sub ? 1 : r->v[2]);
}

/* Raft archer (class 4 kind 3 variant 6): energy 1, the raft pose (chains
 * 50/51), the crossbowman's death (chain 52). v[0] attack timer. */
void rarcher_update(Actor *a)
{
    s16 px = player.x >> 8, dx = px - AX(a);
    if (!a->state)
    {
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = 1;
        a->phase = 0;
        face_player(a);
        chain(a, 50);
        return;
    }
    if (a->state == 4) { if (!--a->timer) { rider_gone(a); actor_free(a); } return; }
    if (encounter_clear) { sound_post(SND_EXPLODE); effect(F_BURST, AX(a), AY(a), 5); rider_gone(a); actor_free(a); return; }
    if (!rider_place(a, 16)) { actor_free(a); return; }
    if (a->hp != a->last_hp && !a->hp)
    {
        sound_post(SND_EXPLODE);
        award(0);
        drop(a, (rnd() & 15) < 14 ? 1 : 2);
        a->state = 4;
        a->active = 2;
        a->timer = 16;
        actor_anim(a, 52);
        return;
    }
    actor_step(a);
    face_player(a);
    if (a->phase == 0)
    {
        if ((u16)(dx + 255) > 510 || !partner_gone(a)) return;
        a->phase = 2;
        a->v[0] = 1;
    }
    if (--a->v[0]) return;
    a->v[0] = 82;
    {
        Actor *s = actor_new(F_ARROW);
        if (!s) return;
        sound_post(SND_ARROW);
        s->sub = FACE(a) ? 12 : 4;
        s->x = a->x + ((s32)(FACE(a) ? -10 : 10) << 8);
        s->y = a->y + (4 << 8);
    }
    chain(a, 50);
}

/* Raft spearman (class 4 kind 14 variant 6): energy 4, throws the returning
 * axe (the stage-1 spearman's art). v[0] attack timer. */
enum { SP_STAND = 2, SP_HURT = 4, SP_THROW = 10 };
void rspear_update(Actor *a)
{
    s16 px = player.x >> 8, dx = px - AX(a);
    if (!a->state)
    {
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = 4;
        a->phase = 0;
        face_player(a);
        chain(a, SP_STAND);
        return;
    }
    if (a->state == 4) { if (!--a->timer) { rider_gone(a); actor_free(a); } return; }
    if (encounter_clear) { sound_post(SND_EXPLODE); effect(F_BURST, AX(a), AY(a), 5); rider_gone(a); actor_free(a); return; }
    if (!rider_place(a, 8)) { actor_free(a); return; }
    if (hit(a, SND_HIT, 0))
    {
        if (!a->hp)
        {
            sound_post(SND_EXPLODE);
            award(4);
            drop(a, 2);
            a->state = 4;
            a->active = 2;
            a->timer = 10;
            chain(a, SP_HURT);
            return;
        }
        a->v[0] = 48;                       /* phase 20: 48 frames, then the cycle again */
        a->v[5] = 15;
        chain(a, SP_HURT);
    }
    immune_tick(a);
    actor_step(a);
    face_player(a);
    if (a->phase == 0)
    {
        if ((u16)(dx + 159) > 318 || !partner_gone(a)) return;
        a->phase = 2;
        a->v[0] = 1;
    }
    if (--a->v[0]) return;
    a->v[0] = 96;
    {
        Actor *x = actor_new(F_AXE);
        if (!x) return;
        x->facing = FACE(a);
        x->x = a->x + ((s32)(FACE(a) ? -32 : 32) << 8);
        x->y = a->y + (8 << 8);
        sound_post(SND_SPEAR);
    }
    chain(a, SP_THROW);
}

/* ---- Leaping fish (5.9, class 4 kind 22). Phases: 4 choose, 6 swim, 8 leap,
 * 10 submerged, 12 leave. v[0] leap type (0, 2, 4), v[1] gravity, v[2] pose
 * step, t2 frames per pose. */
enum { FC_SWIM = 0, FC_LEAP_R = 2, FC_LEAP_L = 7, FC_DIP = 12, FC_UP = 14, FC_SURFACE = 16 };
static const s8 leap_far[16] = { 2, 0, 0, 4, -1, 0, 0, -1, 2, 0, -1, 0, 0, 0, -1, 4 };
static const s8 leap_mid[16] = { 2, -1, 2, 4, 0, 2, -1, 2, 2, -1, 2, 0, 2, -1, 2, 4 };
static const s8 leap_near[16] = { 4, 0, 2, 4, -1, 4, -1, 4, 4, -1, 4, 2, 4, 0, -1, 4 };
static const s16 leap_vy[3] = { 1792, 1792, 1536 }, leap_g[3] = { -56, -72, -72 };
static const u8 leap_step[3] = { 10, 8, 6 };

static void fish_leap(Actor *a, u16 type)
{
    a->v[0] = type;
    a->phase = 8;
    a->sub = 0;
    a->vy = leap_vy[type >> 1];
    a->v[1] = leap_g[type >> 1];
    a->v[2] = 0;
    a->t2 = leap_step[type >> 1];
    a->timer = a->t2;
    actor_anim(a, FACE(a) ? FC_LEAP_L : FC_LEAP_R);
}

void fish_update(Actor *a)
{
    s16 px = player.x >> 8, d = px - AX(a);
    if (d < 0) d = -d;
    if (!a->state)
    {
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = 1;
        a->facing = time_stop ? 1 : (rnd() & 15) < 8;
        a->x = (s32)(FACE(a) ? CAM_X + 416 : CAM_X - 32) << 8;
        a->y = (s32)WATER_Y << 8;
        a->phase = 4;
        chain(a, FC_SWIM);
        return;
    }
    if (encounter_clear) { sound_post(SND_EXPLODE); effect(F_BURST, AX(a), AY(a), 4); fish_pop--; actor_free(a); return; }
    if (a->hp != a->last_hp && !a->hp)
    {
        effect(F_BURST, AX(a), AY(a), 4);
        sound_post(SND_EXPLODE);
        award(4);
        drop(a, rnd() & 3);
        fish_pop--;
        actor_free(a);
        return;
    }
    a->last_hp = a->hp;
    actor_step(a);
    if (a->phase != 8 && a->phase != 10) a->x += DIR(a) * 512;
    if (fish_end && a->phase != 8) a->phase = 12;
    switch (a->phase)
    {
    case 4:
        a->phase = 6;
        a->sub = d >= 128 ? 0 : d >= 80 ? 2 : d >= 48 ? 4 : 0;
        if (d < 48) { a->phase = 10; a->timer = 4; }
        return;
    case 6:
        if ((DIR(a) > 0) != (px > AX(a)) && d > 16) { a->phase = 10; a->timer = 4; return; }   /* swum past the player */
        {
            s8 t = -2;
            if (a->sub == 0 && d < 144) { t = leap_far[rnd() & 15]; if (t < 0) { a->sub = 2; return; } }
            else if (a->sub == 2 && d < 96) { t = leap_mid[rnd() & 15]; if (t < 0) { a->sub = 4; return; } }
            else if (a->sub == 4 && d < 48) { t = leap_near[rnd() & 15]; if (t < 0) { a->phase = 10; a->timer = 4; return; } }
            if (t >= 0) fish_leap(a, t);
        }
        return;
    case 8:
        a->x += DIR(a) * 512;
        if (a->sub == 0)
        {
            a->y += a->vy;
            a->vy += a->v[1];
            if (a->v[2] < 4 && !--a->timer) { a->timer = a->t2; a->v[2]++; actor_anim(a, (FACE(a) ? FC_LEAP_L : FC_LEAP_R) + a->v[2]); }
            if (a->vy < 0 && AY(a) <= WATER_Y)
            {
                if (!off_camera(a)) sound_post(SND_SPLASH);
                effect(F_SPLASH, AX(a), WATER_Y + 12, 0);
                a->sub = 2;
                a->vy = -640;
                a->v[1] = 40;
                a->timer = 3;
                chain(a, FC_DIP);
            }
            return;
        }
        if (a->sub == 2)
        {
            a->y += a->vy;
            a->vy += a->v[1];
            if (a->timer && !--a->timer) a->v[1] = 40;
            if (a->vy > 0 && AY(a) >= WATER_Y) { a->y = (s32)WATER_Y << 8; a->sub = 4; a->timer = 4; chain(a, FC_UP); }
            return;
        }
        if (!--a->timer) { a->phase = 10; a->timer = 4; chain(a, FC_SWIM); }
        return;
    case 10:                                /* under the surface while the player is near */
        if (a->sub == 6) { if (!--a->timer) { face_player(a); a->phase = 4; chain(a, FC_SWIM); } return; }
        if (--a->timer) return;
        a->timer = 4;
        if (d >= 160) { a->sub = 6; a->timer = 16; chain(a, FC_SURFACE); }
        return;
    case 12:
        if (!(game_frame & 3) && off_camera(a)) { fish_pop--; actor_free(a); }
        return;
    }
}

/* ---- Hawk variant 1 (4.2): straight across, the high ones carrying a rock.
 * v[0] carrying, v[1] drift counter, v[2] drift. */
static const u8 rock_bits[8] = { 4, 8, 8, 9, 10, 12, 16, 16 };

void hawk3_update(Actor *a)
{
    s16 px = player.x >> 8, py = player.y >> 8;
    if (!a->state)
    {
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = 3;
        a->v[1] = 40;
        a->v[2] = 128;
        chain(a, a->v[0] ? 0 : 2);
        return;
    }
    if (hit(a, SND_HIT, 0))
    {
        if (!a->hp) { hawk_alive--; die(a, 4, 4); return; }
        a->phase = 1;
        a->timer = 10;
        chain(a, 4);
    }
    actor_step(a);
    if (a->phase == 1) { if (!--a->timer) { a->phase = 0; a->active = 1; chain(a, a->v[0] ? 0 : 2); } return; }
    a->x += a->vx;
    a->y += a->v[2];
    if (!--a->v[1]) { a->v[1] = 40; a->v[2] = -a->v[2]; }
    if (a->v[0] && !(game_frame & 3) && AY(a) - py >= 48 && AY(a) <= CAM_Y + 256 &&
        (u16)(px - (AX(a) - DIR(a) * 16) + 10 + 20) <= 40 && (rnd() & 15) < rock_bits[(rank >> 12) >> 1])
    {
        Actor *r = actor_new(F_ROCK3);
        if (r) { r->x = a->x - ((s32)DIR(a) * 16 << 8); r->y = a->y - (24 << 8); }
        a->v[0] = 0;
        chain(a, 2);
    }
    if (!(game_frame & 7) && off_camera(a)) { hawk_alive--; actor_free(a); }
}

void rock3_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 1; a->vy = 0; actor_anim(a, 0); return; }
    actor_step(a);
    a->vy = a->vy - 32 < -1792 ? -1792 : a->vy - 32;
    a->y += a->vy;
    if (terrain_floor(AX(a), AY(a) - 4, &(s16){0}) || AY(a) < CAM_Y - 64) actor_free(a);
}

/* ---- The storm (2.6, palette controller kind 3): camera x 3456..6272, day
 * to night, lightning, thunder; back to day once the boss is beaten.
 * States: tools/stormpack.py (8 x lines 2-3: 0 day .. 5 night, 6 / 7 flash). */
extern const u16 s3_storm[];
static const u8 seq_night[] = { 0, 1, 2, 3, 4, 5, 0xFF }, seq_double[] = { 5, 6, 5, 6, 7, 5, 0xFF },
                seq_single[] = { 5, 6, 7, 5, 0xFF }, seq_day[] = { 5, 4, 3, 2, 1, 0, 0xFF };
static const u8 flash_wait[4] = { 63, 47, 31, 79 };
static const u8 *storm_seq;
static u8 storm_on, storm_at, storm_tick, storm_shown;
static u16 thunder_timer, flash_timer;

static void storm_show(u16 st)
{
    if (st == storm_shown) return;
    storm_shown = st;
    PAL_setColors(33, s3_storm + st * 32 + 1, 9, DMA_QUEUE);         /* pens 1-9: 10-15 are the water's cycle */
    PAL_setColors(49, s3_storm + st * 32 + 17, 9, DMA_QUEUE);
}

static void storm_update(void)
{
    if (!storm_on)
    {
        if (CAM_X < 3456 || storm_on == 2) return;
        storm_on = 1;
        storm_seq = seq_night; storm_at = 0; storm_tick = 8;
        thunder_timer = 600;
        flash_timer = 63;
    }
    if (storm_on == 2) return;
    if (boss3_dead && storm_seq != seq_day) { storm_seq = seq_day; storm_at = 0; storm_tick = 8; }
    if (storm_seq)
    {
        storm_show(storm_seq[storm_at]);
        if (!--storm_tick)
        {
            storm_tick = 8;
            if (storm_seq[++storm_at] == 0xFF)
            {
                if (storm_seq == seq_day) { storm_on = 2; return; }
                storm_seq = NULL;
            }
        }
    }
    if (boss3_dead) return;
    if (!--thunder_timer) { sound_post(0x8F); thunder_timer = 300; }
    if (!storm_seq && !--flash_timer)
    {
        flash_timer = flash_wait[rnd() & 3];
        storm_seq = (rnd() & 1) ? seq_double : seq_single;
        storm_at = 0;
        storm_tick = 8;
    }
}

/* ---- Spawn events (3.2). */
static const u8 hawk_cap[16] = { 1, 1, 2, 2, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 7, 8 };
static const s8 hawk_less[16] = { -18, -10, 0, 0, 0, 0, 0, 10, 10, 20, 20, 30, 40, 40, 40, 59 };
static const u8 hawk_height[16] = { 2, 3, 0, 1, 0, 2, 1, 0, 0, 1, 2, 0, 2, 3, 0, 1 };
static const s8 hawk_dy[4] = { 72, 56, 0, -40 };
static const u8 raft_wait[16] = { 32, 1, 64, 32, 16, 32, 1, 64, 32, 16, 32, 1, 32, 16, 1, 1 };
static const u8 fish_cap[16] = { 4, 6, 6, 6, 6, 6, 6, 7, 7, 8, 9, 10, 11, 12, 13, 14 };
static const u8 fish_wait[16] = { 64, 32, 48, 64, 80, 80, 64, 48, 48, 80, 48, 64, 32, 48, 64, 32 };
static u16 hawk_timer, raft_timer, fish_timer;
static u8 raft_done, fish_on;

void stage3_events_reset(void)
{
    hawk_alive = 0;
    hawk_left = 18;
    hawk_timer = (rnd() & 15) < 6 ? 10 : 1;
    raft_quota[0] = raft_quota[1] = 1;
    raft_timer = raft_wait[rnd() & 15];
    raft_done = 0;
    fish_pop = 0;
    fish_on = 0;
    fish_timer = 0;
    fish_end = 0;
    boat_drift = 0;
    swell = 0;
    swell_y = swell_v = swell_sub = 0;
    swell_dir = 1;
    lance_back = 0;
    encounter_clear = 0;
    storm_on = 0;
    storm_seq = NULL;
    storm_shown = 0;
}

void stage3_events(void)
{
    s16 py = player.y >> 8;
    storm_update();
    /* Hawks over the sea pillars (event 3), every other frame. */
    if (hawk_left && !(game_frame & 1) && CAM_X >= 1008 && CAM_X < 1696 && !--hawk_timer)
    {
        hawk_timer = 1;
        if (hawk_alive < hawk_cap[rank_row()])
        {
            Actor *h = actor_new(F_HAWK3);
            if (h)
            {
                u16 i = hawk_height[rnd() & 15];
                h->facing = (rnd() & 15) < 12;
                h->x = (s32)(h->facing ? CAM_X + 408 : CAM_X - 24) << 8;
                h->vx = ((rnd() & 15) < 6 ? 512 : 384) * (h->facing ? -1 : 1);
                h->y = (s32)(py + hawk_dy[i]) << 8;
                h->v[0] = i <= 1;
                hawk_alive++;
                hawk_left--;
                hawk_timer = ((rnd() & 15) < 8 ? 120 : 60) - hawk_less[rank_row()];
            }
        }
    }
    /* Enemy rafts (event 2): one from each side at a time. */
    if (!raft_done && CAM_X >= 2816)
    {
        if (CAM_X >= 3824) raft_done = 1;
        else if ((raft_quota[0] || raft_quota[1]) && !--raft_timer)
        {
            Actor *r = actor_new(F_RAFT);
            raft_timer = raft_wait[rnd() & 15];
            if (r)
            {
                r->facing = raft_quota[1] != 0;
                raft_quota[r->facing]--;
                r->x = (s32)(r->facing ? CAM_X + 440 : CAM_X - 56) << 8;
                r->y = (s32)DECK_Y << 8;
            }
        }
    }
    /* Leaping fish (event 1) from camera x 3840 until the arena. */
    if (!fish_end && CAM_X >= 3840)
    {
        if (!fish_on) { fish_on = 1; fish_timer = 0; }
        if (!fish_timer) { if (fish_pop < fish_cap[rank_row()]) fish_timer = fish_wait[rnd() & 15]; }
        else if (!--fish_timer)
        {
            Actor *f = actor_new(F_FISH);
            if (f) fish_pop++;
            else fish_timer = 1;
        }
    }
    /* The arena (6.2): at x 5760 the lake's enemies explode and the camera
     * glides 2 px an update to 5760, then stays. */
    if ((player.x >> 8) >= 5760 && !encounter_clear) { encounter_clear = 1; fish_end = 1; }
    if (encounter_clear && CAM_X < 5760)
    {
        player.cam_x = CAM_X + 2 > 5760 ? 5760 : CAM_X + 2;
        player.cam_min_x = player.cam_x;
    }
    /* The sea swell (2.4, 0x42C5A): the view's y velocity moves 3 a frame
     * toward +-128 (8.8) and back: +-10.5 px, ~172 frames; at the far shore
     * it glides back 3 px a frame. */
    if (swell == 1)
    {
        swell_v += 3 * swell_dir;
        if (swell_v >= 128) swell_dir = -1;
        if (swell_v <= -128) swell_dir = 1;
        swell_sub += swell_v;
        swell_y = swell_sub >> 8;
    }
    else if (swell == 2)
    {
        swell_sub = 0; swell_v = 0;
        swell_y = swell_y > 3 ? swell_y - 3 : swell_y < -3 ? swell_y + 3 : 0;
        if (!swell_y) swell = 0;
    }
}
