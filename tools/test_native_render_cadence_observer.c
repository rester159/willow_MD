/* Measurement logic negative tests, not target execution or fidelity proof. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#define CADENCE_IRQ 0x100068u
#define RENDER_CODE_BEGIN 0x104000u
#define RENDER_CODE_END 0x110000u
#define RENDER_BSS_BEGIN 0xff4000u
#define RENDER_BSS_END 0xff5000u
#define RENDER_STACK_BOTTOM 0xff7000u
#define RENDER_STACK_TOP 0xff7800u
#define RENDER_ATOMIC_PHASE_STATUS 1
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 int cycles;unsigned dar[16],pc;
} ObserverCPU;
static ObserverCPU cpuStore,*cpu=&cpuStore;
static unsigned frameNumber;
static unsigned latchCaptures;
static uint8_t ramStore[65536],sramStore[65536],*ram=ramStore,*metadataSRAM=sramStore;
static unsigned willowMotionWord(const uint8_t*p){return (unsigned)p[0]<<8|p[1];}
static unsigned willowMotionLong(const uint8_t*p){return willowMotionWord(p)<<16|willowMotionWord(p+2);}
static void word(unsigned at,unsigned v){metadataSRAM[at]=v>>8;metadataSRAM[at+1]=v;}
static void longword(unsigned at,unsigned v){word(at,v>>16);word(at+2,v);}
static unsigned rawByte(unsigned at){return ram[(at&65535)^1];}
static unsigned rawWord(unsigned at){return rawByte(at)<<8|rawByte(at+1);}
static unsigned rawLong(unsigned at){return rawWord(at)<<16|rawWord(at+2);}
static unsigned sourcePC(void){return cpu->pc;}
static uint64_t stamp(void){return (uint64_t)frameNumber*896040+(int64_t)cpu->cycles;}
static FILE*outputFile(const char*name,const char*mode){(void)name;(void)mode;return tmpfile();}
static unsigned mockRegister(int index){return index==17?0x2610:0;}
#include "test_native_render_cadence_observer.h"
static void require(unsigned yes,const char*message){if(!yes){fprintf(stderr,"Observer unit failure: %s\n",message);exit(1);}}
static void descriptor(unsigned publish,unsigned bank,unsigned count,unsigned epoch){unsigned at=publish?0xf554:0xf544;word(at,bank);word(at+2,count);longword(at+4,epoch);longword(at+8,77);longword(at+12,1);}
static void dataWrite(unsigned cycles,unsigned retire){cpu->cycles=cycles;renderFIFO[0]=retire;renderVDPAfter(0xc00000);}
int main(void){
 static uint8_t regs[32],vram[65536];static uint16_t vc,status,satbase;static uint32_t fifo[4],dmaLength,dmaEnd,vint=770;
 renderVRAM=vram;
 renderRegs=regs;renderVCounter=&vc;renderStatus=&status;renderSatBase=&satbase;renderFIFO=fifo;renderDMALength=&dmaLength;renderDMAEnd=&dmaEnd;renderVIntCycle=&vint;
 renderEvents=tmpfile();renderIntervals=tmpfile();renderTransfers=tmpfile();renderSatCSV=tmpfile();renderSatBinary=tmpfile();require(renderEvents&&renderIntervals&&renderTransfers&&renderSatCSV&&renderSatBinary,"tmp files");
 ram[0x8004^1]=0;ram[0x8005^1]=2;ram[0x8008^1]=0;ram[0x8009^1]=2;regs[1]=0x44;regs[12]=0x81;
 word(0xf51e,1);word(0xf530,5);longword(0xf534,42);word(0xf532,1);
 renderPhase(0);require(!renderPaths[0].protocolFailures,"cold clear is not a marker");renderPhase(6);
 require(renderPaths[0].ownershipArmed,"explicit initialization arms ownership");
 cpu->cycles=1000;descriptor(0,0,3,41);renderPhase(1);
 cpu->cycles=2000;descriptor(1,1,5,42);renderPhase(3);dataWrite(2100,2200);cpu->cycles=2300;renderPhase(4);
 require(renderPaths[0].prepare.open&&!renderPaths[0].publish.open,"IRQ publication keeps interrupted prepare context");
 cpu->cycles=4000;descriptor(0,0,3,41);renderPhase(2);
 require(renderPaths[0].prepareTicks==3000&&renderPaths[0].publishTicks==300,"nested wall intervals preserved without summing");
 require(!renderPaths[0].protocolFailures&&!renderPaths[0].acquisitionFailures&&!renderPaths[0].deadlineFailures&&!renderPaths[0].modeFailures,"correct nested sample passes");
 cpu->cycles=6000;descriptor(1,1,5,43);renderPhase(3);word(0xf542,99);renderPhase((42u<<8)|5);word(0xf542,0);
 require(renderPaths[0].acquisitionFailures==1&&renderPaths[0].activeRejects==1,"wrong acquired epoch rejected");
 cpu->cycles=120000;descriptor(1,1,5,42);renderPhase(3);dataWrite(121000,126541);cpu->cycles=122000;renderPhase(4);
 require(renderPaths[0].deadlineFailures==1,"FIFO retirement over SAT fetch boundary fails even if CPU returns early");
 cpu->cycles=125000;renderPhase(3);dataWrite(125100,125100);cpu->cycles=126541;renderPhase(4);
 require(renderPaths[0].deadlineFailures==2,"publication CPU return after SAT fetch boundary fails");
 cpu->cycles=30000;renderPhase(3);dataWrite(30100,30100);dmaLength=1;cpu->cycles=30200;renderPhase(4);dmaLength=0;
 require(renderPaths[0].modeFailures==1,"unfinished DMA is unadmitted");
 regs[1]&=~64;cpu->cycles=40000;renderPhase(3);dataWrite(40100,40100);cpu->cycles=40200;renderPhase(4);regs[1]|=64;
 require(renderPaths[0].modeFailures==2,"display-disabled transfer is not active-display admission");
 cpu->cycles=50000;renderPhase(3);cpu->cycles=50100;renderPhase(4);
 require(renderPaths[0].modeFailures==3,"no transfer cannot imply unchanged-resource proof");
 cpu->cycles=55000;renderPhase(2);require(renderPaths[0].protocolFailures==1,"orphan completion fails");
 require(renderAllowedInterruptPC(0x3b58c)&&renderAllowedInterruptPC(0x104010)&&!renderAllowedInterruptPC(0x100300),"interrupt PC whitelist");
 renderPaths[0].irqs=1;renderPaths[0].irqFrame=0;renderPaths[0].irqManager=renderPaths[0].irqScene=2;
 renderPaths[0].intervalCommits=1;renderPaths[0].intervalOnTimeCommits=0;
 frameNumber=1;cpu->cycles=2000;cpu->pc=CADENCE_IRQ+2;renderBeforeWork(0x6ffe,2,0x2600);
 require(renderPaths[0].activeCPUDeadlineFailures==1&&!renderPaths[0].activeMissing,"late source completion differs from absent completion");
 frameNumber=3;cpu->cycles=2000;renderBeforeWork(0x6ffe,2,0x2600);
 require(renderPaths[0].activeIRQGaps==1&&renderPaths[0].activeMissing==1,"missing physical IRQ and missing source completion detected");
 renderGetRegister=mockRegister;cpu->pc=0x1754;cpu->dar[15]=0xffffffee;
 require(!renderExceptionStore(0x1750,0xffea,0),"actual predecrement slot is not hardware PC push");
 require(renderExceptionStore(0x1750,0xffee,0)&&renderExceptionStore(0x1750,0xfff0,0x1754),"hardware PC high/low sequence after1750");
 cpu->dar[15]=0xffffffec;require(renderExceptionStore(0x1750,0xffec,0x10),"hardware original SR completes pending frame");
 ram[0xffec^1]=0;ram[0xffed^1]=0x10;ram[0xffee^1]=0;ram[0xffef^1]=0;ram[0xfff0^1]=0x17;ram[0xfff1^1]=0x54;
 cpu->pc=CADENCE_IRQ+2;cpu->dar[15]=0xffffffea;frameNumber++;
 renderBeforeWork(0xffea,2,0x2610);
 require(renderPaths[0].exceptionFrames==1&&!renderPaths[0].exceptionStage,"only matching actual IRQ frame retires deferred proof");
 fclose(renderEvents);fclose(renderIntervals);fclose(renderTransfers);fclose(renderSatCSV);fclose(renderSatBinary);
 puts("Observer negative tests: nested preparation/publication, acquired epoch, FIFO deadline, CPU deadline, pending DMA, display mode, missing transfer, orphan marker and PC whitelist passed.");
 return 0;
}
