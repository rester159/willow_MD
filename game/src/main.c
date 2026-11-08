/* Willow, Genesis-native engine (NATIVE_ENGINE_PLAN.md). M1: stage 1
 * opening with Willow under player control. */
#include <genesis.h>
#include "cart.h"
#include "stagedata.h"
#include "../data/stage1/layout.h"
#include "stage.h"
#include "bank.h"
#include "shop.h"
#include "coop.h"
#include "player.h"
#include "shot.h"
#include "sound.h"
#include "hud.h"
#include "magic.h"
#include "vram.h"
#include "game.h"
#include "actor.h"
#include "sprite.h"
#include "frontend.h"
#include "screens.h"
#include "spell.h"

#ifdef CAMTEST
/* The streaming test's camera path (measurement builds only: 31 KB). */
__asm__(".section .rodata\n.balign 2\n.global s1_camtrace, s1_camtrace_end\ns1_camtrace: .incbin \"data/stage1/camtrace.bin\"\ns1_camtrace_end:\n.text");
extern const s16 s1_camtrace[], s1_camtrace_end[];
#endif

#define SPR_BASE 1424       /* window tail, 0xB200: the player's current pose (43 tiles) */
#define HUD_BASE 1664       /* after plane B (0xD000; its 32 rows are the far layer's ring): HUD glyphs */
static u16 font_base;       /* debug font digits 0-9, A-F (DEBUG_ROW builds) */
#define SAT_VRAM 0xAC00
#ifndef EXP_NOTORCH
#define EXP_NOTORCH 0       /* measurement builds: no torch flames */
#endif
#ifndef DEBUG_ROW
#define DEBUG_ROW 0         /* 1: top row shows frames, missed VBlanks, x, y, pose, peak scanline */
#endif

static VDPSprite sat[80];
u16 work_end;                   /* measurement: the HV counter when the frame's work is done */

static void put_hex(u16 x, u16 v)
{
    for (s16 i = 3; i >= 0; i--, v >>= 4)
    {
        u16 d = v & 15;
        VDP_setTileMapXY(WINDOW, TILE_ATTR_FULL(PAL1, 1, 0, 0, font_base + d), x + i, 0);
    }
}

static volatile u16 vbl_count;
static u32 pace_vt;                 /* frame skipping: vtimer when play went on */
static u16 pace_n;                  /* updates run since */
u8 heavy;                    /* updates left in a busy stretch (fell behind lately) */
static u8 skip_run;                 /* presents skipped in a row */
#ifndef SKIP_MAX
#define SKIP_MAX 3                  /* after this many skips in a row, an update at most one late is drawn */
#endif
#ifndef FORCE_DRAW_2P
#define FORCE_DRAW_2P 1             /* co-op (Home 2P): drawn after SKIP_MAX skips however late (user, 2026-11-03: "Draw every 4th update"; brief slowdown at the heaviest spots) */
#endif
#ifndef EST_MARGIN
#define EST_MARGIN 8                /* lines added to a measured draw */
#endif
#ifndef EST_DECAY
#define EST_DECAY 2                 /* the estimate falls by 1/2^n of its excess a draw */
#endif
#ifndef LATE_LINE
#define LATE_LINE 240               /* a frame queued from VBlank line 224 to here may still go in it */
#endif
#ifndef LATE_RATE
#define LATE_RATE 150               /* bytes a VBlank line the late start counts on (measured ~170) */
#endif
#ifndef LATE_FIXED
#define LATE_FIXED 17               /* VBlank lines the work takes with nothing queued (measured ~14) */
#endif
#ifndef PRED_FROM
#define PRED_FROM 256               /* the deadline rule looks at decisions before this adjusted line (raw + 32: before the VBlank) */
#endif
#ifndef PRED_LINE
#define PRED_LINE 256               /* a drawing predicted to end past this adjusted line (raw 224, the VBlank; a few lines over: main's late start) is skipped */
#endif
u16 late_starts, late_tries, late_big, late_line;
#ifdef LATE_CHECK
u16 late_over, late_worst, vb_need, vb_lines, vb_q, vb_s;            /* late starts whose VBlank work ended in the picture; the latest line */
#endif   /* frames that went in a VBlank already begun; measurement */
static u16 decide_line, draw_est2[2] = { 96, 96 };   /* the line where drawing began; its measured length (+ margin): [1] after a skipped drawing (two updates' scrolling and pose loads) */
static u8 est_k;                    /* which estimate the drawing being made updates */
#define draw_est draw_est2[skip_run > 0]
u16 skipped_presents;               /* counted for measurements */
/* Update ahead: in a busy stretch (heavy), when the frame is drawn and
 * queued with time left before its VBlank for a whole update (upd_est lines,
 * measured), the next update runs then instead of waiting, and is drawn
 * right after the VBlank. Every update still runs once, in order, with the
 * pad as last read (as after a skipped drawing); the CPU's wait for the
 * VBlank is used. At most one update ahead. */
#if defined(AHEAD_ON) && !defined(CAMTEST) && !defined(NO_CATCHUP)   /* test builds only: measured mixed (round 2), and a frame of input delay */
#define AHEAD 1
#endif
#ifndef AHEAD_MARGIN
#define AHEAD_MARGIN 8              /* lines kept free before the VBlank */
#endif
static u8 ahead, early;            /* ahead: this update ran before the last VBlank; early: running it now */
static u16 upd_est = 160;          /* an update's length (lines): a decaying maximum */
u16 ahead_runs;                    /* counted for measurements */
u16 game_frame;             /* updates since boot */
#if defined(DBG_VRAM) || defined(DBG_FARCHK)
u16 dbg_tile[8], dbg_cram2[64];
#endif
u16 frame_line;             /* scanline when this frame's work ended (measurement; reads 16 while still in VBlank) */
u16 work_start, work_end;
#ifdef LOOPLOG
u16 looplog[32][3], drawlog[32];   /* measurement: per update the loop start, the line after the logic, vtimer | skipped */
#endif
u16 prof[8];
#ifdef SECS2
u16 profd[4];                  /* measurement builds: the drawing's parts */
#define PROFD(i) profd[i] = *(vu16 *)0xC00008
#else
#define PROFD(i)
#endif
u16 cal_vbl;
u16 idle_spin;              /* spin iterations waiting for VBlank after the work (measurement) */                /* VBlanks during the PROF_CAL loop (measurement) */                /* raw H/V counter at points of the frame (measurement) */   /* raw H/V counter at the start and end of the work (tools: frame cost in lines) */
/* Arcade: the attract loop until a credit (A+B+C+Start: the port's menu,
 * which may switch to Home), then the credit screen; or a demo play started
 * (fe.demo: the main loop runs it). from: 1 after a demo. */
static u8 demo_next;                    /* FF0E5A: the demos alternate */
static const u16 *demo_in;              /* the recorded input: (input, frames) pairs */
static u16 demo_left, demo_frames;
#include "demo_inputs.h"

static void arcade_front(u16 from)
{
    fe.demo = 0;
    while (fe.mode == FE_ARCADE)
    {
        u16 r;
        if (credits) { credit_screen(); return; }
        r = attract_loop(from);
        from = 0;
        if (r == ATT_MENU) frontend_menu(FALSE);
        else if (r == ATT_DEMO)
        {
            fe.demo = 1 + demo_next;
            game_demo_setup(demo_next);
            demo_in = demo_next ? demo1 : demo0;
            demo_left = demo_in[1];
            demo_frames = 900;
            demo_next ^= 1;
            return;
        }
    }
}

/* The demo's pad: the recorded input (bit 0 right .. bit 5 jump). */
static u16 demo_pad(void)
{
    static const u16 bits[6] = { BUTTON_RIGHT, BUTTON_LEFT, BUTTON_DOWN, BUTTON_UP, BUTTON_A, BUTTON_B };
    u16 in = demo_in[0], j = 0, i;
    for (i = 0; i < 6; i++) if (in & (1 << i)) j |= bits[i];
    if (demo_left && !--demo_left && demo_in[3]) { demo_in += 2; demo_left = demo_in[1]; }
    return j;
}

#ifdef VBLTIME
u32 vbltime, vblcalls;           /* measurement builds: the VBlank callback's time (1/256 line) */
static void vbl(void)
{
    u16 h0 = *(vu16 *)0xC00008, h1;
    s32 t;
    stage_vblank(); hud_vblank(); vbl_count++;
    h1 = *(vu16 *)0xC00008;
    t = (s32)(((h1 >> 8) - (h0 >> 8) + 262) % 262) * 256 + (s16)((h1 & 0xFF) - (h0 & 0xFF));
    if (t < 0) t += 256;
    if (t < 120 * 256) { vbltime += t; vblcalls++; }
}
#else
#ifdef HUDLINE
u16 hl_max_spr, hl_max_px, hl_over_lines, hl_over_frames, hl_lost_px, hl_frames, hl_hud_px, hl_hud_spr, hl_total;
#endif
#ifdef VBSTEPS
u16 vb_t1, vb_t2, vb_t3;             /* measurement: raw V counter at the callback's start and end, after the process */
static void vbl(void) { vb_t1 = *(vu16 *)0xC00008 >> 8; stage_vblank(); hud_vblank(); vbl_count++; vb_t2 = *(vu16 *)0xC00008 >> 8; }
#else
static void vbl(void) { stage_vblank(); hud_vblank(); vbl_count++; }
#endif
#endif

int main(bool hard)
{
    u16 frames = 0, missed = 0, last, n, peak_line = 0;
    s16 vx, vy;
    (void)hard;

#ifdef PSGTEST
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 440);
    for (;;) SYS_doVBlankProcess();
#endif
#ifdef SNDTEST
    /* Sound-only test: the driver and the stage 1 music, nothing else. */
    sound_init();
    {
        extern u16 sound_upload_check(u16 len);
        extern u16 sound_dbg[4];
        u16 bad = sound_upload_check(0x1000);
        sound_post(SND_MUSIC_STAGE1);
        for (;;) { SYS_doVBlankProcess(); sound_debug(); sound_dbg[0] = bad; }
    }
#endif
    cart_detect();                              /* the mapper's protocol and banks 8-9 (src/cart.c) */
    if (!cart_banks_ok)
    {
        /* The 5 MiB image needs the SSF2 bank mapper (stage 1's boss, stage
         * 6, the ending): say so instead of showing corrupt art later. */
        u16 t;
        VDP_drawText("THIS CARTRIDGE DOES NOT PAGE", 6, 11);
        VDP_drawText("ROM BANKS 8-9 (SSF MAPPER).", 6, 12);
        VDP_drawText("STAGE 1 BOSS AND STAGE 6 WILL", 6, 14);
        VDP_drawText("NOT SHOW CORRECTLY.", 6, 15);
        for (t = 0; t < 300; t++) SYS_doVBlankProcess();
        VDP_clearPlane(BG_A, TRUE);
    }
    bank_stage6(FALSE);                         /* a soft reset may leave the mapper as it was */
    VDP_setScreenWidth320();
#if !defined(NO_INTRO) && !defined(START_CP) && !defined(CAMTEST)
    VDP_setEnable(FALSE);                       /* dark until the menu is drawn (no stage set-up shows) */
#endif
    frontend_load();                            /* settings and high scores (save RAM) */
    JOY_setSupport(PORT_2, JOY_SUPPORT_OFF);    /* one player: pad 2 is not read every VBlank */
    DMA_initEx(256, 0, DMA_BUFFER_SIZE_NTSC);   /* 256 entries: a restart queues two full windows of columns on both planes */
    player_init(192, 72);                         /* stage 1 checkpoint 0 */
    vx = player.cam_x + 32;
    vy = 272 - player.cam_y;
    stage_init(vx, vy);
    PAL_setColors(0, sd->sprpal, 32, DMA);          /* sprite lines 0-1 (tools/sprpal.py) */
    hud.hi = 0x55000;                             /* ranking table default */
    hud.hp = hud.max_hp = 3;                      /* arcade start values (trace) */
    hud.lives = 2;
    hud.min = 3;
#ifdef HUDTEST
    /* Every HUD element away from its start value. */
    hud.score = 0x12345678; hud.score2 = 0x700; hud.gold = 0x123456;
    hud.hp = 2; hud.max_hp = 5; hud.armor = 3; hud.lives = 0x10; hud.sec = 0x12; hud.min = 0;
    hud.items[10] = hud.items[8] = hud.items[9] = hud.items[7] = 1; hud.items[3] = 2; hud.items[11] = 3;
    hud.magic = 6;
#endif
    vram_init(hud_init(HUD_BASE), DEBUG_ROW);
    hud_icons(vram_alloc(32));
    hud_top_vram();
    shot_init(magic.level);
    magic_init();
    if (DEBUG_ROW)
    {
        font_base = vram_alloc(16);
        VDP_loadTileData(font_default.tiles + 16 * 8, font_base, 10, DMA);
        VDP_loadTileData(font_default.tiles + 33 * 8, font_base + 10, 6, DMA);
        PAL_setColor(31, RGB24_TO_VDPCOLOR(0xFFFFFF));
        VDP_setWindowVPos(FALSE, 1);
    }
    families_init();
    posecache_init();
    SYS_setVBlankCallback(vbl);
#if !defined(CAMTEST) && !defined(NOSOUND)
    sound_init();
#endif
#if defined(NO_INTRO) || defined(START_CP)
    /* Test builds (traces, measurements): straight into play, Arcade's
     * settings (the traces' DIP values). */
    fe.mode = FE_ARCADE;
    game_new();
#ifdef COOP_TEST
    fe.mode = FE_HOME;                  /* test builds: Home, so pad 2 can join */
    coop_reset();
    home_credits = frontend_credits();
#endif
#ifdef S5_ENTER
    /* Test builds: the hand-over into stage 5 (world map, select, title, play). */
    game_next_stage();
    game_next_stage();
    game_next_stage();
    stage_enter_next();
#elif defined(S6_ENTER)
    /* Test builds: the hand-over into stage 6 (world map, title, play). */
    game_next_stage();
    game_next_stage();
    game_next_stage();
    game_next_stage();
    stage_enter_next();
#elif defined(STAGE6_VIEW)
    /* Test builds: stage 6 from area S6_AREA's start (default 0). */
    game_next_stage();
    game_next_stage();
    game_next_stage();
    game_next_stage();
    game_next_stage();
    hud_init(HUD_BASE);
    PAL_setColors(0, sd->sprpal, 32, DMA);
#ifdef ENDING_TEST
    { extern void ending_run(void); SYS_doVBlankProcess(); ending_run(); }   /* test builds: the ending at once (S6_AREA=6) */
#endif
#elif defined(STAGE5_VIEW)
    /* Test builds: stage 5 from its start (or checkpoint S5_CP), as S5_FORM. */
    {
        extern u8 stage5_form;
#ifndef S5_FORM
#define S5_FORM 0
#endif
        stage5_form = S5_FORM;
    }
    game_next_stage();
    game_next_stage();
    game_next_stage();
    game_next_stage();
    hud_init(HUD_BASE);
    PAL_setColors(0, sd->sprpal, 32, DMA);
#elif defined(S4_ENTER)
    /* Test builds: the hand-over into stage 4 (world map, title, play). */
    game_next_stage();
    game_next_stage();
    stage_enter_next();
#elif defined(STAGE4_VIEW)
    /* Test builds: stage 4 from its start (or checkpoint S4_CP). */
    game_next_stage();
    game_next_stage();
    game_next_stage();
    hud_init(HUD_BASE);
    PAL_setColors(0, sd->sprpal, 32, DMA);
#elif defined(S3_ENTER)
    /* Test builds: the hand-over into stage 3 (world map, title, play). */
    game_next_stage();
    stage_enter_next();
#elif defined(STAGE3_VIEW)
    /* Test builds: stage 3 from its start (or checkpoint S3_CP). */
    game_next_stage();
    game_next_stage();
    hud_init(HUD_BASE);
    PAL_setColors(0, sd->sprpal, 32, DMA);
#elif defined(STAGE2_VIEW)
    /* Test builds: stage 2 from its start (or checkpoint S2_CP). */
    game_next_stage();
    hud_init(HUD_BASE);
    PAL_setColors(0, sd->sprpal, 32, DMA);
#endif
#ifndef NOSOUND
    game_stage_music();
#endif
#elif !defined(CAMTEST)
    frontend_menu(TRUE);                        /* Home / Arcade (src/frontend.c) */
    if (fe.mode == FE_ARCADE) arcade_front(0);  /* the attract loop until a coin (C), then Start */
    if (!fe.demo) game_begin();
#endif
    last = vbl_count;
    for (;;)
    {
        work_start = *(vu16 *)0xC00008;
#ifdef AHEAD
        if (ahead)
        {
            /* Ran before the VBlank: drawn now, on time. */
            ahead = 0;
            decide_line = VDP_getAdjustedVCounter();
            est_k = 0;
            goto draw_now;
        }
    run_update:
#endif
#ifdef CAMTEST
        /* Streaming test: the view follows the arcade's logged camera path. */
        static const s16 *cam = s1_camtrace;
        if (cam + 2 < s1_camtrace_end) cam += 2;
        vx = stage_scroll(cam[0], cam[1]);
        vy = stage_view_y;
#else
        {
            u16 joy = JOY_readJoypad(JOY_1), joy2 = 0;
#ifdef UPD_INPUT
            if (game_mode == GM_PLAY) joy = BUTTON_RIGHT | ((game_frame % 37) < 2 ? BUTTON_A : 0) | ((game_frame % 151) < 3 ? BUTTON_B : 0);   /* test builds: input by update */
#endif
#ifdef PROF_CAL
            {
                u16 i;
                static u16 done;
                prof[6] = *(vu16 *)0xC00008;
                for (i = 0; i < 1000; i++) __asm__ volatile ("nop" ::: "memory");
                prof[7] = *(vu16 *)0xC00008;
                if (frames == 150 && !done)        /* 100 x 18000 cycles = 1.8 M: ~14 frames at 7.67 MHz */
                {
                    u16 k, v0 = vbl_count;
                    done = 1;
                    for (k = 0; k < 100; k++)
                        for (i = 0; i < 1000; i++) __asm__ volatile ("nop" ::: "memory");
                    cal_vbl = vbl_count - v0;
                }
            }
#endif
            prof[0] = *(vu16 *)0xC00008;
            joy2 = coop_pads(&joy);         /* co-op (Home): pad 2 joins; the lead's and the second's buttons */
#ifndef NO_INTRO
            if (game_mode == GM_ENDED)
            {
                /* The run is over (the scores and continue screens ran at the
                 * last life loss): Home goes back to its menu; Arcade to the
                 * credit screen with credits left, else to its menu page (in
                 * place of the attract loop, not yet built). */
                game_hide_sprites = 0; hud_blank_rows = 0; hud_row_words = NULL;
                fe.paused = 0;
                if (fe.mode == FE_HOME) frontend_menu(FALSE);
                if (fe.mode == FE_ARCADE) arcade_front(0);
                if (!fe.demo) game_begin();
            }
            coin_update(joy);                   /* Arcade: C inserts a coin at any time */
            if (fe.demo)
            {
                /* The attract's demo (arcade_frontend.md 3.2): the recorded
                 * input for 900 frames, or until a life is lost; a coin or
                 * A+B+C+Start leaves it. */
                bool chord = (joy & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START)) == (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START);
                if (credits || chord)
                {
                    PAL_fadeOutAll(16, FALSE);
                    sound_post(SND_STOP);
                    fe.demo = 0;
                    game_hide_sprites = 0;
                    if (chord) frontend_menu(FALSE);
                    if (fe.mode == FE_ARCADE) arcade_front(0);
                    if (!fe.demo) game_begin();
                    joy = 0;
                }
                else if (game_mode == GM_PLAY && (!--demo_frames || player.dead))
                {
                    /* The end (0x8E14): the title over the frozen play, then the attract goes on. */
                    attract_demo_title(fe.demo - 1);
                    PAL_fadeOutAll(16, FALSE);
                    game_hide_sprites = 0;
                    arcade_front(1);
                    if (!fe.demo) game_begin();
                    joy = 0;
                }
                else joy = demo_pad();
            }
            if (fe.mode == FE_HOME && game_mode == GM_PLAY)
            {
                /* Home: Start pauses and resumes (not the press that launched
                 * the game while it is still held); in co-op either pad's. */
                static u16 last_start;
                u16 st = (joy | joy2) & BUTTON_START;
                if (!st) fe.start_held = 0;
                else if (!last_start && !fe.start_held)
                {
                    fe.paused ^= 1;
                    hud_message(fe.paused ? "PAUSE" : NULL, 12);
                    if (fe.paused) XGM2_pause(); else XGM2_resume();
                }
                last_start = st;
            }
#endif
            if (game_mode == GM_ENDED)
            {
                /* The stage is over (stage 2 is not in this build): start again. */
                game_hide_sprites = 0;
                hud_blank_rows = 0;
                game_stage_reset();
                player_init(192, 72);
                stage_init(player.cam_x + 32, 272 - player.cam_y);
                hud_init(HUD_BASE);
                game_new();
                hud_update();
                PAL_setColors(0, stage_sprpal(), 32, DMA);
#ifndef NOSOUND
                sound_post(SND_MUSIC_STAGE1);
#endif
            }
            if (!fe.paused && game_player_runs(&joy))
            {
                player_update(joy);
                prof[1] = *(vu16 *)0xC00008;
                magic_meter();
                shot_update(player.cam_x, frames);
                if (!two_players || !player.form) { spell_update(); magic_update(); }   /* co-op: in Willow's record */
                if (two_players) coop_frame(joy, joy2);
            }
            prof[2] = *(vu16 *)0xC00008;
            if (!fe.paused) game_update(joy);
            prof[3] = *(vu16 *)0xC00008;
        }
#ifndef HUDOFF
        if (game_mode != GM_INTRO) hud_update();
#endif
#ifndef NO_CATCHUP
        /* Whole-display frame skipping (AGENTS.md, the second performance
         * fallback). Update n is due by the n-th VBlank since play went on
         * (vtimer). An update already late, or on time but whose drawing
         * (draw_est lines, measured) would end after the coming VBlank, is
         * not drawn: the next update runs at once and is drawn for its own
         * VBlank (the latter only within 2 s of falling behind, so light
         * scenes keep every update on its own frame). Every update still runs (input, simulation, clock); only
         * drawing is skipped, and the game keeps its speed. More than 3 late
         * (a blocking screen ran, or a long stall) is forgiven. After SKIP_MAX
         * skips in a row an update at most one late is drawn anyway, so a
         * scene at the edge of the budget is not left undrawn for long. */
        if (game_mode == GM_PLAY && !fe.paused)
        {
            extern vu32 vtimer;
            s16 late = (s16)((u16)(vtimer - pace_vt) - pace_n);
            u16 line = VDP_getAdjustedVCounter();
            if (late > 3) { pace_vt = vtimer; pace_n = 0; late = 0; }
            pace_n++;
            /* Fell behind: the deadline rule, 2 s; one frame late (a burst in a
             * light scene), half a second. */
            if (late > 1) heavy = 120;
            else if (late > 0 && heavy < 30) heavy = 30;
            else if (heavy) heavy--;
            decide_line = line;
#ifdef LOOPLOG
            { u16 *r = looplog[game_frame & 31]; r[0] = work_start; r[1] = *(vu16 *)0xC00008; r[2] = vtimer | (late > 0 || (heavy && late == 0 && line < PRED_FROM && line + draw_est > PRED_LINE) ? 0x8000 : 0); }
#endif
            if ((late > 0 || (heavy && late == 0 && line < PRED_FROM && line + draw_est > PRED_LINE)) && !(skip_run >= SKIP_MAX && (late <= 1 || (FORCE_DRAW_2P && two_players))))
            {
                skip_run++;
                skipped_presents++;
                frames++;
                game_frame++;
                continue;
            }
            est_k = skip_run > 0;
            skip_run = 0;
        }
        else
        {
            extern vu32 vtimer;
            pace_vt = vtimer;
            pace_n = 0;
        }
#ifdef AHEAD
        {
            /* The update's length (raw V counter; the VBlank's jump ignored). */
            u16 len = ((*(vu16 *)0xC00008 >> 8) - (work_start >> 8)) & 0xFF;
            if (len > upd_est) upd_est = len;
            else upd_est -= (upd_est - len) >> 3;
        }
        if (early) { early = 0; ahead = 1; goto vblank_now; }
#endif
#endif
#ifdef AHEAD
    draw_now:
#endif
        {
            /* Stage 2's chase: the planes follow the virtual camera, the
             * sprites the actors' strip (stage2.md 2.4). */
            extern s32 chase_vx;
            vx = stage_scroll(chase_vx ? (s16)(chase_vx >> 8) + 32 : player.cam_x + 32, 272 - player.cam_y - swell_y);
            if (chase_vx) vx = player.cam_x + 32;
        }
        vy = stage_view_y;
#endif
#ifdef HUDOFF
        n = 0;
#else
        if (game_mode == GM_SHOP && shop_screen()) { n = shop_draw(sat, 0); hud_sat_ok = 0; }
        else if (game_mode != GM_INTRO) { if (!game_hide_sprites) stage_cycle(); n = hud_draw(sat, 0); }   /* not on the story page */
        else
        {
            extern u16 intro_text_draw(VDPSprite *sat, u16 first);
            extern bool intro_hud(void);
            hud_sat_ok = 0;
            n = intro_text_draw(sat, 0);            /* the story text and titles (sprites) */
            if (intro_hud()) n += hud_draw(sat, n); /* a later stage's title: the HUD's sprites too (as the arcade) */
        }
#endif
        prof[4] = *(vu16 *)0xC00008;
#ifdef DRAWSECT
        __asm__ volatile ("move.w #0x2700,%sr");   /* measurement builds: the drawing timed without interrupts */
#endif
        { extern u16 stage_vq_words; posecache_frame(stage_queued + (player.pose_dirty ? 36 : 0) + (coop_p2_dirty() ? 43 : 0) + (stage_vq_words >> 4)); }   /* the VBlank's other transfers (tiles) */
        if (game_mode == GM_SHOP && shop_screen()) goto sat_done;
#ifdef TEXTONLY
        if (game_mode == GM_INTRO) goto sat_done;
#endif
        n += magic_draw(sat, n, vx, vy);
        if (player.dead != 2 && !intro_hides_player() && !(two_players && (player.hurt_timer & 4)))   /* co-op: hurt blinks (line 0 shared) */
            n += player_draw(sat, n, SPR_BASE, vx, vy);
        if (two_players) n += coop_draw(sat, n, vx, vy);
        n += shot_draw(sat, n, vx, vy);
        n += spell_draw(sat, n, vx, vy);
        PROFD(0);
        if (stage_index == 3) { extern u16 spray_draw(VDPSprite *, u16, s16, s16); n += flakes_draw(sat, n, vx, vy); n += spray_draw(sat, n, vx, vy); }   /* stage 4's falling snow, the sled's spray */
        if (stage_index == 4) { extern u16 boss5_draw(VDPSprite *sat, u16 first, u16 max); n += boss5_draw(sat, n, 80 - n); }   /* Eborsisk's bars */
#ifndef EXP_NOCHAINS
        if (stage_index == 5) { extern u16 chains6_draw(VDPSprite *sat, u16 first, u16 max, s16 view_x, s16 view_y); n += chains6_draw(sat, n, 80, vx, vy); }   /* the crushers' chains */
#endif
#ifndef EXP_NOROOSTDRAW
        if (stage_index == 4) { extern u16 roost_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y); n += roost_draw(sat, n, vx, vy); }   /* the hanging bats */
#endif
        PROFD(1);
#ifndef PROF_NODRAW
        n += actors_draw(sat, n, 80, vx, vy);
#endif
        PROFD(2);
        if (stage_index == 5 && !EXP_NOTORCH) { extern u16 torches6_draw(VDPSprite *sat, u16 first, u16 max, s16 view_x, s16 view_y); n += torches6_draw(sat, n, 80, vx, vy); }   /* the torches' flames, under every other sprite */
        if (boss_present() || boss5_layer)
        {
            /* The boss body's cells carry the priority bit: every sprite
             * goes over them. */
            u16 i;
            for (i = 0; i < n; i++) sat[i].attribut |= 0x8000;
        }
        if (game_hide_sprites) n = 0;
        {
            /* Spell 9: the screen white for 3 frames (the arcade's solid
             * white text layer), then the colours back. */
            static u16 saved[64];
            static u8 white;
            if (spell_flash())
            {
                static const u16 w64[64] = { [0 ... 63] = 0x0EEE };
                if (!white) PAL_getColors(0, saved, 64);
                white = 1;
                PAL_setColors(0, w64, 64, DMA_QUEUE);
            }
            else if (white) { white = 0; PAL_setColors(0, saved, 64, DMA_QUEUE); }
        }
#ifdef EXP_SATCAP
        if (n > EXP_SATCAP) n = EXP_SATCAP;     /* measurement builds: a shorter table */
#endif
#ifdef EXP_SPRITESOFF
        { u16 i; for (i = 0; i < n; i++) sat[i].y = 16; }      /* measurement builds: every sprite above the screen */
#endif
    sat_done:
        if (!n) { sat[0].y = 0; sat[0].x = 0; n = 1; hud_sat_ok = 0; }
        sat[n - 1].link = 0;
#ifdef HUDLINE
        {
            /* Measurement: the top 16 lines' sprites and pixels; what falls
             * past the limits (20 sprites, 320 pixels a line). */
            extern u16 hl_max_spr, hl_max_px, hl_over_lines, hl_over_frames, hl_lost_px, hl_frames, hl_hud_px, hl_hud_spr, hl_total;
            u16 l, i, over = 0, hudn = 0;
            hl_frames++;
            if (n > hl_total) hl_total = n;
            if (n >= 80) hl_hud_spr++;
            for (l = 0; l < 16; l++)
            {
                u16 c = 0, px = 0;
                for (i = 0; i < n; i++)
                {
                    s16 y = (s16)sat[i].y - 128, h = ((sat[i].size_link >> 8) & 3) * 8 + 8, w = ((sat[i].size_link >> 10) & 3) * 8 + 8;
                    if (l >= y && l < y + h && sat[i].x) { c++; px += w; }
                }
                if (c > hl_max_spr) hl_max_spr = c;
                if (px > hl_max_px) hl_max_px = px;
                if (c > 20 || px > 320) { over = 1; hl_over_lines++; if (px > 320) hl_lost_px += px - 320; }
            }
            if (over) hl_over_frames++;
            (void)hudn;
        }
#endif
        prof[5] = *(vu16 *)0xC00008;
#ifdef DRAWSECT
        __asm__ volatile ("move.w #0x2000,%sr");
#endif
        DMA_queueDma(DMA_VRAM, sat, SAT_VRAM, n * 4, 2);
        {
            u16 line = VDP_getAdjustedVCounter();
            frame_line = line;
            if (game_mode == GM_PLAY && !fe.paused)
            {
                /* Drawing's length (lines, + 8): a decaying maximum. */
                u16 len = (line >= decide_line ? line - decide_line : line + 262 - decide_line) + EST_MARGIN;
                if (len > 200) len = 200;
#ifdef LOOPLOG
                { extern u16 drawlog[32]; drawlog[game_frame & 31] = len; }
#endif
                u16 *e = draw_est2 + est_k;
                if (len > *e) *e += (len - *e + 1) >> 1;   /* half way: one burst of pose loads must not hold the deadline rule for 20 frames */
                else *e -= (*e - len) >> EST_DECAY;
            }
            work_end = *(vu16 *)0xC00008;
            if (line < 240 && line > peak_line) peak_line = line;
        }
        if (DEBUG_ROW)
        {
            put_hex(0, frames);
            put_hex(5, missed);
            put_hex(10, player.x >> 8);
            put_hex(15, player.y >> 8);
            put_hex(20, player.pose);
            put_hex(25, peak_line);
        }
#ifdef IDLE_MEASURE
        {
            /* Idle time left this frame: spin until the VBlank interrupt
             * (measurement builds only, tools/frametime.sh: the spin ends
             * after the VBlank started, so SYS_doVBlankProcess then waits
             * for the next one and every frame takes two). */
            extern vu32 vtimer;                 /* SGDK: +1 in the VBlank interrupt */
            u32 t = vtimer;
            u16 spin = 0;
            while (vtimer == t) spin++;
            idle_spin = spin;
        }
#endif
#ifdef DBG_FARCHK
        {
            /* Debug: far-layer tiles in VRAM that differ from ROM: count, first. */
            extern u16 dbg_tile[8];
            u16 t, k, bad = 0, first = 0;
            for (t = 0; t < STAGE_BD_TILES; t++)
            {
                const u16 *src = (const u16 *)(sd->bd_tiles + (u32)t * 32);
                u16 a = (STAGE_BD_BASE + t) * 32;
                *(vu32 *)VDP_CTRL_PORT = ((u32)(a & 0x3FFF) << 16) | (a >> 14);
                for (k = 0; k < 16; k++) if (*(vu16 *)VDP_DATA_PORT != src[k]) { if (!bad++) first = t; break; }
            }
            dbg_tile[0] = bad; dbg_tile[1] = first;
            {
                extern u16 dbg_cram2[64];
                PAL_getColors(0, dbg_cram2, 64);
            }
        }
#endif
#ifdef DBG_VRAM
        {
            /* Debug: VRAM tile DBG_VRAM's first 16 bytes. */
            extern u16 dbg_tile[8];
            u16 i;
            *(vu32 *)VDP_CTRL_PORT = ((u32)((DBG_VRAM) & 0x3FFF) << 16) | ((DBG_VRAM) >> 14);   /* byte address */
            for (i = 0; i < 8; i++) dbg_tile[i] = *(vu16 *)VDP_DATA_PORT;
        }
#endif
#ifdef NO_SPLIT
        stage_split(FALSE);                 /* measurement builds: no HUD scroll splits */
#else
        stage_split(game_mode != GM_INTRO || stage_index);   /* a later stage's title: the HUD rows (empty) show the far layer, as in play (no band since 2026-11-04) */
#endif
        stage_commit();
        { extern u16 work_end; work_end = *(vu16 *)0xC00008; }
#ifdef AHEAD
        if (heavy && game_mode == GM_PLAY && !fe.paused)
        {
            extern vu32 vtimer;
            s16 late_now = (s16)((u16)(vtimer - pace_vt) - pace_n);
            u16 st = *(vu16 *)VDP_CTRL_PORT, raw = *(vu16 *)0xC00008 >> 8;
            if (late_now >= -1 && !(st & VDP_VBLANK_FLAG) && raw < 224 && raw + upd_est + AHEAD_MARGIN < 224)
            {
                frames++; game_frame++;         /* the drawn update's count, as after its VBlank */
                early = 1;
                ahead_runs++;
                goto run_update;
            }
        }
    vblank_now:
#endif
#ifndef NO_LATEVBL
        {
            /* Late start: the frame was queued a few lines after this VBlank
             * began. SGDK's wait would hold it for the whole next frame (and
             * the next update would then be late, a skipped drawing). When
             * its transfers fit in the VBlank lines left (DMA ~200 bytes a
             * line, the CPU-written ones ~100; margin for the pad read and
             * the callbacks), it goes now: what the VBlank interrupt latched
             * is redone with this frame's values. */
            /* The raw V counter (not SGDK's adjusted one, active line + 32):
             * NTSC counts 0-234 (0xEA) then 229-255 (0xE5-0xFF) for lines
             * 235-261, so in VBlank 229-234 is ambiguous: taken as the later
             * pass (fewer lines left). */
            u16 st = *(vu16 *)VDP_CTRL_PORT, raw = *(vu16 *)0xC00008 >> 8, real, left;
            if (game_mode == GM_PLAY && (st & VDP_VBLANK_FLAG) && raw >= 224)
            {
                real = raw >= 229 ? raw + 6 : raw;
                late_tries++; late_line = real;
                /* Measured (stage 6 area 2, the stage 1 bridge): the VBlank
                 * work takes ~14 lines with nothing queued, plus a line per
                 * ~170 bytes; kept: 17 + bytes / 150, done by line 259. */
                left = real <= 259 - LATE_FIXED ? 259 - LATE_FIXED - real : 0;
                if (real <= LATE_LINE && DMA_getQueueTransferSize() + 2 * (u32)stage_vblank_bytes() <= (u32)left * LATE_RATE)
                {
                    stage_vint_late();
                    late_starts++;
                    SYS_doVBlankProcessEx(ON_VBLANK);       /* in VBlank: no wait */
#ifdef LATE_CHECK
                    { extern u16 late_over, late_worst; u16 r = *(vu16 *)0xC00008 >> 8; if (!(*(vu16 *)VDP_CTRL_PORT & VDP_VBLANK_FLAG) && r < 224) { late_over++; if (r > late_worst) late_worst = r; } }
#endif
                }
                else { late_big++; SYS_doVBlankProcess(); }
            }
            else
            {
#ifdef LATE_CHECK
                extern u16 vb_need, vb_lines;
                u32 nd = DMA_getQueueTransferSize() + 2 * (u32)stage_vblank_bytes();
                { extern u16 vb_q, vb_s; vb_q = DMA_getQueueTransferSize(); vb_s = stage_vblank_bytes(); }
                SYS_doVBlankProcess();
                { u16 r = *(vu16 *)0xC00008 >> 8; vb_need = nd > 0xFFFF ? 0xFFFF : nd; vb_lines = (*(vu16 *)VDP_CTRL_PORT & VDP_VBLANK_FLAG) ? (r >= 229 ? r + 6 : r) - 224 : r + 38; }
#else
                SYS_doVBlankProcess();
#endif
            }
        }
#else
        SYS_doVBlankProcess();
#endif
#ifdef VBSTEPS
        { extern u16 vb_t3; vb_t3 = *(vu16 *)0xC00008 >> 8; }
#endif
#ifdef SOUND_DEBUG
        sound_debug();
#endif
#ifdef AHEAD
        if (!fe.paused && !ahead) { frames++; game_frame++; }   /* ahead: counted when the drawn one before it was */
#else
        if (!fe.paused) { frames++; game_frame++; }     /* Home pause: the game's clocks hold */
#endif
        if ((u16)(vbl_count - last) > 1) missed += vbl_count - last - 1;
        last = vbl_count;
    }
    return 0;
}
