#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/willow_native_tile_lookup_packed.h"
static WillowTileLookup lookup;
static uint16_t ids[1296],oracle[65536];
static uint32_t seed=0x195347ab;
static uint32_t randomWord(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void reset(void){memset(&lookup,0,sizeof lookup);memset(lookup.heads,255,sizeof lookup.heads);memset(ids,255,sizeof ids);memset(oracle,255,sizeof oracle);}
static void replace(unsigned slot,uint16_t id){
 if(ids[slot]!=65535){willowTileLookupRemove(&lookup,ids,ids[slot]);oracle[ids[slot]]=65535;}
 ids[slot]=id;willowTileLookupInsert(&lookup,ids,slot);oracle[id]=slot;
}
static void verify(void){
 for(unsigned id=0;id<65535;id++)assert(willowTileLookup(&lookup,ids,id)==oracle[id]);
 unsigned count=0;
 for(unsigned i=0;i<512;i++){unsigned slot=lookup.heads[i];while(slot!=65535){assert(slot<1296);assert((ids[slot]&511)==i);assert(oracle[ids[slot]]==slot);assert(++count<=1296);slot=willowTileNext(&lookup,slot);}}
 unsigned expected=0;for(unsigned i=0;i<1296;i++)expected+=ids[i]!=65535;
 assert(count==expected);
}
int main(void){
 assert(sizeof lookup==2808);
 for(unsigned index=0;index<1296;index++)for(unsigned slot=0;slot<=1296;slot++){
  uint16_t value=slot==1296?65535:slot;memset(&lookup,0,sizeof lookup);
  willowTileNextPut(&lookup,index,value);assert(willowTileNext(&lookup,index)==value);
  if(index)assert(willowTileNext(&lookup,index-1)==65535);
  if(index<1295)assert(willowTileNext(&lookup,index+1)==65535);
 }
 reset();
 /* Long collision chains, then delete every position. */
 unsigned count=0;
 for(unsigned id=0;id<65535 && count<1296;id++)if(willowTileHash(id)>=500)replace(count++,id);
 verify();
 for(unsigned slot=0;slot<count;slot++){
  willowTileLookupRemove(&lookup,ids,ids[slot]);oracle[ids[slot]]=65535;ids[slot]=65535;
  if(!(slot%32))verify();
 }
 verify();reset();
 for(unsigned slot=0;slot<1296;slot++)replace(slot,slot);
 for(unsigned n=0;n<262144;n++){
  uint16_t id=randomWord()%65535;
  if(oracle[id]!=65535)assert(willowTileLookup(&lookup,ids,id)==oracle[id]);
  else replace(randomWord()%1296,id);
  if(!(n%1024))verify();
 }
 verify();puts("Packed tile lookup: 262144 churn operations, exhaustive ID checks and collision-chain deletions passed");
}
