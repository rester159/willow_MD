/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_event_consumer.h"
#include "../src/willow_arcade_tasks.h"
#include "../src/willow_arcade_text_quad.h"
#include "../src/willow_arcade_event_sprites.h"
#include "../src/willow_arcade_event_price.h"
#include "../src/willow_arcade_event_messages.h"
#include "../src/willow_arcade_event_grant.h"
#include "../src/willow_arcade_event_hud.h"
#include "../src/willow_arcade_event_commands.h"
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
static uint8_t expected[65536];
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static unsigned cursor;
static uint32_t eventD4;
static unsigned meterResetCalls,alternateMeterCalls;
static int realTasks,realSymbols,realHealth;
static uint16_t sourceWord(uint32_t at){return willowMotionWord(rom+at);}
static const uint32_t services[]={0x7d80,0x349f8,0x346e0,0x348fc,0x34998,
 0x34272,0x341da,0x4f44,0x1eaa,0x15ee,0x7f3e,
 0x1e88,0x785e,0x0b28,0x78c8,0x548c,0x5532,0x34a4a,
 0x168c,0x786a,0x2de6,0x0978,0x4e32,0x5040,0x4fbe,0x1f14,0x4d6a,0x4a34};
static const uint16_t observed[]={0x800c,0x8010,0x8012,0x804a,0x8376,
 0x0f0e,0x0f10,0x0f12,0x0f14,0x0f16,0x0f18,0x0f1a,0x0f1c,
 0x0f1e,0x0f20,0x0f22,0x0f24,0x0f26,0x0f28,0x0f2a,0x0f2c,
 0x0f2e,0x0f30,0x0f32,0x0f34,0x0f36,0x0f38,0x0f3a,
 0x8342,0x83c4,0x83ca,0x9884,0x8408,0x840c,0x8422,0x8444,0x8446,
 0x844a,0x8488,0x848c,0x84a2,0x84c4,0x84c6,0x84ca,
 0x8032,0x8034,0x8036,0x8038};
static void snapshot(void){
 for(unsigned n=0;n<sizeof observed/sizeof *observed;n++){ww(cursor,rw(observed[n]));cursor+=2;}
}
static void symbolWord(uint32_t address,uint16_t value);
static void task(uint16_t d0,uint16_t d1,uint16_t d2,uint32_t a0);
static uint16_t inventoryWord(unsigned long address){return sourceWord(address);}
static void service(uint32_t address,int argument){
 if(realSymbols&&address==0x1f14){
  const WillowVisibilityBus bus={rw,rb,ww,wb};
  /* This call site is 346DA: D0=0404 and MOVEQ at 346C8 left D1=1. */
  if(argument!=0x0404)abort();
  willowArcadeEventCommand(&bus,argument,1);
  /* The original producer clobbers A4, also used by fixture log stubs.
   * Follow that register value; do not patch the source to preserve it. */
  cursor=0x8072;return;
 }
 if(realHealth&&address==0x4f44){
  const WillowVisibilityBus bus={rw,rb,ww,wb};
  WillowPlayerCollision collision={0};collision.word=inventoryWord;
  eventD4=(eventD4&0xffff0000UL)|willowArcadeEventHudRebuild(&bus,&collision,symbolWord,eventD4);
  cursor=0x8072;return;
 }
 if(realHealth&&(address==0x4fbe||address==0x5040)){
  const WillowVisibilityBus bus={rw,rb,ww,wb};
  WillowPlayerCollision collision={0};collision.word=inventoryWord;
  if(address==0x4fbe)meterResetCalls++;
  else if(rb(0x9866))alternateMeterCalls++;
  if(!willowArcadeEventChargeHud(address,&bus,&collision,symbolWord))abort();return;
 }
 if(realHealth&&address==0x4a34){
  const WillowVisibilityBus bus={rw,rb,ww,wb};
  willowArcadeEventHealthHud(&bus,symbolWord);return;
 }
 if(realSymbols&&address==0x4d6a){
  WillowPlayerCollision collision={0};collision.word=inventoryWord;
  uint16_t result=willowArcadeEventInventoryHud(argument,eventD4,&collision,symbolWord);
  eventD4=(eventD4&0xffff0000UL)|result;return;
 }
 if(realSymbols&&address==0x4e32){
  const WillowVisibilityBus bus={rw,rb,ww,wb};
  willowArcadeEventMoneyHud(&bus,symbolWord);return;
 }
 if(realSymbols&&(address==0x7d80||address==0x7da8)){
  if(!willowArcadeEventClear(address,argument,symbolWord))abort();return;
 }
 if(realSymbols&&address==0x346e0){
  const WillowVisibilityBus bus={rw,rb,ww,wb};
  const WillowEventUiServices ui={service,task,NULL};
  willowArcadeEventMessages(&bus,&ui,sourceWord);return;
 }
 if(realSymbols&&address==0x348fc){
  const WillowVisibilityBus bus={rw,rb,ww,wb};
  eventD4=willowArcadeEventPrice(&bus,sourceWord,symbolWord);return;
 }
 if(realSymbols&&(address==0x34998||address==0x349f8||address==0x34a4a)){
  const WillowVisibilityBus bus={rw,rb,ww,wb};
  if(!willowArcadeEventSprites(address,&bus,sourceWord,symbolWord))abort();
  return;
 }
 unsigned id=0;while(id<sizeof services/sizeof *services&&services[id]!=address)id++;
 ww(cursor,id);cursor+=2;
 if(address==0x7d80||address==0x548c||address==0x5532||address==0x168c||address==0x786a||address==0x1f14||address==0x4d6a){ww(cursor,argument);cursor+=2;}
 snapshot();
 if(address==0x7d80)ww(0x0f10,0x4444); /* ordering fixture */
}
static void task(uint16_t d0,uint16_t d1,uint16_t d2,uint32_t a0){
 if(realTasks){
  const WillowVisibilityBus bus={rw,rb,ww,wb};
  willowArcadeTaskStart(&bus,d0,a0,d1,d2,1);return;
 }
 ww(cursor,9);cursor+=2;ww(cursor,d0);cursor+=2;ww(cursor,d1);cursor+=2;
 ww(cursor,d2);cursor+=2;ww(cursor,a0>>16);cursor+=2;ww(cursor,a0);cursor+=2;
 snapshot();wb(0x0f0e,rb(0x0f0e)^1); /* re-read fixture */
}
static void symbolWord(uint32_t address,uint16_t value){
 if(address>=0x900000&&address<0x902020){ww(0xa000+address-0x900000,value);return;}
 if(address>=0x917aa8&&address<0x917ad0){ww(0x7000+address-0x917aa8,value);return;}
 if(address<0x910000||address>=0x914000){fprintf(stderr,"Invalid symbol destination\n");abort();}
 ww(0x4000+address-0x910000,value);
}
static void text(uint32_t a0,uint32_t a1){
 if(realSymbols){(void)willowArcadeTextQuad(a0,a1,0,1,sourceWord,symbolWord);return;}
 ww(cursor,10);cursor+=2;ww(cursor,a0>>16);cursor+=2;ww(cursor,a0);cursor+=2;
 ww(cursor,a1>>16);cursor+=2;ww(cursor,a1);cursor+=2;snapshot();
}
int main(int argc,char**argv){
 if(argc!=2&&argc!=3)return 2;
 realHealth=argc==3&&!strcmp(argv[2],"--real-health");
 realSymbols=realHealth||(argc==3&&!strcmp(argv[2],"--real-services"));
 realTasks=realSymbols||(argc==3&&!strcmp(argv[2],"--real-tasks"));
 if(argc==3&&!realTasks)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Instrument only service endpoints. The original routing instructions,
  * branches and RAM writes execute unchanged. Each service records its ID,
  * explicit argument and state at invocation, with explicit mutation fixtures. */
 for(unsigned n=0;n<sizeof services/sizeof *services;n++){
  if(realTasks&&services[n]==0x15ee)continue;
  if(realSymbols&&services[n]==0x1f14)continue;
  if(realHealth&&(services[n]==0x4f44||services[n]==0x4a34||services[n]==0x4fbe||services[n]==0x5040))continue;
  if(realSymbols&&(services[n]==0x4e32||services[n]==0x4d6a))continue;
  if(realSymbols&&(services[n]==0x7f3e||services[n]==0x348fc||services[n]==0x7d80||services[n]==0x346e0))continue;
  if(realSymbols&&(services[n]==0x34998||services[n]==0x349f8||services[n]==0x34a4a))continue;
  uint32_t at=0x101000+n*512;
  willowMotionPutWord(rom+services[n],0x4ef9);willowMotionPutLong(rom+services[n]+2,at);
#define EMIT(v) do{willowMotionPutWord(rom+at,(v));at+=2;}while(0)
  EMIT(0x38fc);EMIT(n); /* move.w #id,(a4)+ */
  if(services[n]==0x7d80||services[n]==0x168c||services[n]==0x786a||services[n]==0x1f14||services[n]==0x4d6a)EMIT(0x38c0);
  if(services[n]==0x4d6a){EMIT(0x026c);EMIT(0x00ff);EMIT(0xfffe);}
  if(services[n]==0x548c||services[n]==0x5532)EMIT(0x38ce);
  if(services[n]==0x15ee){EMIT(0x38c0);EMIT(0x38c1);EMIT(0x38c2);EMIT(0x28c8);}
  if(services[n]==0x7f3e){EMIT(0x28c8);EMIT(0x28c9);}
  for(unsigned j=0;j<sizeof observed/sizeof *observed;j++){
   EMIT(0x38f9);EMIT(0x00ff);EMIT(observed[j]);
  }
  if(services[n]==0x7d80){EMIT(0x33fc);EMIT(0x4444);EMIT(0x00ff);EMIT(0x0f10);}
  if(services[n]==0x15ee){EMIT(0x0a39);EMIT(1);EMIT(0x00ff);EMIT(0x0f0e);}
  EMIT(0x4e75);
 }
 if(willowMotionLong(rom+0x34b0a)!=0x917aa8)return 2;
 willowMotionPutLong(rom+0x34b0a,0xff7000);
 if(realHealth){
  const unsigned hudPointers[]={0x4f56,0x4f82,0x4f1a,0x4f30,0x4da8,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c};
  const uint32_t hudDestinations[]={0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88};
  for(unsigned n=0;n<sizeof hudPointers/sizeof *hudPointers;n++){
   if(willowMotionLong(rom+hudPointers[n])!=hudDestinations[n])return 2;
   willowMotionPutLong(rom+hudPointers[n],0xff4000+hudDestinations[n]-0x910000);
  }
  const unsigned meterPointers[]={0x4fc6,0x5012,0x4c50,0x504c,0x5062};
  const uint32_t meterDestinations[]={0x910170,0x910a70,0x910b70,0x910af0,0x910d70};
  for(unsigned n=0;n<5;n++){
   if(willowMotionLong(rom+meterPointers[n])!=meterDestinations[n])return 2;
   willowMotionPutLong(rom+meterPointers[n],0xff4000+meterDestinations[n]-0x910000);
  }
  if(willowMotionLong(rom+0x5026)!=0x5080)return 2;
  willowMotionPutLong(rom+0x5026,0xff3040);
  const unsigned pointers[]={0x4a36,0x4a40};
  for(unsigned n=0;n<2;n++){
   if(willowMotionLong(rom+pointers[n])!=0x91030c)return 2;
   willowMotionPutLong(rom+pointers[n],0xff430c);
  }
 }
 if(realSymbols){
  const unsigned pointers[]={0x349a0,0x34a00,0x34a6c};
  for(unsigned n=0;n<3;n++){
   uint32_t original=n==2?0x900020:0x900000;
   if(willowMotionLong(rom+pointers[n])!=original)return 2;
   willowMotionPutLong(rom+pointers[n],0xffa000+original-0x900000);
  }
  const unsigned clearPointers[]={0x7d84,0x7daa};
  for(unsigned n=0;n<2;n++){
   if(willowMotionLong(rom+clearPointers[n])!=0x910200)return 2;
   willowMotionPutLong(rom+clearPointers[n],0xff4200);
  }
  if(willowMotionLong(rom+0x4d6c)!=0x910df0)return 2;
  willowMotionPutLong(rom+0x4d6c,0xff4df0);
  const unsigned moneyPointers[]={0x4e34,0x4e4a,0x4e5e};
  const uint32_t moneyDestinations[]={0x910c0c,0x910d0c,0x910e0c};
  for(unsigned n=0;n<3;n++){
   if(willowMotionLong(rom+moneyPointers[n])!=moneyDestinations[n])return 2;
   willowMotionPutLong(rom+moneyPointers[n],0xff4000+moneyDestinations[n]-0x910000);
  }
  const unsigned pricePointers[]={0x34952,0x34962,0x34970};
  const uint32_t priceDestinations[]={0x910f58,0x911158,0x911358};
  for(unsigned n=0;n<3;n++){
   if(willowMotionLong(rom+pricePointers[n])!=priceDestinations[n])return 2;
   willowMotionPutLong(rom+pricePointers[n],0xff4000+priceDestinations[n]-0x910000);
  }
  /* Relocate only the event's CPS Scroll1 destination into fixture RAM. */
  if(willowMotionLong(rom+0x33eda)!=0x910e58)return 2;
  willowMotionPutLong(rom+0x33eda,0xff4e58);
 }
 willowMotionPutLong(rom+0xa8,0x15f2); /* original trap 10 handler */
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/event-grant-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/event-grant-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 const WillowEventUiServices ui={service,task,text};
 if(realHealth)puts("Gameplay HUD integration: original health and charge meters execute uninstrumented with valid field fixtures; edge-fixture mode retained separately");
 unsigned checks=0;
 for(unsigned item=0;item<28;item++)for(unsigned position=0;position<12;position++)for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  ww(0x0f10,(position%4)*2);ww(0x0f12,(position/4)*2);wb(0x0f22+position,item);
  ww(0x0f14,sample&1?(1u<<position):0);wb(0x0f0f,sample);
  for(unsigned n=0;n<12;n++)wb(0x8352+n,((sample>>2)&7)==7?255:((sample>>2)&7));
  wb(0x8362,sample&7);wb(0x8363,(sample>>3)&7);wb(0x8364,sample*17);
  wb(0x9866,sample&4?2:0);wb(0x8342,sample*17);
  ww(0x8368,sample&2?0x9999:0);ww(0x836a,sample&2?0x9999:0);
  for(unsigned n=0;n<6;n++){ww(0x0f16+n*2,0x1234);ww(0x8378+n*2,sample*317+n);}
  if(realHealth){
   wb(0x8366,sample%7);wb(0x8367,sample%5);wb(0x9866,(sample>>2)&1);
   for(unsigned level=0;level<7;level++){
    uint32_t at=0x5080+level*4;
    uint32_t original=((uint32_t)sourceWord(at)<<16)|sourceWord(at+2);
    uint32_t relocated=0xff4000+original-0x910000;
    ww(0x3040+level*4,relocated>>16);ww(0x3042+level*4,relocated);
   }
   uint8_t maximum=1+sample%6;
   wb(0x8363,maximum);wb(0x8362,(sample>>3)%(maximum+1));wb(0x8364,sample%4);
  }
  ww(0x801a,(sample%64)*4);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  cursor=0xe000;eventD4=0x5a5a0000UL|((sample*977)&65535);
  m68k_set_reg(4,eventD4);
  if(!willowArcadeEventGrant(&bus,&ui,sourceWord))return 1;
  m68k_set_reg(12,0xffe000);m68k_set_reg(14,0xffaaaa);
  RUN(0x3436e,0xdeadbeef);
  if(realSymbols&&m68k_get_reg(4)!=eventD4){fprintf(stderr,"Event D4 context mismatch sample %u got %08x want %08x\n",sample,m68k_get_reg(4),eventD4);return 1;}
  if(m68k_get_reg(12)!=0xff0000+cursor){fprintf(stderr,"Grant service count mismatch item %u sample %u\n",item,sample);return 1;}
  for(unsigned n=0;n<65536;n++)if(n<0xfe80||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Grant mismatch item %u position %u sample %u address %04x got %02x want %02x\n",item,position,sample,n,ram[n^1],expected[n]);return 1;}
  }
  checks++;
 }
 if(realHealth){
  if(!meterResetCalls||!alternateMeterCalls){fprintf(stderr,"Missing meter coverage\n");return 1;}
  printf("Meter coverage: %u resets and %u active alternate-meter calls\n",meterResetCalls,alternateMeterCalls);
 }
 printf("Source event grants: %u confirmations covering all 28 targets and 12 selection positions; actual payment, marker, message tasks and full non-stack RAM match; external HUD/sound endpoints instrumented\n",checks);
 return 0;
}
