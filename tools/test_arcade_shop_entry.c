/* Full original shop-entry execution across intercepted 1750 waits. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_arcade_shop_resume.h"
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
static void color(uint32_t a,uint16_t v){if(a<0x91c000||a>=0x91d800)abort();ww(0x2000+a-0x91c000,v);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 if(willowMotionLong(rom+0xb38)!=0x91c800||willowMotionLong(rom+0xb4a)!=0x91cc00)return 2;
 willowMotionPutLong(rom+0xb38,0xff2800);willowMotionPutLong(rom+0xb4a,0xff2c00);
 if(willowMotionWord(rom+0x1ea0)!=0x1750)return 2;
 willowMotionPutWord(rom+0x1ea0,0x300);
 willowMotionPutWord(rom+0x300,0x4ef9);willowMotionPutLong(rom+0x302,0x100000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/shop-entry-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/shop-entry-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0;
 for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  ww(0x800c,0);ww(0x8010,0);wb(0x8352,sample);wb(0x8357,255-sample);
  ww(0x801a,(sample*4)&252);ww(0x801e,(sample*2)&126);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(4,0x12345678);m68k_set_reg(12,0x345678);m68k_set_reg(16,0x33bd6);
  uint16_t d4=0x5678,wait=0;uint32_t a4=0x345678;
  WillowPaletteAnimation palette={0};palette.write=color;
  WillowArcadeGameplayServices service={.bus=&bus,.busWord=word,.palette=&palette,.d4=&d4,.a4=&a4,.eventWait=&wait};
  WillowArcadeShopTask task={0};unsigned delays=1+sample%4;
  for(unsigned step=0;step<=delays;step++){
   if(step==delays){wb(0xf891,0);ram[0xf891^1]=0;}
   uint8_t result=willowArcadeShopResume(&task,&bus,word,willowArcadeGameplayServiceResume,&service);
   retro_run();if(m68k_get_reg(16)!=0x100000)return 1;
   if(result!=(step==delays?1:2)){fprintf(stderr,"Entry result sample %u step %u got %u\n",sample,step,result);return 1;}
   for(unsigned n=0;n<65536;n++)if(n<0xfe80||n>=0xff04){
    if(ram[n^1]!=expected[n]){fprintf(stderr,"Entry sample %u step %u mismatch %04x actual %02x expected %02x\n",sample,step,n,ram[n^1],expected[n]);return 1;}
   }
   if((uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=a4)return 1;
   if(step<delays){
    unsigned sp=m68k_get_reg(15);if(willowMotionWord(expected+0x8010)!=2||task.pc!=0x3c5e||wait!=0x1ea2)return 1;
    uint32_t pc=0;for(unsigned n=0;n<4;n++)pc=(pc<<8)|ram[((sp+n)&65535)^1];
    if(pc!=0x1ea2)return 1;m68k_set_reg(15,sp+4);m68k_set_reg(16,pc);
   }else if(task.pc||wait||m68k_get_reg(15)!=0xffff04)return 1;
   checks++;
  }
 }
 printf("Resumable shop entry: %u original-CPU boundaries; variable waits, no replayed setup, exact sound queue/palette/RAM/D4/A4 match\n",checks);
 return 0;
}
