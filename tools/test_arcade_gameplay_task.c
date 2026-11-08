/* Main-task loop oracle. Service bodies are instrumented, not substituted
 * into the port; original loop instructions and branches execute unchanged. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_gameplay_task.h"
#include "../native/engine/src/campaign_control.h"
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
static const unsigned services[]={0x1ecc,0x93be,0x18bc,0x932e};
static const uint16_t observed[]={0x8018,0x828e,0x8066,0x8068,0xf9a0,0x8380};
typedef struct {unsigned enabled,phase,waits;uint32_t address;int argument;} ServiceDelay;
static uint8_t service(void*context,uint32_t at,int arg){
 ServiceDelay*t=context;
 if(t->enabled){
  if(!t->phase){t->phase=1;t->address=at;t->argument=arg;t->waits++;return 2;}
  if(t->address!=at||t->argument!=arg)return 0;
  t->phase=0;
 }
 unsigned id=0;while(id<4&&services[id]!=at)id++;
 if(id==4)return 0;
 ww(cursor,id);cursor+=2;
 if(!id){ww(cursor,arg);cursor+=2;}
 for(unsigned n=0;n<6;n++){ww(cursor,rw(observed[n]));cursor+=2;}
 if(at==0x93be&&rb(0x8380)){wb(0x8380,rb(0x8380)-1);wb(0xf9a1,0);}
 if(at==0x18bc)wb(0x8068,rb(0x8068)^rb(0x8381));
 return 1;
}
static uint8_t nativeService(void*context,unsigned operation){
 if(operation<WILLOW_CAMPAIGN_START_FADE||operation>WILLOW_CAMPAIGN_EXIT)return 0;
 return service(context,services[operation-WILLOW_CAMPAIGN_START_FADE],operation==WILLOW_CAMPAIGN_START_FADE?1:-1);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
 for(unsigned n=0;n<4;n++){
  unsigned at=0x101000+n*256;
  willowMotionPutWord(rom+services[n],0x4ef9);willowMotionPutLong(rom+services[n]+2,at);
#define EMIT(v) do{willowMotionPutWord(rom+at,(v));at+=2;}while(0)
  EMIT(0x38fc);EMIT(n);
  if(!n)EMIT(0x38c0);
  for(unsigned j=0;j<6;j++){EMIT(0x38f9);EMIT(0xff);EMIT(observed[j]);}
  if(n==1){
   EMIT(0x4a39);EMIT(0xff);EMIT(0x8380);EMIT(0x670c);
   EMIT(0x5339);EMIT(0xff);EMIT(0x8380);
   EMIT(0x4239);EMIT(0xff);EMIT(0xf9a1);
  }
  if(n==2){EMIT(0x1039);EMIT(0xff);EMIT(0x8381);EMIT(0xb139);EMIT(0xff);EMIT(0x8068);}
  EMIT(0x4e75);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/gameplay-task-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/gameplay-task-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0,exits=0,sleeps=0,serviceWaits=0;
 const uint16_t entries[]={0x92d2,0x92d8,0x931c,0x932c};
 for(unsigned mode=0;mode<2;mode++)for(unsigned e=0;e<4;e++)for(unsigned sample=0;sample<1024;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31);
  wb(0x828f,sample&1?0x80:0);wb(0x8068,(sample>>1)&6);wb(0x8058,sample&8?64:0);
  wb(0x8067,(sample>>4)&255);wb(0x8019,(sample&16)?255:sample);
  wb(0x8380,(sample>>8)&3);wb(0x8381,(sample&128)?4:0);
  memcpy(initial,expected,sizeof initial);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  cursor=0xe000;WillowNativeGameplayTask task={entries[e]};
  ServiceDelay delay={0};delay.enabled=mode;WillowTransitionStep result;unsigned resumes=0;
  do{
   result=willowNativeGameplayTaskStep(&task,&bus,service,&delay);
   if(!result.supported||++resumes>64)return 1;
  }while(!result.finished&&task.pc!=0x931c&&task.pc!=0x932c);
  if(delay.phase)return 1;
  serviceWaits+=delay.waits;
  memcpy(reference,expected,sizeof reference);memcpy(expected,initial,sizeof expected);unsigned expectedCursor=cursor;cursor=0xe000;
  WillowNativeCampaignTask nativeTask={e?WILLOW_CAMPAIGN_BEGIN:WILLOW_CAMPAIGN_INITIAL};
  WillowNativeCampaignInputs inputs={expected+0x8019,expected+0xf9a1,expected+0x828f,expected+0x8068,expected+0x8058,expected+0x8067};
  ServiceDelay nativeDelay={.enabled=mode};WillowNativeTaskStep nativeResult;unsigned nativeResumes=0;
  do{nativeResult=willowNativeCampaignTaskStep(&nativeTask,&inputs,nativeService,&nativeDelay);if(!nativeResult.action||++nativeResumes>64)return 1;}
  while(nativeResult.action!=WILLOW_TASK_FINISH&&nativeTask.phase!=WILLOW_CAMPAIGN_BEGIN);
  if(nativeResumes!=resumes||nativeDelay.phase||nativeDelay.waits!=delay.waits||cursor!=expectedCursor||
     memcmp(expected,reference,sizeof expected)||nativeResult.delay!=result.delay||
     (nativeResult.action==WILLOW_TASK_FINISH)!=result.finished)return 1;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(12,0xffe000);m68k_set_reg(16,entries[e]);retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(12)!=0xff0000+cursor)return 1;
  for(unsigned n=0;n<65536;n++)if((n<0xfe80||n>=0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Main loop mismatch entry %x sample %u at %x got %x want %x\n",entries[e],sample,n,ram[n^1],expected[n]);return 1;
  }
  if(result.finished){if(m68k_get_reg(15)!=0xffff04||task.pc)return 1;exits++;}
  else {
   if(m68k_get_reg(15)!=0xfffefc||(m68k_get_reg(0)&65535)!=result.delay)return 1;
   unsigned pc=((unsigned)ram[0xfefc^1]<<24)|((unsigned)ram[0xfefd^1]<<16)|((unsigned)ram[0xfefe^1]<<8)|ram[0xfeff^1];
   if(pc!=result.resume||task.pc!=pc)return 1;sleeps++;
  }
  checks++;
 }
 printf("Main gameplay loop: %u original CPU/source/semantic-native comparisons, %u sleeps and %u exits; simulation count, pause gates, variable delays, service snapshots and non-stack RAM match\n",checks,sleeps,exits);
 printf("Injected service suspensions: %u; no replay of counter increments or completed services\n",serviceWaits);
 return 0;
}
