#!/usr/bin/env python3
"""Host sanitizer and actual68000 parity for compact source-key ID storage."""
from pathlib import Path
import argparse,hashlib,json,subprocess
from build_vdp_presentation_probe import ROOT,build

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/native-background-catalog/key-cache-v1');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 source=ROOT/'tools/test_native_background_key_cache.c';header=ROOT/'src/willow_native_background_key_cache.h'
 subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'src'),str(source),'-o',str(out/'host')],check=True)
 host=subprocess.check_output([str(out/'host')],text=True);(out/'host.json').write_text(host)
 fixture=ROOT/'build/cartridge-gap-audit/frame1200_window_v1'
 build({k:fixture/(k+'.bin')for k in ['vram','cram','vsram','registers']},out,native_source=source)
 capture=(ROOT/'tools/capture_vdp_presentation.c').read_text()
 def replace(old,new):
  nonlocal capture
  assert capture.count(old)==1,old;capture=capture.replace(old,new)
 replace('if(frame!=30)return;','(void)d;(void)w;(void)h;(void)pitch;return;\n if(frame!=30)return;')
 replace('for(frame=0;frame<=30;frame++)retro_run();',r'''
 const uint8_t*observed=dlsym(core,"work_ram");if(!observed)return 2;
 for(frame=0;frame<40000;frame++){
  retro_run();uint32_t mark=0;for(unsigned n=0;n<4;n++)mark=mark<<8|observed[n^1];
  if(mark==0x56445031||mark==0x4641494c)break;
 }captured=1;
 ''')
 replace('if(ready!=0x56445031||captured!=1)',r'''if(ready!=0x56445031){fprintf(stderr,"Key cache native failure line%u\n",ram[0x10^1]*256u+ram[0x11^1]);}
 if(ready!=0x56445031||captured!=1)''')
 replace('printf("Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.\\n",format);',r'''
 uint32_t values[3]={0};unsigned at[3]={8,12,0x24};
 for(unsigned k=0;k<3;k++)for(unsigned n=0;n<4;n++)values[k]=values[k]<<8|ram[(at[k]+n)^1];
 printf("{\"cases\":%u,\"checks\":%u,\"digest\":%u,\"emulated_frames\":%u}\n",values[0],values[1],values[2],frame+1);
 ''')
 (out/'capture.c').write_text(capture)
 subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tools'),str(out/'capture.c'),'-o',str(out/'capture')],check=True)
 core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
 native=subprocess.check_output([str(out/'capture'),str(core),str(out/'probe.bin'),str(out/'unused.ppm')],text=True);(out/'native.json').write_text(native)
 h=json.loads(host);n=json.loads(native)
 for k in h:assert h[k]==n[k],k
 report={'scope':'Compact source-key storage parity only. No source hooks, viewport cache, readiness or FPS admission. Actualtarget executes every check; host only reads completion results.','host':h,'native':n,'bytes':509,
         'hashes':{str(p):sha(p)for p in [Path(__file__).resolve(),source,header,ROOT/'tools/build_vdp_presentation_probe.py',ROOT/'tools/capture_vdp_presentation.c',core]},
         'outputs':{p.name:sha(p)for p in out.iterdir()if p.is_file()and p.name!='evidence.json'}}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'host':h,'native':n},indent=2))
if __name__=='__main__':main()
