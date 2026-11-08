/* Terminal integration check: independent full-word palette model versus the
 * compiled scheduler using compact ownership. No presentation claim. */
#include "../src/willow_native_task_runtime.h"
#include "../src/willow_arcade_video_publish.h"
#include "../src/willow_cps_video_control.h"
static unsigned char nptSource[65536],nptHardware[256];
static uint8_t nptInput(uint16_t at){return nptHardware[at];}
static void nptHardwareByte(uint16_t at,uint8_t v){nptHardware[at&255]=v;}
static void nptHardwareWord(uint16_t at,uint16_t v){willowMotionPutWord(nptHardware+(at&255),v);}
static uint16_t nptRead(uint16_t at){return willowMotionWord(nptSource+at);}
static uint8_t nptByte(uint16_t at){return nptSource[at];}
static void nptWrite(uint16_t at,uint16_t value){willowMotionPutWord(nptSource+at,value);}
static void nptPut(uint16_t at,uint8_t value){nptSource[at]=value;}
static uint16_t nptColor(uint32_t at){return nptRead(0x2000+at-0x91c000);}
static void nptColorWrite(uint32_t at,uint16_t value){nptWrite(0x2000+at-0x91c000,value);}
static uint16_t nptNoROM(uint32_t at){fprintf(stderr,"Unexpected fade ROM read %x\n",at);abort();}
/* Independent arithmetic form of the established art conversion. */
static uint16_t nptVDPColor(uint16_t word)
{
 unsigned brightness=15+2*(word>>12),out=0;
 for(unsigned n=0;n<3;n++){
  unsigned rgb=((word>>(8-n*4))&15)*17*brightness/45;
  unsigned level=(rgb+16)>>5;if(level>7)level=7;
  out|=level<<(1+n*4);
 }
 return out;
}
static int nativePaletteTaskCheck(unsigned char*ram,const char*symbols,
 void(*setReg)(int,unsigned),unsigned(*getReg)(int),void(*runFrame)(void))
{
 unsigned control=symbol(symbols,"sourceTaskState"),records=symbol(symbols,"taskRecords");
 unsigned globals=symbol(symbols,"sourceBootGlobals"),owner=symbol(symbols,"debugPaletteOwner");
 const unsigned from[]={0,0x200,0xa10},to[]={records,control,control+16},sizes[]={512,16,32};
 unsigned char expected[0xf600],baseColors[6144];
 unsigned publish=profileSymbol(symbols,"sourceVideoPublishProbe")!=0x10000;
 unsigned commands=symbol(symbols,"taskCommands"),publications=0;
 for(unsigned n=0;n<256;n++)nptHardware[n]=ram[(commands+n)^1];
 memset(nptSource,0,sizeof nptSource);
 for(unsigned r=0;r<3;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[from[r]+n]=ram[(to[r]+n)^1];
 for(unsigned i=0;i<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];i++){
  const WillowNativeBootGlobalRange*g=&willowNativeBootGlobalRanges[i];
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 FILE*f=fopen("build/boot-palette-original-words.bin","rb");if(!f||fread(nptSource+0x2000,1,6144,f)!=6144)return 1;fclose(f);
 f=fopen("build/reference_maincpu.bin","rb");if(!f)return 1;
 for(unsigned g=0;g<192;g++){
  unsigned tag=(ram[(owner+g*2)^1]<<8)|ram[(owner+g*2+1)^1];
  if(fseek(f,0xe0000+tag*2,SEEK_SET)||fread(baseColors+g*32,1,32,f)!=32)return 1;
 }
 fclose(f);
 WillowNativeTaskRuntime runtime;
 for(unsigned n=0;n<16;n++)runtime.resume[n]=(ram[(control+52+n*2)^1]<<8)|ram[(control+53+n*2)^1];
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 /* Fade fixture begins from the independently verified reset layer/video
  * mirrors, with the exact boot palette already initialized by the caller. */
 if(publish){nptWrite(0x804a,0x12f8);nptWrite(0x8044,0x003e);}
 nptPut(0x20e,0);nptWrite(0x82c0,0);nptWrite(0xa3e,0);nptWrite(0xa40,0);
 for(unsigned n=0;n<24;n++)nptPut(0x82a8+n,0);
#define NPT_SCATTER(dst) do { \
 for(unsigned r=0;r<3;r++)for(unsigned n=0;n<sizes[r];n++)(dst)[(to[r]+n)^1]=nptSource[from[r]+n]; \
 for(unsigned i=0;i<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];i++){ \
  const WillowNativeBootGlobalRange*g=&willowNativeBootGlobalRanges[i]; \
  for(unsigned n=0;n<g->count;n++)(dst)[(globals+g->offset+n)^1]=nptSource[g->source+n];} \
 for(unsigned n=0;n<16;n++){(dst)[(control+52+n*2)^1]=runtime.resume[n]>>8;(dst)[(control+53+n*2)^1]=runtime.resume[n];} \
}while(0)
#define NPT_CALL(fn,slot,check) do { \
 ram[0xff80^1]=0x60;ram[0xff81^1]=0xfe; \
 const unsigned char args[]={0,255,255,128,0,0,(slot)>>8,(slot)&255,0,0,0,0}; \
 for(unsigned n=0;n<sizeof args;n++)ram[(0xff00+n)^1]=args[n]; \
 setReg(17,0x2700);setReg(15,0xffff00);setReg(16,fullSymbol(symbols,fn)); \
 for(unsigned slice=0;slice<8&&getReg(16)!=0xffff80;slice++)runFrame(); \
 if(getReg(16)!=0xffff80||((check)&&(getReg(0)&65535)!=(check))||memcmp(expected,ram,sizeof expected)){ \
  fprintf(stderr,"Native fade mismatch in %s PC %x result %x\n",fn,getReg(16),getReg(0)&65535); \
  for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"First byte %04x got %02x want %02x\n",n^1,ram[n],expected[n]);break;}return 1;} \
}while(0)
#define NPT_PUBLISH() do { \
 if(publish){ \
  memcpy(expected,ram,sizeof expected); \
  willowArcadeVideoPublish(&bus,nptInput,nptHardwareByte,nptHardwareWord); \
  NPT_SCATTER(expected); \
  for(unsigned n=0;n<256;n++)expected[(commands+n)^1]=nptHardware[n]; \
  NPT_CALL("sourceVideoPublishProbe",0,0);publications++; \
  NPT_CALL("sourceBackdropProbe",0,0); \
  if((getReg(0)&65535)!=nptVDPColor(nptRead(0x37fe)))return 1; \
  if(willowMotionWord(nptHardware+0x70)!=nptRead(0x804a))return 1; \
 } \
}while(0)
 unsigned dispatches=0,wakes=0,colors=0;
 for(unsigned direction=0;direction<2;direction++){
  unsigned stack=nptRead(0x20a),slot=nptRead(stack+2);
  nptWrite(0x20a,stack+4);nptWrite(0x20c,nptRead(0x20c)-1);
  willowArcadeTaskStart(&bus,slot,0x509c,direction*2,3,1);
  NPT_SCATTER(ram);
  unsigned step;
  for(step=0;step<20;step++){
   memcpy(expected,ram,sizeof expected);
   WillowNativeTaskRun result=willowNativeTaskRunOne(&runtime,&bus,slot,0,nptNoROM,nptColor,nptColorWrite);
   if(result.unsupported||!result.dispatched)return 1;
   NPT_SCATTER(expected);
   memset(expected+owner+384,0,96);
   for(unsigned g=0;g<192;g++){
    unsigned base=willowMotionWord(baseColors+g*32),value=nptRead(0x2000+g*32);
    unsigned offset=((uint16_t)(value-base))>>12;
    expected[(owner+384+g/2)^1]|=offset<<((g&1)?4:0);
    for(unsigned n=0;n<16;n++){
     if((uint16_t)(willowMotionWord(baseColors+g*32+n*2)+(offset<<12))!=nptRead(0x2000+g*32+n*2))return 1;
     colors++;
    }
   }
   NPT_CALL("sourcePaletteTaskDispatch",slot,0x424f);dispatches++;
   if(!nptByte(slot)){
    /* Observe completion at the next publication boundary; no next task is
     * dispatched by this isolated fixture. */
    NPT_PUBLISH();
    if(publish){
     WillowCPSVideoControl v=willowCPSVideoControl(willowMotionWord(nptHardware+0x70),willowMotionWord(nptHardware+0x22));
     if(v.enabled!=(direction?15:1))return 1;
    }
    break;
   }
   for(unsigned tick=0;tick<3;tick++){
    /* Original IRQ 1560 publishes before the 1590 wake scan. */
    NPT_PUBLISH();
    memcpy(expected,ram,sizeof expected);willowArcadeTaskWake(&bus);NPT_SCATTER(expected);
    NPT_CALL("sourceTasksWake",0,0);wakes++;
   }
  }
  if(step==20||runtime.resume[slot/32]||nptRead(0x20c)!=8||nptRead(0x82c0)!=(direction?0:1))return 1;
 }
 unsigned char savedTag[2]={ram[owner^1],ram[(owner+1)^1]};
 const unsigned badTags[]={0xffff,0x9000};
 for(unsigned n=0;n<2;n++){
  ram[owner^1]=badTags[n]>>8;ram[(owner+1)^1]=badTags[n];memcpy(expected,ram,sizeof expected);
  NPT_CALL("sourcePaletteTaskDispatch",0x100,1);
 }
 ram[owner^1]=savedTag[0];ram[(owner+1)^1]=savedTag[1];
 puts("Native global fade preflight: unowned and unavailable ROM groups reject without task/palette mutation");
 printf("Native global fade: %u dispatches, %u original wake scans, %u full-word color comparisons; both directions terminate and release their source task\n",dispatches,wakes,colors);
 if(publish)printf("Native fade/video connection: %u publications (96 before wake scans plus two completion observations); fade-out retains objects, fade-in restores scroll layers\n",publications);
 if(publish){
  unsigned backdropTag=owner+191*2;
  unsigned char saved[2]={ram[backdropTag^1],ram[(backdropTag+1)^1]};
  const unsigned tags[]={0xffff,0x9000};
  for(unsigned n=0;n<2;n++){
   ram[backdropTag^1]=tags[n]>>8;ram[(backdropTag+1)^1]=tags[n];
   memcpy(expected,ram,sizeof expected);NPT_CALL("sourceBackdropProbe",0,0xffff);
  }
  ram[backdropTag^1]=saved[0];ram[(backdropTag+1)^1]=saved[1];
  for(unsigned first=0;first<65536;first+=128){
   memcpy(expected,ram,sizeof expected);
   for(unsigned n=0;n<128;n++){
    unsigned value=nptVDPColor(first+n);
    expected[(commands+n*2)^1]=value>>8;expected[(commands+n*2+1)^1]=value;
   }
   NPT_CALL("sourceCPSColorBatchProbe",first,0);
  }
  puts("Native backdrop: pen BFF matches retained source palette at all 98 fade publications; unowned/missing ROM reject without mutation");
  puts("Compiled CPS conversion: all 65536 words match independent arithmetic through 512 native batches; unrelated non-stack RAM preserved");
 }
#undef NPT_PUBLISH
#undef NPT_CALL
#undef NPT_SCATTER
 return 0;
}
