#ifndef COOP_H
#define COOP_H
#include <genesis.h>
#include "actor.h"

/* Two-player co-op (Home only; specs/coop.md, AGENTS.md "2-player co-op").
 * One player is Willow, the other Madmartigan, for the whole game. The
 * player in the globals (player, magic, the hud's per-player fields,
 * sword_level) is the lead; the other one's record is kept in coop.c and
 * swapped in while it updates, is hit or draws. two_players (actor.h) is 1
 * while both are in play; with it 0 nothing here runs and 1P is exact. */

enum { CO_NONE, CO_IN, CO_OUT };     /* the second player: absent, playing, out of lives */
extern u8 coop_state;
extern u8 coop_cur;                 /* 1 while the second player's record is in the globals */
extern u8 coop_lead_pad;            /* pad of the player in the globals (0 pad 1, 1 pad 2) */
extern u8 coop_owner;               /* 0 lead, 1 second: who scores / takes gold now */

void coop_reset(void);
void coop_both_down(void);
u16 coop_brake(void);               /* stage 4's sled: Left on the second player's pad */
void coop_throw_off(s16 x, s16 y);  /* stage 4's crash: the second player off the snowball */          /* time over: the second player loses a life too */              /* a new game: one player */
bool coop_join(void);               /* Start on the free pad (Home play): the other hero joins (FALSE: not now) */
void coop_frame(u16 joy_lead, u16 joy_second);  /* the second player's update, after the lead's */
bool coop_life_lost(void);          /* a player's life is lost: TRUE when co-op handled it (no restart) */
u16 coop_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y);
void coop_place(void);              /* after the lead's player_init: the second one beside it */
u16 coop_form(u16 form);            /* player_form's filter: the lead keeps its hero in co-op */
u8 coop_takes(s16 x, s16 y);        /* 1: the second player touches (x, y) (grown hurt box) */
void coop_gold(u16 variant, u8 who);
void coop_heart(u8 variant, u8 who);
bool coop_mid(s16 *x, s16 *y);      /* the camera's target: the players' midpoint */
u16 coop_pads(u16 *joy);            /* the second player's buttons; *joy becomes the lead's (Home: joins) */
bool coop_p2_dirty(void);           /* the second player's pose tiles go to VRAM this frame */
void coop_score(u32 bcd);           /* a score for the second player */

typedef struct { u32 score, gold; u8 hp, max_hp, armor, lives, form; const u8 *items; } HudSide;
void coop_side(u16 side, HudSide *h);   /* the HUD's values of side 0 (pad 1) or 1 (pad 2) */
u16 coop_side_now(void);            /* the side whose record is in the globals */
u8 coop_other_lives(void);          /* the other side's lives (BCD) */
void coop_actor_update(Actor *a, u16 cls);   /* an actor's update in co-op (actor.c) */
extern u8 coop_xy;                  /* 1 while an actor sees the second player's position */
void coop_hurt_second(void);        /* player_hurt for the second player (inside coop_xy) */
extern u8 coop_pick;                /* Home 2 PLAYERS: 0 none, 1 pad 1 is Willow, 2 pad 1 is Madmartigan */
u16 coop_pad2(void);                /* pad 2's buttons (read directly) */
const u16 *stage_sprpal(void);      /* the stage's sprite lines 0-1 (co-op: line 0 shared) */

#endif
