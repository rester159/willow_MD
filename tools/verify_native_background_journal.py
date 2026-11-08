#!/usr/bin/env python3
"""Sanitized host/actual68000 changed-source-pair journal proof; no live hooks."""
from pathlib import Path
import argparse,collections,csv,hashlib,json,struct,subprocess
from build_vdp_presentation_probe import ROOT,build

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def route_fixture(out,trace):
    source=ROOT/'build/reference_maincpu.bin'
    assert sha(source)=='0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1'
    rom=source.read_bytes()
    with (trace/'background-events.csv').open() as f:
        writes=[r for r in csv.DictReader(f) if r['kind']=='3' and r['manager']==r['scene']=='2' and 0x914000<=int(r['address'],16)<0x91c000]
    assert len(writes)==768
    pairs=[]
    for n in range(0,len(writes),3):
        raw,adjust,attr=writes[n:n+3]
        at=int(raw['address'],16);layer=2 if at>=0x918000 else 3;base=0x918000 if layer==2 else 0x914000
        assert at%4==0
        assert [int(r['callback_pc'],16) for r in [raw,adjust,attr]]==([0x437ac,0x437b0,0x437b2] if layer==2 else [0x43df8,0x43dfc,0x43dfe])
        assert int(adjust['address'],16)==at and int(attr['address'],16)==at+2
        assert raw['frame']==adjust['frame']==attr['frame'] and raw['irq']==adjust['irq']==attr['irq']
        rom_at=int(raw['a1'],16)-2
        raw_code,final_attr=struct.unpack_from('>HH',rom,rom_at)
        assert int(raw['value'],16)==raw_code and int(attr['value'],16)==final_attr
        final_code=(raw_code+(0xa00 if layer==3 else 0))&65535
        assert int(adjust['value'],16)==final_code and int(adjust['previous'],16)==raw_code
        pairs.append(dict(layer=layer,cell=(at-base)//4,before=(int(raw['previous'],16),int(attr['previous'],16)),final=(final_code,final_attr),frame=int(raw['frame']),end=int(attr['master'])))
    batches=collections.Counter((p['frame'],p['layer']) for p in pairs)
    assert len(batches)==8 and list(batches.values()).count(34)==7 and list(batches.values()).count(18)==1
    with (trace/'background-irqs.csv').open() as f:active=[r for r in csv.DictReader(f) if int(r['active_index'])]
    maps=(trace/'background-active-maps.bin').read_bytes();assert len(maps)==len(active)*32768
    original=bytearray(maps[:32768]);retained=bytearray(original);index=0;matched=0
    # Independent whole-map comparison: source writes every completed pair;
    # retained native model applies only changed pairs after each epoch. Neither
    # gives the target API an assumed ready baseline in actual game execution.
    groups=[]
    for frame in sorted({p['frame'] for p in pairs}):
        group=[p for p in pairs if p['frame']==frame]
        changed=[p for p in group if p['before']!=p['final']]
        assert len(changed)==14 and len({(p['layer'],p['cell'])for p in group})==len(group)
        groups.append((group,changed))
    for n,snapshot in enumerate(active):
        stamp=int(snapshot['master'])
        while index<len(groups) and groups[index][0][-1]['end']<stamp:
            group,changed=groups[index]
            for p in group:
                at=(p['layer']-2)*16384+p['cell']*4
                assert struct.unpack_from('>HH',original,at)==p['before']
                struct.pack_into('>HH',original,at,*p['final'])
            for p in changed:
                at=(p['layer']-2)*16384+p['cell']*4
                struct.pack_into('>HH',retained,at,*p['final'])
            index+=1
        expected=maps[n*32768:(n+1)*32768]
        assert original==retained==expected,(n,snapshot['frame'])
        matched+=1
    assert index==7 and sum(len(changed)for _,changed in groups)==98
    text='/* Generated test-only original source pairs; never a runtime path. */\n'
    starts=[0];expected_starts=[0];rows=[];expected=[]
    for group,changed in groups:
        rows.extend([[p['layer'],p['cell'],*p['before'],*p['final']]for p in group]);starts.append(len(rows))
        expected.extend([[((p['layer']-2)<<12)|p['cell'],*p['final']]for p in changed]);expected_starts.append(len(expected))
    for name,values in [('routeStarts',starts),('routeExpectedStarts',expected_starts)]:
        text+=f'static const uint16_t {name}[]={{{",".join(map(str,values))}}};\n'
    for name,values,width in [('routePairs',rows,6),('routeExpected',expected,3)]:
        text+=f'static const uint16_t {name}[][{width}]={{\n'+''.join('{'+','.join(f'0x{v:04x}'for v in row)+'},\n'for row in values)+'};\n'
    (out/'native_background_journal_route.h').write_text(text)
    return dict(source_pairs=len(pairs),source_word_writes=len(writes),controller_batches=len(batches),combined_controller_epochs=len(groups),changed_cells=len(expected),whole_map_snapshots=matched,exact_map_bytes=matched*32768,
                limitations='Seven Scroll2 columns and one Scroll3 column, no active rows/later stages. Test data only; no source hooks, live baseline certification, interrupt timing, or FPS admission.')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'build/native-background-journal/current')
    p.add_argument('--trace',type=Path,default=ROOT/'build/native-background-lifetime/v1')
    a=p.parse_args();out=a.output.resolve();trace=a.trace.resolve();out.mkdir(parents=True,exist_ok=True)
    core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
    tests=[ROOT/'tools'/n for n in ['test_native_background_journal.c','test_native_background_journal_m68k.c','test_native_background_journal_cases.h']]
    files=tests+[ROOT/'src/willow_native_background_journal.h',Path(__file__),ROOT/'tools/build_vdp_presentation_probe.py',ROOT/'tools/capture_vdp_presentation.c',ROOT/'build/reference_maincpu.bin',core]
    files += [trace/n for n in ['background-events.csv','background-irqs.csv','background-active-maps.bin']]
    fixture=ROOT/'build/cartridge-gap-audit/frame1200_window_v1'
    files += [fixture/(k+'.bin') for k in ['vram','cram','vsram','registers']]
    before={str(q):sha(q)for q in files}
    route=route_fixture(out,trace)
    subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'src'),'-I'+str(out),str(tests[0]),'-o',str(out/'host')],check=True)
    host=subprocess.check_output([str(out/'host')],text=True);(out/'host.json').write_text(host)
    build({k:fixture/(k+'.bin')for k in ['vram','cram','vsram','registers']},out,native_source=tests[1],native_include=out)
    capture=(ROOT/'tools/capture_vdp_presentation.c').read_text()
    def replace(old,new):
        nonlocal capture
        assert capture.count(old)==1,old[:60];capture=capture.replace(old,new)
    replace('static bool env(unsigned cmd,void*data){',r'''
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;int cycles,refresh;unsigned end,dar[16],pc;
} JournalCPU;
static JournalCPU*journalCPU;static uint8_t*journalRAM;
static void(*ramWrite)(unsigned,unsigned),(*vdpWrite)(unsigned,unsigned);
static unsigned measured,active,kind;static uint64_t begin;
static struct {unsigned calls;uint64_t total,min,max;} timings[12];
static unsigned rw(unsigned a){return journalRAM[a^1]*256+journalRAM[(a+1)^1];}
static uint64_t stamp(void){return (uint64_t)frame*896040+(int64_t)journalCPU->cycles;}
static void observeVDP(unsigned at,unsigned v){if(active){fprintf(stderr,"Journal unexpectedly writes VDP\n");exit(1);}vdpWrite(at,v);}
static void observeRAM(unsigned at,unsigned v){
 if((at&0xffffff)==0xff0004){
  if(v==1){if(active)exit(1);kind=rw(0x20);if(kind>=12)exit(1);active=1;begin=stamp();}
  else if(v==2){
   if(!active)exit(1);uint64_t elapsed=stamp()-begin;active=0;
   timings[kind].calls++;timings[kind].total+=elapsed;
   if(timings[kind].calls==1||elapsed<timings[kind].min)timings[kind].min=elapsed;
   if(elapsed>timings[kind].max)timings[kind].max=elapsed;measured++;
  }else exit(1);
 }
 if(ramWrite)ramWrite(at,v);else{at&=65535;journalRAM[at^1]=v>>8;journalRAM[(at+1)^1]=v;}
}
static bool env(unsigned cmd,void*data){''')
    replace('if(frame!=30)return;','(void)d;(void)w;(void)h;(void)pitch;return;\n if(frame!=30)return;')
    replace('for(frame=0;frame<=30;frame++)retro_run();',r'''
 journalCPU=dlsym(core,"m68k");journalRAM=dlsym(core,"work_ram");
 FN(unsigned,m68k_get_reg,(int));
 uint8_t*pal=dlsym(core,"vdp_pal");uint16_t*lines=dlsym(core,"lines_per_frame");
 if(!journalCPU||!journalRAM||!pal||!lines||*pal||*lines!=262||journalCPU->pc!=m68k_get_reg(16))return 2;
 for(unsigned n=0;n<16;n++)if(journalCPU->dar[n]!=m68k_get_reg(n))return 2;
 ramWrite=journalCPU->map[255].write16;vdpWrite=journalCPU->map[192].write16;if(!vdpWrite)return 2;
 journalCPU->map[255].write16=observeRAM;journalCPU->map[192].write16=observeVDP;
 for(frame=0;frame<40000;frame++){
  retro_run();unsigned mark=rw(0)*65536u+rw(2);if(mark==0x56445031||mark==0x4641494c)break;
 }captured=1;
 journalCPU->map[255].write16=ramWrite;journalCPU->map[192].write16=vdpWrite;
''')
    replace('if(ready!=0x56445031||captured!=1)',r'''if(ready!=0x56445031){fprintf(stderr,"Native journal failure line%u actual%x expected%x, PC%x\n",rw(0x10),rw(0x12)*65536u+rw(0x14),rw(0x16)*65536u+rw(0x18),journalCPU->pc);}
 if(ready!=0x56445031||captured!=1||active)''')
    replace('printf("Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.\\n",format);',r'''
 printf("{\"cases\":%u,\"checks\":%u,\"digest\":%u,\"observed_calls\":%u,\"emulated_frames\":%u,\"timings\":[",rw(8)*65536u+rw(10),rw(12)*65536u+rw(14),rw(0x24)*65536u+rw(0x26),measured,frame+1);
 for(unsigned n=0;n<12;n++)printf("%s{\"kind\":%u,\"calls\":%u,\"total_master_cycles\":%llu,\"min_master_cycles\":%llu,\"max_master_cycles\":%llu,\"min_ms\":%.9f,\"mean_ms\":%.9f,\"max_ms\":%.9f}",n?",":"",n,timings[n].calls,(unsigned long long)timings[n].total,(unsigned long long)timings[n].min,(unsigned long long)timings[n].max,timings[n].min/53693.175,timings[n].calls?(double)timings[n].total/timings[n].calls/53693.175:0,timings[n].max/53693.175);
 printf("]}\n");''')
    generated=out/'capture.c';generated.write_text(capture)
    subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tools'),str(generated),'-o',str(out/'capture')],check=True)
    with (out/'native.json').open('w')as f:subprocess.run([str(out/'capture'),str(core),str(out/'probe.bin'),str(out/'unused.ppm')],stdout=f,stderr=subprocess.STDOUT,check=True)
    h=json.loads(host);n=json.loads((out/'native.json').read_text())
    for k in ['cases','checks','digest']:assert h[k]==n[k],k
    assert n['observed_calls']==711 and all(x['calls']==(7 if x['kind']==11 else 64)for x in n['timings'])
    assert all(sha(Path(path))==s for path,s in before.items()),'Input changed during proof'
    with (out/'native.disassembly').open('w') as f:subprocess.run(['m68k-elf-objdump','-d',str(out/'native.o')],stdout=f,check=True)
    result=dict(scope=__doc__,host=h,native=n,route=route,
      m68k_abi=dict(journal_bytes=398,entry_bytes=6,view_bytes=14,offsets=dict(epoch=0,openEpoch=4,count=8,state=10,fault=12,entries=14)),
      timing_names=['inline_pair_unchanged','inline_pair_changed','begin_commit_empty','record_first','record_fourteenth','record_sixtyfourth','coalesce_first_of64','coalesce_last_of64','acquire_ack64','begin_record14_commit','begin_record52_commit','actual_route_epoch_changed_only'],
      checks=['Both4096cell source maps and all32 individual code/attribute bits','Exact insertion order and coalescing; predecessor chain mismatch including no-op rejected','Capacity64, overflow65, unknown writers, incomplete/nested epochs invalidate before readiness','Pending records survive unacknowledged epochs; stale ack changes no bytes','Cold requires explicit baseline; zero/equal/backward/wrapped epochs refused','Every invalid bounds/state/count/null case guards; struct canaries','Original768word trace writes ->256completed pairs ->98changes; all464complete maps exact via independent replay','Actual68000 executes same API checks and test-only original batch fixtures; read-only host observer and zero timedVDP stores'],
      timing_scope='Actual Genesis68000, isolated native C API/inline helper marker-to-marker at NTSC clock. Includes native call/loop/marker overhead; excludes source hooks, source writes, resolution, renderer, baseline construction, and IRQ interference. Not live FPS or CPS-clock equivalence.',
      source_hashes=before,output_hashes={q.name:sha(q)for q in out.iterdir()if q.is_file()and q.name!='evidence.json'})
    (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(host=h,route=route,timings=n['timings']),indent=2))
if __name__=='__main__':main()
