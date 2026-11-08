/* Stage foreground and backdrop (NATIVE_ENGINE_PLAN.md, M1).
 *
 * Every decision is made offline by tools/stagepack.py: tile colours, the
 * plane words and which VRAM slot each tile occupies while it is on screen.
 * Here the game only copies: when the window's edge reaches a map column,
 * the tiles listed for that column and the column's 64 plane words are
 * queued for DMA at the next VBlank.
 *
 * VRAM (byte addresses): tiles 0x0000-0xA7FF, horizontal scroll 0xA800,
 * sprite table 0xAC00, window 0xB000 (top rows only; the rest holds tiles
 * from 0xB200), plane B 0xC000 (64x32: the far layer's ring and the HUD's
 * rows), tiles again from 0xD000 (the HUD's), plane A 0xE000 (64x32). Planes are 64x32: the stage is 64 cells high, so
 * plane A holds a 32-row band around the view and rows stream in as the
 * camera moves vertically (a map row r lives in plane row r & 31; the VDP
 * wraps the vertical scroll at 256 px the same way). 0xF000-0xFFFF is free
 * for sprite tiles (src/vram.c). */
#include <genesis.h>
#include "stagedata.h"
#include "stage.h"
#include "hud.h"
#include "../data/stage1/layout.h"
#include "../data/stage2/chase.h"
#include "sprite.h"
#include "spell.h"
#include "player.h"                 /* player.cam_x: the palette cycle starts at a camera x */


#define WINDOW_COLS 42      /* columns resident: 40 + fine scroll + 1 ahead (stagepack WINDOW) */
#define COLS_PER_FRAME 2    /* at most 2 x 48 tiles + 2 columns of DMA a frame */

static s16 lo, hi;          /* resident map columns */
static s16 rlo;             /* resident map rows rlo .. rlo + 31 */
static const u16 *sky_map;  /* rows above the map (the intro's sky band), NULL: none */
static const u16 *sky_vram; /* VRAM tile of each sky tile */
static s16 rmin;            /* lowest map row the band may hold: 0, or -STAGE_SKY_ROWS with the sky */
#define STAGE_SKY_ROWS 30   /* above the map */
#define STAGE_SKY_EXT 16    /* map rows 0-15: the sky where the map's cell is empty */
#define STAGE_SKY_COLS 48

/* Stage 2's chase (stage2.md 2.4; tools/chasepack.py): past x 1792 the
 * road's blocks by its sequence, tiles resident in the pool slots the fort
 * never uses. */
extern const u8 road_tiles[], road_seq[];
extern const u16 road_blocks[], s2_far_pal[];
static u8 chase;
static u8 torch_loaded;        /* stage 2's torch frames in the road's slots */
static u16 last_wave;          /* spell 10's wave was on last frame */

void stage_chase(bool on)
{
    chase = on;
    torch_loaded = 0;
    if (on) DMA_queueDma(DMA_VRAM, (void *)road_tiles, CHASE_ROAD_BASE * 32, CHASE_ROAD_TILES * 16, 2);
}

/* Once the road fills the view the fort's pool tiles (VRAM 16..412) are
 * free: the sprite cache borrows them (stage_init / stage_restart take them
 * back before reloading the foreground). */
void stage_chase_lend(void)
{
    posecache_lend(16, CHASE_ROAD_BASE);
}

void stage_far_step(u16 s)                  /* the sky's colour step (2.6), line 3 */
{
    PAL_setColors(48, s2_far_pal + s * 16, 16, DMA_QUEUE);
}

static u16 road_cell(s16 c, s16 r)
{
    u16 k = road_seq[c >> 5];
    if (k == 0xFF || r < 32 || r >= 64) return 0;
    return road_blocks[((k << 5) | (c & 31)) << 5 | (r - 32)];   /* words carry CHASE_ROAD_BASE (chasepack.py) */
}

/* Plane word of map cell (c, r); rows above the map, and the empty cells
 * of its top rows, come from the sky while it is on. */
/* The plane-A map word of column c, row r: tools/maptrim.py --meta's 16x16
 * metatiles (dictionary offset; per 16-px column offset, first row, rows;
 * the metatile indices; 4 words a metatile). */
static const u16 *arena_map;     /* stage 5's boss: plane-A words of the arena (stage_arena) */
static u16 arena_c0, arena_c1, arena_r0;

static inline u16 map_word(u16 c, u16 r)
{
    const u16 *e = sd->map + 2 + ((c >> 1) << 2);
    u16 k = (r >> 1) - e[2], i;
    if (k >= e[3]) return 0;
    i = sd->map[2 + (sd->cols << 1) + *(const u32 *)e + k];
    return sd->map[*(const u32 *)sd->map + (i << 2) + ((r & 1) << 1) + (c & 1)];
}

static u16 cell(s16 c, s16 r)
{
    u16 w;
    if (chase && c >= 224) return road_cell(c, r);
    if (arena_map && c >= arena_c0 && c < arena_c1 && (u16)(r - arena_r0) < 32) return arena_map[(c - arena_c0) * 32 + r - arena_r0];
    if (r >= 0)
    {
        w = map_word(c, r);
        if (w || !sky_map || r >= STAGE_SKY_EXT || c >= STAGE_SKY_COLS) return w;
    }
    else if (!sky_map || c >= STAGE_SKY_COLS || r < -STAGE_SKY_ROWS) return 0;
    w = sky_map[(r + STAGE_SKY_ROWS) * STAGE_SKY_COLS + c];
    return (w & 0xF800) | sky_vram[w & 0x7FF];
}

void stage_sky(const u16 *map, const u16 *vram)
{
    sky_map = map;
    sky_vram = vram;
    rmin = map ? -STAGE_SKY_ROWS : 0;
}

/* ---- Nametable and scroll-table writes made while playing (columns and
 * rows of both planes, the HUD's rows, the scroll tables, plane B text):
 * one list, transferred in order by DMA in VBlank (stage_vblank) with the
 * registers set directly - SGDK's DMA queue costs the frame several hundred
 * cycles an entry, and a CPU copy would be ~4x slower than DMA in VBlank.
 * A full list goes at once (restarts, behind a fade). */
static u16 npend;           /* tile loads for VBlank (below) */
#define VQ 192                      /* stage 5's boss: a picture change re-lays 41 far columns (up to 82 entries) in one frame */
#define VQ_WORDS 2048               /* the copied words of a frame's entries */
typedef struct { u16 addr, n, inc; const u16 *src; } VWrite;
static VWrite vq[VQ];
static u16 vq_words[VQ_WORDS], nwords;      /* copies (vq_put copy, vq_alloc) */
u16 stage_vq_words;                         /* words queued this frame (the pose cache's budget counts them) */
static u16 nvq;

/* One entry by 68000-to-VRAM DMA (registers set directly; a source that
 * would cross a 128 KB bank is split). */
static vu16 vq_trigger;              /* the DMA command's last word, written from RAM (below) */

static void vq_dma(u32 src, u16 addr, u16 n)
{
    vu16 *c = (vu16 *)VDP_CTRL_PORT;
    u32 cmd = VDP_DMA_VRAM_ADDR((u32)addr);
    *c = 0x9300 | (n & 0xFF);
    *c = 0x9400 | (n >> 8);
    *c = 0x9500 | ((src >> 1) & 0xFF);
    *c = 0x9600 | ((src >> 9) & 0xFF);
    *c = 0x9700 | ((src >> 17) & 0x7F);
    /* As SGDK's DMA_doDma: the word that starts the transfer comes from
     * memory ("to avoid possible failure on some MD"). */
    *c = cmd >> 16;
    vq_trigger = cmd;
    *c = vq_trigger;
}

static void vq_write(const VWrite *b, u16 *inc)
{
    u32 src = (u32)b->src;
    u16 n = b->n, room = (0x20000 - (src & 0x1FFFF)) >> 1;
    if (b->inc != *inc) { *(vu16 *)VDP_CTRL_PORT = 0x8F00 | b->inc; *inc = b->inc; }
    if (n > room)
    {
        vq_dma(src, b->addr, room);
        vq_dma(src + room * 2, b->addr + room * b->inc, n - room);
    }
    else vq_dma(src, b->addr, n);
}

u16 vq_peak, vq_over;               /* measurement */

static void vq_flush(void);

/* The list full: everything so far now, in order (restarts, behind a fade). */
static void vq_full(void)
{
    u16 ints = SYS_getAndSetInterruptMaskLevel(7);
    vq_over++;
    vq_flush();
    SYS_setInterruptMaskLevel(ints);
}

static inline __attribute__((always_inline)) void vq_put(u16 addr, const u16 *src, u16 n, u16 inc, bool copy)
{
    VWrite *b;
    if (nvq + 1 > vq_peak) vq_peak = nvq + 1;
    if (nvq == VQ || (copy && nwords + n > VQ_WORDS)) vq_full();
    b = vq + nvq++;
    b->addr = addr;
    b->n = n;
    b->inc = inc;
    stage_vq_words += n;
    if (copy) { u16 *w = vq_words + nwords; memcpy(w, src, n * 2); b->src = w; nwords += n; }
    else b->src = src;
}

/* An entry to fill in place. */
static u16 *vq_alloc(u16 addr, u16 n, u16 inc)
{
    VWrite *b;
    u16 *w;
    if (nvq == VQ || nwords + n > VQ_WORDS) vq_full();
    b = vq + nvq++;
    w = vq_words + nwords;
    nwords += n;
    b->addr = addr; b->n = n; b->inc = inc; b->src = w;
    stage_vq_words += n;
    return w;
}

static void vq_flush(void)
{
    u16 i, inc = 2;
    /* The Z80 held, as SGDK's DMA queue does (HALT_Z80_ON_DMA): XGM2 reads
     * the ROM all the time, and on some consoles a Z80 bus access as a DMA
     * starts corrupts the transfer (no emulator shows it). */
    bool z80 = Z80_getAndRequestBus(TRUE);
    for (i = 0; i < nvq; i++) vq_write(vq + i, &inc);
    if (!z80) Z80_releaseBus();
    if (inc != 2) *(vu16 *)VDP_CTRL_PORT = 0x8F02;
    nvq = 0;
    nwords = 0;
    stage_vq_words = 0;
}

#define PLANE_A 0xE000
#define PLANE_B 0xC000
#define NT(base, col, row) ((base) + (((row) * 64 + (col)) << 1))

static void bq_put(u16 col, u16 row, const u16 *w, u16 n, bool column)
{
    vq_put(NT(PLANE_B, col, row), w, n, column ? 128 : 2, TRUE);
}

/* ---- The far layer (arcade scroll3) on plane B (tools/farlayer.py,
 * stagepack.py), scrolled at the arcade's rates both ways: far x = view x /
 * 2 + 16, and screen line L shows far picture row (L + far_ty) / 8 with
 * far_ty = sd->far_top - cam_y / 2 (cam_y = 272 - view y). Plane B is a
 * 32-row ring like plane A (picture row r in plane row r & 31; rows outside
 * the picture are empty: the sky colour). The 28 rows lines 0-207 can show
 * are resident. The HUD's top rows are sprites (src/hud.c, user decision
 * 2026-11-05: as plane B words the far layer behind them stepped in whole
 * tiles); its bottom band, screen rows 26-27, lives in the ring rows after
 * the resident ones (hud_prow) and is shown by giving plane B its own
 * vertical scroll for lines 208-223 (one H interrupt, stage_vint /
 * stage_hint), horizontal scroll 0 (per-row tables). */
#define FAR_RES 28              /* far rows resident: lines 0-207 + fine scroll */
/* A picture in place of the far layer (stage 5's boss: the necks), placed
 * by the caller (stage_far_at) instead of the camera. */
static const u16 *fo_map;
static u16 fo_cols, fo_rows;
static s16 fo_x, fo_ty;
static u8 fo_relay;                 /* the picture changed: 1 lay it out, 2/3 re-lay in halves, 4 re-lay changed spans */
static const u8 *fo_spans;          /* fo_relay 4: per picture column the first and last changed row */
static s16 fo_col;                  /* fo_relay 4: the next column */
#define RELAY_WORDS 200             /* a frame's share (~12 lines of VBlank DMA as measured) */
#define FMAP (fo_map ? fo_map : sd->bd_map)
#define FCOLS (fo_map ? fo_cols : sd->far_cols)
#define FROWS (fo_map ? fo_rows : sd->far_rows)
#define FWRAP (fo_map ? 0 : sd->far_wrap)
/* 16 x 16 products (mulu.w / muls.w, not the 32-bit library routine). */
#define MULU(a, b) ((u32)(u16)(a) * (u16)(b))
#define MULS(a, b) ((s32)(s16)(a) * (s16)(b))
static s16 far_lo, far_hi, far_x;
static s16 far_ty;              /* far picture row (px) at screen line 0 */
static s16 far_rt;              /* first resident far row (far_ty >> 3) */
static u8 far_lock;             /* the far layer's vertical position held (the intro) */
static u16 far_lock_bottom;     /* while held: the word of screen rows 26-27 (the intro's blank band) */
static u8 b_fixed;              /* plane B holds a fixed picture (map, story): no scroll, no streaming */
static const u16 *b_pic;        /* the fixed picture's words (40 x 28) under HUD text, or NULL */
static u16 b_pic_add;
static s16 hs_a[28], hs_b[28];
static s16 last_x = 0x7FFF;     /* scroll tables written for this view x and plane B mode */
static u8 last_fixed = 0xFF, last_split = 0xFF;

/* Plane B vertical scroll per band (lines 0-207, 208-223), as the
 * interrupts load it; published once a frame (stage_commit). */
static u16 vs_next[2], vs_now[2];
static u16 vs_cur[2];               /* latched by stage_vint: the H interrupt of one frame uses one set */
static u8 split_allow, split_on;     /* split_on: the interrupts write (never during CPU streaming) */
static u8 hint_band;
static void vsram_b(u16 v);

/* Off: the interrupts write nothing at all (a register write between a
 * CPU address and its data would corrupt the main thread's stream) and
 * the H interrupt is disabled. */
static void split_set(bool on)
{
    if (on == split_on) return;
    hint_band = 0xF0;                       /* switched on mid-frame: no writes until the VBlank's reset */
    split_on = on;
    VDP_setHInterrupt(on);
}

static void split_update(void) { split_set(split_allow && !b_fixed); }

void stage_split(bool on) { split_allow = on; split_update(); }

/* No splits (a title or story page over the stage): plane B's whole
 * screen takes the scene's value, as the split's middle band shows it. */
void stage_split_scene(void)
{
    u16 s = SYS_getAndSetInterruptMaskLevel(7);
    stage_split(FALSE);
    vsram_b(vs_next[0]);
    SYS_setInterruptMaskLevel(s);
}

/* The frame's writes and scroll values are complete: the next VBlank
 * interrupt takes its scroll values (stage_vint) and SGDK's VBlank process
 * lays out its writes (stage_vblank); a frame not ready by the interrupt
 * keeps the last one's values, in step with the last one's writes. (Laying
 * the writes out in the interrupt itself, before SGDK's DMA queue, measured
 * slower: route trace frame 647 started at line 43 against 23.) */
static volatile u8 frame_ready;
#ifdef DBG_PLANEB
u16 dbg_planeb[32 * 40];
#endif

void stage_commit(void)
{
    u16 s = SYS_getAndSetInterruptMaskLevel(7), i;
    for (i = 0; i < 2; i++) vs_now[i] = vs_next[i];
    frame_ready = 1;
    SYS_setInterruptMaskLevel(s);
}

static void vblank_flush(void);

static void vsram_b(u16 v)
{
    *(vu32 *)VDP_CTRL_PORT = 0x40020010;            /* VSRAM write, plane B */
    *(vu16 *)VDP_DATA_PORT = v;
}

/* Called from the VBlank interrupt: the frame's scroll, the H interrupt
 * after line 207. */
void stage_vint(void)
{
    if (frame_ready)
    {
        frame_ready = 0;
        vs_cur[0] = vs_now[0]; vs_cur[1] = vs_now[1];
    }
#ifdef DBG_PLANEB
    {
        /* Debug: plane B's nametable as this frame shows it (rows 0-31, columns 0-39). */
        extern u16 dbg_planeb[32 * 40];
        u16 r, c;
        for (r = 0; r < 32; r++)
        {
            *(vu32 *)VDP_CTRL_PORT = ((u32)((0xC000 + r * 128) & 0x3FFF) << 16) | ((0xC000 + r * 128) >> 14);
            for (c = 0; c < 40; c++) dbg_planeb[r * 40 + c] = *(vu16 *)VDP_DATA_PORT;
        }
    }
#endif
    if (!split_on) return;
    hint_band = 0;
    *(vu16 *)VDP_CTRL_PORT = 0x8A00 | 207;
    vsram_b(vs_cur[0]);
}

/* The frame was queued after this VBlank began (main.c's late start).
 * stage_vblank_bytes: the CPU-written VBlank transfers (pending tiles,
 * plane words, an allowance per queue entry) in bytes. stage_vint_late:
 * what stage_vint did at the VBlank's start, again with this frame's
 * values. */
u16 stage_vblank_bytes(void)
{
    return npend * 32 + nwords * 2 + nvq * 16;
}

void stage_vint_late(void)
{
    u16 s = SYS_getAndSetInterruptMaskLevel(7);
    if (frame_ready)
    {
        frame_ready = 0;
        vs_cur[0] = vs_now[0]; vs_cur[1] = vs_now[1];
    }
    if (split_on) vsram_b(vs_cur[0]);
    SYS_setInterruptMaskLevel(s);
}

/* After line 207: the bottom band. The counter stays at 207 until the next
 * VBlank, so this is the frame's only interrupt. */
HINTERRUPT_CALLBACK stage_hint(void)
{
    if (!split_on || ++hint_band != 1) return;
    vsram_b(vs_cur[1]);
}

/* The HUD's rows: glyph words kept by src/hud.c; index 0-1 = screen rows
 * 0-1 (sprites in play; on plane B only over a fixed picture), 2-3 = rows
 * 26-27. */
u16 stage_hudrow[4][40];

/* The far picture: sd->far_cols x sd->far_rows words, column-major; a
 * wrapping far layer (sd->far_wrap) repeats every far_cols columns. */
static inline const u16 *far_col(s16 c)                     /* column c's words, NULL outside the picture */
{
    if (FWRAP) { c %= (s16)FCOLS; if (c < 0) c += FCOLS; }
    else if (c < 0 || c >= (s16)FCOLS) return NULL;
    return FMAP + MULU(c, FROWS);
}

static u16 far_word(s16 c, s16 r)                    /* far picture word, 0 outside the picture */
{
    const u16 *m = far_col(c);
    return m && (u16)r < FROWS ? m[r] : 0;
}

/* The top rows on plane B (stage 6's corridors, HUD_PLANE_AREA): at a fixed
 * height, over far rows that show nothing or one tile, nothing can step
 * behind them, and 200-290 sprite pixels a line go back to the crushers.
 * Plane rows 0-1, scroll 0 (per-row tables); far_column leaves them alone. */
static u8 hud_plane;

/* The top rows on the window plane (user decision 2026-11-07, "HUD on plane
 * B there"): while a mid-boss holds the camera (stage 1's sorceress over the
 * waterfall), big sprites at the top of the screen and the sprite HUD went
 * over the 20-sprite / 320-pixel line limits and lost pieces (her head, the
 * bridge logs). The window is fixed (no scroll), so the glyphs sit where the
 * sprites were, over the far layer as it is, and take no sprite budget; it
 * replaces plane A in its rows, which over the waterfall show only sky. Its
 * nametable (0xB000, rows 0-1) holds the sprite HUD's glyph copies otherwise
 * (src/hud.c): enabled at the VBlank after the rows are written, turned off
 * before the copies come back. hud_win: 1 on, 2 rows queued (enable at the
 * next VBlank). */
static u8 hud_win;
#define WIN_NT 0xB000

static u16 hud_prow(u16 k)                           /* plane row of HUD row k */
{
    /* The bottom rows just after the resident rows (a late switch at line
     * 208 shows far rows, never another row's HUD); the top rows only over
     * a fixed picture. */
    return b_fixed || (hud_plane && k < 2) ? (k < 2 ? k : 24 + k) : (far_rt + FAR_RES + k - 2) & 31;
}

/* Columns x0..x0+n-1 of HUD row k: glyphs, or what shows under them. */
static u16 hud_w[4][40];         /* the HUD rows as last laid out */

static void hud_seg_put(u16 k, u16 x0, u16 n)
{
    const u16 *g = stage_hudrow[k] + x0;
    u16 *o = hud_w[k] + x0, i, y = k < 2 ? k : 24 + k;
    if (k < 2 && hud_win && !b_fixed)
    {
        for (i = 0; i < n; i++) o[i] = g[i] ? g[i] | 0x8000 : 0;     /* over the sprites, as the arcade's text layer */
        vq_put(WIN_NT + k * 128 + x0 * 2, o, n, 2, FALSE);
        return;
    }
    if (k < 2 && !b_fixed && !hud_plane) return;   /* the top rows are sprites */
    if (b_pic) for (i = 0; i < n; i++) o[i] = g[i] ? g[i] : b_pic[y * 40 + x0 + i] + b_pic_add;
    else if (k < 2 && hud_plane) { u16 u = far_word(0, k); for (i = 0; i < n; i++) o[i] = g[i] ? g[i] : u; }
    else for (i = 0; i < n; i++) o[i] = g[i];
    vq_put(NT(PLANE_B, x0, hud_prow(k)), o, n, 2, FALSE);    /* hud_w stays put */
}

static void hud_row_put(u16 k) { hud_seg_put(k, 0, 40); }

void stage_hudput(u16 k, u16 x, u16 n)               /* src/hud.c: row k's glyphs x..x+n-1 changed */
{
    hud_seg_put(k, x, n);
}

static void hud_rows_put(void)
{
    u16 k;
    for (k = 0; k < 4; k++) hud_row_put(k);
}

/* The resident far rows of column c into the ring (the HUD's rows never). */
static void far_fill(u16 *b, s16 c, s16 r, u16 n)
{
    const u16 *m = far_col(c);
    if (!m) { memset(b, 0, n * 2); return; }
    for (; n; n--, r++) *b++ = far_lock && r >= 26 ? far_lock_bottom : (u16)r < FROWS ? m[r] : 0;
}

static void far_column(s16 c)
{
    u16 top = hud_plane ? 2 : 0;            /* the HUD's rows stay */
    u16 p0 = (far_rt + top) & 31, res = FAR_RES - top, n0 = 32 - p0 < res ? 32 - p0 : res, *b;
    if (fo_map && far_rt >= 0 && far_rt + FAR_RES <= (s16)fo_rows)
    {
        /* The boss's picture with every resident row inside it: the column
         * straight from ROM (or blank outside it), no copy. */
        static const u16 blank[FAR_RES];
        const u16 *src = (u16)c < fo_cols ? fo_map + MULU(c, fo_rows) + far_rt : blank;
        vq_put(NT(PLANE_B, c & 63, p0), src, n0, 128, FALSE);
        if (n0 < FAR_RES) vq_put(NT(PLANE_B, c & 63, 0), src + ((u16)c < fo_cols ? n0 : 0), FAR_RES - n0, 128, FALSE);
        return;
    }
    b = vq_alloc(NT(PLANE_B, c & 63, p0), n0, 128);
    far_fill(b, c, far_rt + top, n0);
    if (n0 < res)
    {
        b = vq_alloc(NT(PLANE_B, c & 63, 0), res - n0, 128);
        far_fill(b, c, far_rt + top + n0, res - n0);
    }
}

static void far_row(s16 r)                           /* far row r across the resident columns */
{
    static u16 w[42];
    u16 i, n = far_hi - far_lo + 1, c0 = far_lo & 63, n0;
    if (far_lock && r >= 26) for (i = 0; i < n; i++) w[i] = far_lock_bottom;
    else if ((u16)r >= FROWS) memset(w, 0, n * 2);
    else if (fo_map)
    {
        /* The boss's picture: one pointer stepped a column at a time. */
        s16 c = far_lo;
        const u16 *m = fo_map + MULS(c, fo_rows) + r;
        for (i = 0; i < n; i++, c++, m += fo_rows) w[i] = (u16)c < fo_cols ? *m : 0;
    }
    else if (FWRAP)
    {
        /* One wrap and one multiply, then step along the row (a far_col per
         * word, a division each, cost 17 lines on a climbing frame). */
        u16 rows = FROWS, cols = FCOLS, left;
        s16 c = far_lo % (s16)cols;
        const u16 *m;
        if (c < 0) c += cols;
        m = FMAP + MULU(c, rows) + r;
        left = cols - c;
        for (i = 0; i < n; i++)
        {
            w[i] = *m;
            m += rows;
            if (!--left) { m = FMAP + r; left = cols; }
        }
    }
    else
    {
        u16 rows = FROWS, cols = FCOLS;
        s16 c = far_lo;
        const u16 *m = FMAP + MULS(c, rows) + r;
        for (i = 0; i < n; i++, c++, m += rows) w[i] = (u16)c < cols ? *m : 0;
    }
    n0 = 64 - c0 < n ? 64 - c0 : n;
    bq_put(c0, r & 31, w, n0, FALSE);
    if (n0 < n) bq_put(0, r & 31, w + n0, n - n0, FALSE);
}

static s16 far_y_of(s16 y)                           /* far_ty for view y */
{
#ifdef NO_FARY
    (void)y;
    return sd->far_top;                     /* measurement builds: no vertical far movement */
#else
    return far_lock || hud_plane ? sd->far_top : sd->far_top - ((272 - y) >> sd->far_shift);   /* camera y >> shift */
#endif
}

#define FAR_X(x) ((((x) - 32) >> sd->far_shift) + 32)      /* camera x >> shift, + 32 (farlayer.py) */
#define FAR_VIEW_X(fx) ((((fx) - 32) << sd->far_shift) + 32)
/* Stage 6's area-6 sway (stage6.md 2.1): an extra x for the far picture's
 * rows 3-25 only, by their scroll entries (the text rows written into the
 * far ring, the boss bar on row 2, stay put); its columns are streamed
 * that much further. 0 elsewhere. */
s16 stage_far_dx;
static s16 last_dx;

/* Far picture cells (column-major list of (column, row)) set to words:
 * column runs of consecutive rows, one write each (stage 6's storm). */
void stage_far_cells(const u8 *cells, const u16 *w, const u16 *was, u16 n)
{
    u16 i = 0;
    while (i < n)
    {
        u16 k;
        if (was && w[i] == was[i]) { i++; continue; }   /* only the cells that change */
        k = i + 1;
        while (k < n && cells[k * 2] == cells[i * 2] && cells[k * 2 + 1] == cells[(k - 1) * 2 + 1] + 1 && (!was || w[k] != was[k])) k++;
        vq_put(NT(PLANE_B, cells[i * 2] & 63, cells[i * 2 + 1] & 31), w + i, k - i, 128, FALSE);
        i = k;
    }
}

/* The whole far picture into the ring at once (stage 6's area 6: 55
 * columns; its sway then needs no streaming): columns far_hi+1 .. the
 * picture's last, far_lo kept. */
void stage_far_all(void)
{
    while (far_hi < (s16)FCOLS - 1 && far_hi - far_lo < 63) { far_hi++; far_column(far_hi); }
}

static void far_init(s16 x, s16 y)
{
    s16 c;
    far_x = fo_map ? fo_x : FAR_X(x);
    far_ty = fo_map ? fo_ty : far_y_of(y);
    far_rt = far_ty >> 3;
    far_lo = far_x >> 3;
    far_hi = far_lo + 40;
    for (c = far_lo; c <= far_hi; c++) far_column(c);
}

static void far_scroll(s16 x, s16 y)
{
    s16 rt;
    if (fo_relay == 4)
    {
        /* A pose change (stage5's necks): only the rows of each column that
         * differ (tools/bosspack5.py), RELAY_WORDS a frame, from ROM. */
        s16 budget = RELAY_WORDS;
        if (fo_col < far_lo) fo_col = far_lo;
        while (fo_col <= far_hi && budget > 0)
        {
            s16 c = fo_col++, r0, r1, n, p, n0;
            if ((u16)c >= fo_cols) continue;
            r0 = fo_spans[c * 2]; r1 = fo_spans[c * 2 + 1];
            if (r0 > r1) continue;
            if (r0 < far_rt) r0 = far_rt;
            if (r1 > far_rt + FAR_RES - 1) r1 = far_rt + FAR_RES - 1;
            if (r0 > r1 || r0 < 0) continue;
            n = r1 - r0 + 1; p = r0 & 31; n0 = 32 - p < n ? 32 - p : n;
            vq_put(NT(PLANE_B, c & 63, p), fo_map + MULU(c, fo_rows) + r0, n0, 128, FALSE);
            if (n0 < n) vq_put(NT(PLANE_B, c & 63, 0), fo_map + MULU(c, fo_rows) + r0 + n0, n - n0, 128, FALSE);
            budget -= n;
        }
        if (fo_col > far_hi) fo_relay = 0;
    }
    else if (fo_relay >= 2)
    {
        /* A new picture at the same place (a pose change): the resident
         * columns rewritten in place (from ROM: far_column), the ring as it
         * is, half of them a frame (a whole re-lay is ~4 KB and 82 DMA
         * entries: the VBlank interrupt would run into the H interrupt that
         * moves the HUD band). */
        s16 c, mid = far_lo + 21;
        if (fo_relay == 2) { for (c = far_lo; c < mid; c++) far_column(c); fo_relay = 3; }
        else { for (c = mid; c <= far_hi; c++) far_column(c); fo_relay = 0; }
    }
    else if (fo_relay)
    {
        fo_relay = 0;
        far_init(x, y);                     /* the override's position */
        hud_rows_put();
    }
    far_x = fo_map ? fo_x : FAR_X(x);
    while (far_hi < (far_x >> 3) + 40 && (FWRAP || fo_map || far_hi < (s16)FCOLS - 1)) { far_hi++; far_lo++; far_column(far_hi); }
    while (far_lo > (far_x >> 3) && (far_lo > 0 || fo_map)) { far_lo--; far_hi--; far_column(far_lo); }
    /* Vertically one row a frame; until the ring catches up the far
     * layer stays inside the resident rows. */
    far_ty = fo_map ? fo_ty : far_y_of(y);
    rt = far_ty >> 3;
    if (rt != far_rt)
    {
        u16 k;
        if (rt > far_rt) { far_rt++; far_row(far_rt + FAR_RES - 1); }
        else { far_rt--; far_row(far_rt); }
        /* The bottom band moves with the ring: its words as laid out. */
        for (k = 2; k < 4; k++) vq_put(NT(PLANE_B, 0, hud_prow(k)), hud_w[k], 40, 2, FALSE);
    }
    if (far_ty < far_rt * 8) far_ty = far_rt * 8;
    if (far_ty > far_rt * 8 + 7) far_ty = far_rt * 8 + 7;
}

static void put_column(s16 c);

/* Stage 5's boss (src/fam_boss5.c): plane A's arena columns c0..c1-1 from
 * map (32 rows from r0, column-major; their tiles already in VRAM: no
 * column loads while it is on), or NULL. */
void stage_arena(const u16 *map, u16 c0, u16 c1, u16 r0)
{
    s16 c;
    arena_map = map; arena_c0 = c0; arena_c1 = c1; arena_r0 = r0;
    for (c = lo; c <= hi; c++) put_column(c);
}

/* A picture (cols x rows words, column-major) in place of the far layer,
 * or NULL; stage_far_at places it (picture px at screen x 0, picture row
 * px at screen line 0) for the next stage_scroll. */
void stage_far_pic(const u16 *map, u16 cols, u16 rows)
{
    if (map == fo_map) return;
    if (!map) { fo_map = NULL; fo_relay = 0; if (!b_fixed) { far_init(FAR_VIEW_X(far_x), stage_view_y); hud_rows_put(); } return; }
    fo_relay = fo_map ? 2 : 1;              /* laid out in the next stage_scroll (with its scroll values); a relay in progress starts again */
    fo_map = map; fo_cols = cols; fo_rows = rows;
}

void stage_far_at(s16 x, s16 ty) { fo_x = x; fo_ty = ty; }

/* A picture that differs from the one shown only in the rows spans names
 * (per column: first, last; first > last: none): re-laid over a few frames. */
void stage_far_pic_spans(const u16 *map, const u8 *spans)
{
    fo_map = map;
    fo_spans = spans;
    fo_col = far_lo;
    fo_relay = 4;
}

bool stage_far_busy(void) { return fo_relay != 0; }

void stage_far_lock(bool on, u16 bottom)
{
    bool was = far_lock;
    far_lock = on;
    far_lock_bottom = bottom;
    if (was && !on && !b_fixed) { far_init(FAR_VIEW_X(far_x), stage_view_y); hud_rows_put(); }   /* its rows 26-27 back */
}

/* Plane B word under screen cell (i, y): the far layer as shown now. */
u16 stage_bword(u16 i, u16 y)
{
    if (b_pic) return b_pic[y * 40 + i] + b_pic_add;
    if (b_fixed) return 0;
    return far_word((far_x >> 3) + i, (far_ty + (s16)(y * 8)) >> 3);
}

/* The 40 words of screen row y as stage_bword gives them. */
void stage_brow(u16 y, u16 *w)
{
    u16 i;
    if (b_pic)
    {
        const u16 *m = b_pic + y * 40;
        for (i = 0; i < 40; i++) w[i] = m[i] + b_pic_add;
        return;
    }
    {
        s16 c = far_x >> 3, r = (far_ty + (s16)(y * 8)) >> 3;
        for (i = 0; i < 40; i++) w[i] = b_fixed ? 0 : far_word(c + i, r);
    }
}

/* n words at screen cell (x, y) of plane B, where the far scroll puts them
 * now; tm DMA_QUEUE for words that stay put (ROM), else DMA_QUEUE_COPY.
 * (The HUD's rows go through stage_hudput.) */
static void bput(u16 x, u16 y, const u16 *w, u16 n, TransferMethod tm)
{
    u16 c, n0, r;
    bool copy = tm != DMA_QUEUE;
    /* Through the VBlank list: in order with the far layer's own writes. */
    if (b_fixed) { vq_put(NT(PLANE_B, x, y), w, n, 2, copy); return; }
    r = ((far_ty + (s16)(y * 8)) >> 3) & 31;
    c = ((far_x >> 3) + x) & 63;
    n0 = 64 - c < n ? 64 - c : n;
    vq_put(NT(PLANE_B, c, r), w, n0, 2, copy);
    if (n0 < n) vq_put(NT(PLANE_B, 0, r), w + n0, n - n0, 2, copy);
}

void stage_bput(u16 x, u16 y, const u16 *w, u16 n)
{
    bput(x, y, w, n, DMA_QUEUE_COPY);
}

void stage_bput_rom(u16 x, u16 y, const u16 *w, u16 n)
{
    bput(x, y, w, n, DMA_QUEUE);
}

static void vscroll_publish(void)
{
    if (b_fixed) { vs_next[0] = vs_next[1] = 0; return; }
    vs_next[0] = far_ty & 0x3FF;
    vs_next[1] = (hud_prow(2) * 8 - 208) & 0x3FF;
}

/* Plane B as a fixed full-screen picture (world map, story page) or back to
 * the far layer. */
void stage_bfixed(bool on)
{
    u16 i;
    if (hud_win) { VDP_setWindowVPos(FALSE, 0); hud_win = 0; }   /* a picture over the arena: its own top rows */
    b_fixed = on;
    b_pic = NULL;                           /* stage_bpicture sets its picture after this */
    hud_top_mode(on || hud_plane);          /* over a picture the top rows are on plane B: the glyphs' own tiles back */
    split_update();
    last_x = 0x7FFF;                        /* the next stage_scroll rewrites the tables */
    vscroll_publish();
    if (on)
    {
        nvq = 0;                            /* plane writes for the scene being replaced */
        npend = 0;
        stage_commit();
        VDP_setVerticalScroll(BG_B, 0);
    }
    if (!on) return;
    for (i = 0; i < 28; i++) hs_a[i] = hs_b[i] = 0;
    VDP_setHorizontalScrollTile(BG_A, 0, hs_a, 28, CPU);
    VDP_setHorizontalScrollTile(BG_B, 0, hs_b, 28, CPU);
}

/* Plane B as a full-screen picture under the HUD (the shop): map words
 * (40 x 28) + add, the HUD's glyphs kept; NULL: the far layer's tiles back
 * (the picture borrowed their VRAM); the caller's stage_restart lays its
 * words. */
void stage_bpicture(const u16 *map, u16 add)
{
    static u16 w[40];
    u16 r, i;
    if (!map)
    {
        stage_bfixed(FALSE);
        split_set(FALSE);                   /* until stage_restart has laid the far layer */
        VDP_loadTileData((const u32 *)sd->bd_tiles, sd->bd_base, sd->bd_count, DMA);
        return;
    }
    stage_bfixed(TRUE);
    b_pic = map;
    b_pic_add = add;
    for (r = 0; r < 28; r++)
    {
        const u16 *g = r < 2 ? stage_hudrow[r] : r >= 26 ? stage_hudrow[r - 24] : NULL;
        for (i = 0; i < 40; i++) w[i] = g && g[i] ? g[i] : map[r * 40 + i] + add;
        VDP_setTileMapDataRow(BG_B, w, r, 0, 40, CPU);
    }
}

/* The far layer back after the intro's sky borrowed its VRAM: tiles in four
 * parts, then the words (one step a frame, 0..STAGE_BACKDROP_STEPS-1). */
void stage_backdrop(u16 step)
{
    u16 q = (sd->bd_count + 3) / 4;
    if (step < 4)
    {
        u16 n = step == 3 ? sd->bd_count - 3 * q : q;
        DMA_queueDma(DMA_VRAM, (void *)(sd->bd_tiles + (u32)step * q * 32), (sd->bd_base + step * q) * 32, n * 16, 2);
        return;
    }
    far_init(FAR_VIEW_X(far_x), stage_view_y);
    hud_rows_put();
    vscroll_publish();
}

/* rlo: resident map rows rlo .. rlo + 31 (declared with lo, hi), at most
 * RMAX (the map's last 32 rows). */
#define RMAX ((s16)((sd->world_top + 1) >> 3) - 32)
s16 stage_view_y;           /* the view's top map y actually shown */
u16 stage_queued;           /* column tiles queued for this VBlank */
u16 stage_peak_tiles;

static const u16 *right_list(u16 c, u16 *n)
{
    const u16 *offs = sd->loads + 2;
    *n = offs[c + 1] - offs[c];
    return sd->loads + 2 + (2 * sd->cols + 1) + offs[c] * 2;
}

static const u16 *left_list(u16 c, u16 *n)
{
    const u16 *offs = sd->loads + 2 + sd->cols;
    *n = offs[c + 1] - offs[c];
    return sd->loads + 2 + (2 * sd->cols + 1) + offs[c] * 2;
}

/* Tiles to copy at the next VBlank: (source, VRAM byte address). The CPU
 * copies them (stage_vblank): queuing each 32-byte tile as its own SGDK DMA
 * cost ~600 cycles of frame time per tile. */
#define MAX_PENDING 128
static const u32 *pend_src[MAX_PENDING];
static u16 pend_dst[MAX_PENDING];
static u16 npend;

static void load_list(const u16 *e, u16 n)
{
    while (n-- && npend < MAX_PENDING)
    {
        pend_src[npend] = (const u32 *)(sd->tiles + (u32)e[0] * 32);
        pend_dst[npend++] = (STAGE_POOL_BASE + e[1]) * 32;
        e += 2;
        stage_queued++;
    }
}

/* SGDK's VBlank callback: the queued writes. */
void stage_vblank(void)
{
    if (npend || nvq) vblank_flush();
    if (hud_win == 2) { VDP_setWindowVPos(FALSE, 2); hud_win = 1; }   /* the rows just landed */
}

/* The top HUD rows on the window plane (TRUE) or back to sprites. */
void stage_hud_window(bool on)
{
    if (on == (hud_win != 0) || b_fixed || hud_plane) return;
    if (on)
    {
        hud_win = 2;
        hud_top_mode_tm(TRUE, DMA_QUEUE);               /* the glyphs' own tiles; the sprite copies stop */
        hud_seg_put(0, 0, 40);
        hud_seg_put(1, 0, 40);
    }
    else
    {
        VDP_setWindowVPos(FALSE, 0);
        hud_win = 0;
        hud_top_mode(FALSE);
    }
}

static void vblank_flush(void)
{
    vu32 *ctrl = (vu32 *)VDP_CTRL_PORT, *data = (vu32 *)VDP_DATA_PORT;
    u16 i, ints = SYS_getAndSetInterruptMaskLevel(7);   /* CPU streaming: no split write between address and data */
    for (i = 0; i < npend; i++)
    {
        const u32 *s = pend_src[i];
        u16 a = pend_dst[i];
        *ctrl = VDP_WRITE_VRAM_ADDR(a);
        *data = s[0]; *data = s[1]; *data = s[2]; *data = s[3];
        *data = s[4]; *data = s[5]; *data = s[6]; *data = s[7];
    }
    vq_flush();
    SYS_setInterruptMaskLevel(ints);
    npend = 0;
}

/* The waterfall's palette cycle (stagepack CYCLE): the next state's 12
 * words every sd->cycle_step updates, queued for VBlank. Art pass: word 72
 * of the cycle data is the camera x it starts at; left of it pens 10-15 of
 * lines 2-3 are ordinary scenery colours (sd->pal: stagepack CYCLE_FROM),
 * so the stage's first part has 15 colours a line. */
static u16 cyc_state;
static s8 cyc_mode = -1;                    /* 0: scenery pens, 1: cycling, -1: not yet written */

static u16 cyc_wanted(void)
{
    return !(sd->cycle[72] && player.cam_x < (s16)sd->cycle[72]);
}

void stage_cycle_reset(void) { cyc_mode = -1; }

/* A 32-word copy of lines 2-3 (a fade's target) as the camera needs them. */
void stage_cycle_pal(u16 *pal)
{
    if (!sd->cycle_step || !cyc_wanted()) return;
    memcpy(pal + 10, sd->cycle + cyc_state * 12, 12);
    memcpy(pal + 26, sd->cycle + cyc_state * 12 + 6, 12);
}

void stage_cycle(void)
{
    extern u8 boss_colors;
    static u16 timer;
    s8 m;
    if (boss_colors || !sd->cycle_step) return;   /* the boss's fur holds the cycle pens */
    /* The state keeps its own clock from the stage start in both modes, so
     * the cycle's writes fall on the frames they always did (a write moved
     * onto a heavy streaming frame can make it late). */
    if (++timer >= sd->cycle_step) { timer = 0; if (++cyc_state == 6) cyc_state = 0; }
    if (timer) return;                      /* writes only on the cycle's own frames (no extra transfers) */
    m = cyc_wanted();
    if (m != cyc_mode)
    {
        /* The switch waits for a cycle frame (at most cycle_step updates):
         * no tile near the switch uses pens 10-15 (stagepack CYCLE_FROM
         * leaves a strip a window wide), and an extra queued write on another
         * frame was measured to make heavy frames late (05u: 77 drawn per 100). */
        cyc_mode = m;
        PAL_setColors(32 + 10, m ? sd->cycle + cyc_state * 12 : sd->pal + 10, 6, DMA_QUEUE);
        PAL_setColors(48 + 10, m ? sd->cycle + cyc_state * 12 + 6 : sd->pal + 26, 6, DMA_QUEUE);
        return;
    }
    if (!m) return;
    PAL_setColors(32 + 10, sd->cycle + cyc_state * 12, 6, DMA_QUEUE);
    PAL_setColors(48 + 10, sd->cycle + cyc_state * 12 + 6, 6, DMA_QUEUE);
}

/* Column c's resident rows into the plane (two pieces where the band wraps). */
/* Stage 2's wall torches (5.8): their three frames live in the road's pool
 * slots while the fort is played (the road loads over them at boarding).
 * A torch writes its 4 x 4 cells (row-major words) when resident. */
#include "../data/stage2/torch.h"
extern const u8 torch_tiles[];

/* At the end of the road's slots: the fort's added rows use the others
 * (the road loads only at the leap, past those columns). */
#define TORCH_BASE (CHASE_ROAD_BASE + CHASE_ROAD_TILES - TORCH_TILES)

void stage_torch_block(s16 c, s16 r, const u16 *w, u16 pri)
{
    static u16 b[4][4];
    u16 i, k, bit = 1;
    if (chase) return;
    if (!torch_loaded)
    {
        DMA_queueDma(DMA_VRAM, (void *)torch_tiles, TORCH_BASE * 32, TORCH_TILES * 16, 2);
        torch_loaded = 1;
    }
    if (c < lo || c + 3 > hi || r < rlo || r + 3 > rlo + 31) return;   /* not resident: the column load shows frame 0 */
    for (i = 0; i < 4; i++)
    {
        for (k = 0; k < 4; k++, bit <<= 1)
            b[i][k] = (w[i * 4 + k] + TORCH_BASE) | (pri & bit ? 0x8000 : 0);
        if (((c & 63) + 3) < 64) vq_put(NT(PLANE_A, c & 63, (r + i) & 31), b[i], 4, 2, TRUE);
        else for (k = 0; k < 4; k++) vq_put(NT(PLANE_A, (c + k) & 63, (r + i) & 31), b[i] + k, 1, 2, TRUE);
    }
}

/* Stage 6's torches and chandeliers (src/fam_stage6.c): a block of plane-A
 * cells (row-major words, tile index from 0) written when resident. */
u8 anim6_loaded;                /* their tiles are in VRAM (stage_init, stage_restart clear it) */
u8 torch5_loaded;               /* stage 5's torch frames in VRAM (likewise) */
void stage_cells_put(s16 c, s16 r, const u16 *w, u16 cols, u16 rows, u16 base)
{
    stage_cells_put_pri(c, r, w, cols, rows, base, stage_cells_pri(c, r, cols, rows));
}

/* The map's priority bits of a block (up to 16 cells, row-major from bit 0):
 * they never change, so an animator reads them once. */
u16 stage_cells_pri(s16 c, s16 r, u16 cols, u16 rows)
{
    u16 i, k, m = 0, bit = 1;
    for (i = 0; i < rows; i++)
        for (k = 0; k < cols; k++, bit <<= 1)
            if (map_word(c + k, r + i) & 0x8000) m |= bit;
    return m;
}

void stage_cells_put_pri(s16 c, s16 r, const u16 *w, u16 cols, u16 rows, u16 base, u16 pri)
{
    static u16 b[6];
    u16 i, k;
    if (c < lo || c + cols - 1 > hi || r < rlo || r + rows - 1 > rlo + 31) return;
    if (((c & 63) + cols - 1) < 64)
    {
        /* The usual case (no wrap): each row's words built in the queue. */
        for (i = 0; i < rows; i++)
        {
            u16 *o = vq_alloc(NT(PLANE_A, c & 63, (r + i) & 31), cols, 2);
            for (k = 0; k < cols; k++, pri >>= 1) *o++ = (*w++ + base) | (pri & 1 ? 0x8000 : 0);
        }
        return;
    }
    for (i = 0; i < rows; i++)
    {
        for (k = 0; k < cols; k++, pri >>= 1) b[k] = (w[i * cols + k] + base) | (pri & 1 ? 0x8000 : 0);
        if (((c & 63) + cols - 1) < 64) vq_put(NT(PLANE_A, c & 63, (r + i) & 31), b, cols, 2, TRUE);
        else for (k = 0; k < cols; k++) vq_put(NT(PLANE_A, (c + k) & 63, (r + i) & 31), b + k, 1, 2, TRUE);
    }
}

static void put_column(s16 c)
{
    static u16 w2[34];
    const u16 *m;
    u16 r0 = rlo & 31, n0 = 32 - r0;
    if (chase && c >= 224)
    {
        /* A road column: the block's words for map rows 32..63, empty
         * above and below (a column every 1.3 frames at full speed). */
        static u16 w[32];
        u16 k = road_seq[c >> 5], i = 0;
        s16 r = rlo;
        if (k != 0xFF)
        {
            const u16 *src = road_blocks + ((((k << 5) | (c & 31)) << 5));
            for (; i < 32 && r < 32; i++, r++) w[i] = 0;
            for (; i < 32 && r < 64; i++, r++) w[i] = src[r - 32];
        }
        for (; i < 32; i++) w[i] = 0;
        vq_put(NT(PLANE_A, c & 63, r0), w, n0, 128, TRUE);
        if (r0) vq_put(NT(PLANE_A, c & 63, 0), w + n0, r0, 128, TRUE);
        return;
    }
    if (rlo < 0)
    {
        static u16 w[32];
        u16 i;
        for (i = 0; i < 32; i++) w[i] = cell(c, rlo + i);
        vq_put(NT(PLANE_A, c & 63, r0), w, n0, 128, TRUE);
        if (r0) vq_put(NT(PLANE_A, c & 63, 0), w + n0, r0, 128, TRUE);
        return;
    }
    if (arena_map && c >= arena_c0 && c < arena_c1)
    {
        static u16 w[32];
        u16 i;
        for (i = 0; i < 32; i++) w[i] = cell(c, rlo + i);
        vq_put(NT(PLANE_A, c & 63, r0), w, n0, 128, TRUE);
        if (r0) vq_put(NT(PLANE_A, c & 63, 0), w + n0, r0, 128, TRUE);
        return;
    }
    {
        /* The metatile column, two rows a metatile, from the metatile
         * holding row rlo; m is row rlo. */
        const u16 *e = sd->map + 2 + ((c >> 1) << 2);
        const u16 *src = sd->map + 2 + (sd->cols << 1) + *(const u32 *)e;
        const u16 *dict = sd->map + *(const u32 *)sd->map + (c & 1);
        s16 m0 = rlo >> 1, first = e[2] - m0, last = first + e[3];
        u16 *o = w2;
        s16 i;
        for (i = 0; i < 17; i++, o += 2)
        {
            if (i >= first && i < last)
            {
                const u16 *t = dict + (src[i - first] << 2);
                o[0] = t[0];
                o[1] = t[2];
            }
            else o[0] = o[1] = 0;
        }
        m = w2 + (rlo & 1);
    }
    vq_put(NT(PLANE_A, c & 63, r0), m, n0, 128, TRUE);
    if (r0) vq_put(NT(PLANE_A, c & 63, 0), m + n0, r0, 128, TRUE);
}

/* Map row r across the resident columns. */
static void put_row(s16 r)
{
    u16 w[WINDOW_COLS + 1], i, x0 = lo & 63, n0 = 64 - x0;
    if (chase || sky_map || arena_map || r < 0)
        for (i = 0; i < WINDOW_COLS; i++) w[i] = cell(lo + i, r);
    else
    {
        /* The metatiles of row r >> 1 two columns at a time (map_word's data). */
        const u16 *dict = sd->map + *(const u32 *)sd->map + ((r & 1) << 1);
        const u16 *base = sd->map + 2 + (sd->cols << 1);
        s16 c = lo & ~1, mr = r >> 1;
        u16 *o = w - (lo & 1);              /* an odd first column: its pair's left word lands before w */
        u16 skip = lo & 1, first = 1;
        for (; c < lo + WINDOW_COLS; c += 2, o += 2)
        {
            const u16 *e = sd->map + 2 + ((c >> 1) << 2);
            u16 k = mr - e[2];
            u16 a = 0, b = 0;
            if ((u16)c < sd->cols && k < e[3])
            {
                const u16 *t = dict + (base[*(const u32 *)e + k] << 2);
                a = t[0]; b = t[1];
            }
            if (first && skip) o[1] = b;
            else { o[0] = a; if (c + 1 < lo + WINDOW_COLS) o[1] = b; }
            first = 0;
        }
    }
    if (n0 >= WINDOW_COLS) vq_put(NT(PLANE_A, x0, r & 31), w, WINDOW_COLS, 2, TRUE);
    else
    {
        vq_put(NT(PLANE_A, x0, r & 31), w, n0, 2, TRUE);
        vq_put(NT(PLANE_A, 0, r & 31), w + n0, WINDOW_COLS - n0, 2, TRUE);
    }
}

static void foreground(s16 x, s16 y);

static void hud_where(bool fixed)
{
    extern u8 area6;
    extern u8 stage_index;
    hud_plane = stage_index == 5 && area6 < 4;  /* stage 6's corridors */
    if (hud_win) { VDP_setWindowVPos(FALSE, 0); hud_win = 0; }
    hud_top_mode(hud_plane || fixed);
}

void stage_init(s16 x, s16 y)
{
    hud_where(FALSE);                       /* (b_fixed ends here) */
    fo_map = NULL;
    fo_relay = 0;
    arena_map = NULL;
    VDP_setPlaneSize(64, 32, FALSE);
    VDP_setHScrollTableAddress(0xA800);
    VDP_setSpriteListAddress(0xAC00);
    VDP_setWindowAddress(0xB000);
    VDP_setBGBAddress(0xC000);
    VDP_setBGAAddress(0xE000);
    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);  /* per 8-px row: the HUD rows stay put */
    PAL_setColors(32, sd->pal, 32, DMA);
    stage_cycle_reset();                    /* pens 10-15 as the camera needs them at the next update */

    /* The far layer on plane B (parallax; the user's choice, AGENTS.md). */
    b_fixed = 0;
    b_pic = NULL;
    chase = 0;
    stage_far_dx = 0;
    torch_loaded = 0;
    anim6_loaded = 0;
    torch5_loaded = 0;
    { extern u8 chain6_loaded; chain6_loaded = 0; }
    split_update();
    last_x = 0x7FFF;
    {
        static const u32 blank[8];
        VDP_loadTileData(blank, 0, 1, CPU);  /* tile 0: the planes' empty cells (a scene before may have used it) */
    }
    VDP_clearPlane(BG_A, TRUE);             /* rows the map never writes (above a short stage) keep no old scene */
    VDP_loadTileData((const u32 *)sd->bd_tiles, sd->bd_base, sd->bd_count, DMA);
    far_init(x, y);
    hud_rows_put();
    posecache_unlend();
    foreground(x, y);
    SYS_setVIntCallback(stage_vint);
    SYS_setHIntCallback(stage_hint);
    VDP_setHIntCounter(15);
}

/* A restart elsewhere in the stage: the foreground only (the backdrop and
 * the HUD rows on plane B stay). */
void stage_restart(s16 x, s16 y)
{
    hud_where(b_fixed);
    npend = 0;
    fo_map = NULL;
    arena_map = NULL;
    if (!b_fixed) { far_init(x, y); hud_rows_put(); }
    torch_loaded = 0;
    anim6_loaded = 0;
    torch5_loaded = 0;
    { extern u8 chain6_loaded; chain6_loaded = 0; }
    posecache_unlend();
    foreground(x, y);
    stage_commit();                         /* a blocking fade-in may follow: its frames use these values */
}

static void foreground(s16 x, s16 y)
{
    static u16 slot_tile[STAGE_POOL];
    u16 n, i;
    s16 c;

    /* Foreground: replay the column loads up to the window, then upload
     * whichever tile each slot ends up holding. */
    lo = x >> 3;
    hi = lo + WINDOW_COLS - 1;
    y += sd->world_top - 511;               /* the virtual view y (272 - camera y) as a map y */
    rlo = (y >> 3) - 2;
    if (rlo < rmin) rlo = rmin;
    if (rlo > RMAX) rlo = RMAX;
    if (hi >= sd->cols) hi = sd->cols - 1;
    for (i = 0; i < sd->pool; i++) slot_tile[i] = 0;
    for (c = 0; c <= hi; c++)
    {
        const u16 *e = right_list(c, &n);
        while (n--) { slot_tile[e[1]] = e[0]; e += 2; }
    }
    split_set(FALSE);                       /* CPU streaming: the scroll splits wait */
    for (i = 0; i < sd->pool; i++)
        VDP_loadTileData((const u32 *)(sd->tiles + (u32)slot_tile[i] * 32), STAGE_POOL_BASE + i, 1, CPU);
    split_update();
    for (c = lo; c <= hi; c++) put_column(c);
    stage_scroll(x, y);
}

s16 stage_scroll(s16 x, s16 y)
{
    s16 want = x >> 3;
    u16 n, budget = COLS_PER_FRAME;

    stage_queued = 0;
    while (budget && hi < want + WINDOW_COLS - 1 && (chase || hi < sd->cols - 1))
    {
        const u16 *e;
        hi++; lo++; budget--;
        if (hi < sd->cols && !arena_map) { e = right_list(hi, &n); load_list(e, n); }
        put_column(hi);
    }
    while (budget && lo > want && lo > 0)
    {
        const u16 *e;
        lo--; hi--; budget--;
        if (lo < sd->cols && !arena_map) { e = left_list(lo, &n); load_list(e, n); }
        put_column(lo);
    }
    if (stage_queued > stage_peak_tiles) stage_peak_tiles = stage_queued;
    /* After a camera jump the window catches up over a few frames; until
     * then the view stays inside the columns already loaded. */
    if (x >> 3 > lo) x = (lo << 3) | 7;
    if (x >> 3 < lo) x = lo << 3;
    /* Rows: one a frame keeps the 32-row band around the 28-29 rows shown;
     * until it catches up the view stays inside the band. */
    y += sd->world_top - 511;               /* map y; stage_view_y stays virtual (the sprites') */
    if (y >> 3 < rlo && rlo > rmin) put_row(--rlo);
    else if ((y + 223) >> 3 > rlo + 31 && rlo < RMAX) put_row(++rlo + 31);
    if (y < rlo << 3) y = rlo << 3;
    if (y > (rlo << 3) + 32) y = (rlo << 3) + 32;
    stage_view_y = y - (sd->world_top - 511);
    {
        u16 i;
        if (!b_fixed) far_scroll(x, stage_view_y);
        u16 wv = spell_wave();
        if (x != last_x || b_fixed != last_fixed || split_on != last_split || wv || last_wave || stage_far_dx != last_dx)   /* the tables only when the view moved */
        {
            last_x = x;
            last_fixed = b_fixed;
            last_split = split_on;
            {
                s16 a = -x, b = b_fixed ? 0 : -far_x;
                for (i = 0; i < 28; i++) { hs_a[i] = a; hs_b[i] = b; }
                if (stage_far_dx && !b_fixed) for (i = 9; i < 26; i++) hs_b[i] = b - stage_far_dx;   /* rows 2-8 hold plane-B text (the boss bar, dialogue) */
                last_dx = stage_far_dx;
                if (split_on && !b_fixed) hs_b[26] = hs_b[27] = 0;    /* the bottom band holds still */
                if (hud_plane && !b_fixed) hs_b[0] = hs_b[1] = 0;
                if (wv)
                {
                    /* Spell 10's wave (0x690C, table 0x6A46 >> 4, odd rows still),
                     * per 8-px row here (the arcade: per line). */
                    static const u8 wave_tab[32] = { 0, 0, 1, 1, 2, 2, 3, 4, 5, 6, 7, 9, 10, 12, 13, 15, 17, 19, 22, 24,
                                                     27, 29, 32, 35, 38, 41, 44, 48, 51, 55, 58, 62 };
                    for (i = 2; i < 26; i += 2) hs_a[i] = a - (wave_tab[(i + (wv >> 1)) & 31] >> 4);
                }
                last_wave = wv;
            }
            vq_put(0xA800, (u16 *)hs_a, 28, 32, TRUE);     /* tile mode: an entry each 8 lines (32 bytes) */
            vq_put(0xA802, (u16 *)hs_b, 28, 32, TRUE);
        }
    }
    vscroll_publish();
    if (!split_on && !b_fixed) VDP_setVerticalScrollVSync(BG_B, far_ty);    /* the intro: one scroll for the plane */
    VDP_setVerticalScrollVSync(BG_A, y);
    return x;
}
