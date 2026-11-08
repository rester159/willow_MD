#!/usr/bin/env python3
"""Source-key selector host/native trace proof; no inferred palette assignment."""
from pathlib import Path
import argparse,csv,hashlib,json,re,struct,subprocess
from build_vdp_presentation_probe import ROOT,build
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,default=ROOT/'build/native-object-publication/checkpoint-metadata-v1');p.add_argument('--direct-words',action='store_true');p.add_argument('--stream',action='store_true');p.add_argument('--native-stream',action='store_true');p.add_argument('--output',type=Path,default=ROOT/'build/native-object-catalog/trace463');args=p.parse_args()
source=args.capture.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
checkpoint=json.loads((source/'checkpoint.json').read_text())
for name in ['publication-latches.csv','captured-original-banks.bin']:
 assert sha(source/name)==checkpoint['files'][name]
rows=list(csv.DictReader((source/'publication-latches.csv').open()));raw=(source/'captured-original-banks.bin').read_bytes()
records=[];frames=[];keys=set();previous=None;changes=0
for row in rows:
 if row['manager']!=row['scene'] or row['scene']!='2':continue
 assert row['valid_complete_epoch']=='1'
 count=int(row['count']);capture=int(row['capture']);bank=list(struct.iter_unpack('>4H',raw[(capture-1)*2048:capture*2048]))[:count]
 current=tuple((r[2],r[3]) for r in bank)
 if previous is not None:changes+=current!=previous
 previous=current;keys.update((r[2]<<16)|r[3] for r in bank)
 frames.append((len(records),count,int(row['producer_epoch']),int(row['selected_bank'])));records.extend(bank)
real_offset=len(records);binding=json.loads((ROOT/'build/cartridge-gap-audit/frame1200_window_v1/asset-binding.json').read_text());real=(ROOT/binding['source_capture']).read_bytes()[:24];records.extend(struct.iter_unpack('>4H',real))
original=(ROOT/'build/native-prepared-objects/checkpoint-native-inside-v1/presentation/prepared_fixture.h').read_text()
real_definitions='\n'.join(line for line in original.splitlines() if line.startswith('static const WillowPreparedObjectCell') or line.startswith('static const WillowPreparedObjectTemplate'))
real_ids=sorted((int(code),int(attr),int(n)) for n,code,attr in re.findall(r'preparedTemplate(\d+)=\{[^,]+,\d+,(\d+),(\d+)\}',real_definitions))
lines=[f'#define TRACE_KEY_COUNT {len(keys)}',f'#define TRACE_FRAME_COUNT {len(frames)}',f'#define TRACE_TOPOLOGY_CHANGES {changes}', 'typedef struct {uint16_t offset,count;uint32_t epoch;uint16_t bank;} TraceFrame;', 'static const WillowPreparedObjectKey traceKeys[]={'+','.join('{0x%08xUL}'%k for k in sorted(keys))+'};', 'static const uint16_t traceRecords[][4]={'+','.join('{'+','.join(str(v) for v in r)+'}' for r in records)+'};', 'static const TraceFrame traceFrames[]={'+','.join('{%d,%d,%dUL,%d}'%r for r in frames)+'};',real_definitions, 'static const WillowPreparedObjectKey realTemplateKeys[]={'+','.join('{0x%08xUL}'%((code<<16)|attr) for code,attr,_ in real_ids)+'};','static const WillowPreparedObjectTemplate*const realTemplates[]={'+','.join('&preparedTemplate%d'%n for _,_,n in real_ids)+'};',f'static const TraceFrame realFrame={{{real_offset},3,1,0}};']
lines.append('static const WillowPreparedObjectKey wideKeys[]={'+','.join('{0x%08xUL}'%(((n>>8)<<16)|(n&255)) for n in range(65534))+'};')
(out/'trace_fixture.h').write_text('\n'.join(lines)+'\n')
entry=out/'test.c';entry.write_text(('#define CATALOG_DIRECT_WORDS 1\n' if args.direct_words or args.stream or args.native_stream else '')+('#define CATALOG_STREAM 1\n' if args.stream or args.native_stream else '')+('#define WILLOW_PREPARED_NATIVE_SELECT 1\n' if args.native_stream else '')+'#define CATALOG_TRACE_HEADER '+json.dumps(str(out/'trace_fixture.h'))+'\n#include '+json.dumps(str(ROOT/'tools/test_native_object_catalog.c'))+'\n')
subprocess.run(['clang','-std=c99','-Wall','-Wextra','-Werror','-O2','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(entry),'-o',str(out/'host')],check=True)
r=subprocess.run([str(out/'host')],capture_output=True,text=True);(out/'host.log').write_text(r.stdout+r.stderr);r.check_returncode();host=json.loads(r.stdout)
base=ROOT/'build/native-latch-presentation/inputs';target=out/'target'
build({n:base/(n+'.bin') for n in ['vram','cram','vsram','registers']},target,native_source=entry)
s=(ROOT/'tools/capture_vdp_presentation.c').read_text()
s=s.replace('if(frame!=30)return;','(void)d;(void)w;(void)h;(void)pitch;return;\n if(frame!=30)return;')
s=s.replace('static uint64_t phaseCycles[6];\nstatic unsigned phaseCount;','static uint64_t phaseStart;static unsigned phaseValue,phaseCounts[3];')
start=s.index(' if((address&0xffffff)==0xff0004){');end=s.index('\n if(originalWrite16)',start)
s=s[:start]+''' if((address&0xffffff)==0xff0004){
  uint64_t now=(uint64_t)frame*896040+(int64_t)profileCPU->cycles;
  if(value==1||value==3||value==5){if(phaseValue)exit(3);phaseValue=value;phaseStart=now;}
  else if(value==phaseValue+1&&value<=6){unsigned mode=(value-2)/2;printf("COST %u %u %llu %.9f\\n",mode,phaseCounts[mode]++,(unsigned long long)(now-phaseStart),(double)(now-phaseStart)/53693.175);phaseValue=0;}
  else exit(3);
 }'''+s[end:]
s=s.replace('for(frame=0;frame<=30;frame++)retro_run();','for(frame=0;frame<10000;frame++){retro_run();const uint8_t*r=dlsym(core,"work_ram");uint32_t marker=0;for(unsigned n=0;n<4;n++)marker=marker<<8|r[n^1];if(marker==0x56445031||marker==0x4641494c)break;}captured=1;')
s=s.replace('ready,captured);return 1;','ready,captured);for(unsigned at=8;at<24;at++)fprintf(stderr,"%02x",ram[at^1]);fputc(10,stderr);return 1;')
start=s.index(' if(phaseCount!=6)');end=s.index(' profileCPU->map[255].write16=originalWrite16;',start)
s=s[:start]+f' if(phaseCounts[0]!={len(frames)}||phaseCounts[1]!={len(frames)}||phaseCounts[2]!=1||phaseValue)return 3;\n'+s[end:]
s=s.replace(' retro_unload_game();',' printf("CATALOG_WORDS");for(unsigned at=8;at<26;at+=2)printf(" %u",((unsigned)ram[at^1]<<8)|ram[(at+1)^1]);putchar(10);\n retro_unload_game();')
s=s.replace('Native VDP buffers published; captured320x224 pixels after31 emulated frames;', 'Native source-key catalog proof completed; no video admission;')
(target/'capture.c').write_text(s)
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-DWILLOW_PRESENTATION_PHASES','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'tools'),str(target/'capture.c'),'-o',str(target/'capture')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
r=subprocess.run([str(target/'capture'),str(core),str(target/'probe.bin'),str(target/'unused.ppm')],capture_output=True,text=True);(target/'capture.log').write_text(r.stdout+r.stderr);r.check_returncode()
words=[int(x) for x in next(line for line in r.stdout.splitlines() if line.startswith('CATALOG_WORDS')).split()[1:]]
assert words[:6]==[0,host['source_records_exact'],0,host['captures'],host['topology_changes'],host['adversarial_cases']],words
assert words[6:9]==[8,20,host['wide_selection_cases']],words
costs=[[],[],[]]
for line in r.stdout.splitlines():
 if line.startswith('COST '):
  _,mode,n,cycles,ms=line.split();costs[int(mode)].append({'index':int(n),'cycles':int(cycles),'ms':float(ms)})
means={name:{'min_ms':min(x['ms'] for x in values),'mean_ms':sum(x['ms'] for x in values)/len(values),'max_ms':max(x['ms'] for x in values)} for name,values in zip(['source_selection_cold','source_selection_cached','real1200_indexed_publication'],costs)}
files=[Path(__file__),ROOT/'tools/test_native_object_catalog.c',ROOT/'src/willow_native_object_catalog.h',ROOT/'src/willow_native_object_catalog_m68k.h',ROOT/'src/willow_native_object_prepared.h',source/'checkpoint.json',source/'publication-latches.csv',source/'captured-original-banks.bin',ROOT/'build/native-prepared-objects/checkpoint-native-inside-v1/presentation/prepared_fixture.h',out/'trace_fixture.h',entry,ROOT/'tools/build_vdp_presentation_probe.py',target/'probe.bin',target/'capture.c',target/'capture.log',core]
e={'scope':'Actual source-key selection/origin sequence on the pinned active route; real1200 resolved instance publication only. Full-route palette bindings unresolved; no SAT/pixels/FPS admission.', 'direct_source_words':args.direct_words or args.stream or args.native_stream,'producer_owned_stream':args.stream or args.native_stream,'native_stream_loop':args.native_stream,'host':host,'native':{'captures':words[3],'source_records_exact':words[1],'topology_changes':words[4],'adversarial_cases':words[5],'selection_bytes':words[6],'packet_bytes':words[7],'wide_selection_cases':words[8]},'timing_ms':means,'timing_scope':'Native source-key lookup/position selection and cached-key validation only; raw acquisition, palette resolution, SAT construction, source logic, VDP and audio excluded. Store markers add observation overhead.','hashes':{str(f.resolve()):sha(f) for f in files}}
(out/'costs.json').write_text(json.dumps(costs,indent=2)+'\n');(out/'evidence.json').write_text(json.dumps(e,indent=2)+'\n');print(json.dumps({k:v for k,v in e.items() if k!='hashes'},indent=2))
