/* Headless differential verification of 8480..8518 service boundaries. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_panel_replace.h"
#include "../src/willow_native_attract_panel_replace.h"
#define WILLOW_SCENE_BOOT_PROBE
#include "../native/intro_probe/src/task_startup_data.h"
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


static unsigned writes;
static void videoWord(uint32_t at,uint16_t value){
 if(at<0x910000||at>=0x914000||(at&1))abort();
 ww(0x2000+at-0x910000,value);writes++;
}
#ifdef WILLOW_TEST_NATIVE_PANEL_REPLACE
static uint16_t nativeWord(uint32_t at){
 uint16_t value;
 if(at==0x88a8)value=nativePanelReplaceTargets[0];
 else if(at>=0x5c7e&&at<0x61dc&&!(at&1))value=nativeQueuedTextWords[(at-0x5c7e)/2];
 else if(at>=0x1e34&&at<0x1e54&&!(at&1))value=nativeTaskCommandTargets[(at-0x1e34)/2];
 else abort();
 if(value!=romWord(at))abort();return value;
}
#endif
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutLong(rom+0x5b38,0xff2000);willowMotionPutLong(rom+0x5b7c,0xff2000);
 willowMotionPutLong(rom+0x9c,0x160e);
 willowMotionPutLong(rom+0x8bf8,0xff316c);willowMotionPutLong(rom+0x8c08,0xff3170);
 const unsigned patch[]={0x8c70,0x8c80,0x8c8c,0x8c98};
 for(unsigned n=0;n<4;n++)willowMotionPutLong(rom+patch[n],0xff3164+n*4);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/attract-panel-replace-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-panel-replace-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 for(unsigned sample=0;sample<512;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  willowArcadeTaskInitFree(&bus,romWord);
  for(unsigned n=0;n<(sample/2)%9;n++)willowArcadeTaskAllocate(&bus,0x509c,0,3);
  wb(0x82af,sample);
  ww(0x8008,sample==255?0xffff:sample*257);
#ifdef WILLOW_TEST_NATIVE_PANEL_REPLACE
  ww(0x8004,4);ww(0x8008,26);
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  writes=0;
#ifdef WILLOW_TEST_NATIVE_PANEL_REPLACE
  uint16_t d1=sample*311,d2=sample*977;
  if(!willowNativeAttractPanelReplaceBoundary(&bus,nativeWord,videoWord,&d1,&d2)||(sample%2==0&&writes))return 1;
#else
  if(!willowArcadeAttractPanelReplace(&bus,romWord,videoWord)||(sample%2==0&&writes))return 1;
#endif
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);m68k_set_reg(16,
#ifdef WILLOW_TEST_NATIVE_PANEL_REPLACE
  0x8882
#else
  0x8b26
#endif
  );
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Scene wait mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
#ifdef WILLOW_TEST_NATIVE_PANEL_REPLACE
  if((uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2)return 1;
#endif
  checks++;
 }
#ifdef WILLOW_TEST_NATIVE_PANEL_REPLACE
 ww(0x8008,28);uint16_t d1=0x1234,d2=0xabcd;
 uint8_t untouched[65536];memcpy(untouched,expected,sizeof untouched);unsigned prior=writes;
 if(willowNativeAttractPanelReplaceBoundary(&bus,nativeWord,videoWord,&d1,&d2)||d1!=0x1234||d2!=0xabcd||writes!=prior||memcmp(untouched,expected,sizeof untouched))return 1;
#endif
 uint8_t saved[65536];memcpy(saved,expected,sizeof saved);
 if(willowArcadeAttractPanelReplace(&bus,NULL,videoWord)||willowArcadeAttractPanelReplace(&bus,romWord,NULL)||memcmp(saved,expected,sizeof saved))return 1;
 printf("Panel replacement: %u original-code comparisons; mask gate, text/overlay writes, allocation and non-stack RAM match\n",checks);
 return 0;
}
