/* Headless differential verification of 8480..8518 service boundaries. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_stage_update.h"
#include "../src/willow_arcade_stage_objects.h"
#include "../src/willow_arcade_pool_init.h"
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

static uint16_t romWord(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t actorWord(unsigned long a){return romWord(a);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


static void textWrite(uint32_t at,uint16_t value){if(at<0x910000||at>=0x914000)abort();ww(0x4000+at-0x910000,value);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);

 if(willowMotionLong(rom+0x5640)!=0x91056c)return 2;
 willowMotionPutLong(rom+0x5640,0xff456c);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/stage-update0-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/stage-update0-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;

 const uint16_t values[]={0,1,0xff,0x100,0x7fff,0xfffc,0xfffd,0xfffe,0xffff};
 for(unsigned sample=0;sample<2048;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint32_t pc=0x3807c;
  ww(0x8346,values[sample%9]);wb(0xf9a4,(sample/9)%256);
  wb(0x8019,(sample/2304)&1);wb(0x828f,sample&1);wb(0x8068,sample&2?32:0);
  wb(0x8362,sample&4);wb(0x986a,sample&8);
  ww(0x984c,sample*127);ww(0x9850,sample*53);
  willowArcadePoolsInit(&bus);ww(0x8344,0);
  willowStageObjectsInit((WillowStageObject*)(expected+0x8502),0,actorWord);
  ww(0x8408,(sample*7)&0xfff);ww(0x840c,0);ww(0x838c,(sample*3)&0xfff);
  wb(0xf47,0);wb(0x83d0,0);wb(0x83e6,0);wb(0xf40,sample%9);
  wb(0x9802,0);wb(0x9803,0);wb(0xf89a,0);wb(0x9801,sample%33);
  ww(0x8284,sample*317);
  for(unsigned n=0;n<5;n++){
   uint16_t r=0x8502+n*64;wb(r+2,(sample/3)%4*2);wb(r+3,(sample/7)%2*2);
   ww(r+20,sample%2?1:0);wb(r+22,1);wb(r+23,3);
   if(rb(r+16)==7){ww(r+38,0xffff);ww(r+40,0x9900);}
  }
  uint32_t a4=sample&1?0x123456:0xffb000,initialA4=a4;uint16_t d4=sample*977;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeStageUpdate0WithA4(&bus,romWord,textWrite,&a4))return 1;
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(4,d4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Stage update mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Registers sample %u A4 %x/%x D4 %x/%x\n",sample,m68k_get_reg(12),a4,m68k_get_reg(4),d4);return 1;}
  checks++;
 }
 printf("Full stage-zero update 3807C: %u original-instruction comparisons; difficulty overflow/modes, pre-update digits, position gates and retained RAM and D4/A4 match\n",checks);
 return 0;
}
