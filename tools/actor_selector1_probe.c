#include "../src/willow_arcade_actor_selector1.h"
#include "../src/willow_arcade_actor12_explosion.h"
/* Standalone target diagnostic. Keep the original source-addressed RAM and
 * obtain the simulation phase from it, never from a renderer frame count. */
static uint8_t rb(uint16_t at){return *(volatile uint8_t*)(0xffff0000UL+at);}
static uint16_t rw(uint16_t at){return ((uint16_t)rb(at)<<8)|rb(at+1);}
static void wb(uint16_t at,uint8_t value){*(volatile uint8_t*)(0xffff0000UL+at)=value;}
static void ww(uint16_t at,uint16_t value){wb(at,value>>8);wb(at+1,value);}
static uint16_t source(unsigned long at){return *(const uint16_t*)at;}
static const WillowVisibilityBus bus={rw,rb,ww,wb};
static void release(uint8_t *record){willowArcadeActor12Release(&bus,(uint16_t)(unsigned long)record);}
void actorSelector1Probe(void)
{
 uint16_t handle=rw(0x4000);
 WillowActorSelector1Context c={rw(0x8408),rw(0x840c),rw(0x8344),rw(0x838c),rw(0x4002),handle,
  rb(0x8019),rb(0x8395),rb(0x83dd),source,&bus,release};
 ww(0x4004,willowArcadeActorSelector1((uint8_t*)(0xffff0000UL+handle),&c));
}
