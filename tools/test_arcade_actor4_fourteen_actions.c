/* Headless original-code comparison of selector fourteen attack/run/terrain states. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor4_fourteen_actions.h"
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
static uint8_t attribute(uint16_t at){return rb(0x4000+at+2);}
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
 f=fopen("build/actor4-fourteen-actions-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor4-fourteen-actions-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0;
 for(unsigned sample=0;sample<65536;sample++){
  memset(expected,0,sizeof expected);willowArcadePoolsInit(&bus);
  uint16_t actor=willowActor18Pop(&bus,0x9801,0x981c);uint8_t*r=resolve(actor);
  unsigned mix=sample;
  r[0]=1;r[32]=7;r[33]=mix%3*2;r[2]=2;r[3]=2;r[4]=sample%2*2;r[15]=(mix/2)%2;
  r[19]=3;r[14]=2;r[36]=mix%3==0;r[34]=(mix/3)%4;r[48]=mix;r[74]=mix;r[68]=mix;
  uint16_t x=mix%13?400:0xfff0,y=200;
  willowMotionPutLong(r+6,((uint32_t)x<<16)|sample);willowMotionPutLong(r+10,((uint32_t)y<<16)|sample);
  willowMotionPutLong(r+24,0x136b0);willowPlayerAnimationSelect(r,0x13154,sourceWord);
  ww(0x984c,x+(int)(mix%13)*32-192);ww(0x9850,y+(int)((mix/13)%9)*32-128);
  ww(0x8284,sample*997);wb(0x8019,mix);
  ww(0x8408,x-(mix/3)%5*128);ww(0x840c,y-(mix/7)%5*96);
  for(unsigned n=0;n<4096;n++)wb(0x4002+n*4,(mix%4)*4);
  r[42]=mix%3;r[43]=(mix/3)%3;r[1]=mix%2;r[80]=(mix/2)%2;r[68]=(mix/5)%2;
  willowMotionPutLong(r+38,0xff9dc6);ww(0x9dcc,mix*997);ww(0x9dd0,mix*331);
  wb(0x83d0,mix%2);wb(0x8290,(mix/2)%2);wb(0x8293,(mix/4)%2);wb(0x801e,mix%64*2);if(mix%7==0)wb(0x980d,0);

  wb(0x838b,(mix/2)%2);wb(0x8470,(mix/3)%2);wb(0x83ce,mix);wb(0x83e6,mix);wb(0xf47,mix);if(mix%11==0)wb(0xf48c,0);
  r[15]=(mix/4)%4;wb(0x988e,mix%2);r[68]=(mix/7)%4;
  willowMotionPutWord(r+66,mix%16);willowMotionPutWord(r+48,rw(0x984c)+(int)(mix%5)*2-4);willowMotionPutWord(r+50,rw(0x9850)+(int)((mix/5)%5)*2-4);
  willowMotionPutWord(r+54,(mix/3)%4);willowMotionPutWord(r+56,(mix/11)%4);willowMotionPutWord(r+58,mix%2?2:0xfffe);willowMotionPutWord(r+60,mix%2?0xfffe:2);

  r[73]=(mix/3)%4;ww(0x83d2,mix%17==0);
  unsigned used=(mix/7)%17;for(unsigned n=0;n<16;n++){uint16_t at=0xf042+n*64;for(unsigned j=0;j<64;j++)wb(at+j,(uint8_t)(mix+j));wb(at,n<used?1:0);}
  r[4]=sample%3==0?(mix%2)*2:(mix%4)*2;r[62]=(mix/2)%5;r[70]=(mix/7)%10;r[52]=(mix/11)%4;
  r[72]=(mix/3)%4*2;r[64]=(mix/7)%7*2;r[65]=(mix/5)%4*2;
  r[34]=(mix/4)%4;wb(0x82a6,(mix/13)%2);wb(0xc0,(mix/17)%2);
  willowPlayerAnimationSet(r,0x1528a,sourceWord);
  r[32]=14;r[33]=sample%9*2;r[2]=0;r[3]=(sample/9)%(r[33]?7:3)*2;r[15]=(sample/63)%2;
  r[34]=(sample/126)%4;willowPlayerAnimationSet(r,0x18b78,sourceWord);
  ww(0x984c,(sample/504)%3==0?2048:willowMotionWord(r+6)+(sample/1512)%7*64-192);
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
  r[4]=(sample/9)%(sample%3==0?3:2)*2;r[34]=(sample/54)%5;r[19]=(sample/270)%12;
  willowMotionPutLong(r+24,0x18bd0);willowPlayerAnimationSelect(r,0x18588,sourceWord);
  /* Spatially varying source tiles exercise mixed probe outcomes. */
  for(unsigned n=0;n<4096;n++)wb(0x4002+n*4,(uint8_t)((n*17+sample/7)%64*4));
  willowArcadeVisibilityInit(&bus,0,0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  const uint32_t entries[3]={0x17a26,0x17a86,0x18368};
  uint32_t entry=entries[sample%3],a4=0x123456;uint16_t d4=0x1234;uint8_t ok=1;
  if(sample%3==0)ok=willowActor4FourteenAttack(&bus,r,sourceWord,&a4);
  else if(sample%3==1)ok=willowActor4FourteenRun(&bus,r,sample%50,sourceWord,&collision);
  else ok=willowActor4FourteenTerrain(&bus,r,&collision);
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
 printf("Class-4 selector fourteen actions: %u original non-stack RAM/D4/A4 comparisons across attack phases, visibility-gated sound, randomized running, double animation steps and ordered terrain decisions\n",checks);
 return 0;
}
