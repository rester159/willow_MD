#ifndef VRAM_H
#define VRAM_H
#include <genesis.h>

/* Sprite tiles live in the gaps the planes and tables leave (src/vram.c). */
void vram_init(u16 hud_end, bool window_row);
u16 vram_alloc(u16 n);      /* first fit; 0 when nothing is left */
u16 vram_free_tiles(void);
u16 vram_largest(void);    /* the largest free range */

#endif
