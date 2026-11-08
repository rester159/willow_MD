#ifndef ACTOR_H
#define ACTOR_H
#include <genesis.h>
#include "sprite.h"

/* Stage actors (specs/stage1_enemies.md, combat.md, stage1_boss.md).
 * Units as the player: world x right, y UP, 1/256 px; one update a frame.
 * One array, split by class as the arcade's pools (the class decides the
 * update order and which collision passes see the actor). */

enum { C_ENEMY, C_SURFACE, C_ESHOT, C_EFFECT, C_PICKUP, C_OBJECT, C_BOSS, C_CLASSES };

/* Families. */
enum
{
    F_NONE,
    F_SOLDIER, F_BEAST, F_ARCHER, F_SPEARMAN, F_SORCERER, F_APPARITION,
    F_ARROW, F_FIREBALL, F_SQUIGGLE, F_GROUNDFIRE, F_FLAME,
    F_LOG, F_PLATFORM,
    F_BURST, F_SPARKLE, F_SPLASH, F_LABEL,
    F_GOLD,
    F_CHEST, F_REWARD, F_HEART, F_HIDDEN,
    F_SAGE, F_ARENA_BEAST, F_BOSS_BODY, F_BOSS_HEAD, F_BOSS_PAW, F_BOSS_FIRE,
    F_WINDMILL, F_ALDWIN, F_AXE, F_SHOP, F_COMPANION,
    F_WAGON, F_WHEEL, F_DRIVER, F_HORSE,
    F_CHANDELIER, F_CRATEARCHER, F_SHUTTER, F_CROSSBOW, F_LURKER, F_DEBRIS, F_EMERGE, F_DOORFX,
    F_RIDER, F_RHORSE, F_BOARDER, F_HAWK, F_ROCK, F_HOUND, F_TORCH,
    F_TROOPER, F_BOMB, F_KNIGHT, F_LANCE, F_PILLAR, F_BOAT, F_RAFT, F_RARCHER, F_RSPEAR, F_FISH, F_HAWK3, F_ROCK3,
    F_BOSS3, F_BOLT3, F_GLOW, F_SERPENT, F_SERPENTB, F_FIGURE,
    F_SNOWBALL, F_BAT, F_HAWK4, F_TENT, F_TSPEAR, F_DRAGON, F_DRAGONP, F_FIRE, F_SORSHA, F_SLASH4, F_CLASH,
    F_SHIELD, F_CRASH, F_ENDING, F_INTROHORSE,
    F_TROLL, F_ORB, F_STONE, F_DOORWAY, F_CART, F_EBOR, F_NECK5, F_HEAD5, F_FLAME5, F_FLAME5B, F_FLAME5C, F_FLAME5D, F_GATE5,
    F_DOOR6, F_KAEL, F_KSLASH, F_STONE6, F_CRUSH6, F_WHIRL6, F_SMOKE6,
    F_BAV, F_SGLOW6, F_SPARK6, F_BALL6, F_LGLOW6, F_CGLOW6, F_BOLT6, F_CANDLE6, F_FLAME6, F_BRAZIER6, F_FIRE6, F_MORTAR6, F_MPART6, F_TROOP6, F_PSPARK6, F_RARCH6,
    F_COUNT
};

typedef struct Actor Actor;
typedef void (*ActorFn)(Actor *a);

struct Actor
{
    u8 fam;                 /* F_*; F_NONE = free */
    u8 active;              /* 1 collidable, 2 drawn only, 0 hidden (dormant) */
    u8 state, phase, sub;
    u8 facing;              /* family-defined (0 right, 1 left for walkers) */
    u8 variant;
    u8 hp, last_hp;         /* energy and its copy (a change = a hit) */
    u8 flags;               /* family-defined */
    u8 hflip;               /* draw mirrored */
    u8 spawn;               /* stage table entry + 1 (0: spawned at run time) */
    s32 x, y;
    s16 vx, vy;
    s16 v[6];               /* family scratch */
    u16 timer, t2;
    Anim anim;
    u16 pose, shown;        /* pose to draw (set by anim_* or directly), last drawn */
    u8 slot;                /* pose cache entry of its VRAM slot (hint) */
    u8 link;                /* family-defined: another actor's slot + 1 (0: none) */
    u8 ng;                  /* bit 0: NEW GAME PLUS energy already doubled; bits 1-7: stage 5's spawning event (src/fam_stage5.c) */
    u8 pad[11];             /* 64 bytes: slot arithmetic by shifts */
};

typedef struct
{
    ActorFn update;
    PoseSet *ps;            /* NULL: never drawn */
    const s16 *boxes;       /* per pose 2 boxes (vs player, vs shots): xOff, w, yOff, h */
    u8 cls;
} Family;

#define ACTORS 142
extern Actor actors[];
extern const u8 pool_lo[C_CLASSES + 1];     /* first slot of each class; [C_CLASSES] = ACTORS */
extern u8 pool_hi[C_CLASSES];               /* one past the highest slot in use, per class */
#define SLEEP 0x80                          /* flags: not updated while far from the camera */
#define IDLE 0x40                           /* flags: not updated until woken (settled bridge logs) */
#define STOOD 0x02                          /* flags (surfaces): stood on this update */
void actors_retire_below(s16 x);            /* every actor left of x removed (stage 2's chase, 2.5) */
void surface_wake(void);                    /* every surface updated again (a bridge collapses) */
bool surface_floor(s16 x, s16 y, s16 *top); /* a foot at (x, y) on a standable surface */
bool surface_under(s16 x, s16 y, s16 *top); /* the same test without marking the surface stood on */
extern const Family *families[F_COUNT];
void families_init(void);
void families_stage(u16 stage);             /* the stage's colouring of the shared families (0, 1, 2) */
void sparkle_add(s16 x, s16 y);             /* sparkles: a particle list (src/families.c) */
void sparkles_clear(void);
void sparkles_update(void);
u16 sparkles_draw(VDPSprite *sat, u16 first, u16 max, s16 view_x, s16 view_y);
u16 logs_draw(VDPSprite *sat, u16 first, u16 max, s16 view_x, s16 view_y);   /* the bridge logs */                   /* resident art; before posecache_init */
extern u16 game_frame;

Actor *actor_new(u16 fam);                      /* NULL if its class pool is full */
void actor_free(Actor *a);
void actor_clear(void);                         /* every pool emptied (restart) */
u16 actor_free_slots(u16 cls);
void actor_anim(Actor *a, u16 chain);           /* start family chain i */
void actor_step(Actor *a);                      /* animation step; pose follows */
/* actor_step inlined (the busiest families: soldiers, beasts, gold, chests). */
static inline __attribute__((always_inline)) void actor_step_i(Actor *a)
{
    Anim *n = &a->anim;
    const u8 *c = n->chain;
    u16 f = n->frame;
    if (c[2 + 2 * f] != 0xFF && !--n->tick)
    {
        if (++f >= c[0])
        {
            u16 loop = c[1 + 2 * c[0]];
            f = loop == 0xFF ? c[0] - 1 : loop;
        }
        n->frame = f;
        n->tick = c[2 + 2 * f];
    }
    a->pose = c[1 + 2 * f];
}
void actors_update(void);
u16 actors_draw(VDPSprite *sat, u16 first, u16 max, s16 view_x, s16 view_y);
/* Box of an actor's current pose: which 0 = vs player, 1 = vs shots. */
/* The actor's box for its shown pose (0 vs the player, 1 vs shots). Inline:
 * the collision passes call these for every shot and enemy each update. */
static inline const s16 *actor_box(const Actor *a, u16 which)
{
    return families[a->fam]->boxes + (a->shown * 2 + which) * 4;
}

/* The arcade's overlap test (combat.md 1.1): boxes touching count. */
static inline bool box_hit(s16 ax, s16 ay, const s16 *ab, s16 bx, s16 by, const s16 *bb)
{
    return (u16)(ax - bx + ab[0] + bb[0]) <= (u16)(ab[1] + bb[1]) &&
           (u16)(ay - by + ab[2] + bb[2]) <= (u16)(ab[3] + bb[3]);
}
u16 actor_count(u16 fam);

/* Camera (player record) in px. */
#define CAM_X (player.cam_x)
#define CAM_Y (player.cam_y)
#define AX(a) ((s16)((a)->x >> 8))
#define AY(a) ((s16)((a)->y >> 8))

/* Shared stage state (specs/stage1_enemies.md 1, combat.md 9). */
extern Actor *actor_now;        /* co-op: the actor being updated (src/actor.c) */
extern u8 two_players;         /* co-op: the 2P approximations on (src/actor.c) */
extern u16 rank;                /* difficulty rank word; high byte = rank */
extern u8 encounter_clear;      /* FF83D0: ordinary enemies explode */
extern u8 arrival;              /* FF8470: the player stands at a camera stop */
extern u8 gate_open;            /* FF838A: the waterfall gate opened */
extern u8 midboss_active;       /* FF83E6 */
extern u8 edge_alive;           /* FF0F40: live edge soldiers */
extern u8 bridge_collapse[2];   /* FF0F09 (long, 0) / FF0F08 (short, 1) */

u16 rnd(void);                  /* game random numbers */
void award(u16 index);          /* score table index (0 = 50, 2 = 100 ...) */
void add_score(u32 bcd);        /* BCD points */
void add_gold(u16 variant);     /* gold table 0x4E70[variant] */
void drop(Actor *a, u16 set);   /* class-14 pickups (stage1_enemies.md 3) */
void effect(u16 fam, s16 x, s16 y, u16 variant);

/* Terrain (the stage collision grid), world px, y up. */
u16 terrain_floor(s16 x, s16 y, s16 *top);      /* inside a floor: its surface y in *top */
bool terrain_wall(s16 x, s16 y, bool left);
u8 terrain_slope_class(s16 x, s16 y);           /* a slope's class (descriptor byte 5 & 15), 0 on flat or none */     /* the player's wall test at a point, no push */
u16 terrain_floor_s(s16 x, s16 y, s16 *top, u8 *slope);
extern u8 terrain_epoch;
void terrain_changed(void);
u8 terrain_flags(s16 x, s16 y);                 /* descriptor flags of the cell */
u8 terrain_shape(s16 x, s16 y);                 /* descriptor floor shape byte (bit 7: flat) */
u8 terrain_id(s16 x, s16 y);                    /* collision id */
const u8 *terrain_column(s16 c, u16 *first, u16 *rows);   /* a column's ids for a probe cache (NULL: use terrain_id) */
bool actor_ground(Actor *a, s16 dx, s16 dy);    /* snap a foot at (dx, dy) to the floor */

void arrow_break(Actor *a);
/* Stage 3 (src/fam_stage3.c). */
extern u8 knight_beaten, fish_end, boat_drift, swell;
extern s16 swell_y;              /* the sea swell's view offset (px) */
void stage3_events_reset(void);
extern u8 boss3_dead;            /* FF83CF (src/fam_boss3.c) */
extern u8 clear_wait;            /* the stage-3 clear waits for the ending figure */
void boss3_reset(void);
void boss3_new_game(void);
/* Stage 4 (src/fam_stage4.c). */
extern u8 dragon_beaten, sorsha_beaten;
void stage4_reset(void);
void stage4_camera(void);
void stage4_events(void);
void stage4_events_clear(void);
void trooper4_gone(void);
u16 flakes_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y);
/* Stage 5 (src/fam_stage5.c). */
extern u8 boss5_wake, boss5_dead;
extern u8 gate_freeze;          /* FF83EA: stage 5's doorway wipe holds the player */
extern u8 gate5_open;           /* the wipe has replaced the tower's wall column (terrain_id) */
extern u8 boss5_layer;          /* src/fam_boss5.c: the necks show (every sprite high) */
void boss5_reset(void);
#define DEFLECT 0x01            /* flags of F_NECK5 / F_HEAD5 (stage 5's boss): shots stop on it, no damage, no contact harm */
#define DEFLECTS(a) (((a)->fam == F_NECK5 || (a)->fam == F_HEAD5) && ((a)->flags & DEFLECT))
void stage5_reset(void);
void stage5_camera(void);
void stage5_events(void);
void stage5_events_clear(void);
void stage5_new_game(void);
void stage3_events(void);      /* src/fam_archer.c: the arrow breaks (wall, floor, the sword) */

#endif
