#include "../src/willow_native_hud_surface.h"

static uint8_t *messageExpectedSRAM;
static WillowHudSurface messageHud;
static unsigned messageWrites;
static int nativeDemoFinishMessagePair(unsigned id,unsigned position)
{
 for(unsigned stage=0;stage<6;stage++){
  unsigned at=0x8e50+stage,value=bfpWord(at&~1u);
  if(id==(0x600|((at&1)?value&255:value>>8))&&position==0x1422)return 1;
  at=0x8e8e + stage;value=bfpWord(at&~1u);
  if(id==(0x600|((at&1)?value&255:value>>8))&&position==bfpWord(0x8e94+stage*2))return 1;
 }
 return 0;
}
static void messageVideoWord(uint32_t at,uint16_t value)
{
 if(at<0x910000||at>=0x914000||(at&1))abort();
 willowMotionPutWord(messageExpectedSRAM+0x4000+at-0x910000,value);
 willowHudSurfaceWrite(&messageHud,at,value);messageWrites++;
}
static int nativeOpeningMessageCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),unsigned slot)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],saved[65536];
 if(!sram)return 2;
 unsigned control=symbol(symbols,"sourceTaskState"),records=symbol(symbols,"taskRecords");
 unsigned globals=symbol(symbols,"sourceBootGlobals"),hud=symbol(symbols,"debugHudSurface");
 unsigned flags=symbol(symbols,"sourceSceneClearedPlanes");
 memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);
 messageExpectedSRAM=saved;messageWrites=0;
 for(unsigned n=0;n<sizeof messageHud/2;n++)((uint16_t*)&messageHud)[n]=word(ram,hud+n*2);
 memset(nptSource,0,sizeof nptSource);
 const unsigned src[]={0,0x200,0xa10},dst[]={records,control,control+16},sizes[]={512,16,32};
 for(unsigned r=0;r<3;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[src[r]+n]=ram[(dst[r]+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 WillowNativeTaskRuntime runtime;
 for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 unsigned id=nptRead(slot+16),position=nptRead(slot+18);
 if(nptRead(slot)!=0x0c00||!((slot==0xc0&&(((getenv("WILLOW_AUDIT_DEMO_FINISH")||getenv("WILLOW_AUDIT_DEMO_FINISH_CASES"))&&nativeDemoFinishMessagePair(id,position))||(id==0x34&&position==0x1607)||
    (id==0x35&&position==0x1608)||(id==0x1b&&position==0x0609)||(id==0x1d&&position==0x060e)||
                   (id==0x71&&position==0x260b)||(id==0x73&&position==0x260d)||
                   (id==0x3c&&position==0x160f)||(id==0x3d&&position==0x1610)))||
    (slot==0xe0&&((id==0x70&&position==0x240a)||(id==0x72&&position==0x240c))))||!nptByte(0x8293))return 2;
 setVideo(nativeRngVideo);unsigned args[]={slot,0},calls=0,wakes=0;
 /* Unsupported message identity and sound-enabled execution must reject first. */
 for(unsigned test=0;test<2;test++){
  unsigned at=test?globals+108:records+slot+17;
  uint8_t old=ram[at^1];ram[at^1]=expected[at^1]=test?0:0x36;
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  ram[at^1]=expected[at^1]=old;
 }
 while(nptByte(slot)){
  if(calls++>200)return 1;
  WillowNativeTaskRun result=willowNativeTaskRunOneWithVideo(&runtime,&bus,slot,0,bfpWord,0,0,0,0,messageVideoWord);
  if(!result.dispatched||result.unsupported)return 1;
  objectTaskScatter(expected,&runtime,control,records,globals);
  for(unsigned n=0;n<sizeof messageHud/2;n++)sceneCommitPut(expected,hud+n*2,((uint16_t*)&messageHud)[n]);
  if(messageWrites)expected[flags^1]&=~8;
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
   fprintf(stderr,"Resident opening message mismatch call %u\n",calls);
   for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Message RAM %x %x/%x\n",n^1,ram[n],expected[n]);break;}return 1;
  }
  while(nptByte(slot)==1){
   for(unsigned n=0;n<512;n+=32)if(nptByte(n)==1){nptPut(n+1,nptByte(n+1)-1);if(!nptByte(n+1))nptPut(n,4);}
   objectTaskScatter(expected,&runtime,control,records,globals);wakes++;
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
      memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
  }
  if(calls==1){
   const unsigned offsets[]={4,28,8,6};
   for(unsigned bad=0;bad<4;bad++){
    unsigned at=records+slot+offsets[bad];unsigned old=(uint16_t)word(ram,at);
    unsigned invalid=bad==3?old+(160>>(position>>12)):0;
    sceneCommitPut(ram,at,invalid);sceneCommitPut(expected,at,invalid);
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
       memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
    sceneCommitPut(ram,at,old);sceneCommitPut(expected,at,old);
   }
  }
 }
 if(nptByte(0x0e58)||nptByte(0x82a6)||runtime.resume[slot/32])return 1;
 setVideo(video);
 printf("Resident opening message %02x: %u dispatches, %u wakes, %u text words; full Scroll1/HUD, task context, completion flags and unrelated RAM/SRAM match; no Genesis display claim\n",id,calls,wakes,messageWrites);
 return 0;
}
/* Continue at the original second-message or final fade allocation boundary. */
static int nativeOpeningWaitTerminalCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),unsigned fade)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],saved[65536];
 if(!sram)return 2;
 unsigned control=symbol(symbols,"sourceTaskState"),records=symbol(symbols,"taskRecords");
 unsigned globals=symbol(symbols,"sourceBootGlobals");
 memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);
 const unsigned src[]={0,0x200,0xa10},dst[]={records,control,control+16},sizes[]={512,16,32};
 for(unsigned r=0;r<3;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[src[r]+n]=ram[(dst[r]+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 WillowNativeTaskRuntime runtime;
 for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 if(nptRead(0x8008)!=(fade?8:6)||nptRead(0x800a)!=1||nptByte(0xe58)||nptByte(0xc0))return 2;
 unsigned freeBefore=nptRead(0x20c);
 if(fade&&!freeBefore)return 2;
 if(willowArcadeAttractSceneWait(&bus,fade?0x84e0:0x84ac,bfpWord)!=WILLOW_ATTRACT_RETURN)return 1;
 if(fade&&nptRead(0x20c)!=freeBefore-1)return 1;
 nptWrite(0x204,0xff);nptWrite(0x206,0x20);nptWrite(0x20,0x0101);
 nptWrite(0x22,fade?3:0x35);nptWrite(0x24,fade?16:0x1608);nptPut(0x8296,nptByte(0x8296)|12);
 objectTaskScatter(expected,&runtime,control,records,globals);
 willowMotionPutWord(saved+0x8176,fade?3:0x35);willowMotionPutWord(saved+0x8178,fade?16:0x1608);
 setVideo(nativeRngVideo);unsigned args[]={0x20,0};
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 willowArcadeTaskWake(&bus);objectTaskScatter(expected,&runtime,control,records,globals);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 sceneCommitPut(ram,globals+20,12);sceneCommitPut(expected,globals+20,12);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 sceneCommitPut(ram,globals+20,fade?10:8);
 setVideo(video);
 if(fade)puts("Resident 84E0 terminal: original layer mirror, fade allocation, zero timer, substate 10 and live D1=3/D2=16 match with full task sleep/wake and RAM/SRAM");
 else puts("Resident 84AC terminal: original second message start, timer 30, substate 8, live D1=35/D2=1608 and task sleep/wake match; later unsupported state rejects atomically");
 return 0;
}
