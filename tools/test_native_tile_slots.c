#include "../src/willow_native_tile_slots.h"
#include <stdio.h>
#include <string.h>
static WillowTileSlots slots;
static uint8_t available[WILLOW_TILE_SLOTS];
static uint16_t cursor;
static int verify(void){
 for(unsigned n=0;n<WILLOW_TILE_SLOTS;n++)
  if(!!(slots.available[n>>4]&(1u<<(n&15)))!=available[n])return 0;
 for(unsigned n=0;n<WILLOW_TILE_SLOT_WORDS;n++)
  if(!!(slots.summary[n>>4]&(1u<<(n&15)))!=!!slots.available[n])return 0;
 return slots.summary[5]<=1 && slots.cursor==cursor;
}
static int find(void){
 uint16_t expected=0xffff;
 for(unsigned n=0;n<WILLOW_TILE_SLOTS;n++){
  unsigned at=(cursor+n)%WILLOW_TILE_SLOTS;
  if(available[at]){expected=at;break;}
 }
 if(willowTileSlotsFind(&slots)!=expected)return 0;
 if(expected!=0xffff){cursor=(expected+1)%WILLOW_TILE_SLOTS;available[expected]=0;willowTileSlotsRemove(&slots,expected);}
 return verify();
}
int main(void){
 willowTileSlotsInit(&slots);memset(available,1,sizeof available);
 for(unsigned n=0;n<WILLOW_TILE_SLOTS+1;n++)if(!find())return 1;
 uint32_t random=0x57494c4c;
 for(unsigned n=0;n<65536;n++){
  random=random*1664525u+1013904223u;unsigned at=(random>>12)%WILLOW_TILE_SLOTS;
  if(n&1){available[at]=1;willowTileSlotsRelease(&slots,at);}
  else{available[at]=0;willowTileSlotsRemove(&slots,at);}
  if(!verify() || !find()){fprintf(stderr,"Tile-slot mismatch iteration %u\n",n);return 1;}
 }
 uint16_t refs[WILLOW_TILE_SLOTS];
 for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<WILLOW_TILE_SLOTS;n++){
   refs[n]=(n+sample)%7?(uint16_t)(1+((sample*977+n)%65535)):0;
   available[n]=!refs[n];
  }
  slots.cursor=cursor=sample*5;willowTileSlotsRebuild(&slots,refs);
  if(!verify()||!find())return 1;
 }
 printf("Tile slots: full exhaustion, 65536 release/remove/search operations and 256 bulk rebuilds match rotating reference; %u-byte index\n",(unsigned)sizeof slots);
 return 0;
}
