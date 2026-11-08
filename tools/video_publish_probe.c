#include "../src/willow_arcade_video_publish.h"
static uint8_t rb(uint16_t at){return *(volatile uint8_t*)(0xff0000UL+at);}
static uint16_t rw(uint16_t at){return *(volatile uint16_t*)(0xff0000UL+at);}
static void wb(uint16_t at,uint8_t value){*(volatile uint8_t*)(0xff0000UL+at)=value;}
static void ww(uint16_t at,uint16_t value){*(volatile uint16_t*)(0xff0000UL+at)=value;}
static uint8_t input(uint16_t at){return rb(0x4000+at);}
static void byte(uint16_t at,uint8_t value){wb(0x4000+at,value);}
static void word(uint16_t at,uint16_t value){ww(0x4000+at,value);}
void videoPublishProbe(void)
{
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 willowArcadeVideoPublish(&bus,input,byte,word);
}
void videoIrqPrefixProbe(void)
{
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 willowArcadeObjectBasePublish(&bus,word);
 willowArcadeVideoPublish(&bus,input,byte,word);
}
