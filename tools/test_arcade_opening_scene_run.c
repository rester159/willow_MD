/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_opening_scene_run.h"
#include "../src/willow_arcade_pool_init.h"
#include "../src/willow_arcade_sprite_frame.h"
#include "../src/willow_arcade_sprite_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "../src/willow_arcade_actor_pool.h"
#ifdef WILLOW_TEST_OPENING_TASK
#include "../src/willow_native_attract_task.h"
#endif
static uint8_t rom[0x200000],expected[65536];
static WillowActorPool pool;
static uint16_t sourceWord(unsigned long at){return willowMotionWord(rom+at);}
static uint16_t busWord(uint16_t at){return willowMotionWord(expected+at);}
static uint8_t busByte(uint16_t at){return expected[at];}
static void busPutWord(uint16_t at,uint16_t value){willowMotionPutWord(expected+at,value);}
static void busPutByte(uint16_t at,uint8_t value){expected[at]=value;}
static void releaseActor(uint8_t *record){
 willowActorPoolRelease(&pool,(record-pool.records)/128);
 uint16_t next=busWord(0x981c)-2;
 busPutWord(next,record-expected);busPutWord(0x981c,next);expected[0x9801]++;
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
static unsigned emitted,outputCursor;
static uint16_t pieceBudget;
static uint16_t spriteRomWord(uint32_t at){if((at&0xff0000)==0xff0000)return busWord(at);if(at+1>=sizeof rom)abort();return willowMotionWord(rom+at);}
static void emitStage4Words(uint16_t x,uint16_t y,uint16_t code,uint16_t attribute)
{
 busPutWord(outputCursor,x);busPutWord(outputCursor+2,y);
 busPutWord(outputCursor+4,code);busPutWord(outputCursor+6,attribute);
 outputCursor+=8;emitted++;
}
static void emitOrdinarySprite(uint16_t handle)
{
 WillowArcadeSpriteWriter writer={spriteRomWord,emitStage4Words,busWord(0x829a),pieceBudget};
 willowArcadeSprite(&writer,expected+handle,busWord(0x8408),busWord(0x840c));
 busPutWord(0x829a,writer.areaBudget);pieceBudget=writer.pieceBudget;
}
static void emitStage4Sprite(uint16_t handle)
{
 WillowArcadeSpriteWriter writer={spriteRomWord,emitStage4Words,busWord(0x829a),pieceBudget};
 willowArcadeStage4Sprite(&writer,expected+handle,busWord(0x8408),busWord(0x840c));
 busPutWord(0x829a,writer.areaBudget);pieceBudget=writer.pieceBudget;
}
static void frameRamWord(uint32_t at,uint16_t value){if((at&0xff0000)!=0xff0000)abort();busPutWord(at,value);}
static uint32_t relocatedSpriteAddress(uint32_t at)
{
 if(at>=0x900000 && at<0x901000)return 0xff4000+(at-0x900000);
 if(at>=0x908000 && at<0x909000)return 0xff6000+(at-0x908000);
 abort();
}
static void writeFrameSprite(uint32_t at,uint16_t value){frameRamWord(relocatedSpriteAddress(at),value);}
static void drawFrameSprites(WillowArcadeSpriteFrame*f)
{
 uint32_t start=relocatedSpriteAddress(f->cursor);
 outputCursor=start&65535;pieceBudget=f->remaining;emitted=0;
 const WillowVisibilityBus b={busWord,busByte,busPutWord,busPutByte};
 willowArcadeVisibilityRender(&b,emitOrdinarySprite,emitStage4Sprite);
 f->cursor+=outputCursor-(start&65535);f->remaining=pieceBudget;
}
static uint8_t *resolve(uint16_t at){return expected+at;}
static uint8_t sourceByte(uint32_t at){return rom[at];}
static uint16_t paletteWord(uint32_t at){return willowMotionWord(rom+at);}
static uint16_t colorRead(uint32_t at){return busWord(0x2000+at-0x91c000);}
static void colorWrite(uint32_t at,uint16_t value){busPutWord(0x2000+at-0x91c000,value);}
static uint8_t canonicalByte(uint8_t*ram,unsigned at){
 for(unsigned n=0;n<4;n++){
  unsigned base=0xf042+n*64+48;
  if(at>=base&&at<base+4){
   uint32_t value=((uint32_t)ram[base^1]<<24)|((uint32_t)ram[(base+1)^1]<<16)|((uint32_t)ram[(base+2)^1]<<8)|ram[(base+3)^1];
   if(value)value=value-0xff2000+0x91c000;
   return value>>(24-(at-base)*8);
  }
 }
 return ram[at^1];
}
static void unusedMap(uint16_t at,uint16_t value){(void)at;(void)value;abort();}
static unsigned clearWrites;
static void sceneVideo(uint32_t at,uint16_t value){
 clearWrites++;
 if((at>=0x900000&&at<0x900800)||(at>=0x908000&&at<0x908800))writeFrameSprite(at,value);
}
#ifdef WILLOW_TEST_OPENING_TASK
static WillowArcadeSpriteFrame openingOutput;
static uint8_t openingBoundary;
static void openingTick(uint16_t*d1,uint16_t*d2)
{
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 WillowPaletteAnimation g={0};g.byte=sourceByte;g.word=paletteWord;g.read=colorRead;g.write=colorWrite;
 uint8_t initializing=!expected[0x9dc8];uint16_t state=busWord(0x8008);
 openingBoundary=willowArcadeOpeningSceneRun(&bus,resolve,&g,sourceWord,sourceByte,paletteWord,
  colorWrite,unusedMap,unusedMap,releaseActor,drawFrameSprites,writeFrameSprite,sceneVideo,&openingOutput);
 if(openingBoundary==WILLOW_ATTRACT_CLEAR){*d1=0;*d2=0;return;}
 if(openingBoundary==WILLOW_ATTRACT_RETURN){if(state==10){*d1=sourceWord(0x8292);return;}*d1=state==8?3:state==4?0x34:0x35;*d2=state==8?16:state==4?0x1607:0x1608;return;}
 if(openingBoundary!=WILLOW_ATTRACT_ACTORS)abort();
 if(initializing)*d1=8;
 else{
  uint16_t y=busWord(0x9dd0)-busWord(0x840c);
  *d1=(y^255)+1;
  *d2=sourceWord(willowMotionLong(expected+0x9dda)+2);
 }
}
#endif
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;

 willowMotionPutLong(rom+0xc2e,0xff2000);
 willowMotionPutLong(rom+0x65ec,0xff4000);willowMotionPutLong(rom+0x65f6,0xff6000);
 /* Hardware remap: buffer B moves from bit 15 to bit 13; adjust only the
  * corresponding address discriminator, leaving cleanup logic intact. */
 if(willowMotionWord(rom+0x3b57e)!=15)return 2;
 willowMotionPutWord(rom+0x3b57e,13);
 willowMotionPutLong(rom+0x3b538,0xff4000);willowMotionPutLong(rom+0x3b54c,0xff6000);
 #ifdef WILLOW_TEST_OPENING_TASK
 /* Intercept only the source sleep boundary; the complete manager and its
  * actor/palette/publication calls still execute original instructions. */
 willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x9c,0x160e);
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
#endif
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/opening-scene-run-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/opening-scene-run-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 unsigned maxSlices=0;
#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at)); \
 unsigned slices=0; \
 for(;slices<32&&m68k_get_reg(16)!=0x100000;slices++)retro_run(); \
 if(slices>maxSlices)maxSlices=slices; \
 if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Source stalled at %x\n",m68k_get_reg(16));return 1;} \
} while(0)
 unsigned checks=0;
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 memset(expected,0,sizeof expected);
 willowArcadePoolsInit(&bus);
#ifdef WILLOW_TEST_OPENING_TASK
 willowArcadeTaskInitFree(&bus,paletteWord);
#endif
 for(unsigned group=0;group<4;group++)willowArcadeVisibilityInit(&bus,group,0);
 for(unsigned n=0;n<0x800;n++){expected[0x4000+n]=0x5a;expected[0x6000+n]=0xa5;}
 /* Follow the actual looping animation, retaining its state between frames. */
 uint8_t*r=expected+0x9dc6;memset(r,0,128);r[0]=1;r[30]=4;r[32]=8;
 willowMotionPutWord(r+6,0x4c0);willowMotionPutWord(r+10,0x40);
 busPutWord(0x8408,0x400);busPutWord(0x840c,0);
 const uint32_t paletteSources[]={0xea660,0xea700,0xebb00,0xebb60};
 const uint8_t groups[]={1,3,3,4},sequences[]={0,1,2,2};
 for(unsigned n=0;n<4;n++){
  uint8_t*p=expected+0xf042+n*64;p[0]=1;p[32]=1;p[56]=groups[n];
  p[59]=sequences[n];p[58]=4;p[60]=n<2?2:3;willowMotionPutLong(p+52,paletteSources[n]);
 }
 busPutByte(0x8293,1);
 WillowPaletteAnimation g={0};g.byte=sourceByte;g.word=paletteWord;g.read=colorRead;g.write=colorWrite;
 for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
 for(unsigned frame=0;frame<512;frame++){
  /* Fixture exercises buffer selection without inventing game camera motion. */
  busPutByte(0x82a0,(frame/64)%2);ram[0x82a0^1]=busByte(0x82a0);
  /* Exercise each wait branch without manually clearing its visibility
   * queues. Timers/busy state are controlled inputs, not game progression. */
  unsigned state=2+(frame%4)*2;
#ifdef WILLOW_TEST_OPENING_TASK
  state=2+2*(frame%5);
  busPutWord(0x8004,0);putRamWord(ram,0x8004,0);
  busPutWord(0x82c0,(frame/5)&1);putRamWord(ram,0x82c0,busWord(0x82c0));busPutWord(0x8000,4);putRamWord(ram,0x8000,4);
#endif
  busPutWord(0x8008,state);busPutWord(0x800a,frame&8?0:2);
#ifdef WILLOW_TEST_OPENING_TASK
  if(state!=2&&(frame%8>=5))busPutWord(0x800a,1);
#endif
  busPutByte(0xe58,frame&16?1:0);
  putRamWord(ram,0x8008,state);putRamWord(ram,0x800a,busWord(0x800a));ram[0xe58^1]=busByte(0xe58);
#ifdef WILLOW_TEST_OPENING_TASK
  WillowNativeAttractTask task={0x810e,(uint16_t)(frame*311),(uint16_t)(frame*977)};
  /* Without an explicitly preflighted opening capability the legacy entry
   * must reject atomically, including continuation and live register words. */
  WillowNativeAttractTask beforeTask=task;
  uint8_t beforeRam[65536];memcpy(beforeRam,expected,sizeof beforeRam);
  WillowTransitionStep unavailable=willowNativeAttractTaskStep(&task,&bus,paletteWord,colorWrite,sceneVideo);
  if(unavailable.supported||memcmp(&task,&beforeTask,sizeof task)||memcmp(beforeRam,expected,sizeof beforeRam))return 1;
  m68k_set_reg(1,task.d1);m68k_set_reg(2,task.d2);
  WillowTransitionStep step=willowNativeAttractTaskStepWithOpening(&task,&bus,paletteWord,colorWrite,sceneVideo,0,0,0,0,openingTick);
  if(!step.supported||step.finished||step.delay!=1||step.resume!=0x810e)return 1;
  RUN(0x80da,0);
  unsigned sp=m68k_get_reg(15)&65535;
  unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
  if(resume!=step.resume||(m68k_get_reg(0)&65535)!=step.delay||
   (m68k_get_reg(1)&65535)!=task.d1||(m68k_get_reg(2)&65535)!=task.d2){
   fprintf(stderr,"Opening task frame %u live words %x/%x expected %x/%x resume %x\n",frame,m68k_get_reg(1),m68k_get_reg(2),task.d1,task.d2,resume);return 1;
  }
  WillowArcadeSpriteFrame output=openingOutput;
#else
  WillowArcadeSpriteFrame output;
  uint8_t boundary=willowArcadeOpeningSceneRun(&bus,resolve,&g,sourceWord,sourceByte,paletteWord,
   colorWrite,unusedMap,unusedMap,releaseActor,drawFrameSprites,writeFrameSprite,sceneVideo,&output);
  if(boundary!=WILLOW_ATTRACT_ACTORS){fprintf(stderr,"Unsupported scene frame %u\n",frame);return 1;}
  RUN(0x8266,0);
#endif
#ifdef WILLOW_TEST_OPENING_TASK
  if(openingBoundary==WILLOW_ATTRACT_ACTORS)
#endif
  if(m68k_get_reg(8)!=relocatedSpriteAddress(output.cursor)||(m68k_get_reg(7)&65535)!=output.remaining){fprintf(stderr,"Frame %u output actual %x/%x expected %x/%x\n",frame,m68k_get_reg(8),m68k_get_reg(7)&65535,relocatedSpriteAddress(output.cursor),output.remaining);return 1;}
  for(unsigned n=0;n<65536;n++)if((n<0xfed0||n>=0xff04)&&canonicalByte(ram,n)!=expected[n]){
   fprintf(stderr,"Title publication frame %u address %x actual %x expected %x\n",frame,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 /* Execute the terminal clear through the same scene runner and compare
  * state plus both object buffers. Map clear contents have separate coverage. */
 clearWrites=0;busPutWord(0x8004,0);putRamWord(ram,0x8004,0);
 busPutWord(0x8008,10);busPutWord(0x82c0,1);
 putRamWord(ram,0x8008,10);putRamWord(ram,0x82c0,1);
 WillowArcadeSpriteFrame output;
 uint8_t boundary=willowArcadeOpeningSceneRun(&bus,resolve,&g,sourceWord,sourceByte,paletteWord,
  colorWrite,unusedMap,unusedMap,releaseActor,drawFrameSprites,writeFrameSprite,sceneVideo,&output);
 if(boundary!=WILLOW_ATTRACT_CLEAR||clearWrites!=26624)return 1;
 RUN(0x8266,0);
 for(unsigned n=0;n<65536;n++)if((n<0xfed0||n>=0xff04)&&canonicalByte(ram,n)!=expected[n]){
  fprintf(stderr,"Scene clear RAM mismatch at %x\n",n);return 1;
 }
#ifdef WILLOW_TEST_OPENING_TASK
 printf("Opening outer task: %u original sleep boundaries, live D1/D2, full scene RAM, palettes and object banks match\n",checks);
#endif
 printf("Opening scene execution: %u consecutive frames; complete 8266 wait-branch actor/palette/visibility/publication execution and both sprite buffers match; maximum %u oracle slices (not a native frame-rate measurement)\n",checks,maxSlices);
 return 0;
}
