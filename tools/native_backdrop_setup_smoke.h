#include "../src/willow_arcade_attract_backdrop.h"
#include "../src/willow_arcade_attract_scene_wait.h"
static int nativeBackdropSetupCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),unsigned countdown)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],initial[0xf600],saved[65536];
 if(!sram)return 2;
 setVideo(nativeRngVideo);memcpy(expected,ram,sizeof expected);memcpy(initial,ram,sizeof initial);memcpy(saved,sram,sizeof saved);
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals");
 unsigned record=symbol(symbols,"debugScroll3Record"),palette=symbol(symbols,"debugPaletteControllers");
 unsigned map=longword(ram,symbol(symbols,"debugScroll3Cells"))&65535,flags=symbol(symbols,"sourceSceneClearedPlanes");
 for(unsigned test=0;test<3;test++){
  if(test==0)ram[(record+33)^1]=expected[(record+33)^1]=1;
  if(test==1)ram[flags^1]=expected[flags^1]=ram[flags^1]|4;
  if(test==2)for(unsigned n=0;n<1024;n+=64)ram[(palette+n)^1]=expected[(palette+n)^1]=1;
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceBackdropSetupProbe",0,NULL)||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  if(countdown){
   unsigned args[]={0x20,0};
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
      memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  }
  memcpy(ram,initial,sizeof initial);memcpy(expected,initial,sizeof expected);
 }
 memset(nptSource,0,sizeof nptSource);
 const unsigned src[]={0,0x0200,0x0a10,0x801a,0x8072,0x8482,0xf042};
 const unsigned dst[]={task,control,control+16,control+48,symbol(symbols,"taskCommands"),record,palette};
 const unsigned size[]={512,16,32,4,256,128,1024};
 for(unsigned r=0;r<7;r++)for(unsigned n=0;n<size[r];n++)nptSource[src[r]+n]=ram[(dst[r]+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 uint16_t cells[WILLOW_SCROLL3_MAP_WORDS];
 for(unsigned n=0;n<WILLOW_SCROLL3_MAP_WORDS;n++)cells[n]=word(ram,map+n*2);
 for(unsigned n=0;n<4096;n++){
  unsigned id=willowScroll3MapId(cells,n);if(id>=WILLOW_SCROLL3_PAIR_COUNT)return 2;
  willowMotionPutLong(spawnMap+n*4,nativeScroll3Pairs[id]);
 }
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};spawnWrites=0;
 if(!willowArcadeAttractBackdrop(&bus,nptSource+0x8482,spawnByte,bfpWord,spawnWrite)||spawnWrites!=258)return 1;
 for(unsigned r=0;r<7;r++)for(unsigned n=0;n<size[r];n++)expected[(dst[r]+n)^1]=nptSource[src[r]+n];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)expected[(globals+g->offset+n)^1]=nptSource[g->source+n];
 }
 for(unsigned n=0;n<4096;n++){
  uint32_t pair=willowMotionLong(spawnMap+n*4);unsigned lo=0,hi=WILLOW_SCROLL3_PAIR_COUNT;
  while(lo<hi){unsigned mid=(lo+hi)/2;if(nativeScroll3Pairs[mid]<pair)lo=mid+1;else hi=mid;}
  if(lo==WILLOW_SCROLL3_PAIR_COUNT||nativeScroll3Pairs[lo]!=pair||!willowScroll3MapSet(cells,n,lo))return 1;
 }
 for(unsigned n=0;n<WILLOW_SCROLL3_MAP_WORDS;n++)sceneCommitPut(expected,map+n*2,cells[n]);
 expected[flags^1]&=~2;expected[symbol(symbols,"sourceScenePreparedPlanes")^1]&=~2;
 unsigned args[]={0x20,0};
 if(countdown){
  willowMotionPutWord(saved+0x8176,4);willowMotionPutWord(saved+0x8178,16);
  sceneCommitPut(expected,control+4,0xff);sceneCommitPut(expected,control+6,0x20);
  sceneCommitPut(expected,task+0x20,0x0101);sceneCommitPut(expected,task+0x22,4);
  sceneCommitPut(expected,task+0x24,16);expected[(globals+111)^1]|=12;
 }
 unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,countdown?"sourceSceneBootTaskDispatch":"sourceBackdropSetupProbe",countdown?2:0,args);
 if(result!=(countdown?0x424f:1)||memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
  fprintf(stderr,"Resident backdrop mismatch result %x\n",result);
  for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Backdrop RAM %04x %02x/%02x\n",n^1,ram[n],expected[n]);break;}return 1;
 }
 if(countdown){
  for(unsigned n=0;n<512;n+=32)if(expected[(task+n)^1]==1){
   unsigned at=(task+n+1)^1;expected[at]--;
   if(!expected[at])expected[(task+n)^1]=4;
  }
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  sceneCommitPut(ram,globals+20,12);sceneCommitPut(expected,globals+20,12);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  sceneCommitPut(ram,globals+20,4);sceneCommitPut(expected,globals+20,4);
 }
 if(countdown){
  if(nativeOpeningActorsCheck(core,ram,symbols,runFrame,getReg,2))return 1;
  setVideo(nativeRngVideo);memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);
  for(unsigned n=0;n<512;n++)nptSource[n]=ram[(task+n)^1];
  for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
   const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
   for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
  }
  if(willowArcadeAttractSceneWait(&bus,0x8480,bfpWord)!=WILLOW_ATTRACT_RETURN)return 1;
  for(unsigned n=0;n<512;n++)expected[(task+n)^1]=nptSource[n];
  for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
   const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
   for(unsigned n=0;n<g->count;n++)expected[(globals+g->offset+n)^1]=nptSource[g->source+n];
  }
  sceneCommitPut(expected,task+0x20,0x0101);sceneCommitPut(expected,task+0x22,0x34);
  sceneCommitPut(expected,task+0x24,0x1607);expected[(globals+111)^1]|=12;
  willowMotionPutWord(saved+0x8176,0x34);willowMotionPutWord(saved+0x8178,0x1607);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
   fputs("Resident 8480 terminal dispatch mismatch\n",stderr);return 1;
  }
  for(unsigned n=0;n<512;n+=32)if(expected[(task+n)^1]==1){
   unsigned at=(task+n+1)^1;expected[at]--;
   if(!expected[at])expected[(task+n)^1]=4;
  }
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  puts("Resident 8480 wait: 60 dispatched ticks reach original message-task start, timer 10, substate 6 and live D1=34/D2=1607; full RAM/SRAM match");
 }
 setVideo(video);
 if(countdown)puts("Resident countdown terminal tick: original 180th call performs backdrop setup, sets 60 ticks and returns D1=4/D2=16");
 puts("Resident 83B8 backdrop: all 258 source map writes, packed ring, controller, two palette records, timer and fade task exact; full unrelated RAM/SRAM retained; three missing-capability cases reject");return 0;
}
