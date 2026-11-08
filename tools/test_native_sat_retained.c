#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static uint16_t memory[32768];
static unsigned address,controls,writes,savedControls,savedWrites,activeKind,activeCount;
static void testFail(unsigned line,uint32_t actual,uint32_t expected)
{fprintf(stderr,"Retained SAT test line%u actual%u expected%u\n",line,actual,expected);}
static void control(uint32_t v){address=((v>>16)&0x3fff)|((v&3)<<14);controls++;}
static void data(uint16_t v){if(address>=65536){fprintf(stderr,"address%u\n",address);abort();}memory[address/2]=v;address+=2;writes++;}
#define WILLOW_SAT_CONTROL(v) control(v)
#define WILLOW_SAT_DATA(v) data(v)
#include "willow_native_sat_retained.h"
static void testWriteBegin(uint16_t at){address=at;}
static void testWriteWord(uint16_t v){data(v);}
static void testReadBegin(uint16_t at){address=at;}
static uint16_t testReadWord(void){uint16_t v=memory[address/2];address+=2;return v;}
static void testTimingBegin(uint16_t kind,uint16_t count)
{activeKind=kind;activeCount=count;savedControls=controls;savedWrites=writes;}
static void testTimingEnd(const WillowNativeSatRetainedStats*s)
{if(controls-savedControls!=s->transferRuns||writes-savedWrites!=s->dataWords){fprintf(stderr,"traffic kind%u count%u actual %u/%u expected %u/%u\n",activeKind,activeCount,controls-savedControls,writes-savedWrites,s->transferRuns,s->dataWords);abort();}}
static void testTimingRefused(void){if(controls!=savedControls||writes!=savedWrites){fprintf(stderr,"refused traffic %u/%u\n",controls-savedControls,writes-savedWrites);abort();}}
#include "test_native_sat_retained_cases.h"
int main(void)
{if(!testRun())return 1;printf("{\"publications\":%u,\"refusals\":%u,\"native_shadow_bytes\":662,\"readback_bytes_per_case\":656}\n",testCases,testRefusals);return 0;}
