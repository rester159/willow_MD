#!/usr/bin/env python3
"""Observe actual native prepared subphase stores; never modify target state."""
from pathlib import Path
import argparse,hashlib,json,subprocess
from build_vdp_presentation_probe import ROOT,build
p=argparse.ArgumentParser();p.add_argument('--fixture-output',type=Path,default=ROOT/'build/native-prepared-objects/seeded-pointer1200');p.add_argument('--output',type=Path,default=ROOT/'build/native-prepared-objects/profile-seeded1200');args=p.parse_args()
base=args.fixture_output.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
e=json.loads((base/'evidence.json').read_text())
header=ROOT/'src/willow_native_object_prepared.h'
assert e['hashes'][str(header)]==hashlib.sha256(header.read_bytes()).hexdigest()
for name in ['prepared_fixture.h','latch_fixture.h']:(out/name).write_bytes((base/name).read_bytes())
source='#define WILLOW_PREPARED_OBJECT_PROFILE(n) (*(volatile uint16_t*)0xff0006=(n))\n'+(base/'prepared_native.c').read_text()
(out/'prepared_native.c').write_text(source)
build({n:base/(n+'.bin') for n in ['vram','cram','vsram','registers']},out,native_source=out/'prepared_native.c',native_include=out)
s=(ROOT/'tools/capture_vdp_presentation.c').read_text()
changes={
 'static unsigned phaseCount;':'static unsigned phaseCount;\nstatic uint64_t preparedCycles[5];static unsigned preparedCount;',
 ' if((address&0xffffff)==0xff0004){':' if((address&0xffffff)==0xff0006){if(value!=preparedCount+1||preparedCount>=5)exit(3);preparedCycles[preparedCount++]=(uint64_t)frame*896040+(int64_t)profileCPU->cycles;}\n if((address&0xffffff)==0xff0004){',
 ' profileCPU->map[255].write16=originalWrite16;':' if(preparedCount!=5)return 3;const char*preparedNames[]={"seed_construction_or_validation","endpoint_clear","cell_staging","endpoint_commit"};for(unsigned n=0;n<4;n++)printf("PREPARED %s %.9f\\n",preparedNames[n],(double)(preparedCycles[n+1]-preparedCycles[n])/53693.175);\n profileCPU->map[255].write16=originalWrite16;'}
for old,new in changes.items():assert s.count(old)==1;s=s.replace(old,new)
(out/'capture.c').write_text(s)
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-DWILLOW_PRESENTATION_PHASES','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'tools'),str(out/'capture.c'),'-o',str(out/'capture')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
r=subprocess.run([str(out/'capture'),str(core),str(out/'probe.bin'),str(out/'hardware.ppm')],capture_output=True,text=True,check=True);(out/'capture.log').write_text(r.stdout+r.stderr)
assert (out/'hardware.ppm').read_bytes()==(base/'hardware.ppm').read_bytes()
phases={line.split()[1]:float(line.split()[2]) for line in r.stdout.splitlines() if line.startswith('PREPARED ')}
files=[header,ROOT/'src/willow_native_object_cells_m68k.h',Path(__file__),base/'evidence.json',out/'prepared_native.c',out/'capture.c',out/'capture.log',out/'probe.bin',out/'hardware.ppm']
e={'scope':'Instrumented static full1200 prepared native subphases; original native bus writes only; source logic/audio/live caches excluded','phases_ms':phases,'hardware_pixels_exact':71680,'hashes':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}}
(out/'evidence.json').write_text(json.dumps(e,indent=2)+'\n');print(json.dumps(phases,indent=2))
