#!/usr/bin/env python3
"""Summarize source wait completions under Genesis hardware cadence."""
import argparse
import collections
import csv
import json
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory',type=Path,nargs='?',default=Path('build/native-cadence'))
parser.add_argument('--output',type=Path)
args=parser.parse_args()
frames=list(csv.DictReader((args.directory/'frames.csv').open()))
irqs=list(csv.DictReader((args.directory/'irqs.csv').open()))
active=[r for r in irqs if r['manager']=='2' and r['scene']=='2']
def completion_group(rows):
    count=lambda name:sum(int(row[name],16).bit_count() for row in rows)
    missing=[int(row['irq']) for row in rows if not int(row['gameplay_wait_mask'],16)]
    streak=peak=0
    for row in rows:
        streak=streak+1 if not int(row['gameplay_wait_mask'],16) else 0
        peak=max(peak,streak)
    return {'irq_samples':len(rows),'gameplay_wait_completions':count('gameplay_wait_mask'),
            'manager_wait_completions':count('manager_wait_mask'),
            'attract_wait_completions':count('attract_wait_mask'),
            'nested_wait_completions':count('nested_wait_mask'),
            'without_gameplay_wait_irqs':missing,
            'longest_consecutive_samples_without_gameplay_wait':peak}
per_frame=collections.Counter(int(r['frame']) for r in irqs)
states=collections.Counter((int(r['manager']),int(r['scene'])) for r in frames)
result={'schema_version':1,
 'scope':'CPU-only original instructions on physical Genesis NTSC VDP cadence; no renderer/displayed latch/audio adapter',
 'clock':{'master_hz':53693175,'lines_per_frame':262,'master_cycles_per_line':3420,'master_cycles_per_frame':896040,'fps':53693175/896040},
 'reference_scope':'Original-address and relocated paths share Genesis timing; not CPS10MHz/59.63740458Hz speed equivalence',
 'sampling':'IRQ entry before full1554; saved task state1/sleep1/PC1756 and original128-byte stack return classify a wait that expires on this IRQ',
 'completion_return_pcs':{'gameplay':'932C','manager':'7FF2','attract':'810E','nested':['1EA2','1EC4']},
 'hardware_frames':len(frames),'irq_entries':len(irqs),'first_irq_frame':min(per_frame) if per_frame else None,
 'irqs_per_frame_distribution':dict(collections.Counter(per_frame.values())),
 'frames_without_irq':[int(r['frame']) for r in frames if not per_frame[int(r['frame'])]],
 'source_clock_sequence_matches':all(int(r['previous_source_clock'])==((int(r['irq'])-1)&255) for r in irqs),
 'all_irq_samples':completion_group(irqs),'active_irq_filter':{'manager':2,'scene':2},
 'active_irq_samples':completion_group(active),
 'states_after_hardware_frame':[{'manager':m,'scene':s,'frames':n} for (m,s),n in sorted(states.items())],
 'caveat':'A ready task is not automatically backlog: sourceA0 deliberately yields ready. Active IRQ samples without a gameplay wait expose incomplete/other work; continuous IRQ delivery alone is not gameplay FPS.'}
text=json.dumps(result,indent=2)+'\n'
if args.output:args.output.write_text(text)
else:print(text,end='')
