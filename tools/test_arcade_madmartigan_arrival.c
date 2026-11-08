/* Original D5A8/D732/D80E/DA0A scripted states, including health and bounds. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_madmartigan_script.h"
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
static uint8_t map[16384];
static uint8_t attribute(uint16_t at){return rb(0x4002+at);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x88,0x1690);
 if(willowMotionLong(rom+0xc242)!=0x91c000)return 2;
 willowMotionPutLong(rom+0xc242,0xff2000);
 if(willowMotionLong(rom+0x3e44)!=0x918000)return 2;
 willowMotionPutLong(rom+0x3e44,0xff4000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/madmartigan-arrival-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/madmartigan-arrival-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 f=fopen("build/stage0-collision-window.bin","rb");if(!f||fread(map,1,sizeof map,f)!=sizeof map)return 2;fclose(f);
 unsigned checks=0;
 for(unsigned route=0;route<4;route++)for(unsigned stage=0;stage<(route==1?4:route==2?3:2);stage++)for(unsigned state=0;state<(route?1:3);state++)for(unsigned sample=0;sample<4096;sample++){
  const unsigned states[]={12,14,16,22},entries[]={0xd5a8,0xd732,0xd80e,0xda0a};
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  memcpy(expected+0x4000,map,sizeof map);
  wb(0x9848,states[route]);wb(0x9849,stage*2);wb(0x984a,state*2);wb(0x9866,4);wb(0x9855,sample%4);
  wb(0x9868,sample);ww(0x8408,sample*311);ww(0x845c,0x8000);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);ww(0x801e,(sample%64)*2);
  wb(0xf99e,1);wb(0x8362,5);wb(0x8365,5);wb(0x8364,0);wb(0x8389,0);
  uint8_t *player=expected+0x9846;
  willowPlayerAnimationSelect(player,stage?0xe428:0xe194,romWord);
  willowMotionPutWord(player+28,(sample%3)?1:0);
  willowMotionPutLong(player+24,0xc01c);
  wb(0x8470,sample&1);wb(0x838a,(sample>>1)&1);wb(0x82a6,(sample>>2)&1);
  willowMotionPutLong(player+6,((uint32_t)(sample%2048)<<16)|sample);
  willowMotionPutLong(player+10,((uint32_t)(32+(sample%24)*16)<<16)|sample);
  willowMotionPutWord(player+52,56);willowMotionPutWord(player+56,sample*977);
  willowMotionPutWord(player+58,sample*311);player[36]=sample&1;
  player[71]=0;willowMotionPutWord(player+54,0);willowMotionPutWord(player+80,0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  WillowPlayerInput inputs={0};inputs.sound=sound;
  WillowPlayerCollision collision={romWord,attribute,rw(0xa32),rw(0xa34),rb(0xa3a),rb(0xa3b)};
  WillowPlayerHealth health={0};health.hp=health.cachedHp=5;health.rebound=1;
  a4=0x123456;
  if(!(route?willowMadmartiganScriptWalk(&bus,player,&collision,&health,rw(0x845c),rw(0x8408)):willowMadmartiganScriptArrival(&bus,player,&inputs,&collision,&health,rw(0x845c),rw(0x8408))))return 1;
  ww(0xa32,collision.x);ww(0xa34,collision.y);wb(0xa3a,collision.firstContact);wb(0xa3b,collision.firstMaterial);
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(0,0);m68k_set_reg(1,0);m68k_set_reg(4,sample*977);m68k_set_reg(12,0x123456);m68k_set_reg(14,0xff9846);m68k_set_reg(16,entries[route]);
  for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Return stage %u speed %u sample %u PC %x\n",stage,state,sample,m68k_get_reg(16));return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Madmartigan script route %u phase %u sample %u RAM %x got %x want %x\n",route,stage*2+state,sample,n,ram[n^1],expected[n]);return 1;
  }
  if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=(uint16_t)(sample*977)){fprintf(stderr,"Registers stage %u speed %u sample %u A4 %x/%x D4 %x\n",stage,state,sample,m68k_get_reg(12),a4,m68k_get_reg(4));return 1;}
  checks++;
 }
 printf("Madmartigan scripted arrival/walk: %u original-CPU cases; states 12/14/16/22, timer/velocity wrap, floor collision, sound and all non-stack RAM/D4/A4 match\n",checks);
 return 0;
}
