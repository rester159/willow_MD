/* Source-capture replay proof; no live game scheduler. Native source-key
 * selection -> resolved templates -> geometry/admission -> VDP SAT upload.
 * Cold resource binding is outside per-capture timings. Expected words are
 * assertion-only and never supply target geometry or state. */
#define WILLOW_PREPARED_NATIVE_SELECT 1
#define WILLOW_PREPARED_NATIVE_CELLS 1
#include "willow_native_object_catalog.h"
#include "willow_native_sat_publish.h"
#include "route_objects.h"
#include "route_trace.h"
/* Freestanding diagnostic support for the compiler's cold struct copy. */
void*memcpy(void*destination,const void*source,size_t bytes)
{
 uint8_t*d=destination;const uint8_t*s=source;
 while(bytes--)*d++=*s++;
 return destination;
}
static WillowPreparedObjectKeyCatalog keys;
static WillowPreparedObjectResolvedCatalog resolved;
static WillowPreparedObjectStream stream;
static WillowPreparedObjectSelection selections[2][8];
static WillowPreparedObjectInstance instances[8];
static WillowPreparedObjectFrame frame;
static WillowPreparedObjectBinding binding;
static WillowPreparedObjectSeed seed;
static WillowObjectSatBudget budget;
static uint16_t sat[320];
static int8_t deltas[225];
static WillowNativeSatTarget target;
static volatile uint32_t resourceEpoch;
static WillowPresentationLease lease;
static WillowPresentationFrame presentation;
static const WillowPresentationPlan plan={routeColors,0,0,WILLOW_PRESENT_OBJECTS};

uint16_t nativeObjectPresentation(void)
{
 WillowPreparedObjectEpoch epoch={0,77,77,1};
 const WillowObjectSatView view={96,16,320,224,0};
 resourceEpoch=77;lease.plan=&plan;lease.epoch=&resourceEpoch;lease.generation=77;
 if(willowPreparedObjectKeyCatalogBind(&keys,routeKeys,17,1)||
    willowPreparedObjectResolvedCatalogBind(&resolved,&keys,routeShapes,&epoch,&view)||
    willowPreparedObjectStreamBind(&stream,&keys,selections[0],selections[1],8)||
    !willowPreparedObjectSeedEmpty(&seed,1)||
    !willowNativeSatTargetPrepare(&target,0xf400,80))return 0;
 for(uint16_t n=0;n<ROUTE_FRAME_COUNT;n++){
  const RouteTraceFrame*input=routeFrames+n;
  const WillowPreparedObjectSelectionPacket*selected;
  WillowPreparedObjectSourceInput source={0,0,input->epoch,input->count,256,input->bank,1};
  epoch.latch=input->epoch;
  *(volatile uint16_t*)0xff0004=1;
  if(willowPreparedObjectStreamSelect(&stream,&source,routeRecords[input->offset],&selected)||
     willowPreparedObjectPublishSelection(&resolved,selected,&epoch,instances,8,&frame,&binding))return 0;
  WillowObjectSatResult result=willowPreparedObjectBuildSeeded(&binding,&epoch,&seed,1,&budget,sat,80,deltas);
  if(result.status||result.records!=256||result.pieces!=input->pieces||
     !willowPresentationBegin(&presentation,&lease)||
     !willowPresentationPrepared(&presentation,WILLOW_PRESENT_OBJECTS)||
     !willowNativeSatPublish(&target,sat,result.pieces,&presentation))return 0;
  *(volatile uint16_t*)0xff0004=2;
  /* Full result/budget checks are intentionally outside measured work. */
  const uint16_t*expected=routeExpected+input->expected;
  if(budget.count!=*expected++)return 0;
  for(uint16_t j=0;j<result.pieces*4;j++)if(sat[j]!=*expected++)return 0;
  for(uint16_t j=0;j<224;j++){
   if(budget.sprites[j]!=*expected++||budget.pixels[j]!=*expected++)return 0;
  }
 }
 *(volatile uint16_t*)0xff000a=ROUTE_FRAME_COUNT;
 return 1;
}
