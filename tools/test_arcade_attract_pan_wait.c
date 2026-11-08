/* Headless differential verification of 8480..8518 service boundaries. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_pan_wait.h"
#include "../src/willow_native_attract_pan_wait.h"
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
 willowMotionPutWord(rom+0xbc4,0x60fe);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/attract-pan-wait-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-pan-wait-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 const uint16_t pcs[]={0x864e,0x867a,0x868c,0x86a6,0x87f2,0x867a,0x881c,0x8506};
 const uint16_t states[]={6,8,10,12,20,22,24,26};
 const uint16_t timers[]={0,1,2,10,30,180,0x8000,0xffff};
 for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  willowArcadeTaskInitFree(&bus,romWord);
  for(unsigned n=0;n<(sample/128)%9;n++)willowArcadeTaskAllocate(&bus,0x509c,0,3);
  uint16_t pc=pcs[sample%8];
  ww(0x800a,timers[(sample/8)%8]);wb(0x0e58,(sample/64)%2?0xff:0);
  wb(0xc0,(sample/128)%2?0x0c:0);ww(0x82c0,(sample/256)%2?0xffff:0);
  ww(0x8008,sample%3?8:0xffff);ww(0x8004,sample%3?0:0xffff);
  uint16_t state=states[sample%8];
#ifdef WILLOW_TEST_NATIVE_PAN_WAIT
  ww(0x8004,2);ww(0x8008,state);pc=0x8266;
  uint16_t d1=sample*113+0xa512,d2=sample*277+0x5a34;
  m68k_set_reg(1,d1);m68k_set_reg(2,d2);
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
#ifdef WILLOW_TEST_NATIVE_PAN_WAIT
  uint8_t boundary=willowNativeAttractPanWaitBoundary(&bus,romWord,&d1,&d2);
#else
  uint8_t boundary=state==26?willowArcadeAttractSceneWait(&bus,0x8506,romWord):willowArcadeAttractPanWait(&bus,state,romWord);
#endif
  unsigned stop=boundary==WILLOW_ATTRACT_PALETTES?0xbc4:boundary==WILLOW_ATTRACT_CLEAR?0x65a4:0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Scene wait mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
#ifdef WILLOW_TEST_NATIVE_PAN_WAIT
  if((m68k_get_reg(1)&65535)!=d1||(m68k_get_reg(2)&65535)!=d2){
   fprintf(stderr,"Pan wait live words sample %u state %u boundary %u: %x/%x expected %x/%x\n",sample,state,boundary,m68k_get_reg(1)&65535,m68k_get_reg(2)&65535,d1,d2);return 1;
  }
#endif
  checks++;
 }
#ifdef WILLOW_TEST_NATIVE_PAN_WAIT
 for(unsigned bad=0;bad<5;bad++){
  ww(0x8004,bad==0?0:2);ww(0x8008,bad==1?7:bad==2?28:6);
  uint8_t snapshot[65536];memcpy(snapshot,expected,sizeof snapshot);
  uint16_t d1=0xa512,d2=0x5a34;
  if(willowNativeAttractPanWaitBoundary(&bus,bad==3?NULL:romWord,bad==4?NULL:&d1,&d2)||
     memcmp(snapshot,expected,sizeof snapshot)||d1!=0xa512||d2!=0x5a34)return 1;
 }
 puts("Native pan wait routing: original 8266 live D1/D2 and atomic rejection pass");
#endif
 uint8_t saved[65536];memcpy(saved,expected,sizeof saved);
 if(willowArcadeAttractPanWait(&bus,7,romWord)!=WILLOW_ATTRACT_UNSUPPORTED ||
    willowArcadeAttractPanWait(&bus,6,NULL)!=WILLOW_ATTRACT_UNSUPPORTED ||
    memcmp(saved,expected,sizeof saved))return 1;
 printf("Attract pan waits: %u source comparisons; countdowns, busy gating, fixed tasks, fade allocation and service boundaries match\n",checks);
 return 0;
}
