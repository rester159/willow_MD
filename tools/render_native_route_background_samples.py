#!/usr/bin/env python3
"""Render source RGB and preconverted BG diagnostics from exact upcoming views.

This reconstructs numeric background layers; it is not a native VDP capture.
Original pen15 transparency and Scroll3-before-Scroll2 ordering remain exact.
"""
from collections import Counter
from functools import lru_cache
from pathlib import Path
import argparse
import csv
import json
import math
import struct
import zipfile
from PIL import Image, ImageDraw
from cps1_gfx import assemble_region, decode_tile
from cps1_palette import cps_rgb
from prepare_native_route_background import ROOT, sha, physical_code, cell_index, read_capture, active, upcoming_ports


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture', type=Path, default=ROOT/'build/native-background-context/content-v2')
    p.add_argument('--assets', type=Path, default=ROOT/'build/native-route-background-audit/prepared-v2')
    p.add_argument('--capacity', type=Path, default=ROOT/'build/native-route-background-audit/capacity-v3')
    p.add_argument('--output', type=Path, default=ROOT/'build/native-route-background-audit/visual-v1')
    a = p.parse_args();out=a.output.resolve();out.mkdir(parents=True, exist_ok=True)
    source, assets, capacity = a.capture.resolve(), a.assets.resolve(), a.capacity.resolve()
    manifest = json.loads((assets/'manifest.json').read_text())
    for name, digest in manifest['outputs'].items():
        assert sha(assets/name) == digest
    rows, get, input_paths = read_capture(source)
    selected = [r for r in rows if active(r)]
    frames = json.loads((capacity/'frames.json').read_text())
    peak = max(frames, key=lambda f:(f['canonical_visible_patterns'], f['capture']))
    samples = [selected[0], next(r for r in selected if int(r['capture']) == peak['capture'])]
    entries = json.loads((assets/'entries.json').read_text())
    lookup = {(d['layer'], d['code'], d['attribute']):d for d in entries}
    raw = (assets/'background-patterns.bin').read_bytes()
    palette = (assets/'source-palettes.bin').read_bytes();cram=(assets/'cram.bin').read_bytes()
    native_colors=[tuple(((v >> bit)&7)*34 for bit in (1,5,9)) for v in struct.unpack('>64H',cram)]
    with zipfile.ZipFile(ROOT/'willow.zip') as z:
        gfx=assemble_region(z)
    timing_path=ROOT/'build/native-background-context/timing-v1/upcoming-views.csv'
    timing={int(r['capture']):r for r in csv.DictReader(timing_path.open())}

    @lru_cache(None)
    def image(layer, code):
        size=16 if layer==2 else 32; physical=physical_code(layer,code)
        return decode_tile(gfx,physical,size) if physical is not None else [[15]*size for _ in range(size)]

    summaries=[];outputs=[]
    for row in samples:
        capture=int(row['capture']);rec=get(row);ports=upcoming_ports(rec)
        assert rec['palettes'] == palette
        assert ports[0x122//2] == 0x3e and ports[0x170//2] == 0x12f8
        for at,field in [(0x110,'s2x'),(0x112,'s2y'),(0x114,'s3x'),(0x116,'s3y')]:
            assert ports[at//2] == int(timing[capture][field+'_published'])
        backdrop=cps_rgb(struct.unpack_from('>H',palette,0x17fe)[0])
        src=[backdrop]*(320*224);fit=[native_colors[0]]*(320*224);owners=[0]*(320*224)
        checked=0;layer_opaque=Counter();layer_sse=Counter()
        for layer,size,xr,yr in [(3,32,0x114,0x116),(2,16,0x110,0x112)]:
            extent=64*size
            for y in range(224):
                sy=(16+y+ports[yr//2])%extent
                for x in range(320):
                    sx=(96+x+ports[xr//2])%extent
                    code,attr=struct.unpack_from('>2H',rec[f'scroll{layer}'],cell_index(layer,sx//size,sy//size)*4)
                    dx,dy=sx%size,sy%size;d=lookup[layer,code,attr]
                    pen=image(layer,code)[size-1-dy if attr&64 else dy][size-1-dx if attr&32 else dx]
                    index=(dy//8)*(size//8)+dx//8;ref=d['references'][index]
                    byte=raw[(ref&16383)*32+(dy&7)*4+(dx&7)//2]
                    native=(byte >> (0 if dx&1 else 4))&15
                    assert (native==0)==(pen==15)
                    assert native==d['pen_maps'][index][pen]
                    checked+=1
                    if pen==15:continue
                    word=struct.unpack_from('>H',palette,(0x800 if layer==2 else 0xc00)+(attr&31)*32+pen*2)[0]
                    source_rgb=cps_rgb(word);native_rgb=native_colors[(ref>>14)*16+native]
                    src[y*320+x]=source_rgb;fit[y*320+x]=native_rgb;owners[y*320+x]=layer
                    layer_opaque[layer]+=1;layer_sse[layer]+=sum((a-b)**2 for a,b in zip(source_rgb,native_rgb))
        error=sum(sum((a-b)**2 for a,b in zip(s,t)) for s,t in zip(src,fit))
        source_image=Image.new('RGB',(320,224));source_image.putdata(src)
        fitted_image=Image.new('RGB',(320,224));fitted_image.putdata(fit)
        prefix=f'capture-{capture}'
        source_image.save(out/f'{prefix}-source.png');fitted_image.save(out/f'{prefix}-fitted.png')
        sheet=Image.new('RGB',(1280,488),(24,24,24));draw=ImageDraw.Draw(sheet)
        draw.text((8,5),f'Capture {capture} / frame {row["frame"]}: original CPS RGB, upcoming staged view',fill='white')
        draw.text((648,5),'Preconverted 4-page Genesis fit (host reconstruction)',fill='white')
        sheet.paste(source_image.resize((640,448),Image.Resampling.NEAREST),(0,24))
        sheet.paste(fitted_image.resize((640,448),Image.Resampling.NEAREST),(640,24))
        draw.text((8,474),'Background-only diagnostic; same geometry/transparency; source Scroll3 then Scroll2. Objects/Scroll1 excluded.',fill='white')
        sheet.save(out/f'{prefix}-comparison.png')
        names=[f'{prefix}-{suffix}.png' for suffix in ('source','fitted','comparison')];outputs.extend(names)
        summaries.append({'capture':capture,'frame':int(row['frame']),'source_crop':[96,16,320,224],
                          'upcoming_scroll_words':{name:ports[at//2] for at,name in [(0x110,'s2x'),(0x112,'s2y'),(0x114,'s3x'),(0x116,'s3y')]},
                          'verified_layer_pixels':checked,'composite_rgb_rmse':math.sqrt(error/(320*224*3)),
                          'final_pixel_owner_counts':dict(Counter(owners)),
                          'layer_opaque_pixels':dict(layer_opaque),
                          'layer_rgb_rmse':{str(layer):math.sqrt(layer_sse[layer]/(3*layer_opaque[layer])) for layer in layer_opaque},
                          'outputs':names})
    report={'scope':'Background-only numeric reconstruction, not hardware screenshots or complete source composition. Exact raw viewport and source priority between background layers; original mixed-priority vs actors is separately audited. No palette edits.',
            'selection':'First active capture and latest capture attaining peak full visible canonical BG pattern demand.',
            'samples':summaries,
            'hashes':{str(path):sha(path) for path in [Path(__file__).resolve(),ROOT/'tools/prepare_native_route_background.py',ROOT/'tools/cps1_gfx.py',ROOT/'tools/cps1_palette.py',ROOT/'willow.zip',*input_paths,timing_path,assets/'manifest.json',assets/'entries.json',assets/'source-palettes.bin',assets/'cram.bin',assets/'background-patterns.bin',capacity/'frames.json']},
            'outputs':{name:sha(out/name) for name in outputs}}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(summaries,indent=2))

if __name__=='__main__':main()
