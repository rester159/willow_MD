#!/usr/bin/env python3
"""Pack original normal-player tables for native C; no runtime trace inputs."""
from pathlib import Path
from cps1_program import assemble_maincpu

program = assemble_maincpu(Path('willow.zip'))
# Half-open original ROM ranges. Animation records retain original pointers,
# resolved by the data reader; no arcade instructions are executed at runtime.
segments = [
    ('animation', 0xd076, 0xd4ba),
    ('motion', 0xc01c, 0xc03c),
    ('walls', 0x3876, 0x38f6),
    ('feet', 0x397e, 0x39fe),
    ('head', 0x3a64, 0x3a84),
    ('ledge', 0x3af2, 0x3b0a),
    ('collision', 0x4712, 0x488a),
    ('charge_effect', 0xc706, 0xc70e),
    ('charge_tier', 0xcb08, 0xcb24),
    ('shot_power', 0xcb4c, 0xcb84),
    ('shot_kind', 0xcc50, 0xcc5e),
    ('climb', 0xcd74, 0xcda4),
    ('inventory_slots', 0x4cda, 0x4ce4),
    ('inventory_icons', 0x4dd0, 0x4e12),
    ('health_colors', 0xe5000, 0xe50c0),
    ('charge_hud', 0x4bfa, 0x4c4e),
    ('charge_lengths', 0x4fb6, 0x4fbe),
    ('charge_caps', 0x5080, 0x509c),
    ('stage_input', 0x47e8e, 0x4808e),
    ('attract_input', 0x4808e, 0x4840e),
    ('alternate_meter', 0x4c84, 0x4cda),
    ('hud_label_pointer', 0x5c96, 0x5c98),
    ('hud_labels', 0x5f6a, 0x5fa2),
    ('hud_dips', 0x1bee, 0x1bf6),
    ('boot_high_score', 0x183c, 0x1840),
    ('stage0_descriptor_pointer', 0x304c, 0x304e),
    ('stage0_descriptor', 0x3078, 0x3080),
]
lines = ['/* Generated from the original arcade ROM by native_player_data.py. */',
         '#ifndef WILLOW_NATIVE_PLAYER_DATA_H', '#define WILLOW_NATIVE_PLAYER_DATA_H']
for name, start, end in segments:
    words = [int.from_bytes(program[a:a+2], 'big') for a in range(start, end, 2)]
    storage = '' if name in ('stage_input', 'attract_input') else 'static '
    lines.append(f'{storage}const unsigned short player_{name}_words[]={{')
    lines.extend(','.join(f'0x{w:04x}' for w in words[n:n+12])+',' for n in range(0,len(words),12))
    lines.append('};')
lines.append('static unsigned char nativePlayerDataRead(unsigned long address,unsigned short *value){')
lines.append('if(address&1)return 0;')
for name, start, end in segments:
    lines.append(f'if(address>=0x{start:x}UL && address<0x{end:x}UL){{*value=player_{name}_words[(address-0x{start:x}UL)/2];return 1;}}')
lines.extend(['return 0;}', '#endif'])
Path('native/intro_probe/src/player_data.h').write_text('\n'.join(lines)+'\n')
print(f'Native player data: {sum(end-start for _,start,end in segments)} ROM bytes, {len(segments)} source tables')

# Share the same ROM words with the separately compiled input service.
inputs = [segment for segment in segments if segment[0] in ('stage_input', 'attract_input')]
lines = ['/* Generated from original ROM ranges by native_player_data.py. */',
         '#ifndef WILLOW_NATIVE_INPUT_DATA_H', '#define WILLOW_NATIVE_INPUT_DATA_H']
for name, start, end in inputs:
    lines.append(f'extern const unsigned short player_{name}_words[{(end-start)//2}];')
lines += ['static unsigned char nativeInputDataRead(unsigned long address,unsigned short *value){',
          'if(address&1)return 0;']
for name, start, end in inputs:
    lines.append(f'if(address>=0x{start:x}UL && address<0x{end:x}UL){{*value=player_{name}_words[(address-0x{start:x}UL)/2];return 1;}}')
lines += ['return 0;}', '#endif']
Path('native/intro_probe/src/input_data.h').write_text('\n'.join(lines)+'\n')
