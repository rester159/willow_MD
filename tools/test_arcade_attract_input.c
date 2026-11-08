/* Differential verification of queued text task against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_input.h"
#include "../native/intro_probe/src/palette_initial_data.h"
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
static uint16_t romWord(uint32_t at){
 uint16_t value;
 if(!nativePaletteInitialWordRead(at,&value)||value!=willowMotionWord(rom+at)){
  fprintf(stderr,"Missing or incorrect native palette word %x\n",at);abort();
 }
 return value;
}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


static unsigned writes;
static void colorWrite(uint32_t at,uint16_t v){
 if(at<0x91c000||at>=0x91d800||(at&1))abort();
 ww(0x2000+at-0x91c000,v);writes++;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 const unsigned patches[]={0x988,0x99a,0x9b0,0x9cc,0x9da};
 const uint32_t targets[]={0x91c000,0x91c200,0x91c800,0x91c400,0x91cc00};
 for(unsigned n=0;n<5;n++){
  if(willowMotionLong(rom+patches[n])!=targets[n])return 2;
  willowMotionPutLong(rom+patches[n],targets[n]-0x91c000+0xff2000);
 }
 willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x88,0x1690);
 willowMotionPutLong(rom+0xac,0x170c);willowMotionPutWord(rom+0x1636,0x60fe);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/attract-input-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-input-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,finished=0;
 for(unsigned sample=0;sample<256;sample++)for(unsigned mode=0;mode<8;mode++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  
  /* Initialize a valid pool with every combination of eight active slots. */
  ww(0x208,0xff);ww(0x20a,0xa30);ww(0x20c,0);
  for(unsigned n=0;n<8;n++){
   wb(0x100+n*32,(sample&(1u<<n))?4:0);
   if(!(sample&(1u<<n))){uint16_t at=rw(0x20a)-4;ww(at,0xff);ww(at+2,0x100+n*32);ww(0x20a,at);ww(0x20c,rw(0x20c)+1);}
  }
  wb(0xc0,sample&1);wb(0xe0,sample&2);wb(0x40,sample&4);wb(0x60,sample&8);
  wb(0x82a4,mode&1);wb(0x82a2,(mode&2)?3:0);
  wb(0x8058,sample);wb(0x8059,(mode&4)?sample:0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d1=sample*311,d2=sample*977;
  writes=0;WillowTransitionStep step=willowArcadeAttractInput(&bus,d1,d2,romWord,colorWrite);
  if(!step.supported)return 1;
  ram[0xf000^1]=0;ram[0xf001^1]=0x10;ram[0xf002^1]=0;ram[0xf003^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xfff000);
  m68k_set_reg(1,d1);m68k_set_reg(2,d2);m68k_set_reg(16,0x811e);
  unsigned stop=step.finished?0x1636:0x100000;
  for(unsigned slice=0;slice<8&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Input stop %x\n",m68k_get_reg(16));return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xef80&&n<0xf004)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Attract input mismatch mode %u sample %u address %x actual %x expected %x\n",mode,sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;finished+=step.finished;
 }
 printf("Attract input: %u original instruction comparisons, %u handoffs; edge selection, credits, task cleanup, free-list order and palettes match\n",checks,finished);
 return 0;
}
