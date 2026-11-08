/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include <stdint.h>
static uint8_t rowCopy(uint16_t,uint32_t,uint16_t);
#define WILLOW_SCROLL3_ROW_COPY rowCopy
#include "../src/willow_arcade_scroll3_start.h"
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
static uint8_t ring[16384];
static uint16_t sourceWord(uint32_t at){if(at+1>=0x100000)abort();return willowMotionWord(rom+at);}
static void ringWrite(uint16_t at,uint16_t value){if(at+1>=sizeof ring)abort();willowMotionPutWord(ring+at,value);}
static uint8_t sourceByte(uint32_t at){if(at>=0x100000)abort();return rom[at];}
static uint8_t pairCopy(uint16_t at,uint32_t source,uint16_t count){
 while(count--){ringWrite(at,(uint16_t)(sourceWord(source)+2560));ringWrite(at+2,sourceWord(source+2));at+=4;source+=4;}return 1;
}
static uint8_t rowCopy(uint16_t at,uint32_t source,uint16_t count){
 while(count--){if(!pairCopy(at,source,1))return 0;at+=32;source+=32;}return 1;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 /* Redirect only the CPS map base operand into the oracle work RAM. */
 if(willowMotionLong(rom+0x43d50)!=0x914000)return 2;
 willowMotionPutLong(rom+0x43d50,0xff4000);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scroll3-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scroll3-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)

 unsigned checks=0;uint32_t random=0x57494c4c;
 /* All four selector modes, every layout-cell index, fractional cell
  * positions and high coordinate bits are exercised in original code. */
 for(unsigned mode=0;mode<4;mode++)for(unsigned sample=0;sample<16384;sample++){
  uint8_t record[128];for(unsigned n=0;n<128;n++){random=random*1664525u+1013904223u;record[n]=random>>24;}
  record[33]=mode;
  /* A 2048-byte original ROM region covers even the largest selector;
   * selected bytes are data to this routine, regardless of stage layout. */
  willowMotionPutLong(record+66,0x2dc30);
  uint16_t x=(sample*17u),y=(sample*7919u);
  for(unsigned n=0;n<128;n++)ram[(0x9846+n)^1]=record[n];
  uint32_t address;if(!willowScroll3Record(record,x,y,sourceByte,&address))return 2;
  m68k_set_reg(14,0xff9846);m68k_set_reg(5,x);m68k_set_reg(6,y);
  RUN(0x43d82,0xabcdef01);
  if(m68k_get_reg(9)!=address){fprintf(stderr,"Map address mode %u sample %u native %x source %x\n",mode,sample,address,m68k_get_reg(9));return 1;}
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x9846+n)^1]){
   fprintf(stderr,"Map record mode %u sample %u byte %u\n",mode,sample,n);return 1;
  }
  checks++;
 }
 unsigned transfers=0;
 for(unsigned horizontal=0;horizontal<2;horizontal++)for(unsigned mode=0;mode<4;mode++)for(unsigned sample=0;sample<512;sample++){
  uint8_t record[128];for(unsigned n=0;n<128;n++)record[n]=(n*17+sample)&255;
  record[33]=mode;willowMotionPutLong(record+66,0x2dc30);
  uint16_t x=sample*113u,y=sample*7919u;uint32_t source;
  if(!willowScroll3Record(record,x,y,sourceByte,&source))return 2;
  uint16_t destination=((y&0x700)<<3)|((y&224)>>3)|(x&0x7e0);
  for(unsigned n=0;n<128;n++)ram[(0x9846+n)^1]=record[n];
  for(unsigned n=0;n<sizeof ring;n++){ring[n]=(sample+n*23)&255;ram[(0x4000+n)^1]=ring[n];}
  if(!willowScroll3TransferCopy(record,horizontal?x:y,destination,source,horizontal,sourceByte,sourceWord,ringWrite,pairCopy))return 2;
  m68k_set_reg(14,0xff9846);m68k_set_reg(5,x);m68k_set_reg(6,y);
  m68k_set_reg(8,0xff4000+destination);m68k_set_reg(9,source);
  RUN(horizontal?0x43e5e:0x43db4,0);
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x9846+n)^1]){
   fprintf(stderr,"Stream actor mismatch axis %u mode %u case %u byte %u\n",horizontal,mode,sample,n);return 1;
  }
  for(unsigned n=0;n<sizeof ring;n++)if(ring[n]!=ram[(0x4000+n)^1]){
   fprintf(stderr,"Stream ring mismatch axis %u mode %u case %u byte %u\n",horizontal,mode,sample,n);return 1;
  }
  transfers++;
 }
 unsigned starts=0;
 for(unsigned stage=0;stage<7;stage++)for(unsigned sample=0;sample<256;sample++){
  uint8_t record[128];for(unsigned n=0;n<128;n++)record[n]=(n*17+sample)&255;
  record[2]=record[3]=0;
  for(unsigned n=0;n<sizeof ring;n++){ring[n]=(sample+n*23)&255;ram[(0x4000+n)^1]=ring[n];}
  uint16_t sx=0xabcd,sy=0xdef0;
  putRamWord(ram,0x8344,stage);putRamWord(ram,0x8036,sx);putRamWord(ram,0x8038,sy);
  for(unsigned step=0;step<5;step++){
   uint16_t x=sample*113u+step*2,y=sample*7919u-step*2,stageX=sample*93u;
   for(unsigned n=0;n<128;n++)ram[(0x8482+n)^1]=record[n];
   putRamWord(ram,0x8408,x);putRamWord(ram,0x840c,y);putRamWord(ram,0x8398,stageX);
   if(!willowScroll3ControllerCopy(record,stage,0,0,x,y,stageX,&sx,&sy,sourceByte,sourceWord,ringWrite,pairCopy))return 2;
   RUN(0x42f48,0);
   for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x8482+n)^1]){
    fprintf(stderr,"Start stage %u sample %u step %u byte %u native %02x source %02x\n",stage,sample,step,n,record[n],ram[(0x8482+n)^1]);return 1;
   }
   for(unsigned n=0;n<sizeof ring;n++)if(ring[n]!=ram[(0x4000+n)^1]){
    fprintf(stderr,"Start map stage %u sample %u step %u byte %u\n",stage,sample,step,n);return 1;
   }
   if(sx!=(unsigned)((ram[0x8036^1]<<8)|ram[0x8037^1]) || sy!=(unsigned)((ram[0x8038^1]<<8)|ram[0x8039^1]))return 1;
   starts++;
  }
 }
 printf("Scroll3 initialization: %u phase updates match original full controller, map and staged registers\n",starts);
 unsigned active=0;
 for(unsigned stage=0;stage<7;stage++)for(unsigned mode=0;mode<4;mode++)for(unsigned sample=0;sample<256;sample++){
  uint8_t record[128];for(unsigned n=0;n<128;n++)record[n]=(n*17+sample)&255;
  record[33]=mode;record[52]=(sample%4)+1;
  record[64]=(sample&1)*32;record[65]=((sample>>1)&1)*32;
  record[76]=(sample&4)?4:0;record[77]=(sample&8)?12:8;
  willowMotionPutLong(record+66,0x2dc30);
  willowMotionPutWord(record+106,sample*271);willowMotionPutWord(record+108,sample*7919u);
  for(unsigned n=0;n<128;n++)ram[(0x9846+n)^1]=record[n];
  for(unsigned n=0;n<sizeof ring;n++){ring[n]=(sample+n*23)&255;ram[(0x4000+n)^1]=ring[n];}
  uint16_t sx=0xabcd,sy=0xdef0,phase=sample%8;uint8_t freeze=(sample>>3)&1;
  putRamWord(ram,0x8344,stage);putRamWord(ram,0x8374,phase);ram[0x8470^1]=freeze;
  putRamWord(ram,0x8036,sx);putRamWord(ram,0x8038,sy);
  if(!willowScroll3ActiveCopy(record,stage,phase,freeze,&sx,&sy,sourceByte,sourceWord,ringWrite,pairCopy))return 2;
  m68k_set_reg(14,0xff9846);RUN(0x43086,0);
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x9846+n)^1]){
   fprintf(stderr,"Active actor stage %u mode %u case %u byte %u native %02x source %02x\n",stage,mode,sample,n,record[n],ram[(0x9846+n)^1]);return 1;
  }
  for(unsigned n=0;n<sizeof ring;n++)if(ring[n]!=ram[(0x4000+n)^1]){
   fprintf(stderr,"Active ring stage %u mode %u case %u byte %u\n",stage,mode,sample,n);return 1;
  }
  if(sx!=(unsigned)((ram[0x8036^1]<<8)|ram[0x8037^1]) || sy!=(unsigned)((ram[0x8038^1]<<8)|ram[0x8039^1]))return 1;
  active++;
 }
 printf("Scroll3 active motion/publication: %u cases match source controller, full map and staged registers\n",active);
 printf("Scroll3 lookup: %u cases match source addresses and all 128 controller bytes\n",checks);
 printf("Scroll3 transfers: %u cases match all 16384 map bytes and controller bytes\n",transfers);
 return 0;
}
