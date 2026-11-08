/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_task_scheduler.h"
#include "../src/willow_arcade_tasks.h"
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
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Only stop the next scan restart. Dispatch/return and traps stay real. */
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
 willowMotionPutLong(rom+0x90,0x1788);willowMotionPutLong(rom+0x84,0x1638);
 for(unsigned n=0;n<16;n++){
  unsigned entry=0x100800+n*16;
  willowMotionPutWord(rom+entry,0x4eb8);willowMotionPutWord(rom+entry+2,0x1784);
  willowMotionPutWord(rom+entry+4,0x4ef8);willowMotionPutWord(rom+entry+6,0x1636);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scheduler-pass-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scheduler-pass-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,dispatches=0;
 for(unsigned sample=0;sample<1024;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  wb(0x020e,0);
  for(unsigned n=0;n<16;n++){
   uint16_t slot=n*32;uint32_t entry=0x100800+n*16;
   wb(slot,((sample>>(n%10))&1)?12:2);wb(slot+1,sample+n);
   ww(slot+4,entry>>16);ww(slot+6,entry);
   ww(slot+12,0xff);ww(slot+14,0x4100+n*256);
  }
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  for(unsigned pass=0;pass<2;pass++){
   uint16_t cursor=0,remaining=15;
   for(;;){
    WillowTaskSelection select=willowArcadeTaskSelect(&bus,cursor,remaining);
    if(!select.dispatch)break;
    uint32_t saved=willowArcadeSchedulerSave(&bus,0xfff000,0xff0000+select.slot,select.remaining);
    WillowTaskContext context=willowArcadeTaskContext(&bus,select.slot);
    if(!context.supported)return 1;
    if(context.fresh){
     uint32_t returnPC=context.pc+4,stack=context.stack-4;
     ww((uint16_t)stack,returnPC>>16);ww((uint16_t)(stack+2),returnPC);
     /* The unchanged TRAP writes this supervisor exception frame. */
     ww((uint16_t)(saved-6),0);ww((uint16_t)(saved-4),0);ww((uint16_t)(saved-2),0x1786);
     willowArcadeTaskYield(&bus,0,0x1786,stack);
    }else{
     ww((uint16_t)(saved-6),context.sr);ww((uint16_t)(saved-4),0);ww((uint16_t)(saved-2),0x1638);
     willowArcadeTaskFinish(&bus,0);
    }
    WillowSchedulerReturn result=willowArcadeSchedulerRestore(&bus);
    cursor=(uint16_t)result.slot+32;remaining=result.remaining-1;dispatches++;
    if(result.remaining==0)break;
   }
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);
   m68k_set_reg(8,0xff0000);m68k_set_reg(0,15);m68k_set_reg(16,0x14d8);
   for(unsigned slice=0;slice<4&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000||m68k_get_reg(15)!=0xfff000)return 1;
   for(unsigned n=0;n<65536;n++)if(ram[n^1]!=expected[n]){
    fprintf(stderr,"Scheduler pass mismatch sample %u pass %u address %04x got %02x want %02x\n",sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   checks++;
  }
 }
 printf("Source scheduler composition: %u passes, %u task dispatches; ascending scan, fresh entry, yield, resumed return, fixed task finish and every RAM byte match\n",checks,dispatches);
 return 0;
}
