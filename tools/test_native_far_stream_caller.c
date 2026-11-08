/* Actual extracted root caller functions; no renderer implementation replica.
 * The opacity predicate and physical drain certificate are explicit test inputs.
 * Independent original-opacity and actual68000 VDP gates remain separate. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "willow_native_plane_surface.h"
#include "willow_native_far_cache.h"
#define RENDER_BACKGROUND_FAR_STREAM 1
#define BG_FAR_CODE
#define BG_SHARED_FAR_CODE
typedef struct {uint16_t row[2],full[2];} BackgroundNearRowProof;
#if TEST_HAS_ROW_PROOF
#define BG_ROW_PROOF_ARGUMENT ,BackgroundNearRowProof*proof
#define BG_ROW_PROOF_PASS(p) ,p
#else
#define BG_ROW_PROOF_ARGUMENT
#define BG_ROW_PROOF_PASS(p)
#endif
#define BG_INVALID 0xffffu
static unsigned checks;
#define CHECK(v) do { assert(v); checks++; } while(0)
WillowNativeFarCache backgroundFarCache;
uint16_t backgroundFarMissingRef;
WillowNativeFarResources backgroundFarResources;
static struct {
 WillowNativePlaneSurface planes[2];
 uint32_t columns[2][2],preparedEpoch,generation;
 WillowPresentationLease lease;
 uint16_t view[4];
} bg;
static uint16_t *nativeFarRefs, nativeNearRefs[8];
#define WILLOW_BG_CATALOG_LAYER3_REFS_ADDRESS nativeFarRefs
#define WILLOW_BG_CATALOG_LAYER2_REFS_ADDRESS nativeNearRefs
static uint16_t visibleNeed=1;
static uint16_t bgFarNeeded(uint16_t ref,uint16_t x,uint16_t y BG_ROW_PROOF_ARGUMENT){(void)ref;(void)x;(void)y;
#if TEST_HAS_ROW_PROOF
 (void)proof;
#endif
 return visibleNeed;}
#include "actual-caller-functions.h"
#if TEST_HAS_ROW_PROOF
#define bgResolveRef(layer,x,y,ref) bgResolveRef(layer,x,y,ref,0)
#endif
static uint16_t be16(const uint8_t*p){return p[0]*256u+p[1];}
static uint32_t be32(const uint8_t*p){return (uint32_t)be16(p)<<16|be16(p+2);}
static uint8_t*readfile(const char*name,size_t*size){FILE*f=fopen(name,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);uint8_t*p=malloc(n);assert(p&&fread(p,1,n,f)==(size_t)n);fclose(f);*size=n;return p;}
static uint16_t patternRef(uint16_t p){for(uint16_t n=0;n<backgroundFarResources.referenceCount;n++)if(willowFarPattern(&backgroundFarResources,n)==p)return n;assert(0);return 0;}
static uint8_t vram[65536];static unsigned uploads;
static uint16_t upload(void*unused,uint16_t tile,const uint8_t*bytes){(void)unused;CHECK(willowFarSlot(tile)!=65535);memcpy(vram+tile*32,bytes,32);uploads++;return 1;}
static void reset(void){
 memset(&bg,0,sizeof(bg));memset(&backgroundFarCache,0,sizeof(backgroundFarCache));memset(vram,0,sizeof(vram));
 CHECK(willowFarCacheInit(&backgroundFarCache,&backgroundFarResources));
 for(unsigned n=0;n<165;n++)memcpy(vram+willowFarTile(n)*32,backgroundFarResources.patterns+backgroundFarCache.owner[n]*32,32);
 bg.preparedEpoch=19;bg.generation=29;bg.lease.generation=37;bg.view[0]=bg.view[2]=0;bg.view[1]=768;bg.view[3]=1792;
 backgroundFarMissingRef=65535;visibleNeed=1;uploads=0;
}
static void drain(void){uint16_t written;CHECK(willowFarCacheUpload(&backgroundFarCache,&backgroundFarResources,16,upload,0,&written));CHECK(written==16&&uploads==16&&backgroundFarCache.phase==WILLOW_FAR_CACHE_IDLE);}
int main(int argc,char**argv){
 assert(argc==3);size_t bytes,catbytes;uint8_t*b=readfile(argv[1],&bytes),*cat=readfile(argv[2],&catbytes);
 CHECK(be32(b)==0x57424631&&be32(b+8)==bytes&&be32(b+20)==catbytes);
 backgroundFarResources=(WillowNativeFarResources){b+be32(b+40),b+be32(b+44),b+be32(b+48),b+be32(b+52),cat+be32(b+60),be16(b+32),be16(b+36),be16(b+38)};
 nativeFarRefs=malloc(backgroundFarResources.referenceCount*4);assert(nativeFarRefs);
 for(unsigned n=0;n<backgroundFarResources.referenceCount*2;n++)nativeFarRefs[n]=be16(backgroundFarResources.refs+n*2);
 const uint16_t missing=212*16;CHECK(willowFarPattern(&backgroundFarResources,missing)==3201);
 uint16_t resident=patternRef(1431);
 reset();uint16_t x=bgFirstX(1),y=bgFirstY(1);
 CHECK(bgResolveRef(1,x,y,missing));CHECK(backgroundFarMissingRef==missing&&backgroundFarCache.phase==WILLOW_FAR_CACHE_IDLE);
 CHECK(bg.planes[1].words[(y&31)*64+(x&63)]==65535);
 /* A later resident key must be protected before any victim is chosen. */
 CHECK(bgResolveRef(1,x+1,y,resident));CHECK(willowFarUsed(backgroundFarCache.preparedUse,0));
 CHECK(!bgFarFinish());CHECK(backgroundFarCache.phase==WILLOW_FAR_CACHE_QUEUED&&backgroundFarCache.slots[0]==1);
 CHECK(backgroundFarCache.owner[0]==1431&&backgroundFarCache.missing&&bgFarRetry());
 CHECK(!bgFarPublishCurrent());
 /* A new proved blank view removes holes, but a queued upload still blocks bind. */
 backgroundFarMissingRef=65535;visibleNeed=0;bg.generation++;bg.preparedEpoch++;bg.view[2]++;
 CHECK(bgResolveRef(1,x,y,missing)&&bgResolveRef(1,x+1,y,resident));
 CHECK(!bgFarFinish());CHECK(!backgroundFarCache.missing&&backgroundFarCache.phase==WILLOW_FAR_CACHE_QUEUED);
 CHECK(!bgFarPublishCurrent());drain();CHECK(bgFarFinish()&&bgFarPublishCurrent()&&!bgFarRetry());
 for(unsigned i=0;i<6;i++)CHECK(backgroundFarCache.preparedUse[i]==0);

 reset();x=bgFirstX(1);y=bgFirstY(1);CHECK(bgResolveRef(1,x,y,resident));
 CHECK(bgFarFinish());bg.planes[0].dirty=bg.planes[1].dirty=0;CHECK(bgFarPublished());
 CHECK(willowFarUsed(backgroundFarCache.displayedUse,0));
 /* Source view moves; old physical map has not changed, even if the new shadow retires that cell. */
 bg.view[2]+=8;bg.generation++;bg.preparedEpoch++;backgroundFarMissingRef=65535;
 CHECK(bgResolveRef(1,bgFirstX(1)+3,y,missing));CHECK(!bgFarFinish());
 CHECK(bg.planes[1].words[(y&31)*64+(x&63)]==0);
 CHECK(!willowFarUsed(backgroundFarCache.preparedUse,0)&&willowFarUsed(backgroundFarCache.displayedUse,0));
 CHECK(backgroundFarCache.slots[0]!=0);CHECK(!memcmp(vram+759*32,backgroundFarResources.patterns+1431*32,32));
 drain();CHECK(backgroundFarCache.owner[0]==1431);backgroundFarMissingRef=65535;
 CHECK(bgResolveRef(1,bgFirstX(1)+3,y,missing));CHECK(bgFarFinish()&&bgFarPublishCurrent());
 uint16_t newWord=bg.planes[1].words[(y&31)*64+((bgFirstX(1)+3)&63)],newSlot=willowFarSlot(newWord&2047);
 CHECK(newSlot!=65535&&newSlot!=0);
 /* Explicit partial drain certificate must retain old+new ownership. */
 bg.planes[0].dirty=1;bg.planes[1].dirty=0;CHECK(bgFarPublished());
 CHECK(willowFarUsed(backgroundFarCache.displayedUse,0)&&willowFarUsed(backgroundFarCache.displayedUse,newSlot));
 /* Only the actual completed physical drain certificate releases old protection. */
 bg.planes[0].dirty=bg.planes[1].dirty=0;CHECK(bgFarPublished());
 CHECK(!willowFarUsed(backgroundFarCache.displayedUse,0)&&willowFarUsed(backgroundFarCache.displayedUse,newSlot));
 printf("{\"assertions\":%u,\"later_resident_protected_before_request\":true,\"missing_blocks_bind\":true,\"blank_view_clears_missing\":true,\"old_displayed_owner_held_until_full_drain\":true,\"native_vdp_or_timing_claim\":false}\n",checks);
 free(nativeFarRefs);free(cat);free(b);return 0;
}
