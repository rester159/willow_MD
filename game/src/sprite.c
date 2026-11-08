/* Prepared sprite poses and animation chains (src/sprite.h). Pose format
 * (tools/posepack.py): directory of {u16 tile offset, u8 tiles, u8 sprites,
 * u16 entry offset}, then entries {s16 dy, u8 size (bits 0-3; bits 4-5: CRAM
 * line, XORed into the set's line; bit 7: H-flip), u8 tile, s16 dx} around
 * the actor's origin (screen y down). */
#include <genesis.h>
#include "sprite.h"
#include "vram.h"

void poseset_resident(PoseSet *ps)
{
    u16 i, j;
    for (i = 0; i < ps->count; i++)
    {
        const u8 *d = ps->poses + i * 6;
        u16 t0 = (d[0] << 8) | d[1], n = d[2];
        for (j = 0; j < i; j++)                 /* identical / mirrored poses share tiles */
        {
            const u8 *e = ps->poses + j * 6;
            if (((e[0] << 8) | e[1]) == t0) break;
        }
        if (j < i) ps->vram_of[i] = ps->vram_of[j];
        else
        {
            ps->vram_of[i] = n ? vram_alloc(n) : 0;
            if (n) VDP_loadTileData((const u32 *)(ps->tiles + (u32)t0 * 32), ps->vram_of[i], n, DMA);
        }
    }
}

/* A resident set's tiles again, into the VRAM it already holds (another
 * stage's colouring of the same poses). */
void poseset_reload(PoseSet *ps)
{
    u16 i, j;
    for (i = 0; i < ps->count; i++)
    {
        const u8 *d = ps->poses + i * 6;
        u16 t0 = (d[0] << 8) | d[1], n = d[2];
        for (j = 0; j < i; j++)
            if (ps->vram_of[j] == ps->vram_of[i]) break;
        if (j == i && n) VDP_loadTileData((const u32 *)(ps->tiles + (u32)t0 * 32), ps->vram_of[i], n, DMA);
    }
}

const u8 *poseset_chain(const PoseSet *ps, u16 i)
{
    const u8 *a = ps->anim;
    while (i--) a += 2 + 2 * a[0];
    return a;
}

void anim_start(Anim *a, const u8 *chain)
{
    a->chain = chain;
    a->frame = 0;
    a->tick = chain[2];
}

bool anim_step(Anim *a)
{
    const u8 *c = a->chain;
    u16 loop;
    if (c[2 + 2 * a->frame] == 0xFF || --a->tick) return FALSE;
    if (++a->frame >= c[0])
    {
        loop = c[1 + 2 * c[0]];
        a->frame = loop == 0xFF ? c[0] - 1 : loop;
    }
    a->tick = c[2 + 2 * a->frame];
    return TRUE;
}

u16 anim_pose(const Anim *a)
{
    return a->chain[1 + 2 * a->frame];
}

bool anim_done(const Anim *a)
{
    const u8 *c = a->chain;
    return a->frame == c[0] - 1 && (c[2 + 2 * a->frame] == 0xFF || c[1 + 2 * c[0]] == 0xFF);
}

/* Pose cache and actor slots, over what src/vram.c has left (several
 * separate ranges; first fit between the entries of a range).
 *  - Cache entries are keyed by tile data (ROM address), so mirrored and
 *    identical poses share; one not drawn this frame may be evicted, least
 *    recently used first.
 *  - Slots (sets with slot_tiles): one per actor, sized for the family's
 *    largest pose; the actor's pose is DMA'd into it when it changes. A slot
 *    not drawn for 2 frames is reclaimed.
 * At most LOAD_BUDGET tiles are DMA'd a frame; past it an actor keeps the
 * pose already in VRAM. */
#define RANGES 8
/* Tiles a VBlank can take (with the sprite table, palettes and plane
 * words); the stage's column tiles and Willow's pose come first. */
#ifndef LOAD_BUDGET
#ifndef LOAD_BUDGET
#define LOAD_BUDGET 96         /* tiles a frame: at 160 the VBlank could overrun (stage 2 chase 12 of 401 frames; stage 1 route frame 646) */
#endif
#endif
#ifndef ROOM_LATE
#define ROOM_LATE 150
#endif
#ifndef LOAD_LATE
#define LOAD_LATE 196
#endif
typedef PoseEntry Entry;        /* src/sprite.h: pose_draw_inline reads a hinted entry itself */
#define ent pc_ent
#define frame pc_frame
Entry ent[ENTRIES];
u16 frame;
static u16 rlo[RANGES], rhi[RANGES], nranges, budget;
static u16 nkeys;               /* entries in use: room() skips its free-entry search when all are */
static u16 taken, taken_r;     /* tiles borrowed off range taken_r's top (posecache_take) */
u16 posecache_loads, posecache_misses, posecache_unshown;
#ifdef DBG_LOADS
const PoseSet *dl_ps[8]; u16 dl_n[8], dl_c[8], dl_cache, dl_cachec;
#endif
#ifdef DBG_UNSHOWN
u16 why_budget, why_room, why_slot;
#endif   /* unshown: draws refused this frame (no room) */
#ifndef LENT_BUDGET
#define LENT_BUDGET 96
#endif
static u8 lent;            /* the chase lent the fort's pool tiles (posecache_lend) */
extern u8 two_players, stage_index;
extern u8 heavy;                      /* main.c: frames left of the deadline rule (a busy scene: evictions go on as they come) */
static u8 room_held, room_waits;      /* eviction put off this frame; frames in a row (1 at most: a new actor must not stay unseen) */
static u16 room_failed;     /* a request this frame found no room: later ones of this size or more fail at once */

/* Free VRAM of each range as sorted extents (start, length): a gap search
 * is a first fit over a few extents. */
typedef struct { u16 at, n; } Extent;
static Extent fx[RANGES][ENTRIES + 2];
static u8 nfx[RANGES];

static void fx_reset(void)
{
    u16 r;
    for (r = 0; r < nranges; r++) { fx[r][0].at = rlo[r]; fx[r][0].n = rhi[r] - rlo[r]; nfx[r] = 1; }
}

static void ext_move(Extent *to, const Extent *from, u16 n)  /* overlapping moves either way */
{
    if (to < from) while (n--) *to++ = *from++;
    else { to += n; from += n; while (n--) *--to = *--from; }
}

static u16 range_of(u16 vram)
{
    u16 r;
    for (r = 0; r < nranges; r++) if (vram >= rlo[r] && vram < rhi[r]) return r;
    return 0;
}

/* Take n tiles at vram (inside a free extent of range r). */
static void fx_take(u16 r, u16 vram, u16 n)
{
    Extent *e = fx[r];
    u16 i, k = nfx[r];
    for (i = 0; i < k; i++)
        if (vram >= e[i].at && vram < e[i].at + e[i].n)
        {
            u16 end = e[i].at + e[i].n;
            if (vram == e[i].at) { e[i].at += n; e[i].n -= n; if (!e[i].n) { ext_move(e + i, e + i + 1, (k - i - 1)); nfx[r]--; } }
            else if (vram + n == end) e[i].n -= n;
            else
            {
                ext_move(e + i + 2, e + i + 1, (k - i - 1));
                e[i + 1].at = vram + n; e[i + 1].n = end - vram - n;
                e[i].n = vram - e[i].at;
                nfx[r]++;
            }
            return;
        }
}

/* Give n tiles at vram back, merging with neighbours; returns the length
 * of the free extent it ends up in. */
static u16 fx_give(u16 vram, u16 n)
{
    u16 r = range_of(vram);
    Extent *e = fx[r];
    u16 i, k = nfx[r];
    for (i = 0; i < k && e[i].at < vram; i++) ;
    if (i > 0 && e[i - 1].at + e[i - 1].n == vram)
    {
        e[i - 1].n += n;
        if (i < k && vram + n == e[i].at) { e[i - 1].n += e[i].n; ext_move(e + i, e + i + 1, (k - i - 1)); nfx[r]--; }
        return e[i - 1].n;
    }
    if (i < k && vram + n == e[i].at) { e[i].at = vram; e[i].n += n; return e[i].n; }
    ext_move(e + i + 1, e + i, (k - i));
    e[i].at = vram; e[i].n = n;
    nfx[r]++;
    return n;
}

/* Key -> entry index, open addressing (128 slots: 0xFF empty, 0xFE
 * deleted); a miss no longer scans all the entries. */
#define HSIZE 128
static u8 htab[HSIZE];
static u8 hdead;

static u16 hslot(const void *k)
{
    u32 v = (u32)k;
    return ((v >> 4) ^ (v >> 11) ^ (v >> 17)) & (HSIZE - 1);
}

static void hreset(void)
{
    memset(htab, 0xFF, sizeof htab);
    hdead = 0;
}

static u16 hfind(const void *k)
{
    u16 h = hslot(k), n;
    for (n = 0; n < HSIZE; n++, h = (h + 1) & (HSIZE - 1))
    {
        u8 i = htab[h];
        if (i == 0xFF) return 0xFF;
        if (i < ENTRIES && ent[i].key == k) return i;
    }
    return 0xFF;
}

static void hadd(const void *k, u16 idx)
{
    u16 h = hslot(k);
    while (htab[h] < 0xFE) h = (h + 1) & (HSIZE - 1);
    if (htab[h] == 0xFE) hdead--;
    htab[h] = idx;
}

static void hdel(const void *k)
{
    u16 h = hslot(k), n;
    for (n = 0; n < HSIZE; n++, h = (h + 1) & (HSIZE - 1))
    {
        u8 i = htab[h];
        if (i == 0xFF) return;
        if (i < ENTRIES && ent[i].key == k) { htab[h] = 0xFE; hdead++; break; }
    }
    if (hdead > HSIZE / 2)                  /* too many tombstones: rebuild */
    {
        u16 j;
        hreset();
        for (j = 0; j < ENTRIES; j++) if (ent[j].key && ent[j].key != k) hadd(ent[j].key, j);
#ifdef CTIME
        { extern u16 ct_rebuild; ct_rebuild++; }
#endif
    }
}

void posecache_init(void)
{
    u16 n;
    memset(ent, 0, sizeof ent);
    nkeys = 0;
    nranges = 0;
    taken = 0;
    while (nranges < RANGES && (n = vram_largest()) > 0)
    {
        rlo[nranges] = vram_alloc(n);
        rhi[nranges] = rlo[nranges] + n;
        nranges++;
    }
    fx_reset();
    hreset();
}

/* Stage 2's chase lends the cache the fort's pool tiles (unused once the
 * road fills the view); taken back before the stage reloads them. */
void posecache_lend(u16 lo, u16 hi)
{
    if (lent || nranges >= RANGES) return;
    rlo[nranges] = lo; rhi[nranges] = hi;
    fx[nranges][0].at = lo; fx[nranges][0].n = hi - lo; nfx[nranges] = 1;
    nranges++;
    lent = 1;
}

/* Stage 5's doorway wipe borrows n tiles from the top of the largest range
 * while its picture shows (src/fam_stage5.c); posecache_init gives them
 * back too. Returns the first borrowed tile (0: none). */
u16 posecache_take(u16 n)
{
    u16 r, best = 0;
    if (taken) return rhi[taken_r];
    for (r = 1; r < nranges; r++) if (rhi[r] - rlo[r] > rhi[best] - rlo[best]) best = r;
    if (!nranges || rhi[best] - rlo[best] < n + 64) return 0;
    rhi[best] -= n;
    taken = n; taken_r = best;
    posecache_flush();
    return rhi[best];
}

/* Co-op: n tiles for the second player's pose and HUD colours, kept until
 * the next boot (src/coop.c): off the top of the smallest range that holds
 * them, so the largest stays whole for its borrowers (the shop's sprite
 * art, stage 5's doorway wipe); never the doorway's range or the chase's
 * loan. Returns the first tile (0: none). */
u16 posecache_p2(u16 n)
{
    u16 r, best = 0xFFFF;
    for (r = 0; r < nranges; r++)
    {
        if (taken && r == taken_r) continue;
        if (lent && r == nranges - 1) continue;   /* the chase's loan is given back */
        if (rhi[r] - rlo[r] < n) continue;
        if (best == 0xFFFF || rhi[r] - rlo[r] < rhi[best] - rlo[best]) best = r;
    }
    if (best == 0xFFFF) return 0;
    rhi[best] -= n;
    posecache_flush();
    return rhi[best];
}

void posecache_give(void)
{
    if (!taken) return;
    rhi[taken_r] += taken;
    taken = 0;
    posecache_flush();
}

void posecache_unlend(void)
{
    if (!lent) return;
    nranges--;
    lent = 0;
    posecache_flush();
}

/* Everything in the cache forgotten (its VRAM was lent out). */
void posecache_flush(void)
{
    memset(ent, 0, sizeof ent);
    nkeys = 0;
    fx_reset();
    hreset();
}

u16 posecache_ranges(u16 *lo, u16 *hi)
{
    u16 i;
    for (i = 0; i < nranges; i++) { lo[i] = rlo[i]; hi[i] = rhi[i]; }
    return nranges;
}

#ifdef DBG_CACHE
u16 dbg_overlap, dbg_overlap_frame, dbg_bad, dbg_bad_frame, dbg_bad_vram, dbg_bad_slot;
static void check_overlap(void)
{
    u16 i, k;
    /* every entry's VRAM still holds what was loaded into it */
    for (i = 0; i < ENTRIES; i++)
        if (ent[i].key && ent[i].src && frame - ent[i].used < 2)
        {
            const u16 *src = (const u16 *)ent[i].src;
            u16 a = ent[i].vram * 32;
            *(vu32 *)VDP_CTRL_PORT = ((u32)(a & 0x3FFF) << 16) | (a >> 14);
            for (k = 0; k < ent[i].srcn * 16; k++)
                if (*(vu16 *)VDP_DATA_PORT != src[k])
                {
                    dbg_bad++; if (!dbg_bad_frame) { dbg_bad_frame = frame; dbg_bad_vram = ent[i].vram + k / 16; dbg_bad_slot = ent[i].slot; }
                    break;
                }
        }
    u16 j;
    for (i = 0; i < ENTRIES; i++)
        if (ent[i].key)
            for (j = i + 1; j < ENTRIES; j++)
                if (ent[j].key && ent[i].vram < ent[j].vram + ent[j].n && ent[j].vram < ent[i].vram + ent[i].n)
                { dbg_overlap++; if (!dbg_overlap_frame) dbg_overlap_frame = frame; }
}
#endif

#ifdef CTIME
u16 ct_room, ct_rebuild, ct_max, ct_load, ct_slot, ct_cg;
#endif
void posecache_frame(u16 reserved)
{
#ifdef CTIME
    ct_room = ct_load = ct_slot = ct_cg = 0;
#endif
#ifdef DBG_CACHE
    check_overlap();
#endif
    frame++;
    {
        /* The chase streams road and far tiles every frame: a smaller share
         * keeps VBlank's transfers inside VBlank (an overrun delays the HUD
         * split interrupt at line 15: measured 12 of 401 frames at 160, 1 at 112, 0 at 96). */
        extern u8 boss5_layer;          /* stage 5's boss: its picture changes take VBlank time too */
        u16 cap = lent || boss5_layer ? LENT_BUDGET : LOAD_BUDGET;
        budget = reserved >= cap ? 0 : cap - reserved;
    }
    room_waits = room_held ? room_waits + 1 : 0;
    room_held = 0;
    room_failed = 0;
    posecache_loads = posecache_misses = posecache_unshown = 0;
}

/* A gap of n tiles in range r, or 0: the first free extent that fits. */
static u16 gap(u16 r, u16 n)
{
    const Extent *e = fx[r];
    u16 i;
    for (i = 0; i < nfx[r]; i++) if (e[i].n >= n) return e[i].at;
    return 0;
}

/* Room for n tiles: a free entry and VRAM. When none is free, entries not
 * drawn this frame (slots: for 2 frames) go until a free extent is long
 * enough, then the ranges are searched once. Returns the entry or NULL. */
static Entry *room(u16 n)
{
    u16 i, r, at, free_e = ENTRIES;
    if (room_failed && n >= room_failed) return NULL;
    if (nkeys < ENTRIES) for (i = 0; i < ENTRIES; i++) if (!ent[i].key) { free_e = i; break; }
    if (free_e < ENTRIES)
        for (r = 0; r < nranges; r++)
            if ((at = gap(r, n)) != 0) { ent[free_e].vram = at; ent[free_e].n = n; fx_take(r, at, n); return ent + free_e; }
    /* No free room: evicting costs 5-10 lines. Late in a frame that waits a
     * frame (the pose in VRAM stays; a new actor shows one frame later):
     * this is what made the stage 1 route's one late frame. */
    if (room_waits < 1 && !heavy && !two_players && stage_index != 4) {   /* (stage 5's crowds and 2P: measured no better) */ u16 l = *(vu16 *)0xC00008 >> 8; if (l >= ROOM_LATE && l < 224) { room_held = 1; return NULL; } }
    for (i = 0; i < ENTRIES; i++)
    {
        u16 a = frame - ent[i].used;
        if (ent[i].key && a >= (ent[i].slot ? 2 : 1))
        {
            u16 got;
            hdel(ent[i].key);
            ent[i].key = NULL;
            nkeys--;
            got = fx_give(ent[i].vram, ent[i].n);
            if (free_e == ENTRIES) free_e = i;
            if (got >= n) break;            /* enough room now: keep the rest cached */
        }
    }
    if (free_e < ENTRIES)
        for (r = 0; r < nranges; r++)
            if ((at = gap(r, n)) != 0) { ent[free_e].vram = at; ent[free_e].n = n; fx_take(r, at, n); return ent + free_e; }
    room_failed = n;
    return NULL;
}

static void load(Entry *e, const u8 *tiles, u16 n)
{
#ifdef CTIME
    extern u16 ct_load; u16 t0 = *(vu16 *)0xC00008 >> 8;
#endif
#ifdef DBG_CACHE
    e->src = tiles; e->srcn = n;
#endif
    DMA_queueDma(DMA_VRAM, (void *)tiles, e->vram * 32, n * 16, 2);
    budget -= n;
    posecache_loads += n;
    /* The frame is running out: the rest wait a frame with the poses already
     * in VRAM (each load costs over a line; a late frame starts the 2 s
     * deadline rule, main.c). */
    { u16 l = *(vu16 *)0xC00008 >> 8; if (l >= LOAD_LATE && l < 224) budget = 0; }
#ifdef CTIME
    ct_load += ((*(vu16 *)0xC00008 >> 8) - t0) & 0xFF;
#endif
}

u16 cache_get(const u8 *tiles, u16 n, u8 *hint)
{
    u16 i;
    Entry *e;
    if (*hint < ENTRIES && ent[*hint].key == tiles) { ent[*hint].used = frame; return ent[*hint].vram; }
    if ((i = hfind(tiles)) < ENTRIES) { ent[i].used = frame; *hint = i; return ent[i].vram; }
    posecache_misses++;
#ifdef DBG_UNSHOWN
    { extern u16 why_budget, why_room; if (n > budget) { why_budget++; return 0; } if (!(e = room(n))) { why_room++; return 0; } }
#else
    {
#ifdef CTIME
    extern u16 ct_room, ct_rebuild, ct_max;
    u16 t0 = *(vu16 *)0xC00008 >> 8;
#endif
    if (n > budget || !(e = room(n))) return 0;
#ifdef CTIME
    { u16 d = ((*(vu16 *)0xC00008 >> 8) - t0) & 0xFF; ct_room += d; if (d > ct_max) ct_max = d; }
#endif
    }
#endif
    e->key = tiles; e->used = frame; e->slot = 0;
    nkeys++;
#ifdef DBG_LOADS
    { extern u16 dl_cache, dl_cachec; dl_cache += n; dl_cachec++; }
#endif
    hadd(tiles, e - ent);
    *hint = e - ent;
    load(e, tiles, n);
    return e->vram;
}

/* The owner's slot with pose in it (loaded now if needed and the budget
 * allows); returns its VRAM and the pose actually there in *pose. */
u16 slot_get(const PoseSet *ps, const void *owner, u16 *pose, u8 *hint)
{
    u16 i;
    Entry *e = NULL;
    const u8 *d;
    if (*hint < ENTRIES && ent[*hint].key == owner) e = ent + *hint;
    else if ((i = hfind(owner)) < ENTRIES) { e = ent + i; *hint = i; }
    if (!e)
    {
#ifdef DBG_UNSHOWN
        if (!(e = room(ps->slot_tiles))) { extern u16 why_slot; why_slot++; return 0; }
#else
#ifdef CTIME
        { extern u16 ct_slot; u16 t0 = *(vu16 *)0xC00008 >> 8; e = room(ps->slot_tiles); ct_slot += ((*(vu16 *)0xC00008 >> 8) - t0) & 0xFF; }
        if (!e) return 0;
#else
        if (!(e = room(ps->slot_tiles))) return 0;
#endif
#endif
        e->key = owner; e->slot = 1; e->loaded = 0xFFFF;
        nkeys++;
        hadd(owner, e - ent);
        *hint = e - ent;
    }
    e->used = frame;
    d = ps->poses + *pose * 6;
    if (e->loaded != *pose)
    {
        if (d[2] <= budget)
        {
            if (d[2]) load(e, ps->tiles + (u32)((d[0] << 8) | d[1]) * 32, d[2]);
#ifdef DBG_LOADS
            { extern const PoseSet *dl_ps[8]; extern u16 dl_n[8], dl_c[8]; u16 j; for (j = 0; j < 8 && dl_ps[j] && dl_ps[j] != ps; j++) ; if (j < 8) { dl_ps[j] = ps; dl_n[j] += d[2]; dl_c[j]++; } }
#endif
            e->loaded = *pose;
        }
        else if (e->loaded == 0xFFFF) return 0;
        else *pose = e->loaded;                 /* over budget: the pose already there */
    }
    return e->vram;
}

#include "posedraw.h"

u16 pose_draw(const PoseSet *ps, u16 pose, u16 *shown, VDPSprite *sat, u16 first, s16 sx, s16 sy, bool hflip, u8 *slot)
{
    return pose_draw_inline(ps, pose, shown, sat, first, sx, sy, hflip, slot);
}
