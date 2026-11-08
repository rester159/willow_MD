/* Headless differential verification of 8480..8518 service boundaries. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_pan_step.h"
#ifdef WILLOW_TEST_PAN_OUTER
#include "../src/willow_native_attract_pan_step.h"
#include "../native/intro_probe/src/task_startup_data.h"
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
static uint16_t romWord(uint32_t at){return willowMotionWord(rom+at);}
#ifdef WILLOW_TEST_PAN_OUTER
static uint16_t panWord(uint32_t at){
 if(at<0x8526||at>=0x8542||(at&1))abort();
 uint16_t value=nativePanStateTargets[(at-0x8526)/2];
 if(value!=romWord(at))abort();return value;
}
#endif
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


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
 f=fopen("build/attract-pan-step-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-pan-step-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 const uint16_t pcs[]={0x85ce,0x85f6,0x8742,0x876a};
 const uint16_t positions[]={0,1,0x1fe,0x1ff,0x200,0x201,0x300,0xffff,0x1f7,0x1f8,0x1f9};
 for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  willowArcadeTaskInitFree(&bus,romWord);
  for(unsigned n=0;n<(sample/128)%9;n++)willowArcadeTaskAllocate(&bus,0x509c,0,3);
  uint16_t pc=pcs[sample%4];
  ww(0x8038,positions[(sample/4)%11]);ww(0x8034,sample*257);wb(0x0e58,(sample/32)%2?0xff:0);
  wb(0xe0,(sample/64)%2?0x0c:0);ww(0x82c0,(sample/256)%2?0xffff:0);
  ww(0x8008,sample%3?8:0xffff);ww(0x8004,sample%3?0:0xffff);
  for(unsigned n=0;n<16;n++)wb(0xf042+n*64,n<(sample/128)%15?1:0);
#ifdef WILLOW_TEST_PAN_OUTER
  ww(0x8004,2);ww(0x8008,sample%4<2?2+(sample%2)*2:16+(sample%2)*2);
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t liveD1=(uint16_t)(sample*311),liveD2=(uint16_t)(sample*977);
  m68k_set_reg(1,liveD1);m68k_set_reg(2,liveD2);
#ifdef WILLOW_TEST_PAN_OUTER
  if(!willowNativeAttractPanStep(&bus,panWord,&liveD1,&liveD2))return 1;
  pc=0x8266;
#else
  if(!willowArcadeAttractPanStep(&bus,sample%4<2?2+(sample%2)*2:16+(sample%2)*2))return 1;
#endif
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
#ifdef WILLOW_TEST_PAN_OUTER
  if((m68k_get_reg(1)&65535)!=liveD1||(m68k_get_reg(2)&65535)!=liveD2){fprintf(stderr,"Pan live words sample %u got %x/%x expected %x/%x\n",sample,m68k_get_reg(1),m68k_get_reg(2),liveD1,liveD2);return 1;}
#endif
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Scene wait mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 uint8_t saved[65536];memcpy(saved,expected,sizeof saved);
 if(willowArcadeAttractPanStep(&bus,6)||memcmp(saved,expected,sizeof saved))return 1;
 wb(0xe58,0);ww(0x8038,0x200);
 for(unsigned n=0;n<16;n++)wb(0xf042+n*64,1);
 memcpy(saved,expected,sizeof saved);
 if(willowArcadeAttractPanStep(&bus,4)||memcmp(saved,expected,sizeof saved))return 1;
#ifdef WILLOW_TEST_PAN_OUTER
 for(unsigned state=4;state<=18;state+=14){
  ww(0x8004,2);ww(0x8008,state);ww(0x8038,state==4?0x200:0x1f8);
  for(unsigned available=0;available<(state==4?1u:2u);available++){
   for(unsigned n=0;n<16;n++)wb(0xf042+n*64,n<available?0:1);
   memcpy(saved,expected,sizeof saved);uint16_t d1=123,d2=456;
   if(willowNativeAttractPanStep(&bus,panWord,&d1,&d2)||d1!=123||d2!=456||memcmp(saved,expected,sizeof saved))return 1;
  }
 }
 unsigned traceCalls=0;
 for(unsigned direction=0;direction<2;direction++){
  uint16_t state=direction?18:4,d1=0x1234,d2=0x5678;
  memset(expected,0,sizeof expected);ww(0x8004,2);ww(0x8008,state);
  ww(0x8038,direction?0x100:0x300);ww(0x8034,direction?0:0x200);
  unsigned calls=0;
  while(rw(0x8008)==state&&calls<100){
   wb(0xe58,calls<12?1:0);
   for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
   m68k_set_reg(1,d1);m68k_set_reg(2,d2);
   if(!willowNativeAttractPanStep(&bus,panWord,&d1,&d2))return 1;
   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x8266);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(1)&65535)!=d1||(m68k_get_reg(2)&65535)!=d2)return 1;
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n])return 1;
   calls++;traceCalls++;
  }
  if(calls!=(direction?55u:56u)||rw(0x8038)!=(direction?0x202:0x1fe)||
     rw(0x8034)!=(direction?0x102:0xfe)||rw(0x8008)!=state+2||rw(0x800a)!=30)return 1;
 }
 printf("Native pan sequences: %u original updates including 12 busy waits per direction; both complete movements and terminal palette spawns match\n",traceCalls);
 puts("Native pan dispatch: live D1/D2 match original 8266 routing; palette exhaustion preserves all source state and register words");
#endif
 printf("Attract pan step: %u original-code comparisons; busy gating, fixed message, unsigned threshold/scroll steps and palette allocation match\n",checks);
 return 0;
}
