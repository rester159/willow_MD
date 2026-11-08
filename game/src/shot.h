#ifndef SHOT_H
#define SHOT_H
#include <genesis.h>

void shot_init(u16 level);                    /* loads variants level..level+3 and the impact */
u16 shot_fire(s16 x, s16 y, u16 facing, u16 variant);  /* world px, y up; 0 if 4 alive */
u16 shot_spear(s16 x, s16 y, u16 facing);             /* the companion's spear; 0 if 4 in flight */
/* Madmartigan's sword (specs/madmartigan.md 5): one at a time, follows the
 * player; posture at spawn picks its offset; tier 0-3 from the charge. */
enum { SW_STAND, SW_CROUCH, SW_AIR };
extern u8 sword_level;                                /* 0..4: the shop's swords (5.4) */
u16 shot_sword(u16 facing, u16 posture, u16 tier);
bool shot_sword_cuts(s16 x, s16 y, const s16 *box);   /* an enemy projectile touching the live sword */
void shot_update(s16 cam_x, u16 frame);
u16 shot_test_full(s16 x, s16 y, const s16 *box, bool deflect);
/* shot_test's first rejection inline (no side effects): the box outside
 * the union of everything live as the last shot_update left it (empty when
 * nothing is live). su_ok 0: a shot, spear or swing started since. */
extern s16 shot_su_c[4];
extern u8 shot_su_ok;
static inline u16 shot_test(s16 x, s16 y, const s16 *b, bool deflect)
{
    if (shot_su_ok)
    {
        s16 ex = x - b[0], ey = y - b[2];
        if (ex > shot_su_c[1] || ex + b[1] < shot_su_c[0] || ey > shot_su_c[3] || ey + b[3] < shot_su_c[2]) return 0;
    }
    return shot_test_full(x, y, b, deflect);
}
extern u8 shot_hit_sword;                             /* what shot_test hit with: 0 a shot, 1 the sword, 2 a spear */
bool shot_bounds(s16 *u);
void shot_end_beyond(s16 x, const s16 *line);   /* shots past x + line[(y >> 5) & 7] end (stage 5's push wall) */                  /* union of what shot_test can hit (x lo, x hi, y lo, y hi) */   /* power of the shot that hit, 0 if none */
u16 shot_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y);
bool shot_near(s16 x, s16 y, s16 dx, s16 dy);
bool shot_sword_live(void);                           /* Madmartigan's swing is out (Sorsha's parry) */
void sword_art_set(const u8 *poses, const u8 *tiles, const s16 *sat);         /* a live shot within (dx, dy) of (x, y) */
void shot_art(const u8 *poses, const u8 *tiles, const s16 *sat);   /* the stage's colouring */

#endif
