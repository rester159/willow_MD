/* Headless differential verification of class-18 entry and shared palette allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_message_panel.h"
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



static uint8_t source[0x100000];
static uint16_t sourceWord(uint32_t at){return willowMotionWord(source+at);}

static void textWrite(uint32_t at,uint16_t v){if(at<0x910000||at>=0x914000)abort();ww(0x4000+at-0x910000,v);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(source,rom,sizeof source);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 for(unsigned n=0;n<5;n++)for(unsigned table=0;table<2;table++){
  unsigned at=(table?0x7c64:0x7c50)+n*4;uint32_t target=willowMotionLong(rom+at);
  if(target<0x910000||target>=0x914000)return 2;
  willowMotionPutLong(rom+at,0xff4000+target-0x910000);
 }
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
 willowMotionPutWord(rom+0x7bf4,0x4ef9);willowMotionPutLong(rom+0x7bf6,0x100000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/message-panel-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/message-panel-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 for(unsigned sample=0;sample<6144;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*13+sample*31)^0xa5;
  uint16_t slot=sample&1?0xc0:0xe0;
  wb(slot+20,(sample%6)*2);wb(slot+21,(sample/6)%256);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint8_t ready=0;WillowTransitionStep step=willowNativeMessagePanelStep(&bus,slot,sourceWord,textWrite,&ready);
  if(!step.supported)return 1;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|slot);
  m68k_set_reg(0,0);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x7b46);
  for(unsigned slice=0;slice<8&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  unsigned sp=m68k_get_reg(15)&65535;
  if(m68k_get_reg(16)!=0x100000||sp!=0xfefc)return 1;
  if(!ready){
   unsigned pc=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
   if(pc!=step.resume||(m68k_get_reg(0)&65535)!=step.delay)return 1;
  }
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Message panel mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 printf("Message panel: %u original resumption comparisons; five drawing phases, mirrored tiles/attributes, byte-delay wrap, mandatory sleep and same-dispatch text handoff match full non-stack RAM/text\n",checks);
 return 0;
}
