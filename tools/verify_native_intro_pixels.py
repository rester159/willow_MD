#!/usr/bin/env python3
"""Verify emulated pixels against independently composited converted layer tiles."""
from pathlib import Path
import re,json,csv
from PIL import Image
root=Path('native/intro_probe')
header=(root/'src/layers.h').read_text();tiles=(root/'res/layers.bin').read_bytes()
nums=lambda s:[int(x,0) for x in re.findall(r'0x[0-9a-f]+|\d+',s)]
entries=nums(header.split('intro_maps[]={')[1].split('};')[0])
words=nums(header.split('intro_palettes[2][16]={')[1].split('};')[0]);pals=[tuple(((w>>shift)&7)*34 for shift in (1,5,9)) for w in words]
pals=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in pals]
font=(root/'res/font.bin').read_bytes()
font_words=json.loads(Path('build/intro-font.json').read_text())['genesis_palette']
font_pal=[tuple(((w>>shift)&7)*34 for shift in (1,5,9)) for w in font_words]
font_pal=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in font_pal]
writes=[{k:int(v) for k,v in row.items()} for row in csv.DictReader(open('build/arcade-native-text-writes.csv'))]
def add_text(pixels,tick):
 latest={w['address']:w for w in writes if w['frame']<=690+tick}
 for address,w in latest.items():
  col=(address-0x910000)//128;row=((address-0x910000)%128)//4
  if not(4<=col<44 and 13<=row<=14) or w['code']==0x71a6:continue
  assert w['attribute']==3
  offset=((col&1)*64+w['code']-0x7020)*32
  for y in range(8):
   for x in range(8):
    b=font[offset+y*4+x//2];pen=(b&15) if x&1 else b>>4
    if pen:pixels[(row*8-16+y)*320+(col-4)*8+x]=font_pal[pen]
 return pixels
sprite_assets=json.loads(Path('build/intro-sprite-assets.json').read_text())
sprite_bytes=(root/'res/actors.bin').read_bytes()
overlay_assets=json.loads(Path('build/intro-overlay-assets.json').read_text())
overlay_bytes=(root/'res/overlay.bin').read_bytes()
overlay_player=(root/'res/overlay_player.bin').read_bytes()
overlay_codes={(r[0],r[1]):r[2:] for r in overlay_assets['records']}
overlay_player_offsets={v:i*4 for i,v in enumerate(overlay_assets['player_offsets'])}
overlay_palette=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in overlay_assets['palette']]
overlay_palette=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in overlay_palette]
controller={int(r['frame']):r for r in csv.DictReader(open('build/arcade-trace/actor-native/controller.csv'))}
def add_overlay(pixels,tick):
 ram=Path(f'build/arcade-trace/actor-native/text-{690+tick}.bin').read_bytes()
 for col in range(4,44):
  for row in range(2,30):
   at=col*128+row*4;code=int.from_bytes(ram[at:at+2],'big');attr=int.from_bytes(ram[at+2:at+4],'big')
   if code in (0x7020,0x71a6):continue
   pattern=overlay_codes[code,attr&31][col&1]
   for y in range(8):
    for x in range(8):
     xx=7-x if attr&32 else x;yy=7-y if attr&64 else y
     b=overlay_bytes[pattern*32+yy*4+xx//2];pen=b&15 if xx&1 else b>>4
     if pen:pixels[((row-2)*8+y)*320+(col-4)*8+x]=overlay_palette[pen]
 return pixels
sprite_frames={r[0]:r for r in sprite_assets['frames']}
sprite_palette=[tuple(((w>>shift)&7)*34 for shift in (1,5,9)) for w in sprite_assets['palette']]
sprite_palette=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in sprite_palette]
actor_trace={(int(r['frame']),int(r['slot'])):{k:int(v) for k,v in r.items()} for r in csv.DictReader(open('build/arcade-trace/actor-native/actors.csv'))}
for r in csv.DictReader(open('build/arcade-trace/actor-native/player.csv')):
 b=bytes.fromhex(r['record']);w=lambda o:int.from_bytes(b[o:o+2],'big')
 actor_trace[int(r['frame']),2]={'active':b[0],'animation':int.from_bytes(b[20:24],'big'),'x':w(6),'y':w(10),'camera':int(r['camera'])}
def add_actors(pixels,tick,late=False,slots=(0,2,1)):
 for slot in slots:
  a=actor_trace.get((690+tick,slot))
  if not a or not a['active'] or not a['animation'] or (a['y']-a['camera'])&65535>272:continue
  f=sprite_frames[a['animation']];ox=a['x']+f[5]-32;oy=256-a['y']+a['camera']+f[6]-16
  for cx,cy,w,h,offset in sprite_assets['chunks'][f[3]:f[3]+f[4]]:
   data=sprite_bytes;palette=font_pal if f[7]==0 else sprite_palette
   if late and slot==2:
    data=overlay_player;offset=overlay_player_offsets[offset];palette=overlay_palette
   for y in range(h*8):
    for x in range(w*8):
     at=(offset+(x//8)*h+y//8)*32+(y%8)*4+(x%8)//2
     b=data[at];pen=b&15 if x&1 else b>>4
     xx=ox+cx+x;yy=oy+cy+y
     if pen and 0<=xx<320 and 0<=yy<224:pixels[yy*320+xx]=palette[pen]
 return pixels
def render(tick):
 cy=512 if tick<=70 else max(0,512-2*(tick-70));origins=[512-cy,256-(cy>>1)]
 pixels=[]
 for y in range(224):
  for x in range(320):
   c=(0,0,0)
   for layer,base in [(0,0),(1,3680)]:
    yy=y+origins[layer];entry=entries[base+(yy//8)*40+x//8]
    tile=entry&0x7fff;at=tile*32+(yy%8)*4+(x%8)//2
    pen=(tiles[at]>>4) if x%2==0 else tiles[at]&15
    if pen:c=pals[(entry>>15)*16+pen];break
   pixels.append(c)
 return pixels
def main():
 report=[]
 for tick in sorted(set([1,70,130,190,250,325,400,464,468,472,476,480,630,800,810,830,850,890,898]+list(range(260,401,10))+list(range(326,357)))):
  im=Image.open(f'build/intro-probe-{tick}.ppm').convert('RGB');actual=list(im.getdata())
  # Text DMA publishes at the next VBlank (one frame). Camera registers have
  # an additional source-derived publication stage (two frames).
  # The video callback observes the last completed scanout while debug tick
  # may already refer to the next prepared frame. Report alignment explicitly.
  matches=[]
  for lag in [0,1,2]:
   late=int(controller.get(690+tick-1,{}).get('state',0))>=20
   expected=add_actors(render(max(0,tick-lag)),max(0,tick-2),late)
   expected=add_overlay(expected,tick-1) if late else add_text(expected,max(0,tick-1))
   matches.append(sum(a!=b for a,b in zip(actual,expected)))
  best=min(range(3),key=lambda i:matches[i]);report.append({'tick':tick,'scanout_lag':best,'text_scanout_lag':1,'mismatched_pixels':matches[best],'pixels':len(actual)})
  print(report[-1])
 Path('build/native-intro-pixel-parity.json').write_text(json.dumps(report,indent=2)+'\n')
 assert all(r['mismatched_pixels']==0 for r in report)
 assert all(r['scanout_lag']==2 for r in report if 70<r['tick']<326), 'Source register publication delay must be two frames'

if __name__=="__main__":main()
