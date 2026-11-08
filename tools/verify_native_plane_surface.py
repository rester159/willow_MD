#!/usr/bin/env python3
"""Sanitized host and actual 68000 retained-plane VRAM/traffic admission."""
from pathlib import Path
import argparse,hashlib,json,subprocess
from build_vdp_presentation_probe import ROOT,build
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/native-plane-surface/current');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
 files=[ROOT/'tools'/n for n in ['test_native_plane_surface.c','test_native_plane_surface_m68k.c','test_native_plane_surface_cases.h']]
 files+=[ROOT/'src/willow_native_plane_surface.h',ROOT/'src/willow_native_presentation_plan.h',Path(__file__),ROOT/'tools/build_vdp_presentation_probe.py',ROOT/'tools/capture_vdp_presentation.c',core]
 before={str(q):sha(q)for q in files}
 subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'src'),str(files[0]),'-o',str(out/'host')],check=True)
 host=subprocess.check_output([str(out/'host')],text=True);(out/'host.json').write_text(host)
 fixture=ROOT/'build/cartridge-gap-audit/frame1200_window_v1'
 build({k:fixture/(k+'.bin')for k in ['vram','cram','vsram','registers']},out,native_source=files[1])
 capture=(ROOT/'tools/capture_vdp_presentation.c').read_text()
 def replace(old,new):
  nonlocal capture
  assert capture.count(old)==1,old[:60];capture=capture.replace(old,new)
 replace('static bool env(unsigned cmd,void*data){',r'''
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;int cycles,refresh;unsigned end,dar[16],pc;
} PlaneCPU;
static PlaneCPU*planeCPU;static uint8_t*planeRAM;
static void(*ramWrite)(unsigned,unsigned),(*vdpWrite)(unsigned,unsigned);
static unsigned measured,active,kind,expectedWords,dataWords,controlWords;
static uint64_t begin;
static struct {unsigned calls,words,controls;uint64_t total,min,max;} timings[9];
static unsigned rw(unsigned a){return planeRAM[a^1]*256+planeRAM[(a+1)^1];}
static uint64_t stamp(void){return (uint64_t)frame*896040+(int64_t)planeCPU->cycles;}
static void observeVDP(unsigned at,unsigned v){
 if(active){if((at&31)<4)dataWords++;else if((at&31)<8)controlWords++;else exit(1);}
 vdpWrite(at,v);
}
static void observeRAM(unsigned at,unsigned v){
 if((at&0xffffff)==0xff0004){
  if(v==1){if(active)exit(1);kind=rw(0x20);expectedWords=rw(0x22);if(kind>=9)exit(1);active=1;begin=stamp();dataWords=controlWords=0;}
  else if(v==2){
   if(!active)exit(1);uint64_t elapsed=stamp()-begin;active=0;
   if(dataWords!=rw(0x24)||controlWords!=rw(0x26)*2){fprintf(stderr,"Actual VDP traffic call%u kind%u words%u/%u controls%u/%u\n",measured,kind,dataWords,rw(0x24),controlWords,rw(0x26)*2);exit(1);}
   if(dataWords!=expectedWords)exit(1);
   timings[kind].calls++;timings[kind].words+=dataWords;timings[kind].controls+=controlWords;timings[kind].total+=elapsed;
   if(timings[kind].calls==1||elapsed<timings[kind].min)timings[kind].min=elapsed;
   if(elapsed>timings[kind].max)timings[kind].max=elapsed;measured++;
  }else exit(1);
 }
 if(ramWrite)ramWrite(at,v);else{at&=65535;planeRAM[at^1]=v>>8;planeRAM[(at+1)^1]=v;}
}
static bool env(unsigned cmd,void*data){''')
 replace('if(frame!=30)return;','(void)d;(void)w;(void)h;(void)pitch;return;\n if(frame!=30)return;')
 replace('for(frame=0;frame<=30;frame++)retro_run();',r'''
 planeCPU=dlsym(core,"m68k");planeRAM=dlsym(core,"work_ram");
 FN(unsigned,m68k_get_reg,(int));
 uint8_t*pal=dlsym(core,"vdp_pal");uint16_t*lines=dlsym(core,"lines_per_frame");
 if(!planeCPU||!planeRAM||!pal||!lines||*pal||*lines!=262||planeCPU->pc!=m68k_get_reg(16))return 2;
 for(unsigned n=0;n<16;n++)if(planeCPU->dar[n]!=m68k_get_reg(n))return 2;
 ramWrite=planeCPU->map[255].write16;vdpWrite=planeCPU->map[192].write16;if(!vdpWrite)return 2;
 planeCPU->map[255].write16=observeRAM;planeCPU->map[192].write16=observeVDP;
 for(frame=0;frame<40000;frame++){
  retro_run();unsigned mark=rw(0)*65536u+rw(2);if(mark==0x56445031||mark==0x4641494c)break;
 }captured=1;
 planeCPU->map[255].write16=ramWrite;planeCPU->map[192].write16=vdpWrite;
''')
 replace('if(ready!=0x56445031||captured!=1)',r'''if(ready!=0x56445031){fprintf(stderr,"Native plane failure line%u actual%x expected%x, PC%x\n",rw(0x10),rw(0x12)*65536u+rw(0x14),rw(0x16)*65536u+rw(0x18),planeCPU->pc);}
 if(ready!=0x56445031||captured!=1)''')
 replace('printf("Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.\\n",format);',r'''
 printf("{\"publications\":%u,\"refusals\":%u,\"invalid_edits\":%u,\"observed_calls\":%u,\"emulated_frames\":%u,\"timings\":[",rw(8)*65536u+rw(10),rw(12),rw(14),measured,frame+1);
 for(unsigned n=0;n<9;n++)printf("%s{\"kind\":%u,\"calls\":%u,\"data_words\":%u,\"control_words\":%u,\"min_ms\":%.9f,\"mean_ms\":%.9f,\"max_ms\":%.9f}",n?",":"",n,timings[n].calls,timings[n].words,timings[n].controls,timings[n].min/53693.175,timings[n].calls?(double)timings[n].total/timings[n].calls/53693.175:0,timings[n].max/53693.175);
 printf("]}\n");''')
 generated=out/'capture.c';generated.write_text(capture)
 subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tools'),str(generated),'-o',str(out/'capture')],check=True)
 with (out/'native.json').open('w')as f:subprocess.run([str(out/'capture'),str(core),str(out/'probe.bin'),str(out/'unused.ppm')],stdout=f,stderr=subprocess.STDOUT,check=True)
 h=json.loads(host);n=json.loads((out/'native.json').read_text())
 for k in ['publications','refusals','invalid_edits']:assert h[k]==n[k],k
 assert n['observed_calls']==n['publications']+n['refusals']
 assert all(sha(Path(p))==s for p,s in before.items()),'Input changed during proof'
 result={'scope':__doc__,'host':h,'native':n,'surface_bytes_m68k':4182,'words_offset_m68k':20,
  'checks':['Every64x32 cell and unchanged repeat','2x2/4x4 blocks at every origin with both-axis wrap, padded stride and both word alignments','Every bitmap byte0..255 in every row-byte position;1024 independent sparse multi-row patterns','All8 legal base addresses and target moves force full reload','All2048 native plane words plus8-byte before/after VRAM sentinels and RAM struct canaries per call','Every row corrupted first>last or last>=64 refuses before all VDP stores','Epoch/frame/plan/epoch-domain/same-address lease rebind refusal','Invalid set/block bounds are side-effect free','Actual target VDP word/control traffic matches exact expected row spans; unchanged/refused calls emit zero'],
  'timing_scope':'Serialized display-disabled actual68000 publication only, including equal marker/stat bookkeeping; excludes edits/resolution/readback/certification and is not active-display VBlank or live game FPS evidence.',
  'source_hashes':before,'output_hashes':{p.name:sha(p)for p in out.iterdir()if p.is_file()and p.name!='evidence.json'}}
 (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'host':h,'native':n},indent=2))
if __name__=='__main__':main()
