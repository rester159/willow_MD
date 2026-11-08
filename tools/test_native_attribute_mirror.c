#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/willow_native_attribute_mirror.h"
static struct {uint8_t before[16];WillowAttributeMirror mirror;uint8_t after[16];} arena;
static uint16_t expected[WILLOW_TILE_MIRROR_CELLS];
static uint32_t seed=0x135486af;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void guards(void){for(unsigned i=0;i<16;i++)assert(arena.before[i]==0xa5&&arena.after[i]==0xa5);}
int main(void){
 assert(sizeof(WillowAttributeMirror)==WILLOW_TILE_MIRROR_DENSE_BYTES);
 memset(&arena,0xa5,sizeof arena);
 /* Exercise every attribute, including all palette/priority/flip combinations,
  * at both nibble parities. A nonzero neighbor catches accidental clearing. */
 for(unsigned parity=0;parity<2;parity++){
  unsigned cell=1374+parity,neighbor=cell^1;
  assert(willowAttributeMirrorWrite(&arena.mirror,neighbor,0x9ab7));
  for(unsigned attribute=0;attribute<65536;attribute++){
   assert(willowAttributeMirrorWrite(&arena.mirror,cell,attribute));
   assert(willowAttributeMirrorRead(&arena.mirror,cell)==attribute);
   assert(willowAttributeMirrorOwner(&arena.mirror,cell)==(attribute&2047));
   assert(willowAttributeMirrorRead(&arena.mirror,neighbor)==0x9ab7);
  }
 }
 for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++){expected[i]=rnd();assert(willowAttributeMirrorWrite(&arena.mirror,i,expected[i]));}
 for(unsigned n=0;n<131072;n++){
  unsigned cell=rnd()%WILLOW_TILE_MIRROR_CELLS;expected[cell]=rnd();
  assert(willowAttributeMirrorWrite(&arena.mirror,cell,expected[cell]));
  if(!(n%256))for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++)assert(willowAttributeMirrorRead(&arena.mirror,i)==expected[i]);
 }
 /* Reassign the entire metadata tail; ownership must neither read nor write it. */
 memset(arena.mirror.attributes,0x3c,sizeof arena.mirror.attributes);
 for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++)assert(willowAttributeMirrorOwner(&arena.mirror,i)==(expected[i]&2047));
 for(unsigned n=0;n<131072;n++){
  unsigned cell=rnd()%WILLOW_TILE_MIRROR_CELLS;expected[cell]=rnd()&2047;
  assert(willowAttributeMirrorWriteOwner(&arena.mirror,cell,expected[cell]));
  assert(willowAttributeMirrorOwner(&arena.mirror,cell)==expected[cell]);
 }
 for(unsigned i=0;i<sizeof arena.mirror.attributes;i++)assert(arena.mirror.attributes[i]==0x3c);
 for(unsigned i=0;i<WILLOW_TILE_MIRROR_CELLS;i++)assert(willowAttributeMirrorOwner(&arena.mirror,i)==(expected[i]&2047));
 __typeof__(arena) saved=arena;
 assert(!willowAttributeMirrorWrite(&arena.mirror,WILLOW_TILE_MIRROR_CELLS,0));
 assert(!willowAttributeMirrorWriteOwner(&arena.mirror,0,2048));
 assert(!willowAttributeMirrorWriteOwner(&arena.mirror,WILLOW_TILE_MIRROR_CELLS,0));
 assert(!memcmp(&arena,&saved,sizeof arena));guards();
 puts("Split attribute mirror: all 65536 attributes at both parities, 131072 full writes, 131072 ownership writes; exact 5504-byte size and 1376-byte retired-tail guards passed");
}
