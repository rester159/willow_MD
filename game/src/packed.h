#ifndef PACKED_H
#define PACKED_H
#include <genesis.h>

void load_packed(const u8 *src, u16 vram, u16 ntiles);   /* aPLib tiles -> VRAM (src/packed.c) */

const u16 *map_open(const void *src);     /* aPLib 40x28 map -> the DMA buffer (src/packed.c) */
void map_close(void);
const u16 *map_keep(const void *src);    /* aPLib 40x28 map -> a RAM buffer kept until the next call */

#endif
