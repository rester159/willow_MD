#include "../src/willow_native_attract_demo_finish.h"
#include "../src/willow_native_task_runtime.h"
static uint32_t finishExpectedA4;
static uint16_t finishExpectedD1,finishExpectedD2;
static void finishExpectedColor(uint32_t at,uint16_t value){(void)at;(void)value;abort();}
static void finishExpectedRun(uint16_t*d1,uint16_t*d2)
{
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 if(!willowNativeAttractDemoFinishBoundary(&bus,bfpWord,d1,d2,&finishExpectedA4))abort();
 finishExpectedD1=*d1;finishExpectedD2=*d2;
}
/* Continue the actual cartridge endpoint, comparing each finish service with
 * the separately original-CPU-verified translation. Text tasks use the real
 * native task dispatcher and their existing complete-memory verifier. */
static int nativeDemoFinishBoundaryModeCheck(void*core,uint8_t*ram,uint8_t*sram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),unsigned routed)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!setReg)return 2;
 unsigned records=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals"),sound=symbol(symbols,"soundCommands");
 uint8_t expected[0xf600],expectedSRAM[65536];memcpy(expected,ram,sizeof expected);memcpy(expectedSRAM,sram,sizeof expectedSRAM);
 memset(nptSource,0,sizeof nptSource);
 const unsigned src[]={0,0x200,0xa10,0x8172},dst[]={records,control,control+16,sound},sizes[]={512,16,32,128};
 for(unsigned r=0;r<4;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[src[r]+n]=ram[(dst[r]+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 nptWrite(0x801e,word(ram,control+84));nptWrite(0x8344,willowMotionWord(sram+0x8036));
 uint16_t d1=willowMotionWord(sram+0x8176),d2=willowMotionWord(sram+0x8178);uint32_t a4=willowMotionLong(sram+0x86fc);
 unsigned args[]={d1,d2,a4};const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 WillowNativeTaskRuntime runtime={0};
 if(routed){
  for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
  finishExpectedA4=a4;finishExpectedD1=d1;finishExpectedD2=d2;
  WillowNativeTaskRun run=willowNativeTaskRunOneWithDemoFinish(&runtime,&bus,0x20,0,bfpWord,0,finishExpectedColor,0,0,0,0,0,0,0,0,0,0,finishExpectedRun);
  if(run.unsupported||!run.dispatched)return 1;
  d1=finishExpectedD1;d2=finishExpectedD2;a4=finishExpectedA4;
 }else if(!willowArcadeAttractDemoFinish(&bus,bfpWord,&d1,&d2,&a4))return 1;
 for(unsigned r=0;r<4;r++)for(unsigned n=0;n<sizes[r];n++)expected[(dst[r]+n)^1]=nptSource[src[r]+n];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)expected[(globals+g->offset+n)^1]=nptSource[g->source+n];
 }
 if(routed)for(unsigned n=0;n<16;n++)sceneCommitPut(expected,control+52+n*2,runtime.resume[n]);
 sceneCommitPut(expected,control+84,nptRead(0x801e));
 willowMotionPutWord(expectedSRAM+0x8176,d1);willowMotionPutWord(expectedSRAM+0x8178,d2);willowMotionPutLong(expectedSRAM+0x86fc,a4);
 unsigned dispatchArgs[]={0x20,0};
 unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,routed?"sourceSceneBootTaskDispatch":"sourceDemoFinishProbe",routed?2:3,routed?dispatchArgs:args);
 if(result!=(routed?0x424f:1)||memcmp(ram,expected,sizeof expected)||memcmp(sram,expectedSRAM,sizeof expectedSRAM)){
  fprintf(stderr,"Native demo finish boundary mismatch mode %u/%u timer %u\n",nptRead(0x8004),nptRead(0x8008),nptRead(0x800a));
  for(unsigned n=0;n<sizeof expected;n++)if(ram[n]!=expected[n]){fprintf(stderr,"Finish RAM %x %x/%x\n",n^1,ram[n],expected[n]);break;}
  return 1;
 }
 return 0;
}
static int nativeDemoFinishBoundaryCheck(void*core,uint8_t*ram,uint8_t*sram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{return nativeDemoFinishBoundaryModeCheck(core,ram,sram,symbols,runFrame,getReg,0);}
static int nativeDemoFinishCheck(void*core,uint8_t*ram,uint8_t*sram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 unsigned globals=symbol(symbols,"sourceBootGlobals");
 unsigned phase=globals+nativeBootGlobalOffset(0x8004),state=globals+nativeBootGlobalOffset(0x8008);
 unsigned busy=globals+nativeBootGlobalOffset(0xe58),timer=globals+nativeBootGlobalOffset(0x800a);
 if(word(ram,phase)!=6||word(ram,state)!=6||!ram[busy^1])return 2;
 if(nativeDemoFinishBoundaryCheck(core,ram,sram,symbols,runFrame,getReg))return 1;
 if(nativeOpeningMessageCheck(core,ram,symbols,runFrame,getReg,0xc0))return 1;
 if(nativeDemoFinishBoundaryCheck(core,ram,sram,symbols,runFrame,getReg))return 1;
 if(word(ram,state)!=8||word(ram,timer)!=180||!ram[busy^1])return 1;
 if(nativeDemoFinishBoundaryCheck(core,ram,sram,symbols,runFrame,getReg))return 1;
 if(nativeOpeningMessageCheck(core,ram,symbols,runFrame,getReg,0xc0))return 1;
 for(unsigned tick=0;tick<180;tick++)if(nativeDemoFinishBoundaryCheck(core,ram,sram,symbols,runFrame,getReg))return 1;
 if(word(ram,phase)!=8||word(ram,state)||word(ram,timer)||ram[busy^1])return 1;
 puts("Native demo finish: actual update-900 endpoint continues through both real text tasks, busy waits and 180 hold ticks to phase 8/0; every task/service boundary matches source-derived state, text/HUD, sound and unrelated RAM/SRAM; service-driven, not full scheduler/FPS integration");
 return 0;
}
static int nativeDemoFinishCasesCheck(void*core,uint8_t*ram,uint8_t*sram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!setReg)return 2;
 uint8_t saved[65536],savedSRAM[65536],expected[0xf600],expectedSRAM[65536];
 memcpy(saved,ram,sizeof saved);memcpy(savedSRAM,sram,sizeof savedSRAM);
 unsigned records=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals");
 for(unsigned stage=0;stage<6;stage++)for(unsigned kind=0;kind<2;kind++){
  memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);
  unsigned at=(kind?0x8e8e:0x8e50)+stage,packed=bfpWord(at&~1u),id=0x600|((at&1)?packed&255:packed>>8);
  unsigned position=kind?bfpWord(0x8e94+stage*2):0x1422;
  sceneCommitPut(ram,records+0xc0,0x0c00);sceneCommitPut(ram,records+0xc4,0);sceneCommitPut(ram,records+0xc6,0x78f0);
  sceneCommitPut(ram,records+0xd0,id);sceneCommitPut(ram,records+0xd2,position);
  sceneCommitPut(ram,control+52+6*2,0);
  ram[(globals+nativeBootGlobalOffset(0xe58))^1]=1;
  if(nativeOpeningMessageCheck(core,ram,symbols,runFrame,getReg,0xc0))return 1;
 }
 if(getenv("WILLOW_AUDIT_DEMO_FINISH_ROUTING")){
  const unsigned timers[]={0,1,2,180,65535};
  for(unsigned test=0;test<240;test++){
   memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);
   unsigned state=test&1?8:6,stage=(test/2)%6,busy=(test/12)%2,freeze=test/120;
   sceneCommitPut(ram,globals+nativeBootGlobalOffset(0x8000),4);
   sceneCommitPut(ram,globals+nativeBootGlobalOffset(0x8004),6);
   sceneCommitPut(ram,globals+nativeBootGlobalOffset(0x8008),state);
   sceneCommitPut(ram,globals+nativeBootGlobalOffset(0x800a),timers[(test/24)%5]);
   ram[(globals+nativeBootGlobalOffset(0xe58))^1]=busy;
   ram[(globals+nativeBootGlobalOffset(0x828f))^1]=1;
   ram[(globals+nativeBootGlobalOffset(0x8068))^1]=freeze?2:0;
   ram[(globals+nativeBootGlobalOffset(0x8297))^1]=1;
   ram[(globals+nativeBootGlobalOffset(0x8058))^1]=0;
   ram[(globals+nativeBootGlobalOffset(0x82a2))^1]=0;
   ram[(globals+nativeBootGlobalOffset(0x82a4))^1]=0;
   willowMotionPutWord(sram+0x8036,stage);
   sceneCommitPut(ram,records+0x20,0x0400);
   sceneCommitPut(ram,records+0x22,test*311);sceneCommitPut(ram,records+0x24,test*977);
   sceneCommitPut(ram,control+52+2,0x810e);
   for(unsigned slot=0x100;slot<0x200;slot+=32){ram[(records+slot)^1]=slot==0x160?4:0;ram[(records+slot+22)^1]=1;}
   sceneCommitPut(ram,control+8,test&1?0xffff:0xff);sceneCommitPut(ram,control+10,0xa30);sceneCommitPut(ram,control+12,7);
   if(nativeDemoFinishBoundaryModeCheck(core,ram,sram,symbols,runFrame,getReg,1)){
    fprintf(stderr,"Native finish manager dispatch case %u failed\n",test);return 1;
   }
  }
  puts("Native finish manager routing: 240 compiled slot-20 dispatches match source-verified manager, runtime continuation, live registers, freeze gate, sound/cancellation and all unrelated RAM/SRAM");
 }
 for(unsigned test=0;test<8;test++){
  memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);
  sceneCommitPut(ram,globals+nativeBootGlobalOffset(0x8004),test==0?4:6);
  sceneCommitPut(ram,globals+nativeBootGlobalOffset(0x8008),test==1?4:test==2?6:8);
  sceneCommitPut(ram,globals+nativeBootGlobalOffset(0x800a),1);
  ram[(globals+nativeBootGlobalOffset(0xe58))^1]=0;
  ram[(globals+nativeBootGlobalOffset(0x8297))^1]=1;
  willowMotionPutWord(sram+0x8036,test==2?6:0);
  ram[(records+0x100)^1]=4;ram[(records+0x116)^1]=1;
  const unsigned pointers[]={0xff0a30,0xff0a30,0xff0a30,0xff0a10,0xff0a34,0xff0a2f,0x200a30,0xff0a30};
  sceneCommitPut(ram,control+8,pointers[test]>>16);sceneCommitPut(ram,control+10,pointers[test]);sceneCommitPut(ram,control+12,test==7?8:1);
  memcpy(expected,ram,sizeof expected);memcpy(expectedSRAM,sram,sizeof expectedSRAM);
  unsigned args[]={0x1234,0xabcd,0xff9876};
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceDemoFinishProbe",3,args)!=0||memcmp(ram,expected,sizeof expected)||memcmp(sram,expectedSRAM,sizeof expectedSRAM)){
   fprintf(stderr,"Native finish rejection case %u failed\n",test);return 1;
  }
  if(getenv("WILLOW_AUDIT_DEMO_FINISH_ROUTING")){
   if(!test)sceneCommitPut(ram,globals+nativeBootGlobalOffset(0x8004),10);
   if(test==1&&getenv("WILLOW_AUDIT_DEMO_TASK"))sceneCommitPut(ram,globals+nativeBootGlobalOffset(0x8008),10);
   sceneCommitPut(ram,records+0x20,0x0400);sceneCommitPut(ram,control+54,0x810e);
   memcpy(expected,ram,sizeof expected);memcpy(expectedSRAM,sram,sizeof expectedSRAM);
   unsigned dispatchArgs[]={0x20,0};
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,dispatchArgs)!=1||memcmp(ram,expected,sizeof expected)||memcmp(sram,expectedSRAM,sizeof expectedSRAM)){
    fprintf(stderr,"Native finish manager rejection case %u failed\n",test);return 1;
   }
  }
 }
 memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);
 puts("Native demo finish cases: all 12 stage-specific text tasks complete with exact text/HUD and task state; eight invalid state/stage/free-stack cases reject without mutation; constructed inputs restored");
 return 0;
}
