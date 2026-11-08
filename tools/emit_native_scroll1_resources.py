#!/usr/bin/env python3
"""Emit immutable target Scroll1 lookup resources from a verified palette plan."""
from pathlib import Path
import argparse,hashlib,json,re

def emit(prefix):
 d=json.loads(prefix.with_suffix('.json').read_text());blob=prefix.with_suffix('.bin').read_bytes();assert hashlib.sha256(blob).hexdigest()==d['binary_sha256']
 name=re.sub('[^a-zA-Z0-9_]','_',prefix.name);lines=['/* Immutable source-keyed Scroll1; activate through shared presentation owner. */',f'#ifndef {name.upper()}_H',f'#define {name.upper()}_H','#include "willow_native_scroll1_tiles.h"',f'extern const uint8_t {name}_patterns[{len(blob)}];',f'static const uint16_t {name}_cram[64]={{'+','.join(hex(w) for page in d['cram'] for w in page)+'};',f'static const WillowScroll1Map {name}_maps[]={{']
 for m in d['maps']:lines.append('{{'+','.join(hex(w) for w in m['source_words'])+'},'+str(m['source_group'])+','+str(m['page'])+',{'+','.join(str(p) for p in m['pens'])+'}},')
 lines+=['};',f'static const WillowScroll1Entry {name}_entries[]={{']
 for e in d['entries']:lines.append('{'+str(e['code'])+',{'+','.join(str(p) for p in e['patterns_by_source_flip'])+'},'+','.join(str(e[k]) for k in ('source_group','layout','map'))+'},')
 lines+=['};',f'static const WillowScroll1Plan {name}_plan={{',f'{name}_entries,{name}_maps,{name}_patterns,{name}_cram,',f'{len(d["entries"])},{d["patterns"]},{len(d["maps"])}}};','#endif']
 prefix.with_suffix('.h').write_text('\n'.join(lines)+'\n');prefix.with_suffix('.s').write_text(f'.section .rodata\n.balign 4\n.global {name}_patterns\n{name}_patterns:\n.incbin "{prefix.with_suffix(".bin").resolve()}"\n')
 return {'entries':len(d['entries']),'pattern_bytes':len(blob),'metadata_bytes_68000':len(d['entries'])*14+len(d['maps'])*50+128+22,'pattern_sha256':hashlib.sha256(blob).hexdigest()}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('prefix',type=Path);a=p.parse_args();print(json.dumps(emit(a.prefix),indent=2))
