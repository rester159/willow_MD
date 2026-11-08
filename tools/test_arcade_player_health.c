/* Differential oracle: original ROM instructions execute in the installed
 * 68000 core. Only the host test uses emulation; the port routines are C. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_health.h"
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
static uint16_t colors[32],sounds[8];static unsigned soundCount;
static uint8_t textRam[16384];
static uint16_t paletteRead(unsigned n){return colors[n];}
static void paletteWrite(unsigned n,uint16_t value){colors[n]=value;}
static void textWrite(uint32_t at,uint16_t value){if(at<0x910000 || at+1>=0x914000)abort();willowMotionPutWord(textRam+at-0x910000,value);}
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
 const unsigned patchAt[]={0xc242,0x4a36,0x4a40,0x4d6c,0x4da8};
 const unsigned original[]={0x91c000,0x91030c,0x91030c,0x910df0,0x910df0};
 const unsigned relocated[]={0xff2000,0xffa30c,0xffa30c,0xffadf0,0xffadf0};
 for(unsigned n=0;n<5;n++){if(willowMotionLong(rom+patchAt[n])!=original[n])return 2;willowMotionPutLong(rom+patchAt[n],relocated[n]);}
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/health-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/health-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0,hits=0,deaths=0,soundCases=0;
 for(unsigned sample=0;sample<8192;sample++){
  uint8_t native[96];for(unsigned n=0;n<96;n++)native[n]=randomWord()>>24;
  native[0]=1;native[2]=2;native[3]=0;native[4]=2;native[5]=0;
  native[15]=sample%4;native[32]=(sample/4)%2;native[62]=(sample/8)%2?2:0;
  native[78]=(sample/16)%6;native[79]=sample/96;
  const uint16_t timers[]={0,0,0,1,2,128,65535};
  willowMotionPutWord(native+80,timers[(sample/192)%7]);
  willowMotionPutWord(native+54,timers[(sample/32)%7]);
  WillowPlayerHealth health={0};
  health.hp=(sample/8)%9;health.maxHp=8;health.armor=(sample/72)%4;
  health.cachedHp=(health.hp+(sample&1))%9;
  health.cachedArmor=(health.armor+((sample/2)&1))%4;
  if(!health.hp && !health.armor)health.cachedArmor=0;
  health.frame=sample;health.rebound=(sample/256)%2;
  for(unsigned n=0;n<12;n++)health.items[n]=(sample>>(n%8))%4;
  health.paletteRead=paletteRead;health.paletteWrite=paletteWrite;health.textWrite=textWrite;health.sound=sound;
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
  unsigned type=(sample/128)%3==0?39:(sample/128)%3==1?32:0;
  for(unsigned n=0;n<sizeof map;n+=4){map[n+2]=type*4;ram[(0x4002+n)^1]=map[n+2];}
  for(unsigned n=0;n<sizeof textRam;n++){textRam[n]=randomWord()>>24;ram[(0xa000+n)^1]=textRam[n];}
  for(unsigned n=0;n<32;n++){colors[n]=randomWord()>>8;putRamWord(ram,0x2000+n*2,colors[n]);}
  for(unsigned n=0;n<96;n++)ram[(0x9846+n)^1]=native[n];
  for(unsigned n=0;n<12;n++)ram[(0x8352+n)^1]=health.items[n];
  ram[0x8362^1]=health.hp;ram[0x8363^1]=health.maxHp;ram[0x8364^1]=health.armor;
  ram[0x8365^1]=health.cachedHp;ram[0x8389^1]=health.cachedArmor;ram[0x8019^1]=health.frame;ram[0xf99e^1]=health.rebound;
  putRamWord(ram,0xa32,0);putRamWord(ram,0xa34,0);soundCount=0;memset(sounds,0,sizeof sounds);
  ram[0x8290^1]=0;ram[0x8293^1]=0;putRamWord(ram,0x801e,0);
  for(unsigned n=0;n<128;n++)ram[(0x8172+n)^1]=0;
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  for(unsigned n=0;n<8;n++)m68k_set_reg(n,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff9846);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0xc170);
  willowPlayerHealthStep(native,&health,&collision);
  retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Health oracle case %u failed to return: %x\n",sample,m68k_get_reg(16));return 1;}
  for(unsigned n=0;n<96;n++)if(native[n]!=ram[(0x9846+n)^1]){fprintf(stderr,"Health case %u player byte %u native %02x source %02x\n",sample,n,native[n],ram[(0x9846+n)^1]);return 1;}
  for(unsigned n=0;n<12;n++)if(health.items[n]!=ram[(0x8352+n)^1])return 1;
  if(health.hp!=ram[0x8362^1] || health.armor!=ram[0x8364^1] || health.cachedHp!=ram[0x8365^1] || health.cachedArmor!=ram[0x8389^1]){fprintf(stderr,"Health globals case %u\n",sample);return 1;}
  for(unsigned n=0;n<32;n++)if(colors[n]!=ramWord(ram,0x2000+n*2)){fprintf(stderr,"Palette case %u color %u native %04x source %04x\n",sample,n,colors[n],ramWord(ram,0x2000+n*2));return 1;}
  for(unsigned n=0;n<sizeof textRam;n++)if(textRam[n]!=ram[(0xa000+n)^1]){fprintf(stderr,"HUD case %u byte %u native %02x source %02x\n",sample,n,textRam[n],ram[(0xa000+n)^1]);return 1;}
  if(ramWord(ram,0x801e)!=soundCount*2)return 1;
  for(unsigned n=0;n<64;n++)if(ramWord(ram,0x8172+n*2)!=(n<soundCount?sounds[n]:0))return 1;
  if(collision.x!=ramWord(ram,0xa32) || collision.y!=ramWord(ram,0xa34)){fprintf(stderr,"Collision coordinates case %u\n",sample);return 1;}
  hits+=native[0]==2;deaths+=native[2]==4;soundCases+=!!soundCount;checks++;
 }
 if(!hits || !deaths || !soundCases)return 1;
 printf("Common health C170: %u cases match player records, health/inventory globals, all 32 palette words, all 16384 HUD bytes, collision coordinates and sound queue\n",checks);
 printf("Hurt-active records %u; death states %u; sound cases %u\n",hits,deaths,soundCases);return 0;
}
