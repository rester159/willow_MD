#!/usr/bin/env python3
"""Build immutable, explicitly partial native BG residency from frozen resources.

No captured maps, coordinates, camera sequence or physical actor data is emitted.
The guarded required resource set selects source-derived patterns, not behavior.
All unresident references are explicit. Runtime source opacity, palette, view,
priority, occupancy and publication admission remain the renderer's duties.
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
from cps1_gfx import assemble_region, decode_tile, genesis_tiles

ROOT=Path(__file__).resolve().parents[1]
MISSING=0xffff


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def pixel(pattern,x,y):return (pattern[y*4+x//2]>>(0 if x&1 else 4))&15

def flip(pattern,flags):
    return genesis_tiles([[pixel(pattern,7-x if flags&1 else x,7-y if flags&2 else y)
                           for x in range(8)] for y in range(8)])

def original_code(layer,code):
    if layer==2:return 0x4000+code if code<=0x1fff else None
    code&=0x3fff
    return code if 0xa00<=code<=0xdff else None

def native_attributes(page,flags):return (page<<13)|((flags&1)<<11)|((flags&2)<<11)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--checkpoint',type=Path,default=ROOT/'build/native-route-background-audit/checkpoint-v1')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-background-residency/prepared-v1')
    p.add_argument('--include-source-keys',type=Path,help='JSON list of complete layer/code/attribute resource identities; include every subtile, never captured positions')
    a=p.parse_args();checkpoint=a.checkpoint.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    provenance=json.loads((checkpoint/'checkpoint.json').read_text())
    for name,digest in provenance['files'].items():assert sha(checkpoint/name)==digest,name
    # Imported original graphics decoder and original graphics must be exactly
    # those used in the frozen independent numeric proof.
    assert sha(ROOT/'tools/cps1_gfx.py')==provenance['files']['tools/cps1_gfx.py']
    assert sha(ROOT/'willow.zip')==provenance['external_inputs'][str(ROOT/'willow.zip')]
    canonical=checkpoint/'canonical-v1';baseline=checkpoint/'prepared-v2'
    descriptors=json.loads((canonical/'entries.json').read_text())
    raw_descriptors=json.loads((baseline/'entries.json').read_text())
    raw_lookup={(d['layer'],d['code'],d['attribute']):d for d in raw_descriptors}
    lookup={(d['layer'],d['code'],d['attribute']):d for d in descriptors}
    raw=(canonical/'canonical-patterns.bin').read_bytes();assert len(raw)%32==0
    patterns=[raw[at:at+32] for at in range(0,len(raw),32)]
    assert patterns[0]==bytes(32) and len(patterns)<MISSING
    selection=json.loads((checkpoint/'guards-v2/route-pattern-sets.json').read_text())
    required=set(selection['columns43_visible_rows']['required'])
    assert 0 not in required and all(0<n<len(patterns) for n in required)
    additional_keys=[];additional_patterns=set()
    if a.include_source_keys:
        requested=json.loads(a.include_source_keys.read_text())
        assert isinstance(requested,list) and requested
        for item in requested:
            assert isinstance(item,dict) and set(item)=={'layer','code','attribute'}
            assert all(type(item[n]) is int for n in item)
            key=(item['layer'],item['code'],item['attribute'])
            assert key in lookup,('Unknown complete source resource key',key)
            assert item not in additional_keys,('Duplicate resource key',key)
            additional_keys.append(item)
            additional_patterns.update(ref['pattern'] for ref in lookup[key]['references'] if ref['pattern'])
        additional_patterns-=required
        required.update(additional_patterns)
    slots=list(range(16,876))+list(range(1481,1536))
    assert len(slots)==915
    resident_map=[MISSING]*len(patterns);resident_map[0]=0
    physical={};owners={}
    for pattern,tile in zip(sorted(required),slots):
        resident_map[pattern]=tile;physical[tile]=patterns[pattern];owners[tile]={'kind':'background','pattern':pattern}
    assert len(required)<=len(slots) and len(physical)==len(required)

    # Only source resource identity enters the overlay catalog. Capture IDs are
    # used here to recover the exact source priority-mask word, never emitted
    # into a native key or renderer sequence.
    capture_root=ROOT/'build/native-background-context/content-v2'
    layout_path=capture_root/'layout.json';capture_path=capture_root/'source-background-contexts.bin'
    for path in [layout_path,capture_path]:
        assert sha(path)==provenance['external_inputs'][str(path)]
    layout=json.loads(layout_path.read_text());regions={r['name']:r for r in layout['regions']}
    assert regions['ports']['size']==512
    selections=set()
    with capture_path.open('rb') as source:
        for frame in json.loads((checkpoint/'capacity-v3/frames.json').read_text()):
            if not frame['overlays']:continue
            source.seek((frame['capture']-1)*layout['record_bytes']+regions['ports']['offset'])
            ports=struct.unpack('>256H',source.read(512))
            assert ports[0x170//2]==0x12f8 and ports[0x122//2]==0x003e
            for item in frame['overlays']:
                code,attr,subtile=item['code'],item['attribute'],item['subtile']
                mask=ports[(0x16e,0x16c,0x16a,0x168)[(attr>>7)&3]//2]
                selections.add((code,attr,subtile,mask))
    with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
    @lru_cache(None)
    def source_image(layer,code):
        size=16 if layer==2 else 32;physical_code=original_code(layer,code)
        return decode_tile(gfx,physical_code,size) if physical_code is not None else [[15]*size for _ in range(size)]

    overlays=[];overlay_patterns=set();overlay_source_checks=0
    for code,attr,subtile,mask in sorted(selections):
        d=lookup[2,code,attr];ref=d['references'][subtile]
        original=source_image(2,code);tx,ty=subtile%2*8,subtile//2*8
        retained=[];source_pens=0
        for y in range(8):
            line=[]
            for x in range(8):
                source_pen=original[15-(ty+y) if attr&64 else ty+y][15-(tx+x) if attr&32 else tx+x]
                fitted=pixel(patterns[ref['pattern']],7-x if ref['attributes']&0x800 else x,7-y if ref['attributes']&0x1000 else y)
                assert fitted==raw_lookup[2,code,attr]['pen_maps'][subtile][source_pen]
                assert (fitted==0)==(source_pen==15)
                if source_pen!=15:source_pens|=1<<source_pen
                line.append(fitted if source_pen!=15 and mask&(1<<source_pen) else 0)
                overlay_source_checks+=1
            retained.append(line)
        assert source_pens==ref['source_pen_mask']
        assert source_pens&mask&0x7fff
        rendered=genesis_tiles(retained)
        stored,flags=min((flip(rendered,f),f) for f in range(4))
        assert any(stored)
        overlay_patterns.add(stored)
        overlays.append({'code':code,'attribute':attr,'subtile':subtile,'priority_mask':mask,
                         'source_pen_mask':source_pens,'retained_source_pen_mask':source_pens&mask&0x7fff,
                         'source_subtile':ref['source_subtile'],'source_flips':ref['source_flips'],
                         'pattern_hex':stored.hex(),'native_attributes':native_attributes(ref['attributes']>>13,flags)})
    assert {v.hex() for v in overlay_patterns}==set(selection['overlay_patterns_hex'])
    # Deduplicate against all already resident native patterns, then use the
    # remaining allowed slots. This never changes a reserved physical tile.
    by_bytes={value:tile for tile,value in physical.items()};by_bytes[bytes(32)]=0
    free=iter(tile for tile in slots if tile not in physical)
    overlay_allocated=0
    for data in sorted(overlay_patterns):
        if data in by_bytes:continue
        tile=next(free,None);assert tile is not None,'Fixed BG residency exceeds its915 physical slots'
        physical[tile]=data;owners[tile]={'kind':'priority_overlay','pattern_sha256':hashlib.sha256(data).hexdigest()}
        by_bytes[data]=tile;overlay_allocated+=1
    for item in overlays:
        item['physical_tile']=by_bytes[bytes.fromhex(item['pattern_hex'])]
        item['word']=item['physical_tile']|item['native_attributes']
        assert item['word']&0x8000==0

    ordered=sorted(physical);payload=bytearray();runs=[];physical_records=bytearray()
    for tile in ordered:
        if not runs or tile!=runs[-1]['first_tile']+runs[-1]['count']:
            runs.append({'first_tile':tile,'count':0,'data_byte_offset':len(payload)})
        runs[-1]['count']+=1;payload.extend(physical[tile]);physical_records.extend(struct.pack('>H',tile)+physical[tile])
    run_bytes=b''.join(struct.pack('>HHI',r['first_tile'],r['count'],r['data_byte_offset']) for r in runs)
    data_start=16+len(runs)*8
    assert data_start%4==0
    total_bytes=data_start+len(payload)
    upload_blob=struct.pack('>IHHII',0x57425231,1,len(runs),total_bytes,16)
    upload_blob+=b''.join(struct.pack('>HHI',r['first_tile']*32,r['count']*16,data_start+r['data_byte_offset']) for r in runs)
    upload_blob+=payload
    assert len(upload_blob)==total_bytes
    map_bytes=struct.pack('>'+str(len(resident_map))+'H',*resident_map)
    # Reconstruct exclusively from the serialized native run descriptors/data.
    before=bytearray((n*37+(n>>8)*19+11)&255 for n in range(65536));before[:32]=bytes(32)
    after=before[:];written=set()
    for first,count,offset in struct.iter_unpack('>HHI',run_bytes):
        assert count and offset%32==0 and offset+count*32<=len(payload)
        for i in range(count):
            tile=first+i;assert tile in slots and tile not in written
            after[tile*32:(tile+1)*32]=payload[offset+i*32:offset+(i+1)*32];written.add(tile)
    assert written==set(physical)
    for tile in range(2048):
        if tile not in written:assert after[tile*32:(tile+1)*32]==before[tile*32:(tile+1)*32]
    for at in range(0,len(physical_records),34):
        tile=struct.unpack_from('>H',physical_records,at)[0]
        assert after[tile*32:(tile+1)*32]==physical_records[at+2:at+34]
    # Parse the exact WBR1 ROM ABI independently. Validate every run before any
    # simulated store, then compare all64KiB against the tile-run reconstruction.
    magic,version,run_count,blob_size,directory=struct.unpack_from('>IHHII',upload_blob)
    assert (magic,version,blob_size,directory)==(0x57425231,1,len(upload_blob),16)
    admitted=[];prior_end=0
    for n in range(run_count):
        address,words,offset=struct.unpack_from('>HHI',upload_blob,directory+n*8)
        size=words*2
        assert address%32==0 and size and size%32==0 and offset%4==0
        assert data_start<=offset and offset+size<=blob_size and address>=prior_end
        assert (16*32<=address and address+size<=876*32) or (1481*32<=address and address+size<=1536*32)
        admitted.append((address,size,offset));prior_end=address+size
    from_blob=before[:]
    for address,size,offset in admitted:from_blob[address:address+size]=upload_blob[offset:offset+size]
    assert from_blob==after
    parsed_map=list(struct.unpack('>'+str(len(resident_map))+'H',map_bytes));assert parsed_map==resident_map

    header='''/* Exact source-key resources. Partial residency: missing never aliases tile0. */
#ifndef WILLOW_NATIVE_BACKGROUND_RESIDENCY_DATA_H
#define WILLOW_NATIVE_BACKGROUND_RESIDENCY_DATA_H
#include <stdint.h>
#define WILLOW_BG_RESIDENCY_MISSING 0xffffU
typedef struct {uint32_t key;uint16_t offset;} WillowBgResidentPair;
typedef struct {uint16_t pattern,word;} WillowBgResidentRef;
typedef struct {uint32_t key;uint16_t subtile,priorityMask,word,sourcePens,retainedPens;uint8_t sourceSubtile,sourceFlips;} WillowBgResidentOverlay;
/* Native words contain page/H/V/physical tile, no priority bit. The caller must
 * validate live source opacity, layer order, palette/lease, requested residency
 * and resource ownership. Transparent tile0 is already owned and is not uploaded.
 * SourcePens is a presence mask, not a pixel bitmap; local source orientation
 * remains explicit. Overlay keys describe source art, never object positions. */
'''
    header+=f'#define WILLOW_BG_RESIDENCY_PATTERN_COUNT {len(patterns)}\n'
    header+=f'#define WILLOW_BG_RESIDENCY_UPLOAD_TILE_COUNT {len(physical)}\n'
    header+='static const uint16_t willowBgPatternToTile[]={'+','.join(map(str,resident_map))+'};\n'
    resolved=[];ref_bytes=bytearray();counts=Counter();checks=0
    for layer in (2,3):
        keys=[];refs=[];masks=[];subtiles=[];flips=[]
        for d in descriptors:
            if d['layer']!=layer:continue
            code,attr=d['code'],d['attribute'];keys.append((code<<16|attr,len(refs)));converted=[]
            original=source_image(layer,code);size=d['size'];columns=size//8
            for n,ref in enumerate(d['references']):
                pattern=ref['pattern'];tile=parsed_map[pattern]
                word=MISSING if tile==MISSING else tile|ref['attributes']
                assert word==MISSING or word&0x8000==0
                refs.append((pattern,word));masks.append(ref['source_pen_mask']);subtiles.append(ref['source_subtile']);flips.append(ref['source_flips'])
                ref_bytes.extend(struct.pack('>HH',pattern,word))
                converted.append({'pattern':pattern,'word':word,'source_pen_mask':ref['source_pen_mask'],
                                  'source_subtile':ref['source_subtile'],'source_flips':ref['source_flips']})
                if word==MISSING:
                    counts['missing_refs']+=1;assert pattern not in required and pattern!=0
                    continue
                counts['resident_refs']+=1;counts['transparent_refs']+=pattern==0
                tile_data=after[tile*32:(tile+1)*32]
                source_mask=0
                for y in range(8):
                    for x in range(8):
                        dx,dy=n%columns*8+x,n//columns*8+y
                        source_pen=original[size-1-dy if attr&64 else dy][size-1-dx if attr&32 else dx]
                        output=pixel(tile_data,7-x if word&0x800 else x,7-y if word&0x1000 else y)
                        assert output==raw_lookup[layer,code,attr]['pen_maps'][n][source_pen]
                        assert (output==0)==(source_pen==15)
                        if source_pen!=15:source_mask|=1<<source_pen
                        checks+=1
                assert source_mask==ref['source_pen_mask']
            resolved.append({'layer':layer,'code':code,'attribute':attr,'references':converted})
        prefix=f'willowBgResident{layer}'
        header+=f'#define WILLOW_BG_RESIDENCY_{layer}_PAIR_COUNT {len(keys)}\n'
        header+=f'static const WillowBgResidentPair {prefix}Pairs[]={{'+','.join('{%dUL,%d}'%v for v in keys)+'};\n'
        header+=f'static const WillowBgResidentRef {prefix}Refs[]={{'+','.join('{%d,%d}'%v for v in refs)+'};\n'
        for name,typ,values in [('SourcePens','uint16_t',masks),('SourceSubtile','uint8_t',subtiles),('SourceFlips','uint8_t',flips)]:
            header+=f'static const {typ} {prefix}{name}[]={{'+','.join(map(str,values))+'};\n'
    header+=f'#define WILLOW_BG_RESIDENCY_OVERLAY_COUNT {len(overlays)}\n'
    header+='static const WillowBgResidentOverlay willowBgResidentOverlays[]={'+','.join('{%dUL,%d,%d,%d,%d,%d,%d,%d}'%(o['code']<<16|o['attribute'],o['subtile'],o['priority_mask'],o['word'],o['source_pen_mask'],o['retained_source_pen_mask'],o['source_subtile'],o['source_flips']) for o in overlays)+'};\n#endif\n'
    overlay_checks=0
    for o in overlays:
        data=after[o['physical_tile']*32:(o['physical_tile']+1)*32];original=source_image(2,o['code'])
        for y in range(8):
            for x in range(8):
                dx,dy=o['subtile']%2*8+x,o['subtile']//2*8+y;attr=o['attribute']
                source_pen=original[15-dy if attr&64 else dy][15-dx if attr&32 else dx]
                expected=raw_lookup[2,o['code'],attr]['pen_maps'][o['subtile']][source_pen] if source_pen!=15 and o['priority_mask']&(1<<source_pen) else 0
                actual=pixel(data,7-x if o['word']&0x800 else x,7-y if o['word']&0x1000 else y)
                assert expected==actual;overlay_checks+=1
    (out/'background_residency.h').write_text(header)
    (out/'pattern-to-tile.bin').write_bytes(map_bytes);(out/'upload-data.bin').write_bytes(payload)
    (out/'upload-runs.bin').write_bytes(run_bytes);(out/'physical-tiles.bin').write_bytes(physical_records)
    (out/'resident-upload.bin').write_bytes(upload_blob)
    (out/'resident-upload.json').write_text(json.dumps({'magic':'0x57425231','version':1,'run_count':len(runs),'total_bytes':total_bytes,'directory_offset':16,'runs':[{'vram_byte_address':address,'word_count':size//2,'blob_data_offset':offset} for address,size,offset in admitted]},indent=2)+'\n')
    (out/'resolved-references.bin').write_bytes(ref_bytes)
    (out/'entries.json').write_text(json.dumps(resolved,separators=(',',':'))+'\n')
    (out/'overlays.json').write_text(json.dumps(overlays,indent=2)+'\n')
    (out/'physical-tiles.json').write_text(json.dumps([{'tile':tile,**owners[tile],'sha256':hashlib.sha256(physical[tile]).hexdigest()} for tile in ordered],indent=2)+'\n')
    (out/'upload-runs.json').write_text(json.dumps(runs,indent=2)+'\n')
    for name in ['source-palettes.bin','cram.bin']:(out/name).write_bytes((canonical/name).read_bytes())
    command=['m68k-elf-gcc','-m68000','-std=c99','-ffreestanding','-x','c','-c',str(out/'background_residency.h'),'-o',str(out/'background_residency.o')]
    subprocess.run(command,check=True)
    size_text=subprocess.check_output(['m68k-elf-size',str(out/'background_residency.o')],text=True)
    report={'scope':'Concrete immutable source-keyed native BG/overlay residency resources. Fixed resource subset only; no captured maps or positions. No live source-opacity/priority/Window/timing admission or FPS claim.',
            'canonical_patterns':len(patterns),'resident_background_patterns':len(required),'overlay_keys':len(overlays),
            'unique_overlay_patterns':len(overlay_patterns),'new_overlay_physical_patterns':overlay_allocated,
            'uploaded_patterns':len(physical),'upload_bytes':len(payload),'upload_runs':runs,
            'resident_upload_blob_bytes':len(upload_blob),'resident_upload_full_vram_reconstruction_bytes':len(from_blob),
            'unused_bg_slots':[tile for tile in slots if tile not in physical],
            'reserved_tiles':{'low':[0,15],'window':[876,980],'objects':[981,1480],'tables_begin':1536},
            'transparent_tile':'Canonical0 resolves to existing physical0 and is not uploaded. Caller must own/prove tile0 blank.',
            'missing_sentinel':MISSING,'reference_counts':dict(counts),
            'reference_contract':'Every original layer/code/fullattr key remains searchable. A ref carries canonicalID plus resolved nativeword or explicit0xFFFF missing. Runtime may not publish a missing ref or substitute tile0.',
            'overlay_contract':'Key=(Scroll2code,fullattribute,displayed8x8subtile,exactsourceprioritymask). Nativeword contains page/H/V/tile with no prioritybit. Original source pen15 remains transparent regardlessmask. Geometry and ordering remain current runtime inputs.',
            'selection_contract':'899source-derived patterns are the frozen43-column/visible-row original-opacity required routeunion. Far refs hidden by near may be missing. Renderer must prove requested live-source composition/opacity and coverage, never assume captured maps remain valid.',
            'native_resource_original_pixel_checks':checks,'overlay_original_pixel_checks':overlay_checks,
            'overlay_prepare_pixel_checks':overlay_source_checks,'unwritten_vram_bytes_preserved':65536-len(payload),
            'native_compile_command':command,'native_object_size_output':size_text,
            'binary_formats':{'pattern-to-tile.bin':'3512big-endianu16physicaltile values;0xFFFF unresident.',
                              'resident-upload.bin':'WBR1header >IHHII magic57425231,version1,runCount,totalBytes,directoryOffset16; runs >HHI VRAMbyteAddress,wordCount,blobDataOffset; packed4bytealigned data. Allbig-endian.',
                              'upload-runs.bin':'RepeatedBEu16firstTile,u16count,u32dataByteOffset. Payloadwordaligned,32bytes/tile.',
                              'physical-tiles.bin':'Sorted repeatedBEu16physicalTile +32patternbytes. Only allowed BGslots.',
                              'resolved-references.bin':'RepeatedBEu16canonicalID,u16nativeWord, layer/code/attr/displayedsubtile order.'},
            'hashes':{str(path):sha(path) for path in [Path(__file__).resolve(),ROOT/'tools/cps1_gfx.py',ROOT/'willow.zip',checkpoint/'checkpoint.json',canonical/'manifest.json',canonical/'entries.json',canonical/'canonical-patterns.bin',baseline/'entries.json',checkpoint/'guards-v2/route-pattern-sets.json',checkpoint/'capacity-v3/frames.json',layout_path,capture_path]},
            'outputs':{path.name:sha(path) for path in sorted(out.iterdir()) if path.is_file() and path.name!='manifest.json'}}
    if a.include_source_keys:
        report['hashes'][str(a.include_source_keys.resolve())]=sha(a.include_source_keys)
        report['additional_source_keys']=additional_keys
        report['additional_canonical_patterns']=sorted(additional_patterns)
        report['selection_contract']+=' Additional complete source identities include all their subtiles; no map positions or camera sequence enter native data.'
    (out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ['hashes','outputs','native_compile_command','binary_formats']},indent=2))

if __name__=='__main__':main()
