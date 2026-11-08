/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_selector8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "../src/willow_arcade_actor_pool.h"
static uint8_t rom[0x200000],expected[65536];
static WillowActorPool pool;static WillowActorPoolIndex indices[60];
static uint16_t sourceWord(unsigned long at){return willowMotionWord(rom+at);}
static uint16_t busWord(uint16_t at){return willowMotionWord(expected+at);}
static uint8_t busByte(uint16_t at){return expected[at];}
static void busPutWord(uint16_t at,uint16_t value){willowMotionPutWord(expected+at,value);}
static void busPutByte(uint16_t at,uint8_t value){expected[at]=value;}
static void releaseActor(uint8_t *record){
 willowActorPoolRelease(&pool,(record-pool.records)/64);
 uint16_t next=busWord(0x9834)-2;
 busPutWord(next,record-expected);busPutWord(0x9834,next);expected[0x980d]++;
}

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
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;

 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/selector8-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/selector8-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 unsigned checks=0;
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 const int16_t offsets[]={-33,-32,-17,-16,-15,-14,-13,-12,-11,0,11,12,13,14,15,16,17,32,33};
 for(unsigned sample=0;sample<16384;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample+n*17)^0x39;
  pool=(WillowActorPool){expected+0xccc6,indices,60,64,1,59};
  uint8_t*r=expected+0xccc6;r[0]=1;r[2]=((sample/48)%4)*2;r[14]=((sample/4)%3)*2;r[30]=12;r[32]=8;
  r[33]=((sample/3)%16)*16+sample%3;r[3]=((sample/192)%3)*2;r[34]=(sample/16)%3==0?0:(sample/16)%3==1?1:30;
  if(r[2]==2 && (r[33]&15)!=1 && r[3]==4)continue;
  uint16_t x=sample*137,y=sample*71;
  willowMotionPutWord(r+6,x);willowMotionPutWord(r+10,y);
  willowMotionPutLong(r+20,(r[33]&15)==1?0x32cac:0x32c9c);willowMotionPutWord(r+28,sample%3);
  busPutWord(0x9834,0xa002);expected[0x980d]=59;
  busPutWord(0x9836,0xa082);expected[0x980e]=0;
  busPutWord(0x9838,0xa102);expected[0x980f]=0;
  busPutWord(0xf900,0xa182);expected[0xf902]=0;
  WillowActorSelector8Globals g={x+offsets[(sample/5)%19],y+offsets[(sample/7)%19],
   sample&64?4:0,(sample/8)%16,(sample/32)%256,1,0x73,sample&16?1:0};
  WillowActorSelector8Context c={x-(sample%3)*256,y-(sample%4)*128,0xccc6,sourceWord,&bus,releaseActor,&g};
  busPutWord(0x8408,c.cameraX);busPutWord(0x840c,c.cameraY);
  busPutWord(0x984c,g.playerX);busPutWord(0x9850,g.playerY);
#define SAVE_GLOBALS() do {expected[0x8362]=g.hp;expected[0x806a]=g.held;expected[0x839a]=g.used;expected[0x9846]=g.playerActive;expected[0xf0e]=g.eventArgument;expected[0x83c5]=g.eventBusy;}while(0)
  SAVE_GLOBALS();for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  if(!willowArcadeActorSelector8(r,&c))return 1;SAVE_GLOBALS();
  m68k_set_reg(14,0xffccc6);RUN(0x32be6,0);
  for(unsigned n=0;n<65536;n++)if(n<0xfed0 || n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Selector8 mismatch sample %u address %04x native %02x source %02x\n",sample,n,expected[n],ram[n^1]);return 1;}
  }
  checks++;
 }
 unsigned sequences=0;
 for(unsigned variant=0;variant<3;variant++)for(unsigned bit=0;bit<16;bit++){
  memset(expected,0,sizeof expected);pool=(WillowActorPool){expected+0xccc6,indices,60,64,1,59};
  uint8_t*r=expected+0xccc6;r[0]=1;r[30]=12;r[32]=8;r[33]=(bit<<4)|variant;r[15]=0x6d;
  willowMotionPutWord(r+6,1872);willowMotionPutWord(r+10,311);
  busPutWord(0x9834,0xa002);expected[0x980d]=59;
  busPutWord(0x9836,0xa082);busPutWord(0x9838,0xa102);busPutWord(0xf900,0xa182);
  WillowActorSelector8Globals g={1872,311,4,8,0,1,0,0};
  WillowActorSelector8Context c={1872,311,0xccc6,sourceWord,&bus,releaseActor,&g};
  busPutWord(0x8408,c.cameraX);busPutWord(0x840c,c.cameraY);
  busPutWord(0x984c,g.playerX);busPutWord(0x9850,g.playerY);SAVE_GLOBALS();
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  for(unsigned frame=0;frame<50;frame++){
   /* An external event-completion signal is a fixture input. The event
    * consumer itself is outside this actor handler. */
   if(frame==40){g.eventBusy=0;expected[0x83c5]=0;ram[0x83c5^1]=0;}
   if(r[0]){if(!willowArcadeActorSelector8(r,&c))return 1;SAVE_GLOBALS();}
   if(ram[0xccc6^1]){m68k_set_reg(14,0xffccc6);RUN(0x32be6,0);}
   for(unsigned n=0;n<65536;n++)if(n<0xfed0 || n>=0xff04){
    if(ram[n^1]!=expected[n]){fprintf(stderr,"Selector8 sequence mismatch variant %u bit %u frame %u address %04x\n",variant,bit,frame,n);return 1;}
   }
  }
  if(r[0] || pool.available!=60 || g.used!=(1u<<(bit&7)) || g.eventArgument!=0x6d)return 1;
  sequences++;
 }
 printf("Selector-8 event lifetimes: %u consecutive initialization/trigger/wait/completion/release sequences\n",sequences);
 printf("Original selector-8 dispatch: %u variant/state/bit/proximity/input/timer/event/queue/release comparisons, full non-stack RAM\n",checks);
 return 0;
}
