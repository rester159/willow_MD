#ifndef MAGIC_H
#define MAGIC_H
#include <genesis.h>

/* Willow's charged magic (specs/magic.md 1-2, specs/player.md 3.6). */
typedef struct
{
    u16 meter;          /* charge meter; high byte = charge level */
    u8 active;          /* meter running (HUD bar shows a fill) */
    u8 rel;             /* charge level captured at release; 0 for a tap */
    u8 level;           /* Willow's magic level 0..6 */
    u8 hold;            /* hold counter 0..127 */
    u8 sparks, spark_cd;
} Magic;

extern Magic magic;

void magic_init(void);             /* sparks' tiles (src/vram.c) */
/* The handlers' button check (ground, slide, air): returns 1 when an
 * attack starts, on a press or on a release after a hold of 16+. */
u16 magic_charge(u16 held, u16 pressed);
void magic_sparks(void);           /* drop / climb: sparks only */
void magic_meter(void);            /* once per update, after the player */
u16 magic_spell(void);             /* the spell an attack casts now */
u16 magic_tier(void);              /* Madmartigan: the sword's charge tier 0..3 */
void magic_update(void);           /* spark effects */
u16 magic_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y);

void spark_art(const u8 *poses, const u8 *tiles, const s16 *sat);   /* the stage's colouring */

#endif
