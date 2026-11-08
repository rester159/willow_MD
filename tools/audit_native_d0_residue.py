#!/usr/bin/env python3
"""Classify source-width liveness in closed genuine v6 register-failure states.

This produces evidence, never an observer equivalence exception. Future task
types, unknown producer lineage, other registers and other PCs remain strict.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FRAMES = [1405,1448,1466,1467,1469,1470,1500,1501,1503,1506,1530]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--snapshots',type=Path,default=ROOT/'build/native-d0-residue-audit/v6-snapshots-complete')
    p.add_argument('--traces',type=Path,default=ROOT/'build/native-d0-residue-audit/closed-traces-v1')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-d0-residue-audit/proof-v1')
    a = p.parse_args()
    snapshots, traces, output = map(Path.resolve,(a.snapshots,a.traces,a.output))
    assert not output.exists()
    output.mkdir(parents=True)
    rom_path = snapshots/'oracle.bin'
    rom = rom_path.read_bytes()
    decoded = {}

    def decode(pc):
        if pc not in decoded:
            text = subprocess.check_output(['m68k-elf-objdump','-D','-b','binary','-m','m68k:68000','--insn-width=20',
                                             f'--start-address={pc}',f'--stop-address={pc+12}',str(rom_path)],text=True)
            match = next(re.match(r'^\s*([0-9a-f]+):\s*((?:[0-9a-f]{4} )+)\s*\t(.+)$',line)
                         for line in text.splitlines() if re.match(r'^\s*'+format(pc,'x')+r':',line))
            raw, assembly = bytes.fromhex(match[2]), match[3].strip()
            assert rom[pc:pc+len(raw)] == raw
            op, _, args = assembly.partition(' ')
            decoded[pc] = {'pc':pc,'bytes':raw.hex(),'size':len(raw),'assembly':assembly,'op':op,'args':args.strip()}
        return decoded[pc]

    def classify(q):
        op, args = q['op'], q['args']
        if '%d0' not in args:
            return 'no_incoming_d0_read'
        if op == 'moveml' and args.startswith('%sp@+,'):
            return 'full_context_reload'
        if op == 'moveml' and args.endswith(',%sp@-'):
            return 'exact_context_save'
        if op == 'movel' and args.endswith(',%d0') and '%d0' not in args[:-4]:
            return 'independent_full_definition'
        if op == 'moveq':
            return 'independent_full_definition'
        if op in ('dbf','dbra') or op.endswith(('b','w')):
            return 'low_word_or_byte_only'
        raise AssertionError(('Unproved D0 use',q))

    cases = []
    for frame in FRAMES:
        records = list(csv.DictReader((traces/f'frame-{frame}.instructions.csv').open()))
        accesses = list(csv.DictReader((traces/f'frame-{frame}.accesses.csv').open()))
        saved = json.loads((snapshots/f'frame-{frame}.registers.json').read_text())
        assert int(records[0]['pc'],16) == saved['registers'][16]
        assert int(records[0]['d0_before'],16) == saved['registers'][0]
        effects, spills, first_definition = [], [], None
        for row in records:
            pc = int(row['pc'],16)
            q = decode(pc)
            assert int(q['bytes'][:4],16) == int(row['opcode'],16)
            category = classify(q)
            if category != 'no_incoming_d0_read':
                effects.append(dict(**q,step=int(row['step']),classification=category))
            if category == 'exact_context_save':
                assert pc == 0x1750 and q['bytes'] == '48e7fffe'
                sp, d0 = int(row['sp_before'],16), int(row['d0_before'],16)
                address = (sp-60)&0xffffff
                words = [r for r in accesses if r['step']==row['step'] and r['kind']=='W' and
                         int(r['address'],16)&0xffffff in (address,address+2)]
                assert len(words)==2 and {int(r['address'],16)&0xffffff:int(r['value'],16) for r in words} == {address:d0>>16,address+2:d0&65535}
                spills.append({'pc':pc,'d0':d0,'address':address,'bytes':4,
                               'role':'Exact original1750 MOVEM context, reached from original9328 JSR after9324 MOVE.W #1,D0; no scalar full-width read.'})
            if category in ('full_context_reload','independent_full_definition'):
                first_definition = dict(**q,step=int(row['step']),classification=category)
                break
        assert first_definition and first_definition['pc'] == (0x1010ca if frame==1448 else 0x1e2c)
        work = (snapshots/f'frame-{frame}.source').read_bytes()[57856:]
        tasks = []
        for offset in range(0,512,32):
            if not work[offset]:
                continue
            state, sleep = work[offset:offset+2]
            pc, sp = struct.unpack_from('>2I',work,offset+4)
            assert offset in (0x60,0xa0) and state in (1,4,8)
            assert pc == (0x1756 if offset==0x60 else 0x1786)
            if offset==0xa0:
                assert struct.unpack_from('>I',work,sp&65535)[0] == 0x1e18
            tasks.append({'slot':offset,'state':state,'sleep':sleep,'pc':pc,'usp':sp})
        cases.append({'frame':frame,'initial_pc':saved['registers'][16],
                      'initial_d0':saved['registers'][0], 'instructions_to_first_full_definition':first_definition['step']+1,
                      'd0_uses':effects,'first_full_definition':first_definition,'full_width_spills':spills,
                      'live_task_context_guard':tasks})

    # The original IRQ's first subroutine is unconditional. Its incoming D0
    # highword is only saved/restored; every scalar use before191A is byte/word.
    irq_prefix = [decode(0x1554),decode(0x1558),decode(0x1560)]
    pc = 0x18d0
    while pc <= 0x191a:
        q = decode(pc)
        assert not q['op'].startswith('b') and q['op'] not in ('jmp','jsr','rts','rte')
        irq_prefix.append(q)
        if pc != 0x191a:
            assert classify(q) in ('no_incoming_d0_read','low_word_or_byte_only')
        pc += q['size']
    assert irq_prefix[-1]['bytes'] == '202d0058' and classify(irq_prefix[-1]) == 'independent_full_definition'
    assert decode(0x1554)['bytes']=='48e7fffe' and decode(0x1560)['bytes']=='6100036e'
    # All alternate worker paths through1DDC..1E32 remain word-only, then
    # sleep again or overwrite D0 completely at1E2C before the queue branch.
    worker = []
    pc = 0x1ddc
    while pc < 0x1e34:
        q = decode(pc)
        if pc != 0x1e2c:
            assert classify(q) in ('no_incoming_d0_read','low_word_or_byte_only')
        worker.append(q)
        pc += q['size']
    producer = [decode(at) for at in (0x3b57a,0x3b57c,0x9324,0x14d4,0x14f4,0x1524,0x101086)]
    assert producer[0]['bytes']=='2008' and producer[2]['bytes']=='303c0001'
    paths = [Path(__file__),ROOT/'tools/trace_native_d0_residue.c',ROOT/'tools/test_native_d0_residue_audit.py',
             rom_path,snapshots/'snapshot-evidence.json',snapshots/'d0-bus-changes.csv',traces/'runs.json']
    paths += list(snapshots.glob('frame-*')) + list(traces.glob('frame-*'))
    result = {'schema_version':1,'scope':__doc__,'cases':cases,'producer_and_partial_definitions':producer,
              'original_irq_prefix':irq_prefix,'queue_worker_alternative_paths':worker,
              'conclusion':'All11 recorded incoming D0 upperwords have no scalar full-width consumer before an independent full definition/context reload. The source9328 case copies the bits only into the exact original1750 saved context; it is not a zeroed or ignored RAM range. Source9324,14D4,1524 and metadata101086 replace only the low word, preserving the highword of the bank-end pointer produced by3B57A or its validated context restore.',
              'interrupt_caveat':'Closed continuation uses actual saved CPU state but does not schedule a new physical VBlank. The separately inspected unconditional originalIRQ prefix uses only incoming D0.B/.W until191A fully overwrites it, then original15B4 restores its saved context. This is a source-width argument, not a new cycle/FPS run.',
              'limits':'No observer exception is installed. Reusing this proof requires exact opcode, producer/context provenance and current task-dispatch guards. It does not prove arbitrary task sets, arbitrary pointers with equal low words, other registers or the earlier26DAE residue. Newly introduced wrappers remain separately responsible for preserving context.',
              'observer_equivalence_changed':False,'all_game_equivalence_admitted':False,
              'inputs':{str(path):sha(path) for path in paths}}
    (output/'evidence.json').write_text(json.dumps(result,indent=2)+'\n')
    (output/'instructions.txt').write_text('\n'.join(f'{pc:06x}: {q["bytes"]:20} {q["assembly"]}' for pc,q in sorted(decoded.items()))+'\n')
    print(json.dumps({'cases':len(cases),'full_width_spills':sum(len(c['full_width_spills']) for c in cases),
                      'first_definition_pcs':[hex(c['first_full_definition']['pc']) for c in cases],
                      'observer_equivalence_changed':False,'evidence_sha256':sha(output/'evidence.json')},indent=2))


if __name__ == '__main__':
    main()
