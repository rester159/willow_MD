/* Original-instruction verification of complete 8DA8 demo-start boundary. */
#ifdef WILLOW_TEST_NATIVE_DEMO_START_TASK
#define WILLOW_TEST_NATIVE_DEMO_START
#endif
#include "vendor/libretro.h"
#include "../src/willow_native_attract_task.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_demo_start.h"
#include "../src/willow_arcade_pool_init.h"
#include "../src/willow_native_attract_demo_start.h"
#ifdef WILLOW_TEST_NATIVE_DEMO_START
#include "../native/intro_probe/src/demo_start_data.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],original[0x100000];
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

static uint8_t expected[65536],romAccess[0x100000],busAccess[65536];
static unsigned audit;

static uint16_t romWord(unsigned long at){
 if(at>=sizeof original-1||(at&1))abort();
 uint16_t value=willowMotionWord(original+at);
 if(audit){
  romAccess[at]=romAccess[at+1]=1;
#ifdef WILLOW_TEST_NATIVE_DEMO_START
  uint16_t retained;
  if(!nativeDemoStartWordRead(at,&retained)||retained!=value){fprintf(stderr,"Missing/mismatched retained demo-start word %lx\n",at);abort();}
  return retained;
#endif
 }
 return value;
}
static uint16_t word32(uint32_t at){return romWord(at);}
static uint16_t rw(uint16_t a){if(audit){busAccess[a]|=1;busAccess[(uint16_t)(a+1)]|=1;}return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){if(audit)busAccess[a]|=1;return expected[a];}
static void ww(uint16_t a,uint16_t v){if(audit){busAccess[a]|=2;busAccess[(uint16_t)(a+1)]|=2;}willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){if(audit)busAccess[a]|=2;expected[a]=v;}


static uint8_t*resolve(uint16_t at){return expected+at;}
static void textWrite(uint32_t at,uint16_t value){if(at<0x910000||at>=0x914000)abort();ww(0x4000+at-0x910000,value);}
#ifdef WILLOW_TEST_NATIVE_DEMO_START_TASK
static const WillowVisibilityBus demoTaskBus={rw,rb,ww,wb};
static void demoTaskStart(uint16_t*d1,uint16_t*d2)
{if(!willowNativeAttractDemoStartBoundary(&demoTaskBus,resolve,romWord,word32,textWrite,d1,d2))abort();}
static void unusedColor(uint32_t at,uint16_t value){(void)at;(void)value;abort();}
#endif
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(original,rom,sizeof original);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
#ifdef WILLOW_TEST_NATIVE_DEMO_START_TASK
 willowMotionPutWord(rom+0x1750,0x60fe);
#endif
 willowMotionPutWord(rom+0x8474,0x60fe);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 const unsigned patches[]={0x574a,0x5764,0x577e,0x5798,0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x9115ec,0x9116ec,0x91136c,0x9110ec,0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/demo-start-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/demo-start-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;

 const uint16_t timers[]={0,1,2,10,30,180,0x8000,0xffff};
 for(unsigned sample=0;sample<1024;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;

  uint32_t pc=0x8da8;willowArcadePoolsInit(&bus);ww(0x8344,sample&1);
  willowArcadeTaskInitFree(&bus,word32);ww(0x020c,sample%9);
  wb(0x8394,(sample/2)%((sample&1)?10:7));
  wb(0x8366,(sample/4)%7);wb(0x8367,(sample/8)%5);wb(0x9866,sample&2?4:0);
  wb(0x8363,5);wb(0x8364,sample%6);
  ww(0x801a,((sample/4)%64)*4);ww(0x801e,((sample/8)%64)*2);
  wb(0x8062,sample&4?0x20:0);ww(0x0e6a,sample*61);ww(0x0e68,sample*127);
  ww(0x984c,(sample*127)&0x1fff);ww(0x9850,(sample*53)&0x3ff);ww(0x838c,(sample*31)&0x1fff);
  for(unsigned n=0;n<sample%17;n++)wb(0xf042+n*64,1);
  const uint16_t counts[]={0x9801,0x9804,0x980a,0x980d,0x9810,0x9813,0xf48c};
  for(unsigned n=0;n<7;n++)if(sample&(1u<<(n+3)))wb(counts[n],sample%3);

  ww(0x800a,timers[(sample/4)%8]);wb(0x0e58,(sample/32)%2?0xff:0);
  wb(0xc0,(sample/64)%2?0x0c:0);ww(0x82c0,(sample/256)%2?0xffff:0);
  ww(0x8008,sample%3?8:0xffff);ww(0x8004,sample%3?0:0xffff);
#ifdef WILLOW_TEST_NATIVE_DEMO_START
  ww(0x8004,6);ww(0x8008,2);
#endif
#ifdef WILLOW_TEST_NATIVE_DEMO_START_TASK
  ww(0x8000,4);wb(0x8058,0);wb(0x82a4,0);wb(0x82a2,0);wb(0x8068,0);pc=0x810e;
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  audit=1;
#ifdef WILLOW_TEST_NATIVE_DEMO_START
  uint16_t d1=sample*311,d2=sample*977;
#ifdef WILLOW_TEST_NATIVE_DEMO_START_TASK
  WillowNativeAttractTask task={pc,d1,d2};
  if(willowNativeAttractTaskSupportedWithDemo(&task,&bus,word32,unusedColor,0,0,0,0,0))return 1;
  WillowTransitionStep step=willowNativeAttractTaskStepWithDemoStart(&task,&bus,word32,unusedColor,0,0,0,0,0,0,0,demoTaskStart);
  if(!step.supported||step.finished||step.delay!=1||step.resume!=0x810e)return 1;
  d1=task.d1;d2=task.d2;
#else
  if(!willowNativeAttractDemoStartBoundary(&bus,resolve,romWord,word32,textWrite,&d1,&d2))return 1;
#endif
#else
  if(!willowArcadeAttractDemoStart(&bus,resolve,romWord,word32,textWrite))return 1;
#endif
  audit=0;
  unsigned stop=0x100000;
#ifdef WILLOW_TEST_NATIVE_DEMO_START_TASK
  stop=0x1750;
#endif
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Demo start mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
#ifdef WILLOW_TEST_NATIVE_DEMO_START
  if((uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2)return 1;
#endif
#ifdef WILLOW_TEST_NATIVE_DEMO_START_TASK
  unsigned sp=m68k_get_reg(15)&65535;
  unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
  if(resume!=0x810e||(m68k_get_reg(0)&65535)!=1)return 1;
#endif
  checks++;
 }
#ifdef WILLOW_TEST_NATIVE_DEMO_START
 for(unsigned invalid=0;invalid<8;invalid++){
  ww(0x8004,6);ww(0x8008,2);ww(0x8344,0);wb(0x8394,0);wb(0x8366,0);wb(0x8367,0);
  if(invalid==0)ww(0x8004,4);
  if(invalid==1)ww(0x8008,4);
  if(invalid==2)ww(0x8344,2);
  if(invalid==3)wb(0x8394,7);
  if(invalid==4)wb(0x8366,7);
  if(invalid==5)wb(0x8367,5);
  uint8_t saved[65536];memcpy(saved,expected,sizeof saved);
  uint16_t d1=0x1234,d2=0xabcd;
  if(willowNativeAttractDemoStartBoundary(&bus,invalid==6?0:resolve,romWord,word32,textWrite,invalid==7?0:&d1,&d2)||
     d1!=0x1234||d2!=0xabcd||memcmp(saved,expected,sizeof saved))return 1;
 }
 puts("Native demo-start boundary: D1=8/D2=16 match original instructions, retained ROM reads exact; eight invalid contexts preserve RAM and live words");
#endif
#ifdef WILLOW_TEST_NATIVE_DEMO_START_TASK
 puts("Demo-start task: original one-tick sleep, 810E continuation and live D1/D2 match; initializer-only capability rejects state two");
#else
 f=fopen("build/demo-start-rom-access.bin","wb");if(!f||fwrite(romAccess,1,sizeof romAccess,f)!=sizeof romAccess||fclose(f))return 2;
 f=fopen("build/demo-start-bus-access.bin","wb");if(!f||fwrite(busAccess,1,sizeof busAccess,f)!=sizeof busAccess||fclose(f))return 2;
#endif
 printf("Demo start 8DA8: %u complete original-routine comparisons; all non-stack RAM and HUD text match\n",checks);
 return 0;
}
