#include "../src/willow_arcade_opening_scene_run.h"
#include "../src/willow_arcade_sprite_writer.h"
#include "../src/willow_native_task_runtime.h"
#include "../src/willow_native_palette_words.h"
static uint16_t rd(uint16_t at){return *(volatile uint16_t*)(0xff0000UL+at);}
static uint8_t rb(uint16_t at){return *(volatile uint8_t*)(0xff0000UL+at);}
static void wr(uint16_t at,uint16_t v){*(volatile uint16_t*)(0xff0000UL+at)=v;}
static void wb(uint16_t at,uint8_t v){*(volatile uint8_t*)(0xff0000UL+at)=v;}
static uint8_t*resolve(uint16_t at){return (uint8_t*)(0xff0000UL+at);}
static uint16_t source(uint32_t at){uint16_t v;__asm__ volatile("move.w (%1),%0":"=d"(v):"a"(at):"memory");return v;}
static uint16_t actorWord(unsigned long at){uint16_t v;__asm__ volatile("move.w (%1),%0":"=d"(v):"a"(at):"memory");return v;}
static uint8_t sourceByte(uint32_t at){uint8_t v;__asm__ volatile("move.b (%1),%0":"=d"(v):"a"(at):"memory");return v;}
static uint16_t colorRead(uint32_t at){return rd(0x2000+at-0x91c000);}
static void colorWrite(uint32_t at,uint16_t v){wr(0x2000+at-0x91c000,v);}
static void adjustPaletteGroup(uint32_t at,uint16_t delta){
 willowNativePaletteAdjustWords((volatile uint16_t*)(0xff2000UL+at-0x91c000UL),delta);
}
static void mapWrite(uint16_t at,uint16_t v){(void)at;(void)v;}
static uint16_t objectAddress(uint32_t at){return at<0x908000?0x4000+at-0x900000:0x6000+at-0x908000;}
static void objectWrite(uint32_t at,uint16_t v){wr(objectAddress(at),v);}
static void videoWrite(uint32_t at,uint16_t v){
 if((at>=0x900000&&at<0x900800)||(at>=0x908000&&at<0x908800))objectWrite(at,v);
}
static void release(uint8_t*r){
 for(uint16_t n=0;n<6;n++)r[n]=0;
 for(uint16_t n=14;n<20;n++)r[n]=0;
 r[31]=r[32]=r[33]=r[36]=r[46]=r[47]=0;
 uint16_t next=rd(0x981c)-2;wr(next,(uint32_t)r);wr(0x981c,next);wb(0x9801,rb(0x9801)+1);
}
static uint16_t spriteWord(uint32_t at){return (at&0xff0000)==0xff0000?rd(at):source(at);}
static void emit(uint16_t x,uint16_t y,uint16_t code,uint16_t attr){
 uint16_t at=rd(0xb20);wr(at,x);wr(at+2,y);wr(at+4,code);wr(at+6,attr);wr(0xb20,at+8);
}
static void ordinary(uint16_t handle){
 WillowArcadeSpriteWriter w={spriteWord,emit,rd(0x829a),rd(0xb22)};
 willowArcadeSprite(&w,resolve(handle),rd(0x8408),rd(0x840c));wr(0x829a,w.areaBudget);wr(0xb22,w.pieceBudget);
}
static void stage4(uint16_t handle){
 WillowArcadeSpriteWriter w={spriteWord,emit,rd(0x829a),rd(0xb22)};
 willowArcadeStage4Sprite(&w,resolve(handle),rd(0x8408),rd(0x840c));wr(0x829a,w.areaBudget);wr(0xb22,w.pieceBudget);
}
static void draw(WillowArcadeSpriteFrame*f){
 const WillowVisibilityBus b={rd,rb,wr,wb};uint16_t start=objectAddress(f->cursor);
 wr(0xb20,start);wr(0xb22,f->remaining);willowArcadeVisibilityRender(&b,ordinary,stage4);
 f->cursor+=rd(0xb20)-start;f->remaining=rd(0xb22);
}
/* Source pool bases are even; the 68000 permits long stores at even
 * addresses. Preserve ascending word order with unrolled long stores. */
static void zeroPool(const WillowVisibilityBus*b,uint16_t at,uint16_t bytes){
 (void)b;uint8_t*p=(uint8_t*)(0xff0000UL+at);
 if(bytes>=16){
  uint16_t groups=(bytes>>4)-1;
  __asm__ volatile(
   "moveq #0,%%d0\n\tmoveq #0,%%d1\n\tmoveq #0,%%d2\n\tmoveq #0,%%d3\n"
   "1:\n\tmovem.l %%d0-%%d3,(%0)\n\tlea 16(%0),%0\n\tdbra %1,1b"
   :"+a"(p),"+d"(groups)::"d0","d1","d2","d3","memory","cc");
  bytes&=15;
 }
 while(bytes){*(volatile uint16_t*)p=0;p+=2;bytes-=2;}
}
/* Copy full CPS words; Genesis palette fitting happens after this state. */
static void copyPalette(uint32_t from,uint32_t to,uint16_t words){
 const volatile uint32_t*s=(const volatile uint32_t*)from;
 volatile uint32_t*d=(volatile uint32_t*)(0xff2000UL+to-0x91c000UL);
 while(words>=16){d[0]=s[0];d[1]=s[1];d[2]=s[2];d[3]=s[3];d[4]=s[4];d[5]=s[5];d[6]=s[6];d[7]=s[7];d+=8;s+=8;words-=16;}
 while(words>=2){*d++=*s++;words-=2;}
 if(words)*(volatile uint16_t*)d=*(const volatile uint16_t*)s;
}
void openingRuntimeProbe(void){
 const WillowVisibilityBus b={rd,rb,wr,wb};WillowPaletteAnimation g;
 g.byte=sourceByte;g.word=source;g.read=colorRead;g.write=colorWrite;
 WillowArcadeSpriteFrame out;
 if(!rd(0x8004)&&!rd(0x8008)&&!rd(0x800c)){
  if(!willowArcadeAttractSceneInitWithTransfers(&b,source,colorWrite,zeroPool,copyPalette)){wr(0xb24,1);return;}
 }else if(rd(0x8004)==6&&!rd(0x8008)){
  if(!willowArcadeAttractDemoInitWithZero(&b,source,colorWrite,zeroPool)){wr(0xb24,1);return;}
 }else if(!willowArcadeOpeningSceneRun(&b,resolve,&g,actorWord,sourceByte,source,colorWrite,mapWrite,mapWrite,release,draw,objectWrite,videoWrite,&out)){wr(0xb24,1);return;}
 uint16_t cursor=0,remaining=15;
 for(;;){
  WillowNativeTaskRun r=willowNativeTaskRunOneWithVideo((WillowNativeTaskRuntime*)0xff0b00UL,&b,cursor,remaining,source,colorRead,colorWrite,adjustPaletteGroup,0,videoWrite);
  if(r.unsupported){wr(0xb24,2);return;}
  if(!r.dispatched)break;
  if(r.remaining==65535)break;
  cursor=r.cursor;remaining=r.remaining;
 }
 willowArcadeTaskWake(&b);wr(0xb24,0x4f50);
}
/* Isolated target verification of all input words and both fade directions. */
void paletteGroupProbe(void){
 willowNativePaletteAdjustWords((volatile uint16_t*)0xff2000UL,rd(0xb28));
}
