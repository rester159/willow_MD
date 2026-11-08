#include "../src/willow_native_tile_refs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static WillowTileRefs refs;
static unsigned short expected[WILLOW_REF_SLOTS];
static void check(void)
{
 unsigned used=0;
 for(unsigned i=0;i<WILLOW_REF_SLOTS;i++){
  assert(willowRefGet(&refs,i)==expected[i]);
  assert(!!willowRefNonzero(&refs,i)==!!expected[i]);
 }
 for(unsigned n=0;n<WILLOW_REF_UPPERS;n++)if(refs.owner[n]){
  unsigned slot=refs.owner[n]-1;assert(slot<WILLOW_REF_SLOTS);
  assert(expected[slot]>255 && refs.high[n]==expected[slot]>>8);used++;
  for(unsigned j=n+1;j<WILLOW_REF_UPPERS;j++)assert(refs.owner[j]!=refs.owner[n]);
 }
 unsigned high=0;for(unsigned i=0;i<WILLOW_REF_SLOTS;i++)high+=expected[i]>255;
 assert(used==high);
}
int main(void)
{
 _Static_assert(sizeof(WillowTileRefs)==1554,"unexpected compact storage");
 /* Every slot and every 16-bit value, with both arithmetic wrap boundaries. */
 unsigned long checks=0;
 for(unsigned slot=0;slot<WILLOW_REF_SLOTS;slot++){
  for(unsigned value=0;value<65536;value++){
   assert(willowRefSet(&refs,slot,value));
   assert(willowRefGet(&refs,slot)==value);
   assert(willowRefIncrement(&refs,slot));
   assert(willowRefGet(&refs,slot)==(unsigned short)(value+1));
   assert(willowRefDecrement(&refs,slot));
   assert(willowRefGet(&refs,slot)==value);checks++;
  }
  assert(willowRefSet(&refs,slot,0));
 }
 /* Fully occupied upper-byte directory; rejection must be atomic. */
 for(unsigned n=0;n<32;n++){unsigned slot=n*37;expected[slot]=256+n*123;assert(willowRefSet(&refs,slot,expected[slot]));}
 check();WillowTileRefs before=refs;
 assert(!willowRefSet(&refs,1295,256));assert(!memcmp(&before,&refs,sizeof refs));
 assert(willowRefSet(&refs,1295,255));before=refs;
 assert(!willowRefIncrement(&refs,1295));assert(!memcmp(&before,&refs,sizeof refs));
 expected[1295]=255;check();
 for(unsigned n=0;n<32;n++){
  unsigned slot=n*37;assert(willowRefSet(&refs,slot,0));expected[slot]=0;
  unsigned other=1295-n;assert(willowRefSet(&refs,other,65535));expected[other]=65535;check();
 }
 for(unsigned n=0;n<32;n++){unsigned slot=1295-n;assert(willowRefIncrement(&refs,slot));expected[slot]=0;check();}
 puts("Sparse tile references: full upper-directory rejection/reuse and 16-bit wrapping preserve all other counters");
 printf("%lu slot/value cases pass; 1554 bytes versus 2592 dense bytes (1038 saved), not yet renderer-integrated or target-timed\n",checks);
 return 0;
}
