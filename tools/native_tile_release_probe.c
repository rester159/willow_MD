#include "../src/willow_native_tile_slots.h"
#define SLOTS ((WillowTileSlots*)0xff2000UL)
void releaseTransition(uint16_t slot){willowTileSlotsRelease(SLOTS,slot);}
void releaseAlways(uint16_t slot)
{
 unsigned word=slot>>4;
 SLOTS->available[word]|=willowTileBit[slot&15];
 SLOTS->summary[word>>4]|=willowTileBit[word&15];
}
