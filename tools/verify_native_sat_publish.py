#!/usr/bin/env python3
"""Bounded prepared SAT upload: host guards and actual68000 VDP readback."""
from pathlib import Path
import hashlib
import json
import subprocess
from build_vdp_presentation_probe import ROOT, build

OUT = ROOT / 'build/native-sat-publish'
OUT.mkdir(exist_ok=True)
CORE = Path.home() / 'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
host_source = ROOT / 'tools/test_native_sat_publish.c'
native_source = ROOT / 'tools/test_native_sat_publish_m68k.c'
subprocess.run(['clang', '-std=c99', '-Wall', '-Wextra', '-Werror', '-O2',
                '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                '-I', str(ROOT / 'src'), str(host_source), '-o', str(OUT / 'host')], check=True)
host_log = subprocess.check_output([str(OUT / 'host')], text=True)
(OUT / 'host.json').write_text(host_log)
fixture = ROOT / 'build/cartridge-gap-audit/frame1200_window_v1'
build({key: fixture / f'{key}.bin' for key in ['vram', 'cram', 'vsram', 'registers']},
      OUT, native_source=native_source)
capture = (ROOT / 'tools/capture_vdp_presentation.c').read_text()
needle = 'if(frame!=30)return;'
assert capture.count(needle) == 1
capture = capture.replace(needle, '(void)d;(void)w;(void)h;(void)pitch;return;\n '+needle)
needle = 'for(frame=0;frame<=30;frame++)retro_run();'
assert capture.count(needle) == 1
capture = capture.replace(needle, '''for(frame=0;frame<20000;frame++){
 retro_run();const uint8_t*r=dlsym(core,"work_ram");uint32_t marker=0;
 for(unsigned n=0;n<4;n++)marker=marker<<8|r[n^1];
 if(marker==0x56445031||marker==0x4641494c)break;
 }captured=1;''')
needle = 'if(ready!=0x56445031||captured!=1)'
assert capture.count(needle) == 1
capture = capture.replace(needle, '''if(ready!=0x56445031){
 fprintf(stderr,"Native failure:");for(unsigned n=0x10;n<0x1a;n+=2)fprintf(stderr," %04x",ram[n^1]*256+ram[(n+1)^1]);fprintf(stderr,"\\n");
 }
 '''+needle)
needle = 'printf("Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.\\n",format);'
assert capture.count(needle) == 1
capture = capture.replace(needle, '''unsigned total=0;for(unsigned n=0;n<4;n++)total=total<<8|ram[(8+n)^1];
 printf("{\\"cases\\":%u,\\"frames\\":%u}\\n",total,frame+1);''')
(OUT / 'capture.c').write_text(capture)
subprocess.run(['clang', '-Wall', '-Wextra', '-Werror', '-O2', '-fsanitize=address,undefined',
                '-fno-sanitize-recover=all', '-I', str(ROOT / 'tools'), str(OUT / 'capture.c'),
                '-o', str(OUT / 'capture')], check=True)
with (OUT / 'native.log').open('w') as log:
    subprocess.run([str(OUT / 'capture'), str(CORE), str(OUT / 'probe.bin'), str(OUT / 'unused.ppm')],
                   stdout=log, stderr=subprocess.STDOUT, check=True)
native = json.loads((OUT / 'native.log').read_text())
assert native['cases'] == 162
files = [Path(__file__).resolve(), host_source, native_source,
         ROOT / 'src/willow_native_sat_publish.h', ROOT / 'src/willow_native_presentation_plan.h',
         ROOT / 'tools/build_vdp_presentation_probe.py', ROOT / 'tools/capture_vdp_presentation.c', CORE]
files += [OUT / name for name in ['host.json', 'native.log', 'capture.c', 'probe.bin', 'probe.sym']]
result = {'scope': 'Prepared SAT transfer guards and exact target VDP readback; not geometry, sprite budget admission or live frame deadlines',
          'host': json.loads(host_log), 'native': native,
          'native_readback': 'All count0..80, two word alignments, all640 SAT bytes plus8-byte guards on each side; repeat after rejected writes',
          'hashes': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
(OUT / 'evidence.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
