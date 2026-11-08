/* Headless differential verification of class-18 entry and shared palette allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor18_two_entry.h"
#include "../src/willow_native_task_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],original[0x100000];
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



static uint16_t sourceWord(unsigned long at){return (at&0xff0000)==0xff0000?rw(at):willowMotionWord(original+at);}
static uint16_t romWord(uint32_t at){return willowMotionWord(original+at);}
static uint8_t*resolve(uint16_t at){return expected+at;}

static void textWrite(uint32_t at,uint16_t v){if(at<0x910000||at>=0x914000)abort();ww(0x4000+at-0x910000,v);}
static int cpuContext(unsigned a){
 return (a<512&&a%32>=2&&a%32<12)||(a>=0xd4&&a<0xda)||(a>=0xf4&&a<0xfa)||
  (a>=0x200&&a<0x204)||(a>=0x1000&&a<0x2000)||(a>=0xef80&&a<0xf004)||(a>=0xfe80&&a<0xff04);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(original,rom,sizeof original);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 if(willowMotionLong(rom+0x6772)!=0x910010)return 2;
 willowMotionPutLong(rom+0x6772,0xff4010);willowMotionPutLong(rom+0x80,0x15d0);
 for(unsigned n=0;n<47;n++){
  unsigned at=0x7a1e + n*4;uint32_t target=willowMotionLong(rom+at);
  if(target<0x910000||target>=0x914000)return 2;
  willowMotionPutLong(rom+at,target-0x910000+0xff4000);
 }
 for(unsigned n=0;n<5;n++)for(unsigned table=0;table<2;table++){
  unsigned at=(table?0x7c64:0x7c50)+n*4;
  willowMotionPutLong(rom+at,willowMotionLong(rom+at)-0x910000+0xff4000);
 }
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
 willowMotionPutLong(rom+0x88,0x1690);willowMotionPutLong(rom+0xa0,0x1638);
 willowMotionPutLong(rom+0x8c,0x175c);willowMotionPutLong(rom+0x90,0x1788);
 willowMotionPutLong(rom+0x9c,0x160e);willowMotionPutLong(rom+0xa4,0x165c);
 willowMotionPutWord(rom+0x15b4,0x4e75);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor18-two-entrance-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor18-two-entrance-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,dispatches=0,sounds=0,completed=0;
 for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*13+sample*31)^0xa5;
  memset(expected,0,512);WillowNativeTaskRuntime runtime={0};
  willowArcadeTaskInitFree(&bus,romWord);wb(0x20e,0);
  for(unsigned n=0;n<16;n++){ww(n*32+12,0xff);ww(n*32+14,0x1100+n*256);}
  uint16_t actor=0xf490+(sample%8)*128;uint8_t*r=resolve(actor);
  r[0]=1;r[2]=0;r[3]=0;r[30]=18;r[14]=(sample&1)*2;
  willowMotionPutWord(r+6,0x1740);willowMotionPutWord(r+10,0x100);
  ww(0x8408,0x1700);ww(0x840c,0);ww(0x8284,sample*977);wb(0x8286,sample);wb(0x8019,sample/2);
  wb(0x8470,0);wb(0x838b,(sample>>1)&1);wb(0x82a6,0);wb(0x83e9,0);
  wb(0x8290,(sample>>2)&1);wb(0x8293,(sample>>3)&1);wb(0x83c3,(sample>>4)&1);ww(0x801e,(sample*2)&126);
  if(sample&32)willowArcadeTaskStart(&bus,0xc0,0x78f0,52,0x0607,0);
  for(unsigned n=0;n<16;n++)wb(0xf042+n*64,n<sample%17?1:0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  unsigned step;
  for(step=0;step<2048;step++){
   wb(0x8470,step>=3);willowArcadeVisibilityInit(&bus,0,0);
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n))ram[n^1]=expected[n];
   if(!r[2]){
    if(!willowArcadeActor18TwoEntry(&bus,resolve,actor,sourceWord,textWrite))return 1;
    ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
    m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
    m68k_set_reg(0,0);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x27830);
    for(unsigned slice=0;slice<8&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
    if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04)return 1;
    for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n]){
     fprintf(stderr,"Entrance mismatch sample %u step %u at %x actual %x expected %x\n",sample,step,n,ram[n^1],expected[n]);return 1;
    }
   }
   uint16_t oldSound=rw(0x801e),cursor=0,remaining=15;
   for(;;){
    WillowNativeTaskRun run=willowNativeTaskRunOneWithVideo(&runtime,&bus,cursor,remaining,romWord,0,0,0,0,textWrite);
    if(run.unsupported){fprintf(stderr,"Unsupported entrance task %x PC %x\n",run.cursor,runtime.resume[run.cursor/32]);return 1;}
    if(!run.dispatched)break;dispatches++;
    if(run.remaining==65535)break;cursor=run.cursor;remaining=run.remaining;
   }
   sounds+=((rw(0x801e)-oldSound)&126)/4;
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);
   m68k_set_reg(8,0xff0000);m68k_set_reg(0,15);m68k_set_reg(16,0x14d8);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Entrance scheduler mismatch sample %u step %u at %x actual %x expected %x\n",sample,step,n,ram[n^1],expected[n]);return 1;
   }
   willowArcadeTaskWake(&bus);
   ram[0xf000^1]=0;ram[0xf001^1]=0x10;ram[0xf002^1]=0;ram[0xf003^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);m68k_set_reg(16,0x1590);retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n])return 1;
   checks++;
   if(r[2]==2&&!rb(0xc0))break;
  }
  if(step==2048||r[2]!=2||r[3]||rb(0xc0)||rb(0x82a6)||rb(0x83e9)||rw(0x20c)!=8)return 1;
  completed++;
 }
 if(!sounds||completed!=64)return 1;
 printf("Actor18 selector-two entrance: %u actor/scheduler/wake ticks, %u dispatches, %u character sounds, %u completed entrances; original semantic RAM/text and actual message completion match\n",checks,dispatches,sounds,completed);
 return 0;
}
