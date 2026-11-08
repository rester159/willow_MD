#!/usr/bin/env python3
"""Pin and validate the isolated source-native I/O test inputs."""
import hashlib
import json
from pathlib import Path

from native_io_relocation import PORT_OPERANDS, DEFAULT_MIRROR, relocate

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/native-video-relocation'
source = (ROOT / 'build/reference_maincpu.bin').read_bytes()
program, manifest = relocate(source, with_video=True)
assert program == (OUT / 'program-io-v2.bin').read_bytes()
assert manifest == json.loads((OUT / 'io-manifest-v2.json').read_text())
start, end = 0x18D0, 0x1A8E
ports = [p for p in manifest['port_operands'] if start <= p['instruction'] < end]
assert len(ports) == 26
expected = bytearray(source[start:end])
rows = []
for port in ports:
    at = port['instruction']
    opcode = int.from_bytes(source[at:at+2], 'big')
    assert opcode in (0x1039, 0x33ED, 0x13ED, 0x33C0)
    width = 1 if opcode in (0x1039, 0x13ED) else 2
    write = int(opcode != 0x1039)
    offset = port['offset'] - start
    expected[offset:offset+4] = port['target'].to_bytes(4, 'big')
    rows.append(f' {{{at:#x},{port["source"]:#x},{port["target"]:#x},{width},{write}}},')
assert bytes(expected) == program[start:end]
OUT.mkdir(exist_ok=True)
(OUT / 'io-test-input.bin').write_bytes(program)
(OUT / 'io-test-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
lines = [f'#define IO_MIRROR {DEFAULT_MIRROR:#x}u',
         f'#define IO_SOURCE_START {start:#x}u', f'#define IO_SOURCE_END {end:#x}u',
         'static const struct {unsigned pc,source,target,width,write;} ioPorts[] = {',
         *rows, '};']
lines += ['static const struct {unsigned pc,source,target,width,write;} ioAllPorts[] = {']
for port in manifest['port_operands']:
    opcode = int.from_bytes(source[port['instruction']:port['instruction']+2], 'big')
    assert opcode >> 12 in (1, 3)
    lines += [f' {{{port["instruction"]:#x},{port["source"]:#x},{port["target"]:#x},{1 if opcode >> 12 == 1 else 2},{int(opcode != 0x1039)}}},']
lines += ['};']
for name, data in [('ioOriginalSpan', source[start:end]), ('ioNativeSpan', program[start:end])]:
    lines += [f'static const unsigned char {name}[] = {{']
    lines += [' '+','.join(f'0x{b:02x}' for b in data[n:n+16])+',' for n in range(0,len(data),16)]
    lines += ['};']
(OUT / 'io-test-ports.h').write_text('\n'.join(lines)+'\n')
print(f'Pinned {len(ports)} closed-routine port operands; complete candidate SHA {hashlib.sha256(program).hexdigest()}')
