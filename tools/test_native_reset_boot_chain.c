/* Differential verification of reset-initialized boot progression against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_task_scheduler.h"
#include "../src/willow_arcade_tasks.h"
#include "../src/willow_native_boot_runtime.h"
#include "../src/willow_arcade_reset_ram.h"
#include "../src/willow_arcade_startup_latched.h"
#include "../src/willow_native_boot_globals.h"
#include "../src/willow_native_high_scores.h"
#include "../src/willow_native_demo_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "demo_state_lifetime_audit.h"
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
static WillowNativeBootGlobals retainedGlobals;
static WillowNativeHighScores retainedScores;
static WillowNativeDemoState retainedDemo;
static uint8_t retainedSound[128],retainedCurrentScore[4];
static uint8_t*retainedByte(uint16_t a){
 uint8_t*demo=willowNativeDemoStateResolve(&retainedDemo,a,1);if(demo)return demo;
 if(a>=0x8172&&a<0x81f2)return retainedSound+(a-0x8172);
 if(a>=0xefc6&&a<0xf016)return retainedScores.records+(a-0xefc6);
 if(a>=0xf03e&&a<0xf042)return retainedCurrentScore+(a-0xf03e);
 return willowNativeBootGlobalsResolve(&retainedGlobals,a,1);
}
/* Byte footprint of actual translated work-RAM accesses. Hardware palette
 * and video callbacks use their own address spaces, never this audit. */
static uint8_t accessFlags[65536],accessPhase;
static void recordAccess(uint16_t a,unsigned count,uint8_t write){
 if(!accessPhase)return;
 uint8_t flag=(write?2:1)<<(accessPhase==2?2:0);
 for(unsigned n=0;n<count;n++)accessFlags[(uint16_t)(a+n)]|=flag;
}
static uint8_t rb(uint16_t a){
 recordAccess(a,1,0);
 if(a>=0xf016&&a<0xf03e)return willowHighScorePointerRead(&retainedScores,a-0xf016);
 uint8_t*p=retainedByte(a);
 return p?*p:expected[a];
}
static uint16_t rw(uint16_t a){return ((uint16_t)rb(a)<<8)|rb(a+1);}
static void wb(uint16_t a,uint8_t v){
 recordAccess(a,1,1);
 if(a>=0xf016&&a<0xf03e){
  if(!willowHighScorePointerWrite(&retainedScores,a-0xf016,v)){
   fprintf(stderr,"Unsupported retained high-score pointer write %04x=%02x\n",a,v);exit(1);
  }
 }
 uint8_t*p=retainedByte(a);
 if(p)*p=v;expected[a]=v;
}
static void ww(uint16_t a,uint16_t v){wb(a,v>>8);wb(a+1,v);}
static uint16_t source(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t color(uint32_t a){return willowMotionWord(expected+0x2000+a-0x91c000);}
static void putColor(uint32_t a,uint16_t v){willowMotionPutWord(expected+0x2000+a-0x91c000,v);}
/* CPU context representation differs deliberately: native continuations
 * replace saved SR/PC/USP and CPU stack frames. Semantic state stays checked. */
static int cpuContext(unsigned a){
 return (a<512&&(a%32)>=2&&(a%32)<12)||(a>=0x0200&&a<0x0204)||
  (a>=0x0210&&a<0x0a10)||(a>=0xefe0&&a<0xf004);
}

static uint32_t videoBase;static unsigned videoSize,videoWrites,videoFills;
static void videoWord(uint32_t at,uint16_t value){
 videoWrites++;
 if(at>=videoBase&&at<videoBase+videoSize)willowMotionPutWord(expected+0x9000+at-videoBase,value);
}
/* Synchronous repeated-pair publication. Compare every retained result with
 * original word writes; no task state is advanced until the call returns. */
static void videoFill(uint32_t at,uint16_t first,uint16_t second,uint16_t pairs){
 videoFills++;videoWrites+=(unsigned)pairs*2;
 for(unsigned n=0;n<pairs;n++){
  uint32_t a=at+n*4;
  if(a>=videoBase&&a+3<videoBase+videoSize){
   willowMotionPutWord(expected+0x9000+a-videoBase,first);
   willowMotionPutWord(expected+0x9002+a-videoBase,second);
  }
 }
}
int main(int argc,char**argv){
 if(argc!=3)return 2;
 unsigned region=(unsigned)strtoul(argv[2],0,10);if(region>4)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);

 const unsigned operands[]={0x5876,0x588a,0x589e,0x58bc,0x58c6};
 const uint32_t bases[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 videoBase=bases[region];videoSize=region<3?0x4000:0x800;
 if(willowMotionLong(rom+operands[region])!=videoBase)return 2;
 willowMotionPutLong(rom+operands[region],0xff9000);
 if(region==0){willowMotionPutLong(rom+0x5bd0,0xff9000);willowMotionPutLong(rom+0x5c32,0xff9000);}
 const unsigned sceneOperands[]={0x65a6,0x65ba,0x65ce,0x65ec,0x65f6};
 willowMotionPutLong(rom+sceneOperands[region],0xff9000);
 const unsigned initOperands[]={0xae4,0xb06,0xaf6,0xb16};
 for(unsigned n=0;n<4;n++){
  uint32_t at=willowMotionLong(rom+initOperands[n]);
  willowMotionPutLong(rom+initOperands[n],at-0x91c000+0xff2000);
 }
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Stop only at scan restart. Original task bodies/traps remain intact. */
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
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
 f=fopen("build/reset-boot-chain-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/reset-boot-chain-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,dispatches=0;
 for(unsigned sample=0;sample<64;sample++){
  memset(expected,0,sizeof expected);WillowNativeTaskRuntime runtime={0};videoWrites=0;
  willowArcadeResetWorkRAM(&bus);
  accessPhase=1;willowArcadeStartupPrimaryQueue(&bus);accessPhase=0;
  /* Inputs are externally latched; leave service-mode bit clear. */
  wb(0x8066,sample*13);wb(0x8067,sample*73);wb(0x8068,(sample*11)&127);
  accessPhase=1;willowArcadeStartupLatched(&bus,source,0xffff8000UL,sample*977);accessPhase=0;
  /* 76C initializes six banks full-bright; 08FE then replaces the first
   * four with the unmodified ROM words. Neither service changes D1/D2. */
  const uint32_t paletteSources[]={0xe5080,0xe6ea0,0xe7d20,0xeab00,0xe6080,0xe64a0};
  for(unsigned bank=0;bank<6;bank++)for(unsigned n=0;n<512;n++)
   putColor(0x91c000+bank*1024+n*2,source(paletteSources[bank]+n*2)|(bank>=4?0xf000:0));
  uint8_t demoBefore[321];demoLifetimeCapture(demoBefore,expected);
  for(unsigned n=0;n<321;n++)if(demoBefore[n]||retainedDemo.bytes[n])return 1;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  unsigned pass;
  for(pass=0;pass<1024&&(rb(0)||rw(0x8000)!=4);pass++){
   uint16_t cursor=0,remaining=15;
   accessPhase=2;
   for(;;){
    WillowNativeTaskRun run=willowNativeTaskRunWithStoredBootVideoFill(&runtime,&bus,cursor,remaining,source,color,putColor,0,0,videoWord,videoFill);
    if(run.unsupported){fprintf(stderr,"Unsupported slot %x PC %x\n",run.cursor,runtime.resume[run.cursor/32]);return 1;}
    if(!run.dispatched)break;
    dispatches++;if(run.remaining==65535)break;cursor=run.cursor;remaining=run.remaining;
   }
   accessPhase=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xffff8000UL);
   m68k_set_reg(8,0xff0000);m68k_set_reg(0,15);m68k_set_reg(16,0x14d8);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Scheduler stalled PC %x\n",m68k_get_reg(16));return 1;}
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&(ram[n^1]!=expected[n]||rb(n)!=expected[n])){
    fprintf(stderr,"Native scheduler mismatch sample %u pass %u address %04x got %02x want %02x\n",sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   if(!demoLifetimePreserved(demoBefore,ram))return 1;
   checks++;
   accessPhase=2;willowArcadeTaskWake(&bus);accessPhase=0;
   ram[0xf000^1]=0;ram[0xf001^1]=0x10;ram[0xf002^1]=0;ram[0xf003^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xffff8000UL);m68k_set_reg(16,0x1590);retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&(ram[n^1]!=expected[n]||rb(n)!=expected[n]))return 1;
  }

  if(pass==1024||rb(0)||rw(0x8000)!=4||rb(0x82a6)||videoWrites<=106496){fprintf(stderr,"Boot attract missing sample %u pass %u state %u boot %u flag %u writes %u\n",sample,pass,rw(0x8000),rb(0),rb(0x82a6),videoWrites);fprintf(stderr,"phase %u sum %x:%x free %u\n",rw(0x8004),rw(0xa3e),rw(0xa40),rw(0x20c));for(unsigned i=0;i<16;i++)fprintf(stderr,"slot %x state %x pc %x\n",i*32,rw(i*32),runtime.resume[i]);return 1;}
 }
 if(!videoFills)return 1;
 printf("Stored boot synchronous clear routing: %u completed fill calls\n",videoFills);
 char footprintPath[96];snprintf(footprintPath,sizeof footprintPath,"build/reset-boot-access-%u.csv",region);
 FILE*audit=fopen(footprintPath,"w");if(!audit)return 2;
 fputs("address,startup_read,startup_write,runtime_read,runtime_write\n",audit);
 for(unsigned at=0;at<65536;at++)if(accessFlags[at])
  fprintf(audit,"%04x,%u,%u,%u,%u\n",at,!!(accessFlags[at]&1),!!(accessFlags[at]&2),!!(accessFlags[at]&4),!!(accessFlags[at]&8));
 if(fclose(audit))return 2;
 printf("Reset-initialized boot-to-attract region %u: %u scan/wake boundaries, %u dispatches; boot and attract scheduler reach manager 4 through real command completion, palettes and scene clears without injected flags\n",region,checks,dispatches);
 return 0;
}
