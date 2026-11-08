#include "../src/willow_native_window_plan.h"
unsigned willowWindowTestBuild(WillowWindowPlan*p,const uint16_t*a,const uint16_t*b,
 const WillowPresentationLease*l,const WillowWindowPriorityProof*f,
 WillowWindowRead r,void*rc,WillowWindowAsset t,void*tc)
{return willowWindowPlanBuild(p,a,b,l,f,r,rc,t,tc);}
unsigned willowWindowTestPrepare(const WillowWindowPlan*p,WillowWindowRead r,
 void*rc,WillowWindowAsset a,void*ac,WillowWindowStage s,void*sc)
{return willowWindowMapPrepare(p,r,rc,a,ac,s,sc);}
unsigned willowWindowTestCurrent(const WillowWindowPlan*p)
{return willowWindowCurrent(p);}
unsigned willowWindowTestPriority(WillowWindowPlan*p,uint16_t*out)
{return willowWindowObjectPriority(p,0,0,0,0,out);}
unsigned willowWindowTestActivate(WillowPresentationLease*l,const WillowPresentationPlan*p,
 const volatile uint32_t*e,WillowPresentationRead r,void*c,const uint16_t*cram)
{return willowPresentationActivate(l,p,e,r,c,cram);}
