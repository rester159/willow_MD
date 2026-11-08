#!/usr/bin/env python3
"""Audit recorded body-call and coherent stale-view provenance independently."""
import argparse
from collections import Counter
import copy
import csv
import hashlib
import json
from pathlib import Path
import re

sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=lambda p:list(csv.DictReader(p.open()))


def views(events,load_sites,end_pc,marker):
    states={};counts=Counter()
    for r in events:
        event=r['event'];path=int(r['path']);pc=int(r['pc']);value=int(r['value']);at=int(r['address'])
        vals=tuple(int(r[f'v{i}']) for i in range(4));current=tuple(int(r[f'current{i}'])for i in range(4))
        assert int(r['bad'])==0 and event in ('read','current_phase1','coherent_stale_phase1','irq_after_complete_read','original_irq_staging_write')
        if event=='read':
            n=int(r['reads'])-1
            if n==0:assert path not in states;states[path]={'tuple':current,'reads':[],'writes':[],'irq':False}
            s=states[path];assert n==len(s['reads']) and pc==load_sites[n] and at==0xf446+2*n and value==s['tuple'][n] and current==s['tuple'] and not s['irq']
            s['reads'].append(value)
        elif event=='irq_after_complete_read':
            s=states[path];assert len(s['reads'])==4 and not s['irq'] and value==end_pc and current==s['tuple'];s['irq']=True
        elif event=='original_irq_staging_write':
            s=states[path];assert s['irq'] and len(s['writes'])<4
            n=(0,2,1,3)[len(s['writes'])];assert at==0xf446+n*2 and pc==(0x19a2,0x1a20,0x19b4,0x1a34)[n]
            s['writes'].append((n,value))
        else:
            s=states.pop(path);assert pc==marker and tuple(s['reads'])==vals==s['tuple']
            if s['irq']:
                assert len(s['writes'])==4
                updated=list(vals)
                for n,v in s['writes']:updated[n]=v
                assert current==tuple(updated)
            else:assert not s['writes'] and current==vals
            assert (event=='coherent_stale_phase1')==(current!=vals)
        counts[event]+=1
    assert not states
    return dict(counts)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    run=a.run.resolve();out=a.output.resolve();assert not out.exists();pins={}
    def pin(path):pins[str(path)]=sha(path);return path.read_text()
    manifest=json.loads(pin(run/'observer-manifest.json'))
    assert manifest['run']['exit_code']==3
    for n,d in manifest['inputs'].items():assert sha(Path(n))==d
    site_text=pin(run/'forest-view-acquisition-sites.h')
    load=[int(n,16)for n in re.search(r'forestViewLoadNextPC\[\]=\{([^}]+)',site_text)[1].replace('u','').split(',')]
    end_pc=int(re.search(r'FOREST_VIEW_ACQUISITION_END (0x[0-9a-f]+)',site_text)[1],16)
    marker=int(re.search(r'FOREST_VIEW_MARKER_NEXT (0x[0-9a-f]+)',site_text)[1],16)
    pin(run/'view-acquisition-events.csv');events=rows(run/'view-acquisition-events.csv');counts=views(events,load,end_pc,marker)
    rejects=[]
    mutations=[('wrong_read_value','read','value'),('wrong_read_pc','read','pc'),('wrong_read_address','read','address'),
               ('wrong_irq_boundary','irq_after_complete_read','value'),('wrong_original_writer','original_irq_staging_write','pc'),
               ('wrong_descriptor_word','coherent_stale_phase1','v0'),('wrong_current_tuple','coherent_stale_phase1','current0')]
    for label,event,key in mutations:
        changed=copy.deepcopy(events);row=next(r for r in changed if r['event']==event);row[key]=str(int(row[key])+1)
        try:views(changed,load,end_pc,marker)
        except (AssertionError,KeyError,IndexError):rejects.append(label)
        else:raise AssertionError(('Forged acquisition was accepted',label))
    pin(run/'main-body-marker-events.csv');pin(run/'main-loop-events.csv')
    markers=rows(run/'main-body-marker-events.csv');loops=rows(run/'main-loop-events.csv');hardware=[];pending=[]
    for r in markers:
        event=r['event'];pc=int(r['pc']);sp=int(r['sp']);at=int(r['address']);value=int(r['value'])
        if event.startswith('irq_'):
            assert event==('irq_pc_high','irq_pc_low','irq_old_user_sr','irq_frame_confirmed')[len(pending)]
            pending.append(r)
            if len(pending)==4:
                first=pending[0];base=int(first['sp']);savedpc=int(first['pc']);body=first['body'];assert savedpc in(0x92f2,0x92f6)
                assert [(int(e['sp']),int(e['address']),int(e['value']))for e in pending]==[(base,base&65535,savedpc>>16),(base,(base+2)&65535,savedpc&65535),(base-2,(base-2)&65535,int(pending[2]['value'])),(base-4,(base-4)&65535,int(pending[3]['value']))]
                assert not int(pending[2]['value'])&~31 and int(pending[3]['value'])&0x2700==0x2600
                assert all(e['body']==body and e['open']==first['open']for e in pending)
                hardware.append(pending);pending=[]
            continue
        assert not pending and event in('body_call_return','body_complete_call_return')
        top=int(r['task_stack_top']);assert int(r['task'])==0xff0060 and top-128<=sp and sp+4<=top and not int(r['sr'])&0x2000 and at==((sp+2)&65535)
        begin=event=='body_call_return';assert pc==value==(0x92f2 if begin else 0x92f6)
        name='body_begin'if begin else'body_complete';body=int(r['body'])+begin
        assert len([e for e in loops if e['event']==name and int(e['body'])==body and e['master_cycles']==r['master_cycles']])==1
    assert not pending
    pins[str(Path(__file__).resolve())]=sha(Path(__file__))
    assert all(sha(Path(n))==d for n,d in pins.items())
    result={'scope':__doc__,'view_events':counts,'forged_acquisition_traces_rejected':rejects,
            'complete_hardware_body_marker_exceptions':len(hardware),'hardware_exception_sequences':hardware,
            'genuine_source_calls':sum(r['event']=='body_call_return'for r in markers),
            'genuine_source_completion_markers':sum(r['event']=='body_complete_call_return'for r in markers),
            'all_target_memory_context_publication_checks_retained':True,'source_parity_admitted':False,'inputs':pins}
    out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k not in('inputs','hardware_exception_sequences')},indent=2))


if __name__=='__main__':main()
