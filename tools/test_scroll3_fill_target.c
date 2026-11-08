/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_scroll3_map_store.h"
static uint16_t probeStore[WILLOW_SCROLL3_MAP_WORDS];
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

int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 f=fopen("build/scroll3_fill_probe.bin","rb");if(!f)return 2;
 if(!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
 f=fopen("build/scroll3_fill_probe.sym","r");if(!f)return 2;
 unsigned entry=0,address;char line[512],kind,name[256];
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3&&!strcmp(name,"scroll3FillProbe"))entry=address;
 fclose(f);if(!entry)return 2;
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100000,0x60fe);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scroll3-fill-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scroll3-fill-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 unsigned maximumSlices=0;
 for(unsigned sample=0;sample<2049;sample++){
  memset(ram,0xa5,65536);
  for(unsigned n=0;n<WILLOW_SCROLL3_MAP_WORDS;n++){
   probeStore[n]=sample*31+n*7;
   ram[(0x2002+n*2)^1]=probeStore[n]>>8;ram[(0x2003+n*2)^1]=probeStore[n];
  }
  unsigned probeStatus=willowScroll3MapFill(probeStore,sample)?0x544b:1;
  ram[0x1000^1]=sample>>8;ram[0x1001^1]=sample;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  unsigned slices=0;
  do{retro_run();slices++;}while(m68k_get_reg(16)!=0x100000&&slices<20);
  unsigned status=((unsigned)ram[0x1002^1]<<8)|ram[0x1003^1];
  if(m68k_get_reg(16)!=0x100000||status!=probeStatus){fprintf(stderr,"Target task store failed PC %x status %x\n",m68k_get_reg(16),status);return 1;}
  for(unsigned n=0;n<WILLOW_SCROLL3_MAP_WORDS;n++)if((((unsigned)ram[(0x2002+n*2)^1]<<8)|ram[(0x2003+n*2)^1])!=probeStore[n])return 1;
  for(unsigned n=0;n<65536;n++)if(n!=0x1000&&n!=0x1001&&n!=0x1002&&n!=0x1003&&
   !(n>=0x2002&&n<0x2002+sizeof probeStore)&&n<0xfe00){if(ram[n^1]!=0xa5)return 1;}
  if(slices>maximumSlices)maximumSlices=slices;
 }
 printf("68000 packed map fill: all 2048 IDs and invalid-ID rejection, all 5632 bytes and RAM guards match at a two-byte-aligned destination; maximum %u slices\n",maximumSlices);
 return 0;
}
