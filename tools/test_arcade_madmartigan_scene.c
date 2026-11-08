/* Original D94C scene body; stop at fade/HUD service boundaries.
 * Those services are deliberately outside this component oracle. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_madmartigan_scene.h"
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
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static uint16_t romWord(unsigned long a){return willowMotionWord(rom+a);}
static uint32_t liveA4;
static unsigned serviceCount,pending,waitTicks;
static uint16_t services[3];
static uint8_t snapshots[3][65536];
static uint8_t *allocateEffect(void){
 if(!rb(0x980d))return NULL;
 uint16_t cursor=rw(0x9834),handle=rw(cursor);
 ww(0x9834,cursor+2);wb(0x980d,rb(0x980d)-1);
 liveA4=(uint32_t)(int32_t)(int16_t)handle;return expected+handle;
}
static uint8_t service(void*context,uint16_t address){
 (void)context;
 if(pending)return --pending?2:1;
 if(serviceCount>=3)abort();
 services[serviceCount]=address;memcpy(snapshots[serviceCount++],expected,65536);
 if(address==0x4f44)return 1;
 pending=waitTicks;return 2;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 const uint16_t entries[]={0x1e88,0x4f44,0x1eaa};
 for(unsigned n=0;n<3;n++){
  willowMotionPutWord(rom+entries[n],0x4ef9);willowMotionPutLong(rom+entries[n]+2,0x100010+n*16);
  willowMotionPutWord(rom+0x100010+n*16,0x60fe);
 }willowMotionPutLong(rom+0x80,0x15d0);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/madmartigan-scene-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/madmartigan-scene-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0,boundaries=0;
 for(unsigned stage=0;stage<4;stage++)for(unsigned available=0;available<2;available++)for(unsigned sample=0;sample<1024;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  wb(0x9848,20);wb(0x9849,stage*2);wb(0x9866,4);wb(0x9868,sample);
  ww(0x984c,sample*977);wb(0x980d,available);ww(0x9834,0x9600);ww(0x9600,0xc000);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  WillowVisibilityBus bus={rw,rb,ww,wb};WillowMadmartiganScene scene={0};
  liveA4=0x123456;pending=serviceCount=0;waitTicks=1+sample%4;
  unsigned result=willowMadmartiganSceneStep(&scene,&bus,expected+0x9846,romWord,allocateEffect,service,NULL);
  while(result==2){
   if(!scene.resume||!serviceCount||memcmp(expected,snapshots[serviceCount-1],65536))return 1;
   result=willowMadmartiganSceneStep(&scene,&bus,expected+0x9846,romWord,allocateEffect,service,NULL);
  }
  if(result!=1||scene.resume||pending)return 1;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(0,0);m68k_set_reg(1,0);m68k_set_reg(4,sample*977);m68k_set_reg(12,0x123456);m68k_set_reg(14,0xff9846);m68k_set_reg(16,0xd94c);
  for(unsigned boundary=0;boundary<=serviceCount;boundary++){
   for(unsigned slice=0;slice<16&&m68k_get_reg(16)<0x100000;slice++)retro_run();
   unsigned pc=m68k_get_reg(16);
   if(boundary<serviceCount){
    unsigned slot=services[boundary]==0x1e88?0:services[boundary]==0x4f44?1:2;
    if(pc!=0x100010+slot*16){fprintf(stderr,"service order mismatch %x\n",pc);return 1;}
   }else if(pc!=0x100000)return 1;
   const uint8_t*wanted=boundary<serviceCount?snapshots[boundary]:expected;
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=wanted[n]){
    fprintf(stderr,"Madmartigan scene phase %u available %u timer %u boundary %u RAM %x got %x want %x\n",stage*2,available,sample,boundary,n,ram[n^1],wanted[n]);return 1;
   }
   boundaries++;
   if(boundary<serviceCount){
    unsigned sp=m68k_get_reg(15),offset=sp&65535;
    unsigned ret=((unsigned)ram[offset^1]<<24)|((unsigned)ram[(offset+1)^1]<<16)|((unsigned)ram[(offset+2)^1]<<8)|ram[(offset+3)^1];
    m68k_set_reg(15,sp+4);m68k_set_reg(16,ret);
   }
  }
  if(m68k_get_reg(12)!=liveA4||(uint16_t)m68k_get_reg(4)!=(uint16_t)(sample*977))return 1;
  checks++;
 }
 printf("Madmartigan scene body: %u original-CPU cases, %u service/final boundaries; all non-stack RAM/D4/A4 match. Fade/HUD service bodies excluded.\n",checks,boundaries);
 return 0;
}
