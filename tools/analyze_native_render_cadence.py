#!/usr/bin/env python3
"""Summarize direct continuous work boundaries without adding isolated costs."""
import argparse
import csv
import hashlib
import json
from pathlib import Path

TICKS_PER_MS=53693.175

def rows(path):
    with path.open() as stream:return list(csv.DictReader(stream))

def stats(values):
    return {'count':len(values),'min_ms':min(values)/TICKS_PER_MS,
            'mean_ms':sum(values)/len(values)/TICKS_PER_MS,
            'max_ms':max(values)/TICKS_PER_MS} if values else {'count':0}

def analyze(run,output):
    names=['render-summary.json','render-events.csv','render-transfers.csv','headroom-windows.csv','observer-manifest.json']
    hashes={name:hashlib.sha256((run/name).read_bytes()).hexdigest() for name in names}
    manifest=json.loads((run/'observer-manifest.json').read_text())
    assert manifest['run']['exit_code']==0,'Do not summarize partial failed execution as a completed gate'
    summary=json.loads((run/'render-summary.json').read_text())
    preparations={'active':[],'preactive':[],'active_reused':[],'active_geometry':[]}
    opened=None
    for row in rows(run/'render-events.csv'):
        if row['path']!='0':continue
        if row['event']=='prepare_begin':
            assert opened is None
            opened=row
        elif row['event'] in ('prepare_complete','prepare_reject'):
            assert opened is not None
            if row['event']=='prepare_complete' and row['status']=='0':
                elapsed=int(row['absolute_master'])-int(opened['absolute_master'])
                active=row['manager']==row['scene']=='2'
                preparations['active' if active else 'preactive'].append(elapsed)
                if active and 'source_reuses' in row:
                    reused=int(row['source_reuses'])-int(opened['source_reuses'])
                    built=int(row['geometry_builds'])-int(opened['geometry_builds'])
                    assert reused in (0,1) and built in (0,1) and reused+built<=1
                    if reused:preparations['active_reused'].append(elapsed)
                    if built:preparations['active_geometry'].append(elapsed)
            opened=None
    transfers=[row for row in rows(run/'render-transfers.csv') if row['path']=='0' and row['active']=='1']
    publications={'all':[],'zero_transfer':[],'changed':[]}
    headroom=[]
    for row in transfers:
        elapsed=int(row['end_master'])-int(row['begin_master'])
        publications['all'].append(elapsed)
        publications['zero_transfer' if row['data_writes']=='0' else 'changed'].append(elapsed)
        headroom.append(int(row['sat_fetch_deadline'])-max(int(row['end_master']),int(row['retire_upper_master'])))
    windows=rows(run/'headroom-windows.csv')
    active=[row for row in windows if row['start_manager']==row['start_scene']=='2' and int(row['main_commit_master'])]
    work=[int(row['main_commit_master'])-int(row['irq_entry_master']) for row in active]
    wait=[int(row['next_irq_master'])-int(row['main_commit_master']) for row in active]
    transition=[row for row in windows if row['start_manager']!= '2' or row['start_scene']!='2']
    transition=[row for row in transition if row['end_manager']==row['end_scene']=='2' and int(row['main_commit_master'])]
    result={'schema_version':1,'scope':'Actual paired source plus native object rendering under Genesis cadence. Direct nested work spans are not summed. Source sleep-to-next-IRQ is an upper bound on free time because service/scheduler work remains. No BG/Window, synthesis, whole-game or CPS-clock equivalence claim.',
            'run':str(run.resolve()),'summary':summary['paths'][0],
            'preparation':{key:stats(value) for key,value in preparations.items()},
            'publication':{key:stats(value) for key,value in publications.items()},
            'irq_entry_to_source_main_sleep':stats(work),'source_sleep_to_next_irq_marker':stats(wait),
            'fifo_aware_sat_fetch_headroom':stats(headroom),
            'first_transition_windows':transition,'input_sha256':hashes}
    output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({key:value for key,value in result.items() if key not in ('summary','first_transition_windows','input_sha256')},indent=2))
    return result

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();analyze(args.run,args.output)
