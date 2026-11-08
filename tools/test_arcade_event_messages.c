/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_event_messages.h"
#include "../src/willow_arcade_tasks.h"
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

static void putRamWord(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
static uint8_t expected[65536];
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static uint16_t sourceWord(uint32_t at){return willowMotionWord(rom+at);}
static void tileWord(uint32_t address,uint16_t value){
 if(address<0x910000||address>=0x914000)abort();
 ww(0x4000+address-0x910000,value);
}
static void service(uint32_t entry,int argument){if(!willowArcadeEventClear(entry,argument,tileWord))abort();}
static void task(uint16_t d0,uint16_t d1,uint16_t d2,uint32_t a0){
 const WillowVisibilityBus bus={rw,rb,ww,wb};willowArcadeTaskStart(&bus,d0,a0,d1,d2,1);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Relocate the CPS tile destination only; original logic is unchanged. */
 const unsigned pointers[]={0x7d84,0x7daa};
 for(unsigned n=0;n<2;n++){
  if(willowMotionLong(rom+pointers[n])!=0x910200)return 2;
  willowMotionPutLong(rom+pointers[n],0xff4200);
 }
 if(willowMotionLong(rom+0x7d74)!=0x910000)return 2;
 willowMotionPutLong(rom+0x7d74,0xff4000);
 if(willowMotionLong(rom+0x679e)!=0x910034)return 2;
 willowMotionPutLong(rom+0x679e,0xff4034);
 willowMotionPutLong(rom+0xa8,0x15f2);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/event-messages-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/event-messages-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 const WillowEventUiServices ui={service,task,NULL};
 unsigned checks=0;
 for(unsigned item=0;item<256;item++)for(unsigned position=0;position<6;position++)
 for(unsigned sample=0;sample<16;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  ww(0x0f10,(position&1)*2);ww(0x0f12,(position>>1)*2);wb(0x0f22+position,item);
  wb(192,sample);wb(224,15-sample);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  willowArcadeEventMessages(&bus,&ui,sourceWord);
  RUN(0x346e0,0xdeadbeef);
  for(unsigned n=0;n<65536;n++)if(n<0xfed0||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Event messages mismatch item %u position %u sample %u address %04x\n",item,position,sample,n);return 1;}
  }
  checks++;
 }
 for(unsigned half=0;half<3;half++)for(unsigned row=0;row<32;row++){
  for(unsigned n=0;n<65536;n++)expected[n]=(row*31+n*7)^0xa5;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t entry=half==2?0x7d70:half?0x7da8:0x7d80;
  if(!willowArcadeEventClear(entry,row,tileWord))return 1;
  RUN(entry,row);
  for(unsigned n=0;n<65536;n++)if(n<0xfed0||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Event clear mismatch half %u row %u address %04x\n",half,row,n);return 1;}
  }
 }
 for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  m68k_set_reg(4,0x12345678);m68k_set_reg(12,0x345678);
  if(!willowArcadeEventClear(0x679c,0,tileWord))return 1;
  RUN(0x679c,0xdeadbeef);
  if(m68k_get_reg(4)!=0x12345678||m68k_get_reg(12)!=0x345678)return 1;
  for(unsigned n=0;n<65536;n++)if(n<0xfed0||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Stage-entry text clear mismatch %u at %04x\n",sample,n);return 1;}
  }
 }
 puts("679C stage-entry clear: 64 original-CPU cases, exact 48x12 cells, untouched RAM and D4/A4 match");
 printf("Source event messages: %u cases with original clear loops and actual task trap; 96 independent clear cases, all non-stack RAM matches\n",checks);
 return 0;
}
