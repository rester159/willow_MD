/* Headless differential verification of complete 3A8E2 two-pass scan. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_secondary_player_dispatch.h"
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
static uint16_t inventoryWord(unsigned long a){return sourceWord(a);}
static void textWrite(uint32_t a,uint16_t v){ww(0x4000+(a-0x910000),v);}
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
 willowMotionPutLong(rom+0x4d6c,0xff4df0);
 willowMotionPutLong(rom+0x4da8,0xff4df0);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/secondary-player-dispatch-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/secondary-player-dispatch-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 WillowPlayerCollision collision={inventoryWord,NULL,0,0,0,0};

 for(unsigned sample=0;sample<32768;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint32_t pc=0x3a8e2;
  uint16_t x=sample&1?0xfff8:0x200,y=sample&2?0xfff8:0x100;
  ww(0x9906,x);ww(0x990a,y);ww(0x984c,x+(int)(sample%65)-32);
  ww(0x9850,y+(int)((sample/65)%160)-80);
  ww(0x985a,0xff);ww(0x985c,0x3000);ww(0x3006,(sample/19)%3);
  wb(0xf99e,sample&4);wb(0x9846,sample&8?2:1);
  wb(0x8364,(sample/16)%3);wb(0x8362,sample);
  wb(0x835c,sample&32?1:0);wb(0x835a,sample&64?1:0);wb(0x835b,sample&128?1:0);
  wb(0x9846,sample%7?1:(sample/7)%4);ww(0x989a,(sample/17)%3*8);
  wb(0xf48d,sample%9);wb(0xf48e,(sample/9)%9);ww(0xf488,0xf500);ww(0xf48a,0xf520);
  for(unsigned n=0;n<8;n++){
   uint16_t actor=0x9900+n*128;ww(0xf500+n*2,actor);ww(0xf520+n*2,0x9900+(7-n)*128);
   wb(actor,(sample/3+n)%4);wb(actor+32,(sample/4+n)%16);wb(actor+15,n%2);
   wb(actor+46,(sample+n)&8);wb(actor+60,(sample+n)&16);wb(actor+126,(sample+n)&32);wb(actor+127,(sample/7+n)%8);
   ww(actor+20,0xff);ww(actor+22,0x3100+n*16);ww(0x3106+n*16,(sample/11+n)%8);
   wb(actor+33,(sample/9+n)%10);ww(actor+38,0xffff);ww(actor+40,0xa000);wb(0xa021,(sample/13)%32);
   ww(actor+6,x+(int)n*8-32);ww(actor+10,y+(int)n*8-32);ww(actor+44,n&1?0xfff0:16);
  }
  wb(0x835d,(sample/5)%256);
  uint16_t d4=(sample*137)^0xa5a5,initialD4=d4;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeSecondaryPlayerDispatch(&bus,sourceWord,&collision,textWrite,&d4))return 1;
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(14,0xff9846);m68k_set_reg(10,0xff9900);m68k_set_reg(4,0xabcd0000UL|initialD4);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Player contact dispatch mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"D4 mismatch %u\n",sample);return 1;}
  checks++;
 }
 printf("Secondary/player dispatch 3A8E2: %u complete original-pass comparisons; ordered mixed handlers, active filtering, empty queues, protection, inventory and retained RAM match\n",checks);
 return 0;
}
