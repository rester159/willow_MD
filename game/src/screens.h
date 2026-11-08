#ifndef SCREENS_H
#define SCREENS_H
#include <genesis.h>

/* The arcade's screens between plays (src/screens.c, specs/arcade_frontend.md). */
extern u8 credits;                  /* Arcade credits, BCD 0..9 */
bool coin_update(u16 held);         /* each frame (Arcade): C counted on release; TRUE when a coin counted */
void credit_screen(void);           /* blocking until Start with a credit (one is taken) */
void ranking_screen(void);          /* the attract's ranking, 180 frames */
void scores_screens(void);          /* lives out: name entry + ranking when the score qualifies */
bool continue_screen(void);
void screens_game_over(void);      /* after the ending: GAME OVER, no music, no continue */         /* TRUE: continue (a credit taken in Arcade); FALSE: after GAME OVER */
enum { ATT_CREDIT, ATT_MENU, ATT_DEMO };
void attract_home(void);            /* Home idle: the attract's story pictures and the Home ranking, until a button */
u16 attract_loop(u16 from);         /* from 0: the opening, 1: after the demo; until a credit, A+B+C+Start (the port's menu) or the demo */
u16 coop_select_screen(void);       /* co-op (Home 2 PLAYERS): pad 1's hero, Willow (0) or Madmartigan (1) */
u16 select_screen(void);            /* stage 5: Willow (0) or Madmartigan (1) */
void attract_demo_title(u16 stage); /* the demo's end: its stage title, 180 frames */

#endif
