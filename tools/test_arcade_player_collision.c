/* Differential oracle: original ROM instructions execute in the installed
 * 68000 core. Only the host test uses emulation; the port routines are C. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_collision.h"
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
static uint8_t map[16384];
static uint8_t attribute(uint16_t offset){return map[offset+2];}
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
 /* Test-only relocation of the CPS tile RAM base into Genesis work RAM.
  * Verify the exact immediate before replacing it; no branch/logic changes. */
 if(willowMotionLong(rom+0x3e44)!=0x918000)return 2;
 willowMotionPutLong(rom+0x3e44,0xff4000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/collision-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/collision-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0,contacts[11]={0},misses[11]={0};
 uint8_t forest[16384];f=fopen("build/stage0-collision-window.bin","rb");if(!f || fread(forest,1,sizeof forest,f)!=sizeof forest)return 2;fclose(f);
 const unsigned addresses[]={0x3e4c,0x3f62,0x38f6,0x39fe,0x4306,0x43bc,0x3772,0x3814,0x38f6,0x39fe,0x3772};
 for(unsigned routine=0;routine<11;routine++)for(unsigned sample=0;sample<((routine<2 || routine==4 || routine==5)?47*256:4096);sample++){
  uint8_t native[96];for(unsigned i=0;i<96;i++)native[i]=randomWord()>>24;
  native[15]=sample%4;native[32]=(sample/4)%2;
  native[71]=((sample/8)%3)*2;native[62]=((sample/8)%2)*2;
  int16_t yOffset=-16;
  uint16_t x=randomWord(),y=randomWord();
  if(routine<2 || routine==4 || routine==5){x=(x&0xfff0)|(sample&15);y=(y&0xfff0)|((sample>>4)&15);}
  for(unsigned i=0;i<sizeof map;i+=4){
   map[i]=randomWord()>>24;map[i+1]=randomWord()>>24;
   map[i+2]=((routine<2 || routine==4 || routine==5)?(sample/256):((randomWord()>>16)%47))*4;
   map[i+3]=randomWord()>>24;
  }
  if(routine>=8){
   memcpy(map,forest,sizeof map);
   willowMotionPutWord(native+6,32+(sample%20)*16+(sample&15));
   willowMotionPutWord(native+10,32+((sample/16)%12)*16+(sample&15));
   native[71]=0;native[62]=0;
  }
  for(unsigned i=0;i<sizeof map;i++)ram[(0x4000+i)^1]=map[i];
  for(unsigned i=0;i<96;i++)ram[(0x9846+i)^1]=native[i];
  WillowPlayerCollision collision={sourceWord,attribute,x,y,0,0};
  uint8_t xy[4];willowMotionPutWord(xy,x);willowMotionPutWord(xy+2,y);
  for(unsigned i=0;i<4;i++)ram[(0x0a32+i)^1]=xy[i];
  /* FloorPoint uses A2[-2] only for the secondary tile lookup. */
  ram[0x3000^1]=(uint16_t)yOffset>>8;ram[0x3001^1]=yOffset;
  uint8_t ret[4];willowMotionPutLong(ret,0x100000);for(unsigned i=0;i<4;i++)ram[(0xff00+i)^1]=ret[i];
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff9846);m68k_set_reg(15,0xffff00);
  m68k_set_reg(8,0xff4000+willowCollisionOffset(x,y));m68k_set_reg(10,0xff3002);m68k_set_reg(16,addresses[routine]);
  unsigned result=0;
  if(routine==0)result=willowPlayerFloorPoint(native,&collision,yOffset);
  if(routine==1)result=willowPlayerCeilingPoint(native,&collision);
  if(routine==2)result=willowPlayerFloor(native,&collision);
  if(routine==3)result=willowPlayerCeiling(native,&collision);
  if(routine==4 || routine==5)result=willowPlayerWallPoint(native,&collision,routine==5);
  if(routine==6 || routine==7)result=willowPlayerWalls(native,&collision,routine==7);
  if(routine==8)result=willowPlayerFloor(native,&collision);
  if(routine==9)result=willowPlayerCeiling(native,&collision);
  if(routine==10)result=willowPlayerWalls(native,&collision,0);
  retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Oracle failed to return: %x\n",m68k_get_reg(16));return 1;}
  for(unsigned i=0;i<96;i++)if(native[i]!=ram[(0x9846+i)^1]){fprintf(stderr,"Routine %x case %u byte %u native %02x source %02x\n",addresses[routine],sample,i,native[i],ram[(0x9846+i)^1]);return 1;}
  if(result!=(m68k_get_reg(0)&255)){fprintf(stderr,"Return mismatch %x case %u\n",addresses[routine],sample);return 1;}
  willowMotionPutWord(xy,collision.x);willowMotionPutWord(xy+2,collision.y);
  for(unsigned i=0;i<4;i++)if(xy[i]!=ram[(0x0a32+i)^1]){fprintf(stderr,"Sample coordinate mismatch %x case %u\n",addresses[routine],sample);return 1;}
  if(result)contacts[routine]++;else misses[routine]++;checks++;
 }
 for(unsigned i=0;i<11;i++){if(i<8 && (!contacts[i] || !misses[i]))return 1;printf("%04x: %u contacts, %u misses\n",addresses[i],contacts[i],misses[i]);}
 printf("Player collision: %u cases, 96 player bytes, sample coordinates and return value match source instructions (headless)\n",checks);return 0;
}
