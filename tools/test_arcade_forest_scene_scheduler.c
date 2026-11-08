/* Continuous host/native main task, map actors, sprites, fade tasks and wakes.
 * One manager boundary and scheduler pass per fixture tick; no forced completion. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_native_task_runtime.h"
#include "../src/willow_native_gameplay_flow.h"
#include "../src/willow_arcade_player_bus.h"
#include "../src/willow_arcade_sprite_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],source[0x100000];
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

static uint32_t videoBase;static unsigned videoSize;
static unsigned videoWrites,paletteWrites;
static uint8_t scroll2[0x4000];
static uint8_t attribute(uint16_t at){return scroll2[at+2];}
static const WillowVisibilityBus bus={rw,rb,ww,wb};
static uint16_t romWord(uint32_t at){return (at&0xff0000UL)==0xff0000UL?rw((uint16_t)at):willowMotionWord(source+at);}
static void videoWord(uint32_t at,uint16_t v){if(at>=0x918000&&at<0x91c000)willowMotionPutWord(scroll2+(at-0x918000),v);if(at>=videoBase&&at<videoBase+videoSize){ww(0x4000+at-videoBase,v);videoWrites++;}}
static uint8_t*resolve(uint16_t at){return expected+at;}
static uint8_t romByte(uint32_t at){return source[at];}
static void write2(uint16_t at,uint16_t value){videoWord(0x918000UL+at,value);}
static void write3(uint16_t at,uint16_t value){videoWord(0x914000UL+at,value);}
static void colorWrite(uint32_t at,uint16_t v){if(at<0x91c000||at>=0x91d800)abort();ww(0x2000+at-0x91c000,v);paletteWrites++;}
static uint16_t colorRead(uint32_t at){return rw(0x2000+at-0x91c000);}
static uint16_t sourceWord(unsigned long at){return (at&0xff0000UL)==0xff0000UL?rw((uint16_t)at):romWord(at);}
static uint8_t*allocateWithA4(uint32_t*a4){uint16_t at=willowArcadePaletteFreeWithA4(&bus,a4);return at?resolve(at):0;}
static uint8_t*allocate(void){return allocateWithA4(0);}
static uint8_t randomByte(void){return willowActor18Random(&bus);}
static void soundWithA4(uint16_t v,uint32_t*a4){willowBreakableSoundWithA4(&bus,v,a4);}
static void sound(uint16_t v){soundWithA4(v,0);}

static uint32_t *liveA4;

static unsigned availableEffects(void){return rb(0x97fe);}
static uint8_t*playerAllocate(uint8_t type){
 uint16_t count=type==2?0x97fe:type==8?0x9807:0x980d;
 uint16_t cursor=type==2?0x9816:type==8?0x9828:0x9834;
 if(!rb(count))return 0;
 uint16_t stack=rw(cursor),at=rw(stack);ww(cursor,stack+2);wb(count,rb(count)-1);
 *liveA4=(uint32_t)(int32_t)(int16_t)at;return resolve(at);
}
static uint8_t*playerEffect(void){return playerAllocate(2);}
static uint8_t*playerClass12(void){return playerAllocate(12);}
static void playerSound(uint16_t cmd){soundWithA4(cmd,liveA4);}
static void playerCancel(uint16_t at){willowArcadeCancelTaskWithA4(resolve(at),liveA4);}
static uint16_t playerColor(unsigned n){return rw(0x2000+n*2);}
static void playerColorWrite(unsigned n,uint16_t v){ww(0x2000+n*2,v);}

static void loadPlayerRecord(WillowPlayerRecord*p){memcpy(p->bytes,resolve(0x9846),128);}
static void storePlayerRecord(const WillowPlayerRecord*p){memcpy(resolve(0x9846),p->bytes,128);}
static uint8_t playerUpdate(void*context,uint16_t*d4,uint32_t*a4){
 WillowPlayerCollision*c=context;WillowPlayerRecord p={0};
 WillowPlayerInput i={0};i.availableEffects=availableEffects;i.allocateEffect=playerEffect;i.random=randomByte;i.sound=playerSound;
 WillowPlayerShots shots={0};shots.allocate=playerAllocate;
 WillowPlayerHealth h={0};h.paletteRead=playerColor;h.paletteWrite=playerColorWrite;h.textWrite=videoWord;h.sound=playerSound;h.savedD4=d4;
 WillowPlayerCharge q={0};q.textWrite=videoWord;q.sound=playerSound;
 WillowPlayerScript t={0};t.cancelTask=playerCancel;liveA4=a4;
 return willowArcadeWillowControllerWithRecordIO(&bus,&p,&i,c,&shots,&h,&q,&t,playerClass12,loadPlayerRecord,storePlayerRecord);
}
static uint16_t*renderD4;static uint32_t*renderA4;
static uint32_t outputCursor;static uint16_t outputBudget;
static void emitPiece(uint16_t x,uint16_t y,uint16_t code,uint16_t attr){videoWord(outputCursor,x);videoWord(outputCursor+2,y);videoWord(outputCursor+4,code);videoWord(outputCursor+6,attr);outputCursor+=8;}
static void drawOrdinary(uint16_t at){WillowArcadeSpriteWriter w={romWord,emitPiece,rw(0x829a),outputBudget};willowArcadeSpriteWithD4(&w,resolve(at),rw(0x8408),rw(0x840c),renderD4);ww(0x829a,w.areaBudget);outputBudget=w.pieceBudget;}
static void drawStage4(uint16_t at){WillowArcadeSpriteWriter w={romWord,emitPiece,rw(0x829a),outputBudget};willowArcadeStage4SpriteWithD4(&w,resolve(at),rw(0x8408),rw(0x840c),renderD4);ww(0x829a,w.areaBudget);outputBudget=w.pieceBudget;}
static void drawFrame(WillowArcadeSpriteFrame*f){outputCursor=f->cursor;outputBudget=f->remaining;willowArcadeVisibilityRenderWithA4(&bus,drawOrdinary,drawStage4,renderA4);f->cursor=outputCursor;f->remaining=outputBudget;}
static int cpuContext(unsigned a){return (a<512&&a%32>=2&&a%32<12)||(a>=0xd4&&a<0xdc)||(a>=0x200&&a<0x204)||(a>=0x1000&&a<0x2000)||(a>=0xef80&&a<0xf004)||(a>=0xfe80&&a<0xff04);}
int main(int argc,char**argv){
 if(argc!=3)return 2;
 unsigned region=strtoul(argv[2],0,10);if(region>4)return 2;
 const uint32_t videoBases[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 videoBase=videoBases[region];videoSize=region<3?0x4000:0x800;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(source,rom,sizeof source);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Intercept only the outer manager wait; scheduled tasks use real 1750. */
 if(willowMotionWord(rom+0x1ea0)!=0x1750)return 2;
 willowMotionPutWord(rom+0x1ea0,0x300);
 if(willowMotionWord(rom+0x1ec2)!=0x1750)return 2;
 willowMotionPutWord(rom+0x1ec2,0x300);
 if(willowMotionWord(rom+0x932a)!=0x1750)return 2;
 willowMotionPutWord(rom+0x932a,0x300);
 willowMotionPutWord(rom+0x300,0x4ef9);willowMotionPutLong(rom+0x302,0x100000);
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
 willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x9c,0x160e);
 willowMotionPutLong(rom+0x8c,0x175c);willowMotionPutLong(rom+0x90,0x1788);willowMotionPutLong(rom+0xa4,0x165c);
 willowMotionPutWord(rom+0x15b4,0x4e75);
 const unsigned clears[]={0x5876,0x588a,0x589e,0x58bc,0x58c6};
 if(willowMotionLong(rom+clears[region])!=videoBase)return 2;
 willowMotionPutLong(rom+clears[region],0xff4000);
 if(region<3){
  const unsigned resetClears[]={0x6644,0x6656,0x6668};
  if(willowMotionLong(rom+resetClears[region])!=videoBase)return 2;
  willowMotionPutLong(rom+resetClears[region],0xff4000);
 }
 const unsigned fadePatches[]={0x51ee,0x51fe,0x520e,0x521e,0x522e,0x523e};
 for(unsigned n=0;n<6;n++){
  if(willowMotionLong(rom+fadePatches[n])!=0x91c000+n*1024)return 2;
  willowMotionPutLong(rom+fadePatches[n],0xff2000+n*1024);
 }
 /* Bank-one relocation changes its identifying address bit from 15 to 14. */
 if(region==4){
  if(willowMotionWord(rom+0x3b57e)!=15)return 2;
  willowMotionPutWord(rom+0x3b57e,14);
 }
 if(region>=3){
  unsigned spriteAt=region==3?0x3b538:0x3b54c;
  if(willowMotionLong(rom+spriteAt)!=videoBase)return 2;
  willowMotionPutLong(rom+spriteAt,0xff4000);
  unsigned at=region==3?0x58f8:0x5902;
  if(willowMotionLong(rom+at)!=videoBase)return 2;
  willowMotionPutLong(rom+at,0xff4000);
 }
 const unsigned palettePatches[]={0xae4,0xb06,0xaf6,0xb16};
 const uint32_t paletteTargets[]={0x91c800,0x91cc00,0x91c400,0x91c200};
 for(unsigned n=0;n<4;n++){
  if(willowMotionLong(rom+palettePatches[n])!=paletteTargets[n])return 2;
  willowMotionPutLong(rom+palettePatches[n],paletteTargets[n]-0x91c000+0xff2000);
 }
 if(region==1||region==2){
  unsigned at=region==1?0x4365a:0x43d50;
  if(willowMotionLong(rom+at)!=videoBase)return 2;
  willowMotionPutLong(rom+at,0xff4000);
 }
 /* Relocate complete stage palettes and all HUD/message output paths. */
 const unsigned colors[]={0x988,0x99a,0x9b0,0x9cc,0x9da,0xc242,0xea68,0xc2e,0x1276};
 const uint32_t colorBases[]={0x91c000,0x91c200,0x91c800,0x91c400,0x91cc00,0x91c000,0x91c0e0,0x91c000,0x91c000};
 for(unsigned n=0;n<sizeof colors/sizeof colors[0];n++){
  if(willowMotionLong(rom+colors[n])!=colorBases[n])return 2;
  willowMotionPutLong(rom+colors[n],0xff2000+colorBases[n]-0x91c000);
 }
 willowMotionPutLong(rom+0x88,0x1690);willowMotionPutLong(rom+0xa0,0x1638);
 if(region==0){
 const unsigned patches[]={0x7d84,0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910200,0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}

  willowMotionPutLong(rom+0x5c32,0xff4000);
  willowMotionPutLong(rom+0x4b72,0xff4170);
  for(unsigned n=0;n<47;n++){
   unsigned at=0x7a1e + n*4;
   willowMotionPutLong(rom+at,willowMotionLong(rom+at)-0x910000+0xff4000);
  }
  for(unsigned n=0;n<5;n++)for(unsigned t=0;t<2;t++){
   unsigned at=(t?0x7c64:0x7c50)+n*4;
   willowMotionPutLong(rom+at,willowMotionLong(rom+at)-0x910000+0xff4000);
  }
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/forest-scene-scheduler-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/forest-scene-scheduler-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 unsigned checks=0,waits=0,completions=0,descentUpdates=0;
 for(unsigned sample=0;sample<4;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31);
  wb(0xf9a3,5);wb(0x8288,3); /* Valid pre-game health/lives configuration. */
  ww(0x8000,0);ww(0x8004,0);ww(0x8008,0);
  wb(0x8058,0);wb(0x8068,0);ww(0x8070,10000);wb(0x82a0,sample&1);wb(0x8023,0);
  wb(0x8067,0);ww(0x828a,0);ww(0x828c,0x183c);
  ww(0x801a,(sample*4)&252);ww(0x801e,(sample*2)&126);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);
  WillowNativeTaskRuntime runtime={0};
  memset(expected,0,512);wb(0x020e,0);wb(0x828f,0);
  willowArcadeTaskInitFree(&bus,romWord);
  for(unsigned n=0;n<16;n++){ww(n*32+12,0xff);ww(n*32+14,0x1100+n*256);}
  for(unsigned n=0;n<128;n++)ww(0x8072+n*2,65535);
  ww(0x801c,rw(0x801a));willowArcadeTaskStart(&bus,0x60,0x1ddc,0,0,1);
  ww(0x82c0,0);wb(0xf890,0);wb(0xf891,0);
  for(unsigned n=0;n<12;n++)ww(0x82a8+n*2,0);
  for(unsigned n=0;n<3072;n++)ww(0x2000+n*2,(uint16_t)(((n/16+sample)%16)*0x1000)|((n*17)&4095));
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d4=sample*977,eventWait=0;uint32_t a4=0x123456;
  WillowNativeGameplayFlow flow={0};
  WillowPaletteAnimation palette={0};palette.read=colorRead;palette.write=colorWrite;
  palette.byte=romByte;palette.word=romWord;palette.allocate=allocate;palette.allocateWithA4=allocateWithA4;
  palette.random=randomByte;palette.sound=sound;palette.soundWithA4=soundWithA4;
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};WillowArcadeActorUpdate actors={0};WillowArcadeSpriteFrame frame;
  renderD4=&d4;renderA4=&a4;
  WillowArcadeGameplayServices services={.word=sourceWord,.collision=&collision,.actors=&actors,.player=playerUpdate,.playerContext=&collision,
   .videoWrite=videoWord,.textWrite=videoWord,.draw=drawFrame,.spriteWord=videoWord,.frame=&frame,.palette=&palette,.resolve=resolve,.byte=romByte,.write2=write2,.write3=write3,.bus=&bus,.d4=&d4,.a4=&a4,.eventWait=&eventWait,.busWord=romWord,.mapClearWrite=videoWord};
  WillowNativeGameplayFlowBinding binding={&flow,&bus,romWord,&a4,willowArcadeGameplayServiceResume,&services};
  uint32_t pc=0x92d2,sp=0xffff00;
  unsigned mainRegisters[15]={0};mainRegisters[13]=0xff8000;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  uint8_t sawSky=0;unsigned caseDescent=0;
  for(unsigned pass=0;pass<4096;pass++){
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n))ram[n^1]=expected[n];
   /* Intercepted main sleep must retain the register set that real 1750 saves. */
   for(unsigned i=0;i<15;i++)m68k_set_reg(i,mainRegisters[i]);
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,sp);
   m68k_set_reg(12,a4);m68k_set_reg(4,d4);m68k_set_reg(16,pc);

   uint16_t previousY=rw(0x840c);
   WillowTransitionStep result=willowNativeGameplayFlowStep(&binding);
   if(!result.supported||result.finished||result.delay!=1){fprintf(stderr,"Unsupported main sample %u pass %u state %x scene %x actor %u\n",sample,pass,rw(0x8008),flow.scene.pc,actors.phase);return 1;}
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Forest mismatch sample %u pass %u at %x got %x want %x state %x manager %x pc %x\n",sample,pass,n,ram[n^1],expected[n],rw(0x8008),rw(0x8004),pc);
    return 1;
   }
   if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Registers pass %u A4 %x/%x D4 %x/%x\n",pass,m68k_get_reg(12),a4,m68k_get_reg(4),d4);return 1;}
   for(unsigned i=0;i<15;i++)mainRegisters[i]=m68k_get_reg(i);

   if(rw(0x8008)==8&&rw(0x840c)==512)sawSky=1;
   if(sawSky&&rw(0x840c)<previousY){caseDescent++;descentUpdates++;}
   checks++;
   sp=m68k_get_reg(15);
   if((m68k_get_reg(0)&65535)!=1)return 1;
   pc=((uint32_t)ram[(sp&65535)^1]<<24)|((uint32_t)ram[((sp+1)&65535)^1]<<16)|((uint32_t)ram[((sp+2)&65535)^1]<<8)|ram[((sp+3)&65535)^1];sp+=4;
   if(flow.task.pc==0x932c){
    if(pc!=0x932c||sp!=0xffff00||eventWait||flow.manager.target)return 1;
    if(rw(0x8004)==2&&rw(0x8008)==2){
     if(!sawSky||!caseDescent||rw(0x840c))return 1;
     completions++;break;
    }
   }else{
    if(pc!=eventWait)return 1;
    waits++;
   }
   uint16_t cursor=0,remaining=15;
   for(;;){
    WillowNativeTaskRun run=willowNativeTaskRunOneWithVideo(&runtime,&bus,cursor,remaining,romWord,colorRead,colorWrite,0,0,videoWord);
    if(run.unsupported){fprintf(stderr,"Unsupported scheduled slot %x PC %x pass %u scene %x entry %x:%x payload %x/%x\n",run.cursor,runtime.resume[run.cursor/32],pass,rw(0x8008),rw(run.cursor+4),rw(run.cursor+6),rw(run.cursor+16),rw(run.cursor+18));return 1;}
    if(!run.dispatched||run.remaining==65535)break;
    cursor=run.cursor;remaining=run.remaining;
   }
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);
   m68k_set_reg(8,0xff0000);m68k_set_reg(0,15);m68k_set_reg(16,0x14d8);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Scheduler mismatch region %u sample %u pass %u at %x got %x want %x\n",region,sample,pass,n,ram[n^1],expected[n]);return 1;
   }


   willowArcadeTaskWake(&bus);
   putRamWord(ram,0xf000,0x10);putRamWord(ram,0xf002,0);
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);m68k_set_reg(16,0x1590);retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n])return 1;

   if(pass==4095)return 1;
  }
 }
 if(completions!=4||!waits||!paletteWrites||!videoWrites)return 1;
 printf("Forest scene region %u: %u main boundaries, %u real command waits, %u completed forest intros, %u video writes, %u palette writes, %u descending-camera updates; actual actor/sprite updates and scheduler agree with original execution\n",region,checks,waits,completions,videoWrites,paletteWrites,descentUpdates);
 return 0;
}
