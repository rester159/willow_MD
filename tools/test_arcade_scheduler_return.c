/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_task_scheduler.h"
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
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Keep scheduler save/restore instructions. Redirect only the three
  * context-dispatch pointers and stop at the scan continuation. */
 for(unsigned n=0;n<3;n++)willowMotionPutLong(rom+0x1512+n*4,0x100000);
 willowMotionPutWord(rom+0x14ec,0x4ef9);willowMotionPutLong(rom+0x14ee,0x100004);
 willowMotionPutWord(rom+0x100004,0x60fe);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scheduler-return-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scheduler-return-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 const uint8_t states[]={4,8,12};
 for(unsigned e=0;e<3;e++)for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  uint32_t slot=0xff0000+(sample%16)*32,stack=0xff4000+(sample%256)*64;
  uint16_t remaining=sample*977;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t saved=willowArcadeSchedulerSave(&bus,stack,slot,remaining);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,stack);m68k_set_reg(13,0xff8000);
  m68k_set_reg(8,slot);m68k_set_reg(0,0xbeef0000u|remaining);m68k_set_reg(1,states[e]);
  m68k_set_reg(16,0x14fa);retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(15)!=saved)return 1;
  for(unsigned n=0;n<65536;n++)if(ram[n^1]!=expected[n]){
   fprintf(stderr,"Scheduler save mismatch sample %u address %04x\n",sample,n);return 1;
  }
  WillowSchedulerReturn result=willowArcadeSchedulerRestore(&bus);
  m68k_set_reg(15,0xffe000);m68k_set_reg(0,0xdeadffff);m68k_set_reg(8,0xffaaaa);
  m68k_set_reg(16,0x151e);retro_run();
  if(result.pc!=0x14ec||m68k_get_reg(16)!=0x100004||m68k_get_reg(15)!=result.stack||
     m68k_get_reg(8)!=result.slot||m68k_get_reg(0)!=(0xdead0000u|result.remaining))return 1;
  for(unsigned n=0;n<65536;n++)if(ram[n^1]!=expected[n]){
   fprintf(stderr,"Scheduler restore mismatch sample %u address %04x\n",sample,n);return 1;
  }
  checks++;
 }
 printf("Source scheduler return: %u round trips, all dispatch states and slots; exact supervisor stack, current-task pointer, scan cursor and all RAM match\n",checks);
 return 0;
}
