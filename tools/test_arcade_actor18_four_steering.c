/* Headless differential verification of class-18 entry and shared palette allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor18_four_steering.h"
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
 f=fopen("build/actor18-four-steering-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor18-four-steering-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 unsigned checks=0;
 for(unsigned mode=0;mode<3;mode++)for(unsigned sample=0;sample<32768;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*13+sample*31)^0xa5;
  uint16_t actor=0xf490;uint8_t*r=resolve(actor);
  uint32_t route=(sample&1?0x2a38a:0x2a3c0)+((sample/2)%12)*4;
  willowMotionPutLong(r+60,route);r[15]=sample%32;r[48]=(sample/32)%9;
  const uint8_t timers[]={0,1,2,14,255};r[64]=timers[(sample/288)%5];r[58]=timers[(sample/1440)%5];
  uint16_t x=sample*977,y=sample*311;
  willowMotionPutLong(r+6,((uint32_t)x<<16)|sample);willowMotionPutLong(r+10,((uint32_t)y<<16)|sample);
  willowMotionPutWord(r+96,sourceWord(route));willowMotionPutWord(r+98,sourceWord(route+2));
  if(sample%8==0)willowMotionPutWord(r+6,sourceWord(route));
  if(sample%8==1)willowMotionPutWord(r+10,sourceWord(route+2));
  if(sample%8==2){willowMotionPutWord(r+6,sourceWord(route)+16);willowMotionPutWord(r+10,sourceWord(route+2)+16);}
  uint16_t d4=sample*311;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint8_t pose=0;
  if(mode==0)willowArcadeActor18FourSteer(r,sourceWord,&d4);
  else if(mode==1)pose=willowArcadeActor18FourPoseDirection(r,sourceWord,&d4);
  else willowArcadeActor18FourPoseStep(r,sourceWord,&d4);
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(4,(uint16_t)(sample*311));m68k_set_reg(0,0);m68k_set_reg(15,0xffff00);m68k_set_reg(16,mode==0?0x2a3f6:mode==1?0x2a362:0x2a328);
  for(unsigned slice=0;slice<8&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000)return 1;
  if(mode==1&&(uint8_t)m68k_get_reg(6)!=pose)return 1;
  if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Motion D4 mismatch %u/%u\n",mode,sample);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Actor18 four steering mismatch mode %u sample %u at %x actual %x expected %x\n",mode,sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 printf("Actor18 selector-four steering: %u original non-stack RAM/D4 comparisons; waypoint turning, direction quantization, pose clamp, timer wrap and animation selection match\n",checks);
 return 0;
}
