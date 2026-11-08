#ifndef TEST_NATIVE_BACKGROUND_JOURNAL_CASES_H
#define TEST_NATIVE_BACKGROUND_JOURNAL_CASES_H
#include <stddef.h>
#include "willow_native_background_journal.h"

/* This is an API proof, not simulated source behavior. Source writer hooks and
 * complete-view baseline certification are separately owned integration work. */
static struct {
 uint8_t before[16];
 WillowNativeBackgroundJournal j;
 uint8_t after[16];
} guarded;
static WillowNativeBackgroundJournalView view,stale;
static uint8_t saved[sizeof guarded];
static uint32_t checks,cases,digest;
static volatile uint16_t benchPair[6];
#define J (&guarded.j)
#define CHECK(x) do {checks++;if(!(x)){testFail(__LINE__,0,1);return 0;}}while(0)
#define EQ(x,y) do {uint32_t aa=(uint32_t)(x),bb=(uint32_t)(y);checks++;if(aa!=bb){testFail(__LINE__,aa,bb);return 0;}}while(0)
static void bytesFill(void*p,uint8_t v,unsigned n){uint8_t*q=p;while(n--)*q++=v;}
static void saveJournal(void){const uint8_t*p=(const uint8_t*)&guarded;for(unsigned i=0;i<sizeof guarded;i++)saved[i]=p[i];}
static uint8_t savedEqual(void){const uint8_t*p=(const uint8_t*)&guarded;for(unsigned i=0;i<sizeof guarded;i++)if(saved[i]!=p[i])return 0;return 1;}
static uint8_t canaries(void){for(unsigned i=0;i<16;i++)if(guarded.before[i]!=0xa5||guarded.after[i]!=0xa5)return 0;return 1;}
static void fresh(void){bytesFill(&guarded,0xa5,sizeof guarded);willowNativeBackgroundJournalInit(J);}
static uint8_t seed(uint32_t epoch){fresh();CHECK(willowNativeBackgroundJournalBaseline(J,epoch));return 1;}
static uint8_t failed(uint16_t fault){EQ(J->state,WILLOW_BACKGROUND_JOURNAL_INVALID);EQ(J->fault,fault);CHECK(!willowNativeBackgroundJournalAcquire(J,&view));CHECK(canaries());return 1;}
static uint8_t entry(unsigned i,uint16_t key,uint16_t code,uint16_t attr){
 CHECK(i<view.count);EQ(view.entries[i].key,key);EQ(view.entries[i].code,code);EQ(view.entries[i].attribute,attr);
 digest=(digest<<5)|(digest>>27);digest^=((uint32_t)key<<16)^code^((uint32_t)attr<<1);return 1;
}
static uint8_t testAllCells(void){
 CHECK(seed(1));
 for(unsigned layer=2;layer<=3;layer++)for(unsigned cell=0;cell<4096;cell++){
  cases++;
  uint32_t epoch=2+(layer-2)*4096+cell;
  uint16_t beforeCode=(uint16_t)(cell*73+layer),beforeAttr=(uint16_t)(cell*257+0x8000);
  uint16_t code=(uint16_t)(beforeCode^0xa55a),attr=(uint16_t)(beforeAttr^0xffff);
  CHECK(willowNativeBackgroundJournalBegin(J,epoch));
  CHECK(!willowNativeBackgroundJournalAcquire(J,&view));
  CHECK(willowNativeBackgroundJournalRecord(J,layer,cell,beforeCode,beforeAttr,code,attr));
  CHECK(!willowNativeBackgroundJournalAcquire(J,&view));
  CHECK(willowNativeBackgroundJournalCommit(J,epoch));
  CHECK(willowNativeBackgroundJournalAcquire(J,&view));EQ(view.count,1);EQ(view.epoch,epoch);
  CHECK(entry(0,(uint16_t)(((layer-2)<<12)|cell),code,attr));
  CHECK(willowNativeBackgroundJournalAcknowledge(J,&view));EQ(J->count,0);
  CHECK(!willowNativeBackgroundJournalViewCurrent(&view));CHECK(canaries());
 }
 return 1;
}
static uint8_t testPending(void){
 cases++;CHECK(seed(20));CHECK(willowNativeBackgroundJournalBegin(J,21));
 for(unsigned i=0;i<64;i++){
  CHECK(willowNativeBackgroundJournalRecord(J,2+(i&1),i,0,0,i+1,(uint16_t)(0x8000|i)));
  CHECK(!willowNativeBackgroundJournalAcquire(J,&view));
 }
 EQ(J->count,64);
 /* Full journal permits no-op unknown cells and coalesces known cells. */
 CHECK(willowNativeBackgroundJournalRecord(J,3,4095,0xbeef,0xffed,0xbeef,0xffed));EQ(J->count,64);
 CHECK(willowNativeBackgroundJournalRecord(J,2,0,1,0x8000,0,0));
 CHECK(willowNativeBackgroundJournalRecord(J,3,63,64,0x803f,0xabcd,0xffff));
 CHECK(willowNativeBackgroundJournalCommit(J,21));CHECK(willowNativeBackgroundJournalAcquire(J,&view));stale=view;
 for(unsigned i=0;i<64;i++)CHECK(entry(i,(uint16_t)(((i&1)<<12)|i),i==0?0:i==63?0xabcd:i+1,i==0?0:i==63?0xffff:0x8000|i));
 /* Resolution refused: no ack; next complete controller epoch retains all. */
 CHECK(willowNativeBackgroundJournalBegin(J,22));CHECK(!willowNativeBackgroundJournalViewCurrent(&stale));
 CHECK(willowNativeBackgroundJournalRecord(J,3,63,0xabcd,0xffff,0x1234,0x0100));
 CHECK(willowNativeBackgroundJournalCommit(J,22));EQ(J->count,64);
 saveJournal();CHECK(!willowNativeBackgroundJournalAcknowledge(J,&stale));CHECK(savedEqual());
 CHECK(willowNativeBackgroundJournalAcquire(J,&view));CHECK(entry(63,0x103f,0x1234,0x0100));
 CHECK(willowNativeBackgroundJournalAcknowledge(J,&view));EQ(J->count,0);
 CHECK(willowNativeBackgroundJournalAcquire(J,&view));EQ(view.count,0);
 CHECK(willowNativeBackgroundJournalAcknowledge(J,&view));CHECK(willowNativeBackgroundJournalAcknowledge(J,&view));
 CHECK(canaries());return 1;
}
static uint8_t testFailures(void){
 cases++;fresh();EQ(J->state,WILLOW_BACKGROUND_JOURNAL_COLD);CHECK(!willowNativeBackgroundJournalAcquire(J,&view));
 CHECK(!willowNativeBackgroundJournalBegin(J,1));CHECK(failed(WILLOW_BACKGROUND_JOURNAL_NO_BASELINE));
 CHECK(willowNativeBackgroundJournalBaseline(J,1));
 for(unsigned which=0;which<9;which++){
  cases++;CHECK(seed(10));
  switch(which){
   case 0:CHECK(!willowNativeBackgroundJournalBegin(J,0));break;
   case 1:CHECK(!willowNativeBackgroundJournalBegin(J,9));break;
   case 2:CHECK(!willowNativeBackgroundJournalBegin(J,10));break;
   case 3:CHECK(willowNativeBackgroundJournalBegin(J,11));CHECK(!willowNativeBackgroundJournalCommit(J,0));break;
   case 4:CHECK(willowNativeBackgroundJournalBegin(J,11));CHECK(!willowNativeBackgroundJournalCommit(J,10));break;
   case 5:CHECK(willowNativeBackgroundJournalBegin(J,11));CHECK(!willowNativeBackgroundJournalCommit(J,12));break;
   case 6:CHECK(!willowNativeBackgroundJournalBaseline(J,0));break;
   case 7:CHECK(!willowNativeBackgroundJournalBaseline(J,9));break;
   default:CHECK(!willowNativeBackgroundJournalBaseline(J,10));break;
  }
  CHECK(failed(WILLOW_BACKGROUND_JOURNAL_BAD_EPOCH));
  CHECK(willowNativeBackgroundJournalBaseline(J,12));CHECK(willowNativeBackgroundJournalAcquire(J,&view));EQ(view.count,0);
 }
 cases++;CHECK(seed(0xfffffffeu));CHECK(willowNativeBackgroundJournalBegin(J,0xffffffffu));CHECK(willowNativeBackgroundJournalCommit(J,0xffffffffu));
 CHECK(!willowNativeBackgroundJournalBegin(J,0));CHECK(failed(WILLOW_BACKGROUND_JOURNAL_BAD_EPOCH));CHECK(!willowNativeBackgroundJournalBaseline(J,1));
 for(unsigned which=0;which<5;which++){
  cases++;CHECK(seed(10));
  switch(which){
   case 0:CHECK(willowNativeBackgroundJournalBegin(J,11));CHECK(!willowNativeBackgroundJournalBegin(J,12));break;
   case 1:CHECK(!willowNativeBackgroundJournalCommit(J,11));break;
   case 2:CHECK(!willowNativeBackgroundJournalRecord(J,2,0,0,0,1,1));break;
   case 3:CHECK(willowNativeBackgroundJournalBegin(J,11));J->openEpoch=0;CHECK(!willowNativeBackgroundJournalRecord(J,2,0,0,0,1,1));break;
   default:willowNativeBackgroundJournalInvalidate(J);CHECK(!willowNativeBackgroundJournalBegin(J,11));break;
  }
  CHECK(failed(WILLOW_BACKGROUND_JOURNAL_BAD_STATE));
 }
 for(unsigned which=0;which<6;which++){
  static const uint16_t layers[]={0,1,4,65535,2,3};
  static const uint16_t cells[]={0,4095,0,0,4096,65535};
  cases++;CHECK(seed(1));CHECK(willowNativeBackgroundJournalBegin(J,2));
  CHECK(!willowNativeBackgroundJournalRecord(J,layers[which],cells[which],0,0,1,1));CHECK(failed(WILLOW_BACKGROUND_JOURNAL_BAD_PAIR));
 }
 cases++;CHECK(seed(1));CHECK(willowNativeBackgroundJournalBegin(J,2));
 for(unsigned i=0;i<64;i++)CHECK(willowNativeBackgroundJournalRecord(J,2,i,0,0,i+1,i+1));
 CHECK(!willowNativeBackgroundJournalRecord(J,3,4095,0,0,1,1));EQ(J->count,64);CHECK(failed(WILLOW_BACKGROUND_JOURNAL_OVERFLOW));
 CHECK(!willowNativeBackgroundJournalCommit(J,2));CHECK(willowNativeBackgroundJournalBaseline(J,3));EQ(J->count,0);
 for(unsigned which=0;which<3;which++){
  cases++;CHECK(seed(1));CHECK(willowNativeBackgroundJournalBegin(J,2));CHECK(willowNativeBackgroundJournalRecord(J,2,1,0,0,0xeeee,0xaaaa));
  uint16_t bc=which==0?0xefff:0xeeee,ba=which==0?0xaaaa:0xaaab;
  CHECK(!willowNativeBackgroundJournalRecord(J,2,1,bc,ba,which==2?bc:1,which==2?ba:1));CHECK(failed(WILLOW_BACKGROUND_JOURNAL_BAD_PREDECESSOR));
 }
 for(unsigned which=0;which<3;which++){
  cases++;CHECK(seed(1));if(which)CHECK(willowNativeBackgroundJournalBegin(J,2));J->count=65;
  if(!which)CHECK(!willowNativeBackgroundJournalBegin(J,2));
  else if(which==1)CHECK(!willowNativeBackgroundJournalRecord(J,2,0,0,0,1,1));
  else CHECK(!willowNativeBackgroundJournalCommit(J,2));
  CHECK(failed(WILLOW_BACKGROUND_JOURNAL_CORRUPT));
 }
 cases++;CHECK(seed(1));CHECK(willowNativeBackgroundJournalAcquire(J,&stale));willowNativeBackgroundJournalInvalidate(J);
 CHECK(failed(WILLOW_BACKGROUND_JOURNAL_UNTRACKED_WRITE));saveJournal();CHECK(!willowNativeBackgroundJournalAcknowledge(J,&stale));CHECK(savedEqual());
 CHECK(willowNativeBackgroundJournalBaseline(J,2));CHECK(!willowNativeBackgroundJournalViewCurrent(&stale));
 cases++;CHECK(seed(1));CHECK(willowNativeBackgroundJournalBegin(J,2));CHECK(willowNativeBackgroundJournalRecord(J,2,0,0,0,1,1));
 CHECK(willowNativeBackgroundJournalCommit(J,2));CHECK(willowNativeBackgroundJournalAcquire(J,&view));
 for(unsigned which=0;which<5;which++){
  stale=view;
  if(which==0)stale.entries++;
  else if(which==1)stale.epoch++;
  else if(which==2)stale.count++;
  else if(which==3)stale.journal=0;
  else stale.entries=0;
  saveJournal();CHECK(!willowNativeBackgroundJournalViewCurrent(&stale));CHECK(!willowNativeBackgroundJournalAcknowledge(J,&stale));CHECK(savedEqual());
 }
 cases++;CHECK(seed(1));CHECK(willowNativeBackgroundJournalAcquire(J,&stale));J->count=65;
 CHECK(!willowNativeBackgroundJournalAcquire(J,&view));CHECK(!willowNativeBackgroundJournalViewCurrent(&stale));
 cases++;CHECK(seed(1));J->openEpoch=2;CHECK(!willowNativeBackgroundJournalAcquire(J,&view));
 cases++;willowNativeBackgroundJournalInit(0);willowNativeBackgroundJournalInvalidate(0);
 CHECK(!willowNativeBackgroundJournalBaseline(0,1));CHECK(!willowNativeBackgroundJournalBegin(0,1));CHECK(!willowNativeBackgroundJournalRecord(0,2,0,0,0,1,1));
 CHECK(!willowNativeBackgroundJournalCommit(0,1));CHECK(!willowNativeBackgroundJournalAcquire(0,&view));CHECK(!willowNativeBackgroundJournalAcquire(J,0));
 CHECK(!willowNativeBackgroundJournalViewCurrent(0));CHECK(!willowNativeBackgroundJournalAcknowledge(0,&view));CHECK(!willowNativeBackgroundJournalAcknowledge(J,0));
 CHECK(canaries());return 1;
}
static uint8_t testPairBits(void){
 /* Independently flip every bit of each source word, including high attributes. */
 for(unsigned which=0;which<32;which++){
  cases++;CHECK(seed(1));CHECK(willowNativeBackgroundJournalBegin(J,2));
  uint16_t c=0xaaaa,a=0x5555;if(which<16)c^=(uint16_t)(1u<<which);else a^=(uint16_t)(1u<<(which-16));
  CHECK(willowNativeBackgroundPairChanged(0xaaaa,0x5555,c,a));
  CHECK(willowNativeBackgroundJournalRecord(J,3,4095,0xaaaa,0x5555,c,a));CHECK(willowNativeBackgroundJournalCommit(J,2));
  CHECK(willowNativeBackgroundJournalAcquire(J,&view));EQ(view.count,1);CHECK(entry(0,0x1fff,c,a));
 }
 cases++;CHECK(seed(1));CHECK(willowNativeBackgroundJournalBegin(J,2));
 for(unsigned i=0;i<8192;i++){
  uint16_t c=(uint16_t)(i*17),a=(uint16_t)(i*31);CHECK(!willowNativeBackgroundPairChanged(c,a,c,a));
  CHECK(willowNativeBackgroundJournalRecord(J,2+(i>>12),i&4095,c,a,c,a));
 }
 EQ(J->count,0);CHECK(willowNativeBackgroundJournalCommit(J,2));CHECK(willowNativeBackgroundJournalAcquire(J,&view));EQ(view.count,0);CHECK(canaries());return 1;
}
/* noinline keeps each measured API case a real native call across markers. The
 * tiny PairChanged helper is inline exactly as at a source-store hook. */
static __attribute__((noinline)) uint16_t benchRecord(void){return willowNativeBackgroundJournalRecord(J,benchPair[0],benchPair[1],benchPair[2],benchPair[3],benchPair[4],benchPair[5]);}
static __attribute__((noinline)) uint16_t benchEmpty(void){return willowNativeBackgroundJournalBegin(J,2)&&willowNativeBackgroundJournalCommit(J,2);}
static __attribute__((noinline)) uint16_t benchAcknowledge(void){return willowNativeBackgroundJournalAcquire(J,&view)&&willowNativeBackgroundJournalAcknowledge(J,&view);}
static __attribute__((noinline)) uint16_t benchColumn(unsigned n){
 if(!willowNativeBackgroundJournalBegin(J,2))return 0;
 for(unsigned i=0;i<n;i++)if(!willowNativeBackgroundJournalRecord(J,2,(uint16_t)(i*64),0,0,i+1,(uint16_t)(0x8000|i)))return 0;
 return willowNativeBackgroundJournalCommit(J,2);
}
static uint8_t testBenchmarks(void){
 for(unsigned kind=0;kind<11;kind++)for(unsigned repeat=0;repeat<64;repeat++){
  uint16_t result;CHECK(seed(1));
  benchPair[0]=2;benchPair[1]=0;benchPair[2]=0;benchPair[3]=0;benchPair[4]=1;benchPair[5]=0x8000;
  if(kind==0){benchPair[2]=1;benchPair[3]=0x8000;}
  if(kind>=3&&kind<=8){
   CHECK(willowNativeBackgroundJournalBegin(J,2));
   unsigned n=kind==4?13:kind==5?63:kind>=6?64:0;
   for(unsigned i=0;i<n;i++)CHECK(willowNativeBackgroundJournalRecord(J,2,i,0,0,i+1,(uint16_t)(0x8000|i)));
   benchPair[1]=(uint16_t)n;
   if(kind==6||kind==7){unsigned i=kind==6?0:63;benchPair[1]=i;benchPair[2]=i+1;benchPair[3]=(uint16_t)(0x8000|i);benchPair[4]=0xfedc;benchPair[5]=0x4321;}
   if(kind==8)CHECK(willowNativeBackgroundJournalCommit(J,2));
  }
  testTimingBegin(kind);
  if(kind<2)result=willowNativeBackgroundPairChanged(benchPair[2],benchPair[3],benchPair[4],benchPair[5]);
  else if(kind==2)result=benchEmpty();
  else if(kind<=7)result=benchRecord();
  else if(kind==8)result=benchAcknowledge();
  else result=benchColumn(kind==9?14:52);
  testTimingEnd();EQ(result,kind==0?0:1);CHECK(canaries());
 }
 return 1;
}
#ifdef WILLOW_JOURNAL_ROUTE_FIXTURE
#include "native_background_journal_route.h"
static __attribute__((noinline)) uint16_t routeEpoch(unsigned e,uint8_t skipUnchanged){
 if(!willowNativeBackgroundJournalBegin(J,e+2))return 0;
 for(unsigned i=routeStarts[e];i<routeStarts[e+1];i++){
  const uint16_t*p=routePairs[i];
  if(skipUnchanged&&!willowNativeBackgroundPairChanged(p[2],p[3],p[4],p[5]))continue;
  if(!willowNativeBackgroundJournalRecord(J,p[0],p[1],p[2],p[3],p[4],p[5]))return 0;
 }
 return willowNativeBackgroundJournalCommit(J,e+2);
}
static uint8_t testRoute(void){
 for(unsigned skip=0;skip<2;skip++){
  CHECK(seed(1));
  for(unsigned e=0;e<7;e++){
   cases++;uint16_t result;
   if(skip)testTimingBegin(11);
   result=routeEpoch(e,skip);
   if(skip)testTimingEnd();
   CHECK(result);CHECK(willowNativeBackgroundJournalAcquire(J,&view));EQ(view.count,routeExpectedStarts[e+1]-routeExpectedStarts[e]);
   for(unsigned i=routeExpectedStarts[e];i<routeExpectedStarts[e+1];i++){
    const uint16_t*p=routeExpected[i];CHECK(entry(i-routeExpectedStarts[e],p[0],p[1],p[2]));
   }
   CHECK(willowNativeBackgroundJournalAcknowledge(J,&view));CHECK(canaries());
  }
 }
 return 1;
}
#endif
static uint8_t testRun(void){
 checks=cases=0;digest=0x4a4f5552;
 CHECK(testAllCells());CHECK(testPending());CHECK(testFailures());CHECK(testPairBits());CHECK(testBenchmarks());
#ifdef WILLOW_JOURNAL_ROUTE_FIXTURE
 CHECK(testRoute());
#endif
 return 1;
}
#endif
