/* Headless original-instruction verification of 8EC4 -> 8EFA..8FB6. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_interlude_setup.h"
#include "../src/willow_native_attract_task.h"
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

static uint8_t romByte(uint32_t at){return rom[at];}
static uint16_t romWord(uint32_t at){return willowMotionWord(rom+at);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


static const WillowVisibilityBus bus={rw,rb,ww,wb};

static uint8_t graphics[65536];static unsigned nearWrites,farWrites,colorWrites,copies;
static void scroll2Write(uint16_t at,uint16_t v){if(at>=0x4000)abort();willowMotionPutWord(graphics+at,v);nearWrites++;}
static void scroll3Write(uint16_t at,uint16_t v){if(at>=0x4000)abort();willowMotionPutWord(graphics+0x4000+at,v);farWrites++;}
static void colorWrite(uint32_t at,uint16_t v){if(at<0x91c000||at>=0x91d800)abort();willowMotionPutWord(graphics+0x8000+(at-0x91c000),v);colorWrites++;}
#ifndef WILLOW_TEST_INTERLUDE_TASK
static void paletteCopy(uint32_t source,uint32_t target,uint16_t count){for(unsigned n=0;n<count;n++)colorWrite(target+n*2,romWord(source+n*2));copies++;}
static uint8_t nearCopy(uint16_t at,uint32_t source,uint16_t count){if(count!=256)abort();for(unsigned n=0;n<count*2;n++)scroll2Write(at+n*2,romWord(source+n*2));copies++;return 1;}
static uint8_t farCopy(uint16_t at,uint32_t source,uint16_t count){if(count!=64)abort();for(unsigned n=0;n<count;n++){scroll3Write(at+n*4,romWord(source+n*4)+0xa00);scroll3Write(at+n*4+2,romWord(source+n*4+2));}copies++;return 1;}
#endif
static void setupTask(uint16_t*d1,uint16_t*d2)
{if(!willowArcadeAttractInterludeSetup(&bus,expected+0x8402,expected+0x8482,romByte,romWord,
 colorWrite,scroll2Write,scroll3Write,0,0,0,d1,d2))abort();}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
#ifdef WILLOW_TEST_INTERLUDE_TASK
 willowMotionPutWord(rom+0x1750,0x60fe);
#endif
 willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x9c,0x160e);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 const unsigned operands[]={0x4365a,0x43d50,0xae4,0xaf6,0xb06,0xb16};
 const uint32_t original[]={0x918000,0x914000,0x91c800,0x91c400,0x91cc00,0x91c200};
 const unsigned offsets[]={0,0x4000,0x8800,0x8400,0x8c00,0x8200};
 for(unsigned n=0;n<6;n++){
  if(willowMotionLong(rom+operands[n])!=original[n])return fprintf(stderr,"Bad relocation %x\n",operands[n]),2;
  willowMotionPutLong(rom+operands[n],0x200000+offsets[n]);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/interlude-setup-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/interlude-setup-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
 if(!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<sizeof graphics)return 2;
 unsigned checks=0;
 for(unsigned sample=0;sample<513;sample++){
  for(unsigned n=0;n<65536;n++){expected[n]=(n*7+sample*31)^0xa5;graphics[n]=(n*17+sample*13)^0x5a;}
  if(sample==512){
   f=fopen("build/interlude-clear-investigation/sequence-output-ram.bin","rb");
   if(!f||fread(expected,1,sizeof expected,f)!=sizeof expected||fgetc(f)!=EOF)return 2;fclose(f);
  }else{
   unsigned free=sample%9;ww(0x208,sample&1?0xffff:0xff);ww(0x20a,0xa30-free*4);ww(0x20c,free);
   for(unsigned n=0;n<8;n++){ww(0xa10+n*4,sample&2?0xffff:0xff);ww(0xa12+n*4,0x100+n*32);wb(0x100+n*32,0);}
   wb(0x8423,sample%4);wb(0x84a3,(sample/4)%4);wb(0xc0,sample&16?12:0);
   ww(0x8004,8);ww(0x8008,4);
  }
#ifdef WILLOW_TEST_INTERLUDE_TASK
  ww(0x8000,4);wb(0x8058,0);wb(0x82a2,0);wb(0x82a4,0);wb(0x828f,1);wb(0x8068,sample<512&&(sample&256)?2:0);
#endif
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];memcpy(sram,graphics,sizeof graphics);
  uint16_t d1=sample*311,d2=sample*977,d4=sample*73;nearWrites=farWrites=colorWrites=copies=0;
  unsigned entry=0x8ec4,stop=0x100000;
#ifdef WILLOW_TEST_INTERLUDE_TASK
  WillowNativeAttractTask task={0x810e,d1,d2};
  WillowTransitionStep sleep=willowNativeAttractTaskStepWithInterludeSetup(&task,&bus,romWord,colorWrite,0,0,0,0,0,0,0,0,0,0,0,0,setupTask);
  if(!sleep.supported||sleep.finished||sleep.delay!=1||sleep.resume!=0x810e)return 1;
  d1=task.d1;d2=task.d2;entry=0x810e;stop=0x1750;
  if(nearWrites!=((rb(0x8068)&2)?0:1024)||farWrites!=((rb(0x8068)&2)?0:256)||colorWrites!=((rb(0x8068)&2)?0:1792)||copies)return 1;
#else
  if(!willowArcadeAttractInterludeSetup(&bus,expected+0x8402,expected+0x8482,romByte,romWord,
    colorWrite,scroll2Write,scroll3Write,sample&32?paletteCopy:0,sample&64?nearCopy:0,sample&128?farCopy:0,&d1,&d2))return 1;
  if(nearWrites!=1024||farWrites!=256||colorWrites!=1792||copies!=((sample&32?4:0)+(sample&64?2:0)+(sample&128?2:0)))return 1;
#endif
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);m68k_set_reg(4,d4);m68k_set_reg(12,0xffb000);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop)return fprintf(stderr,"Interlude setup stalled sample %u PC %x\n",sample,m68k_get_reg(16)),1;
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n])
   return fprintf(stderr,"Interlude setup sample %u RAM %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]),1;
  for(unsigned n=0;n<sizeof graphics;n++)if(sram[n]!=graphics[n])return fprintf(stderr,"Interlude setup graphics sample %u offset %x actual %x expected %x\n",sample,n,sram[n],graphics[n]),1;
  if((uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2||
     (uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=0xffb000)return fprintf(stderr,"Interlude setup registers %u: D1 %x/%x D2 %x/%x D4 %x/%x A4 %x\n",sample,m68k_get_reg(1),d1,m68k_get_reg(2),d2,m68k_get_reg(4),d4,m68k_get_reg(12)),1;
  checks++;
 }
 uint8_t saved[65536],savedGraphics[65536];
 for(unsigned test=0;test<14;test++){
  ww(0x8004,test==0?6:8);ww(0x8008,test==1?2:4);wb(0x8423,test==12?4:0);wb(0x84a3,test==13?4:0);
  memcpy(saved,expected,sizeof saved);memcpy(savedGraphics,graphics,sizeof graphics);uint16_t d1=0x1234,d2=0xabcd;
  if(willowArcadeAttractInterludeSetup(test==2?NULL:&bus,test==3?NULL:expected+0x8402,test==4?NULL:expected+0x8482,
    test==5?NULL:romByte,test==6?NULL:romWord,test==7?NULL:colorWrite,test==8?NULL:scroll2Write,test==9?NULL:scroll3Write,
    0,0,0,test==10?NULL:&d1,test==11?NULL:&d2)||memcmp(saved,expected,sizeof saved)||memcmp(savedGraphics,graphics,sizeof graphics)||d1!=0x1234||d2!=0xabcd)return 1;
 }
 ww(0x8000,4);ww(0x8004,8);ww(0x8008,4);wb(0x8058,0);wb(0x828f,0);
 WillowNativeAttractTask task={0x810e,0x1234,0xabcd};
 if(willowNativeAttractTaskSupportedWithInterludeClear(&task,&bus,romWord,colorWrite,0,0,0,0,0,0,0,0,0,0))return 1;
 for(unsigned mode=0;mode<4;mode++){
  ww(0x8008,mode==2?6:4);memcpy(saved,expected,sizeof saved);
  unsigned admitted=willowNativeAttractTaskSupportedWithInterludeSetup(&task,&bus,romWord,
   mode==1?NULL:colorWrite,0,0,0,0,0,0,0,0,0,0,mode==0?NULL:setupTask);
  if(admitted!=(mode==3)||memcmp(saved,expected,sizeof saved)||task.d1!=0x1234||task.d2!=0xabcd)return 1;
 }
#ifdef WILLOW_TEST_INTERLUDE_TASK
 puts("Interlude setup manager: original 810E dispatcher and freeze gate match; setup capability required and later states rejected");
#endif
 printf("Interlude setup: %u original-CPU cases match all RAM, both maps, exact CPS palette words and untouched guards, D1/D2/D4/A4; 14 invalid contexts reject unchanged\n",checks);
 return 0;
}
