/* Headless differential verification of class-18 entry and shared palette allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor18_six.h"
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

static void videoWrite(uint32_t at,uint16_t v){ww(at-0x914000+0x4000,v);}
static void textWrite(uint32_t at,uint16_t v){ww(at-0x910000+0x2000,v);}
static uint32_t mapped(uint32_t p){return p>=0x914000&&p<0x918000?p-0x914000+0xff4000:p;}
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
 willowMotionPutLong(rom+0x4ad0,0xff2310);willowMotionPutLong(rom+0x4ad8,0xff2310);
 willowMotionPutLong(rom+0x3e44,0xff4000);
 willowMotionPutLong(rom+0x4ab2,0xff2314);willowMotionPutLong(rom+0x4aba,0xff2314);
 willowMotionPutLong(rom+0x6668,0xff4000);willowMotionPutLong(rom+0x43d50,0xff4000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor18-six-lifecycle-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor18-six-lifecycle-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0,completed=0,allStates=0;
 for(unsigned sample=0;sample<16;sample++){
  memset(expected,0,sizeof expected);willowArcadePoolsInit(&bus);
  uint16_t actor=willowActor18Pop(&bus,0xf48c,0xf486);uint8_t*r=resolve(actor);
  r[0]=1;r[32]=6;r[30]=18;wb(0x83ee,1);
  ww(0x8408,0x1500);ww(0x840c,0);
  ww(0x8284,sample*977);wb(0x8286,sample);wb(0x8346,sample*16);
  wb(0x8290,(sample>>2)&1);wb(0x8293,(sample>>3)&1);wb(0x9866,sample&1);
  for(unsigned n=0;n<16;n++)wb(0xf042+n*64,n<sample?1:0);
  unsigned step,roles=0,states=0;uint16_t d4=0;
  uint32_t a4=sample&1?0xffb000:0x123456;
  for(step=0;step<8192;step++){
   wb(0x8019,step);ww(0x984c,0x1500+(step*3)%288);ww(0x9850,32+(step/32)%160);
   for(unsigned n=0;n<8;n++){
    uint8_t*q=resolve(0xf490+n*128);if(!q[0]||q[33]<2||q[2]!=2)continue;
    unsigned first=2+(sample&1);
    if(step==1000)q[42]=80;
    if(step==(q[33]==first?1800:3300))q[42]=0;
   }
   willowArcadeVisibilityInit(&bus,0,0);
   for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
   for(unsigned n=0;n<8;n++){
    uint16_t at=0xf490+n*128+64;uint32_t p=mapped(willowMotionLong(expected+at));
    for(unsigned j=0;j<4;j++)ram[(at+j)^1]=p>>(24-j*8);
   }
   uint16_t oldD4=d4;uint32_t oldA4=a4;
   for(unsigned n=0;n<8;n++){
    uint16_t at=0xf490+n*128;uint8_t*q=resolve(at);if(!q[0])continue;
    roles|=1u<<q[33];if(!q[33]&&q[2]==2)states|=1u<<(q[3]/2);
#ifndef WILLOW_TEST_ACTOR18_POOL
    if(!willowArcadeActor18SixWithA4(&bus,q,at,7-n,sourceWord,videoWrite,textWrite,&d4,&a4)){
     fprintf(stderr,"Unsupported sample %u frame %u actor %x state %u/%u role %u\n",sample,step,at,q[2],q[3],q[33]);return 1;
    }
#endif
   }
#ifdef WILLOW_TEST_ACTOR18_POOL
   if(!willowArcadeActor18PoolWithRegisters(&bus,resolve,sourceWord,videoWrite,textWrite,NULL,&d4,&a4)){fprintf(stderr,"Pool rejected sample %u frame %u\n",sample,step);return 1;}
#endif
#ifdef WILLOW_TEST_BOSS_EFFECTS
   if(!willowArcadeActor12PoolWithRegisters(&bus,resolve,sourceWord,NULL,NULL,&d4,&a4))return 1;
#endif
   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(12,oldA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(4,oldD4);
   m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x26da6);
#ifdef WILLOW_TEST_BOSS_EFFECTS
   m68k_set_reg(16,0x100010);
#endif
   for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04){fprintf(stderr,"PC %x frame %u\n",m68k_get_reg(16),step);return 1;}
   if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"D4 sample %u frame %u actual %x expected %x\n",sample,step,(uint16_t)m68k_get_reg(4),d4);return 1;}
   for(unsigned n=0;n<8;n++){
    uint16_t at=0xf490+n*128+64;uint32_t p=0;
    for(unsigned j=0;j<4;j++)p=(p<<8)|ram[(at+j)^1];
    if(p!=mapped(willowMotionLong(expected+at))){fprintf(stderr,"Pointer frame %u actor %x\n",step,at-64);return 1;}
   }
   for(unsigned n=0;n<65536;n++){
    if(n>=0xfe80&&n<0xff04)continue;
    if(n>=0xf490&&n<0xf890&&((n-0xf490)%128)>=64&&((n-0xf490)%128)<68)continue;
    if(ram[n^1]!=expected[n]){fprintf(stderr,"Lifecycle sample %u frame %u at %x actual %x expected %x\n",sample,step,n,ram[n^1],expected[n]);return 1;}
   }
   if(m68k_get_reg(12)!=a4){fprintf(stderr,"Lifecycle A4 sample %u frame %u actual %x expected %x\n",sample,step,m68k_get_reg(12),a4);return 1;}
   checks++;if(step>3600&&rb(0x83c3)==2&&rb(0xf48c)==6&&rw(0x848c)>256
#ifdef WILLOW_TEST_BOSS_EFFECTS
    &&rb(0x980d)==60
#endif
   )break;
  }
  if(step==8192||roles!=15||rb(0xefe)!=2||rb(0x83ce)||rb(0x83d0)||rb(0x838b)||!rb(0x83cf)){
   fprintf(stderr,"Incomplete sample %u frame %u roles %x states %x pool %u flags %u/%u scroll %u\n",sample,step,roles,states,rb(0xf48c),rb(0xefe),rb(0x83c3),rw(0x848c));return 1;
  }
  allStates|=states;completed++;
 }
 if(allStates!=15){fprintf(stderr,"Missing body states %x\n",allStates);return 1;}
 printf("Selector-six retained encounter: %u full pool ticks, %u four-part encounters, both head defeat orders; original non-stack RAM/D4 and mapped pointers match through final departure (two body records remain as in source)\n",checks,completed);
 return 0;
}
