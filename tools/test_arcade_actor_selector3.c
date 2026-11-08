/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_selector3.h"
#include "../src/willow_arcade_actor_selector1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "../src/willow_arcade_actor_pool.h"
static uint8_t rom[0x200000],expected[65536];
static WillowActorPool pool;static WillowActorPoolIndex indices[60];
static uint16_t sourceWord(unsigned long at){return willowMotionWord(rom+at);}
static uint16_t busWord(uint16_t at){return willowMotionWord(expected+at);}
static uint8_t busByte(uint16_t at){return expected[at];}
static void busPutWord(uint16_t at,uint16_t value){willowMotionPutWord(expected+at,value);}
static void busPutByte(uint16_t at,uint8_t value){expected[at]=value;}
static void releaseActor(uint8_t *record){
 willowActorPoolRelease(&pool,(record-pool.records)/64);
 uint16_t next=busWord(0x9834)-2;
 busPutWord(next,record-expected);busPutWord(0x9834,next);expected[0x980d]++;
}

static const WillowVisibilityBus schedulerBus={busWord,busByte,busPutWord,busPutByte};
static void stepActor(uint8_t *r,uint16_t remaining)
{
 uint16_t handle=(uint16_t)(r-expected);
 if(r[32]==1){
  WillowActorSelector1Context c={busWord(0x8408),busWord(0x840c),busWord(0x8344),busWord(0x838c),remaining,handle,
   busByte(0x8019),busByte(0x8395),busByte(0x83dd),sourceWord,&schedulerBus,releaseActor};
  if(!willowArcadeActorSelector1(r,&c))abort();
 }else if(r[32]==3){
  WillowActorSelector3Context c={busWord(0x8408),busWord(0x840c),handle,sourceWord,&schedulerBus,releaseActor};
  if(!willowArcadeActorSelector3Intro(r,&c))abort();
 }else abort();
}
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

static void putRamWord(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;

 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/selector3-intro-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/selector3-intro-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 unsigned checks=0,deferred=0;
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 const uint16_t positions[]={0,31,32,218,448,449,1535,1536,32767,32768,65535};
 for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample+n*17)^0x39;
  pool=(WillowActorPool){expected+0xccc6,indices,60,64,1,59};
  uint8_t*r=expected+0xccc6;r[0]=1;r[2]=(sample%4)*2;r[14]=((sample/4)%3)*2;r[30]=12;r[32]=3;r[33]=0;r[3]=((sample/16)%3)*2;
  willowMotionPutWord(r+6,positions[(sample/4)%11]);willowMotionPutWord(r+10,(sample*19)%512);
  willowMotionPutWord(r+48,(sample/4)%4==0?0:(sample/4)%4==1?1:(sample/4)%4==2?222:65535);
  willowMotionPutLong(r+20,0x32aa2);willowMotionPutWord(r+28,(sample%3)==0?1:(sample%3)==1?2:0);
  busPutWord(0x9834,0xa002);expected[0x980d]=59;
  busPutWord(0x9836,0xa082);expected[0x980e]=0;
  busPutWord(0x9838,0xa102);expected[0x980f]=0;
  busPutWord(0xf900,0xa182);expected[0xf902]=0;
  WillowActorSelector3Context c={
   (sample*3)%512,sample&16?(sample*7)%256:0,0xccc6,
   sourceWord,&bus,releaseActor};
  busPutWord(0x8408,c.cameraX);busPutWord(0x840c,c.cameraY);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeActorSelector3Intro(r,&c))return 1;
  m68k_set_reg(14,0xffccc6);m68k_set_reg(7,0);
  RUN(0x32690,0);
  for(unsigned n=0;n<65536;n++)if(n<0xfed0 || n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Selector3 intro mismatch sample %u address %04x native %02x source %02x\n",sample,n,expected[n],ram[n^1]);return 1;}
  }
  if(sample%4==1 && r[2]==6){
   if(!willowArcadeActorSelector3Intro(r,&c))return 1;
   m68k_set_reg(14,0xffccc6);m68k_set_reg(7,0);RUN(0x32690,0);
   for(unsigned n=0;n<65536;n++)if(n<0xfed0 || n>=0xff04){
    if(ram[n^1]!=expected[n]){fprintf(stderr,"Deferred release mismatch sample %u address %04x\n",sample,n);return 1;}
   }
   deferred++;
  }
  checks++;
 }
 /* Original pool scan and both real handlers, carried across frames.
  * Camera/frame values are explicit test inputs, not a ported outer manager.
  * Queue lanes are reset between samples; rendering is verified separately. */
 memset(expected,0,sizeof expected);
 willowActorPoolInit(&pool,expected+0xccc6,indices,60,64,12);
 for(unsigned n=0;n<60;n++)busPutWord(0xa000+n*2,0xccc6+(59-n)*64);
 uint8_t *first=willowActorPoolAllocate(&pool),*second=willowActorPoolAllocate(&pool);
 first[0]=2;first[32]=1;willowMotionPutWord(first+6,218);willowMotionPutWord(first+10,136);
 second[0]=1;second[32]=3;willowMotionPutWord(second+6,160);willowMotionPutWord(second+10,80);
 busPutWord(0x9834,0xa004);expected[0x980d]=58;
 for(unsigned frame=0;frame<650;frame++){
  uint16_t cx=frame>450?(frame-450)*2:0,cy=frame<64?512-frame*8:0;
  busPutWord(0x8408,cx);busPutWord(0x840c,cy);busPutWord(0x838c,cx>100?cx-100:0);
  expected[0x8019]=(uint8_t)(frame+133);
  busPutWord(0x9836,0xa202);expected[0x980e]=0;
  busPutWord(0x9838,0xa282);expected[0x980f]=0;
  busPutWord(0xf900,0xa302);expected[0xf902]=0;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  willowActorPoolSchedule(&pool,stepActor);RUN(0x2f39e,0);
  for(unsigned n=0;n<65536;n++)if(n<0xfed0 || n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Combined scheduler mismatch frame %u address %04x\n",frame,n);return 1;}
  }
 }
 if(first[0] || second[0] || pool.available!=60 || busByte(0x980d)!=60)return 1;
 puts("Combined source class-12 scan: 650 sequential two-actor updates through initialization, animation, boundary checks, and both releases");
 if(!deferred)return 1;
 printf("Deferred boundary-to-release sequences: %u\n",deferred);
 printf("Original selector-3 variant-zero dispatch: %u state/init/animation/boundary/visibility/release comparisons, full non-stack RAM\n",checks);
 return 0;
}
