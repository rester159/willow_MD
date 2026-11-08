/* Headless original-code comparison of the 725E support scan. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor10_three.h"
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
 f=fopen("build/actor-support-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor-support-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0,hitActions=0;
 for(unsigned sample=0;sample<65536;sample++){
  memset(expected,0,sizeof expected);willowArcadePoolsInit(&bus);
  uint16_t actor=willowActor18Pop(&bus,0x980a,0x982e);uint8_t*r=resolve(actor);
  r[0]=1;r[32]=3;r[33]=(sample&1)*4;r[2]=((sample/2)%4)*2;r[3]=((sample/8)%2)*2;
  r[14]=(sample/16)%2*2;r[15]=(sample/32)%2;
  willowMotionPutLong(r+6,((uint32_t)(sample*17)<<16)|sample);willowMotionPutLong(r+10,((uint32_t)(sample*31)<<16)|sample);
  r[34]=(sample/3)%40;r[46]=sample;r[47]=sample>>8;
  willowPlayerAnimationSet(r,0x240fe,sourceWord);
  for(unsigned n=0;n<sample%40;n++)willowPlayerAnimationStep(r,sourceWord);
  ww(0x8408,willowMotionWord(r+6)-(sample%512));ww(0x840c,willowMotionWord(r+10)-(sample%384));wb(0x8019,sample/3);
  ww(0x83d2,(sample/13)%3?0:256);ww(0x83e4,sample*977);
  wb(0x8290,(sample/19)&1);wb(0x8293,(sample/23)&1);ww(0x801e,(sample%64)*2);
  willowArcadeVisibilityInit(&bus,0,0);
  r[19]=(sample/23)%6;r[52]=(sample/11)%5;willowMotionPutWord(r+54,sample*977);
  ww(0x984c,sample*13);ww(0x9850,sample*7);r[36]=sample%2;

  wb(0x9806,4);ww(0x9826,0x9000);
  for(unsigned n=0;n<4;n++){
   uint16_t at=0xb6c6+n*64;ww(0x9000+n*2,at);wb(at,1+(sample/29+n)%3);
   wb(at+32,(sample/31+n)%11);wb(at+33,(sample/37)%4);
   ww(at+6,willowMotionWord(r+6)+(sample%97)-48);
   ww(at+10,willowMotionWord(r+10)-sourceWord(0x72acUL+r[19]*8)+(sample%49)-24);
  }
  if(sample&1){
   static const uint8_t types[]={0,1,3,6};static const int16_t bias[]={4,-8,16,-16};
   unsigned k=(sample/2)%4;wb(0xb6c6,1);wb(0xb6c6+32,types[k]);wb(0xb6c6+33,0);
   ww(0xb6c6+6,willowMotionWord(r+6));
   ww(0xb6c6+10,willowMotionWord(r+10)-sourceWord(0x72acUL+r[19]*8)+bias[k]);
   r[36]=0;
  }
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t entry=0x725e,a4=0x123456;
  if(!willowArcadeActorSupport(&bus,r,sourceWord))return 1;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(7,sample%20);m68k_set_reg(0,0);m68k_set_reg(4,0x1234);m68k_set_reg(15,0xffff00);m68k_set_reg(12,0x123456);m68k_set_reg(16,entry);
  for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04)return 1;
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Class-10 actor sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if((uint16_t)m68k_get_reg(4)!=0x1234||m68k_get_reg(12)!=a4){fprintf(stderr,"Register sample %u A4 %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  if((sample&1)&&r[36])hitActions|=1<<((sample/2)%4);
  checks++;
 }
 if(hitActions!=15){fprintf(stderr,"Support actions covered %x\n",hitActions);return 1;}
 printf("Actor support 725E: %u original non-stack RAM/D4/A4 comparisons across all four responses, six geometries, eligible activity, variant filtering, word wrap and first-hit contact\n",checks);
 return 0;
}
