/* Stage 5's boss, Eborsisk (specs/stage5.md 6): two necks on one anchor,
 * each with a head. The arcade draws one neck on scroll3 (in front of the
 * scroll2 arena) and the other as a sprite; both move rigidly with the
 * anchor, so here they are one plane-B picture per pair of poses
 * (tools/bosspack5.py: 16 pictures, CRAM line 3, high priority; every
 * sprite is drawn high while it shows), and the arena is re-tiled on line 2
 * into the far layer's VRAM to give the necks the foreground pool. The
 * heads, the fire chains and the energy bars are sprites.
 *
 * Records: the controller (F_EBOR: the anchor, the far neck's decisions;
 * the far neck carries head B), the sprite neck's box (F_NECK5: hit once,
 * then a shield that stops shots), head A on the sprite neck, head B on the
 * far neck (F_HEAD5), flames (F_FLAME5).
 *
 * Approximations (M1_NOTES): the bob's 2-px camera shake moves the whole
 * view (the arcade's moves the anchor with the camera: the boss holds
 * still while the arena shakes); a head's hit flash is a blink; the push
 * wall ends Willow's shots past it, not her spells (their class-8 kinds are
 * not mapped); the loads at the wake take ~10 frames (the necks are still below the
 * screen then). */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"
#include "hud.h"
#include "game.h"
#include "stage.h"
#include "shot.h"
#include "dirmath.h"
#include "s5spr.h"
#include "boss5_tables.h"
#include "../data/boss5/boss5.h"
#include "../data/hud/hud_tiles.h"

extern const u16 boss5_arena_map[], boss5_neck_maps[], boss5_arena_pal[], boss5_neck_pal[];
extern const u8 boss5_arena_tiles[], boss5_neck_tiles[], boss5_neck_relay[];
extern u8 stage5_form;

#define SND_BOOM 0x33

static const s16 head_off[2][4][2] = {      /* from the anchor by its neck's pose: head A (0x2C1F0), head B (0x2C548) */
    { { -112, 112 }, { -166, 93 }, { -221, 74 }, { -272, 56 } },
    { { -152, 112 }, { -206, 92 }, { -261, 74 }, { -312, 56 } } };
static const s16 mouth_off[3][2] = { { -32, -24 }, { -16, -32 }, { 0, -40 } };   /* 0x2C198 by facing */
static const s16 fire_off[3][11][2] = {     /* 0x26B0C by facing */
    { { 0, 0 }, { -8, 0 }, { -16, 0 }, { -32, 0 }, { -56, 0 }, { -80, 0 }, { -112, 0 }, { -144, 0 }, { -176, 0 }, { -216, 0 }, { -264, 0 } },
    { { 0, 0 }, { -8, -4 }, { -16, -8 }, { -32, -16 }, { -56, -28 }, { -80, -40 }, { -112, -56 }, { -144, -72 }, { -176, -88 }, { -216, -108 }, { -256, -120 } },
    { { 0, 0 }, { -8, -8 }, { -16, -16 }, { -32, -16 }, { -48, -32 }, { -64, -48 }, { -88, -56 }, { -112, -80 }, { -136, -104 }, { -168, -120 }, { -200, -152 } } };
static const u8 fire_n[3] = { 11, 9, 6 };   /* 0x26B96 + 1 */
static const s16 boom_a[8][2] = { { 0, -8 }, { 40, 8 }, { -40, -32 }, { 24, -32 }, { -24, 8 }, { 24, -40 }, { -40, 0 }, { 22, -12 } };   /* 0x2C352 */
static const s16 boom_b[8][2] = { { -8, 0 }, { -88, -32 }, { -24, 24 }, { 24, -80 }, { 80, -56 }, { -120, -112 }, { 0, -40 }, { -72, 16 } };   /* 0x2C470 */
static const s16 push_line[4][8] = {        /* 0x2C68A: by the far neck's pose, (player y >> 5) & 7 */
    { -128, -128, -96, -64, -64, -96, -96, -96 }, { -128, -128, -96, -96, -96, -128, -128, -128 },
    { -128, -128, -128, -128, -192, -192, -192, -192 }, { -128, -128, -128, -192, -256, -256, -128, -128 } };
static const u8 reach_tab[8] = { 1, 0, 1, 0, 0, 2, 1, 0 };   /* 0x2BD26 */
static const u8 mouth_of_pose[4] = { 2, 1, 1, 0 };          /* 0x2C104 */
static const s16 neck_box[8] = { 40, 136, 0, 96, 40, 136, 0, 96 };

/* A neck: 0 the far one (head B, decisions every 59, rest pose 1), 1 the
 * sprite one (head A, every 64, rest 0). */
enum { NK_IDLE, NK_STRETCH, NK_BREATH, NK_RETRACT };
typedef struct { u8 pose, rest, target, action, state, flag; u16 timer, period; } Neck;

static struct
{
    s16 ax, ay;                     /* the anchor (+48, +4A) */
    s16 bob_acc, bob_v;             /* bob: drop (8.8) and speed */
    u8 sub;                         /* 0 asleep, 1 loading, 2 rise, 3 fight, 4 dying, 5 collapse, 6 done */
    u8 load, sway, bob, bob_t;
    u8 breath;                      /* FF0EFC: a breath runs (no sway) */
    u8 dead;                        /* FF0EFE: heads dead */
    u16 timer;
    const u16 *pic;                 /* the necks' picture shown */
    u8 pic_k;                       /* its index (far pose * 4 + sprite pose) */
    u8 fpose;                       /* the flames' shared node (0..6) */
    Neck nk[2];
    Actor *head[2];                 /* [0] head A, [1] head B */
    u8 hp[2];
} eb;
u8 boss5_layer;                     /* sprites drawn high (main.c) while the necks show */

void boss5_reset(void)
{
    memset(&eb, 0, sizeof eb);
    boss5_layer = 0;
    swell_y = 0;
}

/* ---- The pictures and the loads. */
static void place(void)
{
    s16 fx = CAM_X + 32 - eb.ax - NECK_DX, fty = eb.ay + NECK_DY - CAM_Y - 239 + swell_y;   /* the necks shake with the heads */
    const u16 *pic = boss5_neck_maps + (u32)(eb.nk[0].pose * 4 + eb.nk[1].pose) * (NECK_W * NECK_H);
    stage_far_at(fx, fty);
    u16 k = eb.nk[0].pose * 4 + eb.nk[1].pose;
#ifdef EXP_ONEPIC
    if (eb.pic) { pic = eb.pic; k = eb.pic_k; }     /* measurement builds: the first picture only */
#endif
    if (pic == eb.pic || eb.sub < 2) return;
    if (!eb.pic) { eb.pic = pic; eb.pic_k = k; stage_far_pic(pic, NECK_W, NECK_H); return; }
    if (stage_far_busy()) return;           /* the last change still being laid out: this one next */
    {
        u8 e = boss5_neck_relay[eb.pic_k * 16 + k];
        if (e == 255) stage_far_pic(pic, NECK_W, NECK_H);     /* both necks changed: the whole picture */
        else stage_far_pic_spans(pic, boss5_neck_relay + 256 + e * NECK_W * 2);
    }
    eb.pic = pic; eb.pic_k = k;
}

/* The wake (6.3): the arena's tiles into the far layer's VRAM, plane A
 * and the lines switched, the necks' tiles over the pool, a load a frame
 * (~4-5 KB). */
static bool load_step(void)
{
    u16 k = eb.load++;
    if (k < 3)
    {
        u16 n0 = (ARENA_TILES + 2) / 3, n = k == 2 ? ARENA_TILES - 2 * n0 : n0;
        DMA_queueDma(DMA_VRAM, (void *)(boss5_arena_tiles + (u32)k * n0 * 32), (509 + k * n0) * 32, n * 16, 2);
        return FALSE;
    }
    if (k == 3)
    {
        PAL_setColors(32, boss5_arena_pal, 16, DMA_QUEUE);
        PAL_setColors(48, boss5_neck_pal, 16, DMA_QUEUE);
        stage_arena(boss5_arena_map, ARENA_C0, ARENA_C1, ARENA_R0);
        return FALSE;
    }
    if (k < 9)
    {
        u16 per = (NECK_TILES + 4) / 5, at = (k - 4) * per, n = at + per > NECK_TILES ? NECK_TILES - at : per, i;
        for (i = at; i < at + n; )
        {
            /* pool tiles from 16, the rest after the arena's */
            u16 v = i < NECK_POOL_TILES ? 16 + i : NECK_FAR_AT + i - NECK_POOL_TILES;
            u16 m = i < NECK_POOL_TILES ? (NECK_POOL_TILES - i < at + n - i ? NECK_POOL_TILES - i : at + n - i) : at + n - i;
            DMA_queueDma(DMA_VRAM, (void *)(boss5_neck_tiles + (u32)i * 32), v * 32, m * 16, 2);
            i += m;
        }
        return FALSE;
    }
    /* The bars' glyphs: E N E M, Y, a cell x 4. */
    {
        static const u16 g[9] = { HT_MSG_E, HT_MSG_N, HT_MSG_E, HT_MSG_M, HT_MSG_Y, HT_BOSS_CELL, HT_BOSS_CELL, HT_BOSS_CELL, HT_BOSS_CELL };
        u16 i;
        for (i = 0; i < 9; i++) hud_glyph_copy(g[i], BAR_VRAM + i);
    }
    return TRUE;
}

/* ---- Heads, necks. */
static void head_pos(u16 i, s16 *x, s16 *y)
{
    const Neck *n = &eb.nk[i ? 0 : 1];
    *x = eb.ax + head_off[i][n->pose][0];
    *y = eb.ay + head_off[i][n->pose][1];
}

static u16 head_facing(s16 hx, s16 hy)
{
    u16 d = ((angle128((player.x >> 8) - hx, (player.y >> 8) - hy) + 4) >> 3) & 15;   /* 16 steps, 0 up */
    return d == 11 ? 1 : d > 11 ? 0 : 2;
}

static void flames(u16 type, u16 facing, s16 x, s16 y)
{
    u16 i, n = fire_n[type];
#ifdef EXP_NOFLAMES
    n = 0;                                  /* measurement builds */
#endif
    for (i = 0; i < n; i++)
    {
        Actor *f = actor_new(i < 3 ? F_FLAME5 : i < 6 ? F_FLAME5B : i < 9 ? F_FLAME5C : F_FLAME5D);
        if (!f) break;
        f->x = (s32)(x + fire_off[facing][i][0]) << 8;
        f->y = (s32)(y + fire_off[facing][i][1]) << 8;
        f->timer = 7 + 4 * i;           /* hidden until the one before burns, then 4 updates */
        f->variant = i;
        f->sub = i == n - 1;            /* the last clears the breath when it burns */
        f->active = 0;
    }
}

/* One neck's decisions (6.5): idle sub, a decision every period updates. */
static void neck_update(u16 k)
{
    Neck *n = &eb.nk[k];
    Actor *h = eb.head[k ? 0 : 1];
    if (!h) { n->state = NK_IDLE; return; }     /* its head is gone: it only sways */
    switch (n->state)
    {
    case NK_IDLE:
        if (--n->timer) return;
        n->timer = n->period;
        {
            s16 d = (player.x >> 8) - eb.ax;
            u16 j = (u16)(d < 0 ? -d : d) >> 4, band = k ? (j < 20 ? eb_band1[j] : 48) : 48;
            s8 act = eb_act[stage5_form ? 1 : 0][rank >> 14][band + (rnd() & 15)];
            if (act < 0) return;
            n->action = act;
            if (act == 3) { n->target = 3; n->state = NK_STRETCH; return; }
            n->pose = 0;
            n->target = reach_tab[rnd() & 7];
            if (!n->target) { n->flag = 1; n->state = NK_BREATH; return; }
            n->state = NK_STRETCH;
        }
        return;
    case NK_STRETCH:
        if (--n->timer) return;
        n->timer = n->period;
        if (n->pose < n->target) n->pose++;
        if (n->pose != n->target) return;
        if (n->action == 3) { n->state = NK_RETRACT; return; }
        n->flag = 1;
        n->state = NK_BREATH;
        return;
    case NK_BREATH:
        if (n->flag) return;
        n->state = NK_RETRACT;
        n->timer = n->period;
        return;
    case NK_RETRACT:
        if (--n->timer) return;
        n->timer = n->period;
        if (n->pose > n->rest) n->pose--;
        else if (n->pose < n->rest) n->pose++;
        if (n->pose == n->rest) n->state = NK_IDLE;
        return;
    }
}

static void anchor_update(void)
{
    /* Sway (0x2BA9E), not while a breath runs. */
    if (!eb.breath)
    {
        if (!eb.sway) { if (--eb.ax < 5768) eb.sway = 2; }
        else if (++eb.ax > 6040) eb.sway = 0;
    }
    /* Bob (0x2BB2E), 48 updates: down 15 px over 16, 16 still (the arcade's
     * 2-px shake), back up over 16. */
    if (++eb.bob_t > 16) { eb.bob_t = 1; eb.bob = eb.bob == 4 ? 0 : eb.bob + 2; if (!eb.bob) eb.bob_v = 512; }
    if (eb.bob == 0) { eb.bob_v -= 32; eb.bob_acc += eb.bob_v; }
    else if (eb.bob == 4) { eb.bob_v += 32; eb.bob_acc -= eb.bob_v; if (eb.bob_acc < 0) eb.bob_acc = 0; }
    eb.ay = 104 - (eb.bob_acc >> 8);
    if (eb.bob == 4 && eb.bob_t == 16) { eb.bob_acc = 0; eb.ay = 104; }
    /* The shake (state 2): +-2 negated every update into camera Y, so the
     * view alternates 2 px / 0 (the view offset stage 3's swell uses). */
    swell_y = eb.bob == 2 && (eb.bob_t & 1) ? 2 : 0;
}

/* The far body pushes the player back and hurts (0x2C63A). */
static void push_wall(void)
{
    s16 px = player.x >> 8, py = player.y >> 8;
    if (!(game_frame & 7)) shot_end_beyond(eb.ax, push_line[eb.nk[0].pose]);   /* every 8 updates: shots past the line end */
    if (px <= eb.ax + push_line[eb.nk[0].pose][(py >> 5) & 7]) return;
    player.x -= 4 << 8;
    player_hurt();
}

/* ---- The controller (class 18 kind 6 variant 0). */
void ebor_update(Actor *a)
{
    u16 i;
    if (!a->state) { a->state = 2; a->active = 0; return; }
    if (!(game_frame & 3) && ++eb.fpose >= 7) eb.fpose = 0;
    switch (eb.sub)
    {
    case 0:
        if (!boss5_wake) return;
        memset(&eb, 0, sizeof eb);
        eb.sub = 1;
        eb.ax = 6040; eb.ay = -152;
        eb.nk[0].pose = eb.nk[0].rest = 1; eb.nk[0].period = 59; eb.nk[0].timer = 59;
        eb.nk[1].period = 64; eb.nk[1].timer = 64;
        eb.bob_v = 512;
        encounter_clear = 1;                    /* FF83D0 */
        for (i = 0; i < 2; i++)
        {
            Actor *h = actor_new(F_HEAD5);
            if (!h) continue;
            h->variant = i;
#ifndef BOSS5_HP
#define BOSS5_HP 127                        /* test builds: less */
#endif
            h->hp = h->last_hp = eb.hp[i] = BOSS5_HP;
            eb.head[i] = h;
        }
        {
            Actor *n = actor_new(F_NECK5);
            if (n) { n->hp = n->last_hp = 1; n->x = (s32)eb.ax << 8; n->y = (s32)eb.ay << 8; }
        }
        return;
    case 1:                                     /* loading while the rise starts below the screen */
        eb.ay++;
        if (load_step()) { eb.sub = 2; boss5_layer = 1; place(); }
        return;
    case 2:                                     /* the rise (0x2B920): 258 updates in all */
        if (++eb.ay >= 104) { eb.ay = 104; eb.sub = 3; eb.bob = 0; eb.bob_t = 0; eb.bob_v = 512; }
        place();
        return;
    case 3:
        anchor_update();
        neck_update(0);
        neck_update(1);
        push_wall();
        place();
        return;
    case 4:                                     /* the second head's end: collapse, 240 updates */
        if (!(eb.timer % 3))
        {
            /* Two explosions every 3 updates, one from each table (0x2C470 /
             * 0x2C352), at most 7 at once (the arcade ~13: 8 or more drop
             * this window under 80 frames drawn per 100). */
            u16 k = eb.timer / 3, n = actor_count(F_BURST);
            sound_post(SND_BOOM);
#ifndef BURST_CAP
#define BURST_CAP 7
#endif
            if (n < BURST_CAP) effect(F_BURST, eb.ax + boom_b[k & 7][0], eb.ay + boom_b[k & 7][1], 4);
            if (n + 1 < BURST_CAP) effect(F_BURST, eb.ax - 160 + boom_a[k & 7][0], eb.ay + 80 + boom_a[k & 7][1], 4);
        }
        if (eb.timer < 120) { eb.ax++; eb.ay--; }
        place();
        if (--eb.timer) return;
        eb.sub = 5;
        award(12); award(12);                   /* 10,000 */
        boss5_dead = 1;
        swell_y = 0;                            /* no shake after the end */
        boss5_layer = 0;
        stage_far_pic(NULL, 0, 0);              /* plane B the far layer again (empty here): the results' text */
        {
            u16 i;                              /* the explosions end with it (the results stop the actors) */
            for (i = pool_lo[C_EFFECT]; i < pool_lo[C_EFFECT + 1]; i++) if (actors[i].fam == F_BURST) actor_free(actors + i);
        }
        game_stage_clear();                     /* FF83C3 = 2: results, no exit walk */
        return;
    }
}

/* ---- Heads (variants 0 = A on the sprite neck, 1 = B on the far one).
 * Chains: facing * 3 + mouth. */
void head5_update(Actor *a)
{
    u16 i = a->variant, k = i ? 0 : 1;
    Neck *n = &eb.nk[k];
    s16 hx, hy;
    if (!a->state) { a->state = 2; a->facing = 2; a->sub = 0; actor_anim(a, 6); }
    head_pos(i, &hx, &hy);
    a->x = (s32)hx << 8;
    a->y = (s32)hy << 8;
    if (eb.sub < 3) { a->active = 1; return; }
    if (a->phase == 9)                          /* dying: explosions every 7 updates for 180 */
    {
        if (!(a->timer % 7))
        {
            sound_post(SND_BOOM);
            if (actor_count(F_BURST) < 6) effect(F_BURST, hx + boom_a[(a->timer / 7) & 7][0], hy + boom_a[(a->timer / 7) & 7][1], 4);
        }
        if (--a->timer) return;
        award(12); award(12);                   /* 10,000 */
        eb.head[i] = NULL;
        actor_free(a);
        return;
    }
#ifdef DIE_AT
    { static u16 t[2]; if (++t[i] == DIE_AT + 300 * i && a->hp) a->hp = 0; }   /* test builds: head i dies DIE_AT + 300 i updates after it is up */
#endif
    if (a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        eb.hp[i] = a->hp;
        if (!a->hp)
        {
            a->active = 2;
            n->flag = 0;
            if (eb.dead == 0)
            {
                eb.dead = 1;
                a->phase = 9;
                a->timer = 180;
                return;
            }
            /* The second head (6.7): the clock stops, 14,000 gold, the collapse. */
            eb.dead = 2;
            boss5_dead = 1;
            encounter_clear = 0;
            a->x = (s32)(CAM_X + 288) << 8;
            drop(a, 9);
            eb.head[i] = NULL;
            actor_free(a);
            eb.sub = 4;
            eb.timer = 240;
            return;
        }
        sound_post(0x99);
        a->flags |= DEFLECT;                    /* 48 updates: shots stop on it */
        a->t2 = 48;
        a->active = 1;
    }
    if (a->t2 && !--a->t2) a->flags &= ~DEFLECT;
    a->active = 1;
    /* Facing every 64 updates while idle, and at a breath. */
    if (n->state == NK_IDLE && !(game_frame & 63)) a->facing = head_facing(hx, hy);
    /* Breath (0x2C108): mouth 1, 2 at 10-update steps; at 2 the fire; 2 for
     * 20 updates, 1 for 10, then closed: 50 updates. */
    if (n->flag)
    {
        if (!a->sub) { a->sub = 1; a->timer = 0; a->facing = head_facing(hx, hy); }
        a->timer++;
        a->v[0] = a->timer < 10 ? 0 : a->timer < 20 ? 1 : a->timer < 40 ? 2 : a->timer < 50 ? 1 : 0;
        if (a->timer == 20)
        {
            sound_post(0x8D);
            eb.breath = 1;
            flames(n->action, a->facing, hx + mouth_off[a->facing][0], hy + mouth_off[a->facing][1]);
        }
        if (a->timer >= 50) { n->flag = 0; a->sub = 0; a->v[0] = 0; }
    }
    else a->v[0] = n->action == 3 && n->state != NK_IDLE ? mouth_of_pose[n->pose] : 0;
    actor_anim(a, a->facing * 3 + a->v[0]);
}

/* ---- The sprite neck's box: at the anchor; its first hit leaves it a
 * shield for the rest of the fight (6.6). */
void neck5_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 1; }
    a->x = (s32)eb.ax << 8;
    a->y = (s32)eb.ay << 8;
    if (a->hp != a->last_hp) { a->flags |= DEFLECT; a->hp = a->last_hp = 1; }
    a->active = eb.sub >= 3 && eb.sub < 4 ? 1 : 0;
    if (eb.sub >= 4) actor_free(a);
}

const s16 *neck5_boxes(void) { return neck_box; }

/* ---- Fire (class 10 kind 20): hidden, then 24 updates burning (hurts).
 * A group's flames share one VRAM slot and so one pose, stepped every 4
 * updates by the frame counter (the arcade: each flame its own 7 x 4
 * nodes from its start; the stagger of starts and ends is kept). */
void flame5_update(Actor *a)
{
    u16 g = a->fam - F_FLAME5;
    if (a->state == 0)
    {
        if (--a->timer) return;
        a->state = 2;
        a->active = 1;
        a->timer = 24;
        if (a->sub) eb.breath = 0;
    }
    a->pose = g * 7 + eb.fpose;
    if (!--a->timer) actor_free(a);
}

/* ---- The energy bars (6.6): ENEMY on row 2, head A's bar on row 2, head
 * B's on row 3, (energy - 1) >> 3 cells; as sprites (plane B moves). */
u16 boss5_draw(VDPSprite *sat, u16 first, u16 max)
{
    u16 n = 0, i, line = 1 << 13;
    if (eb.sub < 2 || eb.sub >= 4) return 0;
#define SPR(xx, yy, w, t) do { if (n < max) { VDPSprite *s = sat + first + n++; s->y = 128 + (yy); s->size = ((w) - 1) << 2; \
    s->attribut = 0x8000 | line | (t); s->x = 128 + (xx); s->link = first + n; } } while (0)
    SPR(8, 16, 4, BAR_VRAM);
    SPR(40, 16, 1, BAR_VRAM + 4);
    for (i = 0; i < 2; i++)
    {
        s16 cells = eb.hp[i] ? (eb.hp[i] - 1) >> 3 : 0, x = 56;
        while (cells > 0)
        {
            u16 w = cells > 4 ? 4 : cells;
            SPR(x, 16 + 8 * i, w, BAR_VRAM + 5);
            x += w * 8;
            cells -= w;
        }
    }
#undef SPR
    return n;
}
