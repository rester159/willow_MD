/* Main loop + manager + translated leaves; external services instrumented. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_gameplay_flow.h"
#include "../src/willow_arcade_debug_row.h"
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
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static const WillowVisibilityBus bus={rw,rb,ww,wb};
static unsigned videoWrites;
static void textWrite(uint32_t at,uint16_t value){
 if(at<0x911780||at>=0x911800||(at&1))abort();
 ww(0x4000+at-0x911780,value);videoWrites++;
}
static unsigned cursor;
static const unsigned services[]={0x2dfe,0x4f44,0x4035e,0x9a92,0x42602,0x42f48,0x3807c,0x9ef2,0x3b524,0x679c,0x18bc,0xc230,0x786a,0x6496,0x785e,0x1e88,0x1f88,0x4ea2,0x39a52,0x33bd6,0x6614,0x7878,0x1eaa};
static const uint16_t observed[]={0x8000,0x8004,0x8008,0x800c,0x800e,0x8018,0x839c,0x839e};
static uint16_t sourceWord(uint32_t a){return willowMotionWord(rom+a);}
typedef struct {unsigned mode,phase,waits;uint32_t target;int argument;} Delay;
static uint8_t service(void*context,uint32_t at,int arg){
 if(at==0x18bc)return willowArcadeGameplayDebugRow(&bus,textWrite);
 Delay*t=context;
 if(t->mode&&!t->phase){t->phase=1;t->target=at;t->argument=arg;t->waits++;return 2;}
 if(t->phase&&(t->target!=at||t->argument!=arg))return 0;t->phase=0;
 unsigned id=0;while(id<23&&services[id]!=at)id++;if(id==23)return 0;
 ww(cursor,id);cursor+=2;
 if(at==0x786a||at==0x6496||at==0xc230){ww(cursor,arg);cursor+=2;}
 for(unsigned n=0;n<8;n++){ww(cursor,rw(observed[n]));cursor+=2;}
 return 1;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x18aa,0xff4000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
 for(unsigned n=0;n<23;n++){
  if(services[n]==0x18bc)continue;
  unsigned at=0x101000+n*128;
  willowMotionPutWord(rom+services[n],0x4ef9);willowMotionPutLong(rom+services[n]+2,at);
#define EMIT(v) do{willowMotionPutWord(rom+at,(v));at+=2;}while(0)
  EMIT(0x36fc);EMIT(n);
  if(services[n]==0x786a||services[n]==0x6496)EMIT(0x36c0);
  if(services[n]==0xc230)EMIT(0x36ce);
  for(unsigned j=0;j<8;j++){EMIT(0x36f9);EMIT(0xff);EMIT(observed[j]);}
  EMIT(0x4e75);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/gameplay-flow-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/gameplay-flow-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0,waits=0;
 const uint16_t timers[]={0,1,2,3,4,5,9,0xffff};
 for(unsigned mode=0;mode<2;mode++)for(unsigned path=0;path<12;path++)for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31+(n>>8));
  ww(0x8000,2);ww(0x8004,path>=10?6:2);
  ww(0x8008,path<5?0:path<10?(path-4)*2:(path-10)*2);
  ww(0x800c,path<5?path*2:0);ww(0x8344,sample%6);
  ww(0x800e,timers[sample&7]);ww(0x800a,timers[sample&7]);
  wb(0x82a6,(sample&8)?0x80:0);wb(0xc0,(sample&16)?4:0);
  ww(0x801a,(sample&32)?252:0);ww(0x801e,(sample&63)*2);
  wb(0x8342,sample);wb(0x8346,sample);wb(0x82c2,(sample&8)?1:0);wb(0x82a5,(sample&16)?1:0);
  wb(0x828f,(sample&128)?1:0);wb(0x8068,(sample&64)?32:0);wb(0x8058,0);wb(0x83c2,(sample&1)?1:0);wb(0x83c3,(sample&2)?1:0);
  wb(0x83c5,(sample&4)?1:0);wb(0xf99e,(sample&8)?1:0);ww(0x83c6,0);
  ww(0x839c,(sample&64)?0xffff:0);wb(0x839e,sample);wb(0x8019,sample);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  cursor=0xe000;WillowNativeGameplayFlow flow={0};flow.task.pc=0x92d8;
  Delay delay={0};delay.mode=mode;uint32_t a4=0xff6780;
  WillowNativeGameplayFlowBinding binding={&flow,&bus,sourceWord,&a4,service,&delay};
  unsigned resumes=0;WillowTransitionStep result;
  do{result=willowNativeGameplayFlowStep(&binding);
   if(!result.supported||++resumes>16){fprintf(stderr,"Unsupported path %u sample %u pc %x\n",path,sample,flow.task.pc);return 1;}
  }while(flow.task.pc!=0x932c);
  if(flow.manager.target||flow.entry.pc||flow.route.pc||delay.phase||result.delay!=1)return 1;waits+=delay.waits;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(12,0xff6780);m68k_set_reg(11,0xffe000);m68k_set_reg(16,0x92d8);retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(11)!=0xff0000+cursor||m68k_get_reg(12)!=a4||m68k_get_reg(15)!=0xfffefc||(m68k_get_reg(0)&65535)!=1){fprintf(stderr,"CPU mismatch path %u sample %u PC %x A3 %x A4 %x\n",path,sample,m68k_get_reg(16),m68k_get_reg(11),m68k_get_reg(12));return 1;}
  if(ram[0xfefe^1]!=0x93||ram[0xfeff^1]!=0x2c)return 1;
  for(unsigned n=0;n<65536;n++)if((n<0xfe80||n>=0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Flow mismatch path %u sample %u at %x got %x want %x\n",path,sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 printf("Combined gameplay flow: %u original comparisons, %u injected waits; main loop, elapsed counter, dispatch, stage entry, event routes, life loss, source sleep and non-stack RAM match\n",checks,waits);
 printf("Actual debug-row service: %u Scroll1 word writes verified\n",videoWrites);
 return 0;
}
