#!/usr/bin/env python3
"""Classify frozen columns-v1 physical publication misses, never scheduler sleeps."""
from pathlib import Path
from collections import Counter,defaultdict
import argparse,csv,hashlib,json,shutil,statistics,subprocess
ROOT=Path(__file__).resolve().parents[1];HZ=53693175;FRAME=896040
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib');a=p.parse_args();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
 run=ROOT/'build/native-forest-extension/columns-v1-provenance-v2';target=ROOT/'build/native-forest-extension/target-columns-v1-frozen';cadence=ROOT/'build/native-forest-extension/columns-v1-cadence';m=json.loads((target/'manifest.json').read_text());pins={}
 def pin(q):pins[str(q.resolve())]=sha(q)
 for q in [target/'manifest.json',target/'render.bin',target/'render.disasm',target/'background-resources.bin',cadence/'intervals.json',cadence/'report.json',run/'observer-manifest.json',run/'rejection-002-source.bin',run/'background-source-contexts.bin',Path(__file__).resolve(),a.core]:pin(q)
 names=['background-events.csv','background-publications.csv','irqs.csv','main-loop-events.csv','headroom-events.csv','headroom-windows.csv','render-events.csv','frames.csv']
 for n in names:pin(run/n)
 bg=[r for r in rows(run/'background-events.csv') if r['path']=='0'];windows=json.loads((cadence/'intervals.json').read_text());low=windows[0]['begin_master'];high=windows[-1]['end_master'];assert len(windows)==361
 assert m['symbols']['bg']==0xff404c and m['symbols']['nativeBackgroundPublish']==0x1058da
 assert (target/'render.bin').read_bytes()[0x10597a-0x102000:0x105982-0x102000].hex()=='33fc00030020f590'
 (out/'guard-fixture.h').write_text('#define PHASE3_PC 0x10597au\n')
 funcnames=['bgSlot','bgSet','bgFarStripStart','bgNearOpacityRef','bgNearVisibleRowFull','bgResolveRef','bgFarRowBlank','bgResolveRow','bgFarBlank','bgReadPair','bgMoveRing','nativeBackgroundPrepare','nativeBackgroundPublish']
 (out/'profile-fixture.h').write_text('static const struct {unsigned pc;const char*name;} entries[]={'+','.join('{%su,"%s"}'%(hex(m['symbols'][n]),n) for n in funcnames)+'};\n#define BLANK_PC '+hex(m['symbols']['bgFarBlank'])+'u\n')
 with (run/'background-source-contexts.bin').open('rb') as f:f.seek(433*123392);(out/'profile-source.bin').write_bytes(f.read(123392))
 commands=[]
 for name,driver,args in [('guards','test_native_bg_guard_costs.c',[str(run/'rejection-002-source.bin'),str(out/'closed-guard.bin')]),('profile','profile_native_bg_blank.c',[str(out/'profile-source.bin'),str(out/'closed-profile.bin'),str(target/'background-resources.bin')])]:
  q=ROOT/'tools'/driver;pin(q);shutil.copy2(q,out/driver);cmd=['cc','-std=c11','-O2','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tools'),'-I'+str(out),str(q),'-o',str(out/name)];subprocess.run(cmd,check=True);commands.append(cmd)
  with (out/(name+'.csv')).open('w') as f:subprocess.run([str(out/name),str(a.core),str(target/'render.bin'),*args],stdout=f,check=True)
 fingerprint={r['predicate']:int(r['phase3_to_failure_master']) for r in rows(out/'guards.csv')};assert fingerprint['raw_busy']==fingerprint['controller_active']==1694 and fingerprint['source_epoch_mismatch']==4032 and fingerprint['view_mismatch']==4438
 source=run/'frozen-target-sources/tools/native_render_background.inc';pin(source);shutil.copy2(source,out/'native_render_background.inc');pin(ROOT/'tools/test_native_render_cache_closed.c')
 opened=None;spans=[];previous=None
 for e in bg:
  if e['phase']=='1':assert opened is None;opened=e
  elif e['phase'] in ['2','6'] or(e['phase']=='5' and int(e['status'])<20):
   assert opened;x=opened;opened=None;v=[int(x[k]) for k in ['s2x','s2y','s3x','s3y']];changed=[v[i]!=previous[i] for i in range(4)] if previous else None
   col=bool(previous and any((((96+v[i])&65535)>>3)!=(((96+previous[i])&65535)>>3) for i in [0,2]))
   kind='unsupported' if e['phase']=='5' else 'unchanged_view' if previous==v else 'column' if col else 'fine_x_only' if previous and not changed[1] and not changed[3] else 'other'
   spans.append({'start':int(x['master_cycles']),'end':int(e['master_cycles']),'generation':int(x['generation']),'view':v,'kind':kind,'start_frame':int(x['frame']),'end_frame':int(e['frame']),'success':e['phase']=='2'});previous=v
 assert opened is None or int(opened['master_cycles'])>=high
 irqs={int(r['irq']):r for r in rows(run/'irqs.csv')};bygen={s['generation']:s for s in spans};misses=[];classified={}
 for w in windows:
  if w['background_publications']:classified[w['irq']]='published';continue
  events=[e for e in bg if w['begin_master']<=int(e['master_cycles'])<w['end_master']];busy=[s for s in spans if s['start']<=w['begin_master']<s['end']];pub=next((e for e in events if e['phase']=='3'),None);reject=next((e for e in events if e['phase']=='5' and int(e['status'])>=20),None)
  d={'irq':w['irq'],'frame':w['begin_frame'],'interrupted_pc':irqs[w['irq']]['interrupted_pc'],'source_body_completions':w['source_body_completions']}
  if busy:
   assert len(busy)==1 and not pub and not reject;kind='busy_during_preparation';s=busy[0];d.update(preparation_kind=s['kind'],generation=s['generation'],preparation_ms=(s['end']-s['start'])*1000/HZ)
  else:
   assert pub and reject;cost=int(reject['master_cycles'])-int(pub['master_cycles']);status=int(reject['status']);d.update(status=status,guard_master=cost,generation=int(reject['generation']))
   if status==21:assert cost==fingerprint['view_mismatch'];kind='view_mismatch';assert classified[w['irq']-1]=='busy_during_preparation';d['preparation_kind']=bygen[d['generation']]['kind']
   else:
    assert status==20
    if cost==fingerprint['raw_busy']:kind='source_controller_busy_or_active'
    else:assert cost==fingerprint['source_epoch_mismatch'];kind='source_epoch_mismatch'
  d['reason']=kind;misses.append(d);classified[w['irq']]=kind
 counts=Counter(d['reason'] for d in misses);assert counts=={'busy_during_preparation':45,'view_mismatch':18,'source_controller_busy_or_active':13,'source_epoch_mismatch':23}
 bodies=[];opened=None
 for e in rows(run/'main-loop-events.csv'):
  if e['event']=='body_begin':opened=e
  elif e['event']=='body_complete':assert opened;bodies.append((int(opened['master_cycles']),int(e['master_cycles']),e));opened=None
 groups=defaultdict(list)
 for s in spans:
  if not(low<=s['start']<high):continue
  s['duration_ms']=(s['end']-s['start'])*1000/HZ;s['busy_irqs']=sum(s['start']<w['begin_master']<s['end'] for w in windows)
  body=next((r for st,en,r in bodies if st<=s['start']<en),None)
  if body:s['source_map_writes']=[int(body[k]) for k in ['s2_writes','s2_changed','s3_writes','s3_changed']]
  groups[s['kind']].append(s)
 summary={}
 for kind,group in groups.items():
  ts=[s['duration_ms'] for s in group];summary[kind]={'count':len(group),'wall_mean_ms':statistics.mean(ts),'wall_max_ms':max(ts),'wall_total_ms':sum(ts),'busy_irqs':sum(s['busy_irqs'] for s in group),'source_no_map_write_bodies':sum(s.get('source_map_writes')==[0,0,0,0] for s in group)}
 publications=[r for r in rows(run/'background-publications.csv') if low<=int(r['master_cycles'])<high]
 traffic=Counter('no_transfer' if r['data_words']=='0' else 'scroll_only' if r['plane_a_words']==r['plane_b_words']=='0' else 'plane_and_scroll' for r in publications)
 assert traffic=={'no_transfer':200,'scroll_only':38,'plane_and_scroll':24}
 first=(run/'rejection-002-source.bin').read_bytes()[0xe200:];w16=lambda p:int.from_bytes(first[p&65535:(p&65535)+2],'big');w32=lambda p:int.from_bytes(first[p&65535:(p&65535)+4],'big')
 actual20={'raw_busy':w16(0xff4010),'active':w16(0xff4014),'raw_epoch':w32(0xff4004),'prepared_epoch':w32(0xff632a)};assert actual20=={'raw_busy':0,'active':0,'raw_epoch':747,'prepared_epoch':746}
 e={'scope':__doc__,'intervals':361,'source_completions':325,'source_completions_per_second':325/361*HZ/FRAME,'background_publications':262,'publication_callbacks_per_second':262/361*HZ/FRAME,'misses':dict(counts),'busy_preparation_types':dict(Counter(d['preparation_kind'] for d in misses if d['reason']=='busy_during_preparation')),'view_mismatch_previous_busy_types':dict(Counter(d['preparation_kind'] for d in misses if d['reason']=='view_mismatch')),'preparations':summary,'publication_traffic':dict(traffic),'guard_instruction_fingerprints':fingerprint,'first_epoch_rejection_snapshot':actual20,'native_profile':(out/'profile.csv').read_text(),'extra_publications_required_to_exceed50':40,'limitations':'Marker spans include interrupt time. Silent early guards were not individually instrumented; all45 silent misses lie strictly inside an open preparation, whose busy flag persists until after the IRQ publisher. RawBusy/active share one OR guard and cannot be distinguished individually. Closed guard/profile calls suppress inserted CPU refresh to isolate instruction paths, and exactly match all continuous rejection deltas.200 of262 successful publications transfer no data: callback rate is not unique-picture FPS. Five strict source-register diagnostic failures and final missing-resource refusal remain unadmitted. No source timing/view/epoch guard is relaxed.','compile_commands':commands,'inputs':pins}
 assert all(sha(Path(q))==h for q,h in pins.items())
 (out/'evidence.json').write_text(json.dumps(e,indent=2)+'\n');(out/'misses.json').write_text(json.dumps(misses,indent=2)+'\n');(out/'preparation-spans.json').write_text(json.dumps(dict(groups),indent=2)+'\n');shutil.copy2(Path(__file__),out/Path(__file__).name);print(json.dumps({k:e[k] for k in ['misses','preparations','publication_traffic','native_profile']},indent=2))
if __name__=='__main__':main()
