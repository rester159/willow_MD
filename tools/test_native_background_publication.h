#ifndef WILLOW_TEST_NATIVE_BACKGROUND_PUBLICATION_H
#define WILLOW_TEST_NATIVE_BACKGROUND_PUBLICATION_H
/* Read-only host observer. Every target bus callback still runs exactly once. */
#define BACKGROUND_PHASE_ADDRESS 0x20f590u
#define BACKGROUND_CONTEXT_PREFIX (0x400u+0x10000u+0x80u+0x80u+0x20u)
typedef struct {unsigned generation,palette,view[4],open;uint64_t begin;} BackgroundSpan;
typedef struct {
 BackgroundSpan prepare,publish;
 unsigned irqDepth,irqFrames[4],irqActive[4],intervalPublishes;
 unsigned prepares,prepared,publishes,activePublishes,rejects,activeRejects,coldSteps,stalePrepares;
 unsigned missingPublications,deadlineFailures,protocolFailures,dataWords,controls,counts[4];
 unsigned firstControl,pending,address,addressLatch,code,increment,lastAddress,lastCode,firstChecked,checkedFrames,cold;
 uint64_t lastWrite,lastRetire,prepareTicks,publishTicks;
 uint8_t beforeVRAM[65536],beforeCRAM[128],beforeVSRAM[128],beforeRegs[32];
} BackgroundObserverPath;
static BackgroundObserverPath backgroundPaths[2];
static int (*backgroundSaveVDP)(uint8_t*);
static uint8_t backgroundVDPContext[0x11000];
static FILE *backgroundEvents,*backgroundPublications,*backgroundSource,*backgroundVRAM,*backgroundVDPState;
static unsigned backgroundSamples;
static void backgroundFail(const char*why){
 backgroundPaths[renderPath].protocolFailures++;
 fprintf(stderr,"BG observer failure path%u frame%u PC%x: %s\n",renderPath,frameNumber,sourcePC(),why);abort();
}
static unsigned backgroundCodePC(unsigned pc){
#if defined(RENDER_BG_CODE_BEGIN) && defined(RENDER_BG_CODE_END)
 return pc>=RENDER_BG_CODE_BEGIN&&pc<RENDER_BG_CODE_END;
#else
 (void)pc;return 0;
#endif
}
/* A hardware PC push at current A7 is impossible for predecrement MOVEM:
 * every register word is below its still-original A7. Only exact, identical
 * native MOVEM opcodes in both frozen programs may extend the existing source
 * case. The shared observer then proves all three hardware-frame words and
 * the following actual IRQ entry; these words receive no pointer ownership. */
static unsigned backgroundInterruptedMovem(unsigned ownerPC,unsigned pc){
 if(ownerPC<0x100000||!renderAllowedInterruptPC(ownerPC)||pc!=ownerPC+4||ownerPC+4>0x110000)return 0;
 unsigned offset=ownerPC-0x100000;
 return willowMotionWord(platforms[0]+offset)==0x48e7&&
        !memcmp(platforms[0]+offset,platforms[1]+offset,4);
}
static unsigned backgroundRTEPC(unsigned pc){
 if(pc==0x15b8||pc==0x15ba)return 1;
#ifdef RENDER_BG_RTE_PC
 if(pc==RENDER_BG_RTE_PC||pc==RENDER_BG_RTE_PC+2)return 1;
#endif
 return 0;
}
static unsigned backgroundSourceByte(unsigned address,unsigned final){
 if(address>=0xff0000)return final?sourceRAM[address&65535]:ram[(address&65535)^1];
 if(address>=0x800000&&address<0x800200)return renderPath&&!final?metadataSRAM[CADENCE_IO_MIRROR-0x200000+(address&511)]:ports[address&511];
 for(unsigned n=0;n<sizeof regions/sizeof *regions;n++)if(address>=regions[n].source&&address<regions[n].source+regions[n].size)
  return renderPath&&!final?metadataSRAM[regions[n].target-0x200000+address-regions[n].source]:cps[address-0x900000];
 backgroundFail("unknown source snapshot address");return 0;
}
static unsigned backgroundSourceWord(unsigned address){return backgroundSourceByte(address,0)<<8|backgroundSourceByte(address+1,0);}
static void backgroundDumpSource(FILE*f,unsigned final){
 static const unsigned starts[]={0x918000,0x914000,0x910000,0x91c000,0x800000,0x922000,0xff0000};
 static const unsigned sizes[]={16384,16384,16384,6144,512,2048,65536};
 for(unsigned r=0;r<7;r++)for(unsigned n=0;n<sizes[r];n++)if(fputc(backgroundSourceByte(starts[r]+n,final),f)==EOF)abort();
}
static BackgroundSpan backgroundDescriptor(unsigned publishing){
 unsigned at=publishing?0xf5a4:0xf594;
 BackgroundSpan s={renderLong(at),renderLong(at+4),{renderWord(at+8),renderWord(at+10),renderWord(at+12),renderWord(at+14)},1,stamp()};return s;
}
static unsigned backgroundSame(const BackgroundSpan*a,const BackgroundSpan*b){
 return a->generation==b->generation&&a->palette==b->palette&&!memcmp(a->view,b->view,sizeof a->view);
}
static void backgroundEvent(unsigned phase,unsigned status,const BackgroundSpan*s){
 BackgroundObserverPath*p=backgroundPaths+renderPath;
 fprintf(backgroundEvents,"%u,%u,%u,%u,%u,%" PRIu64 ",%x,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%" PRIu64 "\n",renderPath,phase,status,frameNumber,renderActive(),stamp(),sourcePC(),s?s->generation:0,s?s->palette:0,s?s->view[0]:0,s?s->view[1]:0,s?s->view[2]:0,s?s->view[3]:0,p->dataWords,p->controls,p->counts[0],p->counts[1],p->counts[2]+p->counts[3],p->lastRetire);
}
/* This pinned core's exported save function is read-only. Check its complete
 * public-memory prefix before using the private command-latch fields. */
static void backgroundContext(unsigned*address,unsigned*code,unsigned*pending,unsigned*latch){
 if(backgroundSaveVDP(backgroundVDPContext)!=66886||
    memcmp(backgroundVDPContext+0x400,renderVRAM,65536)||
    memcmp(backgroundVDPContext+0x10400,renderCRAM,128)||
    memcmp(backgroundVDPContext+0x10480,renderVSRAM,128)||
    memcmp(backgroundVDPContext+0x10500,renderRegs,32))backgroundFail("unrecognized read-only VDP context ABI");
 uint16_t word;memcpy(&word,backgroundVDPContext+BACKGROUND_CONTEXT_PREFIX,2);
 *address=word;memcpy(&word,backgroundVDPContext+BACKGROUND_CONTEXT_PREFIX+2,2);*latch=word;*code=backgroundVDPContext[BACKGROUND_CONTEXT_PREFIX+4];*pending=backgroundVDPContext[BACKGROUND_CONTEXT_PREFIX+5];
}
static unsigned backgroundRange(unsigned code,unsigned address){
 if(address&1)return 0;
 if(code==1){if(address>=0xc000&&address<=0xcffe)return 1;if(address>=0xe000&&address<=0xeffe)return 2;if(address>=0xf000&&address<=0xf002)return 3;}
 if(code==5&&address<=2)return 4;
 return 0;
}
static unsigned backgroundViewCurrent(const BackgroundSpan*s,unsigned publishing){
 for(unsigned n=0;n<4;n++)if(s->view[n]!=backgroundSourceWord(publishing?0x800110+n*2:0xfff446+n*2))return 0;
 return 1;
}
static void backgroundCheckView(const BackgroundSpan*s,unsigned publishing){
 if(!backgroundViewCurrent(s,publishing))backgroundFail("descriptor does not match current source view");
 if(!s->generation||!s->palette||s->palette!=renderLong(0xf568)||renderWord(0xf56e)||!renderWord(0xf56c))backgroundFail("uncertified BG generation/palette lease");
}
static void backgroundPublishValid(void){
 BackgroundObserverPath*p=backgroundPaths+renderPath;backgroundCheckView(&p->publish,1);
 if(p->cold>1||(p->cold&&renderActive()))backgroundFail("invalid/active cold partial BG publication");
 if(renderRegs[16]!=1||renderRegs[11]||renderRegs[2]!=0x30||renderRegs[4]!=7||renderRegs[13]!=0x3c||
    (renderRegs[1]&0x5c)!=0x44||(renderRegs[12]&0x87)!=0x81||*renderDMALength)backgroundFail("unsupported BG VDP mode or pending DMA");
}
static void backgroundSnapshot(const BackgroundSpan*s,uint64_t deadline){
 BackgroundObserverPath*p=backgroundPaths+renderPath;
 fprintf(backgroundPublications,"%u,%u,%u,%u,%" PRIu64 ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%" PRIu64 ",%" PRIu64 "\n",backgroundSamples++,frameNumber,renderPaths[renderPath].irqs,latchCaptures,stamp(),s->generation,s->palette,s->view[0],s->view[1],s->view[2],s->view[3],p->dataWords,p->controls,p->counts[0],p->counts[1],p->counts[2],p->counts[3],p->cold,deadline,p->lastRetire);
 backgroundDumpSource(backgroundSource,0);
 for(unsigned n=0;n<65536;n++)if(fputc(renderVRAM[n^1],backgroundVRAM)==EOF)abort();
 for(unsigned n=0;n<128;n+=2){uint16_t word;memcpy(&word,renderCRAM+n,2);unsigned bus=((word&0x1c0)<<3)|((word&0x38)<<2)|((word&7)<<1);if(fputc(bus>>8,backgroundVDPState)==EOF||fputc(bus&255,backgroundVDPState)==EOF)abort();}
 for(unsigned n=0;n<128;n++)if(fputc(renderVSRAM[n^1],backgroundVDPState)==EOF)abort();
 if(fwrite(renderRegs,1,32,backgroundVDPState)!=32)abort();
}
static void backgroundPhase(unsigned encoded){
 BackgroundObserverPath*p=backgroundPaths+renderPath;unsigned phase=encoded&255,status=encoded>>8;
 if(!phase&&!renderPaths[renderPath].ownershipArmed)return;
 if(!backgroundCodePC(sourcePC()))backgroundFail("phase marker outside declared native BG code");
 if(phase==1){if(p->prepare.open)backgroundFail("nested BG preparation");p->prepare=backgroundDescriptor(0);backgroundCheckView(&p->prepare,0);p->prepares++;backgroundEvent(phase,status,&p->prepare);}
 else if(phase==2){BackgroundSpan end=backgroundDescriptor(0);if(status||!p->prepare.open||!backgroundSame(&p->prepare,&end))backgroundFail("unpaired BG prepared marker");if(!backgroundViewCurrent(&end,0)){p->stalePrepares++;fprintf(stderr,"BG preparation completed with stale staged view path%u frame%u generation%u ticks%" PRIu64 "\n",renderPath,frameNumber,end.generation,stamp()-p->prepare.begin);}p->prepared++;p->prepareTicks+=stamp()-p->prepare.begin;backgroundEvent(phase,status,&end);p->prepare.open=0;}
 else if(phase==3){
  if(status||p->publish.open||renderPaths[renderPath].publish.open||!p->irqDepth)backgroundFail("BG publication outside exclusive source IRQ completion");
#ifdef RENDER_BG_IRQ_SP
  if((rawLong(RENDER_BG_IRQ_SP&65535)&0xffffff)+60!=p->irqFrames[p->irqDepth-1])backgroundFail("BG wrapper does not preserve original IRQ restore frame");
#endif
  p->publish=backgroundDescriptor(1);
  backgroundContext(&p->address,&p->code,&p->pending,&p->addressLatch);p->increment=renderRegs[15];p->cold=renderWord(0xf5bc);if(p->pending)backgroundFail("incomplete VDP command at BG begin");
  p->firstControl=1;p->dataWords=p->controls=0;memset(p->counts,0,sizeof p->counts);p->lastWrite=p->lastRetire=0;
  memcpy(p->beforeVRAM,renderVRAM,65536);memcpy(p->beforeCRAM,renderCRAM,128);memcpy(p->beforeVSRAM,renderVSRAM,128);memcpy(p->beforeRegs,renderRegs,32);
  backgroundEvent(phase,status,&p->publish);
 }else if(phase==4){
  BackgroundSpan end=backgroundDescriptor(1);if(status||!p->publish.open||!backgroundSame(&p->publish,&end))backgroundFail("unpaired BG publication");
  backgroundPublishValid();backgroundCheckView(&end,1);unsigned address,code,pending,latch;backgroundContext(&address,&code,&pending,&latch);
  if(pending||p->pending||address!=p->address||latch!=p->addressLatch||code!=p->code||*renderDMALength||p->increment!=2||renderWord(0xf5bc)!=p->cold)backgroundFail("actual VDP command state differs from observed BG bus stream");
  for(unsigned n=0;n<65536;n++)if(renderVRAM[n^1]!=p->beforeVRAM[n^1]&&!backgroundRange(1,n&~1u))backgroundFail("BG changed VRAM outside declared plane/scroll ranges");
  if(memcmp(renderCRAM,p->beforeCRAM,128)||memcmp(renderRegs,p->beforeRegs,32)||memcmp(renderVSRAM+4,p->beforeVSRAM+4,124))backgroundFail("BG changed palette/registers/unowned VSRAM");
  uint64_t deadline=(uint64_t)renderPaths[renderPath].irqFrame*RENDER_FRAME_TICKS+RENDER_SAT_FETCH_TICKS;
  if(renderActive()&&(stamp()>deadline||p->lastRetire>deadline))p->deadlineFailures++;
  p->publishes++;if(!p->cold)p->intervalPublishes++;if(renderActive()&&!p->cold)p->activePublishes++;
  p->publishTicks+=stamp()-p->publish.begin;backgroundEvent(phase,status,&end);
  if(!renderPath)backgroundSnapshot(&end,deadline);p->publish.open=0;
 }else if(phase==5){
  unsigned publishing=p->publish.open;if(!publishing&&!p->prepare.open)backgroundFail("BG reject has no open operation");
  if(publishing&&(p->dataWords||p->controls))backgroundFail("BG rejected after partial physical publication");
  p->rejects++;if(renderActive())p->activeRejects++;backgroundEvent(phase,status,publishing?&p->publish:&p->prepare);
  if(publishing)p->publish.open=0;else p->prepare.open=0;
 }else if(phase==6){if(status||!p->prepare.open)backgroundFail("unpaired incomplete cold preparation");p->coldSteps++;p->prepareTicks+=stamp()-p->prepare.begin;backgroundEvent(phase,status,&p->prepare);p->prepare.open=0;}
 else backgroundFail("unknown BG phase");
}
static void backgroundPhaseByte(void){backgroundFail("byte write to atomic BG phase word");}
static unsigned backgroundVDPBefore(unsigned at,unsigned width,unsigned value){
 BackgroundObserverPath*p=backgroundPaths+renderPath;if(!p->publish.open)return 0;
 backgroundPublishValid();
 if(!p->dataWords&&!p->controls&&p->increment!=2)backgroundFail("BG began with unsupported increment");
 if(width!=2||!backgroundCodePC(sourcePC())||renderPaths[renderPath].publish.open)backgroundFail("unowned or byte BG VDP write");
 unsigned port=at&0x1c;
 if(port==4){
  if(!p->pending){
   if((value&0xc000)!=0x4000&&value!=0x8f02&&value!=0x8f80)backgroundFail("BG register/read/invalid command");
   p->address=p->addressLatch|(value&0x3fff);p->code=(p->code&0x3c)|((value>>14)&3);
   if(value==0x8f02||value==0x8f80){p->increment=value&255;p->firstControl=1;}else p->pending=1;
  }
  else{if(value&~0x13u)backgroundFail("BG DMA/unsupported command bits");p->addressLatch=(value&3)<<14;p->address=p->addressLatch|(p->address&0x3fff);p->code=(p->code&3)|((value>>2)&0x3c);p->pending=0;p->firstControl=0;if(!backgroundRange(p->code,p->address))backgroundFail("BG command points outside owned ranges");}
 }else if(port==0){
  unsigned range=backgroundRange(p->code,p->address);
  if(p->pending||p->firstControl||!range||(range>=3&&p->increment!=2))backgroundFail("BG data has no complete owned command");
  p->lastAddress=p->address;p->lastCode=p->code;p->address=(p->address+p->increment)&65535;p->counts[range-1]++;
 }else backgroundFail("BG write to unsupported VDP port");
 return 1;
}
static void backgroundVDPAfter(unsigned at,unsigned value){
 BackgroundObserverPath*p=backgroundPaths+renderPath;
 if((at&0x1c)==0){unsigned word;if(p->lastCode==1)word=renderVRAM[p->lastAddress^1]<<8|renderVRAM[(p->lastAddress+1)^1];else{uint16_t v;memcpy(&v,renderVSRAM+p->lastAddress,2);word=v;}if(word!=(value&65535))backgroundFail("actual VDP data differs from bus write");p->dataWords++;}else{if(renderRegs[15]!=p->increment)backgroundFail("actual VDP increment differs from BG register write");p->controls++;}
 p->lastWrite=stamp();uint32_t tail=(uint32_t)cpu->cycles;for(unsigned n=0;n<4;n++)if(renderFIFO[n]>tail)tail=renderFIFO[n];
 if((*renderDMALength||(*renderStatus&2))&&*renderDMAEnd>tail)tail=*renderDMAEnd;
 uint64_t retire=(uint64_t)frameNumber*RENDER_FRAME_TICKS+tail;if(retire>p->lastRetire)p->lastRetire=retire;
}
static void backgroundBeforeWork(unsigned at,unsigned width,unsigned value){
 (void)value;BackgroundObserverPath*p=backgroundPaths+renderPath;
 if(width==2&&sourcePC()==CADENCE_IRQ+2){
  if(p->irqDepth>=4)backgroundFail("BG IRQ tracking overflow");
  if(renderPaths[renderPath].irqs&&renderPaths[renderPath].irqManager==2&&renderPaths[renderPath].irqScene==2&&!p->intervalPublishes)p->missingPublications++;
  p->intervalPublishes=0;p->irqFrames[p->irqDepth]=0xff0000|((at+2)&65535);p->irqActive[p->irqDepth]=renderActive();p->irqDepth++;
 }
}
static void backgroundReadWork(unsigned at){
 BackgroundObserverPath*p=backgroundPaths+renderPath;
 if(p->irqDepth&&backgroundRTEPC(sourcePC())&&(0xff0000|(at&65535))==p->irqFrames[p->irqDepth-1]+5){if(p->publish.open)backgroundFail("original IRQ returned with open BG publication");p->irqDepth--;}
}
static int backgroundObserveFrame(unsigned path){
 BackgroundObserverPath*p=backgroundPaths+path;
 if(renderPaths[path].unownedVDPWrites){fprintf(stderr,"Unowned VDP writes remain outside sprite/BG publishers\n");return 1;}
 if(!renderPaths[path].ownershipArmed)return 0;
 if(!p->checkedFrames)p->firstChecked=frameNumber;
 for(unsigned n=0;n<sizeof backgroundExpectedPatterns;n++)if(renderVRAM[n^1]!=backgroundExpectedPatterns[n])backgroundFail("resident/blank pattern changed during live BG work");
 p->checkedFrames++;return 0;
}
static int backgroundInit(void*core){
 backgroundSaveVDP=dlsym(core,"vdp_context_save");if(!backgroundSaveVDP)return 0;
 unsigned address,code,pending,latch;backgroundContext(&address,&code,&pending,&latch);
 backgroundEvents=outputFile("background-events.csv","w");backgroundPublications=outputFile("background-publications.csv","w");backgroundSource=outputFile("background-source-contexts.bin","wb");backgroundVRAM=outputFile("background-vram.bin","wb");backgroundVDPState=outputFile("background-vdp-state.bin","wb");
 if(!backgroundEvents||!backgroundPublications||!backgroundSource||!backgroundVRAM||!backgroundVDPState)return 0;
 fputs("path,phase,status,frame,active,master_cycles,pc,generation,palette,s2x,s2y,s3x,s3y,data_words,control_words,plane_a_words,plane_b_words,scroll_words,retire_upper_master\n",backgroundEvents);
 fputs("sample,frame,irq,capture,master_cycles,generation,palette,s2x,s2y,s3x,s3y,data_words,control_words,plane_a_words,plane_b_words,hscroll_words,vscroll_words,cold_partial,deadline_master,retire_upper_master\n",backgroundPublications);return 1;
}
static void backgroundFinish(void){
 fclose(backgroundEvents);fclose(backgroundPublications);fclose(backgroundSource);fclose(backgroundVRAM);fclose(backgroundVDPState);
 FILE*f=outputFile("background-final-source.bin","wb");if(!f)abort();backgroundDumpSource(f,1);fclose(f);
 f=outputFile("background-summary.json","w");if(!f)abort();
 fprintf(f,"{\"scope\":\"Explicit BG publication ranges and same-IRQ source snapshots; independent pixel/priority oracle and complete production workload admission remain separate.\",\"samples\":%u,\"paths\":[",backgroundSamples);
 for(unsigned n=0;n<2;n++){BackgroundObserverPath*p=backgroundPaths+n;fprintf(f,"%s{\"path\":%u,\"prepares\":%u,\"prepared\":%u,\"publishes\":%u,\"active_publishes\":%u,\"rejects\":%u,\"active_rejects\":%u,\"cold_steps\":%u,\"stale_preparations\":%u,\"missing_active_publications\":%u,\"deadline_failures\":%u,\"protocol_failures\":%u,\"prepare_master_ticks\":%" PRIu64 ",\"publish_master_ticks\":%" PRIu64 ",\"final_prepare_open\":%u,\"final_publish_open\":%u,\"final_irq_depth\":%u,\"residency_frames\":%u,\"first_residency_frame\":%u}",n?",":"",n,p->prepares,p->prepared,p->publishes,p->activePublishes,p->rejects,p->activeRejects,p->coldSteps,p->stalePrepares,p->missingPublications,p->deadlineFailures,p->protocolFailures,p->prepareTicks,p->publishTicks,p->prepare.open,p->publish.open,p->irqDepth,p->checkedFrames,p->firstChecked);}
 fputs("]}\n",f);fclose(f);
}
#endif
