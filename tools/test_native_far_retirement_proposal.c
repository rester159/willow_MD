/* Adversarial ownership MODEL, not a target implementation or timing proof.
 * Uses the real far-cache allocator and immutable WBF1/WBC1 resources. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "willow_native_far_cache.h"
static unsigned long checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
static uint16_t be16(const uint8_t*p){return p[0]*256u+p[1];}
static uint32_t be32(const uint8_t*p){return (uint32_t)be16(p)<<16|be16(p+2);}
static uint8_t*file(const char*n,size_t*s){FILE*f=fopen(n,"rb");assert(f);fseek(f,0,SEEK_END);long z=ftell(f);rewind(f);uint8_t*p=malloc(z);assert(p&&fread(p,1,z,f)==(size_t)z);fclose(f);*s=z;return p;}
/* 13 bits fit existing mode4-only bg.cursor. Bits13..15 must remain zero. */
static uint16_t certificate(unsigned x,unsigned y,unsigned rows){assert(x<64&&y<32&&(rows==28||rows==29));return 0x1000|x|(y<<6)|((rows-28)<<11);}
static unsigned valid(uint16_t c){return (c&0xf000)==0x1000;}
static unsigned inside(uint16_t c,unsigned x,unsigned y){return valid(c)&&((x-(c&63))&63)<43&&((y-((c>>6)&31))&31)<28+((c>>11)&1);}
/* Test-only physical VRAM model. preparedUse is borrowed ONLY in mode5.
 * No target allocation is represented by this enclosing test structure. */
typedef struct {WillowNativeFarCache cache;uint16_t physical[2048],shadow[2048],mode,oldRect,newRect;unsigned writes,reads;} Model;
static WillowNativeFarResources resource;
static void physicalPins(const uint16_t*words,uint32_t*pins){memset(pins,0,24);for(unsigned i=0;i<2048;i++){uint16_t s=willowFarSlot(words[i]&2047);if(s!=65535)willowFarProtect(pins,s);}}
static void init(Model*m){memset(m,0,sizeof(*m));CHECK(willowFarCacheInit(&m->cache,&resource));m->mode=4;m->oldRect=certificate(60,29,29);m->newRect=certificate(61,30,28);}
static unsigned begin(Model*m){
 if(m->mode!=4||!valid(m->oldRect)||!valid(m->newRect)||m->cache.phase!=WILLOW_FAR_CACHE_IDLE)return 0;
 m->mode=5;m->cache.cursor=0;m->cache.preparedResidencyEpoch=0;m->cache.missing=1;
 memset(m->cache.preparedUse,0,24);return 1;
}
static unsigned step(Model*m,unsigned budget,int failAt){
 if(m->mode!=5||!valid(m->oldRect)||!valid(m->newRect)||!budget||m->cache.phase!=WILLOW_FAR_CACHE_IDLE)return 0;
 while(budget--&&m->cache.cursor<2048){
  unsigned i=m->cache.cursor,x=i&63,y=i>>6;
  if((int)i==failAt)return 0; /* No completion certificate after failure. */
  if(!inside(m->oldRect,x,y)&&!inside(m->newRect,x,y)){
   CHECK(m->shadow[i]==0);m->physical[i]=0;m->writes++;
  }else{
   uint16_t s=willowFarSlot(m->physical[i]&2047);m->reads++;
   if(s!=65535)willowFarProtect(m->cache.preparedUse,s);
  }
  m->cache.cursor++;
 }
 if(m->cache.cursor==2048){
  memcpy(m->cache.displayedUse,m->cache.preparedUse,24);
  m->cache.cursor=0;m->cache.missing=1;m->cache.preparedResidencyEpoch=0;m->mode=4;
 }
 return 1;
}
static unsigned request(Model*m,unsigned key){
 /* Required caller certificate: the reusable cache core cannot infer mode5. */
 if(m->mode!=4||m->cache.missing)return 0;
 return willowFarCacheRequest(&m->cache,&resource,key,65535);
}
static void exhaustRectangles(void){
 unsigned char expected[2048];
 for(unsigned rows=28;rows<=29;rows++)for(unsigned y=0;y<32;y++)for(unsigned x=0;x<64;x++){
  uint16_t c=certificate(x,y,rows);CHECK(valid(c));memset(expected,0,sizeof(expected));
  for(unsigned j=0;j<rows;j++)for(unsigned i=0;i<43;i++)expected[((y+j)%32)*64+(x+i)%64]=1;
  for(unsigned i=0;i<2048;i++)CHECK(inside(c,i&63,i>>6)==expected[i]);
  /* Actual320px viewport uses40/41 cells, with one conservative left guard. */
  for(unsigned fine=0;fine<8;fine++)for(unsigned i=0;i<40+(fine!=0);i++)CHECK(inside(c,(x+1+i)&63,y));
 }
 for(unsigned c=0;c<65536;c++)CHECK(valid(c)==(c>=0x1000&&c<0x2000));
}
int main(int argc,char**argv){
 assert(argc==3);size_t size,catSize;uint8_t*b=file(argv[1],&size),*cat=file(argv[2],&catSize);
 CHECK(be32(b)==0x57424631&&be32(b+8)==size&&be32(b+20)==catSize);
 resource=(WillowNativeFarResources){b+be32(b+40),b+be32(b+44),b+be32(b+48),b+be32(b+52),cat+be32(b+60),be16(b+32),be16(b+36),be16(b+38)};
 CHECK(willowFarResourcesValid(&resource));CHECK(willowFarPattern(&resource,212*16)==3201);
 exhaustRectangles();Model m;init(&m);unsigned outside[2048],n=0;
 for(unsigned i=0;i<2048;i++)if(!inside(m.oldRect,i&63,i>>6)&&!inside(m.newRect,i&63,i>>6))outside[n++]=i;
 CHECK(n>=165);for(unsigned s=0;s<165;s++)m.physical[outside[s]]=willowFarTile(s);
 /* Same owner has both a stale offscreen alias and an old-view live alias. */
 unsigned oldCell=((29+2)&31)*64+((60+2)&63);m.physical[oldCell]=willowFarTile(0);
 physicalPins(m.physical,m.cache.displayedUse);for(unsigned i=0;i<165;i++)CHECK(willowFarUsed(m.cache.displayedUse,i));
 WillowNativeFarCache before=m.cache;CHECK(!willowFarCacheRequest(&m.cache,&resource,212,65535));CHECK(!memcmp(&before,&m.cache,sizeof(before)));
 CHECK(begin(&m));uint32_t held[6];memcpy(held,m.cache.displayedUse,24);
 CHECK(!request(&m,212));CHECK(!step(&m,2048,100));CHECK(!memcmp(held,m.cache.displayedUse,24));CHECK(m.mode==5&&m.cache.cursor==100);
 unsigned calls=0;while(m.mode==5){CHECK(step(&m,73,-1));calls++;if(m.mode==5)CHECK(!memcmp(held,m.cache.displayedUse,24));}
 CHECK(calls==27);CHECK(m.reads+m.writes==2048);CHECK(willowFarUsed(m.cache.displayedUse,0));
 for(unsigned i=1;i<165;i++)CHECK(!willowFarUsed(m.cache.displayedUse,i));
 uint32_t exact[6];physicalPins(m.physical,exact);CHECK(!memcmp(exact,m.cache.displayedUse,24));
 CHECK(m.physical[oldCell]==willowFarTile(0));for(unsigned i=0;i<n;i++)CHECK(m.physical[outside[i]]==0);
 CHECK(!request(&m,212)); /* Fresh canonical resolution still mandatory. */
 CHECK(willowFarCachePreparedUse(&m.cache,m.shadow,2048));CHECK(!m.cache.missing);CHECK(request(&m,212)==WILLOW_FAR_CACHE_PLANNED);
 for(unsigned i=0;i<16;i++)CHECK(m.cache.slots[i]!=0);
 /* True old-view165+new16 union cannot fit: safe retirement must not fake space. */
 init(&m);unsigned count=0;
 for(unsigned i=0;i<2048&&count<165;i++)if(inside(m.oldRect,i&63,i>>6))m.physical[i]=willowFarTile(count++);
 CHECK(count==165);physicalPins(m.physical,m.cache.displayedUse);CHECK(begin(&m));while(m.mode==5)CHECK(step(&m,127,-1));
 CHECK(willowFarCachePreparedUse(&m.cache,m.shadow,2048));before=m.cache;CHECK(!request(&m,212));CHECK(!memcmp(&before,&m.cache,sizeof(before)));
 /* No retirement from cold/reset, invalid cert or pending transfer. */
 init(&m);m.mode=3;CHECK(!begin(&m));m.mode=4;m.oldRect=0;CHECK(!begin(&m));m.oldRect=certificate(0,0,28);m.cache.phase=WILLOW_FAR_CACHE_QUEUED;CHECK(!begin(&m));
 /* Both rectangles matter. This physical word is future-visible, old-offscreen. */
 init(&m);m.oldRect=certificate(0,0,28);m.newRect=certificate(10,4,29);unsigned nextOnly=30*64+50;m.physical[nextOnly]=willowFarTile(4);
 CHECK(!inside(m.oldRect,50,30)&&inside(m.newRect,50,30));physicalPins(m.physical,m.cache.displayedUse);CHECK(begin(&m));while(m.mode==5)CHECK(step(&m,101,-1));
 CHECK(m.physical[nextOnly]==willowFarTile(4)&&willowFarUsed(m.cache.displayedUse,4));
 printf("{\"assertions\":%lu,\"rectangle_encodings\":4096,\"bounded_scan_model\":true,\"extra_persistent_bytes\":0,\"actual_allocator\":true,\"native_timing_or_vdp_claim\":false}\n",checks);
 free(cat);free(b);return 0;
}
