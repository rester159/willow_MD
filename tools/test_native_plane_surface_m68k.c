#include <stdint.h>
#include <stddef.h>
#include "willow_native_plane_surface.h"
typedef char checkSurfaceSize[(sizeof(WillowNativePlaneSurface)==4182)?1:-1];
typedef char checkWordsOffset[(offsetof(WillowNativePlaneSurface,words)==20)?1:-1];
static void testFail(unsigned line,uint32_t actual,uint32_t expected){*(volatile uint16_t*)0xff0010=line;*(volatile uint32_t*)0xff0012=actual;*(volatile uint32_t*)0xff0016=expected;}
static void testWriteBegin(uint16_t at){*(volatile uint32_t*)0xc00004=willowNativePlaneCommand(at);}
static void testWriteWord(uint16_t v){*(volatile uint16_t*)0xc00000=v;}
static void testReadBegin(uint16_t at){*(volatile uint32_t*)0xc00004=((uint32_t)(at&0x3fff)<<16)|(at>>14);}
static uint16_t testReadWord(void){return *(volatile uint16_t*)0xc00000;}
static void testTimingBegin(unsigned kind,unsigned words){*(volatile uint16_t*)0xff0020=kind;*(volatile uint16_t*)0xff0022=words;*(volatile uint16_t*)0xff0004=1;}
static void testTimingEnd(unsigned rows,unsigned words){*(volatile uint16_t*)0xff0024=words;*(volatile uint16_t*)0xff0026=rows;*(volatile uint16_t*)0xff0004=2;}
#include "test_native_plane_surface_cases.h"
uint16_t nativeObjectPresentation(void){
 if(!testRun())return 0;
 *(volatile uint32_t*)0xff0008=successes;*(volatile uint16_t*)0xff000c=refusals;*(volatile uint16_t*)0xff000e=invalidEdits;return 1;
}
