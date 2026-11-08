/* Static integration diagnostic. Geometry is computed on the actual 68000
 * from source latch origins and immutable grouped templates. The expected SAT
 * is assertion-only. Preparation and upload run with display disabled; this
 * establishes neither live producer integration nor VBlank/FPS admission. */
#include "willow_native_object_latch.h"
#include "willow_native_object_grouped.h"
#include "willow_native_window_surface.h"
#include "willow_native_sat_publish.h"
#include "grouped_scene_fixture.h"

static WillowObjectSatBudget budget;
static uint16_t staged[320];
static uint8_t spriteDelta[225],unitDelta[225];
static WillowGroupedObjectSeed seed;
static WillowWindowSurface window;
static WillowWindowSurfaceTarget windowTarget;
static WillowNativeSatTarget satTarget;
static volatile uint32_t resourceEpoch;
static WillowPresentationLease lease;
static WillowPresentationFrame presentation;
static const WillowPresentationPlan plan={fixtureColors,0,0,7};

static uint8_t ready(void)
{
 if(fixtureEpoch.palette!=lease.generation||fixtureEpoch.resident!=lease.generation||
    fixtureFrame.epoch.palette!=lease.generation||fixtureFrame.epoch.resident!=lease.generation||
    !willowPresentationBegin(&presentation,&lease))return 0;
 return willowPresentationPrepared(&presentation,WILLOW_PRESENT_BG)&&
        willowPresentationPrepared(&presentation,WILLOW_PRESENT_SCROLL1)&&
        willowPresentationPrepared(&presentation,WILLOW_PRESENT_OBJECTS);
}

static WillowObjectSatResult build(void)
{
 return willowGroupedObjectBuildSeeded(&fixtureBinding,&fixtureEpoch,&seed,1,
                                      &budget,staged,80,spriteDelta,unitDelta);
}

static uint8_t accepted(const WillowObjectSatResult*r)
{
 return !r->status&&r->records==FIXTURE_RECORD_COUNT&&
        r->pieces==FIXTURE_OBJECT_COUNT&&budget.count==FIXTURE_OBJECT_COUNT;
}

static uint8_t coldPrepare(volatile uint8_t*ram)
{
 resourceEpoch=77;lease.plan=&plan;lease.epoch=&resourceEpoch;lease.generation=77;
 if(!fixtureInit()||!willowGroupedObjectSeedEmpty(&seed,1)||
    !willowWindowSurfaceTargetInit(&windowTarget,0xd000)||
    !willowNativeSatTargetPrepare(&satTarget,0xf400,80)||
    !willowWindowSurfaceLoad(&window,&lease,fixtureWindow)||
    !willowNativeObjectLatchCapture(ram,fixtureBank)||!fixtureLatch(ram))return 0;
 WillowObjectSatResult result=build();
 if(!accepted(&result)||!ready()||
    !willowWindowSurfacePublish(&window,&windowTarget,&presentation)||
    !willowNativeSatPublish(&satTarget,staged,result.pieces,&presentation))return 0;
 fixtureEpoch.latch=2;
 return 1;
}

uint16_t nativeObjectPresentation(void)
{
 volatile uint8_t*ram=(volatile uint8_t*)0xff0000;
 if(!coldPrepare(ram))return 0;
 *(volatile uint16_t*)0xff0004=1;
 /* Certified empty constructor initializes output in the grouped builder. */
 *(volatile uint16_t*)0xff0004=2;
 if(!willowNativeObjectLatchCapture(ram,fixtureBank))return 0;
 *(volatile uint16_t*)0xff0004=3;
 if(!fixtureLatch(ram))return 0;
 WillowObjectSatResult result=build();
 if(!accepted(&result)||window.dirty||!ready()||
    !willowWindowSurfacePublish(&window,&windowTarget,&presentation))return 0;
 *(volatile uint16_t*)0xff0004=4;
 for(unsigned n=0;n<result.pieces*4u;n++)if(staged[n]!=fixtureExpected[n])return 0;
 unsigned maxSprites=0,maxPixels=0;
 for(unsigned n=0;n<224;n++){
  if(budget.sprites[n]>maxSprites)maxSprites=budget.sprites[n];
  if(budget.pixels[n]>maxPixels)maxPixels=budget.pixels[n];
 }
 if(maxSprites!=FIXTURE_MAX_SPRITES||maxPixels!=FIXTURE_MAX_PIXELS)return 0;
 *(volatile uint16_t*)0xff0004=5;
 if(!willowNativeSatPublish(&satTarget,staged,result.pieces,&presentation))return 0;
 *(volatile uint16_t*)0xff0004=6;
 return 1;
}
