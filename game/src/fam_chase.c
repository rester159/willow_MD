/* Stage 2's wagon chase enemies and their spawn events (specs/stage2.md
 * 5.9-5.11, 5.13, 5.14): mounted archers (class 4 kind 20) and their horses
 * (class 12 kind 15), boarding soldiers (kind 21), hawks with rocks (kind 15,
 * class 10 kind 9) and the hound (kind 1 variant 2).
 *
 * The chase strip: the actors' camera stays at 1536 (screen x 0..384 = world
 * 1536..1920), the road moves under them at chase_speed (8.8). Distance =
 * virtual camera - 1536. At the end (chase_end) every enemy explodes without
 * reward (FF83D0).
 *
 * Chains (tools/pack_stage2.sh): rider 0 ride, 1 low shot, 2 high shot, 3
 * right-side shot, 4 hit / thrown, 5 lying; boarder 0 ride, 1 strike, 2 jump,
 * 3 fall, 4 landed, 5 dismount, 6 thrown, deck pairs (right / left) 7 walk,
 * 9 stand, 11 stand 2, 13 rest, 15 hit, 17 leap attack; hawk 0 flight, 1
 * flight 2, 2-17 dive by direction, 18 hit; rock 0; hound 0 run, 2 leap, 4
 * fall, 6 run on the road (+ facing); the riders' horse is the wagon horse's
 * chain 1 (root 0x30400). Approximations are listed in M1_NOTES. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"

extern s32 chase_vx;
extern u16 chase_speed;
extern u8 chase_end;
extern s16 wagon_step;

#define CX 1536                         /* the actors' camera during the chase */
#define ROAD_Y 56                       /* bodies and the hound on the road */
#define RANK_HI (rank >> 8)

static u8 rider_left, rider_right, boarder_alive, boarded, hawk_alive, hound_quota;
static u16 ev_timer[6];

static u16 dist(void) { return (u16)((chase_vx >> 8) - CX); }

static void boom(Actor *a)              /* FF83D0: explodes, no reward */
{
    sound_post(SND_EXPLODE);
    effect(F_BURST, AX(a), AY(a), 5);
    actor_free(a);
}

static void horse_for(Actor *a)
{
    Actor *h = actor_new(F_RHORSE);
    if (!h) return;
    h->x = a->x;
    h->y = a->y;
    h->link = (a - actors) + 1;
    h->v[0] = a->fam;
}

/* ---- Spawn events 4-9 (5.14, 5.13). */
static const u16 rider_timer[16] = { 256, 180, 140, 128, 128, 128, 118, 30, 20, 10, 10, 10, 10, 10, 10, 10 };
static const u8 rider_v1_near[16] = { 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1 };     /* 0x390E4 */
static const u8 rider_v1_far[16] = { 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1 };      /* 0x390F4 */
static const u16 hawk_timer[16] = { 240, 224, 224, 224, 224, 208, 192, 176, 160, 144, 128, 112, 96, 80, 64, 48 };
static const u8 hawk_cap[16] = { 1, 1, 2, 3, 4, 4, 5, 5, 5, 5, 6, 6, 7, 7, 8, 9 };
static const u8 hound_pick_a[16] = { 2, 1, 2, 3, 4, 5, 0, 4, 4, 2, 3, 2, 4, 3, 3, 5 };     /* 0x38858 */
static const u8 hound_pick_b[16] = { 0, 1, 2, 3, 4, 2, 1, 0, 3, 1, 1, 5, 0, 2, 0, 2 };     /* 0x38868 */

void chase_reset(void)
{
    rider_left = rider_right = boarder_alive = boarded = hawk_alive = 0;
    hound_quota = 1;
    memset(ev_timer, 0, sizeof ev_timer);
}

static Actor *spawn(u16 fam, s16 x, s16 y, u16 facing, u16 variant)
{
    Actor *a = actor_new(fam);
    if (!a) return NULL;
    a->x = (s32)x << 8;
    a->y = (s32)y << 8;
    a->facing = facing;
    a->variant = variant;
    return a;
}

static bool in(u16 d, u16 lo, u16 hi) { return d >= lo && d < hi; }

void chase_events(void)
{
    u16 d;
    if (!chase_vx || chase_end) return;
#ifdef NO_CHASE_ENEMIES
    return;                             /* measurement builds */
#endif
    d = dist();
    /* 4: hawks from 12800 */
    if (d >= 12800)
    {
        if (ev_timer[0]) ev_timer[0]--;
        else if (hawk_alive < hawk_cap[RANK_HI >> 4] && spawn(F_HAWK, CX, 168, 0, 0))
        {
            hawk_alive++;
            ev_timer[0] = hawk_timer[RANK_HI >> 4];
        }
    }
    /* 5: riders from the left */
    if (in(d, 1024, 4352) || in(d, 12800, 17152) || in(d, 24320, 27648))
    {
        if (ev_timer[1]) ev_timer[1]--;
        else if (!rider_left)
        {
            u16 r = rnd() & 15;
            if (spawn(F_RIDER, CX, 92, 0, d > 2480 ? rider_v1_far[r] : rider_v1_near[r])) rider_left = 1;
            ev_timer[1] = rider_timer[((RANK_HI >> 3) & 30) >> 1];
        }
    }
    /* 6: riders from the right */
    if (in(d, 8704, 18176) || in(d, 22016, 27392))
    {
        if (ev_timer[2]) ev_timer[2]--;
        else if (!rider_right)
        {
            if (spawn(F_RIDER, CX + 384, 92, 1, 0)) rider_right = 1;
            ev_timer[2] = rider_timer[((RANK_HI >> 3) & 30) >> 1];
        }
    }
    /* 7: boarding soldiers, 4608..12544 */
    if (d > 4608 && d <= 12544)
    {
        if (ev_timer[3]) ev_timer[3]--;
        else if (!boarder_alive)
        {
            if (spawn(F_BOARDER, CX, 92, 0, 0)) boarder_alive = 1;
            ev_timer[3] = rider_timer[((RANK_HI >> 3) & 30) >> 1];
        }
    }
    /* 8, 9: the hound */
    if (in(d, 5632, 12800) || in(d, 17152, 28160))
    {
        if (ev_timer[4]) ev_timer[4]--;
        else
        {
            ev_timer[4] = 16 * (d < 20480 ? hound_pick_a[rnd() & 15] : hound_pick_b[rnd() & 15]);
            if (hound_quota && spawn(F_HOUND, CX - 32, ROAD_Y, 0, 2)) hound_quota--;
        }
    }
}

/* ---- The riders' horse: runs with its rider; on alone when the rider is gone. */
void rhorse_update(Actor *a)
{
    Actor *r = a->link ? actors + a->link - 1 : NULL;
    if (!a->state) { a->state = 2; a->active = 2; actor_anim(a, 1); }
    if (r && r->fam == (u8)a->v[0] && r->state < 4)
    {
        if (!r->v[5]) { a->x = r->x; a->y = r->y; }     /* a boarder on the deck: it keeps its place */
    }
    else
    {
        a->link = 0;
        a->vx = 512;                    /* runs on ahead */
        a->x += a->vx;
        if (AY(a) > 92) a->y -= 256;
    }
    actor_step(a);
    {
        /* Every horse runs on the same clock (the road-speed ticks): they
         * share one pose, one pose-cache entry for these 81-tile frames. */
        static u16 seen, shared;
        if (seen != game_frame) { seen = game_frame; shared = a->pose; }
        else a->pose = shared;
    }
    if (!(game_frame & 15) && (u16)(AX(a) - CX + 64) > 512) actor_free(a);
}

/* ---- Mounted archer (5.9). Fields: phase, timer, t2 stay timer, sub
 * aggression, v[0] side (0 left, 1 right), v[1] shot chain, v[2] shot y. */
enum { R_ENTER, R_RIDE, R_SHOOT, R_LEAVE, R_HIT };
static const u8 rider_roll[16][8] = {                                        /* 0x1B408 */
    { 0, 1, 0, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 0, 1, 0, 0 },
    { 0, 1, 0, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 0, 1, 0, 0 },
    { 0, 1, 0, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 0, 1, 0, 0 }, { 1, 1, 0, 0, 0, 1, 0, 0 },
    { 0, 1, 1, 1, 0, 1, 0, 0 }, { 0, 1, 1, 0, 1, 1, 1, 0 }, { 0, 1, 1, 1, 0, 1, 1, 1 }, { 1, 1, 0, 1, 1, 1, 1, 1 }
};
static const u8 rider_high[48] = {                                           /* 0x1B4E6 */
    0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1
};
static const u8 rider_drop[8] = { 1, 1, 1, 1, 2, 1, 1, 1 };                  /* 0x1B954 */

static void rider_arrow(Actor *a)
{
    Actor *s = actor_new(F_ARROW);
    sound_post(SND_ARROW);
    if (!s) return;
    s->sub = a->v[0] ? 12 : 4;
    s->x = a->x + ((a->v[0] ? -16 : 16) << 8);
    s->y = a->y + ((s32)a->v[2] << 8);
}

static void rider_shoot(Actor *a)
{
    if (a->v[0]) { a->v[1] = 3; a->timer = 48; a->v[2] = 10; }
    else if (a->variant ? (rnd() & 7) < 4 : rider_high[a->sub * 16 + (rnd() & 15)])
    { a->v[1] = 2; a->timer = 48; a->v[2] = 24; }
    else { a->v[1] = 1; a->timer = 56; a->v[2] = a->variant ? 16 : 10; }
    actor_anim(a, a->v[1]);
    a->phase = R_SHOOT;
}

void rider_update(Actor *a)
{
    s16 sx;
    switch (a->state)
    {
    case 0:
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = 2;
        a->v[0] = a->facing;
        a->phase = R_ENTER;
        actor_anim(a, 0);
        horse_for(a);
        return;
    case 4:                                 /* thrown off, falling to the road */
        a->x += a->vx;
        if (a->vy < 1792) a->vy += 32;
        a->y -= a->vy;
        if (AY(a) <= ROAD_Y)
        {
            a->y = (s32)ROAD_Y << 8;
            a->state = 6;
            actor_anim(a, 5);
            award(2);
            drop(a, rider_drop[rnd() & 7]);
        }
        return;
    case 6:                                 /* lying: the road carries it away */
        a->x -= chase_speed;
        if (AX(a) < CX - 64) actor_free(a);
        return;
    }
    if (chase_end) { if (a->v[0]) rider_right = 0; else rider_left = 0; boom(a); return; }
    if (a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        sound_post(SND_ENEMY_HIT);
        if (!a->hp)
        {
            if (a->v[0]) rider_right = 0; else rider_left = 0;
            a->state = 4;
            a->active = 2;
            a->vx = a->v[0] ? 768 : -768;   /* thrown away from its side */
            a->vy = 0;
            actor_anim(a, 4);
            return;
        }
        a->active = 2;
        a->phase = R_HIT;
        a->timer = 15;
        actor_anim(a, 4);
        return;
    }
    actor_step(a);
    a->vx = a->facing ? -128 : 384;         /* 0x1B2EE */
    a->x += a->vx;
    sx = AX(a) - CX;
    switch (a->phase)
    {
    case R_ENTER:
        if (a->variant)
        {
            if (sx >= 112) { a->facing ^= 1; a->phase = R_RIDE; a->t2 = 768; }
        }
        else if ((u16)(sx - 80) <= 224) { a->facing ^= 1; a->phase = R_RIDE; a->t2 = 768; }
        break;
    case R_RIDE:
        if (sx < 16 || sx > 368) { a->x = (s32)(CX + (sx < 16 ? 16 : 368)) << 8; a->facing ^= 1; }
        else if (!a->variant && sx > 112 && sx < 272)       /* keep out of the wagon's band */
        { a->x = (s32)(CX + (a->v[0] ? 272 : 112)) << 8; a->facing ^= 1; }
        if (!(game_frame & 15))
        {
            if (rnd() & 1) a->facing ^= 1;
            if (a->variant ? !(sx >= 128 && sx <= 256) && (rnd() & 7) < 4 : rider_roll[RANK_HI >> 4][rnd() & 7])
            { rider_shoot(a); break; }
        }
        if (!a->variant)
        {
            if (a->t2 == 512 || a->t2 == 256) a->sub++;
            if (!--a->t2) { a->phase = R_LEAVE; a->facing = a->v[0] ? 0 : 1; }
        }
        break;
    case R_SHOOT:
        if (a->timer == 16) rider_arrow(a);
        if (--a->timer) break;
        actor_anim(a, 0);
        if (a->variant) { a->phase = R_LEAVE; a->facing = 0; }     /* one arrow, then away */
        else a->phase = R_RIDE;
        break;
    case R_HIT:
        if (--a->timer) break;
        a->active = 1;
        a->phase = R_RIDE;
        actor_anim(a, 0);
        break;
    default:                                /* leaving the way it came */
        a->facing = a->variant ? 0 : a->v[0] ? 0 : 1;
        if ((u16)(sx + 32) > 448)
        {
            if (a->v[0]) rider_right = 0; else rider_left = 0;
            actor_free(a);
        }
        break;
    }
}

/* ---- Boarding soldier (5.10). Fields: phase, timer, t2 stay, sub action,
 * v[1] deck chain, v[2] deck waits left. */
enum { B_ENTER, B_PICK, B_STRIKE, B_SURGE, B_TOBOARD, B_JUMP, B_DECK, B_LEAP, B_RETURN, B_LEAVE, B_HIT };
static const u8 board_pick[16][16] = {                                       /* 0x1BF02 (rank rows) */
    { 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 }, { 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 },
    { 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 }, { 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 },
    { 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 }, { 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 },
    { 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 }, { 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 },
    { 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 }, { 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 },
    { 2, 4, 6, 2, 4, 6, 4, 4, 6, 2, 4, 6, 2, 4, 2, 4 }, { 2, 4, 6, 2, 4, 4, 2, 4, 6, 2, 4, 6, 2, 4, 2, 4 },
    { 2, 4, 4, 2, 4, 6, 2, 4, 6, 4, 4, 6, 2, 4, 2, 4 }, { 2, 4, 4, 2, 4, 6, 4, 4, 6, 2, 4, 4, 2, 4, 2, 4 },
    { 4, 4, 6, 4, 4, 6, 2, 4, 4, 2, 4, 6, 4, 4, 2, 4 }, { 2, 4, 4, 2, 4, 4, 4, 4, 4, 2, 4, 6, 2, 4, 4, 4 }
};
static const u8 board_repick[8] = { 2, 6, 6, 2, 2, 6, 6, 2 };                 /* 0x1C002 */
static const u8 deck_wait[4] = { 64, 128, 192, 255 };                        /* 0x1C21C */
static const u8 board_drop[8] = { 1, 2, 1, 1, 1, 1, 1, 1 };                  /* 0x1C6C2 */

static Actor *wagon(void)
{
    u16 i;
    for (i = pool_lo[C_SURFACE]; i < pool_lo[C_SURFACE + 1]; i++)
        if (actors[i].fam == F_WAGON) return actors + i;
    return NULL;
}

static void deck_pose(Actor *a, u16 c)
{
    a->v[1] = c;
    actor_anim(a, c + (a->facing & 1));
}

static void board_pick_action(Actor *a)
{
    u16 act = board_pick[RANK_HI >> 4][rnd() & 15];
    if (act == 4 && boarded) act = board_repick[rnd() & 7];
    a->sub = act;
    a->phase = act == 2 ? B_STRIKE : act == 4 ? B_TOBOARD : B_PICK;
    a->timer = 32;
    actor_anim(a, 0);
}

void boarder_update(Actor *a)
{
    s16 px = player.x >> 8, sx;
    switch (a->state)
    {
    case 0:
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = 2;
        a->phase = B_ENTER;
        a->t2 = 768;
        actor_anim(a, 0);
        horse_for(a);
        return;
    case 4:                                 /* thrown off */
        a->x += a->vx;
        a->y += a->vy;
        a->vy -= 48;
        if (a->vy < 0 && AY(a) <= ROAD_Y)
        {
            a->y = (s32)ROAD_Y << 8;
            sound_post(SND_EXPLODE);
            effect(F_BURST, AX(a), AY(a), 4);
            drop(a, board_drop[rnd() & 7]);
            a->state = 6;
            actor_anim(a, 6);
        }
        return;
    case 6:
        a->x -= chase_speed;
        if (AX(a) < CX - 64) actor_free(a);
        return;
    }
    if (chase_end) { boarder_alive = 0; if (a->v[5]) boarded = 0; boom(a); return; }
    if (a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        sound_post(SND_ENEMY_HIT);
        if (!a->hp)
        {
            award(2);
            boarder_alive = 0;
            if (a->v[5]) boarded = 0;
            a->v[5] = 1;                    /* off the horse for good */
            a->state = 4;
            a->active = 2;
            a->vx = px < AX(a) ? 256 : -256;
            a->vy = 512;
            actor_anim(a, 6);
            return;
        }
        a->active = 2;
        a->t2 = a->t2 > 8 ? a->t2 : 9;
        a->v[3] = a->phase;
        a->phase = B_HIT;
        a->timer = 8;
        if (a->v[5]) actor_anim(a, 15 + (a->facing & 1)); else actor_anim(a, 6);
        return;
    }
    actor_step(a);
    sx = AX(a) - CX;
    if (a->phase != B_DECK && a->phase != B_LEAP && a->phase != B_JUMP && a->phase != B_HIT && a->phase != B_LEAVE)
        if (!--a->t2) { a->phase = B_LEAVE; actor_anim(a, 0); }
    switch (a->phase)
    {
    case B_ENTER:                           /* rides in at 3 px a frame */
        a->x += 768;
        if (sx >= 32) board_pick_action(a);
        break;
    case B_PICK:                            /* rides alongside a while */
        if (!--a->timer) board_pick_action(a);
        break;
    case B_STRIKE:                          /* toward the player; surge when near and airborne */
        a->x += px > AX(a) ? 768 : -512;
        if ((u16)(px - AX(a) + 88) <= 176)
        {
            if (!player.airborne) { if (!--a->timer) board_pick_action(a); }
            else { a->phase = B_SURGE; a->timer = 32; actor_anim(a, 1); }
        }
        break;
    case B_SURGE:
        a->x += px > AX(a) ? 1024 : -1024;
        if (!--a->timer) board_pick_action(a);
        break;
    case B_TOBOARD:
        a->x += px > AX(a) ? 768 : -512;
        if ((u16)(px - AX(a) + 112) <= 224)
        {
            sound_post(0x90);
            a->v[5] = 1;                    /* leaves the horse */
            boarded = 1;
            a->phase = B_JUMP;
            a->vy = 1024;
            a->vx = px > AX(a) ? 256 : -256;
            actor_anim(a, 2);
        }
        break;
    case B_JUMP:
    {
        s16 top;
        a->x += a->vx;
        a->y += a->vy;
        if (a->vy > -1536) a->vy -= 32;
        if (a->vy < 0) actor_anim(a, 3);
        if (a->vy < 0 && surface_floor(AX(a), AY(a) - 24, &top))
        {
            a->y = (s32)(top + 24) << 8;
            a->facing = px < AX(a);
            a->phase = B_DECK;
            a->timer = deck_wait[rnd() & 3];
            deck_pose(a, 9);
        }
        else if (AY(a) <= ROAD_Y)           /* missed the deck: dies on the road */
        {
            a->hp = 0;
            a->last_hp = 1;
        }
        break;
    }
    case B_DECK:
    {
        Actor *w = wagon();
        a->x += wagon_step;
        if (w && (u16)(AX(a) - AX(w) + 56) > 112) a->x = w->x + (AX(a) > AX(w) ? 56 << 8 : -56 << 8);
        if (!(game_frame & 63)) { a->facing = px < AX(a); deck_pose(a, a->v[1]); }
        if ((u16)(px - AX(a) + 16) <= 32 && (u16)((player.y >> 8) - AY(a) + 16) <= 32)
        {
            a->phase = B_LEAP;
            a->vy = 768;
            a->vx = a->facing ? -256 : 256;
            deck_pose(a, 17);
            break;
        }
        if (--a->timer) break;
        if (board_repick[rnd() & 7] == 6)   /* back to the saddle */
        {
            a->phase = B_RETURN;
            a->timer = 24;
            actor_anim(a, 5);
        }
        else { a->timer = deck_wait[rnd() & 3]; deck_pose(a, rnd() & 1 ? 9 : 11); }
        break;
    }
    case B_LEAP:
    {
        s16 top;
        a->x += a->vx + wagon_step;
        a->y += a->vy;
        a->vy -= 48;
        if (a->vy < 0 && surface_floor(AX(a), AY(a) - 24, &top))
        {
            a->y = (s32)(top + 24) << 8;
            a->phase = B_DECK;
            a->timer = deck_wait[rnd() & 3];
            deck_pose(a, 9);
        }
        else if (AY(a) <= ROAD_Y) { a->hp = 0; a->last_hp = 1; }
        break;
    }
    case B_RETURN:
        a->y = (s32)92 << 8;
        if (--a->timer) break;
        boarded = 0;
        a->v[5] = 0;
        board_pick_action(a);
        break;
    case B_HIT:
        if (--a->timer) break;
        a->active = 1;
        a->phase = a->v[3];
        if (a->phase == B_DECK) deck_pose(a, 9); else actor_anim(a, 0);
        break;
    default:                                /* rides out left */
        a->x -= 512;
        if (sx < -32) { boarder_alive = 0; actor_free(a); }
        break;
    }
}

/* ---- Hawk with a rock (5.11). Fields: phase, timer leg, vx speed, v[0]
 * rock slot + 1, v[1] base y, sub dive direction. */
enum { H_FLY, H_DIVE, H_LOW, H_CLIMB, H_HIT };
static const u8 hawk_leg[16] = { 90, 90, 120, 90, 60, 90, 60, 90, 90, 60, 90, 90, 120, 60, 90, 120 };
static const s16 hawk_speed[16] = { 256, -128, 384, 256, 0, -128, 0, 512, 256, 0, 256, 256, 384, 0, 256, 384 };

void hawk_update(Actor *a)
{
    s16 px = player.x >> 8;
    switch (a->state)
    {
    case 0:
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = 2;
        a->y += (s32)(rnd() & 15) << 8;
        a->v[1] = AY(a);
        a->phase = H_FLY;
        a->vx = hawk_speed[rnd() & 15];
        a->timer = hawk_leg[rnd() & 15];
        actor_anim(a, 0);
        {
            Actor *r = actor_new(F_ROCK);
            if (r) { r->link = (a - actors) + 1; r->x = a->x - (16 << 8); r->y = a->y - (24 << 8); a->v[0] = (r - actors) + 1; }
        }
        return;
    case 4:                                 /* falling dead */
        a->vy += 40;
        a->y -= a->vy;
        if (AY(a) <= ROAD_Y)
        {
            sound_post(SND_EXPLODE);
            effect(F_BURST, AX(a), ROAD_Y, 5);
            award(4);
            drop(a, 4);
            actor_free(a);
        }
        return;
    }
    if (chase_end) { hawk_alive--; boom(a); return; }
    if (a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        sound_post(SND_ENEMY_HIT);
        a->v[0] = 0;                        /* drops the rock */
        if (!a->hp) { hawk_alive--; a->state = 4; a->active = 2; a->vy = 0; actor_anim(a, 18); return; }
        a->active = 2;
        a->v[2] = a->phase;
        a->phase = H_HIT;
        a->timer = 10;
        actor_anim(a, 18);
        return;
    }
    actor_step(a);
    switch (a->phase)
    {
    case H_FLY:                             /* legs of random speed, bobbing */
        a->x += a->vx;
        a->y = (s32)(a->v[1] + ((game_frame & 32) ? 4 : -4)) << 8;
        if (!--a->timer) { a->vx = hawk_speed[rnd() & 15]; a->timer = hawk_leg[rnd() & 15]; }
        if ((u16)(AX(a) - CX) > 352) a->vx = AX(a) < CX + 192 ? 256 : -256;
        if (a->v[0] && (u16)(px + 16 - AX(a) - 10 + 19) <= 38) a->v[0] = 0;     /* rock released over the player */
        if (!a->v[0]) { a->phase = H_DIVE; a->sub = 4; a->timer = 8; }     /* the swoop */
        break;
    case H_DIVE:                            /* turns 4 -> 8 -> 12 diving to the road */
        if (!--a->timer && a->sub < 12) { a->sub++; a->timer = 6; }
        actor_anim(a, 2 + a->sub);
        a->x += a->sub < 8 ? 384 : -384;
        a->y -= 768;
        if (AY(a) <= 88) { a->phase = H_LOW; a->timer = 64; a->active = 2; a->sub = 12; actor_anim(a, 2 + 12); }
        break;
    case H_LOW:                             /* low over the road, not hittable */
        a->x -= 512;
        if (!--a->timer) { a->phase = H_CLIMB; a->active = 1; actor_anim(a, 2 + 2); }
        break;
    case H_CLIMB:
        a->x += 1024;
        a->y += 512;
        break;
    default:
        if (--a->timer) break;
        a->active = 1;
        a->phase = a->v[2];
        actor_anim(a, 0);
        break;
    }
    if (!(game_frame & 15) && ((u16)(AX(a) - CX + 64) > 512 || AY(a) > 300)) { hawk_alive--; actor_free(a); }
}

/* Rock: carried, then falls; on the road it lies and slides with it. */
void rock_update(Actor *a)
{
    Actor *h = a->link ? actors + a->link - 1 : NULL;
    if (!a->state) { a->state = 2; a->active = 1; actor_anim(a, 0); return; }
    if (a->state == 2)
    {
        if (h && h->fam == F_HAWK && h->v[0] == (a - actors) + 1)
        {
            a->x = h->x - (16 << 8);
            a->y = h->y - (24 << 8);
            return;
        }
        a->link = 0;
        a->state = 4;
        a->vy = 0;
    }
    if (a->state == 4)
    {
        actor_step(a);
        a->vy += 32;
        a->y -= a->vy;
        if (surface_floor(AX(a), AY(a), &(s16){0})) { a->state = 8; a->timer = 20; a->active = 2; }
        else if (AY(a) <= 48) { a->y = (s32)48 << 8; a->state = 6; }
        return;
    }
    if (a->state == 6)                      /* on the road */
    {
        a->x -= chase_speed;
        if (AX(a) < CX - 32 || chase_end) actor_free(a);
        return;
    }
    if (!--a->timer) actor_free(a);         /* broke on the deck */
}

/* ---- The hound (5.13): runs beside the wagon, leaps onto it. Fields:
 * phase, timer, vy. */
enum { D_RUN, D_WAIT, D_LEAP, D_ROAD };
static const u8 hound_wait[4] = { 20, 40, 60, 80 };
static const u16 hound_road[8] = { 300, 60, 360, 240, 240, 240, 180, 240 };

void hound_update(Actor *a)
{
    s16 sx = AX(a) - CX;
    switch (a->state)
    {
    case 0:
        a->state = 2;
        a->active = 1;
        a->hp = a->last_hp = 2;
        a->phase = D_RUN;
        actor_anim(a, 0);
        return;
    case 4:
        a->x -= chase_speed;
        if (!--a->timer) actor_free(a);
        return;
    }
    if (chase_end) { boom(a); hound_quota = 1; return; }
    if (a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        sound_post(SND_ENEMY_HIT);
        if (!a->hp)
        {
            sound_post(SND_EXPLODE);
            effect(F_BURST, AX(a), AY(a), 4);
            award(2);
            drop(a, 2);
            hound_quota = 1;
            actor_free(a);
            return;
        }
    }
    actor_step(a);
    switch (a->phase)
    {
    case D_RUN:                             /* 1 px a frame relative to the strip */
        a->x += 256;
        if (!(game_frame & 15) && (u16)(sx - 120 + 16) <= 32) { a->phase = D_WAIT; a->timer = hound_wait[(rnd() + (a - actors)) & 3]; }
        if (sx > 400) { hound_quota = 1; actor_free(a); }
        break;
    case D_WAIT:
        if (--a->timer) break;
        a->phase = D_LEAP;
        a->vy = 1024;
        actor_anim(a, 2);
        break;
    case D_LEAP:
    {
        s16 top;
        a->x += 256 + wagon_step;
        a->y += a->vy;
        if (a->vy > -1536) a->vy -= 48;
        if (a->vy < 0) actor_anim(a, 4);
        if (a->vy < 0 && surface_floor(AX(a), AY(a), &top)) { a->y = (s32)top << 8; a->phase = D_RUN; actor_anim(a, 0); }
        else if (a->vy < 0 && AY(a) <= ROAD_Y)
        {
            a->y = (s32)ROAD_Y << 8;
            a->phase = D_ROAD;
            a->timer = hound_road[(rnd() & 14) >> 1];
            actor_anim(a, 6);
        }
        break;
    }
    default:
        if (--a->timer) break;
        a->phase = D_RUN;
        actor_anim(a, 0);
        break;
    }
    if (a->phase == D_RUN && AY(a) > ROAD_Y)
    {
        s16 top;
        a->x += wagon_step;
        if (!surface_floor(AX(a), AY(a) - 1, &top)) { a->phase = D_LEAP; a->vy = 0; }     /* ran off the deck */
    }
}
