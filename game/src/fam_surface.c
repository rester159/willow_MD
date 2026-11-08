/* Standable surfaces (class 6): bridge logs (kind 0) and sinking platforms
 * (kind 1); specs/stage1_enemies.md 5.3, 5.7, arcade routine 2F470.
 * The player's foot probes find them through surface_floor(). The arcade
 * writer of the "stood on" byte is not known (open question 4): here the
 * log under a foot sags 4 px, its neighbours 2. */
#include <genesis.h>
#include "actor.h"
#include "sound.h"
#include "player.h"


/* A foot at (x, y) inside the top band of a standable surface: its top.
 * v[1] holds the surface's x (they never move sideways). A log's picture is
 * a 16-px block centred on its position, the platform's top is 24 above. */
void surface_wake(void)
{
    u16 i;
    for (i = pool_lo[C_SURFACE]; i < pool_lo[C_SURFACE + 1]; i++) actors[i].flags &= ~IDLE;
}

/* The logs by 16-px column of their x (they never move sideways): slot + 1,
 * written when a log starts; a stale entry fails the slot check. */
#define LOG_COLS 512                /* x < 8192: logs are stage 1's (x 1512-2296) */
static u8 log_at[LOG_COLS];

static bool log_top(s16 x, s16 y, u16 col, s16 *top, Actor **hit)
{
    Actor *a;
    s16 t;
    if (col >= LOG_COLS || !log_at[col]) return FALSE;
    a = actors + log_at[col] - 1;
    if (a->fam != F_LOG || a->active != 1 || (u16)(a->v[1] >> 4) != col || (u16)(x - a->v[1] + 8) >= 16) return FALSE;
    t = AY(a) + 8;
    if (y > t || y < t - 12) return FALSE;
    *top = t;
    *hit = a;
    return TRUE;
}

extern u32 surf_other;     /* src/actor.c: slots that may hold a surface other than a log */

/* A surface other than a log under the probe (x, y): its top. */
static inline __attribute__((always_inline)) bool surf_hit(const Actor *a, s16 x, s16 y, s16 *top)
{
    s16 t;
    if (a->fam == F_LOG || a->active != 1 || !a->fam) return FALSE;
    if (a->fam == F_BOAT)          /* stage 3: the deck is the boat's y (checkpoint records put Willow at y 316 on it) */
    {
        if ((u16)(x - AX(a) + 28) >= 56) return FALSE;
        t = AY(a) - 16;                 /* the player's probes are 16 below the feet */
    }
    else if (a->fam == F_PILLAR)        /* stage 3's sinking pillars: the top 32 above (run B: stood at y + 32) */
    {
        if ((u16)(x - AX(a) + 16) >= 32) return FALSE;
        t = AY(a) + 16;
    }
    else if (a->fam == F_STONE)         /* stage 5's stone pillars: box (8, 16, 48, 96), the top 48 above */
    {
        if ((u16)(x - AX(a) + 12) >= 24) return FALSE;
        t = AY(a) + 32;
    }
    else if (a->fam == F_CRUSH6)        /* stage 6's spike crushers: solid, the top 19 above (stage6_families.md 3) */
    {
        if ((u16)(x - AX(a) + 24) >= 48) return FALSE;
        t = AY(a) + 19;
    }
    else if (a->fam == F_WAGON)         /* stage 2's wagon: the player stands at y + 32 (stage2.md 2.2; probes are 16 below) */
    {
        if ((u16)(x - AX(a) + 80) >= 160) return FALSE;
        t = AY(a) + 16;
    }
    else
    {
        if ((u16)(x - a->v[1] + 64) >= 128) return FALSE;
        t = AY(a) + 24;
    }
    if (y > t || y < t - 12) return FALSE;
    *top = t;
    return TRUE;
}

static Actor *surface_find(s16 x, s16 y, s16 *top)
{
    Actor *lo = actors + pool_lo[C_SURFACE], *end = actors + pool_hi[C_SURFACE], *a, *hit;
    u32 m = surf_other;
    u16 k = 0;
    /* Logs: the one or two columns a log's x can be in for this foot. */
    if (log_top(x, y, (u16)(x - 7) >> 4, top, &hit) || log_top(x, y, (u16)(x + 8) >> 4, top, &hit)) return hit;
    /* The other surfaces, in slot order: the slots in surf_other (runs of
     * clear bits skipped 16, 8, 4 at a time), then those past the mask's 32
     * (logs and empty slots never match). */
    while (m)
    {
        if (!(u16)m) { m >>= 16; k += 16; }
        if (!(m & 0xFF)) { m >>= 8; k += 8; }
        if (!(m & 0xF)) { m >>= 4; k += 4; }
        while (!(m & 1)) { m >>= 1; k++; }
        a = lo + k;
        if (a >= end) return NULL;
        if (surf_hit(a, x, y, top)) return a;
        m >>= 1; k++;
    }
    for (a = lo + 32; a < end; a++)
        if (surf_hit(a, x, y, top)) return a;
    return NULL;
}

/* Is a surface under (x, y) (a probe 16 below a foot)? No side effects
 * (the arcade's support test, 0x72A8: src/fam_sorcerer.c). */
bool surface_under(s16 x, s16 y, s16 *top)
{
    return surface_find(x, y, top) != NULL;
}

bool surface_floor(s16 x, s16 y, s16 *top)
{
    Actor *a = surface_find(x, y, top);
    if (a)
    {
        a->flags |= STOOD;
        /* Wake it and its neighbours (they sag 2 px). */
        a->flags &= ~IDLE;
        if (a > actors + pool_lo[C_SURFACE]) a[-1].flags &= ~IDLE;
        if (a + 1 < actors + pool_lo[C_SURFACE + 1]) a[1].flags &= ~IDLE;
        return TRUE;
    }
    return FALSE;
}

void log_update(Actor *a)
{
    switch (a->state)
    {
    case 0:
        a->state = 2;
        a->active = 1;
        a->v[0] = AY(a);                    /* rest height */
        a->v[1] = AX(a);
        if ((u16)(a->v[1] >> 4) < LOG_COLS) log_at[a->v[1] >> 4] = a - actors + 1;
        a->timer = 4;
        a->t2 = 6 * a->variant + 1;         /* collapse delay (0x2F4E8) */
        actor_anim(a, 0);
        return;
    case 2:
        {
            u16 bridge = a->facing == 1;    /* direction 1: short bridge, 0: long */
            s16 want = a->v[0];
            /* The collapse has started, nobody stands on it or a neighbour and
             * it is at rest: only the clocks run (the same steps as below,
             * without the sag tests; the bridge collapse's busiest updates). */
            if (bridge_collapse[bridge] && !((a->flags | a[-1].flags | a[1].flags) & STOOD) && AY(a) == want)
            {
                if (!--a->timer) a->timer = 4;
                if (!--a->t2) { a->state = 4; a->active = 2; a->vy = 0; }
                if (a->v[1] < player.cam_min_x) actor_free(a);
                return;
            }
            /* Settled and nobody near: nothing to do (most logs, most frames). */
            if (!((a->flags | a[-1].flags | a[1].flags) & STOOD) && AY(a) == want && !bridge_collapse[bridge])
            {
                a->timer = 4;
                if (a->v[1] < player.cam_min_x) actor_free(a);
                else a->flags |= IDLE;      /* until a foot or the collapse wakes it */
                return;
            }
            if (a->flags & STOOD) want -= 4;
            else if (a > actors + pool_lo[C_SURFACE] && (a[-1].flags & STOOD) && a[-1].fam == F_LOG) want -= 2;
            else if (a < actors + pool_lo[C_SURFACE + 1] - 1 && (a[1].flags & STOOD) && a[1].fam == F_LOG) want -= 2;
            a->flags &= ~STOOD;
            if (!--a->timer)
            {
                a->timer = 4;
                if (AY(a) > want) a->y -= 1 << 8;
                else if (AY(a) < want) a->y += 1 << 8;
            }
            if (bridge_collapse[bridge] && !--a->t2) { a->state = 4; a->active = 2; a->vy = 0; }
        }
        if (a->v[1] < player.cam_min_x) actor_free(a);
        return;
    case 4:                                 /* falling into the river */
        a->y += a->vy;
        if (a->vy > -1920) a->vy -= 40;
        if (!(game_frame & 3) && terrain_id(AX(a), AY(a)) == 32)
        {
            effect(F_SPLASH, AX(a), AY(a), 0);
            actor_free(a);
            return;
        }
        if (AY(a) < CAM_Y - 64) actor_free(a);
        return;
    }
}

void platform_update(Actor *a)
{
    switch (a->state)
    {
    case 0:
        a->state = 2;
        a->active = 1;
        a->v[1] = AX(a);
        actor_anim(a, 0);
        return;
    case 2:                                 /* wait to be stood on */
        if (a->flags & STOOD) { a->state = 4; a->timer = 64; }
        a->flags &= ~STOOD;
        return;
    case 4:
        a->flags &= ~STOOD;
        if (!--a->timer) { a->state = 6; a->timer = 96; }
        return;
    case 6:                                 /* sink 0.5 px a frame for 96 frames */
        a->flags &= ~STOOD;
        a->y -= 128;
        if (!--a->timer) actor_free(a);
        return;
    }
}
