/* Opt-in diagnostic, native prior-port metadata -> bounded raw record prefix.
 * Production RAM ownership, prepared instance rendering and VDP admission are
 * separate. Missing source epochs preserve full256-record fallback. */
#include "../src/willow_native_object_latch_compact.h"
#ifdef CADENCE_ORIGINAL_BUS
#define PORT 0x800100u
#define BANK0 0x900000u
#define BANK1 0x908000u
#else
#define PORT 0x20f900u
#define BANK0 0x20c000u
#define BANK1 0x20d000u
#endif
#define WORD(at) (*(volatile uint16_t*)(at))
#define LONG(at) (*(volatile uint32_t*)(at))
__attribute__((noinline)) uint8_t nativeCadenceCompactLatch(void)
{
 uint16_t published=WORD(PORT);
 WORD(0x20f02c)=published;
 if(published!=0x9000&&published!=0x9080)return 0;
 uint16_t bank=published==0x9080,valid=WORD(0x20f520),records=256;
 if(valid>1||WORD(0x20f51e)!=bank)return 0;
 if(valid){
  records=WORD(0x20f51c);
  if(records>256)return 0;
 }
 const volatile void*source=(const volatile void*)(bank?BANK1:BANK0);
 if(!willowNativeObjectLatchCapturePrefix((volatile uint8_t*)0xff0000,source,records))return 0;
 WORD(0x20f530)=records;
 WORD(0x20f532)=valid;
 LONG(0x20f534)=valid?LONG(0x20f518):0;
 LONG(0x20f028)++;
 return 1;
}
