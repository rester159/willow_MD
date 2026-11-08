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
 if(argc!=2 && (argc!=3 || strcmp(argv[2],"--native-rng-fixtures")))return 2;
 FILE*fixtures=argc==3?fopen("build/player-rng-original.bin","wb"):NULL;
 if(argc==3&&!fixtures)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/services-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/services-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 for(unsigned seed=0;seed<65536;seed++){
  WillowArcadeRandom rng={(uint16_t)seed,0xff};
  putRamWord(ram,0x8284,seed);ram[0x8286^1]=0xff;
  uint8_t result=willowArcadeRandomNext(&rng);
  RUN(0x1d8e,0xdeadbeef);
  if(rng.state!=ramWord(ram,0x8284) || rng.scratch!=ram[0x8286^1] || result!=m68k_get_reg(0)){
   fprintf(stderr,"RNG mismatch seed %u\n",seed);return 1;
  }
  if(fixtures){
   uint8_t data[4]={ram[0x8284^1],ram[0x8285^1],ram[0x8286^1],m68k_get_reg(0)};
   if(fwrite(data,1,4,fixtures)!=4)return 2;
  }
 }
 if(fixtures&&fclose(fixtures))return 2;
 unsigned checks=0;
 const unsigned entries[]={0x7818,0x7824,0x783c,0x7844};
 for(unsigned entry=0;entry<4;entry++)for(unsigned gates=0;gates<4;gates++)for(unsigned cursor=0;cursor<128;cursor+=2){
  WillowArcadeSoundQueue q={0};q.writeOffset=cursor;q.readOffset=(cursor+12)&126;
  q.gateA=(gates&1)?0x80:0;q.gateB=(gates&2)?0xff:0;
  putRamWord(ram,0x801e,q.writeOffset);putRamWord(ram,0x8020,q.readOffset);
  ram[0x8290^1]=q.gateA;ram[0x8293^1]=q.gateB;
  for(unsigned n=0;n<64;n++){q.words[n]=(n*717+cursor)^0x1357;putRamWord(ram,0x8172+n*2,q.words[n]);}
  uint16_t command=0x8000+entry*256+gates*128+cursor;
  if(entry<2)willowArcadeSoundWord(&q,command,entry&1);
  else willowArcadeSoundCommand(&q,command,entry&1);
  RUN(entries[entry],command);
  if(q.writeOffset!=ramWord(ram,0x801e) || q.readOffset!=ramWord(ram,0x8020))return 1;
  for(unsigned n=0;n<64;n++)if(q.words[n]!=ramWord(ram,0x8172+n*2)){
   fprintf(stderr,"Sound mismatch entry %u gates %u cursor %u word %u\n",entry,gates,cursor,n);return 1;
  }
  checks++;
 }
 printf("Shared services: all 65536 RNG seeds and %u sound-ring cases match original instructions\n",checks);
 return 0;
}
