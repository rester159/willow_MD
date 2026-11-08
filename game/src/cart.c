/* The cartridge's mapper and save RAM, for the flash carts the 5 MiB image
 * runs on (hardware report 2026-11-05: EverDrive MD v2.6 boots 06d with
 * the boss and backgrounds corrupt, Mega EverDrive PRO does not boot).
 *
 * The header says "SEGA SSF" (src/boot/rom_head.c): the EverDrives switch
 * their bank mapper on only with it. Two protocols answer to it:
 *  - standard SSF2 (the Sega mapper, older EverDrive-MD): byte registers
 *    0xA130F3..0xA130FF select the 512 KB bank of slots 1-7; 0xA130F1 is the
 *    save RAM switch (bit 0 on, bit 1 write protect; odd bytes from
 *    0x200001 over the ROM);
 *  - Krikzz's extended SSF (Mega EverDrive X3/X5/X7/PRO): 16-bit registers
 *    0xA130F0..0xA130FE, one per slot; 0xA130F0 also holds the unlock bit
 *    15 (every write sets it) and the save-write bit 13; save RAM is bank
 *    31, paged into a slot (from 0x240000 in slot 4: the X series' half,
 *    the PRO has all of it).
 * cart_detect tells them apart once at boot: with 0x8001 written to
 * 0xA130F0, extended SSF shows bank 1 in slot 0 (code running from RAM, the
 * interrupts and the Z80 held), standard SSF2 only switches save RAM on.
 * The boot code (src/boot/sega.s) has already unlocked it and set banks 1-7.
 * Save records use word accesses at the same offsets in both modes (the
 * low byte: standard SRAM sits on the odd bytes). */
#include <genesis.h>
#include "cart.h"

u8 cart_mode;               /* CART_STD or CART_EXT */
u8 cart_banks_ok;           /* banks 8-9 page in (cart_detect) */

#define REG(a) (*(vu16 *)(a))
#define REG8(a) (*(vu8 *)(a))

/* move.w #$8001,$A130F0 / move.w $000100,d0 / move.w #$8000,$A130F0 / rts */
static const u16 probe_code[] = { 0x33FC, 0x8001, 0x00A1, 0x30F0,
                                  0x3039, 0x0000, 0x0100,
                                  0x33FC, 0x8000, 0x00A1, 0x30F0,
                                  0x4E75 };
static u16 probe_ram[sizeof probe_code / 2];

static void slots67(u16 b6, u16 b7)
{
    /* Word writes, one sequence for both protocols: standard SSF2 takes the
     * low bytes (0xA130F1: save RAM off, 0xA130FD, 0xA130FF), extended SSF
     * the words (unlock, bank 0 in slot 0; slots 6 and 7). A wrong
     * cart_mode can then only cost the saves, not the banks. */
    REG(0xA130F0) = 0x8000;
    REG(0xA130FC) = b6;
    REG(0xA130FE) = b7;
}

void cart_detect(void)
{
    u16 ints = SYS_getAndSetInterruptMaskLevel(7), w;
    u32 b6, b8, back;
    bool z80 = Z80_getAndRequestBus(TRUE);
    memcpy(probe_ram, probe_code, sizeof probe_code);
    w = ((u16 (*)(void))probe_ram)();
    cart_mode = w == *(vu16 *)0x100 ? CART_STD : CART_EXT;   /* "SE" of "SEGA": slot 0 unchanged */
    /* Banks 8-9 page in: slot 6's first long changes from bank 6's to bank
     * 8's and back (build.sh checks the two differ in the image). */
    b6 = *(vu32 *)0x300000;
    slots67(8, 9);
    b8 = *(vu32 *)0x300000;
    slots67(6, 7);
    back = *(vu32 *)0x300000;
    cart_banks_ok = b8 != b6 && back == b6;
    if (!z80) Z80_releaseBus();
    SYS_setInterruptMaskLevel(ints);
}

void cart_slots67(u16 b6, u16 b7)
{
    u16 ints = SYS_getAndSetInterruptMaskLevel(7);
    slots67(b6, b7);
    SYS_setInterruptMaskLevel(ints);
}

/* ---- Save RAM: the interrupts masked and the Z80 held by the caller's
 * bracket (cart_sram_begin .. cart_sram_end); the code here and its caller
 * are below 0x200000 (the save RAM hides slot 4, or 0x200000-0x3FFFFF). */
static u16 sram_ints;
static bool sram_z80;

void cart_sram_begin(bool write)
{
    sram_ints = SYS_getAndSetInterruptMaskLevel(7);
    sram_z80 = Z80_getAndRequestBus(TRUE);
    if (cart_mode == CART_EXT)
    {
        REG(0xA130F0) = write ? 0xA000 : 0x8000;
        REG(0xA130F8) = 0x001F;             /* save RAM (bank 31) in slot 4 */
    }
    else REG8(0xA130F1) = write ? 1 : 3;
}

u8 cart_sram_read(u16 i)
{
    return cart_mode == CART_EXT ? (u8)REG(0x240000 + (u32)i * 2) : REG8(0x200001 + (u32)i * 2);
}

void cart_sram_write(u16 i, u8 v)
{
    if (cart_mode == CART_EXT) REG(0x240000 + (u32)i * 2) = v;
    else REG8(0x200001 + (u32)i * 2) = v;
}

void cart_sram_end(void)
{
    if (cart_mode == CART_EXT)
    {
        REG(0xA130F8) = 0x0004;             /* ROM bank 4 back in slot 4 */
        REG(0xA130F0) = 0x8000;             /* save writes off */
    }
    else REG8(0xA130F1) = 0;
    if (!sram_z80) Z80_releaseBus();
    SYS_setInterruptMaskLevel(sram_ints);
}
