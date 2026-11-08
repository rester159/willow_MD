/* Stage 2's wall torches (class 12 kind 6 variant 0, 0x32FB4; specs/
 * stage2.md 5.8): no sprite; while near the view, every 7 frames the next of
 * three frames is written into the 2 x 2 16-px cells at its map position
 * (4 x 4 plane-A cells here; tools/torchpack.py). Variants 1 and 2 (the
 * breakable scenery) are not in this build.
 * Stage 5's interior torches (variant 5) are a list in src/fam_stage5.c. */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "stage.h"

extern const u16 torch_words[];

void torch_update(Actor *a)
{
    if (!a->state)
    {
        a->state = 2;
        a->active = 0;                      /* never drawn or hit */
        a->timer = 7;
        a->sub = 0;
        a->v[0] = stage_cells_pri((AX(a) >> 4) * 2, ((511 - AY(a)) >> 4) * 2, 4, 4);   /* the map's priority bits, read once */
        return;
    }
    if ((u16)(AX(a) - CAM_X + 16) > 416 || (u16)(AY(a) - CAM_Y + 16) > 288) return;   /* x-16..x+400, y-16..y+272 */
    if (--a->timer) return;
    a->timer = 7;
    if (++a->sub > 2) a->sub = 0;
    stage_torch_block((AX(a) >> 4) * 2, ((511 - AY(a)) >> 4) * 2, torch_words + a->sub * 16, a->v[0]);
}
