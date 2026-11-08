#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint16_t memory[32768];
static unsigned address,controls,writes;
static void control(uint32_t value){address=((value>>16)&0x3fff)|((value&3)<<14);controls++;}
static void data(uint16_t value){memory[address/2]=value;address+=2;writes++;}
#define WILLOW_SAT_CONTROL(value) control(value)
#define WILLOW_SAT_DATA(value) data(value)
#include "willow_native_sat_publish.h"
static uint16_t colors[64];
static volatile uint32_t epoch=77;
static const WillowPresentationPlan plan={colors,0,0,7};
static WillowPresentationLease lease={&plan,&epoch,77};
static WillowPresentationFrame frame={&lease,77,7};
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"SAT publisher failure at%d\n",__LINE__);return 1;}}while(0)
int main(void)
{
 uint16_t words[322];for(unsigned n=0;n<322;n++)words[n]=(uint16_t)(n*5917u+127);
 WillowNativeSatTarget target;
 unsigned cases=0;
 for(unsigned capacity=1;capacity<=80;capacity++)for(unsigned count=0;count<=capacity;count++)
  for(unsigned alignment=0;alignment<2;alignment++){
   REQUIRE(willowNativeSatTargetPrepare(&target,0xf400,capacity));
   for(unsigned n=0;n<32768;n++)memory[n]=0x5aa5;
   controls=writes=0;
   REQUIRE(willowNativeSatPublish(&target,words+alignment,count,&frame));
   unsigned actualWords=count?count*4:4;
   REQUIRE(controls==1&&writes==actualWords);
   for(unsigned n=0;n<32768;n++){
    uint16_t expected=0x5aa5;
    if(n>=0xf400/2&&n<0xf400/2+actualWords)expected=count?words[alignment+n-0xf400/2]:0;
    REQUIRE(memory[n]==expected);
   }
   cases++;
  }
 REQUIRE(willowNativeSatTargetPrepare(&target,0xf400,80));
 WillowNativeSatTarget saved=target;
 REQUIRE(!willowNativeSatTargetPrepare(&target,0xf402,80));
 REQUIRE(!willowNativeSatTargetPrepare(&target,0xfe00,80));
 REQUIRE(!willowNativeSatTargetPrepare(&target,0xf400,0));
 REQUIRE(!willowNativeSatTargetPrepare(&target,0xf400,81));
 REQUIRE(!memcmp(&target,&saved,sizeof target));
 unsigned before=controls;
 REQUIRE(!willowNativeSatPublish(&target,words,81,&frame));
 REQUIRE(!willowNativeSatPublish(&target,0,1,&frame));
 frame.ready=3;REQUIRE(!willowNativeSatPublish(&target,words,80,&frame));
 frame.ready=7;epoch++;REQUIRE(!willowNativeSatPublish(&target,words,80,&frame));
 epoch--;REQUIRE(controls==before);
 printf("{\"cases\":%u,\"capacities\":80,\"counts\":\"0..capacity\",\"word_alignments\":2,\"guard_bytes_per_case\":65536}\n",cases);
 return 0;
}
