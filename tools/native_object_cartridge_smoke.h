static unsigned char*objectTaskVideoBytes;
static unsigned objectTaskVideoWrites;
static void objectTaskVideoWord(uint32_t at,uint16_t value)
{
 unsigned offset;
 if(at>=0x900000&&at<0x900800)offset=at-0x900000;
 else if(at>=0x908000&&at<0x908800)offset=2048+at-0x908000;
 else{fprintf(stderr,"Unexpected object task video address %x\n",at);abort();}
 objectTaskVideoBytes[offset]=value>>8;objectTaskVideoBytes[offset+1]=value;
 objectTaskVideoWrites++;
}
static void objectTaskScatter(unsigned char*dst,const WillowNativeTaskRuntime*runtime,
 unsigned control,unsigned records,unsigned globals)
{
 const unsigned from[]={0,0x0200,0x0a10},to[]={records,control,control+16},sizes[]={512,16,32};
 for(unsigned r=0;r<3;r++)for(unsigned n=0;n<sizes[r];n++)dst[(to[r]+n)^1]=nptSource[from[r]+n];
 for(unsigned n=0;n<16;n++){dst[(control+52+n*2)^1]=runtime->resume[n]>>8;dst[(control+53+n*2)^1]=runtime->resume[n];}
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)dst[(globals+g->offset+n)^1]=nptSource[g->source+n];
 }
}
/* Guarded cartridge fixture with real source globals and the existing renderer
 * resident. Does not connect the scheduler or publish sprites to Genesis VDP. */
static int nativeObjectCartridgeCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),size_t capacity)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory||capacity!=65536)return 2;
 unsigned char*sram=memory(RETRO_MEMORY_SAVE_RAM);
 if(!sram)return 2;
 unsigned globals=symbol(symbols,"sourceBootGlobals");
 unsigned selector=globals+nativeBootGlobalOffset(0x8022);
 unsigned mode=globals+nativeBootGlobalOffset(0x82a0);
 unsigned budget=globals+nativeBootGlobalOffset(0x829a);
 unsigned char expected[0xf600],expectedSram[65536];unsigned checks=0;
 setVideo(nativeRngVideo);
 memcpy(expected,ram,sizeof expected);
 unsigned args[5]={4,0,0,0,0};
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceObjectCartridgeProbe",5,args)!=1||
    memcmp(expected,ram,sizeof expected))return 1;

#define OBJECT_CHECK(fn,argc,wanted) do{ \
 unsigned got=nativeSATCall(ram,symbols,setReg,getReg,runFrame,fn,argc,args); \
 if(((wanted)!=~0u&&got!=(wanted))||got==~0u||memcmp(expected,ram,sizeof expected)||memcmp(expectedSram,sram,sizeof expectedSram)){ \
  fprintf(stderr,"Resident object storage mismatch case %u function %s result %x/%x\n",checks,fn,got,(unsigned)(wanted));return 1;}checks++; \
}while(0)
 for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)sram[n]=(sample*31+n*17+(n>>8))&255;
  ram[selector^1]=0x90;ram[(selector+1)^1]=(sample&2)?128:0;
  ram[mode^1]=sample&1;
  memcpy(expected,ram,sizeof expected);memcpy(expectedSram,sram,sizeof expectedSram);
  unsigned bank=(sample&1)||(sample&2)?0:1;
  expected[budget^1]=1;expected[(budget+1)^1]=0;
  OBJECT_CHECK("sourceSpriteBankBeginProbe",0,bank?0x9080u:0x9000u);
  for(unsigned word=0;word<4;word++){
   unsigned at=(sample*13%256)*8+word*2,value=sample*97+word;
   args[0]=0;args[1]=0x900000+bank*0x8000+at;args[2]=value;args[3]=args[4]=0;
   expectedSram[bank*2048+at]=value>>8;expectedSram[bank*2048+at+1]=value;
   OBJECT_CHECK("sourceObjectCartridgeProbe",5,1u);
  }
  if(sample&1){expected[selector^1]=0x90;expected[(selector+1)^1]=0;}
  else expected[(selector+1)^1]^=128;
  OBJECT_CHECK("sourceSpriteBankPublishProbe",0,~0u);
  expectedSram[6144]=expected[selector^1];expectedSram[6145]=expected[(selector+1)^1];
  args[0]=3;OBJECT_CHECK("sourceObjectCartridgeProbe",5,1u);
  unsigned selected=(expectedSram[6145]&128)?1:0;
  memcpy(expectedSram+4096,expectedSram+selected*2048,2048);
  args[0]=2;OBJECT_CHECK("sourceObjectCartridgeProbe",5,1u);
  for(unsigned clear=0;clear<2;clear++){
   args[0]=1;args[1]=0x900000+clear*0x8000;args[2]=args[3]=0;args[4]=512;
   memset(expectedSram+clear*2048,0,2048);OBJECT_CHECK("sourceObjectCartridgeProbe",5,1u);
  }
  memset(expectedSram+4096,0,2048);args[0]=2;OBJECT_CHECK("sourceObjectCartridgeProbe",5,1u);
  args[0]=0;args[1]=0x900001;OBJECT_CHECK("sourceObjectCartridgeProbe",5,0u);
  args[0]=1;args[1]=0x9087fe;args[4]=1;OBJECT_CHECK("sourceObjectCartridgeProbe",5,0u);
 }
 unsigned control=symbol(symbols,"sourceTaskState"),records=symbol(symbols,"taskRecords");
 unsigned taskChecks=0,taskRejects=0;
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 for(unsigned sample=0;sample<192;sample++){
  for(unsigned n=0;n<65536;n++)nptSource[n]=(n*31+sample*73)&255;
  WillowNativeTaskRuntime runtime;
  for(unsigned n=0;n<16;n++)runtime.resume[n]=sample*31+n;
  unsigned slot=0x100+(sample%8)*32,state=sample%3==0?12:sample%3==1?4:8;
  nptPut(0x020e,0);nptWrite(0x0208,0xff);nptWrite(0x020a,0x0a14);nptWrite(0x020c,7);
  nptPut(slot,state);nptWrite(slot+4,0);nptWrite(slot+6,sample%6==0?0x58ec:0x582c);nptWrite(slot+16,2);
  if(state!=12)runtime.resume[slot/32]=0x58ec;
  objectTaskScatter(ram,&runtime,control,records,globals);
  for(unsigned n=0;n<65536;n++)sram[n]=(sample*41+n*19+7)&255;
  sram[6144]=0x90;sram[6145]=sample&128;
  memcpy(expected,ram,sizeof expected);memcpy(expectedSram,sram,sizeof expectedSram);
  objectTaskVideoBytes=expectedSram;objectTaskVideoWrites=0;
  WillowNativeTaskRun result=willowNativeTaskRunOneWithVideoFill(&runtime,&bus,slot,0,0,0,0,0,0,objectTaskVideoWord,0);
  if(result.unsupported||!result.dispatched||objectTaskVideoWrites!=2048||nptRead(0x020c)!=8||nptByte(slot))return 1;
  objectTaskScatter(expected,&runtime,control,records,globals);
  args[0]=slot;args[1]=0;OBJECT_CHECK("sourceObjectTaskProbe",2,0x424fu);taskChecks++;
  /* A subsequent hardware latch, not command completion, clears display. */
  memset(expectedSram+4096,0,2048);args[0]=2;
  OBJECT_CHECK("sourceObjectCartridgeProbe",5,1u);
 }
 /* A partial object bus must not claim to support full scene/text commands. */
 const unsigned entries[][3]={{12,0x582c,0},{12,0x582c,0x10},{12,0x5baa,2},{6,0x58ec,2},{4,0x586e,2}};
 for(unsigned n=0;n<sizeof entries/sizeof *entries;n++){
  unsigned slot=0x100;WillowNativeTaskRuntime runtime={0};
  nptPut(0x020e,0);nptPut(slot,entries[n][0]);nptWrite(slot+4,0);
  nptWrite(slot+6,entries[n][1]);nptWrite(slot+16,entries[n][2]);runtime.resume[8]=entries[n][1];
  objectTaskScatter(ram,&runtime,control,records,globals);
  memcpy(expected,ram,sizeof expected);memcpy(expectedSram,sram,sizeof expectedSram);
  args[0]=slot;args[1]=0;OBJECT_CHECK("sourceObjectTaskProbe",2,1u);taskRejects++;
 }
 ram[(control+14)^1]=255;memcpy(expected,ram,sizeof expected);
 args[0]=0x100;args[1]=0;OBJECT_CHECK("sourceObjectTaskProbe",2,2u);
 ram[(control+14)^1]=0;ram[(records+0x100)^1]=0;memcpy(expected,ram,sizeof expected);
 OBJECT_CHECK("sourceObjectTaskProbe",2,2u);
 printf("Resident object-clear tasks: %u fresh/resumed dispatches clear both banks, reset counts, finish/release original task and preserve displayed copy until explicit latch; %u unsupported commands reject atomically; inhibited/empty scans preserve all storage\n",taskChecks,taskRejects);
#undef OBJECT_CHECK
 setVideo(video);
 printf("Resident cartridge object storage: %u source-bus/bank/IRQ-port/latch/clear operations; complete SRAM and unrelated main RAM preserved, current renderer remains resident\n",checks);
 return 0;
}
