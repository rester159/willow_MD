#!/usr/bin/env python3
"""Summarize exact background captures and cross-check the earlier IRQ peeks."""
from pathlib import Path
import argparse,csv,hashlib,json,struct

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--capture',type=Path,default=ROOT/'build/native-background-context/content-v2')
 p.add_argument('--lifetime',type=Path,default=ROOT/'build/native-background-lifetime/v1')
 a=p.parse_args();root=a.capture.resolve();lifetime=a.lifetime.resolve()
 evidence=json.loads((root/'capture-evidence.json').read_text())
 for name,digest in evidence['outputs'].items():assert sha(root/name)==digest,name
 layout=json.loads((root/'layout.json').read_text())
 rows=list(csv.DictReader((root/layout['index']).open()))
 active=[r for r in rows if r['manager']==r['scene']=='2']
 regions={r['name']:r for r in layout['regions']}
 distinct={name:set() for name in regions}
 ports={at:set() for at in [0x102,0x104,0x106,0x108,0x10a,0x10c,0x10e,0x110,0x112,0x114,0x116,0x122,0x168,0x16a,0x16c,0x16e,0x170]}
 pairs={name:set() for name in ['scroll1','scroll2','scroll3']};maps=[]
 with(root/layout['binary']).open('rb')as f:
  for row in active:
   f.seek(int(row['sample'])*layout['record_bytes']);blob=f.read(layout['record_bytes'])
   pieces={name:blob[d['offset']:d['offset']+d['size']] for name,d in regions.items()}
   for name,piece in pieces.items():distinct[name].add(hashlib.sha256(piece).hexdigest())
   for name in pairs:pairs[name].update(struct.iter_unpack('>HH',pieces[name]))
   for at in ports:ports[at].add(struct.unpack_from('>H',pieces['ports'],at)[0])
   maps.append(pieces['scroll2']+pieces['scroll3'])
 irqs=list(csv.DictReader((lifetime/'background-irqs.csv').open()))
 active_irqs=[r for r in irqs if int(r['active_index'])]
 oldmaps=(lifetime/'background-active-maps.bin').read_bytes()
 assert len(active)==len(active_irqs) and len(oldmaps)==len(active)*32768
 for i,(r,irq,ourmap)in enumerate(zip(active,active_irqs,maps)):
  assert r['frame']==irq['frame'],i
  assert ourmap==oldmaps[i*32768:(i+1)*32768],i
 summary={'scope':'Read-only complete map/palette/port input coverage, not rendered background admission. Unique code/attribute pairs include offscreen RAM entries and do not establish visible resource demand.',
  'active_captures':len(active),'distinct_region_snapshots':{n:len(v) for n,v in distinct.items()},
  'full_map_unique_code_attribute_pairs':{n:len(v) for n,v in pairs.items()},'port_words':{hex(n):sorted(v) for n,v in ports.items()},
  'lifetime_irq_boundary_crosscheck':{'active_snapshots_compared':len(active),'all_scroll2_scroll3_bytes_identical':True,
   'different_boundaries':'Lifetime is first native IRQ store; this capture is later prior-port object acquisition before source1554. Equality is observed only for this route.',
   'inputs':{str(path):sha(path) for path in [lifetime/'background-irqs.csv',lifetime/'background-active-maps.bin']}},
  'capture_evidence_sha256':sha(root/'capture-evidence.json'),'audit_tool_sha256':sha(Path(__file__))}
 (root/'input-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
 print(json.dumps({k:v for k,v in summary.items() if k!='port_words'},indent=2))

if __name__=='__main__':main()
