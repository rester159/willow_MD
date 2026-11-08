/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "attract_replay_fixture.h"
#include "source_input_fixture.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_attract_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
static uint16_t sourceWord(uint32_t at){return willowMotionWord(rom+at);}
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
 if(argc!=2 && (argc!=3 || (strcmp(argv[2],"--native-fixtures") && strcmp(argv[2],"--native-input-fixtures"))))return 2;
 FILE*fixtures=argc==3&&!strcmp(argv[2],"--native-fixtures")?fopen("build/replay-phase-original.bin","wb"):NULL;
 if(argc==3&&!strcmp(argv[2],"--native-fixtures")&&!fixtures)return 2;
 FILE*inputFixtures=argc==3&&!strcmp(argv[2],"--native-input-fixtures")?fopen("build/source-input-original.bin","wb"):NULL;
 if(argc==3&&!strcmp(argv[2],"--native-input-fixtures")&&!inputFixtures)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Keep these original prefixes at their original addresses, including
  * the indexed PC-relative descriptor lookup. Stop before later services. */
 willowMotionPutWord(rom+0x8d08,0x4e75);
 willowMotionPutWord(rom+0x8e32,0x4e75);
 for(unsigned n=0;n<4;n++){
  unsigned at=0x100800+n*8;
  willowMotionPutWord(rom+at,0x125a);willowMotionPutWord(rom+at+2,n<2?0:n==2?5:65535);
  willowMotionPutWord(rom+at+4,0x34a5);willowMotionPutWord(rom+at+6,n==1?7:0);
 }


 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/attract-state-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-state-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 unsigned checks=0;
 uint8_t expected[65536];
 for(unsigned sample=0;sample<1024;sample++)for(unsigned replay=0;replay<2;replay++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  WillowArcadeFrameClock clock={(uint8_t)sample,(uint8_t)(sample*73)};
  WillowArcadeRandom random={(uint16_t)(sample*977),(uint8_t)(sample^0x5a)};
  WillowArcadeAttractState state={0x1234,0x5678,(uint16_t)(sample<4?0xfffc+sample:sample*997),0xabcd,0x76543210,0xff,0x80};
#define SAVE_FIELDS() do {  expected[0x8018]=clock.interrupt;expected[0x8019]=clock.simulation;  willowMotionPutWord(expected+0x8284,random.state);expected[0x8286]=random.scratch;  willowMotionPutWord(expected+0xe62,state.savedClock);willowMotionPutWord(expected+0xe60,state.savedRandom);  willowMotionPutWord(expected+0x8008,state.manager);willowMotionPutWord(expected+0x8070,state.countdown);  willowMotionPutLong(expected+0x806c,state.inputPointer);  expected[0x9846]=state.playerActive;expected[0xe58]=state.finished; } while(0)
  uint8_t fixture[45];fixture[0]=0;
  SAVE_FIELDS();willowMotionPutWord(expected+0xe5a,replay);
  for(unsigned n=0;n<22;n++)fixture[1+n]=expected[replayFixtureAddresses[n]];
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  willowArcadeAttractBegin(&state,&clock,&random,replay,sourceWord);SAVE_FIELDS();
  RUN(0x8cda,0xdeadbeef);
  for(unsigned n=0;n<65536;n++)if(n<0xfef0 || n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Attract begin mismatch sample %u replay %u address %04x\n",sample,replay,n);return 1;}
  }
  if(fixtures){
   for(unsigned n=0;n<22;n++)fixture[23+n]=ram[replayFixtureAddresses[n]^1];
   if(fwrite(fixture,1,sizeof fixture,fixtures)!=sizeof fixture)return 2;
  }
  /* Emulate intervening changes as inputs to restoration, not as a claim
   * that the playback/gameplay handlers have been translated. */
  clock.interrupt^=0x9b;clock.simulation+=sample;
  random.state^=0xfedc;random.scratch^=0x81;state.playerActive=2;
  SAVE_FIELDS();for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  fixture[0]=1;for(unsigned n=0;n<22;n++)fixture[1+n]=expected[replayFixtureAddresses[n]];
  willowArcadeAttractRestore(&state,&clock,&random);SAVE_FIELDS();
  RUN(0x8e18,0xdeadbeef);
  for(unsigned n=0;n<65536;n++)if(n<0xfef0 || n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Attract restore mismatch sample %u replay %u address %04x\n",sample,replay,n);return 1;}
  }
  if(fixtures){
   for(unsigned n=0;n<22;n++)fixture[23+n]=ram[replayFixtureAddresses[n]^1];
   if(fwrite(fixture,1,sizeof fixture,fixtures)!=sizeof fixture)return 2;
  }
  checks++;
 }
 if(fixtures&&fclose(fixtures))return 2;
 unsigned inputChecks=0;
 for(unsigned replay=0;replay<2;replay++){
  WillowArcadeFrameClock clock={0,0};WillowArcadeRandom random={0,0};
  WillowArcadeAttractState state={0};
  willowArcadeAttractBegin(&state,&clock,&random,replay,sourceWord);
  WillowArcadeInputMux input={0,0,1,0,0,0,0};
  for(unsigned tick=0;tick<1040;tick++){
   for(unsigned n=0;n<65536;n++)expected[n]=(tick+n*3)^0x5a;
   /* Include ordinary input routing, then consecutive replay playback. */
   if(tick<128){
    input.replayEnabled=0;input.secondPlayer=tick&1?0x80:0;
    input.cabinet=tick>>1;input.p1=tick*3;input.p2=~tick;
   }else input.replayEnabled=1;
   if(tick>=1024){
    const uint16_t countdowns[]={0,1,2,65535};
    state.countdown=countdowns[(tick-1024)/4];
    state.inputPointer=0x100800+((tick-1024)%4)*8;
   }
#define SAVE_INPUT() do {  expected[0x806a]=input.current;expected[0x806b]=input.previous;  expected[0x8293]=input.replayEnabled;expected[0x8292]=input.secondPlayer;  expected[0x828e]=input.cabinet;expected[0x8060]=input.p1;expected[0x8062]=input.p2;  willowMotionPutWord(expected+0x8070,state.countdown);willowMotionPutLong(expected+0x806c,state.inputPointer); } while(0)
   SAVE_INPUT();for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
   uint8_t inputFixture[26];
   for(unsigned n=0;n<13;n++)inputFixture[n]=expected[inputFixtureAddresses[n]];
   willowArcadeInputMuxStep(&input,&state,sourceWord);SAVE_INPUT();
   RUN(0x1f88,0xdeadbeef);
   for(unsigned n=0;n<65536;n++)if(n<0xfef0 || n>=0xff04){
    if(ram[n^1]!=expected[n]){fprintf(stderr,"Input mux mismatch replay %u tick %u address %04x\n",replay,tick,n);return 1;}
   }
   if(inputFixtures&&tick<1024){
    for(unsigned n=0;n<13;n++)inputFixture[13+n]=ram[inputFixtureAddresses[n]^1];
    if(fwrite(inputFixture,1,sizeof inputFixture,inputFixtures)!=sizeof inputFixture)return 2;
   }
   inputChecks++;
  }
 }
 if(inputFixtures&&fclose(inputFixtures))return 2;
 printf("Source input mux: %u live-input/replay updates using both original streams, full non-stack RAM compared\n",inputChecks);
 printf("Source attract clock/RNG lifetime: %u entry/restore pairs, both original replay descriptors, non-stack RAM preserved\n",checks);
 return 0;
}
