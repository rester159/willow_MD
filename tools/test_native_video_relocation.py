#!/usr/bin/env python3
"""Integrity gates for the candidate video relocation (no GUI/emulator)."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from native_video_relocation import REGIONS, ROOT, SOURCE_SHA256, relocate


class VideoRelocationIntegrity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "build/reference_maincpu.bin").read_bytes()
        cls.target, cls.manifest = relocate(cls.source)

    def test_original_bytes_outside_explicit_operands_are_unchanged(self):
        permitted = set()
        for patch in self.manifest["patches"]:
            permitted.update(range(patch["offset"], patch["offset"] + patch["size"]))
        changed = {index for index, (old, new) in enumerate(zip(self.source, self.target)) if old != new}
        self.assertTrue(changed)
        self.assertLessEqual(changed, permitted)
        # These are source instructions/data which merely LOOK like pointers
        # when scanned as longwords. A blind range rewrite corrupts them.
        for offset in (0x6EB2, 0x1ABA2, 0x1E4C6, 0x1E844, 0x347E4, 0x38586,
                       0x404F0, 0xC0400, 0xC0420, 0xE51B6, 0xFC5DC):
            self.assertEqual(self.target[offset:offset + 4], self.source[offset:offset + 4])
        self.assertEqual(hashlib.sha256(self.source).hexdigest(), SOURCE_SHA256)

    def test_all_planes_banks_and_palette_pages_exist_together(self):
        owned = set()
        for region in REGIONS:
            span = set(range(region.target, region.target + region.size))
            self.assertTrue(owned.isdisjoint(span))
            owned.update(span)
            self.assertGreaterEqual(region.target, 0x200000)
            self.assertLessEqual(region.target + region.size, 0x210000)
        self.assertEqual(len(owned), 58 * 1024)
        latch = set(range(0x20C800, 0x20D000))
        self.assertTrue(owned.isdisjoint(latch))
        self.assertEqual(len(owned | latch), 60 * 1024)

    def test_full_object_cursor_keeps_original_bank_selection(self):
        # Include the one-past-end cursor used by source 3B574. Check all byte
        # positions too, even though valid object cursors advance eight bytes.
        for source, target in ((0x900000, 0x20C000), (0x908000, 0x20D000)):
            for cursor in range(0x801):
                self.assertEqual((source + cursor) & 0x8000 != 0,
                                 (target + cursor) & 0x1000 != 0)
        # Regression demonstrating why the tempting adjacent-bank map fails.
        self.assertNotEqual((0x900000 + 0x800) & 0x8000 != 0,
                            (0x20C000 + 0x800) & 0x0800 != 0)
        self.assertEqual(self.target[0x3B57A:0x3B580], bytes.fromhex("20080800000c"))

    def test_modified_source_rejected_before_output_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source, output, manifest = [directory / name for name in ("source.bin", "output.bin", "manifest.json")]
            output.write_bytes(b"preserve existing output")
            manifest.write_bytes(b"preserve existing manifest")
            bad = bytearray(self.source)
            bad[0x3B57E] ^= 1
            for payload in (bad, self.source[:-1]):
                source.write_bytes(payload)
                result = subprocess.run(
                    [sys.executable, str(ROOT / "tools/native_video_relocation.py"),
                     "--source", str(source), "--output", str(output), "--manifest", str(manifest)],
                    capture_output=True, text=True, check=False)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("size/SHA256 mismatch", result.stderr)
                self.assertEqual(output.read_bytes(), b"preserve existing output")
                self.assertEqual(manifest.read_bytes(), b"preserve existing manifest")


if __name__ == "__main__":
    unittest.main()
