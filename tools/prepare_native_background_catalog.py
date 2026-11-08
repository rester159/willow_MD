#!/usr/bin/env python3
"""Compile source-keyed native BG references and exact opacity into WBC1 ROM.

Input is the frozen, partial residency catalog. No source map, coordinate,
frame number or timing sequence enters this blob. Every binary field is BE
and has an explicit stride; generated imports need no packed C structures.
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

ROOT=Path(__file__).resolve().parents[1]
MAGIC=0x57424331


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def pen(data,x,y):return (data[y*4+x//2]>>(0 if x&1 else 4))&15

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--residency',type=Path,default=ROOT/'build/native-background-residency/checkpoint-v1/prepared-v1')
    p.add_argument('--background-checkpoint',type=Path,default=ROOT/'build/native-route-background-audit/checkpoint-v1')
    p.add_argument('--origin',type=lambda v:int(v,0),default=0x120000)
    p.add_argument('--output',type=Path,default=ROOT/'build/native-background-catalog/prepared-v1')
    a=p.parse_args();residency=a.residency.resolve();background=a.background_checkpoint.resolve();out=a.output.resolve()
    out.mkdir(parents=True,exist_ok=True);assert a.origin%4==0
    resident_manifest=json.loads((residency/'manifest.json').read_text())
    for name,digest in resident_manifest['outputs'].items():assert sha(residency/name)==digest,name
    frozen=json.loads((background/'checkpoint.json').read_text())
    for name,digest in frozen['files'].items():assert sha(background/name)==digest,name
    assert sha(ROOT/'tools/cps1_gfx.py')==frozen['files']['tools/cps1_gfx.py']
    assert sha(ROOT/'willow.zip')==frozen['external_inputs'][str(ROOT/'willow.zip')]
    entries=json.loads((residency/'entries.json').read_text())
    overlays=json.loads((residency/'overlays.json').read_text())
    original_refs={(d['layer'],d['code'],d['attribute']):d for d in json.loads((background/'canonical-v1/entries.json').read_text())}
    canonical=(background/'canonical-v1/canonical-patterns.bin').read_bytes()
    physical_raw=(residency/'physical-tiles.bin').read_bytes();assert len(physical_raw)%34==0
    physical={struct.unpack_from('>H',physical_raw,n)[0]:physical_raw[n+2:n+34] for n in range(0,len(physical_raw),34)}
    physical[0]=bytes(32)
    with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
    @lru_cache(None)
    def image(layer,code):
        size=16 if layer==2 else 32
        physical_code=(0x4000+code if code<=0x1fff else None) if layer==2 else (code&0x3fff if 0xa00<=(code&0x3fff)<=0xdff else None)
        return decode_tile(gfx,physical_code,size) if physical_code is not None else [[15]*size for _ in range(size)]
    blob=bytearray(96);regions=[]
    def append(name,data):
        while len(blob)%4:blob.append(0)
        offset=len(blob);blob.extend(data);regions.append({'name':name,'offset':offset,'bytes':len(data)})
        return offset
    layers=[];opaque_counts=Counter();pixel_checks=0;resident_pixel_checks=0;expected_layers=[]
    for layer,size in [(2,16),(3,32)]:
        selected=[d for d in entries if d['layer']==layer]
        keys=[d['code']<<16|d['attribute'] for d in selected]
        assert keys==sorted(set(keys))
        pairs=bytearray();refs=bytearray();pens=bytearray();opacity=bytearray();orientation=bytearray();flags=bytearray()
        offset=0;expected=[]
        for d in selected:
            code,attr=d['code'],d['attribute'];pairs.extend(struct.pack('>IH',code<<16|attr,offset))
            source=image(layer,code);n=size//8
            assert len(d['references'])==n*n
            for subtile,ref in enumerate(d['references']):
                base_ref=original_refs[layer,code,attr]['references'][subtile]
                assert ref['pattern']==base_ref['pattern']
                refs.extend(struct.pack('>HH',ref['pattern'],ref['word']))
                pens.extend(struct.pack('>H',ref['source_pen_mask']))
                orientation.extend(bytes([ref['source_subtile'],ref['source_flips']]))
                rows=[];source_pens=0;tile=canonical[ref['pattern']*32:(ref['pattern']+1)*32]
                for y in range(8):
                    row=0
                    for x in range(8):
                        sx,sy=subtile%n*8+x,subtile//n*8+y
                        source_pen=source[size-1-sy if attr&64 else sy][size-1-sx if attr&32 else sx]
                        opaque=source_pen!=15
                        native_pen=pen(tile,7-x if base_ref['attributes']&0x800 else x,7-y if base_ref['attributes']&0x1000 else y)
                        assert opaque==(native_pen!=0)
                        if opaque:row|=0x80>>x;source_pens|=1<<source_pen
                        if ref['word']!=0xffff:
                            word=ref['word'];assert word&0x7800==base_ref['attributes']
                            actual=pen(physical[word&0x7ff],7-x if word&0x800 else x,7-y if word&0x1000 else y)
                            assert actual==native_pen;resident_pixel_checks+=1
                        pixel_checks+=1
                    rows.append(row)
                assert source_pens==ref['source_pen_mask']
                mask=bytes(rows);opacity.extend(mask)
                flag=1 if mask==b'\xff'*8 else 2 if mask==bytes(8) else 0
                flags.append(flag);opaque_counts['full'if flag==1 else 'empty'if flag==2 else 'mixed']+=1
                expected.append((ref,mask,flag));offset+=1
        arrays={'pairs':pairs,'refs':refs,'source_pens':pens,'opacity':opacity,'orientation':orientation,'flags':flags}
        offsets={name:append(f'layer{layer}_{name}',data) for name,data in arrays.items()}
        record={'layer':layer,'pair_count':len(selected),'reference_count':offset,'subtiles_per_pair':(size//8)**2,'offsets':offsets,'addresses':{name:a.origin+at for name,at in offsets.items()}}
        layers.append(record);expected_layers.append((selected,expected))
        struct.pack_into('>4H6I',blob,32+(layer-2)*32,layer,len(selected),offset,(size//8)**2,*(offsets[name]for name in arrays))
    overlay_data=bytearray();overlay_pixel_checks=0
    for overlay in overlays:
        code,attr,subtile,mask=(overlay[k]for k in ['code','attribute','subtile','priority_mask'])
        original=image(2,code);rows=[];word=overlay['word'];native=physical[word&0x7ff]
        for y in range(8):
            row=0
            for x in range(8):
                sx,sy=subtile%2*8+x,subtile//2*8+y
                source_pen=original[15-sy if attr&64 else sy][15-sx if attr&32 else sx]
                opaque=source_pen!=15 and bool(mask&(1<<source_pen))
                actual=pen(native,7-x if word&0x800 else x,7-y if word&0x1000 else y)
                assert opaque==(actual!=0)
                if opaque:row|=0x80>>x
                overlay_pixel_checks+=1
            rows.append(row)
        overlay_data.extend(struct.pack('>I5H2B',code<<16|attr,subtile,mask,word,overlay['source_pen_mask'],overlay['retained_source_pen_mask'],overlay['source_subtile'],overlay['source_flips'])+bytes(rows))
    assert len(overlay_data)==24*len(overlays)
    overlay_offset=append('overlays',overlay_data)
    total=len(blob)
    struct.pack_into('>IHHIIHHIII',blob,0,MAGIC,1,2,total,32,len(overlays),24,overlay_offset,1,0)
    # Parse compile-independent binary arrays and reconstruct every key and mask.
    assert struct.unpack_from('>IHHIIHHIII',blob)==(MAGIC,1,2,total,32,len(overlays),24,overlay_offset,1,0)
    for index,(selected,expected) in enumerate(expected_layers):
        layer,pair_count,ref_count,subtiles,*offsets=struct.unpack_from('>4H6I',blob,32+index*32)
        pairs_at,refs_at,pens_at,opacity_at,orientation_at,flags_at=offsets
        assert (layer,pair_count,ref_count)==(index+2,len(selected),len(expected))
        for n,d in enumerate(selected):assert struct.unpack_from('>IH',blob,pairs_at+n*6)==(d['code']<<16|d['attribute'],n*subtiles)
        for n,(ref,mask,flag) in enumerate(expected):
            assert struct.unpack_from('>HH',blob,refs_at+n*4)==(ref['pattern'],ref['word'])
            assert struct.unpack_from('>H',blob,pens_at+n*2)[0]==ref['source_pen_mask']
            assert blob[opacity_at+n*8:opacity_at+(n+1)*8]==mask
            assert blob[orientation_at+n*2:orientation_at+n*2+2]==bytes([ref['source_subtile'],ref['source_flips']])
            assert blob[flags_at+n]==flag
    assert blob[overlay_offset:]==overlay_data
    for i,region in enumerate(regions):
        assert region['offset']%4==0 and region['offset']+region['bytes']<=total
        if i:assert region['offset']>=regions[i-1]['offset']+regions[i-1]['bytes']
    header='''/* Generated WBC1 fixed ROM imports. All arrays are explicitly big-endian.
 * No C struct packing, captured maps, coordinates or timing sequence is used.
 * Opacity byte y describes displayed row y: bit7=left, bit0=right. Masks are
 * already in original source-displayed orientation; do NOT apply word H/V again.
 * Nativeword0xffff is missing, not transparent. Source-pair offset counts refs.
 */
#ifndef WILLOW_NATIVE_BACKGROUND_CATALOG_IMPORTS_H
#define WILLOW_NATIVE_BACKGROUND_CATALOG_IMPORTS_H
#include <stdint.h>
#define WILLOW_BG_CATALOG_MAGIC 0x57424331UL
#define WILLOW_BG_CATALOG_VERSION 1U
#define WILLOW_BG_CATALOG_PAIR_STRIDE 6U
#define WILLOW_BG_CATALOG_REF_STRIDE 4U
#define WILLOW_BG_CATALOG_OPACITY_STRIDE 8U
#define WILLOW_BG_CATALOG_OVERLAY_STRIDE 24U
#define WILLOW_BG_CATALOG_FULL_OPAQUE 1U
#define WILLOW_BG_CATALOG_EMPTY 2U
#define WILLOW_BG_CATALOG_MISSING 0xffffU
'''
    header+=f'#define WILLOW_BG_CATALOG_ROM_BASE 0x{a.origin:08x}UL\n#define WILLOW_BG_CATALOG_TOTAL_BYTES {total}UL\n'
    header+=f'#define WILLOW_BG_CATALOG_DIRECTORY_ADDRESS 0x{a.origin+32:08x}UL\n'
    for layer in layers:
        prefix=f'WILLOW_BG_CATALOG_LAYER{layer["layer"]}'
        for name in ['pair_count','reference_count','subtiles_per_pair']:header+=f'#define {prefix}_{name.upper()} {layer[name]}U\n'
        for name,address in layer['addresses'].items():header+=f'#define {prefix}_{name.upper()}_ADDRESS 0x{address:08x}UL\n'
    header+=f'#define WILLOW_BG_CATALOG_OVERLAY_COUNT {len(overlays)}U\n#define WILLOW_BG_CATALOG_OVERLAYS_ADDRESS 0x{a.origin+overlay_offset:08x}UL\n#endif\n'
    layout={'magic':'0x57424331','version':1,'origin':a.origin,'total_bytes':total,'end_address_exclusive':a.origin+total,'header_format':'>IHHIIHHIII','header_fields':['magic','version','layerCount','totalBytes','layerDirectoryOffset','overlayCount','overlayStride','overlayOffset','formatFlags','reserved'],
            'layer_directory_offset':32,'layer_directory_stride':32,'layer_directory_format':'>4H6I','layer_directory_fields':['layer','pairCount','referenceCount','subtilesPerPair','pairsOffset','refsOffset','sourcePensOffset','opacityOffset','orientationOffset','flagsOffset'],
            'layers':layers,'overlay_offset':overlay_offset,'overlay_count':len(overlays),'overlay_stride':24,
            'overlay_fields':{'key_u32':0,'subtile_u16':4,'priorityMask_u16':6,'word_u16':8,'sourcePens_u16':10,'retainedPens_u16':12,'rawSubtile_u8':14,'sourceFlips_u8':15,'opacity_8bytes':16},'regions':regions,
            'opacity_format':'Eight row bytes in displayed source orientation. Row0 first, bit7 left. Big-endian64 interpretation has top-left bit63. Word H/V bits were already accounted for; do not flip mask again.',
            'flags_format':'Bit0 FULL_OPAQUE, bit1 EMPTY, neither means mixed. Other bits zero.'}
    (out/'catalog.bin').write_bytes(blob);(out/'catalog_imports.h').write_text(header);(out/'layout.json').write_text(json.dumps(layout,indent=2)+'\n')
    subprocess.run(['m68k-elf-gcc','-m68000','-std=c99','-ffreestanding','-x','c','-fsyntax-only',str(out/'catalog_imports.h')],check=True)
    report={'scope':'Fixed-ROM source-key lookup and exact original opacity metadata. Explicit partial native residency. No live maps, camera, source timing, VDP output or FPS admission.',
            'origin':a.origin,'blob_bytes':total,'end_address_exclusive':a.origin+total,'pair_count':sum(x['pair_count']for x in layers),'reference_count':sum(x['reference_count']for x in layers),
            'opacity_counts':dict(opaque_counts),'original_opacity_pixel_checks':pixel_checks,'resident_native_pixel_checks':resident_pixel_checks,'overlay_original_opacity_pixel_checks':overlay_pixel_checks,
            'roundtrip_keys':sum(x['pair_count']for x in layers),'roundtrip_references':sum(x['reference_count']for x in layers),'overlay_count':len(overlays),'native_imports_compile':'m68k-elf-gcc -m68000 -std=c99 -ffreestanding -fsyntax-only passed',
            'hashes':{str(path):sha(path) for path in [Path(__file__).resolve(),ROOT/'tools/cps1_gfx.py',ROOT/'willow.zip',residency/'manifest.json',residency/'entries.json',residency/'overlays.json',residency/'physical-tiles.bin',background/'checkpoint.json',background/'canonical-v1/entries.json',background/'canonical-v1/canonical-patterns.bin']},
            'outputs':{name:sha(out/name)for name in ['catalog.bin','catalog_imports.h','layout.json']}}
    (out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items()if k not in ['hashes','outputs']},indent=2))

if __name__=='__main__':main()
