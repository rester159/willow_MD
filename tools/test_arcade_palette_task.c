/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_palette_task.h"
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
static uint16_t colorRead(uint32_t address){return rw(0x2000+address-0x91c000);}
static void colorWrite(uint32_t address,uint16_t value){ww(0x2000+address-0x91c000,value);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Stop precisely on entry to the scheduler sleep service. Preserve its
  * JSR return address for checking; sleep/context restoration is separately
  * verified by the task-continuation oracle, not executed in this fixture. */
 willowMotionPutWord(rom+0x1750,0x4ef9);
 willowMotionPutLong(rom+0x1752,0x100000);
 willowMotionPutWord(rom+0x165a,0x4ef9);
 willowMotionPutLong(rom+0x165c,0x100000);
 const unsigned patches[]={0x51ee,0x51fe,0x520e,0x521e,0x522e,0x523e};
 for(unsigned n=0;n<6;n++){
  if(willowMotionLong(rom+patches[n])!=0x91c000+n*1024)return 2;
  willowMotionPutLong(rom+patches[n],0xff2000+n*1024);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/palette-task-bus-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/palette-task-bus-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at)); \
 for(unsigned slice=0;slice<4&&m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Unexpected PC %x\n",m68k_get_reg(16));return 1;} \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 const uint16_t entries[]={0x50c6,0x5102,0x50e4,0x5126};
 for(unsigned e=0;e<4;e++)for(unsigned sample=0;sample<1024;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  uint16_t slot=0x100+(sample%8)*32;
  ww(0x0204,0xff);ww(0x0206,slot);ww(slot+16,(e&1)*2);ww(slot+18,sample);
  ww(0x82c0,sample&1?0x8000:0);
  ww(0x0a3e,0);ww(0x0a40,sample%3==0?0:sample%3==1?506:123);
  for(unsigned n=0;n<3072;n++)ww(0x2000+n*2,((sample+n*13)&15)*4096+((sample*977+n*37)&4095));
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  WillowTransitionStep step=willowArcadePaletteTask(&bus,entries[e],slot,colorRead,colorWrite);
  if(!step.supported)return 1;
  m68k_set_reg(14,0xff0000+slot);
  RUN(e<2?0x509c:entries[e],0xdeadbeef);
  unsigned sp=m68k_get_reg(15)&65535;
  if(sp!=(step.finished?0xff00:0xfefc))return 1;
  if(!step.finished){
   if((m68k_get_reg(0)&65535)!=step.delay)return 1;
   unsigned resume=((unsigned)ram[0xfefc^1]<<24)|((unsigned)ram[0xfefd^1]<<16)|((unsigned)ram[0xfefe^1]<<8)|ram[0xfeff^1];
   if(resume!=step.resume)return 1;
  }
  for(unsigned n=0;n<65536;n++)if(n<0xfe80||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Fade task mismatch entry %x sample %u address %04x got %02x want %02x\n",entries[e],sample,n,ram[n^1],expected[n]);return 1;}
  }
  checks++;
 }
 printf("Source palette task adapter: %u boundaries; original dispatch, six banks, completion masks, delay bytes, layer control and full non-stack RAM match\n",checks);
 return 0;
}
