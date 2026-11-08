#!/usr/bin/env python3
"""Build independent continuation fixtures and compare 900 headless task scans."""
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parent.parent
    out = root / "build/demo-auxiliary-investigation"
    capture = out / "capture"
    (capture / "build").mkdir(parents=True, exist_ok=True)
    core = Path.home() / "Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
    reference = root / "build/reference_maincpu.bin"
    link = capture / "build/reference_maincpu.bin"
    if not link.exists():
        link.symlink_to(reference)
    if link.resolve() != reference.resolve():
        raise RuntimeError("Capture directory refers to a different source ROM")
    # Explicit inputs prevent unrelated WILLOW_* environment settings changing
    # the checkpoint. All lifecycle output stays in the isolated directory.
    env = {k: v for k, v in os.environ.items() if not k.startswith("WILLOW_")}
    for key, name in [
        ("WILLOW_TITLE_FADE_INPUT", "title-fade-input.bin"),
        ("WILLOW_TITLE_PALETTE_INPUT", "title-palette-input.bin"),
        ("WILLOW_TITLE_COMMAND_INPUT", "title-command-input.bin"),
    ]:
        env[key] = str(root / "build" / name)
    env["WILLOW_DEMO_ZERO_POINTER"] = "1"
    env["WILLOW_DEMO_CONTINUATION_OUTPUT"] = str(out / "checkpoint")
    flags = ["-Wall", "-Wextra", "-Werror", "-O2", "-fsanitize=address,undefined",
             "-fno-sanitize-recover=all"]

    def run(args, name, cwd=root):
        with (out / name).open("w") as log:
            result = subprocess.run(args, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print((out / name).read_text())
            result.check_returncode()

    run(["clang", *flags, "tools/test_arcade_opening_lifecycle.c", "-o", str(out / "lifecycle")], "build-lifecycle.log")
    run([str(out / "lifecycle"), str(core), "--demo-start-0"], "capture.log", capture)
    if (out / "checkpoint-native-ram.bin").read_bytes() != (root / "build/opening-demo-start-0-state.bin").read_bytes():
        raise RuntimeError("Regenerated lifecycle does not match the current demo fixture")
    scroll1 = (root / "build/opening-title-context-scroll1.bin").read_bytes()[-16384:]
    if (out / "checkpoint-scroll1.bin").read_bytes() != scroll1:
        raise RuntimeError("Regenerated retained text does not match the current demo fixture")
    env["WILLOW_DEMO_CONTINUATION_INPUT"] = str(out / "checkpoint")
    run(["clang", *flags, "-DWILLOW_TEST_DEMO_FIRST_FRAME", "-DWILLOW_TEST_DEMO_AUXILIARY_TASKS",
         "-DWILLOW_TEST_DEMO_UPDATE_SEQUENCE=900",
         '-DDEMO_OUTPUT="build/demo-auxiliary-investigation/sequence"',
         "tools/test_arcade_demo_first_actors.c", "-o", str(out / "sequence-test")], "build-sequence.log")
    run([str(out / "sequence-test"), str(core)], "sequence.log")
    print((out / "sequence.log").read_text(), end="")
    paths = [reference, core, root / "tools/test_arcade_demo_first_actors.c",
             root / "tools/test_arcade_opening_lifecycle.c", Path(__file__).resolve()]
    paths += list(out.glob("checkpoint-*.bin")) + list(out.glob("sequence-trace-*.bin"))
    paths += [root / "build" / n for n in ["title-fade-input.bin", "title-palette-input.bin", "title-command-input.bin"]]
    manifest = {
        "scope": "Host translation versus original 68000 instructions; 900 scans/wakes; no cartridge or interrupt/FPS claim",
        "manager_context": "Explicit 1756 restore context returning to 810E; auxiliary contexts captured from verified lifecycle",
        "sha256": {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
    }
    (out / "evidence.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
