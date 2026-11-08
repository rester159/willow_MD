/* Headless differential verification of class-18 entry and shared palette allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor18_four_ranged.h"
#include "../src/willow_arcade_pool_init.h"
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



static uint16_t sourceWord(unsigned long at){return (at&0xff0000)==0xff0000?rw(at):willowMotionWord(rom+at);}
static uint8_t*resolve(uint16_t at){return expected+at;}

int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 if(willowMotionLong(rom+0x6772)!=0x910010)return 2;
 willowMotionPutLong(rom+0x6772,0xff4010);willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x4ad0,0xff4310);willowMotionPutLong(rom+0x4ad8,0xff4310);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor18-four-ranged-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor18-four-ranged-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 const uint32_t entries[]={0x2a260,0x29e08};
 for(unsigned mode=0;mode<2;mode++)for(unsigned sample=0;sample<32768;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*13+sample*31)^0xa5;
  willowArcadePoolsInit(&bus);
  uint16_t actor=0xf490+(sample%8)*128;uint8_t*r=resolve(actor);
  r[0]=1;r[2]=0;r[3]=(sample%3)*2;r[30]=18;r[14]=(sample&1)*2;r[34]=(sample/5)%3;
  willowMotionPutWord(r+6,0x1700+sample%512);willowMotionPutWord(r+10,sample%300);
  ww(0x8408,0x1700);ww(0x840c,0);ww(0x8284,sample*977);wb(0x8286,sample);wb(0x8019,sample/2);
  wb(0x8470,(sample/15)%2);wb(0x838b,(sample/30)%2);wb(0x82a6,(sample/60)%2);
  wb(0xc0,(sample/120)%2);wb(0x8290,(sample/240)%2);wb(0x8293,(sample/480)%2);ww(0x801e,(sample*2)&126);
  for(unsigned n=0;n<16;n++)wb(0xf042+n*64,n<sample%17?1:0);
  r[15]=(sample/3)&1;wb(0x8346,sample);ww(0x801a,(sample*4)&252);
  willowMotionPutWord(r+56,sample%4?sample*311:0);
  r[50]=(sample/6)&1;r[48]=(sample/12)&1;r[51]=(sample/24)%3;
  r[34]=(sample/72)%3;willowMotionPutWord(r+56,(sample/216)%3);
  uint16_t x=sample*977;willowMotionPutLong(r+6,((uint32_t)x<<16)|sample);
  ww(0x984c,x+(int16_t)(sample%3)-1);
  willowMotionPutWord(r+100,x+(int16_t)((sample/648)%5)-2);
  willowMotionPutWord(r+102,x+(int16_t)((sample/3240)%5)-2);
  willowMotionPutLong(r+24,0xed068);willowActor18FourPose(r,0x2a6da,r[48],sourceWord);
  willowMotionPutWord(r+28,(sample/9720)%3);
  r[15]=sample%128;willowMotionPutLong(r+24,0xef5e4);
  if(sample%4){willowMotionPutWord(r+6,0x560+(int16_t)((sample/4)%9)-4);willowMotionPutWord(r+10,0xcd8+(int16_t)((sample/36)%9)-4);}
  if(sample%8==0)willowMotionPutWord(r+6,0x560);
  if(sample%8==1)willowMotionPutWord(r+10,0xcd8);
  wb(0x980a,(sample/324)%3);
  r[3]=10;r[4]=(sample%3)*2;r[48]=(sample/3)%9;r[67]=(sample/27)%5;
  const uint8_t timers[]={0,1,2,14,30,60,255};r[34]=timers[(sample/135)%7];r[58]=timers[(sample/945)%7];
  wb(0x980d,(sample/6615)%3);wb(0x980a,(sample/2205)%3);
  willowActor18FourPose(r,0x2a966,r[48],sourceWord);willowMotionPutWord(r+28,(sample/11025)%3);
  r[3]=12;r[4]=(sample%5)*2;r[105]=timers[(sample/4725)%7];
  willowMotionPutWord(r+96,0x560);willowMotionPutWord(r+98,0xc50);
  r[3]=8;r[4]=(sample%3)*2;
  ww(0x984c,sample*177);ww(0x9850,sample*733);
  if(sample%4==0)ww(0x984c,willowMotionWord(r+6));
  if(sample%4==1)ww(0x9850,willowMotionWord(r+10));
  if(sample%4==2){ww(0x984c,willowMotionWord(r+6)+32);ww(0x9850,willowMotionWord(r+10)+32);}
  uint16_t d1=sample*311,d4=sample*977;
  uint32_t initialA4=sample&1?0xffb000:0x123456,a4=initialA4;
  willowArcadeVisibilityInit(&bus,0,0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(mode==0)willowArcadeActor18FourPlayerAim(&bus,r,sourceWord,&d1,&d4);
  else if(!willowArcadeActor18FourRangedWithA4(&bus,r,actor,sourceWord,&d1,&d4,&a4))return 1;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(4,(uint16_t)(sample*977));m68k_set_reg(1,(uint16_t)(sample*311));m68k_set_reg(0,0);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entries[mode]);
  for(unsigned slice=0;slice<8&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04)return 1;
  if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Target D4 mismatch\n");return 1;}
  if((uint16_t)m68k_get_reg(1)!=d1){fprintf(stderr,"Patrol D1 mismatch mode %u sample %u actual %x expected %x\n",mode,sample,m68k_get_reg(1)&65535,d1);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Actor18 four enraged mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if(m68k_get_reg(12)!=a4){fprintf(stderr,"A4 mismatch sample %u actual %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  checks++;
 }
 printf("Actor18 selector-four ranged attack: %u original non-stack RAM/D1/D4 comparisons; all three substates, player aiming, allocation, parent pointer widths, sound and pose tail match\n",checks);
 return 0;
}
