#!/usr/bin/env python3
"""Check live native SAT publications against matching original source captures.

Read-only after the observer completes. No fixed old-route capture numbers,
emulator launch, target writes, graphics fitting or full-frame pixel claim.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct
import sys

from verify_native_route_objects import oracle

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    with path.open() as file:
        return list(csv.DictReader(file))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def by_capture(data, name):
    result = {}
    for row in data:
        capture = int(row['capture'])
        require(capture > 0 and capture not in result, f'{name}: duplicate/invalid capture{capture}')
        result[capture] = row
    require(set(result) == set(range(1, len(result)+1)), f'{name}: nonconsecutive capture IDs')
    return result


def check_links(raw, expected_count, label):
    """Independently walk hardware links; stale unreachable tail is permitted."""
    entries = list(struct.iter_unpack('>4H', raw))
    require(len(entries) == 80, f'{label}: SAT bank must have80 entries')
    if not expected_count:
        require(entries[0] == (0, 0, 0, 0), f'{label}: empty SAT must publish the explicit blank terminator')
        return 0
    seen = set()
    current = 0
    while True:
        require(current < 80 and current not in seen, f'{label}: invalid/cyclic SAT link{current}')
        require(current == len(seen), f'{label}: nonsequential SAT source order')
        seen.add(current)
        word = entries[current][1]
        require(word & 0xf080 == 0, f'{label}: unexpected reserved size/link bits')
        next_entry = word & 127
        if not next_entry:
            break
        require(next_entry == current+1, f'{label}: SAT link skips source piece')
        current = next_entry
    require(len(seen) == expected_count, f'{label}: terminated after{len(seen)} pieces, expected{expected_count}')
    return len(seen)


def verify(run, assets, output):
    run, assets, output = run.resolve(), assets.resolve(), output.resolve()
    manifest = json.loads((run/'observer-manifest.json').read_text())
    require(manifest.get('run', {}).get('exit_code') == 0, 'Observer did not finish successfully; do not verify partial files')
    for name, digest in manifest['inputs'].items():
        path = Path(name)
        if not path.is_absolute():
            path = ROOT/path
        require(sha(path) == digest, f'Observer input changed: {path}')
    require(sha(run/'test_native_render_cadence.c') == manifest['generated_sha256'], 'Generated observer source changed')
    require(sha(run/'test-native-render-cadence') == manifest['binary_sha256'], 'Observer executable changed')
    require(sha(run/'observer.log') == manifest['run']['log_sha256'], 'Observer log changed')

    prepared = json.loads((assets/'manifest.json').read_text())
    for name, digest in prepared['hashes'].items():
        path = Path(name)
        if not path.is_absolute():
            path = ROOT/path
        require(sha(path) == digest, f'Prepared resource dependency changed: {path}')
    for name, digest in prepared['outputs'].items():
        require(sha(assets/name) == digest, f'Prepared resource changed: {name}')
    entries = {(entry['code'], entry['source_group']): entry for entry in prepared['entries']}
    require(len(entries) == len(prepared['entries']), 'Duplicate prepared code/group key')
    template_keys = {entry['key'] for entry in prepared['templates']}

    inputs = ['observer-manifest.json', 'observer.log', 'render-summary.json', 'render-sat.csv',
              'render-sat.bin', 'publication-latches.csv', 'metadata-acquisitions.csv',
              'captured-original-banks.bin', 'render-final-vram.bin', 'render-final-cram.bin',
              'render-final-cram-internal.bin', 'render-final-registers.bin']
    before = {name:sha(run/name) for name in inputs}
    publications = rows(run/'render-sat.csv')
    require(publications, 'No successful SAT publications to verify')
    required = {'sample', 'frame', 'capture', 'bank', 'records', 'epoch', 'palette', 'view', 'sat_base'}
    require(required <= set(publications[0]), 'render-sat.csv schema changed')
    latches = by_capture(rows(run/'publication-latches.csv'), 'source latches')
    acquired = by_capture(rows(run/'metadata-acquisitions.csv'), 'native acquisitions')
    require(set(latches) == set(acquired), 'Source/native capture IDs differ')
    banks = (run/'captured-original-banks.bin').read_bytes()
    actual = (run/'render-sat.bin').read_bytes()
    require(len(banks) == len(latches)*2048, 'Incomplete original object-bank data')
    require(len(actual) == len(publications)*640, 'SAT data does not match publication row count')
    checked = []
    seen_captures = set()
    seen_keys = set()
    seen_pairs = set()
    previous_capture = 0
    final_prefix = b''
    for number, row in enumerate(publications):
        sample, frame, capture, bank, count, epoch, palette, view = (
            int(row[name]) for name in ('sample','frame','capture','bank','records','epoch','palette','view'))
        sat_base = int(row['sat_base'], 0)
        label = f'sample{sample}/capture{capture}'
        require(sample == number, f'{label}: missing or reordered sample')
        require(capture > previous_capture and capture not in seen_captures, f'{label}: repeated/stale capture')
        previous_capture = capture
        seen_captures.add(capture)
        require(capture in latches, f'{label}: source capture is absent')
        source, native = latches[capture], acquired[capture]
        require(int(source['frame']) == int(native['frame']) and frame >= int(source['frame']), f'{label}: source capture occurs after publication or source/native frames differ')
        require(bank in (0,1) and bank == int(source['selected_bank']) == int(native['bank']), f'{label}: source bank mismatch')
        require(0 <= count <= 8 and count == int(source['count']) == int(native['native_count']) == int(native['source_count']), f'{label}: source record count mismatch')
        require(epoch > 0 and epoch == int(source['producer_epoch']) == int(native['native_epoch']) == int(native['source_epoch']), f'{label}: source producer epoch mismatch')
        require(source['valid_complete_epoch'] == native['native_valid'] == native['source_valid'] == '1', f'{label}: uncertified source prefix')
        require(source['busy_selected'] == source['tail_nonempty'] == '0', f'{label}: incomplete producer or nonempty omitted tail')
        require(source['manager'] == source['scene'] == '2', f'{label}: not an active source publication')
        require(palette > 0 and view == 1 and sat_base == 0xf400, f'{label}: unsupported palette/view/SAT binding')
        start = (capture-1)*2048
        complete = list(struct.iter_unpack('>4H', banks[start:start+2048]))
        records = complete[:count]
        require(all(x == attribute == 0 for x,y,code,attribute in complete[count:]), f'{label}: omitted tail could become visible')
        for x,y,code,attribute in records:
            key = (code << 16) | attribute
            require(key in template_keys, f'{label}: unprepared original template{key:08x}')
            seen_keys.add(key)
        try:
            sat, words, pieces, max_sprites, max_pixels = oracle(records, entries)
        except (AssertionError, KeyError) as error:
            raise ValueError(f'{label}: original geometry/assets fail native admission: {error}') from error
        expected = struct.pack('>'+str(len(sat)*4)+'H', *(value for sprite in sat for value in sprite)) if sat else bytes(8)
        block = actual[number*640:(number+1)*640]
        require(block[:len(expected)] == expected, f'{label}: actual SAT prefix differs from original source geometry/asset oracle')
        check_links(block, len(sat), label)
        for ordinal,x,y,code,attribute,entry in pieces:
            seen_pairs.add((code, attribute & 31))
        checked.append({'sample':sample,'frame':frame,'capture_frame':int(source['frame']),'capture':capture,'bank':bank,'source_records':count,
                        'source_epoch':epoch,'palette_epoch':palette,'sat_entries':len(sat),
                        'sat_words_checked':len(expected)//2,'max_line_sprites':max_sprites,
                        'max_line_pixels':max_pixels,'source_bank_sha256':hashlib.sha256(banks[start:start+2048]).hexdigest(),
                        'actual_sat_sha256':hashlib.sha256(block).hexdigest()})
        final_prefix = expected

    vram = (run/'render-final-vram.bin').read_bytes()
    cram = (run/'render-final-cram.bin').read_bytes()
    raw_cram = (run/'render-final-cram-internal.bin').read_bytes()
    registers = (run/'render-final-registers.bin').read_bytes()
    require(len(vram) == 65536 and len(cram) == len(raw_cram) == 128 and len(registers) == 32, 'Final VDP dump sizes differ from contract')
    require(((registers[5] & 0x7e) << 9) == 0xf400, 'Final H40 SAT base moved')
    require(registers[12] & 1 and registers[1] & 4, 'Oracle requires Mode5/H40')
    require(vram[0xf400:0xf400+len(final_prefix)] == final_prefix, 'Final VRAM no longer holds last successful SAT prefix')
    patterns = (assets/'object-patterns.bin').read_bytes()
    first = prepared['first_tile']*32
    require(len(patterns) == 16000 and first+len(patterns) <= 0xc000, 'Prepared resident atlas extent differs from admitted plan')
    require(vram[first:first+len(patterns)] == patterns, 'Final resident source-derived patterns differ')
    require(cram == (assets/'cram.bin').read_bytes(), 'Final CRAM differs from the explicit shared palette')
    internal = [int.from_bytes(raw_cram[n:n+2], sys.byteorder) for n in range(0,128,2)]
    require(all(word <= 0x1ff for word in internal), 'Unexpected internal CRAM bits')
    expanded = struct.pack('>64H', *(((word&0x1c0)<<3)|((word&0x38)<<2)|((word&7)<<1) for word in internal))
    require(expanded == cram, 'Raw core CRAM conversion differs from canonical dump')
    after = {name:sha(run/name) for name in inputs}
    require(after == before, 'Observer inputs changed while verification was running')

    result = {'schema_version':1,
              'scope':'Independent live-source object SAT/resource verification. Every successful reference-path publication uses its current run capture ID, bank/count/epoch and original raw records. No whole-frame pixels, backgrounds/text, priority, audio, cadence or full-game FPS admission.',
              'successful_publications':len(checked),'source_captures_in_run':len(latches),
              'first_capture_checked':checked[0]['capture'],'last_capture_checked':checked[-1]['capture'],
              'source_records_checked':sum(row['source_records'] for row in checked),
              'sat_entries_checked':sum(row['sat_entries'] for row in checked),
              'sat_words_checked':sum(row['sat_words_checked'] for row in checked),
              'terminated_sat_lists_checked':len(checked),'mismatches':0,
              'max_line_sprites':max(row['max_line_sprites'] for row in checked),
              'max_line_pixels':max(row['max_line_pixels'] for row in checked),
              'source_template_keys':sorted(seen_keys),'visible_nonblank_code_groups':[list(pair) for pair in sorted(seen_pairs)],
              'palette_epochs':sorted({row['palette_epoch'] for row in checked}),
              'final_resident_pattern_bytes_exact':len(patterns),'resident_vram_extent':[first,first+len(patterns)],
              'final_cram_bytes_exact':len(cram),'final_sat_words_exact':len(final_prefix)//2,
              'unreachable_sat_tail':'Old entries after the exact terminating link may remain; no silent truncation is accepted.',
              'source_crop':{'left':96,'top':16,'width':320,'height':224,'flip':0},
              'cram_internal_byte_order':sys.byteorder,'inputs':before,
              'prepared_manifest_sha256':sha(assets/'manifest.json'),
              'sources':{str(path.relative_to(ROOT)):sha(path) for path in [Path(__file__),ROOT/'tools/verify_native_route_objects.py',ROOT/'tools/audit_native_prepared_catalog.py']}}
    output.mkdir(parents=True,exist_ok=True)
    (output/'publications.json').write_text(json.dumps(checked,indent=2)+'\n')
    result['publications_sha256'] = sha(output/'publications.json')
    (output/'evidence.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({key:value for key,value in result.items() if not isinstance(value,(dict,list))},indent=2))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',type=Path,default=ROOT/'build/native-render-cadence/observer-v1')
    parser.add_argument('--assets',type=Path,default=ROOT/'build/native-route-objects/prepared-v2')
    parser.add_argument('--output',type=Path,default=ROOT/'build/native-render-cadence/output-verification-v1')
    args = parser.parse_args()
    verify(args.run,args.assets,args.output)


if __name__ == '__main__':
    main()
