/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_services.h"
#include "../src/willow_arcade_hud_init.h"
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
static uint8_t textRam[16384];
static WillowArcadeCommandQueue queue;
static uint16_t sourceWord(unsigned long at){uint16_t value;if(!nativePlayerDataRead(at,&value)){fprintf(stderr,"Missing ROM word %lx\n",at);abort();}return value;}
static void textWrite(uint32_t at,uint16_t value){if(at<0x910000 || at+1>=0x914000)abort();willowMotionPutWord(textRam+(at-0x910000),value);}
static void command(uint16_t value,uint16_t argument){willowArcadeCommand(&queue,value,argument);}
static uint8_t expected[65536];
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static void busText(uint32_t at,uint16_t v){textWrite(at,v);ww(0x4000+at-0x910000,v);}
static uint32_t seed=0x57494c4c;
static uint32_t randomWord(void){seed=seed*1664525u+1013904223u;return seed;}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Only CPS text-RAM operands/pointers are relocated. No logic edits. */
 const unsigned patches[]={0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/hud-bus-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/hud-bus-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)


 unsigned checks=0;
 WillowPlayerCollision collision={sourceWord,NULL,0,0,0,0};
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 for(unsigned level=0;level<7;level++)for(unsigned character=0;character<2;character++)
 for(unsigned alternate=0;alternate<5;alternate++)for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)ram[n]=(uint8_t)(n*7+sample*31+level*19);
  WillowPlayerCharge q={.value=(uint16_t)randomWord(),.active=(uint8_t)randomWord(),.flash=(uint8_t)randomWord(),
   .level=level,.alternateLevel=alternate,.textWrite=textWrite};
  WillowPlayerHealth health={.hp=sample%9,.maxHp=8,.armor=sample%6,.textWrite=textWrite};
  for(unsigned n=0;n<12;n++)health.items[n]=(sample>>(n%4))%4;
  WillowPlayerHud hud={.lives=(uint8_t)sample,.secondPlayer=(sample&1)?0xff:0,.command=command};
  uint8_t*fields[]={hud.time,hud.gold,hud.score,hud.otherScore,hud.highScore};
  const unsigned addresses[]={0x83c6,0x8368,0x834a,0x82ca,0xf03e},lengths[]={2,4,4,4,4};
  for(unsigned field=0;field<5;field++)for(unsigned n=0;n<lengths[field];n++){
   fields[field][n]=(sample%4)?randomWord()>>24:0;ram[(addresses[field]+n)^1]=fields[field][n];
  }
  for(unsigned n=0;n<12;n++)ram[(0x8352+n)^1]=health.items[n];
  ram[0x8362^1]=health.hp;ram[0x8363^1]=health.maxHp;ram[0x8364^1]=health.armor;
  ram[0x8366^1]=q.level;ram[0x8367^1]=q.alternateLevel;
  ram[0x83ca^1]=q.active;ram[0x0a3c^1]=q.flash;putRamWord(ram,0x8376,q.value);
  ram[0x9866^1]=character?4:0;ram[0x8342^1]=hud.lives;ram[0x8292^1]=hud.secondPlayer;
  for(unsigned n=0;n<sizeof textRam;n++){textRam[n]=randomWord()>>24;ram[(0x4000+n)^1]=textRam[n];}
  queue.writeOffset=sample*4;putRamWord(ram,0x801a,queue.writeOffset);
  for(unsigned n=0;n<128;n++){queue.words[n]=randomWord();putRamWord(ram,0x8072+n*2,queue.words[n]);}
  for(unsigned n=0;n<65536;n++)expected[n]=ram[n^1];
  if(!willowArcadeHudInit(&bus,sourceWord,busText))return 1;
  RUN(0x4f44,0xdeadbeef);
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&expected[n]!=ram[n^1]){
   fprintf(stderr,"HUD bus mismatch sample %u level %u char %u at %x native %x source %x\n",sample,level,character,n,expected[n],ram[n^1]);return 1;
  }
  for(unsigned n=0;n<sizeof textRam;n++)if(textRam[n]!=ram[(0x4000+n)^1]){
   fprintf(stderr,"HUD init level %u char %u alternate %u sample %u byte %x native %x source %x\n",level,character,alternate,sample,n,textRam[n],ram[(0x4000+n)^1]);return 1;
  }
  /* Render command-12 labels through original internal zero-delay entry.
   * Task byte 20 requests RTS instead of scheduler termination. The full
   * queue consumer/scheduler is intentionally not under test here. */
  uint32_t labels=0x5c7eUL+(int16_t)sourceWord(0x5c96);
  m68k_set_reg(8,labels);m68k_set_reg(14,0xff3000);
  ram[0x3012^1]=0;ram[0x3013^1]=0;ram[0x3014^1]=1;
  willowPlayerHudLabels(&health,&collision);
  RUN(0x5bc8,0);
  for(unsigned n=0;n<sizeof textRam;n++)if(textRam[n]!=ram[(0x4000+n)^1]){fprintf(stderr,"HUD labels sample %u byte %x\n",sample,n);return 1;}
  checks++;
 }
 printf("Gameplay HUD bus 4F44: %u original-instruction cases, full Scroll1 RAM, charge state, text-command ring and zero-delay labels match\n",checks);
 return 0;
}
