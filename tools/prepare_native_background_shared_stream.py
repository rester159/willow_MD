#!/usr/bin/env python3
"""Build WBS1 combined near/far references alongside unchanged WBF1 patterns.

No captured maps, positions, or object lists. Near keys retain four real
subtiles and twelve zero pads so the native queue keeps its 16-reference ABI.
"""
from pathlib import Path
from functools import lru_cache
import argparse
import hashlib
import json
import struct
import zipfile
from cps1_gfx import assemble_region, decode_tile

ROOT = Path(__file__).resolve().parents[1]


def prepare(output, far, catalog, residency, checkpoint):
    output = output.resolve()
    assert not output.exists(), 'Use a fresh output directory'
    pins = {}

    def load(path):
        path = path.resolve()
        data = path.read_bytes()
        pins[str(path)] = hashlib.sha256(data).hexdigest()
        return data

    def js(path):
        return json.loads(load(path))

    def checked(directory):
        manifest = js(directory/'manifest.json')
        for name, digest in manifest['outputs'].items():
            assert hashlib.sha256(load(directory/name)).hexdigest() == digest
        return manifest

    load(Path(__file__))
    fm = checked(far)
    checked(catalog)
    checked(residency)
    cl = js(catalog/'layout.json')
    fl = js(far/'layout.json')
    cb = load(catalog/'catalog.bin')
    fb = load(far/'far-stream.bin')
    frozen = js(checkpoint/'checkpoint.json')

    def frozen_load(name):
        data = load(checkpoint/name)
        assert hashlib.sha256(data).hexdigest() == frozen['files'][name]
        return data

    canonical = json.loads(frozen_load('canonical-v1/entries.json'))
    fitted = json.loads(frozen_load('prepared-v2/entries.json'))
    patterns = frozen_load('canonical-v1/canonical-patterns.bin')
    palettes = frozen_load('canonical-v1/source-palettes.bin')
    cram = frozen_load('canonical-v1/cram.bin')
    archive = load(ROOT/'willow.zip')
    load(ROOT/'tools/cps1_gfx.py')
    regions = {r['name']: r for r in fl['regions']}
    for name, data in [('canonical_patterns', patterns), ('source_palettes', palettes),
                       ('cram', cram), ('source_archive', archive), ('catalog', cb),
                       ('initial_resident_image', load(residency/'resident-upload.bin'))]:
        assert hashlib.sha256(data).hexdigest() == fl['bindings'][name]['sha256']
    p = regions['patterns']; assert fb[p['offset']:p['offset']+p['bytes']] == patterns
    byfit = {(e['layer'], e['code'], e['attribute']): e for e in fitted}
    byresident = {(e['layer'], e['code'], e['attribute']): e for e in js(residency/'entries.json')}
    entries = {layer: sorted((e for e in canonical if e['layer'] == layer),
                            key=lambda e: (e['code'], e['attribute'])) for layer in (3, 2)}
    assert len(canonical) == len(byfit) == len(byresident) == sum(map(len, entries.values()))
    refs, attributes, keys = [], [], []
    pixels = 0
    with zipfile.ZipFile(ROOT/'willow.zip') as z:
        gfx = assemble_region(z)

    @lru_cache(None)
    def source(layer, code):
        if layer == 2:
            return decode_tile(gfx, 0x4000+code, 16) if code <= 0x1fff else [[15]*16 for _ in range(16)]
        masked = code & 0x3fff
        return decode_tile(gfx, masked, 32) if 0xa00 <= masked <= 0xdff else [[15]*32 for _ in range(32)]

    for layer in (3, 2):
        table = next(v for v in cl['layers'] if v['layer'] == layer)
        width = 4 if layer == 3 else 2
        for pair, e in enumerate(entries[layer]):
            key = (layer, e['code'], e['attribute'])
            assert struct.unpack_from('>IH', cb, table['offsets']['pairs']+pair*6) == ((key[1]<<16)|key[2], pair*width*width)
            keys.append(dict(combined_key=len(keys), layer=layer, source_pair_id=pair,
                             code=key[1], attribute=key[2], valid_subtile_mask=65535 if layer == 3 else 15))
            img = source(layer, key[1]); size = width*8
            assert len(e['references']) == width*width
            for sub in range(16):
                if sub >= width*width:
                    refs.append((0, 0)); attributes.append(0); continue
                ref = e['references'][sub]; pattern, attr = ref['pattern'], ref['attributes']
                assert 0 <= pattern < len(patterns)//32 and not attr & ~0x7800
                hint = byresident[key]['references'][sub]['word']
                old = struct.unpack_from('>HH', cb, table['offsets']['refs']+(pair*width*width+sub)*4)
                assert old == (pattern, hint)
                refs.append(old); attributes.append(attr >> 11)
                native = patterns[pattern*32:(pattern+1)*32]
                for y in range(8):
                    mask = 0
                    for x in range(8):
                        sx = sub%width*8+x; sy = sub//width*8+y
                        source_pen = img[size-1-sy if key[2]&64 else sy][size-1-sx if key[2]&32 else sx]
                        nx = 7-x if attr&0x800 else x; ny = 7-y if attr&0x1000 else y
                        pen = (native[ny*4+nx//2] >> (0 if nx&1 else 4)) & 15
                        assert pen == byfit[key]['pen_maps'][sub][source_pen]
                        assert (pen == 0) == (source_pen == 15)
                        assert attr >> 13 == byfit[key]['references'][sub] >> 14
                        if source_pen != 15: mask |= 0x80 >> x
                        pixels += 1
                    assert cb[table['offsets']['opacity']+(pair*width*width+sub)*8+y] == mask
    assert len(refs) == len(keys)*16 < 65536
    far_count, near_count = len(entries[3]), len(entries[2])
    refs_raw = b''.join(struct.pack('>HH', *r) for r in refs)
    attr_raw = bytes(attributes[i]<<4 | attributes[i+1] for i in range(0, len(attributes), 2))
    a = regions['attributes']; assert attr_raw[:far_count*8] == fb[a['offset']:a['offset']+a['bytes']]
    keys_raw = b''.join(struct.pack('>HHHH', k['layer'], k['code'], k['attribute'], k['valid_subtile_mask']) for k in keys)
    origin = (fm['origin']+len(fb)+3)&~3
    blob = bytearray(80); layout_regions = []

    def append(name, data):
        while len(blob)%4: blob.append(0)
        at = len(blob); blob.extend(data)
        layout_regions.append(dict(name=name, offset=at, address=origin+at, bytes=len(data)))
        return at

    bindings = [hashlib.sha256(data).digest() for data in (fb, cb, frozen_load('canonical-v1/entries.json'), load(residency/'entries.json'))]
    bind_at = append('bindings', b''.join(bindings))
    refs_at = append('refs', refs_raw); attrs_at = append('attributes', attr_raw); keys_at = append('keys', keys_raw)
    fields = [len(blob), origin, fm['origin'], len(fb), cl['origin'], len(cb), far_count, near_count,
              len(refs), bind_at, 128, refs_at, attrs_at, keys_at, regions['patterns']['address'],
              regions['fixed_map']['address'], regions['pool']['address'], len(patterns)//32]
    struct.pack_into('>IHH18I', blob, 0, 0x57425331, 1, 80, *fields)
    assert origin+len(blob) <= 0x200000
    constants = dict(ORIGIN=origin, BYTES=len(blob), FAR_PAIR_COUNT=far_count, NEAR_PAIR_COUNT=near_count,
                     NEAR_KEY_BASE=far_count, REFERENCE_COUNT=len(refs), NEAR_REQUEST_MASK=15, FAR_REQUEST_MASK=65535)
    constants.update({r['name'].upper()+'_ADDRESS': r['address'] for r in layout_regions})
    header = '#ifndef WILLOW_BACKGROUND_SHARED_IMPORTS_H\n#define WILLOW_BACKGROUND_SHARED_IMPORTS_H\n'
    header += '\n'.join(f'#define WILLOW_BG_SHARED_{k} 0x{v:x}UL' for k, v in constants.items())+'\n#endif\n'
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in pins.items())
    output.mkdir(parents=True)
    (output/'shared-stream.bin').write_bytes(blob)
    (output/'shared_stream_imports.h').write_text(header)
    (output/'keys.json').write_text(json.dumps(keys, indent=2)+'\n')
    (output/'layout.json').write_text(json.dumps(dict(magic='WBS1', version=1, origin=origin, bytes=len(blob),
        header_format='>IHH18I', fields=fields, regions=layout_regions,
        near_reference_conversion='(far_pair_count + (near_ref >> 2))*16 + (near_ref & 3)'), indent=2)+'\n')
    report = dict(scope=__doc__, runtime_admitted=False, origin=origin, bytes=len(blob),
                  far_keys=far_count, near_keys=near_count, references=len(refs), source_pixels=pixels,
                  inputs=pins, outputs={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()})
    (output/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    return {k: v for k, v in report.items() if k not in ('inputs', 'outputs')}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--far', type=Path, default=ROOT/'build/native-background-far-stream/prepared-v1')
    p.add_argument('--catalog', type=Path, default=ROOT/'build/native-background-catalog/forest-277-v1')
    p.add_argument('--residency', type=Path, default=ROOT/'build/native-background-residency/forest-277-v1')
    p.add_argument('--checkpoint', type=Path, default=ROOT/'build/native-route-background-audit/checkpoint-v1')
    a = p.parse_args()
    print(json.dumps(prepare(a.output, a.far, a.catalog, a.residency, a.checkpoint), indent=2))


if __name__ == '__main__':
    main()
