#!/usr/bin/env python3
"""Source anchors plus bounded actor-record checks; no GUI or capture replay."""
from pathlib import Path
import subprocess
from cps1_program import assemble_maincpu
from cps1_object_catalog import scripted_actor_message_sequence
p=assemble_maincpu(Path('willow.zip'))
checks=[(0x3b880,"30290006906d0408064000403229000a926d040c4601524124690014"),
 (0x3b99a,"3a0402457f80ea4d267b500e0244007fd844d6f340004e75"),
 (0x2244,"4a2e00006736422e0001302e0006906d0408064000200c4001c06220"),(0xa2f8,"4a6d040c6710556d8fb24eb900009ef24ef90003b5244e75"),
 (0xa3e2,"4eb900009ef24ef90003b524"),
 (0x9f04,"4eb90002f39e"),(0x2f3c8,'0003257e'),(0x2f3d0,'00032690'),
 (0x32592,'1d7c0002000e542e000241fa001e4ef81f40'),
 (0x325b4,'4eb81f4c4ef82244'),
 (0x326ba,'542e00023d7c00de003041fa03dc4ef81f40'),
 (0x326e2,'4a6d040c6604542e00034e75'),
 (0x326ee,'536e0030660c542e000341fa03d04ef81f40'),
 (0x32700,'4ef81f4c4eb81f4c4ef82162'),
 (0x1f40,'2d4800143d680008001c4e75'),
 (0x1f4c,'536e001c6634206e001430100880000f6702d040d04012280002670270020640000a3228000441f000004a416a0220502d4800143d680008001c4e75')]
for address,data in checks:
 assert p[address:address+len(bytes.fromhex(data))]==bytes.fromhex(data),hex(address)
assert scripted_actor_message_sequence(p)['message_task']['text']=='ONE YOUNG MAN STARTS ON HIS JOURNEY'
subprocess.run(['clang','-Wall','-Wextra','-Werror','tools/test_arcade_intro_actor.c','-o','build/test_arcade_intro_actor'],check=True)
subprocess.run(['build/test_arcade_intro_actor'],check=True)
print(f'{len(checks)} actor/animation source anchors verified; corrected opening-message catalog checked')

subprocess.run(['clang','-Wall','-Wextra','-Werror','tools/test_arcade_intro_actor_trace.c','-o','build/test_arcade_intro_actor_trace'],check=True)
subprocess.run(['build/test_arcade_intro_actor_trace'],check=True)
