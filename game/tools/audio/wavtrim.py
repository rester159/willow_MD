#!/usr/bin/env python3
"""wavtrim: cut the inaudible tail of a mono 16-bit effect WAV (offline tool).

XGM2 plays 8-bit PCM, so samples below half an 8-bit step (|x| < 128 in 16-bit)
round to silence. The WAV is cut after the last sample at or above --floor
(default 192) and the final --fade milliseconds (default 3) are faded out to
avoid a click. Leading silence is kept: it is part of the cue timing.

usage: wavtrim.py IN.wav OUT.wav [--floor N] [--fade MS]
"""
import struct, sys, wave


def trim(pcm, rate, floor=192, fade_ms=3.0):
    last = max((i for i, v in enumerate(pcm) if abs(v) >= floor), default=-1)
    pcm = list(pcm[:last + 1])
    n = min(len(pcm), int(rate * fade_ms / 1000.0))
    for k in range(n):
        i = len(pcm) - n + k
        pcm[i] = int(round(pcm[i] * (n - k) / (n + 1)))
    return pcm


def read(path):
    w = wave.open(path)
    assert w.getnchannels() == 1 and w.getsampwidth() == 2, path
    n = w.getnframes()
    return list(struct.unpack('<%dh' % n, w.readframes(n))), w.getframerate()


def write(path, pcm, rate):
    w = wave.open(path, 'wb')
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
    w.writeframes(struct.pack('<%dh' % len(pcm), *pcm))
    w.close()


def main():
    a = sys.argv[1:]
    if len(a) < 2:
        print(__doc__); sys.exit(2)
    opts = dict(zip(a[2::2], a[3::2]))
    pcm, rate = read(a[0])
    out = trim(pcm, rate, int(opts.get('--floor', 192)), float(opts.get('--fade', 3)))
    write(a[1], out, rate)
    print('%s: %.3f s -> %.3f s at %d Hz' % (a[1], len(pcm) / rate, len(out) / rate, rate))


if __name__ == '__main__':
    main()
