/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_opening_scene_run.h"
#include "../src/willow_native_task_runtime.h"
#include "../src/willow_arcade_pool_init.h"
#include "../src/willow_arcade_task_startup.h"
#include "../src/willow_arcade_boot_options.h"
#include "../src/willow_arcade_sprite_frame.h"
#include "../src/willow_arcade_sprite_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "../src/willow_arcade_actor_pool.h"
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
 for(unsigned n=0;n<16;n++){
  unsigned base=0xf042+n*64+48;
  if(at>=base&&at<base+4){
   uint32_t value=((uint32_t)ram[base^1]<<24)|((uint32_t)ram[(base+1)^1]<<16)|((uint32_t)ram[(base+2)^1]<<8)|ram[(base+3)^1];
   if(value)value=value-0xff2000+0x91c000;
   return value>>(24-(at-base)*8);
  }
 }
 return ram[at^1];
}
static unsigned mapWrites;
static void unusedMap(uint16_t at,uint16_t value){(void)at;(void)value;mapWrites++;}
static unsigned clearWrites;
static uint8_t retainedScroll1[16384];
static void sceneVideo(uint32_t at,uint16_t value){
 clearWrites++;
 if(at>=0x910000&&at<0x914000&&!(at&1))willowMotionPutWord(retainedScroll1+(at-0x910000),value);
 if((at>=0x900000&&at<0x900800)||(at>=0x908000&&at<0x908800))writeFrameSprite(at,value);
}
static int privateContext(unsigned at){
 return (at<512&&(at%32)>=2&&(at%32)<12)||
  (at>=0xc0&&at<0x100&&(at%32)>=20&&(at%32)<26)||
  (at>=0x200&&at<0x204)||(at>=0xf00&&at<0x2000)||
  (at>=0xfed0&&at<0xff04);
}
/* Optional continuation fixture. Keep each machine's private task contexts:
 * the native labels are not interchangeable with saved 68000 user stacks.
 * CPU palette destinations alone are canonicalized for later bus relocation. */
static int exportContinuation(const char*prefix,uint8_t*ram,const WillowNativeTaskRuntime*runtime){
 uint8_t cpu[65536],labels[32];
 for(unsigned n=0;n<sizeof cpu;n++)cpu[n]=canonicalByte(ram,n);
 for(unsigned n=0;n<16;n++)willowMotionPutWord(labels+n*2,runtime->resume[n]);
 const char*names[]={"native-ram","cpu-ram","resume","scroll1"};
 const void*data[]={expected,cpu,labels,retainedScroll1};
 const size_t sizes[]={sizeof expected,sizeof cpu,sizeof labels,sizeof retainedScroll1};
 for(unsigned n=0;n<4;n++){
  char path[4096];int length=snprintf(path,sizeof path,"%s-%s.bin",prefix,names[n]);
  if(length<0||(size_t)length>=sizeof path)return 0;
  FILE*out=fopen(path,"wb");if(!out){perror(path);return 0;}
  int ok=fwrite(data[n],1,sizes[n],out)==sizes[n];
  if(fclose(out))ok=0;
  if(!ok)return 0;
 }
 return 1;
}
int main(int argc,char**argv){
 if(argc!=2&&(argc!=3||(strcmp(argv[2],"--pan-setup")&&strcmp(argv[2],"--pan-move")&&strcmp(argv[2],"--pan-fade")&&strcmp(argv[2],"--phase-two")&&strcmp(argv[2],"--phase-four-text")&&strcmp(argv[2],"--phase-four-panel")&&strcmp(argv[2],"--phase-four-hold")&&strcmp(argv[2],"--phase-four-map")&&strcmp(argv[2],"--phase-four-scroll")&&strcmp(argv[2],"--phase-four-blink")&&strcmp(argv[2],"--phase-four-replace")&&strcmp(argv[2],"--phase-four-exit")&&strcmp(argv[2],"--phase-four")&&strcmp(argv[2],"--demo-init-0")&&strcmp(argv[2],"--demo-init-1")&&strcmp(argv[2],"--demo-start-0")&&strcmp(argv[2],"--demo-start-1"))))return 2;
 unsigned demoStart=argc==3&&(!strcmp(argv[2],"--demo-start-0")||!strcmp(argv[2],"--demo-start-1"));
 unsigned demoInit=demoStart||(argc==3&&(!strcmp(argv[2],"--demo-init-0")||!strcmp(argv[2],"--demo-init-1")));
 unsigned demoVariant=argc==3&&(!strcmp(argv[2],"--demo-init-1")||!strcmp(argv[2],"--demo-start-1"));
 unsigned phaseFour=demoInit||(argc==3&&!strcmp(argv[2],"--phase-four"));
 unsigned phaseFourExit=phaseFour||(argc==3&&!strcmp(argv[2],"--phase-four-exit"));
 unsigned phaseFourReplace=phaseFourExit||(argc==3&&!strcmp(argv[2],"--phase-four-replace"));
 unsigned phaseFourBlink=phaseFourReplace||(argc==3&&!strcmp(argv[2],"--phase-four-blink"));
 unsigned phaseFourScroll=phaseFourBlink||(argc==3&&!strcmp(argv[2],"--phase-four-scroll"));
 unsigned phaseFourMap=phaseFourScroll||(argc==3&&!strcmp(argv[2],"--phase-four-map"));
 unsigned phaseFourHold=phaseFourMap||(argc==3&&!strcmp(argv[2],"--phase-four-hold"));
 unsigned phaseFourPanel=phaseFourHold||(argc==3&&!strcmp(argv[2],"--phase-four-panel"));
 unsigned phaseFourText=phaseFourPanel||(argc==3&&!strcmp(argv[2],"--phase-four-text"));
 unsigned phaseTwo=phaseFourText||(argc==3&&!strcmp(argv[2],"--phase-two"));
 unsigned panFade=phaseTwo||(argc==3&&!strcmp(argv[2],"--pan-fade"));
 unsigned panMove=panFade||(argc==3&&!strcmp(argv[2],"--pan-move"));
 unsigned panSetup=argc==3;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 /* RUN supplies PC/SP explicitly. Preserve original vector bytes because
  * a cold saved-player pointer can legitimately read ROM address zero. */
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;

 willowMotionPutLong(rom+0xc2e,0xff2000);
 willowMotionPutLong(rom+0x65ec,0xff4000);willowMotionPutLong(rom+0x65f6,0xff6000);
 /* Hardware remap: buffer B moves from bit 15 to bit 13; adjust only the
  * corresponding address discriminator, leaving cleanup logic intact. */
 if(willowMotionWord(rom+0x3b57e)!=15)return 2;
 willowMotionPutWord(rom+0x3b57e,13);
 willowMotionPutLong(rom+0x3b538,0xff4000);willowMotionPutLong(rom+0x3b54c,0xff6000);
 const unsigned stageOperands[]={0x988,0x99a,0x9b0,0x9cc,0x9da};
 const unsigned stageOffsets[]={0,0x200,0x800,0x400,0xc00};
 for(unsigned n=0;n<5;n++)willowMotionPutLong(rom+stageOperands[n],0xff2000+stageOffsets[n]);
 const unsigned initOperands[]={0xae4,0xb06,0xaf6,0xb16,0x916};
 const uint32_t initTargets[]={0x91c800,0x91cc00,0x91c400,0x91c200,0x91c000};
 for(unsigned n=0;n<5;n++)willowMotionPutLong(rom+initOperands[n],initTargets[n]-0x91c000+0xff2000);
 const unsigned fades[]={0x51ee,0x51fe,0x520e,0x521e,0x522e,0x523e};
 for(unsigned n=0;n<6;n++)willowMotionPutLong(rom+fades[n],0xff2000+n*1024);
 willowMotionPutLong(rom+0x51b2,0xff2000);willowMotionPutLong(rom+0x514a,0xff2000);
 const unsigned banks[]={0x53c2,0x53f0,0x541e,0x544c};
 for(unsigned n=0;n<4;n++)willowMotionPutLong(rom+banks[n],0xff2000+n*1024);
 willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x8c,0x175c);
 willowMotionPutLong(rom+0x90,0x1788);willowMotionPutLong(rom+0x9c,0x160e);
 willowMotionPutLong(rom+0xa0,0x16be);willowMotionPutLong(rom+0xa4,0x165c);
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
 willowMotionPutWord(rom+0x15b4,0x4e75);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/opening-lifecycle-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/opening-lifecycle-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
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
 unsigned checks=0,dispatches=0;
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 memset(expected,0,sizeof expected);
 willowArcadeTaskInitFree(&bus,paletteWord);
 for(unsigned n=0;n<16;n++){busPutWord(n*32+12,0xff);busPutWord(n*32+14,0x1000+n*256);}
 for(unsigned group=0;group<4;group++)willowArcadeVisibilityInit(&bus,group,0);
 busPutByte(0x8293,1);
 if(demoStart){
  /* Include the source startup services whose retained output becomes visible
   * during demo HUD setup. Both oracle and model receive identical inputs. */
  willowArcadeBootOptions(&bus,0x1ac2,paletteWord);
  willowArcadeBootOptions(&bus,0x1b46,paletteWord);
  willowArcadeBootOptions(&bus,0x1b96,paletteWord);
  willowArcadeStartupPrimaryQueue(&bus);
  willowArcadeStartupRecordPool(&bus,paletteWord,0xffff8000UL);
  willowArcadeTaskStart(&bus,0xa0,0x1ddc,0,0,0);
 }
 if(demoInit){busPutWord(0xe5a,demoVariant);busPutWord(0x828a,0);busPutWord(0x828c,getenv("WILLOW_DEMO_ZERO_POINTER")?0:0x183c);}
 const char*titleFadeInput=getenv("WILLOW_TITLE_FADE_INPUT");
 const char*titleCommandInput=getenv("WILLOW_TITLE_COMMAND_INPUT");
 if(titleCommandInput){
  if(!demoStart||!titleFadeInput)return 2;
  uint8_t data[392];FILE*in=fopen(titleCommandInput,"rb");
  if(!in||fread(data,1,sizeof data,in)!=sizeof data||fgetc(in)!=EOF)return 2;fclose(in);
  const unsigned bases[]={0x801a,0x8072,0x801e,0x8172},sizes[]={4,256,4,128};unsigned offset=0;
  for(unsigned r=0;r<4;r++)for(unsigned n=0;n<sizes[r];n++)busPutByte(bases[r]+n,data[offset++]);
  fprintf(stderr,"Diagnostic inherited primary/sound queue history from %s\n",titleCommandInput);
 }
 if(titleFadeInput){
  uint8_t data[30];FILE*in=fopen(titleFadeInput,"rb");
  if(!in||fread(data,1,sizeof data,in)!=sizeof data||fgetc(in)!=EOF)return 2;fclose(in);
  for(unsigned n=0;n<26;n++)busPutByte(0x82a8+n,data[n]);
  for(unsigned n=0;n<4;n++)busPutByte(0x802e + n,data[26+n]);
  fprintf(stderr,"Diagnostic inherited fade masks/dark and Scroll1 coordinates from %s\n",titleFadeInput);
 }
 const char*titlePaletteInput=getenv("WILLOW_TITLE_PALETTE_INPUT");
 if(titlePaletteInput){
  if(!titleFadeInput)return 2;
  FILE*in=fopen(titlePaletteInput,"rb");uint8_t data[6144];
  if(!in||fread(data,1,sizeof data,in)!=sizeof data||fgetc(in)!=EOF)return 2;fclose(in);
  memcpy(expected+0x2000,data,sizeof data);
 }
 WillowNativeTaskRuntime runtime={0};
 WillowPaletteAnimation g={0};g.byte=sourceByte;g.word=paletteWord;g.read=colorRead;g.write=colorWrite;
 for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
 FILE*trace=fopen(demoStart?(demoVariant?"build/opening-demo-start-1-trace.csv":"build/opening-demo-start-0-trace.csv"):demoInit?(demoVariant?"build/opening-demo-init-1-trace.csv":"build/opening-demo-init-0-trace.csv"):phaseFour?"build/opening-phase-four-trace.csv":phaseFourExit?"build/opening-phase-four-exit-trace.csv":phaseFourReplace?"build/opening-phase-four-replace-trace.csv":phaseFourBlink?"build/opening-phase-four-blink-trace.csv":phaseFourScroll?"build/opening-phase-four-scroll-trace.csv":phaseFourMap?"build/opening-phase-four-map-trace.csv":phaseFourHold?"build/opening-phase-four-hold-trace.csv":phaseFourPanel?"build/opening-phase-four-panel-trace.csv":phaseFourText?"build/opening-phase-four-text-trace.csv":phaseTwo?"build/opening-phase-two-trace.csv":panFade?"build/opening-pan-fade-trace.csv":panMove?"build/opening-pan-move-trace.csv":panSetup?"build/opening-pan-handoff-trace.csv":"build/opening-lifecycle-trace.csv","w");if(!trace)return 2;
 fputs("cycle,scene,timer,message_busy,fade_complete,free_tasks\n",trace);
 FILE*schedule=phaseTwo?fopen(titleFadeInput?"build/opening-title-context-schedule.csv":phaseFour?"build/opening-phase-four-final-schedule.csv":phaseFourExit?"build/opening-phase-four-exit-schedule.csv":phaseFourReplace?"build/opening-phase-four-replace-schedule.csv":phaseFourBlink?"build/opening-phase-four-blink-schedule.csv":phaseFourScroll?"build/opening-phase-four-scroll-schedule.csv":phaseFourMap?"build/opening-phase-four-map-schedule.csv":phaseFourHold?"build/opening-phase-four-hold-schedule.csv":phaseFourText?"build/opening-phase-four-text-schedule.csv":"build/opening-phase-two-schedule.csv","w"):NULL;
 FILE*controllers=phaseFourHold?fopen(titleFadeInput?"build/opening-title-context-controllers.bin":phaseFour?"build/opening-panel-final-controllers.bin":phaseFourExit?"build/opening-panel-exit-controllers.bin":phaseFourReplace?"build/opening-panel-replace-controllers.bin":phaseFourBlink?"build/opening-panel-blink-controllers.bin":phaseFourScroll?"build/opening-panel-scroll-controllers.bin":phaseFourMap?"build/opening-panel-map-controllers.bin":"build/opening-panel-controllers.bin","wb"):NULL;
 FILE*colors=titleFadeInput?fopen("build/opening-title-context-colors.bin","wb"):NULL;
 if(titleFadeInput&&!colors)return 2;
 FILE*scroll1=titleFadeInput&&phaseFourBlink?fopen("build/opening-title-context-scroll1.bin","wb"):NULL;
 if(titleFadeInput&&phaseFourBlink&&!scroll1)return 2;
 if(phaseFourHold&&!controllers)return 2;
 if(phaseTwo&&!schedule)return 2;
 if(schedule)fputs("cycle,phase,scene,timer,message_busy,dark,free_tasks,near_y,far_y,c0_state,e0_state,c0_id,e0_id,palette_busy,panel_x,panel_y\n",schedule);
 const char*controllerInput=getenv("WILLOW_CONTROLLER_INPUT");unsigned controllerInputUsed=0;
 unsigned frame,visited=0,messages=0,panMessage=0,scrollSteps=0,reverseSteps=0,reverseMessages=0,textMessages=0,panelWrites=0;
 for(frame=0;frame<6000&&(busWord(0x8004)==0||(demoInit&&busWord(0x8004)==6&&busWord(0x8008)<=(demoStart?2:0))||(phaseFourText&&busWord(0x8004)==4&&busWord(0x8008)<=(phaseFour?38:phaseFourExit?32:phaseFourReplace?26:phaseFourBlink?24:phaseFourScroll?20:phaseFourMap?14:phaseFourHold?12:phaseFourPanel?8:6))||(panSetup&&busWord(0x8004)==2&&(phaseTwo||(panMove?busWord(0x8008)<=(panFade?12:4):!busWord(0x8008)))));frame++){
  /* Diagnostic only: use the cartridge's actual post-title controller input
   * for BOTH original instructions and model, never patch cartridge results. */
  if(controllerInput&&!controllerInputUsed&&busWord(0x8004)==2&&!busWord(0x8008)){
   uint8_t input[1024];FILE*in=fopen(controllerInput,"rb");
   if(!in||fread(input,1,sizeof input,in)!=sizeof input||fgetc(in)!=EOF)return 2;fclose(in);
   for(unsigned n=0;n<1024;n++){busPutByte(0xf042+n,input[n]);ram[(0xf042+n)^1]=input[n];}
   for(unsigned n=0;n<16;n++){
    unsigned at=0xf042+n*64+48;uint32_t destination=willowMotionLong(input+n*64+48);
    if(destination){
     if(destination<0x91c000||destination>=0x91d800)return 2;
     destination=destination-0x91c000+0xff2000;
     putRamWord(ram,at,destination>>16);putRamWord(ram,at+2,destination);
    }
   }
   controllerInputUsed=1;fprintf(stderr,"Diagnostic controller input imported at cycle %u from %s\n",frame,controllerInput);
  }
  if(panMove&&busWord(0x8004)==2&&busWord(0x8008)==4&&!busByte(0xe58)&&busWord(0x8038)>0x200)scrollSteps++;
  if(phaseTwo&&busWord(0x8004)==2&&busWord(0x8008)==18&&!busByte(0xe58))reverseSteps++;
  visited|=1u<<(busWord(0x8008)/2);
  WillowArcadeSpriteFrame output;
  unsigned priorVideoWrites=clearWrites;
  uint8_t boundary=willowArcadeOpeningSceneRun(&bus,resolve,&g,sourceWord,sourceByte,paletteWord,
   colorWrite,unusedMap,unusedMap,releaseActor,drawFrameSprites,writeFrameSprite,sceneVideo,&output);
  if(boundary==WILLOW_ATTRACT_PANEL)panelWrites+=clearWrites-priorVideoWrites;
  if(!boundary){fprintf(stderr,"Unsupported scene at frame %u state %u\n",frame,busWord(0x8008));return 1;}
  RUN(0x8266,0);
  for(unsigned n=0;n<65536;n++)if(!privateContext(n)&&canonicalByte(ram,n)!=expected[n]){
   fprintf(stderr,"Scene phase mismatch frame %u at %x actual %x expected %x\n",frame,n,canonicalByte(ram,n),expected[n]);return 1;
  }
  if(busByte(0xc0)==12&&busWord(0xd0)==52)messages|=1;
  if(busByte(0xc0)==12&&busWord(0xd0)==53)messages|=2;
  if(busByte(0xe0)==12&&busWord(0xf0)==112)panMessage=1;
  if(busByte(0xe0)==12&&busWord(0xf0)==114)reverseMessages|=1;
  if(busByte(0xc0)==12&&busWord(0xd0)==115)reverseMessages|=2;
  if(busWord(0x8004)==4&&busByte(0xc0)==12&&busWord(0xd0)==60)textMessages|=1;
  if(busWord(0x8004)==4&&busByte(0xc0)==12&&busWord(0xd0)==61)textMessages|=2;
  uint16_t cursor=0,remaining=15;
  for(;;){
   WillowNativeTaskRun run=willowNativeTaskRunOneWithVideo(&runtime,&bus,cursor,remaining,paletteWord,colorRead,colorWrite,0,0,sceneVideo);
   if(run.unsupported){fprintf(stderr,"Unsupported task %x frame %u\n",run.cursor,frame);return 1;}
   if(!run.dispatched)break;
   dispatches++;if(run.remaining==65535)break;cursor=run.cursor;remaining=run.remaining;
  }
  m68k_set_reg(8,0xff0000);RUN(0x14d8,15);
  for(unsigned n=0;n<65536;n++)if(!privateContext(n)&&canonicalByte(ram,n)!=expected[n]){
   fprintf(stderr,"Task phase mismatch frame %u at %x actual %x expected %x\n",frame,n,canonicalByte(ram,n),expected[n]);return 1;
  }
  willowArcadeTaskWake(&bus);RUN(0x1590,0);
  for(unsigned n=0;n<65536;n++)if(!privateContext(n)&&canonicalByte(ram,n)!=expected[n]){
   fprintf(stderr,"Wake phase mismatch frame %u at %x\n",frame,n);return 1;
  }
  fprintf(trace,"%u,%u,%u,%u,%u,%u\n",frame,busWord(0x8008),busWord(0x800a),busByte(0xe58),busWord(0x82c0),busWord(0x20c));
  if(schedule)fprintf(schedule,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",frame,
   busWord(0x8004),busWord(0x8008),busWord(0x800a),busByte(0xe58),busWord(0x82c0),busWord(0x20c),
   busWord(0x8034),busWord(0x8038),busByte(0xc0),busByte(0xe0),busWord(0xd0),busWord(0xf0),busByte(0xf890),busWord(0x802e),busWord(0x8030));
  if(scroll1&&fwrite(retainedScroll1,1,sizeof retainedScroll1,scroll1)!=sizeof retainedScroll1)return 2;
  if(colors&&fwrite(expected+0x2000,1,6144,colors)!=6144)return 2;
  if(controllers){uint8_t data[1024];for(unsigned n=0;n<1024;n++)data[n]=busByte(0xf042+n);if(fwrite(data,1,sizeof data,controllers)!=sizeof data)return 2;}
  checks++;
 }
 if(controllerInput&&!controllerInputUsed)return 2;
 if(colors)fclose(colors);
 if(scroll1)fclose(scroll1);
 if(controllers)fclose(controllers);
 if(schedule)fclose(schedule);
 fclose(trace);
 if((phaseFourPanel&&(panelWrites!=4176||busWord(0x802e)||busWord(0x8030)!=(phaseFour?0x100:phaseFourScroll?0x700:0x740)||busWord(0x800a)!=(demoStart?900:phaseFour?0:phaseFourExit?6:phaseFourScroll?0:phaseFourMap?8:phaseFourHold?28:8)))||(phaseFourText&&textMessages!=3)||visited!=(phaseFour?1048575u:phaseFourExit?131071u:phaseTwo?16383u:panFade?127u:63u)||messages!=3||mapWrites!=(phaseFourMap?4866u:phaseTwo?3842u:panSetup?2562u:1282u)||frame==6000||busWord(0x8004)!=(phaseFour?6:phaseTwo?4:2)||
    (panMove?(busWord(0x8008)!=(demoStart?4:demoInit?2:phaseFour?0:phaseFourExit?34:phaseFourReplace?28:phaseFourBlink?26:phaseFourScroll?22:phaseFourMap?16:phaseFourHold?14:phaseFourPanel?10:phaseFourText?8:phaseTwo?0:panFade?14:6)||busWord(0x8038)!=(phaseTwo?0x202:0x1fe)||busWord(0x8034)!=(phaseFourMap?0:phaseTwo?0x102:0xfe)||busByte(0xe58)||busByte(0xc0)||busByte(0xe0)||busWord(0x20c)!=(demoStart?6:demoInit?7:phaseFourExit?8:phaseFourReplace?1:phaseFourBlink?4:8)||!panMessage||scrollSteps!=43||(phaseTwo&&(reverseSteps!=43||reverseMessages!=3))):panSetup?(busWord(0x8008)!=2||busByte(0xe58)!=1||busByte(0xc0)!=1||busWord(0x20c)!=7):
     (busWord(0x8008)||busByte(0xe58)||busByte(0xc0)||busWord(0x20c)!=8))){fprintf(stderr,"Scene failed to complete cleanly\n");return 1;}
 if(phaseFourHold&&!demoInit){
  unsigned matching=0;
  for(unsigned at=0xf042;at<0xf442;at+=64)if(busByte(at)&&busWord(at+52)==0xe&&busWord(at+54)==0x7aa0){
   if(busByte(at+56)!=16||busByte(at+59)!=9||busByte(at+58)!=4||busByte(at+32)||busByte(at+60)!=1)return 1;
   matching++;
  }
  if(matching!=1)return 1;
 }
 if(demoInit&&getenv("WILLOW_DEMO_ZERO_POINTER")){
  if(busWord(0x828a)||busWord(0x828c)){fputs("Zero-pointer lifecycle changed saved pointer\n",stderr);return 1;}
  printf("Demo zero-pointer fixture: original ROM[0]=%04x, saved pointer preserved through source lifecycle; startup prefix not covered\n",willowMotionWord(rom));
 }
 if(demoStart){
  FILE*out=fopen(demoVariant?"build/opening-demo-start-1-state.bin":"build/opening-demo-start-0-state.bin","wb");
  if(!out||fwrite(expected,1,sizeof expected,out)!=sizeof expected||fclose(out))return 2;
 }
 const char*continuation=getenv("WILLOW_DEMO_CONTINUATION_OUTPUT");
 if(continuation){
  if(!demoStart||!exportContinuation(continuation,ram,&runtime))return 2;
  printf("Demo continuation exported after verified scan/wake: %s; native labels and independent original CPU contexts retained\n",continuation);
 }
 printf("Phase-four panel setup: %u writes\n",panelWrites);
 printf("Phase-four opening text complete: %s\n",phaseFourText?"yes":"no");
 printf("Complete phase two: %s; %u reverse scroll steps\n",phaseTwo?"yes":"no",reverseSteps);
 printf("Post-pan fade included: %s\n",panFade?"yes":"no");
 printf("Camera movement included: %s; %u scroll steps\n",panMove?"yes":"no",scrollSteps);
 printf("Next-phase setup included: %s\n",panSetup?"yes":"no");
 printf("Opening lifecycle: %u scene/scan/wake cycles, %u task dispatches; first scene completes through original message and fade signals with matching RAM, palettes and sprite buffers; %u map writes issued; max %u oracle slices\n",checks,dispatches,mapWrites,maxSlices);
 return 0;
}
