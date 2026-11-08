/* Complete 2DA5E presentation setup through its mandatory fade-in sleep. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_transition_presentation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],source[0x100000];
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
static uint16_t romWord(uint32_t at){if(at+1>=sizeof source)abort();return willowMotionWord(source+at);}
static uint8_t romByte(uint32_t at){if(at>=sizeof source)abort();return source[at];}
static uint8_t*resolve(uint16_t at){return expected+at;}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


static unsigned writes,mapWrites;
static void mapWrite(uint16_t at,uint16_t v){if(at>=0x4000||(at&1))abort();ww(0x4000+at,v);mapWrites++;}
static void colorWrite(uint32_t at,uint16_t v){
 if(at<0x91c000||at>=0x91d800||(at&1))abort();
 ww(0x2000+at-0x91c000,v);writes++;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(source,rom,sizeof source);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 const unsigned patches[]={0xae4,0xb06,0xaf6,0xb16,0x988,0x99a,0x9b0,0x9cc,0x9da};
 const uint32_t targets[]={0x91c800,0x91cc00,0x91c400,0x91c200,0x91c000,0x91c200,0x91c800,0x91c400,0x91cc00};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){
  if(willowMotionLong(rom+patches[n])!=targets[n])return 2;
  willowMotionPutLong(rom+patches[n],targets[n]-0x91c000+0xff2000);
 }
 willowMotionPutLong(rom+0x80,0x15d0);
 if(willowMotionLong(rom+0x4365a)!=0x918000)return 2;
 willowMotionPutLong(rom+0x4365a,0xff4000);
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/transition-presentation-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/transition-presentation-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  const uint16_t actor=0xdbc6+(sample%60)*64;
  wb(actor+3,10);wb(0xc0,sample%4);ww(0x801a,(sample*4)&252);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t initialA4=sample&1?0xffb000:0x123456,a4=initialA4;
  writes=mapWrites=0;
  uint16_t wait=willowArcadeTransitionPresentationWithA4(&bus,resolve,actor,romByte,romWord,colorWrite,mapWrite,&a4);
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x2da5e);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  unsigned sp=m68k_get_reg(15)&65535;
  unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
  if(m68k_get_reg(16)!=0x100000||writes!=1792||mapWrites!=1024||resume!=wait||wait!=0x1ec4||(m68k_get_reg(0)&65535)!=1){
   fprintf(stderr,"Presentation boundary sample %u PC %x resume %x wait %x writes %u/%u\n",sample,m68k_get_reg(16),resume,wait,writes,mapWrites);return 1;
  }
  if(m68k_get_reg(12)!=a4){fprintf(stderr,"Presentation A4 sample %u actual %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Presentation mismatch sample %u address %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  /* Resume the actual tail-called fade wait after controlled completion.
   * It must return directly, without the ordinary transition flag clears. */
  wb(0xf891,0);ram[0xf891^1]=0;
  m68k_set_reg(15,0xff0000|(sp+4));m68k_set_reg(16,resume);
  for(unsigned slice=0;slice<8&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04)return 1;
  if(m68k_get_reg(12)!=a4){fprintf(stderr,"Presentation A4 sample %u actual %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n])return 1;
  checks++;
 }
 unsigned exitChecks=0;
 for(unsigned mode=0;mode<2;mode++)for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*11+sample*37)^0x5a;
  const uint16_t actor=0xdbc6+(sample%60)*64;
  const uint16_t timers[]={0,1,2,255,256,257,32768,65535};
  ww(actor+34,timers[sample%8]);wb(actor+3,12);wb(0x806a,sample&255);
  wb(0x8290,(sample>>8)&1);wb(0x8293,(sample>>9)&1);
  ww(0x801a,(sample*4)&252);ww(0x801e,(sample*2)&126);ww(0x8344,sample%7);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t initialA4=sample&1?0xffb000:0x123456,a4=initialA4;
  writes=mapWrites=0;uint16_t wait=0;
  if(mode){if(!willowArcadeTransitionPresentationRestore(&bus,romWord,colorWrite))return 1;}
  else wait=willowArcadeTransitionPresentationTickWithA4(&bus,actor,&a4);
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(15,0xffff00);m68k_set_reg(16,mode?0x2db0a:0x2dae4);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000||(mode&&writes!=1792)||mapWrites)return 1;
  unsigned sp=m68k_get_reg(15)&65535;
  if(wait){
   unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
   if(resume!=wait||wait!=0x1ea2||(m68k_get_reg(0)&65535)!=1)return 1;
  }else if(sp!=0xff04)return 1;
  if(m68k_get_reg(12)!=a4){fprintf(stderr,"Presentation A4 sample %u actual %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Presentation exit mismatch mode %u sample %u address %x actual %x expected %x\n",mode,sample,n,ram[n^1],expected[n]);return 1;
  }
  exitChecks++;
 }
 printf("Transition presentation: %u original setup/wait/return comparisons; full RAM/A4, 1792 palette words, 1024 map words, occupied/free text task and wrapped command queue match; %u countdown/restore comparisons cover word timer wrap, input, sound gates and all seven stage palettes\n",checks,exitChecks);
 return 0;
}
