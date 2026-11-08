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
