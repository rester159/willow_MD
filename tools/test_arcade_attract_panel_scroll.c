/* Headless differential verification of 8A5A..8A92 panel countdown and scroll boundaries. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_panel_scroll.h"
#include "../src/willow_native_attract_panel_scroll.h"
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


#ifdef WILLOW_TEST_NATIVE_PANEL_SCROLL
static uint16_t nativeWord(uint32_t at){
 if(at<0x889e||at>=0x88a4||(at&1))abort();
 uint16_t value=nativePanelScrollTargets[(at-0x889e)/2];
 if(value!=romWord(at))abort();return value;
}
#endif
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
 f=fopen("build/attract-panel-scroll-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-panel-scroll-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 const uint16_t pcs[]={0x8a5a,0x8a6c,0x8a78};
 const uint16_t scrolls[]={0,0x6ff,0x700,0x701,0x740,0x7ff,0x800,0xffff};
 const uint16_t timers[]={0,1,2,10,30,180,0x8000,0xffff};
 for(unsigned sample=0;sample<6144;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  willowArcadeTaskInitFree(&bus,romWord);
  for(unsigned n=0;n<(sample/128)%9;n++)willowArcadeTaskAllocate(&bus,0x509c,0,3);
  uint16_t pc=pcs[sample%3];
  ww(0x800a,timers[(sample/4)%8]);wb(0x0e58,(sample/32)%2?0xff:0);
  wb(0xc0,(sample/64)%2?0x0c:0);ww(0x82c0,(sample/256)%2?0xffff:0);
  ww(0x8008,sample%3?8:0xffff);ww(0x8004,sample%3?0:0xffff);
  ww(0x8030,scrolls[(sample/32)%8]);
#ifdef WILLOW_TEST_NATIVE_PANEL_SCROLL
  ww(0x8004,4);ww(0x8008,16+(sample%3)*2);pc=0x8882;
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d1=sample*311,d2=sample*977;
#ifdef WILLOW_TEST_NATIVE_PANEL_SCROLL
  uint8_t boundary=willowNativeAttractPanelScrollBoundary(&bus,nativeWord,&d1,&d2);
#else
  uint8_t boundary=willowArcadeAttractPanelScroll(&bus,16+(sample%3)*2);
#endif
  unsigned stop=boundary==WILLOW_ATTRACT_PALETTES?0xbc4:boundary==WILLOW_ATTRACT_CLEAR?0x65a4:0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Scene wait mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
#ifdef WILLOW_TEST_NATIVE_PANEL_SCROLL
  if((uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2)return 1;
#else
  (void)d1;(void)d2;
#endif
  checks++;
 }
#ifdef WILLOW_TEST_NATIVE_PANEL_SCROLL
 ww(0x8008,22);uint16_t d1=0x1234,d2=0xabcd;
 uint8_t untouched[65536];memcpy(untouched,expected,sizeof untouched);
 if(willowNativeAttractPanelScrollBoundary(&bus,nativeWord,&d1,&d2)||d1!=0x1234||d2!=0xabcd||memcmp(untouched,expected,sizeof untouched))return 1;
#endif
 uint8_t saved[65536];memcpy(saved,expected,sizeof saved);
 if(willowArcadeAttractPanelScroll(&bus,8)!=WILLOW_ATTRACT_UNSUPPORTED ||
    willowArcadeAttractPanelScroll(NULL,16)!=WILLOW_ATTRACT_UNSUPPORTED ||
    memcmp(saved,expected,sizeof saved))return 1;
 printf("Panel scroll: %u source comparisons; countdowns, unsigned scroll thresholds, wrap and palette boundaries match\n",checks);
 return 0;
}
