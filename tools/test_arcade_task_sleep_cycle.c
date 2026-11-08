/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_task_scheduler.h"
#include "../src/willow_arcade_tasks.h"
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
 willowMotionPutWord(rom+0x100004,0x60fe);
 willowMotionPutLong(rom+0x8c,0x175c);
 willowMotionPutWord(rom+0x151e,0x4ef9);willowMotionPutLong(rom+0x1520,0x100000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/task-sleep-cycle-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/task-sleep-cycle-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  uint16_t slot=(sample%16)*32,sr=(sample>>3)&31;
  uint32_t stack=0xff4000+(sample%256)*128;
  uint32_t registers[15],restored[15];
  for(unsigned n=0;n<15;n++)registers[n]=0x12345678u+sample*977+n*0x13579u;
  registers[0]=sample%256;registers[13]=0xff8000;
  ww(0x0204,0xff);ww(0x0206,slot);ww((uint16_t)stack,0x0010);ww((uint16_t)(stack+2),4);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t saved=willowArcadeTaskSaveRegisters(&bus,stack,registers);
  willowArcadeTaskSleep(&bus,registers[0],sr,0x1756,saved);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);
  m68k_set_reg(17,sr);m68k_set_reg(15,stack);
  for(unsigned n=0;n<15;n++)m68k_set_reg(n,registers[n]);
  m68k_set_reg(16,0x1750);retro_run();
  if(m68k_get_reg(16)!=0x100000)return 1;
  for(unsigned n=0;n<65536;n++)if(n<0xfefa||n>=0xff00){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Sleep save mismatch sample %u address %04x\n",sample,n);return 1;}
  }
  /* Explicit external resume, including suspended zero-delay cases. This
   * checks context restoration, not automatic wake timing. */
  wb(slot,4);ram[slot^1]=4;
  WillowTaskContext context=willowArcadeTaskContext(&bus,slot);
  uint32_t returnStack=willowArcadeTaskRestoreRegisters(&bus,context.stack,restored);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(8,0xff0000+slot);
  m68k_set_reg(16,0x1528);retro_run();
  if(m68k_get_reg(16)!=0x100004||m68k_get_reg(15)!=returnStack+4||m68k_get_reg(17)!=sr)return 1;
  for(unsigned n=0;n<15;n++)if(m68k_get_reg(n)!=restored[n]||restored[n]!=registers[n]){
   fprintf(stderr,"Sleep restore mismatch sample %u reg %u\n",sample,n);return 1;
  }
  for(unsigned n=0;n<65536;n++)if(n<0xfefa||n>=0xff00){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Sleep restore RAM mismatch sample %u address %04x\n",sample,n);return 1;}
  }
  checks++;
 }
 printf("Source sleep cycles: %u original MOVEM/TRAP/RTE/MOVEM/RTS cycles; all fifteen registers, task context, stack, CCR and non-stack RAM match\n",checks);
 return 0;
}
