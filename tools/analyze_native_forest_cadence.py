#!/usr/bin/env python3
"""Count original game-body completions separately from physical presentation.

Only complete observed IRQ intervals and opcode-guarded original93BE bodies
contribute. Missing background/SAT publications remain in each time window.
Input ticks and scheduler sleeps are not game frames. This is a bounded route
measurement, not source-parity, visual, all-stage, or smooth-play admission.
"""
import argparse
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path

HZ=53693175
FRAME=896040
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=lambda p:list(csv.DictReader(p.open()))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--after-tick',type=int,default=277)
    a=p.parse_args();run=a.run.resolve();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
    names=['observer-manifest.json','coverage-stop.json','frames.csv','main-loop-events.csv','producer-epochs.csv','render-events.csv','background-events.csv','render-intervals.csv','background-publications.csv','render-sat.csv','register-failures.csv']
    pins={str(run/n):sha(run/n) for n in names};pins[str(Path(__file__).resolve())]=sha(Path(__file__))
    manifest=json.loads((run/'observer-manifest.json').read_text());stop=json.loads((run/'coverage-stop.json').read_text())
    assert manifest['run']['exit_code']==3 and stop['all_other_per_frame_source_and_vdp_checks_completed']
    assert manifest['forest_extension']['source_body_observation']=={'entry':0x92ee,'completed_before_call':0x92f2,'original_body':0x93be,'opcode_guards':True,'host_target_writes':False}
    for name,digest in manifest['inputs'].items():assert sha(Path(name))==digest,name
    frame_rows=rows(run/'frames.csv');assert len(frame_rows)==stop['paired_frames_completed']
    capture_path=run/f"rejection-{stop['first_unsupported_capture']:03d}.json"
    pins[str(capture_path)]=sha(capture_path);gap=json.loads(capture_path.read_text())
    assert gap['unsupported'] and gap['operation']=='prepare'
    ticks={int(r['frame']):int(r['route_tick']) for r in frame_rows}
    productions=rows(run/'producer-epochs.csv')
    events=rows(run/'main-loop-events.csv');opened=None;bodies=[]
    for r in events:
        if r['event']=='body_begin':
            assert opened is None and int(r['pc'])==0x92f2 and int(r['task'])==0xff0060
            opened=r
        elif r['event']=='body_complete':
            assert opened and r['body']==opened['body'] and int(r['pc'])==0x92f6 and int(r['task'])==0xff0060
            start,end=int(opened['master_cycles']),int(r['master_cycles']);assert end>start
            if opened['manager']==opened['scene']==r['manager']==r['scene']=='2':
                produced=[e for e in productions if start<=int(e['master_cycles'])<end]
                assert len(produced)==1,('Original fully-active body/producer relationship changed',r,len(produced))
                bodies.append({'body':int(r['body']),'begin':start,'end':end,'start_frame':int(opened['frame']),'end_frame':int(r['frame']),'producer_epoch':int(produced[0]['epoch'])})
            opened=None
    native_events=[r for r in rows(run/'render-events.csv') if r['path']=='0']
    object_events=[r for r in native_events if r['event']=='publish_complete']
    all_bg_events=[r for r in rows(run/'background-events.csv') if r['path']=='0']
    bg_events=[r for r in all_bg_events if r['phase']=='4']
    if gap['kind']=='background':
        starts=[int(r['master_cycles']) for r in all_bg_events if r['phase']=='1' and int(r['master_cycles'])<gap['master_cycles']]
    else:
        starts=[int(r['absolute_master']) for r in native_events if r['event']=='prepare_begin' and int(r['absolute_master'])<gap['master_cycles']]
    assert starts;gap_begin=max(starts)
    windows=[]
    for r in rows(run/'render-intervals.csv'):
        if r['path']!='0' or r['active']!='1':continue
        start,end=int(r['begin_master']),int(r['end_master']);assert end>start
        completed=[b for b in bodies if start<=b['end']<end]
        begin_frame=start//FRAME
        windows.append({'irq':int(r['irq']),'begin_frame':begin_frame,'end_frame':int(r['end_frame']),
                        'begin_active_input_tick':ticks[begin_frame],'active_input_tick':ticks[int(r['end_frame'])],
                        'begin_master':start,'end_master':end,'duration_master':end-start,
                        'source_body_completions':len(completed),'completed_body_ids':[b['body'] for b in completed],
                        'scheduler_sleeps':int(r['source_main_commits']),
                        'background_publications':sum(start<=int(e['master_cycles'])<end for e in bg_events),
                        'object_publications':sum(start<=int(e['absolute_master'])<end for e in object_events)})
    def summary(group):
        count=len(group);body=sum(w['source_body_completions'] for w in group);elapsed=sum(w['duration_master'] for w in group)
        nominal=body/count*HZ/FRAME if count else None
        return {'complete_physical_irq_intervals':count,'original_body_completions':body,
                'original_body_completions_per_second_nominal_refresh':nominal,
                'original_body_completions_per_second_observed_elapsed':body*HZ/elapsed if elapsed else None,
                'body_completions_per_interval':dict(sorted(Counter(w['source_body_completions'] for w in group).items())),
                'background_publications':sum(w['background_publications'] for w in group),
                'intervals_without_background_publication':sum(w['background_publications']==0 for w in group),
                'object_publications':sum(w['object_publications'] for w in group),
                'intervals_without_object_publication':sum(w['object_publications']==0 for w in group),
                'source_body_mean_above_50':nominal>50 if nominal is not None else None}
    after=[w for w in windows if w['begin_active_input_tick']>a.after_tick]
    before_gap=[w for w in windows if w['end_master']<=gap_begin]
    report={'scope':__doc__,'physical_refresh_per_second':HZ/FRAME,'fully_active_bodies_with_exactly_one_original_producer':len(bodies),
            'all_complete_active_intervals':summary(windows),'after_active_input_tick':a.after_tick,
            'all_complete_intervals_after_boundary_including_missing_publications':summary(after),
            'first_unsupported_preparation_begin_master':gap_begin,
            'all_complete_active_intervals_before_unsupported_preparation':summary(before_gap),
            'complete_intervals_after_boundary_before_unsupported_preparation':summary([w for w in before_gap if w['begin_active_input_tick']>a.after_tick]),
            'incomplete_final_body':int(opened['body']) if opened else None,
            'paired_frames_completed':stop['paired_frames_completed'],'active_input_ticks':stop['active_input_ticks'],
            'source_register_failures_retained':stop['source_register_failures_retained'],'coverage_complete':False,
            'source_parity_admitted':False,'smooth_play_admitted':False,'inputs':pins}
    assert all(sha(Path(n))==d for n,d in pins.items())
    (out/'bodies.json').write_text(json.dumps(bodies,indent=2)+'\n');(out/'intervals.json').write_text(json.dumps(windows,indent=2)+'\n')
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='inputs'},indent=2))


if __name__=='__main__':main()
