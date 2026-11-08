/* Optional forward-only latch reader binding; root fixture stays unchanged. */
#include "willow_native_object_sat_budget_m68k.h"
#define WILLOW_OBJECT_SAT_FAST_VALIDATE willowNativeObjectSatBudgetValidateM68k
#define WILLOW_OBJECT_SAT_FAST_CLEAR willowNativeObjectSatDeltaClearM68k
#define WILLOW_OBJECT_SAT_FAST_COMMIT willowNativeObjectSatDeltaCommitM68k
#include "willow_native_object_sat_fast.h"
#include "willow_native_object_latch_sat.h"
#include "willow_native_object_latch_sequential.h"
#ifdef WILLOW_FAST_SCAN_ZERO
#include "willow_native_object_latch_next.h"
#endif
#ifdef WILLOW_FAST_RESIDENT_INDEX
#include "resident_fixture.h"
static uint8_t fixtureIndexedAsset(void*context,uint16_t code,uint16_t source,
 int16_t x,int16_t y,uint16_t*attribute)
{
 (void)context;(void)x;(void)y;
 return willowNativeObjectSatResidentLookup(&fixtureResidentIndex,77,code,source,attribute);
}
#endif
static inline WillowObjectSatResult fixtureSerialBuild(
 WillowObjectSatReadRecord read,void*bankContext,uint16_t records,
 const WillowObjectSatView*view,WillowObjectSatBudget*budget,
 WillowObjectSatPiece*pieces,uint16_t capacity,WillowObjectSatAsset asset,
 void*assetContext,int8_t*deltas)
{
#ifdef WILLOW_FAST_RESIDENT_INDEX
 /* The optional fixture index is emitted from exactly fixtureAssets. This
  * binding supplies only immutable code/group residency, never geometry. */
 asset=fixtureIndexedAsset;assetContext=0;
#endif
 /* Only the known latch adapter gives bankContext the RAM-base meaning. */
 if(read!=willowNativeObjectLatchSatRead)
  return willowObjectSatBuildReaderFast(read,bankContext,records,view,budget,
   pieces,capacity,asset,assetContext,deltas);
 WillowObjectLatchSequential reader;
 if(!willowNativeObjectLatchSequentialInit(&reader,bankContext)){
  WillowObjectSatResult invalid={WILLOW_OBJECT_SAT_INVALID,0,0,0,0,0};return invalid;
 }
#ifdef WILLOW_FAST_SCAN_ZERO
 return willowObjectSatBuildNextFast(willowNativeObjectLatchNext,
#else
 return willowObjectSatBuildReaderFast(willowNativeObjectLatchSequentialRead,
#endif
  &reader,records,view,budget,pieces,capacity,asset,assetContext,deltas);
}
#define WILLOW_DEFERRED_OBJECT_BUDGET 1
#define willowObjectSatBuildReaderDeferred fixtureSerialBuild
#define willowObjectSatBudgetInit willowNativeObjectSatBudgetInitM68k
#define willowObjectSatReserve willowNativeObjectSatReserveM68k
#include "native_latch_presentation.c"
