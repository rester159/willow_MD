"""Extract original task, high-score, latched-option and replay tables without substitutions."""
from pathlib import Path
import hashlib

root = Path(__file__).resolve().parents[1]
program = (root / "build/reference_maincpu.bin").read_bytes()
if len(program) != 0x100000:
    raise SystemExit("Expected the complete 1 MiB original program")
table = program[0x17DC:0x188C]
words = [int.from_bytes(table[n:n + 2], "big") for n in range(0, len(table), 2)]
lines = [
    "/* Original 17DC..188B task/high-score tables; do not replace source handles with native pointers.",
    f" * Source program SHA256: {hashlib.sha256(program).hexdigest()} */",
    "#ifndef WILLOW_NATIVE_TASK_STARTUP_DATA_H",
    "#define WILLOW_NATIVE_TASK_STARTUP_DATA_H",
    "static const unsigned short nativeTaskStartupWords[88]={",
]
for n in range(0, len(words), 8):
    lines.append(" " + ",".join(f"0x{value:04x}" for value in words[n:n + 8]) + ",")
lines += ["};"]
for name,start,end in [('nativeBootCoinOptions',0x1b2e,0x1b46),('nativeBootHealthOptions',0x1bee,0x1bf6),('nativeTaskCommandTargets',0x1e34,0x1e54),('nativeAttractReplayDescriptors',0x8cca,0x8cda),('nativeBootStateTargets',0x7ff4,0x8000),('nativeOpeningStateTargets',0x8288,0x8294),('nativePanStateTargets',0x8526,0x8542),('nativeTextStateTargets',0x888e,0x889e),('nativePanelScrollTargets',0x889e,0x88a4),('nativePanelTextTargets',0x88a4,0x88a8),('nativePanelReplaceTargets',0x88a8,0x88aa),('nativePanelExitTargets',0x88aa,0x88b0),('nativeFinalPanelTargets',0x88b0,0x88b6)]:
    values=[int.from_bytes(program[n:n+2], 'big') for n in range(start,end,2)]
    lines += [f'static const unsigned short {name}[{len(values)}]={{'+','.join(f'0x{x:04x}' for x in values)+'};']
# Source text table and all 23 supported strings, including embedded headers.
values=[int.from_bytes(program[n:n+2], 'big') for n in range(0x5c7e,0x61dc,2)]
lines += ['#ifdef WILLOW_SCENE_BOOT_PROBE', 'static const unsigned short nativeQueuedTextWords[%d]={' % len(values)]
for n in range(0,len(values),8):
    lines.append(' '+','.join(f'0x{x:04x}' for x in values[n:n+8])+',')
lines += ['};']
values=[int.from_bytes(program[n:n+2], 'big') for n in range(0x50ac,0x50c6,2)]
lines += ['static const unsigned short nativePaletteTaskTargets[13]={'+','.join(f'0x{x:04x}' for x in values)+'};', '#endif']
lines += ["#endif", ""]
(root / "native/intro_probe/src/task_startup_data.h").write_text("\n".join(lines))
