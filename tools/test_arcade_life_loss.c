/* Complete life-loss handler with original service bodies and RAM comparison. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../native/engine/src/life_loss.h"
#include "../native/engine/tools/player_status_oracle.h"
#include <assert.h>
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
static uint16_t sourceWord(uint32_t a){return willowMotionWord(rom+a);}
/* Observe ordering, then call the real semantic player-status exchange. */
typedef struct {
 WillowNativeLifeLoss*binding;WillowNativeGameplayRouteState*route;
 WillowNativePlayerStatusBank*bank;uint8_t player,orientation;unsigned calls;
} Exchange;
static int exchange(void*context){
 Exchange*x=context;assert(!x->route->mode&&!x->route->step&&!x->route->substep);
 assert(x->binding->hud->secondPlayer==x->player&&*x->binding->displayOrientation==x->orientation);
 x->calls++;return willowNativePlayerStatusExchange(x->bank);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/life-loss-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/life-loss-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0,swaps=0;
 for(unsigned phase=0;phase<2;phase++)for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31+(n>>7));
  ww(0x8008,phase*2);ww(0x8000,(sample&8)?0xffff:2);
  wb(0x8342,sample);wb(0x8346,sample);wb(0x82c2,(sample&256)?0x99:0);
  wb(0x82a5,(sample&512)?0x80:0);wb(0x828e,(sample&1024)?2:0);
  wb(0x8290,(sample&2048)?1:0);wb(0x8293,(sample&2048)?1:0);
  ww(0x801e,(sample&63)*2);
  wb(0x8386,(sample&1)?4:0);wb(0x8306,(sample&2)?4:0);
  TestPlayerStatusOwners owners;testPlayerStatusOwners(&owners,expected);
  WillowNativeGameplayRouteState route={.mode=rw(0x8004),.step=rw(0x8008),.substep=rw(0x800c),.timer=0xa55a,.eventStep=0x1234};
  uint8_t alternate=rb(0x82a5),cabinet=rb(0x828e),orientation=rb(0x8294);
  WillowNativeSoundQueue sound={.next=rw(0x801e)/2,.gateA=1,.gateB=1};
  for(unsigned n=0;n<64;n++)sound.commands[n]=rw(0x8172+n*2);
  WillowNativeLifeLoss native={&owners.hud,&owners.difficulty,&sound,&alternate,&owners.bank.other.lives,&cabinet,&orientation,0,exchange};
  Exchange swap={&native,&route,&owners.bank,(uint8_t)(owners.hud.secondPlayer^1),(uint8_t)(orientation^((cabinet&2)?128:0)),0};native.context=&swap;
  unsigned mustSwap=phase&&owners.hud.lives&&alternate&&owners.bank.other.lives;
  if(mustSwap){
   WillowNativeGameplayRouteState beforeRoute=route;TestPlayerStatusOwners beforeOwners=owners;
   WillowNativeSoundQueue beforeSound=sound;uint8_t beforeOrientation=orientation;
   native.exchangePlayers=0;assert(!willowNativeLifeLossStep(&native,&owners.campaign,&route));native.exchangePlayers=exchange;
   assert(!memcmp(&route,&beforeRoute,sizeof route)&&!memcmp(&owners,&beforeOwners,sizeof owners)&&
    !memcmp(&sound,&beforeSound,sizeof sound)&&orientation==beforeOrientation);
  }
  if(phase&&rb(0x8342)&&rb(0x82a5)&&rb(0x82c2))swaps++;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t a4=0xff6780;uint16_t d4=0x3210;
  WillowArcadeGameplayServices bindings={0};bindings.bus=&bus;bindings.busWord=sourceWord;bindings.a4=&a4;bindings.d4=&d4;
  if(willowArcadeGameplayServiceResume(&bindings,0x96be,-1)!=1||d4!=0x3210)return 1;
  assert(willowNativeLifeLossStep(&native,&owners.campaign,&route));
  assert(owners.campaign.mode==rw(0x8000)&&route.mode==rw(0x8004)&&route.step==rw(0x8008)&&route.substep==rw(0x800c));
  assert(owners.hud.secondPlayer==rb(0x8292)&&orientation==rb(0x8294)&&swap.calls==mustSwap);
  assert(route.timer==0xa55a&&route.eventStep==0x1234);
  WillowNativePlayerStatus current,reference=testPlayerStatus(expected,0x8342),other=testPlayerStatus(expected,0x82c2);
  assert(willowNativePlayerStatusRead(&owners.bank.current,&current));
  assert(!memcmp(&current,&reference,sizeof current)&&!memcmp(&owners.bank.other,&other,sizeof other));
  testPlayerStatusRetained(&owners,expected);
  assert(sound.next==rw(0x801e)/2&&sound.gateA==1&&sound.gateB==1);
  for(unsigned n=0;n<64;n++)assert(sound.commands[n]==rw(0x8172+n*2));
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(12,0xff6780);m68k_set_reg(4,0x76543210);m68k_set_reg(16,0x96be);retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(12)!=a4||m68k_get_reg(4)!=0x76543210||m68k_get_reg(15)!=0xffff04)return 1;
  for(unsigned n=0;n<65536;n++)if((n<0xfe80||n>=0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Life loss mismatch phase %u sample %u at %x got %x want %x\n",phase,sample,n,ram[n^1],expected[n]);return 1;
  }
  assert(willowNativePlayerStatusClone(&owners.bank)&&!memcmp(&owners.bank.other,&current,sizeof current));
  assert(!memcmp(owners.hud.otherScore,current.score,4));
  checks++;
 }
 printf("Life-loss handler: %u original CPU/source/native comparisons and save clones, %u real semantic player exchanges; persistent health/inventory/equipment/checkpoint/camera/economy/score/campaign fields, sound ring and exchange ordering match; unrelated native fields retained. Source full non-stack RAM also matches.\n",checks,swaps);
 return 0;
}
