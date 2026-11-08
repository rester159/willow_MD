#ifdef WILLOW_TEST_GAMEPLAY_MAIN
#define WILLOW_TEST_GAMEPLAY_ROUTED
#include "../src/willow_native_gameplay_flow.h"
WillowNativeGameplayFlow gameplayProbeFlow;
#endif
/* Target-CPU integration diagnostic. No GUI, VDP or replacement gameplay.
 * Original source-layout state occupies work RAM. Independent graphics and
 * diagnostic globals occupy cartridge SRAM; a reserved 8 KiB text-page tail
 * supplies the C stack. The harness checks that source execution never writes
 * that reserved stack range. This is not the shipping runtime memory layout. */
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_stage0.h"
#include "../src/willow_arcade_player_bus.h"
#include "../src/willow_arcade_gameplay_frame.h"
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
#include "../src/willow_arcade_gameplay_services.h"
WillowArcadeEventRouteTask gameplayProbeRoute;
#endif
#include "../src/willow_arcade_sprite_writer.h"
static uint16_t rw(uint16_t at){return *(volatile uint16_t*)(0xff0000UL+at);}
static uint8_t rb(uint16_t at){return *(volatile uint8_t*)(0xff0000UL+at);}
static void ww(uint16_t at,uint16_t v){*(volatile uint16_t*)(0xff0000UL+at)=v;}
static void wb(uint16_t at,uint8_t v){*(volatile uint8_t*)(0xff0000UL+at)=v;}
static uint8_t*resolve(uint16_t at){return (uint8_t*)(0xff0000UL+at);}
static const WillowVisibilityBus bus={rw,rb,ww,wb};
static uint16_t sourceWord(unsigned long at){if(at<0x100000)at+=0x300000;uint16_t v;__asm__ volatile("move.w (%1),%0":"=d"(v):"a"(at):"memory");return v;}
static uint16_t inputWord(uint32_t at){return sourceWord(at);}
static uint8_t sourceByte(uint32_t at){if(at<0x100000)at+=0x300000;uint8_t v;__asm__ volatile("move.b (%1),%0":"=d"(v):"a"(at):"memory");return v;}
static uint16_t paletteWord(uint32_t at){return sourceWord(at);}
static uint8_t paletteByte(uint32_t at){return sourceByte(at);}
static uint8_t attribute(uint16_t at){return *(volatile uint8_t*)(0x204002UL+at);}
static void textWrite(uint32_t at,uint16_t v){if(at>=0x91c000)ww(0x2000+at-0x91c000,v);else ww(0x4000+at-0x910000,v);}
static uint16_t colorRead(uint32_t at){return rw(0x2000+at-0x91c000);}
static void colorWrite(uint32_t at,uint16_t v){ww(0x2000+at-0x91c000,v);}
static uint8_t*allocateWithA4(uint32_t*a4){uint16_t at=willowArcadePaletteFreeWithA4(&bus,a4);return at?resolve(at):0;}
static uint8_t*allocate(void){return allocateWithA4(0);}
static uint8_t randomByte(void){return willowActor18Random(&bus);}
static void soundWithA4(uint16_t v,uint32_t*a4){willowBreakableSoundWithA4(&bus,v,a4);}
static void sound(uint16_t v){soundWithA4(v,0);}
static void videoWrite(uint32_t at,uint16_t v){*(volatile uint16_t*)(0x204000UL+at-0x918000UL)=v;}
static uint32_t *liveA4;

static unsigned availableEffects(void){return rb(0x97fe);}
static uint8_t*playerAllocate(uint8_t type){
 uint16_t count=type==2?0x97fe:type==8?0x9807:0x980d;
 uint16_t cursor=type==2?0x9816:type==8?0x9828:0x9834;
 if(!rb(count))return 0;
 uint16_t stack=rw(cursor),at=rw(stack);ww(cursor,stack+2);wb(count,rb(count)-1);
 *liveA4=(uint32_t)(int32_t)(int16_t)at;return resolve(at);
}
static uint8_t*playerEffect(void){return playerAllocate(2);}
static uint8_t*playerClass12(void){return playerAllocate(12);}
static void playerSound(uint16_t cmd){soundWithA4(cmd,liveA4);}
static void playerCancel(uint16_t at){willowArcadeCancelTaskWithA4(resolve(at),liveA4);}
static uint16_t playerColor(unsigned n){return rw(0x2000+n*2);}
static void playerColorWrite(unsigned n,uint16_t v){ww(0x2000+n*2,v);}
void*memcpy(void*,const void*,unsigned long);
static void loadPlayerRecord(WillowPlayerRecord*p){memcpy(p->bytes,resolve(0x9846),128);}
static void storePlayerRecord(const WillowPlayerRecord*p){memcpy(resolve(0x9846),p->bytes,128);}
static uint8_t playerUpdate(void*context,uint16_t*d4,uint32_t*a4){
 WillowPlayerCollision*c=context;WillowPlayerRecord p={0};
 WillowPlayerInput i={0};i.availableEffects=availableEffects;i.allocateEffect=playerEffect;i.random=randomByte;i.sound=playerSound;
 WillowPlayerShots shots={0};shots.allocate=playerAllocate;
 WillowPlayerHealth h={0};h.paletteRead=playerColor;h.paletteWrite=playerColorWrite;h.textWrite=textWrite;h.sound=playerSound;h.savedD4=d4;
 WillowPlayerCharge q={0};q.textWrite=textWrite;q.sound=playerSound;
 WillowPlayerScript t={0};t.cancelTask=playerCancel;liveA4=a4;
 return willowArcadeWillowControllerWithRecordIO(&bus,&p,&i,c,&shots,&h,&q,&t,playerClass12,loadPlayerRecord,storePlayerRecord);
}

uint16_t gameplayProbeD4,gameplayProbeStatus;
uint32_t gameplayProbeA4,gameplayProbeCursor;
uint16_t gameplayProbeBudget;
WillowArcadeGameplayActors gameplayProbeTask;
static uint32_t outputCursor;static uint16_t outputBudget;
static void writeMap2(uint16_t at,uint16_t v){*(volatile uint16_t*)(0x204000UL+at)=v;}
static void writeMap3(uint16_t at,uint16_t v){*(volatile uint16_t*)(0x20c000UL+at)=v;}
static void spriteWord(uint32_t at,uint16_t v){*(volatile uint16_t*)(at-0x700000UL)=v;}
static void emitPiece(uint16_t x,uint16_t y,uint16_t code,uint16_t attr){spriteWord(outputCursor,x);spriteWord(outputCursor+2,y);spriteWord(outputCursor+4,code);spriteWord(outputCursor+6,attr);outputCursor+=8;}
static void drawOrdinary(uint16_t at){WillowArcadeSpriteWriter w={inputWord,emitPiece,rw(0x829a),outputBudget};willowArcadeSpriteWithD4(&w,resolve(at),rw(0x8408),rw(0x840c),&gameplayProbeD4);ww(0x829a,w.areaBudget);outputBudget=w.pieceBudget;}
static void drawStage4(uint16_t at){WillowArcadeSpriteWriter w={inputWord,emitPiece,rw(0x829a),outputBudget};willowArcadeStage4SpriteWithD4(&w,resolve(at),rw(0x8408),rw(0x840c),&gameplayProbeD4);ww(0x829a,w.areaBudget);outputBudget=w.pieceBudget;}
static void drawFrame(WillowArcadeSpriteFrame*f){outputCursor=f->cursor;outputBudget=f->remaining;willowArcadeVisibilityRenderWithA4(&bus,drawOrdinary,drawStage4,&gameplayProbeA4);f->cursor=outputCursor;f->remaining=outputBudget;}
#ifdef WILLOW_TEST_GAMEPLAY_MAIN
/* Keep fixed ROM/RAM accessors visible at the service boundary. Passing a
 * mutable service-table pointer through the whole resumable loop prevents GCC
 * from specializing actor and stage scans for the Genesis memory layout. */
/* Copy callbacks receive contiguous original tile/attribute pairs. Ring
 * and chunk boundaries remain controlled by the shared scroll routines. */
static uint8_t copyMap2(uint16_t destination,uint32_t source,uint16_t pairs){
 memcpy((void*)(0x204000UL+destination),(const void*)(0x300000UL+source),(unsigned long)pairs*4);
 return 1;
}
static uint8_t copyMap3(uint16_t destination,uint32_t source,uint16_t pairs){
 /* Big-endian longword: code occupies the high half. Adding only to that
  * half preserves attributes and the code's original 16-bit wrap. */
 volatile uint32_t*out=(volatile uint32_t*)(0x20c000UL+destination);
 const volatile uint32_t*in=(const volatile uint32_t*)(0x300000UL+source);
 if(pairs){
  uint16_t remaining=pairs-1;uint32_t value;
  __asm__ volatile("1: move.l (%1)+,%0\n\taddi.l #0x0a000000,%0\n\t"
                   "move.l %0,(%2)+\n\tdbra %3,1b"
                   :"=&d"(value),"+&a"(in),"+&a"(out),"+&d"(remaining)::"memory","cc");
 }
 return 1;
}
typedef struct {WillowPaletteAnimation*palette;WillowPlayerCollision*collision;WillowArcadeSpriteFrame*frame;} MainProbeServices;
static uint8_t mainProbeService(void*context,uint32_t address,int argument){
 MainProbeServices*s=context;
 WillowArcadeGameplayServices services={.bus=&bus,.resolve=resolve,.word=sourceWord,
  .busWord=inputWord,.byte=sourceByte,.palette=s->palette,.collision=s->collision,.actors=&gameplayProbeTask.actors,
  .videoWrite=videoWrite,.textWrite=textWrite,.d4=&gameplayProbeD4,.a4=&gameplayProbeA4,
  .player=playerUpdate,.playerContext=s->collision,.write2=writeMap2,.write3=writeMap3,
  .copy2=copyMap2,.copy3=copyMap3,
  .draw=drawFrame,.spriteWord=spriteWord,.frame=s->frame};
 return willowArcadeGameplayServiceResume(&services,address,argument);
}
#endif
void gameplayRuntimeProbe(void){
 WillowPaletteAnimation g={0};g.byte=paletteByte;g.word=paletteWord;g.read=colorRead;g.write=colorWrite;g.allocate=allocate;g.random=randomByte;g.sound=sound;g.allocateWithA4=allocateWithA4;g.soundWithA4=soundWithA4;
 WillowPlayerCollision c={sourceWord,attribute,0,0,0,0};WillowArcadeSpriteFrame frame;
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
 frame.cursor=gameplayProbeCursor;frame.remaining=gameplayProbeBudget;
 WillowArcadeGameplayServices services={.bus=&bus,.resolve=resolve,.word=sourceWord,
  .busWord=inputWord,.byte=sourceByte,.palette=&g,.collision=&c,.actors=&gameplayProbeTask.actors,
  .videoWrite=videoWrite,.textWrite=textWrite,.d4=&gameplayProbeD4,.a4=&gameplayProbeA4,
  .player=playerUpdate,.playerContext=&c,.write2=writeMap2,.write3=writeMap3,
  .draw=drawFrame,.spriteWord=spriteWord,.frame=&frame};
#ifdef WILLOW_TEST_GAMEPLAY_MAIN
 if(!gameplayProbeFlow.task.pc)gameplayProbeFlow.task.pc=0x92d8;
 MainProbeServices mainServices={&g,&c,&frame};
 WillowNativeGameplayFlowBinding binding={&gameplayProbeFlow,&bus,inputWord,&gameplayProbeA4,mainProbeService,&mainServices};
 (void)services;
 WillowTransitionStep tick=willowNativeGameplayFlowStep(&binding);
 gameplayProbeStatus=tick.supported&&!tick.finished&&tick.resume==0x932c&&tick.delay==1;
#else
 gameplayProbeStatus=willowArcadeEventRouteResume(&gameplayProbeRoute,0x9530,&bus,willowArcadeGameplayServiceResume,&services);
#endif
 if(gameplayProbeStatus!=1)return;
#else
 gameplayProbeStatus=willowArcadeGameplayStage0Resume(&gameplayProbeTask,&bus,resolve,sourceWord,inputWord,&g,&c,videoWrite,textWrite,0,&gameplayProbeD4,&gameplayProbeA4,playerUpdate,&c);
 if(gameplayProbeStatus!=1)return;
 if(!willowArcadeGameplayFrameWithRegisters(&bus,resolve,sourceByte,inputWord,writeMap2,writeMap3,0,0,&g,drawFrame,spriteWord,&frame,&gameplayProbeD4,&gameplayProbeA4)){gameplayProbeStatus=0;return;}
#endif
 gameplayProbeCursor=frame.cursor;gameplayProbeBudget=frame.remaining;
}
void*memset(void*p,int v,unsigned long n){unsigned char*d=p;while(n--)*d++=v;return p;}
/* Freestanding compiler-generated structure copies. Preserve arbitrary
 * alignment; the 68000 allows word/long accesses at any even address. */
void*memcpy(void*p,const void*q,unsigned long n){
 typedef uint32_t CopyLong __attribute__((__may_alias__));
 typedef uint16_t CopyWord __attribute__((__may_alias__));
 unsigned char*d=p;const unsigned char*s=q;
 if(!(((unsigned long)d^(unsigned long)s)&1)){
  if(n&&((unsigned long)d&1)){*d++=*s++;n--;}
  while(n>=16){
   /* Explicit postincrement avoids four offset-address calculations. */
   __asm__ volatile("move.l (%1)+,(%0)+\n\tmove.l (%1)+,(%0)+\n\t"
                    "move.l (%1)+,(%0)+\n\tmove.l (%1)+,(%0)+"
                    :"+&a"(d),"+&a"(s)::"memory","cc");
   n-=16;
  }
  while(n>=4){*(volatile CopyLong*)d=*(const volatile CopyLong*)s;d+=4;s+=4;n-=4;}
  if(n>=2){*(volatile CopyWord*)d=*(const volatile CopyWord*)s;d+=2;s+=2;n-=2;}
 }
 while(n--)*d++=*s++;
 return p;
}
