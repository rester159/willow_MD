/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_layer_clear_task.h"
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

static uint32_t videoBase;
static unsigned videoSize,videoWrites;
static void videoWord(uint32_t at,uint16_t value){
 videoWrites++;
 if(at>=videoBase&&at<videoBase+videoSize)ww((uint16_t)(0x2000+at-videoBase),value);
}
int main(int argc,char**argv){
 if(argc!=3)return 2;
 unsigned region=(unsigned)strtoul(argv[2],0,10);if(region>4)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);

 const unsigned operands[]={0x5a14,0x5a50,0x5a90,0x59d8,0x59e2};
 const uint32_t bases[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 videoBase=bases[region];videoSize=region<3?0x4000:0x800;
 if(willowMotionLong(rom+operands[region])!=videoBase)return 2;
 /* Remap one complete CPS region per run; other CPS writes remain outside
  * Genesis RAM. All five runs together check every cleared destination. */
 willowMotionPutLong(rom+operands[region],0xff2000);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Stop precisely on entry to the scheduler sleep service. Preserve its
  * JSR return address for checking; sleep/context restoration is separately
  * verified by the task-continuation oracle, not executed in this fixture. */
 willowMotionPutWord(rom+0x1750,0x4ef9);
 willowMotionPutLong(rom+0x1752,0x100000);
 willowMotionPutWord(rom+0x165a,0x4ef9);
 willowMotionPutLong(rom+0x165c,0x100000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/layer-clear-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/layer-clear-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at)); \
 for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 const uint16_t entries[]={0x59ac,0x59f2,0x5a2e,0x5a6e};
 for(unsigned bank=0;bank<4;bank++)for(unsigned argument=0;argument<256;argument++){
  for(unsigned n=0;n<65536;n++)expected[n]=(argument*31+n*7)^0xa5;
  uint16_t slot=0x100+(argument%8)*32,mask=0x82a8+bank*4;
  ww(0x0204,0xff);ww(0x0206,slot);ww(slot+16,0x12+bank*2);ww(slot+18,0xa500|argument);
  ww(0x801a,(argument%64)*4);wb(0xf891,argument);wb(0x83f7,argument);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t pc=entries[bank];
  for(unsigned phase=0;phase<4;phase++){
   uint32_t value=phase==3?0xffffffffUL:phase==2?0xffff0000UL:0x0000ffffUL;
   ww(mask,value>>16);ww(mask+2,value);putRamWord(ram,mask,value>>16);putRamWord(ram,mask+2,value);
   videoWrites=0;
   WillowTransitionStep step=willowArcadeLayerClearTask(&bus,pc,slot,videoWord,0);
   if(!step.supported||step.finished!=(phase==3)||videoWrites!=(phase==3?(bank?8192u:2048u):0u))return 1;
   RUN(phase?pc:0x582c,0xdeadbeef);
   unsigned sp=m68k_get_reg(15)&65535;
   if(sp!=(step.finished?0xff00:0xfefc))return 1;
   if(!step.finished){
    if((m68k_get_reg(0)&65535)!=step.delay)return 1;
    unsigned resume=((unsigned)ram[0xfefc^1]<<24)|((unsigned)ram[0xfefd^1]<<16)|((unsigned)ram[0xfefe^1]<<8)|ram[0xfeff^1];
    if(resume!=step.resume)return 1;
   }
   for(unsigned n=0;n<65536;n++)if(n<0xfed0||n>=0xff04){
    if(ram[n^1]!=expected[n]){fprintf(stderr,"Layer clear mismatch region %u bank %u argument %u phase %u address %04x\n",region,bank,argument,phase,n);return 1;}
   }
   pc=step.resume;checks++;
  }
 }
 printf("Source layer clear region %u: %u boundaries, all four selectors and all delay/flag bytes; queue, partial-mask waits, video and non-stack RAM match\n",region,checks);
 return 0;
}
