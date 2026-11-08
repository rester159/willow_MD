#!/usr/bin/env python3
"""Account exact native call-stack bus spans; prove the replay is observational."""
from pathlib import Path
import argparse,collections,csv,hashlib,json
HZ=53693175
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=lambda p:list(csv.DictReader(p.open()))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();assert not a.output.exists();a.output.mkdir(parents=True)
 pins={};same={}
 for f in a.baseline.iterdir():
  if f.suffix not in ('.bin','.csv','.ppm') or f.name=='far-first-streamed-frame.png':continue
  q=a.run/f.name;assert q.exists(),f.name;h=sha(f);assert sha(q)==h,('Replay changed target product',f.name);same[f.name]=h
  pins[str(f.resolve())]=h;pins[str(q.resolve())]=h
 for f in [a.run/'observer-manifest.json',a.run/'far-profile-sites.json',a.run/'far-profile-open.json',a.run/'far-function-calls.csv',Path(__file__)]:pins[str(f.resolve())]=sha(f)
 site=json.loads((a.run/'far-profile-sites.json').read_text());names={r['begin']:r['name']for r in site['functions']}
 nodes=[]
 for row in rows(a.run/'far-function-calls.csv'):
  r={k:int(v,16)if k in ('callee','call_return','stack')else int(v)for k,v in row.items()};assert r['sample']==len(nodes);r['name']=names[r['callee']];r['children']=[];r['wall']=r['end_master']-r['begin_master'];assert 0<=r['irq_master']<=r['wall'];nodes.append(r)
 stack=[]
 for i in sorted(range(len(nodes)),key=lambda i:(nodes[i]['begin_master'],-nodes[i]['end_master'])):
  r=nodes[i]
  while stack and nodes[stack[-1]]['end_master']<=r['begin_master']:stack.pop()
  if stack:
   parent=nodes[stack[-1]];assert r['end_master']<=parent['end_master'],('Crossed native call intervals',parent,r);parent['children'].append(i)
  stack.append(i)
 for r in nodes:
  child=[nodes[i]for i in r['children']];r['exclusive_wall']=r['wall']-sum(c['wall']for c in child);assert r['exclusive_wall']>=0
  r['suspended_irq']=0 if r['entered_in_irq'] else r['irq_master'];r['active_wall']=r['wall']-r['suspended_irq']
  if r['entered_in_irq']:r['exclusive_active']=r['exclusive_wall']
  else:r['exclusive_active']=r['active_wall']-sum(c['wall']-c['irq_master']for c in child)
  assert r['exclusive_active']>=0
 def group(ns):
  groups=collections.defaultdict(list)
  for r in ns:groups[r['name']].append(r)
  return sorted([{'function':name,'calls':len(rs),**{k+'_ms':sum(r[k]for r in rs)*1000/HZ for k in ('wall','active_wall','exclusive_active','suspended_irq')},'max_wall_ms':max(r['wall']for r in rs)*1000/HZ}for name,rs in groups.items()],key=lambda r:-r['wall_ms'])
 bg=[r for r in rows(a.run/'background-events.csv')if r['path']=='0'];end=next(r for r in bg if r['phase']=='8');begin=next(r for r in reversed(bg[:bg.index(end)])if r['phase']=='1');lo,hi=int(begin['master_cycles']),int(end['master_cycles'])
 first=[r for r in nodes if lo<=r['begin_master'] and r['end_master']<=hi]
 enclosing=[r for r in nodes if r['name']=='nativeBackgroundPrepare' and r['begin_master']<=lo<=hi<=r['end_master']];assert len(enclosing)==1
 batches=[];opened=None
 for r in rows(a.run/'far-batches.csv'):
  if r['path']!='0':continue
  if r['phase']=='9':opened=r
  elif r['phase']=='10':
   assert opened;bl,bh=int(opened['master_cycles']),int(r['master_cycles']);ns=[n for n in nodes if bl<=n['begin_master'] and n['end_master']<=bh]
   batches.append({'frame':int(r['frame']),'patterns':int(r['patterns']),'phase_wall_ms':(bh-bl)*1000/HZ,'functions':group(ns)});opened=None
 report={'scope':__doc__,'identical_deterministic_products':same,'call_boundary':site['duration_boundary'],'irq_boundary':site['irq_subtraction'],'closed_calls':len(nodes),'open_calls':json.loads((a.run/'far-profile-open.json').read_text()),'first_miss':{'begin':begin,'end':end,'phase_wall_ms':(hi-lo)*1000/HZ,'enclosing_prepare':group(enclosing),'functions':group(first)},'batches':batches,'inputs':pins,'target_admitted':False,'note':'Nested inclusive values must not be summed. exclusive_active subtracts direct observed native child spans and only preempting original IRQ time; code tail-called without a new JSR remains attributed to the entered callee. VDP waits are included in elapsed target mastercycles.'}
 (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n');(a.output/'all-function-summary.json').write_text(json.dumps(group(nodes),indent=2)+'\n')
 print(json.dumps({'identical_products':len(same),'closed_calls':len(nodes),'open_calls':report['open_calls'],'first_miss':report['first_miss'],'batches':batches},indent=2))
if __name__=='__main__':main()
