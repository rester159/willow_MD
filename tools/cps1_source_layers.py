"""ROM-backed Scroll2/Scroll3 lookup translated from Willow 43658..43768,
43D4E..43DB2. CPS world Y grows upwards; map row/within-chunk pixels use
its complement. Independent camera positions are inputs, not inferred here.
"""
from cps1_gfx import decode_tile
from cps1_palette import cps_rgb

def word(data,at):return int.from_bytes(data[at:at+2],'big')

class SourceLayers:
 def __init__(self,program,gfx,stage=0):
  self.program=program;self.gfx=gfx;self.stage=stage;self.cache={};self.maps={}
  for layer,pointers,modes in [(2,0x2dbf8,0x2dc1c),(3,0x2eb24,0x2eb44)]:
   pointer=int.from_bytes(program[pointers+stage*4:pointers+stage*4+4],'big')
   self.maps[layer]=(pointer,program[modes+stage])
 def record(self,layer,x,y):
  """x/y are map coordinates after complementing the source upward Y."""
  p=self.program;pointer,mode=self.maps[layer]
  masks=[(0xf00,4,15),(0x700,3,31),(0x1f00,2,63),(0x100,0,255)]
  mask,shift,xmask=masks[mode]
  index=((y&mask)>>shift)|((x>>8)&xmask)
  chunk=p[pointer+index]
  size,base,stride=(16,0x80000,0x400) if layer==2 else (32,0xc0000,0x100)
  n=256//size;offset=base+chunk*stride+(((x&255)//size)*n+(y&255)//size)*4
  code=word(p,offset);attr=word(p,offset+2)
  return (0x4000+(code&0x1fff) if layer==2 else 0xa00+code),attr
 def pen(self,layer,x,y):
  code,attr=self.record(layer,x,y);size=16 if layer==2 else 32
  if layer==3 and not 0xa00<=code<=0xdff:return 15,attr&31
  key=(size,code)
  if key not in self.cache:self.cache[key]=decode_tile(self.gfx,code,size)
  px=x%size;py=y%size
  return self.cache[key][size-1-py if attr&64 else py][size-1-px if attr&32 else px],attr&31
 def render(self,cameras,palette_ram,width=384,height=224,crop_x=0):
  """Return RGB bytes; palette RAM is the source palette/fade task's output."""
  result=bytearray(width*height*3)
  palettes={l:[cps_rgb(word(palette_ram,(0x800 if l==2 else 0xc00)+i*2)) for i in range(512)] for l in (2,3)}
  for y in range(height):
   for x in range(width):
    color=(0,0,0)
    for layer in (2,3):
     cx,cy=cameras[layer]
     # Hardware visible Y starts at 16 in a 256-line coordinate system.
     # 1A14/1A28 subtract camera Y; ~worldY selects the ROM chunk and row.
     pen,group=self.pen(layer,cx+x+crop_x,y-cy-240)
     if pen!=15:color=palettes[layer][group*16+pen];break
    at=(y*width+x)*3;result[at:at+3]=bytes(color)
  return bytes(result)
