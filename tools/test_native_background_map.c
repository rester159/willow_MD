#include "native_background_fixture.h"
int main(void){
 uint16_t packed[WILLOW_MAP14_WORDS];
 FILE*p=fopen("build/reference_maincpu.bin","rb"),*a=fopen("build/arcade-trace/camera-native/frames.bin","rb"),
 *b=fopen("build/arcade-trace/player-walk/frames.bin","rb"),*c=fopen("build/arcade-trace/scroll3-walk/frames.bin","rb"),
 *d=fopen("build/arcade-trace/scroll3-walk/maps.bin","rb");
 assert(p&&a&&b&&c&&d);assert(fread(rom,1,sizeof rom,p)==sizeof rom);fclose(p);
 unsigned checks=0;
 for(unsigned frame=700;frame<=1700;frame++){
  uint8_t*record;
  if(frame<=1620){assert(!fseek(a,(frame-650)*sizeof baseline,SEEK_SET));assert(fread(baseline,1,sizeof baseline,a)==sizeof baseline);record=baseline;}
  else{assert(!fseek(b,(frame-1587)*sizeof walk,SEEK_SET));assert(fread(walk,1,sizeof walk,b)==sizeof walk);record=walk;}
  assert(willowMotionLong(record)==frame);
  assert(!fseek(c,(frame-650)*sizeof globals,SEEK_SET));assert(fread(globals,1,sizeof globals,c)==sizeof globals);
  assert(!fseek(d,(frame-650)*sizeof distant,SEEK_SET));assert(fread(distant,1,sizeof distant,d)==sizeof distant);
  for(unsigned cell=0;cell<4096;cell++){
   near[cell]=idOf(willowMotionLong(record+1286+cell*4),nativeScroll2Pairs,WILLOW_SCROLL2_PAIR_COUNT);
   assert(willowScroll3MapSet(far,cell,idOf(willowMotionLong(distant+cell*4),nativeScroll3Pairs,WILLOW_SCROLL3_PAIR_COUNT)));
  }
  willowMap14Copy(packed,0,near,4096);
  for(unsigned layer=2;layer<=3;layer++){
   uint8_t*r=globals+4+(layer==2?0x402:0x482);
   uint16_t cx=willowMotionWord(r+6),cy=willowMotionWord(r+10);
   const uint16_t*offsets=layer==2?nativeBackground2Offsets:nativeBackground3Offsets;
   const uint16_t*entries=layer==2?nativeBackground2Entries:nativeBackground3Entries;
   const uint32_t*pairs=layer==2?nativeScroll2Pairs:nativeScroll3Pairs;
   unsigned count=layer==2?WILLOW_SCROLL2_PAIR_COUNT:WILLOW_SCROLL3_PAIR_COUNT;
   for(unsigned py=0;py<224;py+=8)for(int px=-8;px<336;px+=8){
    uint16_t x=32+cx+px,y=py-cy-240,actual;
    uint32_t expectedPair=sourcePair(layer,x,y);
    uint16_t cell=layer==2?willowBackground2Cell(x,y):willowBackground3Cell(x,y);
    uint16_t id=layer==2?near[cell]:willowScroll3MapId(far,cell);
    if(pairs[id]!=expectedPair){fprintf(stderr,"Live pair mismatch frame %u layer %u xy %u,%u cell %u got %08x want %08x\n",frame,layer,x,y,cell,(unsigned)pairs[id],(unsigned)expectedPair);return 1;}
    uint16_t expectedId=idOf(expectedPair,pairs,count);unsigned size=layer==2?16:32;
    assert(offsets[expectedId]!=65535);
    uint16_t expected=entries[offsets[expectedId]+(y%size/8)*(size/8)+x%size/8];
    unsigned ok=layer==2?willowBackground2Entry(near,offsets,count,entries,x,y,&actual):willowBackground3Entry(far,offsets,count,entries,x,y,&actual);
    assert(ok && actual==expected);checks++;
    if(layer==2){
     uint16_t compact=0;
     assert(willowBackground2PackedEntry(packed,offsets,count,entries,x,y,&compact));
     assert(compact==expected);
    }
   }
  }
 }
 /* Ring wraps and rejection paths outside the recorded camera interval. */
 unsigned visits2[4096]={0},visits3[4096]={0};
 for(unsigned y=0;y<2048;y+=8)for(unsigned x=0;x<2048;x+=8){
  unsigned n=willowBackground2Cell(x,y),f=willowBackground3Cell(x,y);
  assert(n<4096 && f<4096);visits2[n]++;visits3[f]++;
  assert(n==willowBackground2Cell(x+1024,y+1024));
  assert(f==willowBackground3Cell(x+2048,y+2048));
 }
 for(unsigned n=0;n<4096;n++)assert(visits2[n]==16 && visits3[n]==16);
 uint16_t output=0xbeef;near[0]=0x8000;
 assert(willowMap14Write(packed,0,WILLOW_SCROLL2_PAIR_COUNT));
 assert(!willowBackground2PackedEntry(packed,nativeBackground2Offsets,WILLOW_SCROLL2_PAIR_COUNT,nativeBackground2Entries,0,0,&output) && output==0xbeef);
 assert(!willowBackground2Entry(near,nativeBackground2Offsets,WILLOW_SCROLL2_PAIR_COUNT,nativeBackground2Entries,0,0,&output) && output==0xbeef);
 assert(willowScroll3MapSet(far,0,2047));
 assert(!willowBackground3Entry(far,nativeBackground3Offsets,WILLOW_SCROLL3_PAIR_COUNT,nativeBackground3Entries,0,0,&output) && output==0xbeef);
 for(unsigned id=0;id<WILLOW_SCROLL2_PAIR_COUNT;id++)if(nativeBackground2Offsets[id]==65535){
  near[0]=id;assert(!willowBackground2Entry(near,nativeBackground2Offsets,WILLOW_SCROLL2_PAIR_COUNT,nativeBackground2Entries,0,0,&output));
  assert(willowMap14Write(packed,0,id));
  assert(!willowBackground2PackedEntry(packed,nativeBackground2Offsets,WILLOW_SCROLL2_PAIR_COUNT,nativeBackground2Entries,0,0,&output));
 }
 for(unsigned id=0;id<WILLOW_SCROLL3_PAIR_COUNT;id++)if(nativeBackground3Offsets[id]==65535){
  assert(willowScroll3MapSet(far,0,id));assert(!willowBackground3Entry(far,nativeBackground3Offsets,WILLOW_SCROLL3_PAIR_COUNT,nativeBackground3Entries,0,0,&output));
 }
 fclose(a);fclose(b);fclose(c);fclose(d);
 printf("Live background resolver: %u source pair/subtile checks across 1001 captured frames, both layers\n",checks);
 return 0;
}
