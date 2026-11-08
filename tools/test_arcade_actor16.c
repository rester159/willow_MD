/* Headless differential verification of 36C7E admitted class-16 populations. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor16.h"
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
static uint8_t released[20],spawned[20];
static unsigned reused,immediate,delayed,zeroSpawned;

static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){
 if(a>=0xeac6&&a<0xefc6&&!((a-0xeac6)%64)){
  unsigned n=(a-0xeac6)/64;
  if(!v&&expected[a])released[n]=1;
  if(v==1&&!expected[a]){spawned[n]=1;if(released[n])reused++;}
 }
 expected[a]=v;
}



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
 willowMotionPutLong(rom+0x4e34,0xff2c0c);willowMotionPutLong(rom+0x4e4a,0xff2d0c);willowMotionPutLong(rom+0x4e5e,0xff2e0c);
 willowMotionPutLong(rom+0x4a36,0xff230c);willowMotionPutLong(rom+0x4a40,0xff230c);
 willowMotionPutLong(rom+0x3e44,0xff4000);
 willowMotionPutLong(rom+0x4d6c,0xff2df0);willowMotionPutLong(rom+0x4da8,0xff2df0);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor16-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor16-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;

 for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  ww(0x9840,0xf640);wb(0x9813,0);wb(0x8362,sample%7);wb(0x8363,6);wb(0x8364,sample%4);
  willowMotionPutLong(expected+0x8368,sample&1?0x99999998:0x123456);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);ww(0x801e,(sample*2)&126);
  const uint8_t types[]={0,1,2,3,4,5,6,7,8,9,10};
  for(unsigned n=0;n<20;n++){
   uint16_t at=0xeac6+n*64;uint8_t*r=resolve(at);memset(r,0,64);
   r[0]=(sample+n)%4;r[1]=1;r[2]=((sample/4+n)%4)*2;r[3]=((sample/16+n)%3)*2;
   r[30]=16;r[32]=types[(sample+n)%11];r[33]=(sample+n)%12;r[14]=(n&1)*2;r[34]=(sample+n)%5;
   if(r[32]==0){r[19]=1;willowMotionPutLong(r+24,0xed070);}
   willowMotionPutWord(r+6,0x200+(int)n*4-40);willowMotionPutWord(r+10,0x100+(int)n*3-30);
   willowMotionPutLong(r+20,r[32]==0?0x3766e:r[32]==2?0x3795c:0x37e40);r[35]=(sample+n)%5;willowMotionPutWord(r+28,(sample+n)%3);willowMotionPutWord(r+50,(sample+n)*37);
  }
  /* Give destruction a real, disjoint free stack. Released slots can be
   * reused, and a reward in a later slot must run during the same scan. */
  unsigned freeCount=0;
  for(unsigned k=0;k<20;k++){
   unsigned n=sample&1?19-k:k;uint16_t at=0xeac6+n*64;uint8_t*r=resolve(at);
   if(n==0||n==19){memset(r,0,64);r[30]=16;}
   if(!r[0]){memset(r,0,64);r[30]=16;ww(0xf640+freeCount*2,at);freeCount++;}
   else if(r[32]==4){r[3]&=2;r[15]=n%5;r[42]=r[43]=6;}
  }
  wb(0x9813,freeCount);ww(0x8344,0);wb(0x83a4,sample%4);wb(0x8387,0);
  for(unsigned k=0;k<2;k++){
   uint8_t*r=resolve(0xeac6+(k?14:5)*64);memset(r,0,64);
   r[0]=r[1]=1;r[2]=4;r[3]=2;r[30]=16;r[32]=4;r[34]=1;r[15]=k;
   willowMotionPutWord(r+6,0x200);willowMotionPutWord(r+10,0x100);
  }
  /* Forced live slots must not remain on the allocation stack. */
  unsigned retained=0;
  for(unsigned n=0;n<freeCount;n++){
   uint16_t at=rw(0xf640+n*2);if(!rb(at))ww(0xf640+retained++*2,at);
  }
  wb(0x9813,retained);
  wb(0x980d,60);ww(0x9834,0xf700);
  for(unsigned n=0;n<60;n++){
   uint16_t at=0xccc6+n*64;memset(resolve(at),0,64);wb(at+30,12);ww(0xf700+n*2,at);
  }
  for(unsigned n=0;n<20;n++){
   uint16_t at=0xc2c6+n*64;memset(resolve(at),0,64);
   wb(at,1);wb(at+32,(sample+n)%9);wb(at+42,1+(sample+n)%8);
   ww(at+6,0x200);ww(at+10,0x100);ww(at+48,16);ww(at+50,32);ww(at+52,24);ww(at+54,48);
  }
  wb(0x83a5,0);ww(0x8284,sample*197);wb(0x8286,0);
  for(unsigned n=0;n<12;n++)wb(0x8352+n,(sample+n)%4);
  for(unsigned n=0;n<0x4000;n+=4)wb(0x4000+n+2,sample+((n/4)%3)*4);
  wb(0x9807,10);ww(0x9828,0xf800);
  for(unsigned n=10;n<20;n++){wb(0xc2c6+n*64,0);ww(0xf800+(n-10)*2,0xc2c6+n*64);}
  uint16_t d4=sample*73;uint32_t a4=sample&1?0xffb000:0x123456;
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
  memset(released,0,sizeof released);memset(spawned,0,sizeof spawned);
  for(unsigned step=0;step<512;step++){
  uint32_t pc=0x36c7e;
  for(unsigned group=0;group<4;group++)willowArcadeVisibilityInit(&bus,group,0);
  ww(0x8408,0x180);ww(0x840c,0x80);ww(0x984c,0x1d0+step*2);ww(0x9850,0x100);
  ww(0x985a,0xff);ww(0x985c,0x1000);ww(0x1006,(step/8)%3);wb(0x8019,sample+step);
  wb(0x9855,(step/16)%4);wb(0x986a,(step/32)&1);wb(0x806a,(step/3)&16);wb(0x806b,(step/5)&16);
  uint16_t oldD4=d4;uint32_t oldA4=a4;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeActor16WithRegisters(&bus,resolve,&collision,textWrite,&d4,&a4))return 1;
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,oldA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(14,0xffeac6);m68k_set_reg(10,0xff9900);m68k_set_reg(7,(sample/11)%20);m68k_set_reg(0,0);m68k_set_reg(4,oldD4);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Actor16 mismatch sample %u step %u at %x actual %x expected %x\n",sample,step,n,ram[n^1],expected[n]);return 1;
  }
  for(unsigned n=0;n<20;n++)if(spawned[n]){
   uint8_t state=rb(0xeac8+n*64);
   uint8_t type=rb(0xeae6+n*64);
   if(type==0){
    zeroSpawned++;
    /* Selector zero initializes its substate before changing main state. */
    if(state==0&&!rb(0xeac9+n*64))delayed++;else immediate++;
   }else if(state==0)delayed++;else if(state==2)immediate++;
   spawned[n]=0;
  }
  if(m68k_get_reg(12)!=a4){fprintf(stderr,"A4 sample %u step %u actual %x expected %x\n",sample,step,m68k_get_reg(12),a4);return 1;}
  if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Actor16 D4 mismatch sample %u step %u\n",sample,step);return 1;}
  checks++;
 }
 }
 if(!immediate||!delayed||!reused||!zeroSpawned){fprintf(stderr,"Missing spawn-order coverage: immediate %u delayed %u reused %u selector-zero %u\n",immediate,delayed,reused,zeroSpawned);return 1;}
 printf("Spawn coverage: %u initialized in same scan, %u deferred to next scan, %u reused released slots, %u selector-zero spawns\n",immediate,delayed,reused,zeroSpawned);
 printf("Actor16 scan 36C7E: %u original non-stack RAM/D4/A4 comparisons; all eleven selectors, ordered spawning and pool reuse, actor movement, terrain, attacks, collection timing, HUD, damage, rewards and release match\n",checks);
 return 0;
}
