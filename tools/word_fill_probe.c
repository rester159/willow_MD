#include "../src/willow_native_word_fill.h"
void wordFillProbe(void)
{
 uint16_t value=*(volatile uint16_t*)0xff1000UL,words=*(volatile uint16_t*)0xff1004UL;
 willowNativeWordFill((uint16_t*)0xff2002UL,value,words);
 *(volatile uint16_t*)0xff1002UL=0x544b;
}
