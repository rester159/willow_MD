#ifndef SPRITE_H
#define SPRITE_H
#include <genesis.h>

/* Prepared sprite poses (tools/posepack.py output) and their animation
 * chains, for any actor family. Two VRAM modes:
 *  - resident: every distinct tile run gets VRAM at stage start;
 *  - cached: a pose's tiles are loaded into the pose cache when it is first
 *    drawn and stay while it is in use; actors showing the same pose share
 *    them (families with too many poses to keep resident). */
typedef struct
{
    const u8 *poses;        /* poses.bin: directory then sprite entries */
    const u8 *tiles;        /* tiles.bin */
    const u8 *anim;         /* anim.bin: chains (count, (pose, ticks)..., loop) */
    u16 count;              /* poses */
    u16 max_tiles;          /* largest pose */
    u16 pal;                /* CRAM line << 13 */
    u16 *vram_of;           /* resident: VRAM tile per pose (count entries); NULL: cached */
    u8 *hint;               /* cached: per pose, the cache entry that last held it */
    u16 slot_tiles;         /* non-zero: one VRAM slot of this size per actor (keyed by its shown field) */
    const s16 *sat;         /* sat.bin: entries ready for the sprite table (tools/satpack.py) */
    u8 one_slot;            /* slot mode: one slot for every actor of the set (they share their pose) */
} PoseSet;

typedef struct
{
    const u8 *chain;        /* current animation chain */
    u8 frame, tick;
} Anim;

/* The pose cache's entries (src/sprite.c). */
#define ENTRIES 48
typedef struct
{
    const void *key;        /* cache: first tile's ROM address; slot: the owner; NULL = free */
    u16 vram, n, used;      /* used: frame number of the last draw */
    u16 loaded;             /* slot: pose in VRAM (0xFFFF none) */
    u8 slot;
#ifdef DBG_CACHE
    const u8 *src;          /* debug: tiles last loaded into it */
    u16 srcn;
#endif
} PoseEntry;
extern PoseEntry pc_ent[ENTRIES];
extern u16 pc_frame;                             /* the pose cache's frame count (posecache_frame) */

void poseset_resident(PoseSet *ps);              /* allocates and loads every pose's tiles */
void poseset_reload(PoseSet *ps);                /* a resident set's (recoloured) tiles again, same VRAM */
const u8 *poseset_chain(const PoseSet *ps, u16 i);
void anim_start(Anim *a, const u8 *chain);
bool anim_step(Anim *a);                         /* TRUE when the pose changed */
u16 anim_pose(const Anim *a);
bool anim_done(const Anim *a);                   /* holding on its last node */
u16 posecache_take(u16 n);                       /* n tiles borrowed from the pose cache (0: none) */
void posecache_give(void);

/* The pose cache: call posecache_init once the fixed sprite art is
 * allocated (it takes the rest of the sprite VRAM) and posecache_frame at
 * the start of each frame's drawing. */
void posecache_init(void);
void posecache_frame(u16 reserved);   /* reserved: tiles other VBlank transfers take this frame */
extern u16 posecache_loads, posecache_misses, posecache_unshown;   /* this frame (measurement) */

/* Writes the pose's sprites at sat[first..] around screen (sx, sy), facing
 * mirrored when hflip. For a cached set the pose may not be loadable this
 * frame (load budget, cache full): then *shown (the actor's last drawn
 * pose) is drawn instead; *shown is updated otherwise. For a slotted set
 * the address of shown also identifies the actor's slot. Returns the
 * sprites written. */
u16 pose_draw(const PoseSet *ps, u16 pose, u16 *shown, VDPSprite *sat, u16 first, s16 sx, s16 sy, bool hflip,
              u8 *slot);   /* slot: the actor's slot hint (slotted sets; NULL otherwise) */

void posecache_flush(void);                       /* forget every entry (the VRAM was lent out) */
void posecache_lend(u16 lo, u16 hi);              /* extra VRAM for the cache (stage 2's chase) */
void posecache_unlend(void);                      /* the lent VRAM taken back (and the cache flushed) */
u16 posecache_ranges(u16 *lo, u16 *hi);          /* the cache's VRAM ranges (at most 8) */

#endif
