#ifndef SPELL_H
#define SPELL_H
#include <genesis.h>

/* Willow's charged spells 7-12 (src/spell.c, specs/magic.md 4). */
extern u8 time_stop;                         /* FF83D2: frames left (enemies, surfaces, the clock frozen) */
void spell_init(void);
void spell_cast(u16 spell, s16 x, s16 y, u16 facing);
void spell_update(void);
u16 spell_test(s16 x, s16 y, const s16 *box, u16 hp);
u16 spell_bounds(s16 *u);                  /* union of what spell_test can hit: 0 none, 1 in u, 2 unbounded */   /* energy a spell takes from a target (0: none) */
bool spell_cuts(s16 x, s16 y, const s16 *box);           /* the blast covers an enemy projectile */
bool spell_flash(void);                      /* this frame the screen is white (spell 9) */
u16 spell_wave(void);                        /* time stop's wave phase (0: none) */
u16 spell_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y);

void spell_art(const u8 *poses, const u8 *tiles, const s16 *sat);   /* the stage's colouring */

#endif
