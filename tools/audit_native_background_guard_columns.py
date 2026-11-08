#!/usr/bin/env python3
"""Read-only canonical BG residency demand for explicit guard-window choices."""
from functools import lru_cache
from pathlib import Path
import argparse
import json
import struct
import zipfile
from cps1_gfx import assemble_region, decode_tile
from prepare_native_route_background import ROOT, read_capture, active, upcoming_ports, cell_index, physical_code, sha


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture',type=Path,default=ROOT/'build/native-background-context/content-v2')
    p.add_argument('--assets',type=Path,default=ROOT/'build/native-route-background-audit/canonical-v1')
    p.add_argument('--capacity',type=Path,default=ROOT/'build/native-route-background-audit/capacity-v3')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-route-background-audit/guards-v2')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    assets=a.assets.resolve();manifest=json.loads((assets/'manifest.json').read_text())
    for name,digest in manifest['outputs'].items():assert sha(assets/name)==digest
    palette=(assets/'source-palettes.bin').read_bytes()
    lookup={(d['layer'],d['code'],d['attribute']):d for d in json.loads((assets/'entries.json').read_text())}
    rows,get,paths=read_capture(a.capture.resolve())
    with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
    @lru_cache(None)
    def image(layer,code):
        size=16 if layer==2 else 32;physical=physical_code(layer,code)
        return decode_tile(gfx,physical,size) if physical is not None else [[15]*size for _ in range(size)]

    @lru_cache(None)
    def working_set(near,far,nx,ny,fx,fy,mode):
        full=set();near_ids=set();required_far=set();bounds={};far_cells=[]
        for layer,size,xvalue,yvalue,mapbytes in [(2,16,nx,ny,near),(3,32,fx,fy,far)]:
            extent=64*size;xx=(96+xvalue)%extent;yy=(16+yvalue)%extent
            firstx=xx//8-1;firsty=yy//8;visible_rows=(yy+223)//8-firsty+1
            assert visible_rows<=29
            rows_count=visible_rows if mode=='columns43_visible_rows' else 32
            starty=firsty if rows_count==visible_rows else firsty-(32-visible_rows)//2
            bounds[layer]=(firstx*8-xx,starty*8-yy,(firstx+43)*8-xx,(starty+rows_count)*8-yy)
            for y in range(starty,starty+rows_count):
                for x in range(firstx,firstx+43):
                    sx,sy=x*8%extent,y*8%extent
                    code,attr=struct.unpack_from('>2H',mapbytes,cell_index(layer,sx//size,sy//size)*4)
                    d=lookup[layer,code,attr];subtile=(sy%size)//8*(size//8)+(sx%size)//8
                    pattern=d['references'][subtile]['pattern']
                    if not pattern:continue
                    full.add(pattern)
                    if layer==2:near_ids.add(pattern)
                    else:far_cells.append((pattern,x*8-xx,y*8-yy,code,attr,sx%32,sy%32))
        left,top,right,bottom=bounds[2]
        for pattern,screenx,screeny,code,attr,dx,dy in far_cells:
            original=image(3,code);required=False
            for y in range(8):
                if required:break
                for x in range(8):
                    farpen=original[31-(dy+y) if attr&64 else dy+y][31-(dx+x) if attr&32 else dx+x]
                    if farpen==15:continue
                    px,py=screenx+x,screeny+y
                    if not(left<=px<right and top<=py<bottom):
                        # No retained near cell certifies coverage here.
                        required=True;break
                    sx,sy=(96+nx+px)&1023,(16+ny+py)&1023
                    ncode,nattr=struct.unpack_from('>2H',near,cell_index(2,sx//16,sy//16)*4)
                    nearpen=image(2,ncode)[15-(sy&15) if nattr&64 else sy&15][15-(sx&15) if nattr&32 else sx&15]
                    if nearpen==15:required=True;break
            if required:required_far.add(pattern)
        return frozenset(full),frozenset(near_ids|required_far),frozenset(near_ids),frozenset(required_far)

    modes=('columns43_visible_rows','columns43_rows32')
    frames=[];sets=[];previous={name:(set(),set()) for name in modes}
    route_full={name:set() for name in modes};route_required={name:set() for name in modes}
    route_near={name:set() for name in modes};route_far={name:set() for name in modes}
    for row in rows:
        if not active(row):continue
        rec=get(row);ports=upcoming_ports(rec);displayed=struct.unpack('>256H',rec['ports'])
        assert rec['palettes']==palette
        assert ports[0x122//2]==0x003e and ports[0x170//2]==0x12f8
        maps=bytes(rec['scroll2']),bytes(rec['scroll3'])
        staged_view=tuple(ports[x//2] for x in (0x110,0x112,0x114,0x116))
        displayed_view=tuple(displayed[x//2] for x in (0x110,0x112,0x114,0x116))
        f={'capture':int(row['capture']),'frame':int(row['frame'])};record={'capture':int(row['capture'])}
        for mode in modes:
            full,required,near_ids,far_ids=working_set(*maps,*staged_view,mode)
            current_full,current_required,_,_=working_set(*maps,*displayed_view,mode)
            prior_full,prior_required=previous[mode]
            f[mode]=len(full);f[mode+'_previous_current_union']=len(full|prior_full)
            f[mode+'_previous_displayed_staged_union']=len(full|prior_full|current_full)
            f[mode+'_required']=len(required)
            f[mode+'_required_previous_displayed_staged_union']=len(required|prior_required|current_required)
            f[mode+'_new']=len(full-prior_full);f[mode+'_required_new']=len(required-prior_required)
            record[mode]=sorted(full);record[mode+'_required']=sorted(required)
            record[mode+'_displayed']=sorted(current_full);record[mode+'_displayed_required']=sorted(current_required)
            previous[mode]=(full,required);route_full[mode].update(full);route_required[mode].update(required)
            route_near[mode].update(near_ids);route_far[mode].update(far_ids)
        frames.append(f);sets.append(record)
    required_visible=set();overlays=set()
    capacity=a.capacity.resolve()
    for r in json.loads((capacity/'resident-sets.json').read_text()):
        required_visible.update(r['required_after_far_occlusion_canonical_ids']);overlays.update(r['additional_overlay_patterns_hex'])
    summary={'scope':'Conditional resource counts only. No source behavior or renderer allocation change.43columns starts one8x8 column before visibleleft. Visible-row case retains exactly8x8rows intersecting224pixels;32row case centers extra rows around that span. Full counts do not cull. Required counts retain far patterns where opaque far source pixels can show through original transparent near pens, or where no near guard cell is retained. No blending or color changes. Dynamic source opacity admission and cache scheduling remain unimplemented.',
             'views':'Previous capture staged view/map plus current capture displayed ports/map plus upcoming staged view/map. Source IRQ lifetime proof is external; future beyond next staged view is not assumed.',
             'window_scope':'105tile static Window reserve is conditional, not a verified activeHUD layout. High-priority overlaypatterns/SAT are external;7pattern routeunion reported separately.',
             'active_captures':len(frames),
             'peaks':{key:max(f[key] for f in frames) for key in frames[0] if key not in ('capture','frame') and not key.endswith('_new')},
             'peak_new_after_first':{key:max(f[key] for f in frames[1:]) for key in frames[0] if key.endswith('_new')},
             'route_union':{mode:{'full':len(route_full[mode]),'required':len(route_required[mode]),'near':len(route_near[mode]),'far_required':len(route_far[mode])} for mode in modes},
             'visible_required_route_union':len(required_visible),'additional_high_overlay_route_union':len(overlays),
             'fixed_visible_required_plus_overlay_with_existing_objects_conditional_window_and_low16':len(required_visible)+len(overlays)+500+105+16,
             'fixed_guard_required_plus_overlay_with_existing_objects_conditional_window_and_low16':{mode:len(route_required[mode])+len(overlays)+500+105+16 for mode in modes},
             'fixed_catalog_scope':'These are exact source-derived ROMpattern subsets observed needed along this bounded route, not captured map/position data. A native resource lease must reject any changed palette, missing pattern, unsupported layer/priority or uncovered view instead of silently omitting art. Fullgame residency not proved.',
             'hashes':{str(path):sha(path) for path in [Path(__file__).resolve(),ROOT/'tools/prepare_native_route_background.py',ROOT/'tools/cps1_gfx.py',ROOT/'willow.zip',assets/'manifest.json',assets/'entries.json',capacity/'resident-sets.json',*paths]}}
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    (out/'frames.json').write_text(json.dumps(frames,indent=2)+'\n')
    (out/'resident-sets.json').write_text(json.dumps(sets,separators=(',',':'))+'\n')
    (out/'route-pattern-sets.json').write_text(json.dumps({'visible_required':sorted(required_visible),'overlay_patterns_hex':sorted(overlays),**{mode:{'full':sorted(route_full[mode]),'required':sorted(route_required[mode]),'near':sorted(route_near[mode]),'far_required':sorted(route_far[mode])} for mode in modes}},separators=(',',':'))+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k!='hashes'},indent=2))

if __name__=='__main__':main()
