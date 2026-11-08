#include "../src/willow_arcade_scroll2_start.h"
#include "../src/willow_arcade_map_store.h"
#include "../native/intro_probe/src/camera_data.h"
#include "../native/intro_probe/src/scroll2_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t rom[0x100000],raw[16384];
static uint16_t cells[4096];
static WillowMapStore map={cells,nativeScroll2Pairs,WILLOW_SCROLL2_PAIR_COUNT,0,0,0};
static uint16_t rw(uint32_t at){return willowMotionWord(rom+at);}
static uint8_t rb(uint32_t at){return rom[at];}
static uint16_t nw(uint32_t at){uint16_t v;if(!nativeCameraWordRead(at,&v)||v!=rw(at)){fprintf(stderr,"Missing native attract word %x\n",at);exit(1);}return v;}
static uint8_t nb(uint32_t at){uint8_t v;if(!nativeCameraByteRead(at,&v)||v!=rb(at)){fprintf(stderr,"Missing native attract byte %x\n",at);exit(1);}return v;}
static void write(uint16_t at,uint16_t v){willowMotionPutWord(raw+at,v);}
static unsigned transfers;
static uint8_t copy(uint16_t at,uint32_t source,uint16_t count){
 if(count!=256)return 0;
 transfers++;return willowMapStoreCopyRange(&map,nativeScroll2SourceIds,at,source,count);
}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 for(unsigned n=0;n<4096;n++){cells[n]=n%WILLOW_SCROLL2_PAIR_COUNT;willowMotionPutLong(raw+n*4,nativeScroll2Pairs[cells[n]]);}
 uint8_t reference[128]={0},native[128]={0};
 willowMotionPutLong(reference+66,((uint32_t)rw(0x2dc14)<<16)|rw(0x2dc16));
 willowMotionPutLong(native+66,((uint32_t)nw(0x2dc14)<<16)|nw(0x2dc16));
 willowMotionPutWord(reference+6,0x400);willowMotionPutWord(native+6,0x400);
 const unsigned chunks[]={5,9};
 for(unsigned setup=0;setup<3;setup++){
  unsigned x=setup==0?0x400:setup==1?0:0x100,y=setup?0x500:0;
  willowMotionPutWord(reference+6,x);willowMotionPutWord(native+6,x);
  willowMotionPutWord(reference+10,y);willowMotionPutWord(native+10,y);
 for(unsigned n=0;n<2;n++){
  reference[72]=native[72]=chunks[n];
  if(!willowScroll2LoadChunk(reference,rb,rw,write)||!willowScroll2LoadChunkCopy(native,nb,nw,write,copy)||memcmp(reference,native,128))return 1;
  for(unsigned c=0;c<4096;c++)if(willowMapStorePair(&map,c)!=willowMotionLong(raw+c*4)){fprintf(stderr,"Attract map mismatch %u\n",c);return 1;}
 }
 }
 if(transfers!=6)return 1;
 printf("Native attract spawn/two-pan chunks: %u exact 256-pair transfers, full controller records and 24576 ring-cell comparisons match original ROM reads; untouched cells retained\n",transfers);
 return 0;
}
