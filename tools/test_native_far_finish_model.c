/* Closed policy model, not a runtime implementation. Exact current Finish,
 * Set, protection, geometry and context helpers are extracted by the driver.
 * Proposed changes below deliberately remain outside shipping sources. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "willow_native_far_cache.h"
#include "far_stream_imports.h"
#define BG_INVALID 65535U
#define BG_FAR_CODE
#define RENDER_BACKGROUND_FAR_STREAM 1
static unsigned checks, keys, wraps, fallbacks, uploads, coldCalls, refusals, exactCases;
static unsigned stalePinStopRef,stalePins,physicalPins,progressSteps;
#define CHECK(x) do {checks++; if(!(x)){fprintf(stderr,"finish proof line%u: %s\n",__LINE__,#x);exit(1);}} while(0)
/* Only members touched by extracted helpers; no target ABI/layout assertion. */
typedef struct {uint16_t words[2048];uint32_t dirty;uint8_t first[32],last[32];} WillowNativePlaneSurface;
typedef struct {
 WillowNativePlaneSurface planes[2];uint32_t columns[2][2];
 struct {uint32_t generation;} lease;
 uint32_t preparedEpoch,generation;uint16_t view[4],mode;
} ModelBackground;
typedef struct {
 WillowNativeFarCache cache;ModelBackground state;uint16_t firstMissing;
 uint16_t physical[2048],desired[2048];uint8_t needed[2048];
 uint8_t vram[65536];unsigned scans,ready,failAfter,callbackCalls;
} Fixture;
static Fixture *f;
static WillowNativeFarResources backgroundFarResources;
static uint8_t initialVRAM[65536],*blob,*catalog;
static WillowNativeFarCache initialCache;
#define bg (f->state)
#define backgroundFarCache (f->cache)
#define backgroundFarMissingRef (f->firstMissing)
static uint16_t countedPreparedUse(WillowNativeFarCache*s,const uint16_t*w,uint16_t n)
{f->scans++;return willowFarCachePreparedUse(s,w,n);}
#define willowFarCachePreparedUse countedPreparedUse
#include "exact-caller.inc"
#include "wrapped-caller.inc"
#undef willowFarCachePreparedUse

static void reset(Fixture*p,unsigned mode)
{
 memset(p,0,sizeof(*p));f=p;p->cache=initialCache;p->firstMissing=BG_INVALID;
 bg.mode=mode;bg.preparedEpoch=101;bg.lease.generation=201;bg.generation=301;
 bg.view[0]=132;bg.view[1]=768;bg.view[2]=34;bg.view[3]=1792;
 memcpy(p->vram,initialVRAM,65536);p->failAfter=16;
 CHECK(willowFarCacheBindPrepared(&p->cache,&(WillowNativeFarContext){101,201,301,{132,768,34,1792}}));
}
static unsigned at(unsigned column,unsigned row)
{return (((bgFirstY(1)+row)&31)*64+((bgFirstX(1)+column)&63));}
static unsigned inside(unsigned n)
{return (((n%64-(bgFirstX(1)&63))&63)<43)&&(((n/64-(bgFirstY(1)&31))&31)<bgRows(1));}
static unsigned used(const uint32_t*bits,unsigned slot){return willowFarUsed(bits,slot);}
static void assertPins(void)
{
 for(unsigned n=0;n<2048;n++){
  unsigned a=willowFarSlot(bg.planes[1].words[n]&2047),b=willowFarSlot(f->physical[n]&2047);
  if(bg.planes[1].words[n]!=BG_INVALID&&a!=BG_INVALID)CHECK(used(f->cache.preparedUse,a));
  if(b!=BG_INVALID)CHECK(used(f->cache.displayedUse,b));
 }
}
static void resolveOne(unsigned n,unsigned candidate,unsigned retain)
{
 unsigned x=n%64,y=n/64;uint16_t word=0;
 if(f->needed[n]&&willowFarCacheResolve(&f->cache,&backgroundFarResources,
    f->desired[n],bg.planes[1].words[n],0,&word)!=WILLOW_FAR_CACHE_COMMITTED){
  if(f->firstMissing==BG_INVALID)f->firstMissing=f->desired[n];
  if(candidate&&bg.mode==4){
   f->cache.missing=1;
   if(!retain)bgSet(1,x,y,0);
  }else bgSet(1,x,y,BG_INVALID);
 }else bgSet(1,x,y,word);
}
static void retire(void)
{
 for(unsigned n=0;n<2048;n++)if(!inside(n)&&bg.planes[1].words[n])bgSet(1,n%64,n/64,0);
 CHECK(countedPreparedUse(&f->cache,bg.planes[1].words,2048));
}
/* Proposed live-only path. The actual runtime must put missing=0 immediately
 * before its complete mode4 far resolution, after the unchanged/retry guard.
 * Mode2/3 always execute exact extracted Finish, including FFFF scans. */
static unsigned proposedFinish(void)
{
 if(bg.mode!=4)return bgFarFinish();
 if(f->firstMissing!=BG_INVALID){
  f->cache.missing=1;f->cache.preparedResidencyEpoch=0;
  if(f->cache.phase==WILLOW_FAR_CACHE_IDLE&&
     !willowFarCacheRequest(&f->cache,&backgroundFarResources,f->firstMissing>>4,65535)){
   fallbacks++;retire();
   /* Zero is a safe unpublished placeholder, not evidence of resolution. */
   f->cache.missing=1;
   (void)willowFarCacheRequest(&f->cache,&backgroundFarResources,f->firstMissing>>4,65535);
  }
  return 0;
 }
 WillowNativeFarContext context;bgFarContext(&context);
 return willowFarCacheBindPrepared(&f->cache,&context);
}
static unsigned fullPass(unsigned candidate,unsigned retain)
{
 CHECK(f->cache.phase==WILLOW_FAR_CACHE_IDLE);f->ready=0;f->firstMissing=BG_INVALID;
 /* This model always takes the complete-resolution branch, never an unchanged
  * early exit. Its cached retry flag is checked before certificate removal. */
 unsigned retry=bgFarRetry();
 if(candidate&&bg.mode==4){f->cache.missing=0;f->cache.preparedResidencyEpoch=0;}
 for(unsigned row=0;row<bgRows(1);row++)for(unsigned col=0;col<43;col++)resolveOne(at(col,row),candidate,retain);
 unsigned okay=candidate?proposedFinish():bgFarFinish();
 (void)retry;f->ready=okay;assertPins();return okay;
}
static uint16_t upload(void*ctx,uint16_t tile,const uint8_t*bytes)
{
 CHECK(ctx==f);unsigned slot=willowFarSlot(tile);CHECK(slot<165);
 CHECK(!used(f->cache.displayedUse,slot)&&!used(f->cache.preparedUse,slot));
 CHECK(f->cache.phase==WILLOW_FAR_CACHE_COPYING&&f->cache.owner[slot]==BG_INVALID);
 uint8_t before[65536];memcpy(before,f->vram,65536);
 memcpy(f->vram+tile*32,bytes,f->failAfter*2);
 for(unsigned i=0;i<65536;i++)if(i<tile*32||i>=tile*32+32)CHECK(before[i]==f->vram[i]);
 uploads++;f->callbackCalls++;return f->failAfter==16;
}
static void drain(void)
{
 unsigned limit=0;
 while(f->cache.phase==WILLOW_FAR_CACHE_QUEUED){uint16_t written;CHECK(++limit<=16);
  CHECK(willowFarCacheUpload(&f->cache,&backgroundFarResources,4,upload,f,&written));
  CHECK(written<=4);assertPins();
 }
}
static void checkVisible(void)
{
 for(unsigned n=0;n<2048;n++)if(inside(n)){
  unsigned word=bg.planes[1].words[n],pattern=f->needed[n]?willowFarPattern(&backgroundFarResources,f->desired[n]):0;
  CHECK(word!=BG_INVALID);CHECK(!memcmp(f->vram+(word&2047)*32,backgroundFarResources.patterns+pattern*32,32));
  CHECK((word&0xf800)==(pattern?willowFarAttributes(&backgroundFarResources,f->desired[n]):0));
 }
}
static void publish(unsigned complete)
{
 WillowNativeFarContext context;bgFarContext(&context);
 CHECK(f->ready&&willowFarCachePreparedCurrent(&f->cache,&context));checkVisible();
 for(unsigned n=0;n<2048;n++)if(complete||n/64==0)f->physical[n]=bg.planes[1].words[n];
 CHECK(willowFarCachePublished(&f->cache,&context,complete));assertPins();
}
static unsigned findKey(unsigned code,unsigned attr)
{
 const uint8_t*p=catalog+WILLOW_BG_FAR_STREAM_CATALOG_PAIRS_ADDRESS-WILLOW_BG_FAR_STREAM_CATALOG_ORIGIN;
 for(unsigned k=0;k<248;k++)if(willowFarWord(p+k*6)==code&&willowFarWord(p+k*6+2)==attr)return k;
 CHECK(0);return 0;
}
static void seedKey(unsigned k)
{for(unsigned n=0;n<16;n++){unsigned cell=at(n,0);f->desired[cell]=k*16+n;f->needed[cell]=1;}}
static void allKeys(void)
{
 Fixture a,b;
 for(unsigned k=0;k<248;k++){
  reset(&a,4);seedKey(k);reset(&b,4);seedKey(k);
  f=&a;unsigned aa=fullPass(0,0);f=&b;unsigned bb=fullPass(1,0);CHECK(aa==bb);
  if(!aa){CHECK(a.scans==1&&b.scans==0);CHECK(a.cache.phase==2&&b.cache.phase==2);
   CHECK(!memcmp(a.cache.slots,b.cache.slots,16));f=&a;drain();CHECK(fullPass(0,0));
   f=&b;drain();CHECK(fullPass(1,0));CHECK(b.scans==0);
  }
  f=&a;publish(1);f=&b;publish(1);
  CHECK(!memcmp(a.physical,b.physical,sizeof(a.physical))&&!memcmp(a.vram,b.vram,65536));keys++;
 }
}
static void wrapRetirement(void)
{
 Fixture p;unsigned k=findKey(0xc39,0x26);
 for(unsigned fine=0;fine<2;fine++)for(unsigned y=0;y<32;y++)for(unsigned x=0;x<64;x++){
  reset(&p,4);bg.view[2]=(x*8+8-96)&511;bg.view[3]=((y*8-16)&255)+fine;
  CHECK((bgFirstX(1)&63)==x&&(bgFirstY(1)&31)==y);
  for(unsigned n=0;n<2048;n++){
   unsigned slot=n%165;bg.planes[1].words[n]=willowFarTile(slot);willowFarProtect(p.cache.preparedUse,slot);
  }
  /* All previous references are private prepared residue, no displayed uses.
   * Current view is transparent except one complete new key. */
  seedKey(k);unsigned scans=p.scans;CHECK(!fullPass(1,0));
  CHECK(p.scans==scans+1&&p.cache.phase==2&&p.cache.missing);
  for(unsigned n=0;n<2048;n++)CHECK(bg.planes[1].words[n]==0);
  for(unsigned n=0;n<6;n++)CHECK(!p.cache.preparedUse[n]);
  wraps++;
 }
}
static void coldPartial(void)
{
 Fixture a,b;unsigned k=findKey(0xc39,0x26);
 for(unsigned mode=2;mode<=3;mode++){
  reset(&a,mode);seedKey(k);reset(&b,mode);seedKey(k);
  for(unsigned step=0;step<32;step++){
   for(unsigned which=0;which<2;which++){
    f=which?&b:&a;f->firstMissing=BG_INVALID;
    if(step<16)resolveOne(at(step,0),which,0);
    f->ready=which?proposedFinish():bgFarFinish();
   }
   CHECK(!memcmp(&a,&b,sizeof(a)));CHECK(!a.ready);coldCalls+=2;
  }
  f=&a;drain();CHECK(fullPass(0,0));f=&b;drain();CHECK(fullPass(1,0));
  CHECK(!memcmp(&a,&b,sizeof(a)));
 }
}
static void exactWrappedComparison(void)
{
 Fixture a,b;
 for(unsigned fine=0;fine<2;fine++)for(unsigned y=0;y<32;y++)for(unsigned x=0;x<64;x++)
 for(unsigned variant=0;variant<6;variant++){
  reset(&a,2+variant%3);bg.view[2]=(x*8+8-96)&511;bg.view[3]=((y*8-16)&255)+fine;
  for(unsigned n=0;n<2048;n++){
   unsigned values[8]={0,0,0xffff,willowFarTile(n%165),0xe000|willowFarTile((n*7)%165),16,0x8010,0};
   bg.planes[1].words[n]=variant==0?0:values[(n*13+variant)%8];
  }
  bg.planes[1].dirty=0xa55aa55a;
  for(unsigned row=0;row<32;row++){bg.planes[1].first[row]=17;bg.planes[1].last[row]=42;}
  bg.columns[1][0]=0x50204100;bg.columns[1][1]=0x42010080;
  for(unsigned slot=0;slot<165;slot++){
   willowFarProtect(a.cache.preparedUse,slot);
   if(variant==3||(slot%5)==0)willowFarProtect(a.cache.displayedUse,slot);
  }
  a.cache.missing=variant!=0;a.firstMissing=(variant&1)?findKey(0xc39,0x26)*16:BG_INVALID;
  if(variant==5){a.cache.phase=2;a.cache.key=findKey(0xc39,0x26);a.cache.requested=65535;}
  b=a;f=&a;unsigned ra=bgFarFinish();f=&b;unsigned rb=wrappedBgFarFinish();
  CHECK(ra==rb&&!memcmp(&a,&b,sizeof(a)));exactCases++;
 }
}
static unsigned bitCount(const uint32_t*bits)
{unsigned count=0;for(unsigned n=0;n<165;n++)count+=used(bits,n);return count;}
static void repeatedMissCounterexample(void)
{
 Fixture a,b;uint8_t seen[3512]={0};reset(&a,4);reset(&b,4);unsigned detected=0;
 for(unsigned ref=0;ref<3968;ref++){
  unsigned pattern=willowFarPattern(&backgroundFarResources,ref);
  if(!pattern||seen[pattern]||willowFarWord(backgroundFarResources.fixed+pattern*2)!=BG_INVALID)continue;
  seen[pattern]=1;
  f=&a;memset(a.needed,0,sizeof(a.needed));a.needed[at(0,0)]=1;a.desired[at(0,0)]=ref;
  unsigned okay=fullPass(0,0);if(!okay){CHECK(a.cache.phase==2);drain();CHECK(fullPass(0,0));}publish(1);
  f=&b;memset(b.needed,0,sizeof(b.needed));b.needed[at(0,0)]=1;b.desired[at(0,0)]=ref;
  okay=fullPass(1,0);progressSteps++;
  if(!okay&&b.cache.phase==1){
   stalePinStopRef=ref;stalePins=bitCount(b.cache.displayedUse);
   uint32_t actual[6]={0};for(unsigned n=0;n<2048;n++){unsigned slot=willowFarSlot(b.physical[n]&2047);if(slot<165)willowFarProtect(actual,slot);}
   physicalPins=bitCount(actual);CHECK(physicalPins==1&&stalePins>physicalPins);
   CHECK(!b.ready&&a.ready);detected=1;break;
  }
  if(!okay){CHECK(b.cache.phase==2);drain();CHECK(fullPass(1,0));}publish(1);
 }
 CHECK(detected);
}
static void adversarial(void)
{
 Fixture p;unsigned k=findKey(0xc39,0x26);
 /* Retain-old counterexample: every private prepared owner remains inside
  * the new view, although no old physical plane uses it. A rebuild cannot
  * release these stale words. The zero-placeholder policy can. */
 reset(&p,4);
 for(unsigned n=0;n<165;n++){unsigned cell=at(n%43,n/43);bgSet(1,cell%64,cell/64,willowFarTile(n));
  p.needed[cell]=1;p.desired[cell]=k*16;}
 CHECK(!fullPass(1,1)&&p.cache.phase==1);refusals++;
 CHECK(!fullPass(1,0)&&p.cache.phase==2);drain();CHECK(fullPass(1,0));publish(1);
 /* A fully physically protected pool must refuse unchanged owner/VRAM even
  * after zero placeholders and a fallback rebuild. */
 reset(&p,4);seedKey(k);
 for(unsigned n=0;n<165;n++){p.physical[n]=willowFarTile(n);willowFarProtect(p.cache.displayedUse,n);}
 uint8_t before[65536];memcpy(before,p.vram,65536);uint16_t owners[165];memcpy(owners,p.cache.owner,sizeof(owners));
 CHECK(!fullPass(1,0)&&p.cache.phase==1);CHECK(!memcmp(before,p.vram,65536)&&!memcmp(owners,p.cache.owner,sizeof(owners)));refusals++;
 /* Partial/failing uploads never certify placeholders or stale contexts. */
 for(unsigned prefix=0;prefix<=16;prefix++){
  reset(&p,4);seedKey(k);CHECK(!fullPass(1,0));p.failAfter=prefix;uint16_t written;
  unsigned ok=willowFarCacheUpload(&p.cache,&backgroundFarResources,1,upload,&p,&written);
  CHECK(ok==(prefix==16));WillowNativeFarContext c;bgFarContext(&c);
  CHECK(!willowFarCacheBindPrepared(&p.cache,&c)&&!willowFarCachePreparedCurrent(&p.cache,&c));
  if(prefix<16)CHECK(p.cache.owner[p.cache.slots[0]]==BG_INVALID);
  p.failAfter=16;drain();CHECK(fullPass(1,0));checkVisible();
  c.sourceEpoch++;CHECK(!willowFarCachePreparedCurrent(&p.cache,&c));c.sourceEpoch--;
  c.paletteEpoch++;CHECK(!willowFarCachePreparedCurrent(&p.cache,&c));c.paletteEpoch--;
  c.generation++;CHECK(!willowFarCachePreparedCurrent(&p.cache,&c));c.generation--;
  for(unsigned n=0;n<4;n++){c.view[n]++;CHECK(!willowFarCachePreparedCurrent(&p.cache,&c));c.view[n]--;}
  publish(0);publish(1);
 }
 /* Clearing missing before a cold partial pass would falsely bind an
  * unresolved zero placeholder: detect this intentionally broken variant. */
 reset(&p,4);seedKey(k);CHECK(!fullPass(1,0));p.cache.phase=1;
 p.cache.missing=0;WillowNativeFarContext c;bgFarContext(&c);
 CHECK(willowFarCacheBindPrepared(&p.cache,&c));
 CHECK(memcmp(p.vram+(bg.planes[1].words[at(0,0)]&2047)*32,
  backgroundFarResources.patterns+3201*32,32)!=0);refusals++;
 /* Request refusal from epoch saturation remains nonpublishing. */
 reset(&p,4);seedKey(k);p.cache.residencyEpoch=UINT32_MAX-1;
 CHECK(!fullPass(1,0)&&p.cache.phase==1&&p.cache.missing);refusals++;
}
static uint8_t*load(const char*path,size_t*size)
{
 FILE*file=fopen(path,"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));long n=ftell(file);CHECK(n>0);rewind(file);
 uint8_t*p=malloc(n);CHECK(p);CHECK(fread(p,1,n,file)==(size_t)n);fclose(file);*size=n;return p;
}
int main(int argc,char**argv)
{
 CHECK(argc==4);size_t n;blob=load(argv[1],&n);CHECK(n==WILLOW_BG_FAR_STREAM_BYTES);
 catalog=load(argv[2],&n);CHECK(n==WILLOW_BG_FAR_STREAM_CATALOG_BYTES);
 backgroundFarResources=(WillowNativeFarResources){blob+WILLOW_BG_FAR_STREAM_PATTERNS_OFFSET,
  blob+WILLOW_BG_FAR_STREAM_ATTRIBUTES_OFFSET,blob+WILLOW_BG_FAR_STREAM_POOL_OFFSET,
  blob+WILLOW_BG_FAR_STREAM_FIXED_MAP_OFFSET,
  catalog+WILLOW_BG_FAR_STREAM_CATALOG_REFS_ADDRESS-WILLOW_BG_FAR_STREAM_CATALOG_ORIGIN,
  WILLOW_BG_FAR_STREAM_PATTERN_COUNT,WILLOW_BG_FAR_STREAM_REFERENCE_COUNT,165};
 CHECK(willowFarCacheInit(&initialCache,&backgroundFarResources));
 memset(initialVRAM,0x5a,65536);memset(initialVRAM,0,32);
 uint8_t*physical=load(argv[3],&n);CHECK(n%34==0);
 for(size_t i=0;i<n;i+=34){unsigned tile=willowFarWord(physical+i);CHECK(tile<2048);memcpy(initialVRAM+tile*32,physical+i+2,32);}free(physical);
 allKeys();wrapRetirement();coldPartial();adversarial();exactWrappedComparison();repeatedMissCounterexample();
 printf("{\"assertions\":%u,\"complete_source_keys\":%u,\"wrapped_retirement_cases\":%u,\"exact_old_vs_wrapped_finish_cases\":%u,\"capacity_fallbacks\":%u,\"upload_callbacks\":%u,\"cold_exact_calls\":%u,\"adversarial_refusals_or_mutation_detections\":%u,\"conservative_no_scan_rejected\":true,\"repeated_miss_stop_ref\":%u,\"completed_contexts_before_stop\":%u,\"conservative_displayed_pins_at_stop\":%u,\"actual_physical_pins_at_stop\":%u}\n",checks,keys,wraps,exactCases,fallbacks,uploads,coldCalls,refusals,stalePinStopRef,progressSteps-1,stalePins,physicalPins);
 free(blob);free(catalog);return 0;
}
