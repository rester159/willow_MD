"""Check the generated SDK variant against original missed-frame/average logic."""
from pathlib import Path
import re,subprocess,sys
root=Path(__file__).resolve().parents[1]
original=(Path(sys.argv[1])/'src/sys.c').read_text()
changed=(root/'native/intro_probe/src/willow_sys_generated.inc').read_text()
def profile_source(text,enabled):
 out=[];active=True;inside=False
 for line in text.splitlines():
  if line in ('#ifdef WILLOW_SDK_FRAME_PROFILE','#ifndef WILLOW_SDK_FRAME_PROFILE'):
   assert not inside
   inside=True;active=enabled if line.startswith('#ifdef') else not enabled
  elif inside and line=='#else':active=not active
  elif inside and line=='#endif':inside=False;active=True
  elif active:out.append(line)
 assert not inside
 return '\n'.join(out)
enabled=profile_source(changed,True)
enabled=enabled[enabled.index('#include "config.h"'):]
assert re.sub(r'\s+','',enabled)==re.sub(r'\s+','',original), 'Profiling build changes original SDK behavior'
def function(text,name):
 start=re.search(r'^\w+ '+name+r'\([^\n]*\)\n\{',text,re.M).start()
 brace=text.index('{',start);end=brace+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
def block(text,prefix,history):
 declarations=('static u16 frameLoads[8],frameLoadIndex,cpuFrameLoad;\n' if history else '')
 body=declarations+function(text,'addFrameLoad')+'\n'+function(text,'SYS_getCPULoad')
 names=['frameLoads','frameLoadIndex','cpuFrameLoad','addFrameLoad','SYS_getCPULoad']
 return re.sub(r'\b('+'|'.join(names)+r')\b',lambda m:prefix+m[0],body)
common='''#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef uint16_t u16;typedef uint32_t u32;
#define TRUE true
#define FALSE false
#define LOAD_MEAN_FRAME_NUM 8
static unsigned debugIntroOverflow;
'''
main='''int main(void){
 u32 time=0;unsigned checks=0;
 const u16 loads[]={0,1,223,224,255,65535};
 for(unsigned delta=0;delta<65536;delta++)for(unsigned i=0;i<6;i++){
  time+=delta;bool expected=ref_addFrameLoad(loads[i],time);
  assert(profile_addFrameLoad(loads[i],time)==expected);
  assert(normal_addFrameLoad(loads[i],time)==expected);
  assert(profile_SYS_getCPULoad()==ref_SYS_getCPULoad());
  assert(profile_cpuFrameLoad==ref_cpuFrameLoad && profile_frameLoadIndex==ref_frameLoadIndex);
  assert(!memcmp(profile_frameLoads,ref_frameLoads,sizeof ref_frameLoads));checks++;
 }
 assert(!debugIntroOverflow);assert(normal_SYS_getCPULoad()==65535 && debugIntroOverflow==1);
 printf("SDK frame history: %u original comparisons; all 16-bit frame deltas, timer wrap, missed-frame return and enabled averages match; disabled query reports unavailable\\n",checks);
}'''
source=common+block(original,'ref_',True)+block(enabled,'profile_',True)+block(profile_source(changed,False),'normal_',False)+main
path=root/'build/test_native_frame_history.c';path.write_text(source)
exe=root/'build/test_native_frame_history'
subprocess.run(['clang','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(path),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
