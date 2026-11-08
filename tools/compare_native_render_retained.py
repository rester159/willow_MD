#!/usr/bin/env python3
"""Compare measured continuous native rendering variants, with bounded scope."""
import argparse, csv, hashlib, json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
HZ=53693175
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def rows(path):
    with path.open()as f:return list(csv.DictReader(f))
def stats(values):
    assert values
    return {'count':len(values),'min_ms':min(values)*1000/HZ,'mean_ms':sum(values)*1000/HZ/len(values),'max_ms':max(values)*1000/HZ}
def analyze(run):
    summary=json.loads((run/'render-summary.json').read_text())
    for p in summary['paths']:
        assert p['active_irqs']>0 and p['active_publishes']==p['active_irqs']
        for name in ['active_completed_intervals_without_commit','active_completed_intervals_without_publication','active_cpu_deadline_failures','active_physical_irq_gaps','active_rejects','protocol_failures','acquisition_failures','active_sat_deadline_failures','active_mode_or_transfer_failures','source_ownership_failures','palette_guard_failures','stack_failures','final_prepare_open','final_publish_open','final_native_irq_depth']:
            assert p[name]==0,(run,name,p[name])
        for name in ['retained_contract_failures','current_source_oracle_failures','vdp_stores_outside_publisher']:
            assert p.get(name,0)==0,(run,name,p.get(name))
    pending={};prepares=[];publishes=[]
    for r in rows(run/'render-events.csv'):
        if r['path']!='0':continue
        event=r['event'];kind=event.split('_')[0]
        if event in ['prepare_begin','publish_begin']:pending[kind]=int(r['absolute_master'])
        elif event in ['prepare_complete','publish_complete']:
            begin=pending.pop(kind)
            if r['manager']==r['scene']=='2':
                (prepares if kind=='prepare'else publishes).append(int(r['absolute_master'])-begin)
        elif event in ['prepare_reject','publish_reject']:pending.pop(kind,None)
    intervals=[r for r in rows(run/'headroom-windows.csv')if r['start_manager']==r['start_scene']=='2']
    assert all(int(r['main_commit_master'])>0 for r in intervals)
    updates=[int(r['main_commit_master'])-int(r['irq_entry_master'])for r in intervals]
    slack=[int(r['next_irq_master'])-int(r['main_commit_master'])for r in intervals]
    transfers=[r for r in rows(run/'render-transfers.csv')if r['path']=='0'and r['active']=='1']
    ring=(run/'irq-ring.bin').read_bytes()
    assert int.from_bytes(ring[0x578:0x580],'big')==0,'Native renderer reentry rejected'
    words=sum(int(r['data_writes'])for r in transfers)
    return {'physical_frames':summary['paired_vdp_frames'],'active_publications':summary['paths'][0]['active_publishes'],
            'completed_active_intervals':len(intervals),'update_hz':HZ/896040,'active_prepare':stats(prepares),'active_publish':stats(publishes),
            'irq_to_source_main_sleep':stats(updates),'source_main_sleep_to_next_irq_upper_bound':stats(slack),
            'sprite_data_words':words,'sprite_control_word_writes':sum(int(r['control_writes'])for r in transfers),
            'unchanged_zero_transfer_frames':sum(int(r['data_writes'])==0 for r in transfers),
            'minimum_sat_fetch_headroom_ms':min(int(r['sat_fetch_deadline'])-max(int(r['end_master']),int(r['retire_upper_master']))for r in transfers)*1000/HZ,
            'all_state_reuse_count_including_pre_active':int.from_bytes(ring[0x584:0x588],'big'),
            'all_geometry_build_count_including_pre_active':int.from_bytes(ring[0x588:0x58c],'big'),
            'failures':0}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline',type=Path,default=ROOT/'build/native-render-cadence/checkpoint-v1')
    p.add_argument('--candidate',type=Path,required=True)
    p.add_argument('--verification',type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'build/native-render-cadence/retained-comparison.json')
    a=p.parse_args()
    checkpoint=json.loads((a.baseline/'checkpoint.json').read_text())
    for name,digest in checkpoint['files'].items():assert sha(a.baseline/name)==digest,name
    verification=json.loads(a.verification.read_text())
    # The independent source/SAT verifier already checked these exact outputs;
    # reject a changed report dependency before using its result.
    for name,digest in verification['inputs'].items():assert sha(a.candidate/name)==digest,name
    for name,digest in verification['sources'].items():assert sha(ROOT/name)==digest,name
    before,after=analyze(a.baseline/'observer'),analyze(a.candidate)
    assert before['active_publications']==after['active_publications'],'Use equal observed gameplay lengths for this comparison'
    result={'scope':'Continuous source plus native object rendering only. Preconverted resident patterns and resolved templates in both variants; candidate adds exact-input geometry retention and retained sprite-table publication. Content-stream mode skips unchanged tables and uploads the whole used table when content changes; entry-diff mode is a separate experiment. No full backgrounds, Window, audio, all-stage or CPS-clock admission.',
            'timing_note':'Preparation spans may include interrupt work. Do not sum phase timings. Source main sleep does not mean idle; source service/scheduler work continues.',
            'baseline':before,'retained':after,
            'prepare_mean_reduction_percent':100*(1-after['active_prepare']['mean_ms']/before['active_prepare']['mean_ms']),
            'sprite_data_reduction_percent':100*(1-after['sprite_data_words']/before['sprite_data_words']),
            'irq_to_main_sleep_mean_reduction_percent':100*(1-after['irq_to_source_main_sleep']['mean_ms']/before['irq_to_source_main_sleep']['mean_ms']),
            'hashes':{str(a.baseline/'checkpoint.json'):sha(a.baseline/'checkpoint.json'),str(a.verification):sha(a.verification),str(a.candidate/'observer-manifest.json'):sha(a.candidate/'observer-manifest.json'),str(Path(__file__).resolve()):sha(Path(__file__).resolve())}}
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
