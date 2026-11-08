/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_selector1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "../src/willow_arcade_actor_pool.h"
static uint8_t rom[0x200000],expected[65536];
#ifdef WILLOW_TEST_NATIVE_SELECTOR1
static uint8_t targetInitial[65536];
#endif
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
#ifdef WILLOW_TEST_NATIVE_SELECTOR1
 unsigned targetEntry=0,address;char line[512],kind,name[256];
 f=fopen("build/actor_selector1_probe.bin","rb");if(!f)return 2;
 if(!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
 f=fopen("build/actor_selector1_probe.sym","r");if(!f)return 2;
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3&&!strcmp(name,"actorSelector1Probe"))targetEntry=address;
 fclose(f);if(!targetEntry)return 2;
#endif

 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/selector1-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/selector1-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
#ifdef WILLOW_TEST_NATIVE_SELECTOR1
 /* Run the same input through compiled C before unchanged arcade code.
  * C's stack reservation is FC00..FF03; all other bytes must match. */
#define TARGET_CHECK() do { \
 memcpy(targetInitial,ram,sizeof targetInitial); \
 RUN(targetEntry,0); \
 for(unsigned n=0;n<65536;n++)if(n<0xfc00 || n>=0xff04){ \
  if(ram[n^1]!=expected[n]){fprintf(stderr,"Compiled selector-1 mismatch sample %u address %04x got %02x expected %02x\n",sample,n,ram[n^1],expected[n]);return 1;} \
 } \
 memcpy(ram,targetInitial,sizeof targetInitial); \
} while(0)
#else
#define TARGET_CHECK() ((void)0)
#endif
 unsigned checks=0,deferred=0;
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 const uint16_t positions[]={0,31,32,218,448,449,1535,1536,32767,32768,65535};
 for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample+n*17)^0x39;
  pool=(WillowActorPool){expected+0xccc6,indices,60,64,1,59};
  uint8_t*r=expected+0xccc6;r[0]=1;r[2]=(sample%4)*2;r[14]=2;r[30]=12;r[32]=1;
  willowMotionPutWord(r+6,positions[(sample/4)%11]);willowMotionPutWord(r+10,(sample*19)%512);
  willowMotionPutLong(r+20,0x325bc);willowMotionPutWord(r+28,(sample%3)==0?1:(sample%3)==1?2:0);
  busPutWord(0x9834,0xa002);expected[0x980d]=59;
  busPutWord(0x9838,0xa102);expected[0x980f]=0;
  WillowActorSelector1Context c={
   (sample*3)%512,(sample*7)%256,(sample/8)%6,(sample*97)&65535,
   sample%60,0xccc6,sample/4,(sample&16)?0x80:0,(sample&32)?1:0,
   sourceWord,&bus,releaseActor};
  busPutWord(0x8408,c.cameraX);busPutWord(0x840c,c.cameraY);
  busPutWord(0x8344,c.stage);busPutWord(0x838c,c.boundary);
  expected[0x8019]=c.frame;expected[0x8395]=c.flag395;expected[0x83dd]=c.flag3dd;
#ifdef WILLOW_TEST_NATIVE_SELECTOR1
  busPutWord(0x4000,c.handle);busPutWord(0x4002,c.remaining);busPutWord(0x4004,1);
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeActorSelector1(r,&c))return 1;
  TARGET_CHECK();
  m68k_set_reg(14,0xffccc6);m68k_set_reg(7,c.remaining);
  RUN(0x3257e,0);
  for(unsigned n=0;n<65536;n++)if(n<0xfed0 || n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Selector1 mismatch sample %u address %04x native %02x source %02x\n",sample,n,expected[n],ram[n^1]);return 1;}
  }
  if(sample%4==1 && r[2]==6){
   if(!willowArcadeActorSelector1(r,&c))return 1;
   TARGET_CHECK();
   m68k_set_reg(14,0xffccc6);m68k_set_reg(7,c.remaining);RUN(0x3257e,0);
   for(unsigned n=0;n<65536;n++)if(n<0xfed0 || n>=0xff04){
    if(ram[n^1]!=expected[n]){fprintf(stderr,"Deferred release mismatch sample %u address %04x\n",sample,n);return 1;}
   }
   deferred++;
  }
  checks++;
 }
 if(!deferred)return 1;
#ifdef WILLOW_TEST_NATIVE_SELECTOR1
 printf("Compiled 68000 selector-1: %u dispatches and %u deferred releases match the same host/source oracle outside reserved C stack\n",checks,deferred);
#endif
 printf("Deferred boundary-to-release sequences: %u\n",deferred);
 printf("Original selector-1 complete dispatch: %u state/init/animation/boundary/visibility/release comparisons, full non-stack RAM\n",checks);
 return 0;
}
