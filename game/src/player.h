#ifndef PLAYER_H
#define PLAYER_H
#include <genesis.h>

/* World units as the arcade (specs/player.md): y up, 1/256 px. */
typedef struct
{
    s32 x, y;
    s16 vy, edge_fall;
    u16 action, timer, rising, crouch, air_attack;
    u16 facing;                  /* bit 0 left, bit 1 standing */
    u16 pose, pose_tick, pose_dirty;
    u16 last_held;
    s16 cam_x, cam_y, cam_min_x;
    u16 cam_vy;
    u16 pending;                 /* press latched for the next update */
    u16 phase, node_ticks, material;
    s16 slide_speed;
    u16 last_facing;             /* facing at the end of the previous update */
    u16 airborne;                /* 1 rising / climbing, 2 falling, 0 grounded (camera) */
    u16 cam_yf, cam_track;
    s16 cam_max_x;               /* camera stop (1952, then 3328 after the gate) */
    s16 cam_min_y, cam_max_y;    /* camera y limits (0, 2048; stage 4: 4096 .. 7936, raised as it goes) */
    u8 cam_catch;                /* catching up after a camera stop opened */
    u8 vuln;                     /* 1: can be hit; 2: not (combat.md 2.2) */
    u8 hurt_timer;               /* 128 after a hit (flashing, not hittable) */
    u8 hp_cache, armor_cache;    /* to notice a hit on the next update */
    u8 dead;                     /* 1 dying (animation), 2 removed: life lost */
    u8 dead_timer;
    s16 safe_x, safe_y;          /* last grounded position (checkpoint choice) */
    u8 form;                     /* 0 Willow, 1 Madmartigan (specs/madmartigan.md 0) */
    u8 head_hit;                 /* a ceiling stopped the rise (Madmartigan's climb aborts) */
    u8 script;                   /* scripted life state: 12 wagon leap, 16 chase-end walk (madmartigan.md 2.10) */
} Player;

extern Player player;

void player_init(s16 x, s16 y);
void player_form(u16 form);     /* 0 Willow, 1 Madmartigan: kept by player_init */
u16 player_pose_put(u16 form, u16 pose, VDPSprite *sat, u16 first, u16 base, s16 sx, s16 sy, u16 pal);   /* the select screen's figures */
u16 player_form_stand(u16 form);
void player_update(u16 held);
void player_stand(bool left);  /* scripted standing pose */
extern s16 player_script_x;         /* scripted walks (stage 4): the x to reach */
void player_script(u16 state); /* Madmartigan's scripted states 12 / 16 */
void player_road_death(void);   /* Madmartigan fallen onto the chase road (madmartigan.md 2.9) */
void player_hurt(void);         /* one point of damage (combat.md 2.2), if vulnerable */
const s16 *player_box(void);    /* hurt box of the current pose: xOff, w, yOff, h */
const s16 *player_box_of(const Player *p);   /* another record's (co-op) */
/* Writes the pose's sprites at sat[first..]; returns how many. */
u16 player_draw(VDPSprite *sat, u16 first, u16 base, s16 view_x, s16 view_y);
u16 player_draw_of(Player *p, VDPSprite *sat, u16 first, u16 base, s16 view_x, s16 view_y);   /* co-op */

#endif
