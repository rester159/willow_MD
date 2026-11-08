#ifndef DIRMATH_H
#define DIRMATH_H
/* 128-direction motion (0 up, 32 right, 64 down, 96 left; y up) for the
 * flying bosses and their projectiles (src/fam_boss3.c, src/fam_stage4.c). */
#include <genesis.h>

/* sin(d * 2pi / 128) x 256, d = 0..32. */
static const s16 sin_q[33] = { 0, 13, 25, 38, 50, 62, 74, 86, 98, 109, 121, 132, 142, 152, 162, 172, 181,
                               190, 198, 206, 213, 220, 226, 231, 237, 241, 245, 248, 251, 253, 255, 256, 256 };
static inline s16 sin128(u16 d)
{
    d &= 127;
    if (d <= 32) return sin_q[d];
    if (d <= 64) return sin_q[64 - d];
    if (d <= 96) return -sin_q[d - 64];
    return -sin_q[128 - d];
}

/* Velocity of 128-direction d (0 up, 32 right; y up) at speed (8.8). */
static inline void dir_v(u16 d, s16 speed, s16 *vx, s16 *vy)
{
    *vx = ((s32)speed * sin128(d)) >> 8;
    *vy = ((s32)speed * sin128(d + 32)) >> 8;
}

/* 128-direction of (dx, dy) (y up): octant, then atan of the smaller over the larger. */
static const u8 atan16[17] = { 0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 11, 12, 13, 14, 15, 16 };
static inline u16 angle128(s16 dx, s16 dy)
{
    s16 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    u16 a;
    if (!ax && !ay) return 0;
    if (ax <= ay) a = atan16[((u32)ax << 4) / ay];          /* from up toward the side */
    else a = 32 - atan16[((u32)ay << 4) / ax];
    if (dx >= 0 && dy >= 0) return a;                       /* up-right */
    if (dx >= 0) return 64 - a;                             /* down-right */
    if (dy < 0) return 64 + a;                              /* down-left */
    return (128 - a) & 127;                                 /* up-left */
}

#endif
