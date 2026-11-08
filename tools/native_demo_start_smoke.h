#include "../src/willow_native_attract_demo_start.h"
static uint8_t*demoStartResolve(uint16_t at){return nptSource+at;}
static uint16_t demoStartActorWord(unsigned long at){return bfpWord(at);}
static void demoStartModelRun(uint16_t*d1,uint16_t*d2)
{
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 if(!willowNativeAttractDemoStartBoundary(&bus,demoStartResolve,demoStartActorWord,bfpWord,bfpVideoWord,d1,d2))abort();
}
/* Called after a fully checked initializer, then restores both native and
 * scalar state. Expected storage is projected independently from source outputs. */
static int nativeDemoStartCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM);if(!sram)return 2;
 uint8_t saved[65536],savedSRAM[65536],source[65536],expected[65536],text[65536],colors[6144],afterColors[6144];
 memcpy(saved,ram,sizeof saved);memcpy(savedSRAM,sram,sizeof savedSRAM);memcpy(source,nptSource,sizeof source);
 memcpy(expected,ram,sizeof expected);memcpy(text,sram,sizeof text);
 unsigned globals=symbol(symbols,"sourceBootGlobals"),control=symbol(symbols,"sourceTaskState");
 const unsigned addresses[]={0x801a,0x801c,0x801e,0x8020},offsets[]={48,50,84,86};
 for(unsigned n=0;n<4;n++)nptWrite(addresses[n],(uint16_t)word(ram,control+offsets[n]));
 const char*ringNames[]={"taskCommands","soundCommands"};
 for(unsigned r=0;r<2;r++)for(unsigned n=0,at=symbol(symbols,ringNames[r]);n<(r?128:256);n++)nptSource[(r?0x8172:0x8072)+n]=ram[(at+n)^1];
 /* Capture the retained score used by HUD setup. */
 unsigned args[]={0xf03e};
 unsigned value=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueueWord",1,args);if(value==~0u)return 1;nptWrite(0xf03e,value);
 args[0]=0xf040;value=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueueWord",1,args);if(value==~0u)return 1;nptWrite(0xf040,value);
 static unsigned nextFixture;
 unsigned dispatch=nextFixture&1,fixture=nextFixture++/2;
 WillowNativeTaskRuntime runtime;
 /* Exercise optional statistics and available/exhausted fade allocation
  * independently of the initializer's preceding allocation. */
 unsigned optionArgs[]={0x8062,(fixture&2)?0x20:0};
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutByte",2,optionArgs)==~0u)return 1;
 nptPut(optionArgs[0],optionArgs[1]);
 const unsigned setup[][2]={{0xe68,fixture*127},{0xe6a,fixture*61},
  {0x208,0xffff},{0x20a,0xa10},{0xa10,0xffff},{0xa12,0x140},{0x20c,(fixture&4)?1:0}};
 for(unsigned n=0;n<7;n++){
  unsigned args[]={setup[n][0],setup[n][1]};
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutWord",2,args)==~0u)return 1;
  nptWrite(setup[n][0],setup[n][1]);
 }
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
 memcpy(expected,ram,sizeof expected);memcpy(text,sram,sizeof text);
 if(fixture==7){
  const unsigned badWords[][2]={{0x8004,5},{0x8008,4},{0x8344,2},
   {0x801a,256},{0x801a,1},{0x801e,128},{0x801e,1},
   {0x20c,9},{0x208,0},{0x20a,0xa0c},{0x20a,0xa11},
   {0xa12,0xe0},{0xa12,0x200},{0xa12,0x101}};
  uint8_t rejectedRAM[65536],rejectedSRAM[65536];
  for(unsigned bad=0;bad<22;bad++){
   memcpy(ram,expected,sizeof expected);memcpy(sram,text,sizeof text);
   if(bad<14){
    unsigned args[]={badWords[bad][0],badWords[bad][1]};
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutWord",2,args)==~0u)return 1;
   }else if(bad<17){
    const unsigned badBytes[][2]={{0x8394,10},{0x8366,7},{0x8367,5}};
    unsigned args[]={badBytes[bad-14][0],badBytes[bad-14][1]};
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutByte",2,args)==~0u)return 1;
   }else{
    unsigned pool=symbol(symbols,"debugActorPools"),indices=longword(ram,pool+4)&65535;
    unsigned capacity=ram[(pool+8)^1];
    if(bad==17)ram[(pool+8)^1]=0;
    if(bad==18)ram[(pool+10)^1]=capacity+1;
    if(bad==19)ram[(pool+11)^1]=capacity+1;
    if(bad==20)ram[indices^1]=capacity;
    if(bad==21)ram[(indices+1)^1]=ram[indices^1];
   }
   memcpy(rejectedRAM,ram,sizeof rejectedRAM);memcpy(rejectedSRAM,sram,sizeof rejectedSRAM);
   unsigned callArgs[]={0x20,0};
   if(nativeSATCallBounded(64,NULL,ram,symbols,setReg,getReg,runFrame,dispatch?"sourceSceneBootTaskDispatch":"sourceDemoStartProbe",dispatch?2:0,callArgs)!=(dispatch?1:0)||
      memcmp(ram,rejectedRAM,0xf600)||memcmp(sram,rejectedSRAM,sizeof rejectedSRAM)){
    fprintf(stderr,"Demo start invalid context admitted or mutated memory: %u\n",bad);return 1;
   }
  }
  memcpy(ram,expected,sizeof expected);memcpy(sram,text,sizeof text);
  puts("Compiled demo start: 22 invalid contexts reject without retained-memory mutation");
 }
 if(!panPaletteSnapshot(ram,symbols,colors))return 1;
 uint8_t*oldExpectedRAM=bfpExpectedRam,*oldExpectedSRAM=bfpExpectedSram;
 unsigned oldHud=bfpHud,oldCleared=bfpCleared,oldTextWrites=bfpTextWrites;
 bfpExpectedRam=expected;bfpExpectedSram=text;bfpHud=symbol(symbols,"debugHudSurface");bfpCleared=symbol(symbols,"sourceSceneClearedPlanes");
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 if(dispatch){
  WillowNativeTaskRun run=willowNativeTaskRunOneWithDemoStart(&runtime,&bus,0x20,0,bfpWord,0,demoInitColor,0,0,0,0,0,0,0,0,0,demoStartModelRun);
  if(run.unsupported||!run.dispatched)return 1;
  for(unsigned n=0;n<16;n++)sceneCommitPut(expected,control+52+n*2,runtime.resume[n]);
 }else if(!willowArcadeAttractDemoStart(&bus,demoStartResolve,demoStartActorWord,bfpWord,bfpVideoWord))return 1;
 unsigned overflow=(uint16_t)word(ram,symbol(symbols,"debugIntroOverflow"));
 setVideo(nativeRngVideo);
 unsigned elapsed=0;
 unsigned callArgs[]={0x20,0};
 value=nativeSATCallBounded(128,&elapsed,ram,symbols,setReg,getReg,runFrame,dispatch?"sourceSceneBootTaskDispatch":"sourceDemoStartProbe",dispatch?2:0,callArgs);
 if(value!=(dispatch?0x424f:1)||(uint16_t)word(ram,symbol(symbols,"debugIntroOverflow"))!=overflow){fprintf(stderr,"Compiled demo start result %x overflow %u/%u\n",value,(uint16_t)word(ram,symbol(symbols,"debugIntroOverflow")),overflow);return 1;}
 for(unsigned at=0;at<65536;at++){
  uint8_t*q=demoStateByte(expected,text,globals,at);if(q)*q=nptSource[at];
  uint8_t*p=demoStateByte(ram,sram,globals,at);
  if(p&&*p!=nptSource[at]){fprintf(stderr,"Demo start state mismatch %04x\n",at);return 1;}
 }
 for(unsigned n=0;n<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];n++){
  const WillowNativeBootGlobalRange*r=willowNativeBootGlobalRanges+n;
  for(unsigned k=0;k<r->count;k++)expected[(globals+r->offset+k)^1]=nptSource[r->source+k];
  for(unsigned k=0;k<r->count;k++)if(ram[(globals+r->offset+k)^1]!=nptSource[r->source+k]){fprintf(stderr,"Demo start global %04x\n",r->source+k);return 1;}
 }
 const char*names[]={"debugScroll2Record","debugScroll3Record","debugStageObjects","debugPlayerRecord","debugPaletteControllers","taskRecords","taskCommands","soundCommands"};
 const unsigned bases[]={0x8402,0x8482,0x8502,0x9846,0xf042,0,0x8072,0x8172},sizes[]={128,128,3072,128,1024,512,256,128};
 for(unsigned n=0;n<8;n++)for(unsigned k=0,at=symbol(symbols,names[n]);k<sizes[n];k++)
  if(ram[(at+k)^1]!=nptSource[bases[n]+k]){fprintf(stderr,"Demo start record %s +%u got %x want %x\n",names[n],k,ram[(at+k)^1],nptSource[bases[n]+k]);return 1;}
 for(unsigned n=0;n<8;n++)for(unsigned k=0,at=symbol(symbols,names[n]);k<sizes[n];k++)expected[(at+k)^1]=nptSource[bases[n]+k];
 const unsigned order[]={4,0,1,5,6,2,7,3};
 const unsigned countAt[]={0x97fe,0x9801,0x9804,0x9807,0x980a,0x980d,0x9810,0x9813,0xf48c};
 const unsigned cursorAt[]={0x9816,0x981c,0x9822,0x9828,0x982e,0x9834,0x983a,0x9840,0xf486};
 const unsigned end[]={0x912a,0x918e,0x91ee,0x9216,0x923e,0x92b6,0x932e,0x9356,0xf466};
 for(unsigned i=0;i<9;i++){
  const WillowActorPoolSpec*spec=willowActorPoolSpecification((i+1)*2);
  unsigned pool=i==8?symbol(symbols,"tileIndex")+4648:symbol(symbols,"debugActorPools")+order[i]*12;
  unsigned records=longword(saved,pool);
  for(unsigned k=0;k<spec->capacity*spec->stride;k++)if(*nativeTargetByte(ram,sram,records+k)!=nptSource[spec->sourceBase+k]){fprintf(stderr,"Demo start pool %u +%u\n",i,k);return 1;}
  for(unsigned k=0;k<spec->capacity*spec->stride;k++)*nativeTargetByte(expected,text,records+k)=nptSource[spec->sourceBase+k];
  expected[(pool+10)^1]=(nptRead(cursorAt[i])-(end[i]-spec->capacity*2))/2;
  expected[(pool+11)^1]=nptByte(countAt[i]);
  if(ram[(pool+10)^1]!=(nptRead(cursorAt[i])-(end[i]-spec->capacity*2))/2||ram[(pool+11)^1]!=nptByte(countAt[i]))return 1;
 }
 for(unsigned n=0;n<4;n++)if((uint16_t)word(ram,control+offsets[n])!=nptRead(addresses[n]))return 1;
 for(unsigned n=0;n<16;n++)if(ram[(control+n)^1]!=nptByte(0x200+n))return 1;
 for(unsigned n=0;n<4;n++)sceneCommitPut(expected,control+offsets[n],nptRead(addresses[n]));
 for(unsigned n=0;n<16;n++)expected[(control+n)^1]=nptByte(0x200+n);
 /* Pool auxiliary bytes have separate physical owners. */
 const unsigned aux[][2]={{0x83cb,1},{0x97ff,2},{0x9802,2},{0x9805,2},
  {0x9808,2},{0x980b,2},{0x9811,2},{0x9814,2},{0x9818,4},{0x981e,4},
  {0x9824,4},{0x982a,4},{0x9830,4},{0x983c,4},{0x9842,4},{0xf488,4},
  {0xf48d,2},{0xf898,3},{0xf99d,1}};
 unsigned auxOffset=0;
 for(unsigned n=0;n<19;n++)for(unsigned k=0;k<aux[n][1];k++)text[0x8000+auxOffset++]=nptSource[aux[n][0]+k];
 for(unsigned at=0;at<0xf600;at++)if(ram[at]!=expected[at]){fprintf(stderr,"Demo start unrelated RAM %04x got %02x want %02x\n",at^1,ram[at],expected[at]);return 1;}
 for(unsigned at=0;at<65536;at++)if(sram[at]!=text[at]){fprintf(stderr,"Demo start unrelated SRAM %04x got %02x want %02x\n",at,sram[at],text[at]);return 1;}
 if(memcmp(sram+0x4000,text+0x4000,16384))return fprintf(stderr,"Demo start Scroll1 mismatch\n"),1;
 for(unsigned n=0;n<1188;n++)if(ram[(bfpHud+n)^1]!=expected[(bfpHud+n)^1])return fprintf(stderr,"Demo start HUD projection mismatch\n"),1;
 if(!panPaletteSnapshot(ram,symbols,afterColors)||memcmp(colors,afterColors,sizeof colors))return 1;
 memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);memcpy(nptSource,source,sizeof source);setVideo(video);
 bfpExpectedRam=oldExpectedRAM;bfpExpectedSram=oldExpectedSRAM;bfpHud=oldHud;bfpCleared=oldCleared;bfpTextWrites=oldTextWrites;
 printf("Compiled demo start (%s): source state, actor/palette records, nine pools, task/sound queues, full Scroll1 and HUD match; unrelated RAM/SRAM and free indices preserved (statistics %u, free fade slot %u); %u diagnostic slices, not FPS\n",dispatch?"scheduler":"isolated",!!(fixture&2),!!(fixture&4),elapsed);
 return 0;
}
