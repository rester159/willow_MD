/* Headless original-code comparison of selector fifteen steering/exit and full ordinary active. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor4_fifteen_ordinary.h"
#include "../src/willow_arcade_pool_init.h"
#ifdef WILLOW_TEST_ACTOR18_POOL
#include "../src/willow_arcade_actor18_pool.h"
#endif
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
 willowMotionPutLong(rom+0x3e44,0xff4000);
 willowMotionPutLong(rom+0x4365a,0xff4000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor4-fifteen-ordinary-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor4-fifteen-ordinary-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0;
 for(unsigned sample=0;sample<65536;sample++){
  memset(expected,0,sizeof expected);willowArcadePoolsInit(&bus);
  uint16_t actor=willowActor18Pop(&bus,0x9801,0x981c);uint8_t*r=resolve(actor);
  unsigned mix=sample,kind=mix%3;
  r[0]=1;r[32]=15;r[33]=(mix/3)%4;r[2]=0;r[3]=(mix/12)%2*2;r[15]=(mix/24)%2;
  r[63]=(mix/48)%2;r[34]=(mix/96)%4;r[62]=mix;r[56]=mix;
  uint16_t x=(mix/384)%3==0?0xfff0:400,y=200;
  willowMotionPutLong(r+6,((uint32_t)x<<16)|sample);willowMotionPutLong(r+10,((uint32_t)y<<16)|sample);
  static const int offsets[]={-1,0,1,63,64,65,191,192,193};
  ww(0x984c,x-offsets[(mix/1152)%9]);ww(0x8408,x-offsets[(mix/5)%9]);ww(0x840c,mix*97);
  wb(0x8019,mix/7);ww(0x8284,sample*997+1);willowMotionPutLong(r+24,0xed364);
  willowPlayerAnimationSet(r,0x1994a,sourceWord);
  r[15]=(mix/12)%16;r[55]=(mix/192)%4;r[56]=(mix/768)%4;r[62]=(mix/3072)%8*4;
  r[60]=(mix/24576)%2;r[34]=(mix/96)%4;r[2]=2;r[3]=4;r[4]=mix%8*2;
  r[42]=(mix/24)%3;r[43]=(mix/72)%3;wb(0x83d0,(mix/216)%2);if(mix%7==0)wb(0x980d,0);
  willowMotionPutLong(r+64,0x18d62+(mix/6)%10*4);
  static const uint16_t speeds[]={0,1,0xffff,0x7fff,0x8000,0x7ff8,0x8008,0x1234};
  willowMotionPutWord(r+48,speeds[(mix/5)%8]);willowMotionPutWord(r+50,speeds[(mix/7)%8]);willowMotionPutWord(r+52,speeds[(mix/11)%8]);
  if(kind==3)willowMotionPutWord(r+60,speeds[(mix/13)%8]);
  r[3]=kind*2;r[60]=(mix/6)%2;r[61]=(mix/12)%(kind==5?7:9)*4;
  r[56]=(mix/108)%4;r[62]=(mix/432)%8*4;willowMotionPutWord(r+34,(mix/3456)%4);
  if(kind==0||kind==4)r[34]=(mix/24)%4;
  ww(0x984c,x-26+(mix/96)%24);ww(0x8408,x-320+(int)((mix/2304)%5)-2);
  if(mix%7==0)wb(0x980a,0);
  r[3]=(mix/3)%7*2;r[4]=(mix/21)%3*2;r[61]=(mix/63)%(r[3]==10?7:9)*4;
  r[15]=(mix/567)%16;ww(0x9850,y+(int)((mix/9072)%7)*32-96);
  ww(0x984c,x+(int)((mix/7)%9)*16-64);ww(0x8408,x+(int)((mix/11)%5)*128-256);ww(0x840c,y+(int)((mix/13)%5)*128-256);
  wb(0x83d0,(mix/17)%2);ww(0x83d2,(mix/31)%2);
  willowArcadeVisibilityInit(&bus,0,0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t entry=kind==0?0x191f6:kind==1?0x19006:0x18e38,a4=0x123456;uint16_t d4=0x1234;uint8_t ok=1;
  if(kind==0)willowActor4FifteenSteer(&bus,r,sourceWord,&d4);
  else if(kind==1)ok=willowActor4FifteenExit(&bus,r,sample%50,sourceWord,&d4);
  else ok=willowActor4FifteenOrdinary(&bus,r,actor,resolve,sample%50,sourceWord,&d4,&a4);
  if(!ok)return 1;

  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(7,sample%50);m68k_set_reg(0,0);m68k_set_reg(4,0x1234);m68k_set_reg(15,0xffff00);m68k_set_reg(12,0x123456);m68k_set_reg(16,entry);
  for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04){fprintf(stderr,"Oracle sample %u PC %x SP %x\n",sample,m68k_get_reg(16),m68k_get_reg(15));return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Class-4 actor sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if((uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=a4){fprintf(stderr,"Register sample %u A4 %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  checks++;
 }
 printf("Class-4 selector fifteen ordinary: %u original non-stack RAM/D4/A4 comparisons across steering, exit substates, all ordinary actions, pause, damage and visibility\n",checks);
 return 0;
}
