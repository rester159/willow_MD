"""Analyze sourceDemoWord arguments captured by the CPU-only headless profiler."""
import argparse
import bisect
import collections
import csv
import re
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('prefix')
p.add_argument('symbols',type=Path)
p.add_argument('--data',type=Path,default=Path('native/intro_probe/src/demo_sequence_data.h'))
a=p.parse_args()
spans=[(int(at,16),int(count)) for at,count,_ in re.findall(r'\{(0x[0-9a-f]+)UL,(\d+),(\d+)\}',a.data.read_text())]
starts=[x[0] for x in spans]
assert spans and starts==sorted(starts)
rows=list(csv.DictReader(open(a.prefix+'-rom-reads.csv')))
entry=next(int(line.split()[0],16) for line in a.symbols.read_text().splitlines() if len(line.split())>=3 and line.split()[2]=='sourceDemoWord')
entry_hits=next(int(row['instructions']) for row in csv.DictReader(open(a.prefix+'-cycle-pcs.csv')) if int(row['pc'],16)==entry)
assert len(rows)==entry_hits, 'each sourceDemoWord entry must have exactly one argument record'
requests=[];ram=0;frequency=collections.Counter()
assert set(int(r['update']) for r in rows)==set(range(1,121))
for row in rows:
    at=int(row['address'],16);update=int(row['update'])
    if at&0xff0000==0xff0000:
        ram+=1
        continue
    assert at<0x100000 and not at&1, f'invalid ROM word {at:x}'
    index=bisect.bisect_right(starts,at)-1
    assert index>=0 and at<spans[index][0]+spans[index][1]*2, f'unretained read {at:x}'
    requests.append((update,index));frequency[index]+=1
print(f'{len(rows):,} checked reader calls: {len(requests):,} ROM / {ram:,} source-RAM words')
for ways in [1,2,4,8]:
    cache=[];previous=0;misses=0;hits=[0]*ways
    for update,index in requests:
        if update!=previous:cache=[-1]*ways;previous=update
        if index in cache:hits[cache.index(index)]+=1
        else:misses+=1;cache=[index]+cache[:-1]
    print(f'{ways} range IDs, reset each update, hits retain slot: {sum(hits)/len(requests):.2%} hits / {misses:,} misses / hits by slot {hits}')
print('Most-read original spans:')
for index,count in frequency.most_common(10):
    start,words=spans[index]
    print(f'{start:06x}..{start+words*2:06x}: {count:,} reads ({count/len(requests):.2%})')
