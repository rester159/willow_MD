#!/usr/bin/env python3
"""okiwav: OKI MSM6295 voice sample for one arcade sound command -> WAV for
XGM2 (offline tool; output is data).

Command 81..EF: sample id = (cmd & 0x7F) mod 42 (Z80 03D2; 0E42 holds 41),
voice/attenuation byte = 0E43[id] (high nibble voice mask, low nibble the
MSM6295 attenuation step). The ADPCM decoder and the Blackman-sinc resampler
are those of tools/oki_samples.py (MAME table semantics), imported, not
rerun: that script also rewrites build/oki_wav and build/willow_pcm4.h.
Source rate 1 MHz / 132 (pin 7 high, set by Z80 0070 from 0E6F) = 7,575.76 Hz,
resampled to --rate (13300 = XGM2 full rate), attenuation applied, inaudible
tail trimmed (wavtrim.py).

usage: okiwav.py WILLOW.ZIP SOUNDCPU.bin CMD OUT.wav [--rate 13300]
"""
import json, os, sys, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', '..', 'tools'))
sys.path.insert(0, HERE)
import oki_samples  # noqa: E402
import wavtrim      # noqa: E402

# MSM6295 attenuation steps (MAME okim6295 volume table, /32).
OKI_VOLUME = [0x20, 0x16, 0x10, 0x0B, 0x08, 0x06, 0x04, 0x03, 0x02]


def main():
    a = sys.argv[1:]
    if len(a) < 4:
        print(__doc__); sys.exit(2)
    opts = dict(zip(a[4::2], a[5::2]))
    rate = int(opts.get('--rate', 13300))
    cmd = int(a[2], 0)
    snd = open(a[1], 'rb').read()
    assert 0x81 <= cmd <= 0xEF, 'not an OKI sample command'
    count = snd[0x0E42] + 1
    sid = (cmd & 0x7F) % count
    vv = snd[0x0E43 + sid]
    with zipfile.ZipFile(a[0]) as z:
        rom = z.read('wl_18.11c') + z.read('wl_19.12c')
    start, stop = oki_samples.sample_bounds(rom, sid)
    assert start < stop, 'invalid sample %d' % sid
    pcm = oki_samples.decode_sample(rom, start, stop)
    src_s = len(pcm) / (1e6 / 132)
    pcm = oki_samples.resample_polyphase(pcm, 1_000_000, rate, 132)
    gain = OKI_VOLUME[vv & 15] / 32.0 if (vv & 15) < 9 else 0.0
    pcm = [max(-32768, min(32767, int(round(v * gain)))) for v in pcm]
    out = wavtrim.trim(pcm, rate)
    wavtrim.write(a[3], out, rate)
    print(json.dumps(dict(cmd='%02x' % cmd, sample=sid, voice_mask=vv >> 4, attenuation=vv & 15,
                          source_seconds=round(src_s, 3), seconds=round(len(out) / rate, 3),
                          rate=rate, file=a[3])))


if __name__ == '__main__':
    main()
