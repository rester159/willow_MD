/* Differential oracle: original ROM instructions execute in the installed
 * 68000 core. Only the host test uses emulation; the port routines are C. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
static uint16_t sourceWord(unsigned long a){if(a+1>=sizeof rom)abort();return willowMotionWord(rom+a);}
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
static uint32_t seed=0x57494c4c;
static uint32_t randomWord(void){seed=seed*1664525u+1013904223u;return seed;}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 /* Genesis wrapper only: source routines/data remain at original addresses.
  * Return enters an IRQ-masked idle loop; no display or input surface exists. */
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/motion-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/motion-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0;const unsigned addresses[]={0xc52a,0xc55a,0x208c,0x20b4,0xc72e};
 for(unsigned routine=0;routine<5;routine++)for(unsigned sample=0;sample<1024;sample++){
  uint8_t native[96];for(unsigned i=0;i<96;i++)native[i]=randomWord()>>24;
  uint8_t held=sample&255;
  if(routine<2)native[15]=sample/256;
  if(routine==2 || routine==3){native[15]=routine==3?(sample&255):(sample%4);willowMotionPutLong(native+24,0xc01c+(sample%2)*16);}
  if(routine==4){
   const uint16_t speeds[]={0,127,128,255,256,1663,1664,1665,1792,0x7fff,0xff7f,0xff80,0xffff};
   willowMotionPutWord(native+58,sample<13? speeds[sample] : randomWord());native[36]=(sample%4==3)?1:0;
  }
  for(unsigned i=0;i<96;i++)ram[(0x9846+i)^1]=native[i];
  ram[0x806a^1]=held;
  uint8_t ret[4];willowMotionPutLong(ret,0x100000);for(unsigned i=0;i<4;i++)ram[(0xff00+i)^1]=ret[i];
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff9846);m68k_set_reg(15,0xffff00);m68k_set_reg(16,addresses[routine]);
  if(routine==0)willowPlayerFacing(native,held);
  if(routine==1)willowPlayerCrouchFacing(native,held);
  if(routine==2 || routine==3)willowPlayerMotion(native,sourceWord,routine==3);
  if(routine==4)willowPlayerFall(native);
  retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Oracle failed to return: %x\n",m68k_get_reg(16));return 1;}
  for(unsigned i=0;i<96;i++)if(native[i]!=ram[(0x9846+i)^1]){fprintf(stderr,"Routine %x case %u byte %u native %02x source %02x\n",addresses[routine],sample,i,native[i],ram[(0x9846+i)^1]);return 1;}
  checks++;
 }
 printf("Player motion: %u cases, all 96 record bytes match original 68000 routines (headless)\n",checks);return 0;
}
