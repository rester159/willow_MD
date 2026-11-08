#!/usr/bin/env python3
"""Continue the original/native-host scheduler through both ending messages."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

from test_arcade_demo_auxiliary import main as capture_demo


def main():
    capture_demo()
    root = Path(__file__).resolve().parent.parent
    out = root / "build/demo-post-sequence-investigation"
    out.mkdir(parents=True, exist_ok=True)
    core = Path.home() / "Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
    args = ["clang", "-Wall", "-Wextra", "-Werror", "-O2",
            "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
            "-DWILLOW_TEST_DEMO_FIRST_FRAME", "-DWILLOW_TEST_DEMO_POST_SEQUENCE",
            "-DWILLOW_TEST_DEMO_UPDATE_SEQUENCE=1200",
            '-DDEMO_OUTPUT="build/demo-post-sequence-investigation/sequence"',
            "tools/test_arcade_demo_first_actors.c", "-o", str(out / "reference-test")]
    with (out / "build.log").open("w") as log:
        subprocess.run(args, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("WILLOW_")}
    env["WILLOW_DEMO_CONTINUATION_INPUT"] = str(root / "build/demo-auxiliary-investigation/checkpoint")
    with (out / "reference.log").open("w") as log:
        subprocess.run([str(out / "reference-test"), str(core)], cwd=root, env=env,
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    sizes = {"ram": 65536, "scroll2": 16384, "scroll3": 16384, "sprites": 8192,
             "registers": 6, "access": 65536, "live-registers": 4, "scheduler": 34}
    count = (out / "sequence-trace-ram.bin").stat().st_size // 65536
    if not 900 < count <= 1200:
        raise RuntimeError("Invalid post-demo trace length")
    hashes = {}
    for kind, size in sizes.items():
        trace = out / f"sequence-trace-{kind}.bin"
        data = trace.read_bytes()
        prior = (root / f"build/demo-auxiliary-investigation/sequence-trace-{kind}.bin").read_bytes()
        if len(data) != count * size or data[:900 * size] != prior:
            raise RuntimeError(f"Gameplay prefix or trace length changed: {kind}")
        hashes[str(trace)] = hashlib.sha256(data).hexdigest()
    final = (out / "sequence-output-ram.bin").read_bytes()
    if final[0x8004:0x8006] != bytes([0, 8]) or any(final[0x8008:0x800c]) or final[0xe58]:
        raise RuntimeError("Post-demo did not finish at phase 8/0, timer zero and message idle")
    for path in [root / "tools/test_arcade_demo_first_actors.c", Path(__file__).resolve(),
                 root / "build/reference_maincpu.bin", core]:
        hashes[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    (out / "evidence.json").write_text(json.dumps({
        "scope": "Original CPU versus native host; independent task continuation through post-demo",
        "scans": count, "post_gameplay_scans": count - 900,
        "first_900_unchanged": True, "phase": 8, "substate": 0,
        "interrupt_timing_verified": False, "sha256": hashes,
    }, indent=2) + "\n")
    print((out / "reference.log").read_text(), end="")


if __name__ == "__main__":
    main()
