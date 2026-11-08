#include "../src/willow_native_visibility.h"
/* FF1000 pointer, FF1004 handle, FF1006/8 camera, FF100A mode; FF100C result. */
void nativeVisibilityProbe(void)
{
 uint8_t*r=(uint8_t*)*(volatile uint32_t*)0xff1000UL;
 uint16_t handle=*(volatile uint16_t*)0xff1004UL;
 uint16_t x=*(volatile uint16_t*)0xff1006UL,y=*(volatile uint16_t*)0xff1008UL;
 uint8_t mode=*(volatile uint8_t*)0xff100aUL;
 *(volatile uint8_t*)0xff100cUL=willowNativeVisibilityTryQueue(r,handle,x,y,mode);
}
