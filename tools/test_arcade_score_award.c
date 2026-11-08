/* Headless original-instruction verification of 6274 score award. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_score_award.h"
#include "../src/willow_native_task_runtime.h"
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


static const WillowVisibilityBus bus={rw,rb,ww,wb};

static void videoWord(uint32_t at,uint16_t v){if(at<0x910000||at>=0x914000)abort();ww(0x4000+(at-0x910000),v);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 const unsigned patch[]={0x62a2,0x62ae,0x62cc};
 for(unsigned n=0;n<3;n++){uint32_t at=willowMotionLong(rom+patch[n]);if(at<0x910000||at>=0x914000)return 2;willowMotionPutLong(rom+patch[n],at-0x910000+0xff4000);}
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/score-award-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/score-award-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0;const uint32_t scores[]={0,1,0x99999999,0x12345678,0x00009999,0x00999999,0x00000500,0x88888888};
 for(unsigned sample=0;sample<896;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint16_t award=(sample%14)*2;uint32_t score=scores[(sample/14)%8];
  ww(0x834a,score>>16);ww(0x834c,score);uint32_t high=sample&2?0xffffffff:sample&1?score:0;ww(0xf03e,high>>16);ww(0xf040,high);
  wb(0x8293,(sample/112)&1);wb(0x8292,(sample/224)&1);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d4=sample*73,d3=(sample*73)^0x5ac3;uint32_t a4=0xffb000;
  unsigned entry=0x6274,stop=0x100000;
  m68k_set_reg(3,0x17bc0000UL|d3);
  if(!willowArcadeScoreAwardWithD3(&bus,award,romWord,videoWord,&d3))return 1;
  m68k_set_reg(0,award);
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
m68k_set_reg(4,d4);m68k_set_reg(12,a4);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop)return fprintf(stderr,"Score award stalled sample %u PC %x\n",sample,m68k_get_reg(16)),1;
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n])
   return fprintf(stderr,"Score award sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]),1;
  if((uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=a4||m68k_get_reg(3)!=(0x17bc0000UL|d3))return 1;
  checks++;
 }
 uint8_t saved[65536];
 for(unsigned kind=0;kind<6;kind++){
  wb(0x8293,0);memcpy(saved,expected,sizeof saved);
  if(willowArcadeScoreAward(kind==0?NULL:&bus,kind==3?1:kind==4?28:kind==5?0xffff:0,
    kind==1?NULL:romWord,kind==2?NULL:videoWord)||memcmp(saved,expected,sizeof saved))return 1;
 }
 wb(0x8293,1);memcpy(saved,expected,sizeof saved);
 if(!willowArcadeScoreAward(&bus,0xffff,NULL,NULL)||memcmp(saved,expected,sizeof saved))return 1;
 /* Unknown residue must be refused before allocation/state/HUD mutation.
  * Otherwise a missing producer silently reproduces the original zero bug. */
 wb(0x8293,0);wb(0x020e,0);wb(0x100,12);
 ww(0x104,0);ww(0x106,0x626a);ww(0x110,0);
 memcpy(saved,expected,sizeof saved);
 WillowNativeTaskRuntime runtime={0};WillowNativeTaskRegisters registers={0xa5c3,0};
 WillowNativeTaskRun refused=willowNativeTaskRunOneWithInterludeSetupAndRegisters(
  &runtime,&bus,0x100,1,romWord,0,0,0,0,videoWord,0,
  0,0,0,0,0,0,0,0,0,0,0,&registers);
 if(!refused.unsupported||refused.dispatched||memcmp(saved,expected,sizeof saved)||
    runtime.resume[8]||registers.known||registers.d3!=0xa5c3)return 1;
 puts("Queued score: unknown inherited D3 refused without task, RAM, HUD or register mutation.");
 printf("Score awards: %u original-CPU cases; all 14 awards, BCD carry/overflow, attract gate, both players, high-score comparison and exact HUD words including inherited D3 attribute bits match; unrelated RAM/D4/A4 unchanged\n",checks);
 return 0;
}
