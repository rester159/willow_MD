#include "../src/willow_native_scroll1_tiles.h"
unsigned willowScroll1TestValid(const WillowScroll1Plan*p)
{return willowScroll1PlanValid(p);}
unsigned willowPresentationTestActivate(WillowPresentationLease*l,
 const WillowPresentationPlan*p,const volatile uint32_t*epoch,
 WillowPresentationRead read,void*context,const uint16_t*cram)
{return willowPresentationActivate(l,p,epoch,read,context,cram);}
unsigned willowPresentationTestCurrent(const WillowPresentationLease*l)
{return willowPresentationCurrent(l);}
unsigned willowPresentationTestBegin(WillowPresentationFrame*f,const WillowPresentationLease*l)
{return willowPresentationBegin(f,l);}
unsigned willowPresentationTestPrepared(WillowPresentationFrame*f,uint16_t participant)
{return willowPresentationPrepared(f,participant);}
unsigned willowPresentationTestCanPublish(const WillowPresentationFrame*f)
{return willowPresentationCanPublish(f);}
unsigned willowScroll1TestBind(WillowScroll1Binding*b,const WillowScroll1Plan*p,
 const WillowPresentationLease*l,WillowScroll1Acquire acquire,void*context)
{return willowScroll1Bind(b,p,l,acquire,context);}
unsigned willowScroll1TestResolve(WillowScroll1Binding*b,uint16_t code,
 uint16_t attribute,uint8_t layout,uint16_t priority,uint16_t*out)
{return willowScroll1Resolve(b,code,attribute,layout,priority,out);}
unsigned willowScroll1TestRow(const uint16_t*attributes,const uint8_t*present,
 int16_t y,WillowObjectSatBudget*budget,WillowScroll1SatPiece*out,
 uint16_t capacity,uint16_t*count)
{return willowScroll1SatRow(attributes,present,y,budget,out,capacity,count);}
unsigned willowScroll1TestWords(const WillowScroll1SatPiece*p,uint16_t link,uint16_t*out)
{return willowScroll1SatWords(p,link,out);}
void willowScroll1TestObjects(const uint16_t*bank,WillowObjectSatBudget*budget,
 WillowObjectSatPiece*out,WillowObjectSatAsset asset,void*context,
 WillowObjectSatResult*result)
{
 WillowObjectSatView view={96,16,320,224,0};
 *result=willowObjectSatBuild(bank,1024,&view,budget,out,80,asset,context);
}
