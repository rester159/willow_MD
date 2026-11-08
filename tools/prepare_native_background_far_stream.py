#!/usr/bin/env python3
"""Prepare immutable WBF1 far-stream data; no maps, positions or runtime policy.

WBC1 remains unchanged. Complete source-key metadata determines dynamic pool
ownership, and missing native attributes come from the canonical generator.
Every field is explicit big-endian; no target C structure layout is serialized.
"""
from collections import Counter
from functools import lru_cache
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
import zipfile
from cps1_gfx import assemble_region, decode_tile

ROOT = Path(__file__).resolve().parents[1]
MAGIC = 0x57424631  # WBF1
HEADER_FORMAT = '>IHH6I4H6I'
HEADER_BYTES = 64
BINDINGS = ('catalog', 'source_archive', 'source_palettes', 'cram',
            'canonical_patterns', 'initial_resident_image')
MISSING = 0xffff


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pen(pattern, x, y):
    return (pattern[y * 4 + x // 2] >> (0 if x & 1 else 4)) & 15


def prepare(catalog, residency, checkpoint, origin, output):
    catalog = catalog.resolve(); residency = residency.resolve()
    checkpoint = checkpoint.resolve(); output = output.resolve()
    if not (0 <= origin <= 0xffffffff and origin % 4 == 0):
        raise ValueError('origin must be an aligned unsigned 32-bit address')
    if output.exists():
        raise ValueError('output must be a fresh directory')
    pins = {}

    def load(path):
        path = path.resolve(); data = path.read_bytes()
        pins[str(path)] = hashlib.sha256(data).hexdigest()
        return data

    def read_json(path):
        return json.loads(load(path))

    load(Path(__file__))
    frozen = read_json(checkpoint / 'checkpoint.json')
    canonical_dir = checkpoint / 'canonical-v1'
    fitted_dir = checkpoint / 'prepared-v2'
    canonical_entries = read_json(canonical_dir / 'entries.json')
    canonical = load(canonical_dir / 'canonical-patterns.bin')
    canonical_manifest = read_json(canonical_dir / 'manifest.json')
    fitted_entries = read_json(fitted_dir / 'entries.json')
    source_palettes = load(canonical_dir / 'source-palettes.bin')
    cram = load(canonical_dir / 'cram.bin')
    for path in (canonical_dir / 'entries.json', canonical_dir / 'canonical-patterns.bin',
                 canonical_dir / 'manifest.json', canonical_dir / 'source-palettes.bin',
                 canonical_dir / 'cram.bin', fitted_dir / 'entries.json'):
        assert pins[str(path)] == frozen['files'][str(path.relative_to(checkpoint))], path
    for name in ('entries.json', 'canonical-patterns.bin', 'source-palettes.bin', 'cram.bin'):
        assert pins[str(canonical_dir / name)] == canonical_manifest['outputs'][name]
    assert len(canonical) % 32 == 0 and len(source_palettes) == 6144 and len(cram) == 128
    patterns = [canonical[i:i+32] for i in range(0, len(canonical), 32)]
    assert patterns and patterns[0] == bytes(32) and len(patterns) < MISSING
    assert len(set(patterns)) == len(patterns)
    count = len(patterns)
    original = {(d['layer'], d['code'], d['attribute']): d for d in canonical_entries}
    fitted = {(d['layer'], d['code'], d['attribute']): d for d in fitted_entries}
    assert len(original) == len(canonical_entries) and set(original) == set(fitted)

    rm = read_json(residency / 'manifest.json')
    re = read_json(residency / 'entries.json')
    initial_map_raw = load(residency / 'pattern-to-tile.bin')
    physical_raw = load(residency / 'physical-tiles.bin')
    resident_image = load(residency / 'resident-upload.bin')
    for name in ('source-palettes.bin', 'cram.bin'):
        data = load(residency / name)
        assert data == (source_palettes if name == 'source-palettes.bin' else cram)
    for name in ('entries.json', 'pattern-to-tile.bin', 'physical-tiles.bin',
                 'resident-upload.bin', 'source-palettes.bin', 'cram.bin'):
        assert pins[str(residency / name)] == rm['outputs'][name]
    assert len(initial_map_raw) == count * 2 and len(physical_raw) % 34 == 0
    initial = list(struct.unpack('>' + str(count) + 'H', initial_map_raw))
    assert initial[0] == 0
    physical = {struct.unpack_from('>H', physical_raw, n)[0]: physical_raw[n+2:n+34]
                for n in range(0, len(physical_raw), 34)}
    assert len(physical) * 34 == len(physical_raw)
    resident_lookup = {(d['layer'], d['code'], d['attribute']): d for d in re}
    assert len(resident_lookup) == len(re) and set(resident_lookup) == set(original)
    used = {2: set(), 3: set()}
    for key, d in original.items():
        assert d['layer'] in used and d['size'] == (16 if d['layer'] == 2 else 32)
        current = resident_lookup[key]
        assert len(d['references']) == len(current['references']) == (d['size'] // 8) ** 2
        for ref, current_ref in zip(d['references'], current['references']):
            pattern, attributes = ref['pattern'], ref['attributes']
            assert 0 <= pattern < count and attributes & ~0x7800 == 0
            assert current_ref['pattern'] == pattern
            expected = MISSING if initial[pattern] == MISSING else initial[pattern] | attributes
            assert current_ref['word'] == expected
            if pattern: used[d['layer']].add(pattern)
    resident = set()
    for pattern, tile in enumerate(initial):
        if tile in (0, MISSING):
            assert tile != 0 or pattern == 0
            continue
        assert tile in physical and physical[tile] == patterns[pattern]
        assert 16 <= tile <= 875 or 1481 <= tile <= 1535
        resident.add(pattern)
    assert len({initial[p] for p in resident}) == len(resident)
    pool_patterns = (resident & used[3]) - used[2]
    pool = sorted((initial[pattern], pattern) for pattern in pool_patterns)
    assert pool and len(pool) <= 255
    pool_slots = {tile for tile, _ in pool}
    assert len(pool_slots) == len(pool)
    fixed = [MISSING if tile in pool_slots else tile for tile in initial]
    assert fixed[0] == 0 and not (set(fixed) & pool_slots)
    assert all(fixed[p] == initial[p] for p in resident & used[2])
    overlay_slots = set(physical) - {initial[p] for p in resident}
    assert not (overlay_slots & pool_slots)

    cm = read_json(catalog / 'manifest.json')
    layout = read_json(catalog / 'layout.json')
    catalog_raw = load(catalog / 'catalog.bin')
    load(catalog / 'catalog_imports.h')
    for name in ('layout.json', 'catalog.bin', 'catalog_imports.h'):
        assert pins[str(catalog / name)] == cm['outputs'][name]
    assert pins[str(residency / 'manifest.json')] in cm['hashes'].values()
    assert pins[str(canonical_dir / 'canonical-patterns.bin')] in cm['hashes'].values()
    assert struct.unpack_from('>IHHIIHHIII', catalog_raw) == (
        0x57424331, 1, 2, len(catalog_raw), 32, layout['overlay_count'], 24,
        layout['overlay_offset'], 1, 0)
    assert len(catalog_raw) == layout['total_bytes']
    far = next(d for d in layout['layers'] if d['layer'] == 3)
    near = next(d for d in layout['layers'] if d['layer'] == 2)
    assert len(layout['layers']) == 2
    catalog_refs = []; far_keys = []
    for directory_index, layer in enumerate((near, far)):
        selected = sorted((key, d) for key, d in original.items() if key[0] == layer['layer'])
        assert layer['pair_count'] == len(selected)
        subtiles = 4 if layer['layer'] == 2 else 16
        assert layer['subtiles_per_pair'] == subtiles and layer['reference_count'] == len(selected) * subtiles
        names = ('pairs', 'refs', 'source_pens', 'opacity', 'orientation', 'flags')
        assert struct.unpack_from('>4H6I', catalog_raw, 32+directory_index*32) == (
            layer['layer'], len(selected), layer['reference_count'], subtiles,
            *(layer['offsets'][name] for name in names))
        for n, (key, d) in enumerate(selected):
            assert struct.unpack_from('>IH', catalog_raw, layer['offsets']['pairs']+n*6) == (
                key[1] << 16 | key[2], n*subtiles)
            for i, (ref, current) in enumerate(zip(d['references'], resident_lookup[key]['references'])):
                at = n*subtiles+i
                assert struct.unpack_from('>HH', catalog_raw, layer['offsets']['refs']+at*4) == (
                    ref['pattern'], current['word'])
                assert struct.unpack_from('>H', catalog_raw, layer['offsets']['source_pens']+at*2)[0] == ref['source_pen_mask']
                assert catalog_raw[layer['offsets']['orientation']+at*2:layer['offsets']['orientation']+at*2+2] == bytes((ref['source_subtile'], ref['source_flips']))
                if layer['layer'] == 3: catalog_refs.append(ref)
            if layer['layer'] == 3: far_keys.append(key)
    assert len(catalog_refs) == far['reference_count'] < MISSING
    attributes = bytearray((len(catalog_refs)+1)//2)
    for n, ref in enumerate(catalog_refs):
        attributes[n//2] |= (ref['attributes'] >> 11) << (4 if not n & 1 else 0)

    archive = ROOT / 'willow.zip'; archive_raw = load(archive)
    load(ROOT / 'tools/cps1_gfx.py')
    assert pins[str(archive)] == frozen['external_inputs'][str(archive)]
    assert pins[str(ROOT / 'tools/cps1_gfx.py')] == frozen['files']['tools/cps1_gfx.py']
    with zipfile.ZipFile(archive) as z: gfx = assemble_region(z)

    @lru_cache(None)
    def source_image(code):
        code &= 0x3fff
        return decode_tile(gfx, code, 32) if 0xa00 <= code <= 0xdff else [[15]*32 for _ in range(32)]

    pixels = 0; key_proofs = []; opacity_flags = Counter()
    for key_index, key in enumerate(far_keys):
        source = source_image(key[1]); d = original[key]; before = fitted[key]
        assert before['size'] == 32
        for sub, ref in enumerate(d['references']):
            n = key_index*16+sub
            attrs = ((attributes[n//2] >> (4 if not n & 1 else 0)) & 15) << 11
            assert attrs == ref['attributes'] and attrs >> 13 == before['references'][sub] >> 14
            pattern = patterns[ref['pattern']]; mask = []; pens = 0
            for y in range(8):
                row = 0
                for x in range(8):
                    sx, sy = (sub & 3)*8+x, (sub >> 2)*8+y
                    source_pen = source[31-sy if key[2] & 64 else sy][31-sx if key[2] & 32 else sx]
                    actual = pen(pattern, 7-x if attrs & 0x800 else x, 7-y if attrs & 0x1000 else y)
                    assert actual == before['pen_maps'][sub][source_pen]
                    assert (actual == 0) == (source_pen == 15)
                    if source_pen != 15: row |= 0x80 >> x; pens |= 1 << source_pen
                    pixels += 1
                mask.append(row)
            assert pens == ref['source_pen_mask']
            assert catalog_raw[far['offsets']['opacity']+n*8:far['offsets']['opacity']+n*8+8] == bytes(mask)
            flag = 1 if mask == [255]*8 else 2 if mask == [0]*8 else 0
            assert catalog_raw[far['offsets']['flags']+n] == flag
            opacity_flags[flag] += 1
        key_proofs.append({'code':key[1], 'attribute':key[2], 'source_pixels':1024,
                           'patterns':sorted({r['pattern'] for r in d['references'] if r['pattern']})})

    binding_data = {
        'catalog': bytes.fromhex(pins[str(catalog / 'catalog.bin')]),
        'source_archive': hashlib.sha256(archive_raw).digest(),
        'source_palettes': hashlib.sha256(source_palettes).digest(),
        'cram': hashlib.sha256(cram).digest(),
        'canonical_patterns': hashlib.sha256(canonical).digest(),
        'initial_resident_image': hashlib.sha256(resident_image).digest()}
    blob = bytearray(HEADER_BYTES); regions = []

    def append(name, data):
        while len(blob) % 4: blob.append(0)
        offset = len(blob); blob.extend(data)
        regions.append({'name':name, 'offset':offset, 'bytes':len(data), 'address':origin+offset})
        return offset

    bindings_offset = append('bindings', b''.join(binding_data[name] for name in BINDINGS))
    pattern_offset = append('patterns', canonical)
    attribute_offset = append('attributes', attributes)
    pool_offset = append('pool', b''.join(struct.pack('>HH', *entry) for entry in pool))
    fixed_offset = append('fixed_map', struct.pack('>'+str(count)+'H', *fixed))
    total = len(blob)
    if origin + total > 0x100000000:
        raise ValueError('origin plus resource exceeds the unsigned 32-bit address range')
    if origin < layout['origin']+len(catalog_raw) and origin+total > layout['origin']:
        raise ValueError('far-stream resource overlaps the bound catalog')
    header_values = (MAGIC, 1, HEADER_BYTES, total, 1, layout['origin'], len(catalog_raw),
                     bindings_offset, 32*len(BINDINGS), count, len(far_keys), len(catalog_refs), len(pool),
                     pattern_offset, attribute_offset, pool_offset, fixed_offset,
                     far['offsets']['pairs'], far['offsets']['refs'])
    assert struct.calcsize(HEADER_FORMAT) == HEADER_BYTES
    struct.pack_into(HEADER_FORMAT, blob, 0, *header_values)
    # Binary round trip, with every emitted byte and relationship checked.
    assert struct.unpack_from(HEADER_FORMAT, blob) == header_values
    assert blob[pattern_offset:pattern_offset+count*32] == canonical
    assert blob[attribute_offset:attribute_offset+len(attributes)] == attributes
    for n, ref in enumerate(catalog_refs):
        packed = blob[attribute_offset+n//2]
        assert ((packed >> (4 if not n & 1 else 0)) & 15) << 11 == ref['attributes']
    for n, expected in enumerate(pool):
        tile, owner = struct.unpack_from('>HH', blob, pool_offset+n*4)
        assert (tile, owner) == expected and owner in used[3] and owner not in used[2]
        assert physical[tile] == blob[pattern_offset+owner*32:pattern_offset+(owner+1)*32]
    for n in range(count):
        actual = struct.unpack_from('>H', blob, fixed_offset+n*2)[0]
        assert actual == (MISSING if initial[n] in pool_slots else initial[n])
    for n, name in enumerate(BINDINGS):
        assert blob[bindings_offset+n*32:bindings_offset+(n+1)*32] == binding_data[name]
    end = HEADER_BYTES
    for region in regions:
        assert region['offset'] % 4 == 0 and region['offset'] >= end
        assert blob[end:region['offset']] == bytes(region['offset']-end)
        end = region['offset']+region['bytes']
    assert end == total

    prefix = 'WILLOW_BG_FAR_STREAM'
    lines = ['/* Generated immutable WBF1 imports; WBC1 sentinel ABI is unchanged.',
             ' * All u16/u32 values are big-endian. No captured maps or coordinates.',
             ' * Attribute nibble: even ref high, odd ref low; native attrs=nibble<<11.',
             ' * Pool ownership is initial state, never permission to evict displayed art.',
             ' * Fixed-map FFFF means unresident OR dynamic; resolve pool ownership separately.',
             ' */', '#ifndef WILLOW_NATIVE_BACKGROUND_FAR_STREAM_IMPORTS_H',
             '#define WILLOW_NATIVE_BACKGROUND_FAR_STREAM_IMPORTS_H', '#include <stdint.h>']
    constants = {'MAGIC':MAGIC, 'VERSION':1, 'ORIGIN':origin, 'BYTES':total,
                 'HEADER_BYTES':HEADER_BYTES, 'CATALOG_ORIGIN':layout['origin'],
                 'CATALOG_BYTES':len(catalog_raw), 'PATTERN_COUNT':count, 'PATTERN_STRIDE':32,
                 'PAIR_COUNT':len(far_keys), 'REFERENCE_COUNT':len(catalog_refs), 'POOL_COUNT':len(pool),
                 'POOL_STRIDE':4, 'POOL_TILE_OFFSET':0, 'POOL_OWNER_OFFSET':2,
                 'FIXED_MAP_STRIDE':2, 'ATTRIBUTES_BYTES':len(attributes), 'MISSING':MISSING,
                 'CATALOG_PAIRS_ADDRESS':layout['origin']+far['offsets']['pairs'],
                 'CATALOG_REFS_ADDRESS':layout['origin']+far['offsets']['refs']}
    for name, value in constants.items(): lines.append(f'#define {prefix}_{name} 0x{value:x}UL')
    for region in regions:
        name = region['name'].upper()
        lines.extend([f'#define {prefix}_{name}_OFFSET 0x{region["offset"]:x}UL',
                      f'#define {prefix}_{name}_ADDRESS 0x{region["address"]:x}UL',
                      f'#define {prefix}_{name}_BYTES 0x{region["bytes"]:x}UL'])
    for n, name in enumerate(BINDINGS):
        lines.append(f'#define {prefix}_{name.upper()}_SHA256 "{binding_data[name].hex()}"')
        lines.append(f'#define {prefix}_{name.upper()}_SHA256_ADDRESS 0x{origin+bindings_offset+n*32:x}UL')
    lines += ['#endif', '']
    header = '\n'.join(lines)
    details = {'magic':'WBF1', 'magic_u32':MAGIC, 'version':1, 'origin':origin,
               'total_bytes':total, 'end_address_exclusive':origin+total,
               'header_format':HEADER_FORMAT, 'header_bytes':HEADER_BYTES,
               'header_fields':['magic','version','headerBytes','totalBytes','flags','catalogOrigin','catalogBytes',
                 'bindingsOffset','bindingsBytes','patternCount','farPairCount','farReferenceCount','poolCount',
                 'patternsOffset','attributesOffset','poolOffset','fixedMapOffset','catalogFarPairsOffset','catalogFarRefsOffset'],
               'flags':{'value':1,'bit0':'Attribute even-reference nibble is high; all other bits zero.'},
               'regions':regions, 'bindings':{name:{'offset':bindings_offset+n*32,'sha256':binding_data[name].hex()}
                                            for n, name in enumerate(BINDINGS)},
               'attributes':'Reference order is unchanged WBC1 Scroll3 order; nibble=canonical attributes>>11; even high, odd low.',
               'pool':'Sorted repeated BEu16 physicalTile, BEu16 initialCanonicalOwner; all complete-catalog near/shared and overlay owners excluded.',
               'fixed_map':'BEu16 physicalTile per canonical ID. Existing unresident and every dynamic-pool owner areFFFF. Canonical0 stays physical0.',
               'pattern':'Exact canonical32-byte Genesis8x8 4bpp pattern, indexed by canonicalID.'}
    proof = {'scope':__doc__, 'runtime_admitted':False, 'persistent_ram_bytes':0,
             'origin':origin,'bytes':total,'catalog_origin':layout['origin'],
             'pattern_count':count,'pattern_bytes':len(canonical), 'far_pair_count':len(far_keys),
             'far_reference_count':len(catalog_refs),'attributes_bytes':len(attributes),
             'pool_count':len(pool),'pool_bytes':len(pool)*4,'fixed_map_bytes':count*2,
             'fixed_resident_nonblank_patterns':sum(x not in (0,MISSING) for x in fixed),
             'pinned_near_resident_patterns':len(resident & used[2]),
             'pinned_shared_near_far_patterns':len(resident & used[2] & used[3]),
             'pinned_overlay_slots':sorted(overlay_slots),
             'original_source_fitted_pixel_checks':pixels,'native_palette_page_checks':len(catalog_refs),
             'original_opacity_checks':pixels,'opacity_flag_counts':dict(opacity_flags),
             'key_proofs':key_proofs,'pool':[{'physical_tile':tile,'initial_canonical_owner':owner} for tile,owner in pool],
             'binary_field_roundtrip':'All header/bindings/pattern/attribute/pool/fixed-map/padding fields checked.',
             'catalog_preserved_sha256':pins[str(catalog / 'catalog.bin')],
             'source_palettes_preserved_sha256':hashlib.sha256(source_palettes).hexdigest(),
             'cram_preserved_sha256':hashlib.sha256(cram).hexdigest(),
             'inputs':pins}
    # Refuse input races before creating deliverables; no existing output is overwritten.
    assert all(sha(Path(path)) == digest for path, digest in pins.items())
    output.mkdir(parents=True)
    (output/'far-stream.bin').write_bytes(blob)
    (output/'far_stream_imports.h').write_text(header)
    (output/'layout.json').write_text(json.dumps(details, indent=2)+'\n')
    (output/'proof.json').write_text(json.dumps(proof, indent=2)+'\n')
    command = ['m68k-elf-gcc','-m68000','-std=c99','-Wall','-Wextra','-Werror','-ffreestanding',
               '-x','c','-fsyntax-only',str(output/'far_stream_imports.h')]
    subprocess.run(command, check=True)
    report = {key:proof[key] for key in ('scope','runtime_admitted','persistent_ram_bytes','origin','bytes',
              'catalog_origin','pattern_count','pattern_bytes','far_pair_count','far_reference_count',
              'attributes_bytes','pool_count','pool_bytes','fixed_map_bytes','fixed_resident_nonblank_patterns',
              'original_source_fitted_pixel_checks','native_palette_page_checks','original_opacity_checks')}
    report.update(inputs=pins, native_compile=command,
                  outputs={name:sha(output/name) for name in ('far-stream.bin','far_stream_imports.h','layout.json','proof.json')})
    (output/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--catalog',type=Path,default=ROOT/'build/native-background-catalog/forest-277-v1')
    p.add_argument('--residency',type=Path,default=ROOT/'build/native-background-residency/forest-277-v1')
    p.add_argument('--checkpoint',type=Path,default=ROOT/'build/native-route-background-audit/checkpoint-v1')
    p.add_argument('--origin',type=lambda value:int(value,0),default=0x150000)
    p.add_argument('--output',type=Path,default=ROOT/'build/native-background-far-stream/prepared-v1')
    a=p.parse_args()
    report=prepare(a.catalog,a.residency,a.checkpoint,a.origin,a.output)
    print(json.dumps({key:value for key,value in report.items() if key not in ('inputs','outputs','native_compile')},indent=2))


if __name__ == '__main__':main()
