#include "../src/willow_native_object_tiles.h"
unsigned willowObjectTilesTestValid(const WillowObjectTileAtlas*a)
{return willowObjectTileAtlasValid(a);}
unsigned willowObjectTilesTestAsset(WillowObjectTileBinding*b,uint16_t code,
 uint16_t source,uint16_t*out)
{return willowObjectTileSatAsset(b,code,source,0,0,out);}
void willowObjectTilesTestBuild(const uint16_t*bank,const WillowObjectSatView*v,
 WillowObjectSatBudget*b,WillowObjectSatPiece*out,WillowObjectTileBinding*binding,
 WillowObjectSatResult*result,uint16_t*sat)
{
 *result=willowObjectSatBuild(bank,1024,v,b,out,80,willowObjectTileSatAsset,binding);
 if(!result->status)for(unsigned i=0;i<result->pieces;i++)
  willowObjectSatWords(out+i,i+1<result->pieces?i+1:0,sat+i*4);
}
