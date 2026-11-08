#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/willow_native_intro_attribute_mirror.h"
static struct {uint8_t before[16];WillowIntroAttributeMirror mirror;uint8_t after[16];} arena;
static uint16_t expected[WILLOW_TILE_MIRROR_CELLS];
static uint32_t seed=0x427876ac;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static const unsigned upper[5]={0,4,6,8,9};
static unsigned supported(unsigned attr){unsigned hi=attr>>12;return hi==0||hi==4||hi==6||hi==8||hi==9;}
int main(void){
 assert(sizeof(WillowIntroAttributeMirror)==5160);
 memset(&arena,0xa5,sizeof arena);memset(&arena.mirror,0,sizeof arena.mirror);
 unsigned accepted=0,rejected=0;
 for(unsigned offset=0;offset<8;offset++)for(unsigned attribute=0;attribute<65536;attribute++){
  unsigned cell=offset+1376;__typeof__(arena) saved=arena;
  uint16_t out=0xbeef;
  if(supported(attribute)){
   assert(willowIntroAttributeWrite(&arena.mirror,cell,attribute));
   assert(willowIntroAttributeRead(&arena.mirror,cell,&out)&&out==attribute);
   for(unsigned neighbor=0;neighbor<8;neighbor++)if(neighbor!=offset){uint16_t a,b;assert(willowIntroAttributeRead(&arena.mirror,1376+neighbor,&a));assert(willowIntroAttributeRead(&saved.mirror,1376+neighbor,&b));assert(a==b);}
   accepted++;
  }else{assert(!willowIntroAttributeWrite(&arena.mirror,cell,attribute));assert(!memcmp(&arena,&saved,sizeof arena));rejected++;}
 }
 for(unsigned i=0;i<2752;i++){expected[i]=(upper[rnd()%5]<<12)|(rnd()&4095);assert(willowIntroAttributeWrite(&arena.mirror,i,expected[i]));}
 for(unsigned n=0;n<131072;n++){
  unsigned cell=rnd()%2752;expected[cell]=(upper[rnd()%5]<<12)|(rnd()&4095);
  assert(willowIntroAttributeWrite(&arena.mirror,cell,expected[cell]));
  if(!(n%256))for(unsigned i=0;i<2752;i++){uint16_t out;assert(willowIntroAttributeRead(&arena.mirror,i,&out)&&out==expected[i]);}
 }
 for(unsigned code=5;code<8;code++){
  __typeof__(arena) saved=arena;arena.mirror.styles[0]=(arena.mirror.styles[0]&248)|code;
  uint16_t out=0xbeef;assert(!willowIntroAttributeRead(&arena.mirror,0,&out)&&out==0xbeef);arena=saved;
 }
 memset(arena.mirror.styles,0x3c,sizeof arena.mirror.styles);
 for(unsigned i=0;i<2752;i++)assert(willowIntroAttributeOwner(&arena.mirror,i)==(expected[i]&2047));
 for(unsigned n=0;n<131072;n++){unsigned cell=rnd()%2752;uint16_t tile=rnd()&2047;assert(willowIntroAttributeWriteOwner(&arena.mirror,cell,tile));assert(willowIntroAttributeOwner(&arena.mirror,cell)==tile);}
 for(unsigned i=0;i<sizeof arena.mirror.styles;i++)assert(arena.mirror.styles[i]==0x3c);
 for(unsigned i=0;i<16;i++)assert(arena.before[i]==0xa5&&arena.after[i]==0xa5);
 __typeof__(arena) saved=arena;uint16_t out=0xbeef;
 assert(!willowIntroAttributeWrite(&arena.mirror,2752,0));assert(!willowIntroAttributeRead(&arena.mirror,2752,&out)&&out==0xbeef);
 assert(!willowIntroAttributeWriteOwner(&arena.mirror,0,2048));assert(!memcmp(&arena,&saved,sizeof arena));
 printf("Intro attribute codec: %u supported attributes exact, %u unsupported writes reject atomically; 262144 full/ownership churn writes; 5160 bytes, style and tail guards passed\n",accepted,rejected);
}
