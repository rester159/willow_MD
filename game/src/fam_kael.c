/* General Kael (specs/stage6_kael.md, stage6.md 6.1-6.3): stage 6's
 * mid-boss on the area-4 bridge, and his sword waves (class 10 kind 13).
 *
 * Area 4's native coordinates: y = arcade y - 3072, the camera fixed at
 * (0, 0); the bridge floor y 80 (Kael's origin 88), the right-hand ledge
 * 207. Chains (tools/pack_stage6.sh, tables 0x1ECF6 + 4n, two facings
 * each): K_STAND .. K_COLLAPSE below. v[0] vy, v[1] vx, v[2] invulnerability
 * (+3E), v[3] parry cooldown (+47), v[4] target column (+32), v[5] flags
 * (+40 slash, +41 airborne, +42 parrying); t2 phase-14 no-ground window. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "hud.h"
#include "sound.h"
#include "shot.h"
#include "game.h"

#define FACE(a) ((a)->facing & 1)
#define DIR(a) (FACE(a) ? -1 : 1)
enum { K_STAND, K_WALK, K_WIND, K_SLASH, K_GUARD, K_PUSH, K_JUMP, K_AIRWIND, K_AIRSLASH, K_HOP, K_PLUNGE, K_FLASH, K_COLLAPSE };
enum { F_SLASHING = 1, F_AIR = 2, F_PARRY = 4 };
#define FLOOR_Y 88
#define FOOT (-8)

extern u8 kael_beaten;              /* FF83E7 (src/fam_stage6.c: the exit door) */
static u8 kael_talked;              /* FF838A: the dialogue was shown */
static s16 kael_vy;                 /* +3A: the waves of an air slash follow it */
static s16 push_v, push_d;          /* +44 / +3C during the parry push */

static const char *const kael_text[3] = {
    "THE TIME HAS FINALLY COME TO FIGHT.", "I HAVE BEEN WAITING.", "THE QUEEN WILL ELIMINATE THE BABY SOON.",
};
/* Decisions (3.1). */
static const u8 near_tab[16] = { 6, 14, 4, 14, 6, 6, 6, 6, 6, 6, 6, 6, 4, 14, 6, 14 };   /* 0x1EBE4 */
static const u8 mid_tab[16] = { 6, 14, 4, 14, 6, 6, 6, 4, 6, 6, 6, 6, 4, 14, 4, 14 };    /* 0x1EC36 */
static const u8 rows[5][16] = {                                                         /* 0x1EC82 by column */
    { 0x42, 0x6E, 0x42, 0x8E, 0x62, 0x8E, 0x42, 0x04, 0x62, 0x42, 0x8E, 0x42, 0x6E, 0x04, 0x8E, 0x42 },
    { 0xAE, 0x02, 0x82, 0x04, 0x02, 0x62, 0x8E, 0x62, 0x62, 0x8E, 0x02, 0x62, 0x04, 0xAE, 0x02, 0x82 },
    { 0x02, 0xAE, 0x42, 0x82, 0x0E, 0x42, 0x82, 0xA2, 0x42, 0x82, 0xA2, 0x02, 0x82, 0xAE, 0x0E, 0x42 },
    { 0x0E, 0x62, 0xA2, 0x42, 0xA2, 0x62, 0x4E, 0x04, 0x62, 0x4E, 0x0E, 0x62, 0xA2, 0x42, 0x04, 0xA2 },
    { 0x82, 0x08, 0x82, 0x08, 0x6E, 0x82, 0x08, 0x82, 0x08, 0x6E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08 },
};
static const s16 leap_x[8] = { 32, 96, 160, 224, 288, 352, 416, 480 };                  /* 0x1E4A8 */
/* Parry odds by rank row (0x1EA20): plain swing, /16. */
static const u8 parry_odds[8] = { 2, 4, 7, 10, 11, 12, 13, 14 };

static void chain(Actor *a, u16 k) { actor_anim(a, k * 2 + FACE(a)); }
static void face_player(Actor *a) { a->facing = (player.x >> 8) < AX(a); }
static u16 column(Actor *a) { s16 x = AX(a) - CAM_X; return x < 0 ? 0 : ((x >> 5) & 14); }   /* +31 (0x1ECE2) */

/* Re-face every n updates (the arcade's (FF8019 + slot) & (n - 1)). */
static void reface(Actor *a, u16 n, u16 k)
{
    u16 was = a->facing;
    if ((game_frame + (a - actors)) & (n - 1)) return;
    face_player(a);
    if (a->facing != was) chain(a, k);
}

static void set_phase(Actor *a, u16 p) { a->phase = p; a->sub = 0; a->timer = 0; }

static void decide(Actor *a)
{
    s16 dx = AX(a) - (s16)(player.x >> 8);
    u16 d = column(a), p;
    if (dx >= -112 && dx < 112)
    {
        p = d >= 10 ? 8 : (dx >= -80 && dx < 80 ? near_tab : mid_tab)[rnd() & 15];
        if (p == 14) a->v[4] = 10;
    }
    else
    {
        u8 b = rows[d >= 10 ? 4 : d >> 1][rnd() & 15];
        p = b & 14;
        a->v[4] = b >> 4;
    }
    set_phase(a, p);
}

/* A slash (5.1): sound 0x91 and a sword wave at +-40. */
static void slash(Actor *a)
{
    Actor *w;
    sound_post(0x91);
    w = actor_new(F_KSLASH);
    if (!w) return;
    w->facing = FACE(a);
    w->x = a->x + ((s32)(FACE(a) ? -40 : 40) << 8);
    w->y = a->y;
}

/* The air slash (phases 8 and 14, +5): when the player is level within 120 px. */
static void air_slash(Actor *a, u16 wind)
{
    s16 dx = (s16)(player.x >> 8) - AX(a), dy = (s16)(player.y >> 8) - AY(a);
    switch (a->t2 >> 8)
    {
    case 0:
        if (a->phase == 14) { u16 was = a->facing; face_player(a); if (was != a->facing) chain(a, K_JUMP); }
        if (dy > -24 && dy < 24 && dx > -120 && dx < 120)
        {
            face_player(a);
            chain(a, K_AIRWIND);
            a->t2 = (a->t2 & 0xFF) | (2 << 8);
            a->timer = wind;
        }
        return;
    case 2:
        if (--a->timer) return;
        slash(a);
        chain(a, K_AIRSLASH);
        a->t2 = (a->t2 & 0xFF) | (4 << 8);
        return;
    }
}

static bool landed(Actor *a)
{
    return a->v[0] <= 0 && actor_ground(a, 0, FOOT);
}

/* The parry scan (4): Madmartigan's swing near Kael while both face each other. */
static bool parry(Actor *a)
{
    s16 dx = (s16)(player.x >> 8) - AX(a), px, py;
    bool pleft = player.facing & 1;
    if (a->v[3]) { a->v[3]--; return FALSE; }
    if (a->v[5]) return FALSE;
    face_player(a);
    if (pleft != (dx > 0) || !shot_sword_live()) return FALSE;
    a->v[3] = 8;
    if ((rnd() & 15) >= parry_odds[(rank >> 13) & 7]) return FALSE;
    px = (s16)(player.x >> 8) + (pleft ? -80 : 68);
    py = (s16)(player.y >> 8) + 16;
    if (px - AX(a) > 40 || px - AX(a) < -40 || py - AY(a) > 32 || py - AY(a) < -32) return FALSE;
    set_phase(a, 16);
    return TRUE;
}

static void fight(Actor *a)
{
    s16 dx;
    switch (a->phase)
    {
    case 0:                                 /* the opening approach: 13 updates */
        if (!a->timer) { a->timer = 13; face_player(a); chain(a, K_WALK); }
        reface(a, 8, K_WALK);
        a->x += DIR(a) * 640;
        actor_step(a);
        if (!--a->timer) decide(a);
        break;
    case 2:                                 /* walk to the target column */
        if (!a->sub)
        {
            a->sub = 1; a->timer = 16; a->v[5] &= ~(F_SLASHING | F_AIR);
            a->v[1] = a->v[4] > column(a) ? 640 : -640;
            face_player(a); chain(a, K_WALK);
        }
        reface(a, 8, K_WALK);
        a->x += a->v[1];
        actor_step(a);
        if (column(a) == a->v[4]) { decide(a); break; }
        if (!--a->timer)
        {
            dx = AX(a) - (s16)(player.x >> 8);
            if (dx >= -112 && dx < 112) decide(a); else a->timer = 16;
        }
        break;
    case 4:                                 /* stand 16 */
        if (!a->sub) { a->sub = 1; a->timer = 16; a->v[5] &= ~F_AIR; face_player(a); chain(a, K_STAND); }
        reface(a, 4, K_STAND);
        actor_step(a);
        if (!--a->timer) decide(a);
        break;
    case 6:                                 /* ground slash: wind-up 30, slash 16 */
        if (!a->sub) { a->sub = 1; a->timer = 30; kael_vy = 0; a->v[5] &= ~F_AIR; face_player(a); chain(a, K_WIND); }
        actor_step(a);
        if (--a->timer) break;
        if (a->sub == 1) { a->sub = 2; a->timer = 16; a->v[5] |= F_SLASHING; slash(a); chain(a, K_SLASH); break; }
        a->v[5] &= ~F_SLASHING;
        decide(a);
        break;
    case 8:                                 /* ledge jump (only from column 10+) */
        switch (a->sub)
        {
        case 0: a->sub = 2; a->timer = 10; face_player(a); chain(a, K_STAND); break;
        case 2:
            if (--a->timer) break;
            a->v[5] |= F_AIR; a->t2 = 0; a->v[0] = 1792; a->sub = 4; chain(a, K_JUMP);
            break;
        case 4: case 6:
            a->v[0] -= 48; kael_vy = a->v[0];
            a->y += a->v[0];
            air_slash(a, 10);
            if (a->sub == 4) { if (a->v[0] < 0) a->sub = 6; break; }
            if (landed(a)) { sound_post(0x2B); chain(a, K_STAND); a->sub = 8; a->timer = 30; kael_vy = 0; }
            break;
        case 8:
            actor_step(a);
            if (--a->timer) break;
            dx = (s16)(player.x >> 8) - AX(a);
            a->v[1] = (dx > -32 && dx < 32) ? 0 : (dx - 48) * 4;
            set_phase(a, 10);
            break;
        }
        if (a->sub != 8) actor_step(a);
        break;
    case 10:                                /* drop from the ledge */
        if (!a->sub) { a->sub = 2; a->v[5] |= F_SLASHING; face_player(a); a->v[0] = 832; chain(a, K_HOP); }
        reface(a, 8, a->sub == 6 ? K_PLUNGE : K_HOP);
        a->x += a->v[1];
        a->y -= 256;
        a->v[0] -= a->sub == 2 ? 32 : 48;
        a->y += a->v[0];
        kael_vy = a->v[0];
        actor_step(a);
        dx = (s16)(player.x >> 8) - AX(a);
        if (dx > -24 && dx < 24 && (s16)(player.y >> 8) - AY(a) > -32 && (s16)(player.y >> 8) - AY(a) < 32) { set_phase(a, 12); break; }
        if (a->sub == 2) { if (a->v[0] < 0) { a->sub = 4; a->timer = 14; chain(a, K_HOP); } break; }
        if (a->sub == 4) { if (!--a->timer) { a->sub = 6; chain(a, K_PLUNGE); } break; }
        if (landed(a)) { sound_post(0x2B); a->v[5] &= ~F_SLASHING; kael_vy = 0; set_phase(a, 4); }
        break;
    case 12:                                /* hop away when landing near the player */
        if (!a->sub)
        {
            a->sub = 2; a->v[5] |= F_AIR; a->v[0] = 1152;
            a->v[1] = AX(a) < CAM_X + 320 ? 512 : -512;   /* 0x1E372 (the arcade reads a stray word that equals camera X here) */
            chain(a, K_JUMP);
        }
        if (a->sub == 8) { actor_step(a); if (!--a->timer) decide(a); break; }
        reface(a, 8, K_JUMP);
        a->x += a->v[1];
        a->v[0] -= 80;
        a->y += a->v[0];
        kael_vy = a->v[0];
        actor_step(a);
        if (a->sub == 2) { if (a->v[0] < 0) a->sub = 4; break; }
        if (a->sub == 4) { if (AY(a) <= CAM_Y + 128) a->sub = 6; break; }
        if (landed(a)) { a->v[5] &= ~(F_SLASHING | F_AIR); sound_post(0x2B); kael_vy = 0; chain(a, K_STAND); a->sub = 8; a->timer = 30; }
        break;
    case 14:                                /* leap to a column */
        switch (a->sub)
        {
        case 0: a->sub = 2; a->timer = 10; face_player(a); chain(a, K_STAND); actor_step(a); break;
        case 2:
            actor_step(a);
            if (--a->timer) break;
            a->v[5] |= F_AIR; a->t2 = 48;
            a->v[1] = (CAM_X + leap_x[a->v[4] >> 1] - AX(a)) * 4;
            a->v[0] = 1664; a->sub = 4; chain(a, K_JUMP);
            break;
        case 4:
            air_slash(a, 5);
            a->x += a->v[1];
            a->v[0] -= 56;
            a->y += a->v[0];
            kael_vy = a->v[0];
            actor_step(a);
            if (a->t2 & 0xFF) { a->t2--; break; }
            if (landed(a))
            {
                a->v[5] &= ~(F_SLASHING | F_AIR); face_player(a); sound_post(0x2B); kael_vy = 0;
                chain(a, K_STAND); a->sub = 6; a->timer = 30;
            }
            break;
        case 6:
            actor_step(a);
            if (!--a->timer) decide(a);
            break;
        }
        break;
    case 16:                                /* the parry (4) */
        switch (a->sub)
        {
        case 0:
            a->active = 2; a->v[5] = F_PARRY;
            sound_post(0x28);
            {
                Actor *k = actor_new(F_PSPARK6);   /* the spark (12/28, root 0xF2EC: 12 updates, harmless) */
                if (k) { k->x = a->x + ((s32)(FACE(a) ? -16 : 16) << 8); k->y = a->y; }
            }
            face_player(a); chain(a, K_GUARD);
            a->sub = 2; a->timer = 8;
            break;
        case 2:
            if (--a->timer) break;
            gate_freeze = 1;
            push_v = FACE(a) ? -1408 : 1408; push_d = FACE(a) ? 64 : -64;
            chain(a, K_PUSH); a->sub = 4; a->timer = 8;
            break;
        case 4:
            push_v += push_d;
            player.x += push_v;
            if (--a->timer) break;
            gate_freeze = 0;
            a->sub = 6; a->timer = 20;
            break;
        case 6:
            if (--a->timer) break;
            a->active = 1; a->v[5] = 0;
            set_phase(a, 6);
            break;
        }
        actor_step(a);
        break;
    }
    /* The arena (0x1E762): x in [16, 368] of the camera. */
    if (AX(a) < CAM_X + 16) a->x = (s32)(CAM_X + 16) << 8;
    if (AX(a) >= CAM_X + 368) a->x = (s32)(CAM_X + 368) << 8;
}

void kael_update(Actor *a)
{
    switch (a->state)
    {
    case 0:                                 /* init (0x1DE6A) */
        if (kael_beaten) { actor_free(a); return; }
        a->state = 1; a->active = 0;
        a->hp = a->last_hp = 128;
#ifdef KAEL_HP
        a->hp = a->last_hp = KAEL_HP;       /* test builds */
#endif
        a->v[2] = a->v[3] = a->v[5] = 0;
        kael_vy = 0;
        chain(a, K_STAND);
        return;
    case 1:                                 /* wake: the dialogue once (cut 3: the HUD's rows) */
        if (!a->phase)
        {
            if ((u16)(AX(a) - CAM_X + 32) > 448) return;
            face_player(a);
            if (kael_talked) { a->phase = 6; return; }
            kael_talked = 1;
            sound_post(SND_MUSIC_KAEL);
            sound_post(0x01FA);
            scene_lock = 1; midboss_active = 1; encounter_clear = 1; gate_freeze = 1;
            a->phase = 4; a->sub = 0; a->v[4] = 0; a->timer = 30;
            return;
        }
        if (a->phase == 4)
        {
            gate_freeze = 1; scene_lock = 1;
            if (--a->timer) return;
            a->timer = 3;
            if (a->sub == 3)
            {
                hud_message(NULL, 4); hud_message(NULL, 6); hud_message(NULL, 8);
                scene_lock = 0; gate_freeze = 0;   /* FF83D0 (encounter_clear) stays until his defeat */
                a->phase = 6;
                return;
            }
            {
                const char *t = kael_text[a->sub];
                a->v[4]++;
                hud_text(t, a->v[4], 1, 4 + a->sub * 2);
                if (t[a->v[4] - 1] != ' ') sound_post(0x2F);
                if (!t[a->v[4]]) { a->sub++; a->v[4] = 0; a->timer = a->sub == 3 ? 96 : 12; }
            }
            return;
        }
        /* 6: the fight; the bar at 127 (the bar's maximum, 0x1DF2E). */
        a->state = 2; a->active = 1; midboss_active = 1;
        hud_boss(127);
        set_phase(a, 0);
        return;
    case 2:
        fight(a);
        /* Hits (6): 30 updates immune, sound 0xA2; no stagger. */
        if (a->v[2])
        {
            if (!--a->v[2]) { if (a->phase != 16) a->active = 1; }
            else if (a->phase != 16) a->active = (a->v[2] / 5) & 1 ? 0 : 2;   /* the hit flash (job 0xE6E20, 5 updates a step) as a blink */
            return;
        }
        if (a->phase != 16) parry(a);
        if (a->hp != a->last_hp)
        {
            a->last_hp = a->hp;
            hud_boss(a->hp > 127 ? 127 : a->hp);
            if (!a->hp) { scene_lock = 1; a->state = 4; a->active = 2; a->phase = 0; a->timer = 0; return; }
            sound_post(0xA2);
            a->active = 2; a->v[2] = 30;
        }
        return;
    case 4:                                 /* defeat (7) */
        switch (a->phase)
        {
        case 0:
            if (AY(a) > FLOOR_Y + 72 || (a->v[5] & F_AIR)) { chain(a, K_JUMP); if (a->v[0] > 0) a->v[0] = 0; a->phase = 2; return; }
            a->phase = 6; return;
        case 2:
            a->v[0] -= 56; a->y += a->v[0];
            if (landed(a)) a->phase = 6;
            return;
        case 6:
            if (!a->timer) { sound_post(0x95); a->y = (s32)FLOOR_Y << 8; chain(a, K_FLASH); a->timer = 60; }
            actor_step(a);
            if (!--a->timer) { a->phase = 8; chain(a, K_COLLAPSE); a->timer = 60; }
            return;
        case 8:
            actor_step(a);
            if (--a->timer) return;
            add_score(0x10000);             /* score event 0x1812: 10,000 */
            drop(a, 9);                     /* 10 x 500, 4 x 1000, 1 x variant 10 */
            a->phase = 10; a->timer = 180;
            return;
        case 10:
            actor_step(a);
            if (--a->timer) return;
            kael_beaten = 1;
            midboss_active = 0; encounter_clear = 0;
            hud_boss(-1);
            actor_free(a);
            return;
        }
        return;
    }
}

/* Kael's sword wave (5.1): 14 updates forward (768 + 128 a update), then 10
 * fading; it rises and falls with Kael's air slash. Chains: facing right
 * 0-3, left 4-7 (nodes 0x256A4.., 0x256F2..). */
void kslash_update(Actor *a)
{
    s16 base = FACE(a) ? 4 : 0;
    switch (a->state)
    {
    case 0:
        a->state = 2; a->active = 1; a->timer = 14;
        a->vx = FACE(a) ? -768 : 768;
        actor_anim(a, base);
        return;
    case 2:
        a->vx += FACE(a) ? -128 : 128;
        a->x += a->vx;
        a->y += kael_vy;
        if (--a->timer) return;
        a->state = 4; a->timer = 10;
        return;
    case 4:
        actor_anim(a, base + 1 + (10 - a->timer) / 4);
        if (!--a->timer) actor_free(a);
        return;
    }
}

void kael_new_game(void) { kael_talked = 0; }
