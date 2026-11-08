/* Native API proof entry only; not part of the source writer's shipping blob. */
#include "../src/willow_native_background_columns.h"
#include <stddef.h>
typedef char view_bytes[(sizeof(WillowNativeBackgroundColumnsView)==32)?1:-1];
typedef char view_masks[(offsetof(WillowNativeBackgroundColumnsView,pending)==16)?1:-1];
void willowBackgroundColumnsApiProbe(void)
{
 volatile uint16_t*io=(volatile uint16_t*)0x20f080;
 uint16_t command=io[0];
 WillowNativeBackgroundColumns*s=(command&0x100)?0:(WillowNativeBackgroundColumns*)0x20f000;
 WillowNativeBackgroundColumnsView*v=(command&0x200)?0:(WillowNativeBackgroundColumnsView*)0x20f0a0;
 uint8_t result=0;
 switch(command&255){
 case 0:willowNativeBackgroundColumnsInit(s);result=1;break;
 case 1:result=willowNativeBackgroundColumnsBaseline(s,*(volatile uint32_t*)0x20f084,*(volatile uint32_t*)0x20f088,*(volatile uint32_t*)0x20f08c);break;
 case 2:result=willowNativeBackgroundColumnsAcquire(s,v);break;
 case 3:result=willowNativeBackgroundColumnsViewCurrent(v);break;
 case 4:result=willowNativeBackgroundColumnsAcknowledge(s,v);break;
 }
 io[1]=result;
}
