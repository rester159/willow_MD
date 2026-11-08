/* pose_draw's body, inlined into the actor draw loop (src/actor.c): the
 * call and its nine arguments cost ~300 cycles an actor. */
#ifndef POSEDRAW_H
#define POSEDRAW_H
#include "sprite.h"

u16 cache_get(const u8 *tiles, u16 n, u8 *hint);
u16 slot_get(const PoseSet *ps, const void *owner, u16 *pose, u8 *hint);
extern u16 posecache_unshown;

static inline __attribute__((always_inline)) u16 pose_draw_inline(const PoseSet *ps, u16 pose, u16 *shown, VDPSprite *sat, u16 first, s16 sx, s16 sy, bool hflip, u8 *slot)
{
    const u8 *d, *e;
    u16 ns, e0, k, base;
    if (ps->vram_of) base = ps->vram_of[pose];
    else if (ps->slot_tiles)
    {
        /* slot_get's first test here: the hinted slot is the owner's and
         * already holds the pose (the usual case) without the call. */
        const void *own = ps->one_slot ? (const void *)ps : (const void *)shown;
        u8 h = *slot;
        if (h < ENTRIES && pc_ent[h].key == own && pc_ent[h].loaded == pose) { pc_ent[h].used = pc_frame; base = pc_ent[h].vram; }
        else if (!(base = slot_get(ps, own, &pose, slot))) { posecache_unshown++; return 0; }
    }
    else
    {
        d = ps->poses + pose * 6;
        if (!d[2]) base = 1;
        else
        {
            /* cache_get's first test here: the hinted entry still holds the
             * pose (the usual case) without the call. */
            const u8 *t = ps->tiles + (u32)((d[0] << 8) | d[1]) * 32;
            u8 h = ps->hint[pose];
            if (h < ENTRIES && pc_ent[h].key == t) { pc_ent[h].used = pc_frame; base = pc_ent[h].vram; }
            else base = cache_get(t, d[2], ps->hint + pose);
        }
        if (!base && shown && *shown != pose)       /* not loadable now: keep the last pose */
        {
            pose = *shown;
            d = ps->poses + pose * 6;
            base = d[2] ? cache_get(ps->tiles + (u32)((d[0] << 8) | d[1]) * 32, d[2], ps->hint + pose) : 1;
        }
        if (!base) { posecache_unshown++; return 0; }
    }
    if (shown) *shown = pose;
    d = ps->poses + pose * 6;
    ns = d[3];
    if (first + ns > 80) ns = first < 80 ? 80 - first : 0;     /* the sprite table's 80 entries */
    if (ns)
    {
        /* The prepared entries plus position, link and tile base: 4 words a
         * sprite (68000 loop: ~95 cycles a sprite, GCC's ~150). */
        const s16 *q = ps->sat + (((d[4] << 8) | d[5]) << 2);
        VDPSprite *o = sat + first;
        u16 link = first + 1, n = ns - 1;
        sx += 128; sy += 128;
        (void)hflip; (void)e; (void)e0; (void)k;
        __asm__ volatile (
            "1:\n\t"
            "move.w (%[q])+,%%d0\n\tadd.w %[sy],%%d0\n\tmove.w %%d0,(%[o])+\n\t"
            "move.w (%[q])+,%%d0\n\tor.w %[link],%%d0\n\tmove.w %%d0,(%[o])+\n\taddq.w #1,%[link]\n\t"
            "move.w (%[q])+,%%d0\n\tadd.w %[base],%%d0\n\tmove.w %%d0,(%[o])+\n\t"
            "move.w (%[q])+,%%d0\n\tadd.w %[sx],%%d0\n\tmove.w %%d0,(%[o])+\n\t"
            "dbra %[n],1b"
            : [o] "+a" (o), [q] "+a" (q), [link] "+d" (link), [n] "+d" (n)
            : [sx] "d" (sx), [sy] "d" (sy), [base] "d" (base)
            : "d0", "cc", "memory");
    }
    return ns;
}

#endif
