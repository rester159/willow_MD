/* Differential checks from the verified first-demo endpoint. Actor-only,
 * active-gameplay and continuous post-demo modes use source-derived services.
 * Graphics buses are relocated for headless CPU execution, never shown in GUI. */
#ifdef WILLOW_TEST_INTERLUDE_SETUP_SEQUENCE
#define WILLOW_TEST_INTERLUDE_CLEAR_SEQUENCE
#include "../src/willow_arcade_attract_interlude_setup.h"
#endif
#ifdef WILLOW_TEST_INTERLUDE_CLEAR_SEQUENCE
#define WILLOW_TEST_INTERLUDE_RESET_SEQUENCE
#include "../src/willow_arcade_attract_interlude_clear.h"
#endif
#ifdef WILLOW_TEST_INTERLUDE_RESET_SEQUENCE
#define WILLOW_TEST_DEMO_POST_SEQUENCE
#include "../src/willow_arcade_attract_interlude_reset.h"
#endif
#ifdef WILLOW_TEST_DEMO_POST_SEQUENCE
#define WILLOW_TEST_DEMO_AUXILIARY_TASKS
#include "../src/willow_native_attract_demo_finish.h"
#endif
#ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
#define WILLOW_TEST_DEMO_MANAGER_TASK
#include "../src/willow_native_task_runtime.h"
#endif
#ifdef WILLOW_TEST_DEMO_MANAGER_TASK
#define WILLOW_TEST_DEMO_LIVE_REGISTERS
#include "../src/willow_native_attract_task.h"
#endif
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
#define WILLOW_TEST_DEMO_FIRST_UPDATE
#include "../src/willow_arcade_gameplay_frame.h"
#include "../src/willow_arcade_sprite_writer.h"
#endif
#define WILLOW_TEST_GAMEPLAY_PREFIX
#ifdef WILLOW_TEST_DEMO_FIRST_UPDATE
#include "../src/willow_arcade_attract_demo_update.h"
#include "../src/willow_arcade_stage_update.h"
#endif
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_update.h"
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
#include "../src/willow_arcade_player_bus.h"
#include "../src/willow_arcade_gameplay_actors.h"
#include "../src/willow_arcade_gameplay_collision.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#ifndef WILLOW_TEST_DEMO_UPDATE_SEQUENCE
#define WILLOW_TEST_DEMO_UPDATE_SEQUENCE 1
#endif
#if WILLOW_TEST_DEMO_UPDATE_SEQUENCE > 1 && !defined(WILLOW_TEST_DEMO_FIRST_FRAME)
#error Sequence verification requires the complete update
#endif
#ifndef DEMO_OUTPUT
#if WILLOW_TEST_DEMO_UPDATE_SEQUENCE > 1
#define DEMO_OUTPUT "build/demo-sequence"
#else
#define DEMO_OUTPUT "build/demo-update"
#endif
#endif
static uint8_t rom[0x200000],source[0x100000];
static uint8_t romAccess[0x100000],busAccess[65536];
static unsigned auditAccess;
#if WILLOW_TEST_DEMO_UPDATE_SEQUENCE > 1
static uint8_t sequenceAccess[65536];
#endif
#ifdef WILLOW_TEST_RETAINED_DEMO_UPDATE
#if WILLOW_TEST_DEMO_UPDATE_SEQUENCE > 1
#ifdef WILLOW_TEST_EXTENDED_DEMO_UPDATE
#include "../native/intro_probe/src/demo_sequence_extended_data.h"
#else
#include "../native/intro_probe/src/demo_sequence_data.h"
#endif
#define nativeDemoUpdateWordRead nativeDemoSequenceWordRead
#else
#include "../native/intro_probe/src/demo_update_data.h"
#endif
#endif
static bool env(unsigned cmd,void *data){
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY || cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){((struct retro_variable*)data)->value=NULL;return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT || cmd==RETRO_ENVIRONMENT_SET_VARIABLES || cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS || cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}
static void audio(int16_t l,int16_t r){(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)p;(void)d;(void)i;(void)id;return 0;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing core symbol %s\n",#n);return 2;}

static uint8_t expected[65536],scroll2[0x4000];
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
static uint8_t scroll3[0x4000],spriteMemory[0x2000];
#endif

static uint16_t rw(uint16_t a){if(auditAccess){busAccess[a]|=1;busAccess[(uint16_t)(a+1)]|=1;}return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){if(auditAccess)busAccess[a]|=1;return expected[a];}
static void ww(uint16_t a,uint16_t v){if(auditAccess){busAccess[a]|=2;busAccess[(uint16_t)(a+1)]|=2;}willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){if(auditAccess)busAccess[a]|=2;expected[a]=v;}



static uint16_t sourceWord(unsigned long at){
 if((at&0xff0000)==0xff0000)return rw(at);
 if(at>=sizeof source-1||(at&1))abort();
 uint16_t value=willowMotionWord(source+at);
 if(auditAccess){
  romAccess[at]=romAccess[at+1]=1;
#ifdef WILLOW_TEST_RETAINED_DEMO_UPDATE
  uint16_t retained;if(!nativeDemoUpdateWordRead(at,&retained)||retained!=value){fprintf(stderr,"Missing retained demo-update word %lx\n",at);abort();}return retained;
#endif
 }
 return value;
}
static uint8_t*resolve(uint16_t at){if(auditAccess)busAccess[at]|=4;return expected+at;}
static void textWrite(uint32_t at,uint16_t v){
 if(at>=0x910000&&at<0x914000){ww(0x4000+at-0x910000,v);return;}
 if(at>=0x91c000&&at<0x91d800){ww(0x2000+at-0x91c000,v);return;}
 fprintf(stderr,"Unmapped graphics %x\n",at);abort();
}

static const WillowVisibilityBus bus={rw,rb,ww,wb};
static void videoWrite(uint32_t at,uint16_t v){
 if(at>=0x918000&&at<0x91c000){willowMotionPutWord(scroll2+(at-0x918000),v);return;}
 fprintf(stderr,"Unmapped video %x\n",at);abort();
}
static void mapPointers(uint8_t*ram,int toCore){
 for(unsigned n=0;n<76;n++){
  uint16_t record=n<60?0xccc6+n*64:0xf042+(n-60)*64;
  /* Released tile records retain their mapped destination at +48. */
  uint16_t at=record+48;uint32_t value=0;
  for(unsigned j=0;j<4;j++)value=(value<<8)|ram[(at+j)^1];
  uint32_t original=n<60?0x918000:0x91c000,mapped=n<60?0x200000:0xff2000;
  uint32_t from=toCore?original:mapped,to=toCore?mapped:original;
  unsigned length=n<60?0x4000:0x1800;
  if(value>=from&&value<from+length){value=value-from+to;for(unsigned j=0;j<4;j++)ram[(at+j)^1]=value>>(24-j*8);}
 }
}
static uint8_t attribute(uint16_t at){return scroll2[at+2];}
static uint16_t paletteWord(uint32_t at){return sourceWord(at);}
static uint8_t paletteByte(uint32_t at){
 if(at>=sizeof source)abort();
 if(auditAccess){
  romAccess[at]=1;
#ifdef WILLOW_TEST_RETAINED_DEMO_UPDATE
  uint16_t retained;if(!nativeDemoUpdateWordRead(at&~1UL,&retained))abort();
  uint8_t value=(at&1)?retained:retained>>8;if(value!=source[at])abort();return value;
#endif
 }
 return source[at];
}
static uint16_t colorRead(uint32_t at){if(at<0x91c000||at>=0x91d800)abort();return rw(0x2000+at-0x91c000);}
static void colorWrite(uint32_t at,uint16_t v){if(at<0x91c000||at>=0x91d800)abort();ww(0x2000+at-0x91c000,v);}
static uint8_t*allocateWithA4(uint32_t*a4){uint16_t at=willowArcadePaletteFreeWithA4(&bus,a4);return at?resolve(at):NULL;}
static uint8_t*allocate(void){return allocateWithA4(NULL);}
static uint8_t randomByte(void){return willowActor18Random(&bus);}
static void soundWithA4(uint16_t cmd,uint32_t*a4){willowBreakableSoundWithA4(&bus,cmd,a4);}
static void sound(uint16_t cmd){soundWithA4(cmd,NULL);}
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
static uint32_t *liveA4;
static unsigned allocations,allocationTypes[3];
static unsigned availableEffects(void){return rb(0x97fe);}
static uint8_t*playerAllocate(uint8_t type){
 uint16_t count=type==2?0x97fe:type==8?0x9807:0x980d;
 uint16_t cursor=type==2?0x9816:type==8?0x9828:0x9834;
 if(!rb(count))return NULL;
 uint16_t stack=rw(cursor),at=rw(stack);ww(cursor,stack+2);wb(count,rb(count)-1);
 *liveA4=(uint32_t)(int32_t)(int16_t)at;allocations++;allocationTypes[type==2?0:type==8?1:2]++;return resolve(at);
}
static uint8_t*playerEffect(void){return playerAllocate(2);}
static uint8_t*playerClass12(void){return playerAllocate(12);}
static void playerSound(uint16_t cmd){soundWithA4(cmd,liveA4);}
static void playerCancel(uint16_t at){willowArcadeCancelTaskWithA4(resolve(at),liveA4);}
static uint16_t playerColor(unsigned n){return rw(0x2000+n*2);}
static void playerColorWrite(unsigned n,uint16_t v){ww(0x2000+n*2,v);}
static uint8_t playerUpdate(void*context,uint16_t*d4,uint32_t*a4){
 WillowPlayerCollision*c=context;WillowPlayerRecord p={0};
 WillowPlayerInput i={0};i.availableEffects=availableEffects;i.allocateEffect=playerEffect;i.random=randomByte;i.sound=playerSound;
 WillowPlayerShots shots={0};shots.allocate=playerAllocate;
 WillowPlayerHealth h={0};h.paletteRead=playerColor;h.paletteWrite=playerColorWrite;h.textWrite=textWrite;h.sound=playerSound;h.savedD4=d4;
 WillowPlayerCharge q={0};q.textWrite=textWrite;q.sound=playerSound;
 WillowPlayerScript t={0};t.cancelTask=playerCancel;liveA4=a4;
 return willowArcadeWillowController(&bus,&p,&i,c,&shots,&h,&q,&t,playerClass12);
}
#endif
#ifdef WILLOW_TEST_DEMO_FIRST_UPDATE
static WillowPaletteAnimation*updatePalette;
static WillowPlayerCollision*updateCollision;
static WillowArcadeActorUpdate updateActors;
static uint16_t*updateD4;
#ifdef WILLOW_TEST_DEMO_LIVE_REGISTERS
static uint16_t*updateD1,*updateD2;
#endif
static uint32_t*updateA4;
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
static WillowArcadeSpriteFrame demoFrame;
static unsigned spriteCursor,pieceBudget;
static void mapWrite2(uint16_t at,uint16_t value){if(at>=sizeof scroll2)abort();willowMotionPutWord(scroll2+at,value);}
static void mapWrite3(uint16_t at,uint16_t value){if(at>=sizeof scroll3)abort();willowMotionPutWord(scroll3+at,value);}
static unsigned spriteOffset(uint32_t at){
 if(at>=0x900000&&at<0x901000)return at-0x900000;
 if(at>=0x908000&&at<0x909000)return 0x1000+at-0x908000;
 abort();
}
static void spriteWrite(uint32_t at,uint16_t value){willowMotionPutWord(spriteMemory+spriteOffset(at),value);}
static void emitSprite(uint16_t x,uint16_t y,uint16_t code,uint16_t attr){
 if(spriteCursor+8>sizeof spriteMemory)abort();
 willowMotionPutWord(spriteMemory+spriteCursor,x);willowMotionPutWord(spriteMemory+spriteCursor+2,y);
 willowMotionPutWord(spriteMemory+spriteCursor+4,code);willowMotionPutWord(spriteMemory+spriteCursor+6,attr);spriteCursor+=8;
}
static void drawActor(uint16_t handle){
 WillowArcadeSpriteWriter writer={paletteWord,emitSprite,rw(0x829a),pieceBudget};
#ifdef WILLOW_TEST_DEMO_LIVE_REGISTERS
 willowArcadeSpriteWithRegisters(&writer,resolve(handle),rw(0x8408),rw(0x840c),updateD1,updateD2,updateD4);
#else
 willowArcadeSpriteWithD4(&writer,resolve(handle),rw(0x8408),rw(0x840c),updateD4);
#endif
 ww(0x829a,writer.areaBudget);pieceBudget=writer.pieceBudget;
}
static void drawStage4(uint16_t handle){
 WillowArcadeSpriteWriter writer={paletteWord,emitSprite,rw(0x829a),pieceBudget};
#ifdef WILLOW_TEST_DEMO_LIVE_REGISTERS
 willowArcadeStage4SpriteWithRegisters(&writer,resolve(handle),rw(0x8408),rw(0x840c),updateD1,updateD2,updateD4);
#else
 willowArcadeStage4SpriteWithD4(&writer,resolve(handle),rw(0x8408),rw(0x840c),updateD4);
#endif
 ww(0x829a,writer.areaBudget);pieceBudget=writer.pieceBudget;
}
static void drawFrame(WillowArcadeSpriteFrame*frame){
 unsigned first=spriteOffset(frame->cursor);spriteCursor=first;pieceBudget=frame->remaining;
 willowArcadeVisibilityRenderWithA4(&bus,drawActor,drawStage4,updateA4);
 frame->cursor+=spriteCursor-first;frame->remaining=pieceBudget;
}
#endif
static uint8_t demoService(void*context,uint32_t at,int arg)
{
 (void)context;if(arg!=-1)return 0;
 switch(at){
 /* 785E -> 7844 -> 7824: enqueue F4, FF and retain the sound-ring A4. */
 case 0x785e:{
  uint16_t cursor=rw(0x801e);*updateA4=0xff8172;
  ww(0x8172+cursor,0xf4);cursor=(cursor+2)&126;
  ww(0x8172+cursor,255);ww(0x801e,(cursor+2)&126);return 1;
 }
 case 0x1f88:willowArcadeGameplayInput(&bus,paletteWord);return 1;
 case 0x4ea2:willowArcadeGameplayClockWithA4(&bus,textWrite,updateA4);return 1;
 case 0x9ef2:return willowArcadeActorUpdateResumeWithPlayer(&updateActors,&bus,resolve,
  sourceWord,updatePalette,updateCollision,videoWrite,textWrite,0,updateD4,updateA4,playerUpdate,updateCollision);
 case 0x39a52:return willowArcadeCollisionManagerWithA4(&bus,paletteWord,updateCollision,textWrite,updateD4,updateA4);
 case 0x3807c:return willowArcadeStageUpdateWithA4(&bus,paletteWord,textWrite,updateA4);
 case 0x3b524:
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
  return willowArcadeGameplayFrameWithRegisters(&bus,resolve,paletteByte,paletteWord,mapWrite2,mapWrite3,0,0,updatePalette,drawFrame,spriteWrite,&demoFrame,updateD4,updateA4);
#else
  return 2; /* Explicit boundary before unbound rendering. */
#endif
 default:return 0;
 }
}
#endif
#ifdef WILLOW_TEST_DEMO_MANAGER_TASK
static void demoManagerTick(uint16_t*d1,uint16_t*d2)
{
 WillowArcadeAttractDemoUpdate update={0};
 *d1=paletteWord(0x8cc4);updateD1=d1;updateD2=d2;
 if(willowArcadeAttractDemoUpdateResume(&update,&bus,paletteWord,demoService,0,d1,d2)!=1||update.pc)abort();
}
#endif
#ifdef WILLOW_TEST_INTERLUDE_CLEAR_SEQUENCE
static void interludeVideo(uint32_t at,uint16_t value){
 if(at>=0x910000&&at<0x914000)textWrite(at,value);
 else if(at>=0x918000&&at<0x91c000)willowMotionPutWord(scroll2+(at-0x918000),value);
 else if(at>=0x914000&&at<0x918000)willowMotionPutWord(scroll3+(at-0x914000),value);
 else if(at>=0x900000&&at<0x900800)willowMotionPutWord(spriteMemory+(at-0x900000),value);
 else if(at>=0x908000&&at<0x908800)willowMotionPutWord(spriteMemory+0x1000+(at-0x908000),value);
 else abort();
}
static void interludeClear(uint16_t*d1,uint16_t*d2)
{if(!willowArcadeAttractInterludeClear(&bus,paletteWord,interludeVideo,0,d1,d2))abort();}
#define INTERLUDE_CLEAR_CALLBACK interludeClear
#else
#define INTERLUDE_CLEAR_CALLBACK 0
#endif
#ifdef WILLOW_TEST_INTERLUDE_SETUP_SEQUENCE
static void interludeSetup(uint16_t*d1,uint16_t*d2)
{if(!willowArcadeAttractInterludeSetup(&bus,expected+0x8402,expected+0x8482,paletteByte,paletteWord,
 colorWrite,mapWrite2,mapWrite3,0,0,0,d1,d2))abort();}
#define INTERLUDE_SETUP_CALLBACK interludeSetup
#else
#define INTERLUDE_SETUP_CALLBACK 0
#endif
#ifdef WILLOW_TEST_INTERLUDE_RESET_SEQUENCE
static void interludeReset(uint16_t*d1,uint16_t*d2)
{if(!willowArcadeAttractInterludeReset(&bus,paletteWord,0,0,d1,d2))abort();}
#define INTERLUDE_RESET_CALLBACK interludeReset
#ifdef WILLOW_TEST_INTERLUDE_SETUP_SEQUENCE
#define POST_TERMINAL_STATE 6
#elif defined(WILLOW_TEST_INTERLUDE_CLEAR_SEQUENCE)
#define POST_TERMINAL_STATE 4
#else
#define POST_TERMINAL_STATE 2
#endif
#else
#define INTERLUDE_RESET_CALLBACK 0
#define POST_TERMINAL_STATE 0
#endif
#ifdef WILLOW_TEST_DEMO_POST_SEQUENCE
static void demoManagerFinish(uint16_t*d1,uint16_t*d2)
{
 if(!willowNativeAttractDemoFinishBoundary(&bus,paletteWord,d1,d2,updateA4))abort();
}
#define DEMO_FINISH_CALLBACK demoManagerFinish
#else
#define DEMO_FINISH_CALLBACK 0
#endif
#ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
static WillowNativeTaskRuntime demoRuntime;
static unsigned auxiliaryDispatches,managerDispatches,slotDispatches[16];
static int demoPrivateContext(unsigned at){
 return (at<512&&(at%32)>=2&&(at%32)<12)||
  (at>=0xc0&&at<0x100&&(at%32)>=20&&(at%32)<26)||
  (at>=0x200&&at<0x204)||(at>=0xf00&&at<0x2000)||
  (at>=0xfe80&&at<0xff04);
}
static int loadContinuation(const char*name,void*data,size_t size){
 const char*prefix=getenv("WILLOW_DEMO_CONTINUATION_INPUT");char path[4096];
 if(!prefix)return 0;
 int n=snprintf(path,sizeof path,"%s-%s.bin",prefix,name);
 if(n<0||(size_t)n>=sizeof path)return 0;
 FILE*f=fopen(path,"rb");if(!f){perror(path);return 0;}
 int ok=fread(data,1,size,f)==size&&fgetc(f)==EOF;
 if(fclose(f))ok=0;return ok;
}
#endif
int main(int argc,char**argv){
 if(argc!=2)return 2;
#if defined(WILLOW_TEST_RETAINED_DEMO_UPDATE) && defined(WILLOW_TEST_EXTENDED_DEMO_UPDATE)
 nativeDemoSequenceDataInit();
#endif
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(source,rom,sizeof source);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 /* Independent SRAM backs the 16 KiB Scroll2 map without aliasing work RAM. */
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x203fff);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x8474,0x60fe);
 #ifdef WILLOW_TEST_INTERLUDE_CLEAR_SEQUENCE
 const unsigned clearOperands[]={0x65a6,0x65ba,0x65ce,0x65ec,0x65f6};
 const uint32_t clearOriginal[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 const uint32_t clearTargets[]={0xff4000,0x200000,0x204000,0x208000,0x209000};
 for(unsigned n=0;n<5;n++){
  if(willowMotionLong(rom+clearOperands[n])!=clearOriginal[n])return 2;
  willowMotionPutLong(rom+clearOperands[n],clearTargets[n]);
 }
#else
 willowMotionPutWord(rom+0x65a4,0x60fe);
#endif
#ifdef WILLOW_TEST_INTERLUDE_SETUP_SEQUENCE
 const unsigned paletteOperands[]={0xae4,0xaf6,0xb06,0xb16};
 const uint32_t paletteOriginal[]={0x91c800,0x91c400,0x91cc00,0x91c200};
 for(unsigned n=0;n<4;n++){
  if(willowMotionLong(rom+paletteOperands[n])!=paletteOriginal[n])return 2;
  willowMotionPutLong(rom+paletteOperands[n],0xff2000+(paletteOriginal[n]-0x91c000));
 }
#endif
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 const unsigned patches[]={0x4ad0,0x4ad8,0x4ab2,0x4aba,0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910310,0x910310,0x910314,0x910314,0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}
 for(unsigned n=0;n<2;n++){unsigned at=n?0x1276:0xc2e;if(willowMotionLong(rom+at)!=0x91c000)return 2;willowMotionPutLong(rom+at,0xff2000);}
 willowMotionPutLong(rom+0x3e44,0x200000);willowMotionPutLong(rom+0x4365a,0x200000);
 #ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
 /* Retain the real sleep/yield instructions and stop only after a full scan. */
 willowMotionPutLong(rom+0x8c,0x175c);willowMotionPutLong(rom+0x90,0x1788);
 willowMotionPutLong(rom+0xa0,0x16be);willowMotionPutLong(rom+0xa4,0x165c);
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
 willowMotionPutWord(rom+0x15b4,0x4e75);
 /* Blink erase uses a separate text base from the glyph path at 5BD0. */
 if(willowMotionLong(rom+0x5c32)!=0x910000)return 2;
 willowMotionPutLong(rom+0x5c32,0xff4000);
 /* 47 pointers end at 7ADA; the following bytes are executable code. */
 for(unsigned n=0;n<47;n++){
  unsigned at=0x7a1e + n*4;uint32_t address=willowMotionLong(rom+at);
  if(address<0x910000||address>=0x914000)return 2;
  willowMotionPutLong(rom+at,address-0x910000+0xff4000);
 }
 const unsigned fades[]={0x51ee,0x51fe,0x520e,0x521e,0x522e,0x523e};
 for(unsigned n=0;n<6;n++)willowMotionPutLong(rom+fades[n],0xff2000+n*1024);
 willowMotionPutLong(rom+0x51b2,0xff2000);willowMotionPutLong(rom+0x514a,0xff2000);
 const unsigned banks[]={0x53c2,0x53f0,0x541e,0x544c};
 for(unsigned n=0;n<4;n++)willowMotionPutLong(rom+banks[n],0xff2000+n*1024);
#else
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
#endif
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
 const unsigned playerPatches[]={0xc242,0x4b72,0xea68};
 const uint32_t playerOriginals[]={0x91c000,0x910170,0x91c0e0};
 for(unsigned n=0;n<3;n++){
  if(willowMotionLong(rom+playerPatches[n])!=playerOriginals[n])return 2;
  uint32_t at=playerOriginals[n];willowMotionPutLong(rom+playerPatches[n],at>=0x91c000?0xff2000+at-0x91c000:0xff4000+at-0x910000);
 }
 if(willowMotionWord(rom+0x955c)!=0x4eb9||willowMotionLong(rom+0x955e)!=0x39a52)return 2;
#ifdef WILLOW_TEST_GAMEPLAY_COLLISION
 if(willowMotionWord(rom+0x9562)!=0x4eb9||willowMotionLong(rom+0x9564)!=0x3807c)return 2;
 willowMotionPutWord(rom+0x9562,0x4e75);
#else
 willowMotionPutWord(rom+0x955c,0x4e75);
#endif
#endif
#ifdef WILLOW_TEST_DEMO_FIRST_UPDATE
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
 willowMotionPutLong(rom+0x1b8,0x209fff);
 if(willowMotionLong(rom+0x43d50)!=0x914000||willowMotionWord(rom+0x3b57e)!=15)return 2;
 willowMotionPutLong(rom+0x43d50,0x204000);
 willowMotionPutLong(rom+0x3b538,0x208000);willowMotionPutLong(rom+0x3b54c,0x209000);
 /* Preserve the original sprite-bank selection test after address relocation. */
 willowMotionPutWord(rom+0x3b57e,12);
#else
 willowMotionPutWord(rom+0x3b524,0x60fe);
#endif
#endif
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen(DEMO_OUTPUT "-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={DEMO_OUTPUT "-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 uint8_t*mapRam=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!mapRam||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<sizeof scroll2)return 2;
 WillowPaletteAnimation g={0};g.byte=paletteByte;g.word=paletteWord;g.read=colorRead;g.write=colorWrite;
 g.allocate=allocate;g.random=randomByte;g.sound=sound;g.allocateWithA4=allocateWithA4;g.soundWithA4=soundWithA4;
 /* Start from the CPU-verified continuous first-demo setup endpoint. */
 f=fopen("build/opening-demo-start-0-state.bin","rb");
 if(!f||fread(expected,1,sizeof expected,f)!=sizeof expected||fgetc(f)!=EOF)return 2;fclose(f);
 f=fopen("build/opening-title-context-scroll1.bin","rb");
 if(!f||fseek(f,-16384L,SEEK_END)||fread(expected+0x4000,1,16384,f)!=16384)return 2;fclose(f);
 memset(scroll2,0,sizeof scroll2);
 /* The preceding original clear leaves Scroll2 zero and Scroll3 0x0A00/0. */
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
 memset(scroll3,0,sizeof scroll3);memset(spriteMemory,0,sizeof spriteMemory);
 for(unsigned n=0;n<sizeof scroll3;n+=4)willowMotionPutWord(scroll3+n,0x0a00);
 if(retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<0xa000)return 2;
#endif
 WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
 uint16_t d4=0;uint32_t a4=0xff8172;
#ifndef WILLOW_TEST_DEMO_FIRST_UPDATE
 WillowArcadeActorUpdate actors={0};
#endif
 liveA4=&a4;
#ifdef WILLOW_TEST_DEMO_MANAGER_TASK
 /* The existing service fixture begins inside phase six and omits outer
  * manager state. Supply its documented state-four caller on both sides. */
 ww(0x8000,4);
#endif
 for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
#ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
 uint8_t checkpoint[65536],labels[32];
 if(!loadContinuation("native-ram",checkpoint,sizeof checkpoint)||
    !loadContinuation("resume",labels,sizeof labels))return 2;
 /* The only fixture adaptations are outer manager state and relocated text. */
 for(unsigned n=0;n<65536;n++)if(n!=0x8001&&!(n>=0x4000&&n<0x8000)&&checkpoint[n]!=expected[n])
  return fprintf(stderr,"Continuation fixture differs at %x\n",n),1;
 for(unsigned n=0;n<16;n++)demoRuntime.resume[n]=willowMotionWord(labels+n*2);
 if(!loadContinuation("cpu-ram",checkpoint,sizeof checkpoint))return 2;
 for(unsigned n=0;n<65536;n++)ram[n^1]=checkpoint[n];
 for(unsigned n=0x4000;n<0x8000;n++)ram[n^1]=expected[n];
 ram[0x8000^1]=0;ram[0x8001^1]=4;
 /* Supply the omitted manager at its documented 1756 register-restore
  * boundary, with the same D1/D2/D4/A4 inputs as the service oracle. This
  * synthetic caller context is explicit; auxiliary contexts are captured. */
 ww(0x20,0x0400);ww(0x22,0);ww(0x24,0);demoRuntime.resume[1]=0x810e;
 uint8_t manager[32]={0};willowMotionPutWord(manager,0x0400);
 willowMotionPutLong(manager+4,0x1756);willowMotionPutLong(manager+8,0xff10c0);
 willowMotionPutLong(manager+12,0xff1100);
 for(unsigned n=0;n<32;n++)ram[(0x20+n)^1]=manager[n];
 uint8_t saved[64]={0};willowMotionPutLong(saved+48,0xff8172);
 willowMotionPutLong(saved+52,0xff8000);willowMotionPutLong(saved+60,0x810e);
 for(unsigned n=0;n<64;n++)ram[(0x10c0+n)^1]=saved[n];
#endif
 memcpy(mapRam,scroll2,sizeof scroll2);
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
 memcpy(mapRam+0x4000,scroll3,sizeof scroll3);memcpy(mapRam+0x8000,spriteMemory,sizeof spriteMemory);
#endif
#if WILLOW_TEST_DEMO_UPDATE_SEQUENCE > 1
 const char*traceNames[]={"ram","scroll2","scroll3","sprites","registers","access"};FILE*traces[6];
 for(unsigned n=0;n<6;n++){char path[128];snprintf(path,sizeof path,DEMO_OUTPUT "-trace-%s.bin",traceNames[n]);traces[n]=fopen(path,"wb");if(!traces[n])return 2;}
#endif
/* Carry both machines' outputs forward independently. No model state is
  * copied into the CPU after initialization. AUXILIARY_TASKS uses captured
  * task contexts and scans/wakes; other modes isolate the service chain.
  * Neither mode models interrupt timing. */
 uint16_t cpuD4=0;uint32_t cpuA4=0xff8172;
#ifdef WILLOW_TEST_DEMO_LIVE_REGISTERS
 uint16_t cpuD1=0,cpuD2=0,liveD1=0,liveD2=0;
 FILE*liveTrace=fopen(DEMO_OUTPUT "-trace-live-registers.bin","wb");if(!liveTrace)return 2;
#endif
 #ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
 FILE*schedulerTrace=fopen(DEMO_OUTPUT "-trace-scheduler.bin","wb");if(!schedulerTrace)return 2;
#endif
 unsigned completed=0;
 (void)completed;
 for(unsigned step=0;step<WILLOW_TEST_DEMO_UPDATE_SEQUENCE;step++){
 memset(busAccess,0,sizeof busAccess);
 mapPointers(ram,1);
#ifdef WILLOW_TEST_DEMO_FIRST_UPDATE
 auditAccess=1;
 updatePalette=&g;updateCollision=&collision;updateD4=&d4;updateA4=&a4;
 WillowArcadeAttractDemoUpdate update={0};uint16_t d1=0,d2=0;
#ifdef WILLOW_TEST_DEMO_LIVE_REGISTERS
 d1=liveD1;d2=liveD2;updateD1=&d1;updateD2=&d2;
#endif
#ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
 uint16_t cursor=0,remaining=15,scanDispatches=0;
 for(;;){
  WillowTaskSelection selected=willowArcadeTaskSelect(&bus,cursor,remaining);
  WillowNativeTaskRun run=willowNativeTaskRunOneWithInterludeSetup(&demoRuntime,&bus,cursor,remaining,paletteWord,colorRead,colorWrite,0,0,textWrite,0,0,0,0,0,0,0,DEMO_FINISH_CALLBACK,demoManagerTick,INTERLUDE_RESET_CALLBACK,INTERLUDE_CLEAR_CALLBACK,INTERLUDE_SETUP_CALLBACK);
  if(run.unsupported)return fprintf(stderr,"Unsupported auxiliary task %x update %u resume %x\n",run.cursor,step+1,demoRuntime.resume[run.cursor/32]),1;
  if(!run.dispatched)break;
  scanDispatches++;slotDispatches[selected.slot/32]++;
  if(selected.slot==0x20)managerDispatches++;else auxiliaryDispatches++;
  if(run.remaining==65535)break;
  cursor=run.cursor;remaining=run.remaining;
 }
 d1=rw(0x22);d2=rw(0x24);uint8_t result=1;
#elif defined(WILLOW_TEST_DEMO_MANAGER_TASK)
 WillowNativeAttractTask task={0x810e,d1,d2};
 if(willowNativeAttractTaskSupportedWithDemoFinish(&task,&bus,paletteWord,colorWrite,0,0,0,0,0,0,0))return fprintf(stderr,"Old manager unexpectedly admitted update %u state %u phase %u substate %u\n",step+1,rw(0x8000),rw(0x8004),rw(0x8008)),1;
 WillowTransitionStep sleep=willowNativeAttractTaskStepWithDemoTick(&task,&bus,paletteWord,colorWrite,0,0,0,0,0,0,0,0,0,demoManagerTick);
 if(!sleep.supported||sleep.finished||sleep.delay!=1||sleep.resume!=0x810e)return fprintf(stderr,"Manager task result %u/%u/%u/%x update %u state %u phase %u\n",sleep.supported,sleep.finished,sleep.delay,sleep.resume,step+1,rw(0x8000),rw(0x8004)),1;
 d1=task.d1;d2=task.d2;uint8_t result=1;
#else
 uint8_t result=willowArcadeAttractDemoUpdateResume(&update,&bus,paletteWord,demoService,0,&d1,&d2);
#endif
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
 if(result!=1||update.pc)
#else
 if(result!=2||update.pc!=0x8e0e)
#endif
 {fprintf(stderr,"Demo update %u unsupported/suspended: result %u pc %x actor phase %u\n",step+1,result,update.pc,updateActors.phase);return 1;}
 unsigned entry=0x8de4,stop=
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
 0x100000;
#else
 0x3b524;
#endif
#else
 auditAccess=1;
 uint8_t result=willowArcadeActorUpdateResumeWithPlayer(&actors,&bus,resolve,
  sourceWord,&g,&collision,videoWrite,textWrite,0,&d4,&a4,playerUpdate,&collision);
 if(result!=1){fprintf(stderr,"Cold demo actor chain unsupported/suspended: result %u phase %u\n",result,actors.phase);return 1;}
 unsigned entry=0x9ef2,stop=0x100000;
#endif
#ifdef WILLOW_TEST_DEMO_MANAGER_TASK
 entry=0x810e;
#endif
#ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
 entry=0x14d8;
#endif
 auditAccess=0;
 ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
 m68k_set_reg(12,cpuA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
#ifdef WILLOW_TEST_DEMO_LIVE_REGISTERS
 m68k_set_reg(1,cpuD1);m68k_set_reg(2,cpuD2);
#endif
 m68k_set_reg(4,cpuD4);m68k_set_reg(16,entry);
#ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
 m68k_set_reg(8,0xff0000);m68k_set_reg(0,15);
#endif
 for(unsigned slice=0;slice<64&&m68k_get_reg(16)!=stop;slice++)retro_run();
 if(m68k_get_reg(16)!=stop){fprintf(stderr,"Cold demo actor source boundary %x\n",m68k_get_reg(16));return 1;}
#if defined(WILLOW_TEST_DEMO_MANAGER_TASK) && !defined(WILLOW_TEST_DEMO_AUXILIARY_TASKS)
 unsigned sp=m68k_get_reg(15)&65535;uint32_t resume=0;
 for(unsigned n=0;n<4;n++)resume=(resume<<8)|ram[(sp+n)^1];
 if(resume!=0x810e||(m68k_get_reg(0)&65535)!=1)return fprintf(stderr,"Demo manager sleep mismatch update %u\n",step+1),1;
#endif
 mapPointers(ram,0);
 for(unsigned n=0;n<65536;n++)if(
#ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
 !demoPrivateContext(n)&&
#else
 !(n>=0xfe80&&n<0xff04)&&
#endif
 ram[n^1]!=expected[n]){
  fprintf(stderr,"Demo update %u state %04x source %02x model %02x\n",step+1,n,ram[n^1],expected[n]);return 1;
 }
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
 if(memcmp(mapRam+0x4000,scroll3,sizeof scroll3)||memcmp(mapRam+0x8000,spriteMemory,sizeof spriteMemory))return fprintf(stderr,"Demo frame graphics mismatch\n"),1;
#endif
 unsigned observedD1=m68k_get_reg(1),observedD2=m68k_get_reg(2);
 unsigned observedD4=m68k_get_reg(4),observedA4=m68k_get_reg(12);
#ifndef WILLOW_TEST_DEMO_LIVE_REGISTERS
 (void)observedD1;(void)observedD2;
#endif
#ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
 if(ram[0x20^1]!=1||ram[0x21^1]!=1||demoRuntime.resume[1]!=0x810e||
    ram[0x24^1]||ram[0x25^1]||ram[0x26^1]!=0x17||ram[0x27^1]!=0x56)
  return fprintf(stderr,"Manager sleep context mismatch at update %u\n",step+1),1;
 unsigned managerStack=((unsigned)ram[0x2a^1]<<8)|ram[0x2b^1];
 if(managerStack<0x1000||managerStack>0x10c0)return fprintf(stderr,"Bad manager stack %x\n",managerStack),1;
 /* Compare the manager's saved registers, not the scheduler's scratch
  * registers. Nothing from the native model is supplied to the CPU. */
 const unsigned liveRegs[]={1,2,4,12};
 unsigned*observed[]={&observedD1,&observedD2,&observedD4,&observedA4};
 for(unsigned k=0;k<4;k++){
  unsigned value=0,at=managerStack+liveRegs[k]*4;
  for(unsigned n=0;n<4;n++)value=(value<<8)|ram[(at+n)^1];
  *observed[k]=value;
 }
#endif
 if(memcmp(mapRam,scroll2,sizeof scroll2)||observedA4!=a4||(uint16_t)observedD4!=d4){fprintf(stderr,"Demo update %u map/register mismatch\n",step+1);return 1;}
#ifdef WILLOW_TEST_DEMO_LIVE_REGISTERS
 if((uint16_t)observedD1!=d1||(uint16_t)observedD2!=d2){fprintf(stderr,"Demo update %u live D1/D2 source %x/%x model %x/%x\n",step+1,observedD1&65535,observedD2&65535,d1,d2);return 1;}
 cpuD1=observedD1;cpuD2=observedD2;liveD1=d1;liveD2=d2;
 uint8_t live[4];willowMotionPutWord(live,d1);willowMotionPutWord(live+2,d2);
 if(fwrite(live,1,sizeof live,liveTrace)!=sizeof live)return 2;
#endif
 cpuD4=observedD4;cpuA4=observedA4;
#ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
 willowArcadeTaskWake(&bus);
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
 ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
 m68k_set_reg(16,0x1590);
 for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
 if(m68k_get_reg(16)!=0x100000)return fprintf(stderr,"Wake stalled\n"),1;
 for(unsigned n=0;n<65536;n++)if(!demoPrivateContext(n)&&ram[n^1]!=expected[n])
  return fprintf(stderr,"Demo wake %u mismatch at %x\n",step+1,n),1;
 uint8_t schedulerData[34];willowMotionPutWord(schedulerData,scanDispatches);
 for(unsigned n=0;n<16;n++)willowMotionPutWord(schedulerData+2+n*2,demoRuntime.resume[n]);
 if(fwrite(schedulerData,1,sizeof schedulerData,schedulerTrace)!=sizeof schedulerData)return 2;
#endif
#if WILLOW_TEST_DEMO_UPDATE_SEQUENCE > 1
 for(unsigned n=0;n<65536;n++)sequenceAccess[n]|=busAccess[n];
 uint8_t traceRegisters[6];willowMotionPutWord(traceRegisters,d4);willowMotionPutLong(traceRegisters+2,a4);
 const void*outputs[]={expected,scroll2,scroll3,spriteMemory,traceRegisters,sequenceAccess};
 const unsigned sizes[]={65536,16384,16384,8192,6,65536};
 for(unsigned n=0;n<6;n++)if(fwrite(outputs[n],1,sizes[n],traces[n])!=sizes[n])return 2;
#endif
 completed=step+1;
#ifdef WILLOW_TEST_DEMO_POST_SEQUENCE
 if(rw(0x8004)==8&&rw(0x8008)==POST_TERMINAL_STATE){
#ifdef WILLOW_TEST_INTERLUDE_SETUP_SEQUENCE
  if(rw(0x800a)!=10||rb(0xe58)!=1)return fprintf(stderr,"Interlude setup endpoint invalid\n"),1;
#else
  if(rw(0x800a)||rb(0xe58))return fprintf(stderr,"Post-demo endpoint not clean\n"),1;
#endif
  break;
 }
#endif
 }
#ifdef WILLOW_TEST_DEMO_POST_SEQUENCE
 if(rw(0x8004)!=8||rw(0x8008)!=POST_TERMINAL_STATE)return fprintf(stderr,"Post-demo sequence did not reach phase eight\n"),1;
 printf("Continuous post-demo: %u total scans (%u after gameplay), reaches phase 8/%u with original task continuations, messages and hold timing\n",completed,completed-900,POST_TERMINAL_STATE);
#endif
#ifdef WILLOW_TEST_DEMO_AUXILIARY_TASKS
 if(fclose(schedulerTrace))return 2;
 if(managerDispatches!=completed)return 1;
 for(unsigned n=0;n<16;n++)if(slotDispatches[n])printf("Task %03x: %u dispatches\n",n*32,slotDispatches[n]);
 printf("Demo scheduler: %u manager and %u auxiliary dispatches; all scan/wake boundaries match original CPU; no interrupt timing claim\n",managerDispatches,auxiliaryDispatches);
#elif defined(WILLOW_TEST_DEMO_MANAGER_TASK)
 puts("Active demo manager: all updates execute original 810E dispatch through complete gameplay, post-service flags/input and one-tick 1750 sleep; no scheduler task interleaving or interrupt timing claim");
#endif
#ifdef WILLOW_TEST_DEMO_LIVE_REGISTERS
 if(fclose(liveTrace))return 2;
 printf("Demo live registers: D1/D2 match original CPU at all %u update endpoints; independent register carry, no output substitution\n",completed);
#endif
#if WILLOW_TEST_DEMO_UPDATE_SEQUENCE > 1
 for(unsigned n=0;n<6;n++)if(fclose(traces[n]))return 2;
 memcpy(busAccess,sequenceAccess,sizeof busAccess);
 printf("Demo service sequence: %u consecutive complete updates match original CPU RAM, maps, sprites and D4/A4; interrupt timing not included\n",completed);
#else
#ifdef WILLOW_TEST_DEMO_FIRST_UPDATE
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
 puts("Cold demo: complete first active update including real actors, collision, stage, both scroll maps and sprite publication matches original RAM, graphics buses and D4/A4");
#else
 puts("Cold demo: first active timer/input/clock/player/actor/collision/stage update matches original non-stack RAM, palettes/HUD, Scroll2 and D4/A4; stops before rendering");
#endif
#else
 puts("Cold demo: complete first 9EF2 actor update including real player matches original non-stack RAM, palette/HUD, Scroll2, D4/A4; collision/stage/render are separate");
#endif
#endif
#ifdef WILLOW_TEST_DEMO_FIRST_FRAME
 uint8_t registers[6];willowMotionPutWord(registers,d4);willowMotionPutLong(registers+2,a4);
 f=fopen(DEMO_OUTPUT "-output-registers.bin","wb");if(!f||fwrite(registers,1,sizeof registers,f)!=sizeof registers||fclose(f))return 2;
 f=fopen(DEMO_OUTPUT "-output-ram.bin","wb");if(!f||fwrite(expected,1,sizeof expected,f)!=sizeof expected||fclose(f))return 2;
 f=fopen(DEMO_OUTPUT "-output-scroll2.bin","wb");if(!f||fwrite(scroll2,1,sizeof scroll2,f)!=sizeof scroll2||fclose(f))return 2;
 f=fopen(DEMO_OUTPUT "-output-scroll3.bin","wb");if(!f||fwrite(scroll3,1,sizeof scroll3,f)!=sizeof scroll3||fclose(f))return 2;
 f=fopen(DEMO_OUTPUT "-output-sprites.bin","wb");if(!f||fwrite(spriteMemory,1,sizeof spriteMemory,f)!=sizeof spriteMemory||fclose(f))return 2;
 f=fopen(DEMO_OUTPUT "-rom-access.bin","wb");if(!f||fwrite(romAccess,1,sizeof romAccess,f)!=sizeof romAccess||fclose(f))return 2;
 f=fopen(DEMO_OUTPUT "-bus-access.bin","wb");if(!f||fwrite(busAccess,1,sizeof busAccess,f)!=sizeof busAccess||fclose(f))return 2;
#endif
 return 0;
}
