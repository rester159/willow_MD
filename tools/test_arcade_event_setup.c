/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_event_setup.h"
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
static unsigned cursor;
static uint16_t sourceWord(uint32_t at){return willowMotionWord(rom+at);}
static const uint32_t services[]={0x1e88,0x785e,0x0b28,0x78c8,0x548c,
 0x5532,0x34a4a,0x168c,0x786a,0x2de6,0x4f44,0x0978};
static const uint16_t observed[]={0x800c,0x8010,0x8012,0x804a,0x8376,
 0x0f0e,0x0f10,0x0f12,0x0f14,0x0f16,0x0f18,0x0f1a,0x0f1c,
 0x0f1e,0x0f20,0x0f22,0x0f24,0x0f26,0x0f28,0x0f2a,0x0f2c,
 0x0f2e,0x0f30,0x0f32,0x0f34,0x0f36,0x0f38,0x0f3a,
 0x8342,0x83c4,0x83ca,0x9884,0x8408,0x840c,0x8422,0x8444,0x8446,
 0x844a,0x8488,0x848c,0x84a2,0x84c4,0x84c6,0x84ca,
 0x8032,0x8034,0x8036,0x8038};
static void service(uint32_t address,int argument){
 unsigned id=0;while(id<sizeof services/sizeof *services&&services[id]!=address)id++;
 ww(cursor,id);cursor+=2;
 if(address==0x168c||address==0x786a||address==0x548c||address==0x5532){ww(cursor,argument);cursor+=2;}
 for(unsigned n=0;n<sizeof observed/sizeof *observed;n++){ww(cursor,rw(observed[n]));cursor+=2;}
 /* Fixture effects test ordering, not the actual service implementations. */
 if(address==0x785e)wb(0x8343,0x80);
 if(address==0x5532)wb(0x0f0e,rb(0x0f0e)^1);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Instrument only service endpoints. The original routing instructions,
  * branches and RAM writes execute unchanged. Each service records its ID,
  * explicit argument and state at invocation, with explicit mutation fixtures. */
 for(unsigned n=0;n<sizeof services/sizeof *services;n++){
  uint32_t at=0x101000+n*512;
  willowMotionPutWord(rom+services[n],0x4ef9);willowMotionPutLong(rom+services[n]+2,at);
#define EMIT(v) do{willowMotionPutWord(rom+at,(v));at+=2;}while(0)
  EMIT(0x38fc);EMIT(n); /* move.w #id,(a4)+ */
  if(services[n]==0x786a||services[n]==0x168c)EMIT(0x38c0);
  if(services[n]==0x548c||services[n]==0x5532)EMIT(0x38ce); /* move.w a6,(a4)+ */
  for(unsigned j=0;j<sizeof observed/sizeof *observed;j++){
   EMIT(0x38f9);EMIT(0x00ff);EMIT(observed[j]);
  }
  if(services[n]==0x785e){EMIT(0x13fc);EMIT(0x80);EMIT(0x00ff);EMIT(0x8343);}
  if(services[n]==0x5532){EMIT(0x0a39);EMIT(1);EMIT(0x00ff);EMIT(0x0f0e);}
  EMIT(0x4e75);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/event-setup-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/event-setup-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 const uint32_t entries[]={0x33bf8,0x33c8e,0x33f42};
 const uint16_t timers[]={0,1,2,65535};
 unsigned checks=0;
 for(unsigned e=0;e<3;e++)for(unsigned event=0;event<18;event++)for(unsigned sample=0;sample<256;sample++)for(unsigned phase=0;phase<4;phase++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  wb(0x8352,sample);wb(0x8357,255-sample);
  wb(0x8366,sample);wb(0x8367,event==17&&sample==6?4:sample*73);
  wb(0x0f0e,event);wb(0x9866,sample&1?0x80:0);
  wb(0x0e58,sample&2?0xff:0);
  ww(0x8012,timers[phase]);ww(0x8010,sample&16?65534:0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  cursor=0xe000;
  if(!willowArcadeEventSetup(entries[e],&bus,service,sourceWord))return 1;
  m68k_set_reg(12,0xffe000);m68k_set_reg(14,0xffaaaa);
  RUN(entries[e],0xdeadbeef);
  if(m68k_get_reg(12)!=0xff0000+cursor){fprintf(stderr,"Service count mismatch\n");return 1;}
  for(unsigned n=0;n<65536;n++)if(n<0xfed0||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Event setup mismatch entry %06x event %u sample %u address %04x got %02x want %02x\n",entries[e],event,sample,n,ram[n^1],expected[n]);return 1;}
  }
  checks++;
 }
 printf("Source event setup/completion: %u cases, original ROM tables, service order/arguments/state snapshots and full non-stack RAM match; service bodies instrumented, not verified here\n",checks);
 return 0;
}
