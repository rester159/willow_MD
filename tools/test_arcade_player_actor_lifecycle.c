/* Retained headless original-9EF2 comparison. Inputs and collision map are
 * controlled; visibility queues are drained without rendering. Not a complete
 * frame scheduler or a proof of full campaign progression. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_update.h"
#include "../src/willow_arcade_player_bus.h"
#include "../src/willow_arcade_gameplay_actors.h"
#include "../src/willow_arcade_gameplay_collision.h"
#include "../src/willow_arcade_gameplay_stage0.h"
#include "../src/willow_arcade_stage_objects.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],source[0x100000];
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

static uint8_t expected[65536],scroll2[0x4000];

static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}



static uint16_t sourceWord(unsigned long at){return (at&0xff0000)==0xff0000?rw(at):willowMotionWord(source+at);}
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
static uint16_t inputWord(uint32_t at){return sourceWord(at);}
#endif
static uint8_t*resolve(uint16_t at){return expected+at;}
static void textWrite(uint32_t at,uint16_t v){
 if(at>=0x910000&&at<0x914000){ww(0x4000+at-0x910000,v);return;}
 if(at>=0x91c000&&at<0x91d000){ww(0x2000+at-0x91c000,v);return;}
 fprintf(stderr,"Unmapped graphics %x\n",at);abort();
}

static const WillowVisibilityBus bus={rw,rb,ww,wb};
static void videoWrite(uint32_t at,uint16_t v){
 if(at>=0x918000&&at<0x91c000){willowMotionPutWord(scroll2+(at-0x918000),v);return;}
 fprintf(stderr,"Unmapped video %x\n",at);abort();
}
static void mapPointers(uint8_t*ram,int toCore){
 for(unsigned n=0;n<76;n++){
  uint16_t record=n<60?0xccc6+n*64:0xf042+(n-60)*64;
  /* Released tile records retain their mapped destination at +48. */
  uint16_t at=record+48;uint32_t value=0;
  for(unsigned j=0;j<4;j++)value=(value<<8)|ram[(at+j)^1];
  uint32_t original=n<60?0x918000:0x91c000,mapped=n<60?0x200000:0xff2000;
  uint32_t from=toCore?original:mapped,to=toCore?mapped:original;
  unsigned length=n<60?0x4000:0x1000;
  if(value>=from&&value<from+length){value=value-from+to;for(unsigned j=0;j<4;j++)ram[(at+j)^1]=value>>(24-j*8);}
 }
}
static void discardSprite(uint16_t actor){(void)actor;}
static uint8_t attribute(uint16_t at){return scroll2[at+2];}
static uint16_t paletteWord(uint32_t at){return sourceWord(at);}
static uint8_t paletteByte(uint32_t at){return source[at];}
static uint16_t colorRead(uint32_t at){if(at<0x91c000||at>=0x91d000)abort();return rw(0x2000+at-0x91c000);}
static void colorWrite(uint32_t at,uint16_t v){if(at<0x91c000||at>=0x91d000)abort();ww(0x2000+at-0x91c000,v);}
static uint8_t*allocateWithA4(uint32_t*a4){uint16_t at=willowArcadePaletteFreeWithA4(&bus,a4);return at?resolve(at):NULL;}
static uint8_t*allocate(void){return allocateWithA4(NULL);}
static uint8_t randomByte(void){return willowActor18Random(&bus);}
static void soundWithA4(uint16_t cmd,uint32_t*a4){willowBreakableSoundWithA4(&bus,cmd,a4);}
static void sound(uint16_t cmd){soundWithA4(cmd,NULL);}
static uint32_t *liveA4;
static unsigned allocations,allocationTypes[3];
static unsigned availableEffects(void){return rb(0x97fe);}
static uint8_t*playerAllocate(uint8_t type){
 uint16_t count=type==2?0x97fe:type==8?0x9807:0x980d;
 uint16_t cursor=type==2?0x9816:type==8?0x9828:0x9834;
 if(!rb(count))return NULL;
 uint16_t stack=rw(cursor),at=rw(stack);ww(cursor,stack+2);wb(count,rb(count)-1);
 *liveA4=(uint32_t)(int32_t)(int16_t)at;allocations++;allocationTypes[type==2?0:type==8?1:2]++;return resolve(at);
}
static uint8_t*playerEffect(void){return playerAllocate(2);}
static uint8_t*playerClass12(void){return playerAllocate(12);}
static void playerSound(uint16_t cmd){soundWithA4(cmd,liveA4);}
static void playerCancel(uint16_t at){willowArcadeCancelTaskWithA4(resolve(at),liveA4);}
static uint16_t playerColor(unsigned n){return rw(0x2000+n*2);}
static void playerColorWrite(unsigned n,uint16_t v){ww(0x2000+n*2,v);}
static uint8_t playerUpdate(void*context,uint16_t*d4,uint32_t*a4){
 WillowPlayerCollision*c=context;WillowPlayerRecord p={0};
 WillowPlayerInput i={0};i.availableEffects=availableEffects;i.allocateEffect=playerEffect;i.random=randomByte;i.sound=playerSound;
 WillowPlayerShots shots={0};shots.allocate=playerAllocate;
 WillowPlayerHealth h={0};h.paletteRead=playerColor;h.paletteWrite=playerColorWrite;h.textWrite=textWrite;h.sound=playerSound;h.savedD4=d4;
 WillowPlayerCharge q={0};q.textWrite=textWrite;q.sound=playerSound;
 WillowPlayerScript t={0};t.cancelTask=playerCancel;liveA4=a4;
 return willowArcadeWillowController(&bus,&p,&i,c,&shots,&h,&q,&t,playerClass12);
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(source,rom,sizeof source);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 /* Independent SRAM backs the 16 KiB Scroll2 map without aliasing work RAM. */
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x203fff);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x8474,0x60fe);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 const unsigned patches[]={0x4ad0,0x4ad8,0x4ab2,0x4aba,0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910310,0x910310,0x910314,0x910314,0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}
 for(unsigned n=0;n<2;n++){unsigned at=n?0x1276:0xc2e;if(willowMotionLong(rom+at)!=0x91c000)return 2;willowMotionPutLong(rom+at,0xff2000);}
 willowMotionPutLong(rom+0x3e44,0x200000);willowMotionPutLong(rom+0x4365a,0x200000);
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
 const unsigned playerPatches[]={0xc242,0x4b72,0xea68};
 const uint32_t playerOriginals[]={0x91c000,0x910170,0x91c0e0};
 for(unsigned n=0;n<3;n++){
  if(willowMotionLong(rom+playerPatches[n])!=playerOriginals[n]){fprintf(stderr,"Bad player relocation %x\n",playerPatches[n]);return 2;}
  uint32_t at=playerOriginals[n];willowMotionPutLong(rom+playerPatches[n],at>=0x91c000?0xff2000+at-0x91c000:0xff4000+at-0x910000);
 }
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
 /* Stop before collision processing, after the unmodified prefix returns. */
 if(willowMotionWord(rom+0x955c)!=0x4eb9||willowMotionLong(rom+0x955e)!=0x39a52)return 2;
#ifdef WILLOW_TEST_GAMEPLAY_COLLISION
 if(willowMotionWord(rom+0x9562)!=0x4eb9||willowMotionLong(rom+0x9564)!=0x3807c)return 2;
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
 if(willowMotionWord(rom+0x9568)!=0x4ef9||willowMotionLong(rom+0x956a)!=0x3b524)return 2;
 willowMotionPutWord(rom+0x9568,0x4e75);
 if(willowMotionLong(rom+0x5640)!=0x91056c)return 2;
 willowMotionPutLong(rom+0x5640,0xff456c);
#else
 willowMotionPutWord(rom+0x9562,0x4e75);
#endif
#else
 willowMotionPutWord(rom+0x955c,0x4e75);
#endif
#endif
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/player-actor-lifecycle-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/player-actor-lifecycle-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 uint8_t*mapRam=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!mapRam||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<sizeof scroll2)return 2;
 WillowPaletteAnimation g={0};g.byte=paletteByte;g.word=paletteWord;g.read=colorRead;g.write=colorWrite;
 g.allocate=allocate;g.random=randomByte;g.sound=sound;g.allocateWithA4=allocateWithA4;g.soundWithA4=soundWithA4;
 unsigned checks=0,active=0;
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
 unsigned spawnedUpdates=0,spawnedActiveUpdates=0;
#endif
#ifdef WILLOW_TEST_GAMEPLAY_ENEMIES
 unsigned enemyDamage=0,playerDamage=0;
#endif
 for(unsigned sample=0;sample<16;sample++){
  memset(expected,0,sizeof expected);memset(scroll2,0,sizeof scroll2);
  FILE*map=fopen("build/stage0-collision-window.bin","rb");if(!map||fread(scroll2,1,sizeof scroll2,map)!=sizeof scroll2)return 2;fclose(map);
  willowArcadePoolsInit(&bus);WillowPlayerRecord initial;willowPlayerRecordStage0(&initial,sourceWord);memcpy(resolve(0x9846),initial.bytes,128);
  wb(0x8348,128);wb(0x8362,8);wb(0x8363,8);wb(0x8366,sample%7);
  ww(0x845c,(uint16_t)-64);ww(0x8284,0x1234+sample);wb(0xf99e,1);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);ww(0x801e,sample*2);
  uint16_t d4=sample*977;uint32_t a4=0x123456;WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};WillowArcadeActorUpdate task={0};
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
  WillowArcadeGameplayActors gameplay={0};(void)task;
  /* Real controller selection; mute variants remain covered by direct mode. */
  wb(0x8293,0);wb(0x8292,sample&1);wb(0x828e,3);
  ww(0x83c6,0x0009);wb(0x83c8,sample%4);wb(0xf99e,0);
#endif
#ifdef WILLOW_TEST_GAMEPLAY_ENEMIES
  uint16_t enemies[2];
  for(unsigned n=0;n<2;n++){
   enemies[n]=willowActor18Pop(&bus,0x9801,0x981c);if(!enemies[n])return 2;
   uint8_t*enemy=resolve(enemies[n]);enemy[0]=1;enemy[33]=4;enemy[15]=n;
   willowMotionPutWord(enemy+6,rw(0x984c)+32+n*48);
   willowMotionPutWord(enemy+10,rw(0x9850)+16);
  }
#endif
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
  if(!willowArcadeStageObjects(&bus,resolve,sourceWord))return 2;
  /* First original spawner activates at camera X=224. Hold a controlled
   * camera inside that interval; the camera controller is not integrated. */
  ww(0x8408,256);ww(0x984c,320);
  unsigned duration=1024;
#else
  unsigned duration=256;
#endif
  for(unsigned step=0;step<duration;step++){
   willowArcadeVisibilityRender(&bus,discardSprite,discardSprite);
   wb(0x8019,step);wb(0x806b,rb(0x806a));
   const uint8_t keys[]={0,1,1,0x11,0,0x20,2,0x22};wb(0x806a,keys[(step/16+sample)%8]);
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
   /* Restore old held byte: 1F88, not the fixture, publishes its previous. */
   wb(0x8060,rb(0x806a));wb(0x8062,rb(0x806a));wb(0x806a,rb(0x806b));
#endif
#ifdef WILLOW_TEST_GAMEPLAY_ENEMIES
   uint8_t oldEnemyHp[2]={rb(enemies[0]+42),rb(enemies[1]+42)},oldPlayerHp=rb(0x8362);
#endif
   uint16_t oldD4=d4;uint32_t oldA4=a4;
   for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];memcpy(mapRam,scroll2,sizeof scroll2);mapPointers(ram,1);
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
   uint8_t result=willowArcadeGameplayStage0Resume(
#elif defined(WILLOW_TEST_GAMEPLAY_COLLISION)
   uint8_t result=willowArcadeGameplayCollisionResume(
#else
   uint8_t result=willowArcadeGameplayActorsResume(
#endif
&gameplay,&bus,resolve,sourceWord,inputWord,&g,&collision,videoWrite,textWrite,0,&d4,&a4,playerUpdate,&collision);
#else
   uint8_t result=willowArcadeActorUpdateResumeWithPlayer(&task,&bus,resolve,sourceWord,&g,&collision,videoWrite,textWrite,0,&d4,&a4,playerUpdate,&collision);
#endif
   if(result!=1){fprintf(stderr,"Unsupported sample %u step %u phase %u player %u/%u\n",sample,step,task.phase,rb(0x9848),rb(0x9849));return 1;}
   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(12,oldA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(4,oldD4);m68k_set_reg(16,
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
   0x954e
#else
   0x9ef2
#endif
   );
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04){fprintf(stderr,"Return sample %u step %u PC %x SP %x\n",sample,step,m68k_get_reg(16),m68k_get_reg(15));return 1;}
   mapPointers(ram,0);
   if(memcmp(mapRam,scroll2,sizeof scroll2)){fprintf(stderr,"Scroll2 sample %u step %u\n",sample,step);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){fprintf(stderr,"RAM sample %u step %u at %x actual %x expected %x player %u/%u\n",sample,step,n,ram[n^1],expected[n],rb(0x9848),rb(0x9849));return 1;}
   if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Registers sample %u step %u A4 %x/%x D4 %x/%x\n",sample,step,m68k_get_reg(12),a4,m68k_get_reg(4),d4);return 1;}
#ifdef WILLOW_TEST_GAMEPLAY_ENEMIES
   for(unsigned n=0;n<2;n++)if(oldEnemyHp[n]>rb(enemies[n]+42))enemyDamage++;
   if(oldPlayerHp>rb(0x8362))playerDamage++;
#endif
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
   uint8_t anySpawned=0,anyActive=0;
   for(unsigned n=0;n<50;n++)if(rb(0x9dc6+n*128)){
    anySpawned=1;if(rb(0x9dc8+n*128)==2)anyActive=1;
   }
   spawnedUpdates+=anySpawned;spawnedActiveUpdates+=anyActive;
#endif
   checks++;if(rb(0x9846)&&rb(0x9848)==2)active++;
  }
 }
 if(!allocationTypes[1]||active<3000){fprintf(stderr,"Missing retained player/shot coverage\n");return 1;}
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
#ifdef WILLOW_TEST_GAMEPLAY_COLLISION
 printf("Including collision pass 39A52; ");
#endif
 printf("Including original 954E input/clock prefix; ");
#endif
 printf("Retained 9EF2 player/actor chain: %u updates, %u active Willow updates, %u player allocations; full non-stack RAM, Scroll2, D4/A4 match original with controlled inputs/map and discarded sprite queues\n",checks,active,allocations);
#ifdef WILLOW_TEST_GAMEPLAY_ENEMIES
 printf("Retained enemy damage transitions %u, player HP decreases %u\n",enemyDamage,playerDamage);
 if(!enemyDamage&&!playerDamage){fprintf(stderr,"No combat interaction observed\n");return 1;}
#endif
#ifdef WILLOW_TEST_GAMEPLAY_STAGE0
 printf("Source stage-zero population: %u updates with live class4 actors, %u with actors in active state\n",spawnedUpdates,spawnedActiveUpdates);
 if(!spawnedUpdates||!spawnedActiveUpdates){fprintf(stderr,"No stage-spawned actors observed\n");return 1;}
#endif
 printf("Player allocations: class2 %u, class8 %u, class12 %u\n",allocationTypes[0],allocationTypes[1],allocationTypes[2]);return 0;
}
