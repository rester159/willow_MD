#include "native_demo_rom_leaf_smoke.h"
/* Compare the isolated compiled first update with the CPU-verified real-service
 * oracle. State is restored afterward; this does not admit scheduler execution. */
static FILE*demoProfile;
#include "native_cpu_cycles.h"
#include "native_demo_actor_bus_smoke.h"
#include "native_object_word_leaf_smoke.h"
#include "native_demo_finish_smoke.h"
static int nativeFixedLookupCheck(void*core,uint8_t*ram,const char*symbols,void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!memory)return 2;uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM);if(!sram)return 2;
 uint8_t saved[0xf600],savedSRAM[65536];memcpy(saved,ram,sizeof saved);memcpy(savedSRAM,sram,sizeof savedSRAM);
 for(unsigned page=0;page<256;page++){
  unsigned args[]={page};unsigned result=nativeSATCallBounded(getenv("WILLOW_AUDIT_FIXED_WORD_INDEX")?256:64,NULL,ram,symbols,setReg,getReg,runFrame,"sourceFixedLookupProbe",1,args);
  if(result!=1){fprintf(stderr,"Fixed lookup comparison failed: page %02x result %x PC %x SP %x\n",page,result,getReg(16),getReg(15));return 1;}
 }
 if(getenv("WILLOW_AUDIT_FIXED_WORD_INDEX")){
  unsigned query=256,bytes=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceFixedLookupProbe",1,&query);
  if(!bytes||bytes>4096)return 1;
  query=257;if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceFixedLookupProbe",1,&query)!=1)return 1;
  query=258;unsigned inRAM=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceFixedLookupProbe",1,&query);
  if(inRAM>1||(inRAM?(bytes&15):(bytes&1)))return 1;
  printf("Word ownership index: %u %s metadata bytes exact; all 65536 source addresses match legacy two-byte spans or odd-address rejection\n",bytes,inRAM?"RAM":"ROM");
 }
 if(memcmp(saved,ram,sizeof saved)||memcmp(savedSRAM,sram,sizeof savedSRAM))return 1;
 /* Older retained ROMs expose the same entry without newer byte/overlap
  * checks. Only report the common comparison guaranteed by every version. */
 puts("Fixed lookup: 589824 compiled address/span comparisons match legacy owner pointers; all non-stack RAM/SRAM preserved");return 0;
}
static unsigned demoProfileStep,demoProfileSlice;
static void(*demoProfileRun)(void);
static unsigned(*demoProfileReg)(int);
static void demoProfileFrame(void)
{
 demoProfileRun();
 fprintf(demoProfile,"%u,%u,%x\n",demoProfileStep,++demoProfileSlice,demoProfileReg(16));
}
static FILE*nativeDemoOutput(const char*kind,unsigned step,unsigned sequence,unsigned bytes)
{
 char path[256];const char*prefix=sequence?getenv("WILLOW_DEMO_REFERENCE_PREFIX"):NULL;
 int length=prefix?snprintf(path,sizeof path,"%s-trace-%s.bin",prefix,kind):
  snprintf(path,sizeof path,"build/demo-%s-%s-%s.bin",sequence?"sequence":"update",sequence?"trace":"output",kind);
 if(length<0||(unsigned)length>=sizeof path)return NULL;
 FILE*f=fopen(path,"rb");if(f&&fseek(f,(long)step*bytes,SEEK_SET)){fclose(f);return NULL;}return f;
}
static int nativeDemoUpdateStepCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),unsigned step,unsigned sequence)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!memory)return -2;uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM);if(!sram)return -2;
 uint8_t saved[65536],savedSRAM[65536],source[65536],graphics[16384],access[65536];
 FILE*f=nativeDemoOutput("ram",step,sequence,65536);if(!f||fread(source,1,sizeof source,f)!=sizeof source)return -2;fclose(f);
 f=sequence?nativeDemoOutput("access",step,1,65536):fopen("build/demo-update-bus-access.bin","rb");if(!f||fread(access,1,sizeof access,f)!=sizeof access)return -2;fclose(f);
 memcpy(saved,ram,sizeof saved);memcpy(savedSRAM,sram,sizeof savedSRAM);
 if(!step && getenv("WILLOW_AUDIT_ROM_LEAF") && nativeDemoROMLeafCheck(ram,sram,symbols,setReg,getReg,runFrame))return -1;
 if(!step && getenv("WILLOW_AUDIT_OBJECT_WORD_LEAF") && nativeObjectWordLeafCheck(ram,sram,symbols,setReg,getReg,runFrame))return -1;
 if(!step && getenv("WILLOW_AUDIT_DEMO_BYTE_LEAF")){
  uint8_t initial[65536],initialSRAM[65536],before[0xf600],beforeSRAM[65536];
  memcpy(initial,ram,sizeof initial);memcpy(initialSRAM,sram,sizeof initialSRAM);
  const char*names[]={"debugPlayerRecord","debugStageObjects","debugPaletteControllers"};
  const unsigned bases[]={0x9846,0x8502,0xf042},sizes[]={128,3072,1024};
  for(unsigned pass=0;pass<3;pass++){
   memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
   if(pass){
    unsigned shift=pass==1?0:8;
    for(unsigned at=0x98c6;at<0xefc6;at++)sram[at]=(at>>shift)&255;
    for(unsigned owner=0;owner<3;owner++){
     unsigned target=symbol(symbols,names[owner]);
     for(unsigned n=0;n<sizes[owner];n++)ram[(target+n)^1]=((bases[owner]+n)>>shift)&255;
    }
   }
   memcpy(before,ram,sizeof before);memcpy(beforeSRAM,sram,sizeof beforeSRAM);
   for(unsigned page=0;page<256;page++)if(nativeSATCallBounded(256,NULL,ram,symbols,setReg,getReg,runFrame,"sourceDemoByteLeafProbe",1,&page)!=1||memcmp(before,ram,sizeof before)||memcmp(beforeSRAM,sram,sizeof beforeSRAM)){
    fprintf(stderr,"Demo byte leaf mismatch pass %u page %02x\n",pass,page);return -1;
   }
  }
  memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
  printf("Demo byte leaf: all 65536 source addresses match C values and exact overflow changes under original, low-address and high-address payloads (196608 comparisons); unrelated RAM/SRAM preserved\n");
 }
 if(!step && getenv("WILLOW_AUDIT_SRAM_ACTOR_DIRECT") && nativeDemoActorBusCheck(core,ram,sram,symbols,runFrame,getReg))return -1;
 /* Only explicit unsupported entry contexts are claimed to reject atomically;
  * this diagnostic does not preflight every possible actor population. */
 const unsigned invalid[][2]={{0x8004,4},{0x8008,2},{0x8344,1},{0x800a,899}};
 for(unsigned n=0;n<4&&!step;n++){
  unsigned args[]={invalid[n][0],invalid[n][1]};
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"introQueuePutWord",2,args)==~0u)return -1;
  uint8_t rejected[65536];memcpy(rejected,ram,sizeof rejected);
  uint8_t rejectedSRAM[65536];memcpy(rejectedSRAM,sram,sizeof rejectedSRAM);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceDemoUpdateProbe",0,NULL)!=0||
     memcmp(ram,rejected,0xf600)||memcmp(sram,rejectedSRAM,sizeof rejectedSRAM))
   return fprintf(stderr,"Compiled active demo invalid entry %u admitted or mutated state\n",n),-1;
  memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);
 }
 unsigned routed=getenv("WILLOW_AUDIT_DEMO_TASK")!=NULL;
 unsigned concurrent=getenv("WILLOW_AUDIT_DEMO_AUXILIARY")!=NULL;
 if(concurrent&&!routed)return -2;
 unsigned taskRecords=symbol(symbols,"taskRecords"),taskControl=symbol(symbols,"sourceTaskState");
 if(routed&&!concurrent){
  if(getenv("WILLOW_PROFILE_DEMO_CYCLES"))return -2;
  uint8_t wakeRAM[0xf600],wakeSRAM[65536];memcpy(wakeRAM,ram,sizeof wakeRAM);memcpy(wakeSRAM,sram,sizeof wakeSRAM);
  for(unsigned slot=0;slot<512;slot+=32)if(wakeRAM[(taskRecords+slot)^1]==1){
   uint8_t*delay=wakeRAM+((taskRecords+slot+1)^1);--*delay;
   if(!*delay)wakeRAM[(taskRecords+slot)^1]=4;
  }
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||memcmp(wakeRAM,ram,sizeof wakeRAM)||memcmp(wakeSRAM,sram,sizeof wakeSRAM))return fprintf(stderr,"Demo task wake mismatch update %u\n",step+1),-1;
  if(ram[(taskRecords+0x20)^1]!=4||(uint16_t)word(ram,taskControl+54)!=0x810e)return fprintf(stderr,"Demo task not runnable update %u\n",step+1),-1;
  if(!step&&getenv("WILLOW_AUDIT_DEMO_TASK_CASES")){
   uint8_t startRAM[65536],startSRAM[65536],want[0xf600],wantSRAM[65536];
   memcpy(startRAM,ram,sizeof startRAM);memcpy(startSRAM,sram,sizeof startSRAM);
   unsigned globals=symbol(symbols,"sourceBootGlobals"),dispatchArgs[]={0x20,0};
   for(unsigned invalid=0;invalid<3;invalid++){
    memcpy(ram,startRAM,sizeof startRAM);memcpy(sram,startSRAM,sizeof startSRAM);
    if(!invalid)willowMotionPutWord(sram+0x8036,1);
    else sceneCommitPut(ram,globals+nativeBootGlobalOffset(0x800a),invalid==1?0:901);
    memcpy(want,ram,sizeof want);memcpy(wantSRAM,sram,sizeof wantSRAM);
    if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,dispatchArgs)!=1||memcmp(want,ram,sizeof want)||memcmp(wantSRAM,sram,sizeof wantSRAM))return fprintf(stderr,"Demo manager invalid context %u mutated state\n",invalid),-1;
   }
   memcpy(ram,startRAM,sizeof startRAM);memcpy(sram,startSRAM,sizeof startSRAM);
   ram[(globals+nativeBootGlobalOffset(0x828f))^1]=1;
   ram[(globals+nativeBootGlobalOffset(0x8068))^1]|=2;
   memcpy(want,ram,sizeof want);memcpy(wantSRAM,sram,sizeof wantSRAM);
   sceneCommitPut(want,taskRecords+0x20,0x0101);sceneCommitPut(want,taskControl+4,0xff);sceneCommitPut(want,taskControl+6,0x20);
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,dispatchArgs)!=0x424f||memcmp(want,ram,sizeof want)||memcmp(wantSRAM,sram,sizeof wantSRAM))return fprintf(stderr,"Demo manager freeze mutated gameplay state\n"),-1;
   memcpy(ram,startRAM,sizeof startRAM);memcpy(sram,startSRAM,sizeof startSRAM);
   puts("Compiled active demo admission: three invalid stage/timer contexts reject atomically; freeze gate sleeps without gameplay/register mutation; constructed inputs restored");
  }

 }
 unsigned overflow=symbol(symbols,"debugIntroOverflow"),elapsed=0;
 if(nativeCycleMeasure(ram,sram,symbols,setReg,getReg,step?"sourceDemoContinueProbe":"sourceDemoUpdateProbe",step))return -1;
 demoProfileStep=step+1;demoProfileSlice=0;demoProfileRun=runFrame;demoProfileReg=getReg;
 unsigned taskArgs[]={0x20,0};
 unsigned result=nativeSATCallBounded(512,&elapsed,ram,symbols,setReg,getReg,demoProfile?demoProfileFrame:runFrame,concurrent?"sourcePanCycleProbe":routed?"sourceSceneBootTaskDispatch":step?"sourceDemoContinueProbe":"sourceDemoUpdateProbe",routed&&!concurrent?2:0,routed&&!concurrent?taskArgs:NULL);
 if(nativeCycleCompare(ram,sram,step))return -1;
 if((concurrent?(result&0xff00)!=0x5300:result!=(routed?0x424f:1))||(uint16_t)word(ram,overflow)!=(uint16_t)word(saved,overflow)){
  FILE*dump=fopen("build/demo-sequence-failed-native-ram.bin","wb");if(dump){fwrite(ram,1,65536,dump);fclose(dump);}
  dump=fopen("build/demo-sequence-failed-native-sram.bin","wb");if(dump){fwrite(sram,1,65536,dump);fclose(dump);}
  fprintf(stderr,"First missing ROM caller %x\n",willowMotionLong(sram+0x870c));
  fprintf(stderr,"Compiled active demo result %x overflow %u/%u slices %u; service %x added %u; fault %u at %x\n",result,(uint16_t)word(ram,overflow),(uint16_t)word(saved,overflow),elapsed,willowMotionLong(sram+0x8700),willowMotionWord(sram+0x8704),willowMotionWord(sram+0x8706),willowMotionLong(sram+0x8708));return -1;
 }
 /* Optional exit investigation executes every native update, but only audits
  * the final endpoint. The summary must not claim intermediate parity. */
 if(sequence&&getenv("WILLOW_DEMO_AUDIT_LAST_ONLY")&&step+1<900)return (int)elapsed;
 uint8_t registers[6];f=nativeDemoOutput("registers",step,sequence,6);
 if(!f||fread(registers,1,sizeof registers,f)!=sizeof registers)return -2;fclose(f);
 if(memcmp(sram+0x86fa,registers,sizeof registers))return fprintf(stderr,"Compiled active demo D4/A4 mismatch\n"),-1;
 if(getenv("WILLOW_AUDIT_DEMO_LIVE_REGISTERS")){
  uint8_t live[4];f=nativeDemoOutput("live-registers",step,sequence,4);
  if(!f||fread(live,1,sizeof live,f)!=sizeof live)return -2;fclose(f);
  if(memcmp(sram+0x8176,live,sizeof live))return fprintf(stderr,"Compiled demo update %u live D1/D2 mismatch\n",step+1),-1;
 }
 if(routed){
  if(word(ram,taskRecords+0x20)!=(concurrent?0x0400:0x0101)||(uint16_t)word(ram,taskControl+54)!=0x810e||
     (uint16_t)word(ram,taskRecords+0x22)!=willowMotionWord(sram+0x8176)||
     (uint16_t)word(ram,taskRecords+0x24)!=willowMotionWord(sram+0x8178)||
     (uint32_t)longword(ram,taskControl+4)!=(concurrent?willowMotionLong(source+0x204):0xff0020))return fprintf(stderr,"Demo task sleep/context mismatch update %u\n",step+1),-1;
 }
 if(concurrent){
  uint8_t scheduler[34];f=nativeDemoOutput("scheduler",step,sequence,sizeof scheduler);
  if(!f||fread(scheduler,1,sizeof scheduler,f)!=sizeof scheduler)return -2;fclose(f);
  if((result&255)!=willowMotionWord(scheduler))return fprintf(stderr,"Concurrent dispatch count differs at update %u\n",step+1),-1;
  for(unsigned slot=0x20;slot<512;slot+=32){
   if(slot>0x20&&slot<0xa0)continue;
   if(source[slot]&&(uint16_t)word(ram,taskControl+52+slot/16)!=willowMotionWord(scheduler+2+slot/16))
    return fprintf(stderr,"Concurrent continuation %x differs at update %u\n",slot,step+1),-1;
  }
  /* Fixed boot slots are outside the scene-only source fixture. Check the
   * manager and every auxiliary task's semantic record, including sleepers. */
  for(unsigned slot=0x20;slot<512;slot+=32){
   if(slot>0x20&&slot<0xa0)continue;
   for(unsigned k=0;k<32;k++){
    if(k>=2&&k<16)continue;
    /* The diagnostic source caller omits the manager's boot payload.
     * Preserve the actual cartridge payload rather than replacing it. */
    if(slot==0x20&&k>=16){
     if(ram[(taskRecords+slot+k)^1]!=saved[(taskRecords+slot+k)^1])return -1;
     continue;
    }
    if(ram[(taskRecords+slot+k)^1]!=source[slot+k])
     return fprintf(stderr,"Concurrent demo task %x +%u update %u differs\n",slot,k,step+1),-1;
   }
  }
 }
 unsigned globals=symbol(symbols,"sourceBootGlobals");
 for(unsigned at=0;at<65536;at++){
  uint8_t*p=demoStateByte(ram,sram,globals,at);
  if(p&&*p!=source[at]){fprintf(stderr,"Compiled active demo state %04x got %02x source %02x\n",at,*p,source[at]);return -1;}
  if((access[at]&2)&&(at<0x2000||at>=0x8000)&&
     !(concurrent&&at<512&&(at%32)>=2&&(at%32)<16)){
   unsigned args[]={at};unsigned value=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceOpeningBusByte",1,args);
   if(value==~0u||(value&255)!=source[at]||(uint16_t)word(ram,overflow)!=(uint16_t)word(saved,overflow)){fprintf(stderr,"Compiled active demo bus %04x got %02x source %02x\n",at,value,source[at]);return -1;}
  }
 }
 const char*names[]={"debugScroll2Record","debugScroll3Record","debugStageObjects","debugPlayerRecord","debugPaletteControllers"};
 const unsigned bases[]={0x8402,0x8482,0x8502,0x9846,0xf042},sizes[]={128,128,3072,128,1024};
 for(unsigned n=0;n<5;n++)for(unsigned k=0,at=symbol(symbols,names[n]);k<sizes[n];k++)
  if(ram[(at+k)^1]!=source[bases[n]+k]){fprintf(stderr,"Compiled active demo record %s +%u\n",names[n],k);return -1;}
 const unsigned order[]={4,0,1,5,6,2,7,3};
 const unsigned countAt[]={0x97fe,0x9801,0x9804,0x9807,0x980a,0x980d,0x9810,0x9813,0xf48c};
 const unsigned cursorAt[]={0x9816,0x981c,0x9822,0x9828,0x982e,0x9834,0x983a,0x9840,0xf486};
 const unsigned end[]={0x912a,0x918e,0x91ee,0x9216,0x923e,0x92b6,0x932e,0x9356,0xf466};
 for(unsigned i=0;i<9;i++){
  const WillowActorPoolSpec*spec=willowActorPoolSpecification((i+1)*2);
  unsigned pool=i==8?symbol(symbols,"tileIndex")+4648:symbol(symbols,"debugActorPools")+order[i]*12;
  unsigned records=longword(saved,pool);
  unsigned indices=longword(saved,pool+4)&65535;
  if(memcmp(ram+pool,saved+pool,10)||ram[(pool+10)^1]!=(willowMotionWord(source+cursorAt[i])-(end[i]-spec->capacity*2))/2||
     ram[(pool+11)^1]!=source[countAt[i]])return fprintf(stderr,"Compiled active demo pool metadata %u\n",i),-1;
  for(unsigned k=0;k<spec->capacity;k++){
   unsigned handle=willowMotionWord(source+end[i]-spec->capacity*2+k*2);
   if(handle<spec->sourceBase||(handle-spec->sourceBase)%spec->stride||
      ram[(indices+k)^1]!=(handle-spec->sourceBase)/spec->stride)
    return fprintf(stderr,"Compiled active demo pool free index %u/%u\n",i,k),-1;
  }
  for(unsigned k=0;k<spec->capacity*spec->stride;k++)if(*nativeTargetByte(ram,sram,records+k)!=source[spec->sourceBase+k]){
   fprintf(stderr,"Compiled active demo actor class %u +%u got %02x source %02x\n",(i+1)*2,k,*nativeTargetByte(ram,sram,records+k),source[spec->sourceBase+k]);return -1;
  }
 }
 if(memcmp(sram+0x4000,source+0x4000,16384))return fprintf(stderr,"Compiled active demo Scroll1 mismatch\n"),-1;
 uint8_t colors[6144];if(!panPaletteSnapshot(ram,symbols,colors)||memcmp(colors,source+0x2000,sizeof colors))return fprintf(stderr,"Compiled active demo color mismatch\n"),-1;
 f=nativeDemoOutput("sprites",step,sequence,8192);if(!f||fread(graphics,1,8192,f)!=8192)return -2;fclose(f);
 if(memcmp(sram,graphics,2048)||memcmp(sram+2048,graphics+4096,2048))return fprintf(stderr,"Compiled active demo object banks mismatch\n"),-1;
 /* Independent dictionary expansion of every retained map word. */
 for(unsigned layer=0;layer<2;layer++){
  f=nativeDemoOutput(layer?"scroll3":"scroll2",step,sequence,16384);
  if(!f||fread(graphics,1,sizeof graphics,f)!=sizeof graphics)return -2;fclose(f);
  unsigned cells=longword(ram,symbol(symbols,layer?"debugScroll3Cells":"debugScroll2Map"))&65535;
  for(unsigned cell=0;cell<4096;cell++){
   unsigned id;
   if(layer){unsigned bit=cell*11,index=bit>>4,shift=bit&15;
    unsigned a=(uint16_t)word(ram,cells+index*2),b=shift>5?(uint16_t)word(ram,cells+index*2+2):0;
    id=shift<=5?(a>>(5-shift))&2047:((a<<(shift-5))|(b>>(21-shift)))&2047;
   }else id=(uint16_t)word(ram,cells+cell*2);
   if(id>=(layer?WILLOW_SCROLL3_PAIR_COUNT:WILLOW_SCROLL2_PAIR_COUNT))return -1;
   uint32_t pair=layer?nativeScroll3Pairs[id]:nativeScroll2Pairs[id];
   if(pair!=willowMotionLong(graphics+cell*4)){fprintf(stderr,"Compiled active demo map %u cell %u\n",layer,cell);return -1;}
  }
 }
 if(!sequence){memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);}
 if(!sequence)printf("Compiled first active demo: source bus writes, all actor/fixed records, nine pool cursors/counts/free indices, D4/A4, CPS colors, Scroll1, both maps and object banks match; four unsupported entry contexts reject unchanged; %u diagnostic slices, not FPS; original state restored\n",elapsed);
 return sequence?(int)elapsed:0;
}

static int nativeDemoUpdateCheck(void*core,uint8_t*ram,const char*symbols,void(*runFrame)(void),unsigned(*getReg)(int))
{return nativeDemoUpdateStepCheck(core,ram,symbols,runFrame,getReg,0,0);}
static int nativeDemoSequenceCheck(void*core,uint8_t*ram,const char*symbols,void(*runFrame)(void),unsigned(*getReg)(int))
{
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!memory)return 2;uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM);if(!sram)return 2;
 uint8_t saved[65536],savedSRAM[65536];memcpy(saved,ram,sizeof saved);memcpy(savedSRAM,sram,sizeof savedSRAM);
 unsigned peak=0,total=0,updates=120;const char*count=getenv("WILLOW_DEMO_UPDATES");
 if(count){char*end;unsigned long n=strtoul(count,&end,10);if(!*count||*end||!n||n>(getenv("WILLOW_AUDIT_DEMO_POST_SEQUENCE")?1200:900))return 2;updates=n;}
 if(getenv("WILLOW_AUDIT_DEMO_POST_SEQUENCE")&&
    (!getenv("WILLOW_AUDIT_DEMO_AUXILIARY")||updates<=900||getenv("WILLOW_AUDIT_DEMO_FINISH")||getenv("WILLOW_DEMO_AUDIT_LAST_ONLY")))return 2;
 if(getenv("WILLOW_DEMO_AUDIT_LAST_ONLY")&&updates!=900)return 2;
 if(nativeCycleInit(core,getReg))return 2;
 if(getenv("WILLOW_PROFILE_DEMO")){demoProfile=fopen("build/demo-sequence-profile.csv","w");if(!demoProfile)return 2;fputs("update,slice,pc\n",demoProfile);}
 for(unsigned step=0;step<updates;step++){
  int result=nativeDemoUpdateStepCheck(core,ram,symbols,runFrame,getReg,step,1);
  if(result<0){fprintf(stderr,"Compiled demo service sequence failed at update %u\n",step+1);return 1;}
  if((unsigned)result>peak)peak=result;total+=result;
  if(getenv("WILLOW_AUDIT_DEMO_AUXILIARY")&&((step+1)%100==0||step+1==updates)){
   printf("Concurrent demo verified through scan %u/%u\n",step+1,updates);fflush(stdout);
  }
 }
 if(getenv("WILLOW_AUDIT_DEMO_POST_SEQUENCE")){
  unsigned globals=symbol(symbols,"sourceBootGlobals");
  unsigned terminal=getenv("WILLOW_AUDIT_INTERLUDE_SETUP")?6:getenv("WILLOW_AUDIT_INTERLUDE_CLEAR")?4:getenv("WILLOW_AUDIT_INTERLUDE_RESET")?2:0;
  if(word(ram,globals+nativeBootGlobalOffset(0x8004))!=8||
     word(ram,globals+nativeBootGlobalOffset(0x8008))!=terminal||
     word(ram,globals+nativeBootGlobalOffset(0x800a))!=(terminal==6?10:0))return fprintf(stderr,"Compiled post-demo endpoint has wrong phase, state or timer\n"),1;
  printf("Compiled continuous post-demo: %u full scans reach phase 8/%u; original ending-message continuation retained\n",updates,terminal);
 }
 if(demoProfile){fclose(demoProfile);demoProfile=NULL;}
 if(nativeCycleFinish())return 2;
 if(getenv("WILLOW_AUDIT_DEMO_AUXILIARY"))printf("Compiled concurrent demo: %u full scheduler scans/wakes match source state, graphics and saved manager registers\n",updates);
 else if(getenv("WILLOW_AUDIT_DEMO_TASK"))printf("Compiled active demo manager: %u actual slot-20 task dispatches, source wake effects, D1/D2 context and one-tick 810E sleep match; gameplay/source-state checks follow each dispatch; auxiliary tasks remain undispatched\n",updates);
 if(getenv("WILLOW_AUDIT_DEMO_LIVE_REGISTERS"))printf("Compiled demo live registers: D1/D2 match original CPU at all %u endpoints, in addition to D4/A4 and complete state/graphics checks\n",updates);
 if(getenv("WILLOW_AUDIT_DEMO_FINISH_CASES")&&nativeDemoFinishCasesCheck(core,ram,sram,symbols,runFrame,getReg))return 1;
 if(getenv("WILLOW_AUDIT_DEMO_FINISH")){
  if(updates!=900||getenv("WILLOW_DEMO_AUDIT_LAST_ONLY"))return 2;
  if(nativeDemoFinishCheck(core,ram,sram,symbols,runFrame,getReg))return 1;
 }
 memcpy(ram,saved,sizeof saved);memcpy(sram,savedSRAM,sizeof savedSRAM);
 if(getenv("WILLOW_DEMO_AUDIT_LAST_ONLY"))printf("Native demo exit investigation: all 900 services executed without overflow; ONLY endpoint 900 checked against original CPU; %u total/%u peak slices, not FPS; baseline restored\n",total,peak);
 else printf("Compiled demo service sequence: all %u update endpoints match CPU-verified state and graphics; %u total/%u peak diagnostic slices, not FPS; %s; starting state restored\n",updates,total,peak,getenv("WILLOW_AUDIT_DEMO_AUXILIARY")?"full scan/wake verified, interrupt timing excluded":"auxiliary scheduler tasks and interrupts excluded");
 return 0;
}
