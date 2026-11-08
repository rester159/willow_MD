#!/usr/bin/env python3
"""Bake Stage 0's CPS layers into palette-correct native scrolling tiles."""
from pathlib import Path
from collections import Counter
import zipfile
from cps1_program import assemble_maincpu
from cps1_gameplay_maps import gameplay_stage0
from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from cps1_palette import palette_words, genesis_color, cps_rgb
from native_palette_fit import fit_tile_palettes, color_error
import json

def palette(hist, count=15):
    boxes=[list(hist)]
    while len(boxes)<count:
        candidates=[(max(max(c[k] for c in b)-min(c[k] for c in b) for k in range(3))*sum(hist[c] for c in b),i) for i,b in enumerate(boxes) if len(b)>1]
        if not candidates: break
        _,i=max(candidates);b=boxes.pop(i)
        channel=max(range(3),key=lambda k:max(c[k] for c in b)-min(c[k] for c in b))
        b.sort(key=lambda c:c[channel]);half=sum(hist[c] for c in b)/2;weight=0;split=1
        for j,c in enumerate(b[:-1]):
            weight+=hist[c]
            if weight>=half: split=j+1;break
        boxes.extend([b[:split],b[split:]])
    colors=[tuple(round(sum(c[k]*hist[c] for c in b)/sum(hist[c] for c in b)) for k in range(3)) for b in boxes]
    return colors+[(0,0,0)]*(count-len(colors))

def main():
    program=assemble_maincpu(Path('willow.zip'))
    with zipfile.ZipFile('willow.zip') as z: gfx=assemble_region(z)
    grid,width,height=gameplay_stage0(program)
    pals={layer:[[cps_rgb(w|0xf000) for w in palette_words(program,layer,0,g)] for g in range(32)] for layer in ('scroll2','scroll3')}
    cache={};cells=[]
    def sample(layer,x,y):
        chunk=grid[(15-y//256)*16+x//256]
        if not chunk:return None
        size,base,stride,offset=(16,0x80000,0x400,0x4000) if layer=='scroll2' else (32,0xc0000,0x100,0xa00)
        n=256//size;cell=((x//size)%n)*n+(y//size)%n;a=base+chunk*stride+cell*4
        code=int.from_bytes(program[a:a+2],'big');attr=int.from_bytes(program[a+2:a+4],'big')
        code=(code&0x1fff)+offset if layer=='scroll2' else code+offset
        if layer=='scroll3' and not 0xa00<=code<=0xdff:return None
        key=(size,code)
        if key not in cache:cache[key]=decode_tile(gfx,code,size)
        px=x%size;py=y%size
        if attr&32:px=size-1-px
        if attr&64:py=size-1-py
        pen=cache[key][py][px]
        return None if pen==15 else pals[layer][attr&31][pen]
    for ty in range(height//8):
        for tx in range(width//8):
            pixels=[]
            for py in range(8):
                for px in range(8):
                    x=tx*8+px;y=ty*8+py
                    color=sample('scroll2',x,y)
                    if color is None:color=sample('scroll3',x,y)
                    pixels.append(color or (0,0,0))
            cells.append(pixels)
    # Fit only scenery that can share the 320px viewport. Previously unrelated
    # sky/water elsewhere in the map consumed the opening forest's shades.
    step=24  # 192px sectors; the opening bank ends at the forest/water boundary.
    view_w,view_h=42,30
    bank_w=step+view_w-2  # viewport origins are aligned to two tiles
    horizontal_banks=((width-320)//8)//step+1
    banks=[]
    for bank in range(horizontal_banks):
        origin=bank*step
        local=[cells[y*(width//8)+x] if x<width//8 else [(0,0,0)]*64
               for y in range(view_h) for x in range(origin,origin+bank_w)]
        banks.append((origin,bank_w,view_h,local))
    # Retain a general map bank for the research controller's vertical camera.
    banks.append((0,width//8,height//8,cells))
    tiles=[bytes(32)];indices={tiles[0]:0};tilemap=[];metadata=[];bank_palettes=[]
    previews=[];quality=[];last_palettes=None
    forest_error=0;forest_colors=set();forest_source_colors=set()
    for bank,(origin,bw,bh,local) in enumerate(banks):
        palettes,slots,lookup=fit_tile_palettes(local)
        if last_palettes is not None:
            # Stable pen numbers preserve resident tile patterns at a bank edge.
            for slot in range(2):
                remaining=list(palettes[slot]);ordered=[None]*15
                for i,old in enumerate(last_palettes[slot]):
                    if old in remaining:ordered[i]=old;remaining.remove(old)
                for i,old in enumerate(last_palettes[slot]):
                    if ordered[i] is None:
                        best=min(remaining,key=lambda c:color_error(c,old));ordered[i]=best;remaining.remove(best)
                palettes[slot]=ordered[:15]
            lookup={c:[min((color_error(c,p),i+1) for i,p in enumerate(pal)) for pal in palettes]
                    for c in {c for cell in local for c in cell}}
        last_palettes=palettes
        offset=len(tilemap);preview=bytearray(bw*8*bh*8*3);squared_error=0
        for cell,pixels in enumerate(local):
            slot=min(range(2),key=lambda s:sum(lookup[c][s][0] for c in pixels))
            pens=[lookup[c][slot][1] for c in pixels]
            data=genesis_tiles([pens[y*8:y*8+8] for y in range(8)])
            if data not in indices:indices[data]=len(tiles);tiles.append(data)
            tilemap.append(indices[data]|(slot<<15))
            tx=cell%bw;ty=cell//bw
            for i,pen in enumerate(pens):
                color=palettes[slot][pen-1]
                error=sum((a-b)**2 for a,b in zip(color,pixels[i]))
                squared_error+=error
                if bank==0 and tx<40 and ty<28:
                    forest_error+=error;forest_colors.add(color);forest_source_colors.add(pixels[i])
                at=((ty*8+i//8)*bw*8+tx*8+i%8)*3
                preview[at:at+3]=bytes(color)
        maximum=0
        if bank<horizontal_banks:
            for x in range(0,step,2):
                maximum=max(maximum,len({v&0x7fff for y in range(view_h)
                    for v in tilemap[offset+y*bw+x:offset+y*bw+x+view_w]}))
            if maximum>1024:raise ValueError(f'Bank {bank} needs {maximum} VRAM slots; only 1024 available')
        metadata.append((origin,bw,bh,offset));bank_palettes.append(palettes);previews.append(preview)
        quality.append({'bank':bank,'origin_x':origin*8,'rgb_rmse':(squared_error/(len(local)*64*3))**.5,
                        'max_visible_tiles':maximum})
    forest_rmse=(forest_error/(320*224*3))**.5
    assert forest_rmse<15 and len(forest_colors)>=20, 'Opening forest color-fidelity regression'
    assert len(tiles)<16384, 'Keep the reverse VRAM index safely inside Genesis RAM'
    Path('build/willow_native_bg.bin').write_bytes(b''.join(tiles))
    rows=['/* Generated from arcade RGB; viewport palettes preserve two sprite palettes. */',
          '#ifndef WILLOW_NATIVE_BG_H','#define WILLOW_NATIVE_BG_H',
          f'#define NATIVE_BG_WIDTH {width//8}',f'#define NATIVE_BG_HEIGHT {height//8}',
          f'#define NATIVE_BG_TILE_COUNT {len(tiles)}',f'#define NATIVE_BG_BANK_STEP {step}',
          f'#define NATIVE_BG_HORIZONTAL_BANKS {horizontal_banks}',
          f'#define NATIVE_BG_FALLBACK_BANK {horizontal_banks}',
          f'static const u16 native_bg_palettes[{len(banks)}][2][16] = {{']
    for pals in bank_palettes:
        rows.append('{')
        for pal in pals:rows.append('{0,'+','.join(hex((c[0]//34)*2+(c[1]//34)*32+(c[2]//34)*512) for c in pal)+'},')
        rows.append('},')
    rows+=['};']
    for col,name,ctype in [(0,'origin_x','u16'),(1,'width','u16'),(2,'height','u16'),(3,'map_offset','u32')]:
        rows.append(f'static const {ctype} native_bg_bank_{name}[{len(banks)}] = {{'+','.join(str(m[col]) for m in metadata)+'};')
    rows+=['static const u16 native_bg_map[] = {']
    for i in range(0,len(tilemap),16):rows.append(','.join(hex(v) for v in tilemap[i:i+16])+',')
    rows+=['};','static const u8 native_bg_terrain[NATIVE_BG_WIDTH / 2][16] = {']
    for tx in range(width//16):
        chunk=grid[240+tx//16]
        props=[(program[0x80000+chunk*1024+((tx%16)*16+ty)*4+2]&252)//4 for ty in range(16)]
        rows.append('{'+','.join(map(str,props))+'},')
    rows+=['};','static const s16 native_bg_floor[NATIVE_BG_WIDTH * 8] = {']
    floors=[]
    for x in range(width):
        floor=-1
        for y in range(0,256,16):
            chunk=grid[15*16+x//256]
            at=0x80000+chunk*1024+(((x//16)%16)*16+y//16)*4
            prop=0x4712+(program[at+2]&252)*2
            flags,shape=program[prop:prop+2]
            if flags&8:
                fraction=15-(x&15) if shape&64 else x&15
                h=(shape&31) if shape&128 else min(16,(shape&15)+(fraction>>((shape>>4)&7)))
                floor=y+16-h;break
        floors.append(floor)
    for i in range(0,width,32):rows.append(','.join(map(str,floors[i:i+32]))+',')
    rows+=['};','#endif']
    Path('build/willow_native_bg.h').write_text('\n'.join(rows)+'\n')
    Path('build/native-stage0.ppm').write_bytes(f'P6\n{width} {height}\n255\n'.encode()+previews[-1])
    Path('build/native-forest.ppm').write_bytes(f'P6\n{bank_w*8} {view_h*8}\n255\n'.encode()+previews[0])
    Path('build/native-bg-quality.json').write_text(json.dumps({'source':'CPS RGB888 runtime palettes',
        'normal_rgb_levels':list(range(0,239,34)),'background_palettes':2,'banks':quality,
        'unique_tiles':len(tiles),'forest_rgb_rmse':forest_rmse,
        'forest_visible_colors':len(forest_colors),'forest_source_colors':len(forest_source_colors)},indent=2)+'\n')
    print(f'Baked {width}x{height}: {len(tiles)} tiles, {len(tiles)*32} bytes')
if __name__=='__main__':main()
