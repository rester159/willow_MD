#ifndef FRONTEND_H
#define FRONTEND_H
#include <genesis.h>

/* The port's front end and saves (src/frontend.c). */
enum { FE_HOME, FE_ARCADE };

/* Menu values as the B1 build's port_settings.h: difficulty 0..7 (shown
 * 1..8), lives 1..4, vitality 2..5, continue, Nando speed 0..3, magic
 * continue, demo sound, coin A/B 0..7. */
typedef struct { u8 difficulty, lives, vitality, cont, nando, magic, demo, coinA, coinB; } Settings;

typedef struct
{
    u8 mode;                /* FE_HOME / FE_ARCADE */
    u8 plus;                /* this run is NEW GAME PLUS */
    u8 start_held;          /* Start held since the launch: not a pause yet */
    u8 paused;
    u8 continued;           /* this run took a continue */
    u8 demo;                /* Arcade attract: a demo play runs (1 stage 1's arena, 2 stage 2's chase) */
    u8 debug;               /* this run came from the DEBUG page: no scores or unlocks saved */
    u8 debug_stage;         /* DEBUG: stage index 0..5 */
    u8 debug_cp;            /* DEBUG: checkpoint (stages 2-5) or area (stage 6) */
    u8 invincible;          /* DEBUG: no damage, pits bounce */
} FrontEnd;
extern FrontEnd fe;

void frontend_load(void);               /* settings and scores from save RAM (defaults if none) */
void frontend_save(void);
void frontend_menu(bool first);         /* blocking: until a game starts */
Settings *frontend_settings(void);      /* the current mode's */
extern u8 home_credits;                 /* Home: continues left this game (both players in co-op) */
u8 frontend_credits(void);              /* Home's CREDITS option (0..9, default 3) */
bool frontend_plus(void);               /* Home NEW GAME PLUS unlocked */
void frontend_unlock_plus(void);
const u8 *frontend_scores(u16 mode);    /* 10 x 8 bytes */
u16 frontend_rank(u32 bcd);             /* place 0..9 in the mode's table, 10: none */
void frontend_insert(u16 place, u32 bcd, const char *name);
u32 frontend_best(void);                /* the mode's top score (BCD) */

#endif
