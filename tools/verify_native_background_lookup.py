#!/usr/bin/env python3
"""Independent dictionary expectations and actual68000 background lookup cost."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess
from build_vdp_presentation_probe import ROOT,build
from prepare_native_background_lookup import hashed

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def array(name,data):
 lines=[f'static const uint8_t {name}[{len(data)}] __attribute__((aligned(2)))={{']
 for n in range(0,len(data),32):lines.append(' '+','.join(str(x)for x in data[n:n+32])+',')
 return '\n'.join(lines+['};'])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--assets',type=Path,default=ROOT/'build/native-background-lookup/prepared-v1');p.add_argument('--catalog',type=Path,default=ROOT/'build/native-background-catalog/prepared-v1');p.add_argument('--output',type=Path,default=ROOT/'build/native-background-lookup/native-v2');a=p.parse_args()
 assets=a.assets.resolve();catalog=a.catalog.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 manifest=json.loads((assets/'manifest.json').read_text())
 for path,digest in manifest['inputs'].items():assert sha(Path(path))==digest,path
 for name,digest in manifest['outputs'].items():assert sha(assets/name)==digest,name
 blob=(assets/'lookup.bin').read_bytes();raw=(catalog/'catalog.bin').read_bytes();layers=manifest['layers']
 dictionaries=[];known=[]
 for layer,d in enumerate(layers):
  pairs={struct.unpack_from('>I',raw,d['pairs_address']-0x120000+i*6)[0]:i for i in range(d['pair_count'])};dictionaries.append(pairs)
  known.extend((layer,key)for key in pairs)
 query=list(known)
 for layer,key in known:
  query.extend((layer,key^(1<<bit))for bit in range(32))
  query.extend((layer,(key+delta)&0xffffffff)for delta in [-65536,-1,1,65536])
 random=0x91809140
 for n in range(65536):
  random^=(random<<13)&0xffffffff;random^=random>>17;random^=(random<<5)&0xffffffff
  query.append((n&1,random))
 h=['#include <stdint.h>',array('lookupBlob',blob),array('catalogBlob',raw),'static const WillowNativeBackgroundLookupIndex fixtureIndexes[2]={']
 for d in layers:h.append(' {lookupBlob+%d,catalogBlob+%d,%d,%d,%d,%d,%d,%d,%d,0},'%(d['slot_offset'],d['pairs_address']-0x120000,d['pair_count'],d['mask'],d['max_probes'],d['seed'],d['attr_rotate'],d['mix_left'],d['mix_right']))
 h+=['};','typedef struct {uint32_t key;uint16_t expected,layer,bucket;} LookupQuery;',f'#define LOOKUP_KNOWN_COUNT {len(known)}U',f'#define LOOKUP_QUERY_COUNT {len(query)}U','static const LookupQuery lookupQueries[LOOKUP_QUERY_COUNT]={']
 for layer,key in query:
  d=layers[layer];bucket=hashed(key,d['attr_rotate'],d['mix_left'],d['mix_right'],d['seed'],d['mask'])
  h.append(' {0x%08xUL,%d,%d,%d},'%(key,dictionaries[layer].get(key,65535),layer,bucket))
 h+=['};',''];(out/'background_lookup_fixture.h').write_text('\n'.join(h))
 source=ROOT/'tools/test_native_background_lookup.c';header=ROOT/'src/willow_native_background_lookup.h'
 core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
 files=[Path(__file__).resolve(),source,header,ROOT/'tools/prepare_native_background_lookup.py',ROOT/'tools/build_vdp_presentation_probe.py',ROOT/'tools/capture_vdp_presentation.c',assets/'manifest.json',assets/'lookup.bin',assets/'lookup_imports.h',catalog/'catalog.bin',core]
 before={str(q):sha(q)for q in files}
 subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'src'),'-I'+str(out),str(source),'-o',str(out/'host')],check=True)
 host=subprocess.check_output([str(out/'host')],text=True);(out/'host.json').write_text(host)
 fixture=ROOT/'build/cartridge-gap-audit/frame1200_window_v1';build({k:fixture/(k+'.bin')for k in ['vram','cram','vsram','registers']},out,native_source=source)
 capture=(ROOT/'tools/capture_vdp_presentation.c').read_text()
 def replace(old,new):
  nonlocal capture
  assert capture.count(old)==1,old[:100];capture=capture.replace(old,new)
 replace('static bool env(unsigned cmd,void*data){',r'''
 typedef struct {struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];struct {unsigned pc,cycle,detected;} poll;int cycles,refresh;unsigned end,dar[16],pc;} LookupCPU;
 static LookupCPU*cpu;static uint8_t*observed;static void(*ramWrite)(unsigned,unsigned);
 static unsigned measuring,kind,measured;static uint64_t began,timings[6];
 static unsigned rw(unsigned a){return observed[a^1]*256+observed[(a+1)^1];}
 static uint64_t stamp(void){return (uint64_t)frame*896040+(int64_t)cpu->cycles;}
 static void observeRAM(unsigned at,unsigned v){
  if((at&0xffffff)==0xff0004){
   if(v==1){if(measuring)exit(1);kind=rw(0x20);if(kind>=6)exit(1);began=stamp();measuring=1;}
   else if(v==2){if(!measuring)exit(1);timings[kind]=stamp()-began;measuring=0;measured++;}
   else exit(1);
  }
  if(ramWrite)ramWrite(at,v);else{at&=65535;observed[at^1]=v>>8;observed[(at+1)^1]=v;}
 }
 static bool env(unsigned cmd,void*data){''')
 replace('if(frame!=30)return;','(void)d;(void)w;(void)h;(void)pitch;return;\n if(frame!=30)return;')
 replace('for(frame=0;frame<=30;frame++)retro_run();',r'''
 cpu=dlsym(core,"m68k");observed=dlsym(core,"work_ram");FN(unsigned,m68k_get_reg,(int));
 uint8_t*pal=dlsym(core,"vdp_pal");uint16_t*lines=dlsym(core,"lines_per_frame");
 if(!cpu||!observed||!pal||!lines||*pal||*lines!=262||cpu->pc!=m68k_get_reg(16))return 2;
 for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
 ramWrite=cpu->map[255].write16;cpu->map[255].write16=observeRAM;
 for(frame=0;frame<80000;frame++){
  retro_run();unsigned mark=rw(0)*65536u+rw(2);if(mark==0x56445031||mark==0x4641494c)break;
 }captured=1;cpu->map[255].write16=ramWrite;
 ''')
 replace('if(ready!=0x56445031||captured!=1)',r'''if(ready!=0x56445031){fprintf(stderr,"Lookup native failure line%u actual%x expected%x PC%x\n",rw(0x10),rw(0x12)*65536u+rw(0x14),rw(0x16)*65536u+rw(0x18),cpu->pc);}
 if(ready!=0x56445031||captured!=1)''')
 replace('printf("Native VDP buffers published; captured320x224 pixels after31 emulated frames; format%d; no host target writes or GUI.\\n",format);',r'''
 if(measured!=6)return 2;
 printf("{\"cases\":%u,\"checks\":%u,\"digest\":%u,\"sink\":%u,\"emulated_frames\":%u,\"timings_master\":[%llu,%llu,%llu,%llu,%llu,%llu]}\n",rw(8)*65536u+rw(10),rw(12)*65536u+rw(14),rw(0x24)*65536u+rw(0x26),rw(0x28),frame+1,(unsigned long long)timings[0],(unsigned long long)timings[1],(unsigned long long)timings[2],(unsigned long long)timings[3],(unsigned long long)timings[4],(unsigned long long)timings[5]);''')
 (out/'capture.c').write_text(capture)
 subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tools'),str(out/'capture.c'),'-o',str(out/'capture')],check=True)
 with (out/'native.json').open('w')as f:subprocess.run([str(out/'capture'),str(core),str(out/'probe.bin'),str(out/'unused.ppm')],stdout=f,stderr=subprocess.STDOUT,check=True)
 h=json.loads(host);n=json.loads((out/'native.json').read_text())
 for key in h:assert h[key]==n[key],key
 assert all(sha(Path(q))==v for q,v in before.items()),'Input changed during proof'
 timings=[]
 for kind,name in enumerate(['binary_known','hash_known','binary_varied','hash_varied','admitted_known','admitted_varied']):
  total=n['timings_master'][kind];count=len(known)if kind<2 or kind==4 else len(query)-len(known)
  timings.append({'kind':name,'queries':count,'total_ms':total/53693.175,'mean_us_per_query':total/53.693175/count})
 result={'scope':__doc__,'host':h,'native':n,'independent_queries':len(query),'known_keys':len(known),'timings':timings,'native_index_bytes':20,'runtime_persistent_bytes':0,'timing_scope':'Actual68000 equal batches with noinline originalbinary/hash wrappers; defensive hash includes metadata guards; admitted hash requires immutable coldvalidated binding. Allinclude equal input-loop/sum bookkeeping andexclude one-time fullvalidation. Not integrated renderer deadline or FPS evidence.','checks':['Python exactdictionary expectations for802 fullkeys, all32bit toggles/key, four adjacent variations/key,65,536 seeded32-bit keys','Python16bit hash matches actual68000 wordrotate/shift/XOR for everyquery','72header/directory corruptions rejected','Every802occupiedslot removed and made out-of-range: lookup and full coldvalidation bothrefuse','DuplicateID retaining occupiedcount rejected','Malformed descriptors and null/truncated bindings refuse; mutablefixture canaries unchanged'],'input_hashes':before,'output_hashes':{q.name:sha(q)for q in out.iterdir()if q.is_file()and q.name!='evidence.json'}}
 (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'host':h,'native':n,'timings':timings},indent=2))
if __name__=='__main__':main()
