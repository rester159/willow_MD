/* Test-only packed sink. Seed arbitrary prior RAM, then run original-ROM
 * transfers through the exact production dictionary path. The 4096 initial
 * overrides cover arbitrary test fixtures, not a native capacity choice. */
#include "../src/willow_arcade_map_store.h"
#include "../native/intro_probe/src/scroll2_data.h"
static uint16_t mapCells[4096];
static WillowMapOverride mapOverrides[4096];
static WillowMapStore packedMap={mapCells,nativeScroll2Pairs,WILLOW_SCROLL2_PAIR_COUNT,mapOverrides,4096,0};
static void mapSeed(const uint8_t*ring){
 for(unsigned n=0;n<4096;n++){
  mapCells[n]=0x8000|n;mapOverrides[n].value=willowMotionLong(ring+n*4);mapOverrides[n].references=1;
 }
}
static uint8_t mapCopy(uint16_t at,uint32_t source,uint16_t count){return willowMapStoreCopyRange(&packedMap,nativeScroll2SourceIds,at,source,count);}
static void mapRead(uint8_t*ring){
 unsigned references[4096]={0};
 for(unsigned n=0;n<4096;n++){
  willowMotionPutLong(ring+n*4,willowMapStorePair(&packedMap,n));
  if(mapCells[n]&0x8000)references[mapCells[n]&0x7fff]++;
 }
 for(unsigned n=0;n<4096;n++)if(references[n]!=mapOverrides[n].references)abort();
}
