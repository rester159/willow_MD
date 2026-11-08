#!/usr/bin/env python3
"""Extract Willow's MSM6295/OKI6295 table-driven ADPCM samples to WAV."""

from __future__ import annotations

import argparse
import json
import math
import struct
import wave
import zipfile
from pathlib import Path


STEP_INDEX = (-1, -1, -1, -1, 2, 4, 6, 8)
# MAME computes floor(16 * (11 / 10) ** index) for indexes 0..48.
# Keep the identical step values deterministic without binary-float rounding.
STEP_VALUES = tuple((16 * 11**index) // 10**index for index in range(49))
# SGDK's PCM4 mixer combines four uncompressed 8-bit streams at 16 kHz. This
# rate is above Willow's source OKI rate and preserves both source voices.
PCM4_VOICE_RATE = 16000
PCM4_SAMPLE_IDS = tuple(
    sample_id for sample_id in range(1, 0x2A) if sample_id != 0x24)
PCM4_SAMPLE_ALIGNMENT = 256
PCM4_PACK_SIZE = 0x100000


def decode_nibble(nibble: int, signal: int, step_index: int) -> tuple[int, int]:
    """MSM6295 4-bit OKI ADPCM, matching MAME's oki_adpcm_state tables."""
    step_value = STEP_VALUES[step_index]
    magnitude = step_value // 8
    if nibble & 1:
        magnitude += step_value // 4
    if nibble & 2:
        magnitude += step_value // 2
    if nibble & 4:
        magnitude += step_value
    signal += -magnitude if nibble & 8 else magnitude
    signal = max(-2048, min(2047, signal))
    step_index = max(0, min(48, step_index + STEP_INDEX[nibble & 7]))
    return signal, step_index


def decode_sample(rom: bytes, start: int, stop: int) -> list[int]:
    if start < 0 or stop < start or stop >= len(rom):
        raise ValueError(f"invalid sample range {start:#x}..{stop:#x}")
    signal = 0
    step_index = 0
    pcm: list[int] = []
    for byte in rom[start : stop + 1]:
        for nibble in (byte >> 4, byte & 0x0F):
            signal, step_index = decode_nibble(nibble, signal, step_index)
            # Preserve the chip's signed 12-bit signal in a 16-bit PCM WAV.
            pcm.append(signal << 4)
    return pcm


def _sinc(value: float) -> float:
    if value == 0.0:
        return 1.0
    angle = math.pi * value
    return math.sin(angle) / angle


def resample_polyphase(pcm: list[int], input_rate: int,
                       output_rate: int,
                       input_rate_divisor: int = 1) -> list[int]:
    """Resample from input_rate / input_rate_divisor Hz with a Blackman sinc."""
    if input_rate <= 0 or output_rate <= 0 or input_rate_divisor <= 0:
        raise ValueError("sample rates and divisors must be positive")
    source_denominator = output_rate * input_rate_divisor
    if input_rate == source_denominator or not pcm:
        return pcm

    ratio = source_denominator / input_rate
    cutoff = min(1.0, ratio)
    radius = math.ceil(24.0 / cutoff)
    output_count = round(len(pcm) * ratio)
    phase_count = source_denominator // math.gcd(input_rate,
                                                  source_denominator)
    offsets = range(-radius + 1, radius + 1)
    phase_weights: list[list[float]] = []
    for phase in range(phase_count):
        fraction = phase / phase_count
        weights = []
        for offset in offsets:
            distance = fraction - offset
            window_position = distance / radius
            if abs(window_position) >= 1.0:
                weights.append(0.0)
                continue
            window = (0.42 + 0.5 * math.cos(math.pi * window_position)
                      + 0.08 * math.cos(2.0 * math.pi * window_position))
            weights.append(cutoff * _sinc(cutoff * distance) * window)
        normalization = sum(weights)
        phase_weights.append([weight / normalization for weight in weights])

    result: list[int] = []

    for output_index in range(output_count):
        source_numerator = output_index * input_rate
        center, phase = divmod(source_numerator, source_denominator)
        phase = (phase * phase_count) // source_denominator
        value_sum = 0.0
        weights = phase_weights[phase]
        for tap, weight in enumerate(weights):
            source_index = center + tap - radius + 1
            source_index = min(len(pcm) - 1, max(0, source_index))
            value_sum += pcm[source_index] * weight
        value = round(value_sum)
        result.append(max(-32768, min(32767, value)))

    return result


def sample_bounds(table: bytes, sample_id: int) -> tuple[int, int]:
    entry = table[sample_id * 8 : sample_id * 8 + 8]
    if len(entry) != 8:
        raise ValueError(f"sample table is missing entry {sample_id}")
    start = ((entry[0] << 16) | (entry[1] << 8) | entry[2]) & 0x3FFFF
    stop = ((entry[3] << 16) | (entry[4] << 8) | entry[5]) & 0x3FFFF
    return start, stop


def write_wav(path: Path, pcm: list[int], sample_rate: int) -> None:
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(struct.pack(f"<{len(pcm)}h", *pcm))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/oki_wav"))
    parser.add_argument("--sample-clock-hz", type=int, default=1_000_000,
                        help="OKI6295 clock; default is the CPS-1 1 MHz clock")
    parser.add_argument("--pin7", choices=("high", "low"), default="high",
                        help="OKI6295 pin 7 divider selection")
    parser.add_argument("--output-rate", type=int, default=14000,
                        help="resampled PCM rate for the Genesis driver")
    args = parser.parse_args()
    if args.sample_clock_hz < 1 or args.output_rate < 1:
        parser.error("sample clock and output rate must be positive")
    input_divisor = 132 if args.pin7 == "high" else 165
    try:
        with zipfile.ZipFile(args.archive) as zf:
            # The MAME Willow definition places these adjacent in the 0x40000
            # OKI region; chip 18 contains both the lookup table and first bank.
            rom = zf.read("wl_18.11c") + zf.read("wl_19.12c")
            sound_cpu = zf.read("wl_09.12b")
    except (OSError, zipfile.BadZipFile, KeyError) as exc:
        parser.error(str(exc))
    if len(rom) != 0x40000:
        parser.error(f"expected a 0x40000-byte OKI region; found {len(rom):#x}")
    # Z80 startup at 0x0070 loads the board setting at 0x0e6f and writes it
    # to the CPS-1 OKI pin-7 latch at 0xf006. Willow's value 1 selects high.
    pin7_init = bytes.fromhex("3204f03a6f0e3206f0")
    if (sound_cpu[0x0070:0x0070 + len(pin7_init)] != pin7_init
            or sound_cpu[0x0E6F] != 1):
        parser.error("Willow's Z80 OKI pin-7 startup setting changed")
    if args.pin7 != "high":
        parser.error("Willow's sound program selects OKI pin 7 high")

    args.output.mkdir(parents=True, exist_ok=True)
    report: list[dict[str, int | str]] = []
    pcm4_pack = bytearray()
    pcm4_manifest: list[dict[str, int | str]] = []
    pcm4_attenuated_manifest: list[dict[str, int | str]] = []
    for sample_id in range(128):
        start, stop = sample_bounds(rom, sample_id)
        if start >= stop:
            report.append({"id": sample_id, "start": start, "stop": stop, "status": "invalid"})
            continue
        pcm = decode_sample(rom, start, stop)
        source_samples = len(pcm)
        if sample_id in PCM4_SAMPLE_IDS:
            pcm4_pcm = resample_polyphase(
                pcm, args.sample_clock_hz, PCM4_VOICE_RATE, input_divisor)
            offset = len(pcm4_pack)
            pcm8 = bytes((max(-128, min(127, round(value / 256))) & 0xFF)
                         for value in pcm4_pcm)
            pcm4_pack.extend(pcm8)
            pcm4_pack.extend(b"\0" * (-len(pcm4_pack) % PCM4_SAMPLE_ALIGNMENT))
            pcm4_manifest.append({
                "sample_id": sample_id,
                "offset": offset,
                "size": len(pcm8),
                "padded_size": len(pcm4_pack) - offset,
                "output_sample_rate_hz": PCM4_VOICE_RATE,
            })
            if sample_id == 0x19:
                # All source commands for sample 0x19 use OKI attenuation
                # code 2. Encode its half gain into the PCM itself so the
                # native PCM4 path does not approximate it with one of the
                # driver's 16 volume steps. Keep the full-level clip above
                # for the developer sample browser.
                attenuated_offset = len(pcm4_pack)
                attenuated_pcm8 = bytes(
                    (max(-128, min(127, round(value * 0.5 / 256))) & 0xFF)
                    for value in pcm4_pcm)
                pcm4_pack.extend(attenuated_pcm8)
                pcm4_pack.extend(
                    b"\0" * (-len(pcm4_pack) % PCM4_SAMPLE_ALIGNMENT))
                pcm4_attenuated_manifest.append({
                    "sample_id": sample_id,
                    "volume_nibble": 2,
                    "offset": attenuated_offset,
                    "size": len(attenuated_pcm8),
                    "padded_size": len(pcm4_pack) - attenuated_offset,
                    "output_sample_rate_hz": PCM4_VOICE_RATE,
                })
        pcm = resample_polyphase(pcm, args.sample_clock_hz,
                                 args.output_rate, input_divisor)
        filename = f"sample_{sample_id:02x}.wav"
        write_wav(args.output / filename, pcm, args.output_rate)
        if sample_id == 0x19:
            # The direct OKI table uses attenuation code 2 (0.5 gain) for
            # this sample. XGM PCM has no per-voice volume control, so store
            # the source attenuation as a second uncompressed PCM resource.
            attenuated = [round(value * 0.5) for value in pcm]
            write_wav(args.output / "sample_19_vol2.wav", attenuated,
                      args.output_rate)
        report.append({
            "id": sample_id,
            "start": start,
            "stop": stop,
            "nibbles": source_samples,
            "output_samples": len(pcm),
            "duration_ms": round(1000 * len(pcm) / args.output_rate),
            "file": filename,
        })

    (args.output / "index.json").write_text(json.dumps({
        "source": "Willow OKIM6295 sample ROMs",
        "source_clock_hz": args.sample_clock_hz,
        "pin7": args.pin7,
        "source_sample_rate_hz": args.sample_clock_hz / input_divisor,
        "source_sample_rate_ratio": f"{args.sample_clock_hz}/{input_divisor}",
        "pin7_source": {
            "sound_cpu_rom": "wl_09.12b",
            "startup_code_address": "0x0070",
            "setting_address": "0x0e6f",
            "setting_value": sound_cpu[0x0E6F],
            "write_port": "0xf006",
        },
        "output_sample_rate_hz": args.output_rate,
        "pcm4_pcm_samples": pcm4_manifest,
        "pcm4_pcm_attenuated_variants": pcm4_attenuated_manifest,
        "decoder": "MSM6295 4-bit OKI ADPCM, MAME table semantics",
        "resampler": "24-lobe Blackman-windowed sinc, normalized per output sample",
        "samples": report,
    }, indent=2) + "\n")
    if len(pcm4_pack) > PCM4_PACK_SIZE:
        parser.error(f"PCM4 sample pack is {len(pcm4_pack):#x} bytes, "
                     f"larger than its {PCM4_PACK_SIZE:#x}-byte mapper window")
    pcm4_pack.extend(b"\0" * (PCM4_PACK_SIZE - len(pcm4_pack)))
    (args.output / "pcm4_oki_16k_signed8.bin").write_bytes(pcm4_pack)
    header = ["/* Generated from build/oki_wav/index.json. */",
              "#ifndef WILLOW_PCM4_OFFSETS_H",
              "#define WILLOW_PCM4_OFFSETS_H", ""]
    for sample in pcm4_manifest:
        sample_id = int(sample["sample_id"])
        header.append(f"#define WILLOW_PCM4_{sample_id:02x}_OFFSET "
                      f"0x{int(sample['offset']):X}UL")
        header.append(f"#define WILLOW_PCM4_{sample_id:02x}_SIZE "
                      f"0x{int(sample['size']):X}UL")
        header.append(f"#define WILLOW_PCM4_{sample_id:02x}_PADDED_SIZE "
                      f"0x{int(sample['padded_size']):X}UL")
    for sample in pcm4_attenuated_manifest:
        sample_id = int(sample["sample_id"])
        volume_nibble = int(sample["volume_nibble"])
        header.append(f"#define WILLOW_PCM4_{sample_id:02x}_VOL{volume_nibble}_OFFSET "
                      f"0x{int(sample['offset']):X}UL")
        header.append(f"#define WILLOW_PCM4_{sample_id:02x}_VOL{volume_nibble}_SIZE "
                      f"0x{int(sample['size']):X}UL")
        header.append(f"#define WILLOW_PCM4_{sample_id:02x}_VOL{volume_nibble}_PADDED_SIZE "
                      f"0x{int(sample['padded_size']):X}UL")
    header.extend(["", "#endif", ""])
    Path("build/willow_pcm4.h").write_text("\n".join(header))
    valid = [entry for entry in report if "file" in entry]
    print(f"Decoded {len(valid)} of 128 table entries to {args.output} "
          f"at {args.output_rate} Hz mono PCM from "
          f"{args.sample_clock_hz}/{input_divisor} Hz OKI data")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
