/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_scroll2_motion.h"
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
static uint16_t sourceWord(uint32_t at){if(at+1>=0x100000)abort();return willowMotionWord(rom+at);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scroll2-motion-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scroll2-motion-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)

 unsigned checks=0;uint32_t random=0x57494c4c;
 for(unsigned mode=0;mode<8;mode++)for(unsigned sample=0;sample<4096;sample++){
  uint8_t record[128];for(unsigned n=0;n<128;n++){random=random*1664525u+1013904223u;record[n]=random>>24;}
  record[4]=mode*2;record[103]=sample&1;record[49]=(sample>>1)&1;record[84]=(sample>>2)&1;
  willowMotionPutLong(record+92,0x42e90);
  uint16_t y=(sample&8)?(sample&511):willowMotionWord(record+10);
  willowMotionPutWord(record+10,y);
  const uint16_t distances[]={0,127,128,129,130,131,199,200,201,32767,32768,65535};
  willowMotionPutWord(record+108,y+distances[sample%12]);
  const uint16_t speeds[]={0,1,64,128,1791,1792,65535,0xff80,0x7fff,0x8000};
  willowMotionPutWord(record+82,speeds[(sample/8)%10]);willowMotionPutWord(record+44,speeds[(sample/16)%10]);
  for(unsigned n=0;n<128;n++)ram[(0x9846+n)^1]=record[n];
  uint8_t airborne=(sample>>4)&1;ram[0x988e^1]=airborne;
  /* Put the camera actor away from the player airborne global. */
  for(unsigned n=0;n<128;n++)ram[(0x8402+n)^1]=record[n];
  if(!willowScroll2Vertical(record,airborne,sourceWord))return 2;
  m68k_set_reg(14,0xff8402);RUN(0x42b1c,0);
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x8402+n)^1]){
   fprintf(stderr,"Vertical camera mode %u case %u byte %u native %02x source %02x\n",mode,sample,n,record[n],ram[(0x8402+n)^1]);return 1;
  }
  checks++;
 }
 unsigned boundChecks=0;
 for(unsigned region=0;region<2;region++)for(unsigned stage=0;stage<7;stage++)for(unsigned sample=0;sample<2048;sample++){
  uint8_t record[128];for(unsigned n=0;n<128;n++){random=random*1664525u+1013904223u;record[n]=random>>24;}
  record[102]=(sample>>6)&1;
  const uint16_t edges[]={2367,2368,3527,3528,4607,4608,4735,4736,6911,6912,7039,7040,65535};
  willowMotionPutWord(record+6,edges[sample%13]);
  WillowScroll2Bounds globals={0};globals.stage=stage;globals.stageX=0xabcd;
  globals.playerY=willowMotionWord(record+10)+(sample&255);
  globals.gateA=sample&1;globals.gateB=(sample>>1)&1;globals.eventA=(sample>>2)&1;globals.eventB=(sample>>3)&1;
  globals.stage1Gate=(sample>>4)&1;globals.stage1Event=(sample>>5)&1;
  for(unsigned n=0;n<128;n++)ram[(0x8402+n)^1]=record[n];
  putRamWord(ram,0x8344,stage);putRamWord(ram,0x8398,globals.stageX);putRamWord(ram,0x9850,globals.playerY);
  ram[0x838a^1]=globals.gateA;ram[0x838b^1]=globals.gateB;ram[0x83e7^1]=globals.eventA;ram[0x83cf^1]=globals.eventB;
  ram[0x8395^1]=globals.stage1Gate;ram[0x83dd^1]=globals.stage1Event;
  if(region)willowScroll2VerticalRegion(record,&globals);
  else if(!willowScroll2StageBounds(record,&globals))return 2;
  m68k_set_reg(14,0xff8402);RUN(region?0x42d16:0x42d66,0);
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x8402+n)^1]){
   fprintf(stderr,"Camera bounds region %u stage %u case %u byte %u\n",region,stage,sample,n);return 1;
  }
  if(globals.gateA!=ram[0x838a^1] || globals.gateB!=ram[0x838b^1] || globals.stageX!=((ram[0x8398^1]<<8)|ram[0x8399^1]))return 1;
  boundChecks++;
 }
 printf("Scroll2 stage bounds/regions: %u cases match original actor and stage globals\n",boundChecks);
 printf("Scroll2 vertical motion: %u cases match all original camera bytes across eight states\n",checks);
 return 0;
}
