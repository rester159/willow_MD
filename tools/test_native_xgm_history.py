"""Compare original and local SDK profiling logic with deterministic Z80 samples."""
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
original = (Path(sys.argv[1])/'src/snd/xgm.c').read_text()
changed = (root/'native/intro_probe/src/willow_xgm_generated.inc').read_text()
def function(text, name):
    start = text.index(name+'()\n{')
    start = text.rfind('\n', 0, start)+1
    brace = text.index('{', start)
    depth = 1; end = brace+1
    while depth:
        depth += (text[end]=='{')-(text[end]=='}'); end += 1
    return text[start:end]
def block(text, prefix):
    start = text.index('// Z80 cpu load calculation')
    declarations = text[start:text.index('// set next frame helper', start)]
    result = declarations+function(text, 'XGM_getCPULoad')+'\n'+function(text, 'resetLoadCalculation')
    names = ['xgmIdleTab','xgmWaitTab','xgmLoadHistory','xgmTabInd','xgmIdleMean','xgmWaitMean','XGM_getCPULoad','resetLoadCalculation']
    return re.sub(r'\b('+'|'.join(names)+r')\b', lambda m: prefix+m[0], result)
# All driver code outside the two profiling functions and storage declaration
# must remain identical, including playback, Z80 handshakes and VBlank work.
normalized = changed[changed.index('#include "config.h"'):]
normalized = normalized.replace('#include <../src/snd/drv_xgm.h>', '#include "snd/drv_xgm.h"')
normalized = normalized.replace('static u16 *xgmLoadHistory;\n#define xgmIdleTab xgmLoadHistory\n#define xgmWaitTab (xgmLoadHistory + 32)', 'static u16 xgmIdleTab[32];\nstatic u16 xgmWaitTab[32];')
baseline = original
for name in ['XGM_getCPULoad','resetLoadCalculation']:
    baseline = baseline.replace(function(baseline,name), '/* profiling function */')
    normalized = normalized.replace(function(normalized,name), '/* profiling function */')
assert baseline == normalized, 'Non-profiling SDK behavior changed'
common = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef int16_t s16;typedef volatile uint8_t vu8;
#define NO_INLINE
#define TRUE true
#define Z80_DRIVER_XGM 3
static u8 params[4];
#define Z80_DRV_PARAMS ((uintptr_t)params-0x7c)
static int currentDriver,irqDepth,allocations,failAllocation;
static volatile u16 debugIntroOverflow;
static u16 history[64];
static void *MEM_alloc(unsigned size){assert(size==128&&irqDepth==1);allocations++;return failAllocation?0:history;}
static void memsetU16(u16*p,u16 value,unsigned count){for(unsigned n=0;n<count;n++)p[n]=value;}
static void SYS_disableInts(void){irqDepth++;}
static void SYS_enableInts(void){assert(irqDepth==1);irqDepth--;}
static bool Z80_getAndRequestBus(bool wait){assert(wait&&irqDepth==1);return false;}
static void Z80_releaseBus(void){assert(irqDepth==1);}
'''
main = r'''
int main(void){
 ref_resetLoadCalculation();local_resetLoadCalculation();assert(!allocations);
 currentDriver=Z80_DRIVER_XGM;failAllocation=1;memset(params,0x91,sizeof params);
 assert(!local_XGM_getCPULoad()&&debugIntroOverflow==1&&allocations==1&&!irqDepth);
 for(unsigned n=0;n<4;n++)assert(params[n]==0x91);
 failAllocation=0;unsigned state=0x713256ac;
 for(unsigned n=0;n<10000;n++){
  if(!(n%97)){ref_resetLoadCalculation();local_resetLoadCalculation();}
  currentDriver=n%11?Z80_DRIVER_XGM:-1;
  u8 input[4],after[4];for(unsigned i=0;i<4;i++){state=state*1664525+1013904223;input[i]=state>>24;}
  memcpy(params,input,4);u32 expected=ref_XGM_getCPULoad();memcpy(after,params,4);
  memcpy(params,input,4);u32 actual=local_XGM_getCPULoad();
  assert(expected==actual&&!memcmp(params,after,4)&&!irqDepth);
 }
 assert(allocations==2&&debugIntroOverflow==1);
 puts("SDK XGM profiling: 10000 original comparisons; resets, moving-average wrap and Z80 counter effects match; no allocation on driver reset, one lazy allocation, failed request leaves Z80 counters untouched");
}
'''
path=root/'build/test_native_xgm_history.c';path.write_text(common+block(original,'ref_')+'\n'+block(changed,'local_')+main)
exe=root/'build/test_native_xgm_history'
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(path),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
