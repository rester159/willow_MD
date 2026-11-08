#!/usr/bin/env python3
"""Separate original source-body completion, sleep decisions and renderer cost.

All spans come from one continuous execution. Nested native/BG wall-clock
spans are subtracted, not added from independent benchmarks. The remainder
includes original code, native writer hooks and IRQ work, not pure arcade CPU.
"""
from pathlib import Path
import argparse
import collections
import csv
import hashlib
import json
import statistics

HZ = 53693175
FRAME = 896040
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
rows = lambda p: list(csv.DictReader(p.open()))
tick = lambda r,field='master_cycles': int(r[field])


def stats(values):
    return {'count':len(values),'mean_ms':statistics.mean(values)*1000/HZ,'max_ms':max(values)*1000/HZ} if values else {'count':0}


def analyze(run):
    inputs = {name:sha(run/name) for name in ['observer-manifest.json','read-only-evidence.json','main-loop-events.csv','render-events.csv','background-events.csv','producer-epochs.csv','render-intervals.csv','publication-latches.csv','captured-original-banks.bin','headroom-events.csv']}
    events=rows(run/'main-loop-events.csv');productions=rows(run/'producer-epochs.csv')
    intervals=[r for r in rows(run/'render-intervals.csv') if r['path']=='0' and r['active']=='1']
    render=[r for r in rows(run/'render-events.csv') if r['path']=='0'];bg=[r for r in rows(run/'background-events.csv') if r['path']=='0']
    prefixes={};raw=(run/'captured-original-banks.bin').read_bytes();latches=rows(run/'publication-latches.csv')
    assert len(raw)==len(latches)*2048
    for n,r in enumerate(latches):
        if r['valid_complete_epoch']!='1':continue
        key=int(r['producer_epoch']);digest=hashlib.sha256(raw[n*2048:n*2048+int(r['count'])*8]).hexdigest()
        assert key not in prefixes or prefixes[key]==digest
        prefixes[key]=digest
    render_spans=[];opened=None
    for r in render:
        if r['event']=='prepare_begin':assert opened is None;opened=r
        elif r['event'] in ('prepare_complete','prepare_reject'):
            assert opened is not None;render_spans.append((tick(opened,'absolute_master'),tick(r,'absolute_master'),opened,r));opened=None
    bg_spans=[];opened={};previous_view=None
    for r in bg:
        phase=int(r['phase'])
        if phase in (1,3):assert phase not in opened;opened[phase]=r
        else:
            start=3 if phase==4 or phase==5 and 3 in opened else 1
            a=opened.pop(start)
            if start!=1:continue
            view=tuple(int(a[k]) for k in ['s2x','s2y','s3x','s3y'])
            changed=previous_view is not None and view!=previous_view
            column=previous_view is not None and (view[0]//8,view[2]//8)!=(previous_view[0]//8,previous_view[2]//8)
            bg_spans.append((tick(a),tick(r),a,r,changed,column));previous_view=view
    begun={};bodies=[];decisions={}
    for r in events:
        if r['event'] in ('sleep_flag_set','sleep_bypassed'):decisions[int(r['body'])]=r['event']
        if r['event']=='body_begin':begun[r['body']]=r
        if r['event']!='body_complete':continue
        a=begun.pop(r['body']);start,end=tick(a),tick(r)
        if not a['manager']==a['scene']==r['manager']==r['scene']=='2':continue
        source=[p for p in productions if start<=tick(p)<end]
        native=[p for p in render_spans if start<=p[0] and p[1]<=end]
        back=[p for p in bg_spans if start<=p[0] and p[1]<=end]
        assert len(source)==len(native)==len(back)==1,(r,len(source),len(native),len(back))
        n,b=native[0],back[0];nt=n[1]-n[0];bt=b[1]-b[0]
        assert n[0]<=b[0]<=b[1]<=n[1]
        epoch=int(source[0]['epoch']);view=[int(b[2][k]) for k in ['s2x','s2y','s3x','s3y']]
        bodies.append({'ordinal':len(bodies),'body':int(r['body']),'begin':start,'end':end,'start_frame':int(a['frame']),'end_frame':int(r['frame']),
                       'epoch':epoch,'bank':int(source[0]['bank']),'records':int(source[0]['emitted_records']),'prefix_sha256':prefixes.get(epoch),
                       'view':view,'view_changed':b[4],'column_changed':b[5],
                       'geometry_reused':int(n[3]['source_reuses'])>int(n[2]['source_reuses']),
                       'geometry_built':int(n[3]['geometry_builds'])>int(n[2]['geometry_builds']),
                       's2_writes':int(r['s2_writes']),'s3_writes':int(r['s3_writes']),
                       's2_changed':int(r['s2_changed']),'s3_changed':int(r['s3_changed']),
                       'body_ticks':end-start,'native_prepare_ticks':nt,'bg_prepare_ticks':bt,
                       'native_prepare_outside_bg_ticks':nt-bt,'body_outside_prepare_ticks':end-start-nt,
                       'bg_begin':b[0],'bg_end':b[1],'native_begin':n[0],'native_end':n[1]})
    for b in bodies:b['sleep_decision']=decisions.get(b['body'],'unobserved')
    headroom=rows(run/'headroom-events.csv');windows=[]
    for r in intervals:
        start,end=int(r['begin_master']),int(r['end_master']);over=[b for b in bodies if b['begin']<end and b['end']>start]
        complete=[b for b in bodies if start<=b['end']<end]
        def overlap(a,b):return max(0,min(end,b)-max(start,a))
        race=None
        if not over and not complete:
            irq=[e for e in headroom if e['event']=='irq_first_native_store' and int(e['absolute_master_cycles'])==start]
            commits=[e for e in headroom if e['event']=='main_wait_sleep_committed' and start<int(e['absolute_master_cycles'])<end]
            begin=[e for e in headroom if e['event']=='main_wait_movem_first_store' and int(e['absolute_master_cycles'])<start]
            flags=[e for e in events if e['event']=='sleep_flag_set' and tick(e)<start]
            if len(irq)==len(commits)==1 and int(irq[0]['arg0'],16)==0x1754 and begin and flags:
                at=int(begin[-1]['absolute_master_cycles']);flag=tick(flags[-1])
                if flag<at<start and int(commits[0]['arg1'],16)==0x932c:
                    race={'flag_test':flag,'wait_movem':at,'irq':start,'sleep_commit':int(commits[0]['absolute_master_cycles']),
                          'source_interrupted_pc':0x1754,'meaning':'IRQ ran its task wake pass before the following TRAP3 committed sleep; no body runs before the next IRQ.'}
        windows.append({'irq':int(r['irq']),'end_frame':int(r['end_frame']),'begin':start,'end':end,'sleeps':int(r['source_main_commits']),
                        'body_completions':len(complete),'completed_body_ordinals':[b['ordinal'] for b in complete],
                        'overlapping_body_ordinals':[b['ordinal'] for b in over],
                        'bg_over_1ms':any(b['bg_prepare_ticks']>HZ/1000 and overlap(b['bg_begin'],b['bg_end']) for b in over),
                        'column_change_body':any(b['column_changed'] for b in over),
                        'source_map_writer_body':any(b['s2_writes']+b['s3_writes'] for b in over),
                        'only_unchanged_view_bodies':bool(over) and not any(b['view_changed'] for b in over),
                        'geometry_build_body':any(b['geometry_built'] for b in over),
                        'source_wait_race':race,
                        'bg_prepare_overlap_ticks':sum(overlap(b['bg_begin'],b['bg_end']) for b in over),
                        'native_prepare_overlap_ticks':sum(overlap(b['native_begin'],b['native_end']) for b in over)})
    def describe_windows(selection):
        return {'count':len(selection),**{k:sum(bool(w[k]) for w in selection) for k in ['bg_over_1ms','column_change_body','source_map_writer_body','only_unchanged_view_bodies','geometry_build_body','source_wait_race']},
                'body_completions_histogram':dict(sorted(collections.Counter(w['body_completions'] for w in selection).items()))}
    summaries={}
    groups={'all':bodies,'unchanged_view':[b for b in bodies if not b['view_changed']],
            'changed_view_same_column':[b for b in bodies if b['view_changed'] and not b['column_changed']],
            'changed_column':[b for b in bodies if b['column_changed']],
            'source_map_writer':[b for b in bodies if b['s2_writes']+b['s3_writes']],
            'geometry_reused':[b for b in bodies if b['geometry_reused']],
            'geometry_built':[b for b in bodies if b['geometry_built']]}
    for name,group in groups.items():summaries[name]={k:stats([b[k] for b in group]) for k in ['body_ticks','bg_prepare_ticks','native_prepare_outside_bg_ticks','body_outside_prepare_ticks']}
    completed=sum(w['body_completions'] for w in windows)
    summary={'run':str(run),'scope':__doc__,'fully_active_bodies_checked':len(bodies),'producer_epochs_per_body':{'1':len(bodies)},
             'complete_active_irq_intervals':len(windows),'source_body_completions':completed,'source_body_completions_per_second':completed/len(windows)*HZ/FRAME,
             'source_body_completion_histogram':dict(sorted(collections.Counter(w['body_completions'] for w in windows).items())),
             'sleep_decisions_for_fully_active_bodies':dict(collections.Counter(b['sleep_decision'] for b in bodies)),
             'zero_body_intervals':describe_windows([w for w in windows if not w['body_completions']]),
             'no_main_sleep_intervals':describe_windows([w for w in windows if not w['sleeps']]),
             'body_cost_groups':summaries,'smooth_60fps_admitted':False,'source_register_failure_collection_retained':True,
             'limits':'One producer per body is proved for each observed fully-active93BE invocation, not every branch or every stage. Body timing includes actual native rendering and nested IRQs. Differing input/state sequences are not interchangeable cost samples.',
             'inputs':inputs}
    assert all(sha(run/name)==digest for name,digest in inputs.items())
    return summary,bodies,windows


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',type=Path,required=True);p.add_argument('--compare',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
    summary,bodies,windows=analyze(a.run.resolve())
    (out/'bodies.json').write_text(json.dumps(bodies,indent=2)+'\n');(out/'intervals.json').write_text(json.dumps(windows,indent=2)+'\n')
    if a.compare:
        other,ob,ow=analyze(a.compare.resolve());(out/'comparison-summary.json').write_text(json.dumps(other,indent=2)+'\n')
        pairs=[]
        for x,y in zip(bodies,ob):
            same=x['prefix_sha256'] is not None and x['prefix_sha256']==y['prefix_sha256'] and x['view']==y['view'] and (x['s2_writes'],x['s3_writes'])==(y['s2_writes'],y['s3_writes'])
            pairs.append({'ordinal':x['ordinal'],'body':x['body'],'comparison_body':y['body'],'same_source_prefix_view_and_write_count':same,
                          'body_delta_ticks':x['body_ticks']-y['body_ticks'],'bg_delta_ticks':x['bg_prepare_ticks']-y['bg_prepare_ticks'],
                          'native_outside_bg_delta_ticks':x['native_prepare_outside_bg_ticks']-y['native_prepare_outside_bg_ticks']})
        summary['same_progression_comparison']={'run':str(a.compare.resolve()),'matched_ordinals':len(pairs),'same_exact_prefix_view_and_write_count':sum(p['same_source_prefix_view_and_write_count'] for p in pairs),'warning':'Ordinal alignment alone does not assert identical state; exact source prefix/view/write-count agreement is reported per pair.'}
        (out/'comparison-by-body.json').write_text(json.dumps(pairs,indent=2)+'\n')
    summary['analyzer_sha256']=sha(Path(__file__))
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps({k:v for k,v in summary.items() if k not in ('inputs','body_cost_groups')},indent=2))


if __name__=='__main__':main()
