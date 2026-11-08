#include "../src/willow_arcade_attract_pan_init.h"
#include "../src/willow_arcade_attract_pan_step.h"
#include "../src/willow_arcade_opening_palette_live.h"
#include "../src/willow_arcade_attract_pan_wait.h"
static uint8_t panFar[16384];static unsigned panFarWrites;
static void panFarWrite(uint16_t at,uint16_t value){willowMotionPutWord(panFar+at,value);panFarWrites++;}
/* Message tasks execute to completion through the resident dispatcher. */
static int nativeOpeningPanMotionCheck(void*core,uint8_t*ram,uint8_t*sram,const char*symbols,
 void(*setReg)(int,unsigned),unsigned(*getReg)(int),void(*runFrame)(void),unsigned second)
{
 uint8_t expected[0xf600],saved[65536];
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState");
 unsigned globals=symbol(symbols,"sourceBootGlobals"),palette=symbol(symbols,"debugPaletteControllers");
 const unsigned src[]={0,0x200,0xa10,0xf042},dst[]={task,control,control+16,palette},sizes[]={512,16,32,1024};
 memset(nptSource,0,sizeof nptSource);
 for(unsigned r=0;r<4;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[src[r]+n]=ram[(dst[r]+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 WillowNativeTaskRuntime runtime;
 for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);
 unsigned args[]={0x20,0},calls=0;
 while(nptRead(0x8008)!=(second?20:6)){
  if(calls>80)return 1;
  if(calls==12||calls==25){
   if(nativeOpeningMessageCheck(core,ram,symbols,runFrame,getReg,calls==12?0xc0:0xe0))return 1;
   /* The message fixture also advances all source wake counters. Import its
    * independently checked task completion rather than injecting a busy flag. */
   for(unsigned r=0;r<4;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[src[r]+n]=ram[(dst[r]+n)^1];
   for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
    const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
    for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
   }
   for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
   memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);
   void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");setVideo(nativeRngVideo);
   if(nptByte(0xe58))return 1;
  }
  unsigned state=nptRead(0x8008),d1=bfpWord(0x8526+state),d2=nptRead(0x24);
  if(!nptByte(0xe58)&&(state==2||state==16)){d1=second?0x72:0x70;d2=second?0x240c:0x240a;}
  if(!nptByte(0xe58)&&((state==4&&nptRead(0x8038)<=0x200)||(state==18&&nptRead(0x8038)>=0x1f8))){
   uint8_t rejected[0xf600];
   /* Downward needs one free controller; upward needs two, not just one. */
   for(unsigned n=0;n<16;n++)ram[(palette+n*64)^1]=second&&n==0?0:1;
   memcpy(rejected,ram,sizeof rejected);
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
      memcmp(rejected,ram,sizeof rejected)||memcmp(saved,sram,sizeof saved))return 1;
   memcpy(ram,expected,sizeof expected);
  }
  if(!willowArcadeAttractPanStep(&bus,state))return 1;
  nptWrite(0x204,0xff);nptWrite(0x206,0x20);nptWrite(0x20,0x0101);
  nptWrite(0x22,d1);nptWrite(0x24,d2);nptPut(0x8296,nptByte(0x8296)|12);
  objectTaskScatter(expected,&runtime,control,task,globals);
  for(unsigned n=0;n<1024;n++)expected[(palette+n)^1]=nptSource[0xf042+n];
  willowMotionPutWord(saved+0x8176,d1);willowMotionPutWord(saved+0x8178,d2);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
   fprintf(stderr,"Resident pan motion %u call %u state %u mismatch\n",second,calls,state);
   for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Motion RAM %x %x/%x\n",n^1,ram[n],expected[n]);break;}return 1;
  }
  for(unsigned n=0;n<512;n+=32)if(nptByte(n)==1){nptPut(n+1,nptByte(n+1)-1);if(!nptByte(n+1))nptPut(n,4);}
  objectTaskScatter(expected,&runtime,control,task,globals);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  calls++;
 }
 if(calls!=(second?68:69)||nptRead(0x8038)!=(second?0x202:0x1fe)||
    nptRead(0x8034)!=(second?0x102:0xfe)||nptRead(0x800a)!=30)return 1;
 sceneCommitPut(ram,globals+20,28);sceneCommitPut(expected,globals+20,28);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 sceneCommitPut(ram,globals+20,second?20:6);
 printf("Resident pan motion %u: %u dispatch/wake pairs, full RAM/SRAM and live registers match; resident message completion, terminal palette records and unsupported continuation checked\n",second,calls);
 return 0;
}
static int nativePanPaletteCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),unsigned second)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],saved[65536];
 if(!sram)return 2;
 unsigned palette=symbol(symbols,"debugPaletteControllers"),primary=symbol(symbols,"debugScroll2Record");
 unsigned slots=symbol(symbols,"debugPaletteSlots"),globals=symbol(symbols,"sourceBootGlobals");
 openingOwner=symbol(symbols,"debugPaletteOwner");openingExpected=expected;openingCopies=0;
 memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);
 for(unsigned n=0;n<1024;n++)nptSource[0xf042+n]=ram[(palette+n)^1];
 for(unsigned n=0;n<128;n++)nptSource[0x8402+n]=ram[(primary+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 nptSource[0x83c3]=saved[0x8035];
 for(unsigned n=0;n<3072;n++){
  unsigned group=n/16,tag=(uint16_t)word(ram,openingOwner+group*2);
  if(tag>0xfff0)return 2;
  unsigned offset=(ram[(openingOwner+384+group/2)^1]>>((group&1)?4:0))&15;
  openingColors[n]=bfpWord(0xe0000+tag*2+(n%16)*2)+(offset<<12);
 }
 /* Every descriptor dependency must reject before any controller advances. */
 const unsigned offsets[]={32,59,52,56,58,60,2};
 for(unsigned n=0;n<sizeof offsets/sizeof offsets[0];n++){
  unsigned at=palette+offsets[n];uint8_t old=ram[at^1];
  ram[at^1]=expected[at^1]=255;
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourcePanPalettesProbe",0,NULL)!=0||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  unsigned rejectArgs[]={0x20,0};
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,rejectArgs)!=1||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  ram[at^1]=expected[at^1]=old;
 }
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 WillowPaletteAnimation g={0};g.byte=spawnByte;g.word=bfpWord;g.read=openingRead;g.write=openingWrite;g.copy=openingCopy;
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),args[]={0x20,0};
 WillowNativeTaskRuntime runtime;for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
 for(unsigned tick=0;tick<29;tick++){
  uint16_t d1=bfpWord(0x8526+nptRead(0x8008)),d2=nptRead(0x24);
  nptWrite(0x800a,nptRead(0x800a)-1);
  if(!willowArcadeOpeningPalettesWithLive(&bus,openingResolve,&g,&d1,&d2))return 1;
  nptWrite(0x204,0xff);nptWrite(0x206,0x20);nptWrite(0x20,0x0101);
  nptWrite(0x22,d1);nptWrite(0x24,d2);nptPut(0x8296,nptByte(0x8296)|12);
  objectTaskScatter(expected,&runtime,control,task,globals);
  willowMotionPutWord(saved+0x8176,d1);willowMotionPutWord(saved+0x8178,d2);
  for(unsigned n=0;n<1024;n++)expected[(palette+n)^1]=nptSource[0xf042+n];
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
   fprintf(stderr,"Pan palette %u tick %u mismatch\n",second,tick);return 1;
  }
  if(!tick){
   const unsigned resumed[]={48,57,61};
   for(unsigned n=0;n<3;n++){
    unsigned at=palette+4*64+resumed[n];uint8_t old=ram[at^1];
    ram[at^1]=expected[at^1]=255;
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourcePanPalettesProbe",0,NULL)!=0||
       memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
    ram[at^1]=expected[at^1]=old;
   }
  }
  for(unsigned n=0;n<3072;n++){
   unsigned group=n/16,tag=(uint16_t)word(ram,openingOwner+group*2);
   unsigned offset=(ram[(openingOwner+384+group/2)^1]>>((group&1)?4:0))&15;
   if((uint16_t)(bfpWord(0xe0000+tag*2+(n%16)*2)+(offset<<12))!=openingColors[n])return 1;
  }
  for(unsigned n=0;n<2;n++)if((uint16_t)word(ram,slots+n*34)!=65535)return 1;
  for(unsigned n=0;n<512;n+=32)if(nptByte(n)==1){nptPut(n+1,nptByte(n+1)-1);if(!nptByte(n+1))nptPut(n,4);}
  objectTaskScatter(expected,&runtime,control,task,globals);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 }
 unsigned released=0;
 for(unsigned n=0;n<16;n++){
  uint8_t*r=nptSource+0xf042+n*64;
  if(r[59]==3){if(r[0]||willowMotionLong(r+2))return 1;released++;}
 }
 if(released!=(second?2:1)||nptRead(0x800a)!=1)return 1;
 nptWrite(0x800a,0);nptWrite(0x8008,nptRead(0x8008)+2);nptPut(0xe58,1);
 willowArcadeTaskStart(&bus,0xc0,0x78f0,second?0x73:0x71,second?0x260d:0x260b,0);
 nptWrite(0x20,0x0101);nptWrite(0x22,second?0x73:0x71);nptWrite(0x24,second?0x260d:0x260b);
 objectTaskScatter(expected,&runtime,control,task,globals);
 willowMotionPutWord(saved+0x8176,nptRead(0x22));willowMotionPutWord(saved+0x8178,nptRead(0x24));
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 printf("Resident pan palette %u: 29 countdown/BC4 ticks and terminal message start, %u C92 copies, 89088 exact palette words, %u terminal controllers released; full RAM/SRAM match and player slots free\n",second,openingCopies,released);
 return 0;
}
static int nativePanHoldCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),unsigned second,unsigned busy)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],saved[65536];
 if(!sram)return 2;
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals");
 const unsigned src[]={0,0x200,0xa10},dst[]={task,control,control+16},sizes[]={512,16,32};
 for(unsigned r=0;r<3;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[src[r]+n]=ram[(dst[r]+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 if(nptRead(0x8008)!=(second?22:8)||!!nptByte(0xe58)!=busy)return 2;
 WillowNativeTaskRuntime runtime;
 for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);unsigned args[]={0x20,0};
 if(nptByte(0x20)==1){
  willowArcadeTaskWake(&bus);objectTaskScatter(expected,&runtime,control,task,globals);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 }
 for(unsigned tick=0;tick<(busy?12:51);tick++){
  unsigned state=nptRead(0x8008),d1=bfpWord(0x8526+state),d2=nptRead(0x24);
  if((state==10||state==24)&&nptRead(0x800a)==1){d1=3;d2=16;}
  if(willowArcadeAttractPanWait(&bus,state,bfpWord)!=WILLOW_ATTRACT_RETURN)return 1;
  nptWrite(0x204,0xff);nptWrite(0x206,0x20);nptWrite(0x20,0x0101);
  nptWrite(0x22,d1);nptWrite(0x24,d2);nptPut(0x8296,nptByte(0x8296)|12);
  objectTaskScatter(expected,&runtime,control,task,globals);
  willowMotionPutWord(saved+0x8176,d1);willowMotionPutWord(saved+0x8178,d2);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
   fprintf(stderr,"Pan hold %u busy %u tick %u mismatch\n",second,busy,tick);return 1;
  }
  willowArcadeTaskWake(&bus);objectTaskScatter(expected,&runtime,control,task,globals);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 }
 if(!busy){
  if(nptRead(0x8008)!=(second?26:12)||nptRead(0x800a)||nptRead(0x82c0))return 1;
  sceneCommitPut(ram,globals+20,28);sceneCommitPut(expected,globals+20,28);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  sceneCommitPut(ram,globals+20,second?26:12);
 }
 printf("Resident pan hold %u: %s, full task/global/RAM/SRAM and live D1/D2 match\n",second,busy?"12 busy dispatch/wake pairs":"completion transition, 50 hold ticks and original fade allocation");
 return 0;
}
static int nativeOpeningPanCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],saved[65536];
 if(!sram)return 2;
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals");
 unsigned primary=symbol(symbols,"debugScroll2Record"),secondary=symbol(symbols,"debugScroll3Record");
 unsigned map=symbol(symbols,"debugScroll2Map"),nearAt=longword(ram,map)&65535,farAt=longword(ram,symbol(symbols,"debugScroll3Cells"))&65535;
 unsigned flags=symbol(symbols,"sourceSceneClearedPlanes"),prepared=symbol(symbols,"sourceScenePreparedPlanes"),args[]={0x20,0};
 setVideo(nativeRngVideo);
 for(unsigned pass=0;pass<2;pass++){
  unsigned second=pass;
  if(word(ram,globals+16)!=2||word(ram,globals+20)!=(second?14:0))return 2;
  memcpy(saved,sram,sizeof saved);memcpy(expected,ram,sizeof expected);
  for(unsigned bad=0;bad<6;bad++){
   unsigned at=bad==0?primary+33:bad==1?secondary+33:bad==2?flags:bad==3?primary+66:bad==4?secondary+66:map+8;
   uint8_t old=ram[at^1];unsigned oldWord=(uint16_t)word(ram,at);
   if(bad==5){sceneCommitPut(ram,at,0);sceneCommitPut(expected,at,0);}
   else ram[at^1]=expected[at^1]=bad==2?old|4:bad<2?1:old^255;
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
      memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
   if(bad==5){sceneCommitPut(ram,at,oldWord);sceneCommitPut(expected,at,oldWord);}
   else ram[at^1]=expected[at^1]=old;
  }
  memset(nptSource,0,sizeof nptSource);
  const unsigned src[]={0,0x200,0xa10,0x8402,0x8482},dst[]={task,control,control+16,primary,secondary},sizes[]={512,16,32,128,128};
  for(unsigned r=0;r<5;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[src[r]+n]=ram[(dst[r]+n)^1];
  for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
   const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
   for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
  }
  uint16_t cells[WILLOW_SCROLL3_MAP_WORDS];for(unsigned n=0;n<WILLOW_SCROLL3_MAP_WORDS;n++)cells[n]=word(ram,farAt+n*2);
  for(unsigned n=0;n<4096;n++){
   unsigned nearId=(uint16_t)word(ram,nearAt+n*2),farId=willowScroll3MapId(cells,n);
   if(nearId>=WILLOW_SCROLL2_PAIR_COUNT||farId>=WILLOW_SCROLL3_PAIR_COUNT)return 2;
   willowMotionPutLong(spawnMap+n*4,nativeScroll2Pairs[nearId]);willowMotionPutLong(panFar+n*4,nativeScroll3Pairs[farId]);
  }
  const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};spawnWrites=panFarWrites=0;
  if(!willowArcadeAttractPanInitVariant(&bus,nptSource+0x8402,nptSource+0x8482,spawnByte,bfpWord,spawnWrite,panFarWrite,second)||spawnWrites!=1024||panFarWrites!=256)return 1;
  nptWrite(0x204,0xff);nptWrite(0x206,0x20);nptWrite(0x20,0x0101);nptWrite(0x22,3);nptWrite(0x24,16);nptPut(0x8296,nptByte(0x8296)|12);
  for(unsigned r=0;r<5;r++)for(unsigned n=0;n<sizes[r];n++)expected[(dst[r]+n)^1]=nptSource[src[r]+n];
  for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
   const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
   for(unsigned n=0;n<g->count;n++)expected[(globals+g->offset+n)^1]=nptSource[g->source+n];
  }
  for(unsigned n=0;n<4096;n++)for(unsigned region=0;region<2;region++){
   uint32_t pair=willowMotionLong((region?panFar:spawnMap)+n*4);
   unsigned lo=0,hi=region?WILLOW_SCROLL3_PAIR_COUNT:WILLOW_SCROLL2_PAIR_COUNT;const uint32_t*dict=region?nativeScroll3Pairs:nativeScroll2Pairs;
   while(lo<hi){unsigned mid=(lo+hi)/2;if(dict[mid]<pair)lo=mid+1;else hi=mid;}
   if(lo>=(region?WILLOW_SCROLL3_PAIR_COUNT:WILLOW_SCROLL2_PAIR_COUNT)||dict[lo]!=pair)return 1;
   if(region){if(!willowScroll3MapSet(cells,n,lo))return 1;}else sceneCommitPut(expected,nearAt+n*2,lo);
  }
  for(unsigned n=0;n<WILLOW_SCROLL3_MAP_WORDS;n++)sceneCommitPut(expected,farAt+n*2,cells[n]);
  expected[flags^1]&=~3;expected[prepared^1]&=~3;
  uint8_t wantedSRAM[65536];memcpy(wantedSRAM,saved,sizeof saved);willowMotionPutWord(wantedSRAM+0x8176,3);willowMotionPutWord(wantedSRAM+0x8178,16);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
     memcmp(expected,ram,sizeof expected)||memcmp(wantedSRAM,sram,sizeof saved)){
   fprintf(stderr,"Resident pan %u mismatch\n",second);
   for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Pan RAM %x %x/%x\n",n^1,ram[n],expected[n]);break;}return 1;
  }
  for(unsigned n=0;n<512;n+=32)if(expected[(task+n)^1]==1){
   unsigned at=(task+n+1)^1;expected[at]--;if(!expected[at])expected[(task+n)^1]=4;
  }
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(wantedSRAM,sram,sizeof saved))return 1;
  if(nativeOpeningPanMotionCheck(core,ram,sram,symbols,setReg,getReg,runFrame,second)||
     nativePanPaletteCheck(core,ram,symbols,runFrame,getReg,second))return 1;
  if(nativePanHoldCheck(core,ram,symbols,runFrame,getReg,second,1)||
     nativeOpeningMessageCheck(core,ram,symbols,runFrame,getReg,0xc0))return 1;
  setVideo(nativeRngVideo);
  if(nativePanHoldCheck(core,ram,symbols,runFrame,getReg,second,0)||
     nativeOpeningFadesCheck(core,ram,symbols,runFrame,getReg)||
     nativeOpeningClearCheck(core,ram,symbols,runFrame,getReg))return 1;
  setVideo(nativeRngVideo);
 }
 setVideo(video);puts("Resident consecutive phase-two pans: first clear reaches second setup without state injection; both variants match 1280 source map writes each, full controllers/maps, message/fade allocation, task sleep and live D1/D2; six invalid capabilities per variant reject atomically");return 0;
}
