/* Closed lifecycle probe; never a shipping renderer or timing admission. */
#include "../src/willow_native_far_cache.h"
#include "far_stream_imports.h"
#define CACHE ((WillowNativeFarCache*)0x20f000)
#define IO ((volatile uint16_t*)0x20f200)
#define RESOURCES_INITIALIZER { \
 (const uint8_t*)WILLOW_BG_FAR_STREAM_PATTERNS_ADDRESS, \
 (const uint8_t*)WILLOW_BG_FAR_STREAM_ATTRIBUTES_ADDRESS, \
 (const uint8_t*)WILLOW_BG_FAR_STREAM_POOL_ADDRESS, \
 (const uint8_t*)WILLOW_BG_FAR_STREAM_FIXED_MAP_ADDRESS, \
 (const uint8_t*)WILLOW_BG_FAR_STREAM_CATALOG_REFS_ADDRESS, \
 WILLOW_BG_FAR_STREAM_PATTERN_COUNT,WILLOW_BG_FAR_STREAM_REFERENCE_COUNT,165 }
typedef char state_bytes[(sizeof(WillowNativeFarCache)==436)?1:-1];
typedef char displayed_offset[(offsetof(WillowNativeFarCache,displayedUse)==330)?1:-1];
typedef char context_bytes[(sizeof(WillowNativeFarContext)==20)?1:-1];
static uint16_t upload(void*context,uint16_t tile,const uint8_t*pattern)
{
 uint16_t slot=willowFarSlot(tile),call=++IO[8];
 if(context!=(void*)0x20f230||*(volatile uint32_t*)context!=0xcafebabeUL||
    slot>=165||CACHE->phase!=WILLOW_FAR_CACHE_COPYING||CACHE->owner[slot]!=65535||
    (uintptr_t)pattern<WILLOW_BG_FAR_STREAM_PATTERNS_ADDRESS||
    (uintptr_t)pattern+32>WILLOW_BG_FAR_STREAM_PATTERNS_ADDRESS+WILLOW_BG_FAR_STREAM_PATTERNS_BYTES||
    ((uintptr_t)pattern-WILLOW_BG_FAR_STREAM_PATTERNS_ADDRESS)%32)IO[9]=1;
 const WillowNativeFarResources resources=RESOURCES_INITIALIZER;
 uint16_t unresolved=0;
 if(willowFarCacheResolve(CACHE,&resources,0,0,1,&unresolved)||unresolved!=65535||
    willowFarCacheBindPrepared(CACHE,(const WillowNativeFarContext*)0x20f240)||
    willowFarCacheRequest(CACHE,&resources,0,1))IO[9]=1;
 uint16_t count=IO[11]==call?IO[10]:16;
 if(count>16){IO[9]=1;return 0;}
 if(count){
  uint16_t at=tile*32;
  *(volatile uint32_t*)0xc00004=0x40000000UL|((uint32_t)(at&0x3fff)<<16)|(at>>14);
  for(uint16_t n=0;n<count;n++)*(volatile uint16_t*)0xc00000=willowFarWord(pattern+n*2);
 }
 return IO[11]!=call;
}
void willowFarCacheApiProbe(void)
{
 const WillowNativeFarResources r=RESOURCES_INITIALIZER;
 WillowNativeFarContext*c=(WillowNativeFarContext*)0x20f240;
 uint16_t out=0xbeef,written=0xbeef,result=0;
 switch(IO[0]){
 case 0:result=willowFarCacheInit(CACHE,&r);break;
 case 1:result=willowFarCacheResolve(CACHE,&r,IO[2],IO[3],IO[4],&out);break;
 case 2:result=willowFarCacheRequest(CACHE,&r,IO[2],IO[3]);break;
 case 3:result=willowFarCacheUpload(CACHE,&r,IO[2],upload,(void*)0x20f230,&written);break;
 case 4:result=willowFarCachePreparedUse(CACHE,(const uint16_t*)0x20e000,IO[2]);break;
 case 5:result=willowFarCacheBindPrepared(CACHE,c);break;
 case 6:result=willowFarCachePreparedCurrent(CACHE,c);break;
 case 7:result=willowFarCachePublished(CACHE,c,IO[2]);break;
 case 8:result=willowFarResourcesValid(&r);break;
 case 9:result=willowFarCacheValid(CACHE);break;
 case 10:result=willowFarCachePreparedPlanes(CACHE,IO[4]&1?0:(const uint16_t*)0x20e000,IO[2],IO[4]&2?0:(const uint16_t*)0x20c000,IO[3]);break;
 }
 IO[1]=result;IO[5]=out;IO[7]=written;
}
