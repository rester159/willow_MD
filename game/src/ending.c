/* The ending (specs/stage6.md 7): after Bavmorda, a scripted scene with no
 * input, timed as the arcade's run R (sub-step keys from its start, 7.1):
 * the chamber with the whirlwind; the staff roll (74 lines, 0.25 px an
 * update) on plane B beside six portraits on plane A; the castle sinking
 * into smoke as the sky turns to day; the closing texts; the closing card.
 * Then the arcade's end of the game: lives 1 -> the name entry, GAME OVER
 * without a continue offer (7.5); Home: NEW GAME PLUS unlocks on a clear
 * without a continue (AGENTS.md 2026-10-21).
 *
 * Native (7.6): the roll uses plane B (the chamber's far layer is dark),
 * shifted 4 columns right (the arcade's 384-px screen); the HUD is off for
 * the whole ending; the stage's interrupt handlers are replaced and set up
 * again by the next stage_init. Data: section .s6 (src/s6data.s). */
#include <genesis.h>
#include "actor.h"
#include "player.h"
#include "hud.h"
#include "sound.h"
#include "sprite.h"
#include "stagedata.h"
#include "packed.h"
#include "game.h"
#include "frontend.h"
#include "screens.h"
#include "roll_text.h"
#include "../data/ending/end.h"
#include "../data/ending/portraits.h"

extern const u16 end_por_pal[], end_sky_map[], end_castle_map[], end_card_map[], end_card_pal[];
extern const u16 *const end_por_map[];   /* per portrait: its packed map */
extern const u32 end_por_tiles[];
extern const u8 end_sky_tiles[], end_castle_tiles[], end_card_tiles[];
extern const u16 end_seq[];
extern u16 hud_glyph(char ch);
extern void stage_split(bool on);

#define SAT_VRAM 0xAC00
#define TILE0 16

static VDPSprite esat[80];
static u16 pal_now[64], pal_to[64];
static s16 fade_k, fade_d;          /* level 0..16 (x 4), step per update */
static u16 bvs;                     /* plane B's vertical scroll (the roll) */
static u16 t;                       /* updates since the ending began */

/* Fades by hand (non-blocking): pal_to scaled by fade_k / 64. */
static void fade_apply(void)
{
    u16 i, k;
    if (!fade_d) return;
    fade_k += fade_d;
    if (fade_k <= 0) { fade_k = 0; fade_d = 0; }
    if (fade_k >= 64) { fade_k = 64; fade_d = 0; }
    k = fade_k >> 2;
    for (i = 0; i < 64; i++)
    {
        u16 c = pal_to[i];
        pal_now[i] = ((((c >> 9) & 7) * k >> 4) << 9) | ((((c >> 5) & 7) * k >> 4) << 5) | ((((c >> 1) & 7) * k >> 4) << 1);
    }
    PAL_setColors(0, pal_now, 64, DMA_QUEUE);
}

static void fade_to(s16 dir, s16 speed) { fade_d = dir * speed; }

static void plane_clear(VDPPlane p)
{
    static const u16 z[64];
    u16 r;
    for (r = 0; r < 32; r++) VDP_setTileMapDataRow(p, z, r, 0, 64, CPU);
}

/* A text row on a plane: n characters of s at column x (glyphs of the HUD font). */
static void text_row(VDPPlane p, u16 row, u16 x, const char *s, u16 n)
{
    u16 w[40], i;
    for (i = 0; i < 40; i++) w[i] = 0;
    for (i = 0; i < n && s[i] && x + i < 40; i++) w[x + i] = s[i] == ' ' ? 0 : hud_glyph(s[i]);
    VDP_setTileMapDataRow(p, w, row, 0, 40, CPU);
}

static void picture(VDPPlane p, const u16 *map, u16 add)
{
    u16 w[40], r, i;
    map = map_open(map);                    /* aPLib-packed (src/packed.c) */
    for (r = 0; r < 28; r++)
    {
        for (i = 0; i < 40; i++) w[i] = map[r * 40 + i] ? map[r * 40 + i] + add : 0;
        VDP_setTileMapDataRow(p, w, r, 0, 40, CPU);
    }
    map_close();
}

/* One update: actors (the whirlwind, the smoke), sprites, fades, VBlank. */
static void frame(void)
{
    u16 n;
    actors_update();
    posecache_frame(0);
    n = actors_draw(esat, 0, 80, 32 + CAM_X, 272 - CAM_Y);
    if (!n) { esat[0].y = 0; esat[0].x = 0; n = 1; }
    esat[n - 1].link = 0;
    DMA_queueDma(DMA_VRAM, esat, SAT_VRAM, n * 4, 2);
    fade_apply();
    VDP_setVerticalScroll(BG_B, bvs);
    SYS_doVBlankProcess();
    t++;
}

/* ---- The roll (7.3): line L enters below the screen every 32 updates. */
static u16 roll_t, roll_line;
static void roll_step(void)
{
    if (roll_line >= 74 + 28) return;
    roll_t++;
    bvs = roll_t >> 2;
    if ((roll_t & 31) == 1)
    {
        u16 row = (roll_line + 28) & 31;
        if (roll_line < 74) text_row(BG_B, row, 4, roll_text[roll_line], 16);
        else text_row(BG_B, row, 0, "", 0);
        roll_line++;
    }
}

/* ---- The whirlwind (stage6_bavmorda.md 5): from where she fell; 64
 * updates still, then sweeps right and left, each cycle wider (max speed
 * +384, accel +4, rise +16); the first reversal starts the ending music and,
 * as in the arcade, leaves the speed at 256 (the sound call's d0). The end
 * of each cycle sets whirl_f6 = 1 (the staff roll starts); leaving the
 * screen on a left end, 2 (the chamber fades). v[0] max speed, v[1] accel,
 * v[2] rise step; vx speed. Chain 15 of Bavmorda's art (0x2AC04). */
static u8 whirl_f6, whirl_music;
void whirl6_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 2; a->timer = 64; a->v[0] = 384; a->v[1] = 4; a->v[2] = 16; a->phase = 2; actor_anim(a, 15); sound_post(0x95); return; }
    actor_step(a);
    if (a->phase == 2) { if (!--a->timer) { a->vx = 384; a->phase = 4; } return; }
    a->y += a->v[2];
    switch (a->phase)
    {
    case 4:
        a->vx -= a->v[1]; a->x += a->vx;
        if (a->vx < 0)
        {
            a->phase = 6;
            if (!whirl_music) { whirl_music = 1; sound_post(SND_MUSIC_ENDING); sound_post(0x01FA); a->vx = 256; }
        }
        return;
    case 6:
        a->vx += a->v[1]; a->x -= a->vx;
        if (a->vx >= a->v[0]) a->phase = 8;
        return;
    case 8:
        a->vx -= a->v[1]; a->x -= a->vx;
        if (a->vx < 0) a->phase = 10;
        return;
    case 10:
        if ((u16)(AX(a) - CAM_X + 32) >= 448) { whirl_f6 = 2; actor_free(a); return; }
        a->vx += a->v[1]; a->x += a->vx;
        if (a->vx >= a->v[0]) a->phase = 12;
        return;
    case 12:
        a->vx -= a->v[1]; a->x += a->vx;
        if (a->vx < 0) { a->v[0] += 384; a->v[1] += 4; a->v[2] += 16; whirl_f6 = 1; a->phase = 6; }
        return;
    }
}

/* ---- Smoke (7.1 sub 18): puffs billowing up from the castle's base. */
void smoke6_update(Actor *a)
{
    if (!a->state) { a->state = 2; a->active = 2; actor_anim(a, 0); return; }
    actor_step(a);
    a->y += 32;
}

static void type_text(VDPPlane p, u16 row, u16 x, const char *s, u16 wait_after)
{
    u16 n;
    for (n = 1; s[n - 1]; n++)
    {
        u16 k;
        text_row(p, row, x, s, n);
        for (k = 0; k < 3; k++) frame();
    }
    while (wait_after--) frame();
}

static void wait_until(u16 when) { while (t < when) frame(); }

void ending_run(void)
{
    u16 i, k;
    /* Sub 0-2 (keys 3672-3736): the chamber darkens and comes back. */
    t = 0;
    hud_blank_rows = 1;
    stage_split(FALSE);
    SYS_setVIntCallback(NULL);
    SYS_setHIntCallback(NULL);
    VDP_setHInterrupt(FALSE);
    PAL_getColors(0, pal_to, 64);
    memcpy(pal_now, pal_to, sizeof pal_now);
    fade_k = 64;
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);   /* whole-plane scrolls (the stage's tables are per row) */
    VDP_setWindowVPos(FALSE, 0);
    VDP_setWindowHPos(FALSE, 0);
    {
        static const u32 blank[8];
        VDP_loadTileData(blank, 0, 1, CPU);                /* tile 0: the empty cells */
    }
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setHorizontalScroll(BG_A, -(CAM_X + 32 - ((CAM_X + 32) & ~7)) - ((CAM_X + 32) & ~7));
    plane_clear(BG_B);
    bvs = 0;
    gate_freeze = 1;
    whirl_f6 = whirl_music = 0;
    {
        extern s16 bav_end_x, bav_end_y;
        Actor *w = actor_new(F_WHIRL6);
        if (w) { w->x = (s32)(bav_end_x ? bav_end_x : 192) << 8; w->y = (s32)(bav_end_y ? bav_end_y : 88) << 8; }
    }
    fade_to(-1, 2);
    wait_until(32);
    fade_to(1, 2);
    wait_until(64);
    /* Sub 4 (3736): the whirlwind; the ending music; the roll from its first reversal (4158). */
    {
        u16 t6 = 0;
        while (!t6 || t < t6 + 242)
        {
            if (whirl_f6 || t >= 600) roll_step();
            if (!t6 && (whirl_f6 == 2 || t >= 1400)) { t6 = t; fade_to(-1, 1); }   /* sub 6: the chamber fades out */
            frame();
        }
    }
    actor_clear();
    plane_clear(BG_A);
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setBackgroundColor(0);              /* CRAM 0: black behind the portraits and the card */
    /* Subs 8/10 (4984-7468): six portraits, 414 updates each, the roll beside them. */
    for (i = 0; i < 6; i++)
    {
        load_packed((const u8 *)end_por_tiles + end_por_tiles[i], TILE0, end_por_ntiles[i]);
        picture(BG_A, end_por_map[i], TILE0);
        memcpy(pal_to, end_por_pal + i * 64, sizeof pal_to);
        fade_k = 0; fade_to(1, 2);
        for (k = 0; k < 414; k++)
        {
            if (k == 352) fade_to(-1, 1);
            roll_step();
            frame();
        }
    }
    while (roll_line < 74 + 2) { roll_step(); frame(); }
    for (k = 0; k < 15; k++) { roll_step(); frame(); }
    /* Sub 14/16 (7484/7540): the castle at sunset, smoke; the sky turns to day. */
    fade_k = 0; fade_d = 0; PAL_setColors(0, palette_black, 64, CPU);
    plane_clear(BG_A); plane_clear(BG_B);
    bvs = 0;
    load_packed(end_sky_tiles, TILE0, END_SKY_TILES);
    load_packed(end_castle_tiles, TILE0 + END_SKY_TILES, END_CASTLE_TILES);
    picture(BG_B, end_sky_map, 0);
    {
        u16 w[40], r;
        { const u16 *cm = map_open(end_castle_map); for (r = 0; r < 32; r++) { for (k = 0; k < 40; k++) w[k] = cm[r * 40 + k]; VDP_setTileMapDataRow(BG_A, w, r, 0, 40, CPU); } map_close(); }
    }
    /* Lines: 0 the hills (scroll1 palette 17), 1 the smoke and text, 2 the
     * castle (scroll2 palette 14), 3 the sky (scroll3 palette 12). */
    memcpy(pal_to, end_seq + 2, 16 * 2);
    memcpy(pal_to + 16, sd->sprpal + 16, 16 * 2);
    memcpy(pal_to + 32, end_seq + 2 + 16, 32 * 2);
    fade_k = 0; fade_to(1, 4);
    t = 0;
    {
        const u16 *seq = end_seq + 1;
        u16 nseq = end_seq[0], si = 0, sink = 0, cleared = 0;
        s16 shake = 0;
        while (t < 400)
        {
            u16 rel = t >= 56 ? t - 56 : 0;            /* from sub 16 */
            if (t >= 56 && si + 1 < nseq && seq[(si + 1) * 49] <= rel)
            {
                si++;
                memcpy(pal_to, seq + si * 49 + 1, 32);
                memcpy(pal_to + 32, seq + si * 49 + 17, 64);
                fade_k = 63; fade_d = 1;           /* fade_apply once at the full level */
            }
            if (t >= 56 && t < 56 + 28 * 8 && !((t - 56) & 7))
            {
                Actor *s = actor_new(F_SMOKE6);
                u16 j = (t - 56) >> 3;
                if (s) { s->x = (s32)(CAM_X + 40 + ((j * 37) % 256)) << 8; s->y = (s32)(CAM_Y + 4 + (j & 3) * 6) << 8; }
            }
            if (t >= 138 && sink < 318)                  /* sub 18 (7622): 0.5 px an update, shaking */
            {
                sink++;
                if (!(t % 3)) shake ^= 1;
                VDP_setVerticalScroll(BG_A, -(sink >> 1));
                VDP_setHorizontalScroll(BG_A, shake);
                /* Rows gone below the screen are cleared before plane A's 256-px
                 * wrap brings them back at the top. */
                while (cleared < 32 && (31 - cleared) * 8 + (sink >> 1) >= 224)
                {
                    static const u16 z[40];
                    VDP_setTileMapDataRow(BG_A, z, 31 - cleared, 0, 40, CPU);
                    cleared++;
                }
            }
            frame();
        }
        while (t < 400 + 56) frame();
    }
    /* Subs 22-36 (7940-9178): the closing texts on plane A's top rows. */
    actor_clear();
    plane_clear(BG_A);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_A, 0);
    sound_post(0x86);
    type_text(BG_A, 2, 0, "WILLOW AND MADMARTIGAN FOUGHT BRAVELY TO", 0);
    type_text(BG_A, 3, 0, "DEFEAT THE EVIL QUEEN, BAVMORDA.", 200);
    text_row(BG_A, 2, 0, "", 0); text_row(BG_A, 3, 0, "", 0);
    sound_post(0x87);
    type_text(BG_A, 2, 0, "DARKNESS HAS GIVEN WAY TO A WORLD OF", 0);
    type_text(BG_A, 3, 0, "PEACE AND HAPPINESS.", 63);
    text_row(BG_A, 2, 0, "", 0); text_row(BG_A, 3, 0, "", 0);
    for (k = 0; k < 63; k++) frame();
    sound_post(0x88);
    type_text(BG_A, 2, 3, "A GREAT MAGICIAN.", 0);
    type_text(BG_A, 2, 21, "WILLOW UFGOOD", 0);
    type_text(BG_A, 4, 3, "A BRAVE WARRIOR.", 0);
    type_text(BG_A, 4, 21, "MADMARTIGAN", 351);
    text_row(BG_A, 2, 0, "", 0); text_row(BG_A, 4, 0, "", 0);
    sound_post(0x89);
    type_text(BG_A, 2, 3, "THEIR NAMES WILL LIVE ON FOREVER.", 256);
    /* Sub 38 (9534): the closing card, 127 updates; the music fades. */
    PAL_setColors(0, palette_black, 64, CPU);
    plane_clear(BG_A); plane_clear(BG_B);
    load_packed(end_card_tiles, TILE0, END_CARD_TILES);
    picture(BG_B, end_card_map, TILE0);
    memcpy(pal_to, end_card_pal, sizeof pal_to);
    fade_k = 0; fade_to(1, 8);
    for (k = 0; k < 127; k++) frame();
    sound_post(0x10FB);
    fade_to(-1, 2);
    for (k = 0; k < 32; k++) frame();
    gate_freeze = 0;
    hud_blank_rows = 0;
    /* 7.5: lives 1 -> 0: the name entry; GAME OVER, no continue. */
    if (fe.mode == FE_HOME && !fe.debug && !game_continues()) frontend_unlock_plus();
    sound_post(SND_STOP);
    scores_screens();
    screens_game_over();
    game_mode = GM_ENDED;
}
