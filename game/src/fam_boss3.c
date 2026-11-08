/* Stage 3's boss (specs/stage3.md 6): the flying sorcerer (class 18 kind 2),
 * his magic bolt (class 10 kind 11), the spirit serpents (class 4 kind 13)
 * and the cast / summon glows (class 12 kinds 18, 17). Native y = arcade y -
 * 256 (waypoints y 352..464 -> 96..208).
 *
 * Approximations (M1_NOTES): the fade-in palette job is a plain appearance
 * after the dialogue; the hit and death flashes (white) are blinks; the ending figure
 * (class 16 kind 2) and the arena object (class 6 kind 6 variant 2) are not
 * shown - the clear follows the death after the exit timer's 200 updates;
 * the serpents' sparkle (class 12 kind 5) is not drawn. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"
#include "hud.h"
#include "game.h"
#include "serpent_paths.h"

#ifdef BOSS3_HP
#define BOSS_HP BOSS3_HP            /* test builds */
#else
#define BOSS_HP 80
#endif
#define ARENA_CAM 5760
#define SND_SUMMON 0x93
#define SND_SERPENTS 0x94
#define SND_BOSS_DEATH 0x95
#define SND_BOSS3_MUSIC 0x06
#define TEXT_ROW 4
#define TEXT_COL 2

extern u8 scene_lock;
u8 boss3_dead;              /* FF83CF: the serpents burst, the storm clears */
static u8 boss3_seen;       /* FF838B: the dialogue has been shown */
static Actor *boss3;

#include "dirmath.h"

/* ---- Paths (0x279AA / 0x27A14): x, y - 256, phase, direction. */
typedef struct { s16 x, y; u8 phase, dir; } Way;
static const Way path_a[17] = {
    { 5952, 208, 0, 0 }, { 6024, 192, 0, 32 }, { 5952, 168, 2, 80 }, { 5880, 192, 4, 96 }, { 5824, 160, 0, 96 },
    { 5952, 112, 2, 56 }, { 6080, 160, 4, 32 }, { 6128, 96, 0, 48 }, { 6096, 192, 0, 0 }, { 6128, 96, 10, 48 },
    { 6080, 160, 0, 0 }, { 5952, 112, 2, 72 }, { 5824, 160, 4, 96 }, { 5880, 192, 0, 16 }, { 5952, 168, 2, 48 },
    { 6024, 192, 4, 32 }, { 5952, 208, 0, 112 },
};

static Way way(u16 path, u16 i)
{
    Way w = path_a[i];
    if (path)                                       /* path B: path A mirrored about x 5952 */
    {
        w.x = 2 * 5952 - w.x;
        w.dir = (128 - w.dir) & 127;
        if (w.phase == 10) w.phase = 8;
    }
    return w;
}

static const s16 speeds[4] = { 384, 512, 640, 768 };
static const u8 bolt_reload[16] = { 100, 90, 80, 65, 60, 50, 50, 45, 30, 30, 25, 20, 15, 10, 10, 5 };
static const u16 bolt_bits[16] = { 0x2453, 0x58A4, 0x4595, 0x2A95, 0x6A95, 0x6A95, 0x7FB5, 0xEEDF,
                                   0xEEDF, 0xFDBF, 0xFFBD, 0xFFDF, 0xF7FF, 0xFFFB, 0xBFFF, 0xFFF7 };
static const s8 boom_off[8][2] = { { 0, 0 }, { 24, 16 }, { -24, -8 }, { 8, -8 }, { -8, 16 }, { 8, -24 }, { -24, 8 }, { 6, -4 } };
static const char *const boss3_text[4] = { "I HAVE BEEN WAITING FOR YOU,", "YOUNG MAN.", "NOW HAVE A TASTE OF", "REAL MAGIC." };

/* Chains (tools/pack_stage3.sh "boss3"): glide 0-7, summon 8-15, cast 16-23 by 8-way direction. */
enum { BC_GLIDE = 0, BC_SUMMON = 8, BC_CAST = 16 };
/* Fields: v[0] heading (128), v[1] speed index, v[2] waypoint | path << 8, v[3] bob phase,
 * v[4] attack timer, t2 phase timer, sub attack / summon step, flags: shown 8-way pose. */
enum { BS_HIDDEN, BS_TEXT, BS_HOLD, BS_APPEAR, BS_FIGHT, BS_ATTACK, BS_SUMMON, BS_DEATH, BS_EXIT };

static u16 dir8_to_player(Actor *a)
{
    return ((angle128((player.x >> 8) - AX(a), (player.y >> 8) + 24 - AY(a)) + 16) >> 5) & 7;
}

static void face(Actor *a, u16 base, bool force)
{
    u16 d = dir8_to_player(a);
    if (force || d != a->flags) { a->flags = d; actor_anim(a, base + d); }
}

static void glow(Actor *a, u16 kind)
{
    Actor *g = actor_new(F_GLOW);
    if (!g) return;
    g->variant = kind;                  /* 0: cast, 1: summon */
    g->facing = a->flags;
    g->x = a->x; g->y = a->y;
    g->link = (a - actors) + 1;
}

static void serpents(Actor *a, u16 base)
{
    u16 i, k;
    sound_post(SND_SERPENTS);
    for (i = 0; i < 4; i++)
    {
        u16 path = base + (rnd() & 4) + i;
        Actor *lead = NULL;
        for (k = 0; k < 6; k++)         /* head, 4 body, tail */
        {
            Actor *s = actor_new(k ? F_SERPENTB : F_SERPENT);
            if (!s) break;
            s->x = a->x; s->y = a->y;
            s->variant = k == 0 ? 0 : k == 5 ? 4 : 2;
            s->v[0] = path;
            s->v[1] = 1 + 8 * k;        /* start delay: head 1, then 8 apart (tail 40) */
            s->link = lead ? (lead - actors) + 1 : 0;
            lead = s;
        }
    }
}

void boss3_update(Actor *a)
{
    s16 px = player.x >> 8;
    switch (a->state)
    {
    case BS_HIDDEN:                         /* intangible and not drawn until the arena (6.1) */
        a->active = 0;
        boss3 = a;
        if (CAM_X < ARENA_CAM) return;
        encounter_clear = 1;                /* FF83D0, FF83CE */
        fish_end = 1;
        player.cam_min_x = ARENA_CAM;
        a->hp = a->last_hp = BOSS_HP;
        a->v[2] = (rnd() & 1) << 8;
        a->v[4] = 60;
        a->v[0] = 0;
        if (boss3_seen) { a->state = BS_APPEAR; a->timer = 44; return; }
        boss3_seen = 1;
        sound_post(SND_BOSS3_MUSIC);
        scene_lock = 1;
        a->state = BS_TEXT;
        a->sub = 0;
        a->v[1] = 0;
        a->timer = 30;
        return;
    case BS_TEXT:                           /* typed, one sound 0x2F a letter (cut 3: the HUD's rows) */
        if (--a->timer) return;
        a->timer = 3;
        if (a->sub == 4) { a->state = BS_HOLD; a->timer = 96; return; }
        {
            const char *t = boss3_text[a->sub];
            a->v[1]++;
            hud_text(t, a->v[1], TEXT_COL, TEXT_ROW + a->sub * 2);
            if (t[a->v[1] - 1] != ' ') sound_post(0x2F);
            if (!t[a->v[1]]) { a->sub++; a->v[1] = 0; a->timer = 12; }
        }
        return;
    case BS_HOLD:
        if (--a->timer) return;
        { u16 r; for (r = 0; r < 4; r++) hud_message(NULL, TEXT_ROW + r * 2); }
        scene_lock = 0;
        a->state = BS_APPEAR;
        a->timer = 44;
        return;
    case BS_APPEAR:                         /* 8 + 36 updates fading in (drawn, intangible) */
        a->active = 2;
        if (a->timer == 44) { a->v[1] = 0; face(a, BC_GLIDE, TRUE); }
        actor_step(a);
        if (--a->timer) return;
        a->active = 1;
        a->state = BS_FIGHT;
        a->phase = 0;
        a->v[2] &= 0xFF00;
        hud_boss(BOSS_HP);
        return;
    }
    if (a->state == BS_DEATH)
    {
        actor_step(a);
        /* The death flash (palette job 0xE6B20: 7 white / normal toggles of
         * 3 updates) as a blink, as the hit flash below. */
        a->active = 256 - a->timer < 21 && (((256 - a->timer) / 3) & 1) == 0 ? 0 : 2;
        if (!(a->timer & 15))
        {
            const s8 *o = boom_off[(a->timer >> 4) & 7];
            effect(F_BURST, AX(a) + o[0], AY(a) + o[1], 4);
            sound_post(SND_EXPLODE);
        }
        if (--a->timer) return;
        award(12);                          /* 5000 */
        drop(a, 7);                         /* 15 gold pickups */
        a->state = BS_EXIT;
        a->active = 0;
        a->timer = 200;
        return;
    }
    if (a->state == BS_EXIT)
    {
        if (a->timer == 200)
        {
            /* The ending figure (class 16 kind 2, 6.13): off screen right, waiting. */
            Actor *f = actor_new(F_FIGURE);
            if (f) { f->x = (s32)(CAM_X + 416) << 8; f->y = (s32)95 << 8; clear_wait = 1; }
        }
        if (--a->timer) return;
        boss3 = NULL;
        actor_free(a);
        encounter_clear = 0;
        game_stage_clear();
        return;
    }
#ifdef HIT_AT
    if (game_frame == HIT_AT && a->hp > 1) a->hp--;   /* test builds: a hit at a set frame */
#endif
#ifdef DIE_AT
    if (game_frame == DIE_AT) a->hp = 0;             /* test builds: its death at a set frame */
#endif
    /* Hit (6.5): 40 updates invulnerable. */
    if (a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        hud_boss(a->hp);
        if (!a->hp)
        {
            scene_lock = 1;
            boss3_dead = 1;
            sound_post(SND_BOSS_DEATH);
            hud_boss(-1);
            a->state = BS_DEATH;
            a->active = 2;
            a->timer = 256;
            return;
        }
        sound_post(SND_ENEMY_HIT);
        a->active = 2;
        a->v[5] = 40;
    }
    if (a->v[5] && !--a->v[5]) a->active = 1;
    /* The hit flash (27E96, palette job 0xE6B20: 13 white / normal toggles
     * of 3 updates): its sprite line is fixed (cut 4), so it blinks while
     * not hittable (as Bavmorda's, src/fam_bav.c). */
    if (a->v[5] > 1) a->active = ((a->v[5] / 3) & 1) ? 0 : 2;
    actor_step(a);
    /* Bob (0x28050): +-4 px, 128 updates. */
    {
        u16 t = ++a->v[3] & 127;
        s16 d = t < 64 ? (t < 32 ? 16 : -16) : (t < 96 ? -16 : 16);
        a->y += d * 4;
    }
    if (a->state == BS_ATTACK)              /* 6.8: glow, cast, bolt, resume */
    {
        if (--a->t2) return;
        if (a->sub == 0)
        {
            Actor *b = actor_new(F_BOLT3);
            if (b)
            {
                u16 d = ((angle128(px - AX(a), (player.y >> 8) + 24 - AY(a)) + 2) >> 2) & 31;
                dir_v(d << 2, 1280, &b->vx, &b->vy);
                b->x = a->x; b->y = a->y;
            }
            a->sub = 1;
            a->t2 = 20;
            return;
        }
        a->state = BS_FIGHT;
        face(a, BC_GLIDE, TRUE);
        return;
    }
    if (a->state == BS_SUMMON)              /* 6.7 phases 6/8/10: hover, glow, serpents */
    {
        if (--a->t2) return;
        if (a->sub == 0) { glow(a, 1); a->sub = 1; a->t2 = 28; return; }
        if (a->sub == 1) { serpents(a, a->phase == 6 ? 0 : a->phase == 8 ? 8 : 16); a->sub = 2; a->t2 = 120; return; }
        a->state = BS_FIGHT;
        a->phase = 0;
        face(a, BC_GLIDE, TRUE);
        return;
    }
    /* Fight: steer one step every second update toward the waypoint. */
    {
        u16 path = a->v[2] >> 8, i = a->v[2] & 0xFF;
        Way w = way(path, i);
        s16 vx, vy;
        if (!(game_frame & 1))
        {
            u16 want = angle128(w.x - AX(a), w.y - AY(a));
            s16 diff = (s16)((want - a->v[0]) & 127);
            if (diff) a->v[0] = (a->v[0] + (diff < 64 ? 1 : 127)) & 127;
        }
        dir_v(a->v[0], speeds[a->v[1]], &vx, &vy);
        a->x += vx;
        a->y += vy;
        if ((u16)(AX(a) - w.x + 10) <= 20 && (u16)(AY(a) - w.y + 14) <= 28)
        {
            a->phase = w.phase;
            a->v[0] = w.dir;
            a->t2 = 0;
            if (++i >= 17)
            {
                a->v[2] = ((rnd() + game_frame) & 1) << 8;
                a->phase = 6;
            }
            else a->v[2] = (path << 8) | i;
        }
        if (!(game_frame & 15)) face(a, BC_GLIDE, FALSE);
        switch (a->phase)
        {
        case 0:                             /* cruise: the attack roll */
            if (!--a->v[4])
            {
                u16 r = rank >> 12;
                a->v[4] = bolt_reload[r];
                if (bolt_bits[r] & (1 << (rnd() & 15)))
                {
                    glow(a, 0);
                    face(a, BC_CAST, TRUE);
                    a->state = BS_ATTACK;
                    a->sub = 0;
                    a->t2 = 20;
                }
            }
            break;
        case 2:                             /* speed up every 16 updates */
            if (++a->t2 >= 16) { a->t2 = 0; if (a->v[1] < 3) a->v[1]++; }
            break;
        case 4:                             /* slow down every 14 */
            if (++a->t2 >= 14) { a->t2 = 0; if (a->v[1]) a->v[1]--; }
            break;
        case 6: case 8: case 10:
            sound_post(SND_SUMMON);
            face(a, BC_SUMMON, TRUE);
            a->state = BS_SUMMON;
            a->sub = 0;
            a->t2 = 30;
            break;
        }
    }
}

void boss3_reset(void)
{
    boss3 = NULL;
    boss3_dead = 0;
    clear_wait = 0;
}

void boss3_new_game(void) { boss3_seen = 0; }

/* ---- The bolt (class 10 kind 11): 5 px an update in 32 directions. */
void bolt3_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 1; actor_anim(a, 0); return; }
    actor_step(a);
    a->x += a->vx;
    a->y += a->vy;
    if ((game_frame & 3) && terrain_floor(AX(a), AY(a), &(s16){0})) { actor_free(a); return; }
    if ((u16)(AX(a) - CAM_X + 32) > 384 || (u16)(AY(a) - CAM_Y + 32) > 320) actor_free(a);
}

/* ---- Glows (class 12 kinds 18, 17): cast 20 + 10 updates; summon 18 + 120 + 2. */
void glow_update(Actor *a)
{
    Actor *b = a->link ? actors + a->link - 1 : NULL;
    if (!a->state)
    {
        a->state = 2;
        a->active = 2;
        if (a->variant == 0) { a->timer = 30; actor_anim(a, a->facing & 7); }
        else { a->timer = 140; actor_anim(a, 16); }
        return;
    }
    actor_step(a);
    if (b && b->fam == F_BOSS3) { a->x = b->x; a->y = b->y; }
    if (a->variant == 0 && a->timer == 10) actor_anim(a, 8 + (a->facing & 7));
    if (a->variant == 1 && a->timer == 60) actor_anim(a, 17);
    if (!--a->timer) actor_free(a);
}

/* ---- Spirit serpents (6.9): six segments flying one path, each after its
 * delay; only the head takes shots (bodies and tail are F_SERPENTB, enemy
 * projectiles: they hurt on contact). v[0] path, v[1] start delay, v[2]
 * position in the path (bytes), v[3] updates left on the leg, v[4] the leg's
 * direction (16), v[5] burst delay. */
void serpent_update(Actor *a)
{
    Actor *lead = a->link ? actors + a->link - 1 : NULL;
    if (!a->state)
    {
        a->state = 2;
        a->active = 0;
        a->v[2] = 0;
        a->v[3] = 0;
        if (a->fam == F_SERPENT) a->hp = a->last_hp = 1;
        return;
    }
    if (a->state == 4)                      /* bursting, head to tail, 6 updates apart */
    {
        if (lead && (lead->fam == F_SERPENT || lead->fam == F_SERPENTB) && lead->state == 4) return;
        if (--a->v[5]) return;
        effect(F_BURST, AX(a), AY(a), 4);
        actor_free(a);
        return;
    }
    if (boss3_dead || (a->fam == F_SERPENT && a->hp != a->last_hp)
        || (a->fam == F_SERPENTB && (!lead || (lead->fam != F_SERPENT && lead->fam != F_SERPENTB) || lead->state == 4)))
    {
        a->state = 4;
        a->active = 2;
        a->v[5] = 6;
        return;
    }
    if (a->v[1]) { if (!--a->v[1]) a->active = a->fam == F_SERPENT ? 1 : 1; else return; }
    if (!a->v[3])
    {
        const u8 *p = serpent_paths + serpent_path_at[a->v[0]] + a->v[2];
        if (p[1] == 0xFF) { actor_free(a); return; }
        a->v[3] = p[0];
        a->v[4] = p[1];
        a->v[2] += 2;
        actor_anim(a, a->variant == 2 ? 32 : (a->variant == 4 ? 16 : 0) + a->v[4]);
    }
    a->v[3]--;
    actor_step(a);
    {
        s16 vx, vy;
        dir_v(a->v[4] << 3, 512, &vx, &vy);
        a->x += vx;
        a->y += vy;
    }
}

/* ---- The ending figure (6.13): after the clear starts it runs in 2 px an
 * update to camera x + 240, holds 40, transforms 24, stands as Fin Raziel
 * 120; then the results (FF83C3 = 2). Chains: 0 run, 1 held, 2 burst, 3 her. */
u8 clear_wait;              /* the clear's walk-out waits for the figure */

void figure_update(Actor *a)
{
    switch (a->state)
    {
    case 0:
        a->state = 2;
        a->active = 2;
        a->facing = 1;
        actor_anim(a, 0);
        return;
    case 2:                                 /* until the clear */
        if (game_mode != GM_CLEAR) return;
        actor_step(a);
        a->x -= 512;
        if (AX(a) <= CAM_X + 240) { a->state = 4; a->timer = 40; actor_anim(a, 1); }
        return;
    case 4:
        actor_step(a);
        if (--a->timer) return;
        if (!a->sub) { a->sub = 1; a->timer = 24; actor_anim(a, 2); return; }
        if (a->sub == 1) { a->sub = 2; a->timer = 120; actor_anim(a, 3); return; }
        clear_wait = 0;
        a->state = 6;
        return;
    }
}
