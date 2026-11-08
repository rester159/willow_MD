/* Differential verification of queued text task against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_pan_init.h"
#include "../src/willow_arcade_pool_init.h"
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

static uint8_t expected[65536];
static uint16_t romWord(uint32_t at){return willowMotionWord(rom+at);}
static uint8_t romByte(uint32_t at){return rom[at];}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


static unsigned writes,region;
static void scroll2Write(uint16_t at,uint16_t v){if(!region){ww(0x2000+at,v);writes++;}}
static void scroll3Write(uint16_t at,uint16_t v){if(region){ww(0x2000+at,v);writes++;}}
#ifdef WILLOW_TEST_PAN_COPY
static unsigned copies;
static uint8_t nearCopy(uint16_t at,uint32_t source,uint16_t count){
 if(count!=256)return 0;
 for(unsigned n=0;n<count;n++){scroll2Write(at+n*4,romWord(source+n*4));scroll2Write(at+n*4+2,romWord(source+n*4+2));}
 copies++;return 1;
}
static uint8_t farCopy(uint16_t at,uint32_t source,uint16_t count){
 if(count!=64)return 0;
 for(unsigned n=0;n<count;n++){scroll3Write(at+n*4,romWord(source+n*4)+0xa00);scroll3Write(at+n*4+2,romWord(source+n*4+2));}
 copies++;return 1;
}
#define PAN_INIT(b,p,s,v) willowArcadeAttractPanInitVariantWithCopy(b,p,s,romByte,romWord,scroll2Write,scroll3Write,v,nearCopy,farCopy)
#else
#define PAN_INIT(b,p,s,v) willowArcadeAttractPanInitVariant(b,p,s,romByte,romWord,scroll2Write,scroll3Write,v)
#endif
int main(int argc,char**argv){
 if(argc!=3&&argc!=4)return 2;unsigned second=argc==4;region=strtoul(argv[2],0,10);if(region>1)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutLong(rom+(region?0x43d50:0x4365a),0xff2000);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/attract-pan-init-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-pan-init-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  willowArcadePoolsInit(&bus);willowArcadeTaskInitFree(&bus,romWord);
  for(unsigned n=0;n<sample%9;n++)willowArcadeTaskAllocate(&bus,0x509c,0,3);
  unsigned used=sample%50;ww(0x981c,rw(0x981c)+used*2);wb(0x9801,50-used);
  wb(0x8423,sample%4);wb(0x84a3,(sample/4)%4);wb(0xc0,sample&16?12:0);
  ww(0x8444,romWord(0x2dc14));ww(0x8446,romWord(0x2dc16));
  ww(0x84c4,romWord(0x2eb40));ww(0x84c6,romWord(0x2eb42));
  for(unsigned n=0;n<sample%8;n++)wb(0xf042+n*64,1);
  ww(0x8008,sample==255?65535:sample*257);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  writes=0;if(!PAN_INIT(&bus,expected+0x8402,expected+0x8482,second)||writes!=(region?256:1024))return 1;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*977);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,second?0x86b6:0x8542);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(1)&65535)!=3||(m68k_get_reg(2)&65535)!=16)return 1;
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Pan setup mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
#ifdef WILLOW_TEST_PAN_COPY
 if(copies!=1024)return 1;
#endif
 printf("Attract pan setup: %u source comparisons; camera/map positions, four chunks, message start and fade-in allocation match\n",checks);
 return 0;
}
