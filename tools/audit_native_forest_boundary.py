#!/usr/bin/env python3
"""Find missing source resource identities in a captured forest refusal.

Far demand is proved per original pixel after original near transparency.
Guard-column near demand is reported separately from the visible crop.
This is read-only evidence, not permission to hide or replace missing art.
"""
import argparse
from collections import Counter
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import struct
import zipfile

from cps1_gfx import assemble_region, decode_tile
from prepare_native_route_background import cell_index, physical_code
from test_native_background_context import REGIONS

ROOT=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
word=lambda p,n:struct.unpack_from('>H',p,n)[0]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture',type=Path,required=True)
    p.add_argument('--assets',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();capture=a.capture.resolve();assets=a.assets.resolve();out=a.output.resolve()
    assert not out.exists();out.parent.mkdir(parents=True,exist_ok=True)
    pins={}
    def pin(path):pins[str(path.resolve())]=sha(path);return path.read_bytes()
    meta=json.loads(pin(capture));raw=pin(capture.with_name(capture.stem+'-source.bin'))
    offset=0;regions={}
    for name,size,address in REGIONS:regions[name]=raw[offset:offset+size];offset+=size
    assert len(raw)==offset==123392
    entries=json.loads(pin(assets/'entries.json'));lookup={(d['layer'],d['code'],d['attribute']):d for d in entries}
    assert len(lookup)==len(entries)
    zpath=ROOT/'willow.zip';pin(zpath)
    with zipfile.ZipFile(zpath) as z:gfx=assemble_region(z)
    @lru_cache(None)
    def image(layer,code):
        size=16 if layer==2 else 32;physical=physical_code(layer,code)
        return decode_tile(gfx,physical,size) if physical is not None else [[15]*size for _ in range(size)]
    def source(layer,x,y):
        size=16 if layer==2 else 32;x%=size*64;y%=size*64
        at=cell_index(layer,x//size,y//size)*4;data=regions[f'scroll{layer}']
        code,attr=word(data,at),word(data,at+2);sub=(y%size//8)*(size//8)+x%size//8
        tile=image(layer,code);pen=tile[size-1-y%size if attr&64 else y%size][size-1-x%size if attr&32 else x%size]
        return (layer,code,attr,sub),pen,(0x918000 if layer==2 else 0x914000)+at
    def detail(key,address,count):
        layer,code,attr,sub=key;item=lookup.get((layer,code,attr));ref=item['references'][sub] if item else None
        return dict(layer=layer,code=code,attribute=attr,subtile=sub,source_address=address,
                    catalog_key_present=item is not None,pattern=ref['pattern'] if ref else None,
                    native_word=ref['word'] if ref else None,required_pixels_or_cells=count)
    v=meta['views'];near=Counter();near_addresses={}
    nx=(((96+v[0])>>3)-1)&127;ny=((16+v[1])>>3)&127
    for dy in range(28+bool(v[1]&7)):
        for dx in range(43):
            key,pen,address=source(2,((nx+dx)&127)*8,((ny+dy)&127)*8)
            item=lookup.get(key[:3]);ref=item['references'][key[3]] if item else None
            if ref is None or ref['word']==65535:near[key]+=1;near_addresses[key]=address
    far=Counter();far_addresses={};transparent_near=0;required_far=Counter()
    for y in range(224):
        for x in range(320):
            _,pen,_=source(2,96+v[0]+x,16+v[1]+y)
            if pen!=15:continue
            transparent_near+=1
            key,far_pen,address=source(3,96+v[2]+x,16+v[3]+y)
            if far_pen==15:continue
            required_far[key]+=1
            item=lookup.get(key[:3]);ref=item['references'][key[3]] if item else None
            if ref is None or ref['word']==65535:far[key]+=1;far_addresses[key]=address
    report={'scope':__doc__,'capture':str(capture),'frame':meta['frame'],'active_input_ticks':meta['active_input_ticks'],
            'status':meta['status'],'views':v,'source_pixels_tested':320*224,'near_transparent_visible_pixels':transparent_near,
            'visible_far_pixels_required':sum(required_far.values()),
            'missing_guard_near':[detail(k,near_addresses[k],n) for k,n in sorted(near.items())],
            'missing_exposed_far':[detail(k,far_addresses[k],n) for k,n in sorted(far.items())],
            'target_admitted':False,'inputs':pins}
    report['inputs'][str(Path(__file__).resolve())]=sha(Path(__file__))
    assert all(sha(Path(n))==d for n,d in report['inputs'].items())
    out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='inputs'},indent=2))


if __name__=='__main__':main()
