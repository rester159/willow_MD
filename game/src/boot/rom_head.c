#include "genesis.h"

/* "SEGA SSF" (hardware report 2026-11-05): the EverDrives enable their
 * bank mapper only with it (06d's "SEGA MEGA DRIVE" booted on an EverDrive
 * MD v2.6 without paging banks 8-9 and not at all on a Mega EverDrive PRO).
 * It selects standard SSF2 or Krikzz's extended SSF depending on the cart;
 * src/cart.c detects which and drives banks and save RAM for both. Save
 * RAM declared as the Sega mapper's (odd bytes 0x200001-0x20FFFF). */
__attribute__((externally_visible))
const ROMHeader rom_header = {
#ifdef HDR_MD
    "SEGA MEGA DRIVE ",                 /* test builds: the core's default Sega mapper (standard SSF2 path) */
#else
    "SEGA SSF        ",
#endif
    "(C)RESTER 2026  ",
    "WILLOW                                          ",
    "WILLOW                                          ",
    "GM 00000000-00",
    0x000,
    "J               ",
    0x00000000,
    0x004FFFFF,
    0xE0FF0000,
    0xE0FFFFFF,
    "RA",
    0xF820,
    0x00200001,
    0x0020FFFF,
    "            ",
    "NATIVE PORT                             ",
    "JUE             "
};
