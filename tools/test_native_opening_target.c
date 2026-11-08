/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_opening_scene_run.h"
#include "../src/willow_native_task_runtime.h"
#include "../src/willow_arcade_pool_init.h"
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
static unsigned mapWrites;
static void unusedMap(uint16_t at,uint16_t value){(void)at;(void)value;mapWrites++;}
static unsigned clearWrites;
static void sceneVideo(uint32_t at,uint16_t value){
 clearWrites++;
 if((at>=0x900000&&at<0x900800)||(at>=0x908000&&at<0x908800))writeFrameSprite(at,value);
}
static int privateContext(unsigned at){
 return (at>=0xb20&&at<0xb26)||(at>=0xfc00&&at<0xff04);
}
int main(int argc,char**argv){
 if(argc!=2&&(argc!=3||(strcmp(argv[2],"--zero-retained")&&strcmp(argv[2],"--pan-setup")&&strcmp(argv[2],"--pan-move")&&strcmp(argv[2],"--pan-fade")&&strcmp(argv[2],"--phase-two")&&strcmp(argv[2],"--phase-four-text")&&strcmp(argv[2],"--phase-four-panel")&&strcmp(argv[2],"--phase-four-hold")&&strcmp(argv[2],"--phase-four-map")&&strcmp(argv[2],"--phase-four-scroll")&&strcmp(argv[2],"--phase-four-blink")&&strcmp(argv[2],"--phase-four-replace")&&strcmp(argv[2],"--phase-four-exit")&&strcmp(argv[2],"--phase-four")&&strcmp(argv[2],"--demo-init-0")&&strcmp(argv[2],"--demo-init-1")&&strcmp(argv[2],"--demo-start-0")&&strcmp(argv[2],"--demo-start-1"))))return 2;
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
 unsigned panSetup=panMove||(argc==3&&!strcmp(argv[2],"--pan-setup"));
 unsigned dirtyRetained=argc==2||panSetup;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;

 f=fopen("build/opening_runtime_probe.bin","rb");if(!f)return 2;
 if(!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
 f=fopen("build/opening_runtime_probe.sym","r");if(!f)return 2;
 unsigned entry=0,groupEntry=0,address;char line[512],kind,name[256];
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3){
  if(!strcmp(name,"openingRuntimeProbe"))entry=address;
  if(!strcmp(name,"paletteGroupProbe"))groupEntry=address;
 }
 fclose(f);if(!entry||!groupEntry)return 2;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/opening-target-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/opening-target-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 unsigned maxSlices=0,lastSlices=0,histogram[33]={0};
#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at)); \
 unsigned slices=0; \
 for(;slices<32&&m68k_get_reg(16)!=0x100000;slices++){retro_run();if(frame==0)fprintf(stderr,"Init slice %u PC %x\n",slices+1,m68k_get_reg(16));} \
 lastSlices=slices; \
 if(slices>maxSlices)maxSlices=slices; \
 if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Source stalled at %x\n",m68k_get_reg(16));return 1;} \
} while(0)
 /* Exhaust the 16-bit domain for each signed brightness step on target.
  * Sentinel words verify that the unrolled kernel touches exactly 16 words. */
 if(dirtyRetained&&!panSetup){
  unsigned frame=1;
  for(unsigned direction=0;direction<2;direction++)for(unsigned base=0;base<65536;base+=16){
   unsigned delta=direction?0xf000:0x1000;
   putRamWord(ram,0xb28,delta);putRamWord(ram,0x1ffe,0x1234);putRamWord(ram,0x2020,0xabcd);
   for(unsigned n=0;n<16;n++)putRamWord(ram,0x2000+n*2,base+n);
   RUN(groupEntry,0);
   for(unsigned n=0;n<16;n++){
    unsigned at=0x2000+n*2,value=((unsigned)ram[at^1]<<8)|ram[(at+1)^1];
    if(value!=((base+n+delta)&65535))return 1;
   }
   if(ram[0x1ffe^1]!=0x12||ram[0x1fff^1]!=0x34||ram[0x2020^1]!=0xab||ram[0x2021^1]!=0xcd)return 1;
  }
  puts("Compiled fade kernel: 131072 word results and adjacent sentinels match wrapping arithmetic");
  maxSlices=0;lastSlices=0;
 }
 unsigned checks=0,dispatches=0;
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 memset(expected,0,sizeof expected);
 if(dirtyRetained)for(unsigned n=0x2000;n<0x3800;n++)expected[n]=(n*19)^0x73;
 /* Dirty every range cleared by 2648 so missing/short bulk clears cannot
  * pass merely because the fixture happened to begin with zeroed records. */
 for(unsigned n=0x8502;n<0x9102;n++)expected[n]=(n*31)^0xa5;
 for(unsigned n=0xf042;n<0xf442;n++)expected[n]=(n*31)^0xa5;
 for(unsigned n=0x9846;n<0x98c6;n++)expected[n]=(n*31)^0xa5;
 for(unsigned id=2;id<=18;id+=2){
  const WillowActorPoolSpec*p=willowActorPoolSpecification(id);
  for(unsigned n=0;n<p->capacity*p->stride;n++)expected[p->sourceBase+n]=(n*31)^0xa5;
 }
 willowArcadeTaskInitFree(&bus,paletteWord);
 for(unsigned n=0;n<16;n++){busPutWord(n*32+12,0xff);busPutWord(n*32+14,0x1000+n*256);}
 for(unsigned group=0;group<4;group++)willowArcadeVisibilityInit(&bus,group,0);
 busPutByte(0x8293,1);
 if(demoInit){busPutWord(0xe5a,demoVariant);busPutWord(0x828a,0);busPutWord(0x828c,0x183c);}
 WillowNativeTaskRuntime runtime={0};
 WillowPaletteAnimation g={0};g.byte=sourceByte;g.word=paletteWord;g.read=colorRead;g.write=colorWrite;
 for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
 FILE*trace=fopen(demoStart?(demoVariant?"build/opening-target-demo-start-1-trace.csv":"build/opening-target-demo-start-0-trace.csv"):demoInit?(demoVariant?"build/opening-target-demo-init-1-trace.csv":"build/opening-target-demo-init-0-trace.csv"):phaseFour?"build/opening-target-phase-four-trace.csv":phaseFourExit?"build/opening-target-phase-four-exit-trace.csv":phaseFourReplace?"build/opening-target-phase-four-replace-trace.csv":phaseFourBlink?"build/opening-target-phase-four-blink-trace.csv":phaseFourScroll?"build/opening-target-phase-four-scroll-trace.csv":phaseFourMap?"build/opening-target-phase-four-map-trace.csv":phaseFourHold?"build/opening-target-phase-four-hold-trace.csv":phaseFourPanel?"build/opening-target-phase-four-panel-trace.csv":phaseFourText?"build/opening-target-phase-four-text-trace.csv":phaseTwo?"build/opening-target-phase-two-trace.csv":panFade?"build/opening-target-pan-fade-trace.csv":panMove?"build/opening-target-pan-move-trace.csv":panSetup?"build/opening-target-pan-trace.csv":dirtyRetained?"build/opening-target-trace.csv":"build/opening-target-zero-trace.csv","w");if(!trace)return 2;
 fputs("cycle,scene,timer,message_busy,fade_complete,free_tasks,target_slices\n",trace);
 unsigned frame,visited=0,messages=0,panMessage=0,scrollSteps=0,reverseSteps=0,reverseMessages=0,textMessages=0,panelWrites=0;
 for(frame=0;frame<6000&&(busWord(0x8004)==0||(demoInit&&busWord(0x8004)==6&&busWord(0x8008)<=(demoStart?2:0))||(phaseFourText&&busWord(0x8004)==4&&busWord(0x8008)<=(phaseFour?38:phaseFourExit?32:phaseFourReplace?26:phaseFourBlink?24:phaseFourScroll?20:phaseFourMap?14:phaseFourHold?12:phaseFourPanel?8:6))||(panSetup&&busWord(0x8004)==2&&(phaseTwo||(panMove?busWord(0x8008)<=(panFade?12:4):!busWord(0x8008)))));frame++){
  if(panMove&&busWord(0x8004)==2&&busWord(0x8008)==4&&!busByte(0xe58)&&busWord(0x8038)>0x200)scrollSteps++;
  if(phaseTwo&&busWord(0x8004)==2&&busWord(0x8008)==18&&!busByte(0xe58))reverseSteps++;
  visited|=1u<<(busWord(0x8008)/2);
  WillowArcadeSpriteFrame output;
  unsigned priorVideoWrites=clearWrites;
  uint8_t boundary=willowArcadeOpeningSceneRun(&bus,resolve,&g,sourceWord,sourceByte,paletteWord,
   colorWrite,unusedMap,unusedMap,releaseActor,drawFrameSprites,writeFrameSprite,sceneVideo,&output);
  if(boundary==WILLOW_ATTRACT_PANEL)panelWrites+=clearWrites-priorVideoWrites;
  if(!boundary){fprintf(stderr,"Unsupported scene at frame %u state %u\n",frame,busWord(0x8008));return 1;}
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
  willowArcadeTaskWake(&bus);
  for(unsigned n=0;n<16;n++)busPutWord(0xb00+n*2,runtime.resume[n]);
  RUN(entry,0);
  histogram[lastSlices]++;
  if(((unsigned)ram[0xb24^1]<<8|ram[0xb25^1])!=0x4f50){fprintf(stderr,"Target rejected frame %u\n",frame);return 1;}
  for(unsigned n=0;n<65536;n++)if(!privateContext(n)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Target mismatch frame %u at %x actual %x expected %x\n",frame,n,ram[n^1],expected[n]);return 1;
  }
  fprintf(trace,"%u,%u,%u,%u,%u,%u,%u\n",frame,busWord(0x8008),busWord(0x800a),busByte(0xe58),busWord(0x82c0),busWord(0x20c),lastSlices);
  checks++;
 }
 fclose(trace);
 if((phaseFourPanel&&(panelWrites!=4176||busWord(0x802e)||busWord(0x8030)!=(phaseFour?0x100:phaseFourScroll?0x700:0x740)||busWord(0x800a)!=(demoStart?900:phaseFour?0:phaseFourExit?6:phaseFourScroll?0:phaseFourMap?8:phaseFourHold?28:8)))||(phaseFourText&&textMessages!=3)||visited!=(phaseFour?1048575u:phaseFourExit?131071u:phaseTwo?16383u:panFade?127u:63u)||messages!=3||mapWrites!=(phaseFourMap?4866u:phaseTwo?3842u:panSetup?2562u:1282u)||frame==6000||busWord(0x8004)!=(phaseFour?6:phaseTwo?4:2)||
    (panMove?(busWord(0x8008)!=(demoStart?4:demoInit?2:phaseFour?0:phaseFourExit?34:phaseFourReplace?28:phaseFourBlink?26:phaseFourScroll?22:phaseFourMap?16:phaseFourHold?14:phaseFourPanel?10:phaseFourText?8:phaseTwo?0:panFade?14:6)||busWord(0x8038)!=(phaseTwo?0x202:0x1fe)||busWord(0x8034)!=(phaseFourMap?0:phaseTwo?0x102:0xfe)||busByte(0xe58)||busByte(0xc0)||busByte(0xe0)||busWord(0x20c)!=(demoStart?6:demoInit?7:phaseFourExit?8:phaseFourReplace?1:phaseFourBlink?4:8)||!panMessage||scrollSteps!=43||(phaseTwo&&(reverseSteps!=43||reverseMessages!=3))):panSetup?(busWord(0x8008)!=2||busByte(0xe58)!=1||busByte(0xc0)!=1||busWord(0x20c)!=7):
     (busWord(0x8008)||busByte(0xe58)||busByte(0xc0)||busWord(0x20c)!=8))){fprintf(stderr,"Scene failed to complete cleanly\n");return 1;}
 for(unsigned n=1;n<=32;n++)if(histogram[n])printf("Target execution: %u updates took %u emulator slices\n",histogram[n],n);
 if(phaseFourHold&&!demoInit){
  unsigned matching=0;
  for(unsigned at=0xf042;at<0xf442;at+=64)if(busByte(at)&&busWord(at+52)==0xe&&busWord(at+54)==0x7aa0){
   if(busByte(at+56)!=16||busByte(at+59)!=9||busByte(at+58)!=4||busByte(at+32)||busByte(at+60)!=1)return 1;
   matching++;
  }
  if(matching!=1)return 1;
 }
 printf("Phase-four panel setup: %u writes\n",panelWrites);
 printf("Phase-four opening text complete: %s\n",phaseFourText?"yes":"no");
 printf("Complete phase two: %s; %u reverse scroll steps\n",phaseTwo?"yes":"no",reverseSteps);
 printf("Post-pan fade included: %s\n",panFade?"yes":"no");
 printf("Camera movement included: %s; %u scroll steps\n",panMove?"yes":"no",scrollSteps);
 printf("Next-phase setup included: %s\n",panSetup?"yes":"no");
 printf("Retained palette fixture: %s\n",dirtyRetained?"dirty":"zero");
 printf("Compiled opening lifecycle: %u scene/scan/wake cycles, %u task dispatches; first scene completes through original message and fade signals with matching RAM, palettes and sprite buffers; %u map writes issued; max %u oracle slices\n",checks,dispatches,mapWrites,maxSlices);
 return 0;
}
