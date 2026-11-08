/* Headless original-code comparison of selector twentyfour entry at 1DE52. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor4_twentyfour_entry.h"
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
static void textWrite(uint32_t at,uint16_t value){ww((uint16_t)(at-0x910010+0x4010),value);}
static uint8_t*resolve(uint16_t at){return expected+at;}


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
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor4-twentyfour-entry-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor4-twentyfour-entry-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0;
 for(unsigned sample=0;sample<65536;sample++){
  memset(expected,0,sizeof expected);willowArcadePoolsInit(&bus);
  uint16_t actor=willowActor18Pop(&bus,0x9801,0x981c);uint8_t*r=resolve(actor);
  unsigned mix=sample;
  for(unsigned n=1;n<128;n++)r[n]=(sample*13+n*7)&255;
  r[0]=1;r[14]=(mix/24)%3*2;r[30]=4;r[32]=24;r[33]=(mix/4)%256;r[2]=0;r[3]=mix%4*2;r[15]=(mix/2)%2;
  uint16_t x=mix%13?400:0xfff0,y=mix%17?200:0xfff0;
  willowMotionPutLong(r+6,((uint32_t)x<<16)|sample);willowMotionPutLong(r+10,((uint32_t)y<<16)|sample);
  ww(0x8408,x-160);ww(0x840c,y-100);wb(0x83dd,(mix/8)%3);
  if(mix%5==0)wb(0x980d,0);
  uint16_t next=rw(rw(0x9834));
  for(unsigned n=1;n<64;n++)wb(next+n,(sample*17+n*11)&255);
  static const int cameraX[]={-65,-64,-63,-33,-32,-31,0,416,417,447,448,449};
  static const int cameraY[]={-49,-48,-47,-1,0,272,273,319,320,321};
  ww(0x8408,x-cameraX[(mix/3)%12]);ww(0x840c,y-cameraY[(mix/36)%10]);
  ww(0x8284,sample*997+0x1234);r[1]=(mix/6)%2;
  for(unsigned n=0;n<4096;n++)wb(0x4002+n*4,(n*17+sample/7)%64*4);
  r[34]=(mix/8)%4;wb(0x838a,(mix/32)%2);wb(0x82a6,(mix/64)%2);wb(0xc0,(mix/128)%2);
  wb(0x8290,(mix/256)%2);wb(0x8293,(mix/512)%2);ww(0x801e,mix%64*2);ww(0x801a,mix%64*4);
  for(unsigned n=2;n<32;n++)wb(0xc0+n,mix+n*13);
  willowArcadeVisibilityInit(&bus,0,0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t entry=0x1de52,a4=0x123456;uint16_t d4=0x1234;
  uint8_t ok=willowActor4TwentyFourEntry(&bus,r,actor,sourceWord,textWrite,&a4);
  if(!ok)return 1;

  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(7,sample%50);m68k_set_reg(0,0);m68k_set_reg(4,0x1234);m68k_set_reg(15,0xffff00);m68k_set_reg(12,0x123456);m68k_set_reg(16,entry);
  for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04){fprintf(stderr,"Oracle sample %u PC %x SP %x\n",sample,m68k_get_reg(16),m68k_get_reg(15));return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Class-4 actor sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if((uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=a4){fprintf(stderr,"Register sample %u A4 %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  checks++;
 }
 printf("Class-4 selector twentyfour-entry: %u original non-stack RAM/D4/A4 comparisons across four entry states, visibility/message gates, task retention, sound, text clear and boss health display\n",checks);
 return 0;
}
