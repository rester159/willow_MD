/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_event_ui.h"
#include "../src/willow_arcade_tasks.h"
#include "../src/willow_arcade_text_quad.h"
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
static int realTasks,realSymbols;
static uint16_t sourceWord(uint32_t at){return willowMotionWord(rom+at);}
static const uint32_t services[]={0x7d80,0x349f8,0x346e0,0x348fc,0x34998,
 0x34272,0x341da,0x4f44,0x1eaa,0x15ee,0x7f3e};
static const uint16_t observed[]={0x800c,0x8010,0x8012,0x804a,0x8376,
 0x0f0e,0x0f10,0x0f12,0x0f14,0x0f16,0x0f18,0x0f1a,0x0f1c,
 0x0f1e,0x0f20,0x0f22,0x0f24,0x0f26,0x0f28,0x0f2a,0x0f2c,
 0x0f2e,0x0f30,0x0f32,0x0f34,0x0f36,0x0f38,0x0f3a,
 0x8342,0x83c4,0x83ca,0x9884,0x8408,0x840c,0x8422,0x8444,0x8446,
 0x844a,0x8488,0x848c,0x84a2,0x84c4,0x84c6,0x84ca,
 0x8032,0x8034,0x8036,0x8038};
static void snapshot(void){
 for(unsigned n=0;n<sizeof observed/sizeof *observed;n++){ww(cursor,rw(observed[n]));cursor+=2;}
}
static void service(uint32_t address,int argument){
 unsigned id=0;while(id<sizeof services/sizeof *services&&services[id]!=address)id++;
 ww(cursor,id);cursor+=2;
 if(address==0x7d80){ww(cursor,argument);cursor+=2;}
 snapshot();
 if(address==0x7d80)ww(0x0f10,0x4444); /* ordering fixture */
}
static void task(uint16_t d0,uint16_t d1,uint16_t d2,uint32_t a0){
 if(realTasks){
  const WillowVisibilityBus bus={rw,rb,ww,wb};
  willowArcadeTaskStart(&bus,d0,a0,d1,d2,1);return;
 }
 ww(cursor,9);cursor+=2;ww(cursor,d0);cursor+=2;ww(cursor,d1);cursor+=2;
 ww(cursor,d2);cursor+=2;ww(cursor,a0>>16);cursor+=2;ww(cursor,a0);cursor+=2;
 snapshot();wb(0x0f0e,rb(0x0f0e)^1); /* re-read fixture */
}
static void symbolWord(uint32_t address,uint16_t value){
 if(address<0x910000||address>=0x914000){fprintf(stderr,"Invalid symbol destination\n");abort();}
 ww(0x4000+address-0x910000,value);
}
static void text(uint32_t a0,uint32_t a1){
 if(realSymbols){(void)willowArcadeTextQuad(a0,a1,0,1,sourceWord,symbolWord);return;}
 ww(cursor,10);cursor+=2;ww(cursor,a0>>16);cursor+=2;ww(cursor,a0);cursor+=2;
 ww(cursor,a1>>16);cursor+=2;ww(cursor,a1);cursor+=2;snapshot();
}
int main(int argc,char**argv){
 if(argc!=2&&argc!=3)return 2;
 realSymbols=argc==3&&!strcmp(argv[2],"--real-services");
 realTasks=realSymbols||(argc==3&&!strcmp(argv[2],"--real-tasks"));
 if(argc==3&&!realTasks)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Instrument only service endpoints. The original routing instructions,
  * branches and RAM writes execute unchanged. Each service records its ID,
  * explicit argument and state at invocation, with explicit mutation fixtures. */
 for(unsigned n=0;n<sizeof services/sizeof *services;n++){
  if(realTasks&&services[n]==0x15ee)continue;
  if(realSymbols&&services[n]==0x7f3e)continue;
  uint32_t at=0x101000+n*512;
  willowMotionPutWord(rom+services[n],0x4ef9);willowMotionPutLong(rom+services[n]+2,at);
#define EMIT(v) do{willowMotionPutWord(rom+at,(v));at+=2;}while(0)
  EMIT(0x38fc);EMIT(n); /* move.w #id,(a4)+ */
  if(services[n]==0x7d80)EMIT(0x38c0);
  if(services[n]==0x15ee){EMIT(0x38c0);EMIT(0x38c1);EMIT(0x38c2);EMIT(0x28c8);}
  if(services[n]==0x7f3e){EMIT(0x28c8);EMIT(0x28c9);}
  for(unsigned j=0;j<sizeof observed/sizeof *observed;j++){
   EMIT(0x38f9);EMIT(0x00ff);EMIT(observed[j]);
  }
  if(services[n]==0x7d80){EMIT(0x33fc);EMIT(0x4444);EMIT(0x00ff);EMIT(0x0f10);}
  if(services[n]==0x15ee){EMIT(0x0a39);EMIT(1);EMIT(0x00ff);EMIT(0x0f0e);}
  EMIT(0x4e75);
 }
 if(realSymbols){
  /* Relocate only the event's CPS Scroll1 destination into fixture RAM. */
  if(willowMotionLong(rom+0x33eda)!=0x910e58)return 2;
  willowMotionPutLong(rom+0x33eda,0xff4e58);
 }
 willowMotionPutLong(rom+0xa8,0x15f2); /* original trap 10 handler */
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/event-ui-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/event-ui-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 const WillowEventUiServices ui={service,task,text};
 const uint32_t entries[]={0x33e18,0x33e7c,0x33ef2,0x33f98,0x3400a};
 const uint16_t timers[]={0,1,2,65535};
 unsigned checks=0;
 for(unsigned e=0;e<5;e++)for(unsigned sample=0;sample<(e==1?8192:2304);sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  unsigned event=sample%18;
  wb(0x0f0e,event);wb(0x0e58,sample&2?0xff:0);
  ww(0x8012,timers[(sample>>2)&3]);ww(0x8010,sample&16?65534:0);
  wb(0x0f36,sample&16?254:0);wb(0x0f37,timers[(sample>>2)&3]);
  ww(0x0f10,sample&4096?0x8000:0);
  wb(0x806a,(sample&63)|((sample&1)?0xc0:0));
  wb(0x806b,((sample>>6)&63)|((sample&2)?0xc0:0));
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  cursor=0xe000;
  if(!willowArcadeEventUi(entries[e],&bus,&ui,sourceWord))return 1;
  m68k_set_reg(12,0xffe000);m68k_set_reg(14,0xffaaaa);
  RUN(entries[e],0xdeadbeef);
  if(m68k_get_reg(12)!=0xff0000+cursor){fprintf(stderr,"Service count mismatch\n");return 1;}
  for(unsigned n=0;n<65536;n++)if(n<0xfed0||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Event UI mismatch entry %06x event %u sample %u address %04x got %02x want %02x\n",entries[e],event,sample,n,ram[n^1],expected[n]);return 1;}
  }
  checks++;
 }
 if(realSymbols)puts("Symbol integration mode: original 7F3E writes replace symbol-call instrumentation; CPS destination relocated only");
 if(realTasks)puts("Task integration mode: original 15EE trap and record writes replace the task-call instrumentation");
 printf("Source event UI: %u cases, input edge priority, original text tables, task arguments, call-state snapshots and all non-stack RAM match; service bodies instrumented\n",checks);
 return 0;
}
