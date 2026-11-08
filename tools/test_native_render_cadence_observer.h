/* Host-only observer included into the immutable compact-cadence harness.
 * Calls the installed bus handler exactly once; never executes target code,
 * changes CPU registers/cycles or supplies renderer results. */
#ifndef WILLOW_RENDER_CADENCE_OBSERVER_H
#define WILLOW_RENDER_CADENCE_OBSERVER_H
#define RENDER_FRAME_TICKS 896040u
#define RENDER_SAT_FETCH_TICKS (37u*3420u)
#define RENDER_PHASE_ADDRESS 0x20f540u
typedef struct {
 unsigned bank,records,epoch,palette,view;
 uint64_t begin;
 unsigned open;
} RenderSpan;
typedef struct {
 unsigned irqs,commits,activeIRQs,activeCommits,activeMissing;
 unsigned activeCompletedIntervals,activeMissingPublications;
 unsigned activeCPUDeadlineFailures,activeIRQGaps,irqFrame,intervalOnTimeCommits;
 unsigned prepares,publishes,rejects,activeRejects,protocolFailures;
 unsigned acquisitionFailures,deadlineFailures,modeFailures,sourceOwnedFailures;
 unsigned activePublishes,activePrepareComplete;
 unsigned ownershipArmed;
 unsigned paletteWrites,paletteGuardFailures,stackFailures;
 unsigned prepareLowest,publishLowest,taskStackFree;
 unsigned nativeIRQDepth,nativeIRQFrame[4],nativeIRQTop[4];
 unsigned irqManager,irqScene,intervalCommits,intervalPublishes;
 unsigned vdpWrites,dataWrites,controlWrites,displayWasEnabled;
 unsigned prepareCount,publishCount;
 unsigned retainedChecks,retainedFailures,sourceOracleChecks,sourceOracleFailures;
 unsigned zeroTransferPublishes,changedPublishes,totalDataWrites,totalControlWrites;
 unsigned sourceReuses,geometryBuilds;
 unsigned irqD4,irqPC,hudAttributeWrites,hudNativeAttributeWrites;
 unsigned unownedVDPWrites;
 unsigned sourceContextChecks;
 unsigned exceptionStage,exceptionPC,exceptionBase,exceptionSR,exceptionFrames;
 uint64_t irqBegin,prepareTicks,publishTicks,prepareMaximum,publishMaximum;
 uint64_t lastVDP,lastRetire;
 RenderSpan prepare,publish;
} RenderObserverPath;
static RenderObserverPath renderPaths[2];
static unsigned renderPath;
static uint8_t *renderRegs,*renderVRAM,*renderCRAM,*renderVSRAM;
static uint16_t *renderVCounter,*renderStatus,*renderSatBase;
static uint32_t *renderFIFO,*renderDMALength,*renderDMAEnd,*renderVIntCycle;
static unsigned (*renderGetRegister)(int);
static void (*renderBusVDP8)(unsigned,unsigned),(*renderBusVDP16)(unsigned,unsigned);
static void (*renderBusSRAM8)(unsigned,unsigned),(*renderBusSRAM16)(unsigned,unsigned);
static void (*renderBusWork8)(unsigned,unsigned),(*renderBusWork16)(unsigned,unsigned);
static void (*renderBusMapper8)(unsigned,unsigned);
static FILE *renderEvents,*renderIntervals,*renderTransfers,*renderSatCSV,*renderSatBinary;
static unsigned renderSatSamples;
static uint8_t renderReferenceVRAM[65536],renderReferenceCRAM[128],renderReferenceVSRAM[128],renderReferenceRegs[32];
static unsigned renderComparedFrames;

static unsigned renderActive(void){return rawWord(0x8004)==2&&rawWord(0x8008)==2;}
static unsigned renderAllowedInterruptPC(unsigned pc){
 if(pc<0x100000)return 1;
#if defined(RENDER_CODE_BEGIN) && defined(RENDER_CODE_END)
 if(pc>=RENDER_CODE_BEGIN&&pc<RENDER_CODE_END)return 1;
#endif
#if defined(METADATABEGIN) && defined(METADATAFAULT)
 if(pc>=METADATABEGIN&&pc<METADATAFAULT+20)return 1;
#endif
#if defined(CADENCE_LATCH) && defined(CADENCE_LATCH_END)
 if(pc>=CADENCE_LATCH&&pc<CADENCE_LATCH_END)return 1;
#endif
#if defined(RENDER_PALETTE_CODE_BEGIN) && defined(RENDER_PALETTE_CODE_END)
 if(pc>=RENDER_PALETTE_CODE_BEGIN&&pc<RENDER_PALETTE_CODE_END)return 1;
#endif
 return 0;
}
static unsigned renderPointerStoreOwner(unsigned next){
#ifdef RENDER_POINTER_STORE_COUNT
 for(unsigned n=0;n<RENDER_POINTER_STORE_COUNT;n++)if(next==renderPointerStoreSites[n].pc+renderPointerStoreSites[n].size)return renderPointerStoreSites[n].pc;
#else
 (void)next;
#endif
 return 0;
}
static unsigned renderPointerStoreDetails(unsigned pc,unsigned at,unsigned*reg,unsigned*index,unsigned*value){
#ifdef RENDER_POINTER_STORE_COUNT
 const RenderPointerStoreSite*site=NULL;
 for(unsigned n=0;n<RENDER_POINTER_STORE_COUNT;n++)if(pc==renderPointerStoreSites[n].pc){site=renderPointerStoreSites+n;break;}
 if(!site)return 0;
 unsigned base=(site->size==2?cpu->dar[15]:cpu->dar[14]+site->displacement)&65535;
 if(base<(site->publishing?0x7600:0x7000)||base+4>(site->publishing?0x7800:0x7600)||(at!=base&&at!=base+2))return 0;
 *reg=site->reg;*index=at==base?0:2;
 if(site->kind==0)*value=cpu->dar[*reg];
 else if(site->kind==1)*value=willowMotionLong(rom+pc+2);
 else if(site->kind==2){
  unsigned source=(cpu->dar[14]+site->sourceDisplacement)&65535;
  if(source<0x7000||source+4>0x7600||!owner[source])return 0;
  for(unsigned n=1;n<4;n++)if(owner[source+n]!=owner[source])return 0;
  *value=rawLong(source);
  for(unsigned n=0;n<4;n++)if(savedValue[source+n]!=*value||savedByteIndex[source+n]!=n)return 0;
 }else return 0;
 return 1;
#else
 (void)pc;(void)at;(void)reg;(void)index;(void)value;return 0;
#endif
}
static unsigned renderWord(unsigned offset){return willowMotionWord(metadataSRAM+offset);}
static unsigned renderLong(unsigned offset){return willowMotionLong(metadataSRAM+offset);}
/* At an IRQ immediately after source1750 MOVEM, the hardware frame is pushed
 * while the core still exposes PC1754. Those three words are not MOVEM output.
 * Admit no pointer owner; require their exact sequence and following real IRQ.
 * Core m68ki_stack_frame_3word pushes PC then SR with pre-updated A7. */
static unsigned renderExceptionStore(unsigned ownerPC,unsigned at,unsigned value){
 RenderObserverPath*p=renderPaths+renderPath;
 unsigned sp=cpu->dar[15]&0xffffff,pc=sourcePC();
 if(!p->exceptionStage){
  if(ownerPC!=0x1750||pc!=0x1754||at!=(sp&65535)||sp<0xfff000||sp>0xfffffa||value!=(pc>>16)||
     !renderGetRegister||(renderGetRegister(17)&0x2700)!=0x2600)return 0;
  p->exceptionStage=1;p->exceptionPC=pc;p->exceptionBase=sp;
 }else if(p->exceptionStage==1&&pc==p->exceptionPC&&sp==p->exceptionBase&&at==((sp+2)&65535)&&value==(pc&65535))p->exceptionStage=2;
 else if(p->exceptionStage==2&&pc==p->exceptionPC&&sp+2==p->exceptionBase&&at==(sp&65535)&&!(value&~0x201fu)){
  p->exceptionStage=3;p->exceptionSR=value;
 }else{fprintf(stderr,"Unproved interrupted-MOVEM exception sequence stage%u PC%x SP%x at%x value%x\n",p->exceptionStage,pc,sp,at,value);abort();}
 return 1;
}
static void renderCheckSourceContext(unsigned value){
 if((cpu->dar[13]&0xffffff)!=0xff8000){fprintf(stderr,"Original IRQ global A5 invalid frame%u PC%x A5%x port%x sourceFF8022%x\n",frameNumber,sourcePC(),cpu->dar[13],value,rawWord(0x8022));abort();}
#if defined(RENDER_SOURCE_CONTEXT_READY) && defined(RENDER_SOURCE_CONTEXT_SP)
 if(rawWord(RENDER_SOURCE_CONTEXT_READY)){
  unsigned saved=rawLong(RENDER_SOURCE_CONTEXT_SP)&0xffffff;
  if(saved<0xff0200||saved+62>0xff0a10){fprintf(stderr,"Source IRQ logical context lies outside source task stacks %x\n",saved);abort();}
  for(unsigned n=0;n<15;n++)if(cpu->dar[n]!=rawLong(saved+n*4)){
   fprintf(stderr,"Source IRQ logical register differs frame%u reg%u actual%x saved%x at%x\n",frameNumber,n,cpu->dar[n],rawLong(saved+n*4),saved);abort();
  }
  renderPaths[renderPath].sourceContextChecks++;
 }
#endif
}
static RenderSpan renderDescriptor(unsigned publish){
 unsigned at=publish?0xf554:0xf544;
 RenderSpan s={renderWord(at),renderWord(at+2),renderLong(at+4),renderLong(at+8),renderLong(at+12),stamp(),1};
 return s;
}
#include "test_native_render_cadence_oracle.h"
static void renderEvent(const char*name,const RenderSpan*s,unsigned status){
 fprintf(renderEvents,"%u,%s,%u,%" PRIu64 ",%d,%x,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
  renderPath,name,frameNumber,stamp(),cpu->cycles,sourcePC(),rawWord(0x8004),rawWord(0x8008),
  s?s->bank:0,s?s->records:0,s?s->epoch:0,s?s->palette:0,s?s->view:0,status,
  renderWord(0xf51e),renderWord(0xf530),renderLong(0xf534),renderWord(0xf532),
  *renderVCounter,*renderStatus,renderRegs[1],renderLong(0xf584),renderLong(0xf588));
}
static void renderProtocol(const char*name,const RenderSpan*s){
 renderPaths[renderPath].protocolFailures++;renderEvent(name,s,renderWord(0xf542));
}
static void renderPhase(unsigned encoded){
 RenderObserverPath*p=renderPaths+renderPath;
 unsigned phase=encoded,status=renderWord(0xf542),active=renderActive();
#ifdef RENDER_ATOMIC_PHASE_STATUS
 phase=encoded&255;status=encoded>>8;
#endif
 if(!phase&&!p->ownershipArmed)return; /* Checked platform cold SRAM clear. */
 if(phase==6){p->ownershipArmed=1;renderEvent("renderer_initialized",NULL,status);return;}
 if(phase>=1&&phase<=5)p->ownershipArmed=1;
 if(phase==1){
  RenderSpan next=renderDescriptor(0);
  if(p->prepare.open)renderProtocol("duplicate_prepare_begin",&next);
  p->prepare=next;p->prepares++;renderEvent("prepare_begin",&p->prepare,status);
 }else if(phase==2){
  RenderSpan end=renderDescriptor(0);
  if(!p->prepare.open||p->prepare.bank!=end.bank||p->prepare.records!=end.records||p->prepare.epoch!=end.epoch||p->prepare.palette!=end.palette||p->prepare.view!=end.view)
   renderProtocol("prepare_pair_mismatch",&end);
  if(p->prepare.open){uint64_t ticks=stamp()-p->prepare.begin;p->prepareTicks+=ticks;p->prepareCount++;if(ticks>p->prepareMaximum)p->prepareMaximum=ticks;}
  if(active&&!status)p->activePrepareComplete++;
  renderEvent("prepare_complete",&end,status);p->prepare.open=0;
 }else if(phase==3){
  RenderSpan next=renderDescriptor(1);
  if(p->publish.open)renderProtocol("duplicate_publish_begin",&next);
  p->publish=next;p->lastVDP=p->lastRetire=0;p->vdpWrites=p->dataWrites=p->controlWrites=0;p->displayWasEnabled=!!(renderRegs[1]&64);
  renderEvent("publish_begin",&next,status);
  /* Exact compact descriptor captured from the PRIOR hardware port, not the
   * just-published source FF8022 selector. Fallback/unproved acquisition is a
   * separate reject path, never silently accepted as a ready prefix. */
  if(renderWord(0xf532)&&((next.bank!=renderWord(0xf51e))||(next.records!=renderWord(0xf530))||(next.epoch!=renderLong(0xf534)))){
   p->acquisitionFailures++;renderEvent("publish_acquisition_mismatch",&next,status);
  }
 }else if(phase==4){
  RenderSpan end=renderDescriptor(1);
  if(!p->publish.open||p->publish.bank!=end.bank||p->publish.records!=end.records||p->publish.epoch!=end.epoch||p->publish.palette!=end.palette||p->publish.view!=end.view)
   renderProtocol("publish_pair_mismatch",&end);
  if(p->publish.open){uint64_t ticks=stamp()-p->publish.begin;p->publishTicks+=ticks;p->publishCount++;if(ticks>p->publishMaximum)p->publishMaximum=ticks;}
  p->publishes++;p->intervalPublishes++;if(active)p->activePublishes++;
  uint64_t base=(p->publish.begin/RENDER_FRAME_TICKS)*RENDER_FRAME_TICKS,deadline=base+RENDER_SAT_FETCH_TICKS;
  unsigned mode=(renderRegs[1]&0x4c)==0x44&&(renderRegs[12]&0x87)==0x81&&p->displayWasEnabled;
  unsigned late=stamp()>deadline||p->lastRetire>deadline;
  unsigned retained=0,shadowChecked=0,oracleChecked=0,pieces=0;
#ifdef RENDER_ZERO_TRANSFER_PROOF
  if(!status){
   p->retainedChecks++;
#if defined(RENDER_RETAINED_SAT) && RENDER_RETAINED_SAT == 1
   shadowChecked=renderRetainedProof(&end,&pieces);
#else
   shadowChecked=renderContentProof(&end,&pieces);
#endif
   if(p->unownedVDPWrites)shadowChecked=renderOracleFailed("VDP stores occurred outside exclusive publisher ownership");
   if(!shadowChecked)p->retainedFailures++;
   if(!renderPath){p->sourceOracleChecks++;oracleChecked=renderCurrentSourceOracle(&end,pieces);if(!oracleChecked)p->sourceOracleFailures++;}
   /* Candidate state is paired in full after every frame; the independent
    * source oracle runs on the original bus path only. */
   retained=shadowChecked&&(renderPath||oracleChecked);
  }
#endif
  /* Absence of writes is admitted only by the explicit retained contract,
   * complete physical shadow, current lease and current-source SAT oracle. */
  if(active&&(!mode||(!p->dataWrites&&!retained)||status||*renderDMALength))p->modeFailures++;
  if(!p->dataWrites)p->zeroTransferPublishes++;else p->changedPublishes++;
  p->totalDataWrites+=p->dataWrites;p->totalControlWrites+=p->controlWrites;
  if(active&&late)p->deadlineFailures++;
  fprintf(renderTransfers,"%u,%u,%u,%u,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
   renderPath,frameNumber,active,end.bank,end.records,end.epoch,p->publish.begin,stamp(),p->lastVDP,p->lastRetire,deadline,p->dataWrites,p->controlWrites,mode,late,status,*renderDMALength,*renderSatBase,retained&&!p->dataWrites,shadowChecked,oracleChecked,pieces);
  renderEvent("publish_complete",&end,status);p->publish.open=0;
  if(!renderPath&&!status){
   fprintf(renderSatCSV,"%u,%u,%u,%u,%u,%u,%u,%u,0x%04x\n",renderSatSamples++,frameNumber,latchCaptures,end.bank,end.records,end.epoch,end.palette,end.view,*renderSatBase);
   for(unsigned n=0;n<640;n++)if(fputc(renderVRAM[(0xf400+n)^1],renderSatBinary)==EOF)abort();
  }
 }else if(phase==5){
  unsigned publishing=p->publish.open;
  RenderSpan end=renderDescriptor(publishing);
  p->rejects++;if(active)p->activeRejects++;
  renderEvent(publishing?"publish_reject":"prepare_reject",&end,status);
  if(publishing)p->publish.open=0;else p->prepare.open=0;
 }else renderProtocol("unknown_phase",NULL);
}
static unsigned renderSourceIRQStack(unsigned at,unsigned width){
#if defined(RENDER_STACK_BOTTOM) && defined(RENDER_STACK_TOP)
 RenderObserverPath*p=renderPaths+renderPath;unsigned full=0xff0000|(at&65535),sp=cpu->dar[15]&0xffffff;
 if(p->nativeIRQDepth&&full>=RENDER_STACK_BOTTOM&&full+width<=p->nativeIRQTop[p->nativeIRQDepth-1]&&full+64>=sp)return 1;
#else
 (void)at;(void)width;
#endif
 return 0;
}
static void renderStackWrite(unsigned at,unsigned width){
#if defined(RENDER_STACK_BOTTOM) && defined(RENDER_STACK_TOP)
 RenderObserverPath*p=renderPaths+renderPath;if(!p->ownershipArmed)return;
 unsigned full=0xff0000|(at&65535),sp=cpu->dar[15]&0xffffff;
 if(full>=RENDER_STACK_BOTTOM&&full+width<=RENDER_STACK_TOP){
  unsigned*lowest=full<0xff7600?&p->prepareLowest:&p->publishLowest;
  if(!*lowest||full<*lowest)*lowest=full;
 }
 if(sp>=RENDER_STACK_BOTTOM&&sp<=RENDER_STACK_TOP&&full+80>=sp&&full<=sp){
  unsigned bottom=sp>0xff7600?0xff7600:RENDER_STACK_BOTTOM;
  if(full<bottom){p->stackFailures++;fprintf(stderr,"Renderer stack overflow PC%x SP%x write%x/%u\n",sourcePC(),sp,full,width);abort();}
 }
 if(sourcePC()>=0x100000&&sp>=0xff0200&&sp<=0xff0a10&&full+80>=sp&&full<=sp){
  unsigned task=rawLong(0x204)&65535,top=rawLong(task+12)&0xffffff;
  if(task<=0x1e0&&!(task&31)&&top>=0xff0290&&top<=0xff0a10){
   if(full<top-128||full+width>top){p->stackFailures++;fprintf(stderr,"Renderer wrapper escaped source task stack PC%x write%x top%x\n",sourcePC(),full,top);abort();}
   unsigned free=full-(top-128);if(free<p->taskStackFree)p->taskStackFree=free;
  }
 }
#else
 (void)at;(void)width;
#endif
}
static void renderPaletteWrite(unsigned at,unsigned width){
 if(width==2&&(sourcePC()==0x4d94||sourcePC()==0x4d9c)){
  RenderObserverPath*p=renderPaths+renderPath;p->hudAttributeWrites++;
  if(p->irqPC>=0x100000){p->hudNativeAttributeWrites++;renderEvent("source_irq_hud_native_context",NULL,p->irqD4&0xffff);}
 }
 if(at<0x91c000||at+width>0x91d800)return;
 RenderObserverPath*p=renderPaths+renderPath;p->paletteWrites++;
#if defined(RENDER_PALETTE_CODE_BEGIN) && defined(RENDER_PALETTE_CODE_END)
 if(!renderWord(0xf56e)||renderWord(0xf56c)||!renderLong(0xf568)||sourcePC()<RENDER_PALETTE_CODE_BEGIN||sourcePC()>=RENDER_PALETTE_CODE_END){
  p->paletteGuardFailures++;fprintf(stderr,"Unprotected original palette store PC%x at%x width%u epoch%u ready%u busy%u\n",sourcePC(),at,width,renderLong(0xf568),renderWord(0xf56c),renderWord(0xf56e));abort();
 }
#endif
}
static void renderBeforeWork(unsigned at,unsigned width,unsigned value){
 (void)value;RenderObserverPath*p=renderPaths+renderPath;unsigned pc=sourcePC();at&=65535;
 renderStackWrite(at,width);
#if defined(RENDER_BSS_BEGIN) && defined(RENDER_BSS_END) && defined(RENDER_STACK_BOTTOM) && defined(RENDER_STACK_TOP)
 if(p->ownershipArmed&&pc<0x100000){
  unsigned full=0xff0000|at,last=full+width;
  if((full<RENDER_BSS_END&&last>RENDER_BSS_BEGIN)||((full<RENDER_STACK_TOP&&last>RENDER_STACK_BOTTOM)&&!renderSourceIRQStack(at,width))){
   p->sourceOwnedFailures++;fprintf(stderr,"Original source writes native renderer ownership PC%x address%x width%u\n",pc,full,width);abort();
  }
 }
#endif
 (void)at;
 if(width==2&&pc==CADENCE_IRQ+2){
  if(p->exceptionStage){
   if(p->exceptionStage!=3||((at+4)&65535)!=(p->exceptionBase&65535)||rawWord(at+2)!=p->exceptionSR||rawLong(at+4)!=p->exceptionPC||(value&0x2700)!=0x2600){
    fprintf(stderr,"Deferred hardware exception did not reach its exact native IRQ frame\n");abort();
   }
   p->exceptionFrames++;p->exceptionStage=0;
  }
  p->irqD4=cpu->dar[4];p->irqPC=rawLong(at+4)&0xffffff;
#if defined(RENDER_STACK_BOTTOM) && defined(RENDER_STACK_TOP)
  unsigned frame=0xff0000|(at+2),top=frame+6;
  if(frame>=RENDER_STACK_BOTTOM&&top<=RENDER_STACK_TOP){
   if(p->nativeIRQDepth>=4){fprintf(stderr,"Nested native IRQ stack observer overflow\n");abort();}
   p->nativeIRQFrame[p->nativeIRQDepth]=frame;p->nativeIRQTop[p->nativeIRQDepth++]=top;
  }
#endif
  if(p->irqs){
   unsigned active=p->irqManager==2&&p->irqScene==2;
   if(active)p->activeCompletedIntervals++;
   if(active&&!p->intervalCommits)p->activeMissing++;
   if(active&&!p->intervalPublishes)p->activeMissingPublications++;
   if(active&&!p->intervalOnTimeCommits)p->activeCPUDeadlineFailures++;
   if(active&&frameNumber!=p->irqFrame+1)p->activeIRQGaps++;
   fprintf(renderIntervals,"%u,%u,%u,%u,%u,%u,%" PRIu64 ",%" PRIu64 ",%u,%u,%u,%u\n",renderPath,p->irqs,frameNumber,p->irqManager,p->irqScene,active,p->irqBegin,stamp(),p->intervalCommits,p->intervalPublishes,p->prepare.open,p->publish.open);
  }
  p->irqs++;p->irqBegin=stamp();p->irqFrame=frameNumber;p->irqManager=rawWord(0x8004);p->irqScene=rawWord(0x8008);p->intervalCommits=p->intervalPublishes=p->intervalOnTimeCommits=0;
  if(p->irqManager==2&&p->irqScene==2)p->activeIRQs++;
  renderEvent("hardware_irq_entry",NULL,0);
 }
}
static void renderAfterWork(unsigned at,unsigned width,unsigned value){
 (void)value;RenderObserverPath*p=renderPaths+renderPath;at&=65535;
 if(width==2&&sourcePC()==0x1780&&at==((cpu->dar[8]+6)&65535)){
  unsigned task=cpu->dar[8]&65535,usp=rawLong(task+8);
  if(rawLong(task+4)==0x1756&&rawLong(usp+60)==0x932c){
   p->commits++;p->intervalCommits++;if(renderActive())p->activeCommits++;
   if(p->irqs&&stamp()<=(uint64_t)(p->irqFrame+1)*RENDER_FRAME_TICKS+*renderVIntCycle)p->intervalOnTimeCommits++;
   renderEvent("source_main_sleep_committed",NULL,0);
  }
 }
}
static unsigned renderNativeRead8(unsigned at){
#if defined(RENDER_BSS_BEGIN) && defined(RENDER_BSS_END) && defined(RENDER_STACK_BOTTOM) && defined(RENDER_STACK_TOP)
 RenderObserverPath*p=renderPaths+renderPath;
 if(p->ownershipArmed&&sourcePC()<0x100000){unsigned full=0xff0000|(at&65535);if((full>=RENDER_BSS_BEGIN&&full<RENDER_BSS_END)||((full>=RENDER_STACK_BOTTOM&&full<RENDER_STACK_TOP)&&!renderSourceIRQStack(at,1))){p->sourceOwnedFailures++;fprintf(stderr,"Original source reads renderer ownership PC%x address%x\n",sourcePC(),full);abort();}}
 /* Source RTE consumes the last byte of the actual saved PC. This observation
  * ends only that proven exception frame; it does not emulate the return. */
 if(p->nativeIRQDepth&&(sourcePC()==0x15b8||sourcePC()==0x15ba)&&(0xff0000|(at&65535))==p->nativeIRQFrame[p->nativeIRQDepth-1]+5)p->nativeIRQDepth--;
#endif
 return ram[(at&65535)^1];
}
static unsigned renderNativeRead16(unsigned at){if(at&1)abort();return renderNativeRead8(at)<<8|renderNativeRead8(at+1);}
static void renderNativeWork8(unsigned at,unsigned value){
 renderBeforeWork(at,1,value);
 if(renderBusWork8)renderBusWork8(at,value);else ram[(at&65535)^1]=(uint8_t)value;
 renderAfterWork(at,1,value);
}
static void renderNativeWork16(unsigned at,unsigned value){
 renderBeforeWork(at,2,value);
 if(renderBusWork16)renderBusWork16(at,value);else{at&=65535;if(at&1)abort();ram[at^1]=(uint8_t)(value>>8);ram[(at+1)^1]=(uint8_t)value;}
 renderAfterWork(at,2,value);
}
static void renderSRAM8(unsigned at,unsigned value){renderBusSRAM8(at,value);if(at==RENDER_PHASE_ADDRESS||at==RENDER_PHASE_ADDRESS+1)renderProtocol("byte_phase_store",NULL);}
static void renderSRAM16(unsigned at,unsigned value){renderBusSRAM16(at,value);if(at==RENDER_PHASE_ADDRESS)renderPhase(value&65535);if(at==0x20f900&&sourcePC()==0x1560)renderCheckSourceContext(value);}
static void renderVDPAfter(unsigned at){
 RenderObserverPath*p=renderPaths+renderPath;
 if(!p->publish.open){if(p->ownershipArmed)p->unownedVDPWrites++;return;}
 p->vdpWrites++;if((at&0x1c)==0)p->dataWrites++;if((at&0x1c)==4)p->controlWrites++;
 p->lastVDP=stamp();uint32_t tail=(uint32_t)cpu->cycles;
 for(unsigned n=0;n<4;n++)if(renderFIFO[n]>tail)tail=renderFIFO[n];
 if((*renderDMALength||(*renderStatus&2))&&*renderDMAEnd>tail)tail=*renderDMAEnd;
 uint64_t retire=(uint64_t)frameNumber*RENDER_FRAME_TICKS+tail;if(retire>p->lastRetire)p->lastRetire=retire;
 if(!(renderRegs[1]&64))p->displayWasEnabled=0;
}
static void renderVDP8(unsigned at,unsigned value){renderBusVDP8(at,value);renderVDPAfter(at);}
static void renderVDP16(unsigned at,unsigned value){renderBusVDP16(at,value);renderVDPAfter(at);}
static void renderInstallSRAM(void){
 if(cpu->map[0x20].write8!=renderSRAM8)renderBusSRAM8=cpu->map[0x20].write8;
 if(cpu->map[0x20].write16!=renderSRAM16)renderBusSRAM16=cpu->map[0x20].write16;
 if(!renderBusSRAM8||!renderBusSRAM16){fprintf(stderr,"Unavailable SRAM bus handlers\n");abort();}
 cpu->map[0x20].write8=renderSRAM8;cpu->map[0x20].write16=renderSRAM16;
}
static void renderMapper8(unsigned at,unsigned value){
 renderBusMapper8(at,value);
 /* Enabling SRAM may replace page20 handlers during the very first frame.
  * Reattach observation only after the real mapper has performed its write. */
 if((at&0xffffff)==0xa130f1)renderInstallSRAM();
}
static void renderSelect(unsigned path){
 renderPath=path;
 /* The compact harness restores original map entries before this function.
  * SRAM/VDP maps may survive unserialization; do not chain our own wrappers. */
 if(cpu->map[0xc0].write8!=renderVDP8)renderBusVDP8=cpu->map[0xc0].write8;
 if(cpu->map[0xc0].write16!=renderVDP16)renderBusVDP16=cpu->map[0xc0].write16;
 if(!renderBusVDP8||!renderBusVDP16){fprintf(stderr,"Unavailable VDP bus callbacks\n");abort();}
 cpu->map[0xc0].write8=renderVDP8;cpu->map[0xc0].write16=renderVDP16;
 renderInstallSRAM();
 if(cpu->map[0xa1].write8!=renderMapper8)renderBusMapper8=cpu->map[0xa1].write8;
 if(!renderBusMapper8){fprintf(stderr,"Unavailable mapper bus callback\n");abort();}
 cpu->map[0xa1].write8=renderMapper8;
 if(path){
  renderBusWork8=cpu->map[0xff].write8;renderBusWork16=cpu->map[0xff].write16;
  if((!renderBusWork8||!renderBusWork16)&&cpu->map[0xff].base!=ram){fprintf(stderr,"Unrecognized direct work RAM map\n");abort();}
  cpu->map[0xff].write8=renderNativeWork8;cpu->map[0xff].write16=renderNativeWork16;
  cpu->map[0xff].read8=renderNativeRead8;cpu->map[0xff].read16=renderNativeRead16;
 }
}
static int renderObserveFrame(unsigned path){
 if(renderPaths[path].retainedFailures||renderPaths[path].sourceOracleFailures){fprintf(stderr,"Retained SAT contract failed frame%u\n",frameNumber);return 1;}
 renderPaths[path].sourceReuses=renderLong(0xf584);renderPaths[path].geometryBuilds=renderLong(0xf588);
 if(!path){memcpy(renderReferenceVRAM,renderVRAM,65536);memcpy(renderReferenceCRAM,renderCRAM,128);memcpy(renderReferenceVSRAM,renderVSRAM,128);memcpy(renderReferenceRegs,renderRegs,32);return 0;}
 if(memcmp(renderReferenceVRAM,renderVRAM,65536)||memcmp(renderReferenceCRAM,renderCRAM,128)||memcmp(renderReferenceVSRAM,renderVSRAM,128)||memcmp(renderReferenceRegs,renderRegs,32)){
  fprintf(stderr,"Paired native VDP state differs frame%u\n",frameNumber);return 1;
 }
 renderComparedFrames++;return 0;
}
static int renderInit(void*core){
#define RENDER_SYMBOL(variable,name) do{variable=dlsym(core,name);if(!variable){fprintf(stderr,"Missing read-only VDP symbol %s\n",name);return 0;}}while(0)
 RENDER_SYMBOL(renderRegs,"reg");RENDER_SYMBOL(renderVRAM,"vram");RENDER_SYMBOL(renderCRAM,"cram");RENDER_SYMBOL(renderVSRAM,"vsram");
 RENDER_SYMBOL(renderVCounter,"v_counter");RENDER_SYMBOL(renderStatus,"status");RENDER_SYMBOL(renderSatBase,"satb");RENDER_SYMBOL(renderFIFO,"fifo_cycles");
 RENDER_SYMBOL(renderDMALength,"dma_length");RENDER_SYMBOL(renderDMAEnd,"dma_endCycles");RENDER_SYMBOL(renderVIntCycle,"vint_cycle");
 RENDER_SYMBOL(renderGetRegister,"m68k_get_reg");
#undef RENDER_SYMBOL
 renderEvents=outputFile("render-events.csv","w");renderIntervals=outputFile("render-intervals.csv","w");renderTransfers=outputFile("render-transfers.csv","w");
 renderSatCSV=outputFile("render-sat.csv","w");renderSatBinary=outputFile("render-sat.bin","wb");
 if(!renderEvents||!renderIntervals||!renderTransfers||!renderSatCSV||!renderSatBinary)return 0;
 renderPaths[0].taskStackFree=renderPaths[1].taskStackFree=128;
 fputs("sample,frame,capture,bank,records,epoch,palette,view,sat_base\n",renderSatCSV);
 fputs("path,event,frame,absolute_master,frame_master,callback_pc,manager,scene,bank,records,epoch,palette,view,status,acquired_bank,acquired_records,acquired_epoch,compact_valid,v_counter,vdp_status,vdp_reg1,source_reuses,geometry_builds\n",renderEvents);
 fputs("path,irq,end_frame,manager,scene,active,begin_master,end_master,source_main_commits,sat_publications,prepare_open,publish_open\n",renderIntervals);
 fputs("path,frame,active,bank,records,epoch,begin_master,end_master,last_vdp_callback,retire_upper_master,sat_fetch_deadline,data_writes,control_writes,active_h40_mode,late,status,dma_remaining,sat_base,retained_zero_transfer,retained_contract_checked,current_source_oracle_checked,source_pieces\n",renderTransfers);
 return 1;
}
static void renderFinish(void){
 fclose(renderEvents);fclose(renderIntervals);fclose(renderTransfers);fclose(renderSatCSV);fclose(renderSatBinary);
 FILE*dump=outputFile("render-final-vram.bin","wb");if(!dump)abort();for(unsigned n=0;n<65536;n++)if(fputc(renderReferenceVRAM[n^1],dump)==EOF)abort();fclose(dump);
 dump=outputFile("render-final-cram-internal.bin","wb");if(!dump||fwrite(renderReferenceCRAM,1,128,dump)!=128)abort();fclose(dump);
 dump=outputFile("render-final-cram.bin","wb");if(!dump)abort();for(unsigned n=0;n<128;n+=2){uint16_t word;memcpy(&word,renderReferenceCRAM+n,2);unsigned bus=((word&0x1c0)<<3)|((word&0x38)<<2)|((word&7)<<1);if(fputc(bus>>8,dump)==EOF||fputc(bus&255,dump)==EOF)abort();}fclose(dump);
 dump=outputFile("render-final-registers.bin","wb");if(!dump||fwrite(renderReferenceRegs,1,32,dump)!=32)abort();fclose(dump);
 FILE*f=outputFile("render-summary.json","w");if(!f)abort();
 fprintf(f,"{\n  \"schema_version\":1,\n  \"scope\":\"Continuous paired source/native renderer execution at Genesis cadence; marker and bus observations only. Fixed resources and original-bus reference share actual renderer cost. No CPS-clock equivalence, audio or all-stage claim.\",\n  \"master_hz\":53693175,\n  \"frame_master_ticks\":896040,\n  \"first_sat_fetch_frame_ticks\":126540,\n  \"vint_cycle\":%u,\n  \"paired_vdp_frames\":%u,\n  \"paths\":[\n",*renderVIntCycle,renderComparedFrames);
 for(unsigned n=0;n<2;n++){
  RenderObserverPath*p=renderPaths+n;
  fprintf(f,"    {\"path\":%u,\"irqs\":%u,\"main_commits\":%u,\"active_irqs\":%u,\"active_main_commits\":%u,\"active_completed_intervals\":%u,\"active_completed_intervals_without_commit\":%u,\"active_completed_intervals_without_publication\":%u,\"active_cpu_deadline_failures\":%u,\"active_physical_irq_gaps\":%u,\"prepare_begins\":%u,\"prepare_completes\":%u,\"prepare_total_master\":%" PRIu64 ",\"prepare_max_master\":%" PRIu64 ",\"publishes\":%u,\"publish_total_master\":%" PRIu64 ",\"publish_max_master\":%" PRIu64 ",\"active_publishes\":%u,\"rejects\":%u,\"active_rejects\":%u,\"protocol_failures\":%u,\"acquisition_failures\":%u,\"active_sat_deadline_failures\":%u,\"active_mode_or_transfer_failures\":%u,\"source_ownership_failures\":%u,\"final_prepare_open\":%u,\"final_publish_open\":%u,",
   n,p->irqs,p->commits,p->activeIRQs,p->activeCommits,p->activeCompletedIntervals,p->activeMissing,p->activeMissingPublications,p->activeCPUDeadlineFailures,p->activeIRQGaps,p->prepares,p->prepareCount,p->prepareTicks,p->prepareMaximum,p->publishes,p->publishTicks,p->publishMaximum,p->activePublishes,p->rejects,p->activeRejects,p->protocolFailures,p->acquisitionFailures,p->deadlineFailures,p->modeFailures,p->sourceOwnedFailures,p->prepare.open,p->publish.open);
  fprintf(f,"\"palette_store_calls\":%u,\"palette_guard_failures\":%u,\"stack_failures\":%u,\"prepare_lowest_stack_write\":%u,\"publish_lowest_stack_write\":%u,\"source_task_minimum_free_bytes\":%u,\"final_native_irq_depth\":%u,",p->paletteWrites,p->paletteGuardFailures,p->stackFailures,p->prepareLowest,p->publishLowest,p->taskStackFree,p->nativeIRQDepth);
  fprintf(f,"\"retained_contract_checks\":%u,\"retained_contract_failures\":%u,\"current_source_oracle_checks\":%u,\"current_source_oracle_failures\":%u,\"zero_transfer_publications\":%u,\"changed_publications\":%u,\"total_vdp_data_writes\":%u,\"total_vdp_control_writes\":%u,\"target_source_reuses\":%u,\"target_geometry_builds\":%u,\"source_irq_hud_attribute_stores\":%u,\"source_irq_hud_stores_during_native_code\":%u,\"vdp_stores_outside_publisher\":%u,\"logical_source_irq_context_checks\":%u,\"verified_interrupted_movem_frames\":%u,\"pending_exception_proof_stage\":%u}%s\n",p->retainedChecks,p->retainedFailures,p->sourceOracleChecks,p->sourceOracleFailures,p->zeroTransferPublishes,p->changedPublishes,p->totalDataWrites,p->totalControlWrites,p->sourceReuses,p->geometryBuilds,p->hudAttributeWrites,p->hudNativeAttributeWrites,p->unownedVDPWrites,p->sourceContextChecks,p->exceptionFrames,p->exceptionStage,n?"":",");
 }
 fputs("  ],\n  \"admitted\":false,\n  \"admission_note\":\"Inspect complete source update intervals, all active acquire/publish events, SAT fetch deadline, pending DMA and palette/view ownership proof. Empty marker output is only an observer smoke test. Final partial interval is excluded.\"\n}\n",f);fclose(f);
}
#endif
