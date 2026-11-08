/* Continuous original scheduler under actual headless Genesis NTSC VBlank.
 * Source and candidate use identical Genesis cadence, not the CPS10MHz clock.
 * No host instruction stepping, CPU register writes, or invented frame waits. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../build/native-object-publication/compact-v1/platform-addresses.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],programs[2][0x100000],cps[0x22800],ports[512];
static bool env(unsigned cmd,void *data){
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY||cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){((struct retro_variable*)data)->value=NULL;return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT||cmd==RETRO_ENVIRONMENT_SET_VARIABLES||cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS||cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}
static void audio(int16_t l,int16_t r){(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)p;(void)d;(void)i;(void)id;return 0;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing core symbol %s\n",#n);return 2;}
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} ForestCoreCycles;
static ForestCoreCycles*cpu;static uint8_t*ram;static uint8_t*metadataSRAM;
static const struct {unsigned source,target,size;} regions[]={
 {0x910000,0x200000,0x4000},{0x918000,0x204000,0x4000},
 {0x914000,0x208000,0x4000},{0x900000,0x20c000,0x800},
 {0x908000,0x20d000,0x800},{0x91c000,0x20d800,0x1800},
 {0x922000,0x20c800,0x800}};
static unsigned sourcePC(void);
static unsigned frameNumber;
static void headroomRead(unsigned kind,unsigned at,unsigned width,unsigned value);
static void headroomBeforeWrite(unsigned at,unsigned width,unsigned value);
static void headroomAfterWrite(unsigned at,unsigned width,unsigned value);
static void headroomAccess(unsigned kind,unsigned at,unsigned width,unsigned value);
static void publicationRead(unsigned at,unsigned width);
static void publicationBeforeWork(unsigned at,unsigned width);
static void publicationAfterWork(unsigned at,unsigned width);
static void publicationVideoWrite(unsigned at,unsigned width,unsigned value);
static void publicationLatch(void);
static void publicationInit(void);
static void publicationFinish(void);
static uint8_t sourceRAM[65536],pathSRAM[2][65536],platforms[2][65536];
static uint8_t independentLatch[2048],expectedScatteredLatch[2048],latchSeenWords[1024];
static unsigned latchExpectedWords=1024,compactCaptures,fullFallbackCaptures;
static unsigned latchCaptures,latchWords,latchSelector,latchDifferentNext,captureCycle,latchPublications;
static unsigned captureMinCycles=~0u,captureMaxCycles;
static int latchByte(unsigned at){
 if(at<0x2660||at>=0x3620)return -1;
 unsigned offset=at-0x2660,chunk=offset/128,within=offset%128;
 return chunk<32&&within<64?(int)(chunk*64+within):-1;
}
static void sourceWorkAccess(unsigned at,unsigned width){
 if(!latchCaptures||sourcePC()>=0x100000)return;
 for(unsigned n=0;n<width;n++)if(latchByte((at+n)&65535)>=0){fprintf(stderr,"Original source accesses native latch chunk at%x/%u PC%x frame%u\n",at,width,sourcePC(),frameNumber);abort();}
}
static unsigned workRead8(unsigned at){at&=65535;sourceWorkAccess(at,1);unsigned v=ram[at^1];publicationRead(at,1);headroomRead(0,at,1,v);return v;}
static unsigned workRead16(unsigned at){at&=65535;if(at&1)abort();sourceWorkAccess(at,2);unsigned v=ram[at^1]<<8|ram[(at+1)^1];publicationRead(at,2);headroomRead(0,at,2,v);return v;}

static unsigned owner[65536],savedWaitReturn[65536],accepted[65536],pointerPairs,d0Residues;
static unsigned savedValue[65536],partialPointerBytes;
static uint8_t savedByteIndex[65536],savedRegister[65536];
static unsigned portsRead,portsWritten,videoAccesses;
static const char*outputDirectory="build/native-object-publication/compact-visual-state-v1";
static FILE*outputFile(const char*name,const char*mode){char path[512];if(snprintf(path,sizeof path,"%s/%s",outputDirectory,name)>=(int)sizeof path)return NULL;return fopen(path,mode);}
static unsigned char savedMaps[5][sizeof(((ForestCoreCycles*)0)->map[0])];
static unsigned sourcePC(void){return cpu->pc&0xffffff;}
/* Observation only. These helpers neither execute target instructions nor
 * change target memory/registers/cycles. Times are installed-core bus callback
 * times, not sub-instruction physical pin timing. Never call these through the
 * target bus: raw peeks must not recursively observe their own reads. */
static unsigned rawByte(unsigned at){return ram[(at&65535)^1];}
static unsigned rawWord(unsigned at){return rawByte(at)<<8|rawByte(at+1);}
static unsigned rawLong(unsigned at){return rawWord(at)<<16|rawWord(at+2);}
static uint64_t stamp(void){return (uint64_t)frameNumber*896040+(int64_t)cpu->cycles;}
static FILE *headroomEvents,*headroomWindows,*headroomSites;
static unsigned observedIRQs,mainBegins,mainCommits;
static uint64_t irqStart,waitBegin,waitCommit;
static unsigned windowFrame,windowManager,windowScene,windowMainWaits;
static unsigned postCounts[6],postDispatch[16],postQueueConsumes,postOtherSleeps;
static uint64_t lastPostWrite,lastPostVideo,lastPostPort,lastPostDispatch;
typedef struct {unsigned active,pc,kind,width,firstAt,lastAt,minAt,maxAt;uint64_t count,first,last;} HeadroomSite;
static HeadroomSite siteTable[8192];
static unsigned siteCount;
static void headroomEvent(const char*name,unsigned a,unsigned b){
 fprintf(headroomEvents,"%s,%u,%u,%" PRIu64 ",%d,%x,%x,%x,%x,%u,%u\n",name,frameNumber,observedIRQs,stamp(),cpu->cycles,sourcePC(),rawLong(0x204),a,b,rawWord(0x8004),rawWord(0x8008));
}
static void headroomClose(unsigned nextManager,unsigned nextScene){
 if(!observedIRQs)return;
 fprintf(headroomWindows,"%u,%u,%u,%u,%u,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%u",observedIRQs,windowFrame,frameNumber,windowManager,windowScene,nextManager,nextScene,irqStart,waitBegin,waitCommit,stamp(),windowMainWaits);
 for(unsigned n=0;n<6;n++)fprintf(headroomWindows,",%u",postCounts[n]);
 fprintf(headroomWindows,",%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64,postQueueConsumes,postOtherSleeps,lastPostWrite,lastPostVideo,lastPostPort,lastPostDispatch);
 for(unsigned n=0;n<16;n++)fprintf(headroomWindows,",%u",postDispatch[n]);
 fputc('\n',headroomWindows);
}
static void headroomAccess(unsigned kind,unsigned at,unsigned width,unsigned value){
 (void)value;
 unsigned pc=sourcePC();
 if(!waitCommit||pc>=0x100000)return;
 postCounts[kind]++;
 if(kind==1)lastPostWrite=stamp();
 if(kind==3)lastPostVideo=stamp();
 if(kind==5)lastPostPort=stamp();
 unsigned active=rawWord(0x8004)==2&&rawWord(0x8008)==2;
 unsigned slot=(pc*29+kind*7+width+active*17)&8191;
 while(siteTable[slot].count&&(siteTable[slot].active!=active||siteTable[slot].pc!=pc||siteTable[slot].kind!=kind||siteTable[slot].width!=width))slot=(slot+1)&8191;
 HeadroomSite*s=siteTable+slot;
 if(!s->count){if(++siteCount>=8192)abort();s->active=active;s->pc=pc;s->kind=kind;s->width=width;s->first=stamp();s->firstAt=s->minAt=s->maxAt=at;}
 s->count++;s->last=stamp();s->lastAt=at;if(at<s->minAt)s->minAt=at;if(at>s->maxAt)s->maxAt=at;
}
static void headroomRead(unsigned kind,unsigned at,unsigned width,unsigned value){
 headroomAccess(kind,at,width,value);
}
static void headroomBeforeWrite(unsigned at,unsigned width,unsigned value){
 publicationBeforeWork(at,width);
 unsigned pc=sourcePC();
 if(width==2&&pc==CADENCE_IRQ+2){
  /* Native IRQ's first MOVE SR,-SP occurs after the genuine hardware frame.
   * Validate its exact frame before accepting it as this interval's boundary. */
  unsigned savedSR=rawWord(at+2),savedPC=rawLong(at+4);
  if((value&0x2700)!=0x2600||savedPC>=0x100000){fprintf(stderr,"Unexpected native IRQ stack frame %x/%x at%x\n",value,savedPC,at);abort();}
  unsigned manager=rawWord(0x8004),scene=rawWord(0x8008);
  headroomClose(manager,scene);observedIRQs++;irqStart=stamp();windowFrame=frameNumber;windowManager=manager;windowScene=scene;
  waitBegin=waitCommit=0;windowMainWaits=0;memset(postCounts,0,sizeof postCounts);memset(postDispatch,0,sizeof postDispatch);
  postQueueConsumes=postOtherSleeps=0;lastPostWrite=lastPostVideo=lastPostPort=lastPostDispatch=0;
  headroomEvent("irq_first_native_store",savedPC,savedSR);
 }
 if(width==2&&pc==0x1754&&at==((cpu->dar[15]-2)&65535)&&rawLong(cpu->dar[15])==0x932c){
  if((rawLong(0x204)&0xffffff)!=0xff0060||(cpu->dar[0]&255)!=1){fprintf(stderr,"Unexpected gameplay source wait context\n");abort();}
  mainBegins++;waitBegin=stamp();headroomEvent("main_wait_movem_first_store",cpu->dar[15],rawLong(cpu->dar[15]));
 }
 headroomAccess(1,at,width,value);
}
static void headroomAfterWrite(unsigned at,unsigned width,unsigned value){
 publicationAfterWork(at,width);
 (void)value;unsigned pc=sourcePC();
 if(width==2&&pc==0x1780&&at==((cpu->dar[8]+6)&65535)){
  unsigned task=cpu->dar[8]&65535,usp=rawLong(task+8);
  if(rawLong(task+4)==0x1756&&rawLong(usp+60)==0x932c){
   if(task!=0x60||rawWord(task)!=0x0101||!waitBegin||waitCommit){fprintf(stderr,"Invalid or duplicated committed gameplay sleep\n");abort();}
   mainCommits++;windowMainWaits++;waitCommit=stamp();headroomEvent("main_wait_sleep_committed",usp,rawLong(usp+60));
  }else if(waitCommit){postOtherSleeps++;headroomEvent("post_main_other_sleep",task,rawLong(usp+60));}
 }
 if(waitCommit&&width==2&&pc==0x150a&&at==0x206){
  unsigned task=rawLong(0x204)&0xffffff;if(task<0xff0000||task>0xff01e0||(task&31))abort();
  postDispatch[(task&65535)/32]++;lastPostDispatch=stamp();
  if(task!=0xff00a0)headroomEvent("post_main_other_task_dispatch",task,rawLong(task+4));
 }
 if(waitCommit&&width==2&&pc==0x1688&&at==0x20c)headroomEvent("post_main_dynamic_task_released",cpu->dar[8]&0xffffff,rawLong(0x208));
 if(waitCommit&&width==2&&pc==0x1dbe&&at==((cpu->dar[8]+(int16_t)cpu->dar[2]+2)&65535)){
  postQueueConsumes++;headroomEvent("post_main_service_queue_consume",at,rawLong(at-2));
 }
}
/* Proposed publication metadata is maintained only from observed source
 * boundaries, never inferred by trusting an arbitrary FF829C/E word. Host
 * diagnostic state is independent of both target continuations. */
typedef struct {unsigned valid,count,epoch,writes,changed;uint8_t dirty[256];} PublicationBank;
static PublicationBank publicationBanks[2];
static uint8_t preparedPrefixes[2][2048];
static struct {unsigned stage,bank,epoch,words,count,oldRemaining,cleanupWords;} producer;
static unsigned nativeAccepted,nativeRejected,nativeLateRejections;
static unsigned metadataMinimumTaskFree=128,metadataTaskStackStores;
static FILE*nativeMetadataFile;
static FILE*visualContexts,*visualIndex,*scrollSitesFile,*scrollReadsFile,*scrollWritesFile;
typedef struct {unsigned pc,width,write,minAt,maxAt,minValue,maxValue,firstFrame,lastFrame;uint64_t count,first,last;} ScrollSite;
static ScrollSite scrollSites[1024];
static unsigned scrollSiteCount;
/* Read-only source-bus observer. It does not execute target instructions or
 * alter cycles, values, scheduling, or native metadata. Callback PC is the
 * core's instruction-progress PC, not necessarily the opcode address. */
static void scrollObserve(unsigned write,unsigned at,unsigned width,unsigned value){
 if(at<0x910000||at+width>0x914000)return;
 unsigned pc=sourcePC(),n;
 for(n=0;n<scrollSiteCount;n++)if(scrollSites[n].pc==pc&&scrollSites[n].width==width&&scrollSites[n].write==write)break;
 if(n==scrollSiteCount){if(n==1024)abort();scrollSiteCount++;scrollSites[n]=(ScrollSite){.pc=pc,.width=width,.write=write,.minAt=at,.maxAt=at,.minValue=value,.maxValue=value,.firstFrame=frameNumber,.first=stamp()};}
 ScrollSite*s=scrollSites+n;s->count++;s->lastFrame=frameNumber;s->last=stamp();
 if(at<s->minAt)s->minAt=at;if(at>s->maxAt)s->maxAt=at;
 if(value<s->minValue)s->minValue=value;if(value>s->maxValue)s->maxValue=value;
 if(!write)fprintf(scrollReadsFile,"%u,%u,%x,%x,%u,%x,%" PRIu64 ",%x\n",frameNumber,latchCaptures,pc,at,width,value,stamp(),rawLong(0x204));
 if(write)fprintf(scrollWritesFile,"%u,%u,%x,%x,%u,%x,%" PRIu64 ",%x,%x,%x,%x,%x,%x\n",frameNumber,latchCaptures,pc,at,width,value,stamp(),rawLong(0x204),cpu->dar[0],cpu->dar[1],cpu->dar[2],cpu->dar[8],cpu->dar[9]);
}
static unsigned producerEpochs,producerCommits,nonproducerWrites,producerRecordWords,producerCleanupWords;
static FILE *publicationEpochFile,*publicationLatchFile,*publicationWriterFile,*publicationBanksFile;
typedef struct {unsigned pc,width,producer,cleanup;uint64_t count,changed;unsigned first,last;} PublicationWriter;
static PublicationWriter publicationWriters[128];static unsigned publicationWriterCount;
static unsigned objectBank(unsigned address){if(address>=0x900000&&address<0x900800)return 0;if(address>=0x908000&&address<0x908800)return 1;return 2;}
static unsigned objectBase(unsigned bank){return bank?0x908000:0x900000;}
static void publicationInit(void){
 visualContexts=outputFile("visual-contexts.bin","wb");visualIndex=outputFile("visual-contexts.csv","w");
 scrollSitesFile=outputFile("scroll1-sites.csv","w");scrollReadsFile=outputFile("scroll1-reads.csv","w");scrollWritesFile=outputFile("scroll1-writes.csv","w");
 if(!visualContexts||!visualIndex||!scrollSitesFile||!scrollReadsFile||!scrollWritesFile)abort();
 fputs("frame,capture,bank,source_epoch,source_count,source_valid,native_epoch,native_count,native_valid,manager,scene,prior_selector,next_selector,master_cycles\n",visualIndex);
 fputs("frame,previous_capture,callback_pc,address,width,value,master_cycles,current_task\n",scrollReadsFile);
 fputs("frame,previous_capture,callback_pc,address,width,value,master_cycles,current_task,d0,d1,d2,a0,a1\n",scrollWritesFile);
 nativeMetadataFile=outputFile("metadata-acquisitions.csv","w");if(!nativeMetadataFile)abort();
 fputs("frame,capture,bank,native_epoch,native_count,native_valid,source_epoch,source_count,source_valid,native_pending,native_pending_offset,native_generation\n",nativeMetadataFile);
 publicationEpochFile=outputFile("producer-epochs.csv","w");publicationLatchFile=outputFile("publication-latches.csv","w");publicationWriterFile=outputFile("object-write-sites.csv","w");
 publicationBanksFile=outputFile("captured-original-banks.bin","wb");
 if(!publicationEpochFile||!publicationLatchFile||!publicationWriterFile||!publicationBanksFile)abort();
 fputs("frame,epoch,bank,emitted_records,source_remaining,prior_remaining,cleanup_word_writes,published_selector,master_cycles\n",publicationEpochFile);
 fputs("frame,capture,selected_bank,producer_epoch,valid_complete_epoch,count,tail_nonempty,dirty_records,word_or_byte_writes,changed_writes,busy_selected,manager,scene,master_cycles\n",publicationLatchFile);
}
static void publicationRead(unsigned at,unsigned width){
 if(sourcePC()==0x3b578&&at==0x82a0&&width==1){
  if(producer.stage!=1){fprintf(stderr,"Cleanup without observed complete producer begin\n");abort();}
  unsigned count=256-(cpu->dar[7]&65535),cursor=cpu->dar[8]&0xffffff;
  if(count>256||producer.words!=count*4||cursor!=objectBase(producer.bank)+count*8){fprintf(stderr,"Source emitted count/cursor/word mismatch frame%u bank%u words%u count%u cursor%x\n",frameNumber,producer.bank,producer.words,count,cursor);abort();}
  producer.count=count;producer.stage=2;
  unsigned sourceCountAt=(!rawByte(0x82a0)&&(cursor&0x8000))?0x829e:0x829c;
  producer.oldRemaining=rawWord(sourceCountAt);
 }
}
static void publicationBeforeWork(unsigned at,unsigned width){
 unsigned sp=cpu->dar[15]&0xffffff,pc=sourcePC();
 if(pc>=METADATABEGIN&&pc<METADATAFAULT+20&&sp>=0xff0200&&sp<=0xff0a10){
  unsigned task=rawLong(0x204)&65535,top=rawLong(task+12)&0xffffff;
  if(task>0x1e0||(task&31)||top<0xff0290||top>0xff0a10||sp<top-128||sp>top){fprintf(stderr,"Metadata native hook escaped original128-byte task stack\n");abort();}
  unsigned physical=0xff0000|at;
  if(physical>=top-128&&physical<top){unsigned free=physical-(top-128);if(free<metadataMinimumTaskFree)metadataMinimumTaskFree=free;metadataTaskStackStores++;}
  else if(physical>=sp-32&&physical<sp){fprintf(stderr,"Metadata stack store below original task allocation\n");abort();}
 }
 if(sourcePC()==METADATABEGINDISPLACED+8&&at==0x829a&&width==2){
  if(producer.stage){fprintf(stderr,"Nested object producer\n");abort();}
  unsigned bank=objectBank(cpu->dar[8]&0xffffff);
  if(bank>1||(cpu->dar[8]&0xffffff)!=objectBase(bank)||(cpu->dar[7]&65535)!=256)abort();
  memset(&producer,0,sizeof producer);producer.stage=1;producer.bank=bank;producer.epoch=++producerEpochs;
  publicationBanks[bank].valid=0;
 }
}
static void publicationAfterWork(unsigned at,unsigned width){
 unsigned pc=sourcePC();
 if((pc==0x3b5a2||pc==0x3b5c4)&&width==2&&(at==0x829c||at==0x829e)){
  if(producer.stage!=2||rawWord(at)!=256-producer.count||(cpu->dar[6]&65535)!=256-producer.count){fprintf(stderr,"Cleanup remaining metadata mismatch\n");abort();}
  unsigned expected=(256-producer.count)>producer.oldRemaining?2*((256-producer.count)-producer.oldRemaining):0;
  if(producer.cleanupWords!=expected){fprintf(stderr,"Cleanup write count mismatch %u/%u\n",producer.cleanupWords,expected);abort();}
  producer.stage=3;
 }
 if((pc==METADATADOUBLEDISPLACED+6&&at==0x8023&&width==1)||(pc==METADATASINGLEDISPLACED+6&&at==0x8022&&width==2)){
  if(producer.stage!=3||rawWord(0x8022)!=(producer.bank?0x9080:0x9000)){fprintf(stderr,"Source next-bank publication lacks complete producer\n");abort();}
  PublicationBank*b=publicationBanks+producer.bank;b->valid=1;b->count=producer.count;b->epoch=producer.epoch;producerCommits++;
  fprintf(publicationEpochFile,"%u,%u,%u,%u,%u,%u,%u,%x,%" PRIu64 "\n",frameNumber,producer.epoch,producer.bank,producer.count,256-producer.count,producer.oldRemaining,producer.cleanupWords,rawWord(0x8022),stamp());
  producer.stage=0;
 }
}
static void publicationVideoWrite(unsigned at,unsigned width,unsigned value){
 unsigned bank=objectBank(at);if(bank>1)return;
 if(objectBank(at+width-1)!=bank)abort();
 PublicationBank*b=publicationBanks+bank;unsigned pc=sourcePC(),offset=at-objectBase(bank),part=0,cleanup=0;
 unsigned old=width==2?willowMotionWord(cps+(at-0x900000)):cps[at-0x900000];
 if(producer.stage&&bank==producer.bank){
  if(producer.stage==1&&pc>=0x3b880&&pc<=0x3bb0c){
   if(width!=2||offset!=producer.words*2){fprintf(stderr,"Nonsequential native-object producer write frame%u PC%x offset%x expected%x\n",frameNumber,pc,offset,producer.words*2);abort();}
   willowMotionPutWord(preparedPrefixes[bank]+offset,value);
   producer.words++;producerRecordWords++;part=1;
  }else if(producer.stage==2&&(pc==0x3b592||pc==0x3b596||pc==0x3b5b4||pc==0x3b5b8)){
   if(width!=2||(value&65535)||offset<producer.count*8||((offset&7)!=0&&(offset&7)!=6))abort();
   producer.cleanupWords++;producerCleanupWords++;part=cleanup=1;
  }
 }
 if(!part){
  if(pc!=0x5916&&pc!=0x5918&&pc!=0x663a&&pc!=0x663c){fprintf(stderr,"Uncovered original object writer PC%x address%x\n",pc,at);abort();}
  nonproducerWrites++;b->valid=0;
 }
 b->writes++;if(old!=(width==2?(value&65535):(value&255))){b->changed++;b->dirty[offset/8]=1;}
 unsigned n;for(n=0;n<publicationWriterCount;n++)if(publicationWriters[n].pc==pc&&publicationWriters[n].width==width&&publicationWriters[n].producer==part&&publicationWriters[n].cleanup==cleanup)break;
 if(n==publicationWriterCount){if(n>=128)abort();publicationWriterCount++;publicationWriters[n]=(PublicationWriter){.pc=pc,.width=width,.producer=part,.cleanup=cleanup,.first=at};}
 PublicationWriter*w=publicationWriters+n;w->last=at;w->count++;w->changed+=old!=(width==2?(value&65535):(value&255));
}
static void publicationLatch(void){
 unsigned bank=latchSelector==0x9080;PublicationBank*b=publicationBanks+bank;
 const uint8_t*vm=metadataSRAM+METADATA_SRAM_OFFSET;
 fprintf(visualIndex,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%04x,%04x,%" PRIu64 "\n",frameNumber,latchCaptures+1,bank,b->epoch,b->count,b->valid,willowMotionLong(vm+24),willowMotionWord(vm+28),willowMotionWord(vm+32),rawWord(0x8004),rawWord(0x8008),latchSelector,rawWord(0x8022),stamp());
 /* Fixed 90,624-byte records: palettes, ports, row-scroll, original RAM,
  * Scroll1. All bytes use source big-endian address order. */
 if(fwrite(cps+0x1c000,1,0x1800,visualContexts)!=0x1800||fwrite(ports,1,512,visualContexts)!=512||fwrite(cps+0x22000,1,2048,visualContexts)!=2048)abort();
 uint8_t snapshotRAM[65536];for(unsigned n=0;n<65536;n++)snapshotRAM[n]=ram[n^1];
 if(fwrite(snapshotRAM,1,65536,visualContexts)!=65536||fwrite(cps+0x10000,1,16384,visualContexts)!=16384)abort();
 unsigned tailNonempty=0,dirty=0,busy=producer.stage&&producer.bank==bank;
 const uint8_t*m=metadataSRAM+METADATA_SRAM_OFFSET;
 if(willowMotionLong(m+44)||willowMotionWord(m+30)!=bank){fprintf(stderr,"Native publication metadata fault or wrong prior-port bank\n");abort();}
 if(willowMotionWord(m+32)){
  if(!b->valid||willowMotionLong(m+24)!=b->epoch||willowMotionWord(m+28)!=b->count){fprintf(stderr,"Native metadata admits unproved source epoch frame%u bank%u valid%u count%u/%u epoch%u/%u\n",frameNumber,bank,b->valid,willowMotionWord(m+28),b->count,willowMotionLong(m+24),b->epoch);abort();}
  nativeAccepted++;
 }else{nativeRejected++;if(b->valid)nativeLateRejections++;}
 if(willowMotionLong(m+36)!=nativeAccepted||willowMotionLong(m+40)!=nativeRejected)abort();
 fprintf(nativeMetadataFile,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",frameNumber,latchCaptures+1,bank,willowMotionLong(m+24),willowMotionWord(m+28),willowMotionWord(m+32),b->epoch,b->count,b->valid,willowMotionWord(m+6),willowMotionWord(m+4),willowMotionLong(m));
 if(b->valid){
  if(busy||b->count>256)abort();
  if(memcmp(preparedPrefixes[bank],independentLatch,b->count*8)){fprintf(stderr,"Producer-prepared prefix differs from independently selected original bank\n");abort();}
  /* Original cleanup only clears X and attribute. Y/code are deliberately
   * retained. This is a semantic tail proof, not a raw-byte zero assumption. */
  for(unsigned n=b->count;n<256;n++)if(willowMotionWord(independentLatch+n*8)||willowMotionWord(independentLatch+n*8+6))tailNonempty++;
  if(tailNonempty){fprintf(stderr,"Complete producer left visible-capable tail frame%u count%u bank%u\n",frameNumber,b->count,bank);abort();}
 }
 if(fwrite(independentLatch,1,2048,publicationBanksFile)!=2048)abort();
 for(unsigned n=0;n<256;n++)dirty+=b->dirty[n]!=0;
 fprintf(publicationLatchFile,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%" PRIu64 "\n",frameNumber,latchCaptures+1,bank,b->epoch,b->valid,b->count,tailNonempty,dirty,b->writes,b->changed,busy,rawWord(0x8004),rawWord(0x8008),stamp());
 memset(b->dirty,0,sizeof b->dirty);b->writes=b->changed=0;
}
static void publicationFinish(void){
 fclose(visualContexts);fclose(visualIndex);fclose(scrollReadsFile);fclose(scrollWritesFile);
 fputs("callback_pc,width,write,count,min_address,max_address,min_value,max_value,first_frame,last_frame,first_master,last_master\n",scrollSitesFile);
 for(unsigned n=0;n<scrollSiteCount;n++){ScrollSite*s=scrollSites+n;fprintf(scrollSitesFile,"%x,%u,%u,%" PRIu64 ",%x,%x,%x,%x,%u,%u,%" PRIu64 ",%" PRIu64 "\n",s->pc,s->width,s->write,s->count,s->minAt,s->maxAt,s->minValue,s->maxValue,s->firstFrame,s->lastFrame,s->first,s->last);}
 fclose(scrollSitesFile);
 fclose(nativeMetadataFile);
 fputs("callback_pc,width,producer,cleanup,count,changed,first_address,last_address\n",publicationWriterFile);
 for(unsigned n=0;n<publicationWriterCount;n++){PublicationWriter*w=publicationWriters+n;fprintf(publicationWriterFile,"%x,%u,%u,%u,%" PRIu64 ",%" PRIu64 ",%x,%x\n",w->pc,w->width,w->producer,w->cleanup,w->count,w->changed,w->first,w->last);}
 fclose(publicationEpochFile);fclose(publicationLatchFile);fclose(publicationWriterFile);fclose(publicationBanksFile);
 printf("Source object publication: %u begin/%u complete epochs, %u emitted word stores, %u cleanup word stores, %u other original object writes. No target metadata/count words were invented or trusted without observing source epochs.\n",producerEpochs,producerCommits,producerRecordWords,producerCleanupWords,nonproducerWrites);
 printf("Target native metadata acquisition: %u admitted complete epochs, %u rejected/fallback, %u conservative extra rejections; raw independent latch retained.\n",nativeAccepted,nativeRejected,nativeLateRejections);
 printf("Native compact capture: %u proved prefixes, %u full256-record fallbacks; omitted destination bytes retain exactly their previous values.\n",compactCaptures,fullFallbackCaptures);
 printf("Native metadata task-stack observation: %u stores; minimum%u bytes above original128-byte stack lower bound.\n",metadataTaskStackStores,metadataMinimumTaskFree);
 if(producer.stage||producerEpochs!=producerCommits)abort();
}
static void portBounds(unsigned at,unsigned width){if(at<0x800000||at+width>0x800200){fprintf(stderr,"Unrepresented port %x/%u PC%x frame%u\n",at,width,sourcePC(),frameNumber);abort();}}
static unsigned portRead8(unsigned at){portBounds(at,1);portsRead++;headroomAccess(4,at,1,ports[at-0x800000]);return ports[at-0x800000];}
static unsigned portRead16(unsigned at){portBounds(at,2);if(at&1)abort();portsRead++;
 if(at==0x800100&&sourcePC()>=CADENCE_LATCH&&sourcePC()<CADENCE_LATCH_END){
  if(latchCaptures&&latchWords!=latchExpectedWords){fprintf(stderr,"Previous native capture incomplete\n");abort();}
  latchSelector=willowMotionWord(ports+0x100);
  if(latchSelector!=0x9000&&latchSelector!=0x9080){fprintf(stderr,"Unsupported prior object port %x\n",latchSelector);abort();}
  memcpy(independentLatch,cps+(latchSelector==0x9000?0:0x8000),2048);
  publicationLatch();
  unsigned compact=willowMotionWord(metadataSRAM+METADATA_SRAM_OFFSET+32);
  latchExpectedWords=compact?willowMotionWord(metadataSRAM+METADATA_SRAM_OFFSET+28)*4:1024;
  if(latchExpectedWords>1024)abort();
  if(!latchCaptures)for(unsigned n=0;n<65536;n++){int off=latchByte(n);if(off>=0)expectedScatteredLatch[off]=ram[n^1];}
  memcpy(expectedScatteredLatch,independentLatch,latchExpectedWords*2);
  if(compact)compactCaptures++;else fullFallbackCaptures++;
  memset(latchSeenWords,0,sizeof latchSeenWords);latchWords=0;latchCaptures++;captureCycle=(unsigned)cpu->cycles;
 }
 headroomAccess(4,at,2,willowMotionWord(ports+(at-0x800000)));return willowMotionWord(ports+(at-0x800000));}
static void portWrite8(unsigned at,unsigned v){portBounds(at,1);portsWritten++;headroomAccess(5,at,1,v);ports[at-0x800000]=v;}
static void portWrite16(unsigned at,unsigned v){portBounds(at,2);if(at&1)abort();portsWritten++;
 if(at==0x800100&&latchCaptures){
  if(sourcePC()!=0x1560){fprintf(stderr,"Unexpected source object publication PC%x\n",sourcePC());abort();}
  if(latchWords!=latchExpectedWords){fprintf(stderr,"Original IRQ publishes next bank before complete capture\n");abort();}
  if(willowMotionWord(metadataSRAM+0xf530)!=latchExpectedWords/4||willowMotionWord(metadataSRAM+0xf532)!=willowMotionWord(metadataSRAM+METADATA_SRAM_OFFSET+32)){fprintf(stderr,"Actual native compact capture descriptor mismatch\n");abort();}
  latchPublications++;if((v&65535)!=latchSelector)latchDifferentNext++;
 }
 headroomAccess(5,at,2,v);willowMotionPutWord(ports+(at-0x800000),v);}
static void videoBounds(unsigned at,unsigned width){for(unsigned n=0;n<7;n++)if(at>=regions[n].source&&at+width<=regions[n].source+regions[n].size){videoAccesses++;return;}fprintf(stderr,"Unrepresented CPS memory%x/%u PC%x frame%u\n",at,width,sourcePC(),frameNumber);abort();}
static unsigned videoRead8(unsigned at){videoBounds(at,1);scrollObserve(0,at,1,cps[at-0x900000]);headroomAccess(2,at,1,cps[at-0x900000]);return cps[at-0x900000];}
static unsigned videoRead16(unsigned at){videoBounds(at,2);if(at&1)abort();scrollObserve(0,at,2,willowMotionWord(cps+(at-0x900000)));headroomAccess(2,at,2,willowMotionWord(cps+(at-0x900000)));return willowMotionWord(cps+(at-0x900000));}
static void videoWrite8(unsigned at,unsigned v){videoBounds(at,1);scrollObserve(1,at,1,v);publicationVideoWrite(at,1,v);headroomAccess(3,at,1,v);cps[at-0x900000]=v;}
static void videoWrite16(unsigned at,unsigned v){videoBounds(at,2);if(at&1)abort();scrollObserve(1,at,2,v);publicationVideoWrite(at,2,v);headroomAccess(3,at,2,v);willowMotionPutWord(cps+(at-0x900000),v);}
static unsigned storeOwner(void){
 unsigned next=sourcePC();if(next<4||next>sizeof rom)return 0;
 const uint8_t*p=next<0x100000?programs[0]:rom;
 /* Native MOVEM predecrement context save, or the exact palette pointer field.
  * Every other RAM store invalidates ownership for its bytes. */
 if(willowMotionWord(p+next-4)==0x48e7)return next-4;
 if((next==0xc46||next==0x1282)&&willowMotionLong(p+next-4)==0x2d490030)return next-4;
 return 0;
}
static void workWrite8(unsigned at,unsigned v){at&=65535;sourceWorkAccess(at,1);headroomBeforeWrite(at,1,v);ram[at^1]=v;owner[at]=0;headroomAfterWrite(at,1,v);}
static void workWrite16(unsigned at,unsigned v){
 at&=65535;if(at&1)abort();
 int latchAt=latchByte(at);
 if(latchAt>=0&&sourcePC()>=CADENCE_LATCH&&sourcePC()<CADENCE_LATCH_END){
  unsigned word=(unsigned)latchAt/2;
  if(!latchCaptures||word>=latchExpectedWords||latchSeenWords[word]||willowMotionWord(independentLatch+latchAt)!=(v&65535)){fprintf(stderr,"Native latch store differs from independent prior-port snapshot at%x frame%u\n",at,frameNumber);abort();}
  latchSeenWords[word]=1;latchWords++;
  if(latchWords==latchExpectedWords){unsigned cycles=(unsigned)cpu->cycles-captureCycle;if(cycles<captureMinCycles)captureMinCycles=cycles;if(cycles>captureMaxCycles)captureMaxCycles=cycles;}
  return; /* Reference game RAM retains source bytes; native capture is checked independently. */
 }
 sourceWorkAccess(at,2);headroomBeforeWrite(at,2,v);ram[at^1]=v>>8;ram[(at+1)^1]=v;headroomAfterWrite(at,2,v);unsigned pc=storeOwner();owner[at]=owner[at+1]=pc;
 savedValue[at]=savedValue[at+1]=0;
 if(pc){
  const uint8_t*p=pc<0x100000?programs[0]:rom;
  unsigned value=0,index=0,reg=255,found=0;
  if(willowMotionWord(p+pc)==0x48e7){
   unsigned mask=willowMotionWord(p+pc+2),cursor=cpu->dar[15];
   /* 68000 predecrement MOVEM saves A7..D0 in encoded mask order. The
    * installed core retains the original SP throughout its individual writes.
    * Validate each actual source word against the exact saved register. */
   for(unsigned bit=0;bit<16;bit++)if(mask&(1u<<bit)){
    cursor-=4;
    if(at==(cursor&65535)||at==((cursor+2)&65535)){
     reg=15-bit;index=at==(cursor&65535)?0:2;value=cpu->dar[reg];found=1;break;
    }
   }
  }else if(pc==0xc42||pc==0x127e){
   unsigned base=(cpu->dar[14]+48)&65535;
   if(at==base||at==((base+2)&65535)){reg=9;index=at==base?0:2;value=cpu->dar[9];found=1;}
  }
  if(!found||(reg!=15&&((value>>(index?0:16))&65535)!=(v&65535))){fprintf(stderr,"Unverified context-store layout PC%x addr%x word%x reg%u value%x index%u\n",pc,at,v,reg,value,index);abort();}
  if(reg!=15)for(unsigned n=0;n<2;n++){savedValue[at+n]=value;savedByteIndex[at+n]=index+n;savedRegister[at+n]=reg;}
 }
 if(pc==0x1750)for(unsigned k=0;k<2;k++){
  unsigned site=(at-k*2)&65535,ret=0;
  for(unsigned n=0;n<4;n++)ret=ret<<8|ram[((site+60+n)&65535)^1];
  savedWaitReturn[site]=ret;
 }
}
static void selectProgram(unsigned path){
 for(unsigned page=0;page<16;page++){uint8_t*b=cpu->map[page].base;if(!b)abort();for(unsigned n=0;n<65536;n++)b[n^1]=programs[path][page*65536+n];}
 uint8_t*platformPage=cpu->map[16].base;if(!platformPage)abort();for(unsigned n=0;n<65536;n++)platformPage[n^1]=platforms[path][n];
 const unsigned pages[]={0x80,0x90,0x91,0x92,0xff};for(unsigned n=0;n<5;n++)memcpy(cpu->map+pages[n],savedMaps[n],sizeof savedMaps[n]);
 if(!path){
  cpu->map[0x80].base=ports;cpu->map[0x80].read8=portRead8;cpu->map[0x80].read16=portRead16;cpu->map[0x80].write8=portWrite8;cpu->map[0x80].write16=portWrite16;
  for(unsigned p=0x90;p<=0x92;p++){cpu->map[p].base=cps+(p-0x90)*65536;cpu->map[p].read8=videoRead8;cpu->map[p].read16=videoRead16;cpu->map[p].write8=videoWrite8;cpu->map[p].write16=videoWrite16;}
  cpu->map[0xff].read8=workRead8;cpu->map[0xff].read16=workRead16;cpu->map[0xff].write8=workWrite8;cpu->map[0xff].write16=workWrite16;
 }
}
static int samePointer(unsigned a,unsigned b){for(unsigned n=0;n<7;n++)if(a>=regions[n].source&&a<=regions[n].source+regions[n].size&&b==regions[n].target+a-regions[n].source)return 1;return 0;}
static unsigned nativeLong(unsigned at){unsigned value=0;for(unsigned n=0;n<4;n++)value=value<<8|ram[((at+n)&65535)^1];return value;}
static int compareMemory(const uint8_t*sram){
 for(unsigned n=0;n<65536;n++){
  int latchAt=latchByte(n);
  if(latchCaptures&&latchAt>=0){if(ram[n^1]!=expectedScatteredLatch[latchAt]){fprintf(stderr,"Scattered latch mismatch at%x frame%u\n",n,frameNumber);return 1;}continue;}
  if(!(n&1)&&n<=65532){unsigned a=willowMotionLong(sourceRAM+n),b=nativeLong(n);if(a!=b){
   unsigned pc=owner[n];int context=pc&&owner[n+1]==pc&&owner[n+2]==pc&&owner[n+3]==pc;
   if(samePointer(a,b)&&context){accepted[n]=pc;pointerPairs++;n+=3;continue;}
   if(context&&pc==0x1750&&a==0x900001&&b==0x200001){unsigned ret=savedWaitReturn[n];if((ret==0x932c||ret==0x1ea2||ret==0x1ec4||ret==0x810e)&&willowMotionLong(programs[0]+ret-8)==0x303c0001&&willowMotionLong(programs[0]+ret-4)==0x4eb81750){accepted[n]=pc;d0Residues++;n+=3;continue;}}
  }}
  if(sourceRAM[n]!=ram[n^1]){
   /* A later short stack write may replace only half of an old saved pointer.
    * Compare the surviving byte against the exact original register value
    * observed at its last MOVEM/palette store; never reconstruct a pointer
    * from the unrelated low half currently occupying that stack location. */
   unsigned value=savedValue[n],matched=0,shift=(3-savedByteIndex[n])*8;
   if(owner[n]&&((value>>shift)&255)==sourceRAM[n])for(unsigned r=0;r<7;r++)if(value>=regions[r].source&&value<=regions[r].source+regions[r].size){unsigned target=regions[r].target+value-regions[r].source;if(((target>>shift)&255)==ram[n^1]){matched=1;break;}}
   if(matched){accepted[n]=owner[n];partialPointerBytes++;continue;}
   unsigned at=n&~1u;fprintf(stderr,"Cadence RAM mismatch frame%u at%x original%02x native%02x owner%x; long@%x=%x/%x return+60=%x originalSavedReg%u value%x byte%u\n",frameNumber,n,sourceRAM[n],ram[n^1],owner[n],at,willowMotionLong(sourceRAM+at),nativeLong(at),at<=65472?willowMotionLong(sourceRAM+at+60):0,savedRegister[n],savedValue[n],savedByteIndex[n]);return 1;
  }
 }
 for(unsigned r=0;r<7;r++)if(memcmp(cps+(regions[r].source-0x900000),sram+(regions[r].target-0x200000),regions[r].size)){fprintf(stderr,"Cadence video mismatch frame%u region%u\n",frameNumber,r);return 1;}
 if(memcmp(ports,sram+(CADENCE_IO_MIRROR-0x200000),512)){fprintf(stderr,"Cadence ports mismatch frame%u\n",frameNumber);return 1;}
 if(memcmp(pathSRAM[0]+0xf000,sram+0xf000,0x800)){fprintf(stderr,"Cadence native observation mismatch frame%u\n",frameNumber);return 1;}
 return 0;
}
int main(int argc,char**argv){
 if(argc<2||argc>5)return 2;unsigned frames=argc>=3?(unsigned)strtoul(argv[2],NULL,10):32;if(!frames||frames>36000)return 2;
 unsigned route=argc>=4;if(route&&strcmp(argv[3],"--coin-route"))return 2;if(argc==5)outputDirectory=argv[4];
 FILE*f=fopen("build/native-object-publication/compact-v1/program-original.bin","rb");if(!f||fread(programs[0],1,0x100000,f)!=0x100000)return 2;fclose(f);
 f=fopen("build/native-object-publication/compact-v1/program.bin","rb");if(!f||fread(programs[1],1,0x100000,f)!=0x100000)return 2;fclose(f);
 for(unsigned n=0;n<2;n++){
  uint8_t*p=programs[n];if(willowMotionLong(p+0x68)!=0x1554||willowMotionLong(p+0xa0)!=0x16be||willowMotionWord(p+0x15b8)!=0x4e73)return 2;
  willowMotionPutLong(p,0xffff00);willowMotionPutLong(p+4,CADENCE_BOOT);willowMotionPutLong(p+0x78,CADENCE_IRQ);
  memcpy(p+0x100,"SEGA GENESIS    ",16);memcpy(p+0x1f0,"U               ",16);p[0x1b0]='R';p[0x1b1]='A';p[0x1b2]=0xf8;p[0x1b3]=0x20;willowMotionPutLong(p+0x1b4,0x200000);willowMotionPutLong(p+0x1b8,0x20ffff);
 }
 memcpy(rom,programs[0],0x100000);f=fopen("build/native-object-publication/compact-v1/platform-original.bin","rb");if(!f)return 2;size_t platformBytes=fread(rom+0x100010,1,0x10000,f);if(!platformBytes||!feof(f))return 2;fclose(f);
 memcpy(platforms[0]+0x10,rom+0x100010,platformBytes);
 f=fopen("build/native-object-publication/compact-v1/platform.bin","rb");if(!f||fread(platforms[1]+0x10,1,platformBytes,f)!=platformBytes||fgetc(f)!=EOF)return 2;fclose(f);
 char oraclePath[512];if(snprintf(oraclePath,sizeof oraclePath,"%s/oracle.bin",outputDirectory)>=(int)sizeof oraclePath)return 2;f=fopen(oraclePath,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(unsigned,m68k_get_reg,(int));FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));FN(size_t,retro_serialize_size,(void));FN(bool,retro_serialize,(void*,size_t));FN(bool,retro_unserialize,(const void*,size_t));FN(void,retro_get_system_av_info,(struct retro_system_av_info*));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={oraclePath,rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);metadataSRAM=sram;if(!ram||!cpu||!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<65536)return 2;
 if(m68k_get_reg(16)!=CADENCE_BOOT||cpu->pc!=CADENCE_BOOT)return 2;for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
 uint8_t*pal=dlsym(core,"vdp_pal");uint16_t*lines=dlsym(core,"lines_per_frame");if(!pal||!lines||*pal||*lines!=262)return 2;
 struct retro_system_av_info av;retro_get_system_av_info(&av);printf("Reference clock: GenesisNTSC, 53693175masterHz,262lines,896040mastercycles/frame, libretro %.9fHz; not CPS10MHz cadence\n",av.timing.fps);
 const unsigned pages[]={0x80,0x90,0x91,0x92,0xff};for(unsigned n=0;n<5;n++)memcpy(savedMaps[n],cpu->map+pages[n],sizeof savedMaps[n]);
 size_t stateSize=retro_serialize_size();void*states[2]={malloc(stateSize),malloc(stateSize)};if(!stateSize||!states[0]||!states[1]||!retro_serialize(states[0],stateSize))return 2;memcpy(states[1],states[0],stateSize);
 memset(cps,0,sizeof cps);memset(ports,255,sizeof ports);memset(pathSRAM,0,sizeof pathSRAM);for(unsigned n=0;n<2;n++)memset(pathSRAM[n]+(CADENCE_IO_MIRROR-0x200000),255,512);
 f=outputFile("frames.csv","w");if(!f)return 2;fputs("frame,irq_entries,source_clock,manager,scene,pc,expiring_task_waits,gameplay_waits,manager_waits,nested_waits,active_without_gameplay_wait,irqs_with_ready_tasks,attract_waits,active_irqs,invalid_wait_contexts,credits,demo,raw_system,raw_player1,route_tick\n",f);
 FILE*irqFile=outputFile("irqs.csv","w");if(!irqFile)return 2;fputs("frame,irq,hv,saved_sr,previous_source_clock,task0_state,task1_state,manager,scene,interrupted_pc,expiring_wait_mask,ready_mask,gameplay_wait_mask,nested_wait_mask,manager_wait_mask,attract_wait_mask\n",irqFile);
 headroomEvents=outputFile("headroom-events.csv","w");headroomWindows=outputFile("headroom-windows.csv","w");headroomSites=outputFile("post-main-sites.csv","w");if(!headroomEvents||!headroomWindows||!headroomSites)return 2;
 fputs("event,frame,irq,absolute_master_cycles,frame_master_cycles,pc,current_task,arg0,arg1,manager,scene\n",headroomEvents);
 fputs("irq,start_frame,end_frame,start_manager,start_scene,end_manager,end_scene,irq_entry_master,main_begin_master,main_commit_master,next_irq_master,main_waits,post_ram_reads,post_ram_writes,post_video_reads,post_video_writes,post_port_reads,post_port_writes,post_queue_consumes,post_other_sleeps,last_ram_write_master,last_video_write_master,last_port_write_master,last_dispatch_master",headroomWindows);
 for(unsigned n=0;n<16;n++)fprintf(headroomWindows,",post_dispatch_%02x",n*32);fputc('\n',headroomWindows);
 publicationInit();
 unsigned lastRecordedIRQ=0,routeTicks=0,activeFrames=0,startFrame=~0u;
 for(frameNumber=0;frameNumber<frames;frameNumber++){
  unsigned rawSystem=255,rawPlayer=255;
  if(route){
   /* Source18D0 reads raw800018 (coin/start) and800001 (P1); source1BFA
    * debounces coin history and8186 consumes the actual start-button edge.
    * Source input masks: coin1=01,start1=10,right=01,button1=10. */
   if(frameNumber>=120&&frameNumber<124)rawSystem&=~1u;
   unsigned titleUSP=willowMotionLong(sourceRAM+0x48)&65535;
   unsigned titleReturn=titleUSP<=65472?willowMotionLong(sourceRAM+titleUSP+60):0;
   if(startFrame==~0u&&frameNumber>=140&&sourceRAM[0x82a2]&&willowMotionWord(sourceRAM+0x8000)==4&&sourceRAM[0x40]==1&&willowMotionLong(sourceRAM+0x44)==0x1756&&titleReturn==0x9162)startFrame=frameNumber;
   if(startFrame!=~0u&&frameNumber>=startFrame&&frameNumber<startFrame+4)rawSystem&=~0x10u;
   if(frameNumber&&willowMotionWord(sourceRAM+0x8004)==2&&willowMotionWord(sourceRAM+0x8008)==2&&!sourceRAM[0x8293]){
    activeFrames++;if(routeTicks<180)rawPlayer&=~(1u|((routeTicks%8)<2?0x10u:0));routeTicks++;
   }
  }
  unsigned originalRegs[21],originalCycles=0;
  for(unsigned path=0;path<2;path++){
   selectProgram(path);if(!retro_unserialize(states[path],stateSize))return 2;selectProgram(path);memcpy(sram,pathSRAM[path],65536);
   /* Raw active-low input only, outside the complete hardware-frame call. */
   const unsigned inputPorts[]={0,1,0x11,0x18,0x19,0x1a,0x1c,0x1e};
   for(unsigned n=0;n<8;n++){ports[inputPorts[n]]=255;sram[CADENCE_IO_MIRROR-0x200000+inputPorts[n]]=255;}
   if(route){ports[0x18]=rawSystem;ports[1]=rawPlayer;ports[0x1e]=0xfa;sram[CADENCE_IO_MIRROR-0x200000+0x18]=rawSystem;sram[CADENCE_IO_MIRROR-0x200000+1]=rawPlayer;sram[CADENCE_IO_MIRROR-0x200000+0x1e]=0xfa;}
   retro_run();
   if(!retro_serialize(states[path],stateSize))return 2;memcpy(pathSRAM[path],sram,65536);
   if(!path){for(unsigned n=0;n<65536;n++)sourceRAM[n]=ram[n^1];for(unsigned n=0;n<21;n++)originalRegs[n]=m68k_get_reg(n);originalCycles=cpu->cycles;}
   else{
    if(compareMemory(sram))return 1;
    for(unsigned n=0;n<21;n++)if(m68k_get_reg(n)!=originalRegs[n]&&!samePointer(originalRegs[n],m68k_get_reg(n))){fprintf(stderr,"Cadence register mismatch frame%u r%u original%x native%x\n",frameNumber,n,originalRegs[n],m68k_get_reg(n));return 1;}
    if((unsigned)cpu->cycles!=originalCycles){fprintf(stderr,"Cadence residual-cycle mismatch frame%u original%u native%d\n",frameNumber,originalCycles,cpu->cycles);return 1;}
   }
  }
  unsigned irq=willowMotionLong(sram+0xf000),manager=willowMotionWord(sourceRAM+0x8004),scene=willowMotionWord(sourceRAM+0x8008);
  if(willowMotionLong(sram+0xf024)||willowMotionLong(sram+0xf030)||willowMotionLong(sram+0xf028)!=latchCaptures||irq-lastRecordedIRQ>32){fprintf(stderr,"Invalid source wait context or overwritten native IRQ observations\n");return 1;}
  fprintf(f,"%u,%u,%u,%u,%u,%x",frameNumber,irq,sourceRAM[0x8018],manager,scene,m68k_get_reg(16));for(unsigned n=1;n<10;n++)fprintf(f,",%u",willowMotionLong(sram+0xf000+n*4));fprintf(f,",%u,%u,%02x,%02x,%u\n",sourceRAM[0x82a2],sourceRAM[0x8293],rawSystem,rawPlayer,routeTicks);
  for(unsigned n=lastRecordedIRQ+1;n<=irq;n++){
   const uint8_t*p=sram+0xf040+((n-1)&31)*32;if(willowMotionLong(p)!=n)return 1;
   fprintf(irqFile,"%u,%u,%04x,%04x,%u,%u,%u,%u,%u,%x",frameNumber,n,willowMotionWord(p+4),willowMotionWord(p+6),p[8],p[9],p[10],willowMotionWord(p+12),willowMotionWord(p+14),willowMotionLong(p+16));for(unsigned offset=20;offset<32;offset+=2)fprintf(irqFile,",%04x",willowMotionWord(p+offset));fputc('\n',irqFile);
  }
  lastRecordedIRQ=irq;
  if(!route||!(frameNumber%32))printf("Cadence frame%u IRQ%u sourceClock%u state%u/%u PC%x mainWait%u gameplay%u activeBusy%u credits%u demo%u routeTicks%u\n",frameNumber,irq,sourceRAM[0x8018],manager,scene,m68k_get_reg(16),willowMotionLong(sram+0xf004),willowMotionLong(sram+0xf008),willowMotionLong(sram+0xf014),sourceRAM[0x82a2],sourceRAM[0x8293],routeTicks);fflush(stdout);
 }
 fclose(f);fclose(irqFile);f=outputFile("pointer-sites.csv","w");if(!f)return 2;fputs("address,source_store_pc\n",f);for(unsigned n=0;n<65536;n++)if(accepted[n])fprintf(f,"%04x,%x\n",n,accepted[n]);fclose(f);
 f=outputFile("irq-ring.bin","wb");if(!f||fwrite(sram+0xf000,1,0x800,f)!=0x800)return 2;fclose(f);
 printf("Continuous source cadence: %u paired physicalGenesis frames; full sourceRAM/sevenvideo/ports/registers/nativeIRQobservations and residualcycles match. No host CPU register writes or source wait/scheduler replacements. Proven pointer pairs%u, D0 residues%u, surviving saved-pointer bytes%u; source port reads/writes%u/%u and video accesses%u. Raw scattered object latch included; no renderer/audio or CPSspeed equivalence.\n",frames,pointerPairs,d0Residues,partialPointerBytes,portsRead,portsWritten,videoAccesses);
 if(route){
  printf("Coin/start route: startFrame%u, %u after-start active hardware frames, %u controlled-route ticks; source credits/input history/gameplay own all transitions. Primary manager%u, pending command%u\n",startFrame,activeFrames,routeTicks,willowMotionWord(sourceRAM+0x8000),willowMotionWord(sourceRAM+0x82c0));
  for(unsigned n=0;n<16;n++){unsigned at=n*32,usp=willowMotionLong(sourceRAM+at+8)&65535;printf("Task%02x state%u sleep%u pc%x usp%x return%x\n",at,sourceRAM[at],sourceRAM[at+1],willowMotionLong(sourceRAM+at+4),usp,usp<=65472?willowMotionLong(sourceRAM+usp+60):0);}
 }
 printf("Source object publications checked: %u\n",latchPublications);
 printf("Independent scattered latch: %u captures, %u prior-port banks differing from next source publication; exact native prefix or full fallback per capture, all32chunks including unchanged tail checked, all source accesses to chunks after firstIRQ rejected. Native port-read through final-copy-write mastercycles min%u/max%u.\n",latchCaptures,latchDifferentNext,captureMinCycles,captureMaxCycles);
 if(latchPublications!=latchCaptures){fprintf(stderr,"Object publication count differs from capture count\n");return 1;}
 if(route&&(startFrame==~0u||activeFrames<180||sourceRAM[0x8293])){fprintf(stderr,"Route did not reach180 non-demo active frames\n");return 1;}
 if(observedIRQs!=latchCaptures||mainBegins!=mainCommits){fprintf(stderr,"Read-only observation event counts do not match native observations\n");return 1;}
 fputs("active_at_access,pc,kind,width,count,first_address,last_address,min_address,max_address,first_absolute_master,last_absolute_master\n",headroomSites);
 for(unsigned n=0;n<8192;n++){HeadroomSite*s=siteTable+n;if(s->count)fprintf(headroomSites,"%u,%x,%u,%u,%" PRIu64 ",%x,%x,%x,%x,%" PRIu64 ",%" PRIu64 "\n",s->active,s->pc,s->kind,s->width,s->count,s->firstAt,s->lastAt,s->minAt,s->maxAt,s->first,s->last);}
 fclose(headroomEvents);fclose(headroomWindows);fclose(headroomSites);
 printf("Read-only headroom observations: %u IRQ entries, %u main waits/commits. Final partial IRQ interval is not completed or included in windows.csv.\n",observedIRQs,mainCommits);
 publicationFinish();
 free(states[0]);free(states[1]);return 0;
}
