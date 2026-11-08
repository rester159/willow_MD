#!/usr/bin/env python3
"""Summarize source-derived map updates and old staged scroll publication."""
from pathlib import Path
import argparse,collections,csv,hashlib,json
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return list(csv.DictReader(p.open()))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',type=Path,default=ROOT/'build/native-background-lifetime/v1');a=p.parse_args();out=a.input.resolve()
 events=load(out/'background-events.csv');irqs=load(out/'background-irqs.csv');active=[r for r in irqs if int(r['active_index'])]
 source=(ROOT/'build/reference_maincpu.bin').read_bytes();assert hashlib.sha256(source).hexdigest()=='0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1'
 pin=ROOT/'build/native-render-cadence/checkpoint-content-v2'
 identical=[]
 for name in ['frames.csv','irqs.csv','producer-epochs.csv','metadata-acquisitions.csv','publication-latches.csv','render-sat.bin','render-sat.csv','render-transfers.csv']:
  assert sha(out/name)==sha(pin/'observer'/name),name
  identical.append(name)
 sites=collections.defaultdict(lambda:{'accesses':0,'active':0,'changed':0,'min_address':0xffffff,'max_address':0})
 writes=[]
 for r in events:
  k=int(r['kind']);at=int(r['address'],16);pc=int(r['callback_pc'],16)
  if k not in [2,3] or not 0x914000<=at<0x91c000:continue
  layer=2 if at>=0x918000 else 3;s=sites[layer,k,pc]
  s['accesses']+=1;s['active']+=r['manager']==r['scene']=='2';s['changed']+=k==3 and r['value']!=r['previous'];s['min_address']=min(s['min_address'],at);s['max_address']=max(s['max_address'],at)
  if k==3 and r['manager']==r['scene']=='2':writes.append(r)
 # Every active producer word is checked against source ROM and source RMW.
 assert len(writes)%3==0
 pairs=[]
 for n in range(0,len(writes),3):
  code,adjust,attr=writes[n:n+3];layer=2 if int(code['address'],16)>=0x918000 else 3
  assert [int(x['callback_pc'],16)for x in [code,adjust,attr]]==([0x437ac,0x437b0,0x437b2]if layer==2 else [0x43df8,0x43dfc,0x43dfe])
  base=int(code['address'],16);rom_at=int(code['a1'],16)-2
  assert int(adjust['address'],16)==base and int(attr['address'],16)==base+2
  assert int(code['value'],16)==int.from_bytes(source[rom_at:rom_at+2],'big')
  assert int(adjust['value'],16)==(int(code['value'],16)+(0 if layer==2 else 0xa00))&65535
  assert int(attr['value'],16)==int.from_bytes(source[rom_at+2:rom_at+4],'big')
  assert code['irq']==adjust['irq']==attr['irq'] and code['frame']==attr['frame']
  pairs.append({'layer':layer,'frame':int(code['frame']),'irq':int(code['irq']),'address':base,'source':rom_at,'begin':int(code['master']),'end':int(attr['master'])})
 batches=[]
 for layer,frame in sorted({(p['layer'],p['frame'])for p in pairs},key=lambda x:(x[1],x[0])):
  group=[p for p in pairs if p['layer']==layer and p['frame']==frame]
  assert len(group)==(34 if layer==2 else 18)
  batches.append({'layer':layer,'frame':frame,'cells':len(group),'begin':group[0]['begin'],'last_attribute':group[-1]['end']})
 # Each source port gets its old staging word, not current logical camera.
 by_irq=collections.defaultdict(list)
 for r in events:
  if r['kind']=='5' and int(r['address'],16)in[0x800110,0x800112,0x800114,0x800116]:by_irq[int(r['irq'])].append(r)
 port_checks=0;stage_port_differences=0
 for r in active:
  rows=by_irq[int(r['irq'])];assert len(rows)==4
  for name,port in [('s2x',0x800110),('s2y',0x800112),('s3x',0x800114),('s3y',0x800116)]:
   v=next(e for e in rows if int(e['address'],16)==port)['value']
   assert int(v,16)==int(r['staged_'+name],16),(r,name,v)
   port_checks+=1;stage_port_differences+=r['port_'+name]!=r['staged_'+name]
 maps=(out/'background-active-maps.bin').read_bytes();assert len(maps)==len(active)*32768
 net=[];totals=[0,0]
 for n in range(1,len(active)):
  before=maps[(n-1)*32768:n*32768];after=maps[n*32768:(n+1)*32768]
  delta=[sum(before[j:j+4]!=after[j:j+4]for j in range(layer*16384,(layer+1)*16384,4))for layer in [0,1]]
  totals=[x+y for x,y in zip(totals,delta)]
  if any(delta):net.append({'frame':int(active[n]['frame']),'s2_cells':delta[0],'s3_cells':delta[1]})
 commits=load(out/'producer-epochs.csv');timing=[]
 for b in batches:
  end=b['last_attribute'];c=next(x for x in commits if int(x['master_cycles'])>end);irq=next(x for x in active if int(x['master'])>end)
  timing.append({**b,'following_object_epoch':int(c['epoch']),'object_commit_master':int(c['master_cycles']),'next_irq_master':int(irq['master']),'last_word_to_object_ms':(int(c['master_cycles'])-end)/53693.175,'last_word_to_irq_ms':(int(irq['master'])-end)/53693.175})
 interrupted=[{k:r[k]for k in ['frame','irq','saved_pc','manager','scene']}for r in irqs if 0x548c<=int(r['saved_pc'],16)<0x5570 or 0x58d0<=int(r['saved_pc'],16)<0x5900 or 0x65fe<=int(r['saved_pc'],16)<=0x660e or 0x42602<=int(r['saved_pc'],16)<0x43ef4]
 result={'scope':'Read-only original-address observation on the frozen content-v2 Genesis-clock route; exact source map reads retained, no target hooks/rendering added.',
  'physical_frames':1800,'irq_first_store_snapshots':len(irqs),'active_irq_snapshots':len(active),'identical_baseline_outputs':identical,
  'map_sites':[{'layer':layer,'kind':kind,'callback_pc':hex(pc),**s}for (layer,kind,pc),s in sorted(sites.items())],
  'active_source_pairs':len(pairs),'active_source_word_writes':len(writes),'source_rom_and_addition_checks':len(pairs),'complete_active_batches':timing,
  'active_net_changed_cells':{'scroll2':totals[0],'scroll3':totals[1]},'net_changed_frames':net,
  'active_old_staging_port_checks':port_checks,'old_port_vs_next_staging_differences':stage_port_differences,
  'interrupted_background_or_bulk_clear_sites':interrupted,
  'limitations':'Active route only has 7 Scroll2 columns and 1 Scroll3 column; no active rows, later-stage mutation, all-stage timing, or CPS-clock claim. Map replay sees all existing source writes; it does not remove their runtime cost. Startup/clear interrupts are preserved. IRQ-first snapshots precede canonical object acquisition and do not confer object-latch semantics on tile RAM. RTS callback experiment emitted no events and is not used as return-boundary evidence.',
  'source_sha256':sha(ROOT/'build/reference_maincpu.bin'),'observer_manifest_sha256':sha(out/'manifest.json'),'analyzer_sha256':sha(Path(__file__))}
 (out/'analysis.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps({k:result[k]for k in ['active_irq_snapshots','active_source_pairs','active_net_changed_cells','active_old_staging_port_checks','identical_baseline_outputs']},indent=2))
if __name__=='__main__':main()
