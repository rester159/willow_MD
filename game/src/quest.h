#ifndef QUEST_H
#define QUEST_H
#include <genesis.h>

/* Sorsha's Quest (Home only, a custom campaign; AGENTS.md, out/audit/quest/
 * PLAN.md): six chapters on the arcade's stages, some played right to left
 * (reversed), her own world map and story pages (src/quest.c). */
extern u8 quest_ch;                 /* chapter 0..5 (fe.quest: a quest run) */
extern u8 qrev;                     /* the part being played runs right to left */

void quest_start(void);             /* a new quest from the Home menu (src/intro.c game_begin) */
void quest_map(bool page);          /* her world map (and the chapter's page) instead of the arcade's */
void quest_stage_setup(void);       /* game_next_stage: the part's area and start checkpoint */
void quest_place(void);             /* stage_start: reversed facing and camera limits */
void quest_update(void);            /* once per play update */
void quest_chapter_clear(void);     /* the clear's results are done: the next chapter */
void quest_end(void);               /* Bavmorda is beaten: the quest's ending page */
const char *quest_title(u16 line);  /* the chapter's title lines */
bool quest_family_off(u16 f);       /* placed objects the quest leaves out */

void quest_page(u16 n);             /* src/screens.c: story page n (0 prologue .. 7 ending), blocking */

#endif
