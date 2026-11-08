#include "../src/willow_arcade_scroll3_map_store.h"
#include "../native/intro_probe/src/scroll3_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t memory[WILLOW_SCROLL3_MAP_WORDS+2];
int main(void)
{
 uint16_t*cells=memory+1;memory[0]=0x1234;memory[WILLOW_SCROLL3_MAP_WORDS+1]=0xabcd;
 unsigned checks=0;
 for(unsigned cell=0;cell<4096;cell++)for(unsigned id=0;id<2048;id++){
  uint16_t before=cell?willowScroll3MapId(cells,cell-1):0;
  uint16_t after=cell<4095?willowScroll3MapId(cells,cell+1):0;
  if(!willowScroll3MapSet(cells,cell,id) || willowScroll3MapId(cells,cell)!=id ||
   (cell && willowScroll3MapId(cells,cell-1)!=before) ||
   (cell<4095 && willowScroll3MapId(cells,cell+1)!=after))return 1;
  checks++;
 }
 if(memory[0]!=0x1234 || memory[WILLOW_SCROLL3_MAP_WORDS+1]!=0xabcd)return 1;
 uint8_t program[0x100000];FILE*f=fopen("build/reference_maincpu.bin","rb");
 if(!f || fread(program,1,sizeof program,f)!=sizeof program)return 2;fclose(f);
 for(unsigned n=0;n<16384;n++){
  uint16_t id=nativeScroll3SourceIds[n];uint32_t at=0xc0000+n*4;
  uint32_t pair=((uint32_t)(uint16_t)(willowMotionWord(program+at)+0xa00)<<16)|willowMotionWord(program+at+2);
  if(id>=WILLOW_SCROLL3_PAIR_COUNT || nativeScroll3Pairs[id]!=pair)return 1;
 }
 for(unsigned n=0;n<16384;n++){
  unsigned group=n/16,within=n%16;
  if(willowScroll3MapId(nativeScroll3PackedSourceIds+group*11,within)!=nativeScroll3SourceIds[n])return 1;
  if(willowScroll3PackedSourceId(nativeScroll3PackedSourceIds,n)!=nativeScroll3SourceIds[n])return 1;
 }
 /* Compare complete destination buffers over every source cell. Exercise
  * unaligned ID phases, zero-length copies and both range ends. */
 uint16_t plain[WILLOW_SCROLL3_MAP_WORDS],fromPacked[WILLOW_SCROLL3_MAP_WORDS];
 unsigned packedScalarCopies=0;const unsigned counts[]={0,1,2,7,8,15,16,31,64,4096,4097,65535};
 for(unsigned source=0;source<16384;source++)for(unsigned kind=0;kind<sizeof counts/sizeof counts[0];kind++){
  unsigned destination=(source*71%4096)*4,count=counts[kind];
  memcpy(plain,cells,sizeof plain);memcpy(fromPacked,cells,sizeof fromPacked);
  unsigned a=willowScroll3MapCopy(plain,nativeScroll3SourceIds,destination,0xc0000+source*4,count);
  unsigned b=willowScroll3MapCopyPacked(fromPacked,nativeScroll3PackedSourceIds,destination,0xc0000+source*4,count);
  if(a!=b||memcmp(plain,fromPacked,sizeof plain))return 1;
  packedScalarCopies++;
 }
 const uint32_t badSource[]={0,0xbfffc,0xc0001,0xc0002,0xcfffe,0xd0000,0xffffffff};
 for(unsigned n=0;n<sizeof badSource/sizeof badSource[0];n++)for(unsigned at=0;at<16388;at++){
  memcpy(plain,cells,sizeof plain);memcpy(fromPacked,cells,sizeof fromPacked);
  unsigned a=willowScroll3MapCopy(plain,nativeScroll3SourceIds,at,badSource[n],1);
  unsigned b=willowScroll3MapCopyPacked(fromPacked,nativeScroll3PackedSourceIds,at,badSource[n],1);
  if(a!=b||memcmp(plain,fromPacked,sizeof plain))return 1;
  packedScalarCopies++;
 }
 printf("Packed Scroll3 source: all 16384 IDs and %u full-buffer copy/rejection comparisons match plain storage\n",packedScalarCopies);
 for(unsigned block=0;block<256;block++){
  unsigned destination=(block*71%4033)*4;
  if(!willowScroll3MapCopy(cells,nativeScroll3SourceIds,destination,0xc0000+block*256,64))return 1;
  for(unsigned n=0;n<64;n++)if(willowScroll3MapId(cells,destination/4+n)!=nativeScroll3SourceIds[block*64+n])return 1;
 }
 /* Native packed memcpy operates on big-endian bytes, including odd-byte
  * boundaries. Compare complete buffers against independent per-ID stores. */
 uint8_t packed[WILLOW_SCROLL3_MAP_WORDS*2],sourcePacked[16384*11/8];
 uint16_t expected[WILLOW_SCROLL3_MAP_WORDS];unsigned packedCopies=0;
 for(unsigned n=0;n<sizeof sourcePacked/2;n++){sourcePacked[n*2]=nativeScroll3PackedSourceIds[n]>>8;sourcePacked[n*2+1]=nativeScroll3PackedSourceIds[n];}
 const unsigned lengths[]={8,16,24,64};
 for(unsigned destination=0;destination<4096;destination+=8)for(unsigned phase=0;phase<2;phase++)for(unsigned kind=0;kind<4;kind++){
  unsigned count=lengths[kind],source=((destination*17)%16000)&~15u;source+=phase*8;
  if(destination+count>4096)continue;
  memcpy(expected,cells,sizeof expected);
  for(unsigned n=0;n<count;n++)if(!willowScroll3MapSet(expected,destination+n,nativeScroll3SourceIds[source+n]))return 1;
  for(unsigned n=0;n<WILLOW_SCROLL3_MAP_WORDS;n++){packed[n*2]=cells[n]>>8;packed[n*2+1]=cells[n];}
  memcpy(packed+(destination/8)*11,sourcePacked+(source/8)*11,(count/8)*11);
  for(unsigned n=0;n<WILLOW_SCROLL3_MAP_WORDS;n++)if(((packed[n*2]<<8)|packed[n*2+1])!=expected[n])return 1;
  packedCopies++;
 }
 printf("Packed Scroll3 byte copies: %u complete-buffer comparisons for both byte alignments and 8/16/24/64-ID groups\n",packedCopies);
 uint16_t saved[WILLOW_SCROLL3_MAP_WORDS];memcpy(saved,cells,sizeof saved);
 if(willowScroll3MapSet(cells,4096,0) || willowScroll3MapSet(cells,0,2048) ||
  willowScroll3MapCopy(cells,nativeScroll3SourceIds,16384,0xc0000,1) ||
  willowScroll3MapCopy(cells,nativeScroll3SourceIds,16380,0xc0000,2) ||
  willowScroll3MapCopy(cells,nativeScroll3SourceIds,0,0xcfffc,2) ||
  willowScroll3MapCopy(cells,nativeScroll3SourceIds,1,0xc0000,1) ||
  willowScroll3MapCopy(cells,nativeScroll3SourceIds,0,0xc0002,1) ||
  willowScroll3MapCopy(cells,nativeScroll3SourceIds,0,0xbfffc,1) ||
  willowScroll3MapCopy(cells,nativeScroll3SourceIds,0,0xd0000,1) ||
  memcmp(saved,cells,sizeof saved))return 1;
 printf("Packed Scroll3: %u ID/alignment/neighbor checks; all 16384 source pairs exact; 256 chunk copies and rejected bounds preserve memory; 5632 RAM bytes\n",checks);
 return 0;
}
