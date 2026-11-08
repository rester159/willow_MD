/* Headless differential verification of complete class-14 pickup handlers 2CFC2/2CFD6. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor14_pickups.h"
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
 willowMotionPutWord(rom+0x8474,0x60fe);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 willowMotionPutLong(rom+0x4a36,0xff230c);willowMotionPutLong(rom+0x4a40,0xff230c);
 willowMotionPutLong(rom+0x3e44,0xff4000);
 willowMotionPutLong(rom+0x4e34,0xff2c0c);willowMotionPutLong(rom+0x4e4a,0xff2d0c);willowMotionPutLong(rom+0x4e5e,0xff2e0c);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor14-pickups-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor14-pickups-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;

 const uint32_t entries[]={0x2cfc2,0x2cfd6};
 const uint16_t speeds[]={0,1,31,32,33,767,768,895,896,1024,1535,1536,1664,0x7fff,0x8000,0xffff};
 const uint16_t ages[]={0,1,299,300,301,0xffff};
 for(unsigned entryIndex=0;entryIndex<2;entryIndex++)
 for(unsigned sample=0;sample<32768;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint32_t pc=entries[entryIndex];uint8_t*r=resolve(0xeac6);
  r[0]=1+sample%3;r[1]=(sample/3)&1;r[2]=((sample/5)%4)*2;r[3]=((sample/7)%3)*2;
  r[19]=0;r[30]=14;r[14]=(sample&1)*2;r[15]=(sample/5)&1;
  r[33]=(sample/11)%12;r[34]=(sample/13)%5;r[36]=(sample/17)&1;r[46]=(sample/19)%16;r[47]=(sample/23)&1;r[54]=(sample/29)&1;
  uint16_t x=sample&8?0xfff8:0x200,y=sample&16?0xfff8:0x100;
  ww(0xeacc,x);ww(0xead0,y);willowMotionPutLong(r+20,0x2d35c);ww(0xeae2,(sample/31)%4);
  ww(0xeaf2,speeds[(sample/37)%16]);ww(0xeaf6,speeds[(sample/41)%16]);ww(0xeaf8,speeds[(sample/43)%16]);ww(0xeafa,ages[(sample/47)%6]);
  for(unsigned group=0;group<4;group++)willowArcadeVisibilityInit(&bus,group,0);
  ww(0x8408,x-128);ww(0x840c,y-64);wb(0x8019,sample/53);
  wb(0x83cf,(sample/59)&1);wb(0x83dd,(sample/61)&1);ww(0x83da,speeds[(sample/67)%16]);
  ww(0x984c,x+(int)((sample/71)%601)-300);ww(0x9850,y+(int)((sample/73)%101)-50);
  ww(0x985a,0xff);ww(0x985c,0x1000);ww(0x1006,(sample/79)%3);
  ww(0x0f00,0xff);ww(0x0f02,0x1100);ww(0x0f04,sample&1?0xffff:sample%5);
  ww(0x1100,0x100+sample%12);ww(0x1102,0x234);ww(0x1104,0x123);
  willowMotionPutLong(expected+0x8368,sample&1?0x99999998:0x123456);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);ww(0x801e,(sample*2)&126);
  ww(0x983a,0xf640);wb(0x9810,sample%60);
  wb(0x9805,(sample/3)%5);wb(0x9806,(sample/13)%5);ww(0x9824,0xf600);ww(0x9826,0xf620);
  for(unsigned group=0;group<2;group++)for(unsigned n=0;n<4;n++){
   uint16_t at=0xd000+(group*4+n)*64;ww(0xf600+group*32+n*2,at);
   wb(at,(sample/5+n)%3);wb(at+32,(sample/17+group*4+n)%24);wb(at+33,(sample/19+n)%8);
   ww(at+6,x+(int)((sample/23+n*11+group*7)%193)-96);
   ww(at+10,y+(int)((sample/29+n*3+group*5)%81)-40);
  }
  for(unsigned n=0;n<0x4000;n+=4)wb(0x4000+n+2,sample+((n/4)%3)*4);
  uint8_t ordinal=(sample/11)%60;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
  uint32_t initialA4=sample&1?0xffb000:0x123456,a4=initialA4;
  if(!willowArcadeActor14PickupWithA4(&bus,resolve,0xeac6,ordinal,pc,&collision,textWrite,&a4)){fprintf(stderr,"Unsupported fixture %u\n",sample);return 1;}
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(14,0xffeac6);m68k_set_reg(10,0xff9900);m68k_set_reg(7,ordinal);m68k_set_reg(4,(uint16_t)(sample*73));m68k_set_reg(0,0);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  if(m68k_get_reg(12)!=a4){fprintf(stderr,"Pickup A4 entry %x sample %u actual %x expected %x\n",pc,sample,m68k_get_reg(12),a4);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Actor14 pickup mismatch entry %x sample %u at %x actual %x expected %x\n",pc,sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 printf("Actor14 pickups 2CFC2/2CFD6: %u original non-stack RAM/A4 comparisons; bounce physics, platforms, terrain, reflection, expiry, direct/timed collection, double rewards, streamed records and release match\n",checks);
 return 0;
}
