#!/usr/bin/env python3
"""Verify the fixed player's stage-zero intro path, not native gameplay."""
from pathlib import Path
import subprocess
from cps1_program import assemble_maincpu
p=assemble_maincpu(Path('willow.zip'))
checks=[(0x2f66,'0054'),(0x304c,'002c'),(0x3078,'00c00048'),
 (0x3012,'3d4000063d58000a'),(0xbf74,'542e0003'),
 (0xbfa8,'70034a2d0348670270021d40000f1d40003041fa10c24ef81f34'),
 (0xbfe8,'4a2d0348671c3d7c00100022542e000370021d40000f1d40003041fa107a4ef81f344e75'),
 (0xc00c,'536e00226608542e0002422e00034e75'),
 (0xd084,'00dc'),(0xd15a,'00092220800b0000ffff0033')]
for a,h in checks:assert p[a:a+len(bytes.fromhex(h))]==bytes.fromhex(h),hex(a)
subprocess.run(['clang','-Wall','-Wextra','-Werror','tools/test_arcade_intro_player.c','-o','build/test_arcade_intro_player'],check=True)
subprocess.run(['build/test_arcade_intro_player'],check=True)
print(f'{len(checks)} player source anchors checked; gate-open path has instruction evidence only')
