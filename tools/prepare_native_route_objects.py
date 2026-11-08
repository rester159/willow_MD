#!/usr/bin/env python3
"""Prepare immutable native templates for the captured source-produced route.

Positions/order are NOT compiled into the resource header. Complete original
shapes are compiled, including currently clipped children. This preparation
proof is conditional on an exact palette/residency/view epoch; it is not a live
resource hook or a full-game atlas. Both player CRAM pages stay unchanged.
"""
from collections import Counter
from pathlib import Path
import argparse
import csv
import hashlib
import json
import math
import struct
import zipfile
from PIL import Image, ImageDraw
from audit_native_prepared_catalog import children
from cps1_gfx import assemble_region, decode_tile
from cps1_palette import cps_rgb
from native_object_tiles import verify as verify_pens
from native_palette_fit import color_error

ROOT=Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def unpack(raw):
    return [[(raw[((x//8)*2+y//8)*32+(y&7)*4+(x&7)//2]>>(0 if x&1 else 4))&15
             for x in range(16)] for y in range(16)]


def prepare(context, out):
    context,out=context.resolve(),out.resolve()
    out.mkdir(parents=True,exist_ok=True)
    checkpoint=json.loads((context/'checkpoint.json').read_text())
    for name in ['layout.json','evidence.json','visual-contexts.csv','visual-contexts.bin','captured-original-banks.bin']:
        assert sha(context/name)==checkpoint['files'][name], name
    layout=json.loads((context/'layout.json').read_text())
    evidence=json.loads((context/'evidence.json').read_text())
    rows=list(csv.DictReader((context/'visual-contexts.csv').open()))
    assert layout['record_bytes']==90624 and len(rows)==layout['records']==1797
    active=[r for r in rows if r['manager']==r['scene']=='2']
    assert len(active)==464
    banks=(context/'captured-original-banks.bin').read_bytes()
    assert len(banks)==len(rows)*2048
    assert sha(context/'captured-original-banks.bin')==evidence['identical_cadence_files']['captured-original-banks.bin']
    captures=[];keys=set();palette=None
    with (context/'visual-contexts.bin').open('rb') as stream:
        for row in active:
            index=int(row['capture'])-1
            assert row['source_valid']==row['native_valid']=='1'
            assert row['source_count']==row['native_count'] and row['source_epoch']==row['native_epoch']
            stream.seek(index*layout['record_bytes']);current=stream.read(6144)
            if palette is None:palette=current
            assert len(current)==6144 and current==palette
            count=int(row['source_count'])
            bank=list(struct.iter_unpack('>4H',banks[index*2048:(index+1)*2048]))
            assert all(x==attribute==0 for x,y,code,attribute in bank[count:])
            records=bank[:count]
            assert all(code<=0x27ff and attribute&0xff00!=0xff00 for x,y,code,attribute in records)
            keys.update((code,attribute) for x,y,code,attribute in records)
            captures.append({'capture':index+1,'epoch':int(row['source_epoch']),
                             'bank':int(row['bank']),'records':records})
    assert len(keys)==17
    pairs={(child,attribute&31) for code,attribute in keys for x,y,child in children(code,attribute)}
    codes=sorted({code for code,group in pairs})
    asset=ROOT/'build/cartridge-gap-audit'
    atlas_path=asset/'full-object-capacity/full_object_tiles.json'
    atlas=json.loads(atlas_path.read_text());neutral=(atlas_path.with_suffix('.bin')).read_bytes()
    assert hashlib.sha256(neutral).hexdigest()==atlas['binary_sha256']
    blocks=dict(atlas['entries'])
    archive=ROOT/'willow.zip'
    assert sha(archive)==atlas['archive_sha256']
    with zipfile.ZipFile(archive) as stream:gfx=assemble_region(stream)
    source_pixels_verified=verify_pens(gfx,[(code,blocks[code]) for code in codes],neutral)
    original={code:decode_tile(gfx,code,16) for code in codes}
    old=json.loads((asset/'object_palette_reuse_p3_w16.json').read_text())
    for group,words in old['source_palettes'].items():
        assert list(struct.unpack('>16H',palette[int(group)*32:int(group)*32+32]))==words
    fixture=asset/'frame1200_window_v1'
    vram=(fixture/'vram.bin').read_bytes();cram=(fixture/'cram.bin').read_bytes()
    pages=[[tuple(((word>>s)&7)*34 for s in (1,5,9)) for word in struct.unpack('>16H',cram[p*32:(p+1)*32])] for p in range(4)]
    assert list(struct.unpack('>32H',cram[:64]))==old['cram'][0]+old['cram'][1]
    anchors={(a['group'],tuple(a['source_rgb'])):tuple(a['hardware_rgb']) for a in old['anchors']}
    existing=json.loads((fixture/'asset-binding.json').read_text())
    known={(e['code'],e['source_group']):e for e in existing['entries']}
    first=981;old_end=1117
    data=bytearray(vram[first*32:old_end*32])
    resident={bytes(data[n:n+128]):first+n//32 for n in range(0,len(data),128)}
    entries={};metrics={};source_palettes={}
    for code,group in sorted(pairs):
        words=list(struct.unpack('>16H',palette[group*32:group*32+32]))
        source_palettes[str(group)]=words
        colors=[cps_rgb(w) for w in words]
        raw=neutral[blocks[code]*128:(blocks[code]+1)*128]
        pens=unpack(raw)
        histogram=Counter(colors[p-1] for row in pens for p in row if p)
        if not histogram:
            entry={'code':code,'source_group':group,'status':2,'attribute':0,'map':[0]*16}
        else:
            forced={c:anchors[group,c] for c in histogram if (group,c) in anchors}
            allowed=[group] if group in (0,1) else [3] if forced else range(4)
            candidates=[]
            for page in allowed:
                mapping={}
                for color in histogram:
                    options=[p for p in range(1,16) if color not in forced or pages[page][p]==forced[color]]
                    # Source group8 uses blue/cyan magic. The general weighted
                    # distance can prefer a vivid green over a dimmer blue for
                    # F0FF; preserve the source's cool channel ordering instead.
                    # This constrains existing hardware choices, not source RGB
                    # precision, white/warm spell colors, or either player page.
                    if group==8 and color[2]>=color[1]>color[0]:
                        options=[p for p in options if pages[page][p][2]>=pages[page][p][1]
                                 and pages[page][p][2]>pages[page][p][0]]
                    if not options:break
                    mapping[color]=min(options,key=lambda p:(color_error(color,pages[page][p]),p))
                if len(mapping)!=len(histogram):continue
                cost=sum(n*color_error(color,pages[page][mapping[color]]) for color,n in histogram.items())
                candidates.append((cost,page,mapping))
            assert candidates,'No page preserves the source character/color constraints'
            cost,page,mapping=min(candidates,key=lambda c:(c[0],c[1]))
            pen_map=[0]+[mapping.get(color,1) for color in colors[:15]]
            packed=bytes((pen_map[v>>4]<<4)|pen_map[v&15] for v in raw)
            if (code,group) in known:
                prev=known[code,group]
                assert prev['status']==1 and (prev['attribute']>>13)&3==page
                at=(prev['attribute']&2047)*32
                assert packed==vram[at:at+128],(code,group,'Changed frozen art')
                tile=prev['attribute']&2047
            else:
                if packed not in resident:
                    resident[packed]=first+len(data)//32;data.extend(packed)
                tile=resident[packed]
            entry={'code':code,'source_group':group,'status':1,'attribute':(page<<13)|tile,'map':pen_map}
            metric=metrics.setdefault(str(group),{'pixels':0,'weighted_error':0,'rgb_squared_error':0,'cells':0})
            metric['pixels']+=sum(histogram.values());metric['weighted_error']+=cost;metric['cells']+=1
            metric['rgb_squared_error']+=sum(n*sum((a-b)**2 for a,b in zip(c,pages[page][mapping[c]])) for c,n in histogram.items())
        if (code,group) in known:assert entry['status']==known[code,group]['status']
        entries[code,group]=entry
    highest=first+len(data)//32-1
    assert highest<1536, f'Route atlas exceeds pattern allocation: {highest}'
    templates=[]
    for code,attribute in sorted(keys):
        cells=[]
        for x,y,child in children(code,attribute):
            entry=entries[child,attribute&31]
            if entry['status']==2:continue
            cells.append({'offset':(y<<8)|x,'attribute':entry['attribute']|((attribute<<6)&0x1800),'source_code':child})
        templates.append({'key':(code<<16)|attribute,'code':code,'source_attribute':attribute,'cells':cells})
    # Decode the compiled payload independently at every source child pixel,
    # including children clipped in all recorded frames. Preserve transparency.
    checked=0
    for entry in entries.values():
        code,group=entry['code'],entry['source_group']
        target=[[0]*16 for _ in range(16)] if entry['status']==2 else unpack(data[((entry['attribute']&2047)-first)*32:((entry['attribute']&2047)-first)*32+128])
        for y in range(16):
            for x in range(16):
                source=original[code][y][x]
                assert target[y][x]==(0 if source==15 else entry['map'][source+1])
                assert (target[y][x]==0)==(source==15)
                checked+=1
    # Resource header contains keys/shapes only. All runtime origins, order,
    # source epochs and count admission come from the live producer interface.
    header='/* Source-derived captured-route resources; requires matching palette/residency/view epochs. */\n#include "willow_native_object_catalog.h"\n'
    for n,t in enumerate(templates):
        body=','.join('{'+f'{c["offset"]},{c["attribute"]}'+'}' for c in t['cells']) or '{0,0}'
        header+=f'static const WillowPreparedObjectCell routeCells{n}[]={{'+body+'};\n'
        header+=f'static const WillowPreparedObjectTemplate routeTemplate{n}={{routeCells{n},{len(t["cells"])},{t["code"]},{t["source_attribute"]}}};\n'
    header+='static const WillowPreparedObjectKey routeKeys[]={'+','.join('{'+str(t['key'])+'UL}' for t in templates)+'};\n'
    header+='static const WillowPreparedObjectTemplate*const routeShapes[]={'+','.join('&routeTemplate'+str(n) for n in range(len(templates)))+'};\n'
    (out/'route_objects.h').write_text(header)
    (out/'object-patterns.bin').write_bytes(data);(out/'source-palettes.bin').write_bytes(palette)
    (out/'cram.bin').write_bytes(cram)
    combined=bytearray(vram);combined[first*32:(highest+1)*32]=data
    (out/'resident-vram.bin').write_bytes(combined)
    for metric in metrics.values():metric['rgb_rmse']=math.sqrt(metric['rgb_squared_error']/(3*metric['pixels']))
    # Color comparison sheet uses exact source RGB and fitted native pixels.
    pairs8=[pair for pair in sorted(entries) if pair[1]==8 and entries[pair]['status']==1]
    sheet=Image.new('RGB',(640,40+40*math.ceil(len(pairs8)/8)),(32,32,32));draw=ImageDraw.Draw(sheet)
    draw.text((4,4),'Original CPS palette / source pixels',fill='white');draw.text((324,4),'Prepared Genesis pixels (same geometry)',fill='white')
    for n,pair in enumerate(pairs8):
        entry=entries[pair];code,group=pair;colors=[cps_rgb(w) for w in source_palettes[str(group)]]
        for y,row in enumerate(original[code]):
            for x,pen in enumerate(row):
                if pen==15:continue
                for side,color in enumerate([colors[pen],pages[(entry['attribute']>>13)&3][entry['map'][pen+1]]]):
                    px=side*320+(n%8)*40+x*2;py=30+(n//8)*40+y*2
                    draw.rectangle((px,py,px+1,py+1),fill=color)
    sheet.save(out/'group8-colors.png')
    files=[Path(__file__),ROOT/'tools/audit_native_prepared_catalog.py',ROOT/'tools/native_object_tiles.py',ROOT/'tools/cps1_gfx.py',ROOT/'tools/cps1_palette.py',ROOT/'tools/native_palette_fit.py',
           context/'checkpoint.json',context/'visual-contexts.csv',context/'visual-contexts.bin',context/'layout.json',context/'evidence.json',context/'captured-original-banks.bin',
           atlas_path,atlas_path.with_suffix('.bin'),archive,asset/'object_palette_reuse_p3_w16.json',fixture/'asset-binding.json',fixture/'vram.bin',fixture/'cram.bin']
    report={'scope':'Immutable complete-shape resources for 464 captured active states; no positions/order baked into native header. One proved palette epoch on this route only; not live palette/residency integration, all-level coverage or FPS.',
        'active_captures':len(captures),'templates':templates,'entries':list(entries.values()),'source_palettes':source_palettes,
        'palette_epochs':1,'source_codes':len(codes),'source_pen_pixels_verified':source_pixels_verified,'mapped_pixels_checked':checked,
        'first_tile':first,'highest_tile':highest,'pattern_bytes':len(data),'old_pattern_bytes':(old_end-first)*32,
        'both_player_pages_unchanged':True,'existing_scene_patterns_unchanged':data[:(old_end-first)*32]==vram[first*32:old_end*32],
        'group8_cool_color_rule':'For source B>=G>R, choose existing native colors with B>=G and B>R; white/warm colors and all CRAM words unchanged.',
        'source_groups':sorted({g for c,g in pairs}),'metrics':metrics,
        'hashes':{str(path):sha(path) for path in files},
        'outputs':{name:sha(out/name) for name in ['route_objects.h','object-patterns.bin','source-palettes.bin','cram.bin','resident-vram.bin','group8-colors.png']}}
    (out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ['templates','entries','source_palettes','hashes','outputs']},indent=2))
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--context',type=Path,default=ROOT/'build/native-object-publication/checkpoint-visual-state-v1')
    parser.add_argument('--output',type=Path,default=ROOT/'build/native-route-objects/prepared-v2')
    args=parser.parse_args();prepare(args.context,args.output)
