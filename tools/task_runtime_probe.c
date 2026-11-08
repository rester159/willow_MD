#include "../src/willow_native_task_store.h"
#include "../src/willow_native_task_segments.h"
#include "../src/willow_native_palette_split.h"
#include "../src/willow_native_task_runtime.h"
#ifdef SGDK_GCC
static uint8_t* physical(uint16_t a){return (uint8_t*)(0xff0000UL+a);}
#define RUNTIME ((WillowNativeTaskRuntime*)0xff2400UL)
/* ROM is mapped at physical address zero on the 68000 test cartridge.
 * Express the hardware load directly instead of a C null-pointer object. */
static uint16_t probeSource(uint32_t a){
 uint16_t value;__asm__ volatile ("move.w (%1),%0":"=d"(value):"a"(a):"memory");return value;
}
#else
static uint8_t probeRam[65536];
static WillowNativeTaskRuntime probeRuntime;
static uint8_t* physical(uint16_t a){return probeRam+a;}
#define RUNTIME (&probeRuntime)
static uint16_t probeSource(uint32_t a);
#endif
#if defined(SGDK_GCC) && defined(WILLOW_PROBE_SPLIT_PALETTE)
static const WillowSplitPaletteSlots splitPaletteSlots={{(WillowMutablePaletteGroup*)0xff2800UL,(WillowMutablePaletteGroup*)0xff2900UL}};
#endif
#if defined(SGDK_GCC) && defined(WILLOW_PROBE_SEGMENTED_TASKS)
static const WillowNativeTaskSegments taskSegments={(uint8_t*)0xff2a00UL,(WillowNativeTaskControl*)0xff2c80UL,{(uint8_t*)0xff2d00UL,(uint8_t*)0xff2e00UL}};
#endif
static uint8_t* taskAddress(uint16_t a){
#if defined(SGDK_GCC) && defined(WILLOW_PROBE_SEGMENTED_TASKS)
 uint8_t*p=willowNativeTaskSegmentResolve(&taskSegments,a);
#else
 uint8_t*p=willowNativeTaskResolve((WillowNativeTaskStore*)physical(0x2000),a);
#endif
 return p?p:physical(a);
}
static uint8_t probeByte(uint16_t a){return *taskAddress(a);}
static uint16_t probeWord(uint16_t a){
#ifdef SGDK_GCC
 /* These verified task routines use aligned words inside mapped regions. */
 return *(uint16_t*)taskAddress(a);
#else
 return ((uint16_t)probeByte(a)<<8)|probeByte(a+1);
#endif
}
static void probePutByte(uint16_t a,uint8_t v){*taskAddress(a)=v;}
static void probePutWord(uint16_t a,uint16_t v){
#ifdef SGDK_GCC
 *(uint16_t*)taskAddress(a)=v;
#else
 probePutByte(a,v>>8);probePutByte(a+1,v);
#endif
}
#if !defined(SGDK_GCC) || !defined(WILLOW_PROBE_ROM_PALETTE)
static uint16_t probeBaseColor(uint32_t a){return willowMotionWord(physical(0x4000+a-0x91c000));}
static void probePutBaseColor(uint32_t a,uint16_t v){willowMotionPutWord(physical(0x4000+a-0x91c000),v);}
#endif
#if defined(SGDK_GCC) && defined(WILLOW_PROBE_ROM_PALETTE)
static __attribute__((noinline)) uint16_t probeExceptionalColor(uint32_t a){
 uint16_t value=0;
#ifdef WILLOW_PROBE_SPLIT_PALETTE
 if(!willowSplitPaletteRead((WillowNativePaletteOwner*)physical(0x2600),&splitPaletteSlots,a,probeSource,&value))willowMotionPutWord(physical(0x1004),1);
#else
 if(!willowMutablePaletteRead((WillowNativePaletteOwner*)physical(0x2600),
  (WillowMutablePaletteGroup*)physical(0x2800),8,a,probeSource,&value))willowMotionPutWord(physical(0x1004),1);
#endif
 return value;
}
#endif
static uint16_t probeColor(uint32_t a){
#if defined(SGDK_GCC) && defined(WILLOW_PROBE_ROM_PALETTE)
 /* Global fade visits only the validated CPS group addresses in 51E8/524A.
  * Keep that hot ROM path narrow; exceptions use the checked mutable owner. */
 WillowNativePaletteOwner*owner=(WillowNativePaletteOwner*)physical(0x2600);
 uint16_t group=(uint16_t)(a-0x91c000UL)>>5,source=owner->sources.group[group];
 /* With adjustGroup installed, this callback reads only group-leading
  * colors; per-color updates use that grouped callback instead. */
 if(source<=0xfff0)return (uint16_t)(probeSource(0xe0000UL+(uint32_t)source*2)+
  ((uint16_t)willowNativePaletteOffset(&owner->view,group)<<12));
 return probeExceptionalColor(a);
#elif defined(SGDK_GCC) && defined(WILLOW_PROBE_LAZY_PALETTE)
 return willowNativePaletteRead((WillowNativePaletteView*)physical(0x2500),a,probeBaseColor);
#else
 return probeBaseColor(a);
#endif
}
static void probePutColor(uint32_t a,uint16_t v){
#if defined(SGDK_GCC) && defined(WILLOW_PROBE_ROM_PALETTE)
#ifdef WILLOW_PROBE_SPLIT_PALETTE
 if(!willowSplitPaletteWrite((WillowNativePaletteOwner*)physical(0x2600),&splitPaletteSlots,a,v,probeSource))willowMotionPutWord(physical(0x1004),1);
#else
 if(!willowMutablePaletteWrite((WillowNativePaletteOwner*)physical(0x2600),
  (WillowMutablePaletteGroup*)physical(0x2800),8,a,v,probeSource))willowMotionPutWord(physical(0x1004),1);
#endif
#elif defined(SGDK_GCC) && defined(WILLOW_PROBE_LAZY_PALETTE)
 willowNativePaletteWrite((WillowNativePaletteView*)physical(0x2500),a,v,probePutBaseColor);
#else
 probePutBaseColor(a,v);
#endif
}
#ifdef SGDK_GCC
static void probeAdjustGroup(uint32_t address,uint16_t delta){
#if defined(WILLOW_PROBE_ROM_PALETTE)
 /* The verified fade supplies aligned group addresses and +/-1000 only. */
 uint16_t relative=(uint16_t)address-0xc000;
 uint8_t*pair=physical(0x2780+(relative>>6));
 if(relative&32)*pair=(uint8_t)(*pair+(delta>>8));
 else *pair=(*pair&0xf0)|((*pair+(delta>>12))&15);
#elif defined(WILLOW_PROBE_LAZY_PALETTE)
 (void)willowNativePaletteAdjust((WillowNativePaletteView*)physical(0x2500),address,delta);
#else
 uint16_t*p=(uint16_t*)physical(0x4000+address-0x91c000);
 #pragma GCC unroll 16
 for(uint16_t n=0;n<16;n++)p[n]+=delta;
#endif
}
#endif
#if defined(SGDK_GCC) && defined(WILLOW_PROBE_OWNER_FADE_ALL)
static void probeFadeAll(WillowPaletteFade*f,uint16_t delta){
#ifdef WILLOW_PROBE_SPLIT_PALETTE
 if(!willowSplitPaletteFadeAll((WillowNativePaletteOwner*)physical(0x2600),&splitPaletteSlots,f,delta,probeSource))willowMotionPutWord(physical(0x1004),1);
#else
 if(!willowNativePaletteFadeAll((WillowNativePaletteOwner*)physical(0x2600),
  (WillowMutablePaletteGroup*)physical(0x2800),8,f,delta,probeSource))willowMotionPutWord(physical(0x1004),1);
#endif
}
#endif
void taskRuntimeProbe(void){
 const WillowVisibilityBus bus={probeWord,probeByte,probePutWord,probePutByte};
 uint16_t cursor=0,remaining=15;
 for(;;){
#if defined(SGDK_GCC) && defined(WILLOW_PROBE_OWNER_FADE_ALL)
  WillowNativeTaskRun result=willowNativeTaskRunOneWithPaletteAll(RUNTIME,&bus,cursor,remaining,probeSource,probeColor,probePutColor,probeAdjustGroup,probeFadeAll);
#elif defined(SGDK_GCC)
  WillowNativeTaskRun result=willowNativeTaskRunOneGrouped(RUNTIME,&bus,cursor,remaining,probeSource,probeColor,probePutColor,probeAdjustGroup);
#else
  WillowNativeTaskRun result=willowNativeTaskRunOne(RUNTIME,&bus,cursor,remaining,probeSource,probeColor,probePutColor);
#endif
  if(result.unsupported){willowMotionPutWord(physical(0x1002),1);return;}
  if(!result.dispatched||result.remaining==65535)break;
  cursor=result.cursor;remaining=result.remaining;
 }
 willowArcadeTaskWake(&bus);
 willowMotionPutWord(physical(0x1002),0x5254);
}
