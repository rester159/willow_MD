/* Differential oracle: original ROM instructions execute in the installed
 * 68000 core. Only the host test uses emulation; the port routines are C. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_update.h"
#ifdef WILLOW_TEST_DISPATCH
#include "../src/willow_arcade_player_dispatch.h"
#include "../src/willow_arcade_player_bus.h"
#define ROUTE_COUNT 24
#else
#define ROUTE_COUNT 9
#endif
#include "../src/willow_arcade_services.h"
#include "../native/intro_probe/src/player_data.h"
#include "../src/willow_arcade_actor_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],originalProgram[0x100000];
static uint16_t sourceWord(unsigned long a){
 uint16_t value;
 if(!nativePlayerDataRead(a,&value)){fprintf(stderr,"Missing native player data at %lx\n",a);abort();}
 if(a+1>=sizeof rom || value!=willowMotionWord(originalProgram+a)){fprintf(stderr,"Native player data differs at %lx\n",a);abort();}
 return value;
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
#ifdef WILLOW_TEST_PLAYER_BUS
static uint8_t busRam[65536];
#ifdef WILLOW_TEST_PLAYER_RECORD_IO
static void loadRecord(WillowPlayerRecord*p){memcpy(p->bytes,busRam+0x9846,128);}
static void storeRecord(const WillowPlayerRecord*p){memcpy(busRam+0x9846,p->bytes,128);}
#endif
#ifdef WILLOW_TEST_PLAYER_REGISTERS
static uint32_t playerA4;static uint16_t playerD4;
#endif
static uint16_t busWord(uint16_t at){return willowMotionWord(busRam+at);}
static uint8_t busByte(uint16_t at){return busRam[at];}
static void busPutWord(uint16_t at,uint16_t v){willowMotionPutWord(busRam+at,v);}
static void busPutByte(uint16_t at,uint8_t v){busRam[at]=v;}
#endif
static uint8_t map[16384];
static uint8_t attribute(uint16_t offset){return map[offset+2];}
static uint32_t seed=0x57494c4c;
static uint32_t randomWord(void){seed=seed*1664525u+1013904223u;return seed;}
static uint8_t records[3][9][64];
static WillowActorPoolIndex poolIndices[3][9];
static WillowActorPool pools[3];
static WillowArcadeRandom sharedRandom;
static WillowArcadeSoundQueue sharedSounds;
#ifdef WILLOW_TEST_DISPATCH
static uint8_t cancelledTask[32];
static unsigned cancelCount;
static void cancelTask(uint16_t at){if(at!=0xc0)abort();cancelCount++;
#ifdef WILLOW_TEST_SHARED_PLAYER
#ifdef WILLOW_TEST_PLAYER_REGISTERS
willowArcadeCancelTaskWithA4(busRam+at,&playerA4);
#else
willowArcadeCancelTask(busRam+at);
#endif
#else
willowArcadeCancelTask(cancelledTask);
#endif
}
#endif
#ifdef WILLOW_TEST_SHARED_PLAYER
static uint16_t colors[32];static uint8_t textRam[16384];
static unsigned availableEffects(void){return busByte(0x97fe);}
static uint8_t*allocatePool(uint8_t type){
 unsigned count=type==2?0x97fe:type==8?0x9807:0x980d;
 unsigned cursor=type==2?0x9816:type==8?0x9828:0x9834;
 if(!busByte(count))return NULL;
 uint16_t stack=busWord(cursor),at=busWord(stack);
 busPutWord(cursor,stack+2);busPutByte(count,busByte(count)-1);
#ifdef WILLOW_TEST_PLAYER_REGISTERS
 playerA4=(uint32_t)(int32_t)(int16_t)at;
#endif
 return busRam+at;
}
static uint8_t*allocateEffect(void){return allocatePool(2);}
static uint8_t*allocateClass12(void){return allocatePool(12);}
static uint8_t nextRandom(void){
 WillowArcadeRandom r={busWord(0x8284),busByte(0x8286)};uint8_t v=willowArcadeRandomNext(&r);
 busPutWord(0x8284,r.state);busPutByte(0x8286,r.scratch);return v;
}
static void sound(uint16_t command){
 if(busByte(0x8290)&&busByte(0x8293))return;
#ifdef WILLOW_TEST_PLAYER_REGISTERS
 playerA4=0xff8172;
#endif
 uint16_t cursor=busWord(0x801e);busPutWord(0x8172+cursor,command);cursor=(cursor+2)&126;
 busPutWord(0x8172+cursor,255);busPutWord(0x801e,(cursor+2)&126);
}
static uint16_t paletteRead(unsigned n){return busWord(0x2000+n*2);}
static void paletteWrite(unsigned n,uint16_t v){busPutWord(0x2000+n*2,v);}
static void textWrite(uint32_t at,uint16_t v){if(at<0x910000||at+1>=0x914000)abort();busPutWord(0xa000+at-0x910000,v);}
#else
static unsigned availableEffects(void){return pools[0].available;}
static uint8_t *allocatePool(uint8_t type){unsigned p=type==2?0:type==8?1:2;return willowActorPoolAllocate(pools+p);}
static uint8_t *allocateEffect(void){return allocatePool(2);}
static uint8_t *allocateClass12(void){return allocatePool(12);}
static uint8_t nextRandom(void){return willowArcadeRandomNext(&sharedRandom);}
static void sound(uint16_t command){willowArcadeSoundCommand(&sharedSounds,command,0);}
static uint16_t colors[32];static uint8_t textRam[16384];
static uint16_t paletteRead(unsigned n){return colors[n];}
static void paletteWrite(unsigned n,uint16_t value){colors[n]=value;}
static void textWrite(uint32_t at,uint16_t value){if(at<0x910000 || at+1>=0x914000)abort();willowMotionPutWord(textRam+(at-0x910000),value);}
#endif
static void putRamWord(uint8_t*ram,unsigned at,uint16_t value){ram[at^1]=value>>8;ram[(at+1)^1]=value;}
static unsigned ramWord(uint8_t*ram,unsigned at){return (ram[at^1]<<8)|ram[(at+1)^1];}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(originalProgram,rom,sizeof originalProgram);
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
#ifdef WILLOW_TEST_DISPATCH
 const unsigned chargePatches[]={0x4b72,0x4fc6,0x5012,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t chargeOriginals[]={0x910170,0x910170,0x910a70,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<10;n++){if(willowMotionLong(rom+chargePatches[n])!=chargeOriginals[n])return 2;willowMotionPutLong(rom+chargePatches[n],0xffa000+chargeOriginals[n]-0x910000);}
#endif
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/update-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/update-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0,spawns=0,types[13]={0},routeChecks[ROUTE_COUNT]={0};
 const unsigned bases[]={0xe000,0xe400,0xe800},counts[]={0x97fe,0x9807,0x980d},stacks[]={0x9100,0x9200,0x9300},cursors[]={0x9816,0x9828,0x9834};
 f=fopen("build/stage0-collision-window.bin","rb");if(!f || fread(map,1,sizeof map,f)!=sizeof map)return 2;fclose(f);
 for(unsigned n=0;n<sizeof map;n++)ram[(0x4000+n)^1]=map[n];
 for(unsigned route=0;route<ROUTE_COUNT;route++)for(unsigned sample=0;sample<4096;sample++){
  uint8_t native[128];for(unsigned n=0;n<128;n++)native[n]=randomWord()>>24;
  native[0]=1;native[2]=2;native[3]=route<6?route*2:0;native[4]=(sample%(route==3 || route==5?5:route==4?7:3))*2;native[5]=((sample/3)%3)*2;
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
   pools[p]=(WillowActorPool){&records[p][0][0],poolIndices[p],9,64,0,0};
   for(unsigned n=0;n<9;n++)poolIndices[p][n]=n;
   initial[p]=pools[p].available=(sample>>(p*2))%(p==0?5:p==1?4:10);pools[p].cursor=0;
   ram[counts[p]^1]=pools[p].available;putRamWord(ram,cursors[p],stacks[p]);
   for(unsigned n=0;n<9;n++){
    putRamWord(ram,stacks[p]+n*2,bases[p]+n*64);
    for(unsigned j=0;j<64;j++){records[p][n][j]=randomWord()>>24;ram[(bases[p]+n*64+j)^1]=records[p][n][j];}
   }
  }
  sharedRandom.state=randomWord();sharedRandom.scratch=sample&255;memset(&sharedSounds,0,sizeof sharedSounds);
  uint16_t threshold=(uint16_t)(sample&1?-64:80),cameraX=(sample/32)%64;uint8_t rebound=(sample/64)%2;
  uint8_t mode=route<6 || route>=9?0:route-5;
  WillowPlayerHealth health={0};
  health.hp=(sample/8)%9;health.maxHp=8;health.armor=(sample/72)%4;
  health.cachedHp=(health.hp+(sample&1))%9;health.cachedArmor=(health.armor+((sample/2)&1))%4;
  if(!health.hp && !health.armor)health.cachedArmor=0;
  health.frame=inputs.frame;health.rebound=rebound;
  for(unsigned n=0;n<12;n++)health.items[n]=(sample>>(n%8))%4;
  health.paletteRead=paletteRead;health.paletteWrite=paletteWrite;health.textWrite=textWrite;health.sound=sound;
  const uint16_t healthTimers[]={0,0,0,1,2,128,65535};
  willowMotionPutWord(native+80,healthTimers[(sample/192)%7]);willowMotionPutWord(native+54,healthTimers[(sample/32)%7]);
  native[78]=(sample/16)%6;native[79]=sample/96;
  for(unsigned n=0;n<sizeof textRam;n++){textRam[n]=randomWord()>>24;ram[(0xa000+n)^1]=textRam[n];}
  for(unsigned n=0;n<32;n++){colors[n]=randomWord()>>8;putRamWord(ram,0x2000+n*2,colors[n]);}
  for(unsigned n=0;n<12;n++)ram[(0x8352+n)^1]=health.items[n];
  ram[0x8362^1]=health.hp;ram[0x8363^1]=health.maxHp;ram[0x8364^1]=health.armor;
  ram[0x8365^1]=health.cachedHp;ram[0x8389^1]=health.cachedArmor;
  putRamWord(ram,0x845c,threshold);putRamWord(ram,0x8408,cameraX);ram[0xf99e^1]=rebound;ram[0x83ea^1]=mode;
#ifdef WILLOW_TEST_DISPATCH
  WillowPlayerCharge charge={.value=(uint16_t)(randomWord()%(sourceWord(0x4c08+shots.level*2)+1)),
    .active=(sample%3)?1:0,.flash=(uint8_t)randomWord(),.mode=mode,.level=shots.level,
    .alternateLevel=0,.textWrite=textWrite,.sound=sound};
  uint8_t gate=(sample&1)?0x80:0;
  WillowPlayerScript script={.scriptPointer=0x47e8eUL+(sample%124)*4,.stage=(sample/32)%7,
   .scriptTimer=(sample%3)?1:0,.phase=(sample%3)*2,.cameraReady=(sample&2)?0xff:0,
   .recoveryDelay=(sample&4)?0x80:0,.paused=(sample&8)?1:0,.flags=(sample/16)&3,
   .signal=(uint8_t)sample,.previous=inputs.held^0x3f,.removed=(uint8_t)sample,.cancelTask=cancelTask};
  if(sample&16)script.scriptTimer=2;
  cancelCount=0;
  for(unsigned n=0;n<32;n++){cancelledTask[n]=randomWord()>>24;ram[(0xc0+n)^1]=cancelledTask[n];}
  if(sample&1)cancelledTask[0]=ram[0xc0^1]=0;
  if(route>=9 && route<16){
   native[2]=route<12?0:4;native[3]=route<12?(route-9)*2:(route-12)*2;
   if(route<12)willowMotionPutWord(native+34,(sample%5)?sample%17:65535);
   else {
    const uint8_t deathTimers[]={0,1,2,30,40,255};native[34]=deathTimers[sample%6];
    uint32_t table=route<14?0xd436:0xd08e;
    willowPlayerAnimationSet(native,table+(int16_t)sourceWord(table+(native[15]&1)*2),sourceWord);
    if(sample&1)willowMotionPutWord(native+28,1);
   }
  }
  if(route>=16){
   native[2]=route==16?6:route<20?(route-13)*2:14;
   native[3]=route<20?(sample%6)*2:(route-20)*2;
   native[4]=0;native[5]=0;
   if(route>=20){const uint8_t arrivalTimers[]={0,1,2,96,255};native[34]=arrivalTimers[sample%5];willowMotionPutWord(native+58,(sample%7)*0x3000);}
   if(route==18 && (sample&16))cameraX=(uint16_t)(0x7fff+(sample%256)-192);
   putRamWord(ram,0x8408,cameraX);
  }
  putRamWord(ram,0x8344,script.stage);putRamWord(ram,0xf892,script.scriptTimer);
  putRamWord(ram,0xf894,script.scriptPointer>>16);putRamWord(ram,0xf896,script.scriptPointer);
  ram[0x83d1^1]=script.phase;ram[0x8470^1]=script.cameraReady;
  ram[0x838a^1]=script.recoveryDelay;ram[0x82a6^1]=script.paused;
  ram[0x83c3^1]=script.flags;ram[0xf99c^1]=script.signal;ram[0x806b^1]=script.previous;ram[0x83c2^1]=script.removed;
  putRamWord(ram,0x8376,charge.value);ram[0x83ca^1]=charge.active;ram[0x0a3c^1]=charge.flash;
  ram[0x8367^1]=charge.alternateLevel;ram[0x8348^1]=gate;
  inputs.chargeLevel=charge.value>>8;
#endif
  for(unsigned n=0;n<128;n++)ram[(0x9846+n)^1]=native[n];
  ram[0x806a^1]=inputs.held;ram[0x0e92^1]=inputs.pressed;ram[0x8019^1]=inputs.frame;
  ram[0x83dc^1]=inputs.alternate;ram[0x8376^1]=inputs.chargeLevel;ram[0x0e94^1]=inputs.chargedShot;
  ram[0x8366^1]=shots.level;ram[0x0e93^1]=shots.selected;ram[0xf99d^1]=shots.activeNormal;
  ram[0x0e58^1]=shots.blockA;ram[0x83cf^1]=shots.blockB;
  putRamWord(ram,0x8284,sharedRandom.state);ram[0x8286^1]=sharedRandom.scratch;putRamWord(ram,0xa32,0);putRamWord(ram,0xa34,0);
#ifdef WILLOW_TEST_SHARED_PLAYER
  ram[0x8290^1]=sample&1;ram[0x8293^1]=(sample>>1)&1;putRamWord(ram,0x801e,(sample*2)&126);
#else
  ram[0x8290^1]=0;ram[0x8293^1]=0;putRamWord(ram,0x801e,0);
#endif
  for(unsigned n=0;n<128;n++)ram[(0x8172+n)^1]=0;
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff9846);m68k_set_reg(15,0xffff00);for(unsigned n=0;n<8;n++)m68k_set_reg(n,0);
#ifdef WILLOW_TEST_DISPATCH
  m68k_set_reg(16,0xbf42);
  WillowPlayerRecord record;memcpy(record.bytes,native,sizeof native);
#ifdef WILLOW_TEST_PLAYER_BUS
#ifdef WILLOW_TEST_PLAYER_REGISTERS
  playerA4=sample&1?0x123456:0xffb000;playerD4=sample*977;health.savedD4=&playerD4;
  m68k_set_reg(12,playerA4);m68k_set_reg(4,playerD4);
#endif
  const WillowVisibilityBus playerBus={busWord,busByte,busPutWord,busPutByte};
  for(unsigned n=0;n<65536;n++)busRam[n]=ram[n^1];
#ifdef WILLOW_TEST_PLAYER_CONTROLLER
  busRam[0x828f]=ram[0x828f^1]=(sample>>3)&1;
  busRam[0x8066]=ram[0x8066^1]=(sample&1)?128:0;
  m68k_set_reg(16,0xbef8);
#ifdef WILLOW_TEST_PLAYER_RECORD_IO
  int ok=willowArcadeWillowControllerWithRecordIO(&playerBus,
#else
  int ok=willowArcadeWillowController(&playerBus,
#endif
#else
#ifdef WILLOW_TEST_PLAYER_RECORD_IO
  int ok=willowArcadeWillowDispatchWithRecordIO(&playerBus,
#else
  int ok=willowArcadeWillowDispatch(&playerBus,
#endif
#endif
   &record,&inputs,&collision,&shots,&health,&charge,&script,allocateClass12
#ifdef WILLOW_TEST_PLAYER_RECORD_IO
   ,loadRecord,storeRecord
#endif
   );
#else
  int ok=willowPlayerDispatch(&record,&inputs,&collision,&shots,&health,&charge,&script,gate,threshold,cameraX,allocateClass12);
#endif
#ifdef WILLOW_TEST_SHARED_PLAYER
  /* Mirrors feed the older targeted assertions only. Full shared RAM is
   * independently compared after source execution below. */
  memcpy(cancelledTask,busRam+0xc0,sizeof cancelledTask);
  for(unsigned z=0;z<3;z++){
   pools[z].available=busByte(counts[z]);pools[z].cursor=(busWord(cursors[z])-stacks[z])/2;
   memcpy(records[z],busRam+bases[z],sizeof records[z]);
  }
  for(unsigned z=0;z<32;z++)colors[z]=busWord(0x2000+z*2);
  memcpy(textRam,busRam+0xa000,sizeof textRam);
  sharedRandom.state=busWord(0x8284);sharedRandom.scratch=busByte(0x8286);
  sharedSounds.writeOffset=busWord(0x801e);
  for(unsigned z=0;z<64;z++)sharedSounds.words[z]=busWord(0x8172+z*2);
#endif
  memcpy(native,record.bytes,sizeof native);
#else
  m68k_set_reg(16,0xc13e);
  int ok=willowPlayerNormalUpdate(native,&inputs,&collision,&shots,&health,mode,threshold,cameraX,allocateClass12);
#endif
  if(!ok){fprintf(stderr,"Unsupported route %u case %u level %u selected %u phase %u attackphase %u\n",route,sample,shots.level,shots.selected,native[4],native[5]);return 2;}
  retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Oracle route %u case %u failed to return: %x\n",route,sample,m68k_get_reg(16));return 1;}
  for(unsigned n=0;n<128;n++)if(native[n]!=ram[(0x9846+n)^1]){fprintf(stderr,"Route %u case %u player byte %u native %02x source %02x\n",route,sample,n,native[n],ram[(0x9846+n)^1]);return 1;}
#ifdef WILLOW_TEST_DISPATCH
#ifdef WILLOW_TEST_PLAYER_BUS
  /* Check every RAM field owned by the adapter; services retain the separate
   * pool/HUD/RNG/sound assertions below. */
  const uint16_t adapterBytes[]={0x806a,0x0e92,0x8019,0x83dc,0x0e94,0x8366,0x0e93,0xf99d,0x0e58,0x83cf,0x8362,0x8363,0x8364,0x8365,0x8389,0xf99e,0x83ca,0x0a3c,0x83ea,0x8367,0x83d1,0x8470,0x838a,0x82a6,0x83c3,0xf99c,0x806b,0x83c2};
  for(unsigned n=0;n<sizeof adapterBytes/sizeof adapterBytes[0];n++){unsigned at=adapterBytes[n];if(busRam[at]!=ram[at^1]){fprintf(stderr,"Bus byte route %u case %u at %x\n",route,sample,at);return 1;}}
  const uint16_t adapterWords[]={0x8376,0x8344,0xf892,0x0a32,0x0a34,0xf894,0xf896};
  for(unsigned n=0;n<sizeof adapterWords/sizeof adapterWords[0];n++)if(busWord(adapterWords[n])!=ramWord(ram,adapterWords[n]))return 1;
  for(unsigned n=0;n<128;n++)if(busRam[0x9846+n]!=ram[(0x9846+n)^1])return 1;
  for(unsigned n=0;n<12;n++)if(busRam[0x8352+n]!=ram[(0x8352+n)^1])return 1;
#endif
  const uint8_t actualScript[]={script.phase,script.cameraReady,script.recoveryDelay,script.paused,script.flags,script.signal,script.previous,script.removed,inputs.held,inputs.pressed};
  const unsigned scriptAddresses[]={0x83d1,0x8470,0x838a,0x82a6,0x83c3,0xf99c,0x806b,0x83c2,0x806a,0x0e92};
  for(unsigned n=0;n<sizeof actualScript;n++)if(actualScript[n]!=ram[scriptAddresses[n]^1]){fprintf(stderr,"Script route %u case %u global %x native %x source %x\n",route,sample,scriptAddresses[n],actualScript[n],ram[scriptAddresses[n]^1]);return 1;}
  if(script.scriptTimer!=ramWord(ram,0xf892) || script.scriptPointer!=((uint32_t)ramWord(ram,0xf894)<<16|ramWord(ram,0xf896)))return 1;
  for(unsigned n=0;n<32;n++)if(cancelledTask[n]!=ram[(0xc0+n)^1]){fprintf(stderr,"Task cancel route %u case %u byte %u\n",route,sample,n);return 1;}
  if(cancelCount!=(route==16))return 1;
  if(charge.value!=ramWord(ram,0x8376) || charge.active!=ram[0x83ca^1] || charge.flash!=ram[0x0a3c^1]){fprintf(stderr,"Charge route %u case %u value %x/%x active %u/%u flash %u/%u\n",route,sample,charge.value,ramWord(ram,0x8376),charge.active,ram[0x83ca^1],charge.flash,ram[0x0a3c^1]);return 1;}
#endif
  for(unsigned p=0;p<3;p++){
   if(pools[p].available!=ram[counts[p]^1] || ramWord(ram,cursors[p])!=stacks[p]+pools[p].cursor*2){fprintf(stderr,"Pool %u route %u case %u count/cursor mismatch\n",p,route,sample);return 1;}
   for(unsigned n=0;n<9;n++)for(unsigned j=0;j<64;j++)if(records[p][n][j]!=ram[(bases[p]+n*64+j)^1]){fprintf(stderr,"Pool %u record %u byte %u route %u case %u mismatch\n",p,n,j,route,sample);return 1;}
   spawns+=initial[p]-pools[p].available;
  }
  for(unsigned n=0;n<12;n++)if(health.items[n]!=ram[(0x8352+n)^1])return 1;
  if(health.hp!=ram[0x8362^1] || health.armor!=ram[0x8364^1] || health.cachedHp!=ram[0x8365^1] || health.cachedArmor!=ram[0x8389^1]){fprintf(stderr,"Health globals route %u case %u\n",route,sample);return 1;}
  for(unsigned n=0;n<32;n++)if(colors[n]!=ramWord(ram,0x2000+n*2)){fprintf(stderr,"Palette route %u case %u color %u\n",route,sample,n);return 1;}
  for(unsigned n=0;n<sizeof textRam;n++)if(textRam[n]!=ram[(0xa000+n)^1]){fprintf(stderr,"HUD route %u case %u byte %u\n",route,sample,n);return 1;}
  if(sharedRandom.state!=ramWord(ram,0x8284) || sharedRandom.scratch!=ram[0x8286^1] || inputs.chargedShot!=ram[0x0e94^1] || shots.selected!=ram[0x0e93^1] || shots.activeNormal!=ram[0xf99d^1]){fprintf(stderr,"Global mismatch route %u case %u\n",route,sample);return 1;}
  if(ramWord(ram,0x801e)!=sharedSounds.writeOffset)return 1;
  for(unsigned n=0;n<64;n++)if(ramWord(ram,0x8172+n*2)!=sharedSounds.words[n])return 1;
  if(collision.x!=ramWord(ram,0xa32) || collision.y!=ramWord(ram,0xa34)){fprintf(stderr,"Collision coordinates mismatch route %u case %u\n",route,sample);return 1;}
#ifdef WILLOW_TEST_SHARED_PLAYER
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&busRam[n]!=ram[n^1]){
   fprintf(stderr,"Shared RAM route %u case %u at %x native %x source %x\n",route,sample,n,busRam[n],ram[n^1]);return 1;
  }
#endif
#ifdef WILLOW_TEST_PLAYER_REGISTERS
  if(m68k_get_reg(12)!=playerA4||(uint16_t)m68k_get_reg(4)!=playerD4){fprintf(stderr,"Registers route %u case %u A4 %x/%x D4 %x/%x\n",route,sample,m68k_get_reg(12),playerA4,m68k_get_reg(4)&65535,playerD4);return 1;}
#endif
  if(shots.selected<13)types[shots.selected]++;routeChecks[route]++;checks++;
 }
#ifdef WILLOW_TEST_DISPATCH
 printf("Player dispatcher BF42 all eight Willow states: %u cases, %u allocations match original state, pools, HUD, charge, palettes and sound\n",checks,spawns);
#else
 printf("Full normal-player update C13E: %u cases, %u allocations match original state, pools, HUD, palettes and sound\n",checks,spawns);
#endif
 for(unsigned n=0;n<ROUTE_COUNT;n++)printf("Route %u: %u cases\n",n,routeChecks[n]);return 0;
}
