/* Two-player co-op (Home; specs/coop.md). The lead player lives in the
 * globals as in one-player play; the second player's record (Player, Magic,
 * the hud's per-player values, sword level) is kept here and swapped in
 * while it updates, is tested against enemies or is drawn. The camera
 * belongs to the lead's record; the second player sees a copy of it and its
 * own camera changes are dropped. Everything here is inactive (two_players
 * 0) in one-player play, which stays exact. */
#include <genesis.h>
#define COLD __attribute__((noinline, optimize("Os")))   /* rare paths (joins, deaths, pickups): small */
#include "coop.h"
#include "actor.h"
#include "player.h"
#include "magic.h"
#include "shot.h"
#include "spell.h"
#include "hud.h"
#include "game.h"
#include "sprite.h"
#include "frontend.h"
#include "stagedata.h"
extern u16 game_frame;

#define P2_TILES 43                 /* Madmartigan's largest pose (Willow's 36) */

/* Per player. Willow's magic level (magic.level, hud.magic) and
 * Madmartigan's sword level (sword_level) stay in the globals: each belongs
 * to one hero only. */
typedef struct
{
    Player p;
    Magic m;
    u32 score, gold;
    u8 hp, max_hp, armor, lives;            /* as in Hud: exchanged as one block */
    u8 items[12];
} Ctx;

u8 coop_state;
u8 coop_cur;
u8 coop_lead_pad;
u8 coop_owner;
static Ctx other;                   /* the player not in the globals */
static u16 p2_base;                 /* the second player's pose VRAM (0: not carved yet) */
static u8 in_swap;
static u16 last2;
static u8 apart;
static u8 join_held;                /* Start still held since the join */                    /* updates the players have been too far apart */
static void second_dead(void);
static void line0_now(void);                   /* the free / second pad last update */

extern u8 game_lives_dip(void);
extern void game_count_continue(void);
extern u32 bcd_add(u32 a, u32 b, u16 digits);
extern u16 posecache_p2(u16 n);
extern void collide_player(void);

/* Exchange n bytes (even, word aligned) of two records in place. */
static void xchg(void *a, void *b, u16 n)
{
    u16 *p = a, *q = b;
    n >>= 1;
    while (n--) { u16 t = *p; *p++ = *q; *q++ = t; }
}

/* The globals and the other record exchanged; Willow's magic level stays. */
static void exchange_all(void)
{
    u8 level = magic.level, b[12];
    u32 t;
    xchg(&player, &other.p, sizeof(Player));
    xchg(&magic, &other.m, sizeof(Magic));
    magic.level = other.m.level = level;
    t = hud.score; hud.score = other.score; other.score = t;
    t = hud.gold; hud.gold = other.gold; other.gold = t;
    xchg(&hud.hp, &other.hp, 4);            /* hp, max_hp, armor, lives (adjacent in both) */
    memcpy(b, hud.items, 12); memcpy(hud.items, other.items, 12); memcpy(other.items, b, 12);
    in_swap = 1;
    player_form(player.form);               /* the form's tables (player.c's pf) */
    in_swap = 0;
}

/* The camera fields of the lead's record (the shared camera). */
static void cam_copy(Player *to, const Player *from)
{
    to->cam_x = from->cam_x; to->cam_y = from->cam_y; to->cam_min_x = from->cam_min_x;
    to->cam_vy = from->cam_vy; to->cam_yf = from->cam_yf; to->cam_track = from->cam_track;
    to->cam_max_x = from->cam_max_x; to->cam_min_y = from->cam_min_y; to->cam_max_y = from->cam_max_y;
    to->cam_catch = from->cam_catch;
}

/* The second player into the globals (and back): the camera stays the
 * lead's (the second sees a copy; its changes are dropped on the way back). */
static void swap_in(void)
{
    exchange_all();
    cam_copy(&player, &other.p);
    coop_cur = 1;
}

static void swap_out(void)
{
    exchange_all();
    coop_cur = 0;
}

/* Permanently exchange the lead and the second player (the lead is out). */
static COLD void exchange(void)
{
    exchange_all();
    cam_copy(&player, &other.p);
    coop_lead_pad ^= 1;
}

COLD void coop_reset(void)
{
    coop_state = CO_NONE;
    coop_cur = 0;
    coop_lead_pad = 0;
    coop_owner = 0;
    two_players = 0;
    last2 = 0xFFFF;                 /* a Start held from the menu is not a join */
#ifdef COOP_AUTOJOIN
    coop_pick = 1;                  /* test builds: pad 2 joins as play begins */
#endif
#if defined(COOP) || defined(COOP_TEST)
    if (fe.mode == FE_HOME) *(vu8 *)0xA1000B = 0x40;  /* pad 2: TH an output (read directly, pad2_read) */
#endif
}

/* player_form's filter. In co-op the lead is always the stage's hero (the
 * stage scripts are the arcade's, written for that hero): asked for the
 * other hero, the two players exchange the lead (each pad keeps its hero).
 * A player alone (the other absent or out) plays the stage's hero, as 1P. */
u16 coop_form(u16 form)
{
    if (!two_players || in_swap || coop_cur) return form;
    if (form != player.form && other.p.form == form) exchange();
    return form;
}

/* Place the record in the globals at (x, y) as player_init does, keeping
 * the camera (respawns and joins happen mid-stage). */
static COLD void respawn_at(s16 x, s16 y, u16 facing)
{
    Player cam = player;
    player_init(x, y);
    cam_copy(&player, &cam);
    player.facing = facing | 2;
    player_stand(facing & 1);
    player.hurt_timer = 120;        /* 2 s blinking, not hittable */
    player.vuln = 2;
}

/* The second player at the lead's last safe ground (player.safe_x / y:
 * a respawn beside a partner in mid-air or over a pit would fall again). */
static COLD void beside_lead(void)
{
    s16 x = player.safe_x, y = player.safe_y;
    u16 lf = player.facing & 1;
    swap_in();
    respawn_at(x, y, lf);
    swap_out();
}

static COLD bool join(bool fresh)   /* out of line: coop_pads runs every update */
{
    const Settings *s = frontend_settings();
    if (!fresh && !home_credits) return FALSE;   /* an out player continues with a shared credit (none left: stays out) */
    if (!p2_base)
    {
        p2_base = posecache_p2(P2_TILES + 36);  /* the pose, then the HUD's right bar colours (30) and top cells (6) */
        if (p2_base) hud_2p_vram(p2_base + P2_TILES);
    }
    if (!p2_base) return FALSE;     /* no VRAM now (e.g. stage 5's doorway wipe holds some): try again later */
    if (fresh)
    {
        memset(&other, 0, sizeof other);
        other.p.form = player.form ^ 1;     /* the other hero */
        other.max_hp = s->vitality;
    }
    if (!fresh) { home_credits--; game_count_continue(); }      /* an out player continuing (Home NEW GAME PLUS needs none) */
    other.lives = game_lives_dip();
    other.hp = other.max_hp;
    other.score = 0;
    coop_state = CO_IN;
    two_players = 1;
    line0_now();
    beside_lead();
    hud_layout();
    return TRUE;
}

bool coop_join(void) { return join(coop_state == CO_NONE); }

/* Pad 2, three buttons, read directly once an update (SGDK's per-VBlank
 * port 2 support costs 1P Home drawn frames; main.c keeps it off). */
static u16 pad2_read(void)
{
    vu8 *d = (vu8 *)0xA10005;
    u8 hi, lo;
    *d = 0x40;
    __asm__ volatile ("nop\n\tnop");
    hi = *d;                        /* TH 1: C B R L D U */
    *d = 0x00;
    __asm__ volatile ("nop\n\tnop");
    lo = *d;                        /* TH 0: Start A 0 0 D U */
    *d = 0x40;
    return (~hi & 0x3F) | ((~lo & 0x30) << 2);
}

u16 coop_pad2(void) { return pad2_read(); }

u8 coop_pick;

/* The pads: returns the second player's buttons; *joy becomes the lead's.
 * In Home play the free pad's Start joins (or continues an out player). */
u16 coop_pads(u16 *joy)
{
    u16 j2, second, start;
#if !defined(COOP) && !defined(COOP_TEST)
    (void)joy; (void)j2; (void)second; (void)start;
    return 0;                       /* co-op not enabled yet (colours, HUD to come): -DCOOP builds */
#endif
    if (fe.mode != FE_HOME || fe.demo) return 0;
    j2 = pad2_read();
    if (coop_lead_pad) { u16 t = *joy; *joy = j2; j2 = t; }
    second = j2;
    start = second & ~last2 & BUTTON_START;
    last2 = second;
    if (coop_pick && coop_state == CO_NONE && game_mode == GM_PLAY && !player.script && !player.dead && !scene_lock)
    {
        /* Home 2 PLAYERS: the second player joins as play begins; pad 1
         * plays the hero it picked (the lead is the stage's Willow). */
        coop_lead_pad = coop_pick == 2;
        if (coop_join()) coop_pick = 0;
        else coop_lead_pad = 0;         /* no room yet: again next update */
        last2 = second;
        return 0;
    }
    if (coop_state != CO_IN)
    {
        if (start && game_mode == GM_PLAY && !fe.paused && !player.script && !player.dead && !scene_lock) { coop_join(); join_held = 1; }
        return 0;
    }
    if (!(second & BUTTON_START)) join_held = 0;
    return join_held ? second & ~BUTTON_START : second;    /* the joining press is not a pause */
}

/* The second player's update (after the lead's). */
static u8 sitting_out;               /* the second player waits out the lead's scripted part */
static u8 free_in_script;           /* thrown off stage 4's snowball: it plays on during the lead's script */
static u16 held2;                   /* the second player's buttons this update */

void coop_frame(u16 joy_lead, u16 joy_second)
{
    (void)joy_lead;
    if (!two_players) return;
    held2 = joy_second;
    if (!player.script) free_in_script = 0;
    if ((player.script && !free_in_script) || game_mode != GM_PLAY)
    {
        /* The lead's scripted parts (stage 2's wagon leap, stage 4's carries,
         * walk-ins) and the non-play modes: the second player is not shown,
         * moved or hit; it comes back at the lead's side afterwards. */
        sitting_out = 1;
        return;
    }
    if (sitting_out)
    {
        sitting_out = 0;
        if (!player.dead && !player.airborne) { player.safe_x = player.x >> 8; player.safe_y = player.y >> 8; }
        beside_lead();
    }
    swap_in();
    collide_player();               /* enemies vs the second player (positions of the last game update) */
    player_update(joy_second);
    magic_meter();
    if (!player.form) { spell_update(); magic_update(); }   /* Willow's spells and sparks */
    swap_out();
    if (other.p.dead == 2) { second_dead(); return; }
    /* Leash (specs/coop.md 2.5): the screen edge holds the players within
     * one screen across; kept more than 256 px across or 160 px apart
     * vertically for 2 s (a ledge, a climb, a fall), the second player comes
     * back to the lead's last safe ground, blinking. */
    if (!other.p.dead && !player.dead && !player.script)  /* (not during the lead's scripts: its safe ground is behind) */
    {
        s16 dx = (s16)((other.p.x - player.x) >> 8), dy = (s16)((other.p.y - player.y) >> 8);
        if ((u16)(dx + 256) > 512 || (u16)(dy + 160) > 320) { if (++apart >= 120) { apart = 0; beside_lead(); } }
        else apart = 0;
    }
}


/* A player's life is lost (game.c: the lead's dead == 2, or here the
 * second's). With the partner standing the player comes back at its side;
 * out of lives it is out (Start on its pad continues). Returns TRUE when
 * co-op handled the lead's loss (no checkpoint restart). */
static COLD bool lose_life(void)
{
    if (hud.lives) hud.lives = (hud.lives & 15) ? hud.lives - 1 : hud.lives - 7;
    magic.hold = 0;
    return hud.lives != 0;
}

COLD bool coop_life_lost(void)
{
    if (!two_players) return FALSE;
    if (other.p.dead)
    {
        /* Both down: the checkpoint restart (game.c's life_lost takes the
         * lead's life, coop_place the second's). A lead on its last life with
         * a partner that has more is out, and the partner leads the restart. */
        u8 l = other.lives;
        if ((hud.lives & 0xFF) == 1 && l > 1)
        {
            lose_life();
            exchange();
            coop_state = CO_OUT;
            two_players = 0;            /* (the restart's fade loads the lead's line 0) */
        }
        return FALSE;
    }
    if (lose_life())
    {
        respawn_at(other.p.safe_x, other.p.safe_y, other.p.facing & 1);   /* the partner's last safe ground */
        hud_dirty |= HUD_VITAL;
        return TRUE;
    }
    /* The lead is out: the second player leads on alone. */
    exchange();
    coop_state = CO_OUT;
    two_players = 0;
    line0_now();
    hud_layout();
    return TRUE;
}

/* Stage 4 in co-op (AGENTS.md "Stage 4 in 2P"): Left on either pad brakes
 * the sled; at the crash the second player is thrown off the snowball at
 * (x, y), lands and plays on while the snowball carries the stage hero. */
u16 coop_brake(void) { return two_players ? held2 & BUTTON_LEFT : 0; }

COLD void coop_throw_off(s16 x, s16 y)
{
    if (!two_players || !sitting_out) return;
    sitting_out = 0;
    free_in_script = 1;
    swap_in();
    respawn_at(x - 24, y, 0);
    swap_out();
}

/* Time over (game.c): the second player loses a life with the lead. */
COLD void coop_both_down(void)
{
    if (two_players) other.p.dead = 2;
}

/* The second player's own death (checked after its update). */
static COLD void second_dead(void)
{
    swap_in();
    if (player.dead == 2)
    {
        if (lose_life()) { swap_out(); beside_lead(); return; }
        swap_out();
        coop_state = CO_OUT;
        two_players = 0;
        line0_now();
        hud_layout();
        return;
    }
    swap_out();
}

u16 coop_draw(VDPSprite *sat, u16 first, s16 view_x, s16 view_y)
{
    u16 n = 0;
    if (!two_players) return 0;
    if (sitting_out) return 0;
    if (other.p.hurt_timer && (other.p.hurt_timer & 4)) return 0;   /* hurt: blinks (line 0 is shared) */
    n = player_draw_of(&other.p, sat, first, p2_base, view_x, view_y);
    return n;
}

bool coop_p2_dirty(void) { return two_players && other.p.pose_dirty; }

/* After the lead's player_init (a stage start or a restart): the second
 * player beside it; one that went down with the lead loses that life. */
COLD void coop_place(void)
{
    if (coop_state != CO_IN || coop_cur || in_swap) return;
    if (other.p.dead)
    {
        u8 l = other.lives;
        other.lives = l ? ((l & 15) ? l - 1 : l - 7) : 0;
        if (!other.lives) { coop_state = CO_OUT; two_players = 0; line0_now(); hud_layout(); return; }
    }
    beside_lead();
}

u8 coop_takes(s16 x, s16 y)
{
    const s16 *b;
    s16 px, py;
    if (!two_players || other.p.dead) return 0;
    b = player_box_of(&other.p);
    px = other.p.x >> 8; py = other.p.y >> 8;
    return x >= px - b[0] - 12 && x <= px - b[0] + b[1] + 12 && y >= py - b[2] - 12 && y <= py - b[2] + b[3] + 12;
}

/* Credit to the second player: its values in the globals for the call. */
COLD void coop_gold(u16 variant, u8 who)
{
    if (!who) { add_gold(variant); return; }
    swap_in(); add_gold(variant); swap_out();
}

COLD void coop_heart(u8 variant, u8 who)
{
    if (who) swap_in();
    hud.hp += variant ? 2 : 1;
    if (hud.hp > hud.max_hp) hud.hp = hud.max_hp;
    if (who) swap_out(); else hud_dirty |= HUD_VITAL;
}

/* A score for the second player (actor.c's award / add_score). */
COLD void coop_score(u32 bcd)
{
    other.score = bcd_add(other.score, bcd, 8);
    hud_dirty |= HUD_SCORE;
    if (other.score > hud.hi) { hud.hi = other.score; hud_dirty |= HUD_HI; }
}

/* The camera's target in co-op: the midpoint of the two players. */
bool coop_mid(s16 *x, s16 *y)
{
    if (!two_players || other.p.dead == 2 || sitting_out || player.script) return FALSE;   /* the lead's scripted parts (sled, snowball, walks): the lead's, the other kept on screen by the edge */
    *x = (s16)(((player.x >> 8) + (other.p.x >> 8)) >> 1);
    *y = (s16)(((player.y >> 8) + (other.p.y >> 8)) >> 1);
    return TRUE;
}

/* ---- Enemies aim at the nearer player (specs/coop.md 2.6): every 8
 * updates each enemy, boss part and the companion picks the nearer one
 * (pad[1]); while it updates, the globals show that player's position
 * (x, y exchanged with the second record: the families read player.x / .y;
 * a write moves that player). Contact and projectile hits are tested for
 * both players anyway (coop_frame). */
u8 coop_xy;

static void xy_exchange(void)
{
    s32 t;
    t = player.x; player.x = other.p.x; other.p.x = t;
    t = player.y; player.y = other.p.y; other.p.y = t;
    coop_xy ^= 1;
}

void coop_actor_update(Actor *a, u16 cls)
{
    ActorFn f = families[a->fam]->update;
    actor_now = a;
#ifdef EXP_NOTARGET
    (void)cls;
    if (0)
#else
    (void)cls;
    if (!other.p.dead && !player.script && !sitting_out)
#endif
    {
        if (!((game_frame + (u16)(a - actors)) & 7))
        {
            s32 ax = a->x >> 8, ay = a->y >> 8;
            s32 d1 = abs((s16)(ax - (player.x >> 8))) + abs((s16)(ay - (player.y >> 8)));
            s32 d2 = abs((s16)(ax - (other.p.x >> 8))) + abs((s16)(ay - (other.p.y >> 8)));
            a->pad[1] = d2 < d1;
        }
        if (a->pad[1])
        {
            xy_exchange();
            f(a);
            xy_exchange();
            if (game_mode == GM_SHOP) exchange();   /* the second player went into a shop: it leads (its gold, its items) */
            return;
        }
    }
    f(a);
}

COLD void coop_hurt_second(void)
{
    xy_exchange();
    swap_in();
    player_hurt();
    swap_out();
    xy_exchange();
}

/* The HUD's sides: 0 = pad 1's player, 1 = pad 2's. */
u16 coop_side_now(void) { return coop_lead_pad ^ coop_cur; }
u8 coop_other_lives(void) { return other.lives; }

COLD void coop_side(u16 side, HudSide *h)
{
    if (side == coop_side_now())
    {
        h->score = hud.score; h->gold = hud.gold; h->hp = hud.hp; h->max_hp = hud.max_hp;
        h->armor = hud.armor; h->lives = hud.lives; h->form = player.form; h->items = hud.items;
    }
    else
    {
        h->score = other.score; h->gold = other.gold; h->hp = other.hp; h->max_hp = other.max_hp;
        h->armor = other.armor; h->lives = other.lives; h->form = other.p.form; h->items = other.items;
    }
}

/* The stage's sprite lines 0-1 (sd->sprpal); in co-op line 0 is the one
 * both heroes share (data/coop/line0.bin). */
COLD const u16 *stage_sprpal(void)
{
    extern const u16 coop_line0[];
    static u16 buf[32];
    if (!two_players) return sd->sprpal;
    memcpy(buf, coop_line0, 32);
    memcpy(buf + 16, sd->sprpal + 16, 32);
    return buf;
}

static COLD void line0_now(void)
{
    PAL_setColors(0, stage_sprpal(), 16, DMA_QUEUE);
}

/* Madmartigan's record (his sword follows him). */
const Player *coop_mad(void)
{
    if (two_players && !player.form && other.p.form) return &other.p;
    return &player;
}
