/* Original-instruction verification of ending credits B2A4 and clear B3FA. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_ending_credits.h"
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

static uint8_t expected[65536];static unsigned writes;
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static uint8_t romByte(uint32_t a){return rom[a];}
static void textWrite(uint32_t at,uint16_t value){
 if(at<0x910000||at>=0x914000)abort();
 ww(0x4000+at-0x910000,value);writes++;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 const unsigned patches[]={0xb308,0xb34c,0xb3fc,0xb408,0xb8de,0xb8ee,0xb902,0xb912,0xb922,0xb932};
 const unsigned values[]={0x910000,0x910000,0x910800,0x912800,0x910d40,0x910d44,0x910d4c,0x910d50,0x910d54,0x910d58};
 for(unsigned n=0;n<10;n++){
  if(willowMotionLong(rom+patches[n])!=values[n])return 2;
  willowMotionPutLong(rom+patches[n],0xff4000+values[n]-0x910000);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/ending-credits-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/ending-credits-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,draws=0,independentDraws=0,completed=0,clearChecks=0,titleChecks=0;
 for(unsigned sample=0;sample<10368;sample++){
  /* Independent fixtures sweep rows, completion bits, scroll banks and
   * cadence mismatches. Then retain two complete 3,072-tick sequences. */
  if(sample<4096||sample==4096||sample==7168||sample>=10240){
   for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
   wb(0x0e88,sample<4096?(sample/16)%74:0);
   wb(0x0e89,sample<4096?((sample>>2)&3)*8:8);
   wb(0x0e90,sample<4096?sample&3:sample==7168?1:0);
   ww(0x0e8c,sample<4096?(sample/16)*29:0);
   ww(0x0e8e,sample<4096?0x4000:sample*16384);
   ww(0x800c,8);ww(0x800e,352);
  }
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t a4=0x123456,d2=0x87654321;uint16_t d4=sample*977;
  writes=0;unsigned entry=sample>=10304?0xb8d8:sample>=10240?0xb3fa:0xb2a4;
  if(entry==0xb3fa){willowArcadeEndingCreditsClear(textWrite);if(writes!=4096)return 1;clearChecks++;}
  else if(entry==0xb8d8){willowArcadeEndingTitle(textWrite,&d2);if(writes!=124)return 1;titleChecks++;}
  else{
   willowArcadeEndingCreditsTick(&bus,romByte,textWrite,&d2,&a4);
   draws+=writes==64;completed+=rw(0x800c)==12&&rw(0x800e)==15;
   independentDraws+=sample<4096&&writes==64;
  }
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,0x123456);m68k_set_reg(2,0x87654321);m68k_set_reg(4,d4);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xffff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000)return fprintf(stderr,"Credits stalled sample %u PC %x\n",sample,m68k_get_reg(16)),1;
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n])
   return fprintf(stderr,"Credits sample %u RAM %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]),1;
  if(m68k_get_reg(12)!=a4||m68k_get_reg(2)!=d2||(uint16_t)m68k_get_reg(4)!=d4)
   return fprintf(stderr,"Credits registers sample %u A4 %x/%x D2 %x/%x\n",sample,m68k_get_reg(12),a4,m68k_get_reg(2),d2),1;
  checks++;
 }
 if(independentDraws<400||!completed||clearChecks!=64||titleChecks!=64)return 1;
 printf("Ending credits: %u original-code comparisons, %u text rows, %u completed-track observations, %u clears, %u final titles; non-stack RAM/text map and D2/D4/A4 match\n",checks,draws,completed,clearChecks,titleChecks);
 return 0;
}
