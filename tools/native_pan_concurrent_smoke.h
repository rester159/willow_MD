static int nativeDemoSequenceCheck(void*,uint8_t*,const char*,void(*)(void),unsigned(*)(int));
static int nativeFixedLookupCheck(void*,uint8_t*,const char*,void(*)(void),unsigned(*)(int));
static int nativeDemoUpdateCheck(void*,uint8_t*,const char*,void(*)(void),unsigned(*)(int));
#include "../native/intro_probe/src/palette_initial_data.h"
#include "../src/willow_arcade_attract_panel_map.h"
/* Compare interleaved cartridge task execution with the original-instruction
 * lifecycle trace. This checks logical scheduling, not VBlank performance. */
static unsigned panScheduleByte(uint8_t*ram,unsigned task,unsigned control,unsigned globals,unsigned at)
{
 if(at<512)return ram[(task+at)^1];
 if(at>=0x200&&at<0x210)return ram[(control+at-0x200)^1];
 for(unsigned n=0;n<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];n++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+n;
  if(at>=g->source&&at<g->source+g->count)return ram[(globals+g->offset+at-g->source)^1];
 }
 fprintf(stderr,"Unmapped schedule byte %x\n",at);abort();
}
static void(*panProfileRun)(void);
static unsigned(*panProfileReg)(int);
static FILE*panProfileLog;
static unsigned panProfileCycle,panProfileState;
static void panProfileFrame(void)
{
 openingTicks++;panProfileRun();
 fprintf(panProfileLog,"%u,%u,%u,%x\n",panProfileCycle,panProfileState,openingTicks,panProfileReg(16));
}
static int nativeCommandPaletteIsolationCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),initial[0xf600],expected[0xf600],saved[65536];
 if(!sram)return 2;
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals"),commands=symbol(symbols,"taskCommands");
 if(word(ram,control+12)!=8||ram[(task+0xa0)^1]!=4)return 2;
 memcpy(initial,ram,sizeof initial);memcpy(saved,sram,sizeof saved);
 /* A source-valid queued fade may be allocated while palette storage is
  * unavailable. Only executing that fade requires palette ownership. */
 sceneCommitPut(ram,symbol(symbols,"debugPaletteOwner"),65535);
 sceneCommitPut(ram,control+48,4);sceneCommitPut(ram,control+50,0);
 sceneCommitPut(ram,commands,0x1000);sceneCommitPut(ram,commands+2,3);
 memcpy(expected,ram,sizeof expected);memset(nptSource,0,sizeof nptSource);
 const unsigned from[]={0,0x200,0xa10,0x801a,0x8072},to[]={task,control,control+16,control+48,commands},sizes[]={512,16,32,4,256};
 for(unsigned r=0;r<5;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[from[r]+n]=ram[(to[r]+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 WillowNativeTaskRuntime runtime;for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 WillowNativeTaskRun result=willowNativeTaskRunOneWithVideo(&runtime,&bus,0xa0,0,bfpWord,0,0,0,0,0);
 if(!result.dispatched||result.unsupported||nptRead(0x20c)!=7)return 1;
 objectTaskScatter(expected,&runtime,control,task,globals);
 for(unsigned r=3;r<5;r++)for(unsigned n=0;n<sizes[r];n++)expected[(to[r]+n)^1]=nptSource[from[r]+n];
 unsigned args[]={0xa0,0};
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 unsigned allocated=0x100;while(allocated<512&&!nptByte(allocated))allocated+=32;
 if(allocated==512)return 1;args[0]=allocated;
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 memcpy(ram,initial,sizeof initial);memcpy(sram,saved,sizeof saved);
 puts("Resident command consumer: queued fade allocation matches full RAM/SRAM with invalid palette ownership; allocated fade rejects atomically at execution, baseline restored");return 0;
}
/* Independent decoding of the cartridge's retained palette representation. */
static int panPaletteSnapshot(uint8_t*ram,const char*symbols,uint8_t*out)
{
 unsigned owner=symbol(symbols,"debugPaletteOwner"),slots=symbol(symbols,"debugPaletteSlots");
 for(unsigned n=0;n<3072;n++){
  unsigned group=n/16,tag=(uint16_t)word(ram,owner+group*2),base;
  if(tag<=0xfff0)base=bfpWord(0xe0000+tag*2+(n%16)*2);
  else{
   unsigned slot=0;while(slot<2&&(uint16_t)word(ram,slots+slot*34)!=group)slot++;
   if(slot==2)return 0;
   base=(uint16_t)word(ram,slots+slot*34+2+(n%16)*2);
  }
  unsigned offset=(ram[(owner+384+group/2)^1]>>((group&1)?4:0))&15;
  willowMotionPutWord(out+n*2,base+(offset<<12));
 }
 return 1;
}
static int panelMapExpected(uint8_t*ram,const char*symbols)
{
 memset(nptSource,0,sizeof nptSource);
 unsigned globals=symbol(symbols,"sourceBootGlobals"),control=symbol(symbols,"sourceTaskState");
 const unsigned src[]={0x8402,0xf042,0x8172,0x801e},dst[]={symbol(symbols,"debugScroll2Record"),symbol(symbols,"debugPaletteControllers"),symbol(symbols,"soundCommands"),control+84},size[]={128,1024,128,4};
 for(unsigned r=0;r<4;r++)for(unsigned n=0;n<size[r];n++)nptSource[src[r]+n]=ram[(dst[r]+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 unsigned map=longword(ram,symbol(symbols,"debugScroll2Map"))&65535;
 for(unsigned n=0;n<4096;n++){
  unsigned id=(uint16_t)word(ram,map+n*2);if(id>=WILLOW_SCROLL2_PAIR_COUNT)return 0;
  willowMotionPutLong(spawnMap+n*4,nativeScroll2Pairs[id]);
 }
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};spawnWrites=0;
 return willowArcadeAttractPanelMap(&bus,nptSource+0x8402,spawnByte,bfpWord,spawnWrite)==WILLOW_ATTRACT_PALETTES&&spawnWrites==1024;
}
static int panelMapCompare(uint8_t*ram,const char*symbols)
{
 unsigned map=longword(ram,symbol(symbols,"debugScroll2Map"))&65535;
 for(unsigned n=0;n<4096;n++){
  unsigned id=(uint16_t)word(ram,map+n*2);
  if(id>=WILLOW_SCROLL2_PAIR_COUNT||nativeScroll2Pairs[id]!=willowMotionLong(spawnMap+n*4))return 0;
 }
 const unsigned src[]={0x8402,0x8172,0x801e},dst[]={symbol(symbols,"debugScroll2Record"),symbol(symbols,"soundCommands"),symbol(symbols,"sourceTaskState")+84},size[]={128,128,4};
 for(unsigned r=0;r<3;r++)for(unsigned n=0;n<size[r];n++)if(nptSource[src[r]+n]!=ram[(dst[r]+n)^1])return 0;
 return 1;
}
static int nativePanConcurrentCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),initial[0xf600],saved[65536];
 if(!sram)return 2;
 unsigned task=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals");
 unsigned initialPhase=(uint16_t)word(ram,globals+16),initialSub=(uint16_t)word(ram,globals+20),initialTimer=(uint16_t)word(ram,globals+22);
 unsigned title=initialPhase==0;
 unsigned demoStart=getenv("WILLOW_DEMO_START_ENDPOINT")!=NULL;
 unsigned demoEndpoint=demoStart||getenv("WILLOW_DEMO_INIT_ENDPOINT")!=NULL;
 const unsigned endpoint=demoStart?4:demoEndpoint?2:getenv("WILLOW_FINAL_PANEL")?0:getenv("WILLOW_PANEL_EXIT")?34:getenv("WILLOW_PANEL_REPLACE")?28:getenv("WILLOW_PANEL_BLINK")?26:getenv("WILLOW_PANEL_SCROLL")?22:16;
 if(!((initialPhase==2&&!initialSub)||(title&&initialSub==2)))return 2;
 const unsigned endpointPhase=demoEndpoint?6:endpoint?4:6;
 FILE*f=fopen(title?"build/opening-title-context-schedule.csv":!endpoint?"build/opening-phase-four-final-schedule.csv":endpoint==34?"build/opening-phase-four-exit-schedule.csv":endpoint==28?"build/opening-phase-four-replace-schedule.csv":endpoint==26?"build/opening-phase-four-blink-schedule.csv":endpoint==22?"build/opening-phase-four-scroll-schedule.csv":"build/opening-phase-four-map-schedule.csv","r");if(!f)return 2;
 FILE*controllerTrace=fopen(title?"build/opening-title-context-controllers.bin":!endpoint?"build/opening-panel-final-controllers.bin":endpoint==34?"build/opening-panel-exit-controllers.bin":endpoint==28?"build/opening-panel-replace-controllers.bin":endpoint==26?"build/opening-panel-blink-controllers.bin":endpoint==22?"build/opening-panel-scroll-controllers.bin":"build/opening-panel-map-controllers.bin","rb");if(!controllerTrace)return 2;
 unsigned controllerAt=symbol(symbols,"debugPaletteControllers");
 FILE*paletteTrace=getenv("WILLOW_AUDIT_COLORS")?fopen("build/opening-title-context-colors.bin","rb"):NULL;
 FILE*scrollTrace=getenv("WILLOW_AUDIT_SCROLL1")?fopen("build/opening-title-context-scroll1.bin","rb"):NULL;
 if(getenv("WILLOW_AUDIT_SCROLL1")&&(!title||!scrollTrace))return 2;
 if(getenv("WILLOW_AUDIT_COLORS")&&(!title||!paletteTrace))return 2;
 if(title&&getenv("WILLOW_MATCH_TITLE_COLD_RANDOM")){
  /* The original-CPU lifecycle fixture starts from memset(expected,0): its
   * pre-title RNG input is zero. The prototype bootstrap used random numbers
   * before this diagnostic began. Match this INPUT before any title dispatch,
   * so the later replay save/restore is compared with identical initial state.
   * Never replace the saved seed or any demo-service output. */
  fprintf(stderr,"Title fixture RNG input: %04x/%02x -> cold-zero original-CPU input\n",
   (uint16_t)word(ram,globals+98),ram[(globals+100)^1]);
  sceneCommitPut(ram,globals+98,0);ram[(globals+100)^1]=0;
 }
 if(title){
  /* Match queue history when the CPU fixture starts after the native boot
   * prefix, just as its palette/fade context is inherited below. */
  uint8_t queueContext[392];unsigned q=0;
  const unsigned queueAddresses[]={control+48,symbol(symbols,"taskCommands"),control+84,symbol(symbols,"soundCommands")};
  const unsigned queueSizes[]={4,256,4,128};
  for(unsigned r=0;r<4;r++)for(unsigned n=0;n<queueSizes[r];n++)queueContext[q++]=ram[(queueAddresses[r]+n)^1];
  FILE*queueInput=fopen("build/title-command-input.bin","wb");if(!queueInput)return 2;
  if(fwrite(queueContext,1,sizeof queueContext,queueInput)!=sizeof queueContext||fclose(queueInput))return 2;
  uint8_t colors[6144];if(!panPaletteSnapshot(ram,symbols,colors))return 2;
  FILE*paletteInput=fopen("build/title-palette-input.bin","wb");if(!paletteInput)return 2;
  if(fwrite(colors,1,sizeof colors,paletteInput)!=sizeof colors)return 2;fclose(paletteInput);
  FILE*context=fopen("build/title-fade-input.bin","wb");if(!context)return 2;
  uint8_t data[30];for(unsigned n=0;n<26;n++)data[n]=panScheduleByte(ram,task,control,globals,0x82a8+n);
  for(unsigned n=0;n<4;n++)data[26+n]=panScheduleByte(ram,task,control,globals,0x802e + n);
  if(fwrite(data,1,sizeof data,context)!=sizeof data)return 2;fclose(context);
  if(getenv("WILLOW_TITLE_FINISH_SCAN")){
   for(unsigned slot=0x40;slot<512;slot+=32){
    unsigned state=ram[(task+slot)^1];if(state!=4&&state!=8&&state!=12)continue;
    unsigned args[]={slot,0};
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f)return 1;
   }
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u)return 1;
  }
 }

 const unsigned auditControllers=getenv("WILLOW_AUDIT_FULL_CONTROLLERS")!=NULL;
 char line[512];unsigned row[16],started=0,cycles=0,dispatches=0,peakSlices=0;
 unsigned scheduleMismatchCycles=0;
 FILE*titleLog=title?fopen("build/title-schedule-differences.csv","w"):NULL;
 if(title&&!titleLog)return 2;
 if(titleLog)fputs("cycle,phase,state,address,native,source\n",titleLog);
 unsigned controllerMismatchCycles=0,controllerMismatchBytes=0,lastControllerMismatch=0;
 FILE*auditLog=auditControllers?fopen("build/controller-differences.csv","w"):NULL;
 if(auditControllers&&!auditLog)return 2;
 if(auditLog)fputs("cycle,phase,state,offset,active,native,source\n",auditLog);
 const unsigned addresses[]={0x8004,0x8008,0x800a,0xe58,0x82c0,0x20c,0x8034,0x8038,0xc0,0xe0,0xd0,0xf0,0xf890,0x802e,0x8030};
 const unsigned bytes[]={0,0,0,1,0,0,0,0,1,1,0,0,1,0,0};
 panProfileLog=fopen("build/pan-cycle-profile.csv","w");if(!panProfileLog){fclose(f);return 2;}
 fputs("cycle,substate,slice,pc\n",panProfileLog);panProfileRun=runFrame;panProfileReg=getReg;
 setVideo(nativeRngVideo);
 if(!title&&nativeCommandPaletteIsolationCheck(core,ram,symbols,runFrame,getReg))return 1;
 memcpy(initial,ram,sizeof initial);memcpy(saved,sram,sizeof saved);
 while(fgets(line,sizeof line,f)){
  if(sscanf(line,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",row,row+1,row+2,row+3,row+4,row+5,row+6,row+7,row+8,row+9,row+10,row+11,row+12,row+13,row+14,row+15)!=16)continue;
  if(!started){
   if(row[1]!=initialPhase||row[2]!=initialSub||row[3]!=initialTimer)continue;
   if(auditLog){
    FILE*input=fopen("build/native-controller-input.bin","wb");if(!input)return 2;
    uint8_t data[1024];for(unsigned n=0;n<1024;n++)data[n]=ram[(controllerAt+n)^1];
    if(fwrite(data,1,1024,input)!=1024)return 2;fclose(input);
    uint8_t baseline[1024];if(fseek(controllerTrace,row[0]*1024L,SEEK_SET)||fread(baseline,1,1024,controllerTrace)!=1024)return 2;
    unsigned differences=0;
    for(unsigned n=0;n<1024;n++)if(ram[(controllerAt+n)^1]!=baseline[n]){
     fprintf(auditLog,"%u,%u,%u,%u,%u,%u,%u\n",row[0],row[1],row[2],n,ram[(controllerAt+(n/64)*64)^1],ram[(controllerAt+n)^1],baseline[n]);differences++;
    }
    fprintf(stderr,"Controller baseline before first native scan: %u differing bytes at reference cycle %u\n",differences,row[0]);
   }
   started=1;continue;
  }
  if(word(ram,globals+16)==4&&(word(ram,globals+20)==32||word(ram,globals+20)==38)&&word(ram,globals+143)){
   uint8_t before[65536],beforeSRAM[65536];memcpy(before,ram,sizeof before);memcpy(beforeSRAM,sram,sizeof beforeSRAM);
   if(nativeOpeningClearCheck(core,ram,symbols,runFrame,getReg))return 1;
   memcpy(ram,before,sizeof before);memcpy(sram,beforeSRAM,sizeof beforeSRAM);setVideo(nativeRngVideo);
  }
  panProfileCycle=cycles;panProfileState=(uint16_t)word(ram,globals+20);openingTicks=0;
  unsigned panelMap=word(ram,globals+16)==4&&word(ram,globals+20)==14&&word(ram,globals+22)==1;
  if(panelMap&&!panelMapExpected(ram,symbols))return 2;
  unsigned result=nativeSATCallBounded(word(ram,globals+16)==6?64:8,NULL,ram,symbols,setReg,getReg,panProfileFrame,"sourcePanCycleProbe",0,NULL);
  if(openingTicks>peakSlices)peakSlices=openingTicks;
  if((result&0xff00)!=0x5300||(result&255)>16){
   fprintf(stderr,"Concurrent native pan cycle %u rejected: %x phase %d state %d\n",cycles,result,word(ram,globals+16),word(ram,globals+20));
   for(unsigned bank=0;bank<6;bank++)for(unsigned n=0;n<(bank<4?32:8);n++){
    unsigned tag=(uint16_t)word(ram,symbol(symbols,"debugPaletteOwner")+(bank*32+n)*2);uint16_t v;
    if(tag<=0xfff0&&!nativePaletteInitialWordRead(0xe0000UL+tag*2,&v))fprintf(stderr,"Missing palette source group %u tag %04x\n",bank*32+n,tag);
   }
   for(unsigned slot=0;slot<512;slot+=32)fprintf(stderr,"slot %03x state %u pc %04x resume %04x params %04x %04x\n",slot,ram[(task+slot)^1],(uint16_t)word(ram,task+slot+6),(uint16_t)word(ram,control+52+slot/16),(uint16_t)word(ram,task+slot+16),(uint16_t)word(ram,task+slot+18));return 1;
  }
  if(panelMap&&!panelMapCompare(ram,symbols)){fputs("Panel map/controller/sound retained memory mismatch\n",stderr);return 1;}
  if(panelMap)puts("Continuous panel map: all 4096 retained pairs, full Scroll2 record and sound ring match raw-source terminal writes");
  dispatches+=result&255;
  for(unsigned n=0;n<15;n++){
   /* The standalone arcade oracle starts without native boot's Scroll1 Y.
    * Compare these coordinates only after 893C establishes both explicitly. */
   if(n>=13&&!(row[1]==4&&row[2]>=10))continue;
   unsigned value=panScheduleByte(ram,task,control,globals,addresses[n]);
   if(!bytes[n])value=(value<<8)|panScheduleByte(ram,task,control,globals,addresses[n]+1);
   if(value!=row[n+1]){
    if(titleLog){fprintf(titleLog,"%u,%u,%u,%x,%u,%u\n",row[0],row[1],row[2],addresses[n],value,row[n+1]);scheduleMismatchCycles++;}
    else{fprintf(stderr,"Concurrent pan cycle %u original cycle %u field %x actual %u expected %u\n",cycles,row[0],addresses[n],value,row[n+1]);return 1;}
   }
  }
  uint8_t controllerData[1024];
  if(fseek(controllerTrace,row[0]*1024L,SEEK_SET)||fread(controllerData,1,1024,controllerTrace)!=1024){fprintf(stderr,"Controller trace read failed row %u, offset %ld\n",row[0],row[0]*1024L);perror("controller trace");return 2;}
  if(paletteTrace){
   uint8_t original[6144],native[6144];
   if(fseek(paletteTrace,row[0]*6144L,SEEK_SET)||fread(original,1,6144,paletteTrace)!=6144||!panPaletteSnapshot(ram,symbols,native))return 2;
   for(unsigned n=0;n<3072;n++)if(willowMotionWord(original+n*2)!=willowMotionWord(native+n*2)){
    fprintf(stderr,"Concurrent color mismatch cycle %u address %x native %04x source %04x\n",row[0],0x91c000+n*2,willowMotionWord(native+n*2),willowMotionWord(original+n*2));return 1;
   }
  }
  if(scrollTrace&&((row[1]==4&&row[2]>=10)||row[1]==6)){
   uint8_t original[16384];
   if(fseek(scrollTrace,row[0]*16384L,SEEK_SET)||fread(original,1,sizeof original,scrollTrace)!=sizeof original)return 2;
   if(memcmp(original,sram+0x4000,sizeof original)){
    fprintf(stderr,"Scroll1 mismatch source cycle %u state %u\n",row[0],row[2]);
    unsigned differences=0;
    for(unsigned n=0;n<sizeof original;n++)if(original[n]!=sram[0x4000+n]&&differences++<32)
     fprintf(stderr,"Scroll1 +%04x native %02x source %02x\n",n,sram[0x4000+n],original[n]);
    return 1;
   }
  }
  unsigned differences=0;
  for(unsigned n=0;auditControllers&&n<1024;n++)if(ram[(controllerAt+n)^1]!=controllerData[n]){
   fprintf(auditLog,"%u,%u,%u,%u,%u,%u,%u\n",row[0],row[1],row[2],n,ram[(controllerAt+(n/64)*64)^1],ram[(controllerAt+n)^1],controllerData[n]);differences++;
  }
  if(differences){controllerMismatchCycles++;controllerMismatchBytes+=differences;lastControllerMismatch=row[0];}
  if(word(ram,globals+16)!=6||!word(ram,globals+20))
   for(unsigned n=0;n<321;n++)if(sram[0x8200+n]){fprintf(stderr,"Cold demo state changed before demo entry at %u\n",n);return 1;}
  cycles++;
  if(row[1]==endpointPhase&&row[2]==endpoint)break;
 }
 if(scrollTrace){fclose(scrollTrace);puts("Scroll1 audit: all 16384 retained bytes match raw-source model after panel setup on every remaining cycle");}
 if(paletteTrace){fclose(paletteTrace);printf("Palette audit: 3072 exact CPS words match after each of %u native cycles\n",cycles);}
 if(titleLog)fclose(titleLog);
 if(auditLog)fclose(auditLog);
 if(scheduleMismatchCycles)fprintf(stderr,"Title-context audit: %u differing field observations; see title-schedule-differences.csv\n",scheduleMismatchCycles);
 fclose(controllerTrace);fclose(f);fclose(panProfileLog);
 if(controllerMismatchCycles){
  fprintf(stderr,"Full controller audit FAILED: %u cycles, %u byte differences, last reference cycle %u; details in build/controller-differences.csv\n",controllerMismatchCycles,controllerMismatchBytes,lastControllerMismatch);return 1;
 }
 if(scheduleMismatchCycles)return 1;
 if(auditControllers)printf("Controller audit: all 1024 bytes match the supplied reference after each of %u native cycles; pre-scan context differences are reported separately\n",cycles);
 if(!started||!cycles||word(ram,globals+16)!=endpointPhase||word(ram,globals+20)!=endpoint)return 1;
 if(demoStart&&getenv("WILLOW_AUDIT_DEMO_ACCESS")&&nativeDemoAccessCheck(core,ram,symbols,runFrame,getReg))return 1;
 if(demoStart){
  uint8_t source[65536];FILE*state=fopen("build/opening-demo-start-0-state.bin","rb");
  if(!state||fread(source,1,sizeof source,state)!=sizeof source||fgetc(state)!=EOF)return 2;
  fclose(state);
  for(unsigned at=0;at<65536;at++){
   uint8_t*p=demoStateByte(ram,sram,globals,at);
   if(p&&*p!=source[at]){fprintf(stderr,"Continuous demo state %04x native %02x source %02x\n",at,*p,source[at]);return 1;}
  }
  const char*names[]={"debugScroll2Record","debugScroll3Record","debugStageObjects","debugPlayerRecord"};
  const unsigned bases[]={0x8402,0x8482,0x8502,0x9846},sizes[]={128,128,3072,128};
  for(unsigned n=0;n<4;n++)for(unsigned k=0,at=symbol(symbols,names[n]);k<sizes[n];k++)
   if(ram[(at+k)^1]!=source[bases[n]+k]){fprintf(stderr,"Continuous demo fixed record %s +%u\n",names[n],k);return 1;}
  const unsigned order[]={4,0,1,5,6,2,7,3};
  const unsigned countAt[]={0x97fe,0x9801,0x9804,0x9807,0x980a,0x980d,0x9810,0x9813,0xf48c};
  const unsigned cursorAt[]={0x9816,0x981c,0x9822,0x9828,0x982e,0x9834,0x983a,0x9840,0xf486};
  const unsigned end[]={0x912a,0x918e,0x91ee,0x9216,0x923e,0x92b6,0x932e,0x9356,0xf466};
  for(unsigned i=0;i<9;i++){
   const WillowActorPoolSpec*spec=willowActorPoolSpecification((i+1)*2);
   unsigned pool=i==8?symbol(symbols,"tileIndex")+4648:symbol(symbols,"debugActorPools")+order[i]*12;
   unsigned records=longword(ram,pool);
   for(unsigned k=0;k<spec->capacity*spec->stride;k++)if(*nativeTargetByte(ram,sram,records+k)!=source[spec->sourceBase+k]){
    fprintf(stderr,"Continuous demo pool %u +%u native %02x source %02x\n",i,k,*nativeTargetByte(ram,sram,records+k),source[spec->sourceBase+k]);return 1;
   }
   if(ram[(pool+10)^1]!=(willowMotionWord(source+cursorAt[i])-(end[i]-spec->capacity*2))/2||ram[(pool+11)^1]!=source[countAt[i]])return 1;
  }
  if(getenv("WILLOW_MATCH_TITLE_COLD_RANDOM")){
   for(unsigned n=0;n<4;n++)if(ram[(globals+8+n)^1]!=source[0xe60+n]){
    fprintf(stderr,"Demo replay saved input mismatch at %04x\n",0xe60+n);return 1;
   }
   puts("Demo replay saved RNG/clock inputs match the original-CPU fixture");
  }
  puts("Continuous demo start: retained demo state, fixed records and all nine actor pool records/cursors/counts match original-instruction endpoint");
 }
 if(demoStart&&getenv("WILLOW_AUDIT_FIXED_LOOKUP")&&nativeFixedLookupCheck(core,ram,symbols,runFrame,getReg))return 1;
 if(demoStart&&getenv("WILLOW_AUDIT_DEMO_SEQUENCE")&&nativeDemoSequenceCheck(core,ram,symbols,runFrame,getReg))return 1;
 if(demoStart&&getenv("WILLOW_AUDIT_DEMO_UPDATE")&&nativeDemoUpdateCheck(core,ram,symbols,runFrame,getReg))return 1;
 /* Test the next unimplemented state, independently of the selected trace endpoint. */
 sceneCommitPut(ram,globals+20,demoEndpoint?(getenv("WILLOW_AUDIT_DEMO_TASK")?10:4):40);
 uint8_t ended[0xf600],endSRAM[65536];memcpy(ended,ram,sizeof ended);memcpy(endSRAM,sram,sizeof endSRAM);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourcePanCycleProbe",0,NULL)!=1||
    memcmp(ended,ram,sizeof ended)||memcmp(endSRAM,sram,sizeof endSRAM))return 1;
 memcpy(ram,initial,sizeof initial);memcpy(sram,saved,sizeof saved);setVideo(video);
 printf("Resident native concurrent opening from phase %u: %u logical scheduler cycles, %u dispatches; 13 camera/message/fade/task fields match every scan/wake; panel X/Y match after 893C initialization; full-controller audit status reported separately; baseline restored for full-state service tests; peak %u emulator slices per cycle, no VBlank timing claim\n",initialPhase,cycles,dispatches,peakSlices);
 return 0;
}
