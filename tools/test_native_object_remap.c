#include "../src/willow_native_object_remap.h"
unsigned willowRemapTestValid(const WillowObjectRemapPlan*p)
{return willowObjectRemapPlanValid(p);}
unsigned willowRemapTestBake(const uint8_t*source,const uint8_t*pens,uint8_t*out)
{return willowObjectRemapBake(source,pens,out);}
unsigned willowRemapTestActivate(WillowObjectRemapLease*l,
 const WillowObjectRemapPlan*p,const volatile uint32_t*epoch,
 WillowObjectRemapReadPalette read,void*context,const uint16_t*cram)
{return willowObjectRemapActivate(l,p,epoch,read,context,cram);}
unsigned willowRemapTestCurrent(const WillowObjectRemapLease*l)
{return willowObjectRemapLeaseCurrent(l);}
unsigned willowRemapTestAsset(WillowObjectRemapBinding*b,uint16_t code,
 uint16_t source,uint16_t*out)
{return willowObjectRemapSatAsset(b,code,source,0,0,out);}
void willowRemapTestBuild(const uint16_t*bank,const WillowObjectSatView*v,
 WillowObjectSatBudget*b,WillowObjectSatPiece*out,WillowObjectRemapBinding*binding,
 WillowObjectSatResult*result,uint16_t*sat)
{
 *result=willowObjectSatBuild(bank,1024,v,b,out,80,willowObjectRemapSatAsset,binding);
 if(!result->status)for(unsigned i=0;i<result->pieces;i++)
  willowObjectSatWords(out+i,i+1<result->pieces?i+1:0,sat+i*4);
}
