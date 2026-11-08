/* Headless differential verification of original panel setup 893C. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_panel.h"
#include "../src/willow_native_attract_panel.h"
#include "../native/intro_probe/src/intro_panel_data.h"
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


static uint16_t panelWord(uint32_t at){
 unsigned short value=0;if(!nativeIntroPanelWordRead(at,&value)||value!=romWord(at))abort();return value;
}
#ifdef WILLOW_TEST_NATIVE_PANEL
static WillowNativeScroll1 retainedPanel;
static WillowHudSurface retainedHud;
#endif
static unsigned writes;
static void videoWord(uint32_t at,uint16_t value){
 if(at<0x910000||at>=0x912000||(at&1))abort();
 ww(0x2000+at-0x910000,value);writes++;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutLong(rom+0x546a,0xff2000);
 const unsigned patch[]={0x8c1c,0x8c2c,0x8c3c,0x8c4c};
 for(unsigned n=0;n<4;n++)willowMotionPutLong(rom+patch[n],0xff3164+n*4);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/attract-panel-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-panel-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  ww(0x8008,sample==255?0xffff:sample*257);
#ifdef WILLOW_TEST_NATIVE_PANEL
  ww(0x8004,4);ww(0x8008,8);
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  writes=0;
#ifdef WILLOW_TEST_NATIVE_PANEL
  uint16_t d1=sample*257,d2=sample*131;
  if(!willowNativeAttractPanel(&bus,panelWord,videoWord,&d1,&d2)||writes!=4176)return 1;
#else
  if(!willowArcadeAttractPanel(&bus,panelWord,videoWord)||writes!=4176)return 1;
#endif
#ifdef WILLOW_TEST_NATIVE_PANEL
  /* Re-run the optimized retained sink from exactly the same original input. */
  uint8_t reference[65536];memcpy(reference,expected,sizeof reference);
  for(unsigned n=0;n<65536;n++)expected[n]=ram[n^1];
  for(unsigned n=0;n<8192;n++)retainedPanel.words[n]=(n*37+sample)&65535;
  for(unsigned n=0;n<sizeof retainedHud/2;n++)((uint16_t*)&retainedHud)[n]=(n*13+sample)&65535;
  WillowHudSurface referenceHud=retainedHud;
  for(unsigned n=0;n<4096;n++)retainedPanel.words[n]=rw(0x2000+n*2);
  for(unsigned n=0;n<2048;n++){
   unsigned at=0x910000+n*4;
   willowHudSurfaceWrite(&referenceHud,at,panelWord(0xd2000+n*4)+0x7000);
   willowHudSurfaceWrite(&referenceHud,at+2,panelWord(0xd2002+n*4));
  }
  for(unsigned r=0;r<4;r++)for(unsigned c=0;c<10;c++){
   unsigned at=0x911164+r*4+c*128;
   willowHudSurfaceWrite(&referenceHud,at,0x71c0+r*16+c);
   willowHudSurfaceWrite(&referenceHud,at+2,19);
  }
  if(!willowNativeAttractPanelRetained(&bus,nativeIntroPanelWords,&retainedPanel,&retainedHud,&d1,&d2)||memcmp(&referenceHud,&retainedHud,sizeof retainedHud))return 1;
  for(unsigned n=0;n<4096;n++)ww(0x2000+n*2,retainedPanel.words[n]);
  for(unsigned n=4096;n<8192;n++)if(retainedPanel.words[n]!=((n*37+sample)&65535))return 1;
  if(memcmp(reference,expected,sizeof reference))return 1;
#endif
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,
#ifdef WILLOW_TEST_NATIVE_PANEL
  0x8266
#else
  0x893c
#endif
  );
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Scene wait mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
#ifdef WILLOW_TEST_NATIVE_PANEL
  if((uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2)return 1;
#endif
  checks++;
 }
 uint8_t saved[65536];memcpy(saved,expected,sizeof saved);
 if(willowArcadeAttractPanel(&bus,NULL,videoWord)||willowArcadeAttractPanel(&bus,romWord,NULL)||memcmp(saved,expected,sizeof saved))return 1;
#ifdef WILLOW_TEST_NATIVE_PANEL
 uint16_t d1=123,d2=456;
 for(unsigned state=0;state<40;state++)if(state!=8){
  ww(0x8008,state);memcpy(saved,expected,sizeof saved);writes=0;
  if(willowNativeAttractPanel(&bus,panelWord,videoWord,&d1,&d2)||willowNativeAttractPanelRetained(&bus,nativeIntroPanelWords,&retainedPanel,&retainedHud,&d1,&d2)||writes||d1!=123||d2!=456||memcmp(saved,expected,sizeof saved))return 1;
 }
 ww(0x8008,8);ww(0x8004,2);memcpy(saved,expected,sizeof saved);
 if(willowNativeAttractPanel(&bus,panelWord,videoWord,&d1,&d2)||memcmp(saved,expected,sizeof saved))return 1;
 puts("Native panel: outer arcade routing, retained map, live D1/D2 and unsupported-state preservation pass");
#endif
 printf("Attract panel: %u original-code comparisons; all 4176 tile/attribute writes, overlay, scroll registers, timer and non-stack RAM match\n",checks);
 return 0;
}
