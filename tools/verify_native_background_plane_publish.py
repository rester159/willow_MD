#!/usr/bin/env python3
"""Bounded rows and retained column strips: host and actual68000 all-VRAM admission."""
from pathlib import Path
import argparse,hashlib,json,subprocess
from build_vdp_presentation_probe import ROOT,build
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/native-background-plane-publish/current');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
 files=[ROOT/'tools'/n for n in ['test_native_background_plane_publish.c','test_native_background_plane_publish_m68k.c','test_native_background_plane_publish_cases.h']]
 files+=[ROOT/'src/willow_native_background_plane_publish.h',ROOT/'src/willow_native_plane_surface.h',ROOT/'src/willow_native_presentation_plan.h',Path(__file__),ROOT/'tools/build_vdp_presentation_probe.py',ROOT/'tools/capture_vdp_presentation.c',core]
 before={str(q):sha(q)for q in files}
 subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'src'),str(files[0]),'-o',str(out/'host')],check=True)
 host=subprocess.check_output([str(out/'host'),str(out/'traffic.bin'),str(out/'vram-snapshots.bin')],text=True);(out/'host.json').write_text(host)
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
static PlaneCPU*planeCPU;static uint8_t*planeRAM,*planeVRAM,*planeRegs;
static uint8_t savedRegs[32],expectedVRAM[65536];static FILE*expectedTraffic,*expectedSnapshots;
static void(*ramWrite)(unsigned,unsigned),(*vdpWrite)(unsigned,unsigned);
static unsigned measured,active,kind,expectedWords,expectedControls,dataWords,controlWords;
static uint64_t begin;
static struct {unsigned calls,words,controls;uint64_t total,min,max;} timings[9];
static unsigned rw(unsigned a){return planeRAM[a^1]*256+planeRAM[(a+1)^1];}
static uint64_t stamp(void){return (uint64_t)frame*896040+(int64_t)planeCPU->cycles;}
static void observeVDP(unsigned at,unsigned v){
 if(active){
  unsigned type;if((at&31)<4){dataWords++;type=0;}else if((at&31)<8){controlWords++;type=4;}else exit(1);
  uint8_t b[4];if(fread(b,1,4,expectedTraffic)!=4||type!=(unsigned)(b[0]*256+b[1])||v!=(unsigned)(b[2]*256+b[3])){fprintf(stderr,"Native traffic mismatch call%u port%x value%x\n",measured,at,v);exit(1);}
 }
 vdpWrite(at,v);
}
static void observeRAM(unsigned at,unsigned v){
 if((at&0xffffff)==0xff0004){
  if(v==1){if(active)exit(1);kind=rw(0x20);expectedWords=rw(0x22);expectedControls=rw(0x24);if(kind>=9||planeRegs[15]!=2)exit(1);memcpy(savedRegs,planeRegs,32);active=1;begin=stamp();dataWords=controlWords=0;}
  else if(v==2){
   if(!active)exit(1);uint64_t elapsed=stamp()-begin;active=0;
   if(dataWords!=expectedWords||controlWords!=expectedControls){fprintf(stderr,"Actual VDP traffic call%u kind%u words%u/%u controls%u/%u\n",measured,kind,dataWords,expectedWords,controlWords,expectedControls);exit(1);}
   if(memcmp(savedRegs,planeRegs,32)||planeRegs[15]!=2){fprintf(stderr,"VDP registers changed call%u\n",measured);exit(1);}
   if(fread(expectedVRAM,1,65536,expectedSnapshots)!=65536)exit(1);
   for(unsigned n=0;n<65536;n++)if(planeVRAM[n^1]!=expectedVRAM[n]){fprintf(stderr,"Native fullVRAM mismatch call%u byte%x actual%x expected%x\n",measured,n,planeVRAM[n^1],expectedVRAM[n]);exit(1);}
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
 planeCPU=dlsym(core,"m68k");planeRAM=dlsym(core,"work_ram");planeVRAM=dlsym(core,"vram");planeRegs=dlsym(core,"reg");
 expectedTraffic=fopen("EXPECTED_TRAFFIC","rb");expectedSnapshots=fopen("EXPECTED_SNAPSHOTS","rb");
 if(!planeVRAM||!planeRegs||!expectedTraffic||!expectedSnapshots)return 2;
 FN(unsigned,m68k_get_reg,(int));
 uint8_t*pal=dlsym(core,"vdp_pal");uint16_t*lines=dlsym(core,"lines_per_frame");
 if(!planeCPU||!planeRAM||!pal||!lines||*pal||*lines!=262||planeCPU->pc!=m68k_get_reg(16))return 2;
 for(unsigned n=0;n<16;n++)if(planeCPU->dar[n]!=m68k_get_reg(n))return 2;
 ramWrite=planeCPU->map[255].write16;vdpWrite=planeCPU->map[192].write16;if(!vdpWrite)return 2;
 planeCPU->map[255].write16=observeRAM;planeCPU->map[192].write16=observeVDP;
 for(frame=0;frame<40000;frame++){
  retro_run();unsigned mark=rw(0)*65536u+rw(2);if(mark==0x56445031||mark==0x4641494c)break;
 }captured=1;
 if(fgetc(expectedTraffic)!=EOF||fgetc(expectedSnapshots)!=EOF)return 2;fclose(expectedTraffic);fclose(expectedSnapshots);
 planeCPU->map[255].write16=ramWrite;planeCPU->map[192].write16=vdpWrite;
''')
 replace('if(ready!=0x56445031||captured!=1)',r'''if(ready!=0x56445031){fprintf(stderr,"Native plane failure line%u actual%x expected%x, PC%x\n",rw(0x10),rw(0x12)*65536u+rw(0x14),rw(0x16)*65536u+rw(0x18),planeCPU->pc);}
 if(ready!=0x56445031||captured!=1)''')
 replace('printf("Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.\\n",format);',r'''
 printf("{\"publications\":%u,\"refusals\":%u,\"invalid_edits\":%u,\"observed_calls\":%u,\"emulated_frames\":%u,\"timings\":[",rw(8)*65536u+rw(10),rw(12),rw(14),measured,frame+1);
 for(unsigned n=0;n<9;n++)printf("%s{\"kind\":%u,\"calls\":%u,\"data_words\":%u,\"control_words\":%u,\"min_ms\":%.9f,\"mean_ms\":%.9f,\"max_ms\":%.9f}",n?",":"",n,timings[n].calls,timings[n].words,timings[n].controls,timings[n].min/53693.175,timings[n].calls?(double)timings[n].total/timings[n].calls/53693.175:0,timings[n].max/53693.175);
 printf("]}\n");''')
 capture=capture.replace('EXPECTED_TRAFFIC',str(out/'traffic.bin')).replace('EXPECTED_SNAPSHOTS',str(out/'vram-snapshots.bin'))
 generated=out/'capture.c';generated.write_text(capture)
 subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tools'),str(generated),'-o',str(out/'capture')],check=True)
 with (out/'native.json').open('w')as f:subprocess.run([str(out/'capture'),str(core),str(out/'probe.bin'),str(out/'unused.ppm')],stdout=f,stderr=subprocess.STDOUT,check=True)
 h=json.loads(host);n=json.loads((out/'native.json').read_text())
 for k in ['publications','refusals','invalid_edits']:assert h[k]==n[k],k
 assert n['observed_calls']==n['publications']+n['refusals']
 assert all(sha(Path(p))==s for p,s in before.items()),'Input changed during proof'
 result={'scope':__doc__,'host':h,'native':n,'surface_bytes_m68k':4182,'words_offset_m68k':20,'additional_persistent_bytes':0,'caller_column_mask_bytes_per_plane':8,
  'checks':['All8 legal bases, bounded2-row cold progress, queue drain and retarget during partial upload','Edits to already published cold rows are requeued; no additional persistent RAM','Each64 column and32 row, wrapped63/0 strips, partial limits and sparse masks','Rows/columns/word/complete statistics, optional stats and unchanged zeroVDPwrites','All65536 VRAMbytes compared after every publication/refusal, including every other plane and all pattern/SAT storage','Actual target control/data event sequence exactly matches sanitized host and restores all32 VDP registerbytes including reg15=2','Rows: late invalid spans refuse before anyVDPstore; bothmodes reject null/count/epoch/frame/identity/target errors without metadata mutation','Authoritative bounded column queue ignores irrelevant row envelopes; empty omitted queue and cold-mode refuse; set bounds/canaries'],
  'timing_scope':'Serialized display-disabled actual68000 publication only, including equal marker/stat bookkeeping; excludes edits/resolution/readback/certification and is not active-display VBlank or live game FPS evidence.',
  'source_hashes':before,'output_hashes':{p.name:sha(p)for p in out.iterdir()if p.is_file()and p.name!='evidence.json'}}
 (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'host':h,'native':n},indent=2))
if __name__=='__main__':main()
