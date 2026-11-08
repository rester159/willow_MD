/* Headless original-9F34 comparison across transition waits and stage reloads.
 * Scroll2 uses independent cartridge SRAM; text/palette pages use reserved
 * fixture RAM. Graphics pointers are translated in both directions, including
 * destinations retained after actor release. Fade completion and collision
 * map contents are controlled; sprite queues are drained without rendering.
 * Sleep interception retains original caller frames, not a real scheduler. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor_update.h"
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
#include "../src/willow_arcade_player_bus.h"
#include "../src/willow_arcade_gameplay_actors.h"
#include "../src/willow_arcade_gameplay_collision.h"
#endif
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
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
static uint16_t inputWord(uint32_t at){return sourceWord(at);}
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
#endif
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
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
 const unsigned playerPatches[]={0xc242,0x4b72,0xea68};
 const uint32_t playerOriginals[]={0x91c000,0x910170,0x91c0e0};
 for(unsigned n=0;n<3;n++){
  if(willowMotionLong(rom+playerPatches[n])!=playerOriginals[n])return 2;
  uint32_t at=playerOriginals[n];willowMotionPutLong(rom+playerPatches[n],at>=0x91c000?0xff2000+at-0x91c000:0xff4000+at-0x910000);
 }
 if(willowMotionWord(rom+0x955c)!=0x4eb9||willowMotionLong(rom+0x955e)!=0x39a52)return 2;
#ifdef WILLOW_TEST_GAMEPLAY_COLLISION
 if(willowMotionWord(rom+0x9562)!=0x4eb9||willowMotionLong(rom+0x9564)!=0x3807c)return 2;
 willowMotionPutWord(rom+0x9562,0x4e75);
#else
 willowMotionPutWord(rom+0x955c,0x4e75);
#endif
#endif
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor-update-resume-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor-update-resume-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 uint8_t*mapRam=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!mapRam||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<sizeof scroll2)return 2;
 WillowPaletteAnimation g={0};g.byte=paletteByte;g.word=paletteWord;g.read=colorRead;g.write=colorWrite;
 g.allocate=allocate;g.random=randomByte;g.sound=sound;g.allocateWithA4=allocateWithA4;g.soundWithA4=soundWithA4;
 unsigned checks=0;

 unsigned waits=0,reloads=0,reveals=0;
 for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  for(unsigned n=0;n<sizeof scroll2;n++)scroll2[n]=(n*7+sample*31)^0xa5;
  willowArcadePoolsInit(&bus);uint16_t actor=0xdbc6+30*64;
  uint8_t*r=resolve(actor);r[0]=1;r[32]=2;r[33]=sample%11;if(r[33]==10)r[33]=11;
  willowMotionPutWord(r+6,0x200);willowMotionPutWord(r+10,0x100);
  for(unsigned n=0;n<2;n++){
   uint8_t*p=resolve(0xdbc6+(n?59:0)*64);p[0]=1;p[2]=4;p[3]=2;p[34]=255;
   willowMotionPutWord(p+6,0x200);willowMotionPutWord(p+10,0x100);
  }
  wb(0x9810,0);ww(0x8384,0);ww(0x8344,5);ww(0x8374,0);
  ww(0x984c,0x200);ww(0x9850,0x100);ww(0x8408,0x180);ww(0x840c,0x80);
  wb(0x806a,8);wb(0x8366,sample%7);wb(0x8367,(sample/7)%5);wb(0x9866,sample&1);
  wb(0x8363,8);wb(0x8362,8);wb(0x8364,sample%4);wb(0x8342,3);wb(0x8292,sample&1);
  for(unsigned n=0;n<12;n++)wb(0x8352+n,(sample+n)%4);
  wb(0x8290,sample&1);wb(0x8293,(sample/2)&1);ww(0x801e,(sample*2)&126);ww(0x801a,(sample*4)&252);
  uint32_t a4=sample&1?0xffb000:0x123456;uint16_t d4=sample*977;WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
  WillowArcadeActorUpdate task={0};unsigned pending=0,sourceResume=0,sourceSp=0,waitTicks=0;
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
  WillowArcadeGameplayActors gameplay={0};
  wb(0x9846,0);wb(0x9866,0);wb(0x8293,0);wb(0x8060,8);wb(0x8062,8);
  ww(0x83c6,9);wb(0x83c8,0);ww(0x83d2,0);wb(0x83e9,0);wb(0x83cf,0);wb(0x83e2,0);wb(0xf99e,0);
#endif
  for(unsigned step=0;step<128;step++){
   if(pending){wb(0xf891,++waitTicks>=3?0:sample&1?128:255);}
   else{willowArcadeVisibilityRender(&bus,discardSprite,discardSprite);waitTicks=0;}
   uint8_t oldTimer=r[34],oldSub=r[3];uint16_t oldD4=d4;uint32_t oldA4=a4;
   for(unsigned n=0;n<65536;n++)if(!pending||n<0xfe80||n>=0xff04)ram[n^1]=expected[n];
   memcpy(mapRam,scroll2,sizeof scroll2);
   mapPointers(ram,1);
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
#ifdef WILLOW_TEST_GAMEPLAY_COLLISION
   uint8_t result=willowArcadeGameplayCollisionResume(
#else
   uint8_t result=willowArcadeGameplayActorsResume(
#endif
&gameplay,&bus,resolve,sourceWord,inputWord,&g,&collision,videoWrite,textWrite,0,&d4,&a4,playerUpdate,&collision);
   task=gameplay.actors;
#else
   uint8_t result=willowArcadeActorUpdateResume(&task,&bus,resolve,sourceWord,&g,&collision,videoWrite,textWrite,0,&d4,&a4);
#endif
   if(!result){fprintf(stderr,"Unsupported transition sample %u step %u state %u/%u phase %u\n",sample,step,r[2],r[3],task.phase);for(unsigned k=0;k<16;k++){uint16_t a=0xf042+k*64;if(rb(a))fprintf(stderr,"Palette %x state %u/%u type %u\n",a,rb(a+2),rb(a+3),rb(a+32));}return 1;}
   if(!pending){
    ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
    m68k_set_reg(12,oldA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
    m68k_set_reg(14,0xff0000|actor);m68k_set_reg(7,59);m68k_set_reg(0,0);m68k_set_reg(4,oldD4);m68k_set_reg(16,
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
    0x954e
#else
    0x9f34
#endif
    );
   }else{
    /* Resume the intercepted sleep JSR; task scheduling itself is covered
     * by the separate scheduler fixtures. Preserve original caller frames. */
    m68k_set_reg(15,0xff0000|sourceSp);m68k_set_reg(16,sourceResume);
   }
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Transition boundary sample %u step %u PC %x\n",sample,step,m68k_get_reg(16));return 1;}
   if(result==2){
    unsigned sp=m68k_get_reg(15)&65535;
    sourceResume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
    sourceSp=sp+4;
    if(sourceResume!=task.pickups.transition.wait||(m68k_get_reg(0)&65535)!=1){fprintf(stderr,"Wait mismatch sample %u step %u\n",sample,step);return 1;}
    waits++;
   }else if((m68k_get_reg(15)&65535)!=0xff04){fprintf(stderr,"Return mismatch sample %u step %u\n",sample,step);return 1;}
   mapPointers(ram,0);
   if(memcmp(mapRam,scroll2,sizeof scroll2)){fprintf(stderr,"Scroll2 mismatch sample %u step %u\n",sample,step);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Transition mismatch sample %u step %u at %x actual %x expected %x\n",sample,step,n,ram[n^1],expected[n]);return 1;
   }
   if(m68k_get_reg(12)!=a4){fprintf(stderr,"A4 sample %u step %u actual %x expected %x\n",sample,step,m68k_get_reg(12),a4);return 1;}
   if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Transition D4 mismatch sample %u step %u actual %x expected %x\n",sample,step,m68k_get_reg(4),d4);return 1;}
   if(!pending&&oldSub==6&&oldTimer==1)reloads++;
   if(pending&&result==1&&sourceResume==0x1ec4)reveals++;
   pending=result==2;checks++;
   if(!r[0])break;
  }
 }
 if(!waits||reloads!=64||reveals!=64){fprintf(stderr,"Coverage mismatch waits %u reloads %u reveals %u\n",waits,reloads,reveals);return 1;}
#ifdef WILLOW_TEST_GAMEPLAY_PREFIX
#ifdef WILLOW_TEST_GAMEPLAY_COLLISION
 printf("Including collision pass 39A52; ");
#endif
 printf("Including original 954E input/clock and BEF8 player; ");
#endif
 printf("Resumable actor update chain: %u original update/wait boundaries; %u waits, %u reloads, %u reveal completions, actors before/after suspended slot, full RAM/HUD and D4/A4 match with controlled fade-completion flags\n",checks,waits,reloads,reveals);
 return 0;
}
