/* Headless differential verification of class-18 entry and shared palette allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_effect_item_pass.h"
#include "../src/willow_arcade_pool_init.h"
#ifdef WILLOW_TEST_ACTOR18_POOL
#include "../src/willow_arcade_actor18_pool.h"
#endif
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



static uint16_t sourceWord(unsigned long at){return (at&0xff0000)==0xff0000?rw(at):willowMotionWord(rom+at);}
static uint8_t*resolve(uint16_t at){return expected+at;}


static uint8_t attribute(uint16_t at){return rb(0x4000+at+2);}
static void textWrite(uint32_t at,uint16_t v){ww(0x2000+at-0x910000,v);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 if(willowMotionLong(rom+0x6772)!=0x910010)return 2;
 willowMotionPutLong(rom+0x6772,0xff4010);willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x4ad0,0xff4310);willowMotionPutLong(rom+0x4ad8,0xff4310);
 willowMotionPutLong(rom+0x3e44,0xff4000);
 willowMotionPutLong(rom+0x4365a,0xff4000);
 willowMotionPutLong(rom+0x4e34,0xff2c0c);willowMotionPutLong(rom+0x4e4a,0xff2d0c);willowMotionPutLong(rom+0x4e5e,0xff2e0c);
 willowMotionPutWord(rom+0x100100,0x4eb9);willowMotionPutLong(rom+0x100102,0x2f39e);
 willowMotionPutWord(rom+0x100106,0x4eb9);willowMotionPutLong(rom+0x100108,0x36c7e);willowMotionPutWord(rom+0x10010c,0x4e75);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scripted-effects-items-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scripted-effects-items-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0,completed=0,phases=0,rewarded=0,floorIndex=0;
 for(unsigned n=4;n<256;n+=4)if((sourceWord(0x4712UL+n*2)&0x8df)==0x88f){floorIndex=n;break;}
 if(!floorIndex)return 2;
 for(unsigned sample=0;sample<128;sample++){
  memset(expected,0,sizeof expected);willowArcadePoolsInit(&bus);
  for(unsigned n=0;n<0x4000;n+=4)wb(0x4000+n+2,floorIndex);
  ww(0x8408,15400);ww(0x840c,0);ww(0xf06,sample&1?86:0);
  ww(0x801e,(sample%64)*2);wb(0x8290,(sample/2)&1);wb(0x8293,(sample/4)&1);
  willowPlayerAnimationSet(resolve(0x9846),0xe5a8,sourceWord);
  unsigned count=2+(sample%30)*2;
  for(unsigned n=0;n<count;n++){
   uint16_t actor=willowActor18Pop(&bus,0x980d,0x9834);uint8_t*r=resolve(actor);
   r[0]=1;r[32]=n%2?27:25;r[15]=0;
   willowMotionPutLong(r+6,((uint32_t)(r[32]==25?15400+n%30:15760)<<16)|sample*311);
   willowMotionPutLong(r+10,((uint32_t)(64+sample%16)<<16)|sample*977);
  }
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};uint16_t d4=0x1234;
  unsigned step;
  for(step=0;step<400;step++){
   wb(0x8019,step+sample*13);
   willowArcadeVisibilityInit(&bus,0,0);
   for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
   if(!willowArcadeEffectItemPass(&bus,resolve,&collision,0,textWrite,&d4))return 1;
   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(4,0x1234);
   m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100100);
   for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04)return 1;
   if((uint16_t)m68k_get_reg(4)!=d4)return 1;
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Scripted movement sample %u frame %u at %x actual %x expected %x\n",sample,step,n,ram[n^1],expected[n]);return 1;
   }
   for(unsigned n=0;n<60;n++){uint8_t*r=resolve(0xccc6+n*64);if(r[0]&&r[32]==25&&r[2]==2)phases|=1u<<(r[3]/2);}
   checks++;if(rb(0x980d)==60&&rb(0x9813)==20)break;
  }
  if(step==400){fprintf(stderr,"Unfinished population %u: effect slots %u item slots %u\n",sample,rb(0x980d),rb(0x9813));return 1;}
  if(willowMotionLong(expected+0x8368))rewarded++;
  completed++;
 }
 if(phases!=63||rewarded!=64){fprintf(stderr,"Missing movement phases %x\n",phases);return 1;}
 printf("Scripted effects/item pass: %u original non-stack RAM/D4 comparisons across %u populations; all six phases, floor correction, class-16 allocation, player position/state publication, source-produced scene handshake, gold collection, HUD writes and restoration of both actor pools\n",checks,completed);
 return 0;
}
