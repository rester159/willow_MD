/* Headless differential verification of class-18 entry and shared palette allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor18_five.h"
#include "../src/willow_arcade_pool_init.h"
#ifdef WILLOW_TEST_BOSS_EFFECTS
#include "../src/willow_arcade_actor12_pool.h"
#endif
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

static uint8_t attribute(uint16_t at){return rb(0x4000+at+2);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
#ifdef WILLOW_TEST_BOSS_EFFECTS
 willowMotionPutWord(rom+0x100010,0x4eb9);willowMotionPutLong(rom+0x100012,0x26da6);
 willowMotionPutWord(rom+0x100016,0x4eb9);willowMotionPutLong(rom+0x100018,0x2f39e);
 willowMotionPutWord(rom+0x10001c,0x4e75);
#endif
 if(willowMotionLong(rom+0x6772)!=0x910010)return 2;
 willowMotionPutLong(rom+0x6772,0xff4010);willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x4ad0,0xff4310);willowMotionPutLong(rom+0x4ad8,0xff4310);
 willowMotionPutLong(rom+0x3e44,0xff4000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor18-five-lifecycle-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor18-five-lifecycle-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0,completed=0,allStates=0;
 for(unsigned sample=0;sample<16;sample++){
  memset(expected,0,sizeof expected);willowArcadePoolsInit(&bus);
  uint16_t actor=willowActor18Pop(&bus,0xf48c,0xf486);uint8_t*r=resolve(actor);
  r[0]=1;r[32]=5;r[14]=(sample&1)*2;
  willowMotionPutWord(r+6,0x540);willowMotionPutWord(r+10,0x12cf);
  ww(0x8408,0x480);ww(0x840c,0x1200);ww(0x838c,0x480);ww(0x838e,0x480);
  ww(0x8284,sample*977);wb(0x8286,sample);wb(0x8346,sample*16);
  wb(0x8290,(sample>>2)&1);wb(0x8293,(sample>>3)&1);
  unsigned floorIndex=0;
  for(unsigned n=4;n<256;n+=4)if((sourceWord(0x4712UL+n*2)&0x8df)==0x88f){floorIndex=n;break;}
  if(!floorIndex)return 2;
  for(unsigned x=0;x<1024;x+=16)wb(0x4000+willowCollisionOffset(x,0x1230)+2,floorIndex);
  for(unsigned n=0;n<16;n++)wb(0xf042+n*64,n<sample?1:0);
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
  unsigned step,activeTicks=0,roles=0,states=0;uint16_t d4=0;
  uint32_t a4=sample&1?0xffb000:0x123456;
  for(step=0;step<8192;step++){
   wb(0x8019,step);ww(0x984c,0x490+(step*3)%288);ww(0x9850,0x1237);
   if(r[2]==2){activeTicks++;if(activeTicks==600)r[42]=64;if(activeTicks==1600)r[42]=48;if(activeTicks==3072)r[42]=0;}
   willowArcadeVisibilityInit(&bus,0,0);
   for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
   uint16_t oldD4=d4;
   uint32_t oldA4=a4;
   for(unsigned n=0;n<8;n++){
    uint16_t at=0xf490+n*128;uint8_t*q=resolve(at);if(!q[0])continue;
    roles|=1u<<(q[33]/2);if(!q[33]&&q[2]==2)states|=1u<<(q[3]/2);
#ifndef WILLOW_TEST_ACTOR18_POOL
    if(!willowArcadeActor18FiveWithA4(&bus,resolve,at,sourceWord,&collision,&d4,&a4)){
     fprintf(stderr,"Unsupported lifecycle sample %u frame %u actor %x state %u/%u/%u role %u\n",sample,step,at,q[2],q[3],q[4],q[33]);return 1;
    }
#endif
   }
#ifdef WILLOW_TEST_ACTOR18_POOL
   if(!willowArcadeActor18PoolWithRegisters(&bus,resolve,sourceWord,NULL,NULL,&collision,&d4,&a4)){fprintf(stderr,"Pool rejected sample %u frame %u\n",sample,step);return 1;}
#endif
#ifdef WILLOW_TEST_BOSS_EFFECTS
   if(!willowArcadeActor12PoolWithRegisters(&bus,resolve,sourceWord,NULL,&collision,&d4,&a4))return 1;
#endif
   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(12,oldA4);
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(4,oldD4);
   m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x26da6);
#ifdef WILLOW_TEST_BOSS_EFFECTS
   m68k_set_reg(16,0x100010);
#endif
   for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04)return 1;
   if(m68k_get_reg(12)!=a4){fprintf(stderr,"Lifecycle A4 sample %u frame %u actual %x expected %x\n",sample,step,m68k_get_reg(12),a4);return 1;}
   if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Lifecycle D4 sample %u frame %u\n",sample,step);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Lifecycle sample %u frame %u at %x actual %x expected %x\n",sample,step,n,ram[n^1],expected[n]);return 1;
   }
   checks++;if(rb(0xf48c)==8
#ifdef WILLOW_TEST_BOSS_EFFECTS
    &&rb(0x980d)==60
#endif
   )break;
  }
  if(step==8192||activeTicks<3072||roles!=7||rb(0xf48c)!=8||!rb(0x83e7)||rb(0x83e6)||rb(0x83d0)){
   fprintf(stderr,"Incomplete sample %u frame %u active %u roles %x states %x pool %u\n",sample,step,activeTicks,roles,states,rb(0xf48c));return 1;
  }
  allStates|=states;completed++;
 }
 if(allStates!=15)return 1;
 printf("Actor18 selector-five lifecycle: %u full pool ticks, %u complete three-actor lifecycles, all four leader combat states; original non-stack RAM/D4 match through allocation, linked attacks, defeat and release\n",checks,completed);
 return 0;
}
