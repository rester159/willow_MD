#include "../src/willow_arcade_opening_actors.h"
#include "../src/willow_arcade_sprite_frame.h"
#include "../src/willow_arcade_sprite_writer.h"
#include "../src/willow_native_class4_visibility.h"
#include "../src/willow_native_pool_auxiliary.h"
static uint16_t openingColors[3072];
static unsigned char*openingExpected;
static unsigned openingOwner,openingCopies,openingTicks;
static void(*openingFrame)(void);
static void openingCountFrame(void){openingTicks++;openingFrame();}
static uint8_t*openingResolve(uint16_t at){return nptSource+at;}
static uint16_t openingWord(unsigned long at){return bfpWord(at);}
static uint16_t openingRead(uint32_t at){return openingColors[(at-0x91c000)/2];}
static void openingWrite(uint32_t at,uint16_t value){openingColors[(at-0x91c000)/2]=value;}
static void openingCopy(uint32_t source,uint32_t destination){
 unsigned group=(destination-0x91c000)/32,offset=0;
 for(unsigned n=0;n<16;n++){
  uint16_t base=bfpWord(source+n*2),value=base|(openingRead(destination+n*2)&0xf000);
  unsigned delta=(uint16_t)(value-base)>>12;
  if(n&&delta!=offset){fprintf(stderr,"Opening fixture needs exceptional palette storage\n");exit(1);}
  offset=delta;openingWrite(destination+n*2,value);
 }
 sceneCommitPut(openingExpected,openingOwner+group*2,(source-0xe0000)/2);
 unsigned at=(openingOwner+384+group/2)^1;
 openingExpected[at]=(openingExpected[at]&((group&1)?15:240))|(offset<<((group&1)?4:0));
 openingCopies++;
}
static void openingRelease(uint8_t*r){
 (void)r;fprintf(stderr,"Unexpected release in active opening animation fixture\n");exit(1);
}
static uint8_t*openingObjects;
static WillowArcadeSpriteFrame openingSpriteFrame;
static unsigned openingPieces;
static void openingObjectWrite(uint32_t at,uint16_t value){
 unsigned offset;
 if(at>=0x900000&&at<0x900800)offset=at-0x900000;
 else if(at>=0x908000&&at<0x908800)offset=0x800+at-0x908000;
 else{fprintf(stderr,"Opening object address %x\n",at);exit(1);}
 willowMotionPutWord(openingObjects+offset,value);
}
static void openingSpriteEmit(uint16_t x,uint16_t y,uint16_t code,uint16_t attr){
 uint32_t at=openingSpriteFrame.cursor;
 openingObjectWrite(at,x);openingObjectWrite(at+2,y);openingObjectWrite(at+4,code);openingObjectWrite(at+6,attr);
 openingSpriteFrame.cursor+=8;openingPieces++;
}
static void openingSpriteDraw(uint16_t handle){
 WillowArcadeSpriteWriter writer={bfpWord,openingSpriteEmit,nptRead(0x829a),openingSpriteFrame.remaining};
 willowArcadeSprite(&writer,nptSource+handle,nptRead(0x8408),nptRead(0x840c));
 nptWrite(0x829a,writer.areaBudget);openingSpriteFrame.remaining=writer.pieceBudget;
}
static int nativeOpeningActorsCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),unsigned countdown)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],saved[65536];
 if(!sram)return 2;
 setVideo(nativeRngVideo);memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);
 openingExpected=expected;openingOwner=symbol(symbols,"debugPaletteOwner");openingCopies=0;openingPieces=0;openingObjects=saved;
 unsigned globals=symbol(symbols,"sourceBootGlobals"),palette=symbol(symbols,"debugPaletteControllers");
 unsigned primary=symbol(symbols,"debugScroll2Record"),pools=symbol(symbols,"debugActorPools");
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState");
 unsigned slots=symbol(symbols,"debugPaletteSlots");
 for(unsigned n=0;n<2;n++)if((unsigned short)word(ram,slots+n*34)!=65535)return 2;
 memset(nptSource,0,sizeof nptSource);
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 for(unsigned n=0;n<128;n++)nptSource[0x8402+n]=ram[(primary+n)^1];
 for(unsigned n=0;n<1024;n++)nptSource[0xf042+n]=ram[(palette+n)^1];
 const unsigned order[]={4,0,1,5,6,2,7,3};unsigned records[9];
 for(unsigned i=0;i<9;i++){
  unsigned pool=i==8?symbol(symbols,"tileIndex")+4648:pools+order[i]*12;
  const WillowActorPoolSpec*spec=willowActorPoolSpecification((i+1)*2);
  records[i]=longword(ram,pool);
  for(unsigned n=0;n<spec->capacity*spec->stride;n++)nptSource[spec->sourceBase+n]=*nativeTargetByte(ram,sram,records[i]+n);
 }
 for(unsigned n=0;n<128;n++)nptSource[0x9846+n]=ram[(symbol(symbols,"debugPlayerRecord")+n)^1];
 for(unsigned at=0;at<65536;at++){
  uint8_t*p=willowNativePoolAuxiliaryResolve((WillowNativePoolAuxiliary*)(saved+0x8000),at,1);
  if(p)nptSource[at]=*p;
  p=willowNativeClass4VisibilityResolve((WillowNativeClass4Visibility*)(saved+0x8040),at,1);
  if(p)nptSource[at]=*p;
 }
 nptSource[0x83c3]=saved[0x8035];
 for(unsigned n=0;n<3072;n++){
  unsigned g=n/16,tag=(unsigned short)word(ram,openingOwner+g*2);
  if(tag>0xfff0)return 2;
  unsigned offset=(ram[(openingOwner+384+g/2)^1]>>((g&1)?4:0))&15;
  openingColors[n]=bfpWord(0xe0000+tag*2+(n%16)*2)+(offset<<12);
 }
 /* Rejection must precede palette advancement and visibility writes. */
 for(unsigned i=0;i<9;i++){
  unsigned at=records[i];uint8_t old=*nativeTargetByte(ram,sram,at),selector=*nativeTargetByte(ram,sram,at+32);
  *nativeTargetByte(ram,sram,at)=*nativeTargetByte(expected,saved,at)=1;*nativeTargetByte(ram,sram,at+32)=*nativeTargetByte(expected,saved,at+32)=0;
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceOpeningActorsProbe",0,NULL)||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  if(countdown){
   unsigned args[]={0x20,0};
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
      memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  }
  *nativeTargetByte(ram,sram,at)=*nativeTargetByte(expected,saved,at)=old;*nativeTargetByte(ram,sram,at+32)=*nativeTargetByte(expected,saved,at+32)=selector;
 }
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 WillowPaletteAnimation g={0};g.byte=spawnByte;g.word=bfpWord;g.read=openingRead;g.write=openingWrite;g.copy=openingCopy;
 unsigned words=0,peakFrames=0,updates=countdown?(countdown==5?29:countdown==4?9:countdown==3?32:countdown==2?59:179):512;openingFrame=runFrame;
 unsigned liveD1=4,liveD2=16;
 for(unsigned frame=0;frame<updates;frame++){
  unsigned initializing=!nptSource[0xb648];
  if(countdown&&!(nptRead(0x8008)>=6&&nptByte(0xe58)))nptWrite(0x800a,nptRead(0x800a)-1);
  if(!willowArcadeOpeningActors(&bus,openingResolve,&g,openingWord,openingRelease))return 1;
  for(unsigned i=0;i<9;i++){
   const WillowActorPoolSpec*spec=willowActorPoolSpecification((i+1)*2);
   for(unsigned n=0;n<spec->capacity*spec->stride;n++)*nativeTargetByte(expected,saved,records[i]+n)=nptSource[spec->sourceBase+n];
  }
  for(unsigned n=0;n<1024;n++)expected[(palette+n)^1]=nptSource[0xf042+n];
  for(unsigned at=0;at<65536;at++){
   uint8_t*p=willowNativePoolAuxiliaryResolve((WillowNativePoolAuxiliary*)(saved+0x8000),at,1);
   if(p)*p=nptSource[at];
   p=willowNativeClass4VisibilityResolve((WillowNativeClass4Visibility*)(saved+0x8040),at,1);
   if(p)*p=nptSource[at];
  }
  openingTicks=0;
  unsigned result=1;
  if(!countdown)result=nativeSATCall(ram,symbols,setReg,getReg,openingCountFrame,"sourceOpeningActorsProbe",0,NULL);
  if(openingTicks>peakFrames)peakFrames=openingTicks;
  if(!countdown&&(result!=1||memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))){
   fprintf(stderr,"Native opening actors frame %u result %x PC %x SP %x\n",frame,result,getReg(16),getReg(15));
   for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Opening RAM %04x %02x/%02x\n",n^1,ram[n],expected[n]);break;}
   for(unsigned n=0;n<65536;n++)if(saved[n]!=sram[n]){fprintf(stderr,"Opening SRAM %04x %02x/%02x\n",n,sram[n],saved[n]);break;}
   return 1;
  }
  if(!countdown&&frame==1){
   uint8_t rejectRAM[0xf600],rejectSRAM[65536];
   for(unsigned test=0;test<6;test++){
    if(test==0)sram[0x8001]=1; /* Unsupported class-2 lane. */
    if(test==1)willowMotionPutWord(sram+0x8013,0x9409); /* Unaligned cursor. */
    if(test==2)nativeTargetPutWord(ram,sram,records[1]+49*128+20,0); /* Missing frame. */
    if(test==3)willowMotionPutWord(sram+0x8040+0x9408-0x93a6,0x9dc7);
    if(test==4)ram[symbol(symbols,"debugPlayerRecord")^1]=1;
    if(test==5)sram[0x8003]=51;
    memcpy(rejectRAM,ram,sizeof rejectRAM);memcpy(rejectSRAM,sram,sizeof rejectSRAM);
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceOpeningPublishProbe",0,NULL)||
       memcmp(rejectRAM,ram,sizeof rejectRAM)||memcmp(rejectSRAM,sram,sizeof rejectSRAM))return 1;
    memcpy(ram,expected,sizeof expected);memcpy(sram,saved,sizeof saved);
   }
  }
  openingSpriteFrame=willowArcadeSpriteFrameBegin(&bus);
  willowArcadeVisibilityRender(&bus,openingSpriteDraw,openingSpriteDraw);
  willowArcadeSpriteFrameClear(&bus,&openingSpriteFrame,openingObjectWrite);
  willowArcadeSpriteFramePublish(&bus);
  willowMotionPutLong(saved+0x8170,openingSpriteFrame.cursor);
  willowMotionPutWord(saved+0x8174,openingSpriteFrame.remaining);
  for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
   const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
   for(unsigned n=0;n<g->count;n++)expected[(globals+g->offset+n)^1]=nptSource[g->source+n];
  }
  for(unsigned at=0;at<65536;at++){
   uint8_t*p=willowNativePoolAuxiliaryResolve((WillowNativePoolAuxiliary*)(saved+0x8000),at,1);
   if(p)*p=nptSource[at];
  }
  unsigned args[]={0x20,0};
  if(countdown){
   liveD1=initializing?8:192;liveD2=!initializing?bfpWord(willowMotionLong(nptSource+0xb646+20)+2):liveD2;
   willowMotionPutWord(saved+0x8176,liveD1);willowMotionPutWord(saved+0x8178,liveD2);
   sceneCommitPut(expected,control+4,0xff);sceneCommitPut(expected,control+6,0x20);
   sceneCommitPut(expected,task+0x20,0x0101);sceneCommitPut(expected,task+0x22,liveD1);
   sceneCommitPut(expected,task+0x24,liveD2);expected[(globals+111)^1]|=12;
   nptPut(0x8296,nptByte(0x8296)|12);
  }
  openingTicks=0;
  result=nativeSATCall(ram,symbols,setReg,getReg,openingCountFrame,countdown?"sourceSceneBootTaskDispatch":"sourceOpeningPublishProbe",countdown?2:0,args);
  if(openingTicks>peakFrames)peakFrames=openingTicks;
  if(result!=(countdown?0x424f:1)||memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
   fprintf(stderr,"Native opening publication frame %u result %x\n",frame,result);
   for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Publication RAM %04x %02x/%02x\n",n^1,ram[n],expected[n]);break;}
   for(unsigned n=0;n<65536;n++)if(saved[n]!=sram[n]){fprintf(stderr,"Publication SRAM %04x %02x/%02x\n",n,sram[n],saved[n]);break;}return 1;
  }
  if(countdown){
   for(unsigned n=0;n<512;n+=32)if(expected[(task+n)^1]==1){
    unsigned at=(task+n+1)^1;expected[at]--;
    if(!expected[at])expected[(task+n)^1]=4;
   }
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
      memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  }
  for(unsigned n=0;n<3072;n++){
   unsigned group=n/16,tag=(unsigned short)word(ram,openingOwner+group*2);
   if(tag>0xfff0)return 1;
   unsigned offset=(ram[(openingOwner+384+group/2)^1]>>((group&1)?4:0))&15;
   if((uint16_t)(bfpWord(0xe0000+tag*2+(n%16)*2)+(offset<<12))!=openingColors[n])return 1;
   words++;
  }
 }
 setVideo(video);
 if(!countdown)puts("Resident publication rejection: six invalid states preserve all RAM/SRAM");
 if(countdown)printf("Resident opening countdown: %u dispatches (timer %u, busy %u) with complete actor/publication effects, live D1/D2 and task sleep/wake preserved\n",updates,nptRead(0x800a),nptByte(0xe58));
 printf("Resident object publication: %u source sprite pieces, both banks, stale-entry cleanup and queue consumption match without injected queue resets; hardware latch/SAT publication remains separate\n",openingPieces);
 printf("Resident opening actors: %u consecutive actor/object-bank updates, %u C92 copies, %u exact palette words; actor animation and retained visibility payloads match; nine unsupported populations reject; full unrelated RAM/SRAM retained; peak %u observed emulator frames (not a complete game-frame timing pass)\n",updates,openingCopies,words,peakFrames);
 return 0;
}
