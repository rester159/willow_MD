/* Willow's charged spells 7-12 (specs/magic.md 4; class 8 kinds 3, 7, 4, 6,
 * 5, 8): a pool of their own beside the four normal shots (src/shot.c).
 *
 * 7  piercing bolt: 6 px/frame, power 6 pierce, ends on a wall or spent.
 * 8  rising bolt: forward and curving up, power 8 pierce.
 * 9  flash: the screen white for 3 frames; for ~9 frames every enemy on
 *    screen with energy <= 8 dies (BLAST).
 * 10 time stop: 128 frames, ordinary enemies, enemy shots, moving surfaces
 *    and the clock frozen, Willow not hittable; the arcade's row wave is a
 *    per-8-px-row wave here (the stage's scroll mode).
 * 11 two fire walkers: generators stepping 32 px every 8 frames, six pieces
 *    each (sizes v2, v2, v4, v4, v6, v6), 54 frames, power 10 once.
 * 12 blast: 62 frames, power 20 once in a 160 x 160 box, cancels enemy
 *    projectiles; 8 radial pieces (decoration).
 *
 * Chains (tools/pack_stage1.sh "spell"): 0/1 bolt R/L, 2/3 bolt end, 4/5
 * rising bolt, 6/7 walker v2, 8/9 v4, 10/11 v6, 12 radial moving, 13-20
 * radial pieces by direction. Approximations (M1_NOTES): the flash's
 * "admitted" targets = on screen; time stop's wave per row. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "sound.h"
#include "sprite.h"
#include "hud.h"
#include "s1spr.h"
#include "spell.h"

#define SPELLS 20
enum { K_NONE, K_BOLT, K_RISE, K_FLASH, K_GEN, K_WALKER, K_BLAST, K_RADIAL };
enum { S_RUN = 1, S_END };

typedef struct
{
    s32 x, y;
    s16 s;                   /* rising bolt: its growing speed */
    u8 kind, state, facing, variant, power, step;
    u16 timer, shown;
    Anim anim;
} Spell;

static Spell sp[SPELLS];
static u8 hint_spell[SPELL_POSES];
static PoseSet ps_spell = { spell_poses, spell_tiles, spell_anim, SPELL_POSES, 0, 0, NULL, hint_spell, 0, spell_sat, 0 };

u8 time_stop;                /* FF83D2: frames of time stop left */
static u8 flash_frames;      /* white screen frames left */
static u8 blast_frames;      /* spell 9's BLAST frames left */
static u8 live, blasts;      /* spells alive, blast (12) actors alive: the collision tests' early exit */
static u16 wave;

static const s16 walk_box[3][4] = { { 16, 32, 16, 32 }, { 24, 48, 32, 48 }, { 32, 64, 48, 64 } };   /* 0xF684 */
static const s16 bolt_box[4] = { 10, 20, 20, 40 }, rise_box[4] = { 10, 20, 30, 60 }, blast_box[4] = { 80, 160, 80, 160 };
static const s16 radial_v[8][2] = {                                    /* 0xED2C4 (8.8, y up) */
    { 0, 1708 }, { 1376, 1360 }, { 2048, 0 }, { 1376, -1360 }, { 0, -1708 }, { -1376, -1360 }, { -2048, 0 }, { -1376, 1360 }
};

static Spell *new_spell(u16 kind, s16 x, s16 y, u16 facing)
{
    u16 i;
    for (i = 0; i < SPELLS; i++)
        if (!sp[i].kind)
        {
            Spell *s = sp + i;
            memset(s, 0, sizeof *s);
            s->kind = kind;
            s->state = S_RUN;
            live++;
            if (kind == K_BLAST) blasts++;
            s->x = (s32)x << 8;
            s->y = (s32)y << 8;
            s->facing = facing;
            return s;
        }
    return NULL;
}

static void chain(Spell *s, u16 c) { anim_start(&s->anim, poseset_chain(&ps_spell, c)); }

void spell_init(void)
{
    memset(sp, 0, sizeof sp);
    time_stop = flash_frames = blast_frames = 0;
}

/* The release of a charged attack: spell 7..12 at (x, y). */
void spell_cast(u16 spell, s16 x, s16 y, u16 facing)
{
    Spell *s;
    switch (spell)
    {
    case 7:
        if (!(s = new_spell(K_BOLT, x, y, facing))) return;
        s->power = 6; chain(s, facing); sound_post(0x21);
        break;
    case 8:
        if (!(s = new_spell(K_RISE, x, y, facing))) return;
        s->power = 8; chain(s, 4 + facing); sound_post(0x24);
        break;
    case 9:
        if (flash_frames || blast_frames) return;
        flash_frames = 3;
        blast_frames = 9;
        break;
    case 10:
        if (time_stop) return;
        sound_post(0x23);
        time_stop = 128;
        wave = 0;
        player.vuln = 2;
        break;
    case 11:
        sound_post(0x22);
        if ((s = new_spell(K_GEN, x, y, facing))) s->timer = 1;
        if ((s = new_spell(K_GEN, x, y, facing ^ 1))) s->timer = 1;
        break;
    case 12:
    {
        u16 n;
        for (n = 0; n < 8; n++)
            if ((s = new_spell(K_RADIAL, x, y, 0)))
            {
                s->variant = 7 - n;
                s->timer = 62;
                chain(s, 12);
            }
        if ((s = new_spell(K_BLAST, x, y, facing))) { s->power = 20; s->timer = 62; sound_post(0x25); }
        break;
    }
    }
}

static bool wall_at(s16 x, s16 y, u16 facing)
{
    return (terrain_flags(x, y) & (facing ? 2 : 1)) != 0;
}

void spell_update(void)
{
    u16 i, n = 0, nb = 0;
    if (time_stop)
    {
        player.vuln = 2;
        wave += 2;
        if (!--time_stop) { player.vuln = 1; wave = 0; }
    }
    if (blast_frames) blast_frames--;
    if (!live) return;
    for (i = 0; i < SPELLS; i++)
    {
        Spell *s = sp + i;
        s16 sx;
        if (!s->kind) continue;
        n++;
        if (s->kind == K_BLAST) nb++;
        anim_step(&s->anim);
        sx = (s16)(s->x >> 8) - CAM_X;
        switch (s->kind)
        {
        case K_BOLT:
            s->x += s->facing ? -0x600 : 0x600;
            if (s->state == S_END) { if (!--s->timer) s->kind = 0; break; }
            if (!s->power || wall_at(s->x >> 8, s->y >> 8, s->facing))
            {
                s->state = S_END;
                s->timer = 12;
                chain(s, 2 + s->facing);
            }
            else if ((u16)(sx + 32) > 448) s->kind = 0;
            break;
        case K_RISE:
            if (!s->power) { s->kind = 0; break; }
            s->x += s->facing ? -0x400 : 0x400;
            s->s = s->s + 2 > 768 ? 768 : s->s + 2;
            s->x += s->facing ? -s->s : s->s;
            s->s = s->s + 16 > 1792 ? 1792 : s->s + 16;
            s->y += s->s;
            if ((u16)(sx + 32) > 448 || (s16)(s->y >> 8) > CAM_Y + 300) s->kind = 0;
            break;
        case K_GEN:                         /* every 8 frames a piece 32 px further */
            if (--s->timer) break;
            s->timer = 8;
            s->x += s->facing ? -(32 << 8) : 32 << 8;
            {
                Spell *w = new_spell(K_WALKER, s->x >> 8, s->y >> 8, s->facing);
                if (w)
                {
                    w->variant = s->step >> 1;      /* 0, 0, 1, 1, 2, 2 */
                    w->power = 10;
                    w->timer = 54;
                    chain(w, 6 + w->variant * 2 + s->facing);
                }
            }
            if (++s->step == 6) s->kind = 0;
            break;
        case K_WALKER:                      /* falls to the floor, burns 54 frames */
            if (s->step == 0)
            {
                s16 top;
                s->s = s->s + 48 > 1792 ? 1792 : s->s + 48;
                s->y -= s->s;
                if (terrain_floor(s->x >> 8, s->y >> 8, &top) || surface_floor(s->x >> 8, s->y >> 8, &top))
                { s->y = (s32)top << 8; s->step = 1; }
            }
            if (!--s->timer) s->kind = 0;
            break;
        case K_BLAST:
            if (!--s->timer) s->kind = 0;
            break;
        case K_RADIAL:                      /* 12 frames out, 10 still, 40 on its piece */
            if (s->timer > 50) { s->x += radial_v[s->variant][0]; s->y += radial_v[s->variant][1]; }
            if (s->timer == 40) chain(s, 13 + s->variant);
            if (!--s->timer) s->kind = 0;
            break;
        }
    }
    live = n;
    blasts = nb;
}

/* A spell touching the box (x, y, b) of a target with energy hp: the energy
 * it takes (0: none). Pierce: the bolt goes on with power - hp; once: the
 * target takes the power (its hit state keeps it from more until it ends);
 * the flash: targets on screen with energy <= 8. */

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

/* The union of what spell_test can hit: 1 boxes in u, 0 nothing, 2 no
 * bound (spell 9's blast takes everything near the view). */
#ifdef SHOTBOUND_CHECK
bool spell_would_hit(s16 x, s16 y, const s16 *b)
{
    u16 i;
    if (!live) return FALSE;
    for (i = 0; i < SPELLS; i++)
    {
        Spell *s = sp + i;
        const s16 *box;
        if (!s->kind || s->state != S_RUN || !s->power) continue;
        box = s->kind == K_BOLT ? bolt_box : s->kind == K_RISE ? rise_box : s->kind == K_WALKER ? walk_box[s->variant] :
              s->kind == K_BLAST ? blast_box : NULL;
        if (box && box_hit(s->x >> 8, s->y >> 8, box, x, y, b)) return TRUE;
    }
    return FALSE;
}
#endif

u16 spell_bounds(s16 *u)
{
    u16 i, any = 0;
    u[0] = u[2] = 0x7FFF; u[1] = u[3] = -0x8000;
    if (blast_frames) return 2;
    if (!live) return 0;
    for (i = 0; i < SPELLS; i++)
    {
        Spell *s = sp + i;
        const s16 *box;
        if (!s->kind || s->state != S_RUN || !s->power) continue;
        box = s->kind == K_BOLT ? bolt_box : s->kind == K_RISE ? rise_box : s->kind == K_WALKER ? walk_box[s->variant] :
              s->kind == K_BLAST ? blast_box : NULL;
        if (!box) continue;
        bounds_add(u, s->x >> 8, s->y >> 8, box);
        any = 1;
    }
    return any;
}

u16 spell_test(s16 x, s16 y, const s16 *b, u16 hp)
{
    u16 i;
    if (blast_frames && hp <= 8 && (u16)(x - CAM_X + 32) <= 384 && (u16)(y - CAM_Y + 32) <= 288) return hp ? hp : 1;
    if (!live) return 0;
    for (i = 0; i < SPELLS; i++)
    {
        Spell *s = sp + i;
        const s16 *box;
        u16 p;
        if (!s->kind || s->state != S_RUN || !s->power) continue;
        box = s->kind == K_BOLT ? bolt_box : s->kind == K_RISE ? rise_box : s->kind == K_WALKER ? walk_box[s->variant] :
              s->kind == K_BLAST ? blast_box : NULL;
        if (!box || !box_hit(s->x >> 8, s->y >> 8, box, x, y, b)) continue;
        p = s->power;
        if (s->kind == K_BOLT || s->kind == K_RISE) s->power = p > hp ? p - hp : 0;   /* pierce */
        return p;
    }
    return 0;
}

/* The blast cancels enemy projectiles in its box. */
bool spell_cuts(s16 x, s16 y, const s16 *b)
{
    u16 i;
    if (!blasts) return FALSE;
    for (i = 0; i < SPELLS; i++)
        if (sp[i].kind == K_BLAST && box_hit(sp[i].x >> 8, sp[i].y >> 8, blast_box, x, y, b)) return TRUE;
    return FALSE;
}

bool spell_flash(void) { return flash_frames && flash_frames--; }
u16 spell_wave(void) { return time_stop ? wave : 0; }

u16 spell_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y)
{
    u16 i, n = 0;
    if (!live) return 0;
    for (i = 0; i < SPELLS; i++)
    {
        Spell *s = sp + i;
        if (!s->kind || s->kind == K_GEN || s->kind == K_BLAST) continue;
        n += pose_draw(&ps_spell, anim_pose(&s->anim), &s->shown, sat, first + n,
                       (s->x >> 8) - view_x, (511 - (s->y >> 8)) - view_y, FALSE, NULL);
    }
    return n;
}

/* Stage 3's colouring of the spells (tools/pack_stage3.sh). */
void spell_art(const u8 *poses, const u8 *tiles, const s16 *sat)
{
    ps_spell.poses = poses; ps_spell.tiles = tiles; ps_spell.sat = sat;
}
