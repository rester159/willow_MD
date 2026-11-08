#include "resident_fixture.h"
#if !defined(__m68k__)
#include <stdio.h>
#endif
static uint8_t original(uint16_t code,uint16_t source,uint16_t*out){
 uint32_t key=((uint32_t)code<<5)|(source&31);unsigned lo=0,hi=RESIDENT_TEST_ENTRIES;
 while(lo<hi){unsigned mid=(lo+hi)/2;if(fixtureOriginal[mid].key<key)lo=mid+1;else hi=mid;}
 if(lo==RESIDENT_TEST_ENTRIES||fixtureOriginal[lo].key!=key)return 0;
 *out=fixtureOriginal[lo].attribute;return fixtureOriginal[lo].status;
}
static uint8_t check(uint16_t code,uint16_t source,uint32_t epoch){
 uint16_t a=0x6bad,b=0x6bad,c=0x6bad;
 uint8_t sa=epoch==77?original(code,source,&a):0;
 uint8_t sb=willowNativeObjectSatResidentLookup(&fixtureResidentIndex,epoch,code,source,&b);
 WillowObjectSatResidentContext context={&fixtureResidentIndex,epoch};
 uint8_t sc=willowNativeObjectSatResidentAsset(&context,code,source,-32768,32767,&c);
 return sa==sb&&sa==sc&&a==b&&a==c;
}
static uint8_t syntheticOriginal(uint16_t code,uint16_t source,uint16_t*out){
 uint32_t key=((uint32_t)code<<5)|(source&31);unsigned lo=0,hi=SYNTHETIC_TEST_ENTRIES;
 while(lo<hi){unsigned mid=(lo+hi)/2;if(syntheticAssets[mid].key<key)lo=mid+1;else hi=mid;}
 if(lo==SYNTHETIC_TEST_ENTRIES||syntheticAssets[lo].key!=key)return 0;
 *out=syntheticAssets[lo].attribute;return syntheticAssets[lo].status;
}
static uint8_t syntheticCheck(uint16_t code,uint16_t source,uint32_t epoch){
 uint16_t a=0x6bad,b=0x6bad,c=0x6bad;
 uint8_t sa=epoch==77?syntheticOriginal(code,source,&a):0;
 uint8_t sb=willowNativeObjectSatResidentLookup(&syntheticResidentIndex,epoch,code,source,&b);
 WillowObjectSatResidentContext context={&syntheticResidentIndex,epoch};
 uint8_t sc=willowNativeObjectSatResidentAsset(&context,code,source,-32768,32767,&c);
 return sa==sb&&sa==sc&&a==b&&a==c;
}
static uint8_t guards(void){
 if(!willowNativeObjectSatResidentValidate(&fixtureResidentIndex)||!willowNativeObjectSatResidentValidate(&syntheticResidentIndex))return 0;
 WillowObjectSatResidentIndex bad=fixtureResidentIndex;uint16_t output=0x6bad;
 bad.firstPage=256;
 if(willowNativeObjectSatResidentValidate(&bad)||willowNativeObjectSatResidentLookup(&bad,77,0,0,&output)||output!=0x6bad)return 0;
 bad=fixtureResidentIndex;bad.pageCount=257;
 if(willowNativeObjectSatResidentValidate(&bad)||willowNativeObjectSatResidentLookup(&bad,77,0,0,&output)||output!=0x6bad)return 0;
 if(willowNativeObjectSatResidentLookup(0,77,0,0,&output)||willowNativeObjectSatResidentLookup(&fixtureResidentIndex,77,0,0,0)||willowNativeObjectSatResidentAsset(0,0,0,0,0,&output))return 0;
 return 1;
}
#if defined(__m68k__)
uint16_t nativeObjectPresentation(void){
 if(!guards())return 0;
 uint32_t cases=0;
 for(uint16_t n=0;n<8192;n++){
  if(!check((uint16_t)(n*17u),(uint16_t)(n|0xffe0u),77)||!syntheticCheck((uint16_t)(n*17u),(uint16_t)(n|0xffe0u),77))return 0;
  cases+=2;
 }
 for(uint16_t entry=0;entry<RESIDENT_TEST_ENTRIES;entry++)for(uint16_t group=0;group<32;group++){
  uint16_t code=fixtureOriginal[entry].key>>5;
  for(int16_t offset=-1;offset<=1;offset++){if(!check(code+offset,group|0xffe0,77))return 0;cases++;}
  if(!check(code,group,78))return 0;
  cases++;
 }
 for(uint16_t entry=0;entry<SYNTHETIC_TEST_ENTRIES;entry++)for(uint16_t group=0;group<32;group++){
  uint16_t code=syntheticAssets[entry].key>>5;
  for(int16_t offset=-1;offset<=1;offset++){if(!syntheticCheck(code+offset,group|0xffe0,77))return 0;cases++;}
  if(!syntheticCheck(code,group,78))return 0;
  cases++;
 }
 *(volatile uint32_t*)0xff0008=cases;return 1;
}
#else
int main(void){
 if(!guards())return 1;unsigned hits=0;
 for(unsigned code=0;code<65536;code++)for(unsigned group=0;group<32;group++){
  if(!check(code,(uint16_t)(group|((code&0x7ff)<<5)),77)||!syntheticCheck(code,(uint16_t)(group|((code&0x7ff)<<5)),77))return 1;
  uint16_t ignored;hits+=original(code,group,&ignored)!=0;
 }
 if(hits!=RESIDENT_TEST_ENTRIES)return 1;
 for(unsigned entry=0;entry<RESIDENT_TEST_ENTRIES;entry++)if(!check(fixtureOriginal[entry].key>>5,fixtureOriginal[entry].key&31,78))return 1;
 printf("PASS all4194304 code/group pairs across real and synthetic multi-group/extreme-code corpora, %u real entries, epoch mismatch and both callback/direct interfaces.\n",hits);return 0;
}
#endif
