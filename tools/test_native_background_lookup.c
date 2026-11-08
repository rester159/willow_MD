#include <stdint.h>
#include <stddef.h>
#include "willow_native_background_lookup.h"
#include "background_lookup_fixture.h"
#ifndef __m68k__
#include <stdio.h>
#endif
static uint32_t checks,cases,digest;
static uint16_t sink;
static struct {uint16_t before;uint8_t blob[3144];uint16_t after;} mutable;
static WillowNativeBackgroundLookupIndex edited[2];
static uint8_t expect(unsigned line,uint32_t actual,uint32_t expected){
 checks++;digest=(digest<<5)|(digest>>27);digest^=actual;
 if(actual==expected)return 1;
#ifdef __m68k__
 *(volatile uint16_t*)0xff0010=line;*(volatile uint32_t*)0xff0012=actual;*(volatile uint32_t*)0xff0016=expected;
#else
 fprintf(stderr,"Background lookup line%u actual%u expected%u\n",line,actual,expected);
#endif
 return 0;
}
#define CHECK(a,b) do{if(!expect(__LINE__,(a),(b)))return 0;}while(0)
static void putWord(uint8_t*p,uint16_t value){p[0]=value>>8;p[1]=value;}
static void timing(unsigned kind,unsigned begin){
#ifdef __m68k__
 *(volatile uint16_t*)0xff0020=kind;*(volatile uint16_t*)0xff0004=begin;
#else
 (void)kind;(void)begin;
#endif
}
/* Original sorted-pair source implementation, deliberately independent of
 * hash, slot fields and maxProbe. Same exact fullkey return convention. */
static __attribute__((noinline)) uint16_t binary(const WillowNativeBackgroundLookupIndex*i,uint32_t key){
 uint16_t low=0,high=i->count;
 while(low<high){uint16_t mid=low+((high-low)>>1);uint32_t found=willowNativeBackgroundLookupLong(i->pairs+(uint32_t)mid*6);
  if(found<key)low=mid+1;else if(found>key)high=mid;else return mid;
 }
 return 0xffff;
}
static __attribute__((noinline)) uint16_t hashed(const WillowNativeBackgroundLookupIndex*i,uint32_t key){return willowNativeBackgroundLookupFind(i,key);}
static __attribute__((noinline)) uint16_t admitted(const WillowNativeBackgroundLookupIndex*i,uint32_t key){return willowNativeBackgroundLookupFindAdmitted(i,key);}
static void resetMutable(void){
 for(unsigned n=0;n<3144;n++)mutable.blob[n]=lookupBlob[n];
 for(unsigned layer=0;layer<2;layer++){edited[layer]=fixtureIndexes[layer];edited[layer].slots=mutable.blob+(layer?2120:72);}
 mutable.before=0xace1;mutable.after=0x5a69;
}
static uint8_t testRun(void){
 checks=cases=0;digest=0x57424831UL;sink=0;
 CHECK(willowNativeBackgroundLookupValidate(lookupBlob,sizeof lookupBlob,catalogBlob,sizeof catalogBlob,fixtureIndexes),1);
 /* Independent Python dictionary expectations cover every fullkey, each32
  * single-bit code/attr perturbation, neighboring keys and seeded randoms. */
 for(unsigned n=0;n<LOOKUP_QUERY_COUNT;n++){
  const LookupQuery*q=lookupQueries+n;const WillowNativeBackgroundLookupIndex*i=fixtureIndexes+q->layer;
  CHECK(hashed(i,q->key),q->expected);CHECK(admitted(i,q->key),q->expected);CHECK(binary(i,q->key),q->expected);
  CHECK(willowNativeBackgroundLookupHash(i,q->key),q->bucket);cases++;
 }
 /* Header/directory corruption must reject before trusting mutated bounds. */
 resetMutable();CHECK(willowNativeBackgroundLookupValidate(mutable.blob,sizeof mutable.blob,catalogBlob,sizeof catalogBlob,edited),1);
 for(unsigned n=0;n<72;n++){
  mutable.blob[n]^=0x80;
  CHECK(willowNativeBackgroundLookupValidate(mutable.blob,sizeof mutable.blob,catalogBlob,sizeof catalogBlob,edited),0);
  mutable.blob[n]^=0x80;cases++;
 }
 /* Every occupied slot is independently removed, then replaced by an
  * out-of-range ID. Full validation and the affected exactkey both refuse. */
 for(unsigned layer=0;layer<2;layer++){
  uint8_t*slots=mutable.blob+(layer?2120:72);unsigned count=edited[layer].count;
  for(unsigned at=0;at<=edited[layer].mask;at++){
   uint16_t value=willowNativeBackgroundLookupWord(slots+at*2);if(!value)continue;
   uint32_t key=willowNativeBackgroundLookupLong(edited[layer].pairs+(uint32_t)(value-1)*6);
   putWord(slots+at*2,0);CHECK(hashed(edited+layer,key),65535);
   CHECK(willowNativeBackgroundLookupValidate(mutable.blob,sizeof mutable.blob,catalogBlob,sizeof catalogBlob,edited),0);
   putWord(slots+at*2,count+1);CHECK(hashed(edited+layer,key),65535);
   CHECK(willowNativeBackgroundLookupValidate(mutable.blob,sizeof mutable.blob,catalogBlob,sizeof catalogBlob,edited),0);
   putWord(slots+at*2,value);cases+=2;
  }
  /* A plausible duplicate preserves occupied count but leaves one fullkey
   * absent. It must not pass the complete cold admission. */
  unsigned first=0,second=0;while(!willowNativeBackgroundLookupWord(slots+first*2))first++;second=first+1;while(!willowNativeBackgroundLookupWord(slots+second*2))second++;
  uint16_t saved=willowNativeBackgroundLookupWord(slots+first*2);
  putWord(slots+first*2,willowNativeBackgroundLookupWord(slots+second*2));
  CHECK(willowNativeBackgroundLookupValidate(mutable.blob,sizeof mutable.blob,catalogBlob,sizeof catalogBlob,edited),0);
  putWord(slots+first*2,saved);cases++;
 }
 CHECK(willowNativeBackgroundLookupValidate(mutable.blob,sizeof mutable.blob,catalogBlob,sizeof catalogBlob,edited),1);
 for(unsigned layer=0;layer<2;layer++)for(unsigned field=0;field<15;field++){
  WillowNativeBackgroundLookupIndex bad=fixtureIndexes[layer];
  switch(field){case 0:bad.slots=0;break;case 1:bad.pairs=0;break;case 2:bad.slots++;break;case 3:bad.pairs++;break;case 4:bad.count=0;break;case 5:bad.count=65535;break;case 6:bad.mask=1022;break;case 7:bad.maxProbes=0;break;case 8:bad.maxProbes=65;break;case 9:bad.attrRotate=0;break;case 10:bad.attrRotate=16;break;case 11:bad.mixLeft=0;break;case 12:bad.mixLeft=16;break;case 13:bad.mixRight=0;break;default:bad.reserved=1;break;}
  CHECK(hashed(&bad,0),65535);cases++;
 }
 CHECK(hashed(0,0),65535);
 CHECK(willowNativeBackgroundLookupValidate(0,sizeof lookupBlob,catalogBlob,sizeof catalogBlob,fixtureIndexes),0);
 CHECK(willowNativeBackgroundLookupValidate(lookupBlob,71,catalogBlob,sizeof catalogBlob,fixtureIndexes),0);
 CHECK(willowNativeBackgroundLookupValidate(lookupBlob,sizeof lookupBlob,0,sizeof catalogBlob,fixtureIndexes),0);
 CHECK(willowNativeBackgroundLookupValidate(lookupBlob,sizeof lookupBlob,catalogBlob,60,fixtureIndexes),0);
 CHECK(willowNativeBackgroundLookupValidate(lookupBlob,sizeof lookupBlob,catalogBlob,sizeof catalogBlob,0),0);
 CHECK(mutable.before,0xace1);CHECK(mutable.after,0x5a69);
 /* Separate equal-loop timing for present and perturbed/random queries.
  * Checks/digest are outside the timed batches; both return same summedIDs. */
 uint16_t sums[6];
 for(unsigned kind=0;kind<6;kind++){
  uint16_t sum=0;unsigned first=(kind<2||kind==4)?0:LOOKUP_KNOWN_COUNT,last=(kind<2||kind==4)?LOOKUP_KNOWN_COUNT:LOOKUP_QUERY_COUNT;
  timing(kind,1);
  for(unsigned n=first;n<last;n++){const LookupQuery*q=lookupQueries+n;const WillowNativeBackgroundLookupIndex*i=fixtureIndexes+q->layer;sum+=kind>=4?admitted(i,q->key):((kind&1)?hashed(i,q->key):binary(i,q->key));}
  sink=sum;timing(kind,2);sums[kind]=sum;
 }
 CHECK(sums[0],sums[1]);CHECK(sums[2],sums[3]);CHECK(sums[0],sums[4]);CHECK(sums[2],sums[5]);return 1;
}
#ifdef __m68k__
void*memcpy(void*d,const void*s,unsigned long n){uint8_t*x=d;const uint8_t*y=s;while(n--)*x++=*y++;return d;}
uint16_t nativeObjectPresentation(void){if(!testRun())return 0;*(volatile uint32_t*)0xff0008=cases;*(volatile uint32_t*)0xff000c=checks;*(volatile uint32_t*)0xff0024=digest;*(volatile uint16_t*)0xff0028=sink;return 1;}
#else
int main(void){if(!testRun())return 1;printf("{\"cases\":%u,\"checks\":%u,\"digest\":%u,\"sink\":%u}\n",cases,checks,digest,sink);return 0;}
#endif
