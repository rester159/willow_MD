#include "../src/willow_native_visibility_payload.h"
/* Differential verification of task startup against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_task_startup.h"
#include "../src/willow_native_demo_state.h"
#include "../src/willow_native_demo_start_state.h"
#include "../src/willow_native_demo_update_state.h"
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
static uint16_t romWord(uint32_t a){return willowMotionWord(rom+a);}
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
 /* Queue initializers stop before unrelated services. Fixed-task startup
  * uses the real trap-0 handler and stops before changing interrupt state. */
 willowMotionPutWord(rom+0x1436,0x4ef9);willowMotionPutLong(rom+0x1438,0x100000);
 willowMotionPutWord(rom+0x14c8,0x4ef9);willowMotionPutLong(rom+0x14ca,0x100000);
 willowMotionPutWord(rom+0x1490,0x4ef9);willowMotionPutLong(rom+0x1492,0x100000);
 willowMotionPutLong(rom+0x80,0x15d0);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/task-startup-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/task-startup-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;const unsigned entries[]={0x1400,0x1424,0x14ac,0x1466,0x6f2};
 const uint8_t states[]={0,1,2,4,8,12,0x80,0xff};
 for(unsigned kind=0;kind<5;kind++)for(unsigned sample=0;sample<(kind==4?32u:2048u);sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  expected[0]=states[sample&7];expected[0xa0]=states[(sample>>3)&7];
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d1=sample*977,d2=sample*317;
  uint32_t a5=(kind==3&&(sample&1))?0xffff8000UL:0x00ff8000UL;
  if(!kind)willowArcadeStartupPrimaryQueue(&bus);
  else if(kind==1)willowArcadeStartupSecondaryQueue(&bus);
  else if(kind==2)willowArcadeStartupTasks(&bus,d1,d2);
  else if(kind==3)willowArcadeStartupRecordPool(&bus,romWord,a5);
  else memset(expected,0,sizeof expected);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,a5);m68k_set_reg(15,0xfff000);
  m68k_set_reg(0,0x12345678);m68k_set_reg(1,0xabcd0000UL|d1);m68k_set_reg(2,0x98760000UL|d2);m68k_set_reg(16,entries[kind]);
  if(kind==4)m68k_set_reg(12,0x100000);
  retro_run();
  if(kind==4){
   for(unsigned frame=1;frame<8&&m68k_get_reg(16)!=0x100000;frame++)retro_run();
   if(m68k_get_reg(16)!=0x100000||m68k_get_reg(8)!=0x1000000||m68k_get_reg(0)!=0){fprintf(stderr,"Cold clear exit mismatch\n");return 1;}
   WillowNativeDemoState retained;
   memset(&retained,sample*7+1,sizeof retained);
   willowNativeDemoStateColdReset(&retained);
   WillowNativeDemoStartState start;memset(&start,0x5a,sizeof start);willowNativeDemoStartStateColdReset(&start);
   WillowNativeDemoUpdateState update;memset(&update,0xa5,sizeof update);willowNativeDemoUpdateStateColdReset(&update);
   WillowNativeVisibilityPayload payload;memset(&payload,0x5a,sizeof payload);willowNativeVisibilityPayloadColdReset(&payload);
   unsigned projected=0,startProjected=0,updateProjected=0,payloadProjected=0;
   for(unsigned at=0;at<65536;at++){
    if(ram[at^1]!=expected[at]){fprintf(stderr,"Cold clear RAM mismatch %04x\n",at);return 1;}
    uint8_t*p=willowNativeDemoStateResolve(&retained,at,1);
    if(p){if(*p!=ram[at^1]){fprintf(stderr,"Cold demo projection mismatch %04x\n",at);return 1;}projected++;}
   }
   for(unsigned at=0;at<65536;at++){uint8_t*p=willowNativeDemoStartStateResolve(&start,at,1);if(p){if(*p!=ram[at^1])return 1;startProjected++;}}
   for(unsigned at=0;at<65536;at++){uint8_t*p=willowNativeDemoUpdateStateResolve(&update,at,1);if(p){if(*p!=ram[at^1])return 1;updateProjected++;}}
   for(unsigned at=0;at<65536;at++){uint8_t*p=willowNativeVisibilityPayloadResolve(&payload,at,1);if(p){if(*p!=ram[at^1])return 1;payloadProjected++;}}
   if(projected!=321||startProjected!=111||updateProjected!=27||payloadProjected!=784)return 1;
   checks++;continue;
  }
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(15)!=(kind?0xfff000u:0xffeffcu)){fprintf(stderr,"Startup stop mismatch kind %u sample %u PC %x SP %x\n",kind,sample,m68k_get_reg(16),m68k_get_reg(15));return 1;}
  if(kind<2 && (m68k_get_reg(8)!=(kind?0xff8272u:0xffff8172u)||m68k_get_reg(1)!=(kind?0u:0xffffffffu))){fprintf(stderr,"Queue exit registers kind %u A0 %x D1 %x\n",kind,m68k_get_reg(8),m68k_get_reg(1));return 1;}
  for(unsigned n=0;n<65536;n++)if(!(kind!=3&&n>=0xefc0&&n<0xf004)&&ram[n^1]!=expected[n]){fprintf(stderr,"Startup mismatch kind %u sample %u RAM %04x\n",kind,sample,n);return 1;}
  checks++;
 }
 puts("Source cold clear: 32 original-instruction cases; all 65536 RAM bytes and 1243 retained demo/visibility bytes match; later lifetime not implied");
 printf("Source task startup: %u total cold-clear/queue/fixed-task/record-pool cases; both queue-entry ranges, preserved fields, conditional starts, inherited parameters and pool data match original instructions and all non-stack RAM (all RAM for cold clear and record pool)\n",checks);
 return 0;
}
