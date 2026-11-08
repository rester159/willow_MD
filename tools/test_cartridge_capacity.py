#!/usr/bin/env python3
"""Exercise exact cartridge limits, padding and refusal without altering input."""
import tempfile
import unittest
from pathlib import Path
from finalize_genesis_rom import finalize, DELIVERY_MAX_BYTES, HEADER_SIZE
from native_rom_audit import audit


class CartridgeCapacityTests(unittest.TestCase):
    def test_corrected_delivery_limit(self):
        self.assertEqual(DELIVERY_MAX_BYTES, 5242880)
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'test.bin'
            archive = Path(__file__).resolve().parents[1] / 'willow.zip'
            for header in (b'SEGA GENESIS    ', b'SEGA SSF        '):
                image = bytearray(4 * 1024 * 1024)
                image[0x100:0x110] = header
                path.write_bytes(image)
                finalize(path)
                self.assertEqual(audit(archive, path), [])
            image = bytearray(DELIVERY_MAX_BYTES)
            image[0x100:0x110] = b'SEGA SSF        '
            path.write_bytes(image)
            finalize(path)
            self.assertEqual(path.stat().st_size, 5242880)
            self.assertEqual(audit(archive, path), [])
            image.extend(bytes(DELIVERY_MAX_BYTES - len(image) + 131072))
            path.write_bytes(image)
            self.assertTrue(any('delivery limit' in e for e in audit(archive, path)))

    def test_boundaries_and_preservation(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'test.bin'
            for size in (DELIVERY_MAX_BYTES - 1, DELIVERY_MAX_BYTES):
                image = bytearray(size)
                image[0x100:0x110] = b'SEGA SSF        '
                path.write_bytes(image)
                end, checksum = finalize(path)
                self.assertEqual(path.stat().st_size, DELIVERY_MAX_BYTES)
                self.assertEqual(end, 0x3fffff)
                self.assertEqual(checksum, 0)
            for size in (HEADER_SIZE, DELIVERY_MAX_BYTES + 1, 8 * 1024 * 1024):
                image = bytearray(size)
                image[0x100:0x110] = b'SEGA SSF        '
                path.write_bytes(image)
                with self.assertRaises(ValueError):
                    finalize(path)
                self.assertEqual(path.read_bytes(), image)
            image = bytearray(524288)
            image[0x100:0x110] = b'SEGA GENESIS    '
            path.write_bytes(image)
            end, _ = finalize(path)
            self.assertEqual(end, len(image) - 1)
            image = bytearray(DELIVERY_MAX_BYTES)
            image[0x100:0x110] = b'SEGA GENESIS    '
            path.write_bytes(image)
            with self.assertRaisesRegex(ValueError, 'requires the banked'):
                finalize(path)
            image.extend(bytes(131072))
            path.write_bytes(image)
            with self.assertRaisesRegex(ValueError, 'requires the banked'):
                finalize(path, len(image))  # Explicit research override only.
            self.assertEqual(path.read_bytes(), image)

    def test_padding_counts_toward_budget(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'test.bin'
            image = bytearray(4 * 1024 * 1024 + 1)
            image[0x100:0x110] = b'SEGA SSF        '
            path.write_bytes(image)
            with self.assertRaises(ValueError):
                finalize(path, len(image))
            self.assertEqual(path.read_bytes(), image)


if __name__ == '__main__':
    unittest.main()
