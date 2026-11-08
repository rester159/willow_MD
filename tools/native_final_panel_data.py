"""Retain the original 8BA2 panel map without reducing tile or attribute words."""
from pathlib import Path
import hashlib
root = Path(__file__).resolve().parents[1]
program = (root / 'build/reference_maincpu.bin').read_bytes()
if len(program) != 0x100000:
    raise SystemExit('Expected complete original program')
words = [int.from_bytes(program[a:a+2], 'big') for a in range(0xd6000, 0xd8000, 2)]
lines = ['/* Original D6000 panel map, referenced by 8BA2.',
         f' * Source SHA256: {hashlib.sha256(program).hexdigest()} */',
         '#ifndef WILLOW_NATIVE_FINAL_PANEL_DATA_H', '#define WILLOW_NATIVE_FINAL_PANEL_DATA_H',
         'static const unsigned short nativeFinalPanelWords[4096]={']
lines += [','.join(f'0x{x:04x}' for x in words[n:n+12])+',' for n in range(0, len(words), 12)]
lines += ['};', 'static inline unsigned char nativeFinalPanelWordRead(unsigned long at,unsigned short*value){',
          'if(!value||(at&1)||at<0xd6000UL||at>=0xd8000UL)return 0;',
          '*value=nativeFinalPanelWords[(at-0xd6000UL)/2];return 1;}', '#endif']
(root/'native/intro_probe/src/final_panel_data.h').write_text('\n'.join(lines)+'\n')
print('Final panel: 8192 original map bytes retained')
