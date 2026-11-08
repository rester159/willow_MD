/* Willow's charged magic (specs/magic.md section 1, arcade routines C610,
 * C6C0, 4B0E and CB24 as the specification).
 *
 * Holding attack raises a hold counter (to 127) and the charge meter; a
 * release after 16+ updates starts a second attack whose spell depends on
 * the meter's level at release. Charge sparks (decoration) appear around
 * Willow at a rate that rises with the hold. */
#include <genesis.h>
#include "magic.h"
#include "player.h"
#include "hud.h"
#include "sound.h"
#include "vram.h"
#include "sprite.h"
#include "../data/spark/layout.h"

extern const u8 spark_poses[];
extern const u8 spark_tiles[];
extern const u8 spark_anim[];

Magic magic;
extern u16 game_frame;      /* update counter (the arcade's frame counter for the 0x44 cadence) */

/* Meter increment and cap per magic level (ROM 0x4BFA / 0x4C08). */
static const u16 meter_inc[7] = { 0x11F, 0x14F, 0x16F, 0x17F, 0x17F, 0x17F, 0x17F };
static const u16 meter_cap[7] = { 0x8FFF, 0xA7FF, 0xB7FF, 0xBFFF, 0xBFFF, 0xBFFF, 0xBFFF };
/* Spell tier by (level + 1) >> 3, and the spell per magic level and tier
 * (0xCB08, 0xCB4C). */
static const u8 tier_of[28] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 6, 6 };
static const u8 spell_of[7][8] = {
    { 0, 1, 2, 3, 3, 3, 3, 3 }, { 1, 2, 3, 4, 7, 7, 7, 7 }, { 2, 3, 4, 5, 7, 8, 8, 8 },
    { 3, 4, 5, 6, 7, 8, 9, 9 }, { 4, 5, 6, 7, 8, 9, 10, 10 }, { 5, 6, 7, 8, 9, 10, 11, 11 },
    { 6, 7, 8, 9, 10, 11, 12, 12 }
};
/* Spark cadence by hold / 16 (0xC706). */
static const u8 spark_every[8] = { 10, 6, 5, 4, 3, 2, 2, 1 };
/* Spark offsets from Willow (dx, dy; y up), standing and crouched (0xFB90,
 * 0xFBD0), lifetimes per animation (0xFC60). */
static const s8 spark_at[2][16][2] = {
    { { -2, 20 }, { 2, 18 }, { -4, 16 }, { 4, 14 }, { -8, 12 }, { 8, 10 }, { -10, 8 }, { 10, 6 },
      { -12, 4 }, { 12, 2 }, { 0, 0 }, { -10, -2 }, { 10, -4 }, { -8, -6 }, { 8, -8 }, { -5, -8 } },
    { { -2, 8 }, { 2, 6 }, { -4, 4 }, { 4, 2 }, { -8, 0 }, { 8, -2 }, { -10, -4 }, { 10, -6 },
      { -12, -8 }, { 12, -10 }, { 0, -12 }, { -10, -14 }, { 10, -16 }, { -8, -18 }, { 8, -20 }, { -5, -20 } }
};
static const u8 spark_life[4] = { 9, 16, 7, 24 };

#define SPARKS 16
typedef struct
{
    s16 x, y;               /* world px, y up */
    u8 alive, life, chain, frame, tick, pad;
    u16 shown;
} Spark;
static Spark sparks[SPARKS];
static const u8 *chains[6];         /* sparks 0-3, muzzle flash 4-5 (by facing) */
static u8 spark_hint[SPARK_COUNT];
extern const s16 spark_sat[];
static PoseSet ps_spark = { spark_poses, spark_tiles, spark_anim, SPARK_COUNT, 0, 0, NULL, spark_hint, 0, spark_sat, 0 };
static u16 rng = 0x2D5A;

void magic_init(void)
{
    const u8 *a = spark_anim;
    u16 i;
    memset(sparks, 0, sizeof sparks);
    for (i = 0; i < 6; i++) { chains[i] = a; a += 2 + 2 * a[0]; }
}

static u16 spark_random(void)
{
    rng = rng * 0x6255 + 0x3619;          /* decoration only (fidelity contract) */
    return rng >> 8;
}

static void spawn_spark(void)
{
    u16 i, r;
    for (i = 0; i < SPARKS; i++)
        if (!sparks[i].alive)
        {
            Spark *s = sparks + i;
            const s8 *d = spark_at[player.crouch != 0][spark_random() & 15];
            s->x = (player.x >> 8) + d[0];
            s->y = (player.y >> 8) + d[1];
            r = spark_random() & 3;
            s->alive = 1;
            s->life = spark_life[r];
            s->chain = r;
            s->frame = 0;
            s->tick = chains[r][2];
            return;
        }
}

/* Madmartigan's sword extension (madmartigan.md 5.3): a spark (class 2
 * kind 2, parent the sword) every update, at the sword plus 0xFF28[r & 15]
 * (x mirrored when the sword faces left), with the charge sparks' lifetimes
 * and chains. */
static const s8 sword_spark_at[16][2] = {
    { 2, 0 }, { -2, 8 }, { -2, -8 }, { -6, 0 }, { -10, -8 }, { -14, 8 }, { -14, 0 }, { -10, -16 },
    { -18, 16 }, { -18, -16 }, { -22, 16 }, { -22, -24 }, { -26, -28 }, { 0, 0 }, { 4, 8 }, { 4, -8 }
};

void magic_sword_spark(s16 x, s16 y, bool left)
{
    u16 i, r;
    for (i = 0; i < SPARKS; i++)
        if (!sparks[i].alive)
        {
            Spark *s = sparks + i;
            const s8 *d = sword_spark_at[spark_random() & 15];
            s->x = x + (left ? -d[0] : d[0]);
            s->y = y + d[1];
            r = spark_random() & 3;
            s->alive = 1;
            s->life = spark_life[r];
            s->chain = r;
            s->frame = 0;
            s->tick = chains[r][2];
            return;
        }
}

void magic_sparks(void)
{
    if (!magic.sparks || --magic.spark_cd) return;
    magic.spark_cd = spark_every[magic.hold >> 4];
    spawn_spark();
}

u16 magic_charge(u16 held, u16 pressed)
{
    if (pressed & BUTTON_A)
    {
        magic.sparks = 1;
        magic.spark_cd = 10;
        magic.hold = 0;
        magic.rel = 0;
        return 1;
    }
    if (held & BUTTON_A)
    {
        if (!magic.sparks)
        {
            magic.sparks = 1;
            magic.spark_cd = 10;
            magic.hold = 0;
            magic.rel = 0;
        }
        if (magic.hold != 127)
        {
            if (++magic.hold == 16) sound_post(SND_CHARGE);
        }
        else if (!(game_frame & 15)) sound_post(SND_CHARGED);
        magic_sparks();
        return 0;
    }
    if (magic.hold < 16)
    {
        magic.hold = magic.rel = magic.sparks = 0;
        return 0;
    }
    magic.sparks = 0;
    magic.rel = magic.meter >> 8;
    magic.hold = 0;
    return 1;
}

void magic_meter(void)
{
    u16 lv;
    if (!magic.hold)
    {
        if (magic.active)
        {
            sound_post(SND_CHARGE_END);
            magic.meter = 0;
            magic.active = 0;
            hud_dirty |= HUD_BAR;
        }
        return;
    }
    magic.active = 1;
    lv = player.form ? 0 : magic.level;     /* Madmartigan: +0x11F, cap 0x8FFF (madmartigan.md 2.5) */
    if (magic.meter >= meter_cap[lv]) return;
    magic.meter += meter_inc[lv];
    if (magic.meter >= meter_cap[lv]) { magic.meter = meter_cap[lv]; return; }
    hud_charge(magic.meter >> 8);
}

/* Madmartigan's charge tier (0xDE0A: the same steps, at most 3). */
u16 magic_tier(void)
{
    u16 t = tier_of[(magic.rel + 1) >> 3];
    return t > 3 ? 3 : t;
}

u16 magic_spell(void)
{
    return spell_of[magic.level][tier_of[(magic.rel + 1) >> 3]];
}

void magic_update(void)
{
    u16 i;
    for (i = 0; i < SPARKS; i++)
    {
        Spark *s = sparks + i;
        const u8 *c;
        if (!s->alive) continue;
        if (!--s->life) { s->alive = 0; continue; }
        c = chains[s->chain];
        if (c[2 + 2 * s->frame] != 0xFF && !--s->tick)
        {
            if (++s->frame >= c[0]) s->frame = c[1 + 2 * c[0]] == 0xFF ? c[0] - 1 : c[1 + 2 * c[0]];
            s->tick = c[2 + 2 * s->frame];
        }
    }
}

u16 magic_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y)
{
    u16 i, n = 0;
    for (i = 0; i < SPARKS; i++)
    {
        Spark *s = sparks + i;
        if (!s->alive) continue;
        n += pose_draw(&ps_spark, chains[s->chain][1 + 2 * s->frame], &s->shown, sat, first + n,
                       s->x - view_x, (511 - s->y) - view_y, FALSE, NULL);
    }
    return n;
}

/* Stage 3's colouring of the charge sparks (tools/pack_stage3.sh). */
void spark_art(const u8 *poses, const u8 *tiles, const s16 *sat)
{
    ps_spark.poses = poses; ps_spark.tiles = tiles; ps_spark.sat = sat;
}
