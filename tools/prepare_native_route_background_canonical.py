#!/usr/bin/env python3
"""Lossless native H/V canonical resources from the source-keyed BG catalog.

Each ref stores separate16-bit ROM pattern ID and native H/V/page attributes.
No physical VRAM tile address, captured map, coordinate or timeline is emitted.
Source pen masks are presence sets for the displayed8x8 source subtile; they
are invariant under the canonical H/V transform and are NOT pixel bitmaps.
Original source-subtile/local-flip metadata is emitted separately.
"""
from collections import Counter
from functools import lru_cache
from pathlib import Path
import argparse
import json
import struct
import zipfile
from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from prepare_native_route_background import ROOT, sha, physical_code


def pen(pattern, x, y):
    return (pattern[y*4+x//2] >> (0 if x&1 else 4))&15


def transformed(pattern, flags):
    return genesis_tiles([[pen(pattern,7-x if flags&1 else x,7-y if flags&2 else y)
                           for x in range(8)] for y in range(8)])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--assets',type=Path,default=ROOT/'build/native-route-background-audit/prepared-v2')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-route-background-audit/canonical-v1')
    a=p.parse_args();assets=a.assets.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    source_manifest=json.loads((assets/'manifest.json').read_text())
    for name,digest in source_manifest['outputs'].items():assert sha(assets/name)==digest,name
    raw=(assets/'background-patterns.bin').read_bytes();assert len(raw)%32==0
    patterns=[raw[at:at+32] for at in range(0,len(raw),32)]
    descriptors=json.loads((assets/'entries.json').read_text())
    canonical=[];index={};mapping=[]
    for pattern in patterns:
        stored,flags=min((transformed(pattern,f),f) for f in range(4))
        if stored not in index:index[stored]=len(canonical);canonical.append(stored)
        mapping.append((index[stored],flags))
        assert all(pen(stored,7-x if flags&1 else x,7-y if flags&2 else y)==pen(pattern,x,y)
                   for y in range(8) for x in range(8))
    assert len(canonical)<65536 and canonical[0]==bytes(32)
    allflagchecks=0
    for original in canonical:
        for flags in range(4):
            variant=transformed(original,flags)
            for y in range(8):
                for x in range(8):
                    assert pen(variant,x,y)==pen(original,7-x if flags&1 else x,7-y if flags&2 else y)
                    assert pen(variant,7-x if flags&1 else x,7-y if flags&2 else y)==pen(original,x,y)
                    allflagchecks+=1
    with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
    @lru_cache(None)
    def image(layer,code):
        size=16 if layer==2 else 32;physical=physical_code(layer,code)
        return decode_tile(gfx,physical,size) if physical is not None else [[15]*size for _ in range(size)]
    header='''/* Source-keyed ROM patterns, no physical residency or captured map data. */
#ifndef WILLOW_ROUTE_BACKGROUND_CANONICAL_H
#define WILLOW_ROUTE_BACKGROUND_CANONICAL_H
#include <stdint.h>
typedef struct { uint32_t key; uint16_t offset; } WillowRouteBackgroundCanonicalPair;
typedef struct { uint16_t pattern; uint16_t attributes; } WillowRouteBackgroundCanonicalRef;
/* attributes contains only VDP page and H/V bits. Resolve pattern residency,
 * then OR attributes with the validated physical tile. No priority is assigned.
 * SourcePens is pen presence, not a pixel bitmap. SourceSubtile identifies the
 * original raw tile's row-major8x8, SourceFlips uses H=1/V=2 before canonicalization.
 * Canonical H/V attributes restore the source-displayed subtile. */
'''
    header+=f'#define WILLOW_ROUTE_BACKGROUND_CANONICAL_PATTERN_COUNT {len(canonical)}\n'
    converted=[];checks=0;source_mask_checks=0;ref_flags=Counter();serialized=bytearray()
    for layer in (2,3):
        keys=[];refs=[];masks=[];subtiles=[];sourceflips=[]
        for d in descriptors:
            if d['layer']!=layer:continue
            code,attr=d['code'],d['attribute'];size=d['size'];n=size//8
            original=image(layer,code);keys.append((code<<16|attr,len(refs)))
            local=[]
            for tile,oldref in enumerate(d['references']):
                pattern,flags=mapping[oldref&16383]
                native_attrs=((oldref>>14)<<13)|((flags&1)<<11)|((flags&2)<<11)
                assert native_attrs&~0x7800==0
                tx,ty=tile%n,tile//n
                rawtx=n-1-tx if attr&32 else tx;rawty=n-1-ty if attr&64 else ty
                rawsub=rawty*n+rawtx;rawflips=(attr>>5)&3
                mask=0
                for y in range(8):
                    for x in range(8):
                        sourcepen=original[rawty*8+(7-y if rawflips&2 else y)][rawtx*8+(7-x if rawflips&1 else x)]
                        canonicalpen=pen(canonical[pattern],7-x if native_attrs&0x800 else x,7-y if native_attrs&0x1000 else y)
                        assert canonicalpen==d['pen_maps'][tile][sourcepen]
                        assert (canonicalpen==0)==(sourcepen==15)
                        if sourcepen!=15:mask|=1<<sourcepen
                        checks+=1
                assert mask==d['source_pen_masks'][tile];source_mask_checks+=1
                refs.append((pattern,native_attrs));masks.append(mask);subtiles.append(rawsub);sourceflips.append(rawflips)
                serialized.extend(struct.pack('>HHHBB',pattern,native_attrs,mask,rawsub,rawflips))
                local.append({'pattern':pattern,'attributes':native_attrs,'source_pen_mask':mask,
                              'source_subtile':rawsub,'source_flips':rawflips})
                ref_flags[flags]+=1
            converted.append({'layer':layer,'code':code,'attribute':attr,'size':size,'references':local})
        assert len(refs)<65536
        prefix=f'routeBackgroundCanonical{layer}'
        header+=f'#define WILLOW_ROUTE_BACKGROUND_CANONICAL_{layer}_PAIR_COUNT {len(keys)}\n'
        header+=f'static const WillowRouteBackgroundCanonicalPair {prefix}Pairs[]={{'+','.join('{%dUL,%d}'%v for v in keys)+'};\n'
        header+=f'static const WillowRouteBackgroundCanonicalRef {prefix}Refs[]={{'+','.join('{%d,%d}'%v for v in refs)+'};\n'
        for name,typ,values in [('SourcePens','uint16_t',masks),('SourceSubtile','uint8_t',subtiles),('SourceFlips','uint8_t',sourceflips)]:
            header+=f'static const {typ} {prefix}{name}[]={{'+','.join(map(str,values))+'};\n'
    header+='#endif\n'
    # Re-read emitted binary fields and independently reconstruct all oldrefs.
    at=0
    for d in descriptors:
        # Descriptors are layer-major in the admitted source catalog.
        for tile,oldref in enumerate(d['references']):
            pattern,attrs,mask,subtile,flips=struct.unpack_from('>HHHBB',serialized,at);at+=8
            assert attrs>>13==oldref>>14
            assert mask==d['source_pen_masks'][tile]
            for y in range(8):
                for x in range(8):
                    assert pen(canonical[pattern],7-x if attrs&0x800 else x,7-y if attrs&0x1000 else y)==pen(patterns[oldref&16383],x,y)
    assert at==len(serialized)
    (out/'route_background_canonical.h').write_text(header)
    (out/'canonical-patterns.bin').write_bytes(b''.join(canonical))
    (out/'canonical-map.json').write_text(json.dumps(mapping)+'\n')
    (out/'entries.json').write_text(json.dumps(converted,separators=(',',':'))+'\n')
    (out/'references.bin').write_bytes(serialized)
    for name in ['source-palettes.bin','cram.bin']:(out/name).write_bytes((assets/name).read_bytes())
    outputs=['route_background_canonical.h','canonical-patterns.bin','canonical-map.json','entries.json','references.bin','source-palettes.bin','cram.bin']
    report={'scope':'Lossless H/V canonicalization of preconverted route resources. No residency, native rendering, source map write hook or timing admission.',
            'source_patterns':len(patterns),'canonical_patterns':len(canonical),'pattern_bytes':32*len(canonical),
            'source_keys':len(descriptors),'references':source_mask_checks,'original_source_pixel_checks':checks,
            'all_four_flags_pixel_roundtrip_checks':allflagchecks,'source_pattern_roundtrip_checks':64*len(patterns),
            'serialized_reference_pixel_roundtrip_checks':checks,'reference_flags':dict(ref_flags),
            'native_reference':'Separate uint16 pattern ID and uint16 attributes; attrs contains page bits13..14/Hbit11/Vbit12 only. OR with validated resident tile; priority belongs to current source composition.',
            'source_metadata':'Original source key includes all attr bits. Per-reference pen presence mask is independent of H/V; raw source-subtile index and source local flip flags make pixel orientation explicit. These are not spatial/captured map coordinates.',
            'serialized_reference':'Big-endian u16 pattern,u16 attributes,u16 sourcepenmask,u8 rawsourcesubtile,u8 sourceflips, ordered by source layer/code/attr then displayed row-major subtile.',
            'hashes':{str(path):sha(path) for path in [Path(__file__).resolve(),ROOT/'tools/prepare_native_route_background.py',ROOT/'tools/cps1_gfx.py',ROOT/'willow.zip',assets/'manifest.json',assets/'entries.json',assets/'background-patterns.bin',assets/'source-palettes.bin',assets/'cram.bin']},
            'outputs':{name:sha(out/name) for name in outputs}}
    (out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ['hashes','outputs']},indent=2))

if __name__=='__main__':main()
