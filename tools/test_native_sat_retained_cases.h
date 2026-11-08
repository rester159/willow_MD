/* Shared adversarial workload. Expected VRAM is a complete independent word
 * surface, updated for every used input word regardless of dirty-run choice. */
#include <stddef.h>
#include "willow_native_sat_retained.h"
static uint16_t testWords[322],testColors[64],testExpected[328];
static volatile uint32_t testEpoch,testOtherEpoch;
static const WillowPresentationPlan testPlan={testColors,0,0,7},testOtherPlan={testColors,0,0,7};
static WillowPresentationLease testLease;
static WillowPresentationFrame testFrame;
static WillowNativeSatTarget testTarget;
static struct {uint16_t before;WillowNativeSatRetained value;uint16_t after;} testGuard;
static WillowNativeSatRetained testSaved;
static WillowNativeSatRetainedStats testStats,testSavedStats;
static uint16_t testBase,testKind;
static unsigned testCases,testRefusals;
#define CHECK(x) do{if(!(x)){testFail(__LINE__,0,0);return 0;}}while(0)
#define EQUAL(x,y) do{uint32_t a_=(x),b_=(y);if(a_!=b_){testFail(__LINE__,a_,b_);return 0;}}while(0)
static void testCopy(void*d,const void*s,unsigned bytes)
{uint8_t*to=d;const uint8_t*from=s;for(unsigned n=0;n<bytes;n++)to[n]=from[n];}
static uint8_t testSame(const void*a,const void*b,unsigned bytes)
{const uint8_t*x=a,*y=b;for(unsigned n=0;n<bytes;n++)if(x[n]!=y[n])return 0;return 1;}
static uint16_t testRandom(uint16_t*seed)
{uint16_t v=*seed;v^=v<<7;v^=v>>9;v^=v<<8;*seed=v;return v;}
static uint16_t testMod(uint16_t value,uint16_t divisor)
{while(value>=divisor)value-=divisor;return value;}
static uint8_t testReadback(void)
{
 testReadBegin(testBase-8);
 for(unsigned n=0;n<328;n++)EQUAL(testReadWord(),testExpected[n]);
 EQUAL(testGuard.before,0x1369);EQUAL(testGuard.after,0x9753);
 return 1;
}
static uint8_t testInit(uint16_t base,uint16_t capacity)
{
 testBase=base;testEpoch=77;testOtherEpoch=77;
 testLease.plan=&testPlan;testLease.epoch=&testEpoch;testLease.generation=77;
 testFrame.lease=&testLease;testFrame.generation=77;testFrame.ready=7;
 CHECK(willowNativeSatTargetPrepare(&testTarget,base,capacity));
 uint8_t*p=(uint8_t*)&testGuard;for(unsigned n=0;n<sizeof testGuard;n++)p[n]=0;
 testGuard.before=0x1369;testGuard.after=0x9753;
 testWriteBegin(base-8);
 for(unsigned n=0;n<328;n++){testExpected[n]=(uint16_t)(0x5aa5+n*73u);testWriteWord(testExpected[n]);}
 for(unsigned n=0;n<322;n++)testWords[n]=(uint16_t)(127+n*5917u);
 return 1;
}
static uint8_t testPublish(uint16_t count,uint16_t alignment,int dirty,int runs,int full)
{
 testCopy(&testSaved,&testGuard.value,sizeof testSaved);
 testStats.dirtyEntries=testStats.transferRuns=testStats.dataWords=testStats.fullUpload=0xffff;
 testTimingBegin(testKind,count);
 uint8_t ok=willowNativeSatPublishRetained(&testTarget,count?testWords+alignment:0,count,&testFrame,&testGuard.value,&testStats);
 testTimingEnd(&testStats);
 CHECK(ok);
 if(dirty>=0)EQUAL(testStats.dirtyEntries,(unsigned)dirty);
 if(runs>=0)EQUAL(testStats.transferRuns,(unsigned)runs);
 if(full>=0)EQUAL(testStats.fullUpload,(unsigned)full);
 EQUAL(testStats.dataWords,testStats.dirtyEntries*4u);
 CHECK(testStats.dirtyEntries<=(count?count:1));
 CHECK(testStats.transferRuns<=testStats.dirtyEntries);
 uint16_t used=count?count*4:4;
 for(unsigned n=0;n<used;n++){
  uint16_t expected=count?testWords[alignment+n]:0;
  testExpected[n+4]=expected;EQUAL(testGuard.value.words[n],expected);
 }
 for(unsigned n=used;n<320;n++)EQUAL(testGuard.value.words[n],testSaved.words[n]);
 EQUAL(testGuard.value.ready,1);EQUAL(testGuard.value.count,count);
 EQUAL(testGuard.value.capacity,testTarget.capacity);EQUAL(testGuard.value.command,testTarget.command);
 EQUAL(testGuard.value.generation,testFrame.generation);
 CHECK(testGuard.value.plan==testLease.plan&&testGuard.value.epoch==testLease.epoch);
 CHECK(testReadback());testCases++;return 1;
}
static uint8_t testRefuse(const WillowNativeSatTarget*t,const uint16_t*w,uint16_t n,
 const WillowPresentationFrame*f,WillowNativeSatRetained*s)
{
 testCopy(&testSaved,&testGuard.value,sizeof testSaved);
 testStats.dirtyEntries=0x1357;testStats.transferRuns=0x2468;
 testStats.dataWords=0x369a;testStats.fullUpload=0xaaaa;testSavedStats=testStats;
 testTimingBegin(9,n);
 uint8_t ok=willowNativeSatPublishRetained(t,w,n,f,s,&testStats);
 testTimingRefused();
 CHECK(!ok);CHECK(testSame(&testSaved,&testGuard.value,sizeof testSaved));
 CHECK(testSame(&testSavedStats,&testStats,sizeof testStats));
 CHECK(testReadback());testRefusals++;return 1;
}
static uint8_t testRun(void)
{
 testCases=testRefusals=0;testKind=0;
 /* All physical lengths and two valid word alignments. */
 for(uint16_t count=0;count<=80;count++)for(uint16_t alignment=0;alignment<2;alignment++){
  CHECK(testInit(0xf400,80));testKind=1;
  CHECK(testPublish(count,alignment,count?count:1,1,1));testKind=2;
  CHECK(testPublish(count,alignment,0,0,0));
 }
 /* Every capacity, grows from empty into unknown zero shadow/dirty VRAM. */
 for(uint16_t capacity=1;capacity<=80;capacity++){
  CHECK(testInit(0xf400,capacity));testKind=3;
  for(unsigned n=0;n<320;n++)testWords[n]=0;
  CHECK(testPublish(0,0,1,1,1));
  CHECK(testPublish(capacity,0,capacity-1,capacity>1?1:0,0));
  CHECK(testPublish(capacity,0,0,0,0));
 }
 CHECK(testInit(0x3f80,80));testKind=4; /* Run crosses command16KiB boundary. */
 CHECK(testPublish(34,0,34,1,1));
 testWords[3]^=1;CHECK(testPublish(34,0,1,1,0));
 testWords[5*4+1]^=1;testWords[6*4+2]^=1;testWords[7*4+3]^=1;
 CHECK(testPublish(34,0,3,1,0));
 testWords[0]^=1;testWords[16*4]^=1;testWords[33*4+3]^=1;
 CHECK(testPublish(34,0,3,3,0));
 /* Changed final link terminates shrink. No stale tail writes are needed. */
 testWords[2*4+1]=0;CHECK(testPublish(3,0,1,1,0));
 CHECK(testPublish(3,0,0,0,0));
 for(unsigned n=12;n<20;n++)testWords[n]=0;
 CHECK(testPublish(5,0,2,1,0));CHECK(testPublish(0,0,1,1,0));
 CHECK(testPublish(0,0,0,0,0));CHECK(testPublish(1,0,1,1,0));
 /* New generation, identity, target and explicit invalidation all reload. */
 testKind=5;testEpoch++;testLease.generation++;testFrame.generation++;
 CHECK(testPublish(5,0,5,1,1));testLease.plan=&testOtherPlan;
 CHECK(testPublish(5,0,5,1,1));testOtherEpoch=testEpoch;testLease.epoch=&testOtherEpoch;
 CHECK(testPublish(5,0,5,1,1));testTarget.capacity=79;
 CHECK(testPublish(5,0,5,1,1));willowNativeSatRetainedInvalidate(&testGuard.value);
 CHECK(testPublish(5,0,5,1,1));
 testGuard.value.count=80;CHECK(testPublish(5,0,5,1,1));
 /* Alternate physical table retains no authority over the previous base. */
 CHECK(willowNativeSatTargetPrepare(&testTarget,0xf400,80));testBase=0xf400;
 testWriteBegin(testBase-8);for(unsigned n=0;n<328;n++){testExpected[n]=0xa55a;testWriteWord(0xa55a);}
 CHECK(testPublish(5,0,5,1,1));
 /* Guard failures must be side-effect free, including output stats. */
 WillowNativeSatTarget saved=testTarget;
 CHECK(testRefuse(0,testWords,5,&testFrame,&testGuard.value));
 CHECK(testRefuse(&testTarget,testWords,5,&testFrame,0));
 CHECK(testRefuse(&testTarget,testWords,5,0,&testGuard.value));
 CHECK(testRefuse(&testTarget,0,1,&testFrame,&testGuard.value));
 CHECK(testRefuse(&testTarget,testWords,81,&testFrame,&testGuard.value));
 testTarget.capacity=0;CHECK(testRefuse(&testTarget,testWords,0,&testFrame,&testGuard.value));
 testTarget.capacity=81;CHECK(testRefuse(&testTarget,testWords,5,&testFrame,&testGuard.value));testTarget=saved;
 testTarget.command^=0x80000000;CHECK(testRefuse(&testTarget,testWords,5,&testFrame,&testGuard.value));testTarget=saved;
 testTarget.command^=0x00020000;CHECK(testRefuse(&testTarget,testWords,5,&testFrame,&testGuard.value));testTarget=saved;
 testTarget.command=0x7f000003;CHECK(testRefuse(&testTarget,testWords,5,&testFrame,&testGuard.value));testTarget=saved;
 testFrame.ready=3;CHECK(testRefuse(&testTarget,testWords,5,&testFrame,&testGuard.value));testFrame.ready=7;
 testFrame.generation++;CHECK(testRefuse(&testTarget,testWords,5,&testFrame,&testGuard.value));testFrame.generation--;
 testOtherEpoch++;CHECK(testRefuse(&testTarget,testWords,5,&testFrame,&testGuard.value));testOtherEpoch--;
 const WillowPresentationLease*l=testFrame.lease;testFrame.lease=0;
 CHECK(testRefuse(&testTarget,testWords,5,&testFrame,&testGuard.value));testFrame.lease=l;
 const volatile uint32_t*e=testLease.epoch;testLease.epoch=0;
 CHECK(testRefuse(&testTarget,testWords,5,&testFrame,&testGuard.value));testLease.epoch=e;
 const WillowPresentationPlan*p=testLease.plan;testLease.plan=0;
 CHECK(testRefuse(&testTarget,testWords,5,&testFrame,&testGuard.value));testLease.plan=p;
 /* Fixed-seed mixed growth/shrink/mutations. Input includes arbitrary SAT bits;
  * this transfer primitive deliberately does not infer geometry/link validity. */
 CHECK(testInit(0xf400,80));uint16_t random=0x39ab,phase=0;
 testKind=6;CHECK(testPublish(34,0,34,1,1));
 for(unsigned step=0;step<512;step++){
  uint16_t count=testMod(testRandom(&random),81);
  for(unsigned j=0;j<(random&7);j++){
   uint16_t at=testMod(testRandom(&random),320);testWords[at]^=(uint16_t)(random|1);
  }
  if(!phase){testEpoch++;testLease.generation++;testFrame.generation++;}
  CHECK(testPublish(count,0,-1,-1,!phase));
  CHECK(testPublish(count,0,0,0,0));
  if(++phase==31)phase=0;
 }
 /* Stable representative34-entry stage and dirty one/all for cost comparison. */
 CHECK(testInit(0xf400,80));testKind=7;CHECK(testPublish(34,0,34,1,1));
 for(unsigned step=0;step<32;step++){
  testKind=10;CHECK(testPublish(34,0,0,0,0));
  testKind=11;testWords[step*4]^=1;CHECK(testPublish(34,0,1,1,0));
  testKind=12;for(unsigned n=0;n<34;n++)testWords[n*4]^=1;
  CHECK(testPublish(34,0,34,1,0));
 }
 /* Nullable statistics is supported without changing admission. */
 CHECK(willowNativeSatPublishRetained(&testTarget,testWords,34,&testFrame,&testGuard.value,0));
 CHECK(testReadback());
 return 1;
}
