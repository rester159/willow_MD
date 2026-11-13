/* Stage 1 boss arena (specs/stage1_boss.md): the sage's dialogue (class 4
 * kind 7 variant 0), the giant beast's body (class 18, routine 26DE8), head
 * (class 4 kind 10, 163D2), paw (kind 11, 167B8) and breath flames (class
 * 10 kind 5, 24294), its death and the stage exit.
 *
 * The body is a plane B picture (tools/bosspack.py): every pose composited
 * offline into a 27 x 14 cell region at the arena's fixed camera, its tiles
 * streamed into foreground pool slots no arena column uses (two buffers:
 * the next pose loads while the current one shows, then the region's map
 * is swapped). The head, paw and flames are sprites.
 *
 * Colours: the fur takes the scenery lines' cycle pens 10-15 while the boss
 * shows (tools/sprpal.py, tools/bosspack.py); the hit flash swaps them with
 * the flash colours (only the fur flashes: its claws and horns use pens the
 * scenery shares). */
#include <genesis.h>
#include "stagedata.h"
#include "actor.h"
#include "player.h"
#include "hud.h"
#include "sound.h"
#include "game.h"
#include "stage.h"
#include "../data/boss1/layout.h"

#define SND_BOSS_MUSIC 0x01
#define SND_TYPE 0x2F
#define SND_HEAD_HIT 0x32
#define SND_BREATH 0x8D
#define SND_BOSS_DIE 0x8E

extern const u8 boss_body[];        /* data/boss1/body.bin */
extern const u16 boss_slots[];      /* data/boss1/slots.bin: 2 x BODY_MAX_TILES VRAM tiles */
extern const u16 boss_maps[];       /* data/boss1/maps.bin: per buffer, per pose, the region's words */
extern const u16 boss_fur[12], boss_furflash[12];   /* lines 2-3 pens 10-15: fur, and its hit flash */

#define BODY_WX 3600        /* the body's world position */
#define BODY_WY 56
#define ARENA_X 3328

u8 scene_lock;              /* FF83E9 / FF83CF: the clock stands */
u8 boss_colors;             /* the fur holds the scenery's cycle pens (stage_cycle waits) */
static u8 flash_timer;

/* The fur's colours in, or the hit flash (controller 0xE6A60: flash and
 * normal alternating, 4 updates each, 4 steps). */
static void fur(const u16 *pens)
{
    PAL_setColors(32 + 10, pens, 6, DMA_QUEUE);
    PAL_setColors(48 + 10, pens + 6, 6, DMA_QUEUE);
}

static void flash_start(void)
{
    flash_timer = 16;
}

static void flash_step(void)
{
    if (!flash_timer) return;
    flash_timer--;
    if (!(flash_timer & 3)) fur((flash_timer >> 2) & 1 ? boss_furflash : boss_fur);
    if (flash_timer == 15) fur(boss_furflash);
}
static u8 boss_seen;        /* FF838B: the dialogue and the body's rise are over */
u8 boss_head_spawned;       /* FF0E96 */

void boss_seen_set(void) { boss_seen = 1; }   /* the attract's demo: the fight at once (FF838B) */

u8 boss_seen_flag(void)
{
    return boss_seen;
}
static Actor *body, *head, *paw;

/* ---- Body picture on plane B. */
static s8 body_want = -1, body_shown = -1, body_loading = -1;
static u8 body_buf;
static u16 load_at;
#define LOAD_PER_FRAME 64   /* tiles a frame (2 KiB of DMA) */

static void region_restore(void)
{
    u16 r, c, w[BODY_W];
    for (r = 0; r < BODY_H; r++)
    {
        for (c = 0; c < BODY_W; c++) w[c] = stage_bword(BODY_X + c, BODY_Y + r);
        stage_bput(BODY_X, BODY_Y + r, w, BODY_W);
    }
}

/* Once a frame, after the updates: stream the wanted pose. */
bool boss_present(void)
{
    const u8 *e;
    const u8 *tiles;
    const u16 *slot;
    u16 n, end;
    bool loaded_now;
    if (body_loading < 0)
    {
        if (body_want == body_shown) return body_shown >= 0;
        if (body_want < 0) { region_restore(); body_shown = -1; return FALSE; }
        body_loading = body_want;
        load_at = 0;
    }
    e = boss_body + 10 + body_loading * 10;
    n = *(const u16 *)e;
    tiles = boss_body + *(const u32 *)(e + 2);
    slot = boss_slots + (body_buf ^ 1) * BODY_MAX_TILES;
    end = load_at + LOAD_PER_FRAME < n ? load_at + LOAD_PER_FRAME : n;
    loaded_now = load_at < end;             /* the map goes in a frame of its own */
    while (load_at < end)
    {
        u16 k = 1, v = slot[load_at];
        while (load_at + k < end && slot[load_at + k] == v + k) k++;
        DMA_queueDma(DMA_VRAM, (void *)(tiles + (u32)load_at * 32), v * 32, k * 16, 2);
        load_at += k;
    }
    if (load_at >= n && !loaded_now)
    {
        /* Every tile is queued: the map (resolved offline) follows in the
         * same VBlank, straight from ROM. */
        const u16 *m = boss_maps + ((u32)((body_buf ^ 1) * BODY_POSES + body_loading)) * (BODY_W * BODY_H);
        u16 r;
        for (r = 0; r < BODY_H; r++, m += BODY_W)
            stage_bput_rom(BODY_X, BODY_Y + r, m, BODY_W);     /* where the far scroll puts the region */
        body_buf ^= 1;
        body_shown = body_loading;
        body_loading = -1;
    }
    return body_shown >= 0;
}

/* A restart (stage_start): the region back to the backdrop. */
void boss_reset(void)
{
    if (body_shown >= 0 || body_loading >= 0) region_restore();
    body_want = body_shown = body_loading = -1;
    body = head = paw = NULL;
    boss_head_spawned = 0;
    scene_lock = 0;
    boss_colors = 0;
    flash_timer = 0;
    hud_boss(-1);
}

void boss_new_game(void)
{
    boss_seen = 0;
}

/* ---- Sage (dialogue scene, 0x1497C..0x14A44). */
#define SAGE_TALK 15        /* chain 0x1528A */
#define SAGE_GO 14          /* chain 0x150FE */
static const char *const sage_text[3] = {
    "THE BABY YOU ARE LOOKING FOR",
    "IS IN THE HANDS OF BAVMORDA.",
    "YOU WOULD BE WISE TO GIVE UP.",
};
#define TEXT_ROW 4
#define TEXT_COL 6

static void spawn_body(void)
{
    Actor *b = actor_new(F_BOSS_BODY);
    if (!b) return;
    b->x = (s32)BODY_WX << 8;
    b->y = (s32)BODY_WY << 8;
    b->variant = boss_seen;     /* seen: no rise */
    boss_seen = 1;
    boss_colors = 1;
    flash_timer = 0;
    fur(boss_fur);
}

void sage_update(Actor *a)
{
    switch (a->state)
    {
    case 0:                                 /* dormant until the arena camera */
        a->active = 0;
        if (CAM_X < ARENA_X || player.cam_max_x != ARENA_X) return;
        player.cam_min_x = ARENA_X;
        sound_post(SND_BOSS_MUSIC);
        if (boss_seen)                      /* a restart: the fight at once */
        {
            spawn_body();
            actor_free(a);
            return;
        }
        scene_lock = 1;
        a->state = 2;
        a->active = 2;
        a->sub = 0;                         /* line */
        a->v[0] = 0;                        /* characters typed */
        a->timer = 30;
        actor_anim(a, SAGE_TALK);
        return;
    case 2:                                 /* typed text, 3 updates a character */
        actor_step(a);
        if (--a->timer) return;
        a->timer = 3;
        if (a->sub == 3) { a->state = 4; a->timer = 90; return; }
        {
            const char *t = sage_text[a->sub];
            a->v[0]++;
            hud_text(t, a->v[0], TEXT_COL, TEXT_ROW + a->sub * 2);
            if (t[a->v[0] - 1] != ' ') sound_post(SND_TYPE);
            if (!t[a->v[0]]) { a->sub++; a->v[0] = 0; a->timer = 12; }
        }
        return;
    case 4:                                 /* read, then go */
        actor_step(a);
        if (--a->timer) return;
        hud_message(NULL, TEXT_ROW); hud_message(NULL, TEXT_ROW + 2); hud_message(NULL, TEXT_ROW + 4);
        scene_lock = 0;
        actor_anim(a, SAGE_GO);
        a->state = 6;
        a->timer = 38;
        return;
    case 6:
        actor_step(a);
        if (--a->timer) return;
        spawn_body();
        a->state = 8;
        a->timer = 27;
        return;
    case 8:
        actor_step(a);
        if (!--a->timer) actor_free(a);
        return;
    }
}

/* ---- Body. */
enum { B_RISE, B_WAIT, B_IDLE, B_DEATH, B_SINK, B_EXIT };
static const u8 loop_ticks[8] = { 10, 6, 6, 6, 10, 10, 10, 10 };
static const u8 pose_value[8] = { 1, 2, 3, 4, 5, 4, 3, 2 };         /* 0x2702C */
static const s8 head_off[8][2] = {                                  /* 0x16564 */
    { -32, 80 }, { -52, 68 }, { -72, 56 }, { -92, 44 }, { -112, 32 }, { -92, 44 }, { -72, 56 }, { -52, 68 }
};
static const u8 attack_cmd[6] = { 1, 1, 0x80, 0, 5, 3 };            /* 0x26FDE */
static const u8 attack_pose[6] = { 5, 3, 3, 1, 1, 1 };              /* 0x26FE2 */
static const s8 attack_luck[16] = { 1, 1, -1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, -1, 1 };   /* 0x26FE6 */
static const u8 attack_wait[4] = { 64, 32, 16, 8 };                 /* 0x2709C by rank */
static const u8 burst_variant[16] = { 4, 4, 5, 5, 5, 4, 5, 4, 4, 5, 5, 4, 4, 5, 5, 4 };  /* 0x2715C */
static const s8 burst_off[16] = { 0, -16, 16, -32, 48, -48, 20, -20, 24, -24, 40, -40, 8, -8, 4, -4 };   /* 0x271EE */
/* Boxes by pose value 1..5 (0x3B07A), the same for touching and shots. */
const s16 boss_body_boxes[5][2][4] = {
    { { 40, 136, 0, 96 }, { 40, 136, 0, 96 } }, { { 56, 152, 0, 96 }, { 56, 152, 0, 96 } },
    { { 64, 160, 0, 88 }, { 64, 160, 0, 88 } }, { { 80, 176, 0, 88 }, { 80, 176, 0, 88 } },
    { { 96, 192, 0, 72 }, { 96, 192, 0, 72 } },
};

#define CONTROL(b) ((b)->sub)
#define PENDING(b) ((b)->v[0])

static void wait_time(Actor *a)
{
    u16 r = rank >> 12;
    a->v[3] = attack_wait[r < 6 ? 0 : r < 10 ? 1 : r < 14 ? 2 : 3];
}

static void place_head(void)
{
    if (!head || head->fam != F_BOSS_HEAD) return;
    head->x = body->x + ((s32)head_off[CONTROL(body)][0] << 8);
    head->y = body->y + ((s32)head_off[CONTROL(body)][1] << 8);
}

/* The 8-pose loop: TRUE when the control advanced. */
static bool loop_step(Actor *a)
{
    if (--a->t2) return FALSE;
    CONTROL(a) = (CONTROL(a) + 1) & 7;
    a->t2 = loop_ticks[CONTROL(a)];
    a->shown = pose_value[CONTROL(a)] - 1;
    body_want = 4 + CONTROL(a);
    place_head();
    return TRUE;
}

/* Attack choice (26F5E..26FF4). */
static void choose(Actor *a)
{
    s16 i = ((player.x >> 8) - ARENA_X) >> 6;
    u8 cmd, pose;
    if (i < 0) i = 0;
    if (i > 5) i = 5;
    cmd = attack_cmd[i];
    pose = attack_pose[i];
    if (!cmd) return;
    if (cmd != 0x80)
    {
        if ((player.y >> 8) >= 80) { cmd = 1; pose = 1; }
        else
        {
            s8 luck = attack_luck[rnd() & 15];
            if (!luck) return;
            if (luck < 0) { if (pose == 3) cmd = 0x80; else pose = 3; }
        }
    }
    a->v[1] = cmd;
    a->v[2] = pose;
    a->phase = 1;
}

static void children(void)
{
    boss_head_spawned = 1;
    Actor *h = actor_new(F_BOSS_HEAD), *p = actor_new(F_BOSS_PAW);
    head = h;
    paw = p;
    if (h) { h->x = body->x - (32 << 8); h->y = body->y + (80 << 8); }
    if (p) { p->x = body->x - (80 << 8); p->y = body->y + (24 << 8); }
}

void boss_body_update(Actor *a)
{
    body = a;
    flash_step();
    switch (a->state)
    {
    case B_RISE:                            /* 26DFC, 26E82 */
        if (!a->t2)
        {
            a->active = 2;
            a->shown = 0;
            CONTROL(a) = 0;
            midboss_active = 1;
            a->timer = a->variant ? 1 : 40;
            a->t2 = 1;
        }
        if (!a->variant) body_want = (40 - a->timer) / 10 > 3 ? 3 : (40 - a->timer) / 10;
        if (--a->timer) return;
        children();
        body_want = 4;
        a->state = B_WAIT;
        a->timer = 60;
        return;
    case B_WAIT:
        if (--a->timer) return;
        a->active = 1;
        a->state = B_IDLE;
        a->phase = 0;
        a->t2 = loop_ticks[0];
        PENDING(a) = 0;
        wait_time(a);
        return;
    case B_IDLE:
        switch (a->phase)
        {
        case 0:
            loop_step(a);
            if (!--a->v[3]) { wait_time(a); choose(a); }
            break;
        case 1:                             /* animate to the attack pose */
            if (pose_value[CONTROL(a)] == a->v[2])
            {
                PENDING(a) = a->v[1];
                a->phase = 2;
                break;
            }
            loop_step(a);
            break;
        case 2:                             /* hold while the head or paw acts */
            if (!PENDING(a)) { a->phase = 0; wait_time(a); }
            break;
        }
        return;
    case B_DEATH:                           /* 270AC: 40 bursts, 6 updates apart */
        if (!(a->v[4] % 6))
        {
            effect(F_BURST, AX(a) + burst_off[rnd() & 15], AY(a) + burst_off[rnd() & 15] + 48, burst_variant[rnd() & 15]);
            sound_post(SND_EXPLODE);
            if (!flash_timer) flash_start();
        }
        a->v[4]++;
        if (a->timer && !--a->timer)        /* the head and paw go; the body sinks */
        {
            if (head && head->fam == F_BOSS_HEAD) actor_free(head);
            if (paw && paw->fam == F_BOSS_PAW) actor_free(paw);
            head = paw = NULL;
            a->v[5] = 1;
        }
        if (a->v[5] && a->v[5] <= 50)       /* collapse: 4 poses x 10, then gone */
        {
            u16 k = (a->v[5] - 1) / 10;
            body_want = k < 4 ? 12 + k : -1;
            a->v[5]++;
        }
        if (a->v[4] < 240) return;
        award(10);                          /* 2000 */
        a->y += 128 << 8;
        drop(a, 13);
        a->y -= 128 << 8;
        midboss_active = 0;
        hud_boss(-1);
        player.cam_max_x = 3456;            /* the stage's final camera stop */
        player.cam_catch = 1;
        a->state = B_EXIT;
        a->timer = 200;
        return;
    case B_EXIT:                            /* stage-exit object (374C6) */
        if (a->v[5] && a->v[5] <= 50)
        {
            u16 k = (a->v[5] - 1) / 10;
            body_want = k < 4 ? 12 + k : -1;
            a->v[5]++;
        }
        if (--a->timer) return;
        body_want = -1;
        boss_colors = 0;                    /* the waterfall's cycle takes its pens back */
        actor_free(a);
        body = NULL;
        game_stage_clear();
        return;
    }
}

static void boss_dies(void)
{
    sound_post(SND_BOSS_DIE);
    scene_lock = 1;
    hud_boss(0);
    if (body && body->fam == F_BOSS_BODY)
    {
        body->state = B_DEATH;
        body->active = 2;
        body->timer = 192;
        body->v[4] = 0;
        body->v[5] = 0;
    }
    if (paw && paw->fam == F_BOSS_PAW) paw->active = 2;
}

/* ---- Head (163D2). */
enum { H_IDLE = 2, H_RECOVER, H_BREATH, H_DEAD };
static const s16 flame_off[10][2] = {                               /* 0x164DE by n */
    { 0, 0 }, { -216, 8 }, { -184, 0 }, { -152, -8 }, { -120, -16 }, { -88, -24 }, { -64, -24 }, { -40, -24 }, { -24, -16 }, { -16, -8 }
};

void boss_head_update(Actor *a)
{
    if (!body || body->fam != F_BOSS_BODY) { actor_free(a); return; }
    switch (a->state)
    {
    case 0:
#ifdef BOSS_HP
        a->hp = a->last_hp = BOSS_HP;       /* test builds */
#else
        a->hp = a->last_hp = 32;
#endif
        a->state = H_IDLE;
        a->active = 1;
        actor_anim(a, 0);
        hud_boss(32);
        return;
    case H_DEAD:
        return;
    }
    if (a->hp != a->last_hp)
    {
        a->last_hp = a->hp;
        hud_boss(a->hp);
        flash_start();
        if (a->state == H_BREATH)
        {
            if (!a->hp) { a->state = H_DEAD; a->active = 2; boss_dies(); return; }
            sound_post(SND_HEAD_HIT);
            a->t2 = 17;                     /* hittable again after 17 */
        }
        else
        {
            sound_post(SND_HEAD_HIT);
            a->state = H_RECOVER;
            a->timer = 16;
        }
    }
    switch (a->state)
    {
    case H_IDLE:
        actor_step(a);
        if (PENDING(body) & 1)
        {
            sound_post(SND_BREATH);
            a->state = H_BREATH;
            a->phase = 0;
            a->timer = 20;
            actor_anim(a, 0);
        }
        break;
    case H_RECOVER:                         /* 16 updates, not hittable (the palettes flash) */
        actor_step(a);
        a->active = 2;
        if (--a->timer) break;
        if (!a->hp) { a->state = H_DEAD; a->active = 2; boss_dies(); break; }
        a->active = 1;
        a->state = H_IDLE;
        break;
    case H_BREATH:                          /* 16432..16532: 108 updates */
        actor_step(a);
        if (a->t2) { a->active = 2; if (!--a->t2) a->active = 1; }
        if (--a->timer) break;
        switch (a->phase)
        {
        case 0:
            a->phase = 2;
            a->timer = 10;
            a->v[0] = 9;
            break;
        case 2:                             /* flames 9 .. 1, 6 updates apart */
            {
                Actor *f = actor_new(F_BOSS_FIRE);
                if (f)
                {
                    f->variant = a->v[0];
                    f->x = a->x + ((s32)flame_off[a->v[0]][0] << 8);
                    f->y = a->y + ((s32)flame_off[a->v[0]][1] << 8);
                }
                if (!f || !--a->v[0]) { a->phase = 6; a->timer = 24; }
                else a->timer = 6;
            }
            break;
        case 6:
            PENDING(body) = 0;
            a->state = H_IDLE;
            if (!a->t2) a->active = 1;
            actor_anim(a, 0);
            break;
        }
        break;
    }
}

/* ---- Paw (167B8). */
static const u8 paw_shown[8] = { 0, 0, 1, 0, 0, 0, 1, 0 };          /* 0x16830 */
static const s8 claw_step[8][2] = {                                 /* 0x1689A */
    { 0, 0 }, { -24, 8 }, { -16, 32 }, { 24, 24 }, { -24, -32 }, { 16, -32 }, { 16, -8 }, { 8, 0 }
};

void boss_paw_update(Actor *a)
{
    if (!body || body->fam != F_BOSS_BODY) { actor_free(a); return; }
    if (!a->state)
    {
        a->state = 2;
        a->hp = a->last_hp = 127;
        a->v[0] = AX(a);
        a->v[1] = AY(a);
        actor_anim(a, 0);
    }
    if (body->state != B_IDLE) return;      /* death: shown, harmless */
    a->hp = a->last_hp = 127;               /* shots are absorbed */
    if (a->state == 4)                      /* claw swipe */
    {
        if (anim_step(&a->anim))
        {
            a->pose = anim_pose(&a->anim);
            if (++a->sub == 8)
            {
                PENDING(body) = 0;
                a->state = 2;
                a->x = (s32)a->v[0] << 8;
                a->y = (s32)a->v[1] << 8;
                actor_anim(a, 0);
            }
            else
            {
                a->x += (s32)claw_step[a->sub][0] << 8;
                a->y += (s32)claw_step[a->sub][1] << 8;
            }
        }
        a->active = 1;
        return;
    }
    a->active = paw_shown[CONTROL(body)] ? 1 : 0;
    if (a->active && (PENDING(body) & 0x80))
    {
        a->state = 4;
        a->sub = 0;
        actor_anim(a, 0);
    }
}

/* Drawing only (src/actor.c actors_draw): the body picture takes a few
 * frames to stream a new pose (boss_present), while the head and paw follow
 * the body's control at once. Drawn against the control still shown, they
 * stay on the body; positions, boxes and timing remain the control's
 * (specs/stage1_boss.md). Player report: "its swatting animation is out of
 * tune with the sprite ... a disembodied limb floating around". Returns
 * FALSE when the actor is not to be drawn this frame. */
bool boss_draw_adjust(const Actor *a, s16 *sx, s16 *sy)
{
    u16 c, sc;
    /* The rise's end creates the head and paw at once; the first beast
     * picture streams in over the next frames: until it shows, the red
     * silhouette is up, so they are not drawn (V0.8 player report: "starts
     * with a disembodied arm"). Drawing only. */
    if (body && body->fam == F_BOSS_BODY && body->state <= B_WAIT && body_shown < 4) return FALSE;
    if (!body || body->fam != F_BOSS_BODY || body->state != B_IDLE || body_shown < 4 || body_shown > 11) return TRUE;
    c = CONTROL(body);
    sc = body_shown - 4;
    if (sc == c) return TRUE;
    if (a->fam == F_BOSS_HEAD)
    {
        *sx += head_off[sc][0] - head_off[c][0];
        *sy -= head_off[sc][1] - head_off[c][1];  /* world y up, screen y down */
        return TRUE;
    }
    return a->state == 4 || paw_shown[sc];     /* the paw shows with its poses (a swipe under way stays) */
}

/* ---- Breath flame (24294): stationary; harmful 24 updates, then shown 4. */
void boss_fire_update(Actor *a)
{
    if (!a->state)
    {
        a->state = 2;
        a->active = 1;
        a->timer = 24;
        actor_anim(a, a->variant);
        return;
    }
    actor_step(a);
    if (--a->timer) return;
    if (a->state == 2) { a->state = 4; a->active = 2; a->timer = 4; }
    else actor_free(a);
}
