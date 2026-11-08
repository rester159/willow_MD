/* Headless differential verification of original stage service 3807C. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_stage_update.h"
#include "../src/willow_arcade_stage_objects.h"
#include "../src/willow_arcade_pool_init.h"
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

static uint16_t romWord(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


static void textWrite(uint32_t at,uint16_t value){if(at<0x910000||at>=0x914000)abort();ww(0x4000+at-0x910000,value);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 if(willowMotionLong(rom+0x5b6)!=0x4bf88000UL||willowMotionLong(rom+0x1400)!=0x4bf88000UL)return 2;
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);

 if(willowMotionLong(rom+0x5640)!=0x91056c)return 2;
 willowMotionPutLong(rom+0x5640,0xff456c);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/stage-update-mixed-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/stage-update-mixed-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;

 const uint16_t values[]={0,1,0xff,0x100,0x7fff,0xfffc,0xfffd,0xfffe,0xffff};
 for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint32_t pc=0x3807c;
  ww(0x8346,values[sample%9]);wb(0xf9a4,(sample/9)%256);
  wb(0x8019,(sample/2304)&1);wb(0x828f,sample&1);wb(0x8068,sample&2?32:0);
  wb(0x8362,sample&4);wb(0x986a,sample&8);
  ww(0x984c,sample*127);ww(0x9850,sample*53);
  willowArcadePoolsInit(&bus);ww(0x8344,sample%6);
  const uint8_t types[]={0,1,2,3,7,9,14,14,15,15,20,20,21,22,0,0,1,1,2,2,5,25,32,23,23,23,27,26,26,26,26,26,26,26,26,31};
  for(unsigned n=0;n<36;n++){
   uint16_t r=0x8502+n*64;wb(r,1);wb(r+14,4);wb(r+16,types[n]);if(types[n]==14)wb(r+18,(n&1)*2);if(types[n]==15||types[n]==20)wb(r+17,n&1);
   if(n==14||n==15)wb(r+17,n==14?2:4);
   if(n>=16){wb(r+18,2);wb(r+19,(n&1)*2);}
   if(n==20)wb(r+14,6);
   if(n>=23&&n<=25){wb(r+14,12);wb(r+17,n-23);}
   if(n>=27&&n<=34){wb(r+17,n-27);uint32_t table=0x39614UL+((n-27)%7)*8;ww(r+48,table>>16);ww(r+50,table);}
   ww(r+6,sample*17+n*256);ww(r+8,sample*19);ww(r+10,sample*3);ww(r+12,sample*23);
  }
  ww(0x8408,(sample*7)&0xfff);ww(0x840c,0);ww(0x838c,(sample*3)&0xfff);
  wb(0xf47,0);wb(0x83d0,0);wb(0x83e6,0);wb(0xf40,sample%9);
  wb(0x9802,0);wb(0x9803,0);wb(0xf89a,0);wb(0x9801,sample%33);
  ww(0x8284,sample*317);
  for(unsigned n=0;n<36;n++){
   uint16_t r=0x8502+n*64;wb(r+2,(sample/3)%4*2);wb(r+3,(sample/7)%2*2);
   ww(r+20,sample%2?1:0);wb(r+22,1);wb(r+23,3);
   if(rb(r+16)==3 && rb(r+2)==6)wb(r+2,4);
   if(rb(r+16)==9){wb(r+2,(sample/3)%2*2);wb(0x0f49,(sample/11)%6);}
   if(rb(r+16)==20){wb(r+2,(sample/3)%2*2);wb(r+18,(sample/7)%4);}
   if(rb(r+16)==21||rb(r+16)==22){wb(r+2,(sample/3)%3*2);wb(r+20,sample%3);}
   if(n==14||n==15){if(n==14)ww(r+20,sample%3);else wb(r+20,sample%3);}
   if(n>=16){wb(r+3,(sample/7)%(n&1?2:3)*2);wb(r+20,sample%3);}
   if(n==20){wb(r+2,(sample/3)%3*2);wb(r+3,(sample/7)%2*2);wb(r+20,sample%3);}
   if(n==21){wb(r+3,(sample/7)%3*2);wb(r+20,sample%3);wb(r+4,sample%2);wb(r+32,(sample/11)%3);}
   if(n==22){wb(r+2,(sample/3)%3*2);ww(r+20,sample%3);}
   if(n>=23){wb(r+3,rb(r+2)?(sample/7)%2*2:0);wb(r+20,sample%3);}
   if(n==26){wb(r+20,sample%3);wb(r+22,(sample/11)%2);}
   if(n>=27)wb(r+20,sample%3);
   if(rb(r+16)==7){ww(r+38,0xffff);ww(r+40,0x9900);}
  }
  ww(0x801e,sample%64*2);wb(0x8290,(sample/13)%2);wb(0x8293,(sample/17)%2);ww(0x83d2,0);wb(0x83a6,(sample/19)%2);wb(0x0f5c,(sample/23)%6);
  for(unsigned n=0;n<8;n++)wb(0x0f80+n,(sample/29)%2);
  wb(0x980d,(sample/53)%3);
  wb(0x0f5b,(sample/17)%10);wb(0x0f62,(sample/19)%2);
  wb(0x0f89,(sample/41)%3);wb(0x0f8a,(sample/43)%3);wb(0x9804,(sample/47)%3);
  wb(0x0f57,(sample/29)%2);wb(0x0f58,(sample/31)%16);wb(0x83ce,(sample/37)%2);
  wb(0x0f55,(sample/19)%2);wb(0x0f56,(sample/23)%2);
  wb(0x0f4f,(sample/13)%6);wb(0x0f50,(sample/17)%3);ww(0x8396,sample&1?0x3200:0x31ff);
  wb(0x8502+8*64+20,sample%3);wb(0x8502+9*64+20,sample%3);
  uint32_t a4=sample&1?0x123456:0xffb000,initialA4=a4;uint16_t d4=sample*977;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeStageUpdateWithA4(&bus,romWord,textWrite,&a4))return 1;
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(4,d4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xffff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Stage update mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Registers sample %u A4 %x/%x D4 %x/%x\n",sample,m68k_get_reg(12),a4,m68k_get_reg(4),d4);return 1;}
  checks++;
 }
 /* Unsupported populations must fail before prefix or record mutations. */
 static uint8_t saved[65536],rejected[65536];memcpy(saved,expected,sizeof saved);
 for(unsigned invalid=0;invalid<29;invalid++){
  memcpy(expected,saved,sizeof saved);uint16_t r=0x8502+47*64;
  wb(r,1);wb(r+14,4);wb(r+16,3);wb(r+2,0);ww(0x8344,0);
  if(invalid<4){const uint8_t types[]={4,5,6,255};wb(r+16,types[invalid]);}
  else if(invalid==4)ww(0x8344,6);
  else if(invalid==5)wb(r+2,6);
  else if(invalid==6){wb(r+14,6);wb(r+16,6);}
  else if(invalid==7){wb(r+16,9);wb(r+2,4);}
  else if(invalid==8){wb(r+16,14);wb(r+18,1);}
  else if(invalid==9){wb(r+16,15);wb(r+17,2);}
  else if(invalid<13){wb(r+16,20);wb(r+17,invalid==10?2:0);wb(r+2,invalid==11?4:0);wb(r+3,invalid==12?4:0);}
  else if(invalid<15){wb(r+16,invalid==13?21:22);wb(r+2,6);}
  else if(invalid<18){const uint8_t variants[]={1,3,6};wb(r+16,0);wb(r+17,variants[invalid-15]);}
  else if(invalid<21){wb(r+16,1);wb(r+18,invalid==18?1:2);wb(r+19,invalid==19?1:2);wb(r+2,2);wb(r+3,invalid==20?4:0);}
  else if(invalid<23){wb(r+14,6);wb(r+16,5);wb(r+2,invalid==21?6:2);wb(r+3,invalid==22?4:0);}
  else if(invalid<25){wb(r+16,invalid==23?25:32);wb(r+2,invalid==23?2:6);wb(r+3,6);}
  else if(invalid<28){wb(r+14,12);wb(r+16,invalid==25?24:23);wb(r+17,invalid==26?3:0);wb(r+2,2);wb(r+3,invalid==27?4:0);}
  else {wb(r+16,26);wb(r+17,8);}
  memcpy(rejected,expected,sizeof rejected);uint32_t a4=0xdeadbeef;
  if(willowArcadeStageUpdateWithA4(&bus,romWord,textWrite,&a4)||a4!=0xdeadbeef||memcmp(rejected,expected,sizeof rejected))return 1;
 }
 puts("Unsupported stage populations reject atomically before difficulty, HUD, RNG or pool changes");
 printf("Supported mixed stage update 3807C: %u original-instruction comparisons; family-six/twelve spawners and sixteen family-four subtypes (all subtype-zero and subtype-one/two modes, both subtype-14 modes and subtype-15/20 variants) across six stages, difficulty/HUD prefix, spawn allocation and retained RAM and D4/A4 match\n",checks);
 return 0;
}
