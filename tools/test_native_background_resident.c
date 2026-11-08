/* Resource ownership and all-before-first-write validation of the cold loader. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static uint8_t vram[65536];
static uint32_t position,controls,dataWrites,cases,refusals;
static void control(uint32_t value)
{
 assert((value&0xc000fffcUL)==0x40000000UL);
 position=((value>>16)&0x3fff)|((value&3)<<14);controls++;
}
static void writeData(uint32_t value)
{
 assert(position+4<=65536);
 for(unsigned n=0;n<4;n++)vram[position+n]=(uint8_t)(value>>(24-8*n));
 position+=4;dataWrites++;
}
#define WILLOW_BACKGROUND_RESIDENT_CONTROL(value) control(value)
#define WILLOW_BACKGROUND_RESIDENT_DATA(value) writeData(value)
#include "willow_native_background_resident.h"
static void word(uint8_t*p,uint16_t value){p[0]=value>>8;p[1]=value;}
static void longword(uint8_t*p,uint32_t value){word(p,value>>16);word(p+2,value);}
static void reset(void){memset(vram,0xa5,sizeof vram);controls=dataWrites=0;}
static void reject(const void*p,uint32_t size)
{
 reset();assert(!willowBackgroundResidentLoad(p,size));assert(!controls&&!dataWrites);
 for(unsigned n=0;n<sizeof vram;n++)assert(vram[n]==0xa5);
 refusals++;
}
int main(int argc,char**argv)
{
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);
 assert(!fseek(f,0,SEEK_END));long length=ftell(f);assert(length>=32&&length<1048576);
 rewind(f);uint32_t bytes=(uint32_t)length;
 uint8_t*blob=malloc(bytes),*mutant=malloc(bytes+2);assert(blob&&mutant);
 assert(fread(blob,1,bytes,f)==bytes);fclose(f);
 assert(willowBackgroundResidentValid(blob,bytes));
 reset();assert(willowBackgroundResidentLoad(blob,bytes));
 uint8_t expected[65536];memset(expected,0xa5,sizeof expected);
 unsigned count=willowBackgroundResidentWord(blob+6),written=0;
 for(unsigned n=0;n<count;n++){
  const uint8_t*r=blob+16+n*8;unsigned at=willowBackgroundResidentWord(r),size=2*willowBackgroundResidentWord(r+2);
  memcpy(expected+at,blob+willowBackgroundResidentLong(r+4),size);written+=size;
 }
 assert(!memcmp(expected,vram,sizeof vram));assert(controls==count&&dataWrites*4==written);cases++;
 /* Every native tile position: precisely the two reserved BG ranges work. */
 memcpy(mutant,blob,bytes);word(mutant+6,1);word(mutant+18,16);longword(mutant+20,32);
 for(unsigned tile=0;tile<2048;tile++){
  word(mutant+16,tile*32);int allowed=(tile>=16&&tile<876)||(tile>=1481&&tile<1536);
  assert(willowBackgroundResidentValid(mutant,bytes)==allowed);
  if(!allowed){reject(mutant,bytes);continue;}
  reset();assert(willowBackgroundResidentLoad(mutant,bytes));
  memset(expected,0xa5,sizeof expected);memcpy(expected+tile*32,mutant+32,32);
  assert(!memcmp(expected,vram,sizeof vram));assert(controls==1&&dataWrites==8);cases++;
 }
 reject(NULL,bytes);memcpy(mutant+1,blob,bytes);reject(mutant+1,bytes);
 for(unsigned size=0;size<32;size++)reject(blob,size);
 reject(blob,bytes-1);reject(blob,bytes+1);
 for(unsigned bit=0;bit<32;bit++){
  memcpy(mutant,blob,bytes);longword(mutant,0x57425231UL^(1UL<<bit));reject(mutant,bytes);
 }
 const uint16_t badCounts[]={0,916,65535};
 for(unsigned n=0;n<sizeof badCounts/sizeof*badCounts;n++){
  memcpy(mutant,blob,bytes);word(mutant+6,badCounts[n]);reject(mutant,bytes);
 }
 const uint32_t badOffsets[]={0,1,15,17,0xffffffffUL};
 for(unsigned n=0;n<sizeof badOffsets/sizeof*badOffsets;n++){
  memcpy(mutant,blob,bytes);longword(mutant+12,badOffsets[n]);reject(mutant,bytes);
 }
 /* All-invalid last entry: a valid first run must still make zero writes. */
 const uint16_t badWords[]={0,1,15,17,65535};
 for(unsigned run=0;run<count;run++){
  unsigned at=16+run*8;
  for(unsigned n=0;n<sizeof badWords/sizeof*badWords;n++){
   memcpy(mutant,blob,bytes);word(mutant+at+2,badWords[n]);reject(mutant,bytes);
  }
  const uint32_t badData[]={0,16,31,33,bytes,bytes+4,0xfffffffcUL};
  for(unsigned n=0;n<sizeof badData/sizeof*badData;n++){
   memcpy(mutant,blob,bytes);longword(mutant+at+4,badData[n]);reject(mutant,bytes);
  }
  memcpy(mutant,blob,bytes);word(mutant+at,willowBackgroundResidentWord(mutant+at)+2);reject(mutant,bytes);
 }
 memcpy(mutant,blob,bytes);word(mutant+4,2);reject(mutant,bytes);
 memcpy(mutant,blob,bytes);word(mutant+24,0x200);reject(mutant,bytes);
 printf("{\"accepted\":%u,\"refused\":%u,\"resident_bytes\":%u,\"guarded_bytes\":%u}\n",cases,refusals,written,65536-written);
 free(blob);free(mutant);return 0;
}
