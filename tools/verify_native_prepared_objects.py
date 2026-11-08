#!/usr/bin/env python3
"""Host/native original-consumer parity for prepared native object templates."""
from pathlib import Path
import argparse,hashlib,json,subprocess
from build_vdp_presentation_probe import ROOT,build
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/native-prepared-objects/parity');p.add_argument('--native-cells',action='store_true');p.add_argument('--inside-boundaries',action='store_true');args=p.parse_args()
out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
host=out/'host-test'
entry=ROOT/('tools/test_native_prepared_objects_inside.c' if args.inside_boundaries else 'tools/test_native_prepared_objects_cells.c' if args.native_cells else 'tools/test_native_prepared_objects.c')
subprocess.run(['clang','-std=c99','-Wall','-Wextra','-Werror','-O2','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(entry),'-o',str(host)],check=True)
host_result=subprocess.run([str(host)],capture_output=True,text=True,check=True)
(out/'host.json').write_text(host_result.stdout);host_evidence=json.loads(host_result.stdout)
base=ROOT/'build/native-latch-presentation/inputs';target=out/'target'
build({n:base/(n+'.bin') for n in ['vram','cram','vsram','registers']},target,native_source=entry)
s=(ROOT/'tools/capture_vdp_presentation.c').read_text()
replacements={
 'if(frame!=30)return;':'(void)d;(void)w;(void)h;(void)pitch;return;\n if(frame!=30)return;',
 'for(frame=0;frame<=30;frame++)retro_run();':'for(frame=0;frame<360000;frame++){retro_run();const uint8_t*r=dlsym(core,"work_ram");uint32_t marker=0;for(unsigned n=0;n<4;n++)marker=marker<<8|r[n^1];if(marker==0x56445031||marker==0x4641494c)break;}captured=1;',
 'ready,captured);return 1;':'ready,captured);unsigned(*reg)(int)=dlsym(core,"m68k_get_reg");fprintf(stderr," PC%x SP%x SR%x frame%u ",reg(16),reg(15),reg(17),frame);for(unsigned at=8;at<34;at++)fprintf(stderr,"%02x",ram[at^1]);fputc(10,stderr);return 1;',
 'Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.':'Native prepared object parity completed; no video assertion, host target writes or GUI. Format%d.'}
for old,new in replacements.items():
 assert s.count(old)==1,old;s=s.replace(old,new)
needle=' retro_unload_game();retro_deinit();free(rom);return 0;'
assert s.count(needle)==1
s=s.replace(needle,' printf("PARITY_WORDS");for(unsigned at=8;at<34;at+=2)printf(" %u",((unsigned)ram[at^1]<<8)|ram[(at+1)^1]);putchar(10);\n'+needle)
(target/'capture.c').write_text(s)
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'tools'),str(target/'capture.c'),'-o',str(target/'capture')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
result=subprocess.run([str(target/'capture'),str(core),str(target/'probe.bin'),str(target/'unused.ppm')],capture_output=True,text=True)
(target/'capture.log').write_text(result.stdout+result.stderr)
result.check_returncode()
words=[int(n) for n in next(line for line in result.stdout.splitlines() if line.startswith('PARITY_WORDS')).split()[1:]]
assert words[0:3]==[0,host_evidence["comparisons"],0],words
statuses=words[5:11];assert statuses==host_evidence["status_counts"] and statuses[0]==words[4],words
assert words[11:13]==[host_evidence["certified_seed_comparisons"],host_evidence["empty_seed_comparisons"]],words
files=[Path(__file__),entry,ROOT/'tools/test_native_prepared_objects.c',ROOT/'src/willow_native_object_prepared.h',ROOT/'src/willow_native_object_cells_m68k.h',ROOT/'src/willow_native_object_sat.h',ROOT/'src/willow_native_object_sat_budget_m68k.h',out/'host.json',target/'probe.bin',target/'capture.c',target/'capture.log',core]
e={'scope':'Prepared native object compiler/origin refresh/directSAT versus original deferred source consumer; no live source producer or FPS admission','native_cell_loop':args.native_cells or args.inside_boundaries,'explicit_inside_boundaries':args.inside_boundaries,'host':host_evidence,'native_consumer_comparisons':sum(statuses),'native_certified_seed_comparisons':words[11],'native_empty_seed_comparisons':words[12],'native_successes_exact':words[4],'native_status_counts':statuses,'native_target_ready':True,'hashes':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}}
(out/'evidence.json').write_text(json.dumps(e,indent=2)+'\n');print(json.dumps({k:v for k,v in e.items() if k!='hashes'},indent=2))
