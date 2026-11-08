/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_title_actor.h"
#include "../src/willow_arcade_opening_actors.h"
#include "../src/willow_arcade_opening_palette_live.h"
#include "../src/willow_arcade_pool_init.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "../src/willow_arcade_actor_pool.h"
#ifdef WILLOW_TEST_NATIVE_OPENING_DATA
#include "../native/intro_probe/src/actor_data.h"
#include "../native/intro_probe/src/palette_initial_data.h"
#include "../src/willow_native_palette_mutable.h"
static WillowNativePaletteOwner nativeOwner;
static WillowMutablePaletteGroup nativeSlots[2];
static unsigned nativeCopies,nativeWords;
#endif
static uint8_t rom[0x200000],expected[65536];
static WillowActorPool pool;
static uint16_t sourceWord(unsigned long at){
 uint16_t value=willowMotionWord(rom+at);
#ifdef WILLOW_TEST_NATIVE_OPENING_DATA
 if(introActorWord(at)!=value){fprintf(stderr,"Missing native opening actor word %lx\n",at);exit(1);}
 return introActorWord(at);
#else
 return value;
#endif
}
static uint16_t busWord(uint16_t at){return willowMotionWord(expected+at);}
static uint8_t busByte(uint16_t at){return expected[at];}
static void busPutWord(uint16_t at,uint16_t value){willowMotionPutWord(expected+at,value);}
static void busPutByte(uint16_t at,uint8_t value){expected[at]=value;}
static void releaseActor(uint8_t *record){
 willowActorPoolRelease(&pool,(record-pool.records)/128);
 uint16_t next=busWord(0x981c)-2;
 busPutWord(next,record-expected);busPutWord(0x981c,next);expected[0x9801]++;
}

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
static uint8_t *resolve(uint16_t at){return expected+at;}
static uint16_t paletteWord(uint32_t at){
 uint16_t value=willowMotionWord(rom+at);
#ifdef WILLOW_TEST_NATIVE_OPENING_DATA
 uint16_t native;
 if(!nativePaletteInitialWordRead(at,&native)||native!=value){fprintf(stderr,"Missing native opening palette word %x\n",at);exit(1);}
 return native;
#else
 return value;
#endif
}
static uint8_t sourceByte(uint32_t at){uint16_t value=paletteWord(at&~1u);return at&1?value:value>>8;}
static uint16_t colorRead(uint32_t at){return busWord(0x2000+at-0x91c000);}
static void colorWrite(uint32_t at,uint16_t value){busPutWord(0x2000+at-0x91c000,value);}
#ifdef WILLOW_TEST_NATIVE_OPENING_DATA
static void nativeCopy(uint32_t source,uint32_t destination){
 if(!willowMutablePaletteCopy(&nativeOwner,nativeSlots,2,source,destination,paletteWord)){
  fprintf(stderr,"Native opening palette copy cannot retain %x -> %x\n",source,destination);exit(1);
 }
 for(unsigned n=0;n<16;n++)colorWrite(destination+n*2,paletteWord(source+n*2)|(colorRead(destination+n*2)&0xf000));
 nativeCopies++;
}
static void nativeCompare(void){
 for(unsigned n=0;n<3072;n++){
  uint16_t value;
  if(!willowMutablePaletteRead(&nativeOwner,nativeSlots,2,0x91c000+n*2,paletteWord,&value)||value!=colorRead(0x91c000+n*2)){
   fprintf(stderr,"Native opening palette mismatch word %u\n",n);exit(1);
  }
  nativeWords++;
 }
 /* Opening transfers must leave both mutable player slots available. */
 for(unsigned n=0;n<2;n++)if(nativeSlots[n].group!=0xffff){fprintf(stderr,"Opening palette took a player slot\n");exit(1);}
}
#endif
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;

 willowMotionPutLong(rom+0xc2e,0xff2000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/opening-actors-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/opening-actors-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at)); \
 for(unsigned slice=0;slice<8&&m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Opening oracle did not return: PC %x frame %u\n",m68k_get_reg(16),checks);return 1;} \
} while(0)
 unsigned checks=0;
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 memset(expected,0,sizeof expected);
 willowArcadePoolsInit(&bus);
 for(unsigned group=0;group<4;group++)willowArcadeVisibilityInit(&bus,group,0);
 /* Source scene-spawn palette records, followed by backdrop and both pan variants. */
 const uint32_t paletteSources[]={0xea660,0xea700,0xebb00,0xebb60,0xebbc0,0xebca0,0xebd80};
 const uint8_t groups[]={1,3,3,4,5,6,8},sequences[]={0,1,2,2,3,3,3};
 for(unsigned n=0;n<7;n++){
  uint8_t*r=expected+0xf042+n*64;r[0]=1;r[32]=1;r[56]=groups[n];
  r[59]=sequences[n];r[58]=n<4?4:2;r[60]=n<2?2:3;
  willowMotionPutLong(r+52,paletteSources[n]);
 }
#ifdef WILLOW_TEST_PAN_PALETTE_LIVE
 expected[0xf062]=0; /* Also cover the admitted cyclic type-zero handler. */
#endif
 uint8_t*r=expected+0xb646;r[0]=1;r[32]=8;
 willowMotionPutWord(r+6,0x4c0);willowMotionPutWord(r+10,0x40);
 busPutWord(0x8408,0x400);busPutWord(0x840c,0);busPutByte(0x8293,1);
 WillowPaletteAnimation g={0};g.byte=sourceByte;g.word=paletteWord;g.read=colorRead;g.write=colorWrite;
#ifdef WILLOW_TEST_NATIVE_OPENING_DATA
 nativeSlots[0].group=nativeSlots[1].group=0xffff;
 for(unsigned group=0;group<192;group++){
  uint32_t source=0xe9920+(group%32)*32;
  if(!willowNativePaletteSourceBind(&nativeOwner.sources,0x91c000+group*32,source))return 1;
  willowNativePaletteOwnerOffset(&nativeOwner,group,group&15);
  for(unsigned n=0;n<16;n++)colorWrite(0x91c000+group*32+n*2,(uint16_t)(paletteWord(source+n*2)+((group&15)<<12)));
 }
 g.copy=nativeCopy;
#endif
 for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
 for(unsigned frame=0;frame<512;frame++){
#ifdef WILLOW_TEST_PAN_PALETTE_LIVE
  busPutByte(0x8293,frame%5==0);busPutByte(0x83c3,frame%5==1);
  busPutWord(0x8408,frame%5==2?0x8000:0x400);busPutWord(0x840c,frame%5==3?0x8000:0);
  for(unsigned n=0;n<7;n++){
   willowMotionPutWord(expected+0xf042+n*64+8,0x1234);
   willowMotionPutWord(expected+0xf042+n*64+12,0x2345);
  }
#endif
  /* Publication is tested separately. Reset only queue cursors in both
   * machines before this isolated actor-chain call. */
  for(unsigned group=0;group<4;group++)willowArcadeVisibilityInit(&bus,group,0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  /* Relocated CPS palette pointers are canonicalized on comparison. */
  for(unsigned n=0;n<7;n++){
   unsigned at=0xf042+n*64+48;uint32_t value=willowMotionLong(expected+at);
   if(value){value=value-0x91c000+0xff2000;putRamWord(ram,at,value>>16);putRamWord(ram,at+2,value);}
  }
#ifdef WILLOW_TEST_PAN_PALETTE_LIVE
  uint16_t d1=frame*113+0xa512,d2=frame*277+0x5a34;
  m68k_set_reg(1,d1);m68k_set_reg(2,d2);
  if(!willowArcadeOpeningPalettesWithLive(&bus,resolve,&g,&d1,&d2))return 1;
  RUN(0xbc4,0);
  if((m68k_get_reg(1)&65535)!=d1||(m68k_get_reg(2)&65535)!=d2){
   fprintf(stderr,"Palette live words frame %u: %x/%x expected %x/%x\n",frame,m68k_get_reg(1)&65535,m68k_get_reg(2)&65535,d1,d2);return 1;
  }
#else
  if(!willowArcadeOpeningActors(&bus,resolve,&g,sourceWord,releaseActor)){fprintf(stderr,"Opening model rejected frame %u\n",frame);return 1;}
  RUN(0x9ef2,0);
#endif
#ifdef WILLOW_TEST_NATIVE_OPENING_DATA
  nativeCompare();
#endif
  for(unsigned n=0;n<7;n++){
   unsigned at=0xf042+n*64+48;
   uint32_t value=((uint32_t)ram[at^1]<<24)|((uint32_t)ram[(at+1)^1]<<16)|((uint32_t)ram[(at+2)^1]<<8)|ram[(at+3)^1];
   if(value){value=value-0xff2000+0x91c000;putRamWord(ram,at,value>>16);putRamWord(ram,at+2,value);}
  }
  for(unsigned n=0;n<65536;n++)if((n<0xfed0||n>=0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Opening actor chain frame %u address %x actual %x expected %x\n",frame,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 /* Every unsupported active pool must reject before palette advancement. */
 uint8_t baseline[65536];memcpy(baseline,expected,sizeof baseline);
 for(unsigned classId=2;classId<=18;classId+=2){
  memcpy(expected,baseline,sizeof baseline);
  const WillowActorPoolSpec*spec=willowActorPoolSpecification(classId);
  unsigned at=spec->sourceBase;expected[at]=1;expected[at+32]=0;
  uint8_t snapshot[65536];memcpy(snapshot,expected,sizeof snapshot);
  if(willowArcadeOpeningActors(&bus,resolve,&g,sourceWord,releaseActor)||memcmp(snapshot,expected,sizeof snapshot))return 1;
 }
 memcpy(expected,baseline,sizeof baseline);expected[0x9846]=1;
 uint8_t snapshot[65536];memcpy(snapshot,expected,sizeof snapshot);
 if(willowArcadeOpeningActors(&bus,resolve,&g,sourceWord,releaseActor)||memcmp(snapshot,expected,sizeof snapshot))return 1;
 memcpy(expected,baseline,sizeof baseline);expected[0xf062]=2;
 memcpy(snapshot,expected,sizeof snapshot);
 if(willowArcadeOpeningActors(&bus,resolve,&g,sourceWord,releaseActor)||memcmp(snapshot,expected,sizeof snapshot))return 1;
#ifdef WILLOW_TEST_NATIVE_OPENING_DATA
 printf("Native opening resources: %u complete C92 transfers, %u retained palette-word comparisons; both player slots remain free\n",nativeCopies,nativeWords);
#endif
#ifdef WILLOW_TEST_PAN_PALETTE_LIVE
 printf("Palette live dispatch: %u BC4 updates match original RAM, D1 and D2\n",checks);
#else
 printf("Opening actor chain: %u frames match complete original 9EF2 execution, including palette sequencing and title animation/visibility\n",checks);
#endif
 return 0;
}
