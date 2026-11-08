#!/usr/bin/env python3
"""Frozen old/current Request equivalence on actual immutable far resources."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    assert not out.exists(), 'Preserve previous evidence; choose a fresh output'
    out.mkdir(parents=True)
    current = ROOT/'src/willow_native_far_cache.h'
    test = ROOT/'tools/test_native_far_request_equivalence.c'
    resource = ROOT/'build/native-background-far-stream/prepared-v1/far-stream.bin'
    catalog = ROOT/'build/native-background-catalog/forest-277-v1/catalog.bin'
    inputs = [a.baseline.resolve(), current, test, Path(__file__).resolve(), resource, catalog]
    before = {str(v): digest(v) for v in inputs}
    old = a.baseline.read_text()
    start = old.index('WILLOW_FAR_FN uint16_t willowFarCacheRequest(')
    opening = old.index('{', start)
    depth, end = 1, opening+1
    while depth:
        depth += (old[end] == '{') - (old[end] == '}')
        end += 1
    reference = old[start:end].replace('WILLOW_FAR_FN', 'static').replace('willowFarCacheRequest(', 'referenceFarCacheRequest(', 1)
    (out/'reference-request.h').write_text(reference+'\n')
    (out/'baseline.h').write_bytes(a.baseline.read_bytes())
    (out/'willow_native_far_cache.h').write_bytes(current.read_bytes())
    (out/test.name).write_bytes(test.read_bytes())
    cmd = ['clang', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
           '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-I'+str(out),
           str(out/test.name), '-o', str(out/'test')]
    subprocess.run(cmd, check=True)
    run = [str(out/'test'), str(resource), str(catalog)]
    result = subprocess.check_output(run, text=True)
    (out/'result.json').write_text(result)
    assert before == {str(v): digest(v) for v in inputs}, 'Input changed during proof'
    evidence = {'scope': __doc__, 'status': 'pass', 'result': json.loads(result),
                'inputs': before, 'commands': [cmd, run],
                'limitations': ['Host algorithm equivalence; native instruction/cycle validation is separate.',
                                'Uses admitted immutable resources, not arbitrary malformed ROM layouts.',
                                'No live route, VDP or game-performance admission.'],
                'outputs': {str(v.relative_to(out)): digest(v) for v in out.rglob('*') if v.is_file()}}
    (out/'evidence.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print(result, end='')


if __name__ == '__main__':
    main()
