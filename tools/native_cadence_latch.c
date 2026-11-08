/* Experiment only. Raw object latch before source1554 writes the next bank.
 * The reference build addresses independent CPS buffers; target uses ordinary
 * Genesis SRAM. Both execute the same native copy and dispatch instructions. */
#include "../src/willow_native_object_latch.h"
#if defined(CADENCE_ORIGINAL_BUS)
#define LATCH_PORT 0x800100u
#define LATCH_BANK0 0x900000u
#define LATCH_BANK1 0x908000u
#else
#define LATCH_PORT 0x20f900u
#define LATCH_BANK0 0x20c000u
#define LATCH_BANK1 0x20d000u
#endif
__attribute__((noinline)) uint8_t nativeCadenceLatch(void)
{
 uint16_t published=*(const volatile uint16_t*)LATCH_PORT;
 *(volatile uint16_t*)0x20f02c=published;
 if(published!=0x9000&&published!=0x9080)return 0;
 const volatile void*bank=(const volatile void*)(published==0x9000?LATCH_BANK0:LATCH_BANK1);
 uint8_t valid=willowNativeObjectLatchCapture((volatile uint8_t*)0xff0000,bank);
 if(valid)(*(volatile uint32_t*)0x20f028)++;
 return valid;
}
