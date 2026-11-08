/* Differential verification of successful reset RAM and inherited diagnostic register. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_task_startup.h"
#include "../src/willow_arcade_reset_ram.h"
#include "../src/willow_native_boot_globals.h"
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
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Stop before palette loading/input latching. Diagnostics and reset tail
  * execute unchanged. Use work RAM for the diagnostic's parameterized span. */
 willowMotionPutWord(rom+0x1436,0x4ef9);willowMotionPutLong(rom+0x1438,0x100000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/reset-ram-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/reset-ram-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 for(unsigned sample=0;sample<128;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  uint16_t last=(uint16_t)(sample*977+0x2468);
  ww(0x7ffe,last);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  willowArcadeResetWorkRAM(&bus);willowArcadeStartupPrimaryQueue(&bus);
  /* Final RAM diagnostic in reset calls 682 with a 32 KiB object-RAM span
   * and returns to 592. Relocate only its input pointers, not instructions.
   * The original later RAM clear means the expected end image is independent
   * of this scratch location. High D2 is deliberately nonzero. */
  m68k_set_reg(17,0x2700);m68k_set_reg(8,0xff0000);m68k_set_reg(9,0xff7ffe);
  m68k_set_reg(14,0x592);m68k_set_reg(2,0x76540000UL|sample);
  m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x682);
  unsigned frames=0;
  while(m68k_get_reg(16)!=0x100000&&frames++<100)retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(2)!=(0x76540000UL|last)||
     m68k_get_reg(1)!=0xffffffffUL||m68k_get_reg(13)!=0xffff8000UL){
   fprintf(stderr,"Reset sample %u PC %x D1 %x D2 %x A5 %x frames %u\n",sample,
    m68k_get_reg(16),m68k_get_reg(1),m68k_get_reg(2),m68k_get_reg(13),frames);return 1;
  }
  /* BSR142E leaves its return word at FFFFFC; every other byte is compared. */
  for(unsigned n=0;n<65532;n++)if(ram[n^1]!=expected[n]){
   fprintf(stderr,"Reset sample %u RAM %04x got %02x want %02x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  WillowNativeBootGlobals retained;memset(&retained,sample*73,sizeof retained);
  willowNativeBootGlobalsReset(&retained);
  for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
   const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
   for(unsigned n=0;n<g->count;n++)if(retained.bytes[g->offset+n]!=ram[(g->source+n)^1]){
    fprintf(stderr,"Retained reset mismatch sample %u address %04x\n",sample,g->source+n);return 1;
   }
  }
  checks++;
 }
 printf("Reset RAM: %u successful diagnostic/reset chains; last diagnostic word retained in D2, high register bits preserved, complete work-RAM clear, video mirrors and primary queue match original instructions\n",checks);
 puts("Retained reset: all 178 owned bytes match original reset for 128 initial storage patterns");
 return 0;
}
