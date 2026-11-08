#include "../src/willow_arcade_attract_scene_spawn.h"
static uint8_t spawnMap[16384];
static unsigned spawnWrites;
static uint8_t spawnByte(uint32_t at){return bfpROM[at];}
static void spawnWrite(uint16_t at,uint16_t value){willowMotionPutWord(spawnMap+at,value);spawnWrites++;}
static int nativeSceneSpawnCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],initial[0xf600],saved[65536],initialSRAM[65536];
 if(!sram)return 2;
 setVideo(nativeRngVideo);memcpy(expected,ram,sizeof expected);memcpy(initial,ram,sizeof initial);memcpy(saved,sram,sizeof saved);memcpy(initialSRAM,sram,sizeof initialSRAM);
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals");
 unsigned record=symbol(symbols,"debugScroll2Record"),palette=symbol(symbols,"debugPaletteControllers");
 unsigned pool=symbol(symbols,"debugActorPools"),actors=longword(ram,pool),indices=longword(ram,pool+4)&65535;
 unsigned map=longword(ram,symbol(symbols,"debugScroll2Map"))&65535,flags=symbol(symbols,"sourceSceneClearedPlanes");
 unsigned args[]={0x20,0};
 for(unsigned test=0;test<6;test++){
  unsigned at=test==0?pool+11:test==1?record+33:test==2?flags:test==3?pool+10:indices;
  if(test==5){
   for(unsigned n=0;n<1024;n+=64)ram[(palette+n)^1]=expected[(palette+n)^1]=1;
  }else ram[at^1]=expected[at^1]=test==0?0:test==1?1:test==2?ram[at^1]|4:50;
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneSpawnProbe",0,NULL)||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  memcpy(ram,initial,sizeof initial);memcpy(expected,initial,sizeof expected);
 }
 memset(nptSource,0,sizeof nptSource);
 const unsigned src[]={0,0x0200,0x0a10,0x801a,0x8072,0x8402,0xf042,0x9dc6};
 const unsigned dst[]={task,control,control+16,control+48,symbol(symbols,"taskCommands"),record,palette,actors};
 const unsigned size[]={512,16,32,4,256,128,1024,6400};
 for(unsigned r=0;r<8;r++)for(unsigned n=0;n<size[r];n++)nptSource[src[r]+n]=*nativeTargetByte(ram,sram,dst[r]+n);
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 nptSource[0x9801]=ram[(pool+11)^1];nptWrite(0x981c,0x912a+ram[(pool+10)^1]*2);
 for(unsigned n=0;n<50;n++)nptWrite(0x912a+n*2,0x9dc6+ram[(indices+n)^1]*128);
 for(unsigned n=0;n<4096;n++){
  unsigned id=(unsigned short)word(ram,map+n*2);if(id>=WILLOW_SCROLL2_PAIR_COUNT)return 2;
  willowMotionPutLong(spawnMap+n*4,nativeScroll2Pairs[id]);
 }
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};spawnWrites=0;
 if(!willowArcadeAttractSceneSpawn(&bus,nptSource+0x8402,spawnByte,bfpWord,spawnWrite)||spawnWrites!=1024)return 1;
 for(unsigned r=0;r<8;r++)for(unsigned n=0;n<size[r];n++)*nativeTargetByte(expected,saved,dst[r]+n)=nptSource[src[r]+n];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)expected[(globals+g->offset+n)^1]=nptSource[g->source+n];
 }
 expected[(pool+10)^1]=(nptRead(0x981c)-0x912a)/2;expected[(pool+11)^1]=nptByte(0x9801);
 for(unsigned n=0;n<4096;n++){
  uint32_t pair=willowMotionLong(spawnMap+n*4);unsigned lo=0,hi=WILLOW_SCROLL2_PAIR_COUNT;
  while(lo<hi){unsigned mid=(lo+hi)/2;if(nativeScroll2Pairs[mid]<pair)lo=mid+1;else hi=mid;}
  if(lo==WILLOW_SCROLL2_PAIR_COUNT||nativeScroll2Pairs[lo]!=pair)return 1;
  sceneCommitPut(expected,map+n*2,lo);
 }
 expected[flags^1]&=~1;expected[symbol(symbols,"sourceScenePreparedPlanes")^1]&=~1;
 unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneSpawnProbe",0,NULL);
 if(result!=1||memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
  fprintf(stderr,"Resident spawn mismatch result %x\n",result);
  for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Spawn RAM %04x %02x/%02x\n",n^1,ram[n],expected[n]);break;}return 1;
 }
 /* Replay the same initial state through the actual source task dispatcher. */
 memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
 sceneCommitPut(expected,control+4,0xff);sceneCommitPut(expected,control+6,0x20);
 sceneCommitPut(expected,task+0x20,0x0101);sceneCommitPut(expected,task+0x22,4);
 sceneCommitPut(expected,task+0x24,16);expected[(globals+111)^1]|=12;
 result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args);
 if(result!=0x424f||memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
  fprintf(stderr,"Resident spawn dispatch mismatch result %x\n",result);
  for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Spawn dispatch RAM %04x %02x/%02x\n",n^1,ram[n],expected[n]);break;}return 1;
 }
 for(unsigned n=0;n<512;n+=32)if(expected[(task+n)^1]==1){
  unsigned at=(task+n+1)^1;expected[at]--;
  if(!expected[at])expected[(task+n)^1]=4;
 }
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 sceneCommitPut(ram,globals+20,12);sceneCommitPut(expected,globals+20,12);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 sceneCommitPut(ram,globals+20,2);
 setVideo(video);
 puts("Resident 82BE dispatch: live D1=4/D2=16, original sleep/wake and source allocation state exact; unimplemented substate twelve rejects before mutation");
 puts("Resident 82BE spawn: 1024 source map writes, two palette controllers, class-4 actor and three fade task allocations exact; full retained and unrelated RAM/SRAM match; six missing-capability/invalid-storage cases reject through both isolated and task-dispatch entries");
 return 0;
}
