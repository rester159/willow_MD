/* Headless differential verification of boss HUD, health response and complete active handler. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor18_two_active.h"
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

static void textWrite(uint32_t at,uint16_t v){if(at<0x910000||at>=0x914000)abort();ww(0x4000+at-0x910000,v);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 if(willowMotionLong(rom+0x4ad0)!=0x910310||willowMotionLong(rom+0x4ad8)!=0x910310)return 2;
 willowMotionPutLong(rom+0x4ad0,0xff4310);willowMotionPutLong(rom+0x4ad8,0xff4310);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor18-two-active-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor18-two-active-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;uint32_t random=0x57494c4c;
 const uint32_t entries[]={0x4ac6,0x27e96,0x27a7e};
 for(unsigned mode=0;mode<3;mode++)for(unsigned sample=0;sample<32768;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*13+sample*31)^0xa5;
  willowArcadePoolsInit(&bus);
  uint16_t actor=0xf490+(sample%8)*128;uint8_t*r=resolve(actor);
  r[0]=1;r[2]=2;r[3]=mode==3?4:2;r[4]=(sample%3)*2;r[5]=(sample%4)*2;r[48]=(sample/4)%8;
  const uint8_t timers[]={0,1,2,20,30,120,255};r[34]=timers[(sample/12)%7];
  willowMotionPutWord(r+58,0x0202);
  willowActor18TwoAnimation(r,0x281ee,sourceWord);willowMotionPutWord(r+28,(sample/84)%3);
  random=random*1664525u+1013904223u;willowMotionPutLong(r+6,random);
  random=random*1664525u+1013904223u;willowMotionPutLong(r+10,random);
  random=random*1664525u+1013904223u;ww(0x984c,random>>16);
  random=random*1664525u+1013904223u;ww(0x9850,random>>16);
  if(sample%8==0)ww(0x984c,willowMotionWord(r+6));
  if(sample%8==1)ww(0x9850,willowMotionWord(r+10));
  ww(0x8284,sample*977);wb(0x8286,sample);
  wb(0x8290,(sample/7)&1);wb(0x8293,(sample/13)&1);ww(0x801e,(sample*2)&126);
  wb(0x980d,(sample/3)%2);wb(0x9801,(sample/5)%5);wb(0x980a,(sample/11)%2);
  r[4]=(sample%6)*2;r[5]=r[4]==2?((sample/6)%2)*2:((sample/12)%4)*2;
  r[56]=(sample/48)%3;r[57]=(sample/144)%4;r[55]=(sample/576)%3;r[15]=sample%128;
  willowMotionPutLong(r+24,0xeefe4+r[57]*512);willowMotionPutLong(r+50,0x279aa+(sample%17)*6);
  wb(0x8019,sample/2);wb(0x8346,sample);
  r[3]=((sample/6)%3)*2;
  if(r[3]==4)r[4]=(sample%3)*2;
  r[30]=18;r[14]=(sample&1)*2;r[54]=((sample/18)%4)*2;
  willowMotionPutWord(r+44,(sample/72)%129);
  willowMotionPutWord(r+80,64);willowMotionPutWord(r+82,64);
  willowMotionPutWord(r+84,2);willowMotionPutWord(r+86,2);
  r[42]=sample&255;r[43]=sample%2?r[42]:(uint8_t)(r[42]+1);r[49]=(sample/256)%42;
  wb(0x83cc,sample&255);ww(0x801a,(sample*4)&252);
  wb(0x9804,(sample/17)%2);
  for(unsigned n=0;n<16;n++)wb(0xf042+n*64,n<sample%17?1:0);
  ww(0x8408,willowMotionWord(r+6)-(sample%512));ww(0x840c,willowMotionWord(r+10)-(sample%300));
  willowArcadeVisibilityInit(&bus,0,0);
  uint16_t d4=sample*311;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t initialA4=sample&1?0xffb000:0x123456,a4=initialA4;
  if(mode==0)willowArcadeBossHealthDisplayWithA4(&bus,textWrite,&a4);
  else if(mode==1)willowArcadeActor18TwoHealthWithA4(&bus,r,textWrite,&a4);
  else if(!willowArcadeActor18TwoActiveWithA4(&bus,resolve,actor,sourceWord,textWrite,&d4,&a4))return 1;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(4,(uint16_t)(sample*311));m68k_set_reg(0,0);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entries[mode]);
  for(unsigned slice=0;slice<8&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000)return 1;
  if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Active D4 mismatch mode %u sample %u\n",mode,sample);return 1;}
  if(m68k_get_reg(12)!=a4){fprintf(stderr,"A4 sample %u actual %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Actor18 active mismatch mode %u sample %u at %x actual %x expected %x\n",mode,sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 printf("Actor18 selector-two HUD/health/active: %u original full non-stack RAM/text/D4 comparisons; HUD fill, hit countdown, death transition, pool exhaustion and all active substates match\n",checks);
 return 0;
}
