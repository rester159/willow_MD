/* The stages' prepared data (src/stagedata.h). */
#include <genesis.h>
#include "stagedata.h"
#include "../data/stage1/layout.h"
#include "../data/stage2/chase.h"
#include "../data/stage6/a6/far6.h"

extern const u16 s1_pal[], s1_map[], s1_loads[], s1_bd_map[], s1_cycle[], s1_sprpal[];
extern const u8 s1_tiles[], s1_bd_tiles[], s1_col[], s1_coldesc[];

const StageData stage_data[] = {
    { s1_pal, s1_tiles, s1_map, s1_loads, s1_bd_map, s1_bd_tiles, s1_cycle, s1_sprpal, s1_col, s1_coldesc,
      STAGE_COLS, 240, STAGE_SKY, STAGE_CYCLE_STEP, STAGE_FAR_COLS, STAGE_FAR_ROWS, STAGE_FAR_TOP, 0, 1, 511, STAGE_POOL, STAGE_BD_BASE, STAGE_BD_TILES },
};

/* Stage 2 (data/stage2/layout.h; packed with POOL_SLOTS=687 so its far
 * layer sits where stage 1's does); sprite colours tools/pack_stage2.sh. */
extern const u16 s2_pal[], s2_map[], s2_loads[], s2_bd_map[], s2_sprpal[];
extern const u8 s2_tiles[], s2_bd_tiles[], s2_col[], s2_coldesc[];

const StageData stage2_data = {
    s2_pal, s2_tiles, s2_map, s2_loads, s2_bd_map, s2_bd_tiles, NULL, s2_sprpal, s2_col, s2_coldesc,
    256, 128, CHASE_SKY, 0, 512, 45, 136, 1, 1, 511, 687, 703, 338 };

/* Stage 3 (data/stage3/layout.h, POOL_SLOTS=687; specs/stage3.md): the
 * arcade's map rows from world y 256, so native y = arcade y - 256; sprite
 * colours tools/pack_stage3.sh. */
extern const u16 s3_pal[], s3_map[], s3_loads[], s3_bd_map[], s3_sprpal[], s3_cycle[];
extern const u8 s3_tiles[], s3_bd_tiles[], s3_col[], s3_coldesc[];

const StageData stage3_data = {
    s3_pal, s3_tiles, s3_map, s3_loads, s3_bd_map, s3_bd_tiles, s3_cycle, s3_sprpal, s3_col, s3_coldesc,
    768, 384, 33, 8, 408, 46, 144, 0, 1, 511, 687, 703, 332 };   /* the water's cycle: scroll2 palettes 10 / 11, 6 states x 8 frames (2.6) */

/* Stage 4 (data/stage4/layout.h: pool 281, far tiles 297..1039; specs/stage4.md):
 * world y 0..8191 (the descent), the far layer 256 px wide repeating, 1/8 of
 * the camera both ways. */
extern const u16 s4_pal[], s4_map[], s4_loads[], s4_bd_map[], s4_sprpal[];
extern const u8 s4_tiles[], s4_bd_tiles[], s4_col[], s4_coldesc[];

const StageData stage4_data = {
    s4_pal, s4_tiles, s4_map, s4_loads, s4_bd_map, s4_bd_tiles, NULL, s4_sprpal, s4_col, s4_coldesc,
    2048, 1024, 40, 0, 32, 111, 670, 1, 3, 8191, 281, 297, 743 };

/* Stage 5 (data/stage5/layout.h: pool 493, far tiles 509..962;
 * specs/stage5.md): world y 0..767, the far layer at half the camera both
 * ways (2.1). */
extern const u16 s5_pal[], s5_map[], s5_loads[], s5_bd_map[];
extern u16 s5_sprpal_live[];        /* src/game.c: the stage's fit with the chosen form's line 0 */
extern const u8 s5_tiles[], s5_bd_tiles[], s5_col[], s5_coldesc[];

const StageData stage5_data = {
    s5_pal, s5_tiles, s5_map, s5_loads, s5_bd_map, s5_bd_tiles, NULL, s5_sprpal_live, s5_col, s5_coldesc,
    768, 384, 44, 0, 404, 55, 216, 0, 1, 767, 493, 509, 454 };

/* Stage 6 (specs/stage6.md 2.1): seven areas, each its own scenery set in
 * its own native coordinates (tools/pack_stage6_areas.py: native x = arcade
 * x - XOFF, y = arcade y - YOFF); the data sits in the SSF2 banks above 4 MB
 * (src/s6data.s, bank_stage6). Sprite colours: one fit for the stage
 * (tools/pack_stage6.sh), no scenery lines borrowed (they change by area). */
extern const u16 s6a0_pal[], s6a0_map[], s6a0_loads[], s6a0_bd_map[];
extern const u8 s6a0_tiles[], s6a0_bd_tiles[], s6a0_col[], s6a0_coldesc[];
extern const u16 s6a1_pal[], s6a1_map[], s6a1_loads[], s6a1_bd_map[];
extern const u8 s6a1_tiles[], s6a1_bd_tiles[], s6a1_col[], s6a1_coldesc[];
extern const u16 s6a2_pal[], s6a2_map[], s6a2_loads[], s6a2_bd_map[];
extern const u8 s6a2_tiles[], s6a2_bd_tiles[], s6a2_col[], s6a2_coldesc[];
extern const u16 s6a3_pal[], s6a3_map[], s6a3_loads[], s6a3_bd_map[];
extern const u8 s6a3_tiles[], s6a3_bd_tiles[], s6a3_col[], s6a3_coldesc[];
extern const u16 s6a4_pal[], s6a4_map[], s6a4_loads[], s6a4_bd_map[];
extern const u8 s6a4_tiles[], s6a4_bd_tiles[], s6a4_col[], s6a4_coldesc[];
extern const u16 s6a5_pal[], s6a5_map[], s6a5_loads[], s6a5_bd_map[];
extern const u8 s6a5_tiles[], s6a5_bd_tiles[], s6a5_col[], s6a5_coldesc[];
extern const u16 s6a6_pal[], s6a6_map[], s6a6_loads[], s6a6_bd_map[];
extern const u8 s6a6_tiles[], s6a6_bd_tiles[], s6a6_col[], s6a6_coldesc[];
extern u16 s6_sprpal_live[];        /* src/game.c: the stage's fit with the form's line 0 */

const StageData stage6_data[7] = {
    { s6a0_pal, s6a0_tiles, s6a0_map, s6a0_loads, s6a0_bd_map, s6a0_bd_tiles, NULL, s6_sprpal_live, s6a0_col, s6a0_coldesc,
      320, 160, 52, 0, 168, 28, 0, 0, 1, 511, 257, 273, 460 },
    { s6a1_pal, s6a1_tiles, s6a1_map, s6a1_loads, s6a1_bd_map, s6a1_bd_tiles, NULL, s6_sprpal_live, s6a1_col, s6a1_coldesc,
      320, 160, 52, 0, 173, 28, 0, 0, 1, 511, 282, 298, 34 },
    { s6a2_pal, s6a2_tiles, s6a2_map, s6a2_loads, s6a2_bd_map, s6a2_bd_tiles, NULL, s6_sprpal_live, s6a2_col, s6a2_coldesc,
      320, 160, 54, 0, 168, 28, 0, 0, 1, 511, 275, 291, 161 },
    { s6a3_pal, s6a3_tiles, s6a3_map, s6a3_loads, s6a3_bd_map, s6a3_bd_tiles, NULL, s6_sprpal_live, s6a3_col, s6a3_coldesc,
      320, 160, 47, 0, 173, 28, 0, 0, 1, 511, 216, 232, 1 },
    { s6a4_pal, s6a4_tiles, s6a4_map, s6a4_loads, s6a4_bd_map, s6a4_bd_tiles, NULL, s6_sprpal_live, s6a4_col, s6a4_coldesc,
      64, 32, 34, 0, 45, 28, 0, 0, 1, 511, 153, 169, 238 },
    { s6a5_pal, s6a5_tiles, s6a5_map, s6a5_loads, s6a5_bd_map, s6a5_bd_tiles, NULL, s6_sprpal_live, s6a5_col, s6a5_coldesc,
      128, 64, 44, 0, 83, 158, 1040, 0, 1, 2303, 569, 585, 159 },
    { s6a6_pal, s6a6_tiles, s6a6_map, s6a6_loads, s6a6_bd_map, s6a6_bd_tiles, NULL, s6_sprpal_live, s6a6_col, s6a6_coldesc,
      64, 32, 0, 0, 55, 28, 0, 0, 1, 511, 580, 596, FAR6_TILES },   /* the storm: tools/far6.py (backdrop CRAM 0: black or its glow) */
};

const StageData *sd = stage_data;
