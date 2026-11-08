/* Original-instruction demo active prefix and complete exit verification. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_tick.h"
#include "../src/willow_arcade_attract_demo_update.h"
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

static uint16_t romWord(uint32_t at){return willowMotionWord(rom+at);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


static void textWrite(uint32_t at,uint16_t value){if(at<0x910000||at>=0x914000)abort();ww(0x4000+at-0x910000,value);}
static const WillowVisibilityBus bus={rw,rb,ww,wb};
static unsigned calls[8],callCount;
static uint32_t liveA4;
#ifdef WILLOW_TEST_DEMO_UPDATE_ROUTING
static unsigned suspended;
static const unsigned delegated[]={0x9ef2,0x39a52,0x3807c,0x3b524};
#endif
static uint8_t service(void*context,uint32_t at,int argument)
{
 (void)context;if(argument!=-1||callCount>=8)abort();calls[callCount++]=at;
 if(at==0x1f88){willowArcadeGameplayInput(&bus,romWord);return 1;}
 if(at==0x4ea2){willowArcadeGameplayClockWithA4(&bus,textWrite,&liveA4);return 1;}
 if(at==0x785e){
  uint16_t cursor=rw(0x801e);liveA4=0xff8172;
  ww(0x8172+cursor,0xf4);cursor=(cursor+2)&126;
  ww(0x8172+cursor,255);ww(0x801e,(cursor+2)&126);return 1;
 }
#ifdef WILLOW_TEST_DEMO_UPDATE_ROUTING
 unsigned index=0;while(index<4&&delegated[index]!=at)index++;
 if(index==4)abort();
 if(!suspended){suspended=1;return 2;}suspended=0;
 uint32_t cursor=((uint32_t)rw(0xf504)<<16)|rw(0xf506);
 ww((uint16_t)cursor,index+1);cursor+=2;ww(0xf504,cursor>>16);ww(0xf506,cursor);return 1;
#else
 /* Stop before the unbound actor chain, never report it complete. */
 return 2;
#endif
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
#ifdef WILLOW_TEST_DEMO_UPDATE_ROUTING
 for(unsigned n=0;n<4;n++){
  uint32_t at=0x101000+n*32;
  willowMotionPutWord(rom+delegated[n],0x4ef9);willowMotionPutLong(rom+delegated[n]+2,at);
  /* Instrumented leaves log in order, then return. Gameplay effects are
   * deliberately outside this routing-only oracle. A0 is a scratch register. */
  willowMotionPutWord(rom+at,0x2079);willowMotionPutLong(rom+at+2,0xfff504);
  willowMotionPutWord(rom+at+6,0x30fc);willowMotionPutWord(rom+at+8,n+1);
  willowMotionPutWord(rom+at+10,0x23c8);willowMotionPutLong(rom+at+12,0xfff504);
  willowMotionPutWord(rom+at+16,0x4e75);
 }
#else
 willowMotionPutWord(rom+0x9ef2,0x60fe);
#endif
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 const uint32_t patches[]={0x4f1a,0x4f30};
 for(unsigned n=0;n<2;n++){
  uint32_t at=willowMotionLong(rom+patches[n]);
  if(at<0x910000||at>=0x914000)return 2;
  willowMotionPutLong(rom+patches[n],0xff4000+at-0x910000);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/demo-update-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/demo-update-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0;
 for(unsigned sample=0;sample<2048;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  unsigned exiting=sample&1;
  ww(0x8004,6);ww(0x8008,4);ww(0x8344,(sample/2)%6);
  ww(0x800a,exiting?(sample&2?1:17):(sample&2?0:900));
  wb(0x83c2,exiting&&!(sample&2)?1:0);wb(0xc0,sample&4?4:0);
  ww(0x83c6,sample&8?0x123:0);ww(0x83d2,0);wb(0x83e9,0);wb(0x83cf,0);wb(0x83e2,0);
  wb(0x83c8,(sample/16)%3);wb(0xf99e,0);wb(0x8019,sample>>3);
  wb(0x8290,sample&16);wb(0x8293,sample&32);ww(0x801e,(sample*2)&126);
  ww(0x8070,(sample/32)%4);wb(0x8292,sample&8);wb(0x828e,sample>>4);
  uint32_t pointer=0x8cca+((sample>>6)&1)*8+4;
  pointer=((uint32_t)romWord(pointer)<<16)|romWord(pointer+2);
  ww(0x806c,pointer>>16);ww(0x806e,pointer);
#ifdef WILLOW_TEST_DEMO_UPDATE_ROUTING
  ww(0xf504,0xff);ww(0xf506,0xf600);suspended=0;
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t initialA4=0xffb000;liveA4=initialA4;callCount=0;
  uint16_t d1=sample*311,d2=sample*977;
  WillowArcadeAttractDemoUpdate task={0};
  unsigned result=willowArcadeAttractDemoUpdateResume(&task,&bus,romWord,service,0,&d1,&d2);
  if(exiting){if(result!=1||task.pc||callCount!=1||calls[0]!=0x785e)return 1;}
  else if(result!=2||task.pc!=0x8dfc||callCount!=3||calls[0]!=0x1f88||calls[1]!=0x4ea2||calls[2]!=0x9ef2)return 1;
#ifdef WILLOW_TEST_DEMO_UPDATE_ROUTING
  for(unsigned n=0;result==2&&n<4;n++){
   callCount=0;result=willowArcadeAttractDemoUpdateResume(&task,&bus,romWord,service,0,&d1,&d2);
  }
  if(result!=1||task.pc)return 1;
  unsigned stop=0x100000;
#else
  unsigned stop=exiting?0x100000:0x9ef2;
#endif
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x8de4);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Demo update boundary sample %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Demo update sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if(m68k_get_reg(12)!=liveA4){fprintf(stderr,"Demo A4 sample %u got %x expected %x\n",sample,m68k_get_reg(12),liveA4);return 1;}
  if(exiting&&((uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2))return 1;
#ifndef WILLOW_TEST_DEMO_UPDATE_ROUTING
  if(!exiting){
   uint8_t saved[65536];memcpy(saved,expected,sizeof saved);callCount=0;
   if(willowArcadeAttractDemoUpdateResume(&task,&bus,romWord,service,0,&d1,&d2)!=2||task.pc!=0x8dfc||callCount!=1||calls[0]!=0x9ef2||memcmp(saved,expected,sizeof saved))return 1;
  }
 #endif
  checks++;
 }
 for(unsigned bad=0;bad<7;bad++){
  ww(0x8004,6);ww(0x8008,4);ww(0x8344,0);
  if(bad==0)ww(0x8004,4);
  if(bad==1)ww(0x8008,2);
  if(bad==2)ww(0x8344,6);
  uint8_t saved[65536];memcpy(saved,expected,sizeof saved);
  WillowArcadeAttractDemoUpdate task={bad==3?0x8df0:0},before=task;
  uint16_t d1=0x1234,d2=0xabcd;callCount=0;
  if(willowArcadeAttractDemoUpdateResume(&task,&bus,bad==4?0:romWord,bad==5?0:service,0,bad==6?0:&d1,&d2)||
     task.pc!=before.pc||d1!=0x1234||d2!=0xabcd||callCount||memcmp(saved,expected,sizeof saved))return 1;
 }
 puts("Demo update: seven unavailable/invalid entry contexts preserve state and live words");
#ifdef WILLOW_TEST_DEMO_UPDATE_ROUTING
 printf("Demo update routing: %u original-instruction cases with instrumented actor/collision/stage/publication leaves; call order and full non-stack RAM match after suspension at every delegated call; delegated gameplay effects excluded\n",checks);
#else
 printf("Demo update: %u source-instruction cases; active timer/input/clock prefix and complete timeout/end exit match all non-stack RAM and A4; exit D1/D2 and occupied/free message slot exact; actor boundary remains suspended, not implemented by this test\n",checks);
#endif
 return 0;
}
