/* Sprite tile space (VRAM byte addresses / tile numbers):
 *   after the backdrop's tiles up to the scroll table at 0xA800 (1344)
 *   0xAB80-0xABFF  1372-1375  after the horizontal scroll table (row mode: 0xA800-0xAB7F)
 *   0xAE80-0xAFFF  1396-1407  after the 80-entry sprite table: the HUD's top rows' tiles
 *   0xB000-0xB1FF  1408-1423  window nametable rows 0-3 (debug row): the HUD's top rows' tiles (src/hud.c)
 *   0xB200-0xBFFF  1424-1535  the player's pose (1424-1466: Madmartigan's largest is 43), then free
 *   HUD end-0xDFFF            after the HUD glyphs (from 0xD000), up to plane A at 0xE000
 *   0xF000-0xFFFF  1920-2047  after plane A (64x32)
 * The player keeps 1424-1466; everything else is handed out first fit. Plane B's
 * nametable is 0xC000-0xCFFF (all 32 rows: the far layer's ring): nothing
 * may go there. */
#include <genesis.h>
#include "vram.h"
#include "../data/stage1/layout.h"

static u16 lo[7], hi[7];

void vram_init(u16 hud_end, bool window_row)
{
    lo[0] = 1374; hi[0] = 1376;     /* 0xA800-0xAB7F: the per-line horizontal scroll table; 1372-1373: the HUD's */
    lo[1] = hi[1] = 1408;           /* 1396-1423: the HUD's top rows' tiles (src/hud.c) */
    lo[2] = hi[2] = 1424;       /* window rows 0-3: the intro's text band (and the debug row) */
    (void)window_row;
    lo[3] = 1467; hi[3] = 1536;
    lo[4] = hud_end; hi[4] = 1792;
    lo[5] = 1920; hi[5] = 2048;
    lo[6] = STAGE_BD_BASE + STAGE_BD_TILES; hi[6] = 1344;
#ifdef CACHE_CUT
    lo[6] += CACHE_CUT;         /* measurement builds: less sprite VRAM */
#endif
}

u16 vram_alloc(u16 n)
{
    u16 i;
    for (i = 0; i < 7; i++)
        if (hi[i] - lo[i] >= n)
        {
            u16 t = lo[i];
            lo[i] += n;
            return t;
        }
    return 0;
}

u16 vram_free_tiles(void)
{
    u16 i, n = 0;
    for (i = 0; i < 7; i++) n += hi[i] - lo[i];
    return n;
}

u16 vram_largest(void)
{
    u16 i, n = 0;
    for (i = 0; i < 7; i++) if (hi[i] - lo[i] > n) n = hi[i] - lo[i];
    return n;
}
