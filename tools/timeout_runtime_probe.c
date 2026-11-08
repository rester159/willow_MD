/* Headless target adapter for manager, task-pass and wake boundaries.
 * Source-layout work RAM and diagnostic SRAM are not the shipping layout. */
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_native_task_runtime.h"
#include "../src/willow_native_palette_add.h"
#ifdef WILLOW_TEST_FOREST_SCHEDULER
#include "../src/willow_arcade_player_bus.h"
#include "../src/willow_arcade_sprite_writer.h"
void*memcpy(void*,const void*,unsigned long);
WillowArcadeActorUpdate timeoutProbeActors;
#endif
#ifdef WILLOW_TEST_INITIAL_SCHEDULER
#include "../src/willow_native_gameplay_flow.h"
WillowNativeGameplayFlow timeoutProbeFlow;
#endif
static uint16_t rw(uint16_t at){return *(volatile uint16_t*)(0xff0000UL+at);}
static uint8_t rb(uint16_t at){return *(volatile uint8_t*)(0xff0000UL+at);}
static void ww(uint16_t at,uint16_t v){*(volatile uint16_t*)(0xff0000UL+at)=v;}
static void wb(uint16_t at,uint8_t v){*(volatile uint8_t*)(0xff0000UL+at)=v;}
static const WillowVisibilityBus bus={rw,rb,ww,wb};
static uint16_t word(uint32_t at){
#ifdef WILLOW_FAST_ROM_READ
 /* Original program/data ROM occupies this entire immutable range. */
 if(at<0x100000UL)return *(const uint16_t*)(0x300000UL+at);
#endif
#ifdef WILLOW_TEST_FOREST_SCHEDULER
 if(at>=0x918000&&at<0x91c000)return *(volatile uint16_t*)(0x204000UL+at-0x918000UL);
#endif
 return (at&0xff0000UL)==0xff0000UL?rw((uint16_t)at):*(const uint16_t*)(0x300000UL+at);
}
uint16_t timeoutProbeOperation,timeoutProbeRegion,timeoutProbeStatus,timeoutProbeD4,timeoutProbeWait;
uint32_t timeoutProbeA4;
WillowArcadeEventRouteTask timeoutProbeRoute;
WillowNativeTaskRuntime timeoutProbeRuntime;
static uint16_t colorRead(uint32_t at){return rw(0x2000+at-0x91c000);}
static void colorWrite(uint32_t at,uint16_t v){ww(0x2000+at-0x91c000,v);}
static void colorAdjustGroup(uint32_t at,uint16_t delta){
 /* CPS fade groups are 16 independent wrapping words, including groups
  * whose individual brightness values differ. Preserve sequential writes. */
 volatile uint16_t*p=(volatile uint16_t*)(0xff2000UL+at-0x91c000UL);
#ifdef WILLOW_NATIVE_PALETTE_ADD
 willowNativePaletteAdd16(p,delta);
#else
 for(unsigned n=0;n<16;n++)p[n]=(uint16_t)(p[n]+delta);
#endif
}
#ifdef WILLOW_TEST_FOREST_SCHEDULER
static void paletteCopy(uint32_t source,uint32_t destination,uint16_t words){
 memcpy((void*)(0xff2000UL+destination-0x91c000UL),
        (const void*)(0x300000UL+source),(unsigned long)words*2);
}
static void colorFadeAll(WillowPaletteFade*f,uint16_t delta){
 /* Specialize only the memory binding; shared source logic still owns the
  * first-color test, group order, word arithmetic, masks and completion sum. */
 WillowPaletteFade local;
 for(unsigned n=0;n<6;n++)local.masks[n]=f->masks[n];
 local.sum=0;local.read=colorRead;local.write=colorWrite;local.adjustGroup=colorAdjustGroup;
 willowPaletteFadeAll(&local,delta);
 for(unsigned n=0;n<6;n++)f->masks[n]=local.masks[n];
 f->sum=local.sum;
}
#endif
static void videoWord(uint32_t at,uint16_t v){
 /* Actor callbacks can write CPS palette RAM as well as tile/sprite RAM. */
 if(at>=0x91c000&&at<0x91d800){colorWrite(at,v);return;}
#ifdef WILLOW_TEST_FOREST_SCHEDULER
 if(at>=0x918000&&at<0x91c000)*(volatile uint16_t*)(0x204000UL+at-0x918000UL)=v;
#endif
 static const uint32_t bases[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 uint32_t base=bases[timeoutProbeRegion];uint16_t size=timeoutProbeRegion<3?0x4000:0x800;
 if(at>=base&&at<base+size)ww(0x4000+at-base,v);
}
#ifdef WILLOW_TEST_SCENE_SCHEDULER
static uint8_t*resolve(uint16_t at){return (uint8_t*)(0xff0000UL+at);}
static uint8_t romByte(uint32_t at){return *(const uint8_t*)(0x300000UL+at);}
static void write2(uint16_t at,uint16_t value){videoWord(0x918000UL+at,value);}
static void write3(uint16_t at,uint16_t value){videoWord(0x914000UL+at,value);}
#endif
#ifdef WILLOW_TEST_FOREST_SCHEDULER
static void fillVideoWords(volatile uint16_t*p,uint16_t first,uint16_t second,uint16_t pairs){
 uint32_t value=((uint32_t)first<<16)|second;
 if(pairs>=4){
  uint16_t remaining=pairs/4-1;
  /* Big-endian longwords retain first/second ordering on the 16-bit bus. */
  __asm__ volatile("1: move.l %2,(%0)+\n\tmove.l %2,(%0)+\n\t"
                   "move.l %2,(%0)+\n\tmove.l %2,(%0)+\n\tdbra %1,1b"
                   :"+&a"(p),"+&d"(remaining):"d"(value):"memory","cc");
 }
 for(unsigned n=0;n<(pairs&3);n++){*p++=first;*p++=second;}
}
static void videoFill(uint32_t at,uint16_t first,uint16_t second,uint16_t pairs){
 static const uint32_t bases[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 unsigned region=0;while(region<5&&bases[region]!=at)region++;
 if(region==5||pairs>(region<3?4096:512)){
  willowArcadeVideoFillPairs(videoWord,0,at,first,second,pairs);return;
 }
 /* The diagnostic independently retains collision Scroll2 and one selected
  * video bank. Both receive the same complete range before returning. */
 if(region==1)fillVideoWords((volatile uint16_t*)0x204000UL,first,second,pairs);
 if(region==timeoutProbeRegion)fillVideoWords((volatile uint16_t*)0xff4000UL,first,second,pairs);
}
/* Shared scroll traversal retains chunk boundaries and CPS ring wrapping. */
static uint8_t copyMap2(uint16_t destination,uint32_t source,uint16_t pairs){
 if(pairs==1){
  uint32_t value=((uint32_t)word(source)<<16)|word(source+2);
  if(destination<=0x3ffc){
   *(volatile uint32_t*)(0x204000UL+destination)=value;
   if(timeoutProbeRegion==1)*(volatile uint32_t*)(0xff4000UL+destination)=value;
  }else{write2(destination,value>>16);write2(destination+2,value);}
  return 1;
 }
 unsigned long bytes=(unsigned long)pairs*4;
 if((unsigned long)destination+bytes>0x4000UL||source>0x100000UL||bytes>0x100000UL-source){
  for(unsigned n=0;n<pairs;n++){write2(destination+n*4,word(source+n*4));write2(destination+n*4+2,word(source+n*4+2));}
  return 1;
 }
 memcpy((void*)(0x204000UL+destination),(const void*)(0x300000UL+source),bytes);
 if(timeoutProbeRegion==1)memcpy((void*)(0xff4000UL+destination),(const void*)(0x300000UL+source),bytes);
 return 1;
}
static uint8_t copyMap3(uint16_t destination,uint32_t source,uint16_t pairs){
 unsigned long bytes=(unsigned long)pairs*4;
 if((unsigned long)destination+bytes>0x4000UL||source>0x100000UL||bytes>0x100000UL-source){
  for(unsigned n=0;n<pairs;n++){write3(destination+n*4,(uint16_t)(word(source+n*4)+0x0a00));write3(destination+n*4+2,word(source+n*4+2));}
  return 1;
 }
 if(timeoutProbeRegion==2&&pairs){
  volatile uint32_t*out=(volatile uint32_t*)(0xff4000UL+destination);
  const volatile uint32_t*in=(const volatile uint32_t*)(0x300000UL+source);
  uint16_t remaining=pairs-1;uint32_t value;
  /* Code occupies the high half: this preserves independent code wrap
   * and the unchanged low-half attribute word. */
  __asm__ volatile("1: move.l (%1)+,%0\n\taddi.l #0x0a000000,%0\n\t"
                   "move.l %0,(%2)+\n\tdbra %3,1b"
                   :"=&d"(value),"+&a"(in),"+&a"(out),"+&d"(remaining)::"memory","cc");
 }
 return 1;
}
static uint8_t attribute(uint16_t at){return *(volatile uint8_t*)(0x204002UL+at);}
static uint16_t sourceWord(unsigned long at){return (at&0xff0000UL)==0xff0000UL?rw((uint16_t)at):word(at);}
static uint8_t*allocateWithA4(uint32_t*a4){uint16_t at=willowArcadePaletteFreeWithA4(&bus,a4);return at?resolve(at):0;}
static uint8_t*allocate(void){return allocateWithA4(0);}
static uint8_t randomByte(void){return willowActor18Random(&bus);}
static void soundWithA4(uint16_t v,uint32_t*a4){willowBreakableSoundWithA4(&bus,v,a4);}
static void sound(uint16_t v){soundWithA4(v,0);}

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

static void loadPlayerRecord(WillowPlayerRecord*p){memcpy(p->bytes,resolve(0x9846),128);}
static void storePlayerRecord(const WillowPlayerRecord*p){memcpy(resolve(0x9846),p->bytes,128);}
static uint8_t playerSceneService(void*context,uint16_t address){
 return willowArcadeGameplayServiceResume(context,address,0);
}
static uint8_t playerUpdate(void*context,uint16_t*d4,uint32_t*a4){
 WillowArcadeGameplayServices services=*(WillowArcadeGameplayServices*)context;
 services.d4=d4;services.a4=a4;services.eventWait=&services.actors->playerWait;
 WillowPlayerCollision*c=services.collision;WillowPlayerRecord p={0};
 WillowPlayerInput i={0};i.availableEffects=availableEffects;i.allocateEffect=playerEffect;i.random=randomByte;i.sound=playerSound;
 WillowPlayerShots shots={0};shots.allocate=playerAllocate;
 WillowPlayerHealth h={0};h.paletteRead=playerColor;h.paletteWrite=playerColorWrite;h.textWrite=videoWord;h.sound=playerSound;h.savedD4=d4;
 WillowPlayerCharge q={0};q.textWrite=videoWord;q.sound=playerSound;
 WillowPlayerScript t={0};t.cancelTask=playerCancel;liveA4=a4;
 WillowPlayerSceneBinding scene={&services.actors->playerScene,playerSceneService,&services};
 return willowArcadePlayerControllerWithScene(&bus,&p,&i,c,&shots,&h,&q,&t,playerClass12,loadPlayerRecord,storePlayerRecord,&scene);
}
static uint16_t*renderD4;static uint32_t*renderA4;
static uint32_t outputCursor;static uint16_t outputBudget;
static void emitPiece(uint16_t x,uint16_t y,uint16_t code,uint16_t attr){videoWord(outputCursor,x);videoWord(outputCursor+2,y);videoWord(outputCursor+4,code);videoWord(outputCursor+6,attr);outputCursor+=8;}
static WillowSelectionRegister selectionRegister;
static void drawOrdinary(uint16_t at){WillowArcadeSpriteWriter w={word,emitPiece,rw(0x829a),outputBudget};willowArcadeSpriteWithRegisterTracking(&w,resolve(at),rw(0x8408),rw(0x840c),0,&selectionRegister.d2,renderD4,&selectionRegister.known);ww(0x829a,w.areaBudget);outputBudget=w.pieceBudget;}
static void drawStage4(uint16_t at){WillowArcadeSpriteWriter w={word,emitPiece,rw(0x829a),outputBudget};willowArcadeStage4SpriteWithRegisterTracking(&w,resolve(at),rw(0x8408),rw(0x840c),0,&selectionRegister.d2,renderD4,&selectionRegister.known);ww(0x829a,w.areaBudget);outputBudget=w.pieceBudget;}
static void drawFrame(WillowArcadeSpriteFrame*f){outputCursor=f->cursor;outputBudget=f->remaining;willowArcadeVisibilityRenderWithA4(&bus,drawOrdinary,drawStage4,renderA4);f->cursor=outputCursor;f->remaining=outputBudget;}
typedef struct {WillowPaletteAnimation*palette;WillowPlayerCollision*collision;WillowArcadeSpriteFrame*frame;} ForestServices;
static void poolZero(const WillowVisibilityBus*b,uint16_t at,uint16_t bytes){
 (void)b;
 volatile uint16_t*p=(volatile uint16_t*)(0xff0000UL+at);
 uint16_t blocks=bytes/16;
 if(blocks){
  uint16_t remaining=blocks-1;uint32_t zero=0;
  /* Each longword still writes its two ascending 68000 bus words. The
   * source initializer controls every record boundary and class write. */
  __asm__ volatile("1: move.l %2,(%0)+\n\tmove.l %2,(%0)+\n\t"
                   "move.l %2,(%0)+\n\tmove.l %2,(%0)+\n\tdbra %1,1b"
                   :"+&a"(p),"+&d"(remaining):"d"(zero):"memory","cc");
 }
 for(unsigned n=0;n<(bytes&15)/2;n++)*p++=0;
}
static uint8_t forestService(void*context,uint32_t address,int argument){
 ForestServices*s=context;
 const WillowTransitionPresentationBus presentation={romByte,word,colorWrite,write2};
 /* Keep the fixed bus and callbacks visible to the compiler at dispatch. */
 WillowArcadeGameplayServices services={.sourceA5=0xff8000,.bus=&bus,.resolve=resolve,.word=sourceWord,
  .busWord=word,.byte=romByte,.palette=s->palette,.collision=s->collision,.actors=&timeoutProbeActors,
  .presentation=&presentation,.videoWrite=videoWord,.textWrite=videoWord,.d4=&timeoutProbeD4,.a4=&timeoutProbeA4,
  .player=playerUpdate,.playerContext=&services,.write2=write2,.write3=write3,
  .copy2=copyMap2,.copy3=copyMap3,
  .selectionRegister=&selectionRegister,.draw=drawFrame,.spriteWord=videoWord,.frame=s->frame,
  .mapClearWrite=videoWord,.eventWait=&timeoutProbeWait,.poolZero=poolZero,.videoFill=videoFill,.paletteCopy=paletteCopy};
 return willowArcadeGameplayServiceResume(&services,address,argument);
}
#endif
void timeoutRuntimeProbe(void){
 timeoutProbeStatus=0;
 if(timeoutProbeRegion>4)return;
 if(!timeoutProbeOperation){
  WillowArcadeGameplayServices services={.sourceA5=0xff8000,.bus=&bus,.d4=&timeoutProbeD4,.a4=&timeoutProbeA4,.eventWait=&timeoutProbeWait};
#ifdef WILLOW_TEST_RESET_SCHEDULER
  services.busWord=word;services.mapClearWrite=videoWord;
#endif
#ifdef WILLOW_TEST_SCENE_SCHEDULER
  WillowPaletteAnimation palette={0};palette.read=colorRead;palette.write=colorWrite;
  services.palette=&palette;services.resolve=resolve;services.byte=romByte;
  services.write2=write2;services.write3=write3;
#endif
#ifdef WILLOW_TEST_FOREST_SCHEDULER
  palette.byte=romByte;palette.word=word;palette.allocate=allocate;palette.allocateWithA4=allocateWithA4;
  palette.random=randomByte;palette.sound=sound;palette.soundWithA4=soundWithA4;
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};WillowArcadeSpriteFrame frame;
  renderD4=&timeoutProbeD4;renderA4=&timeoutProbeA4;
  services.word=sourceWord;services.collision=&collision;services.actors=&timeoutProbeActors;
  services.player=playerUpdate;services.playerContext=&services;
  services.videoWrite=videoWord;services.textWrite=videoWord;
  services.draw=drawFrame;services.spriteWord=videoWord;services.frame=&frame;
#endif
#ifdef WILLOW_TEST_INITIAL_SCHEDULER
#ifdef WILLOW_TEST_FOREST_SCHEDULER
  ForestServices forest={&palette,&collision,&frame};
  WillowNativeGameplayFlowBinding binding={&timeoutProbeFlow,&bus,word,&timeoutProbeA4,forestService,&forest};
  (void)services;
#else
  WillowNativeGameplayFlowBinding binding={&timeoutProbeFlow,&bus,word,&timeoutProbeA4,willowArcadeGameplayServiceResume,&services};
#endif
  WillowTransitionStep result=willowNativeGameplayFlowStepWithShop(&binding);
  if(result.supported&&!result.finished&&result.delay==1)
   timeoutProbeStatus=timeoutProbeFlow.task.pc==0x932c?1:2;
#else
  timeoutProbeStatus=willowArcadeEventRouteResume(&timeoutProbeRoute,0x9690,&bus,willowArcadeGameplayServiceResume,&services);
#endif
 }else if(timeoutProbeOperation==1){
  uint16_t cursor=0,remaining=15;
  for(;;){
#ifdef WILLOW_TEST_FOREST_SCHEDULER
   WillowNativeTaskRun run=willowNativeTaskRunOneWithVideoFill(&timeoutProbeRuntime,&bus,cursor,remaining,word,colorRead,colorWrite,colorAdjustGroup,colorFadeAll,videoWord,videoFill);
#else
   WillowNativeTaskRun run=willowNativeTaskRunOneWithVideo(&timeoutProbeRuntime,&bus,cursor,remaining,word,colorRead,colorWrite,colorAdjustGroup,0,videoWord);
#endif
   if(run.unsupported)return;
   if(!run.dispatched||run.remaining==65535)break;
   cursor=run.cursor;remaining=run.remaining;
  }
  timeoutProbeStatus=1;
 }else if(timeoutProbeOperation==2){willowArcadeTaskWake(&bus);timeoutProbeStatus=1;}
}
void*memset(void*p,int v,unsigned long n){unsigned char*d=p;while(n--)*d++=v;return p;}
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
