#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/willow_native_tile_mirror_transpose.h"
static struct {uint8_t before[16],bytes[WILLOW_TILE_MIRROR_DENSE_BYTES],after[16];} arena;
static uint16_t expected[WILLOW_TILE_MIRROR_CELLS];
static uint32_t seed=0x65981234;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static unsigned mapping(unsigned cell){unsigned plane=cell/1376,col=(cell%1376)%43,row=(cell%1376)/43;return (plane*43+col)*32+row;}
static void verify(const WillowMirrorTranspose*s){
 for(unsigned i=0;i<16;i++)assert(arena.before[i]==0xa5&&arena.after[i]==0xa5);
 for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++)assert(willowMirrorTransposeRead(arena.bytes,s,i)==expected[i]);
 for(unsigned i=WILLOW_TILE_MIRROR_PACKED_BYTES+WILLOW_MIRROR_TRANSPOSE_SCRATCH;i<sizeof arena.bytes;i++)assert(arena.bytes[i]==0xa5);
}
int main(void){
 unsigned heldWrites=0,steps=0,checks=0;
 uint8_t destinations[WILLOW_TILE_MIRROR_CELLS]={0};
 for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++){unsigned dest=willowMirrorColumnCell(i);assert(dest==mapping(i));assert(!destinations[dest]);destinations[dest]=1;}
 for(unsigned scenario=0;scenario<16;scenario++){
  memset(&arena,0xa5,sizeof arena);
  for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++){expected[i]=rnd()&2047;willowTileMirrorPackedWrite(arena.bytes,i,expected[i]);}
  WillowMirrorTranspose state;willowMirrorTransposeInit(arena.bytes,&state);verify(&state);
  unsigned units=0;
  while(state.scan<WILLOW_TILE_MIRROR_CELLS){
   WillowMirrorTranspose old=state;
   assert(!willowMirrorTransposeStep(arena.bytes,&state,0));assert(!memcmp(&state,&old,sizeof state));
   unsigned budget=scenario?1+rnd()%61:1;units+=budget;assert(units<6000);
   willowMirrorTransposeStep(arena.bytes,&state,budget);steps++;
   for(unsigned n=0;n<9;n++){
    unsigned cell=n==0&&state.origin!=WILLOW_MIRROR_NO_HELD?state.origin:n==1?state.start:rnd()%WILLOW_TILE_MIRROR_CELLS;
    uint16_t attribute=rnd();if(cell==state.origin)heldWrites++;
    assert(willowMirrorTransposeWrite(arena.bytes,&state,cell,attribute));expected[cell]=attribute&2047;
   }
   verify(&state);checks+=WILLOW_TILE_MIRROR_CELLS;
  }
  assert(state.origin==WILLOW_MIRROR_NO_HELD);
  for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++){assert(willowTileMirrorPackedRead(arena.bytes,mapping(i))==expected[i]);assert(willowMirrorTransposeVisited(arena.bytes,i));}
  assert(willowMirrorTransposeStep(arena.bytes,&state,123));
  __typeof__(arena) saved=arena;WillowMirrorTranspose savedState=state;
  assert(!willowMirrorTransposeWrite(arena.bytes,&state,WILLOW_TILE_MIRROR_CELLS,0));
  assert(!memcmp(&arena,&saved,sizeof arena)&&!memcmp(&state,&savedState,sizeof state));
 }
 assert(heldWrites>1000);
 printf("Incremental mirror transpose: %u batches, %u logical reads, %u live displaced-tile writes; all 16 permutations and scratch guards passed\n",steps,checks,heldWrites);
}
