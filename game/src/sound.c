/* Sound events (specs/audio.md, specs/audio_stage1.md): the game posts
 * arcade sound command words (low byte = Z80 command, high byte = the F00A
 * parameter); music plays on the Z80 (XGM2), effects are PCM rendered
 * offline from the arcade's own sound program (FM effects from its YM2151
 * writes, voices from its OKI ADPCM samples). The 68000 only starts them.
 *
 * PCM channels (the music has no PCM):
 *   1  shot, splash, continue countdown; Willow hurt / death above them
 *   2  the arcade's busy FM effect slot: landing, slide, text and count
 *      ticks, time warning, arrow, pickup, enemy hit, explosion
 *   3  charge sounds (0xF4 stops them), boss voices above them
 * Each channel follows the arcade's effect-slot rule: a new effect replaces
 * the current one unless the current one has a higher priority and is
 * still within its hold time (the arcade effect's own length, or the whole
 * sample for voices; the explosion holds 36 frames, its loud part). After
 * the hold any effect may cut the remaining release, as on the arcade. */
#include <genesis.h>
#include "sound.h"
#include "audio.h"

u16 sound_dbg[4];          /* measurement: playing, driver frame counter, Z80 load, charge channel busy */

void sound_debug(void)
{
    sound_dbg[0] = XGM2_isPlaying();
    sound_dbg[1] = XGM2_getDebugFrameCounter();
    sound_dbg[2] = XGM2_getCPULoad(FALSE);
    sound_dbg[3] = XGM2_isPlayingPCM(SOUND_PCM_CH2_MSK | SOUND_PCM_CH3_MSK);   /* channels 2 (slide, landing, time) and 3 (charge) */
#ifdef SNDSEQ
    /* Measurement build (with -DSNDTEST, which calls this every frame):
     * post every converted cue on a fixed schedule; sound_seq = frame,
     * music playing, PCM channel mask, last command (read with PEEK). */
    {
        static const u16 seq[][2] =
        {
            {30, 0x0001}, {32, 0x01FA},
            {100, 0x2D}, {190, 0x2F}, {280, 0x30}, {370, 0x32}, {460, 0x33}, {550, 0x34},
            {640, 0x38}, {730, 0x42}, {820, 0x82}, {910, 0x83}, {1000, 0x8A}, {1150, 0x8C},
            {1300, 0x8D}, {1450, 0x8E},
            {1600, 0x83}, {1605, 0x20},                  /* death holds channel 1: shot refused */
            {1650, 0x33}, {1655, 0x32}, {1700, 0x32},    /* explosion holds 36 frames: hit refused, then played */
            {1750, 0x8D}, {1760, 0xF4},                  /* fire breath on channel 3 survives 0xF4 */
            {1900, 0x31}, {1920, 0xF4},                  /* charge sound stopped by 0xF4 */
            {3000, 0x10}, {3400, 0x12}, {3650, 0x14}, {3900, 0x15}, {5100, 0xF0},   /* boss music loops at 2508 */
        };
        extern u16 sound_seq[4];
        static u16 f, i;
        while (i < sizeof(seq) / sizeof(seq[0]) && seq[i][0] == f) { sound_post(seq[i][1]); sound_seq[3] = seq[i][1]; i++; }
        sound_seq[0] = f++;
        sound_seq[1] = XGM2_isPlaying();
        sound_seq[2] = XGM2_isPlayingPCM(SOUND_PCM_CH1_MSK | SOUND_PCM_CH2_MSK | SOUND_PCM_CH3_MSK);
    }
#endif
}
#ifdef SNDSEQ
u16 sound_seq[4];
#endif

extern const u8 drv_xgm2[];
u16 sound_upload_check(u16 len)
{
    /* Measurement: bytes of Z80 RAM that differ from the driver image. */
    vu8 *z = (vu8 *)Z80_RAM;
    u16 i, bad = 0;
    Z80_requestBus(TRUE);
    for (i = 0; i < len; i++) if (z[i] != drv_xgm2[i]) bad++;
    Z80_releaseBus();
    return bad;
}

typedef struct { u8 cmd, ch, prio, hold; const u8 *pcm; u32 len; u8 half; } Sfx;

/* Whole sample length in 60 Hz frames (13,300 Hz, 8-bit; the OKI voices
 * 6,650 Hz, played at XGM2's half rate: res/audio.res). */
#define FULL(s) ((u8)(((sizeof(s) * 60UL) / 13300) > 255 ? 255 : ((sizeof(s) * 60UL) / 13300)))
#define FULLH(s) ((u8)(((sizeof(s) * 60UL) / 6650) > 255 ? 255 : ((sizeof(s) * 60UL) / 6650)))
#define E(c, ch, p, h, s) { c, ch, p, h, s, sizeof(s), 0 }
#define EH(c, ch, p, h, s) { c, ch, p, h, s, sizeof(s), 1 }

/* hold: arcade effect length (opmlog end tick x 60 / 249.7) or FULL. */
extern const u8 music_stage6[], music_tower6[], music_kael[], music_bav[], music_ending[];   /* src/s6data.s */

static const Sfx sfx_table[] =
{
    E(SND_SHOT,          SOUND_PCM_CH1,  4, 26, sfx_shot),
    E(SND_SWORD,         SOUND_PCM_CH1,  4, 16, sfx_sword),
    E(0x21,              SOUND_PCM_CH1,  5, 89, sfx_spell21),
    E(0x22,              SOUND_PCM_CH1,  5, 25, sfx_spell22),
    E(0x23,              SOUND_PCM_CH1,  5, 127, sfx_spell23),
    E(0x24,              SOUND_PCM_CH1,  5, 93, sfx_spell24),
    E(0x25,              SOUND_PCM_CH1,  5, 35, sfx_spell25),
    E(SND_SINK,          SOUND_PCM_CH1,  4, 27, sfx_splash),
    E(SND_CONTINUE_TICK, SOUND_PCM_CH1,  4, 33, sfx_countdown),
    EH(SND_HURT,          SOUND_PCM_CH1, 12, FULLH(sfx_hurt), sfx_hurt),
    EH(SND_DEATH,         SOUND_PCM_CH1, 14, FULLH(sfx_death), sfx_death),
    EH(SND_MAD_HURT,      SOUND_PCM_CH1, 12, FULLH(sfx_mad_hurt), sfx_mad_hurt),
    EH(SND_MAD_DEATH,     SOUND_PCM_CH1, 14, FULLH(sfx_mad_death), sfx_mad_death),
    E(0x37,              SOUND_PCM_CH2,  3, 17, sfx_flame),
    E(0x3A,              SOUND_PCM_CH2,  6, 18, sfx_crash),
    EH(0x8B,              SOUND_PCM_CH3,  6, FULLH(sfx_throw), sfx_throw),
    EH(0x90,              SOUND_PCM_CH3,  6, FULLH(sfx_board), sfx_board),
    EH(0xA0,              SOUND_PCM_CH3,  7, FULLH(sfx_lurker), sfx_lurker),
    EH(0xA1,              SOUND_PCM_CH3,  7, FULLH(sfx_crash_voice), sfx_crash_voice),
    E(SND_LAND,          SOUND_PCM_CH2,  2,  8, sfx_land),
    E(SND_SLIDE,         SOUND_PCM_CH2,  2, 17, sfx_slide),
    E(SND_TEXT,          SOUND_PCM_CH2,  2,  7, sfx_text),
    E(SND_COUNT,         SOUND_PCM_CH2,  2,  5, sfx_count),
    E(SND_TIME_WARN,     SOUND_PCM_CH2,  3, 16, sfx_time),
    E(SND_ARROW,         SOUND_PCM_CH2,  4, 23, sfx_arrow),
    E(SND_PICKUP,        SOUND_PCM_CH2,  5, 14, sfx_pickup),
    E(SND_ENEMY_HIT,     SOUND_PCM_CH2,  6, 14, sfx_enemy_hit),
    E(SND_EXPLODE,       SOUND_PCM_CH2,  7, 36, sfx_explode),
#ifndef NOCHARGESND        /* measurement build: charge sounds off */
    E(SND_CHARGE,        SOUND_PCM_CH3,  3, FULL(sfx_charge), sfx_charge),
    E(SND_CHARGED,       SOUND_PCM_CH3,  3, 85, sfx_charged),
#endif
    EH(SND_ROAR,          SOUND_PCM_CH3,  8, FULLH(sfx_roar), sfx_roar),
    EH(SND_FIRE,          SOUND_PCM_CH3,  8, FULLH(sfx_fire), sfx_fire),
    EH(SND_SORCERER,      SOUND_PCM_CH3, 10, FULLH(sfx_sorcerer), sfx_sorcerer),
    EH(SND_BOSS_DEAD,     SOUND_PCM_CH3, 10, FULLH(sfx_boss_dead), sfx_boss_dead),
    /* Stage 3 (stage3.md 9): bomb burst, lance thrust / knight hit, thunder, boss cues. */
    E(0x3E,              SOUND_PCM_CH2,  5, 30, sfx_bomb),
    EH(0x91,              SOUND_PCM_CH3,  6, FULLH(sfx_thrust), sfx_thrust),
    EH(0x92,              SOUND_PCM_CH3,  6, FULLH(sfx_knight_hit), sfx_knight_hit),
    EH(0x8F,              SOUND_PCM_CH3,  5, FULLH(sfx_thunder), sfx_thunder),
    EH(0x93,              SOUND_PCM_CH3,  8, FULLH(sfx_summon), sfx_summon),
    EH(0x94,              SOUND_PCM_CH3,  8, FULLH(sfx_serpents), sfx_serpents),
    EH(0x95,              SOUND_PCM_CH3, 10, FULLH(sfx_boss3_dead), sfx_boss3_dead),
    EH(0x81,              SOUND_PCM_CH3, 10, FULLH(sfx_logo), sfx_logo),
    /* Stage 4 (stage4.md 9). */
    E(0x28,              SOUND_PCM_CH2,  6, 27, sfx_clash),
    E(0x39,              SOUND_PCM_CH2,  3, 30, sfx_hoof),
    EH(0x96,              SOUND_PCM_CH3,  6, FULLH(sfx_slash), sfx_slash),
    EH(0x97,              SOUND_PCM_CH3,  6, FULLH(sfx_thrust4), sfx_thrust4),
    EH(0x98,              SOUND_PCM_CH3,  9, FULLH(sfx_sorsha), sfx_sorsha),
    EH(0x99,              SOUND_PCM_CH3,  8, FULLH(sfx_breath), sfx_breath),
    EH(0x9A,              SOUND_PCM_CH3, 10, FULLH(sfx_dragon_dead), sfx_dragon_dead),
    EH(0x9E,              SOUND_PCM_CH3,  8, FULLH(sfx_crash4), sfx_crash4),
    EH(0x9F,              SOUND_PCM_CH3,  5, FULLH(sfx_shield_jump), sfx_shield_jump),   /* the attract's logo (arcade_frontend.md 3.1) */
    /* Stage 5 (stage5.md 9). */
    EH(0x9B,              SOUND_PCM_CH3,  6, FULLH(sfx_troll_hit), sfx_troll_hit),
    EH(0x9C,              SOUND_PCM_CH3,  7, FULLH(sfx_troll_dead), sfx_troll_dead),
    EH(0x9D,              SOUND_PCM_CH3,  5, FULLH(sfx_orb), sfx_orb),
    EH(0xA8,              SOUND_PCM_CH3,  6, FULLH(sfx_pillar), sfx_pillar),
    /* Stage 6 and the ending (stage6.md 9). */
    EH(0xA2,              SOUND_PCM_CH3,  6, FULLH(sfx_kael_slash), sfx_kael_slash),
    EH(0xA6,              SOUND_PCM_CH3,  7, FULLH(sfx_bav_cast), sfx_bav_cast),
    EH(0xA9,              SOUND_PCM_CH3,  7, FULLH(sfx_bav_ball), sfx_bav_ball),
    EH(0x86,              SOUND_PCM_CH3, 10, FULLH(sfx_chime86), sfx_chime86),
    EH(0x87,              SOUND_PCM_CH3, 10, FULLH(sfx_chime87), sfx_chime87),
    EH(0x88,              SOUND_PCM_CH3, 10, FULLH(sfx_chime88), sfx_chime88),
    EH(0x89,              SOUND_PCM_CH3, 10, FULLH(sfx_chime89), sfx_chime89),
};

/* Per channel (index = SOUND_PCM_CHn): current effect, priority, hold end. */
static u8 ch_cmd[4], ch_prio[4];
static u32 ch_until[4];

void sound_init(void)
{
    Z80_loadDriver(Z80_DRIVER_XGM2, TRUE);
}

static void play_sfx(const Sfx *s)
{
    const u32 now = vtimer;
    const u8 ch = s->ch;
    if (now < ch_until[ch] && s->prio < ch_prio[ch]) return;   /* busy with a more important effect */
    /* The driver's own priority test is kept neutral (always replace). */
    if (!XGM2_playPCMEx(s->pcm, s->len, ch, 8, s->half, FALSE)) return;
    ch_cmd[ch] = s->cmd; ch_prio[ch] = s->prio; ch_until[ch] = now + s->hold;
}

static void stop_charge(void)
{
    /* 0xF4 resets the arcade's FM effects (the meter reset, the death and
     * stage-end hand-offs); the music and the OKI voices play on. Only the
     * charge sounds live on channel 3 as FM effects. */
    if (ch_cmd[SOUND_PCM_CH3] != SND_CHARGE && ch_cmd[SOUND_PCM_CH3] != SND_CHARGED) return;
    XGM2_stopPCM(SOUND_PCM_CH3);
    ch_cmd[SOUND_PCM_CH3] = 0; ch_prio[SOUND_PCM_CH3] = 0; ch_until[SOUND_PCM_CH3] = 0;
}

void sound_post(u16 word)
{
    const u8 cmd = word;         /* high byte: F00A parameter (used by FA/FB only) */
    u16 i;
    switch (cmd)
    {
    case SND_MUSIC_BOSS:     XGM2_play(music_boss); return;     /* its 0x01FA fade-in is in the data */
    case SND_MUSIC_CLEAR:    XGM2_play(music_clear); return;
    case SND_MUSIC_MAP:      XGM2_play(music_map); return;
    case SND_MUSIC_STAGE1:   XGM2_play(music_stage1); return;
    case SND_MUSIC_OVER:     XGM2_play(music_over); return;
    case SND_MUSIC_CONTINUE: XGM2_play(music_continue); return;
    case SND_MUSIC_SHOP:     XGM2_play(music_shop); return;
    case SND_MUSIC_STAGE2:   XGM2_play(music_stage2); return;
    case SND_MUSIC_CHASE:    XGM2_play(music_chase); return;
    case SND_MUSIC_STAGE3:   XGM2_play(music_stage3); return;
    case SND_MUSIC_LAKE:     XGM2_play(music_lake); return;
    case SND_MUSIC_BOSS3:    XGM2_play(music_boss3); return;
    case SND_MUSIC_RANK:     XGM2_play(music_rank); return;
    case SND_MUSIC_STAGE4:   XGM2_play(music_stage4); return;
    case SND_MUSIC_RIDE:     XGM2_play(music_ride); return;
    case SND_MUSIC_CAVE:     XGM2_play(music_cave); return;
    case SND_MUSIC_NAME:     XGM2_play(music_name); return;
    case SND_MUSIC_STAGE5:   XGM2_play(music_stage5); return;
    case SND_MUSIC_BOSS5:    XGM2_play(music_boss5); return;
    case SND_MUSIC_STAGE6:   XGM2_play(music_stage6); return;
    case SND_MUSIC_TOWER6:   XGM2_play(music_tower6); return;
    case SND_MUSIC_KAEL:     XGM2_play(music_kael); return;
    case SND_MUSIC_BAV:      XGM2_play(music_bav); return;
    case SND_MUSIC_ENDING:   XGM2_play(music_ending); return;
    case SND_STOP:           XGM2_stop(); stop_charge(); return;  /* Z80 030E: music off + effect reset */
#ifndef NOCHARGESND
    case SND_CHARGE_END:     stop_charge(); return;
#endif
    default: break;
    }
    for (i = 0; i < sizeof(sfx_table) / sizeof(sfx_table[0]); i++)
        if (sfx_table[i].cmd == cmd) { play_sfx(&sfx_table[i]); return; }
    /* not converted yet (FA/FB fades and the other cues): ignored */
}
