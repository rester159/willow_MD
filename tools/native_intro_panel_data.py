"""Retain the original 893C panel map without reducing tile or attribute words."""
from pathlib import Path
import hashlib
root = Path(__file__).resolve().parents[1]
program = (root / 'build/reference_maincpu.bin').read_bytes()
if len(program) != 0x100000:
    raise SystemExit('Expected complete original program')
words = [int.from_bytes(program[a:a+2], 'big') for a in range(0xd2000, 0xd4000, 2)]
lines = ['/* Original D2000 panel map, referenced by 893C.',
         f' * Source SHA256: {hashlib.sha256(program).hexdigest()} */',
         '#ifndef WILLOW_NATIVE_INTRO_PANEL_DATA_H', '#define WILLOW_NATIVE_INTRO_PANEL_DATA_H',
         'static const unsigned short nativeIntroPanelWords[4096]={']
lines += [','.join(f'0x{x:04x}' for x in words[n:n+12])+',' for n in range(0, len(words), 12)]
lines += ['};', 'static inline unsigned char nativeIntroPanelWordRead(unsigned long at,unsigned short*value){',
          'if(!value||(at&1)||at<0xd2000UL||at>=0xd4000UL)return 0;',
          '*value=nativeIntroPanelWords[(at-0xd2000UL)/2];return 1;}', '#endif']
(root/'native/intro_probe/src/intro_panel_data.h').write_text('\n'.join(lines)+'\n')
print('Intro panel: 8192 original map bytes retained')
