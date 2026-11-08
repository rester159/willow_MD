/* Queen Bavmorda (specs/stage6_bavmorda.md): stage 6's final boss in area 6,
 * her spells, the candelabras and the altar brazier.
 *
 * Area 6's native coordinates: x = arcade x - 1024, y = arcade y - 3072
 * (her floor y 88). Art (tools/pack_stage6.sh): bav chains 0-1 walk, 2-3
 * cast, 4-5 take-off, 6-14 the nine flight poses (facing right .. the
 * viewer .. left), 15 the whirlwind; spell chains 0 spark, 1 ball, 2 ball
 * burst, 3 bolt, 4 flame, 5 flame end, 8 candelabra, 9 lightning glow,
 * 10 charge glow, 11 star glow, 12 candelabra flicker, 13 brazier fireball,
 * 14 charge glow (second part); altar chains 0 still, 1 wake, 2 hop. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "hud.h"
#include "sound.h"
#include "game.h"
#include "dirmath.h"

#define FACE(a) ((a)->facing & 1)
#define XO 1024
#define YO 3072
#define FLOOR 88
enum { B_WALK = 0, B_CAST = 2, B_TAKEOFF = 4, B_FLY = 6, B_WHIRL = 15 };
enum { S_SPARK, S_BALL, S_BURST, S_BOLT, S_FLAME, S_FLAME_END, S_CANDLE = 8, S_LGLOW, S_CGLOW, S_SGLOW, S_FLICKER, S_FIREBALL, S_CGLOW2 };

static u8 bav_talked;               /* FF838B */
u8 bav_end;                         /* her defeat: src/game.c runs the ending */
s16 bav_end_x, bav_end_y;
static s16 bav_x, bav_y;            /* FF83F0 / FF83F2 */
static u8 burst_on, sparks_live;    /* FF83F4 / FF0EC2 */
static u8 candle_hit, altar_hit;    /* FF83F5 / FF83EF */
static u8 bav_right;                /* her +30 = 0 (faces right): the sparks' turn */

static const u8 orders[4][4] = { { 4, 1, 3, 2 }, { 1, 4, 2, 3 }, { 3, 2, 4, 1 }, { 2, 3, 1, 4 } };   /* 0x29A10 */
static const s16 candle_x[5] = { 0, 64, 144, 240, 320 };                                        /* 0x2A256 - 1024 */
static const u8 burst_delay[8][4] = { { 80, 140, 180, 255 }, { 60, 120, 180, 240 }, { 60, 110, 170, 220 }, { 50, 100, 140, 200 },
                                      { 50, 80, 130, 180 }, { 40, 60, 100, 140 }, { 30, 50, 80, 100 }, { 20, 40, 60, 80 } };   /* 0x29AD8 */
static const u16 takeoff[16] = { 320, 320, 320, 320, 640, 640, 640, 640, 640, 640, 960, 960, 960, 960, 960, 960 };   /* 0x29B16 (4/6/6) */
static const u8 ball_int_hi[16] = { 80, 80, 70, 70, 60, 60, 50, 50, 40, 40, 30, 30, 20, 20, 10, 10 };
static const u8 ball_int_lo[16] = { 60, 60, 50, 50, 40, 40, 30, 30, 30, 20, 20, 20, 10, 10, 10, 10 };
static const u16 ball_mask[16] = { 0x2453, 0x58A4, 0x4595, 0x2A95, 0x6A95, 0x6A95, 0x73B5, 0xA69F, 0xAED9, 0x6DB5, 0xDEBD, 0xABDF, 0xB6DB, 0xFDFB, 0xBEFF, 0xFBF7 };
/* Glide paths (0x2A38A, 0x2A3C0), native. */
static const s16 path[2][12][2] = {
    { { 64, 152 }, { 120, 176 }, { 192, 192 }, { 264, 176 }, { 320, 152 }, { 352, 176 }, { 320, 200 }, { 264, 176 }, { 192, 160 }, { 120, 176 }, { 64, 200 }, { 32, 176 } },
    { { 320, 152 }, { 264, 176 }, { 192, 192 }, { 120, 176 }, { 64, 152 }, { 32, 176 }, { 64, 200 }, { 120, 176 }, { 192, 160 }, { 264, 176 }, { 320, 200 }, { 352, 176 } },
};
static const u16 spark_sets[4][8] = { { 30, 70, 110, 150, 190, 230, 270, 310 }, { 310, 30, 70, 110, 150, 190, 230, 270 },
                                      { 110, 190, 270, 30, 310, 70, 230, 150 }, { 310, 270, 230, 190, 150, 110, 70, 30 } };   /* 0x25CB6 */
static const char *const bav_text[3] = { "IT LOOKS LIKE YOU MADE IT IN TIME.", "DO YOU THINK YOU CAN DESTROY ME WITH", "THAT MAGIC TOY OF YOURS." };

/* Angle 0..255 (0 up, clockwise; y up) from (x, y) to (tx, ty). */
static u16 angle256(s16 x, s16 y, s16 tx, s16 ty) { return angle128(tx - x, ty - y) << 1; }

static Actor *spawn(u16 fam, s32 x, s32 y, u16 variant)
{
    Actor *s = actor_new(fam);
    if (s) { s->x = x; s->y = y; s->variant = variant; }
    return s;
}

/* Bavmorda's record fields: v[0] dir (+0F: walk 0/1, glide 0..31, final 0..127),
 * v[1] pose index (+30), v[2] invulnerability (+31), v[3] star-burst count
 * (+33), v[4] waypoint index, v[5] glide list; t2 take-off countdown (+38);
 * vx ball check (+41); vy bob speed; flags: bit 0 the final phase used,
 * bits 4-7 the attack-order position; link: the attack order. */
#define CHAIN(a) ((a)->pad[0])     /* her current chain (the pose rules compare it) */
static void bav_anim(Actor *a, u16 k) { actor_anim(a, k); CHAIN(a) = k; }

static void foot_pose(Actor *a, u16 k)
{
    u16 f = AX(a) >= (s16)(player.x >> 8);
    if (f != a->v[1] || CHAIN(a) != k + f) { a->v[1] = f; bav_anim(a, k + f); }
}

static void fly_pose(Actor *a, s16 tx, s16 ty)
{
    s16 want = (((angle256(AX(a), AY(a), tx, ty) + 8) & 255) >> 4) - 4;
    if (want < 0) want = 0;
    if (want > 8) want = 8;
    if (++a->pad[1] < 14 && CHAIN(a) >= B_FLY && CHAIN(a) < B_WHIRL) return;   /* +3A: a step every 14 updates */
    a->pad[1] = 0;
    if (CHAIN(a) < B_FLY || CHAIN(a) >= B_WHIRL) { a->v[1] = want; bav_anim(a, B_FLY + want); return; }
    if (want == (s16)a->v[1]) return;
    a->v[1] += want > (s16)a->v[1] ? 1 : -1;
    bav_anim(a, B_FLY + a->v[1]);
}


static u16 next_code(Actor *a)
{
    u16 k = a->flags >> 4;
    return k < 4 ? orders[a->link & 3][k] : 0;
}

static void walk(Actor *a)
{
    s16 lo = a->v[5] ? 32 : 224, hi = lo + 128;
    a->x += a->v[0] ? -256 : 256;
    if (AX(a) < lo) a->v[0] = 0;
    if (AX(a) >= hi) a->v[0] = 1;
    if (!(game_frame & 15)) foot_pose(a, B_WALK);
    actor_step(a);
}

static void glide_move(Actor *a)
{
    s16 vx, vy;
    dir_v(a->v[0] << 2, 512, &vx, &vy);
    a->x += vx; a->y += vy;
}

void bav_update(Actor *a)
{
    s16 px = player.x >> 8;
    switch (a->state)
    {
    case 0:
        a->state = 1; a->active = 2;
        a->hp = a->last_hp = 180;
#ifdef BAV_HP
        a->hp = a->last_hp = BAV_HP;        /* test builds */
#endif
        a->v[0] = 1; a->v[1] = 1; a->v[2] = 0; a->v[5] = 1;
        a->flags = 0; a->link = rnd() & 3;
        burst_on = sparks_live = candle_hit = altar_hit = 0;
        bav_anim(a, B_WALK + 1);
        a->phase = 0;
        return;
    case 1:                                 /* wake (state 0): the dialogue once */
        gate_freeze = 1;
        if (!a->phase)
        {
            if (bav_talked) { a->phase = 9; return; }
            bav_talked = 1;
            sound_post(SND_MUSIC_BAV); sound_post(0x01FA);
            scene_lock = 1;
            a->phase = 4; a->sub = 0; a->v[4] = 0; a->timer = 30;
            return;
        }
        if (a->phase == 4)
        {
            scene_lock = 1;
            if (--a->timer) return;
            a->timer = 3;
            if (a->sub == 3) { hud_message(NULL, 4); hud_message(NULL, 6); hud_message(NULL, 8); a->phase = 9; return; }
            {
                const char *t = bav_text[a->sub];
                a->v[4]++;
                hud_text(t, a->v[4], 1, 4 + a->sub * 2);
                if (t[a->v[4] - 1] != ' ') sound_post(0x2F);
                if (!t[a->v[4]]) { a->sub++; a->v[4] = 0; a->timer = a->sub == 3 ? 96 : 12; }
            }
            return;
        }
        /* The fight (0x29A9C). */
        scene_lock = 0; gate_freeze = 0;
        a->state = 2; a->active = 1; a->phase = 0; a->timer = 30;
        midboss_active = 1;
        hud_boss(a->hp);
        return;
    case 2:
        break;
    default:
        return;
    }
    bav_right = a->v[1] == 0 || (CHAIN(a) >= B_FLY && a->v[1] < 4);
    switch (a->phase)
    {
    case 0:                                 /* 30 still */
        if (--a->timer) break;
        a->phase = 2; a->sub = 0; a->v[5] = AX(a) < 192;
        a->t2 = takeoff[rnd() & 15];
        a->v[3] = burst_delay[(rank >> 13) & 7][rnd() & 3];
        break;
    case 2:
        if (!a->sub) { a->v[0] = AX(a) >= px; foot_pose(a, B_WALK); a->sub = 2; break; }
        if (a->sub == 2)
        {
            if (a->t2) a->t2--;
            if (!--a->v[3]) { a->phase = 4; a->sub = 0; a->v[3] = burst_delay[(rank >> 13) & 7][rnd() & 3]; break; }
            if (!a->t2) { a->sub = 4; a->timer = 24; a->t2 = takeoff[rnd() & 15]; bav_anim(a, B_TAKEOFF + a->v[5]); break; }
            walk(a);
            break;
        }
        actor_step(a);                      /* take-off: 24 */
        if (--a->timer) break;
        a->phase = 6; a->sub = 0;
        break;
    case 4:                                 /* the star burst */
        if (!a->sub)
        {
            Actor *g = spawn(F_SGLOW6, a->x, a->y, 0);
            if (g)
            {
                const u16 *set = spark_sets[(rnd() >> 3) & 3];
                u16 i;
                burst_on = 1; sparks_live = 0;
                for (i = 0; i < 8; i++)
                {
                    Actor *s = spawn(F_SPARK6, a->x, a->y, 0);
                    if (!s) break;
                    s->t2 = set[7 - i]; s->timer = 1 + 8 * i; sparks_live++;
                }
            }
            a->sub = 2; a->timer = 180;
            foot_pose(a, B_CAST);
            break;
        }
        if (a->sub == 2) { actor_step(a); if (!--a->timer) { a->sub = 4; foot_pose(a, B_WALK); } break; }
        if (a->t2) a->t2--;
        walk(a);
        if (!burst_on) { a->phase = 2; a->sub = 0; }
        break;
    case 6:                                 /* the glide (figure-8) */
        if (!a->sub)
        {
            a->v[4] = 5; a->v[0] = a->v[5] ? 28 : 4; a->vx = 80; a->pad[1] = 14;
            a->sub = 2;
        }
        if (a->sub == 2)
        {
            const s16 *w = path[a->v[5]][a->v[4]];
            actor_step(a);
            glide_move(a);
            fly_pose(a, px, player.y >> 8);
            if (!(game_frame & 1))
            {
                s16 d = ((((angle256(AX(a), AY(a), w[0], w[1]) + 4) & 255) >> 3) - a->v[0]) & 31;
                if (d) a->v[0] = (a->v[0] + (d < 16 ? 1 : 31)) & 31;
            }
            if (AX(a) - w[0] <= 14 && w[0] - AX(a) <= 14 && AY(a) - w[1] <= 14 && w[1] - AY(a) <= 14)
            {
                if (++a->v[4] >= 12)
                {
                    a->v[4] = 0;
                    if ((px >= 192 ? 0 : 1) == a->v[5])
                    {
                        a->v[5] ^= 1; a->v[0] = a->v[5] ? 14 : 18; a->sub = 4;
                        break;
                    }
                }
            }
            {
                u16 c = next_code(a);
                if (c && AX(a) - candle_x[c] <= 1 && candle_x[c] - AX(a) <= 1)
                {
                    a->flags += 0x10; a->v[3] = c;  /* +43 */
                    a->phase = 10; a->sub = 0;
                    break;
                }
            }
            if (!--a->vx)
            {
                a->vx = (a->hp >= 90 ? ball_int_hi : ball_int_lo)[(rank >> 12) & 15];
                if ((ball_mask[(rank >> 12) & 15] >> (rnd() & 15)) & 1) { a->phase = 8; a->sub = 0; }
            }
            break;
        }
        /* Descent until the feet (y - 32) land. */
        actor_step(a);
        glide_move(a);
        if (AY(a) <= FLOOR) { a->y = (s32)FLOOR << 8; a->phase = 2; a->sub = 0; a->v[0] = AX(a) >= px; }
        break;
    case 8:                                 /* the energy ball, hovering */
        fly_pose(a, px, player.y >> 8);
        actor_step(a);
        if (!a->sub) { spawn(F_CGLOW6, a->x, a->y, 1); sound_post(0xA9); a->sub = 2; a->timer = 20; break; }
        if (--a->timer) break;
        if (a->sub == 2)
        {
            Actor *b = spawn(F_BALL6, a->x, a->y, 0);
            if (b) b->v[0] = (angle256(AX(a), AY(a), px, player.y >> 8) + 2) >> 2;
            a->sub = 4; a->timer = 40;
            break;
        }
        a->phase = 6; a->sub = 2;
        break;
    case 10:                                /* the lightning over a candelabra */
    case 12:                                /* (final) the bolt onto the altar */
        if (a->phase == 12 && a->sub < 4)
        {
            s16 vx, vy;
            a->v[2] = 0xFF; a->active = 2;
            if (!(game_frame & 1)) a->v[0] = angle128(352 - AX(a), 216 - AY(a));
            dir_v(a->v[0], 768, &vx, &vy);
            a->x += vx; a->y += vy;
            actor_step(a);
            fly_pose(a, 352, 80);
            if (AX(a) - 352 <= 2 && 352 - AX(a) <= 2 && AY(a) - 216 <= 2 && 216 - AY(a) <= 2) a->sub = 4;
            break;
        }
        fly_pose(a, a->phase == 12 ? 352 : candle_x[a->v[3]], 88);
        actor_step(a);
        if ((a->phase == 10 && !a->sub) || (a->phase == 12 && a->sub == 4)) { spawn(F_LGLOW6, a->x, a->y, 0); a->sub = a->phase == 10 ? 2 : 6; a->timer = 60; break; }
        if (--a->timer) break;
        if (a->sub == 2 || a->sub == 6)
        {
            Actor *h = spawn(F_BOLT6, a->x, a->y, a->phase == 12 ? 3 : 0);
            if (h) h->v[0] = a->v[3];
            sound_post(0xA6);
            a->sub += 2; a->timer = 30;
            break;
        }
        if (a->phase == 10) { a->phase = 6; a->sub = 2; break; }
        a->phase = 14; a->sub = 0;
        break;
    case 14:                                /* (final) hovering over the centre, bobbing */
        if (!a->sub)
        {
            s16 vx, vy;
            a->v[2] = 0xFF; a->active = 2;
            if (!(game_frame & 1)) a->v[0] = angle128(192 - AX(a), 200 - AY(a));
            dir_v(a->v[0], 512, &vx, &vy);
            a->x += vx; a->y += vy;
            actor_step(a);
            fly_pose(a, 192, 64);
            if (AX(a) - 192 <= 2 && 192 - AX(a) <= 2 && AY(a) - 200 <= 2 && 200 - AY(a) <= 2) { a->v[2] = 1; a->sub = 2; a->vy = 80; a->timer = 0; }
            break;
        }
        actor_step(a);
        if (!(game_frame & 63)) sound_post(0xA3);
        /* The bob (0x28050): a 4-quarter vertical swing. */
        switch (a->timer & 3)
        {
        case 0: a->y += a->vy; a->vy -= 2; if (a->vy < 0) { a->vy = 0; a->timer++; } break;
        case 1: a->y -= a->vy; a->vy += 2; if (a->vy > 80) { a->vy = 80; a->timer++; } break;
        case 2: a->y -= a->vy; a->vy -= 2; if (a->vy < 0) { a->vy = 0; a->timer++; } break;
        default: a->y += a->vy; a->vy += 2; if (a->vy > 80) { a->vy = 80; a->timer++; } break;
        }
        break;
    }
    bav_x = AX(a); bav_y = AY(a);
    /* Damage (4): 68 updates immune; the final phase once at energy <= 24. */
    if (!a->hp)
    {
        sound_post(0x20FB);
        add_score(0x1000000);              /* score event 0x181A: 1,000,000 */
        hud_boss(-1);
        a->state = 4; a->active = 2;
        scene_lock = 1;
        bav_end = 1; bav_end_x = AX(a); bav_end_y = AY(a);
        actor_free(a);                      /* the ending's whirlwind takes her place (src/ending.c) */
        return;
    }
    if (a->v[2] && a->v[2] != 0xFF)
    {
        /* The hit flash (palette job 22: white / normal every 3 updates for
         * ~63): she shares a sprite line, so here she blinks instead. */
        if (!--a->v[2]) a->active = 1;
        else a->active = a->v[2] > 5 && ((a->v[2] / 3) & 1) ? 0 : 2;
    }
    if (a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        hud_boss(a->hp);
#ifdef BAV_NOFINAL
        a->flags |= 1;                      /* test builds: no final phase */
#endif
        if (!(a->flags & 1) && a->hp <= 24) { a->flags |= 1; a->phase = 12; a->sub = 0; a->v[2] = 0xFF; a->active = 2; return; }
        sound_post(0x32);
        a->active = 2; a->v[2] = 68;
    }
}

/* ---- Star burst: the glow (60, following her) and the sparks: wait,
 * spiral 80 updates, orbit, then aimed once at 4 px/update. */
void sglow6_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 2; a->timer = 60; actor_anim(a, S_SGLOW); return; }
    a->x = (s32)bav_x << 8; a->y = (s32)bav_y << 8;
    actor_step(a);
    if (!--a->timer) actor_free(a);
}

static void spark_gone(Actor *a)
{
    if (sparks_live && !--sparks_live) burst_on = 0;
    actor_free(a);
}

void spark6_update(Actor *a)
{
    s16 vx, vy;
    switch (a->state)
    {
    case 0: a->state = 2; a->active = 2; a->v[0] = a->v[1] = 0; a->v[2] = 0; a->v[3] = 44; a->sub = 0; actor_anim(a, S_SPARK); return;
    }
    actor_step(a);
    if (a->sub == 6)
    {
        a->x += a->vx; a->y += a->vy;
        if ((u16)(AX(a) - CAM_X + 48) > 480 || (u16)(AY(a) - CAM_Y + 32) > 320) { spark_gone(a); return; }
        { s16 top; if (terrain_floor(AX(a), AY(a), &top)) { spark_gone(a); return; } }
        return;
    }
    if (!a->sub) { if (--a->timer) { a->x = (s32)bav_x << 8; a->y = (s32)bav_y << 8; return; } a->sub = 2; a->timer = 0; }
    /* The ellipse step (0x2634C: 768 x, 672 y), its offset integrated. */
    {
        s16 d;
        dir_v(a->v[2], 768, &vx, &d);
        dir_v(a->v[2], 672, &d, &vy);
    }
    a->v[0] += vx; a->v[1] += vy;           /* the offset from her (1/256 px) */
    a->x = ((s32)bav_x << 8) + a->v[0];
    a->y = ((s32)bav_y << 8) + a->v[1];
    if (a->sub == 2)
    {
        if (!(++a->timer & 3))
        {
            a->v[2] = (a->v[2] + (bav_right ? a->v[3] : -a->v[3])) & 127;
            a->v[3] -= 2;
            if (a->v[3] <= 4) { a->sub = 4; a->active = 1; a->v[4] = bav_right; }
        }
        return;
    }
    if (!(game_frame & 1))
    {
        if (a->v[4] != bav_right) { a->v[4] = bav_right; a->v[2] = (a->v[2] + 64) & 127; }
        a->v[2] = (a->v[2] + (bav_right ? 4 : -4)) & 127;
    }
    if (!a->t2 || !--a->t2)
    {
        dir_v(angle128((s16)(player.x >> 8) - AX(a), (s16)(player.y >> 8) + 16 - AY(a)), 1024, &a->vx, &a->vy);
        a->sub = 6;
    }
}

/* ---- The energy ball: 64 directions, speed 256 + 128 a level (after 16
 * updates, then every 4, to 1536); bursts where it meets the floor (40). */
void ball6_update(Actor *a)
{
    s16 vx, vy, top;
    if (!a->state) { a->state = 2; a->active = 1; a->timer = 16; a->v[1] = 256; actor_anim(a, S_BALL); return; }
    actor_step(a);
    if (a->state == 4) { if (!--a->timer) actor_free(a); return; }
    dir_v(a->v[0] << 1, a->v[1], &vx, &vy);
    a->x += vx; a->y += vy;
    if (!--a->timer) { a->timer = 4; if (a->v[1] < 1536) a->v[1] += 128; }
    if (terrain_floor(AX(a), AY(a) - 8, &top))
    {
        if ((u16)(AX(a) - CAM_X + 32) <= 448) sound_post(0x37);
        a->state = 4; a->timer = 40; actor_anim(a, S_BURST);
        return;
    }
    if ((u16)(AX(a) - CAM_X + 48) > 480 || (u16)(AY(a) - CAM_Y + 32) > 320) actor_free(a);
}

/* ---- Glows: the lightning (80) and the ball's charge (18 + 60), following her. */
void glow6_update(Actor *a)
{
    if (!a->state)
    {
        a->state = 2; a->active = 2;
        if (a->fam == F_LGLOW6) { a->timer = 80; actor_anim(a, S_LGLOW); }
        else { a->timer = 78; actor_anim(a, S_CGLOW); }
        return;
    }
    a->x = (s32)bav_x << 8; a->y = (s32)bav_y << 8;
    actor_step(a);
    if (a->fam == F_CGLOW6 && a->timer == 60) actor_anim(a, S_CGLOW2);
    if (!--a->timer) actor_free(a);
}

/* ---- The lightning: a head (not drawn) laying 8 segments 16 px apart,
 * one every 3 updates, falling 16 px every 3; the first to land marks its
 * candelabra (var 0 head) or strikes the altar (var 3, the final phase:
 * harmful segments). v[0] the code. */
void bolt6_update(Actor *a)
{
    s16 top;
    if (!a->state)
    {
        a->state = 2; a->active = 2;
        if (a->variant == 0 || a->variant == 3) { a->timer = 1; a->t2 = 8; a->flags = 1; return; }
        a->active = a->variant == 4 ? 1 : 2;
        a->timer = 3;
        actor_anim(a, S_BOLT);
        return;
    }
    if (a->variant == 0 || a->variant == 3)
    {
        if (--a->timer) return;
        a->timer = 3;
        {
            Actor *s = spawn(F_BOLT6, a->x, a->y, a->variant == 3 ? 4 : (a->flags ? 1 : 2));
            if (s) s->v[0] = a->flags ? a->v[0] : 0;
        }
        a->flags = 0;
        if (!--a->t2) actor_free(a);
        return;
    }
    actor_step(a);
    if (--a->timer) return;
    a->timer = 3;
    a->y -= 16 << 8;
    if (terrain_floor(AX(a), AY(a) - 8, &top) || AY(a) < CAM_Y - 64)
    {
        if (a->variant == 1) candle_hit = a->v[0];
        if (a->variant == 4) altar_hit = 1;
        actor_free(a);
    }
}

/* ---- Candelabras (12/40, variants 1-4): struck, 60 flickering, then 3 flames. */
void candle6_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 2; actor_anim(a, S_CANDLE); return; }
    if (a->state == 2)
    {
        if (candle_hit != a->variant) return;
        candle_hit = 0; a->state = 4; a->timer = 60;
        return;
    }
    if (!(a->timer % 3)) actor_anim(a, (a->timer / 3) & 1 ? S_FLICKER : S_CANDLE);
    if (--a->timer) return;
    {
        s16 k;
        for (k = -32; k <= 32; k += 32) spawn(F_FLAME6, a->x + ((s32)k << 8), (s32)(3143 - YO) << 8, 1);
    }
    actor_free(a);
}

void flame6_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 1; a->timer = 240; actor_anim(a, S_FLAME); sound_post(0x37); return; }
    actor_step(a);
    if (--a->timer) return;
    if (a->state == 2) { a->state = 4; a->active = 2; a->timer = 10; actor_anim(a, S_FLAME_END); return; }
    actor_free(a);
}

/* ---- The altar brazier (4/28): crackles (0x8F) every 128..511 updates;
 * woken by the final-phase bolt, it hops (1.5 px a update, vy 1024 - 48)
 * and fires 6 shots at the peak of every other hop. */
static const u16 crackle[4] = { 256, 384, 128, 511 };
void brazier6_update(Actor *a)
{
    s16 top;
    if (!a->state) { a->state = 2; a->active = 2; a->timer = 63; a->t2 = 95; a->facing = 1; actor_anim(a, 0); return; }
    if (!--a->timer) { a->timer = crackle[(rnd() >> 1) & 3]; sound_post(0x8F); }
    switch (a->phase)
    {
    case 0:
        if (!altar_hit || --a->t2) return;
        a->active = 1; a->vy = 1024; a->v[0] = 6; a->v[1] = 8; a->phase = 2; actor_anim(a, 1);
        return;
    case 2:                                 /* crouch 8 */
        actor_step(a);
        if (--a->v[1]) return;
        if (!--a->v[0]) { a->v[0] = 5; a->facing ^= 1; }
        actor_anim(a, 2); a->phase = 4; a->vy = 1024;
        return;
    case 4:                                 /* rise, fire at the peak */
        actor_step(a);
        a->y += a->vy; a->vy -= 48; a->x += FACE(a) ? -384 : 384;
        if (a->vy >= 0) return;
        if (a->v[0] & 1)
        {
            static const u8 dirs[6] = { 32, 40, 48, 80, 88, 96 };
            u16 i;
            for (i = 0; i < 6; i++)
            {
                Actor *f = spawn(F_FIRE6, a->x, a->y + (16 << 8), 0);
                if (f) dir_v(dirs[i], 768, &f->vx, &f->vy);
            }
        }
        a->phase = 6; a->vy = 0;
        return;
    case 6:                                 /* fall */
        actor_step(a);
        if (a->vy < 1024) a->vy += 48;
        a->y -= a->vy; a->x += FACE(a) ? -384 : 384;
        if (actor_ground(a, 0, -8)) { a->phase = 2; a->v[1] = 8; actor_anim(a, 1); }
        (void)top;
        return;
    }
}

void fire6_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 1; actor_anim(a, S_FIREBALL); return; }
    actor_step(a);
    a->x += a->vx; a->y += a->vy;
    if ((u16)(AX(a) - CAM_X + 48) > 480 || (u16)(AY(a) - CAM_Y + 32) > 320) actor_free(a);
}

/* Kael's parry spark (12/28): 12 updates of spell chain 15. */
void pspark6_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 2; a->timer = 12; actor_anim(a, 15); return; }
    actor_step(a);
    if (!--a->timer) actor_free(a);
}

void bav_new_game(void) { bav_talked = 0; bav_end = 0; }
