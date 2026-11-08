#include "../src/willow_native_object_sat_fast.h"
static uint8_t readBank(void*context,uint16_t n,uint16_t*out)
{const uint16_t*p=(const uint16_t*)context+n*4;for(unsigned i=0;i<4;i++)out[i]=p[i];return 1;}
void willowObjectSatFastTestBuild(const uint16_t*bank,uint16_t words,
 const WillowObjectSatView*v,WillowObjectSatBudget*b,WillowObjectSatPiece*out,
 uint16_t capacity,WillowObjectSatAsset asset,void*context,
 WillowObjectSatResult*result,uint16_t*sat)
{
 int8_t deltas[225];
 if(words&3){WillowObjectSatResult invalid={1,0,0,0,0,0};*result=invalid;return;}
 *result=willowObjectSatBuildReaderFast(readBank,(void*)bank,words/4,v,b,out,capacity,asset,context,deltas);
 if(!result->status)for(unsigned n=0;n<result->pieces;n++)
  willowObjectSatWords(out+n,n+1<result->pieces?n+1:0,sat+n*4);
}
unsigned willowObjectSatFastTestReserve(WillowObjectSatBudget*b,int y,unsigned w,unsigned h)
{return willowObjectSatReserve(b,y,w,h);}
