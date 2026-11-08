#!/usr/bin/env python3
"""Check source instruction anchors and native camera behavior against MAME."""
from pathlib import Path
import subprocess
from cps1_program import assemble_maincpu
from cps1_animation_catalog import (
 STAGE0_CAMERA_TARGET_INIT,STAGE0_CAMERA_TARGET_INIT_BYTES,
 STAGE0_CAMERA_TASK_STATE8,STAGE0_CAMERA_TASK_STATE8_BYTES,
 STAGE0_CAMERA_TASK_STATE10,STAGE0_CAMERA_TASK_STATE10_BYTES,
 STAGE0_CAMERA_SCRIPTED_Y_HANDLER,STAGE0_CAMERA_SCRIPTED_Y_HANDLER_BYTES)
ROOT=Path(__file__).resolve().parents[1]
p=assemble_maincpu(ROOT/'willow.zip')
anchors=[(0x7d80,bytes.fromhex("e548227c0091020043f100007a2732fc71a632fc000032fc71a632fc000043e9007851cdffea4e75")),
 (0xa2d0,bytes.fromhex("303c000d4eb87d80")),
 (0xa328,bytes.fromhex("303c000d4eb87d80303c00104eb87d80")),
 (0x7918,bytes.fromhex('0c11002f67580c110026660a')),
 (0x794c,bytes.fromhex('0200000f4eb81750')),
 (0x7994,bytes.fromhex('7200122e00117400142e0012e80ae4a9e789')),
 (0x1598,bytes.fromhex('0c2800010000660c532800016606')),
 (STAGE0_CAMERA_TARGET_INIT,STAGE0_CAMERA_TARGET_INIT_BYTES),
 (STAGE0_CAMERA_TASK_STATE8,STAGE0_CAMERA_TASK_STATE8_BYTES),
 (STAGE0_CAMERA_TASK_STATE10,STAGE0_CAMERA_TASK_STATE10_BYTES),
 (STAGE0_CAMERA_SCRIPTED_Y_HANDLER,STAGE0_CAMERA_SCRIPTED_Y_HANDLER_BYTES),
 (0x42fac,bytes.fromhex('227c0002eb24')),
 (0x43174,bytes.fromhex('122e0034302e006ce268')),
 (0x1a0c,bytes.fromhex('33ed744800800112303c0300906d00343b407448')),
 (0x1a20,bytes.fromhex('33ed744c00800116303c0700906d00383b40744c'))]
for address,data in anchors:
 assert p[address:address+len(data)]==data,f'Source anchor changed at {address:06x}'
print(f'Validated {len(anchors)} source instruction anchors',flush=True)
subprocess.run(['python3','tools/native_intro_text.py'],cwd=ROOT,check=True)
trace=ROOT/'build/arcade-trace/intro-camera.csv'
if not trace.exists():raise SystemExit('Run tools/arcade_intro_trace.lua with MAME willowu first; see SOURCE_PARITY.md')
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','tools/test_arcade_intro.c','-o','build/test_arcade_intro'],cwd=ROOT,check=True)
subprocess.run(['build/test_arcade_intro',str(trace)],cwd=ROOT,check=True)
