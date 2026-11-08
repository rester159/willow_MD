#ifndef SOUND_H
#define SOUND_H
#include <genesis.h>

/* Arcade sound command numbers (specs/audio.md section 2,
 * specs/audio_stage1.md). */
#define SND_MUSIC_BOSS 0x01      /* boss music ("music word 1", then 0x01FA) */
#define SND_MUSIC_CLEAR 0x10     /* stage clear jingle */
#define SND_MUSIC_MAP 0x12       /* world map / stage entry */
#define SND_MUSIC_STAGE1 0x13
#define SND_MUSIC_OVER 0x14      /* TIME OVER, GAME OVER */
#define SND_MUSIC_CONTINUE 0x15  /* continue screen */
#define SND_MUSIC_STAGE2 0x02   /* stage 2 (then 0x01FA) */
#define SND_MUSIC_CHASE 0x03    /* the wagon chase (then 0x01FA) */
#define SND_MUSIC_STAGE3 0x04   /* stage 3, the island (then 0x01FA) */
#define SND_MUSIC_LAKE 0x05     /* stage 3, the lake (then 0x01FA) */
#define SND_MUSIC_BOSS3 0x06    /* stage 3's boss */
#define SND_MUSIC_STAGE4 0x07   /* stage 4, Sorsha's camp (then 0x01FA) */
#define SND_MUSIC_RIDE 0x08     /* stage 4, the shield ride */
#define SND_MUSIC_CAVE 0x09     /* stage 4, the ice cave (then 0x01FA) */
#define SND_MUSIC_STAGE5 0x0A   /* stage 5, Tir Asleen (then 0x01FA) */
#define SND_MUSIC_BOSS5 0x0B    /* stage 5, Eborsisk (event 13; checkpoint 18) */
#define SND_MUSIC_STAGE6 0x0C   /* stage 6, areas 0-3 (then 0x01FA) */
#define SND_MUSIC_TOWER6 0x0D   /* stage 6, the tower */
#define SND_MUSIC_KAEL 0x0E     /* stage 6, Kael's bridge */
#define SND_MUSIC_BAV 0x0F      /* stage 6, Bavmorda's chamber */
#define SND_MUSIC_ENDING 0x1D   /* the ending */
#define SND_MUSIC_RANK 0x16     /* ranking after the name entry */
#define SND_MUSIC_NAME 0x18     /* name entry */
#define SND_MUSIC_SHOP 0x1B     /* shop ("music word 1B", then 0x02FA) */
#define SND_SWORD 0x26           /* Madmartigan's sword */
#define SND_SHOT 0x20
#define SND_SLIDE 0x2A
#define SND_LAND 0x2B
#define SND_PICKUP 0x2D
#define SND_TEXT 0x2F            /* text typing tick, per character */
#define SND_COUNT 0x30           /* results bonus count tick */
#define SND_CHARGE 0x31          /* hold reached 16 */
#define SND_ENEMY_HIT 0x32
#define SND_EXPLODE 0x33
#define SND_ARROW 0x34
#define SND_SINK 0x38            /* splash */
#define SND_SPLASH SND_SINK
#define SND_CONTINUE_TICK 0x42   /* continue countdown digit */
#define SND_TIME_WARN 0x43       /* under 0:10, every 32 frames */
#define SND_CHARGED 0x44         /* full hold, every 16 frames */
#define SND_HURT 0x82            /* OKI voice: Willow hurt */
#define SND_DEATH 0x83           /* OKI voice: Willow death */
#define SND_MAD_HURT 0x84        /* OKI voice: Madmartigan hurt */
#define SND_MAD_DEATH 0x85       /* OKI voice: Madmartigan death */
#define SND_ROAR 0x8A            /* OKI voice: beast roar */
#define SND_SORCERER 0x8C        /* OKI voice: sorcerer appears */
#define SND_FIRE 0x8D            /* OKI voice: fire breath */
#define SND_BOSS_DEAD 0x8E       /* OKI voice: boss killed */
#define SND_STOP 0xF0            /* music and effects stop (life loss) */
#define SND_CHARGE_END 0xF4      /* meter reset */
#define SND_FADE 0xFA            /* posted as 0x01FA / 0x02FA after music */

void sound_init(void);    /* loads the Z80 driver (XGM2) */
void sound_post(u16 cmd);
void sound_debug(void);

#endif
