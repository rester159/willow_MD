#ifndef HUD_H
#define HUD_H
#include <genesis.h>

/* The stage HUD (specs/hud.md, approved cut 2): the game's own values, the
 * arcade glyphs pre-coloured offline (tools/hudpack.py), Genesis layout.
 * Top two rows and the charge bar / item band on plane B (the static
 * backdrop plane), the reserve lives as sprites over the scene. */
typedef struct
{
    u32 score, score2, hi;   /* 8 BCD digits */
    u32 gold;                /* BCD, 6 digits shown */
    u8 hp, max_hp, armor;
    u8 lives;                /* BCD; shown as lives - 1 */
    u8 min, sec, div;        /* BCD minutes, seconds; frame divider 59..0 */
    u8 clock_lock;           /* non-zero: the clock stands (scene locks, spell 10) */
    u8 magic;                /* Willow's magic level 0..6 */
    u8 items[12];
} Hud;

extern Hud hud;

#define HUD_SCORE  0x01
#define HUD_HI     0x02
#define HUD_VITAL  0x04
#define HUD_GOLD   0x08
#define HUD_TIME   0x10
#define HUD_BAR    0x20
#define HUD_ITEMS  0x40
extern u8 hud_dirty;         /* elements to redraw at the next hud_update */

u16 hud_init(u16 vram_base);               /* tiles, layout, every element; returns the first free tile */
void hud_update(void);                     /* once per update: clock, redraws */
u16 hud_draw(VDPSprite *sat, u16 first);   /* the reserve-lives sprites */
void hud_charge(u16 h);
void hud_text(const char *text, u16 n, u16 x, u16 row);
u16 hud_glyph(char c);                     /* plane word of a character (0: none) */
void hud_stage(u16 stage);                 /* the stage's HUD colours (0, 1); before hud_init */
void hud_icons(u16 vram);                  /* the item icons' VRAM block (24 tiles), once at boot */    /* n characters from column x */
void hud_boss(s16 energy);
u16 hud_glyph_copy(u16 ht, u16 v);     /* a glyph tile to VRAM tile v; its line bits */
extern const u16 *hud_row_words;           /* hud_text over a picture (the story page) */
extern u16 hud_row_add;
extern u8 hud_blank_rows;                  /* hud_text on a blank screen (the story page) */                 /* boss energy bar on row 2; < 0 clears it */
void hud_layout(void);                     /* labels for one or two players, every element redrawn (co-op) */
#define HUD_TOP_LOW 1396                   /* VRAM of the top rows' sprites (src/vram.c; with the glyphs' own) */
#define HUD_TOP_LOWN 28
#define HUD_TOP_LOW2 1372
#define HUD_TOP_LOW2N 2
void hud_top_vram(void);
extern u8 hud_sat_ok;
void hud_vblank(void);
void hud_top_mode(bool plane);
void hud_top_mode_tm(bool plane, u8 tm);       /* tm: DMA (now) or DMA_QUEUE (the coming VBlank) */
void hud_2p_vram(u16 vram);                /* co-op: 30 tiles for the right half's bar colours */
void hud_message(const char *text, u16 row);  /* centred text on a plane B row; NULL clears it */                    /* charge meter step at level h */

#endif
