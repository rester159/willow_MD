/* Headless differential verification of complete 42F48 controller lifecycle. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_scroll3_controller.h"
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
static uint8_t sourceByte(uint32_t at){return rom[at];}
static uint8_t*resolve(uint16_t at){return expected+at;}
static void mapWrite(uint16_t at,uint16_t value){if(at>=16384)abort();ww(0x4000+at,value);}
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
 if(willowMotionLong(rom+0x43d50)!=0x914000)return 2;
 willowMotionPutLong(rom+0x43d50,0xff4000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scroll3-controller-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scroll3-controller-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;


 for(unsigned stage=0;stage<7;stage++)for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  wb(0x8484,0);wb(0x8485,0);ww(0x8344,stage);
  for(unsigned step=0;step<32;step++){
  uint32_t pc=0x42f48;
  ww(0x8408,sample*113u+step*17);ww(0x840c,sample*7919u-step*13);
  ww(0x8398,sample*93u+step*7);ww(0x8374,step%8);wb(0x8470,(step/8)&1);
  uint16_t d4=(sample*977+step*137)^0xa5a5,oldD4=d4;uint32_t a4=step&1?0x123456:0xffb000;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeScroll3ControllerWithD4(&bus,resolve,sourceByte,sourceWord,mapWrite,NULL,&d4))return 1;
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(4,oldD4);m68k_set_reg(12,a4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(14,0xff9846);m68k_set_reg(10,0xff9900);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Scroll3 controller mismatch stage %u sample %u step %u at %x actual %x expected %x\n",stage,sample,step,n,ram[n^1],expected[n]);return 1;
  }

  if((uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=a4){fprintf(stderr,"Scroll3 registers stage %u sample %u step %u D4 %x/%x A4 %x/%x\n",stage,sample,step,m68k_get_reg(4),d4,m68k_get_reg(12),a4);return 1;}
  checks++;
 }
 }
 printf("Scroll3 controller 42F48: %u complete original-pass comparisons; seven stages, startup through active streaming, freeze, parallax, full RAM, tile map and D4/A4 match\n",checks);
 return 0;
}
