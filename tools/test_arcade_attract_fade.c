/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_fade.h"
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

static void putRamWord(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
static uint8_t expected[65536];
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}

static uint16_t source(uint32_t at){return willowMotionWord(rom+at);}
static uint32_t videoBase;
static unsigned videoSize,videoWrites;
static void videoWord(uint32_t at,uint16_t value){
 videoWrites++;
 if(at>=videoBase&&at<videoBase+videoSize)ww((uint16_t)(0x2000+at-videoBase),value);
}
int main(int argc,char**argv){
 if(argc!=3)return 2;
 unsigned region=(unsigned)strtoul(argv[2],0,10);if(region>4)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);

 const unsigned operands[]={0x65a6,0x65ba,0x65ce,0x65ec,0x65f6};
 const uint32_t bases[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 videoBase=bases[region];videoSize=region<3?0x4000:0x800;
 if(willowMotionLong(rom+operands[region])!=videoBase)return 2;
 /* Remap one complete CPS region per run; other CPS writes remain outside
  * Genesis RAM. All five runs together check every cleared destination. */
 willowMotionPutLong(rom+operands[region],0xff2000);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/attract-fade-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-fade-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at)); \
 for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 for(unsigned sample=0;sample<256;sample++)for(unsigned phase=0;phase<3;phase++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  willowArcadeTaskInitFree(&bus,source);
  if(sample&1)ww(0x20c,0);
  ww(0x8000,sample==255?65535:sample*257);ww(0x8004,phase?2:0);
  ww(0x82c0,phase==1?0:0xffff);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  videoWrites=0;
  if(!willowArcadeAttractFade(&bus,source,videoWord))return 1;
  if(videoWrites!=(phase==2?26624u:0u))return 1;
  RUN(0x822e,0xdeadbeef);
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Attract fade mismatch region %u phase %u sample %u address %x actual %x expected %x\n",region,phase,sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 printf("Attract fade region %u: %u source comparisons; immediate allocation/exhaustion, wait, manager wrap and scene clear match\n",region,checks);
 return 0;
}
