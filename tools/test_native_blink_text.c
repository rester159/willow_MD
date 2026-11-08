/* Differential verification of queued text task against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_blink_text.h"
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
 if(at<0x910000||at>=0x914000){fprintf(stderr,"Text address outside map: %x\n",at);exit(1);}
 ww((uint16_t)(0x9000+at-0x910000),value);writes++;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x1750,0x60fe);willowMotionPutWord(rom+0x165a,0x60fe);
 /* Only relocate the CPS text map; execute original stream parsing/writes. */
 willowMotionPutLong(rom+0x5bd0,0xff9000);willowMotionPutLong(rom+0x5c32,0xff9000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/blink-text-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/blink-text-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,finished=0;
 const uint8_t delays[]={0,1,3,255};
 for(unsigned id=0;id<23;id++)for(unsigned clear=0;clear<2;clear++)
 for(unsigned d=0;d<4;d++)for(unsigned busy=0;busy<2;busy++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+id*31)^0xa5;
  ww(0x204,0xff);ww(0x206,0x120);wb(0x131,id|(clear?128:0));
  wb(0x132,busy);wb(0x133,delays[d]);wb(0x134,255);wb(0x82a6,0x77);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t pc=0x5ad2;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xfff000);
  m68k_set_reg(0,0xa5a51234);m68k_set_reg(1,0x1234abcd);m68k_set_reg(2,0x4321dcba);m68k_set_reg(16,pc);
  unsigned done=0;
  for(unsigned pass=0;pass<16;pass++){
   WillowTransitionStep step=willowNativeBlinkTextStep(pc,&bus,0x120,romWord,videoWord);
   if(!step.supported)return 1;
   retro_run();unsigned stop=m68k_get_reg(16);
   if(stop!=(step.finished?0x165a:0x1750)){fprintf(stderr,"Text stop id %u clear %u delay %u pass %u PC %x\n",id,clear,delays[d],pass,stop);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xef80&&n<0xf004)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Text RAM id %u clear %u delay %u pass %u address %x actual %x expected %x\n",id,clear,delays[d],pass,n,ram[n^1],expected[n]);return 1;
   }
   checks++;
   if(step.finished){finished++;done=1;break;}
   if((m68k_get_reg(0)&65535)!=step.delay)return 1;
   unsigned sp=m68k_get_reg(15)&65535;
   unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
   if(resume!=step.resume){fprintf(stderr,"Text continuation mismatch\n");return 1;}
   pc=step.resume;
   m68k_set_reg(15,m68k_get_reg(15)+4);m68k_set_reg(16,resume);
  }
  if(done){fprintf(stderr,"Blink task terminated unexpectedly\n");return 1;}
 }
 printf("Native blinking text: %u source boundaries, %u unexpected terminations, %u word writes; redraw/clear alternation, delays, payload and all non-stack RAM match\n",checks,finished,writes);
 return 0;
}
