#ifndef STAGEDATA_H
#define STAGEDATA_H
#include <genesis.h>

/* One stage's prepared data (tools/stagepack.py, colpack.py, sprpal.py).
 * The VRAM geometry is shared by every stage: foreground pool from tile
 * STAGE_POOL_BASE (STAGE_POOL slots at most), backdrop from STAGE_BD_BASE
 * (STAGE_BD_TILES), data/stage1/layout.h; a stage's packer must fit it. */
typedef struct
{
    const u16 *pal;         /* CRAM lines 2-3 */
    const u8 *tiles;        /* foreground tiles */
    const u16 *map;         /* plane words, column-major, trimmed (tools/maptrim.py) */
    const u16 *loads;       /* per column: tiles to load scrolling right / left */
    const u16 *bd_map;      /* backdrop: 40 x 28 plane B words */
    const u8 *bd_tiles;
    const u16 *cycle;       /* palette cycle states (12 words each), or NULL */
    const u16 *sprpal;      /* sprite CRAM lines 0-1 */
    const u8 *col;          /* collision ids: 16 x 16 cells, column-major, trimmed (maptrim.py --col) */
    const u8 *coldesc;      /* collision descriptors, 8 bytes each */
    u16 cols;               /* map columns (8 px) */
    u16 col_cols;           /* collision columns (16 px) */
    u16 sky;                /* CRAM entry of the sky: the background colour */
    u16 cycle_step;         /* updates per cycle state (0: none) */
    u16 far_cols, far_rows; /* far picture (bd_map) columns and rows */
    s16 far_top;            /* far picture rows above the cam_y = 0 view (px) */
    u8 far_wrap;            /* the far picture repeats horizontally */
    u8 far_shift;           /* far layer = camera >> far_shift (1: half; stage 4: 3) */
    u16 world_top;          /* the stage's top world y (511; stage 4: 8191); map row = (world_top - y) / 8 */
    u16 pool;               /* foreground VRAM slots from STAGE_POOL_BASE (layout.h STAGE_POOL) */
    u16 bd_base, bd_count;  /* far layer / backdrop tiles in VRAM (STAGE_BD_BASE, STAGE_BD_TILES) */
} StageData;

extern const StageData stage_data[];
extern const StageData stage2_data, stage3_data, stage4_data, stage5_data, stage6_data[7];
extern const StageData *sd;     /* the stage being played */

/* A placed object (the arcade's 8-byte stage-table entry). */
typedef struct { u8 cls, variant, kind, dir; s16 x, y; } StageObject;

#endif
