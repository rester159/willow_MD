#!/usr/bin/env python3
"""Retained SAT: sanitized host cases plus actual68000 VRAM/traffic/cost proof."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
from build_vdp_presentation_probe import ROOT, build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/native-sat-retained/v1')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    core = Path.home() / 'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
    host_source = ROOT / 'tools/test_native_sat_retained.c'
    native_source = ROOT / 'tools/test_native_sat_retained_m68k.c'
    subprocess.run(['clang', '-std=c99', '-Wall', '-Wextra', '-Werror', '-O2',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I', str(ROOT / 'src'), str(host_source), '-o', str(out / 'host')], check=True)
    host_log = subprocess.check_output([str(out / 'host')], text=True)
    (out / 'host.json').write_text(host_log)
    fixture = ROOT / 'build/cartridge-gap-audit/frame1200_window_v1'
    build({key: fixture / f'{key}.bin' for key in ['vram', 'cram', 'vsram', 'registers']},
          out, native_source=native_source)
    capture = (ROOT / 'tools/capture_vdp_presentation.c').read_text()

    def replace(old, new):
        nonlocal capture
        assert capture.count(old) == 1, old
        capture = capture.replace(old, new)

    replace('static bool env(unsigned cmd,void*data){', r'''
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} RetainedCPU;
static RetainedCPU*retainedCPU;
static uint8_t*retainedRAM;
static void(*ramWrite)(unsigned,unsigned),(*vdpWrite)(unsigned,unsigned);
static unsigned observed,dirtyData,dirtyControl,kind,count,active;
static uint64_t beginCycle;
static struct {unsigned count,data,controls;uint64_t sum,min,max;} timings[14];
static unsigned ramWord(unsigned offset){return retainedRAM[offset^1]*256+retainedRAM[(offset+1)^1];}
static uint64_t nowCycle(void){return (uint64_t)frame*896040+(int64_t)retainedCPU->cycles;}
static void observeVDP(unsigned address,unsigned value){
 if(active){
  if((address&31)<4)dirtyData++;
  else if((address&31)<8)dirtyControl++;
  else{fprintf(stderr,"Unexpected active VDP address%x\n",address);exit(1);}
 }
 vdpWrite(address,value);
}
static void observeRAM(unsigned address,unsigned value){
 if((address&0xffffff)==0xff0004){
  if(value==1){
   if(active)exit(1);
   kind=ramWord(0x20);count=ramWord(0x22);
   if(kind>=14)exit(1);
   dirtyData=dirtyControl=0;active=1;beginCycle=nowCycle();
  }else if(value==2){
   if(!active)exit(1);
   uint64_t elapsed=nowCycle()-beginCycle;active=0;
   if(dirtyData!=ramWord(0x24)||dirtyControl!=ramWord(0x26)*2){
    fprintf(stderr,"Traffic mismatch sample%u kind%u count%u data%u/%u control%u/%u\n",observed,kind,count,dirtyData,ramWord(0x24),dirtyControl,ramWord(0x26)*2);exit(1);
   }
   timings[kind].count++;timings[kind].data+=dirtyData;timings[kind].controls+=dirtyControl;
   timings[kind].sum+=elapsed;
   if(timings[kind].count==1||elapsed<timings[kind].min)timings[kind].min=elapsed;
   if(elapsed>timings[kind].max)timings[kind].max=elapsed;
   observed++;
  }else exit(1);
 }
 if(ramWrite)ramWrite(address,value);
 else{address&=65535;retainedRAM[address^1]=value>>8;retainedRAM[(address+1)^1]=value;}
}
static bool env(unsigned cmd,void*data){''')
    replace('if(frame!=30)return;', '(void)d;(void)w;(void)h;(void)pitch;return;\n if(frame!=30)return;')
    replace('for(frame=0;frame<=30;frame++)retro_run();', r'''
 retainedCPU=dlsym(core,"m68k");retainedRAM=dlsym(core,"work_ram");
 FN(unsigned,m68k_get_reg,(int));
 uint8_t*pal=dlsym(core,"vdp_pal");uint16_t*lines=dlsym(core,"lines_per_frame");
 if(!retainedCPU||!retainedRAM||!pal||!lines||*pal||*lines!=262||retainedCPU->pc!=m68k_get_reg(16))return 2;
 for(unsigned n=0;n<16;n++)if(retainedCPU->dar[n]!=m68k_get_reg(n))return 2;
 ramWrite=retainedCPU->map[255].write16;vdpWrite=retainedCPU->map[192].write16;
 if(!vdpWrite)return 2;
 retainedCPU->map[255].write16=observeRAM;retainedCPU->map[192].write16=observeVDP;
 for(frame=0;frame<20000;frame++){
  retro_run();uint32_t marker=0;for(unsigned n=0;n<4;n++)marker=marker<<8|retainedRAM[n^1];
  if(marker==0x56445031||marker==0x4641494c)break;
 }captured=1;
 retainedCPU->map[255].write16=ramWrite;retainedCPU->map[192].write16=vdpWrite;
''')
    replace('if(ready!=0x56445031||captured!=1)', r'''if(ready!=0x56445031){
 fprintf(stderr,"Native failure:");for(unsigned n=0x10;n<0x1a;n+=2)fprintf(stderr," %04x",ram[n^1]*256+ram[(n+1)^1]);fprintf(stderr,"\n");
 }
 if(ready!=0x56445031||captured!=1)''')
    replace('printf("Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.\\n",format);', r'''
 unsigned total=0;for(unsigned n=0;n<4;n++)total=total<<8|ram[(8+n)^1];
 printf("{\"publications\":%u,\"refusals\":%u,\"observed_calls\":%u,\"frames\":%u,\"timings\":[",total,ramWord(12),observed,frame+1);
 unsigned comma=0;
 for(unsigned n=0;n<14;n++)if(timings[n].count){
  printf("%s{\"kind\":%u,\"calls\":%u,\"data_words\":%u,\"control_words\":%u,\"min_ms\":%.9f,\"mean_ms\":%.9f,\"max_ms\":%.9f}",comma?",":"",n,timings[n].count,timings[n].data,timings[n].controls,(double)timings[n].min/53693.175,(double)timings[n].sum/timings[n].count/53693.175,(double)timings[n].max/53693.175);comma=1;
 }
 printf("]}\n");''')
    (out / 'capture.c').write_text(capture)
    subprocess.run(['clang', '-Wall', '-Wextra', '-Werror', '-O2', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-I', str(ROOT / 'tools'), str(out / 'capture.c'),
                    '-o', str(out / 'capture')], check=True)
    with (out / 'native.json').open('w') as log:
        subprocess.run([str(out / 'capture'), str(core), str(out / 'probe.bin'), str(out / 'unused.ppm')],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    native = json.loads((out / 'native.json').read_text())
    host = json.loads(host_log)
    assert native['publications'] == host['publications'] == 1703
    assert native['refusals'] == host['refusals'] == 16
    assert native['observed_calls'] == 1703+16+32
    names = {10: 'stable34', 11: 'one_dirty_of34', 12: 'all_dirty34', 13: 'baseline_full34'}
    representative = {names[row['kind']]: row for row in native['timings'] if row['kind'] in names}
    assert representative['stable34']['data_words'] == representative['stable34']['control_words'] == 0
    files = [Path(__file__).resolve(), host_source, native_source,
             ROOT / 'tools/test_native_sat_retained_cases.h', ROOT / 'src/willow_native_sat_retained.h',
             ROOT / 'src/willow_native_sat_publish.h', ROOT / 'src/willow_native_presentation_plan.h',
             ROOT / 'tools/build_vdp_presentation_probe.py', ROOT / 'tools/capture_vdp_presentation.c', core]
    files += [out / name for name in ['host.json', 'native.json', 'capture.c', 'probe.bin', 'probe.sym', 'manifest.json']]
    result = {'scope': 'Retained prepared SAT transfer guards, target VRAM readback and actual VDP word traffic; not geometry, sprite budget admission, live source binding or frame deadlines',
              'host': host, 'native': native, 'representative_costs': representative,
              'timing_scope': 'Serialized display-disabled diagnostic; includes identical marker/stat bookkeeping. Compiler context affects live cost; continuous renderer timing required separately.',
              'layout': {'bytes': 662, 'words': 0, 'generation': 640, 'command': 644, 'plan': 648, 'epoch': 652, 'count': 656, 'capacity': 658, 'ready': 660},
              'checks': ['All count0..80 and two word alignments', 'Every capacity1..80 with initially nonzero VRAM, empty first upload and growth into unknown zero shadow', 'Changed and unchanged XY/tile/link words; contiguous and scattered dirty runs', '16KiB VDP command boundary', 'Shrink/link termination, empty clear, zero-count reuse', 'Generation, plan, epoch-domain, base, capacity and explicit invalidation force full used prefix', '512 deterministic mixed-count/mutation sequences with stable repeat', '16 invalid calls leave shadow/stats/VRAM and actual VDP traffic untouched', 'All640 table bytes plus8-byte guards on each side per case', 'Compiler-asserted actual68000 shadow layout'],
              'hashes': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
    (out / 'evidence.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({'output': str(out), 'host': host, 'native': native, 'representative_costs': representative}, indent=2))


if __name__ == '__main__':
    main()
