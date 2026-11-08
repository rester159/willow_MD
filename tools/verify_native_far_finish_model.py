#!/usr/bin/env python3
"""Closed caller-policy proof; never edits or admits the live renderer."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def extract(source, name):
    # Find the declaration containing this name, not a nearby previous one.
    pos = source.index(name + '(')
    start = source.rfind('\nstatic ', 0, pos) + 1
    brace = source.index('{', pos)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/native-far-finish-audit/model-v1')
    parser.add_argument('--baseline-runtime', type=Path,
                        default=ROOT/'build/native-far-finish-audit/model-v1/inputs/tools/native_render_background.inc')
    args = parser.parse_args()
    out = args.output.resolve()
    assert not out.exists(), 'Preserve prior evidence; choose a fresh output'
    out.mkdir(parents=True)
    baseline = args.baseline_runtime.resolve()
    inputs = [ROOT/'tools/native_render_background.inc', baseline, ROOT/'src/willow_native_far_cache.h',
              ROOT/'tools/test_native_far_finish_model.c', Path(__file__).resolve()]
    resource = ROOT/'build/native-background-far-stream/prepared-v1'
    catalog = ROOT/'build/native-background-catalog/forest-277-v1'
    residency = ROOT/'build/native-background-residency/forest-277-v1'
    for directory in (resource, catalog, residency):
        manifest = json.loads((directory/'manifest.json').read_text())
        for name, digest in manifest['outputs'].items():
            assert sha(directory/name) == digest, name
        inputs.append(directory/'manifest.json')
    inputs += [resource/'far-stream.bin', resource/'far_stream_imports.h', catalog/'catalog.bin',
               residency/'physical-tiles.bin']
    before = {str(p): sha(p) for p in inputs}
    for path in inputs:
        target = out/'inputs'/path.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
    source = (out/'inputs'/baseline.relative_to(ROOT)).read_text()
    names = ['bgFirstX', 'bgFirstY', 'bgRows', 'bgFarProtectWord', 'bgSet', 'bgFarContext',
             'bgFarRetry', 'bgFarFinish']
    extracted = ''.join(extract(source, name) for name in names)
    (out/'exact-caller.inc').write_text(extracted)
    candidate = (out/'inputs/tools/native_render_background.inc').read_text()
    candidate_finish = extract(candidate, 'bgFarFinish').replace('bgFarFinish(', 'wrappedBgFarFinish(', 1)
    if 'bgFarClearRow(' in candidate_finish:
        candidate_finish = extract(candidate, 'bgFarClearRow') + candidate_finish
    (out/'wrapped-caller.inc').write_text(candidate_finish)
    commands = [['clang', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                 '-Wno-unused-function', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                 '-I'+str(out), '-I'+str(out/'inputs/src'), '-I'+str(out/'inputs/build/native-background-far-stream/prepared-v1'),
                 str(out/'inputs/tools/test_native_far_finish_model.c'), '-o', str(out/'model')],
                [str(out/'model'), str(out/'inputs/build/native-background-far-stream/prepared-v1/far-stream.bin'),
                 str(out/'inputs/build/native-background-catalog/forest-277-v1/catalog.bin'),
                 str(out/'inputs/build/native-background-residency/forest-277-v1/physical-tiles.bin')]]
    subprocess.run(commands[0], cwd=ROOT, check=True)
    with (out/'result.json').open('w') as stream:
        subprocess.run(commands[1], cwd=ROOT, stdout=stream, check=True)
    result = json.loads((out/'result.json').read_text())
    evidence = {
        'scope': __doc__, 'status': 'pass', 'runtime_admitted': False,
        'actual_68000_executed': False, 'result': result,
        'recommended_policy': 'Exact current wrapped-run Finish; conservative no-scan proposal is rejected by repeated-miss progress counterexample.',
        'exact_extracted_caller_functions': names,
        'resource_scope': 'Frozen WBF1 prepared-v1 / WBC1 forest-277-v1; all248 far keys and3968 refs',
        'limits': ['Host sanitizer proof of a proposed caller policy with the actual cache header, not a live source route or native timing result.',
                   'Source keys, opacity decisions and coherent view acquisition are explicit proof inputs; existing live guards must remain.',
                   'Full producer/IRQ interleavings, actual VDP commands/deadlines and future shared near-cache ownership need separate integration proof.'],
        'commands': commands, 'inputs': before,
        'live_inputs_still_match': {str(p): sha(p) == before[str(p)] for p in inputs},
        'outputs': {str(p.relative_to(out)): sha(p) for p in out.rglob('*') if p.is_file()}}
    (out/'evidence.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
