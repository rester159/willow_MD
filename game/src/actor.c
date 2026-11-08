/* Stage actors: pools, update order, drawing, rewards and terrain queries
 * (src/actor.h). The families live in src/fam_*.c. */
#include <genesis.h>
#include "stagedata.h"
#include "actor.h"
#include "frontend.h"
#include "spell.h"
#include "posedraw.h"
#include "player.h"
#include "coop.h"
#include "hud.h"
#include "game.h"
#include "sound.h"


/* Pool ranges per class (arcade pool sizes trimmed to what stage 1 needs:
 * 30 bridge logs, a few enemies at once). */
const u8 pool_lo[C_CLASSES + 1] = { 0, 32, 66, 82, 106, 126, 138, 142 };   /* objects: 5 chests, 2 shop doors, the hidden object, rewards, the companion */
Actor actors[ACTORS];
u8 pool_hi[C_CLASSES];
_Static_assert(sizeof(Actor) == 64, "actor records are 64 bytes");

u16 rank;
Actor *actor_now;           /* co-op: the actor being updated (its last hit's owner scores, src/coop.c) */
/* Two players (co-op, Home): enables the 2P approximations (AGENTS.md
 * "Two-player approximations"); 0 in one-player play, which stays exact.
 * Set by the co-op code; test builds -DTWO_PLAYERS_TEST set it. */
#ifdef TWO_PLAYERS_TEST
u8 two_players = 1;
#else
u8 two_players;
#endif
#ifdef FAMTIME
u32 famtime[128];
u32 famcount[128];
#endif
#ifdef DRAWTIME
u32 drawtime[128];
#endif
#ifdef PROFA
u16 profa[16];
#endif
u8 encounter_clear, arrival, gate_open, midboss_active, edge_alive;
u8 bridge_collapse[2];

static u16 seed = 0x2472;                  /* the arcade's boot value (FF8284) */

/* The arcade's generator (0x1D8E): a 16-bit shift register, bit 15 fed by
 * bits 1 xor 9; the low byte is the number. Its call order differs from the
 * arcade's, so the sequence of choices is not the arcade's (fidelity
 * contract). */
u16 rnd(void)
{
    u16 s = seed >> 1;
    seed = s | (((s ^ (seed >> 9)) & 1) << 15);
    return seed & 255;
}

/* actor_ground's last collision cell per actor slot: column, row (x >> 4,
 * y >> 4), terrain_epoch, id. A cell's id depends only on the cell and the
 * terrain (tid: patches and the doorway are whole cells; world_top is 15 mod
 * 16), and every terrain change bumps terrain_epoch (never 0). A slot's
 * entry is only reused while its column, row and epoch match. */
static s16 ag_c[ACTORS], ag_r[ACTORS];
static u8 ag_ep[ACTORS], ag_id[ACTORS];

/* The surface slots (bit i: pool_lo[C_SURFACE] + i) that may hold a
 * standable surface other than a bridge log (set when one starts, cleared
 * when it is freed): surface_find walks only these, and every slot past
 * the 32nd. */
u32 surf_other;

Actor *actor_new(u16 fam)
{
    u16 c = families[fam]->cls, i;
    for (i = pool_lo[c]; i < pool_lo[c + 1]; i++)
        if (!actors[i].fam)
        {
            Actor *a = actors + i;
            memset(a, 0, sizeof *a);
            ag_ep[i] = 0;
            a->fam = fam;
            a->active = 1;
            a->slot = 0xFF;
            if (i + 1 > pool_hi[c]) pool_hi[c] = i + 1;
            if (c == C_SURFACE) { extern u8 surf_dirty; surf_dirty = 1; if (fam != F_LOG && i - pool_lo[C_SURFACE] < 32) surf_other |= 1UL << (i - pool_lo[C_SURFACE]); }
            return a;
        }
    return NULL;
}

void actor_free(Actor *a)
{
    u16 i = a - actors - pool_lo[C_SURFACE];
    if (i < 34) { extern u8 surf_dirty; surf_dirty = 1; if (i < 32) surf_other &= ~(1UL << i); }
    a->fam = F_NONE;
}

void actor_clear(void)
{
    u16 c;
    memset(actors, 0, sizeof actors);
    memset(ag_ep, 0, sizeof ag_ep);
    surf_other = 0;
    { extern u8 surf_dirty; surf_dirty = 1; }
    sparkles_clear();
    for (c = 0; c < C_CLASSES; c++) pool_hi[c] = pool_lo[c];
}

u16 actor_free_slots(u16 cls)
{
    u16 i, n = 0;
    for (i = pool_lo[cls]; i < pool_lo[cls + 1]; i++) if (!actors[i].fam) n++;
    return n;
}

u16 actor_count(u16 fam)
{
    u16 i, n = 0;
    for (i = 0; i < ACTORS; i++) if (actors[i].fam == fam) n++;
    return n;
}

void actor_anim(Actor *a, u16 chain)
{
    anim_start(&a->anim, poseset_chain(families[a->fam]->ps, chain));
    a->pose = anim_pose(&a->anim);
}

/* anim_step then anim_pose (src/sprite.c), without the two calls: ~11
 * actors an update in busy scenes. */
void actor_step(Actor *a)
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

/* The bridge logs' common updates without the call (src/fam_surface.c
 * log_update does the same steps; up to 30 logs an update at the stage 1
 * bridge collapse): one counting down to its fall, at rest and untouched;
 * one falling, on the 3 frames in 4 without the water test. FALSE: the
 * full update is needed. */
static inline __attribute__((always_inline)) bool log_quick(Actor *a)
{
    if (a->state == 4)
    {
        if (!(game_frame & 3)) return FALSE;
        a->y += a->vy;
        if (a->vy > -1920) a->vy -= 40;
        if (AY(a) < CAM_Y - 64) actor_free(a);
        return TRUE;
    }
    if (a->state == 2 && bridge_collapse[a->facing == 1] && !((a->flags | a[-1].flags | a[1].flags) & STOOD) && AY(a) == a->v[0])
    {
        if (!--a->timer) a->timer = 4;
        if (!--a->t2) { a->state = 4; a->active = 2; a->vy = 0; }
        if (a->v[1] < player.cam_min_x) actor_free(a);
        return TRUE;
    }
    return FALSE;
}

/* Placed objects and idle surfaces far from the camera (SLEEP) are not
 * updated: their wake-up rules only look on screen. */
#ifdef DBG_ITER
u16 dbg_iter;
#endif
#ifdef DBG_ITER2
u16 dbg_it2[C_CLASSES][3];
#endif
/* The usual case (one player, no time-stop spell, not NEW GAME PLUS, not
 * stage 5's half-rate spearmen): the loop below without the tests that
 * cannot apply, the same order and the same rules (~150 cycles an actor
 * less). */
#if !defined(FAMTIME) && !defined(EXP_NOCALL) && !defined(EXP_SKIPFAM) && !defined(EXP_NEARHALF) && !defined(DBG_ITER) && !defined(DBG_ITER2) && !defined(NO_LEANLOOP)
#define LEANLOOP 1
static void actors_update_lean(s16 cx)
{
    u16 c;
    u16 wx = 192 - cx;                          /* (u16)(AX + wx) > 768: far from the camera */
    for (c = 0; c < C_CLASSES; c++)
    {
        u8 hi0 = pool_hi[c];
        Actor *a = actors + pool_lo[c], *end = actors + hi0, *last = a - 1;
        for (; a < end; a++)
        {
            u8 f = a->fam, fl;
            if (!f) continue;
            fl = a->flags;
            if (fl & (SLEEP | IDLE))
                if ((fl & IDLE) || (u16)(AX(a) + wx) > 768) { last = a; continue; }
            if (f == F_LOG && log_quick(a)) { if (a->fam) last = a; continue; }
            families[f]->update(a);
            if ((f = a->fam) != 0)
            {
                last = a;
                if (a->state && f != F_LOG) a->flags &= ~SLEEP;    /* awake: updates everywhere */
            }
        }
        if (pool_hi[c] == hi0) pool_hi[c] = last + 1 - actors;     /* else one spawned past the end */
    }
}
#endif

void actors_update(void)
{
    sparkles_update();
    u16 c;
    s16 cx = CAM_X;
#ifdef LEANLOOP
    if (!two_players && !time_stop && !fe.plus && stage_index != 4) { actors_update_lean(cx); actor_now = NULL; return; }
#endif
    for (c = 0; c < C_CLASSES; c++)
    {
        u8 hi0 = pool_hi[c];
        /* co-op (src/coop.c): 0 one player; 2 enemies and bosses aim at the
         * nearer player; 1 objects (the companion, shop doors); 3 the rest */
        u8 co = !two_players ? 0 : (c == C_ENEMY || c == C_BOSS) ? 2 : c == C_OBJECT ? 1 : 3;
        Actor *a = actors + pool_lo[c], *end = actors + hi0, *last = a - 1;
        for (; a < end; a++)
        {
#ifdef DBG_ITER
            { extern u16 dbg_iter; dbg_iter++; }
#endif
#ifdef DBG_ITER2
            { extern u16 dbg_it2[C_CLASSES][3]; dbg_it2[c][!a->fam ? 0 : (a->flags & IDLE) || ((a->flags & SLEEP) && (u16)(AX(a) - cx + 192) > 768) ? 1 : 2]++; }
#endif
            if (!a->fam) continue;
            if (a->flags & (SLEEP | IDLE))
                if ((a->flags & IDLE) || (u16)(AX(a) - cx + 192) > 768) { last = a; continue; }
            if (time_stop && (c == C_ENEMY || c == C_ESHOT || c == C_SURFACE) && a->fam != F_CHANDELIER && a->fam != F_WAGON)
            { last = a; continue; }     /* spell 10 (FF83D2): ordinary enemies, their shots, moving surfaces stop */
#ifndef NO_FARHALF
            /* Approximation (user, 2026-11-03: "allow small approximations"
             * for stage 5's busiest spot): in stage 5 an active spearman or
             * bat more than 64 px outside the view updates every second
             * frame, so it moves and counts down at half speed until it nears
             * the view. M1_NOTES.md lists it as a deviation. */
            if (c == C_ENEMY && stage_index == 4 && a->state == 2 && (a->fam == F_SPEARMAN || a->fam == F_BAT || a->fam == F_ARCHER) &&
                ((game_frame + (u16)(a - actors)) & 1) &&
                ((u16)(AX(a) - cx + 64) > 448 || (u16)(AY(a) - CAM_Y + 64) > 384))
            { last = a; continue; }
#endif
#ifdef EXP_SKIPFAM
            if (a->fam == EXP_SKIPFAM && a->state == 2) { last = a; continue; }   /* measurement builds only */
#endif
#ifdef EXP_NEARHALF
            /* Measurement builds only: an active enemy more than 128 px from the
             * player (horizontally or vertically) updates every second frame. */
            if (two_players && c == C_ENEMY && a->state == 2 && ((game_frame + (u16)(a - actors)) & 1) &&
                ((u16)(AX(a) - (s16)(player.x >> 8) + 128) > 256 || (u16)(AY(a) - (s16)(player.y >> 8) + 128) > 256))
            { last = a; continue; }
#endif
#ifndef FAMTIME
            if (a->fam == F_LOG && log_quick(a)) { if (a->fam) last = a; continue; }
#endif
#ifdef FAMTIME
            {
                /* Measurement builds: real time per family (V and H counter, 1/256 line). */
                extern u32 famtime[128];
                u16 f0 = a->fam, h0, h1;
                __asm__ volatile ("move.w #0x2700,%sr");    /* interrupts held: their time is not the family's */
                h0 = *(vu16 *)0xC00008;
                families[a->fam]->update(a);
                h1 = *(vu16 *)0xC00008;
                __asm__ volatile ("move.w #0x2000,%sr");
                {
                    s32 t = (s32)(((h1 >> 8) - (h0 >> 8) + 262) % 262) * 256 + (s16)((h1 & 0xFF) - (h0 & 0xFF));
                    if (t < 0) t += 256;            /* H wraps before V steps */
                    if (t < 120 * 256) famtime[f0] += t;
                    { extern u32 famcount[128]; famcount[f0]++; }   /* the V counter's jump in VBlank (0xEA -> 0xE5): dropped */
                }
            }
#elif defined(EXP_NOCALL)
            ;                               /* measurement builds: the loop without the families */
#else
            if (!co) families[a->fam]->update(a);
            else if (co == 2 || (co == 1 && (a->fam == F_COMPANION || a->fam == F_SHOP)))
                coop_actor_update(a, c);    /* co-op: aims at the nearer player; who scores (src/coop.c) */
            else { actor_now = a; families[a->fam]->update(a); }
#endif
            if (a->fam && fe.plus && !(a->ng & 1) && a->hp && (c == C_ENEMY || c == C_BOSS))
            {
                /* NEW GAME PLUS (Home, AGENTS.md): double energy, set once
                 * the family has given it. */
                a->ng |= 1;
                a->hp = a->last_hp = a->hp > 127 ? 255 : a->hp * 2;
            }
            if (a->fam)
            {
                last = a;
                if (a->state && a->fam != F_LOG) a->flags &= ~SLEEP;    /* awake: updates everywhere */
            }
        }
        if (pool_hi[c] == hi0) pool_hi[c] = last + 1 - actors;     /* else one spawned past the end */
    }
    actor_now = NULL;
}

void actors_retire_below(s16 x)
{
    u16 i;
    for (i = 0; i < ACTORS; i++)
    {
        Actor *a = actors + i;
        if (a->fam && AX(a) < x && a->fam != F_WAGON && a->fam != F_WHEEL && a->fam != F_DRIVER && a->fam != F_HORSE)
            actor_free(a);
    }
}

#ifdef DBG_UNSHOWN
u16 vis_refused, vis_drawn;
#endif

/* Draw order: effects, enemy shots and pickups over the enemies, the boss
 * last (behind). Everything past max sprites is dropped. */
extern u8 stage_index;
u16 actors_draw(VDPSprite *sat, u16 first, u16 max, s16 view_x, s16 view_y)
{
    static const u8 order[C_CLASSES] = { C_EFFECT, C_ESHOT, C_PICKUP, C_ENEMY, C_OBJECT, C_SURFACE, C_BOSS };
    u16 n = 0, k, i;
    /* Stage 3: Willow's boat first, so the per-line sprite limit drops the
     * enemy rafts' pieces rather than the boat under her. */
    if (stage_index == 2) for (i = pool_lo[C_SURFACE]; i < pool_hi[C_SURFACE]; i++)
    {
        Actor *a = actors + i;
        s16 sx = AX(a) - view_x, sy = 511 - AY(a) - view_y;
        if (a->fam != F_BOAT || !a->active || (u16)(sx + 96) > 320 + 192 || (u16)(sy + 96) > 224 + 192) continue;
        n += pose_draw(families[F_BOAT]->ps, a->pose, &a->shown, sat, first + n, sx, sy, a->hflip, &a->slot);
    }
    for (k = 0; k < C_CLASSES; k++)
    {
        u16 c = order[k];
#ifdef PROFA
        { extern u16 profa[16]; profa[k * 2] = *(vu16 *)0xC00008; }   /* measurement builds: each class's start */
#endif
        if (c == C_EFFECT) n += sparkles_draw(sat, first + n, max, view_x, view_y);
#ifdef PROFA
        { extern u16 profa[16]; profa[k * 2 + 1] = *(vu16 *)0xC00008; }   /* after sparkles / before the class loop */
#endif
        if (c == C_SURFACE) { n += logs_draw(sat, first + n, max, view_x, view_y); continue; }   /* logs and platforms */
        for (i = pool_lo[c]; i < pool_hi[c]; i++)
        {
            Actor *a = actors + i;
            const Family *f;
            s16 sx, sy;
            if (!a->fam || !a->active || a->fam == F_LOG) continue;
            sx = AX(a) - view_x;
            if ((u16)(sx + 96) > 320 + 192) continue;
            sy = (511 - AY(a)) - view_y;
            if ((u16)(sy + 96) > 224 + 192) continue;
            if ((a->fam == F_BOSS_HEAD || a->fam == F_BOSS_PAW) && !boss_draw_adjust(a, &sx, &sy)) continue;   /* stage 1 boss: on the shown body pose */
            f = families[a->fam];
            if (!f->ps) continue;
#ifdef EXP_NOFAM
            if (a->fam == EXP_NOFAM) continue;     /* measurement builds: one family not drawn */
#endif
#ifdef EXP_NOWAGONDRAW
            if (a->fam == F_WHEEL || a->fam == F_DRIVER || a->fam == F_HORSE) continue;
#endif
#ifdef EXP_NOCHASEDRAW
            if (a->fam >= F_RIDER || a->fam == F_HORSE || a->fam == F_ARROW || a->fam == F_GOLD || a->fam == F_BURST) continue;
#endif
#ifdef PROF_NOLOGDRAW
            if (a->fam == F_LOG) continue;
#endif
            if (first + n + 4 > max) return n;
            if (f->ps->vram_of)
            {
                /* Resident art (the bridge logs): the sprites written here,
                 * without the call. */
                const PoseSet *ps = f->ps;
                const u8 *d = ps->poses + a->pose * 6;
                u16 ns = d[3], base = ps->vram_of[a->pose], k = first + n;
                const s16 *q = ps->sat + (((d[4] << 8) | d[5]) << 2);
                VDPSprite *o = sat + k;
                a->shown = a->pose;
                if (k + ns > max) ns = max - k;
                sx += 128; sy += 128;
                n += ns;
                while (ns--)
                {
                    o->y = sy + q[0];
                    o->size_link = q[1] | ++k;
                    o->attribut = q[2] + base;
                    o->x = sx + q[3];
                    o++; q += 4;
                }
                continue;
            }
#ifdef DBG_UNSHOWN
            {
                /* Measurement: actors inside the screen refused (no VRAM room). */
                extern u16 vis_refused, vis_drawn;
                u16 u0 = posecache_unshown, m = pose_draw_inline(f->ps, a->pose, &a->shown, sat, first + n, sx, sy, a->hflip, &a->slot);
                if ((u16)(sx + 16) <= 352 && (u16)(sy + 16) <= 256) { if (posecache_unshown != u0) vis_refused++; else if (m) vis_drawn++; }
                n += m;
                continue;
            }
#endif
#ifdef DRAWTIME
            {
                /* Measurement builds: real time per family's draw (1/256 line). */
                extern u32 drawtime[128];
                u16 f0 = a->fam, h0, h1;
                s32 t;
                __asm__ volatile ("move.w #0x2700,%sr");
                h0 = *(vu16 *)0xC00008;
                n += pose_draw_inline(f->ps, a->pose, &a->shown, sat, first + n, sx, sy, a->hflip, &a->slot);
                h1 = *(vu16 *)0xC00008;
                __asm__ volatile ("move.w #0x2000,%sr");
                t = (s32)(((h1 >> 8) - (h0 >> 8) + 262) % 262) * 256 + (s16)((h1 & 0xFF) - (h0 & 0xFF));
                if (t < 0) t += 256;
                if (t < 120 * 256) drawtime[f0] += t;
                continue;
            }
#endif
            n += pose_draw_inline(f->ps, a->pose, &a->shown, sat, first + n, sx, sy, a->hflip, &a->slot);
        }
    }
    return n;
}

/* BCD add, keeping the old value on overflow (hud.md 3.1, 3.4). */
u32 bcd_add(u32 a, u32 b, u16 digits)
{
    u32 r = 0, carry = 0;
    u16 i;
    for (i = 0; i < digits; i++)
    {
        u32 d = ((a >> (i * 4)) & 15) + ((b >> (i * 4)) & 15) + carry;
        carry = d > 9;
        if (carry) d -= 10;
        r |= d << (i * 4);
    }
    return carry ? a : r;
}

static const u32 score_table[14] = {
    0x50, 0x100, 0x200, 0x500, 0x1000, 0x2000, 0x5000, 0x6000, 0x8000, 0x10000, 0x20000, 0x30000, 0x100000, 0x1000000
};
static const u32 gold_table[12] = { 0x1, 0x5, 0x10, 0x20, 0x50, 0x100, 0x200, 0x500, 0x1000, 0x2000, 0x5000, 0x10000 };

void award(u16 index)
{
    if (two_players && actor_now && actor_now->pad[0]) { coop_score(score_table[index >> 1]); return; }
    hud.score = bcd_add(hud.score, score_table[index >> 1], 8);
    hud_dirty |= HUD_SCORE;
    if (hud.score > hud.hi) { hud.hi = hud.score; hud_dirty |= HUD_HI; }
}

void add_score(u32 bcd)
{
    if (fe.demo) return;                    /* the attract's demo scores nothing (hud.md 3.1, FF8293) */
    if (two_players && actor_now && actor_now->pad[0]) { coop_score(bcd); return; }
    hud.score = bcd_add(hud.score, bcd, 8);
    hud_dirty |= HUD_SCORE;
    if (hud.score > hud.hi) { hud.hi = hud.score; hud_dirty |= HUD_HI; }
}

void add_gold(u16 variant)
{
    hud.gold = bcd_add(hud.gold, gold_table[variant], 8);
    hud_dirty |= HUD_GOLD;
}

/* Drop sets (ROM 0x2544): pickup variants; the first gets the highest index. */
static const u8 drop_sets[15][16] = {
    { 1, 0 }, { 1, 2 }, { 1, 5 }, { 1, 6 }, { 3, 5, 6, 6 }, { 3, 6, 6, 6 }, { 3, 7, 7, 7 },
    { 15, 8, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 15, 8, 8, 8, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6 },
    { 15, 10, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 },
    { 15, 10, 10, 10, 9, 9, 9, 9, 9, 9, 7, 7, 7, 7, 7, 7 },
    { 15, 10, 10, 10, 10, 10, 10, 9, 9, 9, 7, 7, 7, 7, 7, 7 },
    { 15, 10, 10, 10, 10, 10, 10, 10, 10, 9, 9, 9, 8, 8, 8, 8 },
    { 15, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 },
    { 15, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 },
};

void drop(Actor *src, u16 set)
{
    const u8 *s = drop_sets[set];
    u16 n = s[0], i;
    for (i = 0; i < n; i++)
    {
        Actor *g = actor_new(F_GOLD);
        if (!g) return;
        g->variant = s[1 + i];
        g->sub = n - 1 - i;            /* spread index */
        g->x = src->x & ~0xFF;
        g->y = src->y & ~0xFF;
    }
}

void effect(u16 fam, s16 x, s16 y, u16 variant)
{
    Actor *e;
    if (fam == F_SPARKLE) { sparkle_add(x, y); return; }
    e = actor_new(fam);
    if (!e) return;
    e->x = (s32)x << 8;
    e->y = (s32)y << 8;
    e->variant = variant;
    e->active = 2;
}

/* Terrain: the stage's 16x16 collision ids and their descriptors
 * (tools/colpack.py, the same grid the player probes). */
/* The collision id at (x, y): tools/maptrim.py --col columns (u32 offset,
 * u16 first row, u16 rows of 16 px from the stage's top), then the ids. */
u8 gate5_open;               /* stage 5: the doorway wiped in (its cells replace the tower's wall column) */

/* Collision overrides (stage 6's flipping stones: two cells of id 37 while
 * flat, stage6_families.md 2.3): cell column, cell row from the world top. */
u8 npatch;
s16 patch_c[16], patch_r[16];
u8 patch_id[16];
s16 patch_c0 = 0x7FFF, patch_c1 = -1, patch_r0 = 0x7FFF, patch_r1 = -1;   /* their bounding box (cells) */

/* The collision id lookup, inlined into the probes (hot: ~16 terrain
 * probes an update in busy scenes; the nested calls cost more than it). */
static inline __attribute__((always_inline)) u8 tid(s16 x, s16 y)
{
    const StageData *g = sd;
    s16 c;
    u16 d, r;
    const u16 *e;
    if (npatch)
    {
        s16 pc = x >> 4, pr = (s16)(g->world_top - y) >> 4;
        if (pc >= patch_c0 && pc <= patch_c1 && pr >= patch_r0 && pr <= patch_r1)
        {
            u16 k;
            for (k = 0; k < npatch; k++)
                if (patch_c[k] == pc && patch_r[k] == pr) return patch_id[k];
        }
    }
    if (gate5_open && (u16)(x - 2592) < 16 && (u16)(y - 544) < 224) x += 16;   /* the corridor's column instead */
    c = x >> 4;
    d = g->world_top - y;
    if ((u16)c >= g->col_cols || d > g->world_top) return 0;
    e = (const u16 *)(g->col + (c << 3));
    r = (d >> 4) - e[2];
    if (r >= e[3]) return 0;
    return g->col[(g->col_cols << 3) + *(const u32 *)e + r];
}

u8 terrain_id(s16 x, s16 y) { return tid(x, y); }

/* One collision column's ids for a caller that keeps them (the player's
 * probes, src/player.c): the ids from row *first, *rows of them (0: none).
 * NULL when the column's cells need tid()'s special cases (patched cells,
 * the opened doorway's column). */
const u8 *terrain_column(s16 c, u16 *first, u16 *rows)
{
    const StageData *g = sd;
    const u16 *e;
    if (npatch && c >= patch_c0 && c <= patch_c1) return NULL;
    if (gate5_open && c == 2592 >> 4) return NULL;
    if ((u16)c >= g->col_cols) { *first = 0; *rows = 0; return g->col; }
    e = (const u16 *)(g->col + (c << 3));
    *first = e[2];
    *rows = e[3];
    return g->col + (g->col_cols << 3) + *(const u32 *)e;
}

static inline __attribute__((always_inline)) const u8 *desc(s16 x, s16 y)
{
    return sd->coldesc + (tid(x, y) << 3);
}

u8 terrain_flags(s16 x, s16 y)
{
    return desc(x, y)[0];
}

u8 terrain_shape(s16 x, s16 y)
{
    return desc(x, y)[1];
}

/* Terrain changes (stage, area, opened gate, stone cells) bump this, so
 * results memoized against it (src/fam_spearman.c) are dropped. Never 0. */
u8 terrain_epoch = 1;

void terrain_changed(void)
{
    if (!++terrain_epoch) { terrain_epoch = 1; memset(ag_ep, 0, sizeof ag_ep); }   /* wrapped: no old entry may match */
}

/* Inside a floor at (x, y), e its cell's descriptor: the surface's world y in *top. */
static inline __attribute__((always_inline)) u16 floor_e(const u8 *e, s16 x, s16 y, s16 *top, u8 *slope)
{
    s16 lx = x & 15, h, in;
    u8 s = e[1];
    if (!(e[0] & 0x08)) return 0;
    if (s & 0x80) h = s & 31;
    else
    {
        *slope = 1;                          /* the answer depends on x within the cell */
        if (s & 0x40) lx = 15 - lx;
        h = (s & 15) + (lx >> ((s >> 4) & 3));
        if (h > 16) h = 16;
    }
    in = 15 - ((511 - y) & 15);              /* height above the cell bottom */
    if (in > h) return 0;
    *top = y + h - in;
    return 1;
}

static inline __attribute__((always_inline)) u16 floor_at(s16 x, s16 y, s16 *top, u8 *slope)
{
    return floor_e(desc(x, y), x, y, top, slope);
}

u16 terrain_floor(s16 x, s16 y, s16 *top)
{
    u8 slope;
    return floor_at(x, y, top, &slope);
}

/* terrain_floor, also telling whether the cell is a slope (*slope = 1;
 * otherwise the answer is the same anywhere in the cell's column span). */
u16 terrain_floor_s(s16 x, s16 y, s16 *top, u8 *slope)
{
    return floor_at(x, y, top, slope);
}

/* A wall at (x, y) for something moving left / right: the player's wall
 * test (0x3E80) without the push: the cell's wall flag for that side,
 * unless the point is below its ceiling or above its floor surface, and
 * the point inside the wall's span. */
/* The slope class of the floor at (x, y) (0x4200): descriptor byte 5 & 15
 * on a slope, 0 on flat floor or none. */
u8 terrain_slope_class(s16 x, s16 y)
{
    const u8 *e = desc(x, y);
    if (!(e[0] & 0x08)) return 0;
    if ((e[1] & 0xC0) == 0xC0) e = desc(x, y + 16);   /* a full cell under a slope (0x429E): the probe again above */
    if (!(e[0] & 0x08) || (e[1] & 0x80)) return 0;
    return e[5] & 15;
}

bool terrain_wall(s16 x, s16 y, bool left)
{
    const u8 *e = desc(x, y);
    s16 lx = x & 15, in = y & 15, c;
    if (!(e[0] & (left ? 0x02 : 0x01))) return FALSE;
    if ((e[0] & 0x04) && (s8)e[2] >= in) return FALSE;
    if ((e[0] & 0x08) && in >= (e[1] & 31)) return FALSE;
    c = left ? (s8)e[4] - lx : lx - (s8)e[3];
    return c >= 0;
}

bool actor_ground(Actor *a, s16 dx, s16 dy)
{
    s16 top, x = AX(a) + dx, y = AY(a) + dy, c = x >> 4, r = y >> 4;
    u16 k = a - actors;
    u8 slope;
    static const StageData *ag_sd;
    if (ag_sd != sd) { ag_sd = sd; memset(ag_ep, 0, sizeof ag_ep); }   /* another stage's grid */
    if (ag_ep[k] != terrain_epoch || ag_c[k] != c || ag_r[k] != r)
    {
        ag_c[k] = c; ag_r[k] = r; ag_ep[k] = terrain_epoch;
        ag_id[k] = tid(x, y);
    }
    if (!floor_e(sd->coldesc + (ag_id[k] << 3), x, y, &top, &slope)) return FALSE;
    a->y = (s32)(top - dy) << 8;
    return TRUE;
}
