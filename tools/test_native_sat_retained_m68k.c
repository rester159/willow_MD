#include <stdint.h>
#include <stddef.h>
#include "willow_native_sat_retained.h"
typedef char checkSize[(sizeof(WillowNativeSatRetained)==662)?1:-1];
#define OFFSET(field,value) typedef char check_##field[(offsetof(WillowNativeSatRetained,field)==value)?1:-1]
OFFSET(words,0);OFFSET(generation,640);OFFSET(command,644);OFFSET(plan,648);
OFFSET(epoch,652);OFFSET(count,656);OFFSET(capacity,658);OFFSET(ready,660);
static void testFail(unsigned line,uint32_t actual,uint32_t expected)
{*(volatile uint16_t*)0xff0010=line;*(volatile uint32_t*)0xff0012=actual;*(volatile uint32_t*)0xff0016=expected;}
static void testWriteBegin(uint16_t at)
{*(volatile uint32_t*)0xc00004=0x40000000UL|(((uint32_t)at&0x3fff)<<16)|(at>>14);}
static void testWriteWord(uint16_t v){*(volatile uint16_t*)0xc00000=v;}
static void testReadBegin(uint16_t at)
{*(volatile uint32_t*)0xc00004=(((uint32_t)at&0x3fff)<<16)|(at>>14);}
static uint16_t testReadWord(void){return *(volatile uint16_t*)0xc00000;}
static void testTimingBegin(uint16_t kind,uint16_t count)
{*(volatile uint16_t*)0xff0020=kind;*(volatile uint16_t*)0xff0022=count;*(volatile uint16_t*)0xff0004=1;}
static void testTimingEnd(const WillowNativeSatRetainedStats*s)
{*(volatile uint16_t*)0xff0024=s->dataWords;*(volatile uint16_t*)0xff0026=s->transferRuns;*(volatile uint16_t*)0xff0004=2;}
static void testTimingRefused(void)
{*(volatile uint16_t*)0xff0024=0;*(volatile uint16_t*)0xff0026=0;*(volatile uint16_t*)0xff0004=2;}
#include "test_native_sat_retained_cases.h"
uint16_t nativeObjectPresentation(void)
{
 if(!testRun())return 0;
 /* Baseline native two-long upload cost, same data, frame and timing stores. */
 for(unsigned n=0;n<32;n++){
  testTimingBegin(13,34);
  if(!willowNativeSatPublish(&testTarget,testWords,34,&testFrame))return 0;
  WillowNativeSatRetainedStats s={34,1,136,1};testTimingEnd(&s);
 }
 *(volatile uint32_t*)0xff0008=testCases;*(volatile uint16_t*)0xff000c=testRefusals;
 return 1;
}
