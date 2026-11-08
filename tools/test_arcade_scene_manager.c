/* Original scene dispatch and branches; called services instrumented except allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_gameplay_flow.h"
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
static const unsigned services[]={0x1f88,0x1eaa,0x9ef2,0x3b536,0x784c,0x1e88,0x42602,0x42f48,0x2de6,0x2648,0x0978,0x2f08,0x2dfe,0x78b8,0x4f44,0x7d80,0x3b524,0x6642,0x4035e,0x9a92,0xa5be,0xa5d6,0x7dae,0x9f34,0x6770};
#define SERVICE_COUNT (sizeof services/sizeof services[0])
static const uint16_t observed[]={0x8008,0x800a,0x806a,0xf99c,0x801a,0x8004,0x800c,0x8348,0x83a0,0x83a2,0x0fb0,0x0fb2,0x8408,0x840c,0x8488,0x848c,0x984c,0x9848,0x82a6,0x0e58,0x0e6e,0x9850,0x9866};
#define OBSERVED_COUNT (sizeof observed/sizeof observed[0])
static uint16_t sourceWord(uint32_t a){return willowMotionWord(rom+a);}
typedef struct {unsigned mode,phase,waits;uint32_t target;uint32_t*a4;} Delay;
static uint8_t service(void*context,uint32_t at,int arg){
 Delay*t=context;if(at==0x7d80?(arg!=13&&arg!=16):at==0x7dae?arg!=26:at==0xa5d6?arg!=0:arg!=-1)return 0;
 if(t->mode&&!t->phase){t->phase=1;t->target=at;t->waits++;return 2;}
 if(t->phase&&t->target!=at)return 0;t->phase=0;
 if(at==0x2b9c){
  if(rb(0x980d)){uint16_t top=rw(0x9834);*t->a4=0xffff0000UL|rw(top);
   ww(0x9834,top+2);wb(0x980d,rb(0x980d)-1);}
  return 1;
 }
 unsigned id=0;while(id<SERVICE_COUNT&&services[id]!=at)id++;if(id==SERVICE_COUNT)return 0;
 if(at==0x1f88)wb(0x806a,rb(0x8060));
 ww(cursor,id);cursor+=2;
 if(at==0x7d80||at==0x7dae||at==0xa5d6){ww(cursor,arg);cursor+=2;}
 for(unsigned n=0;n<OBSERVED_COUNT;n++){ww(cursor,rw(observed[n]));cursor+=2;}
 return 1;
}
int main(int argc,char**argv){
 if(argc<2||argc>4)return 2;
 const char*oraclePath=argc>=3?argv[2]:"build/scene-manager-oracle.bin";
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 willowMotionPutLong(rom+0x80,0x15d0);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 for(unsigned n=0;n<SERVICE_COUNT;n++){
  unsigned at=0x101000+n*256;
  willowMotionPutWord(rom+services[n],0x4ef9);willowMotionPutLong(rom+services[n]+2,at);
#define EMIT(v) do{willowMotionPutWord(rom+at,(v));at+=2;}while(0)
  if(!n){EMIT(0x1b6d);EMIT(0x60);EMIT(0x6a);}
  EMIT(0x36fc);EMIT(n);
  if(services[n]==0x7d80||services[n]==0x7dae||services[n]==0xa5d6)EMIT(0x36c0);
  for(unsigned j=0;j<OBSERVED_COUNT;j++){EMIT(0x36f9);EMIT(0xff);EMIT(observed[j]);}
  EMIT(0x4e75);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen(oraclePath,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={oraclePath,rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0,waits=0;
 const uint16_t timers[]={0,1,2,3,4,5,9,0xffff};
 for(unsigned mode=0;mode<2;mode++)for(unsigned state=0;state<18;state++)for(unsigned stage=0;stage<7;stage++)for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31+(n>>8));
  ww(0x8000,2);ww(0x8004,0);
  const uint16_t states[]={2,4,8,6,10,12,14,16,18,20,22,24,28,30,34,36,38,26};ww(0x8008,states[state]);ww(0x800a,timers[sample&7]);
  ww(0x8344,stage);wb(0x8348,sample&1);wb(0x83a0,sample&2);wb(0x83a3,sample&4);
  wb(0x980d,sample%4);ww(0x9834,0xf800);
  for(unsigned n=0;n<3;n++)ww(0xf800+n*2,0xccc6+n*64);
  wb(0x8060,(stage%4)*16);wb(0x806a,0x80);wb(0xf99c,sample&1?0x80:0);
  ww(0x840c,(sample&16)?0:512);ww(0x0fb2,(sample&32)?0:640);
  wb(0x0e58,(sample&8)?1:0);wb(0x0e6e,(sample&16)?1:0);wb(0x9866,(sample&32)?4:0);
  wb(0x82a6,(sample&8)?0x80:0);wb(0xc0,(sample&16)?4:0);
  ww(0x801a,(sample&32)?252:0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  cursor=0xe000;WillowNativeGameplayFlow flow={0};Delay delay={0};delay.mode=mode;
  uint32_t a4=0xff6780;delay.a4=&a4;unsigned resumes=0;uint8_t result;
  WillowNativeGameplayFlowBinding binding={&flow,&bus,sourceWord,&a4,service,&delay};
  do{result=willowNativeGameplayFlowService(&binding,0x93be,-1);
   if(!result||++resumes>16)return 1;
  }while(result==2);
  if(flow.scene.pc||flow.scene.target||flow.manager.target||delay.phase)return 1;waits+=delay.waits;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(12,0xff6780);m68k_set_reg(11,0xffe000);m68k_set_reg(16,0x93be);retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(11)!=0xff0000+cursor||m68k_get_reg(12)!=a4||m68k_get_reg(15)!=0xffff04){fprintf(stderr,"CPU mismatch state %u sample %u PC %x A3 %x A4 %x\n",state,sample,m68k_get_reg(16),m68k_get_reg(11),m68k_get_reg(12));return 1;}
  for(unsigned n=0;n<65536;n++)if((n<0xfe80||n>=0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Stage entry mismatch state %u stage %u sample %u at %x got %x want %x\n",state,stage,sample,n,ram[n^1],expected[n]);return 1;
  }
  if(argc==4&&!mode&&state==3&&!stage&&!sample){
   FILE*trace=fopen(argv[3],"w");if(!trace)return 2;
   fputs("{\"scope\":\"Original scene manager call boundary; service bodies instrumented, not frame timing\",\"entry_state\":6,\"stage\":0,\"services\":[",trace);
   unsigned at=0xe000,index=0;
   while(at<cursor){
    unsigned id=((unsigned)ram[at^1]<<8)|ram[(at+1)^1];at+=2;if(id>=SERVICE_COUNT)return 1;
    fprintf(trace,"%s{\"address\":%u,\"snapshot\":{",index++?",":"",services[id]);
    for(unsigned n=0;n<OBSERVED_COUNT;n++){
     unsigned value=((unsigned)ram[at^1]<<8)|ram[(at+1)^1];at+=2;
     fprintf(trace,"%s\"%04x\":%u",n?",":"",observed[n],value);
    }
    fputs("}}",trace);
   }
   fputs("]}\n",trace);fclose(trace);
  }
  checks++;
 }
 printf("Scene manager and intro selection: %u original comparisons, %u injected waits; eighteen states, seven stages, intro flags, countdown gates, actual allocation, retained dispatch, service snapshots and non-stack RAM match\n",checks,waits);
 return 0;
}
