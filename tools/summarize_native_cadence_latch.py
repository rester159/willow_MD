#!/usr/bin/env python3
"""Summarize latch-v2 without hiding the transition's missed game update."""
import argparse
import csv
import json
from pathlib import Path
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('directory',type=Path,nargs='?',default=Path('build/native-cadence/latch-v2/route'))
p.add_argument('--output',type=Path)
a=p.parse_args()
result=json.loads(subprocess.check_output([sys.executable,str(Path(__file__).with_name('summarize_native_cadence.py')),str(a.directory)],text=True))
result['schema_version']=2
result['scope']='CPU-only continuous original source scheduler plus native raw scattered object latch at physical Genesis NTSC VBlank; no SAT conversion, renderer, audio adapter or production layout admission'
result['sampling']='Native IRQ observation before source1554, after masked native latch capture; game/task state remains the interrupted source state during capture. State1/sleep1/PC1756 plus original saved return classifies the preceding completed iteration.'
irqs=list(csv.DictReader((a.directory/'irqs.csv').open()))
active=[r for r in irqs if r['manager']=='2' and r['scene']=='2']
missing=[r for r in active if not int(r['gameplay_wait_mask'],16)]
result['active_missing_details']=[{'hardware_frame':int(r['frame']),'irq':int(r['irq']),'interrupted_source_pc':r['interrupted_pc'],'saved_sr':r['saved_sr'],'ready_mask':r['ready_mask']} for r in missing]
last_missing=max((int(r['irq']) for r in missing),default=0)
settled=[r for r in active if int(r['irq'])>last_missing]
result['after_last_missing_active_irq']={'definition':'Descriptive coverage only: all active samples strictly after the last missing active gameplay wait. The all-active totals above retain every transition miss.','last_missing_irq':last_missing or None,'irq_samples':len(settled),'gameplay_wait_completions':sum(int(r['gameplay_wait_mask'],16).bit_count() for r in settled),'first_hardware_frame':int(settled[0]['frame']) if settled else None,'last_hardware_frame':int(settled[-1]['frame']) if settled else None}
result['latch']={'boundary':'PRE-source1554, selected by previous hardware800100 port; never FF8022 next selector','bytes':2048,'chunks':32,'chunk_bytes':64,'first_offset':0x2660,'stride':128,'last_exclusive_offset':0x3620,'ownership':'Conditional source-hole audit; every original source read/write into any chunk after first capture aborts this test','all_other_ram':'Compared against independent original RAM; only exact native-owned chunks compare to the independent pre-IRQ object snapshot'}
result['capture_cost_closed_probe']=json.loads((a.directory.parent/'capture-cost.json').read_text())
text=json.dumps(result,indent=2)+'\n'
if a.output:a.output.write_text(text)
else:print(text,end='')
