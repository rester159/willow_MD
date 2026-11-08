/* Headless differential verification of demo initialization and its task boundary. */
#ifdef WILLOW_TEST_NATIVE_DEMO_TASK
#define WILLOW_TEST_NATIVE_DEMO_INIT
#endif
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_demo_init.h"
#include "../src/willow_native_attract_demo_init.h"
#include "../src/willow_native_task_runtime.h"
#include "../native/intro_probe/src/demo_init_data.h"
#include "../native/intro_probe/src/palette_initial_data.h"
#include "../native/intro_probe/src/task_startup_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
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

static uint8_t expected[65536],busAccess[65536];
static unsigned auditBus;

static uint16_t romWord(uint32_t at){return willowMotionWord(rom+at);}
static uint16_t rw(uint16_t a){if(auditBus){busAccess[a]|=1;busAccess[(uint16_t)(a+1)]|=1;}return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){if(auditBus)busAccess[a]|=1;return expected[a];}
static void ww(uint16_t a,uint16_t v){if(auditBus){busAccess[a]|=2;busAccess[(uint16_t)(a+1)]|=2;}willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){if(auditBus)busAccess[a]|=2;expected[a]=v;}


/* Palette RAM is relocated only for the CPU oracle, not a source bus access. */
static void colorWrite(uint32_t at,uint16_t v){willowMotionPutWord(expected+(0x2000+at-0x91c000),v);}
#ifdef WILLOW_TEST_NATIVE_DEMO_INIT
static unsigned copiedWords,clearedBytes;
static uint16_t nativeWord(uint32_t at){
 uint16_t v;
 if(nativeDemoInitWordRead(at,&v)||nativePaletteInitialWordRead(at,&v)){}
 else if(at>=0x8cca&&at<0x8cda&&!(at&1))v=nativeAttractReplayDescriptors[(at-0x8cca)/2];
 else if(at>=0x1e34&&at<0x1e54&&!(at&1))v=nativeTaskCommandTargets[(at-0x1e34)/2];
 else{fprintf(stderr,"Missing native demo word %x\n",at);abort();}
 if(v!=romWord(at))abort();return v;
}
static void nativeCopy(uint32_t source,uint32_t destination,uint16_t count){
 for(unsigned n=0;n<count;n++)colorWrite(destination+n*2,nativeWord(source+n*2));copiedWords+=count;
}
static void nativeZero(const WillowVisibilityBus*b,uint16_t at,uint16_t count){
 for(unsigned n=0;n<count;n+=2)b->putWord(at+n,0);clearedBytes+=count;
}
#ifdef WILLOW_TEST_NATIVE_DEMO_TASK
static const WillowVisibilityBus demoTaskBus={rw,rb,ww,wb};
static void demoTaskInitialize(uint16_t*d1,uint16_t*d2)
{
 if(!willowNativeAttractDemoInitBoundary(&demoTaskBus,nativeWord,colorWrite,nativeZero,0,nativeCopy,d1,d2))abort();
}
#endif
#endif
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 /* Keep original reset-vector data: player continuation can read ROM zero.
  * Every execution below explicitly supplies PC/SP before retro_run. */
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 const unsigned operands[]={0x988,0x99a,0x9b0,0x9cc,0x9da};
 const unsigned offsets[]={0,0x200,0x800,0x400,0xc00};
 for(unsigned n=0;n<5;n++)willowMotionPutLong(rom+operands[n],0xff2000+offsets[n]);
 willowMotionPutWord(rom+0x65a4,0x60fe);
#ifdef WILLOW_TEST_NATIVE_DEMO_TASK
 willowMotionPutWord(rom+0x1750,0x60fe);
#endif
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/attract-demo-init-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-demo-init-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;

 const uint16_t timers[]={0,1,2,10,30,180,0x8000,0xffff};
 for(unsigned sample=0;sample<1024;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;

  uint16_t pc=0x8cda;ww(0xe5a,sample%2);wb(0x828f,sample&2);wb(0x8067,((sample/2)%6)*32+(sample%32));
  ww(0x800a,timers[(sample/4)%8]);wb(0x0e58,(sample/32)%2?0xff:0);
  wb(0xc0,(sample/64)%2?0x0c:0);ww(0x82c0,(sample/256)%2?0xffff:0);
  ww(0x8008,sample%3?8:0xffff);ww(0x8004,sample%3?0:0xffff);
  ww(0x828a,sample&4?0xff:0);ww(0x828c,sample&4?0x7000:sample&8?0:0x183c+(sample%4)*2);
  wb(0x8346,sample&255);wb(0x8291,(sample*17)&255);wb(0x839b,(sample/4)&255);
  willowArcadeTaskInitFree(&bus,romWord);
  for(unsigned n=0;n<sample%9;n++)willowArcadeTaskAllocate(&bus,0x509c,0,3);
#ifdef WILLOW_TEST_NATIVE_DEMO_INIT
  ww(0x8004,6);ww(0x8008,0);pc=0x8cb4;
#endif
#ifdef WILLOW_TEST_NATIVE_DEMO_TASK
  ww(0x8000,4);wb(0x8058,0);wb(0x82a4,0);wb(0x82a2,0);wb(0x8068,0);pc=0x810e;
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  auditBus=1;
#ifdef WILLOW_TEST_NATIVE_DEMO_INIT
  uint16_t d1=sample*311,d2=sample*977;copiedWords=clearedBytes=0;
#ifdef WILLOW_TEST_NATIVE_DEMO_TASK
  WillowNativeAttractTask task={pc,d1,d2};
  if(willowNativeAttractTaskSupportedWithOpening(&task,&bus,nativeWord,colorWrite,0,0,0,0))return 1;
  WillowTransitionStep step=willowNativeAttractTaskStepWithDemo(&task,&bus,nativeWord,colorWrite,0,0,0,0,0,0,demoTaskInitialize);
  if(!step.supported||step.finished||step.delay!=1||step.resume!=0x810e||copiedWords!=1792||!clearedBytes)return 1;
  d1=task.d1;d2=task.d2;
#else
  if(!willowNativeAttractDemoInitBoundary(&bus,nativeWord,colorWrite,nativeZero,0,nativeCopy,&d1,&d2)||copiedWords!=1792||!clearedBytes)return 1;
#endif
#else
  if(!willowArcadeAttractDemoInit(&bus,romWord,colorWrite))return 1;
#endif
  auditBus=0;
  unsigned stop=0x100000;
#ifdef WILLOW_TEST_NATIVE_DEMO_TASK
  stop=0x1750;
#endif
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Scene wait mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
#ifdef WILLOW_TEST_NATIVE_DEMO_INIT
  if((uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2)return 1;
#endif
#ifdef WILLOW_TEST_NATIVE_DEMO_TASK
  unsigned sp=m68k_get_reg(15)&65535;
  unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
  if(resume!=0x810e||(m68k_get_reg(0)&65535)!=1)return 1;
#endif
  checks++;
 }
#ifdef WILLOW_TEST_NATIVE_DEMO_INIT
 f=fopen("build/native-demo-init-bus-access.bin","wb");if(!f||fwrite(busAccess,1,sizeof busAccess,f)!=sizeof busAccess)return 2;fclose(f);
 ww(0x8004,6);ww(0x8008,2);uint16_t d1=0x1234,d2=0xabcd;
 uint8_t untouched[65536];memcpy(untouched,expected,sizeof untouched);unsigned prior=copiedWords;
 if(willowNativeAttractDemoInitBoundary(&bus,nativeWord,colorWrite,nativeZero,0,nativeCopy,&d1,&d2)||d1!=0x1234||d2!=0xabcd||copiedWords!=prior||memcmp(untouched,expected,sizeof untouched))return 1;
#endif
 ww(0x8008,0);
 uint8_t saved[65536];wb(0x828f,1);wb(0x8067,0xe0);memcpy(saved,expected,sizeof saved);
#ifdef WILLOW_TEST_NATIVE_DEMO_INIT
 if(willowNativeAttractDemoInitBoundary(&bus,nativeWord,colorWrite,nativeZero,0,nativeCopy,&d1,&d2)||d1!=0x1234||d2!=0xabcd||copiedWords!=prior||memcmp(saved,expected,sizeof saved))return 1;
#endif
 if(willowArcadeAttractDemoInit(&bus,romWord,colorWrite)||willowArcadeAttractDemoInit(NULL,romWord,colorWrite)||memcmp(saved,expected,sizeof saved))return 1;
#ifdef WILLOW_TEST_NATIVE_DEMO_TASK
 puts("Demo task capability: original one-tick sleep, 810E continuation and live D1/D2 verified; legacy opening capability still rejects phase six");
#endif
 printf("Demo init: %u source comparisons; replay, pool/player reset, full CPS palettes, checkpoint record and blink allocation match\n",checks);
 return 0;
}
