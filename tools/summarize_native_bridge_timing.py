#!/usr/bin/env python3
"""Summarize native bridge/original CPU costs by state after the full update."""
import argparse
import collections
import csv
import json
from pathlib import Path
import statistics
MASTER_HZ=53_693_175
FRAME_MASTER_CYCLES=896_040

def group(rows):
    result={'updates':len(rows)}
    for field in ('source_cycles','bridge_cycles'):
        values=[int(row[field]) for row in rows]
        peak=max(range(len(values)),key=values.__getitem__) if values else None
        result[field.removesuffix('_cycles')]={
            'mean_master_cycles':statistics.mean(values) if values else None,
            'mean_cpu_ms':statistics.mean(values)*1000/MASTER_HZ if values else None,
            'peak_master_cycles':max(values) if values else None,
            'peak_cpu_ms':max(values)*1000/MASTER_HZ if values else None,
            'over_budget_updates':sum(value>FRAME_MASTER_CYCLES for value in values),
            'peak_pass':int(rows[peak]['pass']) if peak is not None else None,
        }
    overhead=[int(row['bridge_cycles'])-int(row['source_cycles']) for row in rows]
    result['bridge_overhead_mean_cpu_ms']=statistics.mean(overhead)*1000/MASTER_HZ if overhead else None
    return result

def summarize(path):
    rows=list(csv.DictReader(path.open()))
    active=[row for row in rows if row['manager']=='2' and row['scene']=='2']
    states=collections.defaultdict(list)
    for row in rows:states[(int(row['manager']),int(row['scene']))].append(row)
    return {
        'schema_version':1,
        'measurement':'CPU-only native 68000 instructions; excludes rendering/audio and platform interrupt integration',
        'clock_master_hz':MASTER_HZ,
        'frame_budget_master_cycles':FRAME_MASTER_CYCLES,
        'state_sampling':'manager/scene after main, scheduled-task pass and wake have completed',
        'active_filter':{'manager':2,'scene':2},
        'controller':'pattern advances only while pre-update manager/scene are 2/2; distinct from prior post-opening-tick C fixture',
        'all_updates':group(rows),
        'active_updates':group(active),
        'other_updates':group([row for row in rows if row['manager']!='2' or row['scene']!='2']),
        'by_post_update_state':[{'manager':key[0],'scene':key[1],**group(values)} for key,values in sorted(states.items())],
        'over_budget_rows':[row for row in rows if int(row['bridge_cycles'])>FRAME_MASTER_CYCLES],
    }

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv',type=Path,nargs='?',default=Path('build/native-frame-bridge/timing.csv'))
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    result=json.dumps(summarize(args.csv),indent=2)+'\n'
    if args.output:args.output.write_text(result)
    else:print(result,end='')
