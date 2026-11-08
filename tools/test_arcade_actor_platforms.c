/* Headless original-68000/source-C/native verification of actor platforms 3B25C. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_platforms.h"
#include "../native/engine/src/actor_platforms.h"
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


static uint16_t sourceWord(uint32_t a){return (a&0xff0000)==0xff0000?rw(a):willowMotionWord(rom+a);}
typedef struct {uint32_t y;uint16_t x;uint8_t active,kind,variant,grounded,contact;} NativeContactFixture;
static NativeContactFixture nativeRiders[4],nativePlatforms[4];
static int nativeRider(void*context,uint16_t id,WillowNativePlatformRider*out){
 (void)context;if(id>=4)return 0;NativeContactFixture*a=nativeRiders+id;
 *out=(WillowNativePlatformRider){&a->y,a->x,a->active,a->kind,&a->grounded,&a->contact};return 1;
}
static int nativePlatform(void*context,uint16_t id,WillowNativeActorPlatformView*out){
 (void)context;if(id>=4)return 0;NativeContactFixture*a=nativePlatforms+id;
 *out=(WillowNativeActorPlatformView){a->x,a->y>>16,a->active,a->kind,a->variant,&a->contact};return 1;
}
static NativeContactFixture nativeFixture(uint16_t at){
 return (NativeContactFixture){((uint32_t)rw(at+10)<<16)|rw(at+12),rw(at+6),rb(at),rb(at+32),rb(at+33),rb(at+31),rb(at+36)};
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x8474,0x60fe);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor-platforms-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor-platforms-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 uint8_t nativeTables[160];
 memcpy(nativeTables,rom+0x3b2c4,40);memcpy(nativeTables+40,rom+0x3b2ec,40);memcpy(nativeTables+80,rom+0x3b39a,80);
 const WillowNativeActorPlatformServices nativeServices={NULL,nativeRider,nativePlatform,nativeTables};
 const uint16_t nativeIds[4]={0,1,2,3};

 for(unsigned sample=0;sample<16384;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint32_t pc=0x3b25c;
  wb(0x9802,sample%5);wb(0x9806,(sample/5)%5);ww(0x981e,0xf500);ww(0x9826,0xf520);
  const uint8_t types[]={0,3,5,1};
  for(unsigned n=0;n<4;n++){
   uint16_t actor=0x9900+n*128,platform=0xa000+n*128;
   ww(0xf500+n*2,actor);ww(0xf520+n*2,platform);
   wb(actor,(sample+n)%3);wb(actor+32,types[(sample/3+n)%4]);
   ww(actor+6,(sample%101)-50+0x200);ww(actor+10,((sample/101)%64)-16+0x100);
   wb(platform,(sample/7+n)%3);wb(platform+32,n==3?1:0);wb(platform+33,(sample/11+n)%4);
   ww(platform+6,0x200+n*3);ww(platform+10,0x100+n*2);wb(platform+36,(sample/13+n)%4);
  }
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  for(unsigned n=0;n<4;n++){nativeRiders[n]=nativeFixture(0x9900+n*128);nativePlatforms[n]=nativeFixture(0xa000+n*128);}
  if(!willowNativeActorPlatforms(nativeIds,rb(0x9802),nativeIds,rb(0x9806),&nativeServices))return 1;
  if(!willowArcadeActorPlatforms(&bus,sourceWord))return 1;
  for(unsigned n=0;n<4;n++){
   NativeContactFixture a=nativeFixture(0x9900+n*128);
   if(a.y!=nativeRiders[n].y||a.grounded!=nativeRiders[n].grounded||a.contact!=nativeRiders[n].contact||
      rb(0xa024+n*128)!=nativePlatforms[n].contact){fprintf(stderr,"Native actor-platform mismatch sample %u slot %u\n",sample,n);return 1;}
  }
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(14,0xff9846);m68k_set_reg(10,0xff9900);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Actor platforms mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 printf("Actor platform pass 3B25C: %u complete original/source-C/native-pass comparisons; nested queues, type gates, snap/edge flags and early scan termination match\n",checks);
 return 0;
}
