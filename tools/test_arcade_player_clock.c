/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_services.h"
#include "../src/willow_arcade_player_clock.h"
#include "../native/intro_probe/src/player_data.h"
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
static uint8_t textRam[16384];
static WillowArcadeSoundQueue queue;
static uint16_t sourceWord(unsigned long at){uint16_t value;if(!nativePlayerDataRead(at,&value)){fprintf(stderr,"Missing ROM word %lx\n",at);abort();}return value;}
static void textWrite(uint32_t at,uint16_t value){if(at<0x910000 || at+1>=0x914000)abort();willowMotionPutWord(textRam+at-0x910000,value);}
static void sound(uint16_t value){willowArcadeSoundCommand(&queue,value,0);}
static uint32_t seed=0x57494c4c;
static uint32_t randomWord(void){seed=seed*1664525u+1013904223u;return seed;}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Only CPS text-RAM operands/pointers are relocated. No logic edits. */
 const unsigned patches[]={0x4f1a,0x4f30};
 const uint32_t originals[]={0x91140c,0x91158c};
 for(unsigned n=0;n<2;n++){if(willowMotionLong(rom+patches[n])!=originals[n])return 2;willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/clock-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/clock-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)



 unsigned checks=0,sounds=0;
 for(unsigned sample=0;sample<73728;sample++){
  memset(ram,0,65536);
  WillowPlayerHud d={0};WillowPlayerHealth h={.textWrite=textWrite,.sound=sound};
  WillowPlayerClock t={0};
  unsigned value=sample<65536?sample:((sample&1)?(sample>>1)%16:0x0300);
  d.time[0]=value>>8;d.time[1]=value;
  if(sample>=65536){
   unsigned n=sample-65536;t.blocked=(n&1)?0x8000:0;t.blockA=(n&2)?0x80:0;t.blockB=(n&4)?0xff:0;
   t.status=(n/8)%4;t.rebound=(n/32)%2;t.frame=(n/64)%256;
   const uint8_t phases[]={0,1,2,59,255};t.subframes=phases[(n/128)%5];
  }
  putRamWord(ram,0x83c6,value);putRamWord(ram,0x83d2,t.blocked);
  ram[0x83e9^1]=t.blockA;ram[0x83cf^1]=t.blockB;ram[0x83e2^1]=t.status;
  ram[0xf99e^1]=t.rebound;ram[0x8019^1]=t.frame;ram[0x83c8^1]=t.subframes;
  queue.writeOffset=(sample*2)&126;queue.readOffset=(sample*6)&126;
  queue.gateA=(sample&16)?0xff:0;queue.gateB=(sample&32)?0x80:0;
  putRamWord(ram,0x801e,queue.writeOffset);putRamWord(ram,0x8020,queue.readOffset);
  ram[0x8290^1]=queue.gateA;ram[0x8293^1]=queue.gateB;
  for(unsigned n=0;n<64;n++){queue.words[n]=randomWord();putRamWord(ram,0x8172+n*2,queue.words[n]);}
  for(unsigned n=0;n<sizeof textRam;n++){textRam[n]=(sample+n*73)>>3;ram[(0x4000+n)^1]=textRam[n];}
  uint16_t cursor=queue.writeOffset;
  willowPlayerClockStep(&t,&d,&h);RUN(0x4ea2,0xdeadbeef);
  if(t.subframes!=ram[0x83c8^1] || d.time[0]!=ram[0x83c6^1] || d.time[1]!=ram[0x83c7^1]){
   fprintf(stderr,"Clock case %u time %02x%02x/%04x divider %u/%u\n",sample,d.time[0],d.time[1],ramWord(ram,0x83c6),t.subframes,ram[0x83c8^1]);return 1;
  }
  for(unsigned n=0;n<sizeof textRam;n++)if(textRam[n]!=ram[(0x4000+n)^1]){fprintf(stderr,"Clock HUD case %u byte %u\n",sample,n);return 1;}
  if(queue.writeOffset!=ramWord(ram,0x801e) || queue.readOffset!=ramWord(ram,0x8020))return 1;
  for(unsigned n=0;n<64;n++)if(queue.words[n]!=ramWord(ram,0x8172+n*2))return 1;
  if(cursor!=queue.writeOffset)sounds++;checks++;
 }
 printf("Clock 4EA2: %u cases cover every 16-bit time input, divider/gates, full HUD memory and sound ring (%u sound cases)\n",checks,sounds);
 WillowPlayerCollision c={sourceWord,0,0,0,0,0};
 for(unsigned dip=0;dip<16;dip++){
  memset(ram,0,65536);ram[0x8068^1]=dip;
  /* Boot 147A..1482 loads the initial high score directly from ROM 183C. */
  if(willowMotionWord(rom+0x1482)!=0x2b51 || willowMotionWord(rom+0x1484)!=0x703e)return 2;
  for(unsigned n=0;n<4;n++)ram[(0xf03eUL+n)^1]=rom[0x183c+n];
  RUN(0x1b96,0);RUN(0x2cce,0);RUN(0x2dfe,0);
  WillowPlayerHud d={0};WillowPlayerHealth h={0};WillowPlayerCharge q={0};WillowPlayerClock t={0};
  willowPlayerHudColdStage0(&d,&h,&q,&t,&c,dip);
  if(d.lives!=ram[0x8342^1] || h.hp!=ram[0x8362^1] || h.maxHp!=ram[0x8363^1] || h.armor!=ram[0x8364^1] || h.cachedHp!=ram[0x8365^1] || h.cachedArmor!=ram[0x8389^1])return 1;
  for(unsigned n=0;n<4;n++)if(d.highScore[n]!=ram[(0xf03eUL+n)^1] || d.score[n]!=ram[(0x834a+n)^1] || d.otherScore[n]!=ram[(0x82ca+n)^1] || d.gold[n]!=ram[(0x8368+n)^1])return 1;
  for(unsigned n=0;n<12;n++)if(h.items[n]!=ram[(0x8352+n)^1])return 1;
  if((((unsigned)d.time[0]<<8)|d.time[1])!=ramWord(ram,0x83c6) || t.subframes!=ram[0x83c8^1] || q.value!=ramWord(ram,0x8376) || q.level!=ram[0x8366^1] || q.alternateLevel!=ram[0x8367^1] || q.active!=ram[0x83ca^1] || q.mode!=ram[0x83ea^1])return 1;
 }
 puts("Cold Stage 0 HUD subset: all 16 lives/health DIP combinations match original setup routines; boot high-score load checked separately");
 return 0;
}
