#include "../src/willow_arcade_actor_pool.h"
#include "../src/willow_arcade_stage_spawns.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char program[0x100000],before[65536],after[65536];
static unsigned char records4[50*128],records6[48*64],records12[60*64],records16[20*64];
static WillowActorPoolIndex indices4[50],indices6[48],indices12[60],indices16[20];
static WillowActorPool pools[4];
static const unsigned short classes[4]={4,6,12,16},bases[4]={0x9dc6,0xb6c6,0xccc6,0xeac6};
static const unsigned short ends[4]={0x918e,0x91ee,0x92b6,0x9356};
static const unsigned short countOffsets[4]={0x9801,0x9804,0x980d,0x9813},cursorOffsets[4]={0x981c,0x9822,0x9834,0x9840};
static unsigned short sourceAddresses[57],nativeIndices[57],allocatedPools[57],used;
static unsigned short word(unsigned long a){if(a+1>=sizeof(program))abort();return program[a]*256+program[a+1];}
static unsigned short rw(const unsigned char*p,unsigned a){return p[a]*256+p[a+1];}
static void readFile(const char*path,void*out,size_t size){FILE*f=fopen(path,"rb");if(!f || fread(out,1,size,f)!=size)exit(2);fclose(f);}
static WillowStageObject *allocate(unsigned short classId){
 for(unsigned i=0;i<4;i++)if(classes[i]==classId){
  unsigned char *r=willowActorPoolAllocate(pools+i);if(!r)return NULL;
  unsigned index=(r-pools[i].records)/pools[i].stride;
  if(used>=57)abort();sourceAddresses[used]=bases[i]+index*pools[i].stride;nativeIndices[used]=index;allocatedPools[used++]=i;
  return (WillowStageObject*)r;
 }
 return NULL;
}
int main(void){
 readFile("build/reference_maincpu.bin",program,sizeof(program));
 readFile("build/arcade-trace/actor-native/handoff-ram-1587.bin",before,sizeof(before));
 readFile("build/arcade-trace/actor-native/handoff-ram-1588.bin",after,sizeof(after));
 willowActorPoolInit(pools,records4,indices4,50,128,4);
 willowActorPoolInit(pools+1,records6,indices6,48,64,6);
 willowActorPoolInit(pools+2,records12,indices12,60,64,12);
 willowActorPoolInit(pools+3,records16,indices16,20,64,16);
 /* A28A and A2A6 consume two class-12 slots before the stage-entry table.
  * Their state machines are separate; reserve those slots through the same
  * native allocator, without copying either captured actor record. */
 if(willowActorPoolAllocate(pools+2)!=records12+59*64 || willowActorPoolAllocate(pools+2)!=records12+58*64)return 1;
 for(unsigned i=0;i<4;i++){
  WillowActorPool*p=pools+i;unsigned cursor=ends[i]-p->capacity*2+p->cursor*2;
  if(before[countOffsets[i]]!=p->available || rw(before,cursorOffsets[i])!=cursor)return 1;
  for(unsigned j=p->cursor;j<p->capacity;j++){
   unsigned address=bases[i]+p->indices[j]*p->stride;
   if(rw(before,ends[i]-p->capacity*2+j*2)!=address || memcmp(p->records+p->indices[j]*p->stride,before+address,p->stride))return 1;
  }
 }
 WillowStageSpawnContext c={rw(before,0x838c),word(0x3078),word(0x307a)};
 if(willowStageSpawnTable(0x40470,&c,word,allocate)!=57 || used!=57)return 1;
 unsigned bytes=0;
 for(unsigned i=0;i<used;i++){
  WillowActorPool*p=pools+allocatedPools[i];unsigned char *r=p->records+nativeIndices[i]*p->stride;
  if(memcmp(r,after+sourceAddresses[i],p->stride)){fprintf(stderr,"Code-initialized pool record %u differs\n",i);return 1;}
  bytes+=p->stride;
 }
 for(unsigned i=0;i<4;i++)if(after[countOffsets[i]]!=pools[i].available || rw(after,cursorOffsets[i])!=ends[i]-pools[i].capacity*2+pools[i].cursor*2)return 1;
 /* Release preserves unrelated bytes and reuses the just-freed slot first. */
 WillowActorPool*p=pools;unsigned index=nativeIndices[16];unsigned char saved[128];
 unsigned char *r=p->records+index*p->stride;memset(r,0xa5,128);memcpy(saved,r,128);
 willowActorPoolRelease(p,index);
 for(unsigned i=0;i<128;i++){
  int clear=i<6 || (i>=14 && i<20) || i==31 || i==32 || i==33 || i==36 || i==46 || i==47;
  if(r[i]!=(clear?0:saved[i]))return 1;
 }
 if(willowActorPoolAllocate(p)!=r)return 1;
 while(willowActorPoolAllocate(p)){}
 unsigned cursor=p->cursor;if(willowActorPoolAllocate(p) || p->cursor!=cursor || p->available)return 1;
 printf("Code-initialized actor pools: 57 entry records (%u bytes), source free-list order/counts, release preservation and exhaustion pass\n",bytes);
 return 0;
}
