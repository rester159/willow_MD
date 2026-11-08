#include "../src/willow_native_palette_add.h"
void paletteAddProbe(void)
{
 uint16_t delta=*(volatile uint16_t*)0xff1000UL;
 uint16_t offset=*(volatile uint16_t*)0xff1002UL;
 willowNativePaletteAdd16((volatile uint16_t*)(0xff2000UL+offset),delta);
}
