#include "../src/willow_arcade_map_store.h"
#include "../src/willow_arcade_scroll2_start.h"
#include "../native/intro_probe/src/scroll2_data.h"
#include "../native/intro_probe/src/camera_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t program[0x100000],dense[16384];
#ifdef WILLOW_TEST_MAP14
static uint16_t cells[WILLOW_MAP14_WORDS];
#else
static uint16_t cells[4096];
#endif
static WillowMapOverride overrides[4096];
static WillowMapStore map={cells,nativeScroll2Pairs,WILLOW_SCROLL2_PAIR_COUNT,overrides,4096,0};
static uint16_t sourceWord(uint32_t at){if(at+1>=sizeof program)abort();return willowMotionWord(program+at);}
static uint8_t sourceByte(uint32_t at){if(at>=sizeof program)abort();return program[at];}
static void denseWrite(uint16_t at,uint16_t v){willowMotionPutWord(dense+at,v);}
static void packedWrite(uint16_t at,uint16_t v){if(!willowMapStoreWrite(&map,at,v)){fputs("Unexpected packed map exhaustion\n",stderr);exit(1);}}
static uint8_t packedCopy(uint16_t at,uint32_t source,uint16_t count){return willowMapStoreCopyRange(&map,nativeScroll2SourceIds,at,source,count);}
static void verify(void){
 unsigned references[4096]={0};
 for(unsigned n=0;n<4096;n++){
  if(willowMapStorePair(&map,n)!=willowMotionLong(dense+n*4)){fprintf(stderr,"Map pair mismatch cell %u\n",n);exit(1);}
  uint16_t id=willowMapStoreCell(&map,n);
  if(id&0x8000)references[id&0x7fff]++;
 }
 for(unsigned n=0;n<map.overrideCount;n++)if(references[n]!=overrides[n].references){fprintf(stderr,"Override count mismatch %u\n",n);exit(1);}
}
int main(void){
#ifdef WILLOW_TEST_MAP14
 if(!willowMapStoreInitPacked14(&map,WILLOW_SCROLL2_ZERO_PAIR))return 1;
#endif
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(program,1,sizeof program,f)!=sizeof program)return 2;fclose(f);
 unsigned cameraBytes=0;
 for(unsigned at=0;at<sizeof program;at++){
  uint8_t first=0,second=0;uint16_t value=0;
  unsigned present=nativeCameraByteRead(at,&first);
  if(present){if(first!=program[at])return 1;cameraBytes++;}
  unsigned next=nativeCameraByteRead(at+1,&second);
  unsigned readable=nativeCameraWordRead(at,&value);
  if(readable!=(!(at&1) && present && next))return 1;
  if(readable && value!=willowMotionWord(program+at))return 1;
 }
 if(cameraBytes!=WILLOW_CAMERA_DATA_BYTES)return 1;
 printf("Native camera tables: all %u packed bytes and word alignment/range boundaries match source\n",cameraBytes);
 for(unsigned n=0;n<65536;n++)if(nativeScroll2Pairs[nativeScroll2SourceIds[n]]!=willowMotionLong(program+0x80000+n*4)){fprintf(stderr,"Map storage assertion failed at line %d\n",__LINE__);return 1;}
 willowMapStoreInit(&map,WILLOW_SCROLL2_ZERO_PAIR);memset(dense,0,sizeof dense);
 uint32_t random=0x57494c4c;unsigned mutations=0;
 for(unsigned n=0;n<32768;n++){
  random=random*1664525u+1013904223u;unsigned cell=(random>>16)&4095;
  if(!(n&3)){
   unsigned index=(random>>8)&65535,id=nativeScroll2SourceIds[index];
   if(!willowMapStoreSource(&map,cell,id)){fprintf(stderr,"Map storage assertion failed at line %d\n",__LINE__);return 1;}
   willowMotionPutLong(dense+cell*4,nativeScroll2Pairs[id]);
  }else{
   uint16_t at=cell*4+((n&1)*2),value=(n&8)?random>>16:0xabcd;
   if(!willowMapStoreWrite(&map,at,value)){fprintf(stderr,"Map storage assertion failed at line %d\n",__LINE__);return 1;}
   denseWrite(at,value);mutations++;
  }
  if(!(n&255))verify();
 }
 verify();
 /* Streaming uses a deliberately small override budget; intermediate
  * half-written pairs must be reclaimed as ROM pairs become complete. */
 map.overrideCount=64;willowMapStoreInit(&map,WILLOW_SCROLL2_ZERO_PAIR);memset(dense,0,sizeof dense);
 unsigned transfers=0;
 for(unsigned fast=0;fast<2;fast++){
 map.overrideCount=fast?0:64;willowMapStoreInit(&map,WILLOW_SCROLL2_ZERO_PAIR);memset(dense,0,sizeof dense);
 for(unsigned axis=0;axis<2;axis++)for(unsigned mode=0;mode<4;mode++)for(unsigned sample=0;sample<512;sample++){
  uint8_t actor[128]={0};actor[33]=mode;willowMotionPutLong(actor+66,0x2dc30);
  uint16_t x=sample*113,y=sample*7919;uint32_t source;
  if(!willowScroll2Record(actor,x,y,sourceByte,&source)){fprintf(stderr,"Map storage assertion failed at line %d\n",__LINE__);return 1;}
  uint16_t destination=((x&0x3f0)<<2)+((y&0x300)<<4)+((y&0xf0)>>2);
  uint8_t copy[128];memcpy(copy,actor,sizeof copy);
  if(!willowScroll2Transfer(actor,axis?x:y,destination,source,axis,sourceByte,sourceWord,denseWrite)){fprintf(stderr,"Map storage assertion failed at line %d\n",__LINE__);return 1;}
  if(!willowScroll2TransferCopy(copy,axis?x:y,destination,source,axis,sourceByte,sourceWord,packedWrite,fast?packedCopy:0)){fprintf(stderr,"Map storage assertion failed at line %d\n",__LINE__);return 1;}
  if(memcmp(actor,copy,sizeof actor))return 1;
  verify();transfers++;
 }
 }
 /* Cold loads use the same exact ROM pair sink with no override capacity. */
 map.overrideCount=0;willowMapStoreInit(&map,WILLOW_SCROLL2_ZERO_PAIR);memset(dense,0,sizeof dense);
 unsigned chunks=0;
 for(unsigned stage=0;stage<7;stage++)for(unsigned sample=0;sample<64;sample++)for(unsigned chunk=0;chunk<16;chunk++){
  uint8_t actor[128]={0},copy[128];actor[33]=sourceByte(0x2dc1c+stage);actor[72]=chunk;
  willowMotionPutLong(actor+66,willowMotionLong(program+0x2dbf8+stage*4));
  willowMotionPutWord(actor+6,sample*113);willowMotionPutWord(actor+10,sample*7919);
  memcpy(copy,actor,sizeof copy);
  if(!willowScroll2LoadChunk(actor,sourceByte,sourceWord,denseWrite) ||
     !willowScroll2LoadChunkCopy(copy,sourceByte,sourceWord,packedWrite,packedCopy) ||
     memcmp(actor,copy,sizeof actor))return 1;
  verify();chunks++;
 }
 /* Invalid ROM transfers must not mutate map cells or override counts. */
 if(packedCopy(1,0x80000,1) || packedCopy(16384,0x80000,1) || packedCopy(0,0x7fffc,1) ||
    packedCopy(0,0xc0000,1) || packedCopy(0,0x80002,1) ||
    packedCopy(16380,0x80000,2) || packedCopy(0,0xbfffc,2))return 1;
 verify();
 /* Explicit exhaustion is atomic, and a uniquely referenced override can
  * be updated even when every override slot is occupied. */
 map.overrideCount=1;willowMapStoreInit(&map,WILLOW_SCROLL2_ZERO_PAIR);memset(dense,0,sizeof dense);
 if(!willowMapStoreWrite(&map,0,0xf123) || willowMapStoreWrite(&map,4,0xf124)){fprintf(stderr,"Map storage assertion failed at line %d\n",__LINE__);return 1;}
 if(willowMapStorePair(&map,1)!=0 || !willowMapStoreWrite(&map,0,0xf125)){fprintf(stderr,"Map storage assertion failed at line %d\n",__LINE__);return 1;}
 if(willowMapStorePair(&map,0)!=0xf1250000UL){fprintf(stderr,"Map storage assertion failed at line %d\n",__LINE__);return 1;}
 if(!willowMapStoreSource(&map,0,WILLOW_SCROLL2_ZERO_PAIR) || overrides[0].references){fprintf(stderr,"Map storage assertion failed at line %d\n",__LINE__);return 1;}
 printf("Map storage: all 65536 ROM pairs, %u arbitrary word mutations and %u streaming transfers and %u cold chunks preserve exact values/reference counts; exhaustion leaves old cells intact\n",mutations,transfers,chunks);
 return 0;
}
