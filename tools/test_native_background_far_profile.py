#!/usr/bin/env python3
"""Read-only actual JSR/RTS stack-bus profiling of an unchanged far-stream target."""
from pathlib import Path
import json,re,struct,subprocess,sys
import test_native_background_far_publication as observer
ROOT=Path(__file__).resolve().parents[1]
replace=observer.replace

def tables(platform,out):
 text=subprocess.check_output(['m68k-elf-objdump','-d',str(platform/'render.elf')],text=True)
 original=(platform/'platform-original.bin').read_bytes();native=(platform/'platform.bin').read_bytes();extra=(platform/'extra-rom.bin')
 target=json.loads((platform/'manifest.json').read_text());raw_extra=(platform/target['extra_rom']['file']).read_bytes();origin=target['extra_rom']['origin']
 def raw(at,size):
  if 0x100010<=at<0x110000:
   b=original[at-0x100010:at-0x100010+size];assert native[at-0x100010:at-0x100010+size]==b;return b
  return raw_extra[at-origin:at-origin+size]
 functions=[];calls=[];returns=[]
 for line in text.splitlines():
  m=re.match(r'^([0-9a-f]+) <([^>]+)>:',line)
  if m:
   name=m[2];at=int(m[1],16)
   if name.startswith(('bg','willowFar','nativeBackground')):functions.append((at,name))
   continue
  m=re.match(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{4}\s+)+)\s*(.*)',line)
  if not m:continue
  at=int(m[1],16);op=int(m[2].split()[0],16);assembly=m[3]
  if not (0x102000<=at<0x10c000 or 0x146000<=at<0x150000):continue
  if op==0x4e75:assert raw(at,2)==bytes.fromhex('4e75');returns.append(at+2)
  if not assembly.startswith('jsr'):continue
  reg=16
  if op==0x4eb9:size=6;callee=struct.unpack('>I',raw(at+2,4))[0]
  elif op==0x4eba:size=4;callee=at+2+struct.unpack('>h',raw(at+2,2))[0]
  elif op&0xfff8==0x4e90:size=2;callee=0;reg=8+(op&7)
  else:raise AssertionError(('Unrecognized JSR',line))
  assert raw(at,2)==struct.pack('>H',op);calls.append((at+size,callee,reg))
 assert functions and calls and returns
 data={'scope':__doc__,'functions':[{'begin':a,'name':n}for a,n in functions],'calls':calls,'returns':returns,'duration_boundary':'first complete observed native JSR return-PC stack write through RTS final stack-word read; includes callee/nested work, excludes outer instruction prefetch/return refill','irq_subtraction':'actual native IRQ first saved-SR write through RTE last hardware-frame byte read'}
 (out/'far-profile-sites.json').write_text(json.dumps(data,indent=2)+'\n')
 header='typedef struct{unsigned ret,callee,reg;} FarProfileCallSite;\nstatic const FarProfileCallSite farProfileCallSites[]={'+','.join('{%du,%du,%du}'%v for v in sorted(calls))+'};\n'
 header+='static const unsigned farProfileReturnSites[]={'+','.join(str(v)+'u'for v in sorted(returns))+'};\nstatic const struct{unsigned begin;const char*name;}farProfileFunctions[]={'+','.join('{%du,"%s"}'%(a,n)for a,n in functions)+'};\n'
 (out/'far-profile-sites.h').write_text(header)

def bind(platform,target,text,out,manifest):
 text=base_bind(platform,target,text,out,manifest);tables(platform,out)
 own=out/'frozen-observer-tools';header=own/'test_native_background_far_profile.h';header.write_bytes((ROOT/'tools/test_native_background_far_profile.h').read_bytes())
 bg=own/'test_native_background_publication.h';b=bg.read_text()
 b=replace(b,' if(width==2&&sourcePC()==CADENCE_IRQ+2){',' if(width==2&&sourcePC()==CADENCE_IRQ+2){\n  farProfileIRQ(0,at);')
 b=replace(b,'static void backgroundReadWork(unsigned at){','static void backgroundReadWork(unsigned at){\n farProfileIRQ(1,at);');bg.write_text(b)
 text=replace(text,'static unsigned frameNumber;','static unsigned frameNumber;\nstatic void farProfileAfterWork(unsigned,unsigned),farProfileRead(unsigned,unsigned),farProfileIRQ(unsigned,unsigned);')
 text=replace(text,'static void headroomAfterWrite(unsigned at,unsigned width,unsigned value){','static void headroomAfterWrite(unsigned at,unsigned width,unsigned value){\n farProfileAfterWork(at,width);')
 text=replace(text,'static void headroomRead(unsigned kind,unsigned at,unsigned width,unsigned value){','static void headroomRead(unsigned kind,unsigned at,unsigned width,unsigned value){\n if(kind==0)farProfileRead(at,width);')
 needle='#include "'+str(own/'test_native_background_far_publication.h')+'"'
 text=replace(text,needle,needle+'\n#include "'+str(out/'far-profile-sites.h')+'"\n#include "'+str(header)+'"')
 text=replace(text,' if(!farInit())return 2;',' if(!farInit())return 2;\n farProfileInit();')
 for p in [header,bg,out/'far-profile-sites.h',out/'far-profile-sites.json',Path(__file__)]:
  frozen=own/p.name if p.parent==ROOT/'tools' else p
  if frozen!=p:frozen.write_bytes(p.read_bytes())
  manifest['inputs'][str(frozen.resolve())]=observer.sha(frozen)
 fh=own/'test_native_forest_extension.h';f=fh.read_text();f=replace(f,'static void forestStopAfterPairedFrame(unsigned ticks){','static void forestStopAfterPairedFrame(unsigned ticks){\n farProfileFinish();');fh.write_text(f);manifest['inputs'][str(fh.resolve())]=observer.sha(fh)
 manifest['read_only_function_profile']=True;return text
base_bind=observer.bind
if __name__=='__main__':observer.bind=bind;observer.main()
