#!/usr/bin/env python3
from pathlib import Path
import subprocess
from cps1_program import assemble_maincpu
p=assemble_maincpu(Path('willow.zip'))
for address,end,encoded in [(0x2c86,0x2ca4,'49ed7042303c000f4a2c0000660470004e7549ec004051c8fff070014e75'),
 (0x4109a,0x410b4,'302d0344e548227b004a3e196b424eb82c86663c197c00010000')]:
 assert p[address:address+len(bytes.fromhex(encoded))]==bytes.fromhex(encoded),hex(address)
assert p[0xc2c:0xc32]==bytes.fromhex('43f90091c000'), 'Handler destination must be palette RAM'
assert p[0x403f6:0x40400]==bytes.fromhex('ffff00ffffff02ffffff')
for address,encoded in [(0x2658,'41ed118e4ded1dc61b7c00321801303c00317200'),
 (0x26c0,'41ed11ee4ded36c61b7c00301804303c002f'),
 (0x2756,'41ed12b64ded4cc61b7c003c180d303c003b'),
 (0x2788,'41ed13564ded6ac61b7c00141813303c0013'),
 (0x2678,'1d7c0004001e310e4dee0080'),(0x26de,'1d7c0006001e310e4dee0040'),
 (0x2774,'1d7c000c001e310e4dee0040'),(0x27a6,'1d7c0010001e310e4dee0040'),
 (0x2a36,'70002d4000003d4000042d40000e3d4000123d4000201d40001f1d4000243d40002e')]:
 assert p[address:address+len(bytes.fromhex(encoded))]==bytes.fromhex(encoded),hex(address)
for address,encoded in [(0x40390,'30290003b06d038c6552906d184c064000a00c400120621630290005906d1850064000640c4000c8620460000030'),
 (0x403be,'d24141fa0086207010004e906720197c0001000019590021195900201959000f395900063959000a51cfff904e75')]:
 assert p[address:address+len(bytes.fromhex(encoded))]==bytes.fromhex(encoded),hex(address)
for address,encoded in [(0x2ca4,'49ed0502303c002f4a2c0000660470014e7549ec004051c8fff070004e75'),
 (0x9a92,'302d0344d040323b003e43fb103a3e196b324eb82ca4672c522c00001959000e19590010195900111959000f3959001239590006395900083959000a3959000c51cfffd04e75')]:
 assert p[address:address+len(bytes.fromhex(encoded))]==bytes.fromhex(encoded),hex(address)
subprocess.run(['clang','-Wall','-Wextra','-Werror','tools/test_arcade_stage_objects.c','-o','build/test_arcade_stage_objects'],check=True)
subprocess.run(['build/test_arcade_stage_objects'],check=True)
