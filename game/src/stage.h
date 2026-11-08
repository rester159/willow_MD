#ifndef STAGE_H
#define STAGE_H
#include <genesis.h>

/* x, y: the view's top-left corner in stage map pixels. */
void stage_init(s16 x, s16 y);
void stage_restart(s16 x, s16 y);   /* foreground only, at a new camera position */
/* Returns the x actually shown (it lags while the window catches up). */
s16 stage_scroll(s16 x, s16 y);
extern u16 stage_peak_tiles;
extern u16 stage_queued;      /* column tiles queued for the next VBlank */
extern s16 stage_view_y;      /* top map y shown (stage_scroll may hold it inside the loaded rows) */
/* Call in VBlank: copies the tiles the last stage_scroll listed. */
void stage_vblank(void);
/* The intro's sky band above the map (map: 48 x 30 words, tile | attributes;
 * vram: VRAM tile per sky tile); NULL turns it off. */
void stage_sky(const u16 *map, const u16 *vram);
void stage_backdrop(u16 step);  /* far layer tiles and words back: steps 0..STAGE_BACKDROP_STEPS-1, a frame each */
#define STAGE_BACKDROP_STEPS 5
u16 stage_bword(u16 i, u16 y);                      /* plane B word under screen cell (i, y) */
void stage_bput(u16 x, u16 y, const u16 *w, u16 n); /* n words at screen cell (x, y) of plane B */
void stage_bput_rom(u16 x, u16 y, const u16 *w, u16 n);   /* the same from words that stay put (ROM) */
void stage_bfixed(bool on);
void stage_brow(u16 y, u16 *w);                 /* screen row y's plane B words, as stage_bword */
void stage_bpicture(const u16 *map, u16 add);   /* a full-screen plane B picture under the HUD; NULL: the far layer back */
extern u16 stage_hudrow[4][40];     /* the HUD's rows 0, 1, 26, 27: glyph words (0: far layer), src/hud.c keeps them */
void stage_hudput(u16 k, u16 x, u16 n);   /* HUD row k (0, 1: screen rows 0-1; 2, 3: rows 26-27), columns x..x+n-1 changed */
void stage_chase(bool on);          /* stage 2's chase: road columns past x 1792, road tiles resident */
void stage_split_scene(void);   /* splits off, plane B at the scene's scroll (the title over the stage) */
void stage_torch_block(s16 c, s16 r, const u16 *w, u16 pri);   /* stage 2's torches: 4 x 4 plane-A cells at map cell (c, r), the map priority bits pri (stage_cells_pri) */
void stage_cells_put(s16 c, s16 r, const u16 *w, u16 cols, u16 rows, u16 base);
u16 stage_cells_pri(s16 c, s16 r, u16 cols, u16 rows);                                  /* the block's map priority bits (row-major, bit 0 first) */
void stage_cells_put_pri(s16 c, s16 r, const u16 *w, u16 cols, u16 rows, u16 base, u16 pri);   /* stage_cells_put with those bits given */   /* a block of plane-A cells (row-major words from tile 0) at map cell (c, r) when resident */
void stage_chase_lend(void);        /* the fort's pool tiles to the sprite cache (the road fills the view) */
void stage_far_step(u16 s);         /* stage 2: the far layer's colour step 0..9 */
void stage_far_lock(bool on, u16 bottom);   /* hold the far layer's vertical position, rows 26-27 = bottom (the intro) */
void stage_split(bool on);          /* allow the HUD's scroll splits (not in the intro) */
void stage_commit(void);            /* once a frame, before the VBlank: the splits' scroll values */
/* Once per update: the stage's palette cycle. */
void stage_cycle(void);
void stage_cycle_reset(void);       /* the stage palette was reloaded: rewrite the cycle pens */
void stage_cycle_pal(u16 *pal);     /* patch a 32-word copy of lines 2-3 for the camera (fades) */

/* Stage 5's boss (src/fam_boss5.c): the arena's plane-A words, the necks'
 * picture in place of the far layer. */
void stage_arena(const u16 *map, u16 c0, u16 c1, u16 r0);
void stage_far_pic(const u16 *map, u16 cols, u16 rows);
void stage_far_at(s16 x, s16 ty);
void stage_far_pic_spans(const u16 *map, const u8 *spans);
bool stage_far_busy(void);

void stage_hud_window(bool on);
u16 stage_vblank_bytes(void);
void stage_vint_late(void);       /* the top HUD rows on the window plane (a mid-boss's fixed camera) */

#endif
