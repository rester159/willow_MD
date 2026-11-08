/* Headless differential verification of complete 39A52 collision manager. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_collision_manager.h"
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


static uint16_t sourceWord(uint32_t a){return (a&0xff0000)==0xff0000?rw(a):willowMotionWord(rom+a);}
static uint16_t inventoryWord(unsigned long a){return sourceWord(a);}
static void textWrite(uint32_t a,uint16_t v){ww(0x4000+(a-0x910000),v);}
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
 willowMotionPutLong(rom+0x4d6c,0xff4df0);
 willowMotionPutLong(rom+0x4da8,0xff4df0);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/collision-manager-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/collision-manager-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 WillowPlayerCollision collision={inventoryWord,NULL,0,0,0,0};

 for(unsigned sample=0;sample<32768;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint32_t pc=0x39a52;
  ww(0x8344,(sample/4)%7);wb(0x8019,sample);wb(0xf99e,(sample/29)%2);
  ww(0x985a,0xff);ww(0x985c,0x3000);ww(0x3006,(sample/19)%3);
  wb(0x9846,sample%7?1:(sample/7)%4);ww(0x984c,0x200);ww(0x9850,0x100);
  ww(0x989a,(sample/17)%3*8);wb(0x8364,(sample/16)%3);wb(0x8362,sample);
  wb(0x835c,sample&32);wb(0x835a,sample&64);wb(0x835b,sample&128);wb(0x835d,(sample/5)%6);
  const uint16_t counts[]={0x9802,0x9803,0xf89a,0x9805,0x9806,0x9808,0x980b,0x980c,0xf48d,0xf48e};
  const uint16_t pointers[]={0x981e,0x9820,0xf898,0x9824,0x9826,0x982a,0x9830,0x9832,0xf488,0xf48a};
  for(unsigned group=0;group<10;group++){
   wb(counts[group],(sample/(group+1)+group)%5);ww(pointers[group],0xf500+group*16);
   for(unsigned n=0;n<4;n++){
    uint16_t actor=0xa000+group*512+n*128;ww(0xf500+group*16+n*2,actor);
    wb(actor,(sample/3+group+n)%4);
    uint8_t type=(sample/11+group+n)%(group<3?40:group<5?24:group==5?9:group<8?40:16);
    wb(actor+32,type);wb(actor+33,(sample/13+n)%4);wb(actor+15,n&1);
    ww(actor+6,0x200+(int)((sample/7+n*13+group*3)%101)-50);
    ww(actor+10,0x100+(int)((sample/101+n*19)%121)-60);
    ww(actor+20,0xff);ww(actor+22,0x3100+(group*4+n)*8);ww(0x3106+(group*4+n)*8,(sample/17+n)%(group==3?3:8));
    ww(actor+38,0xffff);ww(actor+40,0xd000);wb(0xd021,(sample/13)%32);
    wb(actor+42,(sample/7+n)%16);ww(actor+44,n&1?0xfff0:16);
    wb(actor+46,sample&8);wb(actor+60,sample&16);wb(actor+126,sample&32);wb(actor+127,(sample/7+n)%8);
    wb(actor+64,n&1);wb(actor+71,sample&64);
    ww(actor+48,16);ww(actor+50,32);ww(actor+52,24);ww(actor+54,48);
   }
  }
  uint32_t a4=sample&1?0x123456:0xffb000,initialA4=a4;
  uint16_t d4=(sample*137)^0xa5a5,initialD4=d4;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeCollisionManagerWithA4(&bus,sourceWord,&collision,textWrite,&d4,&a4))return 1;
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(14,0xff9846);m68k_set_reg(10,0xff9900);m68k_set_reg(4,0xabcd0000UL|initialD4);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Collision manager mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"D4 mismatch %u\n",sample);return 1;}
  if(m68k_get_reg(12)!=a4){fprintf(stderr,"A4 mismatch %u actual %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  checks++;
 }
 printf("Collision manager 39A52: %u complete original-pass comparisons; all four phases, stage gate, ten queues, cross-pass state, inventory and retained D4/A4 match\n",checks);
 return 0;
}
