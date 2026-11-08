/* Fixed, disjoint test storage; this layout is not a production RAM plan. */
#include "../src/willow_native_actor_pool_bus.h"
#include "../src/willow_arcade_actor4_pool_services.h"
#include "../src/willow_arcade_actor10_zero.h"
#include "../src/willow_arcade_actor12_explosion.h"
#define POOL ((WillowActorPool*)0xff2000UL)
#define ID (*(volatile uint16_t*)0xff2010UL)
#define OP (*(volatile uint16_t*)0xff2012UL)
#define INDEX (*(volatile uint16_t*)0xff2014UL)
#define RESULT (*(volatile uint16_t*)0xff2016UL)
#define STATUS (*(volatile uint16_t*)0xff2018UL)
static uint16_t word(uint16_t at){uint16_t v=0;if(!willowNativePoolReadWord(POOL,ID,at,&v))STATUS=0;return v;}
static uint8_t byte(uint16_t at){uint8_t v=0;if(!willowNativePoolReadByte(POOL,ID,at,&v))STATUS=0;return v;}
static void putWord(uint16_t at,uint16_t v){if(!willowNativePoolWriteWord(POOL,ID,at,v))STATUS=0;}
static void putByte(uint16_t at,uint8_t v){if(!willowNativePoolWriteByte(POOL,ID,at,v))STATUS=0;}
static const WillowVisibilityBus bus={word,byte,putWord,putByte};
void nativePoolProbe(void)
{
 STATUS=1;RESULT=0;
 const WillowActorPoolSpec*s=willowActorPoolSpecification(ID);
 const WillowNativePoolAddresses*a=willowNativePoolAddresses(ID);
 if(!s||!a){STATUS=0;return;}
 if(OP==0){willowActorPoolInit(POOL,(uint8_t*)0xff3000UL,(uint8_t*)0xff2100UL,s->capacity,s->stride,ID);return;}
 if(OP==1){RESULT=willowActor18Pop(&bus,a->count,a->cursor);return;}
 if(OP==2){
  uint16_t at=willowActorPoolHandle(POOL,s->sourceBase,INDEX);
  if(!at){STATUS=0;return;}
  if(ID==4)willowArcadeActor4Release(&bus,at);
  else if(ID==10)willowArcadeActor10Release(&bus,at);
  else if(ID==12)willowArcadeActor12Release(&bus,at);
  else willowActorPoolRelease(POOL,INDEX);
  return;
 }
 if(OP==3){
  uint16_t start=a->end-s->capacity*2,untouched=0xbeef;
  if(willowNativePoolWriteByte(POOL,ID,a->count,s->capacity+1)||
     willowNativePoolWriteWord(POOL,ID,a->cursor,start+1)||
     willowNativePoolWriteWord(POOL,ID,start,s->sourceBase+1)||
     willowNativePoolWriteWord(POOL,ID,s->sourceBase+1,0xffff)||
     willowNativePoolWriteByte(POOL,ID,a->cursor,0)||
     willowNativePoolReadWord(POOL,ID,s->sourceBase-2,&untouched)||untouched!=0xbeef||
     willowNativePoolRecordSpan(POOL,ID,s->sourceBase+s->capacity*s->stride-1,2))STATUS=0;
  return;
 }
 STATUS=0;
}
