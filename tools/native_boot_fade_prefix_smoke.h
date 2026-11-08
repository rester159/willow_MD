#include "../src/willow_native_boot_runtime.h"
static uint8_t bfpROM[0x100000];
static uint16_t bfpWord(uint32_t at){return willowMotionWord(bfpROM+at);}
static uint8_t *bfpExpectedRam,*bfpExpectedSram;
static unsigned bfpNear,bfpFar,bfpHud,bfpFills,bfpCleared,bfpPrepared,bfpTextWrites;
static void bfpPut(unsigned at,unsigned value)
{bfpExpectedRam[at^1]=value>>8;bfpExpectedRam[(at+1)^1]=value;}
static void bfpVideoWord(uint32_t at,uint16_t value)
{
 if(at<0x910000||at>=0x914000||(at&1)){fprintf(stderr,"Unexpected boot text address %x\n",at);abort();}
 unsigned offset=at-0x910000;
 bfpExpectedSram[0x4000+offset]=value>>8;bfpExpectedSram[0x4001+offset]=value;
 bfpExpectedRam[bfpCleared^1]&=~8;bfpTextWrites++;
 unsigned sourceRow=(offset/4)%32,col=offset/128,half=(offset/2)%2;
 const unsigned rows[]={2,3,26,28,29,30};
 for(unsigned r=0;r<6;r++)if(sourceRow==rows[r]&&col<48){
  unsigned cell=bfpHud+(r*48+col)*4+half*2;
  if((unsigned short)word(bfpExpectedRam,cell)!=value){
   bfpPut(cell,value);
   /* Derive dirty output tiles from the pixel-center projection. */
   for(unsigned x=0;x<320;x++)if(((6*x+3)/5)/8==col){
    unsigned tile=x/8,dirty=bfpHud+1152+(r*3+tile/16)*2;
    bfpPut(dirty,(unsigned short)word(bfpExpectedRam,dirty)|(1u<<(tile%16)));
   }
  }
 }
}
static void bfpVideoFill(uint32_t at,uint16_t first,uint16_t second,uint16_t pairs)
{
 static const uint32_t addresses[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 static const uint16_t tiles[]={0x7020,0,0x0a00,0,0};
 unsigned region=bfpFills%5;
 if(bfpFills>=20||at!=addresses[region]||first!=tiles[region]||second||
    pairs!=(region<3?4096:512)){fprintf(stderr,"Unexpected boot fill %x\n",at);abort();}
 if(region==0){
  for(unsigned n=0;n<4096;n++){
   bfpExpectedSram[0x4000+n*4]=0x70;bfpExpectedSram[0x4001+n*4]=0x20;
   bfpExpectedSram[0x4002+n*4]=bfpExpectedSram[0x4003+n*4]=0;
  }
  for(unsigned n=0;n<288;n++){bfpPut(bfpHud+n*4,0x7020);bfpPut(bfpHud+n*4+2,0);}
  for(unsigned n=0;n<18;n++)bfpPut(bfpHud+1152+n*2,n%3==2?255:65535);
 }else if(region==1){
  for(unsigned n=0;n<4096;n++)bfpPut(bfpNear+n*2,WILLOW_SCROLL2_ZERO_PAIR);
 }else if(region==2){
  for(unsigned n=0;n<5632;n++)bfpExpectedRam[(bfpFar+n)^1]=0;
  for(unsigned bit=0;bit<4096*11;bit++)
   bfpExpectedRam[(bfpFar+bit/8)^1]|=((WILLOW_SCROLL3_CLEAR_PAIR>>(10-bit%11))&1)<<(7-bit%8);
 }else memset(bfpExpectedSram+(region-3)*2048,0,2048);
 if(region<3){
  unsigned bit=region==0?8:1u<<(region-1);
  bfpExpectedRam[bfpCleared^1]|=bit;bfpExpectedRam[bfpPrepared^1]&=~bit;
 }
 bfpFills++;
}
/* Execute the boot/consumer/clear/fade chain. Without scene storage, stop
 * at its first unavailable video write; with storage, continue through the
 * real clear wait and option decoding. No completion flag is injected. */
static int nativeBootFadePrefixCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),size_t sceneCapacity,unsigned warning)
/* warning: 0=first clear/options, 1=warning clear, 2=normal handoff,
 * 3=service-switch handoff, 4=first attract initialization, 5=first attract fade. Completion is never forced. */
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 if(!setReg||!setVideo)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(bfpROM,1,sizeof bfpROM,f)!=sizeof bfpROM)return 2;fclose(f);
 setVideo(nativeRngVideo);
 uint8_t *sram=NULL,expectedSram[65536];
 if(sceneCapacity){
  void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
  if(sceneCapacity!=65536||!memory||!(sram=memory(RETRO_MEMORY_SAVE_RAM)))return 2;
  unsigned args[]={4,0,0,0,0};
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceObjectCartridgeProbe",5,args)!=1)return 1;
  for(unsigned n=0;n<65536;n++)sram[n]=(n*23+17)^0xa5;
  /* Project the separately source-CPU-verified cold clear before the boot
   * chain. Assert that the compiled reset changes only its 1243 owned bytes. */
  uint8_t coldRAM[0xf600];memcpy(coldRAM,ram,sizeof coldRAM);
  memcpy(expectedSram,sram,sizeof expectedSram);memset(expectedSram+0x8200,0,321);memset(expectedSram+0x8400,0,111);memset(expectedSram+0x8500,0,27);memset(expectedSram+0x8800,0,784);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceDemoColdResetProbe",0,NULL)!=1||
     memcmp(coldRAM,ram,sizeof coldRAM)||memcmp(expectedSram,sram,sizeof expectedSram))return 1;
  printf("Retained demo cold reset: 1243 source-derived bytes cleared; full unrelated RAM/SRAM preserved\n");
  memcpy(expectedSram,sram,sizeof expectedSram);bfpExpectedSram=expectedSram;bfpFills=0;bfpTextWrites=0;
  unsigned map=symbol(symbols,"debugScroll2Map");
  bfpNear=longword(ram,map)&65535;bfpFar=longword(ram,symbol(symbols,"debugScroll3Cells"))&65535;
  bfpHud=symbol(symbols,"debugHudSurface");
  bfpCleared=symbol(symbols,"sourceSceneClearedPlanes");bfpPrepared=symbol(symbols,"sourceScenePreparedPlanes");
  if(word(ram,map+14)||ram[(map+16)^1]||bfpNear+8192>0xf600||bfpFar+5632>0xf600||bfpHud+1188>0xf600)return 1;
 }
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceBootPaletteInitialize",0,NULL)!=1)return 1;
 unsigned control=symbol(symbols,"sourceTaskState"),records=symbol(symbols,"taskRecords");
 unsigned globals=symbol(symbols,"sourceBootGlobals"),commands=symbol(symbols,"taskCommands"),owner=symbol(symbols,"debugPaletteOwner");
 const unsigned from[]={0,0x0200,0x0a10,0x801a,0x8072},to[]={records,control,control+16,control+48,commands},sizes[]={512,16,32,4,256};
 uint8_t expected[0xf600],baseColors[6144];
 bfpExpectedRam=expected;
 memset(nptSource,0,sizeof nptSource);
 WillowNativeBootGlobals reset;willowNativeBootGlobalsReset(&reset);
 for(unsigned n=0;n<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];n++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+n;
  memcpy(nptSource+g->source,reset.bytes+g->offset,g->count);
 }
 f=fopen("build/boot-palette-original-words.bin","rb");if(!f||fread(nptSource+0x2000,1,6144,f)!=6144)return 2;fclose(f);
 for(unsigned g=0;g<192;g++){
  unsigned tag=nativeSoundWord(ram,owner+g*2);
  if(tag>0xfff0||0xe0000+tag*2+32>sizeof bfpROM)return 1;
  memcpy(baseColors+g*32,bfpROM+0xe0000+tag*2,32);
 }
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 unsigned nearRecord=symbol(symbols,"debugScroll2Record"),farRecord=symbol(symbols,"debugScroll3Record");
 if(warning>=4)for(unsigned n=0;n<128;n++){
  nptSource[0x8402+n]=ram[(nearRecord+n)^1];nptSource[0x8482+n]=ram[(farRecord+n)^1];
 }
 WillowNativeTaskRuntime runtime={0};
 if(warning==3)nptPut(0x8058,0x40); /* Latched service-switch input. */
 willowArcadeTaskInitFree(&bus,bfpWord);willowArcadeTaskInitStacks(&bus,bfpWord);
 for(unsigned n=0;n<128;n++)nptWrite(0x8072+n*2,0xffff);
 willowArcadeTaskStart(&bus,0xa0,0x1ddc,0,0,1);
 willowArcadeTaskStart(&bus,0,0x7fda,0,0,1);
 for(unsigned bank=0;bank<6;bank++){nptWrite(0x82a8+bank*4,bank<4?65535:0);nptWrite(0x82aa+bank*4,bank<4?65535:255);}
#define BFP_SCATTER(dst) do { \
 for(unsigned r=0;r<5;r++)for(unsigned n=0;n<sizes[r];n++)(dst)[(to[r]+n)^1]=nptSource[from[r]+n]; \
 for(unsigned n=0;n<16;n++){(dst)[(control+52+n*2)^1]=runtime.resume[n]>>8;(dst)[(control+53+n*2)^1]=runtime.resume[n];} \
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){ \
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r; \
  for(unsigned n=0;n<g->count;n++)(dst)[(globals+g->offset+n)^1]=nptSource[g->source+n];} \
 if(warning>=4)for(unsigned n=0;n<128;n++){ \
  (dst)[(nearRecord+n)^1]=nptSource[0x8402+n];(dst)[(farRecord+n)^1]=nptSource[0x8482+n];} \
}while(0)
 BFP_SCATTER(ram);
 unsigned dispatched=0,waits=0,fadeSteps=0,colors=0,wakes=0,blocked=0,completed=0;
 for(unsigned pass=0;pass<512&&!blocked&&!completed;pass++){
  for(unsigned slot=0;slot<512;slot+=32){
   memcpy(expected,ram,sizeof expected);
   unsigned pc=runtime.resume[slot/32];
   WillowNativeTaskRun result=willowNativeTaskRunWithStoredBootVideoFill(&runtime,&bus,slot,0,bfpWord,nptColor,nptColorWrite,0,0,
    sceneCapacity?bfpVideoWord:0,sceneCapacity?bfpVideoFill:0);
   BFP_SCATTER(expected);
   if(warning>=4&&slot==0x20&&pc==0x1ea2&&runtime.resume[1]==0x810e){
    const unsigned src[]={0xe9920,0xeb700,0xe72a0,0xe5280};
    const unsigned first[]={64,96,32,16};
    for(unsigned block=0;block<4;block++)for(unsigned n=0;n<(block==3?16:32);n++){
     unsigned g=first[block]+n,from=src[block]+n*32;
     memcpy(baseColors+g*32,bfpROM+from,32);bfpPut(owner+g*2,(from-0xe0000)/2);
    }
   }
   for(unsigned g=0;g<192;g++){
    unsigned base=willowMotionWord(baseColors+g*32),value=nptRead(0x2000+g*32);
    unsigned offset=((uint16_t)(value-base))>>12;
    unsigned at=(owner+384+g/2)^1,shift=(g&1)?4:0;
    expected[at]=(expected[at]&~(15<<shift))|(offset<<shift);
    for(unsigned n=0;n<16;n++){
     if((uint16_t)(willowMotionWord(baseColors+g*32+n*2)+(offset<<12))!=nptRead(0x2000+g*32+n*2))return 1;
     colors++;
    }
   }
   unsigned args[]={slot,0},want=result.unsupported?1:result.dispatched?0x424f:2;
   unsigned got=nativeSATCall(ram,symbols,setReg,getReg,runFrame,sceneCapacity?"sourceSceneBootTaskDispatch":"sourcePaletteTaskDispatch",2,args);
   if(got!=want||memcmp(expected,ram,sizeof expected)||(sram&&memcmp(sram,expectedSram,sizeof expectedSram))){
    fprintf(stderr,"Boot/fade prefix mismatch pass %u slot %x pc %x status %x/%x\n",pass,slot,pc,got,want);
    for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"First byte %04x got %02x want %02x\n",n^1,ram[n],expected[n]);break;}return 1;
   }
   if(result.unsupported){
    if(pc!=0x586e||!nptRead(0x82c0)||nptByte(0xf891)!=1||nptRead(0x8000)!=0){fprintf(stderr,"Unexpected blocked boot pc %x slot %x state %u\n",pc,slot,nptRead(0x8000));return 1;}
    blocked=1;break;
   }
   if(result.dispatched){dispatched++;waits+=pc==0x586e;fadeSteps+=pc==0x50e4;}
   if(sceneCapacity&&result.dispatched&&!slot&&pc==0x1ea2&&!nptByte(0xf891)&&nptRead(0x8000)==2&&!warning){completed=1;break;}
   if(warning==1&&nptRead(0x8000)==8&&!nptByte(0x82a6)){completed=1;break;}
   if((warning==2||warning==3)&&!nptByte(0)){completed=1;break;}
   if(warning>=4&&runtime.resume[1]==0x810e&&nptRead(0x8000)==(warning==5?4:2)){completed=1;break;}
  }
  if(blocked||completed)break;
  memcpy(expected,ram,sizeof expected);willowArcadeTaskWake(&bus);BFP_SCATTER(expected);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||memcmp(expected,ram,sizeof expected)||
    (sram&&memcmp(sram,expectedSram,sizeof expectedSram)))return 1;
  wakes++;
 }
 if(!waits||!fadeSteps||((!warning||warning==5)&&nptByte(0xf890))){fprintf(stderr,"Boot terminal waits %u fades %u consumer %u state %u\n",waits,fadeSteps,nptByte(0xf890),nptRead(0x8000));return 1;}
 if(sceneCapacity){
  if(!completed||blocked||nptRead(0x020c)!=((warning&&warning!=5)?7:8)||bfpFills!=(warning==5?20:warning==4?15:warning>=2?10:5)||(!warning&&(nptRead(0x829c)||nptRead(0x829e)))){fprintf(stderr,"Boot terminal complete %u blocked %u free %u fills %u state %u text %u\n",completed,blocked,nptRead(0x020c),bfpFills,nptRead(0x8000),bfpTextWrites);return 1;}
  if(!warning){
  /* Continue the same live task after its clear wait. No manager state or
   * completion flag is injected: the next wake executes original option
   * decoding and initializes the previous sprite budget. */
  memcpy(expected,ram,sizeof expected);willowArcadeTaskWake(&bus);BFP_SCATTER(expected);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(sram,expectedSram,sizeof expectedSram))return 1;
  wakes++;
  WillowNativeTaskRun options=willowNativeTaskRunWithStoredBootVideoFill(&runtime,&bus,0,0,bfpWord,
   nptColor,nptColorWrite,0,0,bfpVideoWord,bfpVideoFill);
  BFP_SCATTER(expected);
  unsigned optionArgs[]={0,0};
  if(options.unsupported||!options.dispatched||nptRead(0x8000)!=4||
     nptRead(0x829c)!=0x100||nptByte(0x8293)!=1||
     nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,optionArgs)!=0x424f||
     memcmp(expected,ram,sizeof expected)||memcmp(sram,expectedSram,sizeof expectedSram)){
   fprintf(stderr,"Resident boot option continuation mismatch\n");return 1;
  }
  dispatched++;
  }else{
   if(bfpTextWrites!=460)return 1;
   if(warning==2||warning==3){
    unsigned target=warning==3?0x80:0x20,entry=warning==3?0x45a8c:0x80d0;
    if(nptByte(0)||runtime.resume[0]||nptByte(target)!=12||
       (((unsigned)nptRead(target+4)<<16)|nptRead(target+6))!=entry||
       nptByte(0xf891)||nptByte(0x82a6))return 1;
    for(unsigned at=0x8000;at<0x8014;at+=2)if(nptRead(at))return 1;
   }
   FILE*textFile=fopen(warning==5?"build/scene-attract-fade-scroll1.bin":warning==4?"build/scene-attract-init-scroll1.bin":warning>=2?"build/scene-handoff-scroll1.bin":"build/scene-warning-scroll1.bin","wb");
   if(!textFile||fwrite(sram+0x4000,1,16384,textFile)!=16384)return 1;
   fclose(textFile);
  }
  uint8_t afterBoot[sizeof expected];memcpy(afterBoot,ram,sizeof afterBoot);
  for(unsigned test=0;test<6;test++){
   memcpy(expected,afterBoot,sizeof expected);
   bfpPut(records+0x100,test==1?0x0600:0x0c00);
   bfpPut(records+0x104,test==4?1:0);
   bfpPut(records+0x106,test==0?0x5baa:test==2?0x5138:test==3?0x582c:0x58ec);
   bfpPut(records+0x110,test==0?127:test==3?4:0);
   if(test==2)bfpPut(records+0x112,0x8000); /* Invalid selected-bank parameter. */
   expected[(control+14)^1]=0;
   if(test==5)bfpPut(symbol(symbols,"debugScroll2Map")+8,0);
   memcpy(ram,expected,sizeof expected);
   unsigned args[]={0x100,0};
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
      memcmp(expected,ram,sizeof expected)||memcmp(sram,expectedSram,sizeof expectedSram)){
    fprintf(stderr,"Scene boot capability rejection failed case %u\n",test);return 1;
   }
  }
  if(warning>=4)for(unsigned test=0;test<4;test++){
   memcpy(expected,afterBoot,sizeof expected);
   bfpPut(records+0x20,0x0800);bfpPut(globals+12,test==0?8:0);
   if(test==1)expected[(globals+116)^1]=1; /* credits */
   if(test==2)expected[(globals+117)^1]=1; /* start eligibility */
   if(test==3)expected[(globals+68)^1]|=0x40; /* service input */
   memcpy(ram,expected,sizeof expected);unsigned args[]={0x20,0};
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
      memcmp(expected,ram,sizeof expected)||memcmp(sram,expectedSram,sizeof expectedSram)){
    fprintf(stderr,"Attract capability rejection failed case %u\n",test);return 1;
   }
  }
  memcpy(ram,afterBoot,sizeof afterBoot);
 }else if(!blocked||nptRead(0x020c)!=7)return 1;
 setVideo(video);
 if(warning==5)printf("Resident attract fade: direct source allocation, real completion and fourth five-region clear reach manager state 4\n");
 if(warning==4)printf("Resident attract init: third clear completed, 256 actor bytes cleared and 1792 original palette words installed; manager state 2, continuation 810E; four later/input capabilities reject atomically\n");
 if(warning==2||warning==3)printf("Resident boot handoff: %s task queued, boot slot released, both five-region clears and 460 warning word writes complete without injected flags\n",warning==3?"service 45A8C":"attract 80D0");
 if(warning==1)printf("Resident boot warning clear: manager state %u, %u original text word writes, completion flag released by text task\n",nptRead(0x8000),bfpTextWrites);
 if(sceneCapacity)printf("Native boot/fade/clear: %u dispatches, %u wakes, %u clear waits, %u fade steps, %u palette-word checks; verified retained-region fills, task release and boot wait completion reach option decoding and sprite-budget initialization without injected completion; full SRAM and unrelated main RAM match; six unavailable/invalid tasks reject atomically\n",dispatched,wakes,waits,fadeSteps,colors);
 else
 printf("Native boot/fade prefix: %u dispatches, %u wakes, %u clear waits, %u fade steps, %u full palette-word checks; actual command consumption completes fade, then missing video sink rejects atomically with boot wait still pending\n",dispatched,wakes,waits,fadeSteps,colors);
#undef BFP_SCATTER
 return 0;
}
