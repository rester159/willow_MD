/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
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

static void putRamWord(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
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
 /* Keep both original exception handlers unchanged. Stop only at their
  * shared scheduler-context restoration boundary. The fixture supplies an
  * actual exception-frame layout and initializes USP using MOVE to USP. */
 willowMotionPutWord(rom+0x151e,0x4e75);
 willowMotionPutWord(rom+0x15b4,0x4e75); /* wake scan epilogue */
 /* 100800: move a1,usp; jmp (a2). A1/A2 are explicit fixture inputs. */
 willowMotionPutWord(rom+0x100800,0x4e61);
 willowMotionPutWord(rom+0x100802,0x4ed2);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/task-continuation-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/task-continuation-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,wakeChecks=0;
 for(unsigned yield=0;yield<2;yield++)for(unsigned sample=0;sample<(yield?4096:65536);sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  uint16_t slot=(sample&15)*32;
  uint16_t d0=sample,sr=sample*977;
  uint32_t pc=0x12340000UL+sample*17,usp=0xff0000UL+(uint16_t)(sample*73);
  ww(0x0204,0x00ff);ww(0x0206,slot);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(yield)willowArcadeTaskYield(&bus,sr,pc,usp);
  else willowArcadeTaskSleep(&bus,d0,sr,pc,usp);
  putRamWord(ram,0xff00,sr);putRamWord(ram,0xff02,pc>>16);putRamWord(ram,0xff04,pc);
  putRamWord(ram,0xff06,0x0010);putRamWord(ram,0xff08,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(0,d0);m68k_set_reg(9,usp);m68k_set_reg(10,yield?0x1788:0x175c);
  m68k_set_reg(16,0x100800);retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Continuation fixture did not return\n");return 1;}
  for(unsigned n=0;n<65536;n++)if(n<0xff00||n>=0xff0a){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Task continuation mismatch yield %u sample %u address %04x got %02x want %02x\n",yield,sample,n,ram[n^1],expected[n]);return 1;}
  }
  checks++;
  if(!yield&&(sample==0||sample==1||sample==2||sample==3||sample==16||sample==255)){
   for(unsigned tick=0;tick<=sample+1;tick++){
    willowArcadeTaskWake(&bus);
    putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
    m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
    m68k_set_reg(16,0x1590);retro_run();
    if(m68k_get_reg(16)!=0x100000)return 1;
    for(unsigned n=0;n<65536;n++)if(n<0xff00||n>=0xff0a){
     if(ram[n^1]!=expected[n]){fprintf(stderr,"Sleep/wake mismatch delay %u tick %u address %04x\n",sample,tick,n);return 1;}
    }
    wakeChecks++;
   }
  }
 }
 printf("Source sleep/wake composition: %u consecutive wake scans, zero-delay suspension and short/long countdown completion\n",wakeChecks);
 printf("Source task continuations: %u sleep/yield cases, exhaustive D0 word values for sleep; original handler writes and all non-stack RAM match\n",checks);
 return 0;
}
