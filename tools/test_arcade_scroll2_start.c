/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_scroll2_start.h"
#include "../src/willow_arcade_palette_animation.h"
#include "../src/willow_arcade_services.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#ifdef WILLOW_TEST_PACKED_MAP
#include "test_arcade_map_sink.h"
#include "../native/intro_probe/src/camera_data.h"
#endif
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
static uint8_t ring[16384];
static void ringWrite(uint16_t at,uint16_t value){if(at+1>=sizeof ring)abort();willowMotionPutWord(ring+at,value);}
static uint8_t sourceByte(uint32_t at){if(at>=0x100000)abort();return rom[at];}
#ifdef WILLOW_TEST_PACKED_MAP
static uint8_t cameraByte(uint32_t at){
 uint8_t value;
 if(!nativeCameraByteRead(at,&value) || value!=sourceByte(at)){fprintf(stderr,"Camera byte data mismatch %06x\n",(unsigned)at);abort();}
 return value;
}
static uint16_t cameraWord(uint32_t at){
 uint16_t value;
 if(!nativeCameraWordRead(at,&value) || value!=sourceWord(at)){fprintf(stderr,"Camera word data mismatch %06x\n",(unsigned)at);abort();}
 return value;
}
#endif

static uint8_t palettes[16][64];static uint16_t paletteCalls;
static uint8_t*allocatePalette(void){for(unsigned n=0;n<16;n++)if(!palettes[n][0])return palettes[n];return NULL;}
static WillowArcadeRandom rng;static WillowArcadeSoundQueue sounds;
static uint8_t randomByte(void){return willowArcadeRandomNext(&rng);}
static void sound(uint16_t command){willowArcadeSoundCommand(&sounds,command,0);}
static uint16_t colors[2048];static uint8_t*currentCamera;static WillowPaletteAnimation paletteContext;
static uint16_t readColor(uint32_t at){if(at<0x91c000 || at>=0x91d000)abort();return colors[(at-0x91c000)/2];}
static void writeColor(uint32_t at,uint16_t value){if(at<0x91c000 || at>=0x91d000)abort();colors[(at-0x91c000)/2]=value;}
static void paletteStep(void){
 paletteCalls++;paletteContext.cameraX=willowMotionWord(currentCamera+6);paletteContext.cameraY=willowMotionWord(currentCamera+10);
 if(!willowPaletteSchedule(&palettes[0][0],&paletteContext))abort();
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Verified test-only CPS tile RAM operand relocation. */
 if(willowMotionLong(rom+0x4365a)!=0x918000)return 2;
 willowMotionPutLong(rom+0x4365a,0xff4000);
 /* Real palette handlers run; only their CPS base operands relocate. */
 const unsigned palettePatches[]={0xc2e,0x1276};
 for(unsigned n=0;n<2;n++){if(willowMotionLong(rom+palettePatches[n])!=0x91c000)return 2;willowMotionPutLong(rom+palettePatches[n],0xff2000);}
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scroll2-start-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scroll2-start-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)

 unsigned checks=0;uint32_t random=0x57494c4c;
 for(unsigned stage=0;stage<7;stage++)for(unsigned phase=0;phase<5;phase++)for(unsigned sample=0;sample<128;sample++){
  uint8_t player[128]={0},record[128];for(unsigned n=0;n<128;n++){random=random*1664525u+1013904223u;record[n]=random>>24;}
  record[2]=0;record[3]=phase*2;record[72]=phase<2?0:(phase-1)*4;
  record[33]=sourceByte(0x2dc1c+stage);
  willowMotionPutLong(record+66,willowMotionLong(rom+0x2dbf8+stage*4));
  for(unsigned n=0;n<128;n++)ram[(0x8402+n)^1]=record[n];
  WillowScroll2Bounds g={0};g.stage=stage;g.stage1Gate=sample&1;g.stageX=sample*271;
  WillowScroll2Publication p={0};p.minimumX=sample*79;p.maximumX=sample*173;p.minimumY=sample*101;p.maximumY=sample*137;
  p.scrollX=0xabcd;p.scrollY=0x1234;uint16_t control=sample*311;
  putRamWord(ram,0x8344,stage);ram[0x8395^1]=g.stage1Gate;putRamWord(ram,0x8398,g.stageX);
  putRamWord(ram,0x838c,p.minimumX);putRamWord(ram,0x838e,p.maximumX);putRamWord(ram,0x8390,p.minimumY);putRamWord(ram,0x8392,p.maximumY);
  putRamWord(ram,0x8032,p.scrollX);putRamWord(ram,0x8034,p.scrollY);putRamWord(ram,0x804a,control);
  const unsigned types[]={0,1,2,3,4,5,6,7};
  for(unsigned n=0;n<16;n++){
   for(unsigned j=0;j<64;j++)palettes[n][j]=(sample+n*7+j*19)&255;
   palettes[n][0]=n<sample%17?1:0;palettes[n][2]=palettes[n][0] && (sample&4)?2:0;palettes[n][32]=types[n%8];
   palettes[n][60]=n%4;palettes[n][56]=n;
   palettes[n][3]=0;palettes[n][34]=1;palettes[n][57]=0;palettes[n][58]=2;palettes[n][59]=3;palettes[n][61]=0;
   uint32_t address=0x91c000+(n%4)*1024+(types[n%8]==7?0:n*32);
   willowMotionPutLong(palettes[n]+48,address);willowMotionPutLong(palettes[n]+52,0xea260);
   for(unsigned j=0;j<64;j++)ram[(0xf042+n*64+j)^1]=palettes[n][j];
   uint32_t relocated=address-0x91c000+0xff2000;
   putRamWord(ram,0xf042+n*64+48,relocated>>16);putRamWord(ram,0xf042+n*64+50,relocated);
  }
  paletteContext=(WillowPaletteAnimation){0};paletteContext.stageX=g.stageX;paletteContext.stage1Gate=g.stage1Gate;
  paletteContext.byte=sourceByte;paletteContext.word=sourceWord;paletteContext.read=readColor;paletteContext.write=writeColor;
  paletteContext.allocate=allocatePalette;paletteContext.random=randomByte;paletteContext.sound=sound;
  rng=(WillowArcadeRandom){(uint16_t)(sample*271+1),0};memset(&sounds,0,sizeof sounds);
  putRamWord(ram,0x8284,rng.state);ram[0x8286^1]=0;putRamWord(ram,0x801e,0);ram[0x8290^1]=0;
  for(unsigned n=0;n<128;n++)ram[(0x8172+n)^1]=0;
  ram[0x8019^1]=0;ram[0x83cf^1]=0;ram[0x83e3^1]=0;
  currentCamera=record;paletteContext.bypassA=1;ram[0x8293^1]=1;ram[0x83c3^1]=0;
  for(unsigned n=0;n<2048;n++){colors[n]=n*73+sample;putRamWord(ram,0x2000+n*2,colors[n]);}
  for(unsigned n=0;n<sizeof ring;n++){ring[n]=(sample+n*23)&255;ram[(0x4000+n)^1]=ring[n];}
  paletteCalls=0;putRamWord(ram,0x1ffe,0);
  willowMotionPutWord(player+6,sample*733);willowMotionPutWord(player+10,sample*571);
  for(unsigned n=0;n<128;n++)ram[(0x9846+n)^1]=player[n];
  uint8_t usePlayer=(sample>>1)&1;uint16_t alternateX=sample*379,alternateY=sample*113;
  ram[0x8348^1]=usePlayer;putRamWord(ram,0x0fb0,alternateX);putRamWord(ram,0x0fb2,alternateY);
#ifdef WILLOW_TEST_PACKED_MAP
  mapSeed(ring);
  if(!willowScroll2ControllerCopy(record,player,&g,&p,&control,usePlayer,alternateX,alternateY,cameraByte,cameraWord,ringWrite,allocatePalette,paletteStep,mapCopy))return 2;
#else
  if(!willowScroll2Controller(record,player,&g,&p,&control,usePlayer,alternateX,alternateY,sourceByte,sourceWord,ringWrite,allocatePalette,paletteStep))return 2;
#endif
#ifdef WILLOW_TEST_PACKED_MAP
  mapRead(ring);
#endif
  m68k_set_reg(14,0xff8402);RUN(0x42602,0);
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x8402+n)^1]){
   fprintf(stderr,"Camera start stage %u phase %u case %u byte %u native %02x source %02x\n",stage,phase,sample,n,record[n],ram[(0x8402+n)^1]);return 1;
  }
  for(unsigned n=0;n<sizeof ring;n++)if(ring[n]!=ram[(0x4000+n)^1]){fprintf(stderr,"Start ring stage %u phase %u case %u byte %u\n",stage,phase,sample,n);return 1;}
  for(unsigned n=0;n<16;n++){
   uint32_t expected=willowMotionLong(palettes[n]+48);
   if(expected>=0x91c000 && expected<0x91d000){
    unsigned at=0xf042+n*64+48;uint32_t pointer=0;
    for(unsigned j=0;j<4;j++)pointer=(pointer<<8)|ram[(at+j)^1];
    pointer=pointer-0xff2000+0x91c000;putRamWord(ram,at,pointer>>16);putRamWord(ram,at+2,pointer);
   }
   for(unsigned j=0;j<64;j++)if(palettes[n][j]!=ram[(0xf042+n*64+j)^1]){fprintf(stderr,"Palette scheduler stage %u phase %u case %u slot %u byte %u\n",stage,phase,sample,n,j);return 1;}
  }
  for(unsigned n=0;n<2048;n++)if(colors[n]!=((ram[(0x2000+n*2)^1]<<8)|ram[(0x2001+n*2)^1]))return 1;
  const unsigned offsets[]={0x8032,0x8034,0x804a};const unsigned values[]={p.scrollX,p.scrollY,control};
  for(unsigned n=0;n<3;n++)if(values[n]!=(unsigned)((ram[offsets[n]^1]<<8)|ram[(offsets[n]+1)^1]))return 1;
  if(rng.state!=((ram[0x8284^1]<<8)|ram[0x8285^1]) || rng.scratch!=ram[0x8286^1] || paletteContext.cycleEvent!=ram[0x83e3^1])return 1;
  if(sounds.writeOffset!=((ram[0x801e^1]<<8)|ram[0x801f^1]))return 1;
  for(unsigned n=0;n<64;n++)if(sounds.words[n]!=((ram[(0x8172+n*2)^1]<<8)|ram[(0x8173+n*2)^1]))return 1;
  checks++;
 }
 printf("Scroll2 cold start: %u cases match camera record, entire map ring, real palette scheduling, palette RAM and scroll/control registers\n",checks);
 return 0;
}
