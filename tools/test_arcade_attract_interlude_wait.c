/* Headless original-instruction verification of 8EC4 -> 8FBA..8FEC. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_interlude_wait.h"
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

int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/interlude-wait-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/interlude-wait-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0;const uint16_t timers[]={0,1,2,10,30,0x8000,0xffff};
 for(unsigned sample=0;sample<448;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  ww(0x8004,8);ww(0x8008,6);ww(0x800a,timers[sample%7]);
  wb(0xe58,(sample/7)%4==0?0:(sample/7)%4==1?1:(sample/7)%4==2?0x80:0xff);
  wb(0xc0,(sample/28)%2?12:0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d1=sample*311,d2=sample*977,d4=sample*73;uint32_t a4=0xffb000;
  unsigned entry=0x8ec4,stop=0x100000;
  if(!willowArcadeAttractInterludeWait(&bus,romWord,&d1,&d2))return 1;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);m68k_set_reg(4,d4);m68k_set_reg(12,a4);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop)return fprintf(stderr,"Interlude wait stalled sample %u PC %x\n",sample,m68k_get_reg(16)),1;
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n])
   return fprintf(stderr,"Interlude wait sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]),1;
  if((uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2||
     (uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=a4)return fprintf(stderr,"Interlude wait register mismatch %u\n",sample),1;
  checks++;
 }
 uint8_t saved[65536];
 for(unsigned test=0;test<6;test++){
  ww(0x8004,test==0?6:8);ww(0x8008,test==1?8:6);
  memcpy(saved,expected,sizeof saved);uint16_t d1=0x1234,d2=0xabcd;
  if(willowArcadeAttractInterludeWait(test==2?NULL:&bus,test==3?NULL:romWord,
      test==4?NULL:&d1,test==5?NULL:&d2)||memcmp(saved,expected,sizeof saved)||d1!=0x1234||d2!=0xabcd)return 1;
 }
 printf("Interlude wait: %u original-CPU comparisons; busy message gating, countdown including wrap, message 3F launch, unrelated RAM and D1/D2/D4/A4 match; six invalid contexts reject unchanged\n",checks);
 return 0;
}
