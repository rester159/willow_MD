/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_title_actor.h"
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
 willowActorPoolRelease(&pool,(record-pool.records)/128);
 uint16_t next=busWord(0x981c)-2;
 busPutWord(next,record-expected);busPutWord(0x981c,next);expected[0x9801]++;
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
 f=fopen("build/title-actor-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/title-actor-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 unsigned checks=0;
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 const uint16_t positions[]={0,31,32,218,448,449,1535,1536,32767,32768,65535};
 for(unsigned sample=0;sample<2048;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample+n*17)^0x39;
  pool=(WillowActorPool){expected+0x9dc6,indices,50,128,1,49};
  uint8_t*r=expected+0x9dc6;r[0]=1;r[2]=(sample%4)*2;r[14]=(sample%3)*2;r[30]=4;r[32]=8;r[33]=0;
  willowMotionPutWord(r+6,positions[(sample/4)%11]);willowMotionPutWord(r+10,(sample*19)%512);
  willowMotionPutLong(r+20,0x15512);willowMotionPutWord(r+28,(sample%3)==0?1:(sample%3)==1?2:0);
  busPutWord(0x981c,0xa002);expected[0x9801]=49;
  busPutWord(0x981e,0xa102);busPutWord(0x9820,0xa202);busPutWord(0xf898,0xa302);
  expected[0x9802]=0;expected[0x9803]=0;expected[0xf89a]=0;
  uint16_t cameraX=(sample*3)%512,cameraY=(sample*7)%256;
  busPutWord(0x8408,cameraX);busPutWord(0x840c,cameraY);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeTitleActor(r,0x9dc6,cameraX,cameraY,sourceWord,&bus,releaseActor))return 1;
  m68k_set_reg(14,0xff9dc6);RUN(0x154cc,0);
  for(unsigned n=0;n<65536;n++)if(n<0xfed0||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Title actor mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;}
  }
  checks++;
 }
 /* Follow the actual looping animation, retaining its state between frames. */
 uint8_t*r=expected+0x9dc6;memset(r,0,128);r[0]=1;r[30]=4;r[32]=8;
 willowMotionPutWord(r+6,0x4c0);willowMotionPutWord(r+10,0x40);
 busPutWord(0x8408,0x400);busPutWord(0x840c,0);
 for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
 for(unsigned frame=0;frame<256;frame++){
  /* Publication consumes the queue separately; isolate each admission here. */
  busPutWord(0x981e,0x940a);busPutByte(0x9802,0);putRamWord(ram,0x981e,0x940a);ram[0x9802^1]=0;
  willowArcadeTitleActor(r,0x9dc6,0x400,0,sourceWord,&bus,releaseActor);
  m68k_set_reg(14,0xff9dc6);RUN(0x154cc,0);
  for(unsigned n=0;n<65536;n++)if((n<0xfed0||n>=0xff04)&&ram[n^1]!=expected[n])return 1;
  checks++;
 }
 printf("Title actor: %u original instruction comparisons; source animation loop, visibility lanes, bounds and class-4 release match\n",checks);
 return 0;
}
