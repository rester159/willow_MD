/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_event_route.h"
#ifdef WILLOW_TEST_EVENT_RESUME
#include "../src/willow_arcade_event_resume.h"
#endif
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
static const uint32_t services[]={0xc230,0x786a,0x6496,0x785e,0x1e88,
 0x1f88,0x4ea2,0x9ef2,0x39a52,0x3807c,0x3b524,0x33bd6,0x6614,
 0x42602,0x42f48,0x7878,0x1eaa};
static const uint16_t observed[]={0x8004,0x8008,0x800a,0x800c,0x8010,
 0x83c2,0x83c4,0x83d0,0x9846,0x9848,0x984a,0x987c,0x9894,0x9896};
static void service(uint32_t address,int argument){
 unsigned id=0;while(id<sizeof services/sizeof *services&&services[id]!=address)id++;
 ww(cursor,id);cursor+=2;
 if(address==0x786a||address==0x6496||address==0xc230){ww(cursor,argument);cursor+=2;}
 for(unsigned n=0;n<sizeof observed/sizeof *observed;n++){ww(cursor,rw(observed[n]));cursor+=2;}
 /* Deliberate callback effects test read-after-call and overwrite order.
  * These are fixtures, not translations of the service bodies. */
 if(address==0x785e)wb(0x83a2,rb(0x83a2)^1);
 if(address==0x6614)wb(0x8471,0);
 if(address==0x1f88)wb(0x83c5,0);
 if(address==0x7878)ww(0x8008,65535);
}
#ifdef WILLOW_TEST_EVENT_RESUME
typedef struct {unsigned mode,phase,waits;uint32_t address;int argument;} ServiceTask;
static uint8_t resumeService(void*context,uint32_t address,int argument){
 ServiceTask*t=context;
 if(!t->mode){service(address,argument);return 1;}
 if(!t->phase){t->address=address;t->argument=argument;t->phase=1;t->waits++;return 2;}
 if(t->address!=address||t->argument!=argument)return 0;
 if(t->phase==1){
  service(address,argument);
  if(t->mode==2){t->phase=2;t->waits++;return 2;}
 }
 t->phase=0;return 1;
}
#endif
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
  uint32_t at=0x101000+n*128;
  willowMotionPutWord(rom+services[n],0x4ef9);willowMotionPutLong(rom+services[n]+2,at);
#define EMIT(v) do{willowMotionPutWord(rom+at,(v));at+=2;}while(0)
  EMIT(0x38fc);EMIT(n); /* move.w #id,(a4)+ */
  if(services[n]==0x786a||services[n]==0x6496)EMIT(0x38c0);
  if(services[n]==0xc230)EMIT(0x38ce); /* move.w a6,(a4)+ */
  for(unsigned j=0;j<sizeof observed/sizeof *observed;j++){
   EMIT(0x38f9);EMIT(0x00ff);EMIT(observed[j]);
  }
  if(services[n]==0x785e){EMIT(0x0a39);EMIT(1);EMIT(0x00ff);EMIT(0x83a2);}
  if(services[n]==0x6614){EMIT(0x4239);EMIT(0x00ff);EMIT(0x8471);}
  if(services[n]==0x1f88){EMIT(0x4239);EMIT(0x00ff);EMIT(0x83c5);}
  if(services[n]==0x7878){EMIT(0x33fc);EMIT(65535);EMIT(0x00ff);EMIT(0x8008);}
  EMIT(0x4e75);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/event-route-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/event-route-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 const uint16_t entries[]={0x9530,0x9600,0x962e,0x964c
#ifdef WILLOW_TEST_EVENT_RESUME
 ,0x9690
#endif
 };
 const uint16_t timers[]={0,1,2,65535};
 unsigned checks=0;
#ifdef WILLOW_TEST_EVENT_RESUME
 unsigned waits=0;
 for(unsigned mode=0;mode<3;mode++)
#endif
 for(unsigned e=0;e<sizeof entries/sizeof *entries;e++)for(unsigned sample=0;sample<1024;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  wb(0xf99e,sample&1?0x80:0);ww(0x83c6,sample&2?123:0);
  wb(0x83c2,sample&4?2:0);wb(0x83c5,sample&8?1:0);
  wb(0x83c3,sample&16?0xff:0);wb(0x83a2,sample&32?1:0);
  wb(0x8471,sample&64?0x80:0);ww(0x800a,timers[(sample>>7)&3]);
  ww(0x8008,sample&512?65534:2);ww(0x8004,sample&512?65535:4);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  cursor=0xe000;
#ifdef WILLOW_TEST_EVENT_RESUME
  WillowArcadeEventRouteTask task={0};ServiceTask servicesTask={0};servicesTask.mode=mode;
  uint8_t result;unsigned resumes=0;
  do{
   result=willowArcadeEventRouteResume(&task,entries[e],&bus,resumeService,&servicesTask);
   if(!result||++resumes>64||(result==2&&!task.pc))return 1;
  }while(result==2);
  if(task.pc||servicesTask.phase)return 1;
  waits+=servicesTask.waits;
#else
  if(!willowArcadeEventRoute(entries[e],&bus,service))return 1;
#endif
  m68k_set_reg(12,0xffe000);m68k_set_reg(14,0xffaaaa);
  RUN(entries[e],0xdeadbeef);
  if(m68k_get_reg(12)!=0xff0000+cursor){fprintf(stderr,"Service count mismatch\n");return 1;}
  for(unsigned n=0;n<65536;n++)if(n<0xfed0||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Event route mismatch entry %04x sample %u address %04x got %02x want %02x\n",entries[e],sample,n,ram[n^1],expected[n]);return 1;}
  }
  checks++;
 }
 printf("Source event routing: %u cases, service order/arguments/state snapshots and full non-stack RAM match; service bodies instrumented, not verified here\n",checks);
#ifdef WILLOW_TEST_EVENT_RESUME
 printf("Resumable route: %u service waits, before and after callback writes; no replay of completed services\n",waits);
#endif
 return 0;
}
