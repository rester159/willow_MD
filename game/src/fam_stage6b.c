/* Stage 6's scenery families (specs/stage6_families.md 2, 3): the flipping
 * stepping stones over area 1's spikes and area 2's spike crushers. Native
 * coordinates of their areas (area 1: y = arcade y - 768; area 2: - 1536). */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"
#include "stagedata.h"
#include "hud.h"

#define FACE(a) ((a)->facing & 1)
extern u8 npatch, patch_id[16];
extern s16 patch_c[16], patch_r[16];

/* ---- Stones (2). Chains: 0 tilting (0x31FC2), 1 flat (0x31FCE), 2 tilting
 * (0x31FDA), 3 edge-on (0x31FE6). Phase 0 leader waiting, 2 follower
 * waiting, 4 flat cycle, 6 edge-on cycle; v[0] its patch slot + 1, v[1]
 * group. While flat, two cells (x - 8, y + 8) and (x + 8, y + 8) are id 37. */
enum { ST_TILT_A, ST_FLAT, ST_TILT_B, ST_EDGE };
static const u16 stone_first[14] = { 120, 100, 200, 300, 400, 500, 120, 100, 200, 300, 400, 500, 600, 700 };   /* 0x31DA8 */
static u8 stone_group[2];          /* FF0F0C / FF0F0D: the groups' trigger counts */

static void stone_cells(Actor *a, bool on)
{
    u16 k = a->v[0];
    if (!k)
    {
        if (npatch > 14) return;
        k = a->v[0] = npatch + 1;
        npatch += 2;
        terrain_changed();
        patch_c[k - 1] = (AX(a) - 8) >> 4; patch_c[k] = (AX(a) + 8) >> 4;
        patch_r[k - 1] = patch_r[k] = (s16)(sd->world_top - AY(a) - 8) >> 4;
        {
            extern s16 patch_c0, patch_c1, patch_r0, patch_r1;
            if (patch_c[k - 1] < patch_c0) patch_c0 = patch_c[k - 1];
            if (patch_c[k] > patch_c1) patch_c1 = patch_c[k];
            if (patch_r[k] < patch_r0) patch_r0 = patch_r[k];
            if (patch_r[k] > patch_r1) patch_r1 = patch_r[k];
        }
    }
    if (patch_id[k] != (on ? 37 : 0)) terrain_changed();
    patch_id[k - 1] = patch_id[k] = on ? 37 : 0;
}

void stone6_update(Actor *a)
{
    switch (a->state)
    {
    case 0:
        a->state = 2; a->active = 2;
        a->v[1] = a->variant >= 6;
        if (!a->facing) { stone_group[a->v[1]] = 0; a->phase = 0; actor_anim(a, ST_FLAT); stone_cells(a, TRUE); }
        else { a->phase = 2; actor_anim(a, ST_EDGE); stone_cells(a, FALSE); }
        return;
    }
    switch (a->phase)
    {
    case 0:                                 /* the leader: the player within 16 px in x */
        {
            s16 dx = (s16)(player.x >> 8) - AX(a);
            if (dx <= -16 || dx >= 16 || (u16)(AX(a) - CAM_X + 32) > 448) return;
        }
        stone_group[a->v[1]]++;
        a->phase = 4; a->sub = 4; a->timer = stone_first[a->variant];
        return;
    case 2:                                 /* a follower: its group started */
        if (!stone_group[a->v[1]]) return;
        a->phase = 6; a->sub = 4; a->timer = stone_first[a->variant];
        return;
    }
    if (--a->timer) return;
    if (a->phase == 4)
        switch (a->sub)
        {
        case 0: stone_cells(a, TRUE); a->sub = 2; a->timer = 8; actor_anim(a, ST_TILT_A); return;
        case 2: a->sub = 4; a->timer = 120; actor_anim(a, ST_FLAT); return;
        case 4: a->sub = 6; a->timer = 8; actor_anim(a, ST_TILT_A); return;
        default: a->phase = 6; a->sub = 0; a->timer = 1; return;
        }
    switch (a->sub)
    {
    case 0: stone_cells(a, FALSE); a->sub = 2; a->timer = 8; actor_anim(a, ST_TILT_B); return;
    case 2: a->sub = 4; a->timer = 180; actor_anim(a, ST_EDGE); return;
    case 4: a->sub = 6; a->timer = 8; actor_anim(a, ST_TILT_B); return;
    default: a->phase = 4; a->sub = 0; a->timer = 1; return;
    }
}

/* ---- Spike crushers (3). Wait (by variant), fall (the speed never reset:
 * only the first drop accelerates), 60 on the floor (sound 0xA8), rise 4
 * px/update to the rest y. Solid, standable on top (y + 19), hurt within 32
 * px from the side; shots deflect. v[0] rest y, v[1] fall speed. */
static const u8 crush_wait[3][16] = {                                       /* 0x320EA */
    { 64, 128, 128, 64, 96, 64, 128, 96, 96, 96, 64, 128, 128, 128, 128, 128 },
    { 64, 32, 96, 64, 96, 96, 32, 96, 64, 96, 96, 96, 32, 96, 64, 32 },
    { 16, 64, 64, 32, 64, 64, 16, 32, 64, 32, 64, 16, 64, 32, 64, 16 },
};

void crusher6_update(Actor *a)
{
    s16 dx, dy, top;
    if (!a->state)
    {
        a->state = 2; a->active = 1; a->v[0] = AY(a); a->v[1] = 0;   /* facing: the dir byte (2: the long chain) */
        if (a->facing == 2)                 /* dir 2: an archer rides it (stage6_families.md 11) */
        {
            Actor *r = actor_new(F_RARCH6);
            if (r) { r->variant = 8; r->facing = 1; r->x = a->x; r->y = a->y + (43 << 8); r->link = (u8)(a - actors) + 1; }
        }
        a->phase = 0; a->timer = crush_wait[a->variant < 3 ? a->variant : 0][rnd() & 15];
        actor_anim(a, 0);
        return;
    }
    /* Only near the screen (the 0x321F6 window). */
    if ((u16)(AX(a) - CAM_X + 64) > 512 || (u16)(AY(a) - CAM_Y + 32) > 320) return;
    switch (a->phase)
    {
    case 0:                                 /* waiting at the top */
        if (--a->timer) break;
        a->phase = 1;
        break;
    case 1:                                 /* falling */
        if (a->v[1] < 1792) a->v[1] += 128;
        a->y -= a->v[1];
        if (terrain_floor(AX(a), AY(a) - 19, &top))
        {
            a->y = (s32)(top + 19) << 8;
            if ((u16)(AX(a) - CAM_X + 32) <= 448) sound_post(0xA8);
            a->phase = 2; a->timer = 60;
        }
        break;
    case 2:
        if (!--a->timer) a->phase = 3;
        break;
    case 3:                                 /* rising */
        a->y += 1024;
        if (AY(a) >= a->v[0]) { a->y = (s32)a->v[0] << 8; a->phase = 0; a->timer = crush_wait[a->variant < 3 ? a->variant : 0][rnd() & 15]; }
        break;
    }
    /* The player: on top it stands (the surface pass, C_SURFACE); from the side it hurts. */
    dx = (s16)(player.x >> 8) - AX(a);
    dy = (s16)(player.y >> 8) - AY(a);
    if (dx > -32 && dx < 32 && dy > -19 - 48 && dy < 19) player_hurt();
}

/* ---- Common to the tower's mortars and bomb troopers: 15 updates immune
 * after a hit (sound 0x32); death: sound 0x33, a burst, 500 points, a drop
 * from 0x22D4E (2,1,2,3,2,4,2,2); FF83D0 (encounter_clear): the burst only. */
static const u8 tower_drop[8] = { 2, 1, 2, 3, 2, 4, 2, 2 };
static bool tower_hit(Actor *a)
{
    if (a->v[5] && !--a->v[5]) a->active = 1;
    if (encounter_clear) { effect(F_BURST, AX(a), AY(a), 5); actor_free(a); return TRUE; }
    if (a->hp == a->last_hp) return FALSE;
    a->last_hp = a->hp;
    if (a->hp) { sound_post(0x32); a->active = 2; a->v[5] = 15; return FALSE; }
    sound_post(0x33);
    effect(F_BURST, AX(a), AY(a), 5);
    award(6);
    drop(a, tower_drop[rnd() & 7]);
    actor_free(a);
    return TRUE;
}

static bool in_window(Actor *a) { return (u16)(AX(a) - CAM_X + 64) <= 512 && (u16)(AY(a) - CAM_Y + 32) <= 320; }

static void bomb_at(Actor *a, s16 dx, s16 dy, u16 variant)
{
    Actor *b = actor_new(F_BOMB);
    if (!b) return;
    b->variant = variant; b->facing = a->facing & 1;
    b->x = a->x + ((s32)dx << 8); b->y = a->y + ((s32)dy << 8);
}

/* ---- Mortar (4): bobs 40 px (1 px an update) while near the screen;
 * within 128 px of the player, after 48..128 updates, drops a bomb (fire
 * pose 42 updates, the bomb at 16 left). Chains 0-1 idle by side, 2-3
 * fire, 4 its static part (12/37). v[0] side, v[1] countdown, v[2] up. */
static const u8 mortar_wait[16] = { 48, 64, 96, 64, 48, 96, 128, 64, 48, 64, 96, 64, 64, 96, 64, 96 };

void mpart6_update(Actor *a) { if (!a->state) { a->state = 2; a->active = 2; actor_anim(a, 4); } }

void mortar6_update(Actor *a)
{
    if (!a->state)
    {
        Actor *p;
        a->state = 2; a->active = 1; a->hp = a->last_hp = 24;
        a->v[0] = (s16)(player.x >> 8) < AX(a); a->facing = a->v[0];
        a->timer = 40; a->v[2] = 0; a->v[1] = 0; a->phase = 0;
        actor_anim(a, a->v[0]);
        p = actor_new(F_MPART6);
        if (p) { p->x = a->x + ((s32)(a->v[0] ? -4 : 4) << 8); p->y = a->y; }
        return;
    }
    if (tower_hit(a)) return;
    if (!(game_frame & 7))
    {
        if (!a->phase && in_window(a)) a->phase = 2;
        else if (a->phase && !in_window(a)) { a->phase = 0; a->v[1] = 0; actor_anim(a, a->v[0]); }
    }
    if (!a->phase) return;
    actor_step(a);
    if (a->phase == 4)
    {
        if (a->t2 == 16) bomb_at(a, a->v[0] ? 16 : -16, 16, 0);
        if (!--a->t2) { a->phase = 2; actor_anim(a, a->v[0]); }
        return;
    }
    a->y += a->v[2] ? 256 : -256;
    if (!--a->timer) { a->timer = 40; a->v[2] ^= 1; }
    {
        s16 dx = (s16)(player.x >> 8) - AX(a);
        if (!a->v[1] && dx >= -128 && dx <= 128) a->v[1] = mortar_wait[rnd() & 15];
    }
    if (a->v[1] && !--a->v[1]) { a->phase = 4; a->t2 = 42; actor_anim(a, 2 + a->v[0]); }
}

/* ---- Tower bomb trooper (5): the ground behaviour (walk to its distance,
 * throw lobbed bombs) for 60..240 updates, then for good: to the centre
 * column x 512, up and down the ladder (1 px an update, 30..120 at a
 * time, between its bounds), throwing from it. Chains 0-1 stand, 2-3
 * ground throw, 4-5 run, 6-7 climb, 8-9 ladder throw. v[0] ground time,
 * v[1] W, v[2] ladder top, v[3] bottom, v[4] mode; t2 throw timer. */
static const s16 ladder6[3][2] = { { 1488, 1344 }, { 1744, 1600 }, { 2000, 1856 } };
static const u16 ground6[4] = { 60, 120, 180, 240 };
static const u16 near6[4] = { 160, 176, 144, 168 };
static const u8 rest6[4] = { 40, 20, 60, 10 };
static const u8 climb6[4] = { 30, 60, 90, 120 };

static void tchain(Actor *a, u16 k) { actor_anim(a, k + (a->facing & 1)); }

void trooper6_update(Actor *a)
{
    s16 dx = (s16)(player.x >> 8) - AX(a), top;
    if (!a->state)
    {
        a->active = 0;
        if ((u16)(AX(a) - CAM_X + 32) > 448 || (u16)(AY(a) - CAM_Y) > 272) return;
        a->state = 2; a->active = 1; a->hp = a->last_hp = 24;
        a->v[0] = ground6[rnd() & 3]; a->v[1] = near6[rnd() & 3];
        a->v[2] = ladder6[a->variant < 3 ? a->variant : 0][0]; a->v[3] = ladder6[a->variant < 3 ? a->variant : 0][1];
        a->v[4] = 0; a->phase = 0; a->timer = 1;
        tchain(a, 4);
        return;
    }
    if (tower_hit(a)) return;
    actor_step(a);
    if (!a->v[4])                           /* the ground */
    {
        if (!--a->v[0]) { a->v[4] = 1; a->phase = 0; tchain(a, 4); return; }
        if (a->phase == 2)                  /* throwing */
        {
            if (a->t2 == 16) bomb_at(a, a->facing ? -16 : 16, 8, 5 + (rnd() % 3));
            if (!--a->t2) { a->phase = 3; a->timer = rest6[rnd() & 3]; tchain(a, 0); }
            return;
        }
        if (a->phase == 3) { if (!--a->timer) a->phase = 0; return; }
        if (dx > a->v[1] || dx < -a->v[1]) a->facing = dx < 0;
        else if (dx <= 96 && dx >= -96) a->facing = dx >= 0;
        else { a->facing = dx < 0; a->phase = 2; a->t2 = 48; tchain(a, 2); return; }
        if (!terrain_floor(AX(a) + (a->facing ? -12 : 12), AY(a) - 8, &top))
        {
            a->facing ^= 1; a->phase = 2; a->t2 = 48; tchain(a, 2); return;
        }
        a->x += a->facing ? -512 : 512;
        actor_ground(a, 0, -8);
        return;
    }
    switch (a->phase)
    {
    case 0:                                 /* to the ladder's column */
        a->facing = AX(a) > 512;
        a->x += a->facing ? -512 : 512;
        if (AX(a) - 512 <= 48 && 512 - AX(a) <= 48) { a->phase = 1; a->timer = climb6[rnd() & 3]; tchain(a, 6); }
        return;
    case 1:                                 /* climbing */
        a->y += (s16)(player.y >> 8) >= AY(a) ? 256 : -256;
        if (AY(a) > a->v[2]) a->y = (s32)a->v[2] << 8;
        if (AY(a) < a->v[3]) a->y = (s32)a->v[3] << 8;
        if (--a->timer) return;
        a->facing = dx < 0; a->phase = 2; a->timer = 20; tchain(a, 8);
        return;
    default:                                /* throwing from the ladder */
        if (a->timer == 8) bomb_at(a, a->facing ? -16 : 16, 8, 5);
        if (!--a->timer) { a->phase = 1; a->timer = climb6[rnd() & 3]; tchain(a, 6); }
        return;
    }
}

/* ---- The crushers' chains (class 12 kind 36, stage6_families.md 3): an
 * anchor 16 above the rest position (dir 2: the long one) and a 32-px link
 * for every 32 px the crusher has dropped; drawn here (not actors: area 2
 * has ten crushers). Art: chand chains 0 anchor, 1 link, 2 long anchor. */
#define CHAIN_VRAM 900            /* after the torch frames (free in every stage-6 area) */
u8 chain6_loaded;                 /* cleared with the planes (src/stage.c) */

/* Per chain pose: its sprite entries resolved once (y, size, attribute, x). */
static s16 chain_e[3][6][4];
static u8 chain_n[3];

static void chain_prepare(const PoseSet *ps)
{
    u16 p, i;
    for (p = 0; p < 3; p++)
    {
        const u8 *d = ps->poses + p * 6;
        const s16 *q = ps->sat + (((d[4] << 8) | d[5]) << 2);
        u16 base = CHAIN_VRAM + ((d[0] << 8) | d[1]);
        chain_n[p] = d[3] < 6 ? d[3] : 6;
        for (i = 0; i < chain_n[p]; i++, q += 4)
        {
            chain_e[p][i][0] = q[0] + 128; chain_e[p][i][1] = q[1];
            chain_e[p][i][2] = q[2] + base; chain_e[p][i][3] = q[3] + 128;
        }
    }
}

static inline u16 chain_put(u16 pose, VDPSprite *sat, u16 k, u16 max, s16 sx, s16 sy)
{
    const s16 (*e)[4] = chain_e[pose];
    u16 n = chain_n[pose];
    for (; n && k < max; n--, e++)
    {
        VDPSprite *o = sat + k++;
        o->y = sy + (*e)[0];
        o->size_link = (*e)[1] | k;
        o->attribut = (*e)[2];
        o->x = sx + (*e)[3];
    }
    return k;
}

u16 chains6_draw(VDPSprite *sat, u16 first, u16 max, s16 view_x, s16 view_y)
{
    extern const PoseSet *chain6_poses(void);
    const PoseSet *ps = chain6_poses();
    Actor *a = actors + pool_lo[C_SURFACE], *end = actors + pool_hi[C_SURFACE];
    u16 k = first;
    for (; a < end; a++)
    {
        s16 sx, y;
        if (a->fam != F_CRUSH6) continue;
        sx = AX(a) - view_x;
        if ((u16)(sx + 32) > 384) continue;
        if (!chain6_loaded)
        {
            const u8 *d = ps->poses + (ps->count - 1) * 6;
            VDP_loadTileData((const u32 *)ps->tiles, CHAIN_VRAM, ((d[0] << 8) | d[1]) + d[2], DMA);
            chain_prepare(ps);
            chain6_loaded = 1;
        }
        {
            s16 sy = 511 - view_y - (a->v[0] + 16);
            if ((u16)(sy + 48) < 288) k = chain_put(a->facing == 2 ? 2 : 0, sat, k, max, sx, sy);
        }
        for (y = a->v[0] - 16; y > AY(a) + 16; y -= 32)
        {
            s16 sy = 511 - view_y - y;     /* only the links on screen go in the table */
            if ((u16)(sy + 32) < 256) k = chain_put(1, sat, k, max, sx, sy);
        }
    }
    return k - first;
}

/* ---- The archer riding a dir-2 crusher (class 4 kind 3 variant 8,
 * stage6_families.md 11): dormant until on screen; rides at the crusher's
 * y + 43 (a 4-px hop for 2 updates when it lands); while within 144 px of
 * the camera's top it fires (release 8, draw 48: an arrow every 56), no aim
 * but its facing; hurt: 10 updates; death: it flies off away from the
 * player (up 4 px, gravity 64 to 1920, 1.5 px sideways) to the floor, 10
 * landed, a burst, 50 points, 10 gold (14/16) or 100. Chains 0-1 idle, 2-3
 * release, 4-5 draw, 6-7 hurt, 8-9 death flight, 10-11 landed. link: the
 * crusher's slot + 1. */
void rarch6_update(Actor *a)
{
    Actor *c = a->link ? actors + a->link - 1 : NULL;
    s16 top;
    if (c && c->fam != F_CRUSH6) c = NULL;
    switch (a->state)
    {
    case 0:
        a->active = 0;
        if ((u16)(AX(a) - CAM_X) > 432 || (u16)(AY(a) - CAM_Y) > 288) return;
        a->state = 2; a->active = 1; a->hp = a->last_hp = 4; a->phase = 0;
        a->facing = AX(a) >= (s16)(player.x >> 8);
        actor_anim(a, a->facing);
        return;
    case 2:
    case 4:
        if (encounter_clear) { sound_post(0x33); effect(F_BURST, AX(a), AY(a), 5); actor_free(a); return; }
        if (!(game_frame & 15) && AX(a) < player.cam_min_x) { actor_free(a); return; }
        actor_step(a);
        if (a->state == 2 && a->hp != a->last_hp)
        {
            a->last_hp = a->hp;
            if (!a->hp)
            {
                a->state = 6; a->active = 2; a->facing = AX(a) >= (s16)(player.x >> 8) ? 0 : 1;
                a->vx = a->facing ? -384 : 384; a->vy = 1024;
                actor_anim(a, 8 + a->facing);
                return;
            }
            sound_post(0x32); a->state = 4; a->active = 2; a->timer = 10; actor_anim(a, 6 + a->facing);
        }
        else if (a->state == 4)
        {
            if (!--a->timer) { a->state = 2; a->active = 1; a->phase = 2; a->timer = 1; }
        }
        else switch (a->phase)
        {
        case 0:
            if (AY(a) - CAM_Y <= 144)        /* within 144 of the camera's y (no x test) */
            {
                Actor *s;
                a->facing = AX(a) >= (s16)(player.x >> 8);
                if ((u16)(AX(a) - CAM_X + 32) <= 448) sound_post(SND_ARROW);
                s = actor_new(F_ARROW);
                if (s) { s->sub = a->facing ? 12 : 4; s->x = a->x + ((s32)(a->facing ? -10 : 10) << 8); s->y = a->y + (4 << 8); }
                actor_anim(a, 2 + a->facing);
                a->phase = 1; a->timer = 8;
            }
            else if (!((game_frame + (a - actors)) & 7)) { a->facing = AX(a) >= (s16)(player.x >> 8); actor_anim(a, a->facing); }
            break;
        case 1:                             /* release 8, then the draw */
        case 2:                             /* (after a hurt) the draw at once */
            if (a->phase == 1 && --a->timer) break;
            a->facing = AX(a) >= (s16)(player.x >> 8); actor_anim(a, 4 + a->facing); a->phase = 3; a->timer = 48;
            break;
        case 3:
            if (!--a->timer) a->phase = 0;
            break;
        }
        if (c)                              /* the ride, after the phase */
        {
            if (c->phase == 2 && c->timer == 60) a->t2 = 3;
            a->y = c->y + ((s32)(43 + (a->t2 > 1 ? 4 : 0)) << 8);
            if (a->t2) a->t2--;
        }
        return;
    case 6:                                 /* the death flight */
        actor_step(a);
        a->x += a->vx; a->y += a->vy;
        a->vy = a->vy - 64 < -1920 ? -1920 : a->vy - 64;
        if (!(game_frame & 15) && ((u16)(AX(a) - CAM_X + 32) > 448)) { actor_free(a); return; }
        if (a->vy < 0 && terrain_floor(AX(a), AY(a) - 8, &top)) { a->y = (s32)(top + 8) << 8; a->state = 8; a->timer = 10; actor_anim(a, 10 + a->facing); }
        return;
    case 8:
        if (--a->timer) return;
        sound_post(0x33);
        effect(F_BURST, AX(a), AY(a), 5);
        award(0);
        drop(a, (rnd() & 15) < 2 ? 2 : 1);
        actor_free(a);
        return;
    }
}
