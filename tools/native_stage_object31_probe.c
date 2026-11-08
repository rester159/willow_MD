#include "../src/willow_arcade_stage_object31.h"
static uint16_t readWord(uint16_t at){return *(volatile uint16_t*)(0xff0000UL+at);}
static uint8_t readByte(uint16_t at){return *(volatile uint8_t*)(0xff0000UL+at);}
static void putWord(uint16_t at,uint16_t value){*(volatile uint16_t*)(0xff0000UL+at)=value;}
static void putByte(uint16_t at,uint8_t value){*(volatile uint8_t*)(0xff0000UL+at)=value;}
static uint16_t romWord(uint32_t at){return *(const uint16_t*)at;}
uint32_t stageObject31Probe(uint32_t a4,uint32_t sourceRecord,uint32_t ordinal)
{
 (void)ordinal;
 const WillowVisibilityBus bus={readWord,readByte,putWord,putByte};
 return willowArcadeStageObject31WithA4(&bus,sourceRecord,romWord,&a4)?a4:0;
}
