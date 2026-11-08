#include <stdint.h>
#include <stddef.h>
#include "willow_native_background_key_cache.h"
#ifndef __m68k__
#include <stdio.h>
#endif
static uint32_t checks,cases,digest;
static struct {uint8_t before[8];WillowNativeBackgroundKeyCache c;uint8_t after[8];} guarded;
static uint16_t expectedNear[330],expectedFar[96];
static uint8_t old[sizeof(guarded)];
static uint8_t expect(unsigned line,uint32_t actual,uint32_t expected)
{
 checks++;digest=(digest<<5)|(digest>>27);digest^=actual;
 if(actual==expected)return 1;
#ifdef __m68k__
 *(volatile uint16_t*)0xff0010=(uint16_t)line;
 *(volatile uint32_t*)0xff0012=actual;*(volatile uint32_t*)0xff0016=expected;
#else
 fprintf(stderr,"Keycache line%u actual%u expected%u\n",line,actual,expected);
#endif
 return 0;
}
#define CHECK(a,b) do{if(!expect(__LINE__,(a),(b)))return 0;}while(0)
static uint8_t canaries(void)
{
 for(uint16_t n=0;n<8;n++){CHECK(guarded.before[n],0xa6);CHECK(guarded.after[n],0x59);}
 CHECK(guarded.c.nearHigh[82]&0xf0,0xf0);return 1;
}
static uint8_t testRun(void)
{
 WillowNativeBackgroundKeyCache*c=&guarded.c;uint16_t got;
 checks=cases=0;digest=0x91809140UL;
 for(uint16_t n=0;n<8;n++){guarded.before[n]=0xa6;guarded.after[n]=0x59;}
 CHECK(sizeof(*c),509);CHECK(offsetof(WillowNativeBackgroundKeyCache,nearHigh),330);CHECK(offsetof(WillowNativeBackgroundKeyCache,far),413);
 CHECK(willowNativeBackgroundKeysInit(c),1);
 for(uint16_t n=0;n<330;n++){CHECK(willowNativeBackgroundNearKeyGet(c,n,&got),1);CHECK(got,1023);expectedNear[n]=1023;}
 for(uint16_t n=0;n<96;n++){CHECK(willowNativeBackgroundFarKeyGet(c,n,&got),1);CHECK(got,255);expectedFar[n]=255;}
 /* Every ten-bit value at every near slot, including each packed phase and
  * partial final byte. Neighbor values are independently retained and checked. */
 for(uint16_t n=0;n<330;n++)for(uint16_t id=0;id<1024;id++){
  CHECK(willowNativeBackgroundNearKeySet(c,n,id),1);expectedNear[n]=id;
  for(uint16_t k=(uint16_t)(n&~3U);k<330&&k<(uint16_t)((n&~3U)+4);k++){
   CHECK(willowNativeBackgroundNearKeyGet(c,k,&got),1);CHECK(got,expectedNear[k]);
  }
  cases++;
 }
 for(uint16_t n=0;n<96;n++)for(uint16_t id=0;id<256;id++){
  CHECK(willowNativeBackgroundFarKeySet(c,n,id),1);expectedFar[n]=id;
  CHECK(willowNativeBackgroundFarKeyGet(c,n,&got),1);CHECK(got,id);cases++;
 }
 for(uint16_t n=0;n<sizeof(guarded);n++)old[n]=((uint8_t*)&guarded)[n];
 uint16_t invalidSlots[]={330,331,0xffff},invalidIds[]={1024,1025,0xffff};
 for(uint16_t n=0;n<3;n++){
  got=0xace1;CHECK(willowNativeBackgroundNearKeyGet(c,invalidSlots[n],&got),0);CHECK(got,0xace1);
  CHECK(willowNativeBackgroundNearKeySet(c,invalidSlots[n],1),0);CHECK(willowNativeBackgroundNearKeySet(c,3,invalidIds[n]),0);
  got=0xace1;CHECK(willowNativeBackgroundFarKeyGet(c,(uint16_t)(96+n),&got),0);CHECK(got,0xace1);
  CHECK(willowNativeBackgroundFarKeySet(c,(uint16_t)(96+n),1),0);CHECK(willowNativeBackgroundFarKeySet(c,3,(uint16_t)(256+n)),0);cases++;
 }
 CHECK(willowNativeBackgroundKeysInit(0),0);
 CHECK(willowNativeBackgroundNearKeyGet(0,0,&got),0);CHECK(willowNativeBackgroundNearKeyGet(c,0,0),0);CHECK(willowNativeBackgroundNearKeySet(0,0,0),0);
 CHECK(willowNativeBackgroundFarKeyGet(0,0,&got),0);CHECK(willowNativeBackgroundFarKeyGet(c,0,0),0);CHECK(willowNativeBackgroundFarKeySet(0,0,0),0);
 for(uint16_t n=0;n<sizeof(guarded);n++)CHECK(((uint8_t*)&guarded)[n],old[n]);
 for(uint16_t n=0;n<330;n++){CHECK(willowNativeBackgroundNearKeyGet(c,n,&got),1);CHECK(got,expectedNear[n]);}
 for(uint16_t n=0;n<96;n++){CHECK(willowNativeBackgroundFarKeyGet(c,n,&got),1);CHECK(got,expectedFar[n]);}
 CHECK(willowNativeBackgroundKeysInit(c),1);
 for(uint16_t n=0;n<sizeof(*c);n++)CHECK(((uint8_t*)c)[n],255);
 CHECK(canaries(),1);return 1;
}
#ifdef __m68k__
void*memcpy(void*dst,const void*src,unsigned long n){uint8_t*d=dst;const uint8_t*s=src;while(n--)*d++=*s++;return dst;}
uint16_t nativeObjectPresentation(void)
{
 if(!testRun())return 0;
 *(volatile uint32_t*)0xff0008=cases;*(volatile uint32_t*)0xff000c=checks;*(volatile uint32_t*)0xff0024=digest;return 1;
}
#else
int main(void)
{
 if(!testRun())return 1;
 printf("{\"cases\":%u,\"checks\":%u,\"digest\":%u}\n",cases,checks,digest);return 0;
}
#endif
