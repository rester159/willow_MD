/* Differential verification of boot state handlers against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_boot_state.h"
#include "../src/willow_arcade_boot_options.h"
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
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}

int main(int argc,char**argv){
 if(argc!=2&&argc!=3)return 2;
 unsigned real=argc==3&&!strcmp(argv[2],"--real-options");
 if(argc==3&&!real)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Stop at external services without substituting their behavior. Command
  * enqueue and fixed-task starts execute their real source instructions. */
 const unsigned stops[]={0x1ac2,0x1b46,0x1b96,0x1e72,0x1e88,0x1636,0x7fea};
 for(unsigned n=real?3:0;n<sizeof stops/sizeof stops[0];n++)willowMotionPutWord(rom+stops[n],0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/boot-state-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/boot-state-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0;
 const uint16_t entries[]={0x8000,0x8010,0x803c,0x8030,0x8064,0x8074,0x8014,0x8018,0x801c};
 const uint16_t counts[]={0,1,2,255,256,65535};
 const uint8_t active[]={0,1,2,4,8,12,128,255};
 for(unsigned kind=0;kind<9;kind++)for(unsigned sample=0;sample<2048;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  ww(0x8000,kind<6?kind*2:(uint16_t)(sample*977));
  ww(0x8002,counts[sample%6]);ww(0x801a,(sample&63)*4);
  wb(0x82a6,active[sample&7]);wb(0x8058,(sample&1)?0x40:0);
  wb(0x828f,(sample&2)?128:0);
  wb(0x8066,sample);wb(0x8067,sample*73);wb(0x8068,sample*151);
  wb(0x20,active[(sample>>2)&7]);wb(0x80,active[(sample>>5)&7]);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  /* Handlers entered through 7FDE have its original nested return address;
   * individual continuation entries receive a caller return sentinel. */
  ram[0xf000^1]=0;ram[0xf001^1]=0x10;ram[0xf002^1]=0;ram[0xf003^1]=0;
  uint16_t d1=sample*977,d2=sample*317;
  WillowBootStep step=willowArcadeBootState(&bus,entries[kind],d1,d2);
  if(!step.supported)return 1;
  while(real&&willowArcadeBootOptions(&bus,step.service,romWord)){
   if(!step.resume)return 1;
   step=willowArcadeBootState(&bus,step.resume,d1,d2);
   if(!step.supported)return 1;
  }
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xfff000);
  m68k_set_reg(1,0xabcd0000UL|d1);m68k_set_reg(2,0x98760000UL|d2);
  m68k_set_reg(16,kind<6?0x7fde:entries[kind]);retro_run();
  unsigned stop=step.service?step.service:kind<6?0x7fea:0x100000;
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boot stop kind %u sample %u: %x expected %x\n",kind,sample,m68k_get_reg(16),stop);return 1;}
  if(step.resume){
   unsigned sp=m68k_get_reg(15)&65535;
   unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
   if(resume!=step.resume){fprintf(stderr,"Boot resume %x expected %x\n",resume,step.resume);return 1;}
  }
  for(unsigned n=0;n<65536;n++)if(!(n>=0xefc0&&n<0xf004)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Boot mismatch kind %u sample %u RAM %04x: %02x expected %02x\n",kind,sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 printf("Source boot state (%s): %u boundaries match all non-stack RAM; six original table dispatches, service order/continuations, queue wrap, countdown wrap and conditional task handoff\n",real?"real option services":"service boundaries",checks);
 return 0;
}
