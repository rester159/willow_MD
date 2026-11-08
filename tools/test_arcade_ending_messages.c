/* Original ending message leaves B690..B826, including sound and text clears. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_ending_messages.h"
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
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static void textWrite(uint32_t at,uint16_t v){if(at<0x910000||at>=0x914000)abort();ww(0x4000+at-0x910000,v);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);willowMotionPutLong(rom+0x80,0x15d0);
 if(willowMotionLong(rom+0x7d84)!=0x910200)return 2;
 willowMotionPutLong(rom+0x7d84,0xff4200);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/ending-messages-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/ending-messages-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0;
 const uint32_t entries[]={0xb690,0xb6dc,0xb710,0xb75e,0xb786,0xb7ae,0xb7dc};
 const uint16_t cursors[]={0,2,30,62,94,120,124,126};
 for(unsigned leaf=0;leaf<7;leaf++)for(unsigned sample=0;sample<768;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  wb(0x0e58,(sample%3)==2?255:sample%3);ww(0x800e,(sample/3)%4);
  unsigned gate=(sample/12)%4;wb(0x8290,gate&1);wb(0x8293,gate&2);
  ww(0x801e,cursors[(sample/48)%8]);wb(0xc0,sample/384?12:0);ww(0x800c,sample*2);
  uint16_t d1=sample*311,d2=sample*733,d4=sample*977;uint32_t a4=0x123456;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeEndingMessages(&bus,entries[leaf],textWrite,&a4,&d1,&d2))return 1;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*733);m68k_set_reg(4,d4);m68k_set_reg(12,0x123456);m68k_set_reg(16,entries[leaf]);
  for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000)return fprintf(stderr,"Ending message stalled %u/%u PC %x\n",leaf,sample,m68k_get_reg(16)),1;
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n])
   return fprintf(stderr,"Ending message %u/%u RAM %x got %x want %x\n",leaf,sample,n,ram[n^1],expected[n]),1;
  if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4||(uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2)
   return fprintf(stderr,"Ending message registers %u/%u\n",leaf,sample),1;
  checks++;
 }
 printf("Ending message leaves: %u original-CPU cases; seven handlers, busy/completed flags, countdown boundaries, sound gates/ring wrap, active/inactive message slots, RAM/text map/D1/D2/D4/A4 match\n",checks);
 return 0;
}
