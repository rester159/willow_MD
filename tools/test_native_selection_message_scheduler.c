/* Fixed message task scheduling versus the original complete scheduler. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_task_scheduler.h"
#include "../src/willow_arcade_tasks.h"
#include "../src/willow_native_boot_runtime.h"
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
static uint16_t source(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t nativeSource(uint32_t at){return willowMotionWord(original+at);}
static uint16_t color(uint32_t a){return rw(0x2000+a-0x91c000);}
static void putColor(uint32_t a,uint16_t v){ww(0x2000+a-0x91c000,v);}
/* CPU context representation differs deliberately: native continuations
 * replace saved SR/PC/USP and CPU stack frames. Semantic state stays checked. */
static int cpuContext(unsigned a){
 return ((a>=0xc0&&a<0x100)&&(a%32)>=20&&(a%32)<26)||
  (a<512&&(a%32)>=2&&(a%32)<12)||(a>=0x0200&&a<0x0204)||
  (a>=0x4000&&a<0x5100)||(a>=0xefe0&&a<0xf004);
}

static uint32_t videoBase;static unsigned videoSize,videoWrites;
static void videoWord(uint32_t at,uint16_t value){
 videoWrites++;
 if(at>=videoBase&&at<videoBase+videoSize)ww((uint16_t)(0x9000+at-videoBase),value);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 unsigned region=0;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(original,rom,sizeof original);

 const unsigned operands[]={0x5876,0x588a,0x589e,0x58bc,0x58c6};
 const uint32_t bases[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 videoBase=bases[region];videoSize=region<3?0x4000:0x800;
 if(willowMotionLong(rom+operands[region])!=videoBase)return 2;
 willowMotionPutLong(rom+operands[region],0xff9000);
 if(region==0){willowMotionPutLong(rom+0x5bd0,0xff9000);willowMotionPutLong(rom+0x5c32,0xff9000);}
 for(unsigned n=0;n<47;n++){
  unsigned at=0x7a1e + n*4;
  uint32_t address=((uint32_t)source(at)<<16)|source(at+2);
  willowMotionPutLong(rom+at,address-0x910000+0xff9000);
 }
 for(unsigned n=0;n<5;n++)for(unsigned table=0;table<2;table++){
  unsigned at=(table?0x7c64:0x7c50)+n*4;
  willowMotionPutLong(rom+at,willowMotionLong(rom+at)-0x910000+0xff9000);
 }
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Stop only at scan restart. Original task bodies/traps remain intact. */
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
 willowMotionPutLong(rom+0x88,0x1690);willowMotionPutLong(rom+0xa0,0x1638);
 willowMotionPutLong(rom+0x8c,0x175c);willowMotionPutLong(rom+0x90,0x1788);
 willowMotionPutLong(rom+0x9c,0x160e);willowMotionPutLong(rom+0xa4,0x165c);
 willowMotionPutWord(rom+0x15b4,0x4e75);
 const unsigned patches[]={0x51ee,0x51fe,0x520e,0x521e,0x522e,0x523e};
 for(unsigned n=0;n<6;n++){
  if(willowMotionLong(rom+patches[n])!=0x91c000+n*1024)return 2;
  willowMotionPutLong(rom+patches[n],0xff2000+n*1024);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/selection-message-scheduler-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/selection-message-scheduler-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,dispatches=0;
 for(unsigned sample=0;sample<64;sample++){
  memset(expected,0,sizeof expected);WillowNativeTaskRuntime runtime={0};WillowNativeBootTask boot={0};videoWrites=0;
  willowArcadeTaskInitFree(&bus,source);
  for(unsigned n=0;n<16;n++){ww(n*32+12,0xff);ww(n*32+14,0x4100+n*256);}
  for(unsigned n=0;n<128;n++)ww(0x8072+n*2,0xffff);
  ww(0x801a,sample*4);ww(0x801c,sample*4);
  unsigned delay=sample%4==0?0:sample%4==1?1:sample%4==2?6:15;
  unsigned message=sample<4?52:(sample/4*5)%77;
  wb(0x8293,sample&8?1:0);wb(0x83c3,sample&8?0:1);wb(0xe58,1);wb(0x82a6,1);
  willowArcadeTaskStart(&bus,0xc0,0x78f0,0xa00|message,((0x10|delay)<<8)|7,0);
  willowArcadeTaskStart(&bus,0xe0,0x78f0,0x200|113,0x262d,0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  unsigned pass;
  for(pass=0;pass<3000&&(rb(0xc0)||rb(0xe0));pass++){
   uint16_t cursor=0,remaining=15;
   for(;;){
    WillowNativeTaskRun run=willowNativeTaskRunWithBootVideo(&runtime,&boot,&bus,cursor,remaining,nativeSource,color,putColor,0,0,videoWord);
    if(run.unsupported){fprintf(stderr,"Unsupported slot %x PC %x\n",run.cursor,runtime.resume[run.cursor/32]);return 1;}
    if(!run.dispatched)break;
    dispatches++;if(run.remaining==65535)break;cursor=run.cursor;remaining=run.remaining;
   }
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);
   m68k_set_reg(8,0xff0000);m68k_set_reg(0,15);m68k_set_reg(16,0x14d8);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Scheduler stalled PC %x\n",m68k_get_reg(16));return 1;}
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Native scheduler mismatch sample %u pass %u address %04x got %02x want %02x\n",sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   checks++;
   if(!delay&&pass==256)break;
   willowArcadeTaskWake(&bus);
   ram[0xf000^1]=0;ram[0xf001^1]=0x10;ram[0xf002^1]=0;ram[0xf003^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);m68k_set_reg(16,0x1590);retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n])return 1;
  }

  if(pass==3000||rw(0x020c)!=8||!videoWrites||(!delay&&(rb(0xc0)!=2||rb(0xe0)!=0))){fprintf(stderr,"Text tasks did not finish\n");return 1;}
 }
 printf("Selection message scheduler: %u scan/wake boundaries, %u dispatches; concurrent messages preserve source RAM, delay-zero suspension, completion flags and free pool\n",checks,dispatches);
 return 0;
}
