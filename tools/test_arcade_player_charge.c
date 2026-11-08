/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_services.h"
#include "../src/willow_arcade_player_charge.h"
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
 const unsigned patches[]={0x4b72,0x4fc6,0x5012,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910170,0x910170,0x910a70,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<10;n++){if(willowMotionLong(rom+patches[n])!=originals[n])return 2;willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/charge-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/charge-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)

 unsigned checks=0;
 WillowPlayerCollision collision={sourceWord,NULL,0,0,0,0};
 for(unsigned level=0;level<7;level++)for(unsigned character=0;character<2;character++)
 for(unsigned sample=0;sample<1024;sample++){
  memset(ram,0,65536);
  uint8_t player[128]={0};player[32]=character?0xff:0;player[49]=(sample%8)?1:0;
  WillowPlayerCharge q={0};q.value=sample<512?sample*128:(uint16_t)randomWord();
  const uint16_t cap=character?0x8fff:sourceWord(0x4c08+level*2);
  const uint16_t inc=character?0x11f:sourceWord(0x4bfa+level*2);
  const uint16_t edges[]={0,1,(uint16_t)(cap-inc-1),(uint16_t)(cap-inc),cap,(uint16_t)(cap+1),65535,(uint16_t)(65536-inc)};
  if(sample>=1008)q.value=edges[sample%8];
  q.level=level;q.alternateLevel=(sample&1)?level:255;
  q.mode=(sample%11==0)?0x80:0;q.active=(sample%3)?0x80:0;q.flash=randomWord()>>24;
  q.textWrite=textWrite;q.sound=sound;
  ram[0x8366^1]=q.level;ram[0x8367^1]=q.alternateLevel;
  ram[0x83ea^1]=q.mode;ram[0x83ca^1]=q.active;ram[0x0a3c^1]=q.flash;
  ram[0x9866^1]=player[32];ram[0x9877^1]=player[49];putRamWord(ram,0x8376,q.value);
  for(unsigned n=0;n<sizeof textRam;n++){textRam[n]=randomWord()>>24;ram[(0x4000+n)^1]=textRam[n];}
  queue.writeOffset=(sample*2)&126;queue.readOffset=(sample*6)&126;
  queue.gateA=(sample&1)?0xff:0;queue.gateB=(sample&2)?0x80:0;
  putRamWord(ram,0x801e,queue.writeOffset);putRamWord(ram,0x8020,queue.readOffset);
  ram[0x8290^1]=queue.gateA;ram[0x8293^1]=queue.gateB;
  for(unsigned n=0;n<64;n++){queue.words[n]=randomWord();putRamWord(ram,0x8172+n*2,queue.words[n]);}
  unsigned entry=(sample%13==0)?0x4fbe:0x4b0e;
  if(entry==0x4fbe)willowPlayerChargeHudReset(&q,&collision,player[32]);
  else willowPlayerChargeStep(&q,&collision,player);
  RUN(entry,0xdeadbeef);
  if(q.value!=ramWord(ram,0x8376) || q.active!=ram[0x83ca^1] || q.flash!=ram[0x0a3c^1]){
   fprintf(stderr,"Charge mismatch level %u char %u sample %u value %x/%x active %u/%u flash %u/%u\n",level,character,sample,q.value,ramWord(ram,0x8376),q.active,ram[0x83ca^1],q.flash,ram[0x0a3c^1]);return 1;
  }
  for(unsigned n=0;n<sizeof textRam;n++)if(textRam[n]!=ram[(0x4000+n)^1]){
   fprintf(stderr,"HUD mismatch level %u char %u sample %u byte %x native %x source %x\n",level,character,sample,n,textRam[n],ram[(0x4000+n)^1]);return 1;
  }
  if(queue.writeOffset!=ramWord(ram,0x801e) || queue.readOffset!=ramWord(ram,0x8020))return 1;
  for(unsigned n=0;n<64;n++)if(queue.words[n]!=ramWord(ram,0x8172+n*2))return 1;
  checks++;
 }
 printf("Charge meter: %u original-instruction cases, complete Scroll1 RAM and sound ring match\n",checks);
 return 0;
}
