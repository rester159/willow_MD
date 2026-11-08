/* Minimal stand-in for Genesis Plus GX shared.h: only what ym2612.c, psg.c
 * and blip_buf.c need for the offline VGM renderer (vgmrender.c). */
#ifndef VGMRENDER_SHARED_H
#define VGMRENDER_SHARED_H
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
typedef uint8_t uint8; typedef uint16_t uint16; typedef uint32_t uint32;
typedef int8_t int8; typedef int16_t int16; typedef int32_t int32;
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int8_t INT8; typedef int16_t INT16; typedef int32_t INT32;
#ifndef INLINE
#define INLINE static inline
#endif
#include "blip_buf.h"
#include "ym2612.h"
#include "psg.h"
typedef struct { uint8 hq_psg; uint8 psg_ch_volumes[4]; uint8 md_ch_volumes[6]; } t_config;
typedef struct { blip_t *blips[3]; } t_snd;
extern t_config config; extern t_snd snd;
#define load_param(p,n) do{memcpy(p,&state[bufferptr],n);bufferptr+=n;}while(0)
#define save_param(p,n) do{memcpy(&state[bufferptr],p,n);bufferptr+=n;}while(0)
#endif
