#!/usr/bin/env python3
"""Controller-driven paid shop route using the existing three-way scheduler oracle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/native-shop-purchase-route/v3'))
    parser.add_argument('--core', type=Path, default=Path.home() / 'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    args = parser.parse_args()
    out = args.output.resolve();out.mkdir(parents=True, exist_ok=True)
    if sha(ROOT / 'build/reference_maincpu.bin') != '0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1':
        raise SystemExit('Unexpected original ROM hash')
    source = ROOT / 'tools/test_native_forest_scheduler.c'
    fixture = source.read_text()
    edits = []

    def replace(old, new):
        nonlocal fixture
        if fixture.count(old) != 1:
            raise RuntimeError('Original scheduler fixture no longer matches: ' + old[:100])
        fixture = fixture.replace(old, new)
        edits.append({'original': old, 'replacement': new})

    replace('int main(int argc,char**argv){', '#include "native_shop_purchase_route.h"\nint main(int argc,char**argv){')
    replace('''   if(rw(0x8008)==4){
    input=0;
    if(shopEntries&&rw(0x800c)==2&&!rw(0x8010)){
     input=shopInputTick==1?1:shopInputTick==3?16:0;shopInputTick++;
    }
   }''', '   input=shopPurchaseController(shopEntries,input,&shopInputTick);')
    replace('   TARGET_CHECK(0,flow.task.pc==0x932c?1:2);',
            '   TARGET_CHECK(0,flow.task.pc==0x932c?1:2);\n   shopPurchaseAfterMain(pass);')
    replace('    if(selected.dispatch&&selected.state==12&&!rw(selected.slot+4)&&rw(selected.slot+6)==0x626a)scoreTasks++;',
            '    if(selected.dispatch&&selected.state==12&&!rw(selected.slot+4)&&rw(selected.slot+6)==0x626a)scoreTasks++;\n    shopPurchaseSelected(&selected);')
    replace('   TARGET_CHECK(1,1);', '   TARGET_CHECK(1,1);\n   shopPurchaseAfterScheduler(pass,&runtime);')
    replace(' printf("Shop decline and exit: %u completed exits with original text/countdown/fade waits\\n",shopExits);',
            ' shopPurchaseCheck();\n printf("Shop purchase and exit: %u completed exits with original text/countdown/fade waits\\n",shopExits);')
    replace(' printf("Service-option shop entry: %u fade-wait boundaries, %u completed shop presentations; invulnerability enabled, purchases not yet verified\\n",shopWaits,shopEntries);',
            ' printf("Service-option shop entry: %u fade-wait boundaries, %u completed shop presentations; invulnerability enabled; paid purchase checked above\\n",shopWaits,shopEntries);')
    redirects = {
        'build/forest_runtime_probe.': str(out / 'forest_runtime_probe.'),
        'build/native-shop-entry-investigation/': str(out) + '/',
        'build/first-level-runtime-investigation/': str(out) + '/',
        'build/forest-': str(out / 'forest-'),
    }
    for old, new in redirects.items():
        fixture = fixture.replace(old, new)
    (out / 'test-route.c').write_text(fixture)
    (out / 'fixture-edits.json').write_text(json.dumps({'source': str(source.relative_to(ROOT)), 'source_sha256': sha(source), 'edits': edits, 'path_redirects': redirects}, indent=2) + '\n')
    options = ['-Itools', '-DWILLOW_TEST_FIRST_LEVEL', '-DWILLOW_TEST_BOSS_INPUT',
               '-DWILLOW_TEST_SHOP_ENTRY', '-DWILLOW_TEST_SHOP_EXIT']
    inputs = {source, ROOT / 'tools/native_shop_purchase_route.h', Path(__file__).resolve(),
              ROOT / 'tools/build_timeout_runtime_probe.py', ROOT / 'tools/timeout_runtime_probe.c',
              ROOT / 'tools/timeout_runtime_probe.ld', ROOT / 'build/reference_maincpu.bin'}
    deps = subprocess.check_output(['clang', '-MM', *options, str(out / 'test-route.c')], cwd=ROOT, text=True).replace('\\\n', ' ')
    inputs.update((ROOT / item).resolve() for item in shlex.split(deps.split(':', 1)[1]))
    input_hashes = {str(p.relative_to(ROOT)): sha(p) for p in sorted(inputs)}
    (out / 'source-inputs.json').write_text(json.dumps(input_hashes, indent=2) + '\n')
    commands = []

    def run(command, log, env=None):
        commands.append(command)
        with (out / log).open('w') as stream:
            result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                    env={**os.environ, **(env or {})})
        if result.returncode:
            (out / 'failure.json').write_text(json.dumps({'command': command, 'exit_code': result.returncode, 'log': log}, indent=2) + '\n')
            raise SystemExit(f'Failed: {out / log}')

    run([sys.executable, 'tools/build_timeout_runtime_probe.py', '--forest', '--output-dir', str(out)], 'build.log')
    run(['clang', '-Wall', '-Wextra', '-Werror', '-O2', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
         *options, str(out / 'test-route.c'), '-o', str(out / 'test-route')], 'compile.log')
    run([str(out / 'test-route'), str(args.core), '1'], 'route.log', {'WILLOW_PLAY_TICKS': '8192'})
    log = (out / 'route.log').read_text()
    if 'Continuous paid purchase: item1,50 earned gold debited' not in log:
        raise SystemExit('Paid purchase assertion missing')
    if input_hashes != {str(p.relative_to(ROOT)): sha(p) for p in sorted(inputs)}:
        raise SystemExit('Source input changed while the gate was running')
    evidence = {'status': 'pass', 'scope': 'Controller-only item1 paid purchase under full existing original/host/native main, scheduler and wake oracle; no target completion or money injection',
                'limitations': ['Existing service-option invulnerability route; not normal-play survival.',
                                'One reached shop/item on a source-RAM diagnostic; not Genesis shop graphics or full-game coverage.',
                                'Original source CCR/private-register emulation is not the native C API.'],
                'core_sha256': sha(args.core), 'inputs': input_hashes,
                'outputs': {str(p.relative_to(out)): sha(p) for p in sorted(out.rglob('*')) if p.is_file() and p.name != 'evidence.json'},
                'commands': commands}
    (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(f'PASS: {out / "evidence.json"}')


if __name__ == '__main__':
    main()
