/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_event_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
static uint16_t sourceWord(uint32_t address){return willowMotionWord(rom+address);}
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
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Actual trap-7 allocation. Stop only at the task's yield boundary. */
 willowMotionPutLong(rom+0x9c,0x160e);
 willowMotionPutWord(rom+0x1784,0x4ef9);
 willowMotionPutLong(rom+0x1786,0x100000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/event-command-consumer-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/event-command-consumer-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,consumedCount=0;
 for(unsigned family=0;family<8;family++)for(unsigned offset=0;offset<256;offset+=4)for(unsigned sample=0;sample<32;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  ww(0x801c,offset);
  ww(0x8072+offset,(family*4<<8)|(sample*7&255)|((sample&3)==0?0x8000:0));
  ww(0x8074+offset,sample*977);
  ww(0x0208,0x00ff);ww(0x020a,0x3000+(sample%9)*4);ww(0x020c,8-sample%9);
  wb(0x020e,(sample&7)==1?128:0);
  wb(0xf99c,(sample&7)==2?255:0);wb(0x828f,(sample&4)?128:0);wb(0x8068,sample);
  for(unsigned n=0;n<8;n++){ww(0x3000+n*4,0x00ff);ww(0x3002+n*4,0x100+n*32);}
  const uint8_t states[]={0,1,4,8,12,255,2,128};
  for(unsigned n=0;n<16;n++){wb(n*32,states[(sample+n)%8]);wb(n*32+1,(sample+n)%3);}
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  unsigned consumed=willowArcadeCommandConsumerResume(&bus,sourceWord);
  unsigned wanted=(sample&3)!=0&&(sample%9)!=8&&(sample&7)!=1;
  if(consumed!=wanted)return 1;
  consumedCount+=consumed;
  RUN(0x1e18,0xdeadbeef);
  if((m68k_get_reg(15)&65535)!=0xfefc)return 1;
  if(ram[0xfefe^1]!=0x1e||ram[0xfeff^1]!=0x18)return 1;
  for(unsigned n=0;n<65536;n++)if(n<0xfed0||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Consumer mismatch family %u offset %u sample %u address %04x got %02x want %02x\n",family,offset,sample,n,ram[n^1],expected[n]);return 1;}
  }
  checks++;
 }
 printf("Source command consumer: %u resumes, %u allocations; all eight families, ring offsets, inhibit/exhaustion/negative gates, wake scan and non-stack RAM match\n",checks,consumedCount);
 return 0;
}
