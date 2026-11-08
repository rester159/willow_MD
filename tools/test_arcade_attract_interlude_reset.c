/* Headless original-instruction verification of 8EC4 -> 8EDE..8EF6. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_interlude_reset.h"
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

static void unusedColor(uint32_t at,uint16_t value){(void)at;(void)value;abort();}
static void resetTask(uint16_t*d1,uint16_t*d2)
{if(!willowArcadeAttractInterludeReset(&bus,romWord,0,0,d1,d2))abort();}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
#ifdef WILLOW_TEST_INTERLUDE_TASK
 willowMotionPutWord(rom+0x1750,0x60fe);
#endif
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/interlude-reset-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/interlude-reset-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0;
 for(unsigned sample=0;sample<513;sample++){
  if(sample==512){
   f=fopen("build/demo-post-sequence-investigation/sequence-output-ram.bin","rb");
   if(!f||fread(expected,1,sizeof expected,f)!=sizeof expected||fgetc(f)!=EOF)return 2;fclose(f);
  }else{
   for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
   unsigned free=sample%9;
   ww(0x208,sample&1?0xffff:0xff);ww(0x20a,0xa30-free*4);ww(0x20c,free);
   for(unsigned n=0;n<8;n++){
    ww(0xa10+n*4,sample&2?0xffff:0xff);ww(0xa12+n*4,0x100+n*32);
    wb(0x100+n*32,0);
   }
   ww(0x8004,8);ww(0x8008,0);
  }
  #ifdef WILLOW_TEST_INTERLUDE_TASK
  ww(0x8000,4);wb(0x8058,0);wb(0x82a2,0);wb(0x82a4,0);
  wb(0x828f,1);wb(0x8068,sample<512&&(sample&16)?2:0);
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d1=sample*311,d2=sample*977,d4=sample*73;uint32_t a4=0xffb000;
  unsigned entry=0x8ec4,stop=0x100000;
#ifdef WILLOW_TEST_INTERLUDE_TASK
  WillowNativeAttractTask task={0x810e,d1,d2};
  if(!(rb(0x8068)&2)&&willowNativeAttractTaskSupportedWithDemoTick(&task,&bus,romWord,unusedColor,0,0,0,0,0,0,0,0))return 1;
  WillowTransitionStep sleep=willowNativeAttractTaskStepWithInterlude(&task,&bus,romWord,unusedColor,0,0,0,0,0,0,0,0,0,0,resetTask);
  if(!sleep.supported||sleep.finished||sleep.delay!=1||sleep.resume!=0x810e)return 1;
  d1=task.d1;d2=task.d2;entry=0x810e;stop=0x1750;
#else
  if(!willowArcadeAttractInterludeReset(&bus,romWord,0,0,&d1,&d2))return 1;
#endif
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);m68k_set_reg(4,d4);m68k_set_reg(12,a4);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop)return fprintf(stderr,"Interlude reset stalled sample %u PC %x\n",sample,m68k_get_reg(16)),1;
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n])
   return fprintf(stderr,"Interlude reset sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]),1;
  if((uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2||
     (uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=a4)return fprintf(stderr,"Interlude register mismatch %u\n",sample),1;
  checks++;
 }
 uint8_t saved[65536];unsigned rejected=0;
 for(unsigned test=0;test<6;test++){
  ww(0x8004,test==0?6:8);ww(0x8008,test==1?2:0);
  memcpy(saved,expected,sizeof saved);uint16_t d1=0x1234,d2=0xabcd;
  if(willowArcadeAttractInterludeReset(test==2?NULL:&bus,test==3?NULL:romWord,0,0,
      test==4?NULL:&d1,test==5?NULL:&d2)||memcmp(saved,expected,sizeof saved)||d1!=0x1234||d2!=0xabcd)return 1;
  rejected++;
 }
 ww(0x8000,4);ww(0x8004,8);ww(0x8008,0);wb(0x8058,0);wb(0x828f,0);
 WillowNativeAttractTask task={0x810e,0x1234,0xabcd};
 for(unsigned mode=0;mode<4;mode++){
  ww(0x8008,mode==2?2:0);memcpy(saved,expected,sizeof saved);
  unsigned supported=willowNativeAttractTaskSupportedWithInterlude(&task,&bus,romWord,
   mode==1?NULL:unusedColor,0,0,0,0,0,0,0,0,mode==0?NULL:resetTask);
  if(supported!=(mode==3)||memcmp(saved,expected,sizeof saved)||task.d1!=0x1234||task.d2!=0xabcd)return 1;
 }
#ifdef WILLOW_TEST_INTERLUDE_TASK
 puts("Interlude manager: original 810E dispatch and freeze gate preserve task register/sleep behavior; old APIs require the new capability");
#endif
 printf("Interlude reset: %u original-CPU comparisons including the continuous post-demo endpoint; all nine pools, background actor records, fade allocation, unrelated RAM and D1/D2/D4/A4 match; %u invalid contexts reject unchanged\n",checks,rejected);
 return 0;
}
