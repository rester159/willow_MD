/* Original manager dispatch and elapsed counter; leaf bodies instrumented. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_manager.h"
#include "../native/engine/src/campaign_control.h"
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
static const unsigned leaves[]={0x93d6,0x9412,0x942e,0x9530,0x9600,0x962e,0x964c,0x9690,0x96ba,0x96be,0x973e,0x9742,0x9748};
static uint16_t sourceWord(uint32_t a){return willowMotionWord(rom+a);}
typedef struct {unsigned mode,phase,waits;uint32_t target;} Delay;
static uint8_t service(void*context,uint32_t at,int arg){
 Delay*t=context;if(arg!=-1)return 0;
 if(t->phase){if(t->target!=at)return 0;t->phase=0;return 1;}
 unsigned id=0;while(id<13&&leaves[id]!=at)id++;if(id==13)return 0;
 ww(0x8000,8);ww(0x8004,6);ww(0x8008,10);
 ww(cursor,id);cursor+=2;ww(cursor,rw(0x839c));cursor+=2;
 ww(cursor,rw(0x839e));cursor+=2;
 if(t->mode){t->phase=1;t->target=at;t->waits++;return 2;}return 1;
}
typedef struct {WillowNativeCampaignState*state;WillowNativeGameplayRouteState*route;Delay delay;} NativeManager;
static uint8_t nativeService(void*context,unsigned operation){
 NativeManager*n=context;assert(operation>=1&&operation<=13);
 ww(0x8000,n->state->mode);ww(0x8004,n->route->mode);ww(0x8008,n->route->step);
 ww(0x839c,n->state->elapsedWhole);wb(0x839e,n->state->elapsedFraction);
 uint8_t result=service(&n->delay,leaves[operation-1],-1);
 n->state->mode=rw(0x8000);n->route->mode=rw(0x8004);n->route->step=rw(0x8008);
 return result;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 for(unsigned n=0;n<13;n++){
  unsigned at=0x11000+n*64;
  /* Four-byte branch preserves immediately adjacent source dispatch code. */
  willowMotionPutWord(rom+leaves[n],0x6000);
  willowMotionPutWord(rom+leaves[n]+2,at-(leaves[n]+2));
#define EMIT(v) do{willowMotionPutWord(rom+at,(v));at+=2;}while(0)
  EMIT(0x3b7c);EMIT(8);EMIT(0);
  EMIT(0x3b7c);EMIT(6);EMIT(4);
  EMIT(0x3b7c);EMIT(10);EMIT(8);
  EMIT(0x38fc);EMIT(n);
  EMIT(0x38ed);EMIT(0x39c);EMIT(0x38ed);EMIT(0x39e);EMIT(0x4e75);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/gameplay-manager-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/gameplay-manager-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0,waits=0;
 const uint16_t states[][3]={{0,0,0},{2,0,0},{2,2,0},{2,2,2},{2,2,4},{2,2,6},{2,2,8},{2,2,10},{2,4,0},{2,6,0},{4,0,0},{6,0,0},{8,0,0}};
 for(unsigned mode=0;mode<2;mode++)for(unsigned e=0;e<13;e++)for(unsigned sample=0;sample<1024;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31);
  ww(0x8000,states[e][0]);ww(0x8004,states[e][1]);ww(0x8008,states[e][2]);
  const uint16_t counts[]={0,1,0xfffe,0xffff};
  ww(0x839c,counts[sample>>8]);wb(0x839e,sample);
  memcpy(initial,expected,sizeof initial);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  cursor=0xe000;WillowArcadeGameplayManager task={0};Delay delay={0};delay.mode=mode;
  unsigned resumes=0;uint8_t result;
  do{result=willowArcadeGameplayManagerResume(&task,&bus,sourceWord,service,&delay);
   if(!result||++resumes>2)return 1;
  }while(result==2);
  if(task.target||delay.phase||cursor!=0xe006)return 1;waits+=delay.waits;
  memcpy(reference,expected,sizeof reference);memcpy(expected,initial,sizeof expected);cursor=0xe000;
  WillowNativeCampaignState state={rw(0x8000),rw(0x839c),rb(0x839e)};
  WillowNativeGameplayRouteState route={.mode=rw(0x8004),.step=rw(0x8008)};
  WillowNativeCampaignManager manager={0};NativeManager native={&state,&route,{.mode=mode}};
  unsigned nativeResumes=0;
  do{result=willowNativeCampaignManagerStep(&manager,&state,&route,nativeService,&native);if(!result||++nativeResumes>2)return 1;}while(result==2);
  if(manager.target||native.delay.phase||nativeResumes!=resumes||native.delay.waits!=delay.waits||cursor!=0xe006||memcmp(expected,reference,sizeof expected))return 1;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(12,0xffe000);m68k_set_reg(16,0x93be);retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(12)!=0xff0000+cursor||m68k_get_reg(15)!=0xffff04)return 1;
  for(unsigned n=0;n<65536;n++)if((n<0xfe80||n>=0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Manager mismatch state %u sample %u at %x got %x want %x\n",e,sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 printf("Gameplay manager: %u original CPU/source/semantic-native comparisons, %u injected suspensions; all 13 dispatch leaves, all elapsed-counter bytes, saturation, retained selection and non-stack RAM match\n",checks,waits);
 return 0;
}
