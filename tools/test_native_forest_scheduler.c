/* Continuous host/native main task, map actors, sprites, fade tasks and wakes.
 * One manager boundary and scheduler pass per fixture tick; no forced completion. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_native_task_runtime.h"
#include "../src/willow_native_gameplay_flow.h"
#include "../src/willow_arcade_player_bus.h"
#include "../src/willow_arcade_sprite_writer.h"
#include "../src/willow_arcade_boot_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],source[0x100000];
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

static void putRamWord(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
static uint8_t expected[65536],nativeInput[65536],oracleAfter[65536];
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}

static uint32_t videoBase;static unsigned videoSize;
static unsigned videoWrites,paletteWrites;
static uint8_t scroll2[0x4000];
static uint8_t attribute(uint16_t at){return scroll2[at+2];}
static const WillowVisibilityBus bus={rw,rb,ww,wb};
static uint16_t romWord(uint32_t at){
 if(at>=0x918000&&at<0x91c000)return willowMotionWord(scroll2+(at-0x918000));
 if((at&0xff0000UL)==0xff0000UL)return rw((uint16_t)at);
 if(at>=sizeof source-1){fprintf(stderr,"Unmapped source word %x\n",at);abort();}
 return willowMotionWord(source+at);
}
static void videoWord(uint32_t at,uint16_t v){if(at>=0x91c000&&at<0x91d800){ww(0x2000+at-0x91c000,v);paletteWrites++;return;}if(at>=0x918000&&at<0x91c000)willowMotionPutWord(scroll2+(at-0x918000),v);if(at>=videoBase&&at<videoBase+videoSize){ww(0x4000+at-videoBase,v);videoWrites++;}}
static uint8_t*resolve(uint16_t at){return expected+at;}
static uint8_t romByte(uint32_t at){return source[at];}
static void write2(uint16_t at,uint16_t value){videoWord(0x918000UL+at,value);}
static void write3(uint16_t at,uint16_t value){videoWord(0x914000UL+at,value);}
static void colorWrite(uint32_t at,uint16_t v){if(at<0x91c000||at>=0x91d800)abort();ww(0x2000+at-0x91c000,v);paletteWrites++;}
static uint16_t colorRead(uint32_t at){return rw(0x2000+at-0x91c000);}
static uint16_t sourceWord(unsigned long at){return (at&0xff0000UL)==0xff0000UL?rw((uint16_t)at):romWord(at);}
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
static void drawOrdinary(uint16_t at){WillowArcadeSpriteWriter w={romWord,emitPiece,rw(0x829a),outputBudget};willowArcadeSpriteWithRegisterTracking(&w,resolve(at),rw(0x8408),rw(0x840c),0,&selectionRegister.d2,renderD4,&selectionRegister.known);ww(0x829a,w.areaBudget);outputBudget=w.pieceBudget;}
static void drawStage4(uint16_t at){WillowArcadeSpriteWriter w={romWord,emitPiece,rw(0x829a),outputBudget};willowArcadeStage4SpriteWithRegisterTracking(&w,resolve(at),rw(0x8408),rw(0x840c),0,&selectionRegister.d2,renderD4,&selectionRegister.known);ww(0x829a,w.areaBudget);outputBudget=w.pieceBudget;}
static void drawFrame(WillowArcadeSpriteFrame*f){outputCursor=f->cursor;outputBudget=f->remaining;willowArcadeVisibilityRenderWithA4(&bus,drawOrdinary,drawStage4,renderA4);f->cursor=outputCursor;f->remaining=outputBudget;}
/* Message row/row-count state replaces retired panel payload after the panel
 * completes. Compare it to the original saved registers instead of RAM below. */
static uint16_t messageContextSlots;
static uint32_t oracleLong(const uint8_t *ram,unsigned at){
 uint32_t value=0;for(unsigned j=0;j<4;j++)value=(value<<8)|ram[((at+j)&65535)^1];return value;
}
static int cpuContext(unsigned a){return (a<512&&a%32>=20&&a%32<26&&(messageContextSlots&(1u<<(a/32))))||(a<512&&a%32>=2&&a%32<12)||(a>=0xd4&&a<0xdc)||(a>=0x200&&a<0x204)||(a>=0x1000&&a<0x2000)||(a>=0xef80&&a<0xf004)||(a>=0xfe80&&a<0xff04);}
#ifdef WILLOW_TEST_FIRST_LEVEL
#ifndef WILLOW_TEST_PLAY_TICKS
#define WILLOW_TEST_PLAY_TICKS 2048
#endif
/* Relocate only video pointers owned by the two actor/controller families;
 * preserve every other RAM byte and compare canonical arcade addresses. */
static void firstLevelPointers(uint8_t*ram,int toCore){
 for(unsigned n=0;n<76;n++){
  unsigned record=n<60?0xccc6+n*64:0xf042+(n-60)*64;
  /* Reused records retain old tile addresses. Selector 19 reuses byte 48
   * as a counter, so translating a stale address corrupts that record once
   * the counter changes. Only live tile-writer families own this pointer.
   * On return also canonicalize pointers retained by a retired writer. */
  if(toCore&&n<60){
   uint8_t kind=ram[(record+32)^1],variant=ram[(record+33)^1];
   if(!ram[record^1]||!((kind==6&&(variant<=2||(variant>=5&&variant<=7)))||kind==35))continue;
  }
  unsigned at=record+48;uint32_t v=0;
  for(unsigned j=0;j<4;j++)v=(v<<8)|ram[(at+j)^1];
  uint32_t original=n<60?0x918000:0x91c000,mapped=n<60?0xff4000:0xff2000;
  uint32_t from=toCore?original:mapped,to=toCore?mapped:original;
  if(v>=from&&v<from+(n<60?0x4000:0x1000)){
   v=v-from+to;for(unsigned j=0;j<4;j++)ram[(at+j)^1]=v>>(24-j*8);
  }
 }
}
#else
#define firstLevelPointers(r,d) ((void)0)
#endif
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} ForestCoreCycles;
static unsigned long long cycleProfile[0x80000];
static unsigned long long resetProfile[0x80000];
#ifndef WILLOW_TEST_NEXT_STAGE_PLAY_TICKS
#define WILLOW_TEST_NEXT_STAGE_PLAY_TICKS 128
#endif
#ifndef WILLOW_TEST_STAGE4_PLAY_TICKS
#define WILLOW_TEST_STAGE4_PLAY_TICKS 2048
#endif
int main(int argc,char**argv){
 if(argc!=3&&((argc!=4&&argc!=6)||strcmp(argv[3],"--cycles")))return 2;
 unsigned cycleMode=argc>=4,profileSample=0,profilePass=23;
 unsigned sourceCycleMode=getenv("WILLOW_SOURCE_CYCLES")!=NULL;
 if(argc==6){
  char*end;unsigned long value=strtoul(argv[4],&end,10);
  if(!argv[4][0]||*end||value>=4)return 2;profileSample=value;
  value=strtoul(argv[5],&end,10);
  if(!argv[5][0]||*end||value>=4096)return 2;profilePass=value;
 }
 unsigned region=strtoul(argv[2],0,10);if(region>4)return 2;
#ifdef WILLOW_TEST_FIRST_LEVEL
 if(region!=1)return fprintf(stderr,"First-level CPU investigation requires the live collision-map region (1)\n"),2;
 unsigned requestedTicks=WILLOW_TEST_PLAY_TICKS;
 const char*ticksOption=getenv("WILLOW_PLAY_TICKS");
 if(ticksOption){char*end;unsigned long count=strtoul(ticksOption,&end,10);if(!*ticksOption||*end||!count||count>200000)return 2;requestedTicks=count;}
#endif
 const uint32_t videoBases[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 videoBase=videoBases[region];videoSize=region<3?0x4000:0x800;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(source,rom,sizeof source);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);

 /* Intercept only the outer manager wait; scheduled tasks use real 1750. */
 if(willowMotionWord(rom+0x1ea0)!=0x1750)return 2;
 willowMotionPutWord(rom+0x1ea0,0x300);
 if(willowMotionWord(rom+0x1ec2)!=0x1750)return 2;
 willowMotionPutWord(rom+0x1ec2,0x300);
 if(willowMotionWord(rom+0x932a)!=0x1750)return 2;
 willowMotionPutWord(rom+0x932a,0x300);
 willowMotionPutWord(rom+0x300,0x4ef9);willowMotionPutLong(rom+0x302,0x100000);
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
 willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x9c,0x160e);
 willowMotionPutLong(rom+0x8c,0x175c);willowMotionPutLong(rom+0x90,0x1788);willowMotionPutLong(rom+0xa4,0x165c);
 willowMotionPutLong(rom+0xa8,0x15f2);willowMotionPutLong(rom+0x88,0x1690);
 if(region==1)for(unsigned n=0;n<12;n++){
  uint32_t at=0x340b6+n*4,original=willowMotionLong(rom+at);
  if(original<0x918000||original>=0x91c000)return 2;
  willowMotionPutLong(rom+at,0xff4000+original-0x918000);
 }
 /* 32E58 stores an absolute Scroll2 pointer before the first transition
  * write. Relocate that literal as well as retained record pointers. */
 if(willowMotionLong(rom+0x32e5a)!=0x9197dc)return 2;
 if(region==1)willowMotionPutLong(rom+0x32e5a,0xff57dc);
 /* 31FB8 computes the paired floor attributes from the Scroll2 base. */
 if(willowMotionLong(rom+0x31fba)!=0x918000)return 2;
 if(region==1)willowMotionPutLong(rom+0x31fba,0xff4000);
 willowMotionPutWord(rom+0x15b4,0x4e75);
 if(willowMotionLong(rom+0x679e)!=0x910034)return 2;
 if(region==0)willowMotionPutLong(rom+0x679e,0xff4034);
 if(willowMotionLong(rom+0xb38)!=0x91c800||willowMotionLong(rom+0xb4a)!=0x91cc00)return 2;
 willowMotionPutLong(rom+0xb38,0xff2800);willowMotionPutLong(rom+0xb4a,0xff2c00);
 if(region>=3){
  unsigned at=region==3?0x6620:0x662a;
  if(willowMotionLong(rom+at)!=videoBase)return 2;
  willowMotionPutLong(rom+at,0xff4000);
 }
 const unsigned clears[]={0x5876,0x588a,0x589e,0x58bc,0x58c6};
 if(willowMotionLong(rom+clears[region])!=videoBase)return 2;
 willowMotionPutLong(rom+clears[region],0xff4000);
 const unsigned layerClears[]={0x5a14,0x5a50,0x5a90,0x59d8,0x59e2};
 if(willowMotionLong(rom+layerClears[region])!=videoBase)return 2;
 willowMotionPutLong(rom+layerClears[region],0xff4000);
 if(region<3){
  const unsigned resetClears[]={0x6644,0x6656,0x6668};
  if(willowMotionLong(rom+resetClears[region])!=videoBase)return 2;
  willowMotionPutLong(rom+resetClears[region],0xff4000);
 }
 const unsigned fadePatches[]={0x51ee,0x51fe,0x520e,0x521e,0x522e,0x523e};
 for(unsigned n=0;n<6;n++){
  if(willowMotionLong(rom+fadePatches[n])!=0x91c000+n*1024)return 2;
  willowMotionPutLong(rom+fadePatches[n],0xff2000+n*1024);
 }
 const unsigned bankFadePatches[]={0x531c,0x5346,0x5370,0x539a,0x53c2,0x53f0,0x541e,0x544c};
 for(unsigned n=0;n<8;n++){
  if(willowMotionLong(rom+bankFadePatches[n])!=0x91c000+(n%4)*1024)return 2;
  willowMotionPutLong(rom+bankFadePatches[n],0xff2000+(n%4)*1024);
 }
 /* Bank-one relocation changes its identifying address bit from 15 to 14. */
 if(region==4){
  if(willowMotionWord(rom+0x3b57e)!=15)return 2;
  willowMotionPutWord(rom+0x3b57e,14);
 }
 if(region>=3){
  unsigned spriteAt=region==3?0x3b538:0x3b54c;
  if(willowMotionLong(rom+spriteAt)!=videoBase)return 2;
  willowMotionPutLong(rom+spriteAt,0xff4000);
  unsigned at=region==3?0x58f8:0x5902;
  if(willowMotionLong(rom+at)!=videoBase)return 2;
  willowMotionPutLong(rom+at,0xff4000);
 }
 const unsigned palettePatches[]={0xae4,0xb06,0xaf6,0xb16};
 const uint32_t paletteTargets[]={0x91c800,0x91cc00,0x91c400,0x91c200};
 for(unsigned n=0;n<4;n++){
  if(willowMotionLong(rom+palettePatches[n])!=paletteTargets[n])return 2;
  willowMotionPutLong(rom+palettePatches[n],paletteTargets[n]-0x91c000+0xff2000);
 }
 if(region==1||region==2){
  unsigned at=region==1?0x4365a:0x43d50;
  if(willowMotionLong(rom+at)!=videoBase)return 2;
  willowMotionPutLong(rom+at,0xff4000);
 }
#ifdef WILLOW_TEST_FIRST_LEVEL
 if(willowMotionLong(rom+0x3e44)!=0x918000)return 2;
 willowMotionPutLong(rom+0x3e44,0xff4000);
#endif
 /* Relocate complete stage palettes and all HUD/message output paths. */
 const unsigned colors[]={0x988,0x99a,0x9b0,0x9cc,0x9da,0xc242,0xea68,0xc2e,0x1276};
 const uint32_t colorBases[]={0x91c000,0x91c200,0x91c800,0x91c400,0x91cc00,0x91c000,0x91c0e0,0x91c000,0x91c000};
 for(unsigned n=0;n<sizeof colors/sizeof colors[0];n++){
  if(willowMotionLong(rom+colors[n])!=colorBases[n])return 2;
  willowMotionPutLong(rom+colors[n],0xff2000+colorBases[n]-0x91c000);
 }
 willowMotionPutLong(rom+0x88,0x1690);willowMotionPutLong(rom+0xa0,0x1638);
 if(region==0){
 if(willowMotionLong(rom+0x7d74)!=0x910000)return 2;
 willowMotionPutLong(rom+0x7d74,0xff4000);
 const unsigned patches[]={0x7d84,0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910200,0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}

  willowMotionPutLong(rom+0x5c32,0xff4000);
  willowMotionPutLong(rom+0x4b72,0xff4170);
  for(unsigned n=0;n<47;n++){
   unsigned at=0x7a1e + n*4;
   willowMotionPutLong(rom+at,willowMotionLong(rom+at)-0x910000+0xff4000);
  }
  for(unsigned n=0;n<5;n++)for(unsigned t=0;t<2;t++){
   unsigned at=(t?0x7c64:0x7c50)+n*4;
   willowMotionPutLong(rom+at,willowMotionLong(rom+at)-0x910000+0xff4000);
  }
 }
 memcpy(rom+0x300000,source,sizeof source);
 f=fopen("build/forest_runtime_probe.bin","rb");if(!f)return 2;
 size_t nativeSize=fread(rom+0x100010,1,0xffff0,f);if(!nativeSize||!feof(f))return 2;fclose(f);
 unsigned targetEntry=0,targetAddress[10]={0};
 const char*names[]={"timeoutProbeOperation","timeoutProbeRegion","timeoutProbeStatus","timeoutProbeD4","timeoutProbeA4","timeoutProbeWait","timeoutProbeFlow","timeoutProbeRuntime","timeoutProbeActors",0};
 f=fopen("build/forest_runtime_probe.sym","r");if(!f)return 2;
 unsigned address;char kind,name[128];
 while(fscanf(f,"%x %c %127s",&address,&kind,name)==3){
  if(!strcmp(name,"timeoutRuntimeProbe"))targetEntry=address;
  for(unsigned n=0;names[n];n++)if(!strcmp(name,names[n]))targetAddress[n]=address-0x200000;
 }
 fclose(f);if(targetEntry<0x100010||targetEntry>=0x200000)return 2;
 for(unsigned n=0;names[n];n++)if(targetAddress[n]<0x800||targetAddress[n]>0x1fc0)return 2;
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 willowMotionPutWord(rom+0x100004,0x13fc);willowMotionPutWord(rom+0x100006,1);
 willowMotionPutLong(rom+0x100008,0xa130f1);willowMotionPutWord(rom+0x10000c,0x4e75);willowMotionPutWord(rom+0x10000e,0x4e71);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/native-forest-scheduler-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/native-forest-scheduler-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<65536)return 2;
 putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100004);retro_run();
 if(m68k_get_reg(16)!=0x100000)return 2;
 FN(void,m68k_run,(unsigned));ForestCoreCycles*cpu=dlsym(core,"m68k");if(!cpu)return 2;
 if(cycleMode||sourceCycleMode){
  m68k_set_reg(16,0x10000e);if(cpu->pc!=0x10000e)return 2;
  for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
  cpu->cycles=0;cpu->refresh=896;m68k_run(1);
  if(cpu->cycles!=28||m68k_get_reg(16)!=0x100010)return 2;
 }
 unsigned targetChecks=0,tickCycles=0,timingCount=0,timingOver=0,timingMax=0,maxPass=0,maxSample=0,maxState=0;
 unsigned sourceTickCycles=0;
 FILE*sourceTiming=sourceCycleMode?fopen("build/forest-source-timing.csv","w"):NULL;
 if(sourceCycleMode&&!sourceTiming)return 2;
 if(sourceTiming)fputs("sample,pass,manager,scene,master_cycles\n",sourceTiming);
 /* Count the original instructions on the same Genesis 68000 timing model.
  * This retains oracle sleep/stop interception and mapped CPS memory, so it
  * excludes display/audio/interrupt integration just like the native CPU
  * profile. It is a translation-cost baseline, not a playable-ROM FPS test. */
#define RUN_SOURCE_BOUNDARY() do{ \
 if(sourceCycleMode){ \
  cpu->cycles=0;cpu->refresh=896;unsigned instructions=0; \
  while(m68k_get_reg(16)!=0x100000&&instructions++<4000000)m68k_run(cpu->cycles+1); \
  if(cpu->cycles<0||m68k_get_reg(16)!=0x100000)return 2; \
  sourceTickCycles+=(unsigned)cpu->cycles; \
 }else for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
}while(0)
 unsigned opCount[3]={0},opMax[3]={0};unsigned long long opTotal[3]={0},timingTotal=0;
 FILE*timing=cycleMode?fopen("build/forest-timing.csv","w"):NULL;
 if(cycleMode&&!timing)return 2;
 if(timing)fputs("sample,pass,manager,scene,master_cycles\n",timing);
#define TARGET_CHECK(operation,wanted) do{ \
 int savedCycles=cpu->cycles,savedRefresh=cpu->refresh;unsigned savedEnd=cpu->end; \
 unsigned regs[18];for(unsigned i=0;i<18;i++)regs[i]=m68k_get_reg(i); \
 memcpy(oracleAfter,ram,65536); \
 for(unsigned i=0;i<65536;i++)ram[i^1]=nativeInput[i]; \
 willowMotionPutWord(sram+targetAddress[0],operation); \
 willowMotionPutWord(sram+targetAddress[2],0xdead); \
 willowMotionPutLong(sram+0xff00,0x100000); \
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0x20ff00);m68k_set_reg(16,targetEntry); \
 if(cycleMode){ \
  cpu->cycles=0;cpu->refresh=896;unsigned instructions=0; \
  while(m68k_get_reg(16)!=0x100000&&instructions++<4000000){ \
   unsigned at=m68k_get_reg(16),before=cpu->cycles;m68k_run(cpu->cycles+1); \
   if(at>=0x100000&&at<0x200000){unsigned index=(at-0x100000)/2,cost=(unsigned)cpu->cycles-before;cycleProfile[index]+=cost;if(sample==profileSample&&pass==profilePass)resetProfile[index]+=cost;} \
  } \
  if(cpu->cycles<0)return 2;unsigned cost=cpu->cycles; \
  tickCycles+=cost;opTotal[operation]+=cost;opCount[operation]++;if(cost>opMax[operation])opMax[operation]=cost; \
 }else for(unsigned slice=0;slice<128&&m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000||willowMotionWord(sram+targetAddress[2])!=(wanted)){fprintf(stderr,"Target boundary failed op %u region %u sample %u pass %u PC %x status %x\n",operation,region,sample,pass,m68k_get_reg(16),willowMotionWord(sram+targetAddress[2]));return 1;} \
 if(m68k_get_reg(15)!=0x20ff04)return 1; \
 for(unsigned i=0xbf00;i<0xc000;i++)if(sram[i]!=0xa5)return 1; \
 for(unsigned i=0;i<65536;i++)if(ram[i^1]!=expected[i]){fprintf(stderr,"Target RAM op %u region %u sample %u pass %u at %x actual %x expected %x\n",operation,region,sample,pass,i,ram[i^1],expected[i]);return 1;} \
 if(willowMotionWord(sram+targetAddress[3])!=d4||willowMotionLong(sram+targetAddress[4])!=a4||willowMotionWord(sram+targetAddress[5])!=eventWait||willowMotionWord(sram+targetAddress[6])!=flow.task.pc||willowMotionWord(sram+targetAddress[6]+2)!=flow.manager.target||willowMotionWord(sram+targetAddress[6]+4)!=flow.entry.pc||willowMotionWord(sram+targetAddress[6]+6)!=flow.route.pc||willowMotionWord(sram+targetAddress[6]+8)!=flow.initialD1||willowMotionWord(sram+targetAddress[6]+10)!=flow.initialD1Known||willowMotionWord(sram+targetAddress[6]+12)!=flow.scene.pc||willowMotionWord(sram+targetAddress[6]+14)!=flow.scene.target||willowMotionWord(sram+targetAddress[6]+16)!=flow.shop.pc)return 1; \
 for(unsigned i=0;i<16;i++)if(willowMotionWord(sram+targetAddress[7]+i*2)!=runtime.resume[i])return 1; \
 if(memcmp(sram+0x4000,scroll2,sizeof scroll2)){fprintf(stderr,"Target shadow map op %u sample %u pass %u\n",operation,sample,pass);return 1;} \
 if(sram[targetAddress[8]]!=actors.phase||sram[targetAddress[8]+2]!=actors.pickups.nextSlot||willowMotionLong(sram+targetAddress[8]+4)!=actors.pickups.transition.resume||willowMotionWord(sram+targetAddress[8]+8)!=actors.pickups.transition.wait||willowMotionWord(sram+targetAddress[8]+10)!=actors.playerScene.resume||willowMotionWord(sram+targetAddress[8]+12)!=actors.playerWait)return 1; \
 memcpy(ram,oracleAfter,65536);m68k_set_reg(17,regs[17]);for(unsigned i=0;i<17;i++)m68k_set_reg(i,regs[i]);if(cycleMode){cpu->cycles=savedCycles;cpu->refresh=savedRefresh;cpu->end=savedEnd;}targetChecks++; \
}while(0)
 unsigned checks=0,waits=0,completions=0,descentUpdates=0;
#ifdef WILLOW_TEST_FIRST_LEVEL
 unsigned playTicks=0,maxPlayCamera=0,lifeLosses=0,scoreTasks=0;
#ifdef WILLOW_TEST_ENDING_FIXTURE
 unsigned endingInjected=0,endingFinished=0;
#ifdef WILLOW_TEST_ENDING_CREDITS_FIXTURE
 uint32_t endingStates=0;
#endif
#endif
#ifdef WILLOW_TEST_SHOP_ENTRY
 unsigned shopWaits=0,shopEntries=0;
#ifdef WILLOW_TEST_SHOP_EXIT
 unsigned shopInputTick=0,shopExits=0;
#ifdef WILLOW_TEST_STAGE_CLEAR_ENTRY
 unsigned resultsEntries=0;
#ifdef WILLOW_TEST_NEXT_STAGE_PLAY
 unsigned nextPlayTicks=0;
#ifdef WILLOW_TEST_STAGE2_PLAY
 unsigned thirdPlayTicks=0,thirdMaxX=0;
#ifdef WILLOW_TEST_STAGE3_PLAY
 unsigned fourthPlayTicks=0,fourthMaxX=0;
#ifdef WILLOW_TEST_STAGE4_PLAY
 unsigned fifthPlayTicks=0,fifthMaxX=0;
#ifdef WILLOW_TEST_STAGE5_PLAY
 unsigned sixthPlayTicks=0,sixthMaxX=0,sixthSections=0;
#ifdef WILLOW_TEST_STAGE5_COMPLETE
 uint32_t campaignEndingStates=0;
#endif
#ifdef WILLOW_TEST_STAGE5_TOWER
 unsigned towerRoute=0,towerLaunch=0;
#endif
#ifndef WILLOW_TEST_STAGE5_PLAY_TICKS
#define WILLOW_TEST_STAGE5_PLAY_TICKS 2048
#endif
#endif
#ifdef WILLOW_TEST_STAGE4_ROUTE
 unsigned fifthRoute=0;
#endif
#endif
#endif
#endif
#ifdef WILLOW_TEST_MADMARTIGAN_SCENE
 unsigned sceneInjected=0,sceneOutWaits=0,sceneInWaits=0;
#endif
#ifdef WILLOW_TEST_NEXT_STAGE_ROUTE
 unsigned stageRouteMaxX=0;
#endif
#endif
#endif
#endif
#endif
 for(unsigned sample=0;sample<1;sample++){
#else
 for(unsigned sample=0;sample<4;sample++){
#endif
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31);
  wb(0xf9a3,5);wb(0x8288,3); /* Valid pre-game health/lives configuration. */
  ww(0x8000,0);ww(0x8004,0);ww(0x8008,0);
  wb(0x8058,0);wb(0x8068,0);ww(0x8070,10000);wb(0x82a0,sample&1);wb(0x8023,0);
  wb(0x8067,0);ww(0x828a,0);ww(0x828c,0x183c);
  ww(0x801a,(sample*4)&252);ww(0x801e,(sample*2)&126);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);
  WillowNativeTaskRuntime runtime={0};messageContextSlots=0;
  memset(expected,0,512);wb(0x020e,0);wb(0x828f,0);
  willowArcadeTaskInitFree(&bus,romWord);
  for(unsigned n=0;n<16;n++){ww(n*32+12,0xff);ww(n*32+14,0x1100+n*256);}
  for(unsigned n=0;n<128;n++)ww(0x8072+n*2,65535);
  ww(0x801c,rw(0x801a));willowArcadeTaskStart(&bus,0x60,0x1ddc,0,0,1);
  ww(0x82c0,0);wb(0xf890,0);wb(0xf891,0);
  for(unsigned n=0;n<12;n++)ww(0x82a8+n*2,0);
  for(unsigned n=0;n<3072;n++)ww(0x2000+n*2,(uint16_t)(((n/16+sample)%16)*0x1000)|((n*17)&4095));
#ifdef WILLOW_TEST_FIRST_LEVEL
  /* Normal play option state: the dirty-RAM opening fixture otherwise leaves
   * F99E nonzero and disables player damage (39A52). 1A9C clears it when
   * service mode is off; decode other pre-game settings from source tables. */
  /* 928A..92BC immediately precedes creation of the 92D2 game task. */
  for(uint16_t at=0x8000;at<0x8014;at+=2)ww(at,0);
  wb(0x8293,0);wb(0xf99f,0);wb(0x82a3,0);
  wb(0x8066,0);wb(0x8067,0);wb(0x8068,0);wb(0xf99e,0);
#ifdef WILLOW_TEST_SHOP_ENTRY
  /* Source service-mode option 1A9C..1ABC isolates boss/shop traversal.
   * Explicitly NOT evidence of normal-play survival. */
  wb(0x8068,0x88);wb(0xf99e,1);
#endif
  if(!willowArcadeBootOptions(&bus,0x1ac2,romWord)||
     !willowArcadeBootOptions(&bus,0x1b46,romWord)||
     !willowArcadeBootOptions(&bus,0x1b96,romWord))return 2;
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d4=sample*977,eventWait=0;uint32_t a4=0x123456;
  WillowNativeGameplayFlow flow={0};
  memset(sram+0x800,0,0x1800);memset(sram+0xbf00,0xa5,256);
  memcpy(sram+0x4000,scroll2,sizeof scroll2);
  willowMotionPutWord(sram+targetAddress[1],region);
  willowMotionPutWord(sram+targetAddress[3],d4);willowMotionPutLong(sram+targetAddress[4],a4);
  WillowPaletteAnimation palette={0};palette.read=colorRead;palette.write=colorWrite;
  palette.byte=romByte;palette.word=romWord;palette.allocate=allocate;palette.allocateWithA4=allocateWithA4;
  palette.random=randomByte;palette.sound=sound;palette.soundWithA4=soundWithA4;
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};WillowArcadeActorUpdate actors={0};WillowArcadeSpriteFrame frame;
  renderD4=&d4;renderA4=&a4;
  const WillowTransitionPresentationBus presentation={romByte,romWord,colorWrite,write2};
  WillowArcadeGameplayServices services={.sourceA5=0xff8000,.word=sourceWord,.collision=&collision,.actors=&actors,.player=playerUpdate,.playerContext=&services,
   .presentation=&presentation,.videoWrite=videoWord,.textWrite=videoWord,.selectionRegister=&selectionRegister,.draw=drawFrame,.spriteWord=videoWord,.frame=&frame,.palette=&palette,.resolve=resolve,.byte=romByte,.write2=write2,.write3=write3,.bus=&bus,.d4=&d4,.a4=&a4,.eventWait=&eventWait,.busWord=romWord,.mapClearWrite=videoWord};
  WillowNativeGameplayFlowBinding binding={&flow,&bus,romWord,&a4,willowArcadeGameplayServiceResume,&services};
  uint32_t pc=0x92d2,sp=0xffff00;
  unsigned mainRegisters[15]={0};mainRegisters[13]=0xff8000;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  uint8_t sawSky=0;unsigned caseDescent=0;
#ifdef WILLOW_TEST_FIRST_LEVEL
  uint8_t playing=0;unsigned casePlay=0;
#ifdef WILLOW_TEST_NEXT_STAGE_ROUTE
  unsigned stageRoute=0;
#ifdef WILLOW_TEST_NEXT_STAGE_GAP
  unsigned stageRouteWait=0,stageRouteFlight=0;
#endif
#endif
#endif
  for(unsigned pass=0;pass<206000;pass++){
#ifdef WILLOW_TEST_FIRST_LEVEL
   const uint8_t keys[]={1,1,0x11,1,0x21,1,0x11,1};
   uint8_t input=playing?keys[(casePlay/16)%8]:0;
#ifdef WILLOW_TEST_ENDING_FIXTURE
   /* Controlled post-credits fixture, NOT a campaign-completion claim.
    * Install the same manager entry in the native/host/original runs;
    * from here actor/message/fade tasks execute normally. */
   if(playing&&!endingInjected&&flow.task.pc==0x932c&&!flow.manager.target&&!actors.phase){
    ww(0x8344,5);ww(0x8374,6);ww(0x8004,4);ww(0x8008,20);ww(0x800c,14);
    wb(0x83a2,1);wb(0x83c3,2);wb(0x83f6,2);wb(0x83f7,3);wb(0x0e90,3);ww(0x0e8a,6);
#ifdef WILLOW_TEST_ENDING_CREDITS_FIXTURE
    /* Controlled entry after the final boss's death completion (83F6=2).
     * This does not test the boss death sequence that normally sets it. */
    ww(0x800c,0);wb(0x83f7,0);wb(0x0e90,0);ww(0x0e8a,0);
#endif
    endingInjected=1;
   }
#ifdef WILLOW_TEST_ENDING_CREDITS_FIXTURE
   if(endingInjected&&rw(0x8004)==4&&rw(0x8008)==20&&rw(0x800c)<=38&&!(rw(0x800c)&1))
    endingStates|=1UL<<(rw(0x800c)/2);
#endif
   input=0;
#endif
#ifdef WILLOW_TEST_BOSS_INPUT
   /* Test controller inputs aim toward the active first boss record, with
    * alternating charged attacks and high jumps; no actor state edits. */
   if(playing&&casePlay>=1200){
    uint16_t boss=0;
    for(unsigned slot=0;slot<50;slot++){
     uint16_t at=0x9dc6+slot*128;
     if(rb(at)&&rb(at+32)==7&&rb(at+33)==2&&rb(at+2)){boss=at;break;}
    }
#ifdef WILLOW_TEST_SHOP_PROGRESS
    /* The final forest boss has a separate damageable head (selector 10).
     * Aim at that live record after the shop, using controller input only. */
    if(shopExits)for(unsigned slot=0;slot<50;slot++){
     uint16_t at=0x9dc6+slot*128;
     if(rb(at)&&rb(at+32)==10&&rb(at+2)==2&&rb(at+42)){boss=at;break;}
    }
#endif
    int delta=boss?(int16_t)(rw(boss+6)-rw(0x984c)):1;
    if(rb(0x83e7)&&(!boss||rb(boss+32)==7))delta=1;
    input=delta<0?2:1;
    if(casePlay%96<80)input|=16;
    if(casePlay%96<16)input|=40;
#ifdef WILLOW_TEST_SHOP_ENTRY
    if(boss&&(!rb(0x83e7)||rb(boss+32)==10)){
     input=delta<0?2:1;
     if(delta>-96&&delta<96&&((rb(0x9855)&1)==(delta<0)))input=0;
     if(casePlay%64<48)input|=16;
     if(casePlay%64<8)input|=40;
#ifdef WILLOW_TEST_SHOP_PROGRESS
     if(rb(boss+32)==10){
      int target=(int)rw(boss+6)-128,player=rw(0x984c);
      input=player>target+8?2:player<target-8?1:(rb(0x9855)&1)?1:0;
      unsigned phase=casePlay%64;
      /* Release the charged shot during ascent rather than after landing. */
      if(phase<16||phase>=20)input|=16;
      if(phase<8)input|=40;
     }
#endif
     if(!(casePlay%256))printf("Boss %04x HP %u at %u/%u state %u/%u/%u\n",boss,rb(boss+42),rw(boss+6),rw(boss+10),rb(boss+2),rb(boss+3),rb(boss+4));
    }
#endif
   }
#endif
#ifdef WILLOW_TEST_LEDGE_INPUT
   /* C7EC uses held-up for the high jump; C8xx requires it to grab ledges.
    * Change controller input only, retaining the baseline through tick 1200. */
   if(playing&&casePlay>=1200)input=9|((casePlay%64)<16?32:0);
#endif
#ifdef WILLOW_TEST_SHOP_EXIT
   if(rw(0x8008)==4){
    input=0;
    if(shopEntries&&rw(0x800c)==2&&!rw(0x8010)){
     input=shopInputTick==1?1:shopInputTick==3?16:0;shopInputTick++;
    }
   }
#endif
#ifdef WILLOW_TEST_NEXT_STAGE_LEDGE
   /* Hold up throughout the jump to exercise the source ledge-grab gate;
    * change controller input only, after the forest route has finished. */
   if(playing&&rw(0x8344)==1){
    input=9;
    if(casePlay%64<16)input|=32;
    if(casePlay%64<48)input|=16;
   }
#endif
#ifdef WILLOW_TEST_NEXT_STAGE_ROUTE
   /* The source map has a solid roof at x=640..752, y=352. Jump through
    * the one-way section to its left before proceeding over the roof. */
   if(playing&&rw(0x8344)==1){
    if(rw(0x8004)==2&&rw(0x8008)==2&&rb(0x9866)==4&&rb(0x9848)==2){
     if(rw(0x984c)>stageRouteMaxX)stageRouteMaxX=rw(0x984c);
     if(!stageRoute&&rw(0x984c)>=700&&rw(0x9850)>=280)stageRoute=1;
     if(stageRoute==1&&rw(0x984c)<=608)stageRoute=2;
     if(stageRoute==2&&rw(0x9850)>=375)stageRoute=3;
#ifdef WILLOW_TEST_NEXT_STAGE_SECOND_ROOF
     if(stageRoute==3&&rw(0x984c)>=1480&&rw(0x9850)>=195&&rb(0x986a)&&!rb(0x9849))stageRoute=4;
     if(stageRoute==4&&rw(0x984c)<=1040)stageRoute=5;
     if(stageRoute==5&&rw(0x9850)>=350&&rb(0x986a)&&!rb(0x9849))stageRoute=6;
#endif
#ifdef WILLOW_TEST_NEXT_STAGE_GAP
     if(stageRoute==3&&rw(0x984c)>=818)stageRoute=4;
     if(stageRoute==4&&++stageRouteWait>2&&rb(0x986a)&&!rb(0x9849))stageRoute=5;
     if(stageRoute==5&&rw(0x984c)>=1032)stageRoute=6;
     if(stageRoute==6&&rw(0x9850)>=355)stageRoute=7;
#endif
    }
#ifdef WILLOW_TEST_NEXT_STAGE_GAP
    input=stageRoute==1?10:(stageRoute==2||stageRoute==4||stageRoute==6)?8:9;
    if(stageRoute==5){if(!stageRouteFlight++)input|=32;}
    else if(stageRoute!=1&&stageRoute!=4&&casePlay%64<16)input|=32;
    if((stageRoute==0||stageRoute==3||stageRoute==7)&&casePlay%64<48)input|=16;
#else
#ifdef WILLOW_TEST_NEXT_STAGE_SECOND_ROOF
    input=(stageRoute==1||stageRoute==4)?10:(stageRoute==2||stageRoute==5)?8:9;
    if(stageRoute!=1&&casePlay%64<16)input|=32;
    /* Jump before the feet leave the roof at x=1136; the periodic jump
     * otherwise arrives after C72E has already selected falling. */
    if(stageRoute==4&&rw(0x984c)>=1140&&rw(0x984c)<=1150&&rb(0x986a)&&!rb(0x9849))input|=32;
    if((stageRoute==0||stageRoute==3||stageRoute==6)&&casePlay%64<48)input|=16;
#else
    input=stageRoute==1?10:stageRoute==2?8:9;
    if(stageRoute!=1&&casePlay%64<16)input|=32;
    if((stageRoute==0||stageRoute==3)&&casePlay%64<48)input|=16;
#endif
#endif
   }
#endif
#ifdef WILLOW_TEST_NEXT_STAGE_SECOND_ROOF
   if(rw(0x8344)==1&&stageRoute>=4&&casePlay<9900&&!(casePlay%8))
    printf("Roof input tick %u phase %u x/y %u/%u player %u/%u contact %u material %x velocity %u input %x held %x pressed %x\n",casePlay,stageRoute,rw(0x984c),rw(0x9850),rb(0x9849),rb(0x984a),rb(0x986a),rb(0x988c),rw(0x987e),input,rb(0x806a),rb(0x0e92));
#endif
#ifdef WILLOW_TEST_STAGE2_PLAY
   if(playing&&rw(0x8344)==2){
    input=9;
    if(casePlay%64<16)input|=32;
    if(casePlay%64<48)input|=16;
#ifdef WILLOW_TEST_STAGE2_COMPLETE
    if(rw(0x8408)>=5500)for(unsigned slot=0;slot<8;slot++){
     uint16_t at=0xf490+slot*128;
     if(!rb(at)||rb(at+32)!=2||rb(at+2)>=4||!rb(at+42))continue;
     /* Original map has floor from x=5824 through 6112; chasing the
      * boss off its left edge triggers the service-option rebound. */
     int target=6048,x=rw(0x984c);unsigned facing=x>rw(at+6);
     input=x>target+8?2:x<target-8?1:((rb(0x9855)&1)!=facing)?(facing?2:1):0;
     unsigned phase=casePlay%64;if(phase<8)input|=32;if(phase<16||phase>=20)input|=16;
     if(!(casePlay%128))printf("Stage-two boss %x HP %u position %u/%u state %u/%u player %u/%u\n",at,rb(at+42),rw(at+6),rw(at+10),rb(at+2),rb(at+3),rw(0x984c),rw(0x9850));
     break;
    }
#endif
   }
#endif
#ifdef WILLOW_TEST_STAGE3_PLAY
   if(playing&&rw(0x8344)==3){
    input=9;if(casePlay%64<16)input|=32;if(casePlay%64<48)input|=16;
#ifdef WILLOW_TEST_STAGE3_COMPLETE
    if(rw(0x8408)>=3400)for(unsigned slot=0;slot<8;slot++){
     uint16_t at=0xf490+slot*128;
     unsigned kind=rb(at+32);
     if(!rb(at)||(kind!=5&&kind!=3)||rb(at+33)||!rb(at+42))continue;
     if(kind==5&&rb(at+2)!=2)continue;
     unsigned facing=rw(0x984c)>rw(at+6);
     if(kind==5){
      input=8;if((rb(0x9855)&1)!=facing)input|=facing?2:1;
      if(casePlay%64<16)input|=32;
     }else{
      int distance=(int)rw(at+6)-(int)rw(0x984c);
      input=distance>40?1:distance< -40?2:((rb(0x9855)&1)!=facing)?(facing?2:1):0;
     }
     if(casePlay%64<48)input|=16;
     if(!(casePlay%128))printf("Stage-three boss %x HP %u position %u/%u state %u/%u player %u/%u\n",at,rb(at+42),rw(at+6),rw(at+10),rb(at+2),rb(at+3),rw(0x984c),rw(0x9850));
     break;
    }
#endif
   }
#endif
#ifdef WILLOW_TEST_STAGE4_PLAY
   if(playing&&rw(0x8344)==4){
    input=9;if(casePlay%64<16)input|=32;if(casePlay%64<48)input|=16;
#ifdef WILLOW_TEST_STAGE4_ROUTE
    /* The ROM terrain has a lower descending ledge at x992..1488,
     * an upper ledge beginning at x992/y336, and a wall at x1488.
     * Backtrack along the lower ledge to jump onto the upper route.
     * Only controller inputs change; all three executions share them. */
    if(rw(0x8004)==2&&rw(0x8008)==2){
     if(!fifthRoute&&rw(0x984c)>=1440&&rb(0x986a)&&!rb(0x9849))fifthRoute=1;
     if(fifthRoute==1&&rw(0x984c)<=1040)fifthRoute=2;
     if(fifthRoute==2&&rw(0x9850)>=360&&rb(0x986a)&&!rb(0x9849))fifthRoute=3;
     if(fifthRoute==3&&rw(0x984c)>=4780&&rb(0x986a)&&!rb(0x9849))fifthRoute=4;
     if(fifthRoute==4&&rw(0x984c)<=4624)fifthRoute=5;
     if(fifthRoute==5&&rw(0x984c)>=4752&&rw(0x9850)<=100&&rb(0x986a)&&!rb(0x9849))fifthRoute=6;
     if(fifthRoute==6&&rw(0x984c)>=4864)fifthRoute=7;
     input=fifthRoute==1?10:fifthRoute==2?8:9;
     if(casePlay%64<16)input|=32;
     if((fifthRoute==0||fifthRoute>=3)&&casePlay%64<48)input|=16;
     /* 318E0's falling pillars reset eight updates after destruction.
      * Try walking and firing horizontally through their opening; the
      * prior up/jump pattern stalled at the first pillar. */
     if(fifthRoute==3&&rw(0x984c)>=2720&&rw(0x984c)<3360){
      input=1;if(casePlay%16<8)input|=16;
     }
     /* These ledges have material0C, so down+jump slides rather than
      * dropping. Leave both ledges on the left, descend onto the low
      * floor, then jump through the wall's y112..160 opening. */
     if(fifthRoute==4)input=2;
     if(fifthRoute==5)input=1;
     if(fifthRoute==7)for(unsigned slot=0;slot<8;slot++){
      uint16_t at=0xf490+slot*128;
      if(!rb(at)||rb(at+32)!=6||rb(at+33)<2||rb(at+2)!=2||!rb(at+42))continue;
      /* Stay on the arena floor to the left of the two moving heads,
       * face the live target and use the ordinary up-jump/charge inputs. */
      int x=rw(0x984c),target=5584;unsigned facing=x>rw(at+6);
      input=x>target+8?2:x<target-8?1:((rb(0x9855)&1)!=facing)?(facing?2:1):0;
      input|=8;if(casePlay%64<16)input|=32;if(casePlay%64<48)input|=16;
      break;
     }
     if(!(casePlay%128)&&rw(0x984c)>4000)for(unsigned slot=0;slot<8;slot++){
      uint16_t at=0xf490+slot*128;
      if(rb(at)&&rb(at+2))printf("Fifth boss %x kind %u/%u HP %u state %u/%u XY %u/%u player %u/%u\n",at,rb(at+32),rb(at+33),rb(at+42),rb(at+2),rb(at+3),rw(at+6),rw(at+10),rw(0x984c),rw(0x9850));
     }
     if(!(casePlay%64))printf("Fifth route %u X/Y %u/%u player %u/%u grounded %u\n",fifthRoute,rw(0x984c),rw(0x9850),rb(0x9849),rb(0x984a),rb(0x986a));
    }
#endif
   }
#endif
#ifdef WILLOW_TEST_STAGE5_PLAY
   if(playing&&rw(0x8344)==5){
    input=9;if(casePlay%64<16)input|=32;if(casePlay%64<48)input|=16;
#ifdef WILLOW_TEST_STAGE5_COMPLETE
    if(rw(0x8004)==4&&rw(0x8008)==20&&rw(0x800c)<=38&&!(rw(0x800c)&1))
     campaignEndingStates|=1UL<<(rw(0x800c)/2);
    if(rw(0x8374)==6){
     for(unsigned n=0;n<8;n++){
      uint16_t at=0xf490+n*128;
      if(!rb(at)||rb(at+32)!=4)continue;
      int x=rw(0x984c),target=1176;unsigned facing=x>rw(at+6);
      input=x>target+2?2:x<target-2?1:((rb(0x9855)&1)!=facing)?(facing?2:1):0;
      /* C7EC's high jump peaks after 26 updates; CA3E/CAB0 delays
       * projectile creation after release. Charge between jumps, release
       * near the apex, and preserve the original routines' shot timing. */
      unsigned phase=casePlay%80;
      input|=8;if(phase<2)input|=32;if(phase<20||phase>=40)input|=16;
      if(!(casePlay%128))printf("Final boss %x HP %u state %u/%u XY %u/%u player %d/%u death %u\n",at,rb(at+42),rb(at+2),rb(at+3),rw(at+6),rw(at+10),x,rw(0x9850),rb(0x83f6));
      if(!(casePlay%8))printf("Final boss aim phase %u player Y %u attack %u charge %u boss Y %u HP %u input %02x\n",phase,rw(0x9850),rb(0x984b),rb(0x9877),rw(at+10),rb(at+42),input);
      break;
     }
    }
#endif
#ifdef WILLOW_TEST_STAGE5_FIRST_PORTAL
    /* 40B9A portal zero is at 2240/72. Source 2D7EA admits Up only
     * within 12 horizontal and 14 vertical pixels. Approach on foot so
     * the test controller does not jump over the admission rectangle. */
    if(rw(0x8374)==0&&rw(0x984c)>=2160){
     input=rw(0x984c)<2236?1:rw(0x984c)>2244?2:8;
    }
#endif
#ifdef WILLOW_TEST_STAGE5_ROUTE
    /* The forward destinations are portal variants 0, 1, 3 and 7.
     * These source table coordinates steer inputs only; no state writes. */
    if(rw(0x8374)<4){
     static const uint16_t portalX[]={2240,2112,2048,2240};
     unsigned target=portalX[rw(0x8374)],x=rw(0x984c);
     if(x>=target-80)input=x<target-4?1:x>target+4?2:8;
    }
#endif
#ifdef WILLOW_TEST_STAGE5_TOWER
    if(rw(0x8374)==5){
     /* Source Scroll2 floor spans form the tower's alternating ledges.
      * These are input waypoints; position, contact and camera stay under
      * the original player/collision routines. */
     static const uint16_t ledges[][2]={
      {3792,256},{3376,384},{3420,624},{3576,832},{3608,832},{3748,896},
      {3556,1056},{3420,1152},{3608,1280},{3618,1312},
      {3550,1424},{3618,1536},{3550,1568},{3618,1680},
      {3550,1792},{3618,1824},{3550,1936},{3618,2048},{3584,2130}};
     int x=rw(0x984c),y=rw(0x9850),target=ledges[towerRoute][0];
     const unsigned lastLedge=sizeof(ledges)/sizeof(ledges[0])-1;
     /* A grabbed ledge can set contact before its climb raises Y. Only
      * retry after a genuine grounded fall below the middle platform. */
     if(towerRoute==3&&y<550&&rb(0x986a)&&!rb(0x9849)){towerRoute=1;towerLaunch=0;target=ledges[towerRoute][0];}
     if(towerRoute>=10&&y<1200&&rb(0x986a)&&!rb(0x9849)){towerRoute=0;towerLaunch=0;target=ledges[towerRoute][0];}
     if(towerRoute<lastLedge&&x>=target-2&&x<=target+2&&y>=ledges[towerRoute][1]&&rb(0x986a)&&!rb(0x9849)){
      towerRoute++;target=ledges[towerRoute][0];towerLaunch=18;
     }
     input=(x<target-2?1:x>target+2?2:0)|8;
     if(towerRoute==4)towerLaunch=0; /* Walk to the upper ledge's edge. */
     else if(towerLaunch){if(towerLaunch<=16)input|=32;towerLaunch--;}
     else if(casePlay%64<16)input|=32;
     if(towerRoute==lastLedge&&x>=3572&&x<=3596&&y>=2130&&y<=2158)input=8;
     if(!(casePlay%128))printf("Tower route %u target %d X/Y %d/%d ground %u\n",towerRoute,target,x,y,rb(0x986a));
     if(towerRoute==3&&!(casePlay%8))printf("Tower gap trace X/Y %d/%d player %u/%u/%u ground %u speed %d gravity %u launch %u input %02x\n",x,y,rb(0x9848),rb(0x9849),rb(0x984a),rb(0x986a),(int16_t)rw(0x987e),rw(0x987a),towerLaunch,input);
     if(towerRoute>=9&&!(casePlay%8))printf("Upper tower trace route %u X/Y %d/%d player %u/%u contact %u input %02x\n",towerRoute,x,y,rb(0x9849),rb(0x984a),rb(0x986a),input);
    }
#endif
    if(!(casePlay%128))printf("Final-stage section %u X/Y %u/%u manager %u/%u\n",rw(0x8374),rw(0x984c),rw(0x9850),rw(0x8004),rw(0x8008));
    if(rw(0x8374)==4&&!(casePlay%128))for(unsigned n=0;n<50;n++){
     uint16_t at=0x9dc6+n*128;
     if(rb(at)&&rb(at+32)==24)printf("Final-stage arena boss %x HP %u state %u/%u XY %u/%u special %u\n",at,rb(at+42),rb(at+2),rb(at+3),rw(at+6),rw(at+10),rb(0x83e7));
    }
   }
#endif
   wb(0x8060,input);wb(0x8062,input);
   uint8_t oldLives=rb(0x8342),wasPlaying=playing;
   if(playing&&!(casePlay%256)){printf("First-level continuous: %u ticks camera %u position %u/%u lives %u HP %u state %u/%u/%u\n",casePlay,rw(0x8408),rw(0x984c),rw(0x9850),rb(0x8342),rb(0x8362),rw(0x8000),rw(0x8004),rw(0x8008));printf("Player dispatch %u/%u/%u ground %u held %02x pressed %02x stage %u world %u scroll-phase %u\n",rb(0x9848),rb(0x9849),rb(0x984a),rb(0x986a),rb(0x806a),rb(0x0e92),rw(0x8344),rw(0x8398),rb(0x8405));fflush(stdout);}
#endif
#ifdef WILLOW_TEST_SHOP_ENTRY
   if(flow.shop.pc)shopWaits++;
#endif
#ifdef WILLOW_TEST_MADMARTIGAN_SCENE
   /* Synthetic actor-25 countdown entry, applied equally to all three runs.
    * Original 3591E expires it and selects player state 20. This does not
    * claim a naturally reached scene or bypass the command-task fades. */
   if(!sceneInjected&&rw(0x8344)==1&&rw(0x8004)==2&&rw(0x8008)==2&&
      rb(0x9866)==4&&rb(0x9848)==2&&flow.task.pc==0x932c&&!actors.phase){
    if(!rb(0x980d))return 1;
    uint16_t cursor=rw(0x9834),at=rw(cursor);
    ww(0x9834,cursor+2);wb(0x980d,rb(0x980d)-1);
    for(unsigned n=0;n<64;n++)wb(at+n,0);
    wb(at,1);wb(at+2,4);wb(at+30,12);wb(at+32,25);wb(at+34,1);
    sceneInjected=1;
    printf("Synthetic actor-25 timer expiry at pass %u, record %x\n",pass,at);
   }
#endif
   tickCycles=0;
   sourceTickCycles=0;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n))ram[n^1]=expected[n];
   firstLevelPointers(ram,1);
   /* Intercepted main sleep must retain the register set that real 1750 saves. */
   for(unsigned i=0;i<15;i++)m68k_set_reg(i,mainRegisters[i]);
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,sp);
   m68k_set_reg(12,a4);m68k_set_reg(4,d4);m68k_set_reg(16,pc);

   memcpy(nativeInput,expected,65536);
   uint16_t previousY=rw(0x840c);
   WillowTransitionStep result=willowNativeGameplayFlowStepWithShop(&binding);
#ifdef WILLOW_TEST_SELECTION_NUMBER_TRACE
   if(!result.supported&&(flow.scene.pc==0xa522||flow.scene.pc==0xa590)){
    cpu->cycles=0;cpu->refresh=896;
    for(unsigned step=0;step<1000000&&m68k_get_reg(16)!=0xa5be;step++)m68k_run(cpu->cycles+1);
    fprintf(stderr,"Selection count source entry pass %u PC %x D2 %08x timer %u\n",pass,m68k_get_reg(16),m68k_get_reg(2),rw(0x800a));
    FILE*trace=fopen("build/native-shop-entry-investigation/selection-number-entry-ram.bin","wb");
    if(trace){for(unsigned n=0;n<65536;n++)fputc(ram[n^1],trace);fclose(trace);}
   }
#endif
   if(!result.supported||result.finished||result.delay!=1){fprintf(stderr,"Unsupported main sample %u pass %u state %x scene %x actor %u manager %x route %x entry %x shop %x outer %x state %x timer %x\n",sample,pass,rw(0x8008),flow.scene.pc,actors.phase,flow.manager.target,flow.route.pc,flow.entry.pc,flow.shop.pc,rw(0x800c),rw(0x8010),rw(0x8012));return 1;}
#ifdef WILLOW_TEST_SELECTION_NUMBER_TRACE
   if(rw(0x8344)==4&&rw(0x8008)==36){
    cpu->cycles=0;cpu->refresh=896;
    for(unsigned step=0;step<1000000&&m68k_get_reg(16)!=0x100000;step++){
     unsigned address=m68k_get_reg(16),old=m68k_get_reg(2);m68k_run(cpu->cycles+1);
     if(old!=m68k_get_reg(2))fprintf(stderr,"Source D2 pass %u at %x: %08x -> %08x\n",pass,address,old,m68k_get_reg(2));
    }
   }else
#endif
   RUN_SOURCE_BOUNDARY();
   if(m68k_get_reg(16)!=0x100000)return 1;
   firstLevelPointers(ram,0);
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n]){
#ifdef WILLOW_TEST_FIRST_LEVEL
    FILE*dump=fopen("build/first-level-runtime-investigation/mismatch-before.bin","wb");
    if(dump){fwrite(nativeInput,1,65536,dump);fclose(dump);}
    dump=fopen("build/first-level-runtime-investigation/mismatch-native.bin","wb");
    if(dump){fwrite(expected,1,65536,dump);fclose(dump);}
    dump=fopen("build/first-level-runtime-investigation/mismatch-original.bin","wb");
    if(dump){for(unsigned k=0;k<65536;k++)fputc(ram[k^1],dump);fclose(dump);}
#endif
    fprintf(stderr,"Forest mismatch sample %u pass %u at %x got %x want %x state %x manager %x pc %x\n",sample,pass,n,ram[n^1],expected[n],rw(0x8008),rw(0x8004),pc);
    return 1;
   }
   if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Registers pass %u A4 %x/%x D4 %x/%x\n",pass,m68k_get_reg(12),a4,m68k_get_reg(4),d4);return 1;}
   if(rw(0x8344)==4&&rw(0x8004)==0&&rw(0x8008)>=34&&rw(0x8008)<=38&&selectionRegister.known&&
      (uint16_t)m68k_get_reg(2)!=selectionRegister.d2){
    fprintf(stderr,"Selection D2 pass %u got %x want %x state %x\n",pass,(uint16_t)m68k_get_reg(2),selectionRegister.d2,rw(0x8008));return 1;
   }
   for(unsigned i=0;i<15;i++)mainRegisters[i]=m68k_get_reg(i);

   TARGET_CHECK(0,flow.task.pc==0x932c?1:2);
   if(rw(0x8008)==8&&rw(0x840c)==512)sawSky=1;
   if(sawSky&&rw(0x840c)<previousY){caseDescent++;descentUpdates++;}
   checks++;
   sp=m68k_get_reg(15);
   if((m68k_get_reg(0)&65535)!=1)return 1;
   pc=((uint32_t)ram[(sp&65535)^1]<<24)|((uint32_t)ram[((sp+1)&65535)^1]<<16)|((uint32_t)ram[((sp+2)&65535)^1]<<8)|ram[((sp+3)&65535)^1];sp+=4;
   if(flow.task.pc==0x932c){
    if(pc!=0x932c||sp!=0xffff00||eventWait||flow.manager.target)return 1;
    if(rw(0x8004)==2&&rw(0x8008)==2
#ifdef WILLOW_TEST_FIRST_LEVEL
       &&!playing
#endif
       ){
     if(!sawSky||!caseDescent||rw(0x840c))return 1;
     completions++;
#ifdef WILLOW_TEST_FIRST_LEVEL
     playing=1;
#else
     break;
#endif
    }
   }else{
    uint16_t expectedWait=actors.playerWait?actors.playerWait:
      actors.pickups.transition.wait?actors.pickups.transition.wait:eventWait;
    if(pc!=expectedWait){fprintf(stderr,"Wait continuation %x expected event %x player %x portal %x pass %u\n",pc,eventWait,actors.playerWait,actors.pickups.transition.wait,pass);return 1;}
#ifdef WILLOW_TEST_MADMARTIGAN_SCENE
    if(actors.playerWait==0x1ea2)sceneOutWaits++;
    if(actors.playerWait==0x1ec4)sceneInWaits++;
#endif
    waits++;
   }
   memcpy(nativeInput,expected,65536);
   uint16_t cursor=0,remaining=15;
   for(;;){
#ifdef WILLOW_TEST_FIRST_LEVEL
    WillowTaskSelection selected=willowArcadeTaskSelect(&bus,cursor,remaining);
    if(selected.dispatch&&selected.state==12&&!rw(selected.slot+4)&&rw(selected.slot+6)==0x626a)scoreTasks++;
#endif
    WillowNativeTaskRun run=willowNativeTaskRunOneWithVideo(&runtime,&bus,cursor,remaining,romWord,colorRead,colorWrite,0,0,videoWord);
    if(run.unsupported){fprintf(stderr,"Unsupported scheduled slot %x PC %x pass %u scene %x entry %x:%x payload %x/%x\n",run.cursor,runtime.resume[run.cursor/32],pass,rw(0x8008),rw(run.cursor+4),rw(run.cursor+6),rw(run.cursor+16),rw(run.cursor+18));return 1;}
    if(!run.dispatched||run.remaining==65535)break;
    unsigned slot=run.cursor-32, resume=runtime.resume[slot/32];
    if(resume==0x7954||resume==0x7dfa)messageContextSlots|=1u<<(slot/32);
    else if(resume==0x7b5c)messageContextSlots&=~(1u<<(slot/32));
    cursor=run.cursor;remaining=run.remaining;
   }
   firstLevelPointers(ram,1);
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);
   m68k_set_reg(8,0xff0000);m68k_set_reg(0,15);m68k_set_reg(16,0x14d8);
   RUN_SOURCE_BOUNDARY();
   if(m68k_get_reg(16)!=0x100000)return 1;
   firstLevelPointers(ram,0);
   for(unsigned slot=0;slot<512;slot+=32){
    unsigned resume=runtime.resume[slot/32];
    if(resume!=0x7954&&resume!=0x7dfa)continue;
    unsigned sp=oracleLong(ram,slot+8)&65535;
    uint32_t actual[]={oracleLong(ram,sp+60),oracleLong(ram,sp+36),
     oracleLong(ram,sp+40),oracleLong(ram,sp+44),
     oracleLong(ram,sp+20)&65535,oracleLong(ram,sp+24)&65535};
    uint32_t wanted[]={resume,willowMotionLong(expected+slot+4),
     willowMotionLong(expected+slot+8),willowMotionLong(expected+slot+20),rw(slot+2),rw(slot+24)};
    for(unsigned j=2;j<4;j++)if(actual[j]>=0xff4000&&actual[j]<0xff4000+videoSize)
     actual[j]=actual[j]-0xff4000+videoBase;
    for(unsigned j=0;j<6;j++)if(actual[j]!=wanted[j]){
     fprintf(stderr,"Message saved context slot %x pass %u field %u got %x want %x SP %x\n",slot,pass,j,actual[j],wanted[j],sp);return 1;
    }
   }
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Scheduler mismatch region %u sample %u pass %u at %x got %x want %x\n",region,sample,pass,n,ram[n^1],expected[n]);return 1;
   }


   TARGET_CHECK(1,1);
   memcpy(nativeInput,expected,65536);
   willowArcadeTaskWake(&bus);
   putRamWord(ram,0xf000,0x10);putRamWord(ram,0xf002,0);
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);m68k_set_reg(16,0x1590);RUN_SOURCE_BOUNDARY();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n])return 1;

   TARGET_CHECK(2,1);
   if(sourceTiming)fprintf(sourceTiming,"%u,%u,%u,%u,%u\n",sample,pass,rw(0x8004),rw(0x8008),sourceTickCycles);
   if(cycleMode){
    timingTotal+=tickCycles;timingCount++;if(tickCycles>896040)timingOver++;
    if(tickCycles>timingMax){timingMax=tickCycles;maxPass=pass;maxSample=sample;maxState=rw(0x8008);}
    fprintf(timing,"%u,%u,%u,%u,%u\n",sample,pass,rw(0x8004),rw(0x8008),tickCycles);
   }
#ifdef WILLOW_TEST_FIRST_LEVEL
   if(wasPlaying){
    casePlay++;playTicks++;if(rw(0x8408)>maxPlayCamera)maxPlayCamera=rw(0x8408);
    if(rb(0x8342)<oldLives)lifeLosses++;
#ifdef WILLOW_TEST_ENDING_FIXTURE
    if(endingInjected&&rw(0x8004)==6&&!rw(0x8008)&&!flow.route.pc&&!eventWait){endingFinished=1;break;}
#endif
#ifdef WILLOW_TEST_SHOP_ENTRY
    if(!shopEntries&&rw(0x8008)==4&&rw(0x800c)==2&&rw(0x8010)==0&&!flow.shop.pc&&!eventWait){shopEntries++;
#ifndef WILLOW_TEST_SHOP_EXIT
     break;
#endif
    }
#ifdef WILLOW_TEST_SHOP_EXIT
    if(!shopExits&&shopEntries&&shopInputTick>3&&!rw(0x800c)&&!rw(0x8010)&&!flow.shop.pc&&!eventWait){shopExits++;
#ifndef WILLOW_TEST_SHOP_PROGRESS
     break;
#endif
    }
#endif
#endif
#ifdef WILLOW_TEST_STAGE_CLEAR_ENTRY
#ifdef WILLOW_TEST_NEXT_STAGE_PLAY
    if(rw(0x8344)==1&&rw(0x8004)==2&&rw(0x8008)==2)nextPlayTicks++;
#ifdef WILLOW_TEST_STAGE2_PLAY
#ifdef WILLOW_TEST_STAGE3_PLAY
#ifdef WILLOW_TEST_STAGE4_PLAY
    if(rw(0x8344)==4&&rw(0x8004)==2&&rw(0x8008)==2){fifthPlayTicks++;if(rw(0x984c)>fifthMaxX)fifthMaxX=rw(0x984c);}
#ifdef WILLOW_TEST_STAGE5_PLAY
    if(rw(0x8344)==5&&rw(0x8004)==2&&rw(0x8008)==2){sixthPlayTicks++;if(rw(0x984c)>sixthMaxX)sixthMaxX=rw(0x984c);if(rw(0x8374)<7)sixthSections|=1u<<rw(0x8374);}
#endif
#endif
    if(rw(0x8344)==3&&rw(0x8004)==2&&rw(0x8008)==2){fourthPlayTicks++;if(rw(0x984c)>fourthMaxX)fourthMaxX=rw(0x984c);}
#endif
    if(rw(0x8344)==2&&rw(0x8004)==2&&rw(0x8008)==2){thirdPlayTicks++;if(rw(0x984c)>thirdMaxX)thirdMaxX=rw(0x984c);}
#endif
#endif
#ifdef WILLOW_TEST_MADMARTIGAN_SCENE
    if(sceneInjected&&sceneOutWaits&&sceneInWaits&&rb(0x9849)==6&&
       !actors.playerScene.resume&&!actors.playerWait&&!actors.phase&&rb(0x83c3)==2){resultsEntries++;break;}
#else
    if(shopExits&&
#ifdef WILLOW_TEST_STAGE5_COMPLETE
      rw(0x8344)==5&&rw(0x8004)==6&&!rw(0x8008)&&!eventWait
#elif defined(WILLOW_TEST_STAGE5_PLAY)
      sixthPlayTicks>=WILLOW_TEST_STAGE5_PLAY_TICKS
#elif defined(WILLOW_TEST_STAGE4_COMPLETE)
      rw(0x8344)==5&&rw(0x8004)==0&&rw(0x8008)==0
#elif defined(WILLOW_TEST_STAGE4_PLAY)
      fifthPlayTicks>=WILLOW_TEST_STAGE4_PLAY_TICKS
#elif defined(WILLOW_TEST_STAGE3_COMPLETE)
      rw(0x8344)==4&&rw(0x8004)==0&&rw(0x8008)==0
#elif defined(WILLOW_TEST_STAGE3_PLAY)
      fourthPlayTicks>=2048
#elif defined(WILLOW_TEST_STAGE2_COMPLETE)
      rw(0x8344)==3&&rw(0x8004)==0&&rw(0x8008)==0
#elif defined(WILLOW_TEST_STAGE2_PLAY)
      thirdPlayTicks>=2048
#elif defined(WILLOW_TEST_STAGE1_COMPLETE)
      rw(0x8344)==2&&rw(0x8004)==0&&rw(0x8008)==0
#elif defined(WILLOW_TEST_MADMARTIGAN_START)
      rw(0x8344)==1&&rb(0x9866)==4&&rb(0x9848)==2
#elif defined(WILLOW_TEST_NEXT_STAGE_PLAY)
      nextPlayTicks>=WILLOW_TEST_NEXT_STAGE_PLAY_TICKS
#elif defined(WILLOW_TEST_NEXT_STAGE)
      rw(0x8344)==1&&rw(0x8004)==0&&rw(0x8008)==0
#else
      rw(0x8004)==4&&rw(0x8008)==
#ifdef WILLOW_TEST_INTERSTAGE_MAP
      16
#elif defined(WILLOW_TEST_RESULTS_TALLY)
      14
#else
      6
#endif
      &&(rb(0x83c3)&2)
#endif
      &&!flow.route.pc){resultsEntries++;break;}
#endif
#endif
    if(casePlay>=requestedTicks)break;
   }
#endif
   if(pass==205999)return 1;
  }
 }
#ifdef WILLOW_TEST_FIRST_LEVEL
#ifdef WILLOW_TEST_SHOP_ENTRY
 if(completions!=1||!shopEntries||!shopWaits||!scoreTasks)return 1;
#ifdef WILLOW_TEST_SHOP_EXIT
 if(!shopExits)return 1;
#ifdef WILLOW_TEST_STAGE_CLEAR_ENTRY
 if(resultsEntries!=1){
#ifdef WILLOW_TEST_STAGE2_COMPLETE
  FILE*unfinished=fopen(
#ifdef WILLOW_TEST_STAGE5_PLAY
  "build/native-shop-entry-investigation/stage5-incomplete-workram.bin",
#elif defined(WILLOW_TEST_STAGE4_PLAY)
  "build/native-shop-entry-investigation/stage4-incomplete-workram.bin",
#elif defined(WILLOW_TEST_STAGE3_COMPLETE)
  "build/native-shop-entry-investigation/stage3-incomplete-workram.bin",
#else
  "build/native-shop-entry-investigation/stage2-incomplete-workram.bin",
#endif
  "wb");
  if(unfinished){fwrite(expected,1,sizeof expected,unfinished);fclose(unfinished);}
  fprintf(stderr,"Stage completion not reached: stage %u manager %u/%u player %u/%u\n",rw(0x8344),rw(0x8004),rw(0x8008),rw(0x984c),rw(0x9850));
#endif
  return 1;
 }
#ifdef WILLOW_TEST_STAGE2_PLAY
#ifdef WILLOW_TEST_STAGE2_COMPLETE
 if(thirdPlayTicks<=2048)return 1;
#ifdef WILLOW_TEST_STAGE3_COMPLETE
#ifdef WILLOW_TEST_STAGE4_COMPLETE
 if(rw(0x8344)!=5||fourthPlayTicks<=2048||fifthPlayTicks<=2048)return 1;
 printf("Fifth-stage completion: final-stage manager initialization reached\n");
#ifdef WILLOW_TEST_STAGE5_PLAY
#ifdef WILLOW_TEST_STAGE5_COMPLETE
 if(sixthPlayTicks<2048||sixthMaxX<=192||campaignEndingStates!=0xfffff||rw(0x8004)!=6||rw(0x8008))return 1;
 printf("Full campaign diagnostic: final boss and all twenty ending states reached through controller inputs; service invulnerability enabled\n");
#else
 if(sixthPlayTicks!=WILLOW_TEST_STAGE5_PLAY_TICKS||sixthMaxX<=192)return 1;
#endif
#ifdef WILLOW_TEST_STAGE5_TOWER
 if(!(sixthSections&64)){
  FILE*unfinished=fopen("build/native-shop-entry-investigation/stage5-tower-incomplete-workram.bin","wb");
  if(unfinished){fwrite(expected,1,sizeof expected,unfinished);fclose(unfinished);}
  fprintf(stderr,"Tower traversal incomplete: section %u route %u player %u/%u visited %02x\n",rw(0x8374),towerRoute,rw(0x984c),rw(0x9850),sixthSections);return 1;
 }
#endif
#ifdef WILLOW_TEST_STAGE5_ROUTE
 if(!(sixthSections&4)){fprintf(stderr,"Final-stage route has not reached section two: visited %x\n",sixthSections);return 1;}
#endif
#ifdef WILLOW_TEST_STAGE5_FIRST_PORTAL
 if(rw(0x8374)!=1){fprintf(stderr,"Final-stage first portal not traversed: section %u\n",rw(0x8374));return 1;}
#endif
 printf("Final-stage gameplay: %u original/host/native updates, maximum player X %u, current section %u\n",sixthPlayTicks,sixthMaxX,rw(0x8374));
 printf("Final-stage visited-section mask: %02x\n",sixthSections);
#endif
#else
 if(rw(0x8344)!=4||fourthPlayTicks<=2048)return 1;
#endif
#ifdef WILLOW_TEST_STAGE4_PLAY
#ifndef WILLOW_TEST_STAGE4_COMPLETE
 if(fifthPlayTicks!=WILLOW_TEST_STAGE4_PLAY_TICKS||fifthMaxX<=192)return 1;
#endif
 printf("Fifth-stage gameplay: %u original/host/native updates, maximum player X %u\n",fifthPlayTicks,fifthMaxX);
#endif
 printf("Fourth-stage completion: fifth-stage manager initialization reached\n");
#else
 if(rw(0x8344)!=3)return 1;
#endif
#ifdef WILLOW_TEST_STAGE3_PLAY
#ifndef WILLOW_TEST_STAGE3_COMPLETE
 if(fourthPlayTicks!=2048||fourthMaxX<=192)return 1;
#endif
 printf("Fourth-stage gameplay: %u original/host/native updates, maximum player X %u\n",fourthPlayTicks,fourthMaxX);
#endif
 printf("Third-stage completion: results/interstage and fourth-stage initialization verified\n");
#else
 if(thirdPlayTicks!=2048||thirdMaxX<=192)return 1;
#endif
 printf("Third-stage gameplay: %u original/host/native updates, maximum player X %u\n",thirdPlayTicks,thirdMaxX);
#endif
#ifdef WILLOW_TEST_STAGE1_COMPLETE
 printf("Second-stage completion: results, interstage and stage-two initialization reached from controller-only traversal\n");
#endif
#ifdef WILLOW_TEST_MADMARTIGAN_SCENE
 if(!sceneInjected||!sceneOutWaits||!sceneInWaits)return 1;
 printf("Synthetic Madmartigan scene: %u fade-out and %u fade-in waits; compiled/native/original scheduler parity, both fades and presentation completion signal verified\n",sceneOutWaits,sceneInWaits);
#endif
#ifdef WILLOW_TEST_MADMARTIGAN_START
 printf("Madmartigan startup: source gate/reset/pose/countdown and charge tail complete; active controller tested separately\n");
#endif
#ifdef WILLOW_TEST_NEXT_STAGE_PLAY
 printf("Next-stage gameplay: %u native/host/original ticks after stage-one startup\n",nextPlayTicks);
#ifdef WILLOW_TEST_NEXT_STAGE_ROUTE
 printf("Second-stage route maximum X: %u\n",stageRouteMaxX);
#if defined(WILLOW_TEST_NEXT_STAGE_GAP) || defined(WILLOW_TEST_NEXT_STAGE_SECOND_ROOF)
 if(stageRouteMaxX<1600){fprintf(stderr,"Route did not clear both roof/wall obstacles\n");return 1;}
#else
 if(stageRouteMaxX<800){fprintf(stderr,"Route did not clear the first roof/wall obstacle\n");return 1;}
#endif
#endif
#endif
#ifdef WILLOW_TEST_NEXT_STAGE
 printf("Next-stage initialization: stage counter, equipment, spawn, difficulty and palette initialized from source\n");
#endif
#ifdef WILLOW_TEST_INTERSTAGE_MAP
 printf("Inter-stage map: source palette/map setup and fade completed\n");
#endif
#ifdef WILLOW_TEST_RESULTS_TALLY
 printf("Forest results tally: score %08x, bonus flags %u, both fade stages and sprite clear verified\n",willowResultsLong(&bus,0x834a),rb(0x0e84));
 if(rb(0x0e84))return 1;
#else
 printf("Forest results entry: final boss defeated, scripted player exit and initial results messages verified\n");
#endif
#elif defined(WILLOW_TEST_SHOP_PROGRESS)
 if(playTicks!=requestedTicks)return 1;
#endif
 printf("Shop decline and exit: %u completed exits with original text/countdown/fade waits\n",shopExits);
#endif
 printf("Service-option shop entry: %u fade-wait boundaries, %u completed shop presentations; invulnerability enabled, purchases not yet verified\n",shopWaits,shopEntries);
#else
#ifdef WILLOW_TEST_ENDING_FIXTURE
 if(completions!=1||!endingInjected||!endingFinished||!waits||!paletteWrites||!videoWrites){
  fprintf(stderr,"Post-credits ending fixture incomplete: manager %u/%u state %u ticks %u\n",rw(0x8004),rw(0x8008),rw(0x800c),playTicks);return 1;
 }
#ifdef WILLOW_TEST_ENDING_CREDITS_FIXTURE
 if(endingStates!=0xfffff){fprintf(stderr,"Ending state coverage incomplete: %05x\n",(unsigned)endingStates);return 1;}
 printf("Controlled full-ending fixture: all 20 states, %u updates with real credits/actor/message/fade tasks; final manager exit reached, campaign completion not asserted\n",playTicks);
#else
 printf("Controlled post-credits ending fixture: %u updates with real actor/message/fade tasks; final manager exit reached, campaign completion not asserted\n",playTicks);
#endif
#else
 if(completions!=1||playTicks!=requestedTicks||!scoreTasks||!waits||!paletteWrites||!videoWrites)return 1;
#endif
#endif
#else
 if(completions!=4||!waits||!paletteWrites||!videoWrites)return 1;
#endif
#ifdef WILLOW_TEST_FIRST_LEVEL
 FILE*snapshot=fopen("build/first-level-runtime-investigation/final-workram.bin","wb");
 if(!snapshot||fwrite(expected,1,sizeof expected,snapshot)!=sizeof expected)return 2;
 fclose(snapshot);
 printf("Score-award task dispatches: %u\n",scoreTasks);
 printf("First-level continuous evidence: %u ticks after real opening, max camera %u, %u life losses; native/host/original main, task scan and wake parity; level completion not asserted\n",playTicks,maxPlayCamera,lifeLosses);
#endif
 printf("Forest scene region %u: %u main boundaries, %u real command waits, %u completed forest intros, %u video writes, %u palette writes, %u descending-camera updates; actual actor/sprite updates and scheduler agree with original execution\n",region,checks,waits,completions,videoWrites,paletteWrites,descentUpdates);
 printf("Compiled forest intro: %u target/host/original boundaries, retained flow/actor/task state and Scroll2 shadow match\n",targetChecks);
 if(cycleMode){
  fclose(timing);
  FILE*profile=fopen("build/forest-cycle-profile.csv","w");if(!profile)return 2;
  fputs("pc,master_cycles\n",profile);
  for(unsigned n=0;n<0x80000;n++)if(cycleProfile[n])fprintf(profile,"%06x,%llu\n",0x100000+n*2,cycleProfile[n]);
  fclose(profile);
  profile=fopen(argc==6?"build/forest-selected-profile.csv":"build/forest-reset-profile.csv","w");if(!profile)return 2;
  fputs("pc,master_cycles\n",profile);
  for(unsigned n=0;n<0x80000;n++)if(resetProfile[n])fprintf(profile,"%06x,%llu\n",0x100000+n*2,resetProfile[n]);
  fclose(profile);
  printf("Selected CPU profile: sample %u pass %u\n",profileSample,profilePass);
  for(unsigned n=0;n<3;n++)printf("Operation %u: %u calls, mean %.0f max %u master cycles\n",n,opCount[n],(double)opTotal[n]/opCount[n],opMax[n]);
  printf("Opening CPU: %u complete main/task/wake ticks, mean %.0f max %u master cycles, %u over 896040; worst sample %u pass %u scene %u\n",timingCount,(double)timingTotal/timingCount,timingMax,timingOver,maxSample,maxPass,maxState);
 }
 if(sourceTiming){fclose(sourceTiming);printf("Original-instruction CPU timing written to build/forest-source-timing.csv; mapped-memory oracle, not rendered gameplay FPS\n");}
 return 0;
}
