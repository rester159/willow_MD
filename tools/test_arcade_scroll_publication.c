/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
typedef uint32_t u32;typedef uint16_t u16;typedef int16_t s16;
#include "../src/willow_arcade_scroll_publication.h"
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
static unsigned ramWord(uint8_t*r,unsigned a){return (r[a^1]<<8)|r[(a+1)^1];}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 /* Relocate only the three CPS MMIO destinations to observable work RAM.
  * All arithmetic, loads and staged writes remain original instructions. */
 const unsigned instructions[3]={0x197e,0x1990,0x19a2};
 for(unsigned i=0;i<3;i++){
  unsigned at=instructions[i];
  if(willowMotionWord(rom+at)!=0x33ed || willowMotionLong(rom+at+4)!=0x80010c+4*i)return 2;
  willowMotionPutLong(rom+at+4,0xff2000+2*i);
 }
 willowMotionPutWord(rom+0x19b4,0x4e75); /* end of isolated X publication slice */
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scroll-publication-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scroll-publication-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 uint32_t rng=0x57494c4c;unsigned checks=0;
 const uint16_t edges[]={0,1,63,64,65,255,511,32767,32768,65535};
 for(unsigned sample=0;sample<4096;sample++){
  uint16_t staged[3],published[3],hardware[3];
  memset(ram,0,65536);
  for(unsigned i=0;i<3;i++){
   rng=rng*1664525u+1013904223u;staged[i]=rng>>16;
   rng=rng*1664525u+1013904223u;published[i]=sample<30?edges[(sample+i)%10]:rng>>16;
   putRamWord(ram,0xf442+4*i,staged[i]);putRamWord(ram,0x802e + 4*i,published[i]);
  }
  willowScrollPublishX(staged,published,hardware);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x197a);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Publication oracle PC %x\n",m68k_get_reg(16));return 1;}
  for(unsigned i=0;i<3;i++){
   if(ramWord(ram,0xf442+4*i)!=staged[i] || ramWord(ram,0x2000+2*i)!=hardware[i]){
    fprintf(stderr,"Scroll X publication mismatch sample %u layer %u\n",sample,i);return 1;
   }
   checks++;
  }
 }
 printf("Original 197A..19B2 horizontal publication: %u layer cases match old staged hardware output and next source-minus-64 word\n",checks);
 return 0;
}
