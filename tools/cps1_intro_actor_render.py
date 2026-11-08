#!/usr/bin/env python3
"""Translate the blocked-record path 3B880/3B99A for intro actor records."""
from pathlib import Path
import csv,json
from cps1_program import assemble_maincpu

def word(p,a):return int.from_bytes(p[a:a+2],'big')
def signed(w):return w-65536 if w&32768 else w

def emit(p,actor,camera_x,camera_y):
 # 2244 visibility test, followed by class-12 rendering queue.
 if not actor['active']:return None
 if ((actor['x']-camera_x+32)&65535)>448:return None
 if ((actor['y']-camera_y)&65535)>272:return None
 a=actor['animation'];count=word(p,a);attr=word(p,a+2);shape=word(p,a+4)
 assert count and not count&0x8000 and attr&0xff00, 'Only evidenced blocked-record path'
 table=int.from_bytes(p[0x3b9b2+((shape&0x7f80)>>5):0x3b9b6+((shape&0x7f80)>>5)],'big')
 at=table+signed(word(p,table+(shape&127)*2))
 ox,oy=signed(word(p,at)),signed(word(p,at+2))
 width=((attr>>8)&15)+1;height=((attr>>12)&15)+1
 if attr&32:ox=-ox-width*16
 if attr&64:oy=-oy-height*16
 dy=(actor['y']-camera_y)&65535
 sy=((dy&0xff00)|((~dy)&255))+1
 return {'x':(actor['x']-camera_x+64+ox)&511,'y':(sy+oy)&511,'code':word(p,a+10),'attribute':attr,'width':width,'height':height}

def actor_trace():
 rows=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(open('build/arcade-trace/actor-native/actors.csv'))]
 for r in csv.DictReader(open('build/arcade-trace/actor-native/player.csv')):
  b=bytes.fromhex(r['record']);w=lambda o:int.from_bytes(b[o:o+2],'big')
  rows.append({'frame':int(r['frame']),'slot':2,'camera':int(r['camera']),'active':b[0],'animation':int.from_bytes(b[20:24],'big'),'x':w(6),'y':w(10)})
 return rows

def verify():
 p=assemble_maincpu(Path('willow.zip'));rows=actor_trace();report=[]
 for frame in [960,990,1020,1050,1080]:
  ram=Path(f'build/arcade-trace/intro-vram-{frame}.bin').read_bytes()
  for row in rows:
   actor={k:int(v) for k,v in row.items()}
   if actor['frame']!=frame:continue
   expected=emit(p,actor,0,actor['camera'])
   if expected is None:
    assert word(p,actor['animation']+10) not in [word(ram,a+4) for a in range(0,256*8,8)]
    report.append({'frame':frame,'slot':actor['slot'],'record':None});continue
   want=[expected[k] for k in ('x','y','code','attribute')]
   records=[[word(ram,a+i) for i in range(0,8,2)] for a in range(0,256*8,8)]
   assert want in records,(frame,actor['slot'],want,records[:4])
   report.append({'frame':frame,'slot':actor['slot'],'record':expected})
 Path('build/arcade-intro-actor-render.json').write_text(json.dumps(report,indent=2)+'\n')
 print(f'Intro blocked sprite writer: {len(report)} records match captured object RAM exactly')
if __name__=='__main__':verify()
