#include "../src/willow_native_map14.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t cells[WILLOW_MAP14_WORDS],reference[WILLOW_MAP14_CELLS];
static void check(void){
 for(unsigned n=0;n<WILLOW_MAP14_CELLS;n++)assert(willowMap14Read(cells,n)==reference[n]);
}
int main(void){
 unsigned checks=0;uint32_t random=0x57494c4c;
 /* Visit every cell and every 14-bit token, including final-cell boundaries. */
 for(unsigned value=0;value<0x4000;value++){
  unsigned cell=(value*73)&4095;
  assert(willowMap14Write(cells,cell,value));reference[cell]=value;
  if(!(value&127))check();checks++;
 }
 check();
 /* Independent token array models interleaved contiguous and strided copies. */
 for(unsigned pass=0;pass<4096;pass++){
  random=random*1664525u+1013904223u;unsigned start=(random>>16)&4095;
  unsigned stride=(pass&1)?16:1,count=1+(pass&31);
  for(unsigned n=0;n<count&&start+n*stride<4096;n++){
   random=random*1664525u+1013904223u;uint16_t value=(random>>16)&0x3fff;
   assert(willowMap14Write(cells,start+n*stride,value));reference[start+n*stride]=value;checks++;
  }
 check();
 }
 /* Optimized immutable copies must preserve every surrounding cell. */
 uint16_t source[4096];
 for(unsigned n=0;n<4096;n++)source[n]=(n*197+0x321)&0x3fff;
 for(unsigned start=0;start<4096;start++){
  unsigned count=1+(start%39);if(count>4096-start)count=4096-start;
  willowMap14Copy(cells,start,source,count);
  for(unsigned n=0;n<count;n++)reference[start+n]=source[n];
  check();
  count=1+(start%31);if(count>1+(4095-start)/16)count=1+(4095-start)/16;
  willowMap14CopyStride16(cells,start,source,count);
  for(unsigned n=0;n<count;n++)reference[start+n*16]=source[n*16];
  check();
 }
 /* Current full dictionary plus a distinct mutable slot for every map cell. */
 for(unsigned id=0;id<9069;id++){
  uint16_t token;assert(willowMap14Encode(9069,4096,id,&token));
  assert(token==id&&willowMap14Decode(9069,token)==id);checks++;
 }
 for(unsigned slot=0;slot<4096;slot++){
  uint16_t token,id=0x8000|slot;assert(willowMap14Encode(9069,4096,id,&token));
  assert(token==9069+slot&&willowMap14Decode(9069,token)==id);checks++;
 }
 uint16_t token=0xbeef;assert(!willowMap14Encode(9069,7316,0,&token));assert(token==0xbeef);
 assert(!willowMap14Encode(9069,4096,9069,&token));
 assert(!willowMap14Encode(9069,4096,0x9000,&token));
 uint16_t before[WILLOW_MAP14_WORDS];memcpy(before,cells,sizeof cells);
 assert(!willowMap14Write(cells,4096,0));assert(!willowMap14Write(cells,0,0x4000));
 assert(!memcmp(before,cells,sizeof cells));
 for(unsigned value=0;value<0x4000;value++){
  assert(willowMap14Fill(cells,value));
  for(unsigned cell=0;cell<4096;cell++)assert(willowMap14Read(cells,cell)==value);
 }
 assert(!willowMap14Fill(cells,0x4000));
 for(unsigned cell=0;cell<4096;cell++)assert(willowMap14Read(cells,cell)==0x3fff);
 printf("Map14: %u writes/ID round trips; full-map checks, boundary preservation and rejected overflow pass; 7168-byte exact store\n",checks);
 return 0;
}
