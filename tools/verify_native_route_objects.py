#!/usr/bin/env python3
"""Replay original captures through native selection, prepared geometry and VDP.

Every source capture's complete SAT/budget is checked on target. The final
isolated object layer is independently pixel-checked against decoded original
pixels and the explicit source-derived color map. No live/FPS claim is made.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import struct
import subprocess
from PIL import Image
from audit_native_prepared_catalog import children
from build_vdp_presentation_probe import ROOT,build
from prepare_native_route_objects import unpack


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def oracle(records, entries):
    sat=[];counts=[0]*224;pixels=[0]*224;pieces=[]
    for ordinal,(x,y,code,attr) in enumerate(records):
        for dx,dy,child in children(code,attr):
            sx=((x+dx)&511)-96;sy=((y+dy)&511)-16
            if sx<=-16 or sx>=320 or sy<=-16 or sy>=224:continue
            entry=entries[child,attr&31]
            if entry['status']==2:continue
            native=entry['attribute']|((attr<<6)&0x1800)
            sat.append([sy+128,0x500,native,sx+128])
            pieces.append((ordinal,sx,sy,child,attr,entry))
            for line in range(max(0,sy),min(224,sy+16)):
                counts[line]+=1;pixels[line]+=16
    assert len(sat)<=80 and max(counts)<=20 and max(pixels)<=320
    for n,s in enumerate(sat):s[1]|=n+1 if n+1<len(sat) else 0
    words=[len(sat)]+[v for s in sat for v in s]+[v for pair in zip(counts,pixels) for v in pair]
    return sat,words,pieces,max(counts),max(pixels)


def verify(assets, context, out, core, max_captures=464):
    assets,context,out=assets.resolve(),context.resolve(),out.resolve()
    out.mkdir(parents=True,exist_ok=True)
    assert 1<=max_captures<=464
    manifest=json.loads((assets/'manifest.json').read_text())
    for name,digest in manifest['hashes'].items():
        assert sha(Path(name))==digest, f'Prepared resource dependency changed: {name}'
    for name,digest in manifest['outputs'].items():assert sha(assets/name)==digest
    checkpoint=json.loads((context/'checkpoint.json').read_text())
    for name in ['visual-contexts.csv','visual-contexts.bin','captured-original-banks.bin']:
        assert sha(context/name)==checkpoint['files'][name]
    rows=list(csv.DictReader((context/'visual-contexts.csv').open()))
    banks=(context/'captured-original-banks.bin').read_bytes()
    entries={(e['code'],e['source_group']):e for e in manifest['entries']}
    records=[];frames=[];expected_words=[];max_sprites=max_pixels=topology_changes=0;prior=None
    with (context/'visual-contexts.bin').open('rb') as states:
        for row in rows:
            if row['manager']!=row['scene'] or row['scene']!='2':continue
            capture=int(row['capture']);count=int(row['source_count']);start=(capture-1)*2048
            complete=list(struct.iter_unpack('>4H',banks[start:start+2048]))
            current=complete[:count]
            assert row['source_valid']==row['native_valid']=='1' and 0<=count<=8
            assert row['source_count']==row['native_count'] and row['source_epoch']==row['native_epoch']
            assert all(x==attribute==0 for x,y,code,attribute in complete[count:])
            states.seek((capture-1)*90624)
            assert states.read(6144)==(assets/'source-palettes.bin').read_bytes()
            ports=states.read(512)
            assert int.from_bytes(ports[0x122:0x124],'big')==0x003e
            assert int.from_bytes(ports[0x10a:0x10c],'big')==0x91c0
            selector=int.from_bytes(ports[0x100:0x102],'big')
            assert selector==int(row['prior_selector'],16)==0x9000+int(row['bank'])*0x80
            sat,expected,last_pieces,sprites,pixels=oracle(current,entries)
            max_sprites=max(max_sprites,sprites);max_pixels=max(max_pixels,pixels)
            topology=[r[2:] for r in current]
            if prior is not None:topology_changes+=topology!=prior
            prior=topology
            frames.append((len(records),count,int(row['source_epoch']),int(row['bank']),len(sat),len(expected_words)))
            expected_words.extend(expected)
            records.extend(current)
            last_capture=capture
            if len(frames)==max_captures:break
    assert len(frames)==max_captures
    colors=list(struct.unpack('>64H',(assets/'cram.bin').read_bytes()))
    header=f'#define ROUTE_FRAME_COUNT {len(frames)}\n'
    header+='typedef struct {uint16_t offset,count;uint32_t epoch;uint16_t bank,pieces;uint32_t expected;} RouteTraceFrame;\n'
    header+='static const uint16_t routeColors[]={'+','.join(map(str,colors))+'};\n'
    header+='static const uint16_t routeRecords[][4]={'+','.join('{'+','.join(map(str,r))+'}' for r in records)+'};\n'
    header+='static const RouteTraceFrame routeFrames[]={'+','.join('{%d,%d,%dUL,%d,%d,%dUL}'%f for f in frames)+'};\n'
    header+='static const uint16_t routeExpected[]={'+','.join(map(str,expected_words))+'};\n'
    (out/'route_trace.h').write_text(header);(out/'route_objects.h').write_bytes((assets/'route_objects.h').read_bytes())
    # Isolated object layer: all source graphics/coordinates are original, while
    # background/name tables are zero so every actor pixel can be inspected.
    vram=bytearray(65536);first=manifest['first_tile']*32
    patterns=(assets/'object-patterns.bin').read_bytes();vram[first:first+len(patterns)]=patterns
    fixture=ROOT/'build/cartridge-gap-audit/frame1200_window_v1'
    regs=bytearray((fixture/'registers.bin').read_bytes());regs[18]=0
    inputs=out/'inputs';inputs.mkdir(exist_ok=True)
    for name,data in [('vram',vram),('cram',(assets/'cram.bin').read_bytes()),('vsram',bytes(80)),('registers',regs)]:
        (inputs/(name+'.bin')).write_bytes(data)
    build({n:inputs/(n+'.bin') for n in ['vram','cram','vsram','registers']},out,native_source=ROOT/'tools/native_route_objects.c')
    # Observe repeated native phase pairs. The host never supplies RAM, CPU
    # registers, SAT contents or execution time to the target.
    capture=(ROOT/'tools/capture_vdp_presentation.c').read_text()
    capture=capture.replace('if(frame!=30)return;','if(!captureNow)return;')
    capture=capture.replace('static unsigned frame,captured;','static unsigned frame,captured,captureNow;')
    capture=capture.replace('static uint64_t phaseCycles[6];\nstatic unsigned phaseCount;','static uint64_t startCycles;static unsigned activePhase,costCount;')
    a=capture.index(' if((address&0xffffff)==0xff0004){');b=capture.index('\n if(originalWrite16)',a)
    capture=capture[:a]+''' if((address&0xffffff)==0xff0004){
  uint64_t now=(uint64_t)frame*896040+(int64_t)profileCPU->cycles;
  if(value==1&&!activePhase){activePhase=1;startCycles=now;}
  else if(value==2&&activePhase){printf("COST %u %llu %.9f\\n",costCount++,(unsigned long long)(now-startCycles),(double)(now-startCycles)/53693.175);activePhase=0;}
  else exit(3);
 }'''+capture[b:]
    capture=capture.replace('for(frame=0;frame<=30;frame++)retro_run();','''for(frame=0;frame<20000;frame++){
 retro_run();uint32_t marker=0;for(unsigned n=0;n<4;n++)marker=(marker<<8)|profileRAM[n^1];
 if(marker==0x56445031){captureNow=1;frame++;retro_run();break;}if(marker==0x4641494c)break;
 }''')
    a=capture.index(' if(phaseCount!=6)');b=capture.index(' profileCPU->map[255].write16=originalWrite16;',a)
    capture=capture[:a]+f' if(costCount!={len(frames)}||activePhase)return 3;\n'+capture[b:]
    (out/'capture-route.c').write_text(capture)
    subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-DWILLOW_PRESENTATION_PHASES','-DWILLOW_PRESENTATION_VRAM_DUMP',
        '-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'tools'),str(out/'capture-route.c'),'-o',str(out/'capture')],check=True)
    with (out/'capture.log').open('w') as log:
        subprocess.run([str(out/'capture'),str(core),str(out/'probe.bin'),str(out/'hardware.ppm')],stdout=log,stderr=subprocess.STDOUT,check=True)
    costs=[{'index':int(n),'master_cycles':int(c),'ms':float(ms)} for line in (out/'capture.log').read_text().splitlines() if line.startswith('COST ') for _,n,c,ms in [line.split()]]
    assert len(costs)==len(frames)
    # Decode original neutral pens independently of emitted resident patterns.
    atlas_path=ROOT/'build/cartridge-gap-audit/full-object-capacity/full_object_tiles.json'
    atlas=json.loads(atlas_path.read_text());blocks=dict(atlas['entries']);neutral=atlas_path.with_suffix('.bin').read_bytes()
    assert hashlib.sha256(neutral).hexdigest()==atlas['binary_sha256']
    palette=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in colors]
    expected=Image.new('RGB',(320,224),palette[0])
    for ordinal,sx,sy,child,attr,entry in reversed(last_pieces):
        pens=unpack(neutral[blocks[child]*128:(blocks[child]+1)*128]);page=(entry['attribute']>>13)&3
        for y in range(16):
            for x in range(16):
                pen=pens[15-y if attr&64 else y][15-x if attr&32 else x]
                if pen and 0<=sx+x<320 and 0<=sy+y<224:expected.putpixel((sx+x,sy+y),palette[page*16+entry['map'][pen]])
    expected.save(out/'expected-objects.png')
    target=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in expected.get_flattened_data()]
    actual=Image.open(out/'hardware.ppm').convert('RGB');assert actual.size==(320,224)
    bad=sum(a!=b for a,b in zip(actual.get_flattened_data(),target));actual.save(out/'hardware.png')
    actual_vram=(out/'hardware.ppm.vram').read_bytes()
    expected_sat=struct.pack('>'+str(len(sat)*4)+'H',*(v for s in sat for v in s))
    assert actual_vram[0xf400:0xf400+len(expected_sat)]==expected_sat
    assert actual_vram[first:first+len(patterns)]==patterns
    files=[Path(__file__),ROOT/'tools/native_route_objects.c',ROOT/'src/willow_native_object_catalog.h',ROOT/'src/willow_native_object_catalog_m68k.h',
           ROOT/'src/willow_native_object_prepared.h',ROOT/'src/willow_native_object_cells_m68k.h',ROOT/'src/willow_native_object_sat_budget_m68k.h',
           ROOT/'src/willow_native_object_sat.h',ROOT/'src/willow_native_sat_publish.h',ROOT/'src/willow_native_presentation_plan.h',
           ROOT/'tools/build_vdp_presentation_probe.py',ROOT/'tools/capture_vdp_presentation.c',assets/'manifest.json',context/'checkpoint.json',
           out/'route_objects.h',out/'route_trace.h',out/'probe.bin',out/'capture-route.c',out/'capture.log',out/'hardware.ppm',out/'hardware.ppm.vram',core]
    report={'scope':f'Actual68000 capture replay of native source-key selection, all17 resolved templates, native SAT build/admission and VDP upload. Every SAT/budget word checked exactly for {len(frames)} captures. Final isolated object layer pixels only; no live producers, BG/Window integration, audio or FPS admission.',
        'captures':len(frames),'source_records':len(records),'topology_changes':topology_changes,'templates':17,
        'max_line_sprites':max_sprites,'max_line_pixels':max_pixels,'pixel_checked_capture':last_capture,
        'pixels':71680,'mismatches':bad,'final_sat_words':len(sat)*4,'resident_pattern_bytes_exact':len(patterns),
        'native_assertion_words_exact':len(expected_words),
        'timing_scope':'Display disabled; stable certified resident/palette state. Selection, pointer publication, native geometry, whole-frame readiness and SAT upload included. Raw acquisition, cold binding, source game logic, assertions and active-display transfer timing excluded.',
        'timing_ms':{'min':min(c['ms'] for c in costs),'mean':sum(c['ms'] for c in costs)/len(costs),'max':max(c['ms'] for c in costs)},
        'hashes':{str(p):sha(p) for p in files}}
    (out/'costs.json').write_text(json.dumps(costs,indent=2)+'\n');(out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    assert bad==0,report
    print(json.dumps({k:v for k,v in report.items() if k!='hashes'},indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--assets',type=Path,default=ROOT/'build/native-route-objects/prepared-v2')
    p.add_argument('--context',type=Path,default=ROOT/'build/native-object-publication/checkpoint-visual-state-v1')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-route-objects/replay-v2')
    p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    p.add_argument('--max-captures',type=int,default=464)
    args=p.parse_args();verify(args.assets,args.context,args.output,args.core,args.max_captures)
