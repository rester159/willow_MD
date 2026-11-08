#!/usr/bin/env python3
"""Read-only full-source-key residency and ownership audit; emits no target assets."""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import struct
import zipfile
from cps1_gfx import assemble_region, decode_tile

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--assets', type=Path, default=ROOT/'build/native-background-residency/forest-277-v1')
    p.add_argument('--target', type=Path, default=ROOT/'build/native-render-cadence/background-live-forest-v3')
    p.add_argument('--output', type=Path, default=ROOT/'build/native-far-residency-audit/v1')
    p.add_argument('--layer', type=int, default=3)
    p.add_argument('--code', type=lambda x:int(x,0), default=0xc39)
    p.add_argument('--attribute', type=lambda x:int(x,0), default=0x26)
    a = p.parse_args(); out = a.output.resolve(); assert not out.exists(); out.mkdir(parents=True)
    pins = {}
    def load(path):
        path=path.resolve(); raw=path.read_bytes(); pins[str(path)]=hashlib.sha256(raw).hexdigest(); return raw
    def js(path):return json.loads(load(path))
    key=(a.layer,a.code,a.attribute)
    base=ROOT/'build/native-route-background-audit/checkpoint-v1'
    checkpoint=js(base/'checkpoint.json')
    canonical=js(base/'canonical-v1/entries.json'); raw=load(base/'canonical-v1/canonical-patterns.bin')
    prepared=js(base/'prepared-v2/entries.json')
    for name in ['canonical-v1/entries.json','canonical-v1/canonical-patterns.bin','prepared-v2/entries.json']:
        assert pins[str(base/name)]==checkpoint['files'][name]
    assets=a.assets.resolve(); manifest=js(assets/'manifest.json')
    entries=js(assets/'entries.json'); mapping=load(assets/'pattern-to-tile.bin'); physical=load(assets/'physical-tiles.bin')
    owners=js(assets/'physical-tiles.json')
    for n in ['entries.json','pattern-to-tile.bin','physical-tiles.bin','physical-tiles.json']:
        assert pins[str(assets/n)]==manifest['outputs'][n]
    assert len(raw)%32==0 and len(mapping)==len(raw)//16 and len(physical)%34==0
    patterns=[raw[i:i+32] for i in range(0,len(raw),32)]
    slots=list(struct.unpack('>'+str(len(patterns))+'H',mapping))
    physical_patterns={struct.unpack_from('>H',physical,i)[0]:physical[i+2:i+34] for i in range(0,len(physical),34)}
    by_bytes=defaultdict(list)
    for tile,data in physical_patterns.items():by_bytes[data].append(tile)
    def keyed(items):return {(e['layer'],e['code'],e['attribute']):e for e in items}
    item=keyed(canonical)[key]; current=keyed(entries)[key]; fitted=keyed(prepared)[key]
    assert len(item['references'])==len(current['references'])==(item['size']//8)**2
    usage=defaultdict(set); layers=defaultdict(set)
    for e in canonical:
        identity=(e['layer'],e['code'],e['attribute'])
        for r in e['references']:
            usage[r['pattern']].add(identity)
            if r['pattern']:layers[e['layer']].add(r['pattern'])
    refs=[]
    for n,(r,c) in enumerate(zip(item['references'],current['references'])):
        assert r['pattern']==c['pattern']
        expected=0xffff if slots[r['pattern']]==0xffff else slots[r['pattern']]|r['attributes']
        assert c['word']==expected
        refs.append({'displayed_subtile':n,**r,'physical_tile':slots[r['pattern']],
                     'current_word':c['word'],'physical_byte_aliases':by_bytes[patterns[r['pattern']]]})
    source=ROOT/'willow.zip'; load(source); load(ROOT/'tools/cps1_gfx.py')
    assert pins[str(source)]==checkpoint['external_inputs'][str(source)]
    assert pins[str(ROOT/'tools/cps1_gfx.py')]==checkpoint['files']['tools/cps1_gfx.py']
    with zipfile.ZipFile(source) as z:gfx=assemble_region(z)
    image=decode_tile(gfx,fitted['physical_code'],item['size']); pixels=0
    for n,r in enumerate(item['references']):
        tile=patterns[r['pattern']]; tx=(n%(item['size']//8))*8; ty=(n//(item['size']//8))*8
        for y in range(8):
            for x in range(8):
                sx=tx+x;sy=ty+y
                pen=image[item['size']-1-sy if a.attribute&64 else sy][item['size']-1-sx if a.attribute&32 else sx]
                nx=7-x if r['attributes']&0x800 else x;ny=7-y if r['attributes']&0x1000 else y
                native=(tile[ny*4+nx//2]>>(0 if nx&1 else 4))&15
                assert native==fitted['pen_maps'][n][pen] and ((native==0)==(pen==15))
                pixels+=1
    required={r['pattern'] for r in item['references'] if r['pattern']}
    missing={i for i in required if slots[i]==0xffff}
    resident={i for i,s in enumerate(slots) if i and s!=0xffff}
    far_only=(resident&layers[3])-layers[2]
    target=js(a.target.resolve()/'manifest.json'); symbols=target['symbols']
    blob=load(a.target.resolve()/'background-resources.bin')
    assert hashlib.sha256(blob).hexdigest()==target['extra_rom']['sha256']
    # Missing native attributes are absent from WBC1 refs, and source orientation
    # is not canonical fitted H/V/palette orientation. A future streaming ABI
    # must retain these attributes separately from physical residency.
    catalog=ROOT/'build/native-background-catalog/forest-277-v1'
    layout=js(catalog/'layout.json'); catalog_raw=load(catalog/'catalog.bin')
    layer=next(x for x in layout['layers'] if x['layer']==a.layer)
    pairs=layer['offsets']['pairs']; found=[]
    for n in range(layer['pair_count']):
        packed,at=struct.unpack_from('>IH',catalog_raw,pairs+n*6)
        if packed==a.code<<16|a.attribute:found.append(at)
    assert len(found)==1
    for n,r in enumerate(refs):
        assert struct.unpack_from('>HH',catalog_raw,layer['offsets']['refs']+(found[0]+n)*4)==(r['pattern'],r['current_word'])
    report={'scope':__doc__,'source_key':{'layer':a.layer,'code':a.code,'attribute':a.attribute},
       'references':refs,'distinct_patterns':sorted(required),'resident_patterns':sorted(required-missing),
       'missing_patterns':sorted(missing),'full_key_pattern_bytes':len(required)*32,'missing_pattern_bytes':len(missing)*32,
       'independent_original_source_fitted_pixels':pixels,
       'shared_keys':{str(i):[{'layer':l,'code':c,'attribute':a} for l,c,a in sorted(usage[i])] for i in sorted(required)},
       'current_allocation':{'physical_slots':len(physical_patterns),'background_canonical':len(resident),
         'overlay_patterns':manifest['new_overlay_physical_patterns'],'spare_slots':manifest['unused_bg_slots'],
         'full_key_addition_without_eviction':len(physical_patterns)+len(missing),
         'reserved':manifest['reserved_tiles'],'far_only_resident_patterns':len(far_only),
         'near_resident_patterns':len(resident&layers[2]),'shared_near_far_patterns':len(resident&layers[2]&layers[3]),
         'far_only_candidates_scope':'Resource identity only, NOT evidence these slots are safe to evict from displayed/prepared state.'},
       'far_only_resident_candidates':[{'pattern':i,'physical_tile':slots[i]} for i in sorted(far_only)],
       'target_constraints':{'bss_bytes':symbols['__render_bss_end']-symbols['__render_bss_begin'],
          'bss_spare_bytes':symbols['__render_bss_limit']-symbols['__render_bss_end'],
          'resource_end_exclusive':target['extra_rom']['origin']+len(blob),
          'all_canonical_pattern_payload_bytes':len(raw),
          'missing_native_attributes_not_in_catalog_refs':True,
          'current_background_residency_epoch':False},
       'target_modified':False,'runtime_residency_admitted':False,'inputs':pins}
    load(Path(__file__))
    assert all(hashlib.sha256(Path(n).read_bytes()).hexdigest()==d for n,d in pins.items())
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report[k] for k in ['source_key','distinct_patterns','resident_patterns','missing_pattern_bytes','independent_original_source_fitted_pixels','current_allocation','target_constraints']},indent=2))

if __name__=='__main__':main()
