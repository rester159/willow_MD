#!/usr/bin/env python3
"""Publish a source-verified static frame through the native Genesis VDP.

This checks combined scene pixels, not gameplay, streaming, DMA frame budgets,
all scenes, or host VSync. Run using the existing Pillow-enabled project Python.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from PIL import Image
from build_vdp_presentation_probe import ROOT, build


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(fixture: Path, out: Path, core: Path) -> dict:
    fixture, out, core = fixture.resolve(), out.resolve(), core.resolve(strict=True)
    if fixture == out or out.is_relative_to(fixture):
        raise ValueError("keep native carrier outputs outside the frozen source fixture")
    manifest_path = fixture / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    if manifest["source_composition_pixels_verified"] != 320*224 or manifest["source_composition_mismatches"]:
        raise ValueError("fixture does not establish complete captured-source composition")
    for name, expected in manifest["buffers"].items():
        path = fixture / name
        if path.stat().st_size != expected["bytes"] or sha(path) != expected["sha256"]:
            raise ValueError(f"fixture buffer hash/size mismatch: {name}")
    expected_path = fixture / manifest["expected_image"]
    expected_image = Image.open(expected_path).convert("RGB")
    if expected_image.size != (320,224):
        raise ValueError("fixture image must be exactly320x224")
    build({key: fixture / (key + ".bin") for key in ["vram", "cram", "vsram", "registers"]}, out)
    commands = [["clang", "-Wall", "-Wextra", "-Werror", "-O2", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", str(ROOT / "tools/capture_vdp_presentation.c"), "-o", str(out / "capture")],
                [str(out / "capture"), str(core), str(out / "probe.bin"), str(out / "hardware.ppm")]]
    subprocess.run(commands[0], cwd=ROOT, check=True)
    with (out / "capture.log").open("w") as log:
        subprocess.run(commands[1], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    actual_image = Image.open(out / "hardware.ppm").convert("RGB")
    if actual_image.size != expected_image.size:
        raise AssertionError("native image dimensions changed")
    # RGB565 libretro output is quantized independently of the source fitting.
    expected = [((r>>3)*255//31, (g>>2)*255//63, (b>>3)*255//31)
                for r,g,b in expected_image.get_flattened_data()]
    actual = list(actual_image.get_flattened_data())
    bad = [i for i,(a,b) in enumerate(zip(actual,expected)) if a != b]
    result = {"frame": manifest["frame"], "pixels": len(actual), "mismatched_rgb565_pixels": len(bad),
              "first_mismatches": [{"x": i%320, "y": i//320, "actual": actual[i], "expected": expected[i]} for i in bad[:10]],
              "scope": "native static VDP publication with combined source-verified background/object/Scroll1 scene; not live full-game or FPS admission",
              "commands": commands,
              "files": {str(path): sha(path) for path in [manifest_path, expected_path, core, ROOT / "tools/build_vdp_presentation_probe.py", ROOT / "tools/capture_vdp_presentation.c", Path(__file__).resolve(), out / "probe.bin", out / "hardware.ppm", out / "capture.log"]}}
    actual_image.save(out / "hardware.png")
    (out / "pixel-comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    if bad:
        raise AssertionError(f"{len(bad)} native pixels differ; see {out / 'pixel-comparison.json'}")
    return result


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--fixture", type=Path, default=ROOT / "build/cartridge-gap-audit/frame1020_reuse_p3_w16_v1")
    p.add_argument("--output", type=Path, default=ROOT / "build/vdp-presentation-test/frame1020")
    p.add_argument("--core", type=Path, default=Path.home() / "Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
    a = p.parse_args()
    result = verify(a.fixture, a.output, a.core)
    print(f"Frame{result['frame']}: all{result['pixels']} native Genesis VDP pixels match the combined source-derived palette/geometry fixture; no GUI or host target writes.")
