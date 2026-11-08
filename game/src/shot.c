/* Willow's normal magic shot (specs/player.md 3.5; arcade routine EC84 as
 * the specification). Up to 4 alive; variant 0-6 = the spell cast (the
 * magic level for a tap, more for a charged release, specs/magic.md 1).
 *
 * As the arcade: the spawn update only sets the shot up; each later update
 * counts its 64-update life down, tests the screen bounds every 4th update,
 * moves, animates and tests for a wall. Running out of life or off the
 * screen removes it after that update; a wall starts the 12-update impact
 * animation (table 0xEFB8) in place. It counts toward the 4 until removed.
 *
 * Poses are drawn through the pose cache (src/sprite.c); left-facing poses
 * are the right-facing tiles flipped (tools/posepack.py MIRROR=1). */
#include <genesis.h>
#include "stagedata.h"
#include "shot.h"
#include "sound.h"
#include "vram.h"
#include "actor.h"
#include "sprite.h"
#include "../data/shot/layout.h"
#include "s1spr.h"
#include "s2spr.h"
#include "player.h"

extern const u8 shot_poses[];
extern const u8 shot_tiles[];
extern const u8 shot_anim[];

#define MAX_SHOTS 4
#define LIFETIME 64
#define IMPACT_TIME 12
#define CHAIN_IMPACT 7      /* chains 14/15: wall impact (0xEFB8); 16/17: hit (0xEFB4, M2) */

enum { S_FREE, S_RUN, S_SPAWN, S_REMOVE, S_IMPACT };

typedef struct
{
    s32 x, y;
    s16 vx;
    u16 alive, timer, facing, frame, tick, variant, shown;
} Shot;

static Shot shots[MAX_SHOTS];
/* The union of the live shot, spear and sword boxes as the last shot_update
 * left them (shot_bounds): shot_test rejects a box outside it at once
 * (exact: shots only end later in the update). A new shot, spear or swing
 * drops it until the next shot_update. */
s16 shot_su_c[4];
u8 shot_su_ok;
#define su_c shot_su_c
#define su_ok shot_su_ok
static u8 live_shots;       /* shots + spears alive (shot_test's early exit; counted in shot_update) */

/* The companion's spears (specs/stage1_shops_companion.md 2.5): player shots
 * of their own (not counted in Willow's 4), power 1, box (8,16,4,8); 20
 * updates straight at 4.5 px/f, then falling (+32 a frame up to 7 px/f)
 * until the floor removes them; off-camera checks every 4th update; a hit
 * plays the shots' impacts. */
#define MAX_SPEARS 4
typedef struct { s32 x, y; s16 vx, vy; u16 alive, timer, facing, frame, tick, variant, shown; } Spear;
static Spear spears[MAX_SPEARS];
static const s16 spear_box[4] = { 8, 16, 4, 8 };
static u8 spear_hint[SPEAR_POSES];
static PoseSet ps_spear = { spear_poses, spear_tiles, spear_anim, SPEAR_POSES, 0, 0, NULL, spear_hint, 0, spear_sat, 0 };
/* Madmartigan's sword (specs/madmartigan.md 5; actor 0xE740): spawned
 * invisible, after 4 updates shown and hitting at the player + the posture's
 * offset (fixed at spawn), image 0 for 4 updates, image 1 for 4, then for a
 * charged swing tier x 4 updates moving out 4 px a frame (y frozen). It is
 * not used up by a hit (one target per update) and cuts arrows.
 * Its colour steps during the extension (5.5) are not shown: the pieces
 * share fixed lines (approved cut 4). */
enum { SW_FREE, SW_WAIT, SW_SWING, SW_EXTEND };
static struct { s16 x, y, ext; u8 state, timer, facing, posture, tier, used; u16 shown; } sword;
u8 sword_level;
u8 shot_hit_sword;
static const s8 sword_dy[3] = { 16, 0, 24 };                  /* 0xE8E4 / E8F4 / E904 */
static const s16 sword_box[5][2][4] = {                       /* 0xE78C: (xOff, w, yOff, h) right / left */
    { { 0, 40, 16, 40 }, { 40, 40, 16, 40 } }, { { 0, 40, 16, 40 }, { 40, 40, 16, 40 } },
    { { 8, 48, 16, 40 }, { 40, 48, 16, 40 } }, { { 8, 48, 16, 40 }, { 40, 48, 16, 40 } },
    { { 28, 68, 16, 40 }, { 40, 68, 16, 40 } }
};
static const u8 sword_power[5][4] = {                         /* 0xE7DC: per level, tier */
    { 1, 2, 3, 4 }, { 2, 4, 6, 8 }, { 3, 5, 7, 9 }, { 6, 8, 12, 14 }, { 12, 14, 16, 18 }
};
static const u8 sword_art[5] = { 0, 0, 4, 4, 8 };             /* image set by level (0xEB10) */
static u8 sword_hint[S2_SWORD_POSES];
static PoseSet ps_sword = { s2_sword_poses, s2_sword_tiles, s2_sword_anim, S2_SWORD_POSES, 0, 0, NULL, sword_hint, 0, s2_sword_sat, 0 };

static const u8 *chain[9][2];       /* animation chain per variant (7: impact, 8: hit) and facing */
static u8 shot_hint[SHOT_COUNT];
extern const s16 shot_sat[];
static PoseSet ps_shot = { shot_poses, shot_tiles, shot_anim, SHOT_COUNT, 0, 0, NULL, shot_hint, 0, shot_sat, 0 };

static const u8 *chain_of(u16 v, u16 facing)
{
    const u8 *a = shot_anim;
    u16 i = v * 2 + facing;
    while (i--) a += 2 + 2 * a[0];
    return a;
}

void shot_init(u16 level)
{
    u16 v, f;
    (void)level;
    memset(shots, 0, sizeof shots);
    memset(spears, 0, sizeof spears);
    memset(&sword, 0, sizeof sword);
    for (v = 0; v < 9; v++)
        for (f = 0; f < 2; f++)
            chain[v][f] = chain_of(v, f);
}

static u16 wall_hit(const Shot *s)
{
    u8 flags = sd->coldesc[terrain_id(s->x >> 8, s->y >> 8) * 8];
    return s->facing ? (flags & 2) != 0 : (flags & 1) != 0;
}

u16 shot_fire(s16 x, s16 y, u16 facing, u16 variant)
{
    u16 i;
    for (i = 0; i < MAX_SHOTS; i++)
        if (!shots[i].alive)
        {
            Shot *s = shots + i;
            su_ok = 0;
            s->alive = S_SPAWN;
            live_shots++;
            s->x = (s32)x << 8;
            s->y = (s32)y << 8;
            s->facing = facing;
            s->vx = facing ? -(1024 + 64 * variant) : 1024 + 64 * variant;
            s->timer = LIFETIME;
            s->frame = 0;
            s->variant = variant;
            s->tick = chain[variant][facing][2];
            sound_post(SND_SHOT);
            return 1;
        }
    return 0;           /* a fifth shot does nothing */
}

static void animate_ft(u16 *frame, u16 *tick, const u8 *c)
{
    if (c[2 + 2 * *frame] == 0xFF || --*tick) return;
    if (++*frame >= c[0]) *frame = c[1 + 2 * c[0]] == 0xFF ? c[0] - 1 : c[1 + 2 * c[0]];
    *tick = c[2 + 2 * *frame];
}

static void animate(Shot *s, const u8 *c) { animate_ft(&s->frame, &s->tick, c); }

u16 shot_spear(s16 x, s16 y, u16 facing)
{
    u16 i;
    for (i = 0; i < MAX_SPEARS; i++)
        if (!spears[i].alive)
        {
            Spear *s = spears + i;
            su_ok = 0;
            s->alive = S_SPAWN;
            live_shots++;
            s->x = (s32)x << 8;
            s->y = (s32)y << 8;
            s->facing = facing;
            s->vx = facing ? -1152 : 1152;
            s->vy = 0;
            s->timer = 20;
            s->variant = 0;
            return 1;
        }
    return 0;
}

u16 shot_sword(u16 facing, u16 posture, u16 tier)
{
    if (sword.state) return 0;              /* FF83CB: one sword at a time */
    su_ok = 0;
    sword.state = SW_WAIT;
    sword.timer = 5;
    sword.facing = facing;
    sword.posture = posture;
    sword.tier = tier;
    sword.ext = 0;
    return 1;
}

static void sword_place(void)
{
    extern const Player *coop_mad(void);
    const Player *o = two_players ? coop_mad() : &player;  /* co-op: Madmartigan may be the second player */
    s16 dx = 48 + (sword.ext >> 8);
    sword.x = (o->x >> 8) + (sword.facing ? -dx : dx);
    if (sword.state != SW_EXTEND) sword.y = (o->y >> 8) + sword_dy[sword.posture];
}

static void sword_update(void)
{
    sword.used = 0;
    switch (sword.state)
    {
    case SW_WAIT:
        if (--sword.timer) return;
        sword.state = SW_SWING;
        sword.timer = 8;
        sound_post(SND_SWORD);
        break;
    case SW_SWING:
        if (--sword.timer) break;
        if (!sword.tier) { sword.state = SW_FREE; return; }
        sword.state = SW_EXTEND;
        sword.timer = sword.tier * 4;
        /* fall through */
    case SW_EXTEND:
        if (!sword.timer) { sword.state = SW_FREE; return; }
        sword.timer--;
        sword.ext += 1024;
        break;
    }
    sword_place();
    if (sword.state == SW_EXTEND) { extern void magic_sword_spark(s16 x, s16 y, bool left); magic_sword_spark(sword.x, sword.y, sword.facing & 1); }   /* 5.3: a spark every update */
}

static bool sword_hits(s16 x, s16 y, const s16 *b)
{
    return sword.state >= SW_SWING && box_hit(sword.x, sword.y, sword_box[sword_level][sword.facing], x, y, b);
}

bool shot_sword_cuts(s16 x, s16 y, const s16 *b)
{
    return sword_hits(x, y, b);
}

static void spears_update(s16 cam_x, u16 frame)
{
    u16 i;
    for (i = 0; i < MAX_SPEARS; i++)
    {
        Spear *s = spears + i;
        s16 top;
        switch (s->alive)
        {
        case S_SPAWN:
            s->alive = S_RUN;
            break;
        case S_RUN:
            if (!(frame & 3))
            {
                s16 sx = (s->x >> 8) - cam_x;
                if (sx < -32 || sx > 384 + 32) { s->alive = S_REMOVE; break; }
            }
            s->x += s->vx;
            if (s->timer) s->timer--;
            else
            {
                if (s->vy < 1792) s->vy += 32;
                s->y -= s->vy;
                if (terrain_floor(s->x >> 8, s->y >> 8, &top)) s->alive = S_REMOVE;
            }
            break;
        case S_IMPACT:
            if (!--s->timer) s->alive = S_REMOVE;
            animate_ft(&s->frame, &s->tick, chain[s->variant][s->facing]);
            break;
        case S_REMOVE:
            s->alive = S_FREE;
            break;
        }
    }
}

void shot_update(s16 cam_x, u16 frame)
{
    u16 i;
    for (i = 0; i < MAX_SHOTS; i++)
    {
        Shot *s = shots + i;
        switch (s->alive)
        {
        case S_SPAWN:
            s->alive = S_RUN;
            break;
        case S_RUN:
            if (!--s->timer) s->alive = S_REMOVE;
            if (!(frame & 3))
            {
                s16 sx = (s->x >> 8) - cam_x;
                if (sx < -32 || sx > 384 + 32) s->alive = S_REMOVE;
            }
            s->x += s->vx;
            animate(s, chain[s->variant][s->facing]);
            if (wall_hit(s))
            {
                s->alive = S_IMPACT;
                s->variant = CHAIN_IMPACT;
                s->timer = IMPACT_TIME;
                s->frame = 0;
                s->tick = chain[CHAIN_IMPACT][s->facing][2];
            }
            break;
        case S_IMPACT:
            if (!--s->timer) s->alive = S_REMOVE;
            animate(s, chain[s->variant][s->facing]);
            break;
        case S_REMOVE:
            s->alive = S_FREE;
            break;
        }
    }
    spears_update(cam_x, frame);
    sword_update();
    {
        u16 n = 0;
        for (i = 0; i < MAX_SHOTS; i++) n += shots[i].alive == S_RUN || shots[i].alive == S_SPAWN;
        for (i = 0; i < MAX_SPEARS; i++) n += spears[i].alive == S_RUN || spears[i].alive == S_SPAWN;
        live_shots = n;
    }
    shot_bounds(su_c);
    su_ok = 1;
}

/* Shot boxes per variant (player.md 3.5, ROM 0xED44). */
static const s16 shot_box[7][4] = {
    { 4, 8, 3, 6 }, { 5, 10, 4, 8 }, { 8, 16, 6, 12 }, { 10, 20, 8, 16 },
    { 10, 20, 9, 18 }, { 12, 24, 12, 24 }, { 12, 24, 14, 28 }
};

/* A live shot (not yet in its impact) touching the box (x, y, b): it ends
 * (hit: impact animation 0xEFB4; deflect: 0xEFB8) and its power (variant +
 * 1) is returned; 0 if none touches. One shot hits one target. */
/* A live shot within (dx, dy) of (x, y): the bomb trooper's dodge test
 * (stage3.md 5.1, the player-shot pool scan). */
bool shot_near(s16 x, s16 y, s16 dx, s16 dy)
{
    u16 i;
    for (i = 0; i < MAX_SHOTS; i++)
        if (shots[i].alive == S_RUN && (u16)((shots[i].x >> 8) - x + dx) <= 2 * dx && (u16)((shots[i].y >> 8) - y + dy) <= 2 * dy) return TRUE;
    return FALSE;
}

/* Sorsha's parry (stage4.md 6.8): a swing is out. */
bool shot_sword_live(void) { return sword.state >= SW_SWING; }

/* Stage 4's colouring of Madmartigan's sword (tools/pack_stage4.sh). */
void sword_art_set(const u8 *poses, const u8 *tiles, const s16 *sat)
{
    ps_sword.poses = poses; ps_sword.tiles = tiles; ps_sword.sat = sat;
}

/* Stage 3's colouring of Willow's shots (tools/pack_stage3.sh: line 1 differs). */
void shot_art(const u8 *poses, const u8 *tiles, const s16 *sat)
{
    ps_shot.poses = poses; ps_shot.tiles = tiles; ps_shot.sat = sat;   /* cached art: the stage change flushes the cache */
}


/* Grow the union u (x lo, x hi, y lo, y hi) by box b at (x, y), in box_hit's
 * terms: the box spans [x + b[0] - b[1], x + b[0]] across, likewise down. */
static void bounds_add(s16 *u, s16 x, s16 y, const s16 *b)
{
    s16 x1 = x + b[0], y1 = y + b[2];
    if (x1 - b[1] < u[0]) u[0] = x1 - b[1];
    if (x1 > u[1]) u[1] = x1;
    if (y1 - b[3] < u[2]) u[2] = y1 - b[3];
    if (y1 > u[3]) u[3] = y1;
}

/* The union of everything shot_test can hit with this update (the sword
 * while swinging, shots and spears in flight); FALSE when nothing. */
#ifdef SHOTBOUND_CHECK
/* Measurement: would anything shot_test looks at hit (x, y, b)? No effects. */
bool shot_would_hit(s16 x, s16 y, const s16 *b)
{
    u16 i;
    if (!live_shots && sword.state < SW_SWING) return FALSE;
    if (!sword.used && sword_hits(x, y, b)) return TRUE;
    for (i = 0; i < MAX_SHOTS; i++)
        if ((shots[i].alive == S_RUN || shots[i].alive == S_SPAWN) && box_hit(shots[i].x >> 8, shots[i].y >> 8, shot_box[shots[i].variant], x, y, b)) return TRUE;
    for (i = 0; i < MAX_SPEARS; i++)
        if ((spears[i].alive == S_RUN || spears[i].alive == S_SPAWN) && box_hit(spears[i].x >> 8, spears[i].y >> 8, spear_box, x, y, b)) return TRUE;
    return FALSE;
}
#endif

/* Stage 5's boss's push wall (stage5.md 6.6, 0x2C6CA): Willow's shots in
 * flight past x + line[(y >> 5) & 7] end (the wall impact). */
void shot_end_beyond(s16 x, const s16 *line)
{
    u16 i;
    for (i = 0; i < MAX_SHOTS; i++)
    {
        Shot *s = shots + i;
        if (s->alive != S_RUN && s->alive != S_SPAWN) continue;
        if ((s->x >> 8) <= x + line[((s->y >> 8) >> 5) & 7]) continue;
        s->alive = S_IMPACT;
        s->variant = CHAIN_IMPACT;
        s->timer = IMPACT_TIME;
        s->frame = 0;
        s->tick = chain[CHAIN_IMPACT][s->facing][2];
    }
}

bool shot_bounds(s16 *u)
{
    u16 i;
    bool any = FALSE;
    u[0] = u[2] = 0x7FFF; u[1] = u[3] = -0x8000;
    if (!live_shots && sword.state < SW_SWING) return FALSE;
    if (sword.state >= SW_SWING) { bounds_add(u, sword.x, sword.y, sword_box[sword_level][sword.facing]); any = TRUE; }
    for (i = 0; i < MAX_SHOTS; i++)
        if (shots[i].alive == S_RUN || shots[i].alive == S_SPAWN) { bounds_add(u, shots[i].x >> 8, shots[i].y >> 8, shot_box[shots[i].variant]); any = TRUE; }
    for (i = 0; i < MAX_SPEARS; i++)
        if (spears[i].alive == S_RUN || spears[i].alive == S_SPAWN) { bounds_add(u, spears[i].x >> 8, spears[i].y >> 8, spear_box); any = TRUE; }
    return any;
}

u16 shot_test_full(s16 x, s16 y, const s16 *b, bool deflect)
{
    u16 i;
    if (!live_shots && sword.state < SW_SWING) return 0;   /* nothing in flight */
    if (su_ok)
    {
        s16 ex = x - b[0], ey = y - b[2];
        if (ex > su_c[1] || ex + b[1] < su_c[0] || ey > su_c[3] || ey + b[3] < su_c[2]) return 0;
    }
    if (!sword.used && sword_hits(x, y, b))
    {
        shot_hit_sword = 1;
        sword.used = 1;                     /* not used up; one target per pass (5.6) */
        return sword_power[sword_level][sword.tier];
    }
    for (i = 0; i < MAX_SHOTS; i++)
    {
        Shot *s = shots + i;
        if (s->alive != S_RUN && s->alive != S_SPAWN) continue;
        if (!box_hit(s->x >> 8, s->y >> 8, shot_box[s->variant], x, y, b)) continue;
        {
            u16 power = s->variant + 1;
            shot_hit_sword = 0;
            s->alive = S_IMPACT;
            s->timer = deflect ? IMPACT_TIME : 15;
            s->variant = deflect ? CHAIN_IMPACT : CHAIN_IMPACT + 1;
            s->frame = 0;
            s->tick = chain[s->variant][s->facing][2];
            return power;
        }
    }
    for (i = 0; i < MAX_SPEARS; i++)
    {
        Spear *s = spears + i;
        if (s->alive != S_RUN && s->alive != S_SPAWN) continue;
        if (!box_hit(s->x >> 8, s->y >> 8, spear_box, x, y, b)) continue;
        shot_hit_sword = 2;
        s->alive = S_IMPACT;
        s->timer = deflect ? IMPACT_TIME : 15;
        s->variant = deflect ? CHAIN_IMPACT : CHAIN_IMPACT + 1;
        s->frame = 0;
        s->tick = chain[s->variant][s->facing][2];
        return 1;
    }
    return 0;
}

u16 shot_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y)
{
    u16 i, n = 0;
    for (i = 0; i < MAX_SHOTS; i++)
    {
        Shot *s = shots + i;
        if (!s->alive) continue;
        n += pose_draw(&ps_shot, chain[s->variant][s->facing][1 + 2 * s->frame], &s->shown, sat, first + n,
                       (s->x >> 8) - view_x, (511 - (s->y >> 8)) - view_y, FALSE, NULL);
    }
    if (sword.state >= SW_SWING)
        n += pose_draw(&ps_sword, sword_art[sword_level] + (sword.state != SW_SWING || sword.timer <= 4) * 2 + sword.facing,
                       &sword.shown, sat, first + n, sword.x - view_x, (511 - sword.y) - view_y, FALSE, NULL);
    for (i = 0; i < MAX_SPEARS; i++)
    {
        Spear *s = spears + i;
        s16 sx = (s->x >> 8) - view_x, sy = (511 - (s->y >> 8)) - view_y;
        if (!s->alive) continue;
        if (s->alive == S_IMPACT)
            n += pose_draw(&ps_shot, chain[s->variant][s->facing][1 + 2 * s->frame], &s->shown, sat, first + n, sx, sy, FALSE, NULL);
        else
            n += pose_draw(&ps_spear, s->facing, &s->shown, sat, first + n, sx, sy, FALSE, NULL);
    }
    return n;
}
