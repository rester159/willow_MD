/* Original E0DA Madmartigan death and scrolling death sequences. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_madmartigan_death.h"
#include "../src/willow_arcade_sound_queue.h"
#include "../src/willow_arcade_player_dispatch.h"
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
static const WillowVisibilityBus bus={rw,rb,ww,wb};
static uint32_t a4;
static void sound(uint16_t v){willowArcadeSoundQueueCommandWithA4(&bus,v,&a4);}
static uint16_t paletteRead(unsigned n){return rw(0x2000+n*2);}
static void paletteWrite(unsigned n,uint16_t v){ww(0x2000+n*2,v);}
static void cancelTask(uint16_t at){willowArcadeCancelTaskWithA4(expected+at,&a4);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x88,0x1690);
 if(willowMotionLong(rom+0xc242)!=0x91c000)return 2;
 willowMotionPutLong(rom+0xc242,0xff2000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/madmartigan-death-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/madmartigan-death-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0;
 for(unsigned stage=0;stage<5;stage++)for(unsigned state=0;state<12;state++)for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  wb(0x9848,4);wb(0x9849,stage*2);wb(0x9866,4);wb(0x9855,sample%2);
  wb(0x9868,sample);ww(0x83da,state*256+sample);ww(0x8408,sample*311);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);ww(0x801e,(sample%64)*2);
  uint8_t *player=expected+0x9846;
  if(stage<2)willowPlayerAnimationSelect(player,0xe658,romWord);
  else willowArcadeScaledAnimationSet(&bus,player,0xe600,romWord);
  willowMotionPutWord(player+28,(sample%3)?1:0);
  willowMotionPutLong(player+6,((uint32_t)(sample*977)<<16)|sample);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  WillowPlayerInput inputs={0};inputs.sound=sound;
  WillowPlayerCollision collision={0};collision.word=romWord;
  a4=0x123456;
  if(stage==4){
   WillowPlayerHealth health={0};health.paletteRead=paletteRead;health.paletteWrite=paletteWrite;
   WillowPlayerCharge charge={0};charge.value=rw(0x8376);
   WillowPlayerScript script={0};script.removed=rb(0x83c2);script.cancelTask=cancelTask;
   willowPlayerRemove(player,&collision,&health,&charge,&script);
   ww(0x8376,charge.value);wb(0x83c2,script.removed);
  }else if(!willowMadmartiganDeath(&bus,player,&inputs,&collision))return 1;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(0,0);m68k_set_reg(1,0);m68k_set_reg(4,sample*977);m68k_set_reg(12,0x123456);m68k_set_reg(14,0xff9846);m68k_set_reg(16,stage==4?0xe172:0xe0da);
  for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Return stage %u speed %u sample %u PC %x\n",stage,state,sample,m68k_get_reg(16));return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Madmartigan death substate %u speed %u timer %u RAM %x got %x want %x\n",stage*2,state,sample,n,ram[n^1],expected[n]);return 1;
  }
  if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=(uint16_t)(sample*977)){fprintf(stderr,"Registers stage %u speed %u sample %u A4 %x/%x D4 %x\n",stage,state,sample,m68k_get_reg(12),a4,m68k_get_reg(4));return 1;}
  checks++;
 }
 printf("Madmartigan death: %u original-CPU cases; all four death substates and removal, twelve speed indices, timer wrap, sound/palette/task state and all non-stack RAM/D4/A4 match\n",checks);
 return 0;
}
