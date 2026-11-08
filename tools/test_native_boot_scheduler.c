/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "boot_scheduler_fixture.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_task_scheduler.h"
#include "../src/willow_arcade_tasks.h"
#include "../src/willow_native_boot_runtime.h"
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
static uint16_t source(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t color(uint32_t a){return rw(0x2000+a-0x91c000);}
static void putColor(uint32_t a,uint16_t v){ww(0x2000+a-0x91c000,v);}
/* CPU context representation differs deliberately: native continuations
 * replace saved SR/PC/USP and CPU stack frames. Semantic state stays checked. */
static int cpuContext(unsigned a){
 return (a>=2&&a<12)||(a>=0x0200&&a<0x0204)||
  (a>=0x4000&&a<0x4100)||(a>=0xefe0&&a<0xf004);
}
static void packFixture(uint8_t*out,const WillowNativeTaskRuntime*r)
{
 memcpy(out,expected+0x0200,16);memcpy(out+16,expected+0x0a10,32);
 memcpy(out+48,expected+0x801a,4);
 for(unsigned n=0;n<16;n++)willowMotionPutWord(out+52+n*2,r->resume[n]);
 memcpy(out+84,expected,512);memcpy(out+596,expected+0x8072,256);
 for(unsigned n=0;n<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];n++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+n;
  memcpy(out+852+g->offset,expected+g->source,g->count);
 }
}
static int exportFixture(FILE*f,uint8_t flags,const uint8_t*before,const WillowNativeTaskRuntime*r)
{
 if(!f)return 1;
 uint8_t after[BOOT_SCHEDULER_FIXTURE_BYTES];packFixture(after,r);
 return fwrite(&flags,1,1,f)==1 && fwrite(before,1,sizeof after,f)==sizeof after &&
  fwrite(after,1,sizeof after,f)==sizeof after;
}
int main(int argc,char**argv){
 if(argc!=2 && (argc!=3||strcmp(argv[2],"--native-fixtures")))return 2;
 FILE*fixtures=argc==3?fopen("build/boot-continuation-original.bin","wb"):NULL;
 if(argc==3&&!fixtures)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Stop only at scan restart. Original task bodies/traps remain intact. */
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
 willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x84,0x1638);
 willowMotionPutLong(rom+0x8c,0x175c);willowMotionPutLong(rom+0x90,0x1788);
 willowMotionPutLong(rom+0x9c,0x160e);willowMotionPutLong(rom+0xa4,0x165c);
 willowMotionPutWord(rom+0x15b4,0x4e75);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/boot-scheduler-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/boot-scheduler-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0,dispatches=0,finishes=0;
 for(unsigned state=0;state<6;state++)for(unsigned sample=0;sample<256;sample++){
  memset(expected,0,sizeof expected);
  WillowNativeTaskRuntime runtime={0};WillowNativeBootTask boot={0};
  willowArcadeTaskInitFree(&bus,source);
  for(unsigned n=0;n<16;n++){ww(n*32+12,0xff);ww(n*32+14,0x4100+n*256);}
  ww(0x8000,state*2);ww(0x8002,sample%3);ww(0x801a,(sample&63)*4);
  wb(0x8066,sample);wb(0x8067,sample*73);wb(0x8068,sample*151);
  wb(0x8058,(sample&1)?0x40:0);wb(0x828f,(sample&2)?128:0);
  wb(0x20,(sample&4)?0:12);wb(0x80,(sample&8)?0:12);
  wb(0x82a2,sample);wb(0x82a6,1);
  willowArcadeTaskStart(&bus,0,0x7fda,sample*977,sample*317,1);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  for(unsigned pass=0;pass<24;pass++){
   if(pass%4==3){wb(0xf891,0);ram[0xf891^1]=0;wb(0x82a6,0);ram[0x82a6^1]=0;}
   uint8_t packedBefore[BOOT_SCHEDULER_FIXTURE_BYTES];packFixture(packedBefore,&runtime);
   uint8_t fixtureFlags=(pass==0?1:0)|(pass%4==3?2:0);
   WillowNativeTaskRun run=fixtures?
    willowNativeTaskRunWithStoredBootVideo(&runtime,&bus,0,0,source,color,putColor,0,0,0):
    willowNativeTaskRunWithBoot(&runtime,&boot,&bus,0,0,source,color,putColor,0,0);
   if(run.unsupported||!run.dispatched){fprintf(stderr,"Boot dispatch failed state %u sample %u pass %u\n",state,sample,pass);return 1;}
   dispatches++;
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);
   m68k_set_reg(8,0xff0000);m68k_set_reg(0,0);m68k_set_reg(16,0x14d8);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Boot scheduler stalled PC %x\n",m68k_get_reg(16));return 1;}
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Boot scheduler mismatch state %u sample %u pass %u address %x got %x expected %x\n",state,sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   checks++;
   if(!rb(0)){finishes++;if(rw(0x020c)!=8||runtime.resume[0]||boot.pc)return 1;
    if(!exportFixture(fixtures,fixtureFlags|4,packedBefore,&runtime))return 2;break;}
   willowArcadeTaskWake(&bus);
   ram[0xf000^1]=0;ram[0xf001^1]=0x10;ram[0xf002^1]=0;ram[0xf003^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);m68k_set_reg(16,0x1590);retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n])return 1;
   if(!exportFixture(fixtures,fixtureFlags,packedBefore,&runtime))return 2;
  }
 }
 if(fixtures){if(fclose(fixtures))return 2;printf("Exported %u stored-context scheduler/wake fixtures\n",checks);}
 for(unsigned stored=0;stored<2;stored++){
  static uint8_t before[65536];memset(expected,0,sizeof expected);
  WillowNativeTaskRuntime runtime={0};runtime.resume[0]=0x7ff2;
  WillowNativeBootTask boot={0x7ff2,0,3,4},savedBoot=boot;
  WillowNativeTaskRuntime savedRuntime=runtime;
  if(stored){ww(2,boot.waitReturn);ww(4,boot.d1);ww(8,boot.d2);}
  wb(0,4);ww(0x8000,11);memcpy(before,expected,sizeof before);
  WillowNativeTaskRun run=(stored?willowNativeTaskRunWithStoredBootVideo(&runtime,&bus,0,0,source,color,putColor,0,0,0):willowNativeTaskRunWithBoot(&runtime,&boot,&bus,0,0,source,color,putColor,0,0));
  if(!run.unsupported||run.dispatched||memcmp(before,expected,sizeof before)||memcmp(&runtime,&savedRuntime,sizeof runtime)||memcmp(&boot,&savedBoot,sizeof boot))return 1;
  wb(0x020e,255);memcpy(before,expected,sizeof before);
  run=(stored?willowNativeTaskRunWithStoredBootVideo(&runtime,&bus,0,0,source,color,putColor,0,0,0):willowNativeTaskRunWithBoot(&runtime,&boot,&bus,0,0,source,color,putColor,0,0));
  if(run.dispatched||run.unsupported||memcmp(before,expected,sizeof before))return 1;
  wb(0x020e,0);willowArcadeTaskStart(&bus,0,0x1ddc,0,0,1);
  run=(stored?willowNativeTaskRunWithStoredBootVideo(&runtime,&bus,0,0,source,color,putColor,0,0,0):willowNativeTaskRunWithBoot(&runtime,&boot,&bus,0,0,source,color,putColor,0,0));
  if(!run.dispatched||run.unsupported||runtime.resume[0]!=0x1e18||memcmp(&boot,&savedBoot,sizeof boot))return 1;
 }
 printf("Boot scheduler: %u scan boundaries, %u dispatches, %u fixed-task terminations; source scheduler, register save/restore, sleep/wake, handoff parameters and non-context RAM match; invalid-state preservation, inhibit and consumer delegation pass for separate and stored boot contexts\n",checks,dispatches,finishes);
 return 0;
}
