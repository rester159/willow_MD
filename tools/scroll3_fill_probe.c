#include "../src/willow_arcade_scroll3_map_store.h"
void scroll3FillProbe(void)
{
 uint16_t id=*(volatile uint16_t*)0xff1000UL;
 *(volatile uint16_t*)0xff1002UL=willowScroll3MapFill((uint16_t*)0xff2002UL,id)?0x544b:1;
}
