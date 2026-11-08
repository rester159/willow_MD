"""Retain the original 7994 lookup tables and opening and phase-two pan message streams."""
from pathlib import Path
import hashlib

root = Path(__file__).resolve().parents[1]
program = (root / 'build/reference_maincpu.bin').read_bytes()
if len(program) != 0x100000:
    raise SystemExit('Expected the complete original 1 MiB program')
# Original callers: 8480/84AC (34/35, layout 1), 8542/86B6
# 88B6/88E8 also use 3C/3D (layout 1).
# (1B/1D, layout 0), 85CE/8742 (70/72, layout 2), 864E/87F2 (71/73, layout 2).
records = sorted({identity >> layout for identity, layout in
                  [(0x34,1),(0x35,1),(0x3c,1),(0x3d,1),(0x1b,0),(0x1d,0),(0x70,2),(0x71,2),(0x72,2),(0x73,2)]})
spans = [(0x7980, 0x7994), (0x7a1a, 0x7b1a)]
spans += [(0x70000 + record * 168, 0x70000 + (record + 1) * 168) for record in records]
base_spans = len(spans)
finish_ids = list(program[0x8e50:0x8e56]) + list(program[0x8e8e:0x8e94])
finish_records = sorted({identity >> 1 for identity in finish_ids} - set(records))
spans += [(0x8cc6,0x8cca),(0x8e50,0x8e56),(0x8e8e,0x8ea0)]
spans += [(0x70000+r*168,0x70000+(r+1)*168) for r in finish_records]
interlude_start = len(spans)
# 8FAA starts 3E with layout 1; retain its complete original 168-byte record.
interlude_record = 0x3e >> 1
if interlude_record not in set(records + finish_records):
    spans.append((0x70000 + interlude_record * 168, 0x70000 + (interlude_record + 1) * 168))
lines = ['/* Original opening message words; source addresses and all text bytes retained.',
         f' * Source SHA256: {hashlib.sha256(program).hexdigest()} */',
         '#ifndef WILLOW_NATIVE_OPENING_MESSAGE_DATA_H',
         '#define WILLOW_NATIVE_OPENING_MESSAGE_DATA_H']
for index, (start, end) in enumerate(spans):
    if index == base_spans: lines.append('#ifdef WILLOW_DEMO_FINISH_PROBE')
    if index == interlude_start: lines.extend(['#endif', '#ifdef WILLOW_INTERLUDE_SETUP_PROBE'])
    words = [int.from_bytes(program[at:at+2], 'big') for at in range(start, end, 2)]
    lines.append(f'static const unsigned short opening_message_{index}[]={{')
    lines.extend(','.join(f'0x{v:04x}' for v in words[n:n+12])+',' for n in range(0,len(words),12))
    lines.append('};')
lines += ['#endif']
lines += ['static inline unsigned char nativeOpeningMessageWordRead(unsigned long at,unsigned short*value){',
          'if(at&1)return 0;']
for index, (start, end) in enumerate(spans):
    if index == base_spans: lines.append('#ifdef WILLOW_DEMO_FINISH_PROBE')
    if index == interlude_start: lines.extend(['#endif', '#ifdef WILLOW_INTERLUDE_SETUP_PROBE'])
    lines.append(f'if(at>=0x{start:x}UL&&at<0x{end:x}UL){{*value=opening_message_{index}[(at-0x{start:x}UL)/2];return 1;}}')
lines += ['#endif','return 0;}', '#endif']
(root / 'native/intro_probe/src/opening_message_data.h').write_text('\n'.join(lines)+'\n')
print(f'Opening messages: {sum(end-start for start,end in spans)} exact source bytes')
