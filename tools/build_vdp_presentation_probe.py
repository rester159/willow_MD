#!/usr/bin/env python3
"""Build a native static VDP-publication diagnostic, never a playable port.

Input files are full big-endian VRAM (65536 bytes), CRAM (128), VSRAM (80),
and VDP registers (24). The CPU publishes them itself with display disabled,
then enables the display. No runtime game behavior is supplied or replaced.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def build(inputs: dict[str, Path], out: Path, *, native_source: Path | None = None,
          native_include: Path | None = None, native_optimization: str = "-O2") -> dict:
    if native_optimization not in ("-O2", "-Os", "-O3"):
        raise ValueError("unsupported native diagnostic optimization")
    sizes = {"vram": 65536, "cram": 128, "vsram": 80, "registers": 24}
    blobs = {key: path.read_bytes() for key, path in inputs.items()}
    for key, size in sizes.items():
        if len(blobs[key]) != size:
            raise ValueError(f"{key} must contain exactly {size} bytes")
    regs = blobs["registers"]
    if regs[1] & 4 == 0 or regs[1] & 8 or regs[12] != 0x81:
        raise ValueError("fixture requires noninterlaced H40/224-line Mode5")
    if regs[0] & 0x10 or regs[1] & 0x20:
        raise ValueError("static fixture must disable HBlank/VBlank interrupts")
    if regs[15] != 2:
        raise ValueError("fixture expects normal two-byte VDP auto increment")
    if any(word & ~0x0EEE for word in struct.unpack(">64H", blobs["cram"])):
        raise ValueError("CRAM contains non-hardware bits")
    out = out.resolve()
    protected = {path.resolve() for path in inputs.values()}
    outputs = {out / name for name in ["probe.S", "probe.o", "probe.elf", "probe.bin", "probe.sym", "manifest.json"]}
    outputs.update(out / (name + ".bin") for name in sizes)
    if outputs & protected:
        raise ValueError("output aliases an input; use a separate output directory")
    out.mkdir(parents=True, exist_ok=True)
    for key, blob in blobs.items():
        (out / (key + ".bin")).write_bytes(blob)
    assembly = '''.section .text
.globl boot
.long 0x00fff000
.long boot
.rept 62
.long fault
.endr
.org 0x100
.ascii "SEGA GENESIS    "
.org 0x1f0
.ascii "U               "
.org 0x200
boot:
 move.w #0x2700,%sr
 move.b 0xa10001,%d0
 andi.b #15,%d0
 beq 1f
 move.l #0x53454741,0xa14000
1:
 move.w #0x100,0xa11100
 move.w #0x100,0xa11200
 lea 0xc00004,%a5
 lea 0xc00000,%a6
 move.w #0x8004,(%a5)
 move.w #0x8104,(%a5)
 move.w #0x8f02,(%a5)
 /* Set SAT base BEFORE filling VRAM so the VDP's sprite cache sees writes. */
 lea register_data,%a0
 move.w #0x8000,%d1
 moveq #23,%d0
5:
 move.w %d1,%d2
 move.b (%a0)+,%d2
 cmpi.w #0x8100,%d1
 bne 6f
 andi.w #0xffbf,%d2
6:
 move.w %d2,(%a5)
 addi.w #0x0100,%d1
 dbra %d0,5b
 move.l #0x40000000,(%a5)
 lea vram_data,%a0
 move.w #32767,%d0
2:
 move.w (%a0)+,(%a6)
 dbra %d0,2b
 move.l #0xc0000000,(%a5)
 lea cram_data,%a0
 moveq #63,%d0
3:
 move.w (%a0)+,(%a6)
 dbra %d0,3b
 move.l #0x40000010,(%a5)
 lea vsram_data,%a0
 moveq #39,%d0
4:
 move.w (%a0)+,(%a6)
 dbra %d0,4b
 moveq #0,%d0
 move.b register_data+1,%d0
 ori.w #0x8140,%d0
 move.w %d0,(%a5)
 move.l #0x56445031,0xff0000
ready:
 bra ready
fault:
 move.l #0x4641494c,0xff0000
 bra fault
.org 0x1000
vram_data:
 .incbin "vram.bin"
cram_data:
 .incbin "cram.bin"
vsram_data:
 .incbin "vsram.bin"
register_data:
 .incbin "registers.bin"
.balign 4
'''
    objects = ["probe.o"]
    commands = []
    if native_source is not None:
        # Optional renderer diagnostic runs with the display disabled and the
        # independent supervisor stack. It never runs on a source task stack.
        marker = " moveq #0,%d0\n move.b register_data+1,%d0"
        assert assembly.count(marker) == 1
        assembly = assembly.replace(marker, " jsr nativeObjectPresentation\n tst.w %d0\n beq fault\n" + marker)
        commands.append(["m68k-elf-gcc", "-m68000", "-std=c99", native_optimization, "-ffreestanding",
                         "-fno-builtin", "-fstack-usage", "-Wall", "-Wextra", "-Werror",
                         "-I", str(ROOT / "src"), "-I", str((native_include or out).resolve()),
                         "-c", str(native_source.resolve()), "-o", "native.o"])
        objects.append("native.o")
    (out / "probe.S").write_text(assembly)
    commands += [["m68k-elf-as", "-m68000", "probe.S", "-o", "probe.o"],
                 ["m68k-elf-ld", "-Ttext=0", "-Tbss=0xff8000", "-e", "boot", *objects, "-o", "probe.elf"],
                 ["m68k-elf-objcopy", "-O", "binary", "probe.elf", "probe.bin"]]
    for command in commands:
        subprocess.run(command, cwd=out, check=True, capture_output=True)
    with (out / "probe.sym").open("w") as symbols:
        subprocess.run(["m68k-elf-nm", "-n", "probe.elf"], cwd=out, stdout=symbols, check=True)
    rom = bytearray((out / "probe.bin").read_bytes())
    # Larger closed trace diagnostics may carry exact per-capture assertions.
    # Keep the existing128KiB minimum, with power-of-two padding under the
    # Genesis flat4MiB ROM limit. This does not change the shipping cartridge.
    rom_size=max(0x20000,1<<(len(rom)-1).bit_length())
    if rom_size>0x400000:
        raise ValueError('static diagnostic exceeds flat4MiB ROM capacity')
    rom.extend(bytes(rom_size-len(rom)))
    struct.pack_into(">II", rom, 0x1A0, 0, len(rom) - 1)
    struct.pack_into(">II", rom, 0x1A8, 0xFF0000, 0xFFFFFF)
    struct.pack_into(">H", rom, 0x18E, sum(struct.unpack(">" + "H" * ((len(rom)-0x200)//2), rom[0x200:])) & 65535)
    (out / "probe.bin").write_bytes(rom)
    manifest = {"scope": "static native VDP publication diagnostic; no game logic or FPS admission",
                "commands": commands, "ready_address": 0xFF0000, "ready_value": 0x56445031,
                "inputs": {key: {"path": str(inputs[key].resolve()), "bytes": len(blob), "sha256": hashlib.sha256(blob).hexdigest()} for key, blob in blobs.items()},
                "rom_sha256": hashlib.sha256(rom).hexdigest(),
                "builder_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    if native_source is not None:
        manifest["native_renderer"] = {"source": str(native_source.resolve()),
                                       "sha256": hashlib.sha256(native_source.read_bytes()).hexdigest(),
                                       "stack_usage": (out / "native.su").read_text(),
                                       "diagnostic_bss_base": "FF8000", "supervisor_stack_top": "FFF000"}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ["vram", "cram", "vsram", "registers"]:
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    result = build({key: getattr(args, key) for key in ["vram", "cram", "vsram", "registers"]}, args.output)
    print(f"Native static presentation diagnostic: {args.output / 'probe.bin'} ({result['rom_sha256']})")
