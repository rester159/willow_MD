/* Original B4C8 and B858 display setup before their fade-in calls. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_ending_map.h"
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
static uint8_t expected[65536];
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static uint16_t word(uint32_t a){return willowMotionWord(rom+a);}
static uint8_t graphics[65536];static unsigned textWrites,nearWrites,farWrites;
static uint8_t byte(uint32_t a){return rom[a];}
static void textWrite(uint32_t a,uint16_t v){if(a<0x910000||a>=0x914000)abort();willowMotionPutWord(graphics+(a-0x910000),v);textWrites++;}
static void write2(uint16_t a,uint16_t v){if(a>=0x4000)abort();willowMotionPutWord(graphics+0x4000+a,v);nearWrites++;}
static void write3(uint16_t a,uint16_t v){if(a>=0x4000)abort();willowMotionPutWord(graphics+0x8000+a,v);farWrites++;}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 const unsigned operands[]={0x546a,0x4365a,0x43d50};
 const unsigned values[]={0x910000,0x918000,0x914000};
 for(unsigned n=0;n<3;n++){
  if(willowMotionLong(rom+operands[n])!=values[n])return 2;
  willowMotionPutLong(rom+operands[n],0x200000+n*0x4000);
 }
 const unsigned stops[]={0xb562,0xb898};
 for(unsigned n=0;n<2;n++){
  if(willowMotionLong(rom+stops[n])!=0x4ef81eaa)return 2;
  willowMotionPutWord(rom+stops[n],0x4ef9);willowMotionPutLong(rom+stops[n]+2,0x100000);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/ending-scene-setup-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/ending-scene-setup-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
 if(!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<sizeof graphics)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at)); \
 for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Map PC %x\n",m68k_get_reg(16));return 1;} \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0;
 for(unsigned sample=0;sample<512;sample++){
  for(unsigned n=0;n<65536;n++){expected[n]=(sample*31+n*7)^0xa5;graphics[n]=(sample*17+n*3)^0x5a;}
  wb(0x84a3,(sample/2)%4);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];memcpy(sram,graphics,sizeof graphics);
  textWrites=nearWrites=farWrites=0;
  m68k_set_reg(4,0x12345678);m68k_set_reg(12,0x345678);
  if(sample&1){
   if(!willowArcadeEndingFinalMap(&bus,expected+0x8482,word,byte,write3))return 1;
   if(textWrites||nearWrites||farWrites!=256)return 1;
  }else{
   if(!willowArcadeEndingSceneSetup(&bus,expected+0x8402,expected+0x8482,word,byte,textWrite,write2,write3))return 1;
   if(textWrites!=4096||nearWrites!=1024||farWrites!=256)return 1;
  }
  RUN(sample&1?0xb858:0xb4c8,0xdeadbeef);
  if(m68k_get_reg(4)!=0x12345678||m68k_get_reg(12)!=0x345678)return fprintf(stderr,"Ending setup registers %u\n",sample),1;
  for(unsigned n=0;n<65536;n++)if(n<0xfe80||n>=0xff04){
   if(ram[n^1]!=expected[n])return fprintf(stderr,"Ending setup sample %u RAM %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]),1;
  }
  for(unsigned n=0;n<sizeof graphics;n++)if(sram[n]!=graphics[n])return fprintf(stderr,"Ending setup sample %u graphics %x actual %x expected %x\n",sample,n,sram[n],graphics[n]),1;
  checks++;
 }
 printf("Ending scene setup: %u original-CPU cases; text panel, near/far maps, all four retained far-map modes, non-stack RAM and D4/A4 match\n",checks);
 return 0;
}
