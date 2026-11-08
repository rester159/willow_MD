#!/usr/bin/env python3
"""Measure live contiguous four-tile object requirements; no allocation change."""
from pathlib import Path
import argparse,collections,csv,hashlib,json,struct

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def rows(path):return list(csv.DictReader(path.open()))

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--run',type=Path,default=ROOT/'build/native-object-residency/observer-v2')
 p.add_argument('--assets',type=Path,default=ROOT/'build/native-route-objects/prepared-v2')
 p.add_argument('--background',type=Path,default=ROOT/'build/native-route-background-audit/capacity-v3')
 p.add_argument('--output',type=Path,default=ROOT/'build/native-object-residency/capacity-v1')
 a=p.parse_args();run=a.run.resolve();assets=a.assets.resolve();background=a.background.resolve();out=a.output.resolve()
 assert not out.exists(),'Use a fresh audit directory'
 evidence=json.loads((run/'evidence.json').read_text())
 for name,digest in evidence['outputs'].items():assert sha(run/name)==digest
 manifest=json.loads((assets/'manifest.json').read_text());patterns=(assets/'object-patterns.bin').read_bytes()
 assert sha(assets/'object-patterns.bin')==manifest['outputs']['object-patterns.bin']
 first=manifest['first_tile'];assert len(patterns)%128==0
 blocks={first+n//32:patterns[n:n+128] for n in range(0,len(patterns),128)}
 assert len(set(blocks.values()))==len(blocks),'Exact duplicate blocks need explicit canonicalization'
 shapes={t['key']:{cell['attribute']&2047 for cell in t['cells']} for t in manifest['templates']}
 assert len(shapes)==17 and all(block in blocks for shape in shapes.values() for block in shape)
 raw=(run/'object-residency.bin').read_bytes();events=rows(run/'object-residency.csv')
 assert len(raw)==len(events)*4096
 sat_rows=rows(run/'render-sat.csv');sat_raw=(run/'render-sat.bin').read_bytes()
 sats={(int(r['frame']),int(r['capture'])):(r,sat_raw[int(r['sample'])*640:(int(r['sample'])+1)*640]) for r in sat_rows}
 transfers={int(r['frame']):int(r['source_pieces']) for r in rows(run/'render-transfers.csv') if r['path']=='0' and r['status']=='0'}
 displayed=set();displayed_pose=set();cached=[set(),set()];cached_epochs=[None,None];inflight=set()
 output=[];sets=[];published=0;active_keys=set();previous=None;phase_counts=collections.Counter()
 for event in events:
  sample=int(event['sample']);phase=int(event['phase']);bank=int(event['bank']);epoch=int(event['epoch']);frame=int(event['frame'])
  active=event['manager']==event['scene']=='2';blob=raw[sample*4096:(sample+1)*4096]
  bank_sets=[];prefixes=[];supported=True;keys_by_bank=[]
  for b in range(2):
   count=int(event[f'bank{b}_count']);assert 0<=count<=256
   records=list(struct.iter_unpack('>4H',blob[b*2048:(b+1)*2048]));prefixes.append(records[:count])
   keys={(code<<16)|attribute for x,y,code,attribute in records[:count]};keys_by_bank.append(keys)
   if active:
    assert int(event[f'bank{b}_valid']) and all(x==attribute==0 for x,y,code,attribute in records[count:])
    assert keys<=shapes.keys(),(sample,b,keys-shapes.keys());active_keys.update(keys)
   supported&=keys<=shapes.keys()
   bank_sets.append(set().union(*(shapes.get(key,set()) for key in keys)))
  if phase==1:inflight=bank_sets[bank] if supported else set()
  if phase==2:
   cached[bank]=bank_sets[bank] if supported else set();cached_epochs[bank]=epoch if supported else None;inflight=set()
  if phase==4:
   assert active and cached_epochs[bank]==epoch
   r,sat=sats[frame,int(event['capture'])];assert int(r['bank'])==bank and int(r['epoch'])==epoch
   displayed=set();pieces=transfers[frame]
   assert 0<pieces<=80
   for n in range(pieces):
    y,size_link,attribute,x=struct.unpack_from('>4H',sat,n*8)
    assert size_link>>8==5 and (size_link&127)==(n+1 if n+1<pieces else 0)
    base=attribute&2047;assert base in blocks;displayed.add(base)
   assert displayed<=bank_sets[bank] and bank_sets[bank]==cached[bank]
   displayed_pose=bank_sets[bank];published+=1
  if not active:continue
  assert cached_epochs[0] is not None and cached_epochs[1] is not None
  phase_counts[phase]+=1
  required={
   'displayed_sat':displayed,
   'displayed_full_pose':displayed_pose,
   'both_source_banks':bank_sets[0]|bank_sets[1],
   'displayed_sat_and_both_source_banks':displayed|bank_sets[0]|bank_sets[1],
   'displayed_full_pose_and_both_caches':displayed_pose|cached[0]|cached[1],
   'conservative_displayed_pose_both_caches_inflight':displayed_pose|cached[0]|cached[1]|inflight,
  }
  conservative=required['conservative_displayed_pose_both_caches_inflight']
  row={'sample':sample,'frame':frame,'master_cycles':int(event['master_cycles']),'phase':phase,'bank':bank,'epoch':epoch,'capture':int(event['capture'])}
  row.update({name+'_patterns':len(values)*4 for name,values in required.items()})
  row['new_conservative_patterns_vs_previous_event']=len(conservative-previous)*4 if previous is not None else len(conservative)*4
  row['initial_active_event']=previous is None;previous=conservative;output.append(row)
  sets.append({'sample':sample,'frame':frame,'phase':phase,'displayed_sat_block_bases':sorted(displayed),
               'displayed_full_pose_block_bases':sorted(displayed_pose),'cached_bank0_block_bases':sorted(cached[0]),
               'cached_bank1_block_bases':sorted(cached[1]),'inflight_block_bases':sorted(inflight),
               'conservative_block_bases':sorted(conservative)})
 assert published==464 and phase_counts=={1:464,2:464,4:464}
 assert active_keys<=shapes.keys()
 peak_columns=[key for key in output[0] if key.endswith('_patterns')]
 peaks={key:max(r[key] for r in output) for key in peak_columns}
 by_frame=collections.defaultdict(list)
 for row in output:by_frame[row['frame']].append(row)
 bg_rows=json.loads((background/'frames.json').read_text());assert len(bg_rows)==464
 bg_summary=json.loads((background/'summary.json').read_text())
 bg_by_frame={r['frame']:r for r in bg_rows};frame_results=[]
 for frame,group in sorted(by_frame.items()):
  bg=bg_by_frame[frame];object_peak=max(r['conservative_displayed_pose_both_caches_inflight_patterns'] for r in group)
  bg_current=bg['previous_current_visible_union_including_overlays']
  next_bg=bg_by_frame.get(frame+1)
  # Current source preparation may prepare the next IRQ's background while the
  # present frame remains visible. Compare both index associations explicitly.
  bg_next=next_bg['previous_current_visible_union_including_overlays'] if next_bg else None
  frame_results.append({'frame':frame,'capture':bg['capture'],'object_patterns':object_peak,
                        'bg_previous_current_union':bg_current,'bg_current_next_union':bg_next,
                        'conditional_window_patterns':105,'reserved_low_patterns':16,
                        'combined_current':object_peak+bg_current+105+16,
                        'combined_preparing_next':object_peak+bg_next+105+16 if bg_next is not None else None})
 object_max=peaks['conservative_displayed_pose_both_caches_inflight_patterns']
 bg_max=bg_summary['peak_previous_current_visible_union_including_overlays'];upper=object_max+bg_max+105+16
 out.mkdir(parents=True)
 for name,content in [('events.json',output),('event-resident-sets.json',sets),('frames.json',frame_results)]:
  (out/name).write_text(json.dumps(content,indent=2)+'\n')
 summary={'scope':__doc__,'observed_successful_publications':published,'active_events_by_phase':dict(phase_counts),
  'immutable_resource_templates':len(shapes),'all_route_patterns':len(patterns)//32,'referenced_route_patterns':len(set().union(*shapes.values()))*4,
  'exact_128byte_blocks':len(blocks),'block_layout':'Four contiguous Genesis8x8 tiles in original TL,BL,TR,BR order; byte-identical sharing only. Original tile bases identify ROM blocks in this audit, not a proposed fixed VRAM location.',
  'peak_patterns':peaks,'peak_new_conservative_patterns_after_first':max(r['new_conservative_patterns_vs_previous_event'] for r in output if not r['initial_active_event']),
  'maximum_capacity':{'patterns_before_plane_A':1536,'object_conservative_peak':object_max,'bg_previous_current_union_peak':bg_max,'conditional_window':105,'reserved_low':16,'separate_maxima_upper_bound':upper,'headroom':1536-upper,
                      'matched_frame_peak':max(r['combined_current'] for r in frame_results),'preparing_next_frame_peak':max(r['combined_preparing_next'] for r in frame_results if r['combined_preparing_next'] is not None)},
  'limitations':['Finite route coverage; no positions, animation timing or source records are baked into resources. All cells of each complete source pose are retained, including clipped cells.','Window105 is the static opening resource reserve, not admission of all active or future HUD patterns.','Background counts cover previous/current visible canonical patterns and conditional priority overlays; guard columns/rows and a working streaming allocator are not included.','No ROM/VRAM allocation changed. A future allocator must preserve contiguous four-tile object blocks, actual displayed SAT ownership, immutable palette lease and both prepared-cache/inflight references.','DMA residency lifetime, fragmentation, loading CPU/bus cost and the future complete-scene VBlank deadline still require actual native proof.'],
  'inputs':{str(path):sha(path) for path in [Path(__file__).resolve(),run/'evidence.json',run/'object-residency.csv',run/'object-residency.bin',run/'render-sat.csv',run/'render-sat.bin',run/'render-transfers.csv',assets/'manifest.json',assets/'object-patterns.bin',background/'summary.json',background/'frames.json']},
  'outputs':{name:sha(out/name) for name in ['events.json','event-resident-sets.json','frames.json']}}
 (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
 print(json.dumps({key:summary[key] for key in ['observed_successful_publications','active_events_by_phase','all_route_patterns','referenced_route_patterns','peak_patterns','peak_new_conservative_patterns_after_first','maximum_capacity']},indent=2))

if __name__=='__main__':main()
