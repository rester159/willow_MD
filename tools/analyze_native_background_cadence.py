#!/usr/bin/env python3
"""Report combined cadence without mistaking source sleeps for game updates.

The refresh rate and main-task sleep rate are not the game update rate.
Original IRQ158C clears FFF9A1; main931E skips its sleep when that flag is zero.
Nested native preparation and
publication timings are descriptive spans, never added as independent costs.
Completed diagnostic runs retain every register failure and cannot be admitted.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path

HZ = 53693175
FRAME = 896040


def rows(path):
    return list(csv.DictReader(path.open()))


def stats(values):
    return {'count':len(values), 'min_ms':min(values)*1000/HZ,
            'mean_ms':sum(values)*1000/(HZ*len(values)), 'max_ms':max(values)*1000/HZ} if values else {'count':0}


def analyze(run, output):
    run, output = run.resolve(), output.resolve()
    names = ['observer-manifest.json','observer.log','render-summary.json',
             'background-summary.json','render-intervals.csv','render-events.csv',
             'background-events.csv','headroom-windows.csv','producer-epochs.csv']
    if (run/'register-failures.csv').exists():
        names.append('register-failures.csv')
    hashes = {name:hashlib.sha256((run/name).read_bytes()).hexdigest() for name in names}
    manifest = json.loads((run/'observer-manifest.json').read_text())
    render = json.loads((run/'render-summary.json').read_text())
    background = json.loads((run/'background-summary.json').read_text())
    assert render['paired_vdp_frames'] == manifest['run']['frames'], 'Run did not reach the requested frame count'
    reg_failures = rows(run/'register-failures.csv') if 'register-failures.csv' in names else []
    if manifest['run']['exit_code'] != 0:
        assert manifest.get('diagnostic_register_failure_collection') and reg_failures
        assert f'Diagnostic register failures: {len(reg_failures)}; NOT ADMITTED' in (run/'observer.log').read_text()
        assert manifest['run']['exit_code'] == 1, 'Diagnostic aborted before completion'
    intervals = [r for r in rows(run/'render-intervals.csv') if r['path']=='0' and r['active']=='1']
    assert len(intervals) == render['paths'][0]['active_completed_intervals'] and intervals
    sleeps = sum(int(r['source_main_commits']) for r in intervals)
    no_sleeps = sum(r['source_main_commits']=='0' for r in intervals)
    producers = rows(run/'producer-epochs.csv')
    preparation_ends = [r for r in rows(run/'render-events.csv')
                        if r['path']=='0' and r['event']=='prepare_complete']
    def distribution(events, field):
        counts = [sum(int(r['begin_master']) <= int(e[field]) < int(r['end_master'])
                      for e in events) for r in intervals]
        return {'total':sum(counts),'per_interval_histogram':{str(n):counts.count(n) for n in sorted(set(counts))},
                'rate_per_second':sum(counts)/len(intervals)*HZ/FRAME}
    spans = {'background_prepare':[], 'background_publish':[], 'object_and_background_prepare':[]}
    open_bg = {}
    for r in rows(run/'background-events.csv'):
        if r['path'] != '0':
            continue
        phase = int(r['phase'])
        if phase in (1,3):
            assert phase not in open_bg
            open_bg[phase] = r
        elif phase in (2,4,6):
            begin = open_bg.pop(3 if phase==4 else 1)
            if begin['active']=='1':
                spans['background_publish' if phase==4 else 'background_prepare'].append(int(r['master_cycles'])-int(begin['master_cycles']))
        elif phase == 5:
            open_bg.pop(3 if 3 in open_bg else 1)
    opened = None
    for r in rows(run/'render-events.csv'):
        if r['path'] != '0':
            continue
        if r['event'] == 'prepare_begin':
            assert opened is None
            opened = r
        elif r['event'] in ('prepare_complete','prepare_reject'):
            assert opened is not None
            if r['event']=='prepare_complete' and opened['manager']==opened['scene']=='2':
                spans['object_and_background_prepare'].append(int(r['absolute_master'])-int(opened['absolute_master']))
            opened = None
    active_windows = [r for r in rows(run/'headroom-windows.csv') if r['start_manager']==r['start_scene']=='2']
    completed = [r for r in active_windows if int(r['main_commit_master'])]
    work = [int(r['main_commit_master'])-int(r['irq_entry_master']) for r in completed]
    wait = [int(r['next_irq_master'])-int(r['main_commit_master']) for r in completed]
    failures = {}
    for prefix, path, fields in [
        ('source_and_objects',render['paths'][0],['active_cpu_deadline_failures','active_completed_intervals_without_publication','active_rejects','active_sat_deadline_failures','protocol_failures','acquisition_failures','source_ownership_failures','palette_guard_failures','stack_failures','retained_contract_failures','current_source_oracle_failures','vdp_stores_outside_publisher']),
        ('background',background['paths'][0],['missing_active_publications','active_rejects','deadline_failures','protocol_failures'])]:
        failures.update({f'{prefix}.{key}':path[key] for key in fields if path[key]})
    if reg_failures:
        failures['source_register_mismatches'] = len(reg_failures)
    # Existing observer names are preserved as raw evidence. The native
    # active_cpu_deadline_failures counter means no observed9328 sleep, not
    # proof that an entire game update was missing in that IRQ interval.
    old = failures.pop('source_and_objects.active_cpu_deadline_failures', 0)
    assert old == no_sleeps
    report = {'schema_version':2, 'scope':__doc__, 'run':str(run),
              'strict_state_parity_passed':manifest['run']['exit_code']==0 and not reg_failures,
              'combined_cadence_passed':False if failures else None, 'failures':failures,
              'cadence_admission_requires':'Separate source-body boundary evidence with exactly one completion in every required interval, plus all source and publication checks. No failures here alone is not admission.',
              'physical_refresh_hz':HZ/FRAME,'complete_active_intervals':len(intervals),
              'completed_source_main_sleeps':sleeps,'intervals_without_source_main_sleep':no_sleeps,
              'legacy_raw_active_cpu_deadline_failures':old,
              'source_main_sleeps_per_second':sleeps/len(intervals)*HZ/FRAME,
              'source_object_producer_epochs':distribution(producers,'master_cycles'),
              'native_preparation_completions':distribution(preparation_ends,'absolute_master'),
              'game_update_fps':'Not derived from main sleeps or producer epochs without a source-body boundary proof.',
              'sleep_metric_source_proof':'92D8 setsFFF9A1; IRQ158C clears it;931E/9322 bypass9324/9328 sleep when zero and branch directly92D8.',
              'background_active_publications':background['paths'][0]['active_publishes'],
              'object_active_publications':render['paths'][0]['active_publishes'],
              'nested_active_spans':{k:stats(v) for k,v in spans.items()},
              'intervals_with_observed_main_sleep_only':{'irq_to_source_wait':stats(work),'source_wait_to_next_irq':stats(wait)},
              'pixel_verification':'Separate independent oracle required; this report cannot certify pixels.',
              'input_sha256':hashes}
    assert all(hashlib.sha256((run/name).read_bytes()).hexdigest()==digest for name,digest in hashes.items())
    output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k != 'input_sha256'},indent=2))
    return False  # This report alone has no original-body boundary proof.


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a = p.parse_args()
    raise SystemExit(0 if analyze(a.run,a.output) else 1)
