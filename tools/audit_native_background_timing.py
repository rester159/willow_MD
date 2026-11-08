#!/usr/bin/env python3
"""Audit the source's staged scroll publication without adding target hooks."""
from pathlib import Path
import argparse,bisect,collections,csv,hashlib,json

ROOT=Path(__file__).resolve().parents[1]
AXES=[('s2x',0xf446,0x8032,0x800110,0x1998,0x199e),
      ('s2y',0xf448,0x8034,0x800112,0x1a14,0x1a1c),
      ('s3x',0xf44a,0x8036,0x800114,0x19aa,0x19b0),
      ('s3y',0xf44c,0x8038,0x800116,0x1a28,0x1a30)]
VIEW_PORTS=[0x102,0x104,0x106,0x108,0x10a,0x122,0x168,0x16a,0x16c,0x16e,0x170]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def csvrows(path):return list(csv.DictReader(path.open()))
def word(blob,at):return int.from_bytes(blob[at:at+2],'big')

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--capture',type=Path,default=ROOT/'build/native-background-context/content-v2')
 p.add_argument('--lifetime',type=Path,default=ROOT/'build/native-background-lifetime/v1')
 p.add_argument('--output',type=Path,default=ROOT/'build/native-background-context/timing-v1')
 a=p.parse_args();capture=a.capture.resolve();lifetime=a.lifetime.resolve();out=a.output.resolve()
 assert not out.exists(),'Use a fresh audit directory'
 inputs=[capture/'checkpoint.json',capture/'layout.json',capture/'source-background-contexts.csv',capture/'source-background-contexts.bin',
         lifetime/'checkpoint.json',lifetime/'background-events.csv',lifetime/'background-irqs.csv',lifetime/'render-events.csv',
         ROOT/'build/reference_maincpu.bin',Path(__file__).resolve()]
 for base,names in [(capture,inputs[1:4]),(lifetime,inputs[5:8])]:
  checkpoint=json.loads((base/'checkpoint.json').read_text())
  for name in names:assert sha(name)==checkpoint['files'][str(name.relative_to(base))]
 program=(ROOT/'build/reference_maincpu.bin').read_bytes()
 # Pin the source load/store instructions, not only the observed values.
 for name,staged,logical,port,callback,stagepc in AXES:
  expected=bytes.fromhex('33ed')+(staged-0x8000).to_bytes(2,'big')+port.to_bytes(4,'big')
  assert program[callback-8:callback]==expected
  assert program[stagepc:stagepc+4]==bytes.fromhex('3b40')+(staged-0x8000).to_bytes(2,'big')
 layout=json.loads((capture/'layout.json').read_text());regions={r['name']:r for r in layout['regions']}
 contexts=[r for r in csvrows(capture/layout['index']) if r['manager']==r['scene']=='2']
 irqs={int(r['frame']):r for r in csvrows(lifetime/'background-irqs.csv') if int(r['active_index'])}
 events=csvrows(lifetime/'background-events.csv');event_times=[int(r['master']) for r in events]
 prepares=[r for r in csvrows(lifetime/'render-events.csv') if r['path']=='0' and r['event']=='prepare_begin']
 completions=[r for r in csvrows(lifetime/'render-events.csv') if r['path']=='0' and r['event']=='prepare_complete']
 prepare_times=[int(r['absolute_master']) for r in prepares];completion_times=[int(r['absolute_master']) for r in completions]
 stage_history={axis[1]:[] for axis in AXES};port_events=collections.defaultdict(list)
 for event in events:
  at=int(event['address'],16)
  if event['kind']=='1' and at in stage_history:
   assert event['width']=='2'
   stage_history[at].append((int(event['master']),int(event['value'],16)))
  if event['kind']=='5':port_events[int(event['irq'])].append(event)
 results=[];checks=collections.Counter();last_scroll_ticks=[];example=None
 with(capture/layout['binary']).open('rb')as stream:
  for context in contexts:
   frame=int(context['frame']);irq=irqs[frame];irq_time=int(irq['master']);irq_number=int(irq['irq'])
   stream.seek(int(context['sample'])*layout['record_bytes']);blob=stream.read(layout['record_bytes'])
   ram=blob[regions['work_ram']['offset']:regions['work_ram']['offset']+65536]
   oldports=blob[regions['ports']['offset']:regions['ports']['offset']+512]
   prepare=prepares[bisect.bisect_left(prepare_times,irq_time)-1]
   complete=completions[bisect.bisect_left(completion_times,irq_time)-1]
   begin=int(prepare['absolute_master']);end=int(complete['absolute_master'])
   assert begin<=end<irq_time
   result={'frame':frame,'capture':int(context['capture']),'irq':irq_number,
           'prepare_begin_master':begin,'prepare_complete_master':end,
           'pre_source_irq_capture_master':int(context['master_cycles'])}
   outputs=port_events[irq_number];latest=dict((int(e['address'],16),int(e['value'],16))for e in outputs)
   stores=[]
   for name,staged,logical,port,callback,stagepc in AXES:
    found=[e for e in outputs if int(e['address'],16)==port]
    assert len(found)==1 and int(found[0]['callback_pc'],16)==callback
    store=found[0];stores.append(store);value=int(store['value'],16)
    old=word(oldports,port-0x800000);stage=word(ram,staged);camera=word(ram,logical)
    assert old==int(irq['port_'+name],16) and stage==int(irq['staged_'+name],16)
    assert value==stage
    history=stage_history[staged];index=bisect.bisect_right(history,(begin,65536))
    prepared=history[index-1][1] if index else 0
    assert prepared==stage,(frame,name,prepared,stage)
    # Each old staging word is published first. Its replacement is derived
    # from the source logical camera, and belongs to the following IRQ.
    future=(camera-64 if name.endswith('x') else (768 if name=='s2y' else 1792)-camera)&65535
    successors=[e for e in events[bisect.bisect_right(event_times,int(store['master'])):bisect.bisect_left(event_times,irq_time+896040)]
                if e['kind']=='1' and int(e['address'],16)==staged]
    assert successors and int(successors[0]['callback_pc'],16)==stagepc+4 and int(successors[0]['value'],16)==future
    checks['axis_staged_at_prepare_matches_actual_publication']+=1
    checks['old_port_differs_from_upcoming_port']+=old!=value
    result.update({name+'_old_port':old,name+'_staged':stage,name+'_published':value,name+'_logical':camera,name+'_next_staged':future})
    if old!=value and example is None:example={'frame':frame,'axis':name,'old_port':old,'upcoming_published':value,'logical_camera':camera,'next_staged':future}
   last=max(int(e['master'])for e in stores)
   result['last_scroll_port_master']=last;last_scroll_ticks.append(last-frame*896040)
   between=events[bisect.bisect_right(event_times,begin):bisect.bisect_right(event_times,last)]
   writes=[e for e in between if e['kind']=='3' and 0x914000<=int(e['address'],16)<0x91c000]
   assert not writes
   checks['complete_preparation_to_last_scroll_port_map_stability']+=1
   checks['preparation_on_previous_physical_frame']+=int(prepare['frame'])==frame-1
   for at in VIEW_PORTS:
    current=latest.get(0x800000+at,word(oldports,at))
    assert current==word(oldports,at),(frame,hex(at))
    checks['fixed_actual_view_control_words']+=1
   assert word(oldports,0x122)==0x3e,'This bounded route has neither global flip nor row scroll'
   results.append(result)
 # Verify every actual write against the current published viewport, including
 # intermediate Scroll3 raw-code stores before its source ADDI #0A00.
 liveports={};visible_writes={320:0,384:0};write_frames=set();active_writes=0
 for event in events:
  kind=int(event['kind']);at=int(event['address'],16)
  if kind==5:liveports[at]=int(event['value'],16)
  if kind!=3 or event['manager']!=event['scene'] or event['manager']!='2' or not 0x914000<=at<0x91c000:continue
  layer=2 if at>=0x918000 else 3;size=16 if layer==2 else 32
  index=(at-(0x918000 if layer==2 else 0x914000))//4
  col=(index>>(4 if layer==2 else 3))&63
  row=(index&(15 if layer==2 else 7))|((index>>6)&(48 if layer==2 else 56))
  for crop,width in [(96,320),(64,384)]:
   x0=(liveports[0x800110 if layer==2 else 0x800114]+crop)%(size*64)
   y0=(liveports[0x800112 if layer==2 else 0x800116]+16)%(size*64)
   columns={(x0+x)//size%64 for x in range(width)};rows={(y0+y)//size%64 for y in range(224)}
   visible_writes[width]+=col in columns and row in rows
  active_writes+=1;write_frames.add(int(event['frame']))
 assert active_writes==768 and not any(visible_writes.values())
 out.mkdir(parents=True)
 with(out/'upcoming-views.csv').open('w')as f:
  writer=csv.DictWriter(f,fieldnames=list(results[0]));writer.writeheader();writer.writerows(results)
 summary={'scope':__doc__,'active_presentations':len(results),'checks':dict(checks),'first_old_port_latency_example':example,
  'source_map_write_visibility':{'word_writes':active_writes,'frames':sorted(write_frames),'intersecting_current_visible_cells':visible_writes,'scope':'Complete source word writes, including temporary values, in captured unflipped/no-row-scroll route; 320x224 Genesis crop and 384x224 arcade viewport. No generalized offscreen assumption.'},
  'last_scroll_port_frame_ticks':{'minimum':min(last_scroll_ticks),'maximum':max(last_scroll_ticks),'maximum_ms':max(last_scroll_ticks)/53693.175,'note':'Actual source port callbacks only; does not include remaining source IRQ or any future native BG publication cost.'},
  'prepare_hook':'After original Scroll2/3 controller completion at source3B536, or the later existing object commit wrapper. Store the existing FFF446/F448/F44A/F44C values as the upcoming view; do not recompute from FF8032/34/36/38.',
  'publish_hook':'Conservative source end-of-IRQ bridge before original15B4 MOVEM restore/15B8 RTE, preserving its exception frame, after18D0 ports and15BA palette-base/control publication plus later Scroll1/row-scroll work. Revalidate the actual port view and source map generation; deadline and all later state must be independently measured after implementation.',
  'oracle_boundary':'Source state at completed original IRQ plus actual published scroll ports. The earlier PRE1554 snapshot provides maps on this bounded route but its old scroll ports must be replaced by these same-IRQ original writes. Keep object latch association separate.',
  'unproved_transition_hazards':['No end-of-IRQ palette/Scroll1/row-table snapshot was added in this read-only audit; their entire lifetime is not admitted by map checks.','Cold fills, clears, active row writers, later stages, palette/view changes, global flip and row scroll are outside this route; map writers can be interrupted in startup.','If new native preparation overlaps VBlank, a changed map/lease/view generation must reject or use a separately exact fallback; retaining a stale plane is not exact.','Full source IRQ completion plus native BG publication must meet actual VDP fetch deadline; this audit adds no native hook and makes no total timing claim.'],
  'inputs':{str(path):sha(path) for path in inputs},'outputs':{'upcoming-views.csv':sha(out/'upcoming-views.csv')}}
 (out/'evidence.json').write_text(json.dumps(summary,indent=2)+'\n')
 print(json.dumps({k:summary[k] for k in ['active_presentations','checks','first_old_port_latency_example','source_map_write_visibility','last_scroll_port_frame_ticks']},indent=2))

if __name__=='__main__':main()
