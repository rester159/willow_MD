/* Frozen source-verified frame: prepare native words once, then publish only
 * dirty rows. No source writer hook or live scene/resource ownership claimed. */
#include "willow_native_window_surface.h"
#include "window_surface_fixture.h"
static WillowWindowSurface surface;
static WillowWindowSurfaceTarget target;
static volatile uint32_t epoch;
static WillowPresentationLease lease;
static WillowPresentationFrame frame;
static const WillowPresentationPlan plan={fixtureColors,0,0,7};
uint16_t nativeObjectPresentation(void)
{
 epoch=77;lease.plan=&plan;lease.epoch=&epoch;lease.generation=77;
 frame.lease=&lease;frame.generation=77;frame.ready=7;
 *(volatile uint16_t*)0xff0004=1;
 if(!willowWindowSurfaceTargetInit(&target,0xd000))return 0;
 *(volatile uint16_t*)0xff0004=2;
 if(!willowWindowSurfaceLoad(&surface,&lease,fixtureWindow))return 0;
 *(volatile uint16_t*)0xff0004=3;
 if(!willowWindowSurfacePublish(&surface,&target,&frame))return 0;
 *(volatile uint16_t*)0xff0004=4;
 if(!willowWindowSurfacePublish(&surface,&target,&frame))return 0;
 *(volatile uint16_t*)0xff0004=5;
 /* Mark one restored row dirty. The last phase includes both cell updates
  * and row publication and finishes on the independent expected pixels. */
 if(!willowWindowSurfaceSet(&surface,5,4,fixtureWindow[165]^1)||
    !willowWindowSurfaceSet(&surface,5,4,fixtureWindow[165]))return 0;
 if(!willowWindowSurfacePublish(&surface,&target,&frame))return 0;
 *(volatile uint16_t*)0xff0004=6;
 for(unsigned n=0;n<1120;n++)if(surface.words[n]!=fixtureWindow[n])return 0;
 return surface.dirty==0;
}
