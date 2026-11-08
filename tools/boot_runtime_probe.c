#include "../src/willow_native_boot_runtime.h"
_Static_assert(sizeof(WillowNativeBootTask)==8,"local boot continuation layout");
_Static_assert(sizeof(WillowNativeTaskRuntime)==32,"keep shared runtime layout");
static uint16_t rd(uint16_t at){return *(volatile uint16_t*)(0xff0000UL+at);}
static uint8_t rb(uint16_t at){return *(volatile uint8_t*)(0xff0000UL+at);}
static void wr(uint16_t at,uint16_t value){*(volatile uint16_t*)(0xff0000UL+at)=value;}
static void wb(uint16_t at,uint8_t value){*(volatile uint8_t*)(0xff0000UL+at)=value;}
static uint16_t source(uint32_t at){return *(const uint16_t*)at;}
void bootRuntimeProbe(void)
{
 const WillowVisibilityBus bus={rd,rb,wr,wb};
 WillowNativeTaskRun run=willowNativeTaskRunWithStoredBootVideo(
  (WillowNativeTaskRuntime*)0xff3000UL,
  &bus,0,0,source,0,0,0,0,0);
 wr(0x1002,run.unsupported?1:run.dispatched?0x424f:2);
}
