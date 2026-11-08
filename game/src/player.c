/* Willow (the player), written natively from specs/player.md.
 *
 * Units follow the arcade so traces compare directly: world x right, world
 * y UP, positions in 1/256 px (s32), speeds in 1/256 px per frame. The
 * stage map's downward y is 511 - world y (specs/player.md section 1).
 * Collision is the stage's 16x16 collision id grid plus the 64 descriptors
 * (tools/colpack.py). */
#include <genesis.h>
#include "frontend.h"
#include "stagedata.h"
#include "player.h"
#include "coop.h"
#include "shot.h"
#include "sound.h"
#include "magic.h"
#include "hud.h"
#include "actor.h"
#include "spell.h"
#include "../data/willow/layout.h"

extern const u8 willow_poses[];
extern const u8 willow_tiles[];
extern const u8 willow_nodes[];    /* per pose: u16 ticks (FFFF hold), u8 next, u8 0 */
extern const s16 willow_boxes[];   /* per pose: hurt box (ROM 0x3B1C2 geometry) */
extern const u8 mad_poses[], mad_tiles[], mad_nodes[];
extern const s16 mad_boxes[];

#define COL_ROWS 32
#define COL_COLS (sd->col_cols)

/* Descriptor bytes. */
#define F_WALL_R 0x01
#define F_WALL_L 0x02
#define F_CEIL 0x04
#define F_FLOOR 0x08

/* The two player forms (specs/madmartigan.md 0): Madmartigan is a form of
 * this player, not a second one; the record, input, camera and most rules are
 * shared. Per form: art, pose numbers (right; left as noted) and probes. */
typedef struct
{
    const u8 *poses, *tiles, *nodes;
    const s16 *boxes;
    u8 walk_r, walk_l, crouch, stand, attack, crouch_attack, air_attack, jump;
    u8 drop, drop_fall, climb, slide, slide_left, slide_recover, death, death_left;
    s8 foot_dx, foot_dy, head_dx, head_y, head_y_crouch, wall_y, ledge_y;
    u8 swing;               /* updates of sword / attack pose */
} Form;

static const Form forms[2] = {
    /* Willow (specs/player.md 5, 6) */
    { willow_poses, willow_tiles, willow_nodes, willow_boxes,
      0, 6, 12, 14, 20, 24, 28, 32, 34, 37, 42, 52, 3, 58, 60, 5,
      8, -16, 8, 16, 0, -8, 14, 10 },
    /* Madmartigan (specs/madmartigan.md 1, 4: native ids 70.. less 70) */
    { mad_poses, mad_tiles, mad_nodes, mad_boxes,
      0, 6, 12, 14, 16, 20, 24, 28, 30, 33, 38, 48, 2, 52, 58, 6,
      12, -24, 10, 24, 8, -16, 40, 13 },
};
static const Form *pf = &forms[0];

/* Poses (right; left: drop +4, climb +5, slide per form, death per form). */
#define P_WALK_R (pf->walk_r)
#define P_WALK_L (pf->walk_l)
#define P_CROUCH (pf->crouch)
#define P_STAND (pf->stand)
#define P_ATTACK (pf->attack)
#define P_CROUCH_ATTACK (pf->crouch_attack)
#define P_AIR_ATTACK (pf->air_attack)
#define P_JUMP (pf->jump)
#define P_DROP (pf->drop)
#define P_DROP_FALL (pf->drop_fall)
#define P_CLIMB (pf->climb)
#define P_SLIDE (pf->slide)
#define P_SLIDE_RECOVER (pf->slide_recover)
#define MAD (player.form)

#define WALK_SPEED 512
#define GROUND_PUSH 512
#define JUMP_SPEED 1088
#define JUMP_SPEED_UP 1408
#define GRAVITY 56
#define FALL_MAX 1792
#define EDGE_GRAVITY 128
#define SLIDE_SPEED 1792
#define SLIDE_SLOW 64
#define DROP_SPEED 512
#define F_LEDGE 0x80

/* Climb displacement after each 5-frame step (ROM 0xCD74): y +14 after the
 * first, +16 after the last. */
static const s16 climb_dy[6] = { 0, 14, 0, 0, 0, 16 };

enum { A_GROUND, A_JUMP, A_ATTACK, A_SLIDE, A_DROP, A_CLIMB };
enum { PEND_NONE, PEND_JUMP, PEND_ATTACK, PEND_SLIDE, PEND_DROP, PEND_CLIMB };

Player player;

/* The collision id at (x, y), as terrain_id (src/actor.c), from the last
 * four columns' headers kept here: an update's probes stay within three
 * columns. Dropped when the terrain or the stage changes. */
static s16 pc_c[4] = { 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF };
static u16 pc_first[4], pc_rows[4];
static const u8 *pc_ids[4];
static u8 pc_next, pc_ep;
static const StageData *pc_sd;

static u8 cell_id(s16 x, s16 y)
{
    const StageData *g = sd;
    s16 c = x >> 4;
    u16 i, d, r;
    if (pc_ep != terrain_epoch || pc_sd != g)
    {
        pc_c[0] = pc_c[1] = pc_c[2] = pc_c[3] = 0x7FFF;
        pc_ep = terrain_epoch; pc_sd = g;
    }
    if (pc_c[0] == c) i = 0;
    else if (pc_c[1] == c) i = 1;
    else if (pc_c[2] == c) i = 2;
    else if (pc_c[3] == c) i = 3;
    else
    {
        i = pc_next; pc_next = (pc_next + 1) & 3;
        pc_c[i] = c;
        pc_ids[i] = terrain_column(c, &pc_first[i], &pc_rows[i]);
    }
    if (!pc_ids[i]) return terrain_id(x, y);
    d = g->world_top - y;
    if (d > g->world_top) return 0;
    r = (d >> 4) - pc_first[i];
    return r < pc_rows[i] ? pc_ids[i][r] : 0;
}

static const u8 *desc(s16 x, s16 y)
{
    return sd->coldesc + cell_id(x, y) * 8;   /* src/actor.c: the trimmed collision columns */
}

/* Floor height (px above the cell bottom) at local x, -1 if none. */
static s16 floor_height(const u8 *e, s16 lx)
{
    u8 s = e[1];
    s16 h;
    if (!(e[0] & F_FLOOR)) return -1;
    if (s & 0x80) return s & 31;
    if (s & 0x40) lx = 15 - lx;
    h = (s & 15) + (lx >> ((s >> 4) & 3));
    return h > 16 ? 16 : h;
}

/* One foot probe at (x+dx, y+dy): if inside a floor, move the player up to
 * its surface. Returns 1 on contact. */
static u16 foot(s16 dx, s16 dy)
{
    s16 px = (player.x >> 8) + dx, py = (player.y >> 8) + dy;
    const u8 *e = desc(px, py);
    s16 h = floor_height(e, px & 15);
    s16 in = 15 - ((511 - py) & 15);          /* height of the probe above the cell bottom */
    if (in <= h)                              /* a foot on the surface counts as contact */
    {
        player.y = (player.y & ~0xFF) + ((s32)(h - in) << 8);
        player.y &= ~0xFF;
        player.material = e[0];
        if ((e[1] & 0xC0) == 0xC0)
        {
            /* Shape bits 6 and 7 (3E4C): the floor continues into the cell
             * above (a slope's full cell): once more at the corrected height. */
            py = (player.y >> 8) + dy;
            e = desc(px, py);
            h = floor_height(e, px & 15);
            in = 15 - ((511 - py) & 15);
            if (in <= h)
            {
                player.y = (player.y & ~0xFF) + ((s32)(h - in) << 8);
                player.material = e[0];
            }
        }
        return 1;
    }
    /* Bridge logs, platforms, the wagon (class 6): the surface pass places
     * the player's origin the same for both forms (the wagon deck at y + 32,
     * stage2.md 6.1), so Madmartigan's lower probe asks at Willow's height. */
    if (surface_floor(px, py - (pf->foot_dy + 16), &h))
    {
        h += pf->foot_dy + 16;
        player.y = (s32)(h - dy) << 8;
        player.material = 0;
        return 1;
    }
    return 0;
}

static u16 feet(void)
{
    u16 hit;
    s16 dx = pf->foot_dx, dy = pf->foot_dy;
    if (player.facing & 1) { hit = foot(-dx, dy); hit |= foot(dx, dy); }
    else { hit = foot(dx, dy); hit |= foot(-dx, dy); }
    return hit;
}

/* One wall probe (routine 3876 table, contact test as the arcade): a wall
 * flag for the probe's side, unless the probe is below the cell's ceiling
 * or above its floor surface; the player is pushed out to the wall face. */
static u16 wall(s16 dx, s16 dy)
{
    s16 px = (player.x >> 8) + dx, py = (player.y >> 8) + dy;
    const u8 *e = desc(px, py);
    s16 lx = px & 15, in = py & 15, c;
    if (!(e[0] & (dx < 0 ? F_WALL_L : F_WALL_R))) return 0;
    if ((e[0] & F_CEIL) && (s8)e[2] >= in) return 0;
    if ((e[0] & F_FLOOR) && in >= (e[1] & 31)) return 0;
    c = dx < 0 ? (s8)e[4] - lx : lx - (s8)e[3];
    if (c < 0) return 0;
    player.x = ((s32)((player.x >> 8) + (dx < 0 ? c : -c))) << 8;
    return 1;
}

/* Ledge probe on the facing side (ROM 0x3AF2, routine 3FA2): a tile with
 * both floor and ledge flags at or above the probe snaps the player up onto
 * its surface. */
static u16 ledge(void)
{
    s16 px = (player.x >> 8) + ((player.facing & 1) ? -8 : 8), py = (player.y >> 8) + pf->ledge_y;
    const u8 *e = desc(px, py);
    s16 lx = px & 15, s, in = py & 15;
    if (MAD)
    {
        /* Madmartigan (3A84 -> 3AB8): no grab under a ceiling behind the
         * head; the ledge point needs ledge material; no snap (his climb is
         * a spring, 2.8). */
        const u8 *b = desc((player.x >> 8) + ((player.facing & 1) ? 10 : -10), py);
        if (b[0] & F_CEIL) return 0;
        return (e[0] & (F_FLOOR | F_LEDGE)) == (F_FLOOR | F_LEDGE);
    }
    if ((e[0] & (F_FLOOR | F_LEDGE)) != (F_FLOOR | F_LEDGE)) return 0;
    if (e[1] & 0x80) s = e[1] & 31;
    else
    {
        if (e[1] & 0x40) lx = 15 - lx;
        s = (e[1] & 15) + (lx >> ((e[1] >> 4) & 3));
        if (s > 16) s = 16;
    }
    if (s < in) return 0;
    player.y = ((player.y >> 8) + s - in) << 8;
    player.material = e[0];
    return 1;
}

/* Head probes: a ceiling moves the player down and ends a rise. */
static u16 head(void)
{
    s16 hy = player.crouch ? pf->head_y_crouch : pf->head_y, i;
    u16 hit = 0;
    for (i = -pf->head_dx; i <= pf->head_dx; i += 2 * pf->head_dx)
    {
        s16 px = (player.x >> 8) + i, py = (player.y >> 8) + hy;
        const u8 *e = desc(px, py);
        s16 in = 15 - ((511 - py) & 15);
        if ((e[0] & F_CEIL) && in >= (s8)e[2])
        {
            player.y -= (s32)(in - (s8)e[2] + 1) << 8;
            hit = 1;
            player.head_hit = 1;
        }
    }
    return hit;
}

static void set_pose(u16 p)
{
    if (p != player.pose) { player.pose = p; player.pose_tick = 0; player.pose_dirty = 1; }
}

/* Arcade animation nodes (routines 1F30 / 1F4C): selecting a node loads
 * its tick count; each step counts down and moves to the next node at 0. */
static void node_set(u16 p)
{
    player.pose = p;
    player.pose_dirty = 1;
    player.node_ticks = (pf->nodes[p * 4] << 8) | pf->nodes[p * 4 + 1];
}

static void node_step(void)
{
    if (player.node_ticks == 0xFFFF || --player.node_ticks) return;
    node_set(pf->nodes[player.pose * 4 + 2]);
}

static void facing_from(s16 dir)
{
    if (dir) player.facing = dir < 0 ? 1 : 0;
    else player.facing |= 2;
}

/* A jump press on the ground or in slide recovery: jump, or with down held
 * slide, or drop through when standing on ledge material. */
static u16 jump_press(u16 held)
{
    if (!(held & BUTTON_DOWN)) return PEND_JUMP;
    return (player.material & F_LEDGE) ? PEND_DROP : PEND_SLIDE;
}

void player_form(u16 form)
{
    form = coop_form(form);                 /* co-op: each player keeps its hero (src/coop.c) */
    player.form = form;
    pf = &forms[form];
}

void player_init(s16 x, s16 y)
{
    u8 form = player.form;
    memset(&player, 0, sizeof player);
    player.form = form;
    player.x = (s32)x << 8;
    player.y = (s32)y << 8;
    player.facing = 2;
    player.pose = 0xFFFF;
    set_pose(P_STAND);
    player.cam_x = x - 192;
    player.cam_y = y - 128;
    player.cam_max_y = 2048;
    if (player.cam_x < 0) player.cam_x = 0;
    if (player.cam_y < 0) player.cam_y = 0;
    player.cam_max_x = 1952;
    player.vuln = 1;
    player.hp_cache = hud.hp;
    player.armor_cache = hud.armor;
    player.safe_x = x;
    player.safe_y = y;
    if (two_players) coop_place();          /* co-op: the second player beside the lead */
}

/* Scripted standing (the intro and the stage-start gate). */
void player_stand(bool left)
{
    player.facing = left ? 3 : 2;
    set_pose(P_STAND + left);
}

const s16 *player_box(void)
{
    return pf->boxes + player.pose * 4;
}

const s16 *player_box_of(const Player *p)
{
    return forms[p->form].boxes + p->pose * 4;
}

/* The damage routine (3A66C): one armour point (and one armour item) or
 * one HP; the reaction follows on the next update. */
void player_hurt(void)
{
    static const u8 armor_item[3] = { 10, 8, 9 };
    u16 i;
#ifdef INVINCIBLE
    return;                                 /* trace builds: the references were recorded invincible */
#endif
    if (fe.invincible) return;              /* the DEBUG page's option */
    if (coop_xy) { coop_hurt_second(); return; }   /* co-op: the actor was aiming at the second player */
    if (player.vuln != 1 || player.dead) return;
    player.vuln = 2;
    if (hud.armor)
    {
        hud.armor--;
        for (i = 0; i < 3; i++)
            if (hud.items[armor_item[i]]) { hud.items[armor_item[i]]--; hud_dirty |= HUD_ITEMS; break; }
    }
    else if (hud.hp) hud.hp--;
}

/* Health step (C170): a changed HP or armour starts the hurt reaction
 * (sound 0x82, 128 updates not hittable) or, at 0 HP, the death. */
static void health(void)
{
    if (player.hurt_timer && !--player.hurt_timer) player.vuln = 1;
    if (hud.hp == player.hp_cache && hud.armor == player.armor_cache) return;
    player.hp_cache = hud.hp;
    player.armor_cache = hud.armor;
    hud_dirty |= HUD_VITAL;
    if (!hud.hp)
    {
        player.dead = 1;
        player.vuln = 2;
        player.dead_timer = MAD ? 60 : 40;      /* Madmartigan: 0xE0DA, 6 nodes x 10 */
        player.facing = (player.facing & 1) ^ 1;
        magic.hold = 0;
        sound_post(MAD ? SND_MAD_DEATH : SND_DEATH);
        node_set(pf->death + (player.facing & 1) * pf->death_left);
        return;
    }
    sound_post(MAD ? SND_MAD_HURT : SND_HURT);
    player.hurt_timer = 128;
    player.vuln = 2;
}

static void walk_pose(u16 moving)
{
    u16 base = (player.facing & 1) ? P_WALK_L : P_WALK_R;
    if (!moving) { set_pose(P_STAND + (player.facing & 1)); return; }
    if (player.pose < base || player.pose >= base + 6)
    {
        /* A facing change selects the walk root afresh; entering the walk
         * with the facing unchanged (after a landing or a climb) the
         * arcade has already counted one tick. */
        set_pose(base);
        if (player.facing == player.last_facing) player.pose_tick = 1;
    }
    else if (++player.pose_tick >= 6)
    {
        player.pose = base + (player.pose - base + 1) % 6;
        player.pose_tick = 0;
        player.pose_dirty = 1;
    }
}

/* Camera (arcade routines 4298E / 42B1C / 42BAC as the specification):
 * X locked to the player; Y keeps the player 128 above the camera: down at
 * once, up by snapping (within 130) or accelerating 64 a frame to 1792 in
 * 1/65536 px; while airborne it follows only past 200, at 2 px a frame. */
static void camera(void)
{
    s16 px = player.x >> 8, py = player.y >> 8, cx, d;
    if (two_players)
    {
        if (coop_cur) return;               /* co-op: the lead's record moves the camera */
        coop_mid(&px, &py);                 /* to the players' midpoint */
    }
    cx = px - 192;
    if (cx < player.cam_min_x) cx = player.cam_min_x;
    if (cx > player.cam_max_x) cx = player.cam_max_x;
    /* After a camera stop opens the camera catches up at 3 px a frame. */
    if (player.cam_catch)
    {
        if (cx > player.cam_x + 3) cx = player.cam_x + 3;
        else player.cam_catch = 0;
    }
    if (cx - 768 > player.cam_min_x) player.cam_min_x = cx - 768;
    player.cam_x = cx;

    d = py - player.cam_y;
    if (d > 128)
    {
        if (player.airborne)
        {
            if (d <= 200) { player.cam_track = 1; player.cam_vy = 0; }
            else player.cam_y += 2;
        }
        else if (player.cam_track || d > 130)
        {
            u32 y;
            player.cam_vy += 64;
            if (player.cam_vy > 1792) player.cam_vy = 1792;
            y = ((u32)(u16)player.cam_y << 16 | player.cam_yf) + ((u32)player.cam_vy << 8);
            player.cam_y = y >> 16;
            player.cam_yf = y;
        }
        else { player.cam_y += d - 128; player.cam_vy = 0; }
        if (player.cam_y > player.cam_max_y) player.cam_y = player.cam_max_y;
    }
    else if (d < 128)
    {
        player.cam_y += d - 128;
        if (player.cam_y < player.cam_min_y) player.cam_y = player.cam_min_y;
        else { player.cam_track = 1; player.cam_vy = 0; }
    }
    else { player.cam_track = 0; player.cam_vy = 0; }
    {
        extern s32 chase_vx;                /* stage 2's chase: the camera's y stays 0 (stage2.md 2.4) */
        if (chase_vx) { player.cam_y = 0; player.cam_vy = 0; }
    }
}

/* The attack's spell (CB24): spells 0-6 are shot variants; the charged
 * spells 7-12 need magic level 1+ (not reachable in the opening yet). */
static void cast(s16 dy)
{
    u16 spell = magic_spell();
    s16 x = player.x >> 8, y = player.y >> 8;
    if (spell <= 6) shot_fire(x, y + dy, player.facing & 1, spell);
    else if (spell == 11) spell_cast(spell, x, y, player.facing & 1);           /* CC60 */
    else if (spell == 12) spell_cast(spell, x, y + dy - 8, player.facing & 1);  /* CC90: y, y-8 crouched */
    else spell_cast(spell, x, y + dy, player.facing & 1);                       /* 7, 8, 9, 10: y+8 / y crouched */
}

/* Madmartigan's swing (0xDE22): a sword actor (src/shot.c) if none is
 * alive, its follow offset by posture at spawn; charge tier from the meter
 * level at release (0xDE0A). */
static void swing(u16 posture)
{
    shot_sword(player.facing & 1, posture, magic_tier());
}

/* Madmartigan's slide (0xE02A): 1792 less 64 a frame (28 updates, 101 px
 * on flat floor) or to a wall, then 6 updates of recovery; no jump out of
 * it. An attack press makes a crouch swing (handled by the caller). */
static void mad_slide(void)
{
    u16 f = player.facing & 1;
    switch (player.phase)
    {
    case 0:
        sound_post(SND_SLIDE);
        player.facing = f;
        player.slide_speed = SLIDE_SPEED;
        player.phase = 2;
        node_set(P_SLIDE + f * pf->slide_left);
        break;
    case 2:
    {
        u16 stop;
        node_step();
        player.x += f ? -player.slide_speed : player.slide_speed;
        stop = wall(f ? -16 : 16, pf->wall_y) | wall(f ? -16 : 16, 0);
        player.slide_speed -= SLIDE_SLOW;
        if (stop || player.slide_speed < 0)
        {
            player.phase = 4;
            player.timer = 6;
            player.facing |= 2;
            node_set(P_SLIDE_RECOVER + f);
        }
        break;
    }
    default:
        node_step();
        if (!--player.timer) { player.action = A_GROUND; player.crouch = 0; }
        break;
    }
}

/* Madmartigan's drop through a platform (0xDD04): 6 updates crouched, then
 * 12 updates falling without a floor test, then a normal fall. */
static void mad_drop(s16 dir)
{
    (void)dir;
    switch (player.phase)
    {
    case 0:
        player.crouch = 0;
        player.vy = 0;
        player.facing |= 2;
        player.airborne = 2;
        player.timer = 6;
        player.phase = 2;
        node_set(P_DROP + (player.facing & 1) * 4);
        break;
    case 2:
        node_step();
        if (!--player.timer) { player.phase = 4; player.timer = 12; }
        break;
    default:
        node_step();
        if (player.vy < FALL_MAX) player.vy += GRAVITY;
        player.y -= player.vy;
        if (!--player.timer) { player.action = A_JUMP; player.rising = 0; }
        break;
    }
    if (!(player.facing & 2)) player.x += (player.facing & 1) ? -WALK_SPEED : WALK_SPEED;
}

/* Madmartigan's climb (0xDF40): a vertical spring of 1344 less 56 a frame
 * (about 66 px), then a short fall onto the ledge; a ceiling ends it. */
static void mad_fall(void)
{
    player.action = A_JUMP;
    player.rising = 0;
    player.vy = 0;
    player.airborne = 2;
    set_pose(P_JUMP + (player.facing & 1));
}

static void mad_climb(void)
{
    switch (player.phase)
    {
    case 0:
        if (player.head_hit) { mad_fall(); return; }
        player.airborne = 0;
        player.crouch = 0;
        player.timer = 6;
        player.phase = 2;
        node_set(P_CLIMB + (player.facing & 1) * 5);
        return;
    case 2:
        node_step();
        if (!--player.timer) { player.phase = 4; player.vy = 1344; player.airborne = 1; }
        return;
    case 4:
        node_step();
        player.y += player.vy;
        player.vy -= GRAVITY;
        if (player.vy < 0) { player.vy = 0; player.phase = 6; }
        player.head_hit = 0;
        if (head()) mad_fall();
        return;
    default:
        node_step();
        player.y -= player.vy;
        if (player.vy < 128) player.vy += GRAVITY;
        if (feet() || player.vy >= 128) { player.action = A_GROUND; player.airborne = 0; player.vy = 0; }
        return;
    }
}

/* Scripted life states (madmartigan.md 2.10, stage2.md 5.12).
 * 12, the wagon leap (0xD5A8): the music fades; he falls to the floor
 * (gravity 24), walks right 2 px a frame for 16 frames, then leaps (1088 up,
 * gravity 56) at 2 px a frame toward x 1728; landing (on the wagon): normal
 * play, the clock at 3:00, sound 0x2B, music 0x03.
 * 16, the chase end (0xD80E): sword and charge cleared, he walks (2 px a
 * frame, gravity 24, foot snap) to the screen centre, then stands facing
 * right. */
s16 player_script_x;         /* scripted walks: the x to reach */

void player_script(u16 state)
{
    player.script = state;
    player.phase = 0;
    player.vy = 0;
    player.pending = 0;
    player.air_attack = 0;
    player.crouch = 0;
    magic.hold = 0;
    if (state == 12) sound_post(0x08FB);
}

/* The road death (0xE12C, set by the wagon 0x2FE5A): timer 96, the road
 * death poses (0xE600, looping), sound 0x85; the body slides away with the
 * road. */
void player_road_death(void)
{
    hud.hp = player.hp_cache = 0;
    hud_dirty |= HUD_VITAL;
    player.dead = 1;
    player.vuln = 2;
    player.dead_timer = 96;
    player.script = 4;
    magic.hold = 0;
    sound_post(SND_MAD_DEATH);
    node_set(54);
}

static void script_fall(s16 g)
{
    if (player.vy < FALL_MAX) player.vy += g;
    player.y -= player.vy;
    if (feet()) player.vy = 0;
}

static void script_update(void)
{
    s16 px = player.x >> 8;
    if (player.script == 12)
    {
        switch (player.phase)
        {
        case 0:                                     /* fall to the floor */
            set_pose(P_JUMP);
            player.facing = 0;
            player.airborne = 2;
            if (player.vy < FALL_MAX) player.vy += 24;
            player.y -= player.vy;
            if (feet()) { player.vy = 0; player.airborne = 0; player.phase = 2; player.timer = 16; }
            break;
        case 2:                                     /* walk right 16 frames */
            walk_pose(1);
            player.x += WALK_SPEED;
            script_fall(24);
            if (!--player.timer)
            {
                player.phase = 4;
                player.vy = JUMP_SPEED;
                player.rising = 1;
                player.airborne = 1;
                set_pose(P_JUMP);
                sound_post(SND_MUSIC_CHASE);
                sound_post(0x01FA);
            }
            break;
        default:                                    /* the leap */
            if (px < 1728) player.x += px + 2 > 1728 ? (s32)(1728 - px) << 8 : WALK_SPEED;
            if (player.rising)
            {
                player.y += player.vy;
                player.vy -= GRAVITY;
                if (player.vy < 0) { player.rising = 0; player.vy = 0; player.airborne = 2; }
            }
            else
            {
                player.y -= player.vy;
                if (player.vy < FALL_MAX) player.vy += GRAVITY;
                if (feet())
                {
                    player.script = 0;
                    player.action = A_GROUND;
                    player.vy = 0;
                    player.airborne = 0;
                    hud.min = 3; hud.sec = 0;
                    hud_dirty |= HUD_TIME;
                    sound_post(SND_LAND);
                }
            }
            break;
        }
        return;
    }
    /* Stage 4 (stage4.md 6.10, 6.11, 6.13): 22 walks right to script_x, then
     * stands; 14 walks to script_x (the arrival); 10 is carried by the shield
     * sled (src/fam_stage4.c sets the position; not drawn). */
    if (player.script == 10) { player.airborne = 0; return; }
    if (player.script == 22 || player.script == 14)
    {
        if (px < player_script_x) { player.facing = 0; walk_pose(1); player.x += WALK_SPEED; }
        else { player.facing = 0; set_pose(P_STAND); if (player.script == 14) player.script = 0; }
        if (px >= 16) script_fall(24);      /* off the map's left edge (the walk-in starts at -32): no floor there */
        return;
    }
    /* 16: the chase end */
    {
        s16 c = player.cam_x + 192;
        if (px == c) { player.facing = 2; set_pose(P_STAND); }
        else
        {
            player.facing = px > c;
            walk_pose(1);
            player.x += (s32)(px > c ? (px - c < 2 ? c - px : -2) : (c - px < 2 ? c - px : 2)) << 8;
        }
        script_fall(24);                            /* gravity 24 and the foot snap */
    }
}

/* Hazard tiles (combat.md 2.6, C2B8): collision id 39 at the player's x and,
 * on even updates, y + 16 (y crouched), on odd ones y - 8 (Madmartigan y -
 * 16) hurts (stage 5's spike rails). */
static void hazard(void)
{
    s16 y = player.y >> 8;
    if (player.vuln != 1 || player.dead) return;
    y = (game_frame & 1) ? y - (MAD ? 16 : 8) : y + (player.crouch ? 0 : 16);
    if (cell_id(player.x >> 8, y) == 39) player_hurt();
}

void player_update(u16 held)
{
    u16 pressed = held & ~player.last_held;
    s16 dir = (held & BUTTON_RIGHT) ? 1 : (held & BUTTON_LEFT) ? -1 : 0;
    u16 pend, was_crouched, prev_facing;
    player.last_held = held;

    hazard();
    health();
    /* Pit (C33E): below the camera's lower limit the player is removed at
     * once, no animation; also while dying. */
#ifdef INVINCIBLE
    /* Service invincibility (FF799E): a pit bounces the player up (C33E). */
    if (player.y < (s32)(player.cam_min_y + 1) << 8) { player.action = A_JUMP; player.rising = 1; player.vy = 3584; player.airborne = 1; }
#else
    if (fe.invincible && player.y < (s32)(player.cam_min_y + 1) << 8) { player.action = A_JUMP; player.rising = 1; player.vy = 3584; player.airborne = 1; }   /* DEBUG */
    else if (player.y < (s32)(player.cam_min_y + 1) << 8 && player.dead != 2) { player.dead = 2; player.vuln = 2; }
#endif
    if (player.dead)
    {
        if (player.dead == 1)                       /* death animation (root 0xD436), 40 updates */
        {
            node_step();
            if (player.script == 4)                 /* the road death: carried off by the road */
            {
                extern u16 chase_speed;
                player.x -= chase_speed;
                if ((s16)(player.x >> 8) - player.cam_x + 32 < 0) { player.dead = 2; return; }
            }
            if (!--player.dead_timer) player.dead = 2;
        }
        return;
    }

    if (player.script)
    {
        player.last_facing = player.facing;
        script_update();
        camera();
        return;
    }

    /* The arcade latches a jump or attack press and acts on it in the
     * next update (checked against the reference trace). */
    pend = player.pending;
    player.pending = 0;

    switch (player.action)
    {
    case A_GROUND:
        if (pend == PEND_JUMP) goto jump;
        if (pend == PEND_SLIDE) { player.action = A_SLIDE; player.phase = 0; goto slide; }
        if (pend == PEND_DROP) { player.action = A_DROP; player.phase = 0; goto drop; }
        if (pend == PEND_ATTACK)
        {
            player.action = A_ATTACK;
            player.timer = 0;
            if (MAD) { player.crouch = (held & BUTTON_DOWN) != 0; swing(player.crouch ? SW_CROUCH : SW_STAND); }
            set_pose((player.crouch ? P_CROUCH_ATTACK : P_ATTACK) + (player.facing & 1) * 2);
            goto attack;
        }
        was_crouched = player.crouch;
        player.crouch = (held & BUTTON_DOWN) != 0;
        if (player.crouch) player.facing = (dir ? dir < 0 : player.facing & 1) | 2;   /* crouched: turn only */
        else facing_from(dir);
        /* The attack button is checked every ground update (charge, C610),
         * also on a jump press, whose attack is then ignored. */
        if (pressed & BUTTON_B) { player.pending = jump_press(held); magic_charge(held, pressed); }
        else if (magic_charge(held, pressed)) player.pending = PEND_ATTACK;
        /* Pose timing as the arcade: no pose change on a press frame or on
         * the frame crouch is released (both show from the next update). */
        if (player.crouch) { set_pose(P_CROUCH + (player.facing & 1)); dir = 0; }
        else if (!player.pending && !was_crouched) walk_pose(dir != 0);
        player.x += dir * WALK_SPEED;
    ground_move:
        player.y -= GROUND_PUSH + player.edge_fall;
        if (feet()) player.edge_fall = 0;
        else
        {
            player.edge_fall += EDGE_GRAVITY;
            if (player.edge_fall >= 384)
            {
                player.edge_fall = 0;
                player.action = A_JUMP;
                player.airborne = 2;
                player.rising = 0;
                player.vy = 0;
                player.crouch = 0;
                set_pose(P_JUMP + (player.facing & 1));
            }
        }
        break;

    jump:
        player.action = A_JUMP;
        player.vy = (held & BUTTON_UP) ? JUMP_SPEED_UP : JUMP_SPEED;
        player.rising = 1;
        player.airborne = 1;
        player.crouch = 0;
        player.head_hit = 0;
        /* fall through */
    case A_JUMP:
        if (pend == PEND_CLIMB) { player.action = A_CLIMB; player.phase = 0; goto climb; }
        /* Order as the arcade: vertical move and its probes, the ledge
         * test, then (routine CA1C) the horizontal step, so floor, ceiling
         * and ledge probes see the previous update's x. */
        prev_facing = player.facing;
        facing_from(dir);
        if (pend == PEND_ATTACK) { player.air_attack = 1; player.timer = 0; if (MAD) swing(SW_AIR); }
        else if (!player.air_attack && magic_charge(held, pressed)) player.pending = PEND_ATTACK;
        if (player.air_attack)
        {
            player.timer++;
            if (MAD)
            {
                /* 4 updates of the swing pose, 8 of the follow (2.4). */
                set_pose(P_AIR_ATTACK + (player.facing & 1) * 2 + (player.timer >= 5));
                if (player.timer >= pf->swing) player.air_attack = 0;
            }
            else
            {
                set_pose(P_AIR_ATTACK + (player.facing & 1) * 2 + (player.timer > 5));
                if (player.timer == 5) cast(8);
                if (player.timer >= 10) player.air_attack = 0;
            }
        }
        /* After a drop the drop-fall pose stays until the facing changes;
         * after Madmartigan's air swing, the follow pose (2.4). */
        else if (player.facing != prev_facing ||
                 (player.pose != P_DROP_FALL && player.pose != P_DROP_FALL + 4 &&
                  !(MAD && player.pose == P_AIR_ATTACK + (player.facing & 1) * 2 + 1)))
            set_pose(P_JUMP + (player.facing & 1));
        if (player.rising)
        {
            player.y += player.vy;
            player.vy -= GRAVITY;
            if (player.vy < 0 || head()) { player.rising = 0; player.vy = 0; player.airborne = 2; }
        }
        else
        {
            player.y -= player.vy;
            if (player.vy < FALL_MAX) player.vy += GRAVITY;
            if (feet())
            {
                /* Landed: the ground handler runs next update. Walking, the
                 * walk cycle starts now, one tick in; standing, the jump pose
                 * stays for this update. */
                player.action = A_GROUND;
                if (MAD && player.air_attack) { player.action = A_ATTACK; player.crouch = 0; }   /* 0xDBF6 */
                player.air_attack = 0;
                player.vy = 0;
                player.airborne = 0;
                sound_post(SND_LAND);
            }
        }
        /* Up held in the air grabs a ledge (climbs from the next update). */
        if (player.action == A_JUMP && (held & BUTTON_UP) && ledge())
        {
            player.pending = PEND_CLIMB;
            player.rising = 0;
            player.vy = 0;
            player.airborne = 0;
        }
        player.x += dir * WALK_SPEED;
        break;

    case A_SLIDE:
    slide:
        if (pend == PEND_JUMP) goto jump;
        if (pend == PEND_DROP) { player.action = A_DROP; player.phase = 0; goto drop; }
        if (pend == PEND_SLIDE) player.phase = 0;
        if (pend == PEND_ATTACK)
        {
            player.action = A_ATTACK;
            player.crouch = 1;
            player.timer = 0;
            if (MAD) { player.facing &= 1; swing(SW_CROUCH); }
            set_pose(P_CROUCH_ATTACK + (player.facing & 1) * 2);
            goto attack;
        }
        if (MAD) { mad_slide(); goto slide_charge; }
        switch (player.phase)
        {
        case 0:
            sound_post(SND_SLIDE);
            player.phase = 2;
            player.timer = 3;
            node_set(P_SLIDE + (player.facing & 1) * 3);
            break;
        case 2:                                     /* wind-up */
            if (!--player.timer) { player.facing &= 1; player.slide_speed = SLIDE_SPEED; player.phase = 4; }
            node_step();
            break;
        case 4:                                     /* slide, slowing; a wall stops it */
        {
            u16 stop, f = player.facing & 1;
            node_step();
            player.x += f ? -player.slide_speed : player.slide_speed;
            stop = wall(f ? -16 : 16, pf->wall_y) | wall(f ? -16 : 16, 0);
            if (!stop) { player.slide_speed -= SLIDE_SLOW; stop = player.slide_speed < 0; }
            if (stop)
            {
                player.phase = 6;
                player.timer = 6;
                player.facing |= 2;
                node_set(P_SLIDE_RECOVER + f);
            }
            break;
        }
        default:                                    /* recovery: 6 + 6 frames */
            if (!--player.timer)
            {
                if (player.phase == 6) { player.timer = 6; player.phase = 8; }
                else { player.action = A_GROUND; player.crouch = 0; }
            }
            if (pressed & BUTTON_B) { facing_from(dir); player.pending = jump_press(held); }
            break;
        }
    slide_charge:
        if (magic_charge(held, pressed))
        {
            player.pending = PEND_ATTACK;
            if (held & BUTTON_DOWN) set_pose(P_CROUCH + (player.facing & 1));   /* shows at once */
        }
        goto ground_move;

    case A_DROP:
    drop:
        if (MAD) { mad_drop(dir); magic_sparks(); break; }
        switch (player.phase)
        {
        case 0:
            player.facing |= 2;
            player.phase = 2;
            player.timer = 6;
            node_set(P_DROP + (player.facing & 1) * 4);
            break;
        case 2:
        case 4:
            if (!--player.timer) { player.phase += 2; player.timer = 6; }
            node_step();
            break;
        case 6:
            if (--player.timer) node_step();
            else
            {
                player.phase = 8;
                player.airborne = 2;
                player.vy = DROP_SPEED;
                player.timer = 8;
                facing_from(dir);
                node_set(P_DROP_FALL + (player.facing & 1) * 4);
            }
            break;
        default:                                    /* falling through the platform */
            if (!--player.timer) { player.action = A_JUMP; player.rising = 0; }
            player.y -= player.vy;
            if (player.vy < FALL_MAX) player.vy += GRAVITY;
            prev_facing = player.facing;
            facing_from(dir);
            if (player.facing != prev_facing) node_set(P_DROP_FALL + (player.facing & 1) * 4);
            break;
        }
        if (!(player.facing & 2)) player.x += (player.facing & 1) ? -WALK_SPEED : WALK_SPEED;
        magic_sparks();
        break;

    case A_CLIMB:
    climb:
        if (MAD) { mad_climb(); magic_sparks(); break; }
        if (!player.phase)
        {
            player.phase = 2;
            player.timer = 5;
            player.airborne = 1;
            node_set(P_CLIMB + (player.facing & 1) * 5);
        }
        else if (player.phase == 12)
        {
            if (!--player.timer) { player.action = A_GROUND; player.airborne = 0; feet(); }
        }
        else
        {
            if (!--player.timer)
            {
                player.y += (s32)climb_dy[player.phase >> 1] << 8;
                player.timer = 5;
                player.phase += 2;
            }
            node_step();
        }
        magic_sparks();
        break;

    case A_ATTACK:
    attack:
        if (MAD)
        {
            /* A jump press cancels the swing (C716 -> 0xDB30, slide and
             * drop included); attack input is not read (2.4). */
            if (pend == PEND_JUMP) goto jump;
            if (pend == PEND_SLIDE) { player.action = A_SLIDE; player.phase = 0; goto slide; }
            if (pend == PEND_DROP) { player.action = A_DROP; player.phase = 0; goto drop; }
            if (pressed & BUTTON_B) player.pending = jump_press(held);
            if (++player.timer == 5) set_pose(player.pose + 1);     /* follow pose: the sword shows */
            if (player.timer >= pf->swing) player.action = A_GROUND;
            goto ground_move;
        }
        if (pend == PEND_JUMP && !player.crouch) goto jump;
        if (pressed & BUTTON_B && !player.crouch) player.pending = PEND_JUMP;
        player.timer++;
        if (player.timer == 5)                               /* follow pose: the shot spawns now */
        {
            set_pose(player.pose + 1);
            cast(player.crouch ? 0 : 8);
        }
        if (player.timer >= 10) player.action = A_GROUND;
        goto ground_move;
    }

    player.last_facing = player.facing;
    if (player.action == A_GROUND && hud.hp) { player.safe_x = player.x >> 8; player.safe_y = player.y >> 8; }
    wall(16, pf->wall_y); wall(16, 0); wall(-16, pf->wall_y); wall(-16, 0);
    if (MAD)
    {
        /* The head probe runs every update (39FE); its flag +0x56 is what the
         * next update's climb start reads (2.8). Only the flag is kept here. */
        s32 y = player.y;
        player.head_hit = 0;
        head();
        player.y = y;
    }
    {
        s16 px = player.x >> 8;
        /* Screen clamp (C392): the arcade's 16..368 of its 384-px screen;
         * the Genesis shows 32..352 of it, so 40..344 keeps Willow in view
         * at camera stops (screen-width adaptation; the camera follows her
         * everywhere else). */
        if (px < player.cam_x + 40) player.x = (s32)(player.cam_x + 40) << 8;
        if (px > player.cam_x + 344) player.x = (s32)(player.cam_x + 344) << 8;
    }
    camera();
}

/* Sprite output: the pose's prepared entries with the position added.
 * Tiles go to VRAM at tile index `base` when the pose changed. */
static inline u16 draw_rec(Player *p, const Form *f, VDPSprite *sat, u16 first, u16 base, s16 view_x, s16 view_y)
{
    if (p->script == 10) return 0;          /* on the shield sled: its poses carry both riders */
    {
    const u8 *dir = f->poses + p->pose * 6;
    u16 tile0 = (dir[0] << 8) | dir[1], ntiles = dir[2], nspr = dir[3], e0 = (dir[4] << 8) | dir[5];
    const u8 *e = f->poses + POSE_COUNT * 6 + e0 * 6;       /* both forms: 70 poses */
    s16 sx = (p->x >> 8) - view_x, sy = (511 - (p->y >> 8)) - view_y;
    u16 i;

    if (p->pose_dirty)
    {
        DMA_queueDma(DMA_VRAM, (void *)(f->tiles + (u32)tile0 * 32), base * 32, ntiles * 16, 2);
        p->pose_dirty = 0;
    }
    {
        /* The table's words written directly (the struct's bit fields cost a
         * read-modify-write each). */
        VDPSprite *s = sat + first;
        u16 link = first;
        sy += 128; sx += 128;
        for (i = 0; i < nspr; i++, e += 6, s++)
        {
            s->y = sy + (s16)((e[0] << 8) | e[1]);
            s->size_link = ((e[2] & 15) << 8) | ++link;
            s->attribut = ((e[2] & 0x80) << 4) + base + e[3];   /* bit 7: a mirrored pose (Madmartigan's left): H-flip, line 0 */
            s->x = sx + (s16)((e[4] << 8) | e[5]);
        }
    }
    return nspr;
    }
}

u16 player_draw(VDPSprite *sat, u16 first, u16 base, s16 view_x, s16 view_y)
{
    return draw_rec(&player, pf, sat, first, base, view_x, view_y);
}

/* Another record's pose (co-op: the second player, src/coop.c). */
u16 player_draw_of(Player *p, VDPSprite *sat, u16 first, u16 base, s16 view_x, s16 view_y)
{
    return draw_rec(p, &forms[p->form], sat, first, base, view_x, view_y);
}

/* A form's pose at screen (sx, sy) on CRAM line pal, its tiles at VRAM base
 * (the stage-5 select screen's figures, stage5.md 1.3). */
u16 player_pose_put(u16 form, u16 pose, VDPSprite *sat, u16 first, u16 base, s16 sx, s16 sy, u16 pal)
{
    const Form *f = &forms[form];
    const u8 *dir = f->poses + pose * 6;
    u16 tile0 = (dir[0] << 8) | dir[1], ntiles = dir[2], nspr = dir[3], e0 = (dir[4] << 8) | dir[5];
    const u8 *e = f->poses + POSE_COUNT * 6 + e0 * 6;
    u16 i;
    DMA_queueDma(DMA_VRAM, (void *)(f->tiles + (u32)tile0 * 32), base * 32, ntiles * 16, 2);
    for (i = 0; i < nspr; i++, e += 6)
    {
        VDPSprite *s = sat + first + i;
        s->y = 128 + sy + (s16)((e[0] << 8) | e[1]);
        s->size = e[2] & 15;
        s->attribut = TILE_ATTR_FULL(pal, 1, 0, e[2] >> 7, base + e[3]);
        s->x = 128 + sx + (s16)((e[4] << 8) | e[5]);
        s->link = first + i + 1;
    }
    return nspr;
}

/* A form's standing pose (facing right). */
u16 player_form_stand(u16 form) { return forms[form].stand; }
