/* Actual68000 publisher proof, including zero/80-entry and two word alignments.
 * Native CPU writes and reads VDP memory itself. Host does not patch state. */
#include "willow_native_sat_publish.h"
static uint16_t words[322],colors[64];
static volatile uint32_t epoch;
static WillowPresentationLease lease;
static WillowPresentationFrame frame;
static const WillowPresentationPlan plan={colors,0,0,7};
static WillowNativeSatTarget target;
static unsigned cases;
static void failure(uint16_t count,uint16_t alignment,uint16_t at,uint16_t actual,uint16_t expected)
{
 *(volatile uint16_t*)0xff0010=count;
 *(volatile uint16_t*)0xff0012=alignment;
 *(volatile uint16_t*)0xff0014=at;
 *(volatile uint16_t*)0xff0016=actual;
 *(volatile uint16_t*)0xff0018=expected;
}
static uint8_t verify(uint16_t count,uint16_t alignment)
{
 *(volatile uint32_t*)0xc00004=0x33f80003UL;
 for(unsigned n=0;n<328;n++){
  uint16_t expected=0x5aa5;
  unsigned length=count?count*4u:4u;
  if(n>=4&&n<4+length)expected=count?words[n-4+alignment]:0;
  uint16_t actual=*(volatile uint16_t*)0xc00000;
  if(actual!=expected){failure(count,alignment,n,actual,expected);return 0;}
 }
 return 1;
}
uint16_t nativeObjectPresentation(void)
{
 cases=0;epoch=77;lease.plan=&plan;lease.epoch=&epoch;lease.generation=77;
 frame.lease=&lease;frame.generation=77;frame.ready=7;
 for(unsigned n=0;n<322;n++)words[n]=(uint16_t)(n*5917u+127);
 if(!willowNativeSatTargetPrepare(&target,0xf400,80))return 0;
 for(uint16_t count=0;count<=80;count++)for(uint16_t alignment=0;alignment<2;alignment++){
  *(volatile uint32_t*)0xc00004=0x73f80003UL;
  for(unsigned n=0;n<328;n++)*(volatile uint16_t*)0xc00000=0x5aa5;
  if(!willowNativeSatPublish(&target,words+alignment,count,&frame)||!verify(count,alignment))return 0;
  if(willowNativeSatPublish(&target,words,81,&frame)||
     willowNativeSatPublish(&target,0,1,&frame))return 0;
  frame.ready=3;if(willowNativeSatPublish(&target,words,80,&frame))return 0;
  frame.ready=7;epoch++;if(willowNativeSatPublish(&target,words,80,&frame))return 0;
  epoch--;
  if(!verify(count,alignment))return 0;
  cases++;
 }
 *(volatile uint32_t*)0xff0008=cases;
 return 1;
}
