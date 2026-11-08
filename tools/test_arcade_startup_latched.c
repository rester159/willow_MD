/* Differential verification of latched startup chain against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_startup_latched.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "demo_state_lifetime_audit.h"
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
static uint16_t romWord(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
int main(int argc,char**argv){
 if(argc!=2 && (argc!=3||strcmp(argv[2],"--native-fixtures")))return 2;
 FILE*fixtures=argc==3?fopen("build/startup-latched-native-fixtures.bin","wb"):NULL;
 if(argc==3&&!fixtures)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Stop only at interrupt enabling; all intervening instructions, services,
  * ROM tables and trap-0 task starts execute unchanged. */
 willowMotionPutWord(rom+0x14c8,0x4ef9);willowMotionPutLong(rom+0x14ca,0x100000);
 willowMotionPutLong(rom+0x80,0x15d0);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/startup-latched-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/startup-latched-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 const uint8_t states[]={0,1,2,4,8,12,0x80,0xff};
 for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  expected[0]=states[sample&7];expected[0xa0]=states[(sample>>3)&7];
  wb(0x8066,sample);wb(0x8067,sample*73);wb(0x8068,sample*151);
  ww(0x801e,(sample&63)*2);
  uint8_t demoBefore[321];demoLifetimeCapture(demoBefore,expected);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(fixtures&&sample<256&&fwrite(expected,1,sizeof expected,fixtures)!=sizeof expected)return 2;
  uint32_t d2=0x98760000UL|(uint16_t)(sample*317);
  uint16_t d1=willowArcadeStartupLatched(&bus,romWord,0xffff8000UL,d2);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xffff8000UL);m68k_set_reg(15,0xffff00);
  m68k_set_reg(0,0x12345678);m68k_set_reg(1,0xabcdffffUL);m68k_set_reg(2,d2);m68k_set_reg(16,0x143e);
  retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(15)!=0xffff00||
     m68k_get_reg(1)!=(0xabcd0000UL|d1)||m68k_get_reg(2)!=d2){
   fprintf(stderr,"Latched startup registers sample %u PC %x SP %x D1 %x D2 %x\n",
    sample,m68k_get_reg(16),m68k_get_reg(15),m68k_get_reg(1),m68k_get_reg(2));return 1;
  }
  for(unsigned n=0;n<65536;n++)if((n<0xfec0||n>=0xff00)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Latched startup sample %u RAM %04x got %02x want %02x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if(fixtures&&sample<256&&fwrite(expected,1,sizeof expected,fixtures)!=sizeof expected)return 2;
  if(!demoLifetimePreserved(demoBefore,ram))return 1;
   checks++;
 }
 if(fixtures&&fclose(fixtures))return 2;
 printf("Latched startup: %u original chain comparisons; option decoding, four sound words, stack/pool/free-list initialization, conditional fixed starts, inherited D1/D2 and full non-stack RAM match\n",checks);
 puts("Supplemental demo lifetime: all 321 source bytes preserved at every tested CPU boundary; external service bodies excluded where the fixture suspends");
 return 0;
}
