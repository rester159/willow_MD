#include "../src/willow_native_tile_slots.h"
#include <stdio.h>
#include <string.h>
static uint16_t reference(const WillowTileSlots*s,unsigned count)
{
 for(unsigned start=0;start+count<=WILLOW_TILE_SLOTS;start++){
  unsigned n=0;while(n<count && (s->available[(start+n)>>4]&(1u<<((start+n)&15))))n++;
  if(n==count)return start;
 }
 return 0xffff;
}
int main(void)
{
 WillowTileSlots s;unsigned checks=0;uint32_t rng=0x57494c4c;
 for(unsigned sample=0;sample<4096;sample++){
  memset(&s,0,sizeof s);
  for(unsigned w=0;w<WILLOW_TILE_SLOT_WORDS;w++){rng=rng*1664525u+1013904223u;s.available[w]=rng>>16;}
  for(unsigned n=1;n<=16;n++){
   uint16_t actual=willowTileSlotsFindRun(&s,n),expected=reference(&s,n);
   if(actual!=expected){fprintf(stderr,"Random case %u width %u: %u/%u\n",sample,n,actual,expected);return 1;}
   checks++;
  }
 }
 for(unsigned start=0;start<WILLOW_TILE_SLOTS;start++)for(unsigned n=1;n<=16;n++){
  memset(&s,0,sizeof s);
  for(unsigned i=0;i<n && start+i<WILLOW_TILE_SLOTS;i++)s.available[(start+i)>>4]|=1u<<((start+i)&15);
  if(willowTileSlotsFindRun(&s,n)!=(start+n<=WILLOW_TILE_SLOTS?start:0xffff))return 1;
  checks++;
 }
 if(willowTileSlotsFindRun(&s,0)!=0xffff || willowTileSlotsFindRun(&s,17)!=0xffff)return 1;
 printf("Contiguous tile allocation: %u random, boundary-crossing and end-of-cache cases match linear reference\n",checks);
 return 0;
}
