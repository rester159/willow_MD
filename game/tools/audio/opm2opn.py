#!/usr/bin/env python3
"""opm2opn: convert an opmlog YM2151 register log (arcade Willow sound program,
249.69 Hz service ticks) to a VGM 1.60 for YM2612 (7,670,453 Hz) + SN76489
(3,579,545 Hz). Offline tool; its output is data, never arcade code.

Mapping (game/specs/audio.md section 6), default plan: OPM voices 0-4 ->
FM 1-5, 5-7 -> PSG tone 1-3, OPM noise (voice 7) -> PSG noise. FM6 stays free
for the XGM2 DAC.
  KC/KF            -> F-number/block (exact frequency, 3.579545 MHz OPM)
  DT1/MUL, TL, KS/AR, AM/D1R, D2R, D1L/RR  -> copied (operator order M1,M2,C1,C2
                      = OPN S1,S3,S2,S4 register offsets 0,4,8,12)
  DT2              -> folded into MUL when the product is within 6 % of an
                      available MUL value (6 %, about a semitone), else dropped
  RL/FB/CON        -> B0 (FB/ALG) and B4 L/R (bit order swapped)
  LFO              -> OPN LFO: nearest of the 8 rates (triangle only), off when
                      the OPM rate is below 2 Hz; per-channel PMS/AMS chosen by
                      nearest effective depth (OPM sensitivity x PMD/AMD);
                      OPM LFO reset (reg 01 bit 1) -> OPN LFO disable/enable
  PSG voices       -> period from KC/KF; attenuation from the loudest carrier
                      TL ((TL*3+4)/8), silent on key-off or no output side
Timer registers (10-14) and the CT bits are ignored.

usage: opm2opn.py IN.opmlog OUT.vgm [--plan 0=F0,1=F1,...,5=P0,6=P1,7=P2|auto]
                  [--tail SECONDS] [--report OUT.json] [--loop TICK] [--lead FRAMES]
--plan auto maps the voices that are keyed on to FM 1.. in order (effects).
"""
import json, math, struct, sys

YM_CLK, PSG_CLK, OPM_CLK = 7670453, 3579545, 3579545
NOTEIX = [0, 1, 2, 2, 3, 4, 5, 5, 6, 7, 8, 8, 9, 10, 11, 11]
DT2 = [1.0, 1.41, 1.57, 1.73]
OPN_LFO = [3.98, 5.56, 6.02, 6.37, 6.88, 9.63, 48.1, 72.2]
OPM_PMS = [0, 5, 10, 20, 50, 100, 400, 700]          # cents at PMD=127
OPN_PMS = [0, 3.4, 6.7, 10, 14, 20, 40, 80]
OPM_AMS = [0, 23.90625, 47.8125, 95.625]             # dB at AMD=127
OPN_AMS = [0, 1.4, 5.9, 11.8]
CARRIERS = [8, 8, 8, 8, 12, 14, 14, 15]              # bit0 M1, bit1 M2, bit2 C1, bit3 C2
FM_KEYCH = [0, 1, 2, 4, 5, 6]


def read_log(path):
    ev, hdr = [], {}
    for line in open(path):
        f = line.split()
        if not f or f[0].startswith('#'):
            continue
        if f[0] == 'Y':
            ev.append((int(f[1]), int(f[2], 16), int(f[3], 16)))
        elif f[0] in ('O', 'W'):
            hdr.setdefault('oki', []).append((int(f[1]), f[2:]))
        elif f[0] == 'rate':
            hdr['rate'] = float(f[1])
        elif f[0] == 'end':
            hdr['end'] = int(f[1]); hdr['reason'] = f[2]
        elif f[0] == 'loop':
            hdr['loop'] = (int(f[1]), int(f[2]))
        elif f[0] in ('cmd', 'param'):
            hdr[f[0]] = int(f[1], 16)
    return ev, hdr


def parse_plan(text, ev):
    if text == 'auto':
        used = sorted({v & 7 for _, r, v in ev if r == 8 and (v >> 3) & 15})
        if len(used) > 5:
            raise SystemExit('auto plan: more than 5 voices keyed')
        return {u: ('F', i) for i, u in enumerate(used)}
    plan = {}
    for part in text.split(','):
        k, v = part.split('=')
        plan[int(k)] = (v[0].upper(), int(v[1:]))
    return plan


def opm_freq(kc, kf):
    oct_, note = (kc >> 4) & 7, NOTEIX[kc & 15]
    return 440.0 * 2 ** ((oct_ - 4) + (note + kf / 64.0 - 8) / 12.0)


def nearest(table, value):
    return min(range(len(table)), key=lambda i: abs(table[i] - value))


class Conv:
    def __init__(self, plan):
        self.plan = plan
        self.opm = [0] * 256
        self.keys = [0] * 8
        self.out = []               # (tick, kind, a, b): kind 0/1 = YM port, 2 = PSG byte
        self.shadow = {}
        self.pmd = self.amd = 0
        self.lfo_reset = False
        self.stats = dict(dt2_folded=0, dt2_dropped=0, lfo_off_slow=0, psg_clamped=0, fnum_clamped=0)
        self.tick = 0

    # ---- low level
    def ym(self, port, reg, val, force=False):
        key = (port, reg)
        if not force and self.shadow.get(key) == val:
            return
        self.shadow[key] = val
        self.out.append((self.tick, port, reg, val))

    def psg(self, byte, key=None):
        if key is not None:
            if self.shadow.get(key) == byte:
                return
            self.shadow[key] = byte
        self.out.append((self.tick, 2, byte, 0))

    def fm(self, base, n, val, force=False):
        self.ym(n // 3, base + n % 3, val, force)

    def invalidate(self):
        self.shadow = {}

    # ---- init
    def init(self):
        self.ym(0, 0x22, 0, True); self.ym(0, 0x27, 0, True); self.ym(0, 0x2B, 0, True)
        for c in FM_KEYCH:
            self.ym(0, 0x28, c, True)
        for p in (0, 1):
            for c in range(3):
                self.ym(p, 0xB4 + c, 0xC0, True)
                for o in range(4):
                    self.ym(p, 0x90 + o * 4 + c, 0, True)
                    self.ym(p, 0x40 + o * 4 + c, 0x7F, True)
        for c in range(4):
            self.psg(0x9F | c << 5, ('v', c))

    # ---- LFO
    def lfo_rate(self):
        l = self.opm[0x18]
        return (16 + (l & 15)) / 32.0 * 2 ** (l >> 4) * (52.9 / 31744.0)

    def lfo_active(self):
        return (self.pmd or self.amd) and self.lfo_rate() >= 2.0

    def update_lfo(self):
        if (self.pmd or self.amd) and self.lfo_rate() < 2.0:
            self.stats['lfo_off_slow'] += 1
        val = 0
        if self.lfo_active() and not self.lfo_reset:
            val = 8 | nearest(OPN_LFO, self.lfo_rate())
        self.ym(0, 0x22, val)
        for v, (kind, n) in self.plan.items():
            if kind == 'F':
                self.update_b4(v, n)

    def update_b4(self, v, n):
        rl = self.opm[0x20 + v] >> 6
        lr = ((rl & 1) << 1 | (rl >> 1)) << 6          # OPM bit6 L/bit7 R -> OPN bit7 L/bit6 R
        sens = self.opm[0x38 + v]
        pms = ams = 0
        if self.lfo_active():
            pms = nearest(OPN_PMS, OPM_PMS[(sens >> 4) & 7] * self.pmd / 127.0)
            ams = nearest(OPN_AMS, OPM_AMS[sens & 3] * self.amd / 127.0)
        self.fm(0xB4, n, lr | ams << 4 | pms)

    # ---- pitch
    def update_pitch(self, v, kind, n):
        f = opm_freq(self.opm[0x28 + v] & 0x7F, self.opm[0x30 + v] >> 2)
        if kind == 'F':
            for block in range(8):
                fnum = round(f * 144 * 2 ** (21 - block) / YM_CLK)
                if fnum < 2048:
                    break
            else:
                fnum = 2047; self.stats['fnum_clamped'] += 1
            self.fm(0xA4, n, block << 3 | fnum >> 8, True)
            self.fm(0xA0, n, fnum & 255, True)
        elif not (v == 7 and self.opm[0x0F] & 0x80):
            per = PSG_CLK / (32.0 * f)
            while per > 1023:
                per /= 2; self.stats['psg_clamped'] += 1
            per = max(1, int(round(per)))
            self.psg(0x80 | n << 5 | (per & 15), ('pl', n))
            self.psg(per >> 4, ('ph', n))

    def update_mul(self, v, op, n):
        reg = self.opm[0x40 + op * 8 + v]
        dt2 = self.opm[0xC0 + op * 8 + v] >> 6
        mul = reg & 15
        if dt2:
            eff = (mul or 0.5) * DT2[dt2]
            cands = [0.5] + list(range(1, 16))
            best = min(cands, key=lambda m: abs(math.log(m / eff)))
            if abs(math.log(best / eff)) < math.log(1.06):
                mul = 0 if best == 0.5 else best; self.stats['dt2_folded'] += 1
            else:
                self.stats['dt2_dropped'] += 1
        self.fm(0x30 + op * 4, n, (reg & 0x70) | mul)

    # ---- PSG level
    def update_psg(self, v, n):
        con = self.opm[0x20 + v]
        mask, level = CARRIERS[con & 7], 127
        noise = v == 7 and self.opm[0x0F] & 0x80
        for op in range(4):
            if mask >> op & 1:
                if noise and op == 3:
                    continue
                level = min(level, self.opm[0x60 + op * 8 + v] & 127)
        att = min(15, (level * 3 + 4) // 8)
        if not (con & 0xC0) or not self.keys[v]:
            att = 15
        if noise:
            natt = 15 if (not (con & 0xC0) or not (self.keys[v] & 8)) else min(15, ((self.opm[0x78 + v] & 127) * 3 + 4) // 8)
            nf = self.opm[0x0F] & 31
            rate = OPM_CLK / 64.0 / ((nf ^ 31) + 1)
            fixed = [PSG_CLK / 512.0, PSG_CLK / 1024.0, PSG_CLK / 2048.0]
            if n == 2 and rate > fixed[0] * 1.2:
                per = max(1, min(1023, round(PSG_CLK / (32.0 * rate))))
                self.psg(0x80 | 2 << 5 | (per & 15), ('pl', 2)); self.psg(per >> 4, ('ph', 2))
                self.psg(0xE7, ('nm',)); att = 15
            else:
                self.psg(0xE4 | nearest(fixed, rate), ('nm',))
            self.psg(0xF0 | natt, ('v', 3))
        elif v == 7:
            self.psg(0xFF, ('v', 3))
        self.psg(0x90 | n << 5 | att, ('v', n))

    # ---- one OPM write
    def write(self, reg, val):
        self.opm[reg] = val
        if reg == 0x01:
            r = bool(val & 2)
            if r != self.lfo_reset:
                self.lfo_reset = r; self.update_lfo()
            return
        if reg == 0x08:
            v = val & 7; self.keys[v] = (val >> 3) & 15
            kind, n = self.plan.get(v, (None, 0))
            if kind == 'F':
                self.ym(0, 0x28, self.keys[v] << 4 | FM_KEYCH[n], True)
            elif kind == 'P':
                self.update_psg(v, n)
            return
        if reg == 0x0F:
            kind, n = self.plan.get(7, (None, 0))
            if kind == 'P':
                self.update_psg(7, n); self.update_pitch(7, kind, n)
            return
        if reg in (0x18, 0x1B):
            self.update_lfo(); return
        if reg == 0x19:
            if val & 0x80: self.pmd = val & 0x7F
            else: self.amd = val & 0x7F
            self.update_lfo(); return
        if reg < 0x20:
            return
        v = reg & 7
        kind, n = self.plan.get(v, (None, 0))
        if kind is None:
            return
        if reg < 0x28:
            if kind == 'F':
                self.fm(0xB0, n, val & 0x3F); self.update_b4(v, n)
            else:
                self.update_psg(v, n)
        elif reg < 0x38:
            self.update_pitch(v, kind, n)
        elif reg < 0x40:
            if kind == 'F':
                self.update_b4(v, n)
        else:
            group, op = reg >> 5, (reg >> 3) & 3
            if kind == 'P':
                if group == 3: self.update_psg(v, n)
                return
            if group == 2 or group == 6:
                self.update_mul(v, op, n)
                if group == 6:
                    self.fm(0x70 + op * 4, n, val & 0x1F)
            elif group == 3: self.fm(0x40 + op * 4, n, val & 0x7F)
            elif group == 4: self.fm(0x50 + op * 4, n, val & 0xDF)
            elif group == 5: self.fm(0x60 + op * 4, n, val & 0x9F)
            elif group == 7: self.fm(0x80 + op * 4, n, val)


def write_vgm(path, out, rate, end_tick, loop_tick, tail, ninit=0, lead=0):
    # lead: 60 Hz frames between the chip set-up (this converter's reset plus
    # the sound program's tick-0 voice reset, no key-on) and tick 1, for songs
    # whose first frame is over XGM2's frame size limit.
    def sample(t):
        return int(round(t * 44100.0 / rate)) + lead * 735
    data, now, loop_off, loop_sample = bytearray(), 0, None, None

    def wait(to):
        nonlocal now, data
        d = to - now
        while d > 0:
            if d == 735: data.append(0x62); d = 0
            elif d == 882: data.append(0x63); d = 0
            elif d <= 16: data.append(0x70 + d - 1); d = 0
            else:
                w = min(d, 65535); data.append(0x61); data += struct.pack('<H', w); d -= w
        now = to
    i = 0
    if lead:
        while i < len(out) and out[i][0] == 0:
            _, kind, a, b = out[i]; i += 1
            if kind == 2: data += bytes((0x50, a))
            else: data += bytes((0x52 + kind, a, b))
    for t in range(1 if lead else 0, end_tick + 1):
        wait(sample(t))
        if loop_tick is not None and t == loop_tick:
            loop_off, loop_sample = len(data), now
        while i < len(out) and out[i][0] == t:
            _, kind, a, b = out[i]; i += 1
            if kind == 2: data += bytes((0x50, a))
            else: data += bytes((0x52 + kind, a, b))
    wait(sample(end_tick + 1) + int(round(tail * 44100)))
    data.append(0x66)
    hdr = bytearray(0x100)
    hdr[0:4] = b'Vgm '
    struct.pack_into('<I', hdr, 0x04, 0x100 + len(data) - 4)
    struct.pack_into('<I', hdr, 0x08, 0x160)
    struct.pack_into('<I', hdr, 0x0C, PSG_CLK)
    struct.pack_into('<I', hdr, 0x18, now)
    if loop_off is not None:
        struct.pack_into('<I', hdr, 0x1C, 0x100 + loop_off - 0x1C)
        struct.pack_into('<I', hdr, 0x20, now - loop_sample)
    struct.pack_into('<I', hdr, 0x24, 60)
    struct.pack_into('<HB', hdr, 0x28, 0x0009, 16)
    struct.pack_into('<I', hdr, 0x2C, YM_CLK)
    struct.pack_into('<I', hdr, 0x34, 0x100 - 0x34)
    open(path, 'wb').write(hdr + data)
    return 0x100 + len(data), now, (now - loop_sample) if loop_off is not None else 0


def measure(ev, rate, plan, end_tick, loop):
    keyons, keyoffs, usage = [], [], {}
    for t, r, v in ev:
        if r == 8:
            vo = v & 7
            if (v >> 3) & 15:
                keyons.append((t, vo)); usage[vo] = usage.get(vo, 0) + 1
            else:
                keyoffs.append((t, vo))
    def shifts(mode):
        out = []
        for t, _ in keyons:
            s = t / rate
            q = (round(s * 60) if mode == 'round' else math.floor(s * 60)) / 60.0
            out.append(abs(s - q) * 1000.0)
        return out
    res = dict(keyons=len(keyons), voices_keyed={str(k): usage[k] for k in sorted(usage)},
               plan={str(k): '%s%d' % plan[k] for k in sorted(plan)},
               unmapped_keyed_voices=[k for k in sorted(usage) if k not in plan])
    for mode in ('round', 'floor'):
        s = shifts(mode)
        res['shift_' + mode] = dict(max_ms=round(max(s), 3) if s else 0, mean_ms=round(sum(s) / len(s), 3) if s else 0,
                                    over_2ms=sum(1 for x in s if x > 2.0), over_4ms=sum(1 for x in s if x > 4.0))
    # ticks that land in one 60 Hz frame with an earlier tick: key-off/on pairs of one voice merged
    merged, last = 0, {}
    evs = sorted([(t, vo, 0) for t, vo in keyoffs] + [(t, vo, 1) for t, vo in keyons])
    for t, vo, on in evs:
        f = math.floor(t / rate * 60)
        if on and vo in last and last[vo][1] == f and last[vo][0] != t:
            merged += 1
        last[vo] = (t, f)
    res['retrigger_same_frame'] = merged
    distinct = sorted({t for t, _ in keyons})
    frames = {math.floor(t / rate * 60) for t in distinct}
    res['keyon_ticks'] = len(distinct); res['keyon_frames'] = len(frames)
    return res


def main():
    a = sys.argv[1:]
    if len(a) < 2:
        print(__doc__); sys.exit(2)
    src, dst = a[0], a[1]
    opts = dict(zip(a[2::2], a[3::2]))
    ev, hdr = read_log(src)
    rate, end = hdr['rate'], hdr['end']
    loop = hdr.get('loop')
    reason = hdr.get('reason')
    # A "loop" whose repeated part writes nothing but the timer registers
    # (10-14) is the song's end: the tracks stopped and only the tick counter
    # D014 (period 256) still changes. Convert it as a one-shot.
    if loop and not any(loop[0] <= t <= end and not 0x10 <= r <= 0x14 for t, r, v in ev):
        end, loop, reason = loop[0] - 1, None, 'ended'
    # --loop L: loop the logged ticks L..end (a musical loop found by hand when
    # the whole sound RAM never repeats, e.g. one track with its own period).
    if '--loop' in opts:
        loop, reason = (int(opts['--loop'], 0), end + 1), 'loop (forced)'
    plan = parse_plan(opts.get('--plan', '0=F0,1=F1,2=F2,3=F3,4=F4,5=P0,6=P1,7=P2'), ev)
    tail = float(opts.get('--tail', 0 if loop else 0.5))
    c = Conv(plan); c.init(); ninit = len(c.out)
    for t, r, v in ev:
        if loop and c.tick < loop[0] <= t:
            c.invalidate()
        c.tick = t
        c.write(r, v)
    size, total, loop_samples = write_vgm(dst, c.out, rate, end, loop[0] if loop else None, tail,
                                          ninit, int(opts.get('--lead', 0)))
    rep = dict(source=src, vgm=dst, vgm_bytes=size, total_samples=total, seconds=round(total / 44100.0, 3),
               ticks=end + 1, end_reason=reason, oki_events=len(hdr.get('oki', [])))
    if loop:
        rep.update(loop_start_tick=loop[0], loop_ticks=loop[1] - loop[0],
                   loop_seconds=round((loop[1] - loop[0]) / rate, 3), loop_samples=loop_samples,
                   intro_seconds=round(loop[0] / rate, 3))
    rep.update(measure(ev, rate, plan, end, loop))
    rep['conversion'] = c.stats
    rep['chip_writes'] = len(c.out)
    js = json.dumps(rep, indent=1)
    if '--report' in opts:
        open(opts['--report'], 'w').write(js + '\n')
    print(js)


if __name__ == '__main__':
    main()
