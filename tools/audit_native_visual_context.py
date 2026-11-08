#!/usr/bin/env python3
"""Read-only epoch summary of exact PRE-IRQ source visual context snapshots."""
from pathlib import Path
import argparse,csv,hashlib,json,struct,collections
ROOT=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,default=ROOT/'build/native-object-publication/checkpoint-visual-state-v1');p.add_argument('--output',type=Path,default=ROOT/'build/cartridge-gap-audit/active-visual-context-v1');a=p.parse_args()
source=a.capture.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
checkpoint=json.loads((source/'checkpoint.json').read_text());names=['layout.json','visual-contexts.csv','visual-contexts.bin','captured-original-banks.bin']
for name in names:assert sha(source/name)==checkpoint['files'][name],name
layout=json.loads((source/'layout.json').read_text());raw=(source/'visual-contexts.bin').read_bytes();banks=(source/'captured-original-banks.bin').read_bytes();rows=list(csv.DictReader((source/'visual-contexts.csv').open()));assert len(raw)==len(rows)*layout['record_bytes']
active=[r for r in rows if r['manager']==r['scene']=='2'];regions={r['name']:(r['offset'],r['size']) for r in layout['regions']}
seen=collections.defaultdict(set);portvalues=collections.defaultdict(set);palettes={};sourcekeys=set();sourcegroups=set();scrollkeys=set();captures=[]
prior_palette=None;prior_view=None;palette_epoch=0;view_epoch=0
for r in active:
 capture=int(r['capture']);base=(capture-1)*layout['record_bytes'];record=memoryview(raw)[base:base+layout['record_bytes']]
 pieces={name:bytes(record[offset:offset+size]) for name,(offset,size) in regions.items()}
 for name in ['palettes','ports','row_scroll','scroll1']:seen[name].add(hashlib.sha256(pieces[name]).hexdigest())
 ports=struct.unpack('>256H',pieces['ports'])
 for n,w in enumerate(ports):portvalues[0x800000+n*2].add(w)
 count=int(r['native_count']);assert r['source_valid']==r['native_valid']=='1' and count==int(r['source_count']) and r['source_epoch']==r['native_epoch']
 groups=set()
 for x,y,code,attribute in struct.iter_unpack('>4H',banks[(capture-1)*2048:(capture-1)*2048+count*8]):
  sourcekeys.add((code,attribute));groups.add(attribute&31)
 sourcegroups.update(groups)
 # Explicit layer/priority and fixed-crop geometry identity excludes current
 # source X offsets; those are recorded separately as native BG scrolling.
 view=(ports[0x122//2],ports[0x170//2],*(ports[at//2] for at in [0x16e,0x16c,0x16a,0x168]),96,16,320,224)
 palette=pieces['palettes']
 if palette!=prior_palette:palette_epoch+=1;prior_palette=palette
 if view!=prior_view:view_epoch+=1;prior_view=view
 for group in groups|{0,1,10,31}:
  words=tuple(struct.unpack('>16H',palette[group*32:group*32+32]));palettes.setdefault(group,set()).add(words)
 col=((96+ports[0x10c//2])&511)>>3;row=((16+ports[0x10e//2])&511)>>3
 framekeys=set()
 for y in range(28):
  for x in range(40):
   c=(col+x)&63;rr=(row+y)&63;at=4*((rr&31)+(c<<5)+((rr&32)<<6));code,attribute=struct.unpack_from('>2H',pieces['scroll1'],at)
   if 0x7000<=code<=0x7fff:framekeys.add((code,attribute,c&1))
 scrollkeys.update(framekeys)
 captures.append({'capture':capture,'frame':int(r['frame']),'bank':int(r['bank']),'source_epoch':int(r['source_epoch']),'source_records':count,'palette_epoch':palette_epoch,'layer_priority_crop_epoch':view_epoch,'scroll2_x':ports[0x110//2],'scroll2_y':ports[0x112//2],'scroll3_x':ports[0x114//2],'scroll3_y':ports[0x116//2],'scroll1_column':col,'scroll1_row':row,'source_object_groups':sorted(groups),'source_scroll1_keys':len(framekeys)})
oldpath=ROOT/'build/cartridge-gap-audit/object_palette_reuse_p3_w16.json';old=json.loads(oldpath.read_text());matching={}
for group,words in old['source_palettes'].items():matching[group]=palettes.get(int(group))=={tuple(words)}
source_palette_words={str(group):[list(words) for words in sorted(variants)] for group,variants in sorted(palettes.items())}
summary={'scope':'Exact source visual inputs on the464-active-capture compact route; no inferred future stability, color fit, complete native rendering or FPS admission. Current source execution is relocated arcade68000, not complete high-level decompilation.', 'captures':len(active),'first_capture':int(active[0]['capture']),'last_capture':int(active[-1]['capture']),'region_distinct_hashes':{name:len(values) for name,values in seen.items()},'palette_epochs':palette_epoch,'layer_priority_crop_epochs':view_epoch,'unique_object_code_attribute_keys':len(sourcekeys),'source_object_groups':sorted(sourcegroups),'source_palette_variants':{str(g):len(v) for g,v in sorted(palettes.items())},'frozen1200_object_source_words_match':matching,'changed_port_words':{hex(at):sorted(values) for at,values in portvalues.items() if len(values)>1},'fixed_video_control':sorted(portvalues[0x800122]),'fixed_layer_control':sorted(portvalues[0x800170]),'fixed_scroll2_priority_masks':{hex(at):sorted(portvalues[at]) for at in [0x80016e,0x80016c,0x80016a,0x800168]},'source_scroll1_code_attribute_layout_keys':len(scrollkeys),'source_scroll1_palette_groups':sorted({attribute&31 for code,attribute,layout in scrollkeys}),'view_epoch_limit':'Only captured layer/priority registers and explicitcentered320crop/globalflip identity. Scroll2/3 horizontal positions change independently. Source mask values alone do not prove sprite/background overlap compositing across moving pixels; that needs the corresponding source pen proof.', 'hashes':{str(path):sha(path) for path in [Path(__file__).resolve(),source/'checkpoint.json',*(source/name for name in names),oldpath]}}
for name,data in [('summary.json',summary),('source-object-palettes.json',source_palette_words),('captures.json',captures),('source-template-keys.json',sorted(sourcekeys)),('source-scroll1-keys.json',sorted(scrollkeys))]:(out/name).write_text(json.dumps(data,indent=2)+'\n')
(out/'palettes.bin').write_bytes(prior_palette)
print(json.dumps({k:v for k,v in summary.items() if k not in ['hashes','changed_port_words']},indent=2))
