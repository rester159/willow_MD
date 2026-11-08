/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_palette_task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
static uint16_t sourceWord(uint32_t address){return willowMotionWord(rom+address);}
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
static uint16_t colorRead(uint32_t address){return rw(0x2000+address-0x91c000);}
static void colorWrite(uint32_t address,uint16_t value){ww(0x2000+address-0x91c000,value);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Stop precisely on entry to the scheduler sleep service. Preserve its
  * JSR return address for checking; sleep/context restoration is separately
  * verified by the task-continuation oracle, not executed in this fixture. */
 willowMotionPutWord(rom+0x1750,0x4ef9);
 willowMotionPutLong(rom+0x1752,0x100000);
 /* Preserve the finish body outside the overlapping 165A stop patch.
  * Its sole PC-relative instruction is the final scheduler branch. */
 memcpy(rom+0x100100,rom+0x165e,0x1688-0x165e);
 willowMotionPutWord(rom+0x10012a,0x4ef9);willowMotionPutLong(rom+0x10012c,0x151e);
 willowMotionPutWord(rom+0x165a,0x4ef9);
 willowMotionPutLong(rom+0x165c,0x100000);
 willowMotionPutLong(rom+0x9c,0x160e);
 willowMotionPutWord(rom+0x1784,0x4ef9);willowMotionPutLong(rom+0x1786,0x100000);
 willowMotionPutWord(rom+0x151e,0x4e75);
 const unsigned patches[]={0x51ee,0x51fe,0x520e,0x521e,0x522e,0x523e};
 for(unsigned n=0;n<6;n++){
  if(willowMotionLong(rom+patches[n])!=0x91c000+n*1024)return 2;
  willowMotionPutLong(rom+patches[n],0xff2000+n*1024);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/transition-sequence-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/transition-sequence-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at)); \
 for(unsigned slice=0;slice<4&&m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Unexpected PC %x\n",m68k_get_reg(16));return 1;} \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};

#define MATCH() do { \
 for(unsigned n=0;n<65536;n++)if(n<0xfe80||n>=0xff04){ \
  if(ram[n^1]!=expected[n]){fprintf(stderr,"Sequence mismatch sample %u boundary %u address %04x got %02x want %02x\n",sample,checks,n,ram[n^1],expected[n]);return 1;} \
 } checks++; \
}while(0)
#define CURRENT(slot) do{ww(0x0204,0xff);ww(0x0206,slot);putRamWord(ram,0x0204,0xff);putRamWord(ram,0x0206,slot);m68k_set_reg(14,0xff0000+(slot));}while(0)
#define STEP_CHECK(step) do{ \
 unsigned sp=m68k_get_reg(15)&65535; \
 if(sp!=((step).finished?0xff00:0xfefc))return 1; \
 if(!(step).finished){ \
  if((m68k_get_reg(0)&65535)!=(step).delay)return 1; \
  unsigned resume=((unsigned)ram[0xfefe^1]<<8)|ram[0xfeff^1]; \
  if(resume!=(step).resume)return 1; \
 } \
}while(0)
 unsigned checks=0,sequences=0;
 for(unsigned sample=0;sample<128;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  ww(0x801a,(sample%64)*4);ww(0x801c,(sample%64)*4);
  ww(0x0208,0xff);ww(0x020a,0x3800);ww(0x020c,8);wb(0x020e,0);
  wb(0xf99c,0);wb(0x828f,0);
  for(unsigned n=0;n<8;n++){ww(0x3800+n*4,0xff);ww(0x3802+n*4,0x100+n*32);}
  for(unsigned n=0;n<128;n++)ww(0x8072+n*2,0xffff);
  ww(0x82c0,sample&1);
  for(unsigned bank=0;bank<6;bank++){
   ww(0x82a8+bank*4,bank<4?0xffff:0);ww(0x82aa+bank*4,bank<4?0xffff:255);
  }
  for(unsigned n=0;n<3072;n++)ww(0x2000+n*2,(sample%16)*4096+((n*37)&4095));
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(willowArcadeEventCommandWaitBegin(&bus,0x1eaa,0)!=0x1ec4)return 1;
  RUN(0x1eaa,0);MATCH();
  if(!willowArcadeCommandConsumerResume(&bus,sourceWord))return 1;
  RUN(0x1e18,0);MATCH();
  if(rw(0x0106)!=0x582c||rw(0x0110)!=0x10||rw(0x0112)!=3)return 1;
  CURRENT(0x100);
  WillowTransitionStep reveal=willowArcadeTransitionReveal(&bus,0x597e,0x100);
  RUN(0x582c,0);STEP_CHECK(reveal);MATCH();
  if(!willowArcadeCommandConsumerResume(&bus,sourceWord))return 1;
  RUN(0x1e18,0);MATCH();
  if(rw(0x0126)!=0x509c||rw(0x0130)!=2||rw(0x0132)!=3)return 1;
  uint16_t pc=0x5102;
  unsigned iterations=0;
  for(;;){
   CURRENT(0x120);
   WillowTransitionStep fade=willowArcadePaletteTask(&bus,pc,0x120,colorRead,colorWrite);
   RUN(pc==0x5102?0x509c:pc,0);STEP_CHECK(fade);MATCH();
   if(fade.finished){
    willowArcadeTaskFinish(&bus,1);RUN(0x100100,0);MATCH();break;
   }
   pc=fade.resume;
   CURRENT(0x100);
   reveal=willowArcadeTransitionReveal(&bus,0x5994,0x100);
   RUN(0x5994,0);STEP_CHECK(reveal);MATCH();
   if(reveal.resume!=0x5994)return 1;
   if(++iterations>17)return 1;
  }
  CURRENT(0x100);
  reveal=willowArcadeTransitionReveal(&bus,0x5994,0x100);
  RUN(0x5994,0);STEP_CHECK(reveal);MATCH();
  if(reveal.resume!=0x59a4||reveal.delay!=3||!rb(0xf891))return 1;
  reveal=willowArcadeTransitionReveal(&bus,0x59a4,0x100);
  RUN(0x59a4,0);STEP_CHECK(reveal);MATCH();
  willowArcadeTaskFinish(&bus,1);RUN(0x100100,0);MATCH();
  if(willowArcadeEventCommandWaitPending(&bus))return 1;
  RUN(0x1ec4,0);MATCH();
  if(rw(0x020c)!=8||rw(0x82c0)!=0)return 1;
  sequences++;
 }
 printf("Source reveal composition: %u sequences, %u boundaries; outer wait, two queue allocations, fade, final delay, flag clear and both task releases match\n",sequences,checks);
 return 0;
}
