#!/usr/bin/env python3
"""Independent visible-pixel sampling of captured native object presentation.

Selects prefix endpoints from original decoded opaque pixel ownership, invokes
unchanged verify_native_route_objects.py, and checks the resulting actual core
pixels again. Full background/layer priority, live execution and FPS excluded.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
import argparse, csv, hashlib, json, struct, subprocess, sys, zipfile
from PIL import Image, ImageDraw
from cps1_gfx import assemble_region, decode_tile
from cps1_palette import cps_rgb
ROOT=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--assets',type=Path,default=ROOT/'build/native-route-objects/prepared-v1')
    p.add_argument('--context',type=Path,default=ROOT/'build/native-object-publication/checkpoint-visual-state-v1')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-route-objects/pixel-samples')
    p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    p.add_argument('--jobs',type=int,default=2)
    p.add_argument('--no-run',action='store_true')
    a=p.parse_args();assets=a.assets.resolve();context=a.context.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    assert 1<=a.jobs<=4
    manifest=json.loads((assets/'manifest.json').read_text())
    for name,digest in manifest['outputs'].items():assert sha(assets/name)==digest
    checkpoint=json.loads((context/'checkpoint.json').read_text())
    for name in ['visual-contexts.csv','captured-original-banks.bin']:
        assert sha(context/name)==checkpoint['files'][name]
    entries={(e['code'],e['source_group']):e for e in manifest['entries']}
    pairs=sorted(entries);pair_id={pair:n for n,pair in enumerate(pairs)}
    keys=sorted((t['code'],t['source_attribute']) for t in manifest['templates']);key_id={key:n for n,key in enumerate(keys)}
    archive=ROOT/'willow.zip';assert sha(archive)==manifest['hashes'][str(archive)]
    with zipfile.ZipFile(archive) as z:gfx=assemble_region(z)
    original={code:decode_tile(gfx,code,16) for code,group in pairs}
    # Source pixel tokens keep original code/group and unflipped coordinates;
    # they deliberately do not conflate distinct colors or baked native tiles.
    all_opaque={pair_id[code,group]*256+y*16+x for code,group in pairs for y in range(16) for x in range(16) if original[code][y][x]!=15}
    rows=[r for r in csv.DictReader((context/'visual-contexts.csv').open()) if r['manager']==r['scene']=='2']
    banks=(context/'captured-original-banks.bin').read_bytes()
    colors=struct.unpack('>64H',(assets/'cram.bin').read_bytes())
    native_palette=[tuple(((w>>s)&7)*34 for s in [1,5,9]) for w in colors]
    source_palettes={int(group):[cps_rgb(w) for w in words] for group,words in manifest['source_palettes'].items()}
    source_palette_words={int(group):words for group,words in manifest['source_palettes'].items()}

    def render(row,materialize=False):
        capture=int(row['capture']);count=int(row['source_count']);assert row['source_valid']==row['native_valid']=='1'
        raw=banks[(capture-1)*2048:(capture-1)*2048+count*8]
        records=list(struct.iter_unpack('>4H',raw));owner=[-1]*71680;owner_key=[-1]*71680
        in_crop=set();frame_opaque=set();emitted=set();full_keys=set()
        # The earlier source object is frontmost. First opaque writer wins.
        for x,y,base,attribute in records:
            key=(base,attribute);kid=key_id[key];full_keys.add(kid)
            if base>0x27ff or attribute&0xff00==0xff00:continue
            columns=((attribute>>8)&15)+1;lines=(attribute>>12)+1;group=attribute&31
            for row_index in range(lines):
                for column in range(columns):
                    code_x=columns-1-column if attribute&32 else column
                    code_y=lines-1-row_index if attribute&64 else row_index
                    child=(base&0xfff0)+((base+code_x)&15)+code_y*16
                    pid=pair_id[child,group];pens=original[child]
                    sx=((x+column*16)&511)-96;sy=((y+row_index*16)&511)-16
                    opaque_child=False;visible_child=False
                    for dy in range(16):
                        for dx in range(16):
                            px=15-dx if attribute&32 else dx;py=15-dy if attribute&64 else dy
                            pen=pens[py][px]
                            if pen==15:continue
                            token=pid*256+py*16+px;frame_opaque.add(token);opaque_child=True
                            tx,ty=sx+dx,sy+dy
                            if not(0<=tx<320 and 0<=ty<224):continue
                            visible_child=True;in_crop.add(token);at=ty*320+tx
                            if owner[at]<0:owner[at]=token;owner_key[at]=kid
                    if opaque_child and visible_child:emitted.add(pid)
        visible={token for token in owner if token>=0};visible_keys={k for k in owner_key if k>=0}
        result={'prefix':rows.index(row)+1,'capture':capture,'records':records,'keys':visible_keys,'present_keys':full_keys,'pairs':{t//256 for t in visible},'pixels':visible,'in_crop':in_crop,'frame_opaque':frame_opaque,'admitted_pairs_before_occlusion':emitted}
        if materialize:
            native=Image.new('RGB',(320,224),native_palette[0]);source_image=native.copy()
            for at,token in enumerate(owner):
                if token<0:continue
                pid,pos=divmod(token,256);code,group=pairs[pid];py,px=divmod(pos,16);pen=original[code][py][px]
                entry=entries[code,group];assert entry['status']==1 and entry['map'][pen+1]!=0
                native.putpixel((at%320,at//320),native_palette[((entry['attribute']>>13)&3)*16+entry['map'][pen+1]])
                source_image.putpixel((at%320,at//320),source_palettes[group][pen])
            result.update(native=native,source=source_image,owner=owner)
        return result

    frames=[render(row) for row in rows]
    achievable_keys=set().union(*(f['keys'] for f in frames));achievable_pairs=set().union(*(f['pairs'] for f in frames));achievable_pixels=set().union(*(f['pixels'] for f in frames));crop_pixels=set().union(*(f['in_crop'] for f in frames))
    remaining_keys=set(achievable_keys);remaining_pairs=set(achievable_pairs);remaining_pixels=set(achievable_pixels);selected=[]
    while remaining_pixels or remaining_keys or remaining_pairs:
        best=max((f for f in frames if f not in selected),key=lambda f:(len(f['keys']&remaining_keys),len(f['pairs']&remaining_pairs),len(f['pixels']&remaining_pixels),-f['prefix']))
        gain={'keys':len(best['keys']&remaining_keys),'code_group_pairs':len(best['pairs']&remaining_pairs),'original_opaque_pixels':len(best['pixels']&remaining_pixels)}
        assert any(gain.values());selected.append(best);best['gain']=gain
        remaining_keys-=best['keys'];remaining_pairs-=best['pairs'];remaining_pixels-=best['pixels']
    selected.sort(key=lambda f:f['prefix'])
    plan={'strategy':'Greedy lexicographic new visible source template keys, code/group pairs, then original opaque pixel coordinates; first opaque source object wins. Covers every pixel coordinate attainable on the full captured route.', 'captures':[{'capture':f['capture'],'prefix':f['prefix'],'gain_at_selection':f['gain'],'visible_keys':[list(keys[k]) for k in sorted(f['keys'])],'visible_pairs':[list(pairs[k]) for k in sorted(f['pairs'])],'visible_original_opaque_pixel_coordinates':len(f['pixels'])} for f in selected]}
    (out/'selection-plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    print(json.dumps({'selected_prefixes':[f['prefix'] for f in selected],'selected_captures':[f['capture'] for f in selected],'attainable_keys':len(achievable_keys),'attainable_pairs':len(achievable_pairs),'attainable_opaque_pixels':len(achievable_pixels),'catalog_opaque_pixels':len(all_opaque)},indent=2),flush=True)
    verifier=ROOT/'tools/verify_native_route_objects.py';verifier_hash=sha(verifier)
    def run(f):
        dest=out/str(f['capture']);dest.mkdir(exist_ok=True)
        if not a.no_run:
            cmd=[sys.executable,str(verifier),'--assets',str(assets),'--context',str(context),'--core',str(a.core),'--output',str(dest),'--max-captures',str(f['prefix'])]
            with (dest/'verify.log').open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
        e=json.loads((dest/'evidence.json').read_text())
        assert e['captures']==f['prefix'] and e['pixel_checked_capture']==f['capture'] and e['pixels']==71680 and e['mismatches']==0
        assert e['hashes'][str(verifier)]==verifier_hash and e['hashes'][str(assets/'manifest.json')]==sha(assets/'manifest.json') and e['hashes'][str(context/'checkpoint.json')]==sha(context/'checkpoint.json')
        for name in ['hardware.ppm','hardware.ppm.vram','probe.bin']:
            assert e['hashes'][str(dest/name)]==sha(dest/name)
        independent=render(rows[f['prefix']-1],True)
        native=independent['native'];source_image=independent['source']
        target=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in native.get_flattened_data()]
        actual=Image.open(dest/'hardware.ppm').convert('RGB');assert list(actual.get_flattened_data())==target
        native.save(dest/'independent-expected.png');source_image.save(dest/'original-source-objects.png')
        print(f"Capture{f['capture']} prefix{f['prefix']}:71680 actual pixels exact",flush=True)
        return independent
    rendered={}
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        future={pool.submit(run,f):f for f in selected}
        for result in as_completed(future):rendered[future[result]['capture']]=result.result()
    assert sha(verifier)==verifier_hash,'Verifier changed during sample run'
    columns=3;cell_width=328;cell_height=252
    sheet=Image.new('RGB',(columns*cell_width,32+((len(selected)+columns-1)//columns)*cell_height),(28,28,28));draw=ImageDraw.Draw(sheet)
    draw.text((8,8),'Actual Genesis native object layers - no background/compositing admission',fill='white')
    group8=[]
    for n,f in enumerate(selected):
        x=(n%columns)*cell_width+4;y=32+(n//columns)*cell_height
        actual=Image.open(out/str(f['capture'])/'hardware.ppm').convert('RGB');sheet.paste(actual,(x,y+24))
        groups=sorted({pairs[pid][1] for pid in f['pairs']});draw.text((x,y),f"Capture{f['capture']} / prefix{f['prefix']} / groups{','.join(map(str,groups))}",fill='white')
        state=rendered[f['capture']]
        points=[(at%320,at//320) for at,t in enumerate(state['owner']) if t>=0 and pairs[t//256][1]==8]
        if points:
            left=max(0,min(x for x,y in points)-3);top=max(0,min(y for x,y in points)-3);right=min(320,max(x for x,y in points)+4);bottom=min(224,max(y for x,y in points)+4)
            group8.append((f['capture'],(left,top,right,bottom),state['source'].crop((left,top,right,bottom)),actual.crop((left,top,right,bottom))))
    sheet.save(out/'native-object-contact-sheet.png')
    if group8:
        height=sum(max(80,source.height*3+28) for _,_,source,_ in group8)
        detail=Image.new('RGB',(980,height+30),(28,28,28));draw=ImageDraw.Draw(detail);draw.text((8,7),'Original CPS RGB',fill='white');draw.text((498,7),'Actual Genesis RGB565 - same crop, nearest-neighbor3x',fill='white');y=30
        for capture,crop,source_image,actual in group8:
            draw.text((8,y),f"Capture{capture}, crop{crop}",fill='white');y+=20
            detail.paste(source_image.resize((source_image.width*3,source_image.height*3),Image.Resampling.NEAREST),(8,y));detail.paste(actual.resize((actual.width*3,actual.height*3),Image.Resampling.NEAREST),(498,y));y+=max(60,source_image.height*3+8)
        detail.save(out/'group8-source-vs-native.png')
    missing_pixels=[]
    for token in sorted(all_opaque-achievable_pixels):
        pid,pos=divmod(token,256);code,group=pairs[pid];y,x=divmod(pos,16)
        missing_pixels.append({'code':code,'source_group':group,'x':x,'y':y,'source_pen':original[code][y][x], 'reason':'never_in_crop' if token not in crop_pixels else 'always_behind_earlier_source_object'})
    (out/'remaining-source-pixels.json').write_text(json.dumps(missing_pixels,indent=2)+'\n')
    group8_colors={}
    for token in sorted(achievable_pixels):
        pid,pos=divmod(token,256);code,group=pairs[pid]
        if group!=8:continue
        y,x=divmod(pos,16);pen=original[code][y][x];entry=entries[code,group]
        rgb=source_palettes[group][pen];native=native_palette[((entry['attribute']>>13)&3)*16+entry['map'][pen+1]]
        actual=((native[0]>>3)*255//31,(native[1]>>2)*255//63,(native[2]>>3)*255//31)
        key=(pen,rgb,actual)
        item=group8_colors.setdefault(key,{'source_pen':pen,'source_word':source_palette_words[group][pen],'source_rgb':list(rgb),'actual_native_rgb565':list(actual),'verified_unique_source_pixel_coordinates':0,'source_codes':set()})
        item['verified_unique_source_pixel_coordinates']+=1;item['source_codes'].add(code)
    color_rows=[]
    for key,item in sorted(group8_colors.items()):item['source_codes']=sorted(item['source_codes']);color_rows.append(item)
    (out/'group8-source-native-colors.json').write_text(json.dumps(color_rows,indent=2)+'\n')
    pair_coverage=[]
    for pid,(code,group) in enumerate(pairs):
        all_pixels={t for t in all_opaque if t//256==pid};visible=all_pixels&achievable_pixels;clipped=all_pixels-crop_pixels;occluded=(all_pixels&crop_pixels)-achievable_pixels
        pair_coverage.append({'code':code,'source_group':group,'original_opaque_pixels':len(all_pixels),'actual_pixel_checked_source_coordinates':len(visible),'never_in_crop_source_coordinates':len(clipped),'in_crop_but_always_object_occluded':len(occluded)})
    summary={'scope':'Independent original source graphics/first-opaque object ownership versus actual native isolated object layers. Selected source prefix replays only; no BG/Window priority, live source integration, audio or FPS admission.', 'route_captures':len(frames),'selected_endpoints':len(selected),'actual_native_pixels_checked':len(selected)*71680,'independent_actual_pixel_mismatches':0,'source_template_keys_total':len(keys),'source_template_keys_visible_on_route':len(achievable_keys),'source_template_keys_actual_pixel_checked':len(set().union(*(f['keys'] for f in selected))),'keys_without_visible_source_pixels':[list(keys[k]) for k in sorted(set(range(len(keys)))-achievable_keys)],'code_group_pairs_total':len(pairs),'nontransparent_code_group_pairs':sum(e['status']==1 for e in entries.values()),'code_group_pairs_actual_pixel_checked':len(achievable_pairs),'original_opaque_source_pixel_coordinates':len(all_opaque),'original_opaque_source_pixel_coordinates_actual_checked':len(achievable_pixels),'source_coordinates_never_in_crop':len(all_opaque-crop_pixels),'source_coordinates_in_crop_but_always_occluded':len((all_opaque&crop_pixels)-achievable_pixels),'transparent_code_group_pairs':sum(e['status']==2 for e in entries.values()),'template_keys':[{'code':code,'source_attribute':attribute,'visible_in_samples':[f['capture'] for f in selected if key_id[code,attribute] in f['keys']]} for code,attribute in keys],'source_pixel_coverage':pair_coverage,'group8_visual_crops':[{'capture':capture,'crop':list(crop)} for capture,crop,_,_ in group8], 'rendering':'Source sprites preserve original source code arithmetic, local flips,512wrap,centered320crop and front-first object order. Every selected original opaque coordinate has a winning actual source sprite contribution. Original source images use full CPS numeric RGB; actual images retain core RGB565.', 'hashes':{str(path):sha(path) for path in [Path(__file__).resolve(),verifier,ROOT/'tools/cps1_gfx.py',ROOT/'tools/cps1_palette.py',archive,assets/'manifest.json',context/'checkpoint.json',context/'visual-contexts.csv',context/'captured-original-banks.bin',out/'selection-plan.json',out/'native-object-contact-sheet.png',out/'remaining-source-pixels.json',out/'group8-source-native-colors.json',*([out/'group8-source-vs-native.png'] if group8 else []),*(out/str(f['capture'])/'evidence.json' for f in selected)]}}
    (out/'coverage.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k not in ['hashes','template_keys','source_pixel_coverage','group8_visual_crops']},indent=2))
if __name__=='__main__':main()
