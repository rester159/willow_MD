/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_palette_initialization.h"
#include "../src/willow_native_palette_boot.h"
#include "../native/intro_probe/src/palette_initial_data.h"
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

static void putRamWord(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
static unsigned ramWord(uint8_t*r,unsigned a){return (r[a^1]<<8)|r[(a+1)^1];}
static uint16_t colors[2048];
static uint16_t sourceWord(uint32_t at){if(at+1>=0x100000 || (at&1))abort();uint16_t value;if(!nativePaletteInitialWordRead(at,&value) || value!=willowMotionWord(rom+at)){fprintf(stderr,"Palette source data mismatch %06x\n",(unsigned)at);abort();}return value;}
static uint16_t mixedBootWord(uint32_t at){return sourceWord(at)^(at==0xe64a2UL?0x1000:0);}
static void colorWrite(uint32_t at,uint16_t v){if(at<0x91c000 || at>=0x91d000 || (at&1))abort();colors[(at-0x91c000)/2]=v;}
static void paletteCopy(uint32_t source,uint32_t destination,uint16_t words){
 for(unsigned n=0;n<words;n++)colorWrite(destination+n*2,sourceWord(source+n*2));
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Relocate only the five CPS palette destination operands into test RAM. */
 const unsigned patches[]={0x988,0x99a,0x9b0,0x9cc,0x9da};
 const uint32_t addresses[]={0x91c000,0x91c200,0x91c800,0x91c400,0x91cc00};
 for(unsigned n=0;n<5;n++){
  if(willowMotionLong(rom+patches[n])!=addresses[n])return 2;
  willowMotionPutLong(rom+patches[n],addresses[n]-0x91c000+0xff2000);
 }
 /* Boot copies: change destination operands only, retaining original
  * OR-brightness code, source words, loops and return conventions. */
 const unsigned bootPatches[]={0x774,0x788,0x79c,0x7b0,0x7c4,0x7d8,0x916,0x926,0x936,0x946};
 for(unsigned n=0;n<10;n++){
  uint32_t address=0x91c000UL+(n<6?n:n-6)*1024;
  if(willowMotionLong(rom+bootPatches[n])!=address)return 2;
  willowMotionPutLong(rom+bootPatches[n],address-0x91c000+0xff2000);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/palette-initialization-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/palette-initialization-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at)); \
 for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Palette oracle stopped at PC %08x stage %u sample %u\n",m68k_get_reg(16),stage,sample);return 1;} \
} while(0)
 unsigned checks=0;uint32_t random=0x57494c4c;
 for(unsigned bulk=0;bulk<2;bulk++)for(unsigned stage=0;stage<7;stage++)for(unsigned sample=0;sample<256;sample++){
  uint16_t masks[4];
  for(unsigned n=0;n<4;n++){masks[n]=sample*311+n;putRamWord(ram,0x804c+n*2,masks[n]);}
  for(unsigned n=0;n<2048;n++){
   random=random*1664525u+1013904223u;colors[n]=random>>16;putRamWord(ram,0x2000+n*2,colors[n]);
  }
  putRamWord(ram,0x8344,stage);
  if(!(bulk?willowPaletteStageInitWithCopy(stage,masks,sourceWord,colorWrite,paletteCopy):
       willowPaletteStageInit(stage,masks,sourceWord,colorWrite)))return 2;
  RUN(0x978,0xdeadbeef);
  for(unsigned n=0;n<4;n++)if(masks[n]!=ramWord(ram,0x804c+n*2)){fprintf(stderr,"Priority mask mismatch stage %u word %u\n",stage,n);return 1;}
  for(unsigned n=0;n<2048;n++)if(colors[n]!=ramWord(ram,0x2000+n*2)){fprintf(stderr,"Initial palette mismatch stage %u sample %u word %u native %04x source %04x\n",stage,sample,n,colors[n],ramWord(ram,0x2000+n*2));return 1;}
  checks++;
 }
 unsigned bootChecks=0;
 for(unsigned sample=0;sample<16;sample++){
  unsigned stage=0;
  WillowNativePaletteOwner owner;WillowMutablePaletteGroup slots[2];
  memset(&owner,sample*17,sizeof owner);memset(slots,sample*31,sizeof slots);
  for(unsigned n=0;n<3072;n++)putRamWord(ram,0x2000+n*2,sample*977+n*317);
  m68k_set_reg(12,0x100000);RUN(0x76c,0xdeadbeef);RUN(0x8fe,0x12345678);
  if(!willowNativePaletteBoot(&owner,slots,2,sourceWord))return 1;
  for(unsigned n=0;n<3072;n++){
   uint16_t value;
   if(!willowMutablePaletteRead(&owner,slots,2,0x91c000+n*2,sourceWord,&value)||value!=ramWord(ram,0x2000+n*2)){
    fprintf(stderr,"Boot palette mismatch sample %u word %u\n",sample,n);return 1;
   }
   bootChecks++;
  }
  if(slots[0].group!=65535||slots[1].group!=65535)return 1;
  WillowNativePaletteOwner saved=owner;WillowMutablePaletteGroup savedSlots[2];memcpy(savedSlots,slots,sizeof slots);
  if(willowNativePaletteBoot(&owner,slots,2,mixedBootWord)||memcmp(&owner,&saved,sizeof owner)||memcmp(slots,savedSlots,sizeof slots))return 1;
 }
 f=fopen("build/boot-palette-original-words.bin","wb");if(!f)return 2;
 for(unsigned n=0;n<6144;n++)if(fputc(ram[(0x2000+n)^1],f)==EOF)return 2;
 if(fclose(f))return 2;
 puts("Boot palettes: original 076C then 08FE, all six banks and both mutable slots verified");
 printf("Boot palette comparisons: %u original CPS words, including D-to-F brightness groups\n",bootChecks);
 printf("Palette initialization: %u cases across all 7 stages match all 2048 CPS color words and 4 priority masks\n",checks);
 return 0;
}
