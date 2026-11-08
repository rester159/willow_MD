#include "../src/willow_arcade_scroll3_start.h"
#include "../src/willow_arcade_scroll3_map_store.h"
#include "../native/intro_probe/src/scroll3_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t rom[0x100000],raw[16384];
static uint16_t cells[WILLOW_SCROLL3_MAP_WORDS];
static uint16_t rw(uint32_t at){return willowMotionWord(rom+at);}
static uint8_t rb(uint32_t at){return rom[at];}
static uint16_t nw(uint32_t at){uint16_t v;if(!nativeScroll3WordRead(at,&v)||v!=rw(at)){fprintf(stderr,"Missing native backdrop word %x\n",at);exit(1);}return v;}
static uint8_t nb(uint32_t at){uint8_t v;if(!nativeScroll3ByteRead(at,&v)||v!=rb(at)){fprintf(stderr,"Missing native backdrop byte %x\n",at);exit(1);}return v;}
static void write(uint16_t at,uint16_t v){willowMotionPutWord(raw+at,v);}
static unsigned transfers,comparisons;
static uint8_t copy(uint16_t at,uint32_t source,uint16_t count){
 if(count!=64)return 0;
 transfers++;return willowScroll3MapCopy(cells,nativeScroll3SourceIds,at,source,count);
}
static int same(void){
 for(unsigned n=0;n<4096;n++){
  if(nativeScroll3Pairs[willowScroll3MapId(cells,n)]!=willowMotionLong(raw+n*4)){fprintf(stderr,"Backdrop map mismatch %u\n",n);return 0;}
  comparisons++;
 }
 return 1;
}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 for(unsigned n=0;n<4096;n++){unsigned id=n%WILLOW_SCROLL3_PAIR_COUNT;willowScroll3MapSet(cells,n,id);willowMotionPutLong(raw+n*4,nativeScroll3Pairs[id]);}
 uint8_t reference[128]={0},native[128]={0};
 willowMotionPutLong(reference+66,((uint32_t)rw(0x2eb40)<<16)|rw(0x2eb42));
 willowMotionPutLong(native+66,((uint32_t)nw(0x2eb40)<<16)|nw(0x2eb42));
 const unsigned chunks[]={5,9};
 for(unsigned setup=0;setup<3;setup++){
  unsigned x=setup==2?0x100:0,y=setup?0x200:0;
  willowMotionPutWord(reference+6,x);willowMotionPutWord(native+6,x);
  willowMotionPutWord(reference+10,y);willowMotionPutWord(native+10,y);
 for(unsigned n=0;n<2;n++){
  reference[72]=native[72]=chunks[n];
  if(!willowScroll3LoadChunk(reference,rb,rw,write)||!willowScroll3LoadChunkCopy(native,nb,nw,write,copy)||memcmp(reference,native,128)||!same())return 1;
 }
 }
 willowMotionPutLong(raw+0x390c,willowMotionLong(rom+0x8404));
 if(!willowScroll3MapSet(cells,0x390c/4,WILLOW_SCROLL3_BACKDROP_PAIR)||!same()||transfers!=6)return 1;
 printf("Native backdrop/two-pan chunks: %u exact 64-pair chunk transfers, original literal pair and %u packed-ring comparisons match ROM; controller and untouched cells retained\n",transfers,comparisons);
 return 0;
}
