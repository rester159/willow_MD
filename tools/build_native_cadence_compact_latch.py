#!/usr/bin/env python3
"""Compose native bounded capture onto immutable metadata-v1 evidence."""
import hashlib
import json
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "build/native-object-publication/checkpoint-metadata-v1"
OUT = ROOT / "build/native-object-publication/compact-v1"
OUT.mkdir(parents=True, exist_ok=True)
def sha(data): return hashlib.sha256(data).hexdigest()
pinned = json.loads((BASE / "checkpoint.json").read_text())["files"]
for name in ("manifest.json", "platform.bin", "platform-original.bin", "program.bin", "program-original.bin", "platform-addresses.h"):
    assert sha((BASE / name).read_bytes()) == pinned[name]
metadata = json.loads((BASE / "manifest.json").read_text())
symbols = None;products = {};patches = []
for original in (True, False):
    suffix = "-original" if original else ""
    name = "compact" + suffix
    elf = OUT / (name + ".elf")
    subprocess.run(["m68k-elf-gcc", "-m68000", "-O2", "-ffreestanding", "-fno-builtin", "-nostdlib", "-Wall", "-Wextra", "-Werror",
                    *(["-DCADENCE_ORIGINAL_BUS=1"] if original else []),
                    "-Wl,-T,tools/native_cadence_compact_latch.ld", "tools/native_cadence_compact_latch.c", "-o", str(elf)], cwd=ROOT, check=True)
    binary = OUT / (name + ".bin")
    subprocess.run(["m68k-elf-objcopy", "-O", "binary", str(elf), str(binary)], check=True)
    sym = subprocess.check_output(["m68k-elf-nm", "-n", str(elf)], text=True)
    (OUT / (name + ".sym")).write_text(sym)
    (OUT / (name + ".disasm")).write_text(subprocess.check_output(["m68k-elf-objdump", "-d", str(elf)], text=True))
    current = {r.split()[2]: int(r.split()[0],16) for r in sym.splitlines() if len(r.split()) == 3}
    if symbols is None: symbols = current
    else: assert symbols == current
    helper = current["nativeCadenceCompactLatch"]
    platform = bytearray((BASE / ("platform" + suffix + ".bin")).read_bytes())
    start = metadata["symbols"]["metadataAcquireAndLatch"] - 0x100010
    hits = [n for n in range(start, start + 12, 2) if platform[n:n+6] == bytes.fromhex("4eb900100218")]
    assert len(hits) == 1
    at = hits[0];platform[at:at+6] = bytes.fromhex("4eb9") + helper.to_bytes(4,"big")
    position = helper - 0x100010
    assert len(platform) < position
    platform.extend(bytes(position-len(platform)));platform.extend(binary.read_bytes())
    product = "platform" + suffix + ".bin";(OUT/product).write_bytes(platform);products[product]=sha(platform)
    program = "program" + suffix + ".bin";data=(BASE/program).read_bytes();(OUT/program).write_bytes(data);products[program]=sha(data)
    if original: patches.append({"address":at+0x100010,"expected":"4eb900100218","target":helper})
header=(BASE/"platform-addresses.h").read_text()
header=header.replace("#define CADENCE_LATCH 0x100218u",f'#define CADENCE_LATCH {symbols["nativeCadenceCompactLatch"]:#x}u')
header=header.replace("#define CADENCE_LATCH_END 0x10029au",f'#define CADENCE_LATCH_END {symbols["willowNativeObjectLatchOffsets"]:#x}u')
(OUT/"platform-addresses.h").write_text(header)
manifest={"schema_version":1,"scope":"Opt-in native compact-prefix capture with full fallback; no renderer/CPS-speed admission",
          "base_checkpoint_sha256":sha((BASE/"checkpoint.json").read_bytes()),"products":products,"native_call_patch":patches,
          "symbols":symbols,"diagnostic_capture_fields":{"count":"20F530","compact_valid":"20F532","epoch":"20F534"},
          "source_files":{n:sha((ROOT/n).read_bytes()) for n in ("src/willow_native_object_latch.h","src/willow_native_object_latch_compact.h","tools/native_cadence_compact_latch.c","tools/native_cadence_compact_latch.ld","tools/build_native_cadence_compact_latch.py")}}
(OUT/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
print(json.dumps({"native_helper_bytes":len((OUT/"compact.bin").read_bytes()),"products":products,"symbols":symbols},indent=2))
