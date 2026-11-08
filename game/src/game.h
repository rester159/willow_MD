#ifndef GAME_H
#define GAME_H
#include <genesis.h>

/* Stage flow (src/game.c). */
enum { GM_PLAY, GM_TIME_OVER, GM_CONTINUE, GM_GAME_OVER, GM_CLEAR, GM_ENDED, GM_INTRO, GM_SHOP };
extern u8 game_mode;
extern u8 checkpoint;
extern u8 game_hide_sprites;

void game_new(void);                 /* a new game from stage 1's start */
void game_stage_reset(void);         /* back to stage 1's data and Willow (before a new game's stage set-up) */
void game_next_stage(void);          /* the next stage's start (stage 2) */
void stage_enter_next(void);         /* src/intro.c: world map, the next stage, its title */
extern u8 stage_index;
void game_update(u16 held);          /* once per update, after the player */
void game_open_gate(void);           /* the waterfall camera gate (mid-boss death) */
void game_stage_clear(void);
void game_play_start(void);          /* the intro is over: the stage-start gate, then play */
void game_stage_music(void);
void game_demo_setup(u16 demo);      /* the attract's demo play: 0 stage 1's arena, 1 stage 2's chase */          /* the stage's music word (by checkpoint in stage 3) */
void intro_world_map(u16 stage);     /* blocking: the world map, stage marker blinking, 128 frames */
void intro_start(void);              /* a new game: world map and forest intro (src/intro.c) */
void game_begin(void);               /* a game from the menu: the intro, or the DEBUG page's stage (src/intro.c) */
void intro_update(u16 held);
bool intro_hides_player(void);         /* the boss's stage-exit object ran out (FF83C3) */
/* The player runs this update; *joy may be replaced (the scripted exit walk). */
bool game_player_runs(u16 *joy);
extern u8 scene_lock;
u8 game_continues(void);              /* continues taken this run (Home NEW GAME PLUS needs none) */                /* FF83E9 / FF83CF: the clock stands */
bool boss_present(void);             /* once a frame: the body picture; TRUE while it is shown */
struct Actor;
bool boss_draw_adjust(const struct Actor *a, s16 *sx, s16 *sy);   /* stage 1 boss: head and paw drawn on the shown body pose */
void boss_reset(void);
void boss_new_game(void);

#endif
