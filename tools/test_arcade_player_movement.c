/* Differential oracle: original ROM instructions execute in the installed
 * 68000 core. Only the host test uses emulation; the port routines are C. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_movement.h"
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
static uint8_t records[3][9][64],available[3],cursor[3],rngHigh;
static uint16_t rngState,sounds[8];static unsigned soundCount;
static unsigned availableEffects(void){return available[0];}
static uint8_t *allocatePool(uint8_t type){unsigned p=type==2?0:type==8?1:2;if(!available[p])return NULL;available[p]--;return records[p][cursor[p]++];}
static uint8_t *allocateEffect(void){return allocatePool(2);}
static uint8_t *allocateClass12(void){return allocatePool(12);}
static uint8_t nextRandom(void){
 rngHigh=(rngState&0x202)>>8;
 unsigned bit=((rngState>>1)^(rngState>>9))&1;
 rngState=(rngState>>1)|(bit<<15);return rngState&255;
}
static void sound(uint16_t command){if(soundCount+2>8)abort();sounds[soundCount++]=command;sounds[soundCount++]=255;}
static void putRamWord(uint8_t*ram,unsigned at,uint16_t value){ram[at^1]=value>>8;ram[(at+1)^1]=value;}
static unsigned ramWord(uint8_t*ram,unsigned at){return (ram[at^1]<<8)|ram[(at+1)^1];}
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
 /* Scope oracle to the movement prefix: return at the first health call. */
 if(sourceWord(0xc158)!=0x6100)return 2;
 willowMotionPutWord(rom+0xc158,0x4e75);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/movement-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/movement-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0,spawns=0,types[13]={0},routeChecks[8]={0};
 const unsigned bases[]={0xa000,0xa400,0xac00},counts[]={0x97fe,0x9807,0x980d},stacks[]={0x9100,0x9200,0x9300},cursors[]={0x9816,0x9828,0x9834};
 f=fopen("build/stage0-collision-window.bin","rb");if(!f || fread(map,1,sizeof map,f)!=sizeof map)return 2;fclose(f);
 for(unsigned n=0;n<sizeof map;n++)ram[(0x4000+n)^1]=map[n];
 for(unsigned route=0;route<8;route++)for(unsigned sample=0;sample<8192;sample++){
  uint8_t native[96];for(unsigned n=0;n<96;n++)native[n]=randomWord()>>24;
  native[0]=1;native[2]=2;native[3]=route*2;native[4]=(sample%(route==3 || route==5?5:route==4?7:3))*2;native[5]=((sample/3)%3)*2;
  native[15]=(sample/3)%4;native[48]=(sample/12)%4;native[32]=0;
  if(route==5 && native[4]==4)native[15]&=1;
  native[34]=(sample/32)%9;willowMotionPutWord(native+74,(sample/8)%1800);
  native[62]=(sample/8)%2?2:0;native[71]=0;native[72]=(sample/16)%3;
  const uint8_t timers[]={0,1,2,4,5,255};native[76]=timers[(sample/16)%6];
  native[60]=(sample/128)%2;native[36]=(sample/32)%2;native[82]=(sample/128)%9;
  native[49]=(sample/64)&127;native[63]=(sample/4096)%2;native[68]=(sample/128)%11;
  willowMotionPutWord(native+6,32+(sample%20)*16+(sample&15));
  willowMotionPutWord(native+10,32+((sample/16)%12)*16+(sample&15));
  willowMotionPutWord(native+52,56);willowMotionPutWord(native+56,(sample/16)%1800);
  willowMotionPutLong(native+24,0xc01c);if(route==3 || route==4 || route==5){
   unsigned table=route==3?0xd2ca:route==4?0xd33a:0xd3be;
   willowPlayerAnimationSet(native,table+(int16_t)sourceWord(table+(native[15]&1)*2),sourceWord);
  }else willowPlayerAnimationSelect(native,route==0?0xd07e:0xd27a,sourceWord);
  if(sample&1)willowMotionPutWord(native+28,1);
  WillowPlayerInput inputs={(uint8_t)(sample&63),(uint8_t)((sample>>6)&63),(uint8_t)sample,
    (uint8_t)((sample/2048)%2),(uint8_t)(sample%5),(uint8_t)(sample%192),availableEffects,allocateEffect,nextRandom,sound};
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
  WillowPlayerShots shots={(uint8_t)((sample/256)%7),(uint8_t)(sample&15),(uint8_t)((sample/64)%6),(uint8_t)((sample/17)%2),(uint8_t)((sample/31)%2),allocatePool};
  unsigned initial[3];
  for(unsigned p=0;p<3;p++){
   initial[p]=available[p]=(sample>>(p*2))%(p==0?5:p==1?4:10);cursor[p]=0;
   ram[counts[p]^1]=available[p];putRamWord(ram,cursors[p],stacks[p]);
   for(unsigned n=0;n<9;n++){
    putRamWord(ram,stacks[p]+n*2,bases[p]+n*64);
    for(unsigned j=0;j<64;j++){records[p][n][j]=randomWord()>>24;ram[(bases[p]+n*64+j)^1]=records[p][n][j];}
   }
  }
  rngState=randomWord();rngHigh=sample&255;soundCount=0;memset(sounds,0,sizeof sounds);
  uint16_t threshold=randomWord(),cameraX=randomWord();uint8_t rebound=sample&1;
  if(route>=6){willowMotionPutWord(native+6,randomWord()>>8);willowMotionPutWord(native+10,randomWord()>>8);}
  putRamWord(ram,0x845c,threshold);putRamWord(ram,0x8408,cameraX);ram[0xf99e^1]=rebound;
  for(unsigned n=0;n<96;n++)ram[(0x9846+n)^1]=native[n];
  ram[0x806a^1]=inputs.held;ram[0x0e92^1]=inputs.pressed;ram[0x8019^1]=inputs.frame;
  ram[0x83dc^1]=inputs.alternate;ram[0x8376^1]=inputs.chargeLevel;ram[0x0e94^1]=inputs.chargedShot;
  ram[0x8366^1]=shots.level;ram[0x0e93^1]=shots.selected;ram[0xf99d^1]=shots.activeNormal;
  ram[0x0e58^1]=shots.blockA;ram[0x83cf^1]=shots.blockB;
  putRamWord(ram,0x8284,rngState);ram[0x8286^1]=rngHigh;putRamWord(ram,0xa32,0);putRamWord(ram,0xa34,0);
  ram[0x8290^1]=0;ram[0x8293^1]=0;putRamWord(ram,0x801e,0);
  for(unsigned n=0;n<128;n++)ram[(0x8172+n)^1]=0;
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff9846);m68k_set_reg(15,0xffff00);m68k_set_reg(0,0);m68k_set_reg(1,0);
  m68k_set_reg(16,route<6?0xc144:route==6?0xc33e:0xc392);
  int ok=1;
  if(route<6)ok=willowPlayerMovement(native,&inputs,&collision,&shots,allocateClass12);
  else if(route==6)willowPlayerLowerBound(native,threshold,rebound,&collision);
  else willowPlayerScreenBounds(native,cameraX);
  if(!ok){fprintf(stderr,"Unsupported route %u case %u level %u selected %u phase %u attackphase %u\n",route,sample,shots.level,shots.selected,native[4],native[5]);return 2;}
  retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Oracle route %u case %u failed to return: %x\n",route,sample,m68k_get_reg(16));return 1;}
  for(unsigned n=0;n<96;n++)if(native[n]!=ram[(0x9846+n)^1]){fprintf(stderr,"Route %u case %u player byte %u native %02x source %02x\n",route,sample,n,native[n],ram[(0x9846+n)^1]);return 1;}
  for(unsigned p=0;p<3;p++){
   if(available[p]!=ram[counts[p]^1] || ramWord(ram,cursors[p])!=stacks[p]+cursor[p]*2){fprintf(stderr,"Pool %u route %u case %u count/cursor mismatch\n",p,route,sample);return 1;}
   for(unsigned n=0;n<9;n++)for(unsigned j=0;j<64;j++)if(records[p][n][j]!=ram[(bases[p]+n*64+j)^1]){fprintf(stderr,"Pool %u record %u byte %u route %u case %u mismatch\n",p,n,j,route,sample);return 1;}
   spawns+=initial[p]-available[p];
  }
  if(rngState!=ramWord(ram,0x8284) || rngHigh!=ram[0x8286^1] || inputs.chargedShot!=ram[0x0e94^1] || shots.selected!=ram[0x0e93^1] || shots.activeNormal!=ram[0xf99d^1]){fprintf(stderr,"Global mismatch route %u case %u\n",route,sample);return 1;}
  if(ramWord(ram,0x801e)!=soundCount*2)return 1;
  for(unsigned n=0;n<64;n++)if(ramWord(ram,0x8172+n*2)!=(n<soundCount?sounds[n]:0))return 1;
  if(collision.x!=ramWord(ram,0xa32) || collision.y!=ramWord(ram,0xa34)){fprintf(stderr,"Collision coordinates mismatch route %u case %u\n",route,sample);return 1;}
  if(shots.selected<13)types[shots.selected]++;routeChecks[route]++;checks++;
 }
 printf("Movement dispatcher plus lower/screen bounds: %u cases, %u allocations match original instructions\n",checks,spawns);
 for(unsigned n=0;n<8;n++)printf("Route %u: %u cases\n",n,routeChecks[n]);return 0;
}
