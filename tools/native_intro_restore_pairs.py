#!/usr/bin/env python3
"""Stage-zero intro restoration offsets, derived from original ROM map selectors.

This is asset addressing, not camera/actor logic. The renderer checks each
predecoded value against its live source-controlled map before using the table.
"""
from pathlib import Path
import hashlib
import json
import re
from cps1_program import assemble_maincpu
ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native/intro_probe'
def array(path, name):
    body = path.read_text().split(name+'[]={')[1].split('};')[0]
    return [int(v, 0) for v in re.findall(r'0x[0-9a-f]+|\d+', body)]
def generate():
    program = assemble_maincpu(ROOT/'willow.zip')
    offsets = []
    evidence = []
    for layer, columns, rows, y0, size, pointers, modes, base, stride in (
            (2, 20, 14, -240, 16, 0x2dbf8, 0x2dc1c, 0x80000, 0x400),
            (3, 10, 8, -256, 32, 0x2eb24, 0x2eb44, 0xc0000, 0x100)):
        pairs = array(NATIVE/f'src/scroll{layer}_data.h', f'nativeScroll{layer}Pairs')
        native_offsets = array(NATIVE/'src/background_pairs.h', f'nativeBackground{layer}Offsets')
        lookup = {pair: offset for pair, offset in zip(pairs, native_offsets)}
        pointer = int.from_bytes(program[pointers:pointers+4], 'big')
        mode = program[modes]
        mask, shift, xmask = ((0xf00,4,15),(0x700,3,31),(0x1f00,2,63),(0x100,0,255))[mode]
        for row in range(rows):
            for col in range(columns):
                x, y = 32+col*size, y0+row*size
                selector = ((y&mask)>>shift)|((x>>8)&xmask)
                chunk = program[pointer+selector]
                address = base+chunk*stride+((x&255)//size*(256//size)+(y&255)//size)*4
                pair = int.from_bytes(program[address:address+4], 'big')
                if layer == 3:
                    pair = (pair+0x0a000000)&0xffffffff
                offset = lookup[pair]
                if offset == 65535:
                    raise ValueError(f'Unsupported original pair at {address:x}')
                offsets.append(offset)
                evidence.append(dict(layer=layer, x=x, y=y, source_address=address, pair=pair, offset=offset))
    assert len(offsets) == 360
    lines = ['/* Original stage-zero map selectors; verified against live maps. */',
             '#ifndef WILLOW_INTRO_RESTORE_PAIRS_H', '#define WILLOW_INTRO_RESTORE_PAIRS_H',
             'static const u16 nativeIntroRestorePairs[]={']
    for n in range(0, len(offsets), 12):
        lines.append(' '+','.join(str(v) for v in offsets[n:n+12])+',')
    lines += ['};', '#endif']
    (NATIVE/'src/intro_restore_pairs.h').write_text('\n'.join(lines)+'\n')
    (ROOT/'build/native-intro-restore-pairs.json').write_text(json.dumps(dict(
        program_sha256=hashlib.sha256(program).hexdigest(), offsets=evidence), indent=2)+'\n')
    print('360 restoration offsets derived from original stage-zero ROM map selectors')
if __name__ == '__main__':
    generate()
