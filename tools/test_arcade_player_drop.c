/* Differential oracle: original ROM instructions execute in the installed
 * 68000 core. Only the host test uses emulation; the port routines are C. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_drop.h"
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
static uint8_t effect[64],effectCount,rngHigh;
static uint16_t rngState,sounds[8];static unsigned soundCount;
static unsigned availableEffects(void){return effectCount;}
static uint8_t *allocateEffect(void){if(!effectCount)return NULL;effectCount--;return effect;}
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
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/drop-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/drop-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0,transitions=0,effects=0;
 f=fopen("build/stage0-collision-window.bin","rb");if(!f || fread(map,1,sizeof map,f)!=sizeof map)return 2;fclose(f);
 for(unsigned n=0;n<sizeof map;n++)ram[(0x4000+n)^1]=map[n];
 uint8_t continuity[96]={0};
 for(unsigned sample=0;sample<32768+27;sample++){
  uint8_t native[96];for(unsigned n=0;n<96;n++)native[n]=randomWord()>>24;
  native[0]=1;native[2]=2;native[3]=6;native[4]=(sample%5)*2;
  native[15]=(sample/3)%4;native[48]=(sample/12)%4;native[32]=0;
  native[62]=native[4]==4?2:0;native[71]=0;
  const uint8_t charges[]={0,1,15,16,17,126,127,255};
  native[49]=charges[(sample/64)%8];native[63]=(sample/8192)%2;
  native[68]=(sample/1024)%11;
  willowMotionPutWord(native+6,32+(sample%20)*16+(sample&15));
  willowMotionPutWord(native+10,32+((sample/16)%12)*16+(sample&15));
  willowMotionPutLong(native+24,0xc01c);
  WillowPlayerCollision pose={sourceWord,attribute,0,0,0,0};
  willowPlayerDropPose(native,native[4]==8?0xd2ce:0xd2ca,&pose);
  const uint16_t speeds[]={0,511,512,1791,1792,1793,32767,32768,65535};
  willowMotionPutWord(native+56,speeds[(sample/8)%9]);
  willowMotionPutWord(native+52,sample&1?56:randomWord());
  native[34]=(sample/64)%9;
  if(sample&1)willowMotionPutWord(native+28,1);
  WillowPlayerInput inputs={(uint8_t)(sample&63),(uint8_t)((sample>>6)&63),(uint8_t)sample,
    (uint8_t)((sample/2048)%2),(uint8_t)(sample%5),(uint8_t)(sample%7),
    availableEffects,allocateEffect,nextRandom,sound};
  if(sample>=32768){
   inputs.held=1;inputs.pressed=0;inputs.alternate=0;
   if(sample==32768){
    memset(native,0,sizeof native);native[0]=1;native[2]=2;native[3]=6;
    native[15]=2;native[48]=2;
    willowMotionPutWord(native+6,192);willowMotionPutWord(native+10,72);
    willowMotionPutWord(native+52,56);willowMotionPutLong(native+24,0xc01c);
    willowPlayerDropPose(native,0xd2ca,&pose);
   }else memcpy(native,continuity,sizeof native);
  }
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
  effectCount=(sample/8)%4;rngState=randomWord();rngHigh=sample&255;soundCount=0;
  memset(sounds,0,sizeof sounds);
  for(unsigned n=0;n<64;n++){effect[n]=randomWord()>>24;ram[(0xa000+n)^1]=effect[n];}
  for(unsigned n=0;n<96;n++)ram[(0x9846+n)^1]=native[n];
  ram[0x806a^1]=inputs.held;ram[0x0e92^1]=inputs.pressed;ram[0x8019^1]=inputs.frame;
  ram[0x83dc^1]=inputs.alternate;ram[0x8376^1]=inputs.chargeLevel;ram[0x0e94^1]=inputs.chargedShot;
  ram[0x97fe^1]=effectCount;putRamWord(ram,0x9816,0x9100);putRamWord(ram,0x9100,0xa000);
  putRamWord(ram,0x8284,rngState);ram[0x8286^1]=rngHigh;
  ram[0x8290^1]=0;ram[0x8293^1]=0;putRamWord(ram,0x801e,0);
  for(unsigned n=0;n<128;n++)ram[(0x8172+n)^1]=0;
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff9846);m68k_set_reg(15,0xffff00);m68k_set_reg(0,0);m68k_set_reg(16,0xc8f2);
  unsigned initialEffects=effectCount;
  if(!willowPlayerDrop(native,&inputs,&collision))return 2;
  retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Oracle case %u failed to return: %x\n",sample,m68k_get_reg(16));return 1;}
  for(unsigned n=0;n<96;n++)if(native[n]!=ram[(0x9846+n)^1]){fprintf(stderr,"Ground case %u byte %u native %02x source %02x\n",sample,n,native[n],ram[(0x9846+n)^1]);return 1;}
  for(unsigned n=0;n<64;n++)if(effect[n]!=ram[(0xa000+n)^1]){fprintf(stderr,"Effect case %u byte %u\n",sample,n);return 1;}
  if(effectCount!=ram[0x97fe^1] || ramWord(ram,0x9816)!=0x9100+(initialEffects-effectCount)*2 || rngState!=ramWord(ram,0x8284) || rngHigh!=ram[0x8286^1] || inputs.chargedShot!=ram[0x0e94^1]){fprintf(stderr,"Global side effect mismatch case %u\n",sample);return 1;}
  if(ramWord(ram,0x801e)!=soundCount*2){fprintf(stderr,"Sound cursor case %u native %u source %u\n",sample,soundCount*2,ramWord(ram,0x801e));return 1;}
  for(unsigned n=0;n<64;n++)if(ramWord(ram,0x8172+n*2)!=(n<soundCount?sounds[n]:0)){fprintf(stderr,"Sound value case %u offset %u\n",sample,n);return 1;}

  if(native[3]==2 && native[4]==4)transitions++;
  effects+=initialEffects-effectCount;checks++;
  if(sample>=32768)memcpy(continuity,native,sizeof native);
 }
 if(!transitions || !effects)return 1;
 if(continuity[3]!=2 || continuity[4]!=4 || willowMotionLong(continuity+6)!=0x00d20000 || willowMotionLong(continuity+10)!=0x0031e000){fputs("27-update sequence endpoint mismatch\n",stderr);return 1;}
 puts("Complete 27-update source sequence: x=210, y=49.875, descending state 2/4");
 printf("Player substate 6 (C8F2): %u cases match all 96 player bytes, effect allocation, RNG and preserved charge/sound globals\n",checks);
 printf("Transitions to descending substate 2: %u; effect allocations: %u\n",transitions,effects);return 0;
}
