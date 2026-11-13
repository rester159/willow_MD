/* Pictures stored aPLib-packed (tools/pack_compress.sh), at scene changes
 * only (world maps, story pages, the shop, the attract loop's pictures):
 * chunks of up to 256 tiles, each unpacked into SGDK's 8 KB DMA buffer and
 * copied to VRAM. (It was an 8 KB heap block: the heap has barely that much
 * free, and a failed allocation left the old tiles under the picture's map:
 * the corrupted world map of bug 5, reproduced in 06j's first test.) */
#include <genesis.h>
#include "packed.h"

void load_packed(const u8 *src, u16 vram, u16 ntiles)
{
    u16 n = (src[0] << 8) | src[1], k;
    u8 *buf;
    (void)ntiles;
    DMA_flushQueue();                       /* the queue empty, the buffer free (the CPU copies below don't queue) */
    buf = DMA_allocateTemp(4096);
    src += 2;
    for (k = 0; k < n; k++)
    {
        u16 tiles = (src[0] << 8) | src[1], bytes = (src[2] << 8) | src[3];
        aplib_unpack((u8 *)src + 4, buf);
        VDP_loadTileData((const u32 *)buf, vram, tiles, CPU);
        vram += tiles;
        src += 4 + bytes;
    }
    DMA_releaseTemp(4096);
}

/* A picture's 40x28 map stored aPLib-packed (proof build): unpacked into
 * the DMA buffer (as load_packed does); valid until map_close(). */
const u16 *map_open(const void *src)
{
    u16 *buf;
    DMA_flushQueue();
    buf = (u16 *)DMA_allocateTemp(1280);
    aplib_unpack((u8 *)src, (u8 *)buf);
    return buf;
}
void map_close(void) { DMA_releaseTemp(1280); }

/* A packed 40x28 map that must stay readable while its picture is up (the
 * story pictures and the shop: stage.c's b_pic redraws HUD rows over it):
 * one RAM buffer, valid until the next map_keep. */
static u16 keepbuf[1120];
const u16 *map_keep(const void *src)
{
    aplib_unpack((u8 *)src, (u8 *)keepbuf);
    return keepbuf;
}
