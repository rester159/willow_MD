#include "../src/willow_arcade_attract_panel_wait.h"
#include "../src/willow_arcade_attract_panel.h"
#include "../src/willow_arcade_attract_text_sequence.h"
static int nativePhaseFourTextCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],saved[65536];if(!sram)return 2;
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals");
 if(word(ram,globals+16)!=4||word(ram,globals+20))return 2;
 unsigned calls=0,busyCalls=0,messages=0,args[]={0x20,0};setVideo(nativeRngVideo);
 while(word(ram,globals+20)!=6){
  if(calls>100)return 1;
  if(ram[(globals+4)^1]&&busyCalls==12){
   if(nativeOpeningMessageCheck(core,ram,symbols,runFrame,getReg,0xc0))return 1;
   setVideo(nativeRngVideo);busyCalls=0;messages++;
  }
  memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);memset(nptSource,0,sizeof nptSource);
  const unsigned from[]={0,0x200,0xa10},to[]={task,control,control+16},sizes[]={512,16,32};
  for(unsigned r=0;r<3;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[from[r]+n]=ram[(to[r]+n)^1];
  for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
   const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
   for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
  }
  WillowNativeTaskRuntime runtime;for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
  const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
  unsigned state=nptRead(0x8008),d1=bfpWord(0x888e + state),d2=nptRead(0x24);
  if(nptByte(0xe58))busyCalls++;
  if(!state){d1=0x3c;d2=0x160f;}
  else if(!nptByte(0xe58)&&nptRead(0x800a)==1){d1=state==2?0x3d:3;d2=state==2?0x1610:16;}
  if(willowArcadeAttractTextSequence(&bus,state,bfpWord)!=WILLOW_ATTRACT_RETURN)return 1;
  nptWrite(0x204,0xff);nptWrite(0x206,0x20);nptWrite(0x20,0x0101);
  nptWrite(0x22,d1);nptWrite(0x24,d2);nptPut(0x8296,nptByte(0x8296)|12);
  objectTaskScatter(expected,&runtime,control,task,globals);
  willowMotionPutWord(saved+0x8176,d1);willowMotionPutWord(saved+0x8178,d2);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
   fprintf(stderr,"Phase four text mismatch call %u state %u\n",calls,state);return 1;
  }
  willowArcadeTaskWake(&bus);objectTaskScatter(expected,&runtime,control,task,globals);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  calls++;
 }
 if(calls!=85||messages!=2)return 1;
 if(nativeOpeningFadesCheck(core,ram,symbols,runFrame,getReg)||nativeOpeningClearCheck(core,ram,symbols,runFrame,getReg))return 1;
 setVideo(video);puts("Resident phase-four text: 85 dispatch/wake pairs, both actual messages and thirty-tick holds, fade allocation/completion and clear to state eight verified; full RAM/SRAM and live words match, blockwise schedule only");return 0;
}

/* Continue from the actual phase-four text clear; use raw arcade map words. */
static int nativeIntroPanelCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],saved[65536];if(!sram)return 2;
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals"),hud=symbol(symbols,"debugHudSurface");
 if(word(ram,globals+16)!=4||word(ram,globals+20)!=8)return 2;
 memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);memset(nptSource,0,sizeof nptSource);
 const unsigned from[]={0,0x200,0xa10},to[]={task,control,control+16},sizes[]={512,16,32};
 for(unsigned r=0;r<3;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[from[r]+n]=ram[(to[r]+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 WillowNativeTaskRuntime runtime;for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 messageExpectedSRAM=saved;messageWrites=0;
 for(unsigned n=0;n<sizeof messageHud/2;n++)((uint16_t*)&messageHud)[n]=word(ram,hud+n*2);
 if(!willowArcadeAttractPanel(&bus,bfpWord,messageVideoWord)||messageWrites!=4176)return 1;
 nptWrite(0x204,0xff);nptWrite(0x206,0x20);nptWrite(0x20,0x0101);
 nptWrite(0x22,65535);nptWrite(0x24,19);nptPut(0x8296,nptByte(0x8296)|12);
 objectTaskScatter(expected,&runtime,control,task,globals);
 for(unsigned n=0;n<sizeof messageHud/2;n++)sceneCommitPut(expected,hud+n*2,((uint16_t*)&messageHud)[n]);
 expected[symbol(symbols,"sourceSceneClearedPlanes")^1]&=~8;
 willowMotionPutWord(saved+0x8176,65535);willowMotionPutWord(saved+0x8178,19);
 unsigned args[]={0x20,0},slices=0;setVideo(nativeRngVideo);
 if(nativeSATCallBounded(8,&slices,ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
  fputs("Resident panel setup mismatch\n",stderr);return 1;
 }
 willowArcadeTaskWake(&bus);objectTaskScatter(expected,&runtime,control,task,globals);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;

 printf("Panel setup diagnostic cost: %u emulator slices; not a frame-budget pass\n",slices);
 setVideo(video);puts("Resident intro panel: 4176 raw source writes, overlay, camera, timer, live words, sleep/wake and full RAM/SRAM match");return 0;
}

static int nativeIntroPanelHoldCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],saved[65536];if(!sram)return 2;
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals"),palettes=symbol(symbols,"debugPaletteControllers");
 if(word(ram,globals+16)!=4||word(ram,globals+20)!=10||word(ram,globals+22)!=8)return 2;
 unsigned args[]={0x20,0},calls=0;setVideo(nativeRngVideo);
 while(word(ram,globals+20)!=14){
  if(calls>=72)return 1;
  memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);memset(nptSource,0,sizeof nptSource);
  const unsigned from[]={0,0x200,0xa10,0xf042},to[]={task,control,control+16,palettes},sizes[]={512,16,32,1024};
  for(unsigned r=0;r<4;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[from[r]+n]=ram[(to[r]+n)^1];
  for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
   const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
   for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
  }
  WillowNativeTaskRuntime runtime;for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
  const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
  unsigned state=nptRead(0x8008),d1=bfpWord(0x888e + state),d2=nptRead(0x24);
  if(state==10&&nptRead(0x800a)==1){d1=0x3003;d2=16;}
  if(state==12&&nptRead(0x800a)==1){
   uint8_t tags[16];for(unsigned n=0;n<16;n++){tags[n]=ram[(palettes+n*64)^1];ram[(palettes+n*64)^1]=expected[(palettes+n*64)^1]=1;}
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
   for(unsigned n=0;n<16;n++)ram[(palettes+n*64)^1]=expected[(palettes+n*64)^1]=tags[n];
  }
  if(willowArcadeAttractPanelWait(&bus,state,bfpWord)!=WILLOW_ATTRACT_RETURN)return 1;
  nptWrite(0x204,0xff);nptWrite(0x206,0x20);nptWrite(0x20,0x0101);
  nptWrite(0x22,d1);nptWrite(0x24,d2);nptPut(0x8296,nptByte(0x8296)|12);
  objectTaskScatter(expected,&runtime,control,task,globals);
  for(unsigned n=0;n<1024;n++)expected[(palettes+n)^1]=nptByte(0xf042+n);
  willowMotionPutWord(saved+0x8176,d1);willowMotionPutWord(saved+0x8178,d2);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
   fprintf(stderr,"Panel hold mismatch call %u state %u\n",calls,state);return 1;
  }
  willowArcadeTaskWake(&bus);objectTaskScatter(expected,&runtime,control,task,globals);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  calls++;
 }
 if(calls!=72||word(ram,globals+22)!=28)return 1;
 sceneCommitPut(ram,globals+20,40);sceneCommitPut(expected,globals+20,40);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 sceneCommitPut(ram,globals+20,14);
 setVideo(video);puts("Resident panel hold: 72 dispatch/wake pairs, both fade allocations and original palette controller match full RAM/SRAM; exhaustion rejects atomically; allocated effects not executed concurrently");return 0;
}
