/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_services.h"
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
 FILE*f=fopen("build/hud_surface_probe.bin","rb");if(!f)return 2;
 if(!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
 f=fopen("build/hud_surface_probe.sym","r");if(!f)return 2;
 unsigned entry=0,address;char line[512],kind,name[256];
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3 && !strcmp(name,"hudProbe"))entry=address;
 fclose(f);if(!entry)return 2;
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/hud-surface-probe-rom.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/hud-surface-probe-rom.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;


 const unsigned rows[]={2,3,26,28,29,30},frames[]={1588,1590,1600};
 uint8_t source[16384],expected[7680];unsigned maximumRuns=0,deltaChecks=0;
 for(unsigned frame=0;frame<3;frame++)for(unsigned flips=0;flips<2;flips++){
  char path[128];snprintf(path,sizeof path,"build/arcade-trace/actor-native/text-%u.bin",frames[frame]);
  f=fopen(path,"rb");if(!f || fread(source,1,sizeof source,f)!=sizeof source)return 2;fclose(f);
  snprintf(path,sizeof path,"build/hud-surface-%u-%u.bin",frames[frame],flips);
  f=fopen(path,"rb");if(!f || fread(expected,1,sizeof expected,f)!=sizeof expected)return 2;fclose(f);
  memset(ram,0,65536);
  for(unsigned row=0;row<6;row++)for(unsigned col=0;col<48;col++)for(unsigned w=0;w<2;w++){
   uint16_t value=willowMotionWord(source+col*128+rows[row]*4+w*2);
   if(w && flips)value^=((col&1)?32:0)|((col&2)?64:0);
   putRamWord(ram,0x2000+(row*48+col)*4+w*2,value);
  }
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  unsigned runs=0;do{retro_run();runs++;}while(m68k_get_reg(16)!=0x100000 && runs<20);
  if(ramWord(ram,0x1000)!=0x4844 || m68k_get_reg(16)!=0x100000){fprintf(stderr,"Target HUD failed PC %x result %x\n",m68k_get_reg(16),ramWord(ram,0x1000));return 1;}
  for(unsigned n=0;n<sizeof expected;n++)if(ram[(0x3000+n)^1]!=expected[n]){fprintf(stderr,"Target HUD pixel byte mismatch frame %u flips %u byte %u\n",frames[frame],flips,n);return 1;}
  if(runs>maximumRuns)maximumRuns=runs;
  for(unsigned row=0;row<6;row++){
   putRamWord(ram,0x1002,1);putRamWord(ram,0x1004,row);
   putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);retro_run();
   if(m68k_get_reg(16)!=0x100000 || ramWord(ram,0x1000)!=0x4844){fputs("Four-tile HUD update exceeded one emulated frame\n",stderr);return 1;}
   for(unsigned n=0;n<128;n++)if(ram[(0x3000+n)^1]!=expected[row*1280+n])return 1;
   deltaChecks++;
  }
  printf("68000 HUD frame %u flips %u: 240 composed tiles match in %u emulated frame interval(s)\n",frames[frame],flips,runs);
 }
 printf("Target HUD: six full compositions, 46080 packed bytes match; worst %u frame intervals, VDP upload excluded\n",maximumRuns);
 printf("Target HUD: %u four-tile updates fit one frame interval and match host pixels; upload/game workload excluded\n",deltaChecks);
 return 0;
}
