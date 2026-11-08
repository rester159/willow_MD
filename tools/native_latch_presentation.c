/* Static diagnostic integration: captured records -> scattered raw latch ->
 * native geometry/budget/SAT -> actual VDP. No source scheduler or game loop.
 * All assets are already resident; this does not measure cache/DMA work. */
#include "willow_native_object_latch_sat.h"
#include "latch_fixture.h"

static WillowObjectSatBudget budget;
static WillowObjectSatPiece pieces[80];
static uint16_t staged[320];
#ifdef WILLOW_DEFERRED_OBJECT_BUDGET
static int8_t lineDeltas[225];
#endif

static uint8_t residentAsset(void*context,uint16_t code,uint16_t source,
 int16_t x,int16_t y,uint16_t*attribute)
{
 (void)context;(void)x;(void)y;
 uint32_t key=((uint32_t)code<<5)|(source&31);
 unsigned lo=0,hi=sizeof fixtureAssets/sizeof fixtureAssets[0];
 while(lo<hi){
  unsigned mid=(lo+hi)/2;
  if(fixtureAssets[mid].key<key)lo=mid+1;else hi=mid;
 }
 if(lo==sizeof fixtureAssets/sizeof fixtureAssets[0]||fixtureAssets[lo].key!=key)
  return WILLOW_OBJECT_ASSET_MISSING;
 *attribute=fixtureAssets[lo].attribute;
 return fixtureAssets[lo].status;
}

uint16_t nativeObjectPresentation(void)
{
 volatile uint8_t*ram=(volatile uint8_t*)0xff0000;
 *(volatile uint16_t*)0xff0004=1;
 willowObjectSatBudgetInit(&budget);
#if FIXTURE_TEXT_COUNT
 for(unsigned n=0;n<FIXTURE_TEXT_COUNT;n++){
  const uint16_t*entry=fixtureText+n*4;
  unsigned width=(((entry[1]>>10)&3)+1)*8;
  unsigned height=(((entry[1]>>8)&3)+1)*8;
  if(willowObjectSatReserve(&budget,(int16_t)entry[0]-128,width,height))return 0;
 }
#endif
 *(volatile uint16_t*)0xff0004=2;
 if(!willowNativeObjectLatchCapture(ram,fixtureBank))return 0;
 *(volatile uint16_t*)0xff0004=3;
 const WillowObjectSatView view={96,16,320,224,0};
 WillowObjectSatResult result;
#ifdef WILLOW_DEFERRED_OBJECT_BUDGET
 result=willowObjectSatBuildReaderDeferred(willowNativeObjectLatchSatRead,(void*)ram,
  256,&view,&budget,pieces,80,residentAsset,0,lineDeltas);
#else
 result=willowNativeObjectLatchSatBuild(ram,&view,&budget,pieces,80,residentAsset,0);
#endif
 if(result.status||result.records!=FIXTURE_RECORD_COUNT||result.pieces!=FIXTURE_OBJECT_COUNT||
    budget.count!=FIXTURE_TEXT_COUNT+FIXTURE_OBJECT_COUNT)return 0;
 for(unsigned n=0;n<result.pieces;n++){
  uint8_t link=n+1<result.pieces?FIXTURE_TEXT_COUNT+n+1:0;
  if(!willowObjectSatWords(pieces+n,link,staged+n*4))return 0;
 }
 *(volatile uint16_t*)0xff0004=4;
 /* Diagnostic oracle is consulted only after native conversion. It is never
  * used as a generated SAT input or an asset-callback geometry source. */
 for(unsigned n=0;n<result.pieces*4u;n++)if(staged[n]!=fixtureExpected[n])return 0;
 unsigned maxSprites=0,maxPixels=0;
 for(unsigned n=0;n<224;n++){
  if(budget.sprites[n]>maxSprites)maxSprites=budget.sprites[n];
  if(budget.pixels[n]>maxPixels)maxPixels=budget.pixels[n];
 }
 if(maxSprites!=FIXTURE_MAX_SPRITES||maxPixels!=FIXTURE_MAX_PIXELS)return 0;
 *(volatile uint16_t*)0xff0004=5;
 const uint32_t address=0xf400+FIXTURE_TEXT_COUNT*8;
 *(volatile uint32_t*)0xc00004=0x40000000|((address&0x3fff)<<16)|(address>>14);
 for(unsigned n=0;n<result.pieces*4u;n++)*(volatile uint16_t*)0xc00000=staged[n];
 *(volatile uint16_t*)0xff0004=6;
 return 1;
}
