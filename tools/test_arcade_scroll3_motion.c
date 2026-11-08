/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_scroll3_motion.h"
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
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scroll3-motion-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scroll3-motion-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)

 unsigned checks=0;uint32_t random=0x57494c4c;
 for(unsigned axis=0;axis<2;axis++)for(unsigned sample=0;sample<65536;sample++){
  uint8_t record[128];for(unsigned n=0;n<128;n++){random=random*1664525u+1013904223u;record[n]=random>>24;}
  const uint8_t shifts[]={0,1,3,15,16,31,32,63,64,65,127,255};
  record[52]=shifts[sample%12];
  const uint16_t edges[]={0,1,31,32,63,64,32767,32768,32769,65534,65535};
  unsigned coordinate=axis?10:6,target=axis?108:106;
  if(sample&1)willowMotionPutWord(record+coordinate,edges[(sample/12)%11]);
  if(sample&2)willowMotionPutWord(record+target,edges[(sample/132)%11]);
  uint16_t stage=sample%7,phase=(sample/7)%8;
  if((sample&15)==0){stage=5;phase=6;}
  record[114]=sample&255;record[115]=(sample/256)&255;
  for(unsigned n=0;n<128;n++)ram[(0x8482+n)^1]=record[n];
  putRamWord(ram,0x8344,stage);putRamWord(ram,0x8374,phase);
  if(axis)willowScroll3Vertical(record);else willowScroll3Horizontal(record,stage,phase);
  m68k_set_reg(1,0xabcde000);m68k_set_reg(14,0xff8482);RUN(axis?0x43174:0x4310c,0);
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x8482+n)^1]){
   fprintf(stderr,"Scroll3 axis %u case %u byte %u native %02x source %02x\n",axis,sample,n,record[n],ram[(0x8482+n)^1]);return 1;
  }
  checks++;
 }
 printf("Scroll3 horizontal/vertical motion: %u cases match all 128 original controller bytes, including stage-5 oscillation\n",checks);
 return 0;
}
