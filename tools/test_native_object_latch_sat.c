/* Host-only adapters; the production entry allocates only one8-byte record. */
#include "../src/willow_native_object_latch_sat.h"
#include <string.h>
void willowObjectSatTestBuildReader(WillowObjectSatReadRecord read,void*bankContext,
 uint16_t records,const WillowObjectSatView*v,WillowObjectSatBudget*b,
 WillowObjectSatPiece*out,uint16_t capacity,WillowObjectSatAsset asset,void*context,
 WillowObjectSatResult*result,uint16_t*sat)
{
 *result=willowObjectSatBuildReader(read,bankContext,records,v,b,out,capacity,asset,context);
 if(!result->status)for(unsigned i=0;i<result->pieces;i++)
  willowObjectSatWords(out+i,i+1<result->pieces?i+1:0,sat+i*4);
}
void willowObjectSatTestBuildScattered(const uint16_t*bank,
 const WillowObjectSatView*v,WillowObjectSatBudget*b,WillowObjectSatPiece*out,
 uint16_t capacity,WillowObjectSatAsset asset,void*context,
 WillowObjectSatResult*result,uint16_t*sat)
{
 /* Host-only backing. No such arrays exist in the native runtime adapter. */
 static uint32_t memory[16384],source[512];
 for(unsigned n=0;n<16384;n++)memory[n]=0xa5a5a5a5;
 memcpy(source,bank,sizeof source);
 if(!willowNativeObjectLatchCapture((volatile uint8_t*)memory,source))return;
 memset(source,0,sizeof source);
 *result=willowNativeObjectLatchSatBuild((volatile uint8_t*)memory,v,b,out,capacity,asset,context);
 if(!result->status)for(unsigned i=0;i<result->pieces;i++)
  willowObjectSatWords(out+i,i+1<result->pieces?i+1:0,sat+i*4);
}
void willowObjectSatTestBuildReaderDeferred(WillowObjectSatReadRecord read,void*bankContext,
 uint16_t records,const WillowObjectSatView*v,WillowObjectSatBudget*b,
 WillowObjectSatPiece*out,uint16_t capacity,WillowObjectSatAsset asset,void*context,
 WillowObjectSatResult*result,uint16_t*sat)
{
 int8_t deltas[225];
 *result=willowObjectSatBuildReaderDeferred(read,bankContext,records,v,b,out,capacity,asset,context,deltas);
 if(!result->status)for(unsigned i=0;i<result->pieces;i++)
  willowObjectSatWords(out+i,i+1<result->pieces?i+1:0,sat+i*4);
}
