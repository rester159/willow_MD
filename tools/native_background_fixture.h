#ifndef WILLOW_NATIVE_BACKGROUND_FIXTURE_H
#define WILLOW_NATIVE_BACKGROUND_FIXTURE_H
/* Source-coordinate ROM lookup is the oracle; captured maps are input only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/willow_native_background_map.h"
#include "../native/intro_probe/src/scroll2_data.h"
#include "../native/intro_probe/src/scroll3_data.h"
#include "../native/intro_probe/src/background_pairs.h"
static uint8_t rom[0x100000];
static uint16_t near[4096],far[WILLOW_SCROLL3_MAP_WORDS];
static uint8_t baseline[17670],walk[40198],globals[1286],distant[16384];
static uint16_t idOf(uint32_t pair,const uint32_t*d,unsigned count){
 unsigned low=0,high=count;
 while(low<high){unsigned mid=(low+high)/2;if(d[mid]<pair)low=mid+1;else high=mid;}
 assert(low<count && d[low]==pair);return low;
}
static uint32_t sourcePair(unsigned layer,uint16_t x,uint16_t y){
 unsigned pointers=layer==2?0x2dbf8:0x2eb24,modes=layer==2?0x2dc1c:0x2eb44;
 unsigned mode=rom[modes],index;
 switch(mode){
 case 0:index=((y&0xf00)>>4)|((x>>8)&15);break;
 case 1:index=((y&0x700)>>3)|((x>>8)&31);break;
 case 2:index=((y&0x1f00)>>2)|((x>>8)&63);break;
 default:index=(y&0x100)|(x>>8);break;
 }
 unsigned chunk=rom[willowMotionLong(rom+pointers)+index];
 unsigned size=layer==2?16:32;
 unsigned at=(layer==2?0x80000:0xc0000)+chunk*(layer==2?1024:256)+
   (((x%256)/size)*(256/size)+(y%256)/size)*4;
 unsigned code=willowMotionWord(rom+at);
 if(layer==3)code=(code+0xa00)&65535;
 return ((uint32_t)code<<16)|willowMotionWord(rom+at+2);
}

#endif
