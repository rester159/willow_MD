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
 for(unsigned n=0;n<16;n++)willowMotionPutWord(rom+0x100000+n*4,0x60fe);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/task-context-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/task-context-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 const uint8_t states[]={4,8,12};
 const uint16_t entries[]={0x1528,0x152e,0x153e};
 for(unsigned e=0;e<3;e++)for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  uint16_t slot=(sample%16)*32;
  wb(slot,states[e]);wb(slot+1,sample);
  ww(slot+2,(sample&31)|((sample&7)<<8));
  uint32_t pc=0x100000+(sample%16)*4;
  ww(slot+4,pc>>16);ww(slot+6,pc);
  ww(slot+8,0xff);ww(slot+10,0x4000+(sample%256)*2);
  ww(slot+12,0xff);ww(slot+14,0x6000+(sample%256)*2);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  WillowTaskContext context=willowArcadeTaskContext(&bus,slot);
  if(!context.supported||context.fresh!=(e==2))return 1;
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(8,0xff0000+slot);
  m68k_set_reg(16,entries[e]);retro_run();
  if(m68k_get_reg(16)!=context.pc||m68k_get_reg(15)!=context.stack||m68k_get_reg(17)!=context.sr){
   fprintf(stderr,"Context mismatch state %u sample %u pc %x/%x sp %x/%x sr %x/%x\n",states[e],sample,m68k_get_reg(16),context.pc,m68k_get_reg(15),context.stack,m68k_get_reg(17),context.sr);return 1;
  }
  for(unsigned n=0;n<65536;n++)if(n<0xfefa||n>=0xff00){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Context RAM mismatch state %u sample %u address %04x\n",states[e],sample,n);return 1;}
  }
  checks++;
 }
 printf("Source task context: %u dispatches; states 4/8/12, original RTE/fresh user-mode entry, saved versus initial stack, status and non-stack RAM match\n",checks);
 return 0;
}
