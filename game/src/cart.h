#ifndef CART_H
#define CART_H
#include <genesis.h>

/* The cartridge's bank mapper and save RAM (src/cart.c). */
#define CART_STD 0          /* standard SSF2 / Sega mapper (0xA130F1 save RAM switch) */
#define CART_EXT 1          /* Krikzz extended SSF (16-bit registers, save RAM = bank 31) */
extern u8 cart_mode;
extern u8 cart_banks_ok;    /* banks 8-9 page in */

void cart_detect(void);                 /* once, first thing at boot */
void cart_slots67(u16 b6, u16 b7);      /* banks of slots 6 and 7 (0x300000, 0x380000) */
void cart_sram_begin(bool write);       /* save RAM visible (interrupts and Z80 held) */
u8 cart_sram_read(u16 i);               /* record byte i */
void cart_sram_write(u16 i, u8 v);
void cart_sram_end(void);

#endif
