#!/usr/bin/env python3
"""Compose opt-in source publication metadata hooks onto frozen cadence-v2."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "build/native-cadence/checkpoint-latch-v2"
OUT = ROOT / "build/native-object-publication/metadata-v1"
OUT.mkdir(parents=True, exist_ok=True)
def sha(data): return hashlib.sha256(data).hexdigest()
pinned = json.loads((BASE / "evidence.json").read_text())
source = (ROOT / "build/reference_maincpu.bin").read_bytes()
assert sha(source) == pinned["source_rom_sha256"]
for n in ("program.bin", "platform.bin", "platform-original.bin"):
    assert sha((BASE / n).read_bytes()) == pinned["files"][n]
common_symbols = None
patches = []
products = {}
for original in (True, False):
    suffix = "-original" if original else ""
    name = "metadata" + suffix
    elf = OUT / (name + ".elf")
    subprocess.run(["m68k-elf-gcc", "-m68000", "-ffreestanding", "-nostdlib",
                    *(["-DMETADATA_ORIGINAL_BUS=1"] if original else []),
                    "-Wl,-T,tools/native_object_publication_metadata.ld",
                    "tools/native_object_publication_metadata.S", "-o", str(elf)], cwd=ROOT, check=True)
    binary = OUT / (name + ".bin")
    subprocess.run(["m68k-elf-objcopy", "-O", "binary", str(elf), str(binary)], check=True)
    sym = subprocess.check_output(["m68k-elf-nm", "-n", str(elf)], text=True)
    (OUT / (name + ".sym")).write_text(sym)
    (OUT / (name + ".disasm")).write_text(subprocess.check_output(["m68k-elf-objdump", "-d", str(elf)], text=True))
    symbols = {r.split()[2]: int(r.split()[0], 16) for r in sym.splitlines() if len(r.split()) == 3}
    if common_symbols is None: common_symbols = symbols
    else: assert symbols == common_symbols
    program = bytearray(source if original else (BASE / "program.bin").read_bytes())
    sites = [
        (0x3b550, bytes.fromhex("3e3c01003b47029a"), "metadataBegin"),
        (0x3b56c, bytes.fromhex("086d00070023"), "metadataDouble"),
        (0x3b564, bytes.fromhex("3b7c90000022"), "metadataSingle"),
        (0x590e, bytes.fromhex("303c00ff7200"), "metadataClear"),
        (0x6632, bytes.fromhex("303c00ff7200"), "metadataClear"),
    ]
    for at, expected, target in sites:
        assert program[at:at + len(expected)] == expected
        replacement = bytes.fromhex("4eb9") + symbols[target].to_bytes(4, "big")
        if len(expected) == 8: replacement += bytes.fromhex("4e71")
        program[at:at + len(expected)] = replacement
        if original:
            patches.append({"address": at, "expected": expected.hex(), "replacement": replacement.hex(), "target": target})
    platform = bytearray((BASE / ("platform" + suffix + ".bin")).read_bytes())
    call_offset = 0x10006e - 0x100010
    assert platform[call_offset:call_offset + 6] == bytes.fromhex("4eb900100218")
    platform[call_offset:call_offset + 6] = bytes.fromhex("4eb9") + symbols["metadataAcquireAndLatch"].to_bytes(4, "big")
    start = symbols["metadataBegin"] - 0x100010
    assert len(platform) < start and start + len(binary.read_bytes()) < 65520
    platform.extend(bytes(start - len(platform)))
    platform.extend(binary.read_bytes())
    for product, data in (("program" + suffix + ".bin", program), ("platform" + suffix + ".bin", platform)):
        (OUT / product).write_bytes(data)
        products[product] = sha(data)
header = (BASE / "platform-addresses.h").read_text()
for name, address in common_symbols.items():
    if name.startswith("metadata"): header += f"#define {name.upper()} 0x{address:x}u\n"
header += "#define METADATA_SRAM_OFFSET 0xf500u\n"
(OUT / "platform-addresses.h").write_text(header)
manifest = {
    "schema_version": 1,
    "scope": "Opt-in native publication metadata only, full raw latch retained. Experimental diagnostic SRAM20F500..20F52F. No rendering/FPS/production layout admission.",
    "source_rom_sha256": sha(source), "base_evidence_sha256": sha((BASE / "evidence.json").read_bytes()),
    "source_patches": patches, "irq_call_patch": {"address": 0x10006e, "original": "4eb900100218", "target": common_symbols["metadataAcquireAndLatch"]},
    "products": products, "symbols": common_symbols,
    "source_files": {n: sha((ROOT / n).read_bytes()) for n in
                     ("tools/native_object_publication_metadata.S", "tools/native_object_publication_metadata.ld", "tools/build_native_object_publication_metadata.py")},
    "preservation": "Displaced original instructions retain register/CCR effects. Native source hooks save working registers and restore CCR (not privileged SR). No IRQ-mask or wait changes. Ready flag is published last; late ready may conservatively reject acquisition."}
(OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps({"bytes": len((OUT / "metadata.bin").read_bytes()), "products": products, "symbols": common_symbols}, indent=2))
