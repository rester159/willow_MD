/* Headless differential verification of class-18 entry and shared palette allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_direction.h"
#include "../src/willow_arcade_visibility.h"
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



static uint16_t sourceWord(unsigned long at){return (at&0xff0000)==0xff0000?rw(at):willowMotionWord(rom+at);}
static uint8_t*resolve(uint16_t at){return expected+at;}

int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor-direction-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor-direction-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 unsigned checks=0;uint32_t rng=0x57494c4c;
 for(unsigned mode=0;mode<2;mode++)for(unsigned sample=0;sample<65536;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*13+sample*31)^0xa5;
  uint16_t actor=0xf490;uint8_t*r=resolve(actor);
  rng=rng*1664525u+1013904223u;uint16_t x=rng>>16;
  rng=rng*1664525u+1013904223u;uint16_t y=rng>>16;
  rng=rng*1664525u+1013904223u;willowMotionPutLong(r+6,rng);
  rng=rng*1664525u+1013904223u;willowMotionPutLong(r+10,rng);
  if(sample%8==0)x=willowMotionWord(r+6);
  if(sample%8==1)y=willowMotionWord(r+10);
  if(sample%8==2){willowMotionPutWord(r+6,0x8000);willowMotionPutWord(r+10,0x8000);x=0x8000+sample%32768;y=0x8000-sample%32768;}
  if(sample%8==3){willowMotionPutWord(r+6,65535);willowMotionPutWord(r+10,0);x=0;y=65535;}
  willowMotionPutLong(r+24,0xeefe4+(sample%4)*512);r[15]=sample%128;
  uint16_t d4=sample*311;uint8_t direction=0;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(mode)direction=willowArcadeActorDirection(r,x,y,&d4,sourceWord);
  else willowArcadeActorDirectionMove(r,sourceWord);
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(0,x);m68k_set_reg(1,y);m68k_set_reg(4,(uint16_t)(sample*311));
  m68k_set_reg(15,0xffff00);m68k_set_reg(16,mode?0x20f4:0x208c);
  for(unsigned slice=0;slice<8&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000)return 1;
  if(mode&&((uint8_t)m68k_get_reg(6)!=direction||(uint16_t)m68k_get_reg(4)!=d4)){
   fprintf(stderr,"Direction mismatch sample %u actual %x/%x expected %x/%x\n",sample,m68k_get_reg(6),m68k_get_reg(4),direction,d4);return 1;
  }
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Direction RAM mismatch mode %u sample %u at %x\n",mode,sample,n);return 1;
  }
  checks++;
 }
 printf("Actor direction: %u original comparisons; exact integer octants, axes/diagonals, D4 preservation, table velocities and fractional position wrap match\n",checks);
 return 0;
}
