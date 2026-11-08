/* Headless original-instruction verification of 8EC4 -> 86A6 -> 65A4. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_interlude_clear.h"
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

static uint16_t romWord(uint32_t at){return willowMotionWord(rom+at);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


static const WillowVisibilityBus bus={rw,rb,ww,wb};

static uint8_t graphics[65536];static unsigned writes;
static void videoWord(uint32_t at,uint16_t value){
 unsigned offset;
 if(at>=0x910000&&at<0x914000)offset=at-0x910000;
 else if(at>=0x918000&&at<0x91c000)offset=0x4000+at-0x918000;
 else if(at>=0x914000&&at<0x918000)offset=0x8000+at-0x914000;
 else if(at>=0x900000&&at<0x900800)offset=0xc000+at-0x900000;
 else if(at>=0x908000&&at<0x908800)offset=0xc800+at-0x908000;
 else abort();
 willowMotionPutWord(graphics+offset,value);writes++;
}
static void videoFill(uint32_t at,uint16_t first,uint16_t second,uint16_t pairs){
 for(unsigned n=0;n<pairs;n++){videoWord(at+n*4,first);videoWord(at+n*4+2,second);}
}
static void unusedColor(uint32_t at,uint16_t value){(void)at;(void)value;abort();}
static void clearTask(uint16_t*d1,uint16_t*d2)
{if(!willowArcadeAttractInterludeClear(&bus,romWord,videoWord,0,d1,d2))abort();}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);

 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 const unsigned operands[]={0x65a6,0x65ba,0x65ce,0x65ec,0x65f6};
 const uint32_t original[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 const unsigned offsets[]={0,0x4000,0x8000,0xc000,0xc800};
 for(unsigned n=0;n<5;n++){
  if(willowMotionLong(rom+operands[n])!=original[n])return 2;
  willowMotionPutLong(rom+operands[n],0x200000+offsets[n]);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/interlude-clear-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/interlude-clear-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
 if(!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<sizeof graphics)return 2;
 unsigned checks=0;const uint16_t ready[]={0,1,0x8000,0xffff};
 for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++){expected[n]=(n*7+sample*31)^0xa5;graphics[n]=(n*17+sample*13)^0x5a;}
  ww(0x8004,8);ww(0x8008,2);ww(0x82c0,ready[sample%4]);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  memcpy(sram,graphics,sizeof graphics);
  uint16_t d1=sample*311,d2=sample*977,d4=sample*73;writes=0;
  if(!willowArcadeAttractInterludeClear(&bus,romWord,sample&4?NULL:videoWord,
     sample&4?videoFill:NULL,&d1,&d2))return 1;
  if(writes!=(ready[sample%4]?26624:0))return 1;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);m68k_set_reg(4,d4);m68k_set_reg(12,0xffb000);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x8ec4);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000)return fprintf(stderr,"Interlude clear stalled sample %u PC %x\n",sample,m68k_get_reg(16)),1;
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n])
   return fprintf(stderr,"Interlude clear sample %u RAM %x\n",sample,n),1;
  if(memcmp(sram,graphics,sizeof graphics))return fprintf(stderr,"Interlude clear graphics sample %u\n",sample),1;
  if((uint16_t)m68k_get_reg(1)!=d1||(uint16_t)m68k_get_reg(2)!=d2||
     (uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=0xffb000)return fprintf(stderr,"Interlude clear registers %u\n",sample),1;
  checks++;
 }
 uint8_t saved[65536],savedGraphics[65536];
 for(unsigned test=0;test<7;test++){
  ww(0x8004,test==0?6:8);ww(0x8008,test==1?0:2);ww(0x82c0,1);
  memcpy(saved,expected,sizeof saved);memcpy(savedGraphics,graphics,sizeof graphics);
  uint16_t d1=0x1234,d2=0xabcd;
  if(willowArcadeAttractInterludeClear(test==2?NULL:&bus,test==3?NULL:romWord,
     test==4?NULL:videoWord,0,test==5?NULL:&d1,test==6?NULL:&d2)||
     memcmp(saved,expected,sizeof saved)||memcmp(savedGraphics,graphics,sizeof graphics)||d1!=0x1234||d2!=0xabcd)return 1;
 }
 ww(0x8000,4);ww(0x8004,8);ww(0x8008,2);wb(0x8058,0);wb(0x828f,0);
 WillowNativeAttractTask task={0x810e,0x1234,0xabcd};
 if(willowNativeAttractTaskSupportedWithInterlude(&task,&bus,romWord,unusedColor,videoWord,0,0,0,0,0,0,0,0))return 1;
 for(unsigned mode=0;mode<4;mode++){
  ww(0x8008,mode==2?4:2);memcpy(saved,expected,sizeof saved);
  unsigned admitted=willowNativeAttractTaskSupportedWithInterludeClear(&task,&bus,romWord,
   mode==1?NULL:unusedColor,videoWord,0,0,0,0,0,0,0,0,mode==0?NULL:clearTask);
  if(admitted!=(mode==3)||memcmp(saved,expected,sizeof saved)||task.d1!=0x1234||task.d2!=0xabcd)return 1;
 }
 puts("Interlude clear admission: old reset-only API rejects; explicit clear capability and color sink required, later states reject unchanged");
 printf("Interlude clear: %u original-CPU cases match RAM, all five video regions and guards, D1/D2/D4/A4; pending fade, nonzero completion values, word/bulk sinks and 7 atomic rejection cases pass\n",checks);
 return 0;
}
