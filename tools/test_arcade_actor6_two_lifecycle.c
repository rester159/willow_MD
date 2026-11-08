/* Headless differential verification of class-6 platform retained scans. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor6_pool.h"
#include "../src/willow_arcade_actor12_pool.h"
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
static uint8_t attribute(uint16_t offset){return rb(0x4002+offset);}
static uint8_t*resolve(uint16_t at){return expected+at;}


int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100100,0x4eb9);willowMotionPutLong(rom+0x100102,0x2f34c);
 willowMotionPutWord(rom+0x100106,0x4eb9);willowMotionPutLong(rom+0x100108,0x2f39e);
 willowMotionPutWord(rom+0x10010c,0x4e75);
 if(willowMotionLong(rom+0x6772)!=0x910010)return 2;
 willowMotionPutLong(rom+0x6772,0xff4010);willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x4ad0,0xff4310);willowMotionPutLong(rom+0x4ad8,0xff4310);
 willowMotionPutLong(rom+0x3e44,0xff4000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor6-two-lifecycle-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor6-two-lifecycle-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0,completed=0,travel=0,ending=0,bobbing=0;
 unsigned floorIndex=0;for(unsigned n=4;n<256;n+=4)if((sourceWord(0x4712UL+n*2)&0x8df)==0x88f){floorIndex=n;break;}if(!floorIndex)return 2;
 for(unsigned sample=0;sample<16;sample++){
  memset(expected,0,sizeof expected);willowArcadePoolsInit(&bus);
  ww(0x8408,0);ww(0x840c,0);ww(0x8398,1536);ww(0x846a,6000);
  ww(0x8284,sample*311);ww(0x8286,sample*977);
  uint16_t actor=willowActor18Pop(&bus,0x9804,0x9822);uint8_t*r=resolve(actor);
  r[0]=1;r[32]=2;willowMotionPutWord(r+6,176+sample*2);willowMotionPutWord(r+10,160);
  willowMotionPutWord(r+8,sample*311);willowMotionPutWord(r+12,sample*977);
  for(unsigned n=0;n<4096;n++)wb(0x4002+n*4,floorIndex);
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
  uint16_t d4=0x1234;
  unsigned step;
  for(step=0;step<3800;step++){
   wb(0x8019,step+sample*13);
   if(step==8)r[36]=1;
   if(step==2800)ww(0x846a,3584);
   if(r[2]==2&&r[3]==4){travel|=1u<<(r[4]/2);bobbing|=1u<<(r[5]/2);}
   if(r[2]==2&&r[3]==6)ending|=1u<<(r[4]/2);
   if(step==3700){
    if(r[0])r[2]=4;
    for(unsigned n=0;n<60;n++){uint8_t*child=resolve(0xccc6+n*64);if(child[0])child[2]=6;}
   }
   willowArcadeVisibilityInit(&bus,0,0);
   for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
   uint16_t oldD4=d4;
   if(!willowArcadeActor6PoolWithCollision(&bus,resolve,sourceWord,&collision))return 1;
   if(!willowArcadeActor12PoolWithServices(&bus,resolve,sourceWord,0,0,&d4))return 1;
   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(4,oldD4);
   m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100100);
   for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04)return 1;
   if((uint16_t)m68k_get_reg(4)!=d4)return 1;
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Class-6 scripted platform sample %u frame %u at %x actual %x expected %x\n",sample,step,n,ram[n^1],expected[n]);return 1;
   }
   checks++;if(rb(0x9804)==48&&rb(0x980d)==60)break;
  }
  if(step==3800)return 1;completed++;
 }
 if(travel!=1023||ending!=127||bobbing!=7){fprintf(stderr,"coverage travel=%x ending=%x bobbing=%x\n",travel,ending,bobbing);return 1;}
 printf("Class-6 scripted platforms retained scan: %u original non-stack RAM/D4 comparisons across %u mixed populations; all ten travel phases, three bobbing phases, seven ending phases and child effect integration; contact/terrain/end trigger and final scene retirement are external inputs\n",checks,completed);
 return 0;
}
