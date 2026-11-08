/* Static integration diagnostic: one shared prepared frame combines native
 * sprite templates and the native Window cache. No source scheduler or GUI.
 * First publication is cold preparation; the measured pass refreshes source
 * origins again and reuses the unchanged Window. Both are actual target code. */
#include "willow_native_window_surface.h"
#include "willow_native_sat_publish.h"
#include "combined_fixture.h"
static WillowWindowSurface combinedWindow;
static WillowWindowSurfaceTarget combinedTarget;
static WillowNativeSatTarget combinedSat;
static volatile uint32_t combinedEpoch;
static WillowPresentationLease combinedLease;
static WillowPresentationFrame combinedFrame;
static const WillowPresentationPlan combinedPlan={combinedColors,0,0,7};

/* The source/palette/residency content was admitted by the frozen fixture's
 * independent source composition and resource manifest. This diagnostic only
 * binds those immutable resources, not a general live palette activation. */
static uint8_t combinedReady(void)
{
 if(preparedExpected.palette!=combinedLease.generation||
    preparedExpected.resident!=combinedLease.generation||
    preparedFrame.epoch.palette!=combinedLease.generation||
    preparedFrame.epoch.resident!=combinedLease.generation||
    !willowPresentationBegin(&combinedFrame,&combinedLease))return 0;
 return willowPresentationPrepared(&combinedFrame,WILLOW_PRESENT_BG)&&
        willowPresentationPrepared(&combinedFrame,WILLOW_PRESENT_SCROLL1)&&
        willowPresentationPrepared(&combinedFrame,WILLOW_PRESENT_OBJECTS);
}
static uint8_t combinedColdPrepare(void)
{
 combinedEpoch=77;combinedLease.plan=&combinedPlan;
 combinedLease.epoch=&combinedEpoch;combinedLease.generation=77;
 preparedExpected.latch=1;preparedExpected.palette=77;
 preparedExpected.resident=77;preparedExpected.view=1;
 if(!preparedFixtureInit()||!willowPreparedObjectSeedEmpty(&preparedSeed,1)||
    !willowWindowSurfaceTargetInit(&combinedTarget,0xd000)||
    !willowNativeSatTargetPrepare(&combinedSat,0xf400,80)||
    !willowWindowSurfaceLoad(&combinedWindow,&combinedLease,combinedWords)||
    !willowNativeObjectLatchCapture((volatile uint8_t*)0xff0000,fixtureBank)||
    !preparedFixtureLatch((volatile uint8_t*)0xff0000))return 0;
 WillowObjectSatResult result=willowPreparedObjectBuildSeeded(&preparedBinding,
  &preparedExpected,&preparedSeed,1,&budget,staged,80,lineDeltas);
 if(result.status||result.records!=FIXTURE_RECORD_COUNT||
    result.pieces!=FIXTURE_OBJECT_COUNT||!combinedReady()||
    !willowWindowSurfacePublish(&combinedWindow,&combinedTarget,&combinedFrame))return 0;
 /* Prepare a complete first frame before timing a second native publication.
  * The measured pass still performs its own latch refresh/build/SAT upload. */
 if(!willowNativeSatPublish(&combinedSat,staged,result.pieces,&combinedFrame))return 0;
 preparedExpected.latch=2;
 return 1;
}
static uint8_t combinedSteadyPrepare(void)
{
 if(combinedWindow.dirty||!combinedReady())return 0;
 return willowWindowSurfacePublish(&combinedWindow,&combinedTarget,&combinedFrame);
}
