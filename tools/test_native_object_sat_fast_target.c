#include <stdint.h>
#include <stddef.h>
#include "willow_native_object_sat_budget_m68k.h"
#define WILLOW_OBJECT_SAT_FAST_VALIDATE willowNativeObjectSatBudgetValidateM68k
#define WILLOW_OBJECT_SAT_FAST_CLEAR willowNativeObjectSatDeltaClearM68k
#define WILLOW_OBJECT_SAT_FAST_COMMIT willowNativeObjectSatDeltaCommitM68k
void *memcpy(void*to,const void*from,size_t bytes)
{uint8_t*d=to;const uint8_t*s=from;while(bytes--)*d++=*s++;return to;}
void *memset(void*to,int value,size_t bytes)
{uint8_t*d=to;while(bytes--)*d++=(uint8_t)value;return to;}
uint32_t __umodsi3(uint32_t dividend,uint32_t divisor)
{uint32_t remainder=0;for(unsigned n=0;n<32;n++){remainder=(remainder<<1)|(dividend>>31);dividend<<=1;if(remainder>=divisor)remainder-=divisor;}return remainder;}
static int testCompare(const void*left,const void*right,size_t bytes)
{const uint8_t*a=left,*b=right;while(bytes--)if(*a++!=*b++)return 1;return 0;}
#define memcmp testCompare
#define fprintf(...) (*(volatile uint32_t*)0xff0008=test)
#define printf(...) ((void)0)
#define main runParity
#define WILLOW_FAST_PARITY_NATIVE 1
#define WILLOW_FAST_PARITY_CASES 2048
#include "test_native_object_sat_fast_parity.c"
#undef main
uint16_t nativeObjectPresentation(void)
{
 int result=runParity();
 *(volatile uint16_t*)0xff000c=(uint16_t)result;
 if(result)return 0;
 /* Exercise the two native long reads at both possible word alignments,
  * including each individual nonzero bit and independent nonzero halves. */
 uint16_t words[8] __attribute__((aligned(4)));
 for(unsigned n=0;n<4096;n++){
  uint16_t*source=words+(n&1);
  for(unsigned j=0;j<4;j++)source[j]=(n&7)?(uint16_t)nextRandom():0;
  if(n<128){for(unsigned j=0;j<4;j++)source[j]=0;source[(n>>5)&3]=(uint16_t)(1u<<((n>>1)&15));}
  uint16_t combined=0;for(unsigned j=0;j<4;j++)combined|=source[j];
  if(willowObjectSatFastRecordIsZero(source)!=(combined==0)){
   *(volatile uint16_t*)0xff000c=8;return 0;
  }
 }
 *(volatile uint32_t*)0xff0008=WILLOW_FAST_PARITY_CASES+4096+256;
 return 1;
}
