#!/usr/bin/env python3
"""Read-only source clear/capture analysis; not a target speedup or clock model."""
import csv,hashlib,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/native-object-publication/checkpoint-visual-state-v1'
OUT=ROOT/'build/native-scroll1-audit';OUT.mkdir(exist_ok=True)
rows=lambda n:list(csv.DictReader((BASE/n).open()))
index=rows('visual-contexts.csv');writes=rows('scroll1-writes.csv');data=(BASE/'visual-contexts.bin').read_bytes()
assert len(writes)==86238 and len(index)==1797 and len(data)==1797*90624
rom=(ROOT/'build/reference_maincpu.bin').read_bytes()
assert hashlib.sha256(rom).hexdigest()=='0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1'
assert rom[0x65fe:0x6614].hex()=='30c130c230c130c230c130c230c130c251c8ffee4e75'
assert rom[0x58e2:0x58ec].hex()=='30c130c251c8fffa4e75'
# Logical representation experiment. The cursor advances ONLY on each observed
# source write. Removing source instructions would remove that timing source;
# this model does not supply an independently admitted target clock/scheduler.
default=(0,0);sparse={};pending=None;clears=[];max_sparse=0;read_checks=0;wi=0
families={0x58e4:0x58e2,0x58e6:0x58e2,**{pc:0x65fe for pc in range(0x6600,0x6610,2)}}
def logical(word):
 if pending:
  update=pending['updates'].get(word)
  if update and (word>=pending['cursor'] or word<update[1]):return update[0]
  if word<pending['cursor']:return pending['defaults'][word&1]
 return sparse.get(word,default[word&1])
def write(row):
 global pending,default,sparse,max_sparse
 pc=int(row['callback_pc'],16);at=int(row['address'],16);value=int(row['value'],16);word=(at-0x910000)//2
 assert int(row['width'])==2 and 0<=word<8192
 if pc in families:
  if not pending:
   assert word==0 and pc in(0x58e4,0x6600)
   family=families[pc]
   assert int(row['d0'],16)&65535==(4095 if family==0x58e2 else 1023)
   pending={'routine':f'{family:04x}','start_frame':int(row['frame']),'start_master':int(row['master_cycles']),'cursor':0,'defaults':(int(row['d1'],16)&65535,int(row['d2'],16)&65535),'updates':{},'interrupted_captures':[],'interleaved_nonclear_writes':0}
   assert pending['defaults']==(0x7020,0)
  assert f'{families[pc]:04x}'==pending['routine'] and word==pending['cursor']
  assert value==pending['defaults'][word&1]
  assert int(row['a0'],16)&0xffffff==at+2
  if pending['routine']=='58e2':
   assert pc==(0x58e4 if word%2==0 else 0x58e6)
   assert int(row['d0'],16)&65535==4095-word//2
  else:
   assert pc==0x6600+2*(word%8)
   assert int(row['d0'],16)&65535==1023-word//8
  pending['cursor']+=1
  if pending['cursor']==8192:
   default=pending['defaults']
   sparse={i:v for i,(v,cursor) in pending['updates'].items() if i<cursor and v!=default[i&1]}
   pending.update(end_frame=int(row['frame']),end_master=int(row['master_cycles']),elapsed_first_to_last_ms=(int(row['master_cycles'])-pending['start_master'])/53693.175)
   finished={k:v for k,v in pending.items() if k not in('updates','defaults')};finished['defaults']=list(default);clears.append(finished);pending=None
 else:
  if pending:
   pending['updates'][word]=(value,pending['cursor']);pending['interleaved_nonclear_writes']+=1
  elif value==default[word&1]:sparse.pop(word,None)
  else:sparse[word]=value
 max_sparse=max(max_sparse,len(sparse)+(len(pending['updates']) if pending else 0))
for capture in index:
 n=int(capture['capture'])
 while wi<len(writes) and int(writes[wi]['previous_capture'])<n:
  write(writes[wi]);wi+=1
 snapshot=data[(n-1)*90624+74240:n*90624]
 logical_words=[logical(i) for i in range(8192)];read_checks+=8192
 assert struct.pack('>8192H',*logical_words)==snapshot,(n,pending)
 if pending:
  mismatches=[i for i,v in enumerate(logical_words) if v!=pending['defaults'][i&1]]
  pending['interrupted_captures'].append({'capture':n,'frame':int(capture['frame']),'manager':int(capture['manager']),'scene':int(capture['scene']),'clear_words_completed':pending['cursor'],'atomic_default_different_words':len(mismatches),'first_different_byte_offset':hex(mismatches[0]*2) if mismatches else None,'last_different_byte_offset':hex(mismatches[-1]*2) if mismatches else None,'source_scroll1_sha256':hashlib.sha256(snapshot).hexdigest(),'master_cycles':int(capture['master_cycles'])})
while wi<len(writes):write(writes[wi]);wi+=1
assert not pending and len(clears)==10
result={'schema_version':1,'scope':'Read-only logical representation and exact original clear-interrupt snapshots. Cursor is driven by actual original source write events; no native timing replacement, speedup, atomic clear or CPS10MHz admission.','source_rom_sha256':hashlib.sha256(rom).hexdigest(),'clear_epoch_count':len(clears),'clear_word_writes':8192*len(clears),'other_word_writes':len(writes)-8192*len(clears),'snapshots':len(index),'logical_word_read_checks':read_checks,'max_sparse_word_entries_on_this_route':max_sparse,'clears':clears,'clock':'Genesis53,693,175 masterHz; source runs unchanged on7,670,453.571Hz68000. Not original CPS10MHz/59.63740458Hz.','files':{n:hashlib.sha256((BASE/n).read_bytes()).hexdigest() for n in('visual-contexts.csv','visual-contexts.bin','scroll1-writes.csv')},'source_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
(OUT/'clear-evidence.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in('clear_epoch_count','clear_word_writes','other_word_writes','snapshots','logical_word_read_checks','max_sparse_word_entries_on_this_route')},indent=2))
for c in clears:print(c['routine'],c['start_frame'],c['end_frame'],c['elapsed_first_to_last_ms'],c['interrupted_captures'])
