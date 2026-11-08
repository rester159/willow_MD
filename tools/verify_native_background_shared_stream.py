#!/usr/bin/env python3
"""Validate serialized WBS1 against bound WBF1, WBC1 and canonical source references."""
from pathlib import Path
import argparse
import hashlib
import json
import struct


def verify(resource):
    manifest = json.loads((resource/'manifest.json').read_text())
    for path, digest in manifest['inputs'].items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest, path
    for name, digest in manifest['outputs'].items():
        assert hashlib.sha256((resource/name).read_bytes()).hexdigest() == digest, name
    def input_path(suffix):
        hits = [Path(p) for p in manifest['inputs'] if p.endswith(suffix)]
        assert len(hits) == 1, suffix
        return hits[0]
    blob = (resource/'shared-stream.bin').read_bytes()
    magic, version, header, *f = struct.unpack_from('>IHH18I', blob)
    assert (magic, version, header) == (0x57425331, 1, 80)
    total, origin, base, basebytes, catbase, catbytes, nf, nn, nr, bind, bindbytes, refs, attrs, keys, patterns, fixed, pool, np = f
    assert total == len(blob) and origin == manifest['origin'] and origin+total <= 0x200000
    assert nr == (nf+nn)*16 < 65536 and bindbytes == 128
    expected = [(bind,128), (refs,nr*4), (attrs,nr//2), (keys,(nf+nn)*8)]
    end = 80
    for offset, size in expected:
        assert offset == (end+3)&~3 and blob[end:offset] == bytes(offset-end)
        end = offset+size
    assert end == total
    farpath = input_path('/far-stream.bin'); catalogpath = input_path('/catalog.bin')
    canonicalpath = input_path('/canonical-v1/entries.json')
    candidates = [Path(p) for p in manifest['inputs'] if p.endswith('/entries.json') and 'canonical-v1' not in p and 'prepared-v2' not in p]
    assert len(candidates) == 1
    residentpath = candidates[0]
    for n, p in enumerate([farpath, catalogpath, canonicalpath, residentpath]):
        assert blob[bind+n*32:bind+(n+1)*32] == hashlib.sha256(p.read_bytes()).digest()
    far = farpath.read_bytes(); cat = catalogpath.read_bytes()
    fl = json.loads(farpath.with_name('layout.json').read_text())
    cl = json.loads(catalogpath.with_name('layout.json').read_text())
    regions = {r['name']:r for r in fl['regions']}
    assert (base,basebytes,catbase,catbytes) == (fl['origin'],len(far),cl['origin'],len(cat))
    assert origin == (base+basebytes+3)&~3
    assert (patterns,fixed,pool,np) == (regions['patterns']['address'],regions['fixed_map']['address'],regions['pool']['address'],regions['patterns']['bytes']//32)
    canonical = json.loads(canonicalpath.read_text())
    resident = {(e['layer'],e['code'],e['attribute']):e for e in json.loads(residentpath.read_text())}
    emitted_keys = json.loads((resource/'keys.json').read_text())
    actual_refs = bytearray()
    combined = 0
    for layer, count in [(3,nf),(2,nn)]:
        entries = sorted((e for e in canonical if e['layer']==layer),key=lambda e:(e['code'],e['attribute']))
        assert len(entries) == count
        table = next(t for t in cl['layers'] if t['layer']==layer)
        stride = 16 if layer==3 else 4
        for pair,e in enumerate(entries):
            key = (layer,e['code'],e['attribute']); mask = 65535 if layer==3 else 15
            assert struct.unpack_from('>4H',blob,keys+combined*8) == (*key,mask)
            assert emitted_keys[combined] == dict(combined_key=combined,layer=layer,source_pair_id=pair,code=key[1],attribute=key[2],valid_subtile_mask=mask)
            assert struct.unpack_from('>IH',cat,table['offsets']['pairs']+pair*6) == ((key[1]<<16)|key[2],pair*stride)
            for sub in range(16):
                at = combined*16+sub
                pattern,hint = struct.unpack_from('>HH',blob,refs+at*4)
                attr = ((blob[attrs+at//2]>>(0 if at&1 else 4))&15)<<11
                if sub>=stride:
                    assert (pattern,hint,attr)==(0,0,0)
                else:
                    ref=e['references'][sub]
                    assert (pattern,attr)==(ref['pattern'],ref['attributes']) and pattern<np
                    assert hint==resident[key]['references'][sub]['word']
                    assert (pattern,hint)==struct.unpack_from('>HH',cat,table['offsets']['refs']+(pair*stride+sub)*4)
                actual_refs.extend(struct.pack('>HH',pattern,hint))
            combined+=1
    assert combined == nf+nn
    # Independent previous source/opacity audit used a separate implementation.
    audit=Path(__file__).resolve().parents[1]/'build/native-background-near-stream-audit/v1'
    assert bytes(actual_refs)==(audit/'combined-refs.bin').read_bytes()
    assert blob[attrs:attrs+nr//2]==(audit/'combined-attributes.bin').read_bytes()
    return dict(status='pass',references=nr,far_keys=nf,near_keys=nn,
                header_and_regions_checked=True,prior_independent_audit_equal=True,runtime_admitted=False)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--resource',type=Path,required=True)
    a=p.parse_args();print(json.dumps(verify(a.resource),indent=2))
