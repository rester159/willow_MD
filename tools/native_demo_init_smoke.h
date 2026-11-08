static int nativeDemoStartCheck(void*,uint8_t*,const char*,void(*)(void),unsigned(*)(int));
#include "../src/willow_native_attract_demo_init.h"
static void demoInitColor(uint32_t at,uint16_t value)
{willowMotionPutWord(nptSource+0x2000+at-0x91c000,value);}
static void demoInitModelRun(uint16_t*d1,uint16_t*d2)
{
 const WillowVisibilityBus b={nptRead,nptByte,nptWrite,nptPut};
 if(!willowNativeAttractDemoInitBoundary(&b,bfpWord,demoInitColor,0,0,0,d1,d2))abort();
}
/* Isolated initializer and full retained-memory audit. Scheduler admission
 * and its live-register handoff are separate checks. */
static int nativeDemoInitCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM);if(!sram)return 2;
 uint8_t saved[65536],savedSRAM[65536],colors[6144],expectedRAM[65536],expectedSRAM[65536];
 memcpy(saved,ram,sizeof saved);memcpy(savedSRAM,sram,sizeof savedSRAM);
 unsigned globals=symbol(symbols,"sourceBootGlobals"),control=symbol(symbols,"sourceTaskState");
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 for(unsigned sample=0;sample<8;sample++){
  unsigned variant=sample&1,dispatch=sample>=4;
  WillowNativeTaskRuntime runtime;
  memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);
  memset(nptSource,0,sizeof nptSource);
  for(unsigned at=0;at<65536;at++){
   uint8_t*p=demoStateByte(ram,sram,globals,at);if(p)nptSource[at]=*p;
  }
  for(unsigned n=0;n<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];n++){
   const WillowNativeBootGlobalRange*r=willowNativeBootGlobalRanges+n;
   for(unsigned k=0;k<r->count;k++)nptSource[r->source+k]=ram[(globals+r->offset+k)^1];
  }
  unsigned taskBase=symbol(symbols,"taskRecords");
  for(unsigned k=0;k<512;k++)nptSource[k]=ram[(taskBase+k)^1];
  for(unsigned k=0;k<16;k++)nptSource[0x200+k]=ram[(control+k)^1];
  for(unsigned k=0;k<32;k++)nptSource[0xa10+k]=ram[(control+16+k)^1];
  if(!panPaletteSnapshot(ram,symbols,nptSource+0x2000))return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
  const unsigned setup[][2]={{0x8004,6},{0x8008,0},{0xe5a,variant},{0x828a,0},{0x828c,0},{0x20c,0}};
  for(unsigned n=0;n<6;n++){
   unsigned args[]={setup[n][0],setup[n][1]};
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutWord",2,args)==~0u)return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
   nptWrite(setup[n][0],setup[n][1]);
  }
  if(sample%4>=2){
   const unsigned freeSetup[][2]={{0x208,0xffff},{0x20a,0xa10},{0xa10,0xffff},{0xa12,0x100},{0x20c,1}};
   for(unsigned n=0;n<5;n++){
    unsigned args[]={freeSetup[n][0],freeSetup[n][1]};
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutWord",2,args)==~0u)return 1;
    nptWrite(freeSetup[n][0],freeSetup[n][1]);
   }
  }
  unsigned args[]={0x828f,0};
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutByte",2,args)==~0u)return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
  nptPut(0x828f,0);
  if(dispatch){
   const unsigned setup[][2]={{0x8000,4},{0x20,0x0400}};
   for(unsigned n=0;n<2;n++){
    unsigned args[]={setup[n][0],setup[n][1]};
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutWord",2,args)==~0u)return 1;
    nptWrite(setup[n][0],setup[n][1]);
   }
   const unsigned bytes[]={0x82a4,0x82a2,0x8058,0x8068};
   for(unsigned n=0;n<4;n++){
    unsigned args[]={bytes[n],0};
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutByte",2,args)==~0u)return 1;
    nptPut(bytes[n],0);
   }
   sceneCommitPut(ram,control+54,0x810e);
   for(unsigned n=0;n<16;n++)runtime.resume[n]=(uint16_t)word(ram,control+52+n*2);
  }
  memcpy(expectedRAM,ram,sizeof expectedRAM);memcpy(expectedSRAM,sram,sizeof expectedSRAM);
  if(sample==3||sample==7){
   const unsigned badWords[][2]={{0x8004,5},{0x8008,4},{0xe5a,2},{0x828c,2},
    {0x20c,9},{0x208,0},{0x20a,0xa11},{0xa10,0},{0xa12,0xe0},{0x828a,0xffff}};
   uint8_t rejectedRAM[65536],rejectedSRAM[65536];
   for(unsigned bad=0;bad<12;bad++){
    memcpy(ram,expectedRAM,sizeof expectedRAM);memcpy(sram,expectedSRAM,sizeof expectedSRAM);
    if(bad<10){
     unsigned args[]={badWords[bad][0],badWords[bad][1]};
     if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutWord",2,args)==~0u)return 1;
    }else if(bad==10)ram[(symbol(symbols,"debugActorPools")+8)^1]=0;
    else{
     unsigned args[]={0x828f,1};
     if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutByte",2,args)==~0u)return 1;
     args[0]=0x8067;args[1]=192;
     if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutByte",2,args)==~0u)return 1;
    }
    memcpy(rejectedRAM,ram,sizeof rejectedRAM);memcpy(rejectedSRAM,sram,sizeof rejectedSRAM);
    unsigned rejectedArgs[]={0x20,0};
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,dispatch?"sourceSceneBootTaskDispatch":"sourceDemoInitProbe",dispatch?2:0,rejectedArgs)!=(dispatch?1:0)||
       memcmp(ram,rejectedRAM,0xf600)||memcmp(sram,rejectedSRAM,sizeof rejectedSRAM)){
     fprintf(stderr,"Demo invalid state mutated memory or was admitted: %u\n",bad);return 1;
    }
   }
   memcpy(ram,expectedRAM,sizeof expectedRAM);memcpy(sram,expectedSRAM,sizeof expectedSRAM);
  }
  if(dispatch){
   WillowNativeTaskRun run=willowNativeTaskRunOneWithDemo(&runtime,&bus,0x20,0,bfpWord,0,demoInitColor,0,0,0,0,0,0,0,0,demoInitModelRun);
   if(run.unsupported||!run.dispatched)return 1;
   for(unsigned n=0;n<16;n++)sceneCommitPut(expectedRAM,control+52+n*2,runtime.resume[n]);
  }else if(!willowArcadeAttractDemoInit(&bus,bfpWord,demoInitColor))return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
  unsigned overflow=(unsigned short)word(ram,symbol(symbols,"debugIntroOverflow"));
  setVideo(nativeRngVideo);
  unsigned dispatchArgs[]={0x20,0};
  if(nativeSATCallBounded(64,NULL,ram,symbols,setReg,getReg,runFrame,dispatch?"sourceSceneBootTaskDispatch":"sourceDemoInitProbe",dispatch?2:0,dispatchArgs)!=(dispatch?0x424f:1)||
     (unsigned short)word(ram,symbol(symbols,"debugIntroOverflow"))!=overflow)return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
  for(unsigned at=0;at<65536;at++){
   uint8_t*p=demoStateByte(ram,sram,globals,at);
   uint8_t*q=demoStateByte(expectedRAM,expectedSRAM,globals,at);if(q)*q=nptSource[at];
   if(p&&*p!=nptSource[at]){fprintf(stderr,"Demo init state %04x got %02x want %02x\n",at,*p,nptSource[at]);return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;}
  }
  for(unsigned n=0;n<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];n++){
   const WillowNativeBootGlobalRange*r=willowNativeBootGlobalRanges+n;
   for(unsigned k=0;k<r->count;k++)expectedRAM[(globals+r->offset+k)^1]=nptSource[r->source+k];
   for(unsigned k=0;k<r->count;k++)if(ram[(globals+r->offset+k)^1]!=nptSource[r->source+k])return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
  }
  const char*names[]={"debugScroll2Record","debugScroll3Record","debugStageObjects","debugPlayerRecord","debugPaletteControllers"};
  const unsigned bases[]={0x8402,0x8482,0x8502,0x9846,0xf042},sizes[]={128,128,3072,128,1024};
  for(unsigned n=0;n<5;n++)for(unsigned k=0,at=symbol(symbols,names[n]);k<sizes[n];k++)
   if(ram[(at+k)^1]!=nptSource[bases[n]+k])return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
  for(unsigned n=0;n<5;n++)for(unsigned k=0,at=symbol(symbols,names[n]);k<sizes[n];k++)expectedRAM[(at+k)^1]=nptSource[bases[n]+k];
  const unsigned order[]={4,0,1,5,6,2,7,3};
  for(unsigned i=0;i<9;i++){
   const WillowActorPoolSpec*spec=willowActorPoolSpecification((i+1)*2);
   unsigned pool=i==8?symbol(symbols,"tileIndex")+4648:symbol(symbols,"debugActorPools")+order[i]*12;
   unsigned records=longword(expectedRAM,pool),indices=longword(expectedRAM,pool+4)&65535;
   for(unsigned k=0;k<spec->capacity*spec->stride;k++)*nativeTargetByte(expectedRAM,expectedSRAM,records+k)=nptSource[spec->sourceBase+k];
   for(unsigned k=0;k<spec->capacity;k++)expectedRAM[(indices+k)^1]=spec->capacity-1-k;
   expectedRAM[(pool+10)^1]=0;expectedRAM[(pool+11)^1]=spec->capacity;
   for(unsigned k=0;k<spec->capacity*spec->stride;k++)if(*nativeTargetByte(ram,sram,records+k)!=nptSource[spec->sourceBase+k])return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
   for(unsigned k=0;k<spec->capacity;k++)if(ram[(indices+k)^1]!=spec->capacity-1-k)return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
   if(ram[(pool+10)^1]||ram[(pool+11)^1]!=spec->capacity)return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
  }
  for(unsigned k=0;k<512;k++)if(ram[(taskBase+k)^1]!=nptSource[k])return 1;
  for(unsigned k=0;k<16;k++)if(ram[(control+k)^1]!=nptSource[0x200+k])return 1;
  for(unsigned k=0;k<512;k++)expectedRAM[(taskBase+k)^1]=nptSource[k];
  for(unsigned k=0;k<16;k++)expectedRAM[(control+k)^1]=nptSource[0x200+k];
  /* Independent storage projection of pool-reset auxiliary fields. */
  const unsigned aux[][2]={{0x83cb,1},{0x97ff,2},{0x9802,2},{0x9805,2},
   {0x9808,2},{0x980b,2},{0x9811,2},{0x9814,2},{0x9818,4},{0x981e,4},
   {0x9824,4},{0x982a,4},{0x9830,4},{0x983c,4},{0x9842,4},{0xf488,4},
   {0xf48d,2},{0xf898,3},{0xf99d,1}};
  unsigned offset=0;
  for(unsigned n=0;n<19;n++)for(unsigned k=0;k<aux[n][1];k++)expectedSRAM[0x8000+offset++]=nptSource[aux[n][0]+k];
  if(offset!=53)return 1;
  unsigned visibility=longword(expectedRAM,symbol(symbols,"debugClass12Visibility"))&65535;
  sceneCommitPut(expectedRAM,visibility+360,nptRead(0x9836));
  sceneCommitPut(expectedRAM,visibility+362,nptRead(0x9838));
  sceneCommitPut(expectedRAM,visibility+364,nptRead(0xf900));
  expectedRAM[(visibility+366)^1]=nptByte(0x980e);
  expectedRAM[(visibility+367)^1]=nptByte(0x980f);
  expectedRAM[(visibility+368)^1]=nptByte(0xf902);
  /* Raw ROM replacement also resets brightness and releases overwritten
   * mutable owners. Preserve every unselected group/slot byte. */
  unsigned owner=symbol(symbols,"debugPaletteOwner"),slots=symbol(symbols,"debugPaletteSlots");
  const unsigned sources[]={0xe5080,0xe5480+variant*512,0xe7d20+variant*1024,0xe6ea0,
   ((unsigned)bfpWord(0xa12+variant*4)<<16)|bfpWord(0xa14+variant*4)};
  unsigned selector=bfpWord(0xa2e);selector=variant?selector&255:selector>>8;
  const unsigned starts[]={0,16,64,32,96+(selector?16:0)},counts[]={16,16,32,32,16};
  for(unsigned block=0;block<5;block++){
   for(unsigned n=0;n<counts[block];n++){
    unsigned group=starts[block]+n;
    sceneCommitPut(expectedRAM,owner+group*2,(sources[block]+n*32-0xe0000)/2);
    expectedRAM[(owner+384+group/2)^1]&=(group&1)?15:240;
   }
   for(unsigned n=0;n<2;n++){
    unsigned group=(uint16_t)word(expectedRAM,slots+n*34);
    if(group>=starts[block]&&group<starts[block]+counts[block])sceneCommitPut(expectedRAM,slots+n*34,0xffff);
   }
  }
  for(unsigned at=0;at<0xf600;at++)if(ram[at]!=expectedRAM[at]){fprintf(stderr,"Demo full RAM mismatch %04x got %02x want %02x\n",at^1,ram[at],expectedRAM[at]);return 1;}
  for(unsigned at=0;at<65536;at++)if(sram[at]!=expectedSRAM[at]){fprintf(stderr,"Demo full SRAM mismatch %04x got %02x want %02x\n",at,sram[at],expectedSRAM[at]);return 1;}
  if(word(ram,control+12)||!panPaletteSnapshot(ram,symbols,colors)||memcmp(colors,nptSource+0x2000,sizeof colors))return fprintf(stderr,"Compiled demo audit failed at line %d variant %u\n",__LINE__,variant),1;
  if(getenv("WILLOW_AUDIT_DEMO_START")&&(nativeDemoStartCheck(core,ram,symbols,runFrame,getReg)||nativeDemoStartCheck(core,ram,symbols,runFrame,getReg)))return 1;
  if(dispatch){
   sceneCommitPut(ram,globals+20,4);sceneCommitPut(expectedRAM,globals+20,4);
   ram[(taskBase+0x20)^1]=expectedRAM[(taskBase+0x20)^1]=4;
   unsigned args[]={0x20,0};
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
      memcmp(ram,expectedRAM,0xf600)||memcmp(sram,expectedSRAM,sizeof expectedSRAM))return 1;
  }
 }
 memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);setVideo(video);
 puts("Compiled demo initializer: isolated and scheduler entries for both replay selectors match source model for retained player/global state, fixed records, nine indexed pools and all 3072 CPS words; available/exhausted task allocation; full unrelated RAM/SRAM preserved; twelve invalid states reject atomically through both entries; active demo state four remains unsupported; original state restored");
 return 0;
}
