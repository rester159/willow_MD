#!/usr/bin/env python3
"""Independently verify the bounded full-bank zero-run iterator on host/68000."""
from pathlib import Path
import hashlib,json,subprocess
from build_vdp_presentation_probe import ROOT,build
OUT=ROOT/'build/native-object-latch-next';OUT.mkdir(exist_ok=True)
test=ROOT/'tools/test_native_object_latch_next.c'
subprocess.run(['clang','-std=c99','-Wall','-Wextra','-Werror','-O2','-fstrict-aliasing',
                '-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'src'),str(test),'-o',str(OUT/'host-test')],check=True)
host=subprocess.check_output([str(OUT/'host-test')],text=True)
(OUT/'host.log').write_text(host);expected=json.loads(host)
assert expected['cases']==8*257*2
inputs={name:ROOT/'build/cartridge-gap-audit/frame1200_window_v1'/f'{name}.bin' for name in ['vram','cram','vsram','registers']}
build(inputs,OUT,native_source=test)
source=(ROOT/'tools/capture_vdp_presentation.c').read_text()
needle='if(frame!=30)return;';assert source.count(needle)==1
source=source.replace(needle,'(void)d;(void)w;(void)h;(void)pitch;return;\n '+needle)
needle='for(frame=0;frame<=30;frame++)retro_run();';assert source.count(needle)==1
source=source.replace(needle,'''for(frame=0;frame<120000;frame++){
 retro_run();const uint8_t*r=dlsym(core,"work_ram");uint32_t marker=0;
 for(unsigned n=0;n<4;n++)marker=marker<<8|r[n^1];
 if(marker==0x56445031||marker==0x4641494c)break;
 }captured=1;''')
needle='printf("Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.\\n",format);'
assert source.count(needle)==1
source=source.replace(needle,'''uint32_t totals[3]={0,0,0};
 for(unsigned n=0;n<3;n++)for(unsigned j=0;j<4;j++)totals[n]=totals[n]<<8|ram[(8+n*4+j)^1];
 printf("{\\"cases\\":%u,\\"reads\\":%u,\\"rejections\\":%u,\\"frames\\":%u}\\n",totals[0],totals[1],totals[2],frame+1);''')
(OUT/'capture.c').write_text(source)
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-fsanitize=address,undefined','-fno-sanitize-recover=all',
                '-I',str(ROOT/'tools'),str(OUT/'capture.c'),'-o',str(OUT/'capture')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
with (OUT/'native.log').open('w') as log:
    subprocess.run([str(OUT/'capture'),str(core),str(OUT/'probe.bin'),str(OUT/'unused.ppm')],stdout=log,stderr=subprocess.STDOUT,check=True)
actual=json.loads((OUT/'native.log').read_text())
assert all(actual[k]==v for k,v in expected.items()),(expected,actual)
files=[test,Path(__file__).resolve(),ROOT/'src/willow_native_object_latch_next.h',ROOT/'src/willow_native_object_latch_sequential.h',
       ROOT/'src/willow_native_object_latch.h',ROOT/'tools/capture_vdp_presentation.c',ROOT/'tools/build_vdp_presentation_probe.py',
       OUT/'capture.c',OUT/'host.log',OUT/'native.log',OUT/'probe.bin',OUT/'probe.sym',core]
report={'scope':'Independent record/address/canary iterator proof on sanitized host and actual68000; not geometry, production ownership orFPS',
        'host':expected,'native':actual,'source_span_canary_bytes':0x3680-0x2600,
        'limits':'Every exclusive limit0..256,8 source patterns,visible-zero flags0/1,output2/4-bytealignment',
        'invalid':'Null context/output/resolved,wrong record,limit>256,reversedbounds,invalid visibilityflag,failedinitializer; no mutation',
        'sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
(OUT/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
