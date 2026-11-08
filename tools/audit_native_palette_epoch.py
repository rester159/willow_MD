#!/usr/bin/env python3
"""Read-only palette writer observation; never patches the target program.

Derives a dedicated probe from the immutable visual-state observer, adds only
reference-bus logging, and checks every old/new byte plus all pre-IRQ snapshots.
The dirty-group lease below is a host model, not an admitted target hook.
"""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/native-object-publication/checkpoint-visual-state-v1'
CADENCE = ROOT / 'build/native-object-publication/checkpoint-compact-v1'
SOURCE = ROOT / 'build/reference_maincpu.bin'
START, SIZE, STRIDE = 0x91c000, 6144, 90624


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    with path.open() as f:
        return list(csv.DictReader(f))


def replace_once(text, old, new):
    assert text.count(old) == 1, (old, text.count(old))
    return text.replace(old, new)


def probe_source():
    text = (BASE / 'tools/test_native_compact_visual_state.c').read_text()
    # Includes in the original file are relative to the repository tools/.
    text = re.sub(r'#include "([^"]+)"', lambda m: '#include "' +
                  str((ROOT / 'tools' / m[1]).resolve()) + '"', text)
    observer = r'''
/* Read-only palette observer: source reference bus only, no target mutation. */
static FILE *paletteWrites,*paletteCaptures;
static unsigned paletteWriteCount;
static void paletteObserve(unsigned at,unsigned width,unsigned value){
 if(at<0x91c000||at+width>0x91d800)return;
 unsigned old=width==1?cps[at-0x900000]:willowMotionWord(cps+(at-0x900000));
 value&=width==1?255:65535;
 fprintf(paletteWrites,"%u,%u,%u,%x,%x,%u,%x,%x,%" PRIu64 ",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
  ++paletteWriteCount,frameNumber,latchCaptures,sourcePC(),at,width,old,value,stamp(),
  rawLong(0x204),cpu->dar[8],cpu->dar[9],cpu->dar[0],cpu->dar[1],cpu->dar[2],cpu->dar[6],cpu->dar[7],cpu->dar[15],rawLong(cpu->dar[15]));
}
'''
    text = replace_once(text, 'static void scrollObserve(', observer + '\nstatic void scrollObserve(')
    text = replace_once(text, 'videoBounds(at,1);scrollObserve(1,at,1,v);',
                        'videoBounds(at,1);paletteObserve(at,1,v);scrollObserve(1,at,1,v);')
    text = replace_once(text, 'videoBounds(at,2);if(at&1)abort();scrollObserve(1,at,2,v);',
                        'videoBounds(at,2);if(at&1)abort();paletteObserve(at,2,v);scrollObserve(1,at,2,v);')
    text = replace_once(text, ' if(fwrite(cps+0x1c000,1,0x1800,visualContexts)',
                        ' fprintf(paletteCaptures,"%u,%u,%u,%" PRIu64 "\\n",frameNumber,latchCaptures+1,paletteWriteCount,stamp());\n if(fwrite(cps+0x1c000,1,0x1800,visualContexts)')
    text = replace_once(text, ' publicationInit();', '''
 paletteWrites=outputFile("palette-writes.csv","w");paletteCaptures=outputFile("palette-captures.csv","w");
 if(!paletteWrites||!paletteCaptures)abort();
 fputs("event,frame,previous_capture,callback_pc,address,width,old,value,master_cycles,current_task,a0,a1,d0,d1,d2,d6,d7,sp,stack_return\\n",paletteWrites);
 fputs("frame,capture,write_count,master_cycles\\n",paletteCaptures);
 publicationInit();''')
    text = replace_once(text, ' publicationFinish();',
                        ' publicationFinish();fclose(paletteWrites);fclose(paletteCaptures);')
    return text


def analyze(out, core):
    identical = ['frames.csv','irqs.csv','irq-ring.bin','captured-original-banks.bin',
                 'metadata-acquisitions.csv','publication-latches.csv','producer-epochs.csv',
                 'headroom-events.csv','headroom-windows.csv','visual-contexts.bin',
                 'visual-contexts.csv','scroll1-sites.csv','scroll1-writes.csv']
    for name in identical:
        assert sha(out / name) == sha(BASE / name), name
    writes, captures = rows(out / 'palette-writes.csv'), rows(out / 'palette-captures.csv')
    # Independent grouping from exact original writer bodies. MOVE.L emits two
    # ordered 16-bit bus writes with one instruction-progress callback PC.
    families = {
        0x964: ('raw_095e', [pc for pc in range(0x964,0x974,2) for _ in range(2)], 512),
        0x9fe: ('copy_09fc', [pc for pc in range(0x9fe,0xa0e,2) for _ in range(2)], None),
        0xca2: ('animated_0c92', [pc for pc in (0xca2,0xcb0,0xcbe,0xccc) for _ in range(2)], 16),
        0x5274: ('fade_526e', [0x5274,0x5276,0x5278,0x527a], 16),
    }
    transactions=[]; cursor=0
    while cursor<len(writes):
        first=writes[cursor]; name,sequence,count=families[int(first['callback_pc'],16)]
        if count is None:
            loops=int(first['d1'],16)&65535
            assert loops in (15,31)
            count=(loops+1)*16
        block=writes[cursor:cursor+count];assert len(block)==count
        base=int(first['address'],16)
        for j,row in enumerate(block):
            assert int(row['callback_pc'],16)==sequence[j%len(sequence)], (cursor,j,row)
            assert int(row['address'],16)==base+j*2 and int(row['width'])==2
            assert row['current_task']==first['current_task']
        transactions.append({'family':name,'first_event':cursor+1,'last_event':cursor+count,
                             'first_frame':int(first['frame']),'last_frame':int(block[-1]['frame']),
                             'first_address':hex(base),'bytes':count*2,'task':first['current_task']})
        cursor+=count
    partial=[]; transaction_cursor=0
    for capture in captures:
        at=int(capture['write_count'])
        while transaction_cursor<len(transactions) and transactions[transaction_cursor]['last_event']<=at:
            transaction_cursor+=1
        if transaction_cursor<len(transactions):
            t=transactions[transaction_cursor]
            if t['first_event']<=at<t['last_event']:
                partial.append({'capture':int(capture['capture']),'frame':int(capture['frame']),
                                'written_words':at-t['first_event']+1,**t})
    (out/'writer-transactions.json').write_text(json.dumps({'transactions':transactions,'partial_at_capture':partial},indent=2)+'\n')
    index = rows(BASE / 'visual-contexts.csv')
    snapshots = (BASE / 'visual-contexts.bin').read_bytes()
    active = [i for i,r in enumerate(index) if r['manager'] == r['scene'] == '2']
    known = snapshots[active[0]*STRIDE:active[0]*STRIDE+SIZE]
    assert len(captures) == len(index) == 1797 and len(active) == 464
    image = bytearray(SIZE)
    dirty = set(range(192))  # Unknown initialization must never inherit a lease.
    mismatching = set(range(192))
    write_cursor = 0
    sites = collections.defaultdict(lambda: {'writes':0,'changed':0,'addresses':set(),'frames':set(),'values':set()})
    reports = []
    compared_groups = 0
    for i, (capture, meta) in enumerate(zip(captures,index,strict=True)):
        assert capture['capture'] == meta['capture'] == str(i+1)
        assert capture['frame'] == meta['frame'] and capture['master_cycles'] == meta['master_cycles']
        target = int(capture['write_count'])
        assert write_cursor <= target <= len(writes)
        previous = write_cursor
        while write_cursor < target:
            w = writes[write_cursor]
            assert int(w['event']) == write_cursor+1
            at,width,old,value = int(w['address'],16)-START,int(w['width']),int(w['old'],16),int(w['value'],16)
            assert width in (1,2) and 0 <= at <= SIZE-width
            assert int.from_bytes(image[at:at+width],'big') == old
            image[at:at+width] = value.to_bytes(width,'big')
            dirty.update(range(at//32,(at+width-1)//32+1))
            s = sites[(w['callback_pc'],width)]
            s['writes']+=1;s['changed']+=old!=value;s['addresses'].add(at+START)
            s['frames'].add(int(w['frame']));s['values'].add(value)
            write_cursor+=1
        assert bytes(image) == snapshots[i*STRIDE:i*STRIDE+SIZE], i
        scanned = len(dirty)
        compared_groups += scanned
        for g in dirty:
            if image[g*32:g*32+32] == known[g*32:g*32+32]: mismatching.discard(g)
            else: mismatching.add(g)
        lease = not mismatching
        assert lease == (image == known)
        reports.append({'capture':i+1,'frame':int(meta['frame']), 'writes_since_capture':target-previous,
                        'write_epoch':target,'dirty_groups_compared':scanned,
                        'known_palette_matches':lease,'mismatching_groups':len(mismatching),
                        'manager':int(meta['manager']),'scene':int(meta['scene'])})
        dirty.clear()
    # Writes after the final capture are still included and validated.
    for w in writes[write_cursor:]:
        at,width = int(w['address'],16)-START,int(w['width'])
        old,value = int(w['old'],16),int(w['value'],16)
        assert int.from_bytes(image[at:at+width],'big') == old
        image[at:at+width] = value.to_bytes(width,'big')
        s = sites[(w['callback_pc'],width)]
        s['writes']+=1;s['changed']+=old!=value;s['addresses'].add(at+START)
        s['frames'].add(int(w['frame']));s['values'].add(value)
    serial_sites = [{'callback_pc':pc,'width':width, 'writes':s['writes'],'changed':s['changed'],
                     'unchanged':s['writes']-s['changed'],'first_frame':min(s['frames']),
                     'last_frame':max(s['frames']),'frames':len(s['frames']),
                     'min_address':hex(min(s['addresses'])),'max_address':hex(max(s['addresses'])),
                     'distinct_addresses':len(s['addresses']),'distinct_values':len(s['values'])}
                    for (pc,width),s in sorted(sites.items(),key=lambda item:int(item[0][0],16))]
    (out/'writer-sites.json').write_text(json.dumps(serial_sites,indent=2)+'\n')
    with (out/'lease-model.csv').open('w') as f:
        writer=csv.DictWriter(f,fieldnames=list(reports[0]));writer.writeheader();writer.writerows(reports)
    active_reports=[r for r in reports if r['manager']==r['scene']==2]
    result={'schema_version':1,'scope':'Read-only source bus observation and host lease model; no native palette hook, production patch, palette fitting or target cost admission.',
            'paired_frames':1800,'captures':len(captures),'active_captures':len(active),
            'palette_bus_writes':len(writes),'changed_writes':sum(s['changed'] for s in serial_sites),
            'write_callback_sites':len(sites),'widths':sorted({int(w['width']) for w in writes}),
            'source_writer_transactions':dict(collections.Counter(t['family'] for t in transactions)),
            'partial_writer_transactions_at_capture':len(partial),
            'observed_dirty_capture_intervals':sum(r['writes_since_capture']>0 for r in reports),
            'known_palette_matching_captures':sum(r['known_palette_matches'] for r in reports),
            'first_matching_capture':next(r['capture'] for r in reports if r['known_palette_matches']),
            'active_writes_between_captures':sum(r['writes_since_capture'] for r in active_reports),
            'active_known_palette_matches':sum(r['known_palette_matches'] for r in active_reports),
            'model_groups_compared':compared_groups,'full_scan_groups_avoided':len(captures)*192-compared_groups,
            'model_note':'24-byte dirty and 24-byte mismatch bitsets, invalidation on every write including equal values. Dirty-group recheck is a diagnostic model only; complete native writer interception and coherent certification still required.',
            'source_rom_sha256':sha(SOURCE),'core_sha256':sha(core),'known_palette_sha256':hashlib.sha256(known).hexdigest(),
            'identical_baseline_outputs':{name:sha(out/name) for name in identical},
            'sources':{str(p.relative_to(ROOT)):sha(p) for p in [Path(__file__),BASE/'tools/test_native_compact_visual_state.c',out/'probe.c']},
            'outputs':{name:sha(out/name) for name in ['palette-writes.csv','palette-captures.csv','writer-sites.json','writer-transactions.json','lease-model.csv','probe','cadence.log']},
            'limits':['Route starts at6F2; power-on diagnostics before that are not executed.',
                      'Observed route is not all-stage writer coverage.',
                      'Callback PC is instruction-progress PC; map to exact opcode bytes before selecting hooks.',
                      'A completion-only hook is unsafe if an IRQ can observe partial writes.',
                      'No source RAM/SRAM/stack allocation or native timing is claimed.']}
    (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if not isinstance(v,(dict,list))},indent=2))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/native-palette-epoch-audit/route-v1')
    parser.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    parser.add_argument('--no-run',action='store_true')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    evidence=json.loads((BASE/'evidence.json').read_text())
    assert sha(SOURCE)==evidence['source_rom_sha256'] and sha(args.core)==evidence['core_sha256']
    for name,digest in evidence['target_products'].items():
        assert sha(ROOT/'build/native-object-publication/compact-v1'/name)==digest,name
    if not args.no_run:
        (out/'probe.c').write_text(probe_source())
        subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-fstrict-aliasing','-fsanitize=address,undefined',
                        '-fno-sanitize-recover=all',str(out/'probe.c'),'-o',str(out/'probe')],cwd=ROOT,check=True)
        with (out/'cadence.log').open('w') as log:
            subprocess.run([str(out/'probe'),str(args.core),'1800','--coin-route',str(out)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    analyze(out,args.core)


if __name__=='__main__': main()
