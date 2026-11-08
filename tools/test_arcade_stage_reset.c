/* Headless differential verification of original 2DFE stage reset. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_stage_reset.h"
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


int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x8474,0x60fe);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/stage-reset-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/stage-reset-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;

 const uint16_t timers[]={0,1,2,10,30,180,0x8000,0xffff};
 for(unsigned sample=0;sample<4096+(28+19+42)*4*64;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;

  uint16_t pc=0x2dfe;unsigned stage=sample&1;ww(0x8344,stage);
  wb(0x8370,(sample/2)%(stage?7:16));wb(0x8372,(sample/8)&255);
  wb(0x8394,(sample/32)%(stage?10:7));wb(0x838a,sample&16);wb(0x8395,sample&4);ww(0x8398,sample*257);
  ww(0x800a,timers[(sample/4)%8]);wb(0x0e58,(sample/32)%2?0xff:0);
  wb(0xc0,(sample/64)%2?0x0c:0);ww(0x82c0,(sample/256)%2?0xffff:0);
  ww(0x8008,sample%3?8:0xffff);ww(0x8004,sample%3?0:0xffff);
  ww(0x801e,((sample/4)%64)*2);wb(0x8290,sample&2);wb(0x8293,sample&4);
  if(sample>=4096&&sample<4096+28*4*64){
   unsigned index=sample-4096;ww(0x8344,2+index/(14*4*64));wb(0x8394,(index/(4*64))%14);
   wb(0x8290,index&1);wb(0x8293,(index>>1)&1);ww(0x801e,((index>>2)%64)*2);
  }
  if(sample>=4096+28*4*64&&sample<4096+(28+19)*4*64){
   unsigned index=sample-4096-28*4*64;ww(0x8344,4);wb(0x8394,index/(4*64));
   wb(0x8290,index&1);wb(0x8293,(index>>1)&1);ww(0x801e,((index>>2)%64)*2);
  }
  if(sample>=4096+(28+19)*4*64){
   unsigned index=sample-4096-(28+19)*4*64,record=index/(4*64),section=0;
   const unsigned counts[7]={8,5,9,9,1,9,1};
   while(record>=counts[section])record-=counts[section++];
   ww(0x8344,5);ww(0x8374,section);wb(0x8394,record);
   wb(0x8290,index&1);wb(0x8293,(index>>1)&1);ww(0x801e,((index>>2)%64)*2);
  }
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeStageReset(&bus,romWord))return 1;
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Scene wait mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 uint8_t saved[65536];ww(0x8344,6);memcpy(saved,expected,sizeof saved);
 if(willowArcadeStageReset(&bus,romWord)||willowArcadeStageReset(NULL,romWord)||memcmp(saved,expected,sizeof saved))return 1;
 printf("Stage reset: %u source comparisons; all six stages/seven final-stage sections, full clear ranges, checkpoint timer, sound command gate/ring and retained player record match\n",checks);
 return 0;
}
