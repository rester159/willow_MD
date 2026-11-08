/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include <stdint.h>
static uint8_t rowCopy(uint16_t,uint32_t,uint16_t);
#define WILLOW_SCROLL2_ROW_COPY rowCopy
#include "../src/willow_arcade_scroll2.h"
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
 while(count--){ringWrite(at,(uint16_t)(sourceWord(source)+0));ringWrite(at+2,sourceWord(source+2));at+=4;source+=4;}return 1;
}
static uint8_t rowCopy(uint16_t at,uint32_t source,uint16_t count){
 while(count--){if(!pairCopy(at,source,1))return 0;at+=64;source+=64;}return 1;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scroll2-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scroll2-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
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
  uint32_t address;if(!willowScroll2Record(record,x,y,sourceByte,&address))return 2;
  m68k_set_reg(14,0xff9846);m68k_set_reg(5,x);m68k_set_reg(6,y);
  RUN(0x4368e,0xabcdef01);
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
  if(!willowScroll2Record(record,x,y,sourceByte,&source))return 2;
  uint16_t destination=((x&0x3f0)<<2)+((y&0x300)<<4)+((y&0xf0)>>2);
  for(unsigned n=0;n<128;n++)ram[(0x9846+n)^1]=record[n];
  for(unsigned n=0;n<sizeof ring;n++){ring[n]=(sample+n*23)&255;ram[(0x4000+n)^1]=ring[n];}
  if(!willowScroll2TransferCopy(record,horizontal?x:y,destination,source,horizontal,sourceByte,sourceWord,ringWrite,pairCopy))return 2;
  m68k_set_reg(14,0xff9846);m68k_set_reg(5,x);m68k_set_reg(6,y);
  m68k_set_reg(8,0xff4000+destination);m68k_set_reg(9,source);
  RUN(horizontal?0x4382a:0x43768,0);
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x9846+n)^1]){
   fprintf(stderr,"Stream actor mismatch axis %u mode %u case %u byte %u\n",horizontal,mode,sample,n);return 1;
  }
  for(unsigned n=0;n<sizeof ring;n++)if(ring[n]!=ram[(0x4000+n)^1]){
   fprintf(stderr,"Stream ring mismatch axis %u mode %u case %u byte %u\n",horizontal,mode,sample,n);return 1;
  }
  transfers++;
 }
 unsigned centers=0;
 for(unsigned sample=0;sample<8192;sample++){
  uint8_t record[128];for(unsigned n=0;n<128;n++){random=random*1664525u+1013904223u;record[n]=random>>24;}
  for(unsigned n=0;n<128;n++)ram[(0x9846+n)^1]=record[n];
  WillowScroll2Publication publication={0};willowScroll2Center(record,&publication);
  m68k_set_reg(14,0xff9846);RUN(0x42754,0);
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x9846+n)^1]){
   fprintf(stderr,"Camera center case %u byte %u\n",sample,n);return 1;
  }
  if(publication.scrollX!=((ram[0x8032^1]<<8)|ram[0x8033^1]) || publication.scrollY!=((ram[0x8034^1]<<8)|ram[0x8035^1]))return 1;
  centers++;
 }
 printf("Scroll2 initial centering: %u cases match original camera record and hardware scroll outputs\n",centers);
 /* One verified operand relocation redirects CPS tile RAM to the
  * oracle's work RAM; publication instructions and branches are unchanged. */
 if(willowMotionLong(rom+0x4365a)!=0x918000)return 2;
 willowMotionPutLong(rom+0x4365a,0xff4000);
 /* The core owns its ROM copy; unload/reload the modified test wrapper. */
 FN(void,retro_unload_game,(void));retro_unload_game();
 f=fopen("build/scroll2-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 if(!retro_load_game(&game))return 2;
 unsigned publications=0;
 for(unsigned stage=0;stage<7;stage++)for(unsigned mode=0;mode<4;mode++)for(unsigned sample=0;sample<256;sample++){
  uint8_t record[128];for(unsigned n=0;n<128;n++)record[n]=(n*17+sample)&255;
  record[33]=mode;record[3]=(sample%8)*2;record[48]=sample&1;
  record[64]=(sample&1)*16;record[65]=((sample>>1)&1)*16;
  record[76]=(sample&4)?4:0;record[77]=(sample&8)?12:8;
  willowMotionPutLong(record+66,0x2dc30);
  willowMotionPutWord(record+6,stage==3?4400+sample*4:sample*271);
  willowMotionPutWord(record+10,sample*7919u);
  for(unsigned n=0;n<128;n++)ram[(0x9846+n)^1]=record[n];
  for(unsigned n=0;n<sizeof ring;n++){ring[n]=(sample+n*23)&255;ram[(0x4000+n)^1]=ring[n];}
  WillowScroll2Publication publication={0};publication.stage=stage;publication.alternate=(sample>>1)&1;publication.stageX=0x1234;
  putRamWord(ram,0x8344,stage);ram[0x83dc^1]=publication.alternate;putRamWord(ram,0x8398,publication.stageX);
  if(!willowScroll2PublishCopy(record,&publication,sourceByte,sourceWord,ringWrite,pairCopy))return 2;
  m68k_set_reg(14,0xff9846);RUN(0x4282e,0);
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x9846+n)^1]){
   fprintf(stderr,"Publication actor stage %u mode %u case %u byte %u\n",stage,mode,sample,n);return 1;
  }
  for(unsigned n=0;n<sizeof ring;n++)if(ring[n]!=ram[(0x4000+n)^1]){
   fprintf(stderr,"Publication ring stage %u mode %u case %u byte %u\n",stage,mode,sample,n);return 1;
  }
  const unsigned offsets[]={0x8032,0x8034,0x838c,0x838e,0x8390,0x8392,0x8398};
  const unsigned values[]={publication.scrollX,publication.scrollY,publication.minimumX,publication.maximumX,publication.minimumY,publication.maximumY,publication.stageX};
  for(unsigned n=0;n<7;n++)if(values[n]!=(unsigned)((ram[offsets[n]^1]<<8)|ram[(offsets[n]+1)^1])){
   fprintf(stderr,"Publication global stage %u case %u at %x\n",stage,sample,offsets[n]);return 1;
  }
  publications++;
 }
 printf("Scroll2 camera publication: %u cases match source ring, actor, scroll registers and stage outputs\n",publications);
 /* Rebuild the forest viewport from source tables, then compare the
  * actual captured CPS ring. Capture is verification only, never an input. */
 uint8_t capture[0x30000];f=fopen("build/arcade-trace/intro-vram-1530.bin","rb");
 if(!f || fread(capture,1,sizeof capture,f)!=sizeof capture)return 2;fclose(f);
 memset(ring,0xa5,sizeof ring);
 for(unsigned x=0;x<384;x+=16){
  uint8_t record[128]={0};record[33]=sourceByte(0x2dc1c);
  willowMotionPutLong(record+66,willowMotionLong(rom+0x2dbf8));
  uint32_t source;uint16_t inverseY=0xff00;
  if(!willowScroll2Record(record,x,inverseY,sourceByte,&source))return 2;
  uint16_t destination=((x&0x3f0)<<2)+((inverseY&0x300)<<4);
  if(!willowScroll2TransferCopy(record,inverseY,destination,source,0,sourceByte,sourceWord,ringWrite,pairCopy))return 2;
 }
 unsigned cells=0;
 for(unsigned y=0;y<256;y+=16)for(unsigned x=0;x<384;x+=16){
  uint16_t iy=(uint16_t)~y,at=((x&0x3f0)<<2)+((iy&0x300)<<4)+((iy&0xf0)>>2);
  if(memcmp(ring+at,capture+0x18000+at,4)){fprintf(stderr,"Forest capture mismatch x %u y %u\n",x,y);return 1;}
  cells++;
 }
 printf("Source-loaded forest: all %u tile codes and full attributes match captured CPS ring\n",cells);
 printf("Scroll2 source lookup: %u cases match original record addresses and all 128 streaming-actor bytes\n",checks);
 printf("Scroll2 transfers: %u row/column cases match all 16384 ring bytes and streaming-actor state\n",transfers);
 return 0;
}
