/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_frame_clock.h"
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
int main(int argc,char**argv){
 if(argc!=2 && (argc!=3 || strcmp(argv[2],"--native-fixtures")))return 2;
 FILE*fixtures=argc==3?fopen("build/source-clock-original.bin","wb"):NULL;
 if(argc==3&&!fixtures)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Original branch/increment prefix, unchanged. Observe which exit is
  * taken instead of executing the downstream game task. */
 memcpy(rom+0x100100,rom+0x92dc,0x12);
 willowMotionPutWord(rom+0x100112,0x4ef9);
 willowMotionPutLong(rom+0x100114,0x100200);
 willowMotionPutWord(rom+0x10011a,0x4e75);
 willowMotionPutWord(rom+0x100200,0x13fc);
 willowMotionPutWord(rom+0x100202,1);
 willowMotionPutLong(rom+0x100204,0xff4000);
 willowMotionPutWord(rom+0x100208,0x4e75);
 /* Periodic condition's original prefix; exits instrumented as above. */
 memcpy(rom+0x100300,rom+0x325a4,12);
 willowMotionPutWord(rom+0x10030c,0x6000);
 willowMotionPutWord(rom+0x10030e,0xfef2); /* BRA 100200 */
 willowMotionPutWord(rom+0x100310,0x4e75);
 memcpy(rom+0x100400,rom+0x1584,4);
 willowMotionPutWord(rom+0x100404,0x4e75);

 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/frame-clock-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/frame-clock-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 unsigned checks=0,periodic=0;
 const uint8_t gates[]={0,1,0x80,0xff},inputs[]={0,1,2,3,0x80,0xfd,0xfe,0xff};
 for(unsigned phase=0;phase<256;phase++)for(unsigned g=0;g<4;g++)for(unsigned i=0;i<8;i++){
  memset(ram,0,65536);
  WillowArcadeFrameClock clock={(uint8_t)(phase^0xa5),(uint8_t)phase};
  ram[0x8018^1]=clock.interrupt;ram[0x8019^1]=clock.simulation;
  ram[0x828f^1]=gates[g];ram[0x8068^1]=inputs[i];
  uint8_t scheduled=willowArcadeFrameStep(&clock,gates[g],inputs[i]);
  RUN(0x100100,0xdeadbeef);
  if(ram[0x4000^1]!=scheduled || ram[0x8019^1]!=clock.simulation || ram[0x8018^1]!=clock.interrupt)return 1;
  if(ram[0x828f^1]!=gates[g] || ram[0x8068^1]!=inputs[i])return 1;
  willowArcadeFrameInterrupt(&clock);RUN(0x100400,0);
  if(ram[0x8018^1]!=clock.interrupt || ram[0x8019^1]!=clock.simulation)return 1;
  if(fixtures){
   const uint8_t fixture[7]={(uint8_t)(phase^0xa5),(uint8_t)phase,gates[g],inputs[i],
    ram[0x8018^1],ram[0x8019^1],ram[0x4000^1]};
   if(fwrite(fixture,1,sizeof fixture,fixtures)!=sizeof fixture)return 2;
  }
  checks++;
 }
 if(fixtures&&fclose(fixtures))return 2;
 for(unsigned phase=0;phase<256;phase++)for(unsigned slot=0;slot<60;slot++){
  ram[0x8019^1]=phase;ram[0x4000^1]=0;
  m68k_set_reg(7,slot);RUN(0x100300,0xdeadbeef);
  if(ram[0x4000^1]!=willowArcadeActorPeriodic(phase,slot) || ram[0x8019^1]!=phase || m68k_get_reg(7)!=slot){
   fprintf(stderr,"Periodic mismatch phase %u slot %u\n",phase,slot);return 1;
  }
  periodic++;
 }
 /* Captured input and initial state are oracle inputs only. Production
  * must receive this state from the translated boot/attract/title path. */
 f=fopen("build/arcade-trace/camera-native/frames.bin","rb");if(!f)return 2;
 uint8_t trace[17670];WillowArcadeFrameClock traced={0};unsigned traceChecks=0;
 while(fread(trace,1,sizeof trace,f)==sizeof trace){
  unsigned frame=willowMotionLong(trace);
  if(frame<690)continue;
  if(frame==690){traced.simulation=trace[4+25];continue;}
  willowArcadeFrameStep(&traced,trace[4+0x28f],trace[4+0x68]);
  if(traced.simulation!=trace[4+25]){
   fprintf(stderr,"Captured simulation phase mismatch frame %u: %u != %u\n",frame,traced.simulation,trace[4+25]);return 1;
  }
  traceChecks++;
 }
 fclose(f);if(traceChecks!=930)return 1;
 printf("Captured simulation phase: %u consecutive intro/handoff/gameplay transitions matched\n",traceChecks);
 printf("Source frame clock: %u gate/byte-wrap/interrupt cases; %u periodic actor phase/slot cases\n",checks,periodic);
 return 0;
}
