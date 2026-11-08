#ifndef SHOP_H
#define SHOP_H
#include <genesis.h>
#include "actor.h"

/* Shops (src/shop.c, specs/stage1_shops_companion.md 1). */
extern u8 shop_used;                    /* door bits used this stage */
extern u8 shop_safe;                    /* post-shop safety updates left (GM_PLAY counts them) */
void shop_new_game(void);               /* prices back to the new-game values, doors unused */
void shop_enter(u16 index);
bool shop_update(u16 held, u16 pressed);    /* GM_SHOP, once per update; TRUE: back to play */
u16 shop_draw(VDPSprite *sat, u16 first);
bool shop_screen(void);                 /* the shop screen shows (not the stage fading back in) */
void shopkeeper_update(Actor *a);

#endif
