#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/willow_native_tile_mirror.h"
static struct {uint8_t before[16];uint16_t words[WILLOW_TILE_MIRROR_CELLS];uint8_t after[16];} arena;
static uint16_t expected[WILLOW_TILE_MIRROR_CELLS];
static uint32_t seed=0x718a443d;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void verify(uint16_t converted){
 for(unsigned i=0;i<sizeof arena.before;i++)assert(arena.before[i]==0xa5&&arena.after[i]==0xa5);
 for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++){
  assert(willowTileMirrorRead(arena.words,converted,i)==(expected[i]&2047));
  if(i>=converted)assert(arena.words[i]==expected[i]);
 }
}
int main(void){
 assert(WILLOW_TILE_MIRROR_DENSE_BYTES-WILLOW_TILE_MIRROR_PACKED_BYTES==1376);
 /* Every possible tile value in every position, preserving both neighbors. */
 uint8_t packed[WILLOW_TILE_MIRROR_PACKED_BYTES];memset(packed,0,sizeof packed);
 for(unsigned cell=0;cell<WILLOW_TILE_MIRROR_CELLS;cell++){
  for(unsigned value=0;value<2048;value++){
   willowTileMirrorPackedWrite(packed,cell,value);
   assert(willowTileMirrorPackedRead(packed,cell)==value);
   if(cell)assert(willowTileMirrorPackedRead(packed,cell-1)==0);
   if(cell+1<WILLOW_TILE_MIRROR_CELLS)assert(willowTileMirrorPackedRead(packed,cell+1)==0);
  }
  willowTileMirrorPackedWrite(packed,cell,0);
 }
 for(unsigned scenario=0;scenario<16;scenario++){
  memset(&arena,0xa5,sizeof arena);
  for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++)arena.words[i]=expected[i]=rnd();
  uint16_t converted=0;verify(converted);
  while(converted<WILLOW_TILE_MIRROR_CELLS){
   unsigned budget=scenario?1+rnd()%73:1;
   assert(willowTileMirrorConvert(arena.words,&converted,budget));
   for(unsigned n=0;n<9;n++){
    unsigned cell=rnd()%WILLOW_TILE_MIRROR_CELLS;uint16_t attribute=rnd();
    assert(willowTileMirrorWrite(arena.words,converted,cell,attribute));expected[cell]=attribute;
   }
   verify(converted);
  }
  /* Scheduler data may claim the tail only after full conversion. */
  memset((uint8_t*)arena.words+WILLOW_TILE_MIRROR_PACKED_BYTES,0x3c,1376);
  for(unsigned n=0;n<8192;n++){
   unsigned cell=rnd()%WILLOW_TILE_MIRROR_CELLS;uint16_t attribute=rnd();
   assert(willowTileMirrorWrite(arena.words,converted,cell,attribute));expected[cell]=attribute;
  }
  verify(converted);
  for(unsigned i=WILLOW_TILE_MIRROR_PACKED_BYTES;i<WILLOW_TILE_MIRROR_DENSE_BYTES;i++)assert(((uint8_t*)arena.words)[i]==0x3c);
  __typeof__(arena) saved=arena;
  assert(!willowTileMirrorWrite(arena.words,converted,WILLOW_TILE_MIRROR_CELLS,1));
  converted=WILLOW_TILE_MIRROR_CELLS+1;assert(!willowTileMirrorConvert(arena.words,&converted,1));
  assert(!memcmp(&arena,&saved,sizeof arena));
 }
 puts("Tile ownership mirror: all 2048 indices in 2752 cells, 16 interleaved conversions, 131072 post-conversion writes and reclaimed-tail guards passed; saves 1376 bytes");
}
