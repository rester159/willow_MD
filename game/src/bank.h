#ifndef BANK_H
#define BANK_H
#include <genesis.h>

/* Stage 6 and the ending live in banks 8-9 (the ROM from 0x400000), linked at
 * 0x300000 (slots 6-7): bank_stage6(TRUE) maps them in, FALSE maps the
 * stage 1-5 data back (src/bank.c, tools/md_willow.ld). */
void bank_stage6(bool on);
extern u8 bank_s6;              /* stage 6's banks are in */

#endif
