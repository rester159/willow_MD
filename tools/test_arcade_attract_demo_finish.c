/* Headless original-instruction verification of 8E56..8EC2. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_attract_demo_finish.h"
#include "../src/willow_native_attract_task.h"
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

static uint8_t expected[65536];

static uint16_t romWord(uint32_t at){return willowMotionWord(rom+at);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


static const WillowVisibilityBus bus={rw,rb,ww,wb};

#ifdef WILLOW_TEST_NATIVE_DEMO_FINISH_TASK
static uint32_t taskA4;
static void unusedColor(uint32_t at,uint16_t value){(void)at;(void)value;abort();}
static void finishTask(uint16_t*d1,uint16_t*d2)
{if(!willowNativeAttractDemoFinishBoundary(&bus,romWord,d1,d2,&taskA4))abort();}
#endif
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
#ifdef WILLOW_TEST_NATIVE_DEMO_FINISH_TASK
 willowMotionPutWord(rom+0x1750,0x60fe);
#endif
 willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0xa0,0x16be);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/demo-finish-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/demo-finish-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0,exits=0;const uint16_t timers[]={0,1,2,180,65535};
 for(unsigned sample=0;sample<3840;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  unsigned state=sample&1?8:6,stage=(sample/2)%6,busy=(sample/12)%2;
  ww(0x8004,6);ww(0x8008,state);ww(0x8344,stage);wb(0xe58,busy);
  ww(0x800a,timers[(sample/24)%5]);wb(0xc0,(sample/120)%2?4:0);
  wb(0x8290,(sample/240)%2);wb(0x8293,(sample/480)%2);ww(0x801e,(sample*2)&126);
  unsigned tag=(sample/960)%4;wb(0x8297,tag);
  for(unsigned slot=0x100;slot<0x200;slot+=32){wb(slot,slot==0x160||slot==0x1c0?4:0);wb(slot+22,slot==0x160||tag==1?1:2);}
  ww(0x208,0xff);ww(0x20a,0xa30);ww(0x20c,2);
  #ifdef WILLOW_TEST_NATIVE_DEMO_FINISH_TASK
  ww(0x8000,4);wb(0x8058,0);wb(0x82a2,0);wb(0x82a4,0);
  wb(0x828f,1);wb(0x8068,(sample/1920)?2:0);
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d1=sample*311,d2=sample*977;uint32_t a4=0xffb000;
  unsigned stop=0x100000,pc=state==6?0x8e56:0x8ea0;
#ifdef WILLOW_TEST_NATIVE_DEMO_FINISH_TASK
  taskA4=a4;WillowNativeAttractTask task={0x810e,d1,d2};
  if(!(rb(0x8068)&2)&&willowNativeAttractTaskSupportedWithDemoStart(&task,&bus,romWord,unusedColor,0,0,0,0,0,0))return 1;
  WillowTransitionStep step=willowNativeAttractTaskStepWithDemoFinish(&task,&bus,romWord,unusedColor,0,0,0,0,0,0,0,0,finishTask);
  if(!step.supported||step.finished||step.delay!=1||step.resume!=0x810e)return 1;
  d1=task.d1;d2=task.d2;a4=taskA4;pc=0x810e;stop=0x1750;
#else
  if(!willowArcadeAttractDemoFinish(&bus,romWord,&d1,&d2,&a4))return 1;
#endif
  if(rw(0x8004)==8)exits++;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,0xffb000);m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Finish boundary sample %u PC %x\n",sample,m68k_get_reg(16));return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Finish sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2){fprintf(stderr,"Finish registers sample %u D1 %x/%x D2 %x/%x A4 %x/%x\n",sample,m68k_get_reg(1)&65535,d1,m68k_get_reg(2)&65535,d2,m68k_get_reg(12),a4);return 1;}
  #ifdef WILLOW_TEST_NATIVE_DEMO_FINISH_TASK
  unsigned sp=m68k_get_reg(15)&65535;
  uint32_t resume=0;for(unsigned n=0;n<4;n++)resume=(resume<<8)|ram[(sp+n)^1];
  if(resume!=0x810e||(m68k_get_reg(0)&65535)!=1)return 1;
#endif
  checks++;
 }
 unsigned rejected=0;uint8_t saved[65536];
 for(unsigned test=0;test<9;test++){
  ww(0x8004,test==0?4:6);ww(0x8008,test==1?4:6);ww(0x8344,test==2?6:0);wb(0xe58,0);
  memcpy(saved,expected,sizeof saved);uint16_t d1=0x1234,d2=0xabcd;uint32_t a4=0xff9876;
#ifdef WILLOW_TEST_NATIVE_DEMO_FINISH_TASK
  if(willowNativeAttractDemoFinishBoundary(test==3?NULL:&bus,
#else
  if(willowArcadeAttractDemoFinish(test==3?NULL:&bus,
#endif
      test==4?NULL:romWord,
      test==5?NULL:&d1,test==6?NULL:&d2,test==7?NULL:&a4)!=(test==8)||
      (test!=8&&(memcmp(saved,expected,sizeof saved)||d1!=0x1234||d2!=0xabcd||a4!=0xff9876)))return 1;
  if(test!=8)rejected++;
 }
 #ifdef WILLOW_TEST_NATIVE_DEMO_FINISH_TASK
 puts("Native finish manager: original 810E dispatch, freeze gate, D1 jump-table result, post-service input/flags and one-tick sleep boundary match");
#endif
 printf("Demo finish: %u original-instruction comparisons, %u phase-eight exits; both message waits, 180-tick countdown, sound gates/wrap, first-match/no-match task cancellation and D1/D2/A4 exact; %u invalid contexts reject before mutation\n",checks,exits,rejected);
 return 0;
}
