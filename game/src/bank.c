/* The cartridge's Sega (SSF2) mapper: eight 512 KB slots, slot n selected
 * by the byte at 0xA130F1 + 2n (slot 0 fixed). The ROM is 5 MiB: banks 0-7
 * as a plain 4 MB cartridge, banks 8-9 stage 6 and the ending (section .s6,
 * linked at 0x300000: src/s6data.s, s6spr.s). While they are in, nothing
 * from 0x300000 up but them may be used: the common data ends below it
 * (the linker checks; stages 2-5's data sits above, .stagedata). The header
 * Art pass: stage 1 maps them in too (its boss body is in .s6).
 * says "SEGA SSF" (hardware report 2026-11-05: the EverDrives page banks
 * only with it); src/cart.c speaks both mapper protocols. */
#include "bank.h"
#include "cart.h"

u8 bank_s6;

void bank_stage6(bool on)
{
    cart_slots67(on ? 8 : 6, on ? 9 : 7);       /* standard or extended SSF (src/cart.c) */
    bank_s6 = on;
}
