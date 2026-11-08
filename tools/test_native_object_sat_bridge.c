/* Host/shared-library adapter; the consumer itself is freestanding 68000 C. */
#include "../src/willow_native_object_sat.h"
void willowObjectSatTestBuild(const uint16_t*bank,uint16_t words,
 const WillowObjectSatView*v,WillowObjectSatBudget*b,WillowObjectSatPiece*out,
 uint16_t capacity,WillowObjectSatAsset asset,void*context,
 WillowObjectSatResult*result,uint16_t*sat)
{
 *result=willowObjectSatBuild(bank,words,v,b,out,capacity,asset,context);
 if(!result->status)for(unsigned i=0;i<result->pieces;i++)
  willowObjectSatWords(out+i,i+1<result->pieces?i+1:0,sat+i*4);
}
unsigned willowObjectSatTestReserve(WillowObjectSatBudget*b,int y,
 unsigned width,unsigned height)
{return willowObjectSatReserve(b,y,width,height);}
