#ifdef WILLOW_TEST_GAMEPLAY_MAIN
#define WILLOW_TEST_GAMEPLAY_ROUTED
#include "../src/willow_native_gameplay_flow.h"
#endif
#define WILLOW_TEST_GAMEPLAY_PREFIX
#define WILLOW_TEST_GAMEPLAY_COLLISION
#define WILLOW_TEST_GAMEPLAY_STAGE0
/* Retained original 954E gameplay body through camera, both scroll layers,
 * sprite rendering and publication. Inputs/frame counter are controlled;
 * MAIN mode includes 92D8/main-manager dispatch and source simulation ticks;
 * VBlank and other scheduled tasks remain external. Graphics use separate
 * 64 KiB cartridge SRAM regions; no actor pool aliases either sprite bank. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_update.h"
#include "../src/willow_arcade_player_bus.h"
#include "../src/willow_arcade_gameplay_actors.h"
#include "../src/willow_arcade_gameplay_collision.h"
#include "../src/willow_arcade_gameplay_stage0.h"
#include "../src/willow_arcade_stage_objects.h"
#include "../src/willow_arcade_gameplay_frame.h"
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
#include "../src/willow_arcade_gameplay_services.h"
#endif
#include "../src/willow_arcade_sprite_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#ifdef WILLOW_TEST_NATIVE_TARGET
static uint8_t rom[0x400000],source[0x100000];
#else
static uint8_t rom[0x200000],source[0x100000];
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

static uint8_t expected[65536],scroll2[0x4000],scroll3[0x4000],sprites[2][0x800];
static uint16_t *renderD4;static uint32_t *renderA4;
static uint32_t spriteCursor;static uint16_t spriteBudget;static unsigned emittedSprites;

static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}



static uint16_t sourceWord(unsigned long at){return (at&0xff0000)==0xff0000?rw(at):willowMotionWord(source+at);}
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
static uint16_t inputWord(uint32_t at){return sourceWord(at);}
#endif
static uint8_t*resolve(uint16_t at){return expected+at;}
static void textWrite(uint32_t at,uint16_t v){
 if(at>=0x910000&&at<0x914000){ww(0x4000+at-0x910000,v);return;}
 if(at>=0x91c000&&at<0x91d000){ww(0x2000+at-0x91c000,v);return;}
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
  uint32_t original=n<60?0x918000:0x91c000,mapped=n<60?0x204000:0xff2000;
  uint32_t from=toCore?original:mapped,to=toCore?mapped:original;
  unsigned length=n<60?0x4000:0x1000;
  if(value>=from&&value<from+length){value=value-from+to;for(unsigned j=0;j<4;j++)ram[(at+j)^1]=value>>(24-j*8);}
 }
}

static uint8_t attribute(uint16_t at){return scroll2[at+2];}
static uint16_t paletteWord(uint32_t at){return sourceWord(at);}
static uint8_t paletteByte(uint32_t at){return source[at];}
static uint16_t colorRead(uint32_t at){if(at<0x91c000||at>=0x91d000)abort();return rw(0x2000+at-0x91c000);}
static void colorWrite(uint32_t at,uint16_t v){if(at<0x91c000||at>=0x91d000)abort();ww(0x2000+at-0x91c000,v);}
static uint8_t*allocateWithA4(uint32_t*a4){uint16_t at=willowArcadePaletteFreeWithA4(&bus,a4);return at?resolve(at):NULL;}
static uint8_t*allocate(void){return allocateWithA4(NULL);}
static uint8_t randomByte(void){return willowActor18Random(&bus);}
static void soundWithA4(uint16_t cmd,uint32_t*a4){willowBreakableSoundWithA4(&bus,cmd,a4);}
static void sound(uint16_t cmd){soundWithA4(cmd,NULL);}
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
static uint8_t sourceByte(uint32_t at){return (at&0xff0000)==0xff0000?rb(at):source[at];}
static void writeMap2(uint16_t at,uint16_t v){if(at>=sizeof scroll2)abort();willowMotionPutWord(scroll2+at,v);}
static void writeMap3(uint16_t at,uint16_t v){if(at>=sizeof scroll3)abort();willowMotionPutWord(scroll3+at,v);}
static void spriteWord(uint32_t at,uint16_t v){
 unsigned bank=at>=0x908000;uint32_t offset=at-(bank?0x908000:0x900000);
 if(offset>=sizeof sprites[0]){fprintf(stderr,"Sprite overflow %x\n",at);abort();}
 willowMotionPutWord(sprites[bank]+offset,v);
}
static void emitPiece(uint16_t x,uint16_t y,uint16_t code,uint16_t attr){
 spriteWord(spriteCursor,x);spriteWord(spriteCursor+2,y);spriteWord(spriteCursor+4,code);spriteWord(spriteCursor+6,attr);spriteCursor+=8;emittedSprites++;
}
static void drawOrdinary(uint16_t at){
 WillowArcadeSpriteWriter w={inputWord,emitPiece,rw(0x829a),spriteBudget};
 willowArcadeSpriteWithD4(&w,resolve(at),rw(0x8408),rw(0x840c),renderD4);ww(0x829a,w.areaBudget);spriteBudget=w.pieceBudget;
}
static void drawStage4(uint16_t at){
 WillowArcadeSpriteWriter w={inputWord,emitPiece,rw(0x829a),spriteBudget};
 willowArcadeStage4SpriteWithD4(&w,resolve(at),rw(0x8408),rw(0x840c),renderD4);ww(0x829a,w.areaBudget);spriteBudget=w.pieceBudget;
}
static void drawFrame(WillowArcadeSpriteFrame*f){
 spriteCursor=f->cursor;spriteBudget=f->remaining;
 willowArcadeVisibilityRenderWithA4(&bus,drawOrdinary,drawStage4,renderA4);
 f->cursor=spriteCursor;f->remaining=spriteBudget;
}
static void graphicsToCore(uint8_t*map){
 memcpy(map+0x4000,scroll2,sizeof scroll2);memcpy(map+0xc000,scroll3,sizeof scroll3);
 memcpy(map,sprites[0],sizeof sprites[0]);memcpy(map+0x8000,sprites[1],sizeof sprites[1]);
}
static int graphicsMatch(const uint8_t*map){return !memcmp(map+0x4000,scroll2,sizeof scroll2)&&!memcmp(map+0xc000,scroll3,sizeof scroll3)&&!memcmp(map,sprites[0],sizeof sprites[0])&&!memcmp(map+0x8000,sprites[1],sizeof sprites[1]);}
#ifdef WILLOW_TEST_NATIVE_TARGET
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} GameplayCoreCycles;
static unsigned long long cycleProfile[0x80000];
static uint8_t nativeBefore[65536],nativeGraphics[65536];
static unsigned nativeEntry,nativeD4,nativeA4,nativeStatus,nativeCursor,nativeBudget;
#ifdef WILLOW_TEST_MEMORY_COPY
static unsigned nativeMemcpy;
#endif
#endif
int main(int argc,char**argv){
#ifdef WILLOW_TEST_NATIVE_TARGET
 if(argc!=2&&(argc!=3||strcmp(argv[2],"--cycles")))return 2;
 int cycleMode=argc==3;
#else
 if(argc!=2)return 2;
#endif
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(source,rom,sizeof source);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 /* SRAM: sprite0 at +0, Scroll2 at +4000, sprite1 at +8000,
  * Scroll3 at +C000. Sprite addresses preserve source bank bit 15. */
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x8474,0x60fe);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 const unsigned patches[]={0x4ad0,0x4ad8,0x4ab2,0x4aba,0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910310,0x910310,0x910314,0x910314,0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}
 for(unsigned n=0;n<2;n++){unsigned at=n?0x1276:0xc2e;if(willowMotionLong(rom+at)!=0x91c000)return 2;willowMotionPutLong(rom+at,0xff2000);}
 willowMotionPutLong(rom+0x3e44,0x204000);willowMotionPutLong(rom+0x4365a,0x204000);
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
 const unsigned playerPatches[]={0xc242,0x4b72,0xea68};
 const uint32_t playerOriginals[]={0x91c000,0x910170,0x91c0e0};
 for(unsigned n=0;n<3;n++){
  if(willowMotionLong(rom+playerPatches[n])!=playerOriginals[n]){fprintf(stderr,"Bad player relocation %x\n",playerPatches[n]);return 2;}
  uint32_t at=playerOriginals[n];willowMotionPutLong(rom+playerPatches[n],at>=0x91c000?0xff2000+at-0x91c000:0xff4000+at-0x910000);
 }
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
 /* Verify service boundaries used by the earlier prefix fixtures. */
 if(willowMotionWord(rom+0x955c)!=0x4eb9||willowMotionLong(rom+0x955e)!=0x39a52)return 2;
#ifdef WILLOW_TEST_GAMEPLAY_COLLISION
 if(willowMotionWord(rom+0x9562)!=0x4eb9||willowMotionLong(rom+0x9564)!=0x3807c)return 2;
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
 if(willowMotionWord(rom+0x9568)!=0x4ef9||willowMotionLong(rom+0x956a)!=0x3b524)return 2;
 /* Preserve the real tail to camera/scroll/sprite publication. */
 if(willowMotionLong(rom+0x43d50)!=0x914000||willowMotionLong(rom+0x3b538)!=0x900000||willowMotionLong(rom+0x3b54c)!=0x908000)return 2;
 willowMotionPutLong(rom+0x43d50,0x20c000);
 willowMotionPutLong(rom+0x3b538,0x200000);willowMotionPutLong(rom+0x3b54c,0x208000);
 if(willowMotionLong(rom+0x5640)!=0x91056c)return 2;
 willowMotionPutLong(rom+0x5640,0xff456c);
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
 if(willowMotionLong(rom+0x64b4)!=0x910000)return 2;
 willowMotionPutLong(rom+0x64b4,0xff4000);
#endif
#else
 willowMotionPutWord(rom+0x9562,0x4e75);
#endif
#else
 willowMotionPutWord(rom+0x955c,0x4e75);
#endif
#endif
#ifdef WILLOW_TEST_NATIVE_TARGET
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
#ifdef WILLOW_TEST_GAMEPLAY_MAIN
 const char*nativeBinary="build/gameplay_main_probe.bin",*nativeSymbols="build/gameplay_main_probe.sym";
#else
 const char*nativeBinary="build/gameplay_routed_probe.bin",*nativeSymbols="build/gameplay_routed_probe.sym";
#endif
#else
 const char*nativeBinary="build/gameplay_runtime_probe.bin",*nativeSymbols="build/gameplay_runtime_probe.sym";
#endif
 f=fopen(nativeBinary,"rb");if(!f)return 2;
 memcpy(rom+0x300000,source,sizeof source);
 /* A ROM larger than 2 MiB requires explicit cartridge SRAM enable. */
 willowMotionPutWord(rom+0x100004,0x13fc);willowMotionPutWord(rom+0x100006,1);
 willowMotionPutLong(rom+0x100008,0xa130f1);willowMotionPutWord(rom+0x10000c,0x4e75);willowMotionPutWord(rom+0x10000e,0x4e71);
 size_t nativeSize=fread(rom+0x100010,1,sizeof rom-0x100010,f);if(!nativeSize||!feof(f))return 2;fclose(f);
 f=fopen(nativeSymbols,"r");if(!f)return 2;
 unsigned address;char kind,name[128];
 while(fscanf(f,"%x %c %127s",&address,&kind,name)==3){
  if(!strcmp(name,"gameplayRuntimeProbe"))nativeEntry=address;
#ifdef WILLOW_TEST_MEMORY_COPY
  if(!strcmp(name,"memcpy"))nativeMemcpy=address;
#endif
  if(!strcmp(name,"gameplayProbeD4"))nativeD4=address-0x200000;
  if(!strcmp(name,"gameplayProbeA4"))nativeA4=address-0x200000;
  if(!strcmp(name,"gameplayProbeStatus"))nativeStatus=address-0x200000;
  if(!strcmp(name,"gameplayProbeCursor"))nativeCursor=address-0x200000;
  if(!strcmp(name,"gameplayProbeBudget"))nativeBudget=address-0x200000;
 }
 fclose(f);if(nativeEntry<0x100010||nativeEntry>=0x200000)return 2;
 const unsigned nativeOffsets[]={nativeD4,nativeA4,nativeStatus,nativeCursor,nativeBudget};
 for(unsigned n=0;n<sizeof nativeOffsets/sizeof nativeOffsets[0];n++)if(nativeOffsets[n]<0x800||nativeOffsets[n]>0x1ffc){fprintf(stderr,"Invalid native state address %x\n",nativeOffsets[n]);return 2;}
#endif
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/stage0-full-frame-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/stage0-full-frame-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 uint8_t*mapRam=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!mapRam||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<0x10000)return 2;
#ifdef WILLOW_TEST_NATIVE_TARGET
 ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100004);retro_run();
 if(m68k_get_reg(16)!=0x100000)return 2;
 FN(void,m68k_run,(unsigned));
 GameplayCoreCycles*cpu=dlsym(core,"m68k");if(!cpu)return 2;
 if(cycleMode){
  m68k_set_reg(16,0x10000e);if(cpu->pc!=0x10000e)return 2;
  for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
  cpu->cycles=0;cpu->refresh=896;m68k_run(1);
  if(cpu->cycles!=28||m68k_get_reg(16)!=0x100010){fprintf(stderr,"Cycle calibration failed\n");return 2;}
 }
#ifdef WILLOW_TEST_MEMORY_COPY
 if(!cycleMode||nativeMemcpy<0x100010||nativeMemcpy>=0x200000)return 2;
 unsigned copies=0;
 for(unsigned so=0;so<4;so++)for(unsigned dest=0;dest<4;dest++)for(unsigned length=0;length<=260;length++){
  for(unsigned n=0;n<512;n++){ram[(0x5000+n)^1]=(uint8_t)(n*37+length);ram[(0x5400+n)^1]=0xa5;}
  const unsigned args[]={0x100000,0xff5400+dest,0xff5000+so,length};
  for(unsigned n=0;n<4;n++)for(unsigned k=0;k<4;k++)ram[(0x7f00+n*4+k)^1]=(uint8_t)(args[n]>>(24-k*8));
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xff7f00);m68k_set_reg(16,nativeMemcpy);
  for(unsigned n=2;n<15;n++)if(n!=8&&n!=9)m68k_set_reg(n,0x12340000+n);
  cpu->cycles=0;cpu->refresh=896;unsigned steps=0;
  while(m68k_get_reg(16)!=0x100000&&steps++<10000)m68k_run(cpu->cycles+1);
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(0)!=args[1]||m68k_get_reg(15)!=0xff7f04)return 1;
  for(unsigned n=2;n<15;n++)if(n!=8&&n!=9&&m68k_get_reg(n)!=0x12340000+n)return 1;
  for(unsigned n=0;n<512;n++){
   uint8_t value=n>=dest&&n<dest+length?(uint8_t)((n-dest+so)*37+length):0xa5;
   if(ram[(0x5400+n)^1]!=value||ram[(0x5000+n)^1]!=(uint8_t)(n*37+length)){
    fprintf(stderr,"Memcpy mismatch source offset %u destination offset %u length %u byte %u\n",so,dest,length,n);return 1;
   }
  }
  copies++;
 }
 printf("Native memcpy: %u copies match across offsets 0..3 and lengths 0..260, with source/guards and callee-saved registers intact\n",copies);
 return 0;
#endif
#endif
 WillowPaletteAnimation g={0};g.byte=paletteByte;g.word=paletteWord;g.read=colorRead;g.write=colorWrite;
 g.allocate=allocate;g.random=randomByte;g.sound=sound;g.allocateWithA4=allocateWithA4;g.soundWithA4=soundWithA4;
 unsigned checks=0,active=0;
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
 unsigned timeouts=0;
#endif
#ifdef WILLOW_TEST_NATIVE_TARGET
 unsigned nativeChecks=0,minimumStack=0x7f00,maximumCycles[2]={0},cycleCounts[2]={0},overBudget[2]={0};unsigned long long totalCycles[2]={0};
 unsigned maximumSample[2]={0},maximumStep[2]={0};
#endif
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
 unsigned spawnedUpdates=0,spawnedActiveUpdates=0;uint16_t maxCameraX=0;
#endif
#ifdef WILLOW_TEST_GAMEPLAY_ENEMIES
 unsigned enemyDamage=0,playerDamage=0;
#endif
 for(unsigned sample=0;sample<
#ifdef WILLOW_TEST_NATIVE_TARGET
#ifndef WILLOW_TEST_NATIVE_SAMPLES
#define WILLOW_TEST_NATIVE_SAMPLES 2
#endif
 WILLOW_TEST_NATIVE_SAMPLES
#else
 16
#endif
 ;sample++){
#ifdef WILLOW_TEST_NATIVE_TARGET
  memset(mapRam+0x800,0,0x1800);
#endif
  memset(expected,0,sizeof expected);memset(scroll2,0,sizeof scroll2);memset(scroll3,0,sizeof scroll3);memset(sprites,0,sizeof sprites);
  FILE*map=fopen("build/stage0-collision-window.bin","rb");if(!map||fread(scroll2,1,sizeof scroll2,map)!=sizeof scroll2)return 2;fclose(map);
  willowArcadePoolsInit(&bus);WillowPlayerRecord initial;willowPlayerRecordStage0(&initial,sourceWord);memcpy(resolve(0x9846),initial.bytes,128);
  wb(0x8348,128);wb(0x8362,8);wb(0x8363,8);wb(0x8366,sample%7);
  ww(0x845c,(uint16_t)-64);ww(0x8284,0x1234+sample);wb(0xf99e,1);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);ww(0x801e,sample*2);
  uint16_t d4=sample*977;uint32_t a4=0x123456;WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};WillowArcadeActorUpdate task={0};
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
  WillowArcadeGameplayActors gameplay={0};(void)task;
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
#ifdef WILLOW_TEST_GAMEPLAY_MAIN
  WillowNativeGameplayFlow flow={0};flow.task.pc=0x92d8;
  ww(0x8000,2);ww(0x8004,2);ww(0x8008,2);
#else
  WillowArcadeEventRouteTask route={0};
#endif
  (void)gameplay;
#endif
  /* Real controller selection; mute variants remain covered by direct mode. */
  wb(0x8293,0);wb(0x8292,sample&1);wb(0x828e,3);
  ww(0x83c6,0x0009);wb(0x83c8,sample%4);wb(0xf99e,0);
#endif
#ifdef WILLOW_TEST_GAMEPLAY_ENEMIES
  uint16_t enemies[2];
  for(unsigned n=0;n<2;n++){
   enemies[n]=willowActor18Pop(&bus,0x9801,0x981c);if(!enemies[n])return 2;
   uint8_t*enemy=resolve(enemies[n]);enemy[0]=1;enemy[33]=4;enemy[15]=n;
   willowMotionPutWord(enemy+6,rw(0x984c)+32+n*48);
   willowMotionPutWord(enemy+10,rw(0x9850)+16);
  }
#endif
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
  if(!willowArcadeStageObjects(&bus,resolve,sourceWord))return 2;
  /* Original scroll-controller startup and sprite-buffer histories. */
  ww(0x838e,7936);ww(0x8392,768);ww(0x829c,256);ww(0x829e,256);
  unsigned duration=1024;
#else
  unsigned duration=256;
#endif
  for(unsigned step=0;step<duration;step++){

#ifndef WILLOW_TEST_GAMEPLAY_MAIN
   wb(0x8019,step);
#endif
   wb(0x806b,rb(0x806a));
   const uint8_t keys[]={1,1,0x11,1,0x21,1,0x11,1};wb(0x806a,keys[(step/16+sample)%8]);
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
   /* Restore old held byte: 1F88, not the fixture, publishes its previous. */
   wb(0x8060,rb(0x806a));wb(0x8062,rb(0x806a));wb(0x806a,rb(0x806b));
#endif
#ifdef WILLOW_TEST_GAMEPLAY_ENEMIES
   uint8_t oldEnemyHp[2]={rb(enemies[0]+42),rb(enemies[1]+42)},oldPlayerHp=rb(0x8362);
#endif
#ifdef WILLOW_TEST_NATIVE_TARGET
   memcpy(nativeBefore,expected,sizeof nativeBefore);
#endif
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
   unsigned timedOut=!rb(0xf99e)&&!rw(0x83c6);
#endif
   uint16_t oldD4=d4;uint32_t oldA4=a4;
   for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];graphicsToCore(mapRam);mapPointers(ram,1);
#ifdef WILLOW_TEST_NATIVE_TARGET
   memcpy(nativeGraphics,mapRam,sizeof nativeGraphics);
#endif
   renderD4=&d4;renderA4=&a4;WillowArcadeSpriteFrame frame;
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
   WillowArcadeGameplayServices services={.bus=&bus,.resolve=resolve,.word=sourceWord,
    .busWord=inputWord,.byte=sourceByte,.palette=&g,.collision=&collision,.actors=&task,
    .videoWrite=videoWrite,.textWrite=textWrite,.d4=&d4,.a4=&a4,
    .player=playerUpdate,.playerContext=&collision,.write2=writeMap2,.write3=writeMap3,
    .draw=drawFrame,.spriteWord=spriteWord,.frame=&frame};
#ifdef WILLOW_TEST_GAMEPLAY_MAIN
   WillowNativeGameplayFlowBinding binding={&flow,&bus,inputWord,&a4,willowArcadeGameplayServiceResume,&services};
   WillowTransitionStep tick=willowNativeGameplayFlowStep(&binding);
   uint8_t result=tick.supported&&!tick.finished&&tick.resume==0x932c&&tick.delay==1;
   if(!result){fprintf(stderr,"Main flow incomplete sample %u step %u pc %x manager %x; camera %u player %u/%u position %u/%u HP %u clock %04x outer %u/%u/%u\n",sample,step,flow.task.pc,flow.manager.target,rw(0x8408),rb(0x9848),rb(0x9849),rw(0x984c),rw(0x9850),rb(0x8362),rw(0x83c6),rw(0x8000),rw(0x8004),rw(0x8008));return 1;}
#else
   uint8_t result=willowArcadeEventRouteResume(&route,0x9530,&bus,willowArcadeGameplayServiceResume,&services);
   if(result!=1){fprintf(stderr,"Routed service incomplete sample %u step %u PC %x flags %x/%x/%x clock %x rebound %x\n",sample,step,route.pc,rb(0x83c2),rb(0x83c5),rb(0x83c3),rw(0x83c6),rb(0xf99e));return 1;}
#endif
#else
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
   uint8_t result=willowArcadeGameplayStage0Resume(
#elif defined(WILLOW_TEST_GAMEPLAY_COLLISION)
   uint8_t result=willowArcadeGameplayCollisionResume(
#else
   uint8_t result=willowArcadeGameplayActorsResume(
#endif
&gameplay,&bus,resolve,sourceWord,inputWord,&g,&collision,videoWrite,textWrite,0,&d4,&a4,playerUpdate,&collision);
#else
   uint8_t result=willowArcadeActorUpdateResumeWithPlayer(&task,&bus,resolve,sourceWord,&g,&collision,videoWrite,textWrite,0,&d4,&a4,playerUpdate,&collision);
#endif
   if(result==1&&!willowArcadeGameplayFrameWithRegisters(&bus,resolve,sourceByte,inputWord,writeMap2,writeMap3,0,0,&g,drawFrame,spriteWord,&frame,&d4,&a4)){
    fprintf(stderr,"Unsupported frame sample %u step %u scroll2 %u/%u scroll3 %u/%u\n",sample,step,rb(0x8404),rb(0x8405),rb(0x8484),rb(0x8485));return 1;
   }
#endif
   if(result!=1){fprintf(stderr,"Unsupported sample %u step %u phase %u player %u/%u\n",sample,step,task.phase,rb(0x9848),rb(0x9849));return 1;}
   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(12,oldA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(4,oldD4);m68k_set_reg(16,
#ifdef WILLOW_TEST_GAMEPLAY_MAIN
   0x92d8
#elif defined(WILLOW_TEST_GAMEPLAY_ROUTED)
   0x9530
#elif defined(WILLOW_TEST_GAMEPLAY_PREFIX)
   0x954e
#else
   0x9ef2
#endif
   );
#ifdef WILLOW_TEST_NATIVE_TARGET
   if(cycleMode){cpu->cycles=0;cpu->refresh=896;}
#endif
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=
#ifdef WILLOW_TEST_GAMEPLAY_MAIN
    0xfefc
#else
    0xff04
#endif
   ){fprintf(stderr,"Return sample %u step %u PC %x SP %x\n",sample,step,m68k_get_reg(16),m68k_get_reg(15));return 1;}
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
   if(!timedOut)
#endif
   if(m68k_get_reg(8)!=frame.cursor-0x700000||(uint16_t)m68k_get_reg(7)!=frame.remaining){fprintf(stderr,"Frame outputs sample %u step %u cursor %x/%x budget %x/%x\n",sample,step,m68k_get_reg(8),frame.cursor-0x700000,m68k_get_reg(7),frame.remaining);return 1;}
#ifdef WILLOW_TEST_GAMEPLAY_MAIN
   if((m68k_get_reg(0)&65535)!=1||ram[0xfefe^1]!=0x93||ram[0xfeff^1]!=0x2c)return 1;
#endif
   mapPointers(ram,0);
   if(!graphicsMatch(mapRam)){fprintf(stderr,"Graphics sample %u step %u\n",sample,step);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){fprintf(stderr,"RAM sample %u step %u at %x actual %x expected %x player %u/%u\n",sample,step,n,ram[n^1],expected[n],rb(0x9848),rb(0x9849));return 1;}
   if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Registers sample %u step %u A4 %x/%x D4 %x/%x\n",sample,step,m68k_get_reg(12),a4,m68k_get_reg(4),d4);return 1;}
#ifdef WILLOW_TEST_GAMEPLAY_ENEMIES
   for(unsigned n=0;n<2;n++)if(oldEnemyHp[n]>rb(enemies[n]+42))enemyDamage++;
   if(oldPlayerHp>rb(0x8362))playerDamage++;
#endif
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
   uint8_t anySpawned=0,anyActive=0;
   for(unsigned n=0;n<50;n++)if(rb(0x9dc6+n*128)){
    anySpawned=1;if(rb(0x9dc8+n*128)==2)anyActive=1;
   }
   spawnedUpdates+=anySpawned;spawnedActiveUpdates+=anyActive;
   if(rw(0x8408)>maxCameraX)maxCameraX=rw(0x8408);
#endif
#ifdef WILLOW_TEST_NATIVE_TARGET
   /* The reserved C stack cannot hide any source-observable write. */
   if(memcmp(expected+0x6000,nativeBefore+0x6000,0x2000)){fprintf(stderr,"Source uses reserved C stack sample %u step %u\n",sample,step);return 1;}
   for(unsigned n=0;n<65536;n++)ram[n^1]=nativeBefore[n];
   memcpy(mapRam,nativeGraphics,sizeof nativeGraphics);
   willowMotionPutWord(mapRam+nativeD4,oldD4);willowMotionPutLong(mapRam+nativeA4,oldA4);willowMotionPutWord(mapRam+nativeStatus,0xdead);
   ram[0x7f00^1]=0;ram[0x7f01^1]=0x10;ram[0x7f02^1]=0;ram[0x7f03^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xff7f00);m68k_set_reg(16,nativeEntry);
   if(cycleMode){
    if(cpu->pc!=nativeEntry)return 2;
    for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
    cpu->cycles=0;cpu->refresh=896;unsigned instructions=0;
    while(m68k_get_reg(16)!=0x100000&&instructions++<2000000){
     unsigned pc=m68k_get_reg(16),before=cpu->cycles;m68k_run(cpu->cycles+1);
     if(pc>=0x100000&&pc<0x200000)cycleProfile[(pc-0x100000)/2]+=(unsigned)cpu->cycles-before;
     unsigned sp=m68k_get_reg(15)&65535;if(sp<minimumStack)minimumStack=sp;
    }
    if(cpu->cycles<0)return 2;
    unsigned phase=nativeBefore[0x8404]==2;
    totalCycles[phase]+=(unsigned)cpu->cycles;cycleCounts[phase]++;
    if((unsigned)cpu->cycles>maximumCycles[phase]){maximumCycles[phase]=cpu->cycles;maximumSample[phase]=sample;maximumStep[phase]=step;}
    if(cpu->cycles>3420*262)overBudget[phase]++;
   }else for(unsigned slice=0;slice<128&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000||willowMotionWord(mapRam+nativeStatus)!=1){fprintf(stderr,"Native target failed sample %u step %u PC %x SP %x status %x\n",sample,step,m68k_get_reg(16),m68k_get_reg(15),willowMotionWord(mapRam+nativeStatus));return 1;}
   if(!graphicsMatch(mapRam)){fprintf(stderr,"Native graphics mismatch sample %u step %u\n",sample,step);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0x6000&&n<0x8000)&&!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){fprintf(stderr,"Native RAM mismatch sample %u step %u at %x actual %x expected %x\n",sample,step,n,ram[n^1],expected[n]);return 1;}
   if(willowMotionWord(mapRam+nativeD4)!=d4||willowMotionLong(mapRam+nativeA4)!=a4||
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
    (!timedOut&&
#endif
    (willowMotionLong(mapRam+nativeCursor)!=frame.cursor||willowMotionWord(mapRam+nativeBudget)!=frame.remaining)
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
    )
#endif
    ){fprintf(stderr,"Native outputs mismatch sample %u step %u\n",sample,step);return 1;}
   nativeChecks++;
#endif
   checks++;if(rb(0x9846)&&rb(0x9848)==2)active++;
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
   if(timedOut){timeouts++;break;} /* 956E changes the outer manager; do not re-enter 9530. */
#endif
  }
 }
 if(!emittedSprites||!allocationTypes[1]||active<
#ifdef WILLOW_TEST_NATIVE_TARGET
 500
#else
 3000
#endif
){fprintf(stderr,"Missing retained player/shot coverage\n");return 1;}
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
#ifdef WILLOW_TEST_GAMEPLAY_COLLISION

#endif

#endif
#ifdef WILLOW_TEST_GAMEPLAY_ROUTED
 if(timeouts!=
#ifdef WILLOW_TEST_NATIVE_TARGET
 WILLOW_TEST_NATIVE_SAMPLES
#else
 16
#endif
 ){fprintf(stderr,"Missing timeout route coverage\n");return 1;}
#ifdef WILLOW_TEST_GAMEPLAY_MAIN
 printf("Main task 92D8 through manager, actual gameplay/render services and one-tick source sleep verified; simulation counter advances in source/native loop\n");
#endif
 printf("Source 9530 routed gameplay: %u updates, %u timeout transitions; actual input, clock, actors, collision, stage, frame, palette/sound/text services match original with fixture-supplied input\n",checks,timeouts);
#else
 printf("Complete 954E stage-zero gameplay body: %u updates, %u active Willow updates, %u player allocations; full non-stack RAM, both scroll layers, both sprite buffers, D4/A4 and frame outputs match original with controlled input/frame counter\n",checks,active,allocations);
#endif
#ifdef WILLOW_TEST_GAMEPLAY_ENEMIES
 printf("Retained enemy damage transitions %u, player HP decreases %u\n",enemyDamage,playerDamage);
 if(!enemyDamage&&!playerDamage){fprintf(stderr,"No combat interaction observed\n");return 1;}
#endif
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
 printf("Maximum source camera X %u\n",maxCameraX);
 printf("Source stage-zero population: %u updates with live class4 actors, %u with actors in active state\n",spawnedUpdates,spawnedActiveUpdates);
 if(!spawnedUpdates||!spawnedActiveUpdates){fprintf(stderr,"No stage-spawned actors observed\n");return 1;}
#endif
#ifdef WILLOW_TEST_NATIVE_TARGET
 if(cycleMode){
  for(unsigned n=0;n<2;n++)printf("CPU timing %s: %u updates, mean %.0f and max %u master cycles; %u exceed NTSC frame budget 896040\n",n?"active":"startup",cycleCounts[n],cycleCounts[n]?(double)totalCycles[n]/cycleCounts[n]:0,maximumCycles[n],overBudget[n]);
  for(unsigned n=0;n<2;n++)printf("Slowest %s update: sample %u step %u\n",n?"active":"startup",maximumSample[n],maximumStep[n]);
  printf("Maximum observed C stack descent %u bytes; bus refresh included, VDP/DMA/audio/scheduler workload excluded\n",0x7f00-minimumStack);
  FILE*profile=fopen("build/gameplay-cycle-profile.csv","w");if(!profile)return 2;
  fprintf(profile,"pc,master_cycles\n");for(unsigned n=0;n<0x80000;n++)if(cycleProfile[n])fprintf(profile,"%06x,%llu\n",0x100000+n*2,cycleProfile[n]);fclose(profile);
 }
 printf("Compiled 68000 target: %u gameplay invocations match host translation and original arcade execution\n",nativeChecks);
#endif
 printf("Emitted sprite pieces %u\n",emittedSprites);
 printf("Player allocations: class2 %u, class8 %u, class12 %u\n",allocationTypes[0],allocationTypes[1],allocationTypes[2]);return 0;
}
