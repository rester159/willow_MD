#!/usr/bin/env python3
"""Verify live background pixels from a deliberately stopped paired route prefix.

The coverage stop and all register failures remain failures. This wrapper only
admits complete publication snapshots before a strictly completed paired frame;
it does not manufacture final campaign summaries or waive pixel checks.
"""
import argparse
import csv
import json
from pathlib import Path
import types

import verify_native_background_publication as base
from test_native_render_cadence import replace

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run', type=Path, required=True)
    p.add_argument('--assets', type=Path, required=True)
    p.add_argument('--canonical', type=Path, default=ROOT/'build/native-route-background-audit/checkpoint-v1/canonical-v1')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--require-after-tick', type=int, default=0)
    a = p.parse_args()
    run = a.run.resolve()
    manifest = json.loads((run/'observer-manifest.json').read_text())
    stop = json.loads((run/'coverage-stop.json').read_text())
    rows = list(csv.DictReader((run/'frames.csv').open()))
    pubs = list(csv.DictReader((run/'background-publications.csv').open()))
    assert manifest['run']['exit_code'] == 3 and manifest['forest_extension']['target_products_unchanged']
    assert stop['all_other_per_frame_source_and_vdp_checks_completed'] and not stop['coverage_complete']
    assert len(rows) == stop['paired_frames_completed'] and pubs
    assert all(int(r['frame']) < len(rows) for r in pubs)
    report_path = ROOT/'tools/verify_native_background_publication.py'
    text = report_path.read_text()
    start = text.index('    if not strict_parity:\n')
    end = text.index('    for name, digest in manifest[\'inputs\'].items():\n', start)
    text = text[:start] + '''    require(manifest['run']['exit_code'] == 3, 'Not a confirmed paired coverage stop')
    stopped = json.loads(pin(run/'coverage-stop.json'))
    require(stopped['all_other_per_frame_source_and_vdp_checks_completed'] and not stopped['coverage_complete'], 'Invalid paired coverage stop')
    pin(run/'register-failures.csv')
    register_failures = list(csv.DictReader((run/'register-failures.csv').open()))
    require(len(register_failures) == stopped['source_register_failures_retained'], 'Lost register failures')
    pin(run/'frames.csv')
    paired = list(csv.DictReader((run/'frames.csv').open()))
    require(len(paired) == stopped['paired_frames_completed'], 'Paired prefix is incomplete')
''' + text[end:]
    text = replace(text, "require(complete_register_diagnostic and not strict_parity, 'Missing complete publication layout')", "require(not strict_parity, 'Coverage prefix must remain unadmitted')")
    text = replace(text, "json.loads(pin(run/'background-summary.json'))['samples']", "len(list(csv.DictReader((run/'background-publications.csv').open())))")
    text = replace(text, "json.loads(pin(run/'background-summary.json'))['paths'][0]['missing_active_publications']", "None")
    module = types.ModuleType('forest_prefix_pixels')
    module.__file__ = str(report_path)
    exec(compile(text, str(report_path), 'exec'), module.__dict__)
    report = module.verify(run, a.assets, a.canonical, a.output, True)
    output = a.output.resolve()
    frozen = output/'prefix-verifier.py'
    frozen.write_text(text)
    frames = json.loads((output/'frames.json').read_text())
    ticks = {int(r['frame']): int(r['route_tick']) for r in rows}
    after = [r for r in frames if ticks[r['frame']] > a.require_after_tick]
    report.update(scope=__doc__, paired_prefix_frames=len(rows), coverage_complete=False,
                  stopped_on_unsupported_source_resource=True, target_admitted=False,
                  required_input_boundary=a.require_after_tick, checked_publications_after_boundary=len(after),
                  required_boundary_verified=bool(after),
                  last_checked_active_input_tick=max(ticks[r['frame']] for r in frames))
    for path in (Path(__file__), frozen, run/'coverage-stop.json', run/'frames.csv'):
        report['inputs'][str(path.resolve())] = base.sha(path)
    assert all(base.sha(Path(n)) == d for n,d in report['inputs'].items())
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k != 'inputs'}, indent=2))
    assert after, 'No exact current-source publication beyond requested ordinary-input boundary'
    raise SystemExit(1)  # Evidence is a partial route with retained source failures.


if __name__ == '__main__':
    main()
