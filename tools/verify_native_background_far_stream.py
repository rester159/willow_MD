#!/usr/bin/env python3
"""Independently parse WBF1 output against original canonical/residency inputs.

This validates immutable data, imports and source binding; it does not emulate
or admit a runtime cache. Corruptions are local copies, never target writes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT=Path(__file__).resolve().parents[1]


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(resource, catalog, residency, checkpoint, output):
    resource=resource.resolve();catalog=catalog.resolve();residency=residency.resolve()
    checkpoint=checkpoint.resolve();output=output.resolve()
    assert not output.exists()
    manifest=json.loads((resource/'manifest.json').read_text())
    for name,digest in manifest['outputs'].items():assert sha(resource/name)==digest,name
    for name,digest in manifest['inputs'].items():assert sha(Path(name))==digest,name
    layout=json.loads((resource/'layout.json').read_text())
    catalog_layout=json.loads((catalog/'layout.json').read_text())
    cat=(catalog/'catalog.bin').read_bytes()
    canonical=(checkpoint/'canonical-v1/canonical-patterns.bin').read_bytes()
    entries=json.loads((checkpoint/'canonical-v1/entries.json').read_text())
    initial=struct.unpack('>'+str(len(canonical)//32)+'H',(residency/'pattern-to-tile.bin').read_bytes())
    used_near={r['pattern'] for e in entries if e['layer']==2 for r in e['references']}
    used_far={r['pattern'] for e in entries if e['layer']==3 for r in e['references']}
    expected_pool=sorted((tile,n) for n,tile in enumerate(initial)
                         if tile not in (0,65535) and n in used_far and n not in used_near)
    dynamic_slots={tile for tile,_ in expected_pool}
    expected_fixed=[65535 if tile in dynamic_slots else tile for tile in initial]
    far=next(x for x in catalog_layout['layers'] if x['layer']==3)
    ordered=sorted((e for e in entries if e['layer']==3),key=lambda e:(e['code'],e['attribute']))
    refs=[r for e in ordered for r in e['references']]
    expected_attrs=[r['attributes'] for r in refs]
    binding_paths=[catalog/'catalog.bin',ROOT/'willow.zip',checkpoint/'canonical-v1/source-palettes.bin',
                   checkpoint/'canonical-v1/cram.bin',checkpoint/'canonical-v1/canonical-patterns.bin',
                   residency/'resident-upload.bin']
    expected_bindings=b''.join(bytes.fromhex(sha(p)) for p in binding_paths)

    def parse(blob):
        assert len(blob)>=64
        # Read by independently specified byte offsets, not generator format/helpers.
        word=lambda at:struct.unpack_from('>H',blob,at)[0]
        long=lambda at:struct.unpack_from('>I',blob,at)[0]
        assert long(0)==0x57424631 and word(4)==1 and word(6)==64
        assert long(8)==len(blob) and long(12)==1
        assert long(16)==catalog_layout['origin'] and long(20)==len(cat)
        bo,bb=long(24),long(28)
        pc,kc,rc,oc=(word(at) for at in (32,34,36,38))
        po,ao,so,fo=(long(at) for at in (40,44,48,52))
        assert pc==len(initial) and kc==len(ordered) and rc==len(refs) and oc==len(expected_pool)
        assert long(56)==far['offsets']['pairs'] and long(60)==far['offsets']['refs']
        assert (bo,bb)==(64,192)
        regions=[(bo,bb),(po,pc*32),(ao,(rc+1)//2),(so,oc*4),(fo,pc*2)]
        at=64
        for offset,size in regions:
            assert offset==(at+3)&~3 and offset+size<=len(blob)
            assert blob[at:offset]==bytes(offset-at)
            at=offset+size
        assert at==len(blob)
        assert blob[bo:bo+bb]==expected_bindings
        assert blob[po:po+pc*32]==canonical
        attrs=[]
        for n in range(rc):
            value=blob[ao+n//2]
            attrs.append(((value//16 if n%2==0 else value%16)&15)*2048)
        assert attrs==expected_attrs
        if rc&1:assert blob[ao+rc//2]&15==0
        pool=list(struct.iter_unpack('>HH',blob[so:so+oc*4]));assert pool==expected_pool
        fixed=list(struct.unpack('>'+str(pc)+'H',blob[fo:fo+pc*2]));assert fixed==expected_fixed
        assert not (set(fixed)&dynamic_slots)
        for n,p in enumerate(refs):
            assert struct.unpack_from('>H',cat,far['offsets']['refs']+n*4)[0]==p['pattern']
        return {'bindings':bo,'patterns':po,'attributes':ao,'pool':so,'fixed_map':fo}

    raw=(resource/'far-stream.bin').read_bytes();offsets=parse(raw)
    # Imports are actual generated C definitions, not a second hand-built layout.
    header=(resource/'far_stream_imports.h').read_text()
    values={name:int(value,16) for name,value in re.findall(r'^#define WILLOW_BG_FAR_STREAM_(\w+) (0x[0-9a-f]+)UL$',header,re.M)}
    assert values['MAGIC']==0x57424631 and values['VERSION']==1 and values['HEADER_BYTES']==64
    assert values['ORIGIN']==layout['origin']==manifest['origin'] and values['BYTES']==len(raw)
    assert values['CATALOG_ORIGIN']==catalog_layout['origin'] and values['CATALOG_BYTES']==len(cat)
    assert values['PATTERN_COUNT']==len(initial) and values['PATTERN_STRIDE']==32
    assert values['PAIR_COUNT']==len(ordered) and values['REFERENCE_COUNT']==len(refs)
    assert values['POOL_COUNT']==len(expected_pool) and values['POOL_STRIDE']==4
    assert (values['POOL_TILE_OFFSET'],values['POOL_OWNER_OFFSET'],values['FIXED_MAP_STRIDE'],values['MISSING'])==(0,2,2,65535)
    assert values['CATALOG_PAIRS_ADDRESS']==catalog_layout['origin']+far['offsets']['pairs']
    assert values['CATALOG_REFS_ADDRESS']==catalog_layout['origin']+far['offsets']['refs']
    sizes={'bindings':192,'patterns':len(canonical),'attributes':(len(refs)+1)//2,'pool':len(expected_pool)*4,'fixed_map':len(initial)*2}
    for name,offset in offsets.items():
        assert values[name.upper()+'_OFFSET']==offset
        assert values[name.upper()+'_ADDRESS']==layout['origin']+offset
        assert values[name.upper()+'_BYTES']==sizes[name]
    binding_names=['CATALOG','SOURCE_ARCHIVE','SOURCE_PALETTES','CRAM','CANONICAL_PATTERNS','INITIAL_RESIDENT_IMAGE']
    for n,name in enumerate(binding_names):
        match=re.search(r'^#define WILLOW_BG_FAR_STREAM_'+name+r'_SHA256 "([0-9a-f]{64})"$',header,re.M)
        assert match and match.group(1)==expected_bindings[n*32:n*32+32].hex()
        assert values[name+'_SHA256_ADDRESS']==layout['origin']+64+n*32
    corruptions=[]
    def reject(name,data):
        try:parse(data)
        except (AssertionError,struct.error,IndexError):corruptions.append(name)
        else:raise AssertionError('corrupted data admitted: '+name)
    for at in range(64):
        bad=bytearray(raw);bad[at]^=1;reject('header_byte_'+str(at),bad)
    for name,at in offsets.items():
        for delta in sorted({0,sizes[name]//2,sizes[name]-1}):
            bad=bytearray(raw);bad[at+delta]^=1;reject(name+'_byte_'+str(delta),bad)
    reject('truncated',raw[:-1]);reject('extra_byte',raw+b'\0')
    for at in [offsets['attributes'],offsets['attributes']+len(refs)//2-1]:
        bad=bytearray(raw);bad[at]=(bad[at]>>4)|(bad[at]<<4&255)
        if bad!=raw:reject('swapped_attribute_nibbles_'+str(at),bad)
    # Decode both source flip identities independently from the exact catalog order.
    flip_examples=[]
    for attr in (0x0006,0x0026):
        ix=next(n for n,e in enumerate(ordered) if (e['code'],e['attribute'])==(0xc39,attr))
        pats={r['pattern'] for r in refs[ix*16:(ix+1)*16]}
        assert pats=={3024,*range(3198,3213)}
        flip_examples.append({'code':0xc39,'attribute':attr,'catalog_pair':ix,'patterns':sorted(pats)})
    proof=json.loads((resource/'proof.json').read_text())
    assert proof['original_source_fitted_pixel_checks']==len(ordered)*1024
    assert proof['native_palette_page_checks']==len(refs)
    assert len(proof['key_proofs'])==len(ordered)
    report={'scope':__doc__,'runtime_admitted':False,'binary_bytes':len(raw),
            'pattern_bytes_compared':len(canonical),'reference_attributes_compared':len(refs),
            'pool_entries_compared':len(expected_pool),'fixed_mapping_entries_compared':len(initial),
            'header_bytes_checked':64,'binding_bytes_checked':192,
            'generated_import_constants_checked':len(values),
            'corruptions_rejected':len(corruptions),'corruption_cases':corruptions,
            'both_0c39_source_flip_keys':flip_examples,
            'generator_original_source_fitted_pixel_checks':proof['original_source_fitted_pixel_checks'],
            'inputs':{str(p):sha(p) for p in [Path(__file__).resolve(),resource/'manifest.json',resource/'far-stream.bin',
                    resource/'far_stream_imports.h',resource/'layout.json',resource/'proof.json',*binding_paths,
                    catalog/'layout.json',residency/'pattern-to-tile.bin',checkpoint/'canonical-v1/entries.json']}}
    output.mkdir(parents=True);(output/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('inputs','corruption_cases')},indent=2))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--resource',type=Path,default=ROOT/'build/native-background-far-stream/prepared-v1')
    p.add_argument('--catalog',type=Path,default=ROOT/'build/native-background-catalog/forest-277-v1')
    p.add_argument('--residency',type=Path,default=ROOT/'build/native-background-residency/forest-277-v1')
    p.add_argument('--checkpoint',type=Path,default=ROOT/'build/native-route-background-audit/checkpoint-v1')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-background-far-stream/verified-v1')
    a=p.parse_args();verify(a.resource,a.catalog,a.residency,a.checkpoint,a.output)


if __name__=='__main__':main()
