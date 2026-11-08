#include "../src/willow_native_object_sat_fast.h"
#include "../src/willow_native_object_latch_next.h"
static uint16_t memory[8192];
#ifndef WILLOW_FAST_PARITY_NATIVE
#include <stdio.h>
#include <string.h>
#endif
#ifndef WILLOW_FAST_PARITY_CASES
#define WILLOW_FAST_PARITY_CASES 24000
#endif
static uint32_t randomState;
static uint32_t nextRandom(void)
{uint32_t x=randomState;x^=x<<13;x^=x>>17;x^=x<<5;return randomState=x;}
typedef struct {uint32_t hash,calls;uint16_t mode;} Assets;
static uint8_t readBank(void*context,uint16_t n,uint16_t*out)
{memcpy(out,(const uint16_t*)context+n*4,8);return 1;}
static uint8_t asset(void*context,uint16_t code,uint16_t source,int16_t x,int16_t y,uint16_t*out)
{
 Assets*a=context; a->calls++;
 a->hash=(a->hash*33)^code;a->hash=(a->hash*33)^source;
 a->hash=(a->hash*33)^(uint16_t)x;a->hash=(a->hash*33)^(uint16_t)y;
 if(a->mode==1)return WILLOW_OBJECT_ASSET_TRANSPARENT;
 if(a->mode==2&&(code&3))return WILLOW_OBJECT_ASSET_TRANSPARENT;
 if(a->mode==3&&code%7==0)return WILLOW_OBJECT_ASSET_MISSING;
 *out=(uint16_t)(((source&3)<<13)|((code&255)*4));
 return WILLOW_OBJECT_ASSET_READY;
}
static int fieldsEqual(const WillowObjectSatResult*a,const WillowObjectSatResult*b)
{return a->status==b->status&&a->pieces==b->pieces&&a->records==b->records&&
 a->record==b->record&&a->column==b->column&&a->row==b->row;}
int main(void)
{
 randomState=0x71ac46d1;
 unsigned successes=0,statuses[6]={0};
 for(unsigned test=0;test<WILLOW_FAST_PARITY_CASES;test++){
#ifdef WILLOW_FAST_PARITY_NATIVE
  *(volatile uint32_t*)0xff0008=test;
#endif
  uint16_t bank[1024];
  uint16_t records=(uint16_t)(nextRandom()%257),capacity=(uint16_t)(nextRandom()%81);
  WillowObjectSatView view={(uint16_t)(nextRandom()%193),(uint16_t)(nextRandom()%289),320,224,(uint8_t)(nextRandom()&1)};
  WillowObjectSatBudget a={0},b;WillowObjectSatPiece p[80],q[80];
  int8_t da[225],db[225];
  memset(p,0x5a,sizeof p);memset(q,0x5a,sizeof q);
  for(unsigned n=0;n<records;n++){
   bank[n*4]=(uint16_t)nextRandom();bank[n*4+1]=(uint16_t)nextRandom();
   bank[n*4+2]=(uint16_t)(nextRandom()%0x2a00);bank[n*4+3]=(uint16_t)nextRandom();
   if(test%4==0||n%4==0)bank[n*4+3]&=255;
   if(test%9==0&&n%3)memset(bank+n*4,0,8);
  }
  if(test%3==0&&records)bank[(nextRandom()%records)*4+3]=0xff00;
  if(test%7==0){
   a.count=(uint16_t)(nextRandom()%81);
   for(unsigned n=0;n<224;n++){a.sprites[n]=(uint8_t)(nextRandom()%21);a.pixels[n]=(uint16_t)(nextRandom()%321);}
  }
  if(test%101==0)a.count=81;
  if(test%103==0)a.sprites[nextRandom()%224]=21;
  if(test%107==0)a.pixels[nextRandom()%224]=321;
  const WillowObjectSatBudget initial=a;
  for(unsigned n=0;n<records;n++){
   uint16_t*destination=memory+(willowNativeObjectLatchOffsets[n>>3]>>1)+((n&7)<<2);
   memcpy(destination,bank+n*4,8);
  }
  Assets aa={0,0,(uint16_t)(test%4)},ab=aa;
  WillowObjectSatResult ra=willowObjectSatBuildReaderDeferred(readBank,bank,records,&view,&a,p,capacity,asset,&aa,da);
  for(unsigned variant=0;variant<2;variant++){
  b=initial;ab.hash=0;ab.calls=0;memset(q,0x5a,sizeof q);
  WillowObjectLatchSequential reader;
  if(!willowNativeObjectLatchSequentialInit(&reader,(const volatile uint8_t*)memory))return 9;
  WillowObjectSatResult rb=variant?
   willowObjectSatBuildNextFast(willowNativeObjectLatchNext,&reader,records,&view,&b,q,capacity,asset,&ab,db):
   willowObjectSatBuildReaderFast(readBank,bank,records,&view,&b,q,capacity,asset,&ab,db);
  if(ra.status!=rb.status){fprintf(stderr,"status mismatch case%u ref%u fast%u\n",test,ra.status,rb.status);return 1;}
  statuses[ra.status]++;
  if(!ra.status){
   if(!fieldsEqual(&ra,&rb)||memcmp(&a,&b,sizeof a)||memcmp(p,q,sizeof p)||aa.hash!=ab.hash||aa.calls!=ab.calls){
    fprintf(stderr,"success mismatch case%u: pieces %u/%u records %u/%u record %u/%u row %u/%u col %u/%u\n",test,ra.pieces,rb.pieces,ra.records,rb.records,ra.record,rb.record,ra.row,rb.row,ra.column,rb.column);return 2;
   }
   successes++;
  }
  }
 }
#ifdef WILLOW_FAST_PARITY_NATIVE
 *(volatile uint16_t*)0xff0010=(uint16_t)successes;
 for(unsigned n=0;n<6;n++)((volatile uint16_t*)0xff0012)[n]=(uint16_t)statuses[n];
#endif
 /* Every sequential record differs, including the last one in each chunk.
  * Invalid ordering must leave state/output untouched and never read gaps. */
 for(unsigned n=0;n<8192;n++)memory[n]=(uint16_t)nextRandom();
 WillowObjectLatchSequential reader;
 if(!willowNativeObjectLatchSequentialInit(&reader,(const volatile uint8_t*)memory))return 3;
 for(unsigned n=0;n<256;n++){
  uint16_t wanted[4],got[4]={0x5555,0x6666,0x7777,0x8888},saved[4];memcpy(saved,got,8);
  const volatile WillowObjectLatchWord*pointer=reader.next;
  if(willowNativeObjectLatchSequentialRead(&reader,(uint16_t)(n+1),got)||memcmp(saved,got,8)||reader.next!=pointer||reader.record!=n)return 4;
  if(!willowNativeObjectLatchReadRecord((const volatile uint8_t*)memory,n,wanted)||
     !willowNativeObjectLatchSequentialRead(&reader,n,got)||memcmp(wanted,got,8))return 5;
 }
 uint16_t got[4];
 if(willowNativeObjectLatchSequentialRead(&reader,256,got)||willowNativeObjectLatchSequentialRead(&reader,0,got))return 6;
 if(willowNativeObjectLatchSequentialInit(&reader,(const volatile uint8_t*)memory+1)||
    willowNativeObjectLatchSequentialRead(&reader,0,got)||willowNativeObjectLatchSequentialInit(&reader,0)||
    willowNativeObjectLatchSequentialInit(0,(const volatile uint8_t*)memory))return 7;
 printf("{\"cases\":48000,\"successes_exact_fields_pieces_budgets_callback_order\":%u,\"status_counts\":[%u,%u,%u,%u,%u,%u],\"sequential_records_exact\":256,\"invalid_order_refusals\":258}\n",successes,statuses[0],statuses[1],statuses[2],statuses[3],statuses[4],statuses[5]);
 return 0;
}
