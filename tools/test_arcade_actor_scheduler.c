/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],records[60*64];
static uint16_t visits[180];static unsigned visitWords;
static void visit(uint8_t*r,uint16_t remaining)
{
 unsigned index=(unsigned)(r-records)/64,selector=r[32];
 visits[visitWords++]=0xccc6+index*64;visits[visitWords++]=remaining;visits[visitWords++]=selector;
 if(selector==1)records[59*64]=1;
 if(selector==2)records[59*64]=0;
 if(selector==3)records[0]=1;
}

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
static unsigned ramWord(uint8_t*r,unsigned a){return (r[a^1]<<8)|r[(a+1)^1];}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Unchanged scheduler loop relocated with its table. Callback slots are
  * instrumented observers, not substitutes claimed as actor behavior. */
 memcpy(rom+0x100100,rom+0x2f39e,0x26);
 for(unsigned selector=0;selector<4;selector++){
  unsigned at=0x100200+selector*32;
  willowMotionPutLong(rom+0x100126+selector*4,at);
  willowMotionPutWord(rom+at,0x32ce); /* move.w a6,(a1)+ */
  willowMotionPutWord(rom+at+2,0x32c7); /* move.w d7,(a1)+ */
  willowMotionPutWord(rom+at+4,0x32fc);willowMotionPutWord(rom+at+6,selector);
  if(selector){
   willowMotionPutWord(rom+at+8,0x13fc); /* move.b #value,absolute-long */
   willowMotionPutWord(rom+at+10,selector==2?0:1);
   willowMotionPutLong(rom+at+12,selector==3?0xffccc6:0xffdb86);
   willowMotionPutWord(rom+at+16,0x4e75);
  }else willowMotionPutWord(rom+at+8,0x4e75);
 }

 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor-scheduler-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor-scheduler-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 WillowActorPoolIndex indices[60];unsigned checks=0;
 for(unsigned sample=0;sample<1024;sample++){
  memset(ram,0,65536);visitWords=0;
  WillowActorPool pool;willowActorPoolInit(&pool,records,indices,60,64,12);
  for(unsigned n=0;n<60;n++){
   records[n*64]=(sample+n*37)%7==0?0:(sample>>(n%10))&3;
   records[n*64+32]=(sample+n)%4;
   for(unsigned j=0;j<64;j++)ram[(0xccc6+n*64+j)^1]=records[n*64+j];
  }
  willowActorPoolSchedule(&pool,visit);
  m68k_set_reg(9,0xff4000);RUN(0x100100,0);
  if(m68k_get_reg(9)!=0xff4000+visitWords*2 || (m68k_get_reg(7)&65535)!=65535 || (m68k_get_reg(14)&65535)!=0xdbc6)return 1;
  for(unsigned n=0;n<visitWords;n++)if(ramWord(ram,0x4000+n*2)!=visits[n]){fprintf(stderr,"Scheduler visit mismatch case %u word %u\n",sample,n);return 1;}
  for(unsigned n=0;n<sizeof records;n++)if(ram[(0xccc6+n)^1]!=records[n])return 1;
  checks++;
 }
 printf("Original class-12 scheduler loop: %u activity/dispatch/order/counter cases, including mutations of future and past slots\n",checks);
 return 0;
}
