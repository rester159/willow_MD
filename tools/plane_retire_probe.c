#include "../src/willow_native_plane_retire.h"
#include "../src/willow_native_tile_slots.h"
void planeRetireProbe(void)
{
 volatile uint16_t*p=(volatile uint16_t*)0xff1000UL;
 uint16_t*references=(uint16_t*)0xff3002UL;
 if(p[0])willowNativePlaneReleaseSpan(references,(const uint16_t*)0xff4002UL,p[1],p[2],p[3]);
 willowTileSlotsRebuild((WillowTileSlots*)0xff2002UL,references);
 p[4]=0x5242;
}
