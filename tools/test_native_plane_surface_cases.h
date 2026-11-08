/* Shared native/host cases. Expected VRAM is an independent complete plane,
 * never copied from the surface under test. Reads and sentinel checks occur
 * outside the measured publication call. */
#include <stddef.h>
#include "willow_native_plane_surface.h"
static uint16_t colors[64],model[2048],initial[2049],blockWords[40];
static volatile uint32_t epoch,otherEpoch;
static const WillowPresentationPlan plan={colors,0,0,7},otherPlan={colors,0,0,7},noBG={colors,0,0,2};
static WillowPresentationLease lease,otherLease;
static WillowPresentationFrame frameState;
static WillowNativePlaneTarget target;
static struct {uint16_t before;WillowNativePlaneSurface surface;uint16_t after;} guarded;
static WillowNativePlaneSurface saved;
static WillowNativePlaneTransfer transfer;
static unsigned successes,refusals,invalidEdits,kind;
static uint16_t activeBase;
#define CHECK(x) do{if(!(x)){testFail(__LINE__,0,0);return 0;}}while(0)
#define EQ(x,y) do{uint32_t aa=(x),bb=(y);if(aa!=bb){testFail(__LINE__,aa,bb);return 0;}}while(0)
static void copyBytes(void*d,const void*s,unsigned n){uint8_t*x=d;const uint8_t*y=s;while(n--)*x++=*y++;}
static uint8_t equalBytes(const void*a,const void*b,unsigned n){const uint8_t*x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static uint8_t readback(void){
 testReadBegin(activeBase-8);for(unsigned n=0;n<4;n++)EQ(testReadWord(),0x1369+n);
 for(unsigned n=0;n<2048;n++){EQ(testReadWord(),model[n]);EQ(guarded.surface.words[n],model[n]);}
 for(unsigned n=0;n<4;n++)EQ(testReadWord(),0x9753+n);
 EQ(guarded.before,0xace1);EQ(guarded.after,0x5a69);return 1;
}
static void seedVRAM(uint16_t base){
 activeBase=base;testWriteBegin(base-8);
 for(unsigned n=0;n<4;n++)testWriteWord(0x1369+n);
 for(unsigned n=0;n<2048;n++)testWriteWord(0x5aa5^n);
 for(unsigned n=0;n<4;n++)testWriteWord(0x9753+n);
}
static uint8_t init(uint16_t base,uint16_t alignment,uint16_t blank){
 epoch=77;otherEpoch=77;lease.plan=&plan;lease.epoch=&epoch;lease.generation=77;
 otherLease=lease;frameState.lease=&lease;frameState.generation=77;frameState.ready=7;
 guarded.before=0xace1;guarded.after=0x5a69;
 for(unsigned n=0;n<2049;n++)initial[n]=(uint16_t)(n*7919u+0x1379);
 for(unsigned n=0;n<2048;n++)model[n]=blank?0:initial[n+alignment];
 CHECK(willowNativePlaneTargetInit(&target,base));
 CHECK(willowNativePlaneLoad(&guarded.surface,&lease,blank?0:initial+alignment));
 seedVRAM(base);return 1;
}
static uint8_t publish(unsigned rows,unsigned words,unsigned noStats){
 transfer.rows=transfer.words=0xffff;testTimingBegin(kind,words);
 uint8_t ok=willowNativePlanePublish(&guarded.surface,&target,&frameState,noStats?0:&transfer);
 testTimingEnd(rows,words);CHECK(ok);
 if(!noStats){EQ(transfer.rows,rows);EQ(transfer.words,words);}
 EQ(guarded.surface.dirty,0);EQ(guarded.surface.publishedBase,target.base);
 CHECK(readback());successes++;return 1;
}
static uint8_t refuse(WillowNativePlaneSurface*s,const WillowNativePlaneTarget*t,const WillowPresentationFrame*f){
 copyBytes(&saved,&guarded.surface,sizeof saved);transfer.rows=transfer.words=0xffff;
 testTimingBegin(7,0);uint8_t ok=willowNativePlanePublish(s,t,f,&transfer);testTimingEnd(0,0);
 CHECK(!ok);CHECK(equalBytes(&saved,&guarded.surface,sizeof saved));EQ(transfer.rows,0);EQ(transfer.words,0);
 CHECK(readback());refusals++;return 1;
}
static uint8_t set(unsigned x,unsigned y,uint16_t v){CHECK(willowNativePlaneSet(&guarded.surface,x,y,v));model[y*64+x]=v;return 1;}
static uint8_t invalidSet(unsigned x,unsigned y){
 copyBytes(&saved,&guarded.surface,sizeof saved);CHECK(!willowNativePlaneSet(&guarded.surface,x,y,0x9999));CHECK(equalBytes(&saved,&guarded.surface,sizeof saved));invalidEdits++;return 1;
}
static uint8_t invalidBlock(const uint16_t*w,unsigned x,unsigned y,unsigned width,unsigned height,unsigned stride){
 copyBytes(&saved,&guarded.surface,sizeof saved);CHECK(!willowNativePlaneBlock(&guarded.surface,x,y,w,width,height,stride));CHECK(equalBytes(&saved,&guarded.surface,sizeof saved));invalidEdits++;return 1;
}
static uint8_t testRun(void){
 successes=refusals=invalidEdits=0;
 /* All 8 legal plane bases, both source word alignments and explicit blanks. */
 for(unsigned base=0;base<65536;base+=8192)for(unsigned align=0;align<2;align++){
  CHECK(init(base,align,align));kind=0;CHECK(publish(32,2048,0));kind=1;CHECK(publish(0,0,0));
 }
 CHECK(init(0xc000,0,0));kind=0;CHECK(publish(32,2048,0));
 /* Every cell independently exercises its row bit and both column endpoints. */
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<64;x++){
  kind=2;CHECK(set(x,y,model[y*64+x]^0x5555));CHECK(publish(1,1,0));
  kind=1;CHECK(set(x,y,model[y*64+x]));CHECK(publish(0,0,0));
 }
 /* Resolve all source blocks first; exercise 2x2 and 4x4 at every native origin,
  * with padded input stride and both source alignments. Native ring wrap occurs
  * in both axes; a wrapped row spans columns 0..63, including the clean gap. */
 for(unsigned size=2;size<=4;size+=2)for(unsigned y=0;y<32;y++)for(unsigned x=0;x<64;x++){
  unsigned alignment=(x+y)&1,stride=size+1;
  for(unsigned dy=0;dy<size;dy++)for(unsigned dx=0;dx<size;dx++)blockWords[alignment+dy*stride+dx]=model[((y+dy)&31)*64+((x+dx)&63)]^0x3333;
  CHECK(willowNativePlaneBlock(&guarded.surface,x,y,blockWords+alignment,size,size,stride));
  for(unsigned dy=0;dy<size;dy++)for(unsigned dx=0;dx<size;dx++)model[((y+dy)&31)*64+((x+dx)&63)]=blockWords[alignment+dy*stride+dx];
  kind=size==2?3:4;CHECK(publish(size,size*(x+size>64?64:size),0));
 }
 /* Dirty spans retain their clean gaps and revert history intentionally. */
 for(unsigned y=0;y<32;y++){
  uint16_t old=model[y*64];CHECK(set(0,y,old^1));CHECK(set(0,y,old));CHECK(set(63,y,model[y*64+63]^1));kind=5;CHECK(publish(1,64,0));
 }
 /* Every nonzero/zero bitmap byte at every row-byte position, including
  * discontiguous dirty rows. This exercises the native table and byte skips
  * independently of the ordinary contiguous 2x2/4x4 cases. */
 for(unsigned group=0;group<4;group++)for(unsigned mask=0;mask<256;mask++){
  unsigned rows=0;
  for(unsigned bit=0;bit<8;bit++)if(mask&(1u<<bit)){
   unsigned y=group*8+bit,x=(mask+bit*9)&63;
   CHECK(set(x,y,model[y*64+x]^0x7777));rows++;
  }
  kind=8;CHECK(publish(rows,rows,0));
 }
 /* All changed-target transitions force a full plane, even with zero dirty. */
 for(unsigned base=0;base<65536;base+=8192){CHECK(willowNativePlaneTargetInit(&target,base));seedVRAM(base);kind=6;CHECK(publish(32,2048,0));}
 kind=1;CHECK(publish(0,0,1));CHECK(set(31,31,model[2015]^7));kind=2;CHECK(publish(1,1,1));
 CHECK(refuse(0,&target,&frameState));CHECK(refuse(&guarded.surface,0,&frameState));CHECK(refuse(&guarded.surface,&target,0));
 uint32_t oldCommand=target.command;target.command^=0x10000;CHECK(refuse(&guarded.surface,&target,&frameState));target.command=oldCommand;
 target.base++;CHECK(refuse(&guarded.surface,&target,&frameState));target.base--;
 frameState.ready=3;CHECK(refuse(&guarded.surface,&target,&frameState));frameState.ready=7;
 frameState.generation++;CHECK(refuse(&guarded.surface,&target,&frameState));frameState.generation--;
 frameState.lease=&otherLease;CHECK(refuse(&guarded.surface,&target,&frameState));frameState.lease=&lease;
 epoch++;CHECK(refuse(&guarded.surface,&target,&frameState));CHECK(invalidSet(0,0));CHECK(invalidBlock(blockWords,0,0,2,2,2));epoch--;
 /* Explicit corrupt late row proves preflight before ANY VDP store. */
 for(unsigned y=0;y<32;y++){
  guarded.surface.dirty=(1UL<<0)|(1UL<<y);guarded.surface.first[0]=0;guarded.surface.last[0]=63;
  guarded.surface.first[y]=63;guarded.surface.last[y]=62;CHECK(refuse(&guarded.surface,&target,&frameState));
  guarded.surface.last[y]=64;CHECK(refuse(&guarded.surface,&target,&frameState));guarded.surface.dirty=0;
 }
 CHECK(invalidSet(64,0));CHECK(invalidSet(0,32));CHECK(invalidSet(65535,65535));
 CHECK(invalidBlock(0,0,0,2,2,2));CHECK(invalidBlock(blockWords,64,0,2,2,2));CHECK(invalidBlock(blockWords,0,32,2,2,2));
 CHECK(invalidBlock(blockWords,0,0,0,2,2));CHECK(invalidBlock(blockWords,0,0,2,0,2));CHECK(invalidBlock(blockWords,0,0,65,2,65));
 CHECK(invalidBlock(blockWords,0,0,2,33,2));CHECK(invalidBlock(blockWords,0,0,4,4,3));
 /* Same lease storage rebound across identity/domain at the same generation. */
 epoch++;lease.plan=&otherPlan;lease.epoch=&otherEpoch;
 CHECK(!willowNativePlaneCurrent(&guarded.surface));CHECK(refuse(&guarded.surface,&target,&frameState));CHECK(invalidSet(0,0));
 lease.plan=&plan;CHECK(!willowNativePlaneCurrent(&guarded.surface));CHECK(refuse(&guarded.surface,&target,&frameState));
 lease.epoch=&epoch;epoch--;lease.plan=&otherPlan;CHECK(!willowNativePlaneCurrent(&guarded.surface));CHECK(refuse(&guarded.surface,&target,&frameState));
 lease.plan=&plan;CHECK(willowNativePlaneCurrent(&guarded.surface));
 lease.plan=&noBG;CHECK(!willowNativePlaneCurrent(&guarded.surface));CHECK(refuse(&guarded.surface,&target,&frameState));
 CHECK(!willowNativePlaneLoad(&guarded.surface,&lease,initial));CHECK(!willowNativePlaneCurrent(&guarded.surface));lease.plan=&plan;
 CHECK(!willowNativePlaneLoad(0,&lease,initial));CHECK(!willowNativePlaneLoad(&guarded.surface,0,initial));
 for(unsigned n=0;n<2048;n++)model[n]=initial[n];
 CHECK(willowNativePlaneLoad(&guarded.surface,&lease,initial));kind=0;CHECK(publish(32,2048,0));
 return 1;
}
