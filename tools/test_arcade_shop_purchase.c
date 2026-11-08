/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_arcade_shop_resume.h"
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
static uint8_t expected[65536],initial[65536];
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static int realTasks,realSymbols,realHealth;
static uint16_t sourceWord(uint32_t at){return willowMotionWord(rom+at);}
static uint16_t inventoryWord(unsigned long at){return sourceWord(at);}
static void symbolWord(uint32_t address,uint16_t value){
 if(address>=0x900000&&address<0x902020){ww(0xa000+address-0x900000,value);return;}
 if(address>=0x917aa8&&address<0x917ad0){ww(0x7000+address-0x917aa8,value);return;}
 if(address<0x910000||address>=0x914000){fprintf(stderr,"Invalid symbol destination\n");abort();}
 ww(0x4000+address-0x910000,value);
}
int main(int argc,char**argv){
 if(argc!=2&&argc!=4)return 2;
 realHealth=realSymbols=realTasks=1;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* All original shop/task/HUD/input/payment bodies execute unchanged.
  * Only checked CPS video operand addresses are relocated to fixture RAM. */
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
 unsigned nativeEntry=0,nativeChecks=0;
 if(argc==4){
  nativeEntry=(unsigned)strtoul(argv[3],0,0);if(nativeEntry<0x180000||nativeEntry>=0x200000)return 2;
  f=fopen(argv[2],"rb");if(!f)return 2;size_t bytes=fread(rom+0x180000,1,0x80000,f);
  if(!bytes||!feof(f))return 2;fclose(f);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 const char*oraclePath=getenv("WILLOW_SHOP_ORACLE");
 if(!oraclePath)oraclePath="build/native-shop-purchase/source-consumer-oracle.bin";
 f=fopen(oraclePath,"wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={oraclePath,rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700|(sample&31));m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 WillowPlayerCollision collision={0};collision.word=inventoryWord;
 unsigned checks=0,entries=0,confirmations=0,navigation=0,messageTicks=0;
 /* Entire outer33BD6 consumer, including34AD4 animation after each handler. */
 for(unsigned mode=0;mode<5;mode++)for(unsigned item=0;item<(mode==4?18:28);item++)
 for(unsigned position=0;position<(mode==0||mode==4?1:12);position++)for(unsigned sample=0;sample<32;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  ww(0x800c,2);ww(0x8010,mode==0?0:mode==4?6:2);ww(0x8012,mode==3?0:1);
  for(unsigned n=0;n<12;n++)wb(0x0f22+n,(item+n+28-position)%28);
  ww(0x0f10,mode==0?0:(position%4)*2);ww(0x0f12,mode==0?0:(position/4)*2);
  ww(0x0f14,sample&1?(1u<<position):0);wb(0x0f0f,0);
  wb(0x806a,mode==2?(1u<<(sample%4)):0x10);wb(0x806b,0);
  for(unsigned n=0;n<12;n++)wb(0x8352+n,(sample>>2)%4);
  wb(0x8366,sample%7);wb(0x8367,sample%5);wb(0x9866,(sample>>2)&1);
  wb(0x8363,1+sample%8);wb(0x8362,(sample>>3)%(rb(0x8363)+1));wb(0x8364,sample%7);
  wb(0x8342,sample*3);ww(0x8368,sample&2?0x9999:0);ww(0x836a,sample&2?0x9999:0);
  for(unsigned n=0;n<6;n++){ww(0x0f16+n*2,0x1234);ww(0x8378+n*2,sample*317+n);}
  ww(0x0f2e,0);ww(0x0f30,0x1234);ww(0x0f32,0);ww(0x0f34,0x5678);
  wb(0x0f38,sample%64);wb(0x0f39,sample&1);wb(0x0f3a,sample%11);wb(0x0f3b,0);
  wb(0x0f36,mode==4?(sample&1?2:0):0);wb(0x0f0e,mode==4?item:item%18);
  wb(0x0e58,(sample>>1)&1);wb(0x0f37,sample%3);
  ww(0x801a,(sample%64)*4);
  for(unsigned level=0;level<7;level++){
   uint32_t at=0x5080+level*4,original=((uint32_t)sourceWord(at)<<16)|sourceWord(at+2);
   uint32_t relocated=0xff4000+original-0x910000;ww(0x3040+level*4,relocated>>16);ww(0x3042+level*4,relocated);
  }
  memcpy(initial,expected,sizeof initial);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d4=(sample*977)&65535;uint32_t a4=0xffe000;
  WillowArcadeGameplayServices native={.bus=&bus,.busWord=sourceWord,.collision=&collision,.d4=&d4,.a4=&a4,.videoWrite=symbolWord,.textWrite=symbolWord};
  WillowArcadeShopTask taskState={0};
  if(willowArcadeShopResume(&taskState,&bus,sourceWord,willowArcadeGameplayServiceResume,&native)!=1||taskState.pc){fprintf(stderr,"Native reject mode%u item%u pos%u sample%u\n",mode,item,position,sample);return 1;}
  m68k_set_reg(4,0x5a5a0000UL|((sample*977)&65535));m68k_set_reg(12,0xffe000);m68k_set_reg(14,0xffaaaa);
  RUN(0x33bd6,0xdeadbeef);
  if((uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=a4){fprintf(stderr,"Live source context mismatch mode%u item%u pos%u sample%u D4%x/%x A4%x/%x\n",mode,item,position,sample,m68k_get_reg(4),d4,m68k_get_reg(12),a4);return 1;}
  for(unsigned n=0;n<65536;n++)if(n<0xfe80||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Shop consumer mismatch mode%u item%u pos%u sample%u RAM%04x got%02x want%02x\n",mode,item,position,sample,n,ram[n^1],expected[n]);return 1;}
  }
  if(nativeEntry&&position==0&&sample<8){
   for(unsigned n=0;n<65536;n++)ram[n^1]=initial[n];
   putRamWord(ram,0x3000,(sample*977)&65535);putRamWord(ram,0x3004,0xff);putRamWord(ram,0x3006,0xe000);
   putRamWord(ram,0x3008,0);putRamWord(ram,0x300a,0);putRamWord(ram,0x300c,0);
   putRamWord(ram,0xeffc,0x10);putRamWord(ram,0xeffe,0);
   m68k_set_reg(17,0x2700|(sample&31));m68k_set_reg(15,0xffeffc);
   unsigned saved[15];for(unsigned n=0;n<15;n++){saved[n]=0x15370000u+n*0x1031u;m68k_set_reg(n,saved[n]);}
   m68k_set_reg(16,nativeEntry);
   for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000||m68k_get_reg(15)!=0xfff000){fprintf(stderr,"Native did not return PC%x SP%x\n",m68k_get_reg(16),m68k_get_reg(15));return 1;}
   for(unsigned n=2;n<15;n++)if(n!=8&&n!=9&&m68k_get_reg(n)!=saved[n]){fprintf(stderr,"Native C ABI register%u changed\n",n);return 1;}
   unsigned gotD4=(ram[0x3001]<<8)|ram[0x3000],gotA4=((uint32_t)ram[0x3005]<<24)|((uint32_t)ram[0x3004]<<16)|(ram[0x3007]<<8)|ram[0x3006];
   if(gotD4!=d4||gotA4!=a4||ram[0x3008]!=1||ram[0x3009]||ram[0x300a]||ram[0x300b]||ram[0x300c]||ram[0x300d]){fprintf(stderr,"Native context/result mode%u item%u sample%u D4%x/%x A4%x/%x status%x pc%x bad%x\n",mode,item,sample,gotD4,d4,gotA4,a4,ram[0x3008],ram[0x300a],ram[0x300c]);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0x3000&&n<0x300e)&&!(n>=0xe000&&n<0xf000)){
    if(ram[n^1]!=expected[n]){fprintf(stderr,"Native shop mismatch mode%u item%u sample%u RAM%04x got%02x want%02x\n",mode,item,sample,n,ram[n^1],expected[n]);return 1;}
   }
   nativeChecks++;
  }
  checks++;if(mode==0)entries++;else if(mode==1)confirmations++;else if(mode==2)navigation++;else if(mode==4)messageTicks++;
 }
 unsigned sequenceTicks=0,negativeCases=0;
 /* Persistent source state across entry, repeated confirmation, asynchronous
  * message completion,31-tick response delay and the leave request. External
  * text-task completion is applied equally before both consumers, never used
  * as target behavior; executing message text itself is a separate scheduler. */
 for(unsigned episode=0;episode<46;episode++){
  memset(expected,0,sizeof expected);ww(0x800c,2);ww(0x8010,0);ww(0x8012,1);
  unsigned item=episode<28?episode:1;wb(0x0f0e,episode<28?episode%18:episode-28);
  for(unsigned n=0;n<12;n++)wb(0x0f22+n,item);
  wb(0x8362,2);wb(0x8363,4);wb(0x8364,5);wb(0x9866,episode&1);wb(0x8342,2);
  ww(0x8368,0x0099);ww(0x836a,0x9999);wb(0x0f38,63);wb(0x0f3a,7);
  for(unsigned n=0;n<6;n++){ww(0x0f16+n*2,0x0010);ww(0x8378+n*2,0x0010);}
  ww(0x0f30,0x1234);ww(0x0f34,0x5678);
  for(unsigned level=0;level<7;level++){
   uint32_t at=0x5080+level*4,original=((uint32_t)sourceWord(at)<<16)|sourceWord(at+2),relocated=0xff4000+original-0x910000;
   ww(0x3040+level*4,relocated>>16);ww(0x3042+level*4,relocated);
  }
  uint16_t d4=0xabcd;uint32_t a4=0xffe000;
  WillowArcadeGameplayServices native={.bus=&bus,.busWord=sourceWord,.collision=&collision,.d4=&d4,.a4=&a4,.videoWrite=symbolWord,.textWrite=symbolWord};
  WillowArcadeShopTask taskState={0};
  for(unsigned sample=0;sample<66;sample++){
   if(sample==6||sample==45)wb(0x0e58,0);
   if(sample==60&&rw(0x8010)==2){ww(0x0f10,0);ww(0x0f12,0);wb(0x0f22,6);}
   wb(0x806b,0);wb(0x806a,(sample==0||sample==2||sample==4||sample==40||sample==60)?16:0);
   for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
   uint16_t beforeD4=d4;uint32_t beforeA4=a4;
   if(willowArcadeShopResume(&taskState,&bus,sourceWord,willowArcadeGameplayServiceResume,&native)!=1||taskState.pc){fprintf(stderr,"Sequence reject episode%u tick%u state%u\n",episode,sample,rw(0x8010));return 1;}
   m68k_set_reg(4,0x5a5a0000UL|beforeD4);m68k_set_reg(12,beforeA4);m68k_set_reg(14,0xffaaaa);RUN(0x33bd6,0xdeadbeef);
   if((uint16_t)m68k_get_reg(4)!=d4||m68k_get_reg(12)!=a4){fprintf(stderr,"Sequence register mismatch episode%u tick%u\n",episode,sample);return 1;}
   for(unsigned n=0;n<65536;n++)if(n<0xfe80||n>=0xff04)if(ram[n^1]!=expected[n]){fprintf(stderr,"Sequence mismatch episode%u tick%u RAM%x got%x want%x\n",episode,sample,n,ram[n^1],expected[n]);return 1;}
   sequenceTicks++;
  }
 }
 for(unsigned bad=0;bad<13;bad++){
  memset(expected,0,sizeof expected);ww(0x800c,2);ww(0x8010,2);ww(0x8012,1);wb(0x8363,4);wb(0x806a,16);
  uint16_t d4=0xabcd;uint32_t a4=0xffe000;
  WillowArcadeGameplayServices native={.bus=&bus,.busWord=sourceWord,.collision=&collision,.d4=&d4,.a4=&a4,.videoWrite=symbolWord,.textWrite=symbolWord};
  if(bad==0)native.textWrite=0;if(bad==1)native.videoWrite=0;if(bad==2)native.collision=0;if(bad==3)native.busWord=0;
  if(bad==4)ww(0x0f10,1);if(bad==5)ww(0x0f10,8);if(bad==6)ww(0x0f12,1);if(bad==7)ww(0x0f12,6);
  if(bad==8)wb(0x0f22+11,28);if(bad==9)wb(0x8366,7);if(bad==10)wb(0x8367,5);
  if(bad==11){ww(0x8010,6);wb(0x0f36,4);}if(bad==12){ww(0x8010,6);wb(0x0f0e,18);}
  memcpy(initial,expected,sizeof initial);WillowArcadeShopTask taskState={0};
  if(willowArcadeShopResume(&taskState,&bus,sourceWord,willowArcadeGameplayServiceResume,&native)||memcmp(initial,expected,sizeof expected)||d4!=0xabcd||a4!=0xffe000){fprintf(stderr,"Negative shop admission%u mutated state\n",bad);return 1;}
  negativeCases++;
 }
 printf("Persistent source shop sequences: %u ticks in46 episodes; repeated payments, sold-out/full limits, all18 paid-message tables, explicit external completion and response countdown; %u invalid/missing bindings refuse without game-RAM/context mutation.\n",sequenceTicks,negativeCases);
 printf("Source shop consumer: %u original68000 closed cases; %u item-menu entries, %u all28-item confirmations, %u direction cases, %u paid-message wait/countdown cases; actual price/payment/effects/HUD/text/task/animation and full non-stack RAM, retained D4.w/A4 match.\n",checks,entries,confirmations,navigation,messageTicks);
 printf("Compiled68000 shop binding: %u cases, exact non-stack RAM/live D4.w/A4, native C callee-save registers, no unexpected video destinations.\n",nativeChecks);
 return 0;
}
