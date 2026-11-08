/* Headless differential verification of class-18 entry and shared palette allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_projectile_targets.h"
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
 f=fopen("build/projectile-targets-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/projectile-targets-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0,floorIndex=0,actions=0,parentFlags=0;
 for(unsigned n=4;n<256;n+=4)if((sourceWord(0x4712UL+n*2)&0x8df)==0x88f){floorIndex=n;break;}
 if(!floorIndex)return 2;
 for(unsigned sample=0;sample<65536;sample++){
  memset(expected,0,sizeof expected);willowArcadePoolsInit(&bus);
  uint16_t actor=willowActor18Pop(&bus,0x9807,0x9828);uint8_t*r=resolve(actor);
  r[0]=1;r[32]=(sample%2)*8;r[33]=0;r[2]=((sample/2)%4)*2;r[3]=((sample/8)%3)*2;r[15]=(sample/84)%2;r[14]=(sample/168)%2*2;
  willowMotionPutLong(r+6,((uint32_t)(sample*17)<<16)|sample);willowMotionPutLong(r+10,((uint32_t)(sample*31)<<16)|sample);
  r[34]=sample/336;r[19]=4;r[42]=(sample/7)%7;r[43]=(sample/11)%7;willowMotionPutWord(r+44,sample*311);willowMotionPutWord(r+46,sample*977);willowMotionPutLong(r+24,r[32]==3?0xed0bc:0xed0a4);
  willowPlayerAnimationSet(r,0xf4d0,sourceWord);
  wb(0x83d6,sample/24);wb(0x8349,sample/48);
  ww(0x8408,willowMotionWord(r+6)+(sample%600)-300);ww(0x840c,willowMotionWord(r+10)+(sample%400)-200);wb(0x8019,sample/3);
  wb(0x8290,sample&1);wb(0x8293,(sample/2)&1);ww(0x801e,(sample%64)*2);wb(0xf99d,sample/31);
  for(unsigned n=0;n<0x4000;n+=4)wb(0x4000+n+2,sample/1024*4);

  willowArcadeVisibilityInit(&bus,0,0);
  willowMotionPutWord(r+48,sample%101);willowMotionPutWord(r+50,(sample%101)*2);willowMotionPutWord(r+52,(sample/2)%101);willowMotionPutWord(r+54,((sample/2)%101)*2);
  wb(0x8019,sample/4);ww(0x9830,0x9000);ww(0x9832,0x9100);wb(0x980b,sample%21);wb(0x980c,(sample/21)%21);
  for(unsigned n=0;n<20;n++){
   uint16_t at=0xc7c6+n*64;ww(0x9000+n*2,at);ww(0x9100+n*2,0xc7c6+(19-n)*64);
   wb(at,1+(sample/24+n)%3);wb(at+32,(sample+n)%24);wb(at+3,6);wb(at+4,8);
   ww(at+6,willowMotionWord(r+6)+(sample%257)-128+n);ww(at+10,willowMotionWord(r+10)+((sample/257)%257)-128+n);
   ww(at+20,0xff);ww(at+22,0xb000+n*8);ww(0xb006+n*8,(sample+n)%256);
  }
  ww(0x8344,(sample/5)%6);wb(0xb821,(sample/6)%32);
  for(unsigned n=0;n<20;n++){uint16_t at=0xc7c6+n*64;wb(at+33,(sample/7)%2);ww(at+38,0xff);ww(at+40,0xb800);}
  if(sample%8==0){
   static const uint8_t types[]={0,7,8,10};unsigned which=(sample/8)%4;r[32]=which==3?8:0;
   wb(0x980b,1);wb(0x980c,1);ww(0x9000,0xc7c6);ww(0x9100,0xc7c6);wb(0xc7c6,1);wb(0xc7c6+32,types[which]);wb(0xc7c6+33,0);
   ww(0xc7c6+6,willowMotionWord(r+6));ww(0xc7c6+10,willowMotionWord(r+10));ww(0xb006,0);
   willowMotionPutWord(r+48,80);willowMotionPutWord(r+50,160);willowMotionPutWord(r+52,80);willowMotionPutWord(r+54,160);
  }
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t entry=(sample&4)?0x70ae:0x709c;uint32_t a4=0x123456;
  if(!willowArcadeProjectileTargetsWithA4(&bus,r,(sample>>2)&1,sourceWord,&a4))return 1;
  if(a4!=0x123456)parentFlags++;
  for(unsigned n=0;n<20;n++){uint16_t at=0xc7c6+n*64;if(ram[at^1]==1&&rb(at)==2){uint8_t action=willowActor18Byte(sourceWord,(r[32]?0x7102UL:0x7122UL)+rb(at+32));actions|=1u<<action;}}
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(7,sample%48);m68k_set_reg(0,0);m68k_set_reg(4,0x1234);m68k_set_reg(15,0xffff00);m68k_set_reg(12,0x123456);m68k_set_reg(16,entry);
  for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04)return 1;
  if((uint16_t)m68k_get_reg(4)!=0x1234||m68k_get_reg(12)!=a4){fprintf(stderr,"Register sample %u A4 %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Class-8 projectile sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 if(actions!=85||!parentFlags){fprintf(stderr,"Response coverage %x parent flags %u\n",actions,parentFlags);return 1;}
 printf("Projectile target scans: %u original non-stack RAM/D4/A4 comparisons across both source eligibility tables, geometry, lane order, stage gates, parent flags and first-hit response\n",checks);
 return 0;
}
