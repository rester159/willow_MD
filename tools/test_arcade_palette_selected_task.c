/* Differential verification of selected-palette task against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_palette_selected_task.h"
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


static uint16_t colorRead(uint32_t at){return rw(0x2000+at-0x91c000);}
static void colorWrite(uint32_t at,uint16_t v){ww(0x2000+at-0x91c000,v);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x1750,0x60fe);willowMotionPutWord(rom+0x165a,0x60fe);
 const unsigned operands[]={0x514a,0x51b2};
 for(unsigned n=0;n<2;n++){
  if(willowMotionLong(rom+operands[n])!=0x91c000)return 2;
  willowMotionPutLong(rom+operands[n],0xff2000);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/palette-selected-task-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/palette-selected-task-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,finished=0;
 const uint16_t entries[]={0x5138,0x519e};
 for(unsigned id=0;id<2;id++)for(unsigned sample=0;sample<2048;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  for(unsigned n=0;n<2048;n++)ww(0x2000+n*2,((sample+n/16)%16)*4096+((n*17)&4095));
  /* Include terminal and mixed masks independently of color brightness. */
  unsigned bank=(sample%128)/32;
  uint32_t mask=(sample/128)%4==0?0:(sample/128)%4==1?0xffffffffUL:0xa55a03f0UL;
  ww(0x82a8+bank*4,mask>>16);ww(0x82aa+bank*4,mask);
  ww(0x204,0xff);ww(0x206,0x100);ww(0x110,4+id*2);wb(0x112,sample%128);wb(0x113,sample%3==0?0:sample%3==1?4:255);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xfff000);m68k_set_reg(16,0x509c);
  uint16_t pc=entries[id];unsigned done=0;
  for(unsigned pass=0;pass<20;pass++){
   WillowTransitionStep step=willowArcadePaletteSelectedTask(&bus,pc,0x100,colorRead,colorWrite,0);
   retro_run();unsigned stop=m68k_get_reg(16);
   if(!step.supported||stop!=(step.finished?0x165a:0x1750))return 1;
   for(unsigned n=0;n<65536;n++)if(!(n>=0xef80&&n<0xf004)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Selected fade RAM id %u sample %u pass %u at %x actual %x expected %x\n",id,sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   checks++;
   if(step.finished){finished++;done=1;break;}
   unsigned sp=m68k_get_reg(15)&65535;
   unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
   if(resume!=step.resume||(m68k_get_reg(0)&65535)!=step.delay)return 1;
   pc=step.resume;m68k_set_reg(15,m68k_get_reg(15)+4);m68k_set_reg(16,pc);
  }
  if(!done)return 1;
 }
 uint8_t saved[65536];
 for(unsigned parameter=128;parameter<256;parameter++){
  wb(0x112,parameter);memcpy(saved,expected,sizeof saved);
  if(willowArcadePaletteSelectedTask(&bus,0x519e,0x100,colorRead,colorWrite,0).supported||memcmp(saved,expected,sizeof saved))return 1;
 }
 memcpy(saved,expected,sizeof saved);
 if(willowArcadePaletteSelectedTask(&bus,0x519e,0x100,NULL,colorWrite,0).supported||
    willowArcadePaletteSelectedTask(&bus,0x1234,0x100,colorRead,colorWrite,0).supported||memcmp(saved,expected,sizeof saved))return 1;
 printf("Selected palette tasks: %u source boundaries, %u completed fade-in/out cases across all 128 groups in four banks; masks, colors, layer bits, delays and all non-stack RAM match\n",checks,finished);
 return 0;
}
