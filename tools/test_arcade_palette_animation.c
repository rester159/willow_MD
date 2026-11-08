/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_palette_animation.h"
#include "../src/willow_arcade_services.h"
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
static uint16_t colors[2048];
static uint16_t readColor(uint32_t at){if(at<0x91c000 || at>=0x91d000 || (at&1))abort();return colors[(at-0x91c000)/2];}
static void writeColor(uint32_t at,uint16_t value){if(at<0x91c000 || at>=0x91d000 || (at&1))abort();colors[(at-0x91c000)/2]=value;}
static uint8_t sourceByte(uint32_t at){if(at>=0x100000)abort();return rom[at];}
static uint8_t pool[16][64];static WillowArcadeRandom rng;static WillowArcadeSoundQueue sounds;
static uint8_t*allocate(void){for(unsigned n=0;n<16;n++)if(!pool[n][0])return pool[n];return NULL;}
static uint8_t randomByte(void){return willowArcadeRandomNext(&rng);}
static void sound(uint16_t command){willowArcadeSoundCommand(&sounds,command,0);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Only palette RAM base operands are relocated for the oracle. */
 const unsigned patches[]={0xc2e,0x1276};
 for(unsigned n=0;n<2;n++){if(willowMotionLong(rom+patches[n])!=0x91c000)return 2;willowMotionPutLong(rom+patches[n],0xff2000);}
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/palette-animation-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/palette-animation-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)

 unsigned checks=0;uint32_t random=0x57494c4c;
 const unsigned types[]={0,1,2,3,4,5,6,7},entries[]={0xc0a,0xd1a,0xeba,0x100e,0x1112,0x11f0,0x1224,0x1252};
 for(unsigned type=0;type<8;type++)for(unsigned sample=0;sample<8192;sample++){
  uint8_t record[64];for(unsigned n=0;n<64;n++){random=random*1664525u+1013904223u;record[n]=random>>24;}
  record[32]=types[type];record[2]=((sample/2048)%4)*2;record[3]=(sample&1)*2;
  record[60]=(sample/128)%4;record[56]=(sample/16)%24;record[58]=(sample/64)%8;
  if(type==4 && (sample&8))record[56]=10;
  record[34]=(sample%4)?1:0;record[59]=(sample/4)%20;record[57]=(sample/16)%8;record[61]=(sample/32)%4;
  uint32_t destination=0x91c000+record[60]*1024+(type==7?0:record[56]*32);
  willowMotionPutLong(record+48,destination);willowMotionPutLong(record+52,0xea260);
  WillowPaletteAnimation g={0};g.cameraX=sample*113;g.cameraY=sample*271;
  g.bypassA=sample&1;g.bypassB=(sample>>1)&1;g.stage1Gate=(sample>>2)&1;g.stageX=1536+(sample%64)*256;
  g.byte=sourceByte;g.word=sourceWord;g.read=readColor;g.write=writeColor;
  g.frame=sample&255;g.event=(sample>>5)&1;g.cycleEvent=(sample>>4)&1;
  g.allocate=allocate;g.random=randomByte;g.sound=sound;
  willowMotionPutWord(record+4,(sample&2)?1:0);
  for(unsigned n=0;n<16;n++)for(unsigned j=0;j<64;j++){
   pool[n][j]=(sample+n*17+j*29)&255;if(!j)pool[n][j]=n<sample%17?1:0;
   ram[(0xf042+n*64+j)^1]=pool[n][j];
  }
  rng.state=sample*271;rng.scratch=sample;putRamWord(ram,0x8284,rng.state);ram[0x8286^1]=rng.scratch;
  memset(&sounds,0,sizeof sounds);ram[0x8290^1]=0;putRamWord(ram,0x801e,0);
  for(unsigned n=0;n<128;n++)ram[(0x8172+n)^1]=0;
  ram[0x83cf^1]=g.event;ram[0x83e3^1]=g.cycleEvent;
  if(sample&8){willowMotionPutWord(record+6,g.cameraX);willowMotionPutWord(record+10,g.cameraY);}
  for(unsigned n=0;n<64;n++)ram[(0x9846+n)^1]=record[n];
  /* Store the exact relocated pointer, preserving its byte offset. */
  uint32_t pointer=0xff2000+(destination-0x91c000);
  putRamWord(ram,0x9876,pointer>>16);putRamWord(ram,0x9878,pointer);
  putRamWord(ram,0x8408,g.cameraX);putRamWord(ram,0x840c,g.cameraY);putRamWord(ram,0x8398,g.stageX);
  ram[0x8293^1]=g.bypassA;ram[0x83c3^1]=g.bypassB;ram[0x8395^1]=g.stage1Gate;ram[0x8019^1]=sample&255;
  for(unsigned n=0;n<2048;n++){random=random*1664525u+1013904223u;colors[n]=random>>8;putRamWord(ram,0x2000+n*2,colors[n]);}
  if(!willowPaletteAnimate(record,&g))return 2;
  m68k_set_reg(14,0xff9846);RUN(entries[type],0);
  /* Normalize only the relocated palette pointer before raw comparison. */
  uint32_t raw=0;for(unsigned n=0;n<4;n++)raw=(raw<<8)|ram[(0x9876+n)^1];raw=raw-0xff2000+0x91c000;
  putRamWord(ram,0x9876,raw>>16);putRamWord(ram,0x9878,raw);
  for(unsigned n=0;n<64;n++)if(record[n]!=ram[(0x9846+n)^1]){fprintf(stderr,"Palette type %u case %u byte %u native %02x source %02x\n",types[type],sample,n,record[n],ram[(0x9846+n)^1]);return 1;}
  for(unsigned n=0;n<2048;n++)if(colors[n]!=((ram[(0x2000+n*2)^1]<<8)|ram[(0x2001+n*2)^1])){fprintf(stderr,"Palette type %u case %u color %u\n",types[type],sample,n);return 1;}
  for(unsigned n=0;n<16;n++)for(unsigned j=0;j<64;j++)if(pool[n][j]!=ram[(0xf042+n*64+j)^1]){fprintf(stderr,"Palette allocation type %u case %u slot %u byte %u\n",type,sample,n,j);return 1;}
  if(rng.state!=((ram[0x8284^1]<<8)|ram[0x8285^1]) || rng.scratch!=ram[0x8286^1] || g.cycleEvent!=ram[0x83e3^1])return 1;
  if(sounds.writeOffset!=((ram[0x801e^1]<<8)|ram[0x801f^1]))return 1;
  for(unsigned n=0;n<64;n++)if(sounds.words[n]!=((ram[(0x8172+n*2)^1]<<8)|ram[(0x8173+n*2)^1]))return 1;
  checks++;
 }
 unsigned schedulerChecks=0;
 for(unsigned scenario=0;scenario<32;scenario++){
  WillowPaletteAnimation g={0};g.byte=sourceByte;g.word=sourceWord;g.read=readColor;g.write=writeColor;
  g.allocate=allocate;g.random=randomByte;g.sound=sound;g.bypassA=1;
  g.cameraX=3328;g.cameraY=128;g.stageX=3072;g.stage1Gate=1;
  for(unsigned n=0;n<16;n++){
   memset(pool[n],0,64);pool[n][0]=n<scenario%17?1:0;pool[n][32]=(n+scenario)%8;
   pool[n][60]=n%4;pool[n][56]=n==4?10:n;pool[n][58]=1;pool[n][59]=3;
   willowMotionPutLong(pool[n]+52,0xea260);
   willowMotionPutWord(pool[n]+8,7936);willowMotionPutWord(pool[n]+12,768);
   for(unsigned j=0;j<64;j++)ram[(0xf042+n*64+j)^1]=pool[n][j];
  }
  rng.state=1+scenario*271;rng.scratch=0;putRamWord(ram,0x8284,rng.state);ram[0x8286^1]=0;
  memset(&sounds,0,sizeof sounds);ram[0x8290^1]=0;putRamWord(ram,0x801e,0);
  for(unsigned n=0;n<128;n++)ram[(0x8172+n)^1]=0;
  for(unsigned n=0;n<2048;n++){colors[n]=n*73+scenario;putRamWord(ram,0x2000+n*2,colors[n]);}
  for(unsigned tick=0;tick<128;tick++){
   g.frame=tick;g.event=tick>=96;g.cycleEvent=tick==16?1:tick==80?0:g.cycleEvent;
   ram[0x8019^1]=g.frame;ram[0x83cf^1]=g.event;ram[0x83e3^1]=g.cycleEvent;
   ram[0x8293^1]=g.bypassA;ram[0x83c3^1]=g.bypassB;ram[0x8395^1]=g.stage1Gate;
   putRamWord(ram,0x8408,g.cameraX);putRamWord(ram,0x840c,g.cameraY);putRamWord(ram,0x8398,g.stageX);
   if(!willowPaletteSchedule(&pool[0][0],&g))return 2;
   RUN(0xbc4,0);
   for(unsigned n=0;n<16;n++){
    uint8_t observed[64];for(unsigned j=0;j<64;j++)observed[j]=ram[(0xf042+n*64+j)^1];
    uint32_t pointer=willowMotionLong(observed+48);
    if(pointer>=0xff2000 && pointer<0xff3000)willowMotionPutLong(observed+48,pointer-0xff2000+0x91c000);
    if(memcmp(pool[n],observed,64)){fprintf(stderr,"Palette scheduler scenario %u tick %u slot %u\n",scenario,tick,n);return 1;}
   }
   for(unsigned n=0;n<2048;n++)if(colors[n]!=((ram[(0x2000+n*2)^1]<<8)|ram[(0x2001+n*2)^1]))return 1;
   if(rng.state!=((ram[0x8284^1]<<8)|ram[0x8285^1]) || rng.scratch!=ram[0x8286^1] || g.cycleEvent!=ram[0x83e3^1])return 1;
   if(sounds.writeOffset!=((ram[0x801e^1]<<8)|ram[0x801f^1]))return 1;
   for(unsigned n=0;n<64;n++)if(sounds.words[n]!=((ram[(0x8172+n*2)^1]<<8)|ram[(0x8173+n*2)^1]))return 1;
   schedulerChecks++;
  }
 }
 printf("Full palette scheduler: %u successive updates match original pools, colors, events, RNG and sound ring\n",schedulerChecks);
 printf("All eight palette handlers: %u cases match controller/pool bytes, 2048 CPS color words, RNG, sound and event globals\n",checks);
 return 0;
}
