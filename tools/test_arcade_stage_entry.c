/* Original stage-entry dispatch, commands and task creation; rendering/service leaves instrumented. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_stage_entry.h"
#include "../native/engine/src/stage_entry.h"
#include <assert.h>
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
static uint8_t initial[65536],reference[65536];
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static unsigned cursor;
static const unsigned services[]={0x2dfe,0x4f44,0x4035e,0x9a92,0x42602,0x42f48,0x3807c,0x9ef2,0x3b524,0x679c};
static const uint16_t observed[]={0x8008,0x800c,0x800e,0x82a6,0xf99c};
static uint16_t sourceWord(uint32_t a){return willowMotionWord(rom+a);}
typedef struct {unsigned mode,phase,waits;uint32_t target;} Delay;
static uint8_t service(void*context,uint32_t at,int arg){
 Delay*t=context;if(arg!=-1)return 0;
 if(t->mode&&!t->phase){t->phase=1;t->target=at;t->waits++;return 2;}
 if(t->phase&&t->target!=at)return 0;t->phase=0;
 unsigned id=0;while(id<10&&services[id]!=at)id++;if(id==10)return 0;
 ww(cursor,id);cursor+=2;
 for(unsigned n=0;n<5;n++){ww(cursor,rw(observed[n]));cursor+=2;}
 return 1;
}
typedef struct {WillowNativeGameplayRouteState*route;Delay delay;uint32_t a4;} NativeEntry;
static void nativeStore(const NativeEntry*n){ww(0x8008,n->route->step);ww(0x800c,n->route->substep);ww(0x800e,n->route->eventStep);}
static uint8_t nativeService(void*context,unsigned operation){
 NativeEntry*n=context;assert(operation<10);nativeStore(n);return service(&n->delay,services[operation],-1);
}
/* These callbacks record native command/message requests into the oracle's
 * source-format observation buffers. The live game binds the real native
 * queues instead; their consumers are tested separately against original CPU. */
static int nativeCommand(void*context,uint16_t command,uint16_t argument){
 NativeEntry*n=context;const WillowVisibilityBus bus={rw,rb,ww,wb};nativeStore(n);
 willowArcadeEventCommand(&bus,command,argument);n->a4=0xff8072;return 1;
}
static int nativeMessage(void*context,uint16_t message,uint16_t layout){
 NativeEntry*n=context;const WillowVisibilityBus bus={rw,rb,ww,wb};nativeStore(n);
 willowArcadeTaskStart(&bus,0xc0,0x78f0,message,layout,0);return 1;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 uint8_t entryBytes[32];f=fopen("build/native-world/campaign-entry-data-v1/stage-entry.bin","rb");
 if(!f||fread(entryBytes,1,sizeof entryBytes,f)!=sizeof entryBytes)return 2;fclose(f);
 WillowNativeStageEntryData entryData;assert(willowNativeStageEntryDataBind(&entryData,entryBytes,sizeof entryBytes));
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 willowMotionPutLong(rom+0x80,0x15d0);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 for(unsigned n=0;n<10;n++){
  unsigned at=0x101000+n*128;
  willowMotionPutWord(rom+services[n],0x4ef9);willowMotionPutLong(rom+services[n]+2,at);
#define EMIT(v) do{willowMotionPutWord(rom+at,(v));at+=2;}while(0)
  EMIT(0x36fc);EMIT(n);
  for(unsigned j=0;j<5;j++){EMIT(0x36f9);EMIT(0xff);EMIT(observed[j]);}
  EMIT(0x4e75);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/stage-entry-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/stage-entry-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0,waits=0;
 const uint16_t timers[]={0,1,2,3,4,5,9,0xffff};
 for(unsigned mode=0;mode<2;mode++)for(unsigned state=0;state<5;state++)for(unsigned stage=0;stage<6;stage++)for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31+(n>>8));
  ww(0x800c,state*2);ww(0x8008,0);ww(0x8344,stage);ww(0x800e,timers[sample&7]);
  wb(0x82a6,(sample&8)?0x80:0);wb(0xc0,(sample&16)?4:0);
  ww(0x801a,(sample&32)?252:0);
  memcpy(initial,expected,sizeof initial);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  cursor=0xe000;WillowArcadeStageEntry task={0};Delay delay={0};delay.mode=mode;
  uint32_t a4=0xff6780;unsigned resumes=0;uint8_t result;
  do{result=willowArcadeStageEntryResume(&task,&bus,sourceWord,&a4,service,&delay);
   if(!result||++resumes>8)return 1;
  }while(result==2);
  if(task.pc||delay.phase)return 1;waits+=delay.waits;
  memcpy(reference,expected,sizeof reference);memcpy(expected,initial,sizeof expected);unsigned referenceCursor=cursor;cursor=0xe000;
  WillowNativeGameplayRouteState route={.step=rw(0x8008),.substep=rw(0x800c),.eventStep=rw(0x800e)};
  WillowNativeStageEntry nativeTask={0};NativeEntry native={&route,{.mode=mode},0xff6780};
  WillowNativeStageEntryServices nativeServices={&native,nativeService,nativeCommand,nativeMessage};unsigned nativeResumes=0;
  do{result=willowNativeStageEntryStep(&nativeTask,&route,stage,expected+0x82a6,expected+0xf99c,&entryData,&nativeServices);
   if(!result||++nativeResumes>8)return 1;
  }while(result==2);
  nativeStore(&native);
  if(nativeTask.phase||native.delay.phase||native.delay.waits!=delay.waits||nativeResumes!=resumes||native.a4!=a4||cursor!=referenceCursor||memcmp(expected,reference,sizeof expected)){
   fprintf(stderr,"Native stage entry mismatch state %u stage %u sample %u mode %u\n",state,stage,sample,mode);return 1;
  }
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(12,0xff6780);m68k_set_reg(11,0xffe000);m68k_set_reg(16,0x942e);retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(11)!=0xff0000+cursor||m68k_get_reg(12)!=a4||m68k_get_reg(15)!=0xffff04){fprintf(stderr,"CPU mismatch state %u sample %u PC %x A3 %x A4 %x\n",state,sample,m68k_get_reg(16),m68k_get_reg(11),m68k_get_reg(12));return 1;}
  for(unsigned n=0;n<65536;n++)if((n<0xfe80||n>=0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Stage entry mismatch state %u stage %u sample %u at %x got %x want %x\n",state,stage,sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 printf("Stage entry: %u original CPU/source/semantic-native comparisons, %u injected waits; five states, six stages, countdown gates, occupied message slots, command queue wrap, service snapshots and non-stack RAM match\n",checks,waits);
 return 0;
}
