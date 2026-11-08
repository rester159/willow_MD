/* Original BEF8/D94C including HUD, charge, and real command waits. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_arcade_player_bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],original[0x100000];
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
static uint16_t word(uint32_t a){return willowMotionWord(original+a);}
static uint16_t sourceWord(unsigned long a){return word(a);}
static uint32_t liveA4;
static uint8_t *allocateClass12(void){
 if(!rb(0x980d))return NULL;
 uint16_t cursor=rw(0x9834),handle=rw(cursor);ww(0x9834,cursor+2);wb(0x980d,rb(0x980d)-1);
 liveA4=(uint32_t)(int32_t)(int16_t)handle;return expected+handle;
}
static void textWrite(uint32_t a,uint16_t v){if(a<0x910000||a>=0x914000)abort();ww(0x4000+a-0x910000,v);}
static void sound(uint16_t command){
 if(rb(0x8290)&&rb(0x8293))return;
 liveA4=0xff8172;uint16_t cursor=rw(0x801e);ww(0x8172+cursor,command);cursor=(cursor+2)&126;
 ww(0x8172+cursor,255);ww(0x801e,(cursor+2)&126);
}
static uint8_t sceneService(void*context,uint16_t address){return willowArcadeGameplayServiceResume(context,address,0);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(original,rom,sizeof original);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 const unsigned patches[]={0x7d84,0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910200,0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}
 willowMotionPutLong(rom+0x4b72,0xff4170);
 for(unsigned n=0;n<2;n++){
  unsigned at=n?0x1ec2:0x1ea0;if(willowMotionWord(rom+at)!=0x1750)return 2;
  willowMotionPutWord(rom+at,0x300);
 }
 willowMotionPutWord(rom+0x300,0x4ef9);willowMotionPutLong(rom+0x302,0x100000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/madmartigan-scene-controller-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/madmartigan-scene-controller-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0;
 for(unsigned phase=0;phase<4;phase++)for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  wb(0x9846,1);wb(0x9848,20);wb(0x9849,phase*2);wb(0x9866,4);wb(0x9868,sample%4);
  wb(0x9877,(sample>>2)&1);wb(0x83ca,(sample>>3)&1);wb(0x83ea,0);
  wb(0x8366,sample%7);wb(0x8367,sample%5);wb(0x8066,0);wb(0x828f,0);
  for(unsigned n=0;n<12;n++)wb(0x8352+n,(sample>>n)&1);
  wb(0x8362,3);wb(0x8363,5);wb(0x8364,1);wb(0x8342,3);
  for(unsigned n=0;n<4;n++){wb(0x834a+n,0);wb(0x82ca+n,0);wb(0xf03e + n,0);}
  for(unsigned n=0;n<4;n++)wb(0x8368+n,0);
  ww(0x83c6,0x0123);wb(0x980d,sample&1);ww(0x9834,0x9600);ww(0x9600,0xe000);
  ww(0x801a,(sample*4)&252);ww(0x801e,(sample*2)&126);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(0,0);m68k_set_reg(4,0x12345678);m68k_set_reg(12,0x345678);m68k_set_reg(16,0xbef8);
  uint16_t d4=0x5678,wait=0;liveA4=0x345678;
  WillowPlayerCollision collision={0};collision.word=sourceWord;
  WillowArcadeGameplayServices services={.bus=&bus,.busWord=word,.collision=&collision,.textWrite=textWrite,.d4=&d4,.a4=&liveA4,.eventWait=&wait};
  WillowMadmartiganScene scene={0};unsigned delays=1+sample%4;
  unsigned suspended=(phase==0||(phase==2&&sample%4==1));
  for(unsigned step=0;step<=(suspended?delays:0);step++){
   if(step){
    /* Real outside-task input changes must not regenerate BEF8's edges. */
    wb(0x806a,rb(0x806a)^255);ram[0x806a^1]=rb(0x806a);
    wb(0x8066,128);ram[0x8066^1]=128;wb(0x828f,1);ram[0x828f^1]=1;
   }
   if(step==delays){wb(0xf891,0);ram[0xf891^1]=0;}
   WillowPlayerRecord player={0};WillowPlayerInput inputState={0};WillowPlayerShots shots={0};WillowPlayerHealth health={0};
   WillowPlayerCharge charge={0};charge.textWrite=textWrite;charge.sound=sound;
   WillowPlayerScript script={0};WillowPlayerSceneBinding sceneBinding={&scene,sceneService,&services};
   uint8_t result=willowArcadePlayerControllerWithScene(&bus,&player,&inputState,&collision,&shots,&health,&charge,&script,allocateClass12,0,0,&sceneBinding);
   retro_run();if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"PC %x\n",m68k_get_reg(16));return 1;}
   unsigned done=!suspended||step==delays;
   if(result!=(done?1:2)){fprintf(stderr,"Result phase %u sample %u step %u got %u\n",phase,sample,step,result);return 1;}
   for(unsigned n=0;n<65536;n++)if(n<0xfe80||n>=0xff04){
    if(ram[n^1]!=expected[n]){fprintf(stderr,"Scene phase %u sample %u step %u mismatch %04x actual %02x expected %02x\n",phase,sample,step,n,ram[n^1],expected[n]);return 1;}
   }
   if((uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=liveA4){fprintf(stderr,"Registers phase %u sample %u step %u D4 %x/%x A4 %x/%x\n",phase,sample,step,m68k_get_reg(4),d4,m68k_get_reg(12),liveA4);return 1;}
   if(!done){
    unsigned sp=m68k_get_reg(15);uint32_t pc=0;for(unsigned n=0;n<4;n++)pc=(pc<<8)|ram[((sp+n)&65535)^1];
    if(pc!=wait)return 1;m68k_set_reg(15,sp+4);m68k_set_reg(16,pc);
   }else if(scene.resume||wait||m68k_get_reg(15)!=0xffff04)return 1;
   checks++;
  }
 }
 printf("Madmartigan scene controller: %u original-CPU boundaries; actual fade commands/HUD/charge, variable waits, input-edge suspension, RAM/D4/A4 match\n",checks);
 return 0;
}
