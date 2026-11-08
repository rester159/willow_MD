#!/usr/bin/env python3
"""Export the position-independent 2244..2507 native 68000 visibility leaf."""
import argparse
import hashlib
from pathlib import Path

SOURCE_SHA256 = "0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1"
SPAN_SHA256 = "2faeb040de7a549e7da83efba93aed14133de6e20ec0cf65aafa2043748ea3b6"
START, END = 0x2244, 0x2508

def generate(source: Path, output: Path):
    rom = source.read_bytes()
    if len(rom) != 0x100000 or hashlib.sha256(rom).hexdigest() != SOURCE_SHA256:
        raise SystemExit("Unsupported original program: size/SHA256 mismatch")
    block = rom[START:END]
    if hashlib.sha256(block).hexdigest() != SPAN_SHA256:
        raise SystemExit("Native visibility source span SHA256 mismatch")
    # Every branch, indexed jump and table address in this closed block is
    # PC-relative. No calls, traps, yields, absolute ROM/video or hardware I/O.
    # A5 is the original FF8000 RAM base; A6 is the source-layout actor.
    # Queue movea.w sign-extends 16-bit handles to FFxxxx on the 24-bit bus.
    assembly = [
        "/* Generated from verified original ROM; execute directly on 68000. */",
        f"/* Source SHA256 {SOURCE_SHA256}; span SHA256 {SPAN_SHA256}. */",
        ".text", ".balign 2", ".globl willowNativeVisibilityInvoke",
        ".type willowNativeVisibilityInvoke,@function",
        "willowNativeVisibilityInvoke:",
        "  movem.l %a5-%a6,-(%sp)",
        "  movea.l 12(%sp),%a6",  # pointer argument after saved A5/A6
        "  movea.l #0xff8000,%a5",
        "  move.l 16(%sp),%d0",   # explicit uint32_t mode argument
        "  add.w %d0,%d0",
        "  move.w .LvisibilityModes(%pc,%d0.w),%d0",
        "  lea willowNativeVisibilityCode(%pc),%a0",
        "  adda.w %d0,%a0",
        "  jsr (%a0)",
        "  movem.l (%sp)+,%a5-%a6",
        "  rts",
        ".size willowNativeVisibilityInvoke,.-willowNativeVisibilityInvoke",
        ".LvisibilityModes: .word 0,0x52,0x98",
        ".balign 2", ".globl willowNativeVisibilityCode",
        ".type willowNativeVisibilityCode,@function", "willowNativeVisibilityCode:",
    ]
    for at in range(0, len(block), 16):
        chunk = block[at:at+16]
        words = [f"0x{int.from_bytes(chunk[n:n+2], 'big'):04x}" for n in range(0,len(chunk),2)]
        assembly.append("  .word " + ",".join(words) + f" /* {START+at:06x} */")
    assembly.extend([".size willowNativeVisibilityCode,.-willowNativeVisibilityCode", ""])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(assembly))
    print(f"Native visibility: {len(block)} unchanged bytes, entries +0/+52/+98 -> {output}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("build/reference_maincpu.bin"))
    parser.add_argument("--output", type=Path, default=Path("build/native_visibility.S"))
    args = parser.parse_args()
    generate(args.source, args.output)
