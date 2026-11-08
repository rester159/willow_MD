/* Compare the actual Request implementations across admitted ROM resources,
 * including arbitrary owner histories, aliases, masks and refusal states.
 * This is an algorithm-equivalence check, not a live timing admission. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "willow_native_far_cache.h"
#include "reference-request.h"
static uint32_t randomState=0x57a110;
static unsigned cases,planned,refused,committed;
static uint32_t nextRandom(void)
{randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
static uint16_t be16(const uint8_t*p){return p[0]*256u+p[1];}
static uint32_t be32(const uint8_t*p){return (uint32_t)be16(p)<<16|be16(p+2);}
static uint8_t*readFile(const char*name,size_t*size)
{FILE*f=fopen(name,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0);rewind(f);uint8_t*p=malloc(n);assert(p&&fread(p,1,n,f)==(size_t)n);fclose(f);*size=n;return p;}
static void compare(const WillowNativeFarCache*seed,const WillowNativeFarResources*r,uint16_t key,uint16_t mask)
{
 WillowNativeFarCache old,new;memcpy(&old,seed,sizeof old);memcpy(&new,seed,sizeof new);
 uint16_t a=referenceFarCacheRequest(&old,r,key,mask),b=willowFarCacheRequest(&new,r,key,mask);
 if(a!=b||memcmp(&old,&new,sizeof old)){
  fprintf(stderr,"Request mismatch case%u key%u mask%04x old%u new%u\n",cases,key,mask,a,b);abort();
 }
 if(!a){assert(!memcmp(&old,seed,sizeof old));refused++;}else if(a==1)committed++;else planned++;
 cases++;
}
int main(int argc,char**argv)
{
 assert(argc==3);size_t bytes,catbytes;uint8_t*b=readFile(argv[1],&bytes),*cat=readFile(argv[2],&catbytes);
 assert(be32(b)==0x57424631&&be32(b+8)==bytes&&be32(b+20)==catbytes);
 WillowNativeFarResources r={b+be32(b+40),b+be32(b+44),b+be32(b+48),b+be32(b+52),cat+be32(b+60),be16(b+32),be16(b+36),be16(b+38)};
 WillowNativeFarCache initial;memset(&initial,0,sizeof initial);assert(willowFarCacheInit(&initial,&r));
 for(uint16_t key=0;key<r.referenceCount/16;key++){
  for(unsigned scenario=0;scenario<48;scenario++){
   WillowNativeFarCache seed=initial;
   for(unsigned i=0;i<165;i++){
    if(scenario&1)seed.owner[i]=(uint16_t)(nextRandom()%r.patternCount);
    if(scenario&2)seed.owner[i]=willowFarPattern(&r,key*16+nextRandom()%16);
    if(scenario&4)seed.owner[i]=65535;
    if((scenario&8)&&(nextRandom()&1))willowFarProtect(seed.preparedUse,i);
    if((scenario&16)&&(nextRandom()&1))willowFarProtect(seed.displayedUse,i);
   }
   if(scenario&32){for(unsigned i=0;i<6;i++)seed.preparedUse[i]=seed.displayedUse[i]=UINT32_MAX;seed.preparedUse[5]=seed.displayedUse[5]=31;}
   seed.residencyEpoch=1+(nextRandom()%1000000);
   seed.preparedResidencyEpoch=nextRandom();seed.sourceEpoch=nextRandom();seed.paletteEpoch=nextRandom();seed.preparationGeneration=nextRandom();
   for(unsigned i=0;i<4;i++)seed.view[i]=(uint16_t)nextRandom();
   /* Preserve old hint priority even if a canonical owner also occurs at a
    * lower pool slot. Duplicate histories deliberately stress that contract. */
   if(scenario&2)for(unsigned n=0;n<16;n++){
    uint16_t slot=willowFarSlot(willowFarWord(r.refs+(uint32_t)(key*16+n)*4+2)&2047);
    if(slot<165)seed.owner[slot]=willowFarPattern(&r,key*16+n);
   }
   compare(&seed,&r,key,65535);compare(&seed,&r,key,0);
   for(unsigned n=0;n<16;n++)compare(&seed,&r,key,(uint16_t)(1u<<n));
   for(unsigned n=0;n<6;n++)compare(&seed,&r,key,(uint16_t)nextRandom());
   seed.residencyEpoch=UINT32_MAX-1;compare(&seed,&r,key,65535);
   seed.residencyEpoch=UINT32_MAX-2;compare(&seed,&r,key,65535);
   seed.phase=WILLOW_FAR_CACHE_QUEUED;compare(&seed,&r,key,65535);
   seed.phase=WILLOW_FAR_CACHE_COPYING;compare(&seed,&r,key,65535);
   seed.phase=WILLOW_FAR_CACHE_IDLE;seed.reserved=1;compare(&seed,&r,key,65535);
   seed.reserved=0;seed.preparedUse[5]|=32;compare(&seed,&r,key,65535);
  }
 }
 compare(&initial,&r,r.referenceCount/16,65535);compare(&initial,0,0,65535);
 assert(!referenceFarCacheRequest(0,&r,0,65535)&&!willowFarCacheRequest(0,&r,0,65535));
 printf("{\"cases\":%u,\"planned\":%u,\"committed\":%u,\"refused\":%u,\"entire_state_equivalence\":true,\"native_or_live_timing_claim\":false}\n",cases,planned,committed,refused);
 free(cat);free(b);return 0;
}
