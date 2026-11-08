/* vgmrender: offline sanity renderer for opm2opn VGMs (YM2612 + SN76489 only).
 * Uses Genesis Plus GX ym2612.c / psg.c / blip_buf.c copied into gpgx/.
 * Writes a mono 16-bit 44.1 kHz WAV (never played) and prints RMS, peak and
 * a dominant-pitch estimate (autocorrelation) for a few windows.
 *
 * usage: vgmrender IN.vgm OUT.wav [--seconds S]   (loop followed once if S > length)
 * build: clang -O2 -Igpgx vgmrender.c gpgx/ym2612.c gpgx/psg.c gpgx/blip_buf.c -o vgmrender
 */
#include "shared.h"
#include <stdio.h>
t_config config; t_snd snd; int8 audio_hard_disable = 0;

#define MCLK 53693175.0
#define FMRATE (MCLK / 7.0 / 144.0)

static uint8_t *vgm; static long vgmLen;
static short *outbuf; static long outLen, outCap;
static double fmPos; static long mclkNow;

static void emit(long samples)
{
  /* FM at native rate, linearly resampled to 44.1 kHz; PSG through blip. */
  static int fmbuf[2 * 4096]; static double prevFm = 0;
  while (samples > 0) {
    long n = samples > 1024 ? 1024 : samples; samples -= n;
    long mclkEnd = mclkNow + (long)(n * MCLK / 44100.0);
    psg_end_frame((unsigned)(mclkEnd - mclkNow));
    blip_end_frame(snd.blips[0], (unsigned)(mclkEnd - mclkNow));
    mclkNow = 0;
    (void)mclkEnd;
    short ps[2 * 1100]; int got = blip_read_samples(snd.blips[0], ps, (int)n);
    double need = fmPos + n * FMRATE / 44100.0; int fmN = (int)need; if (fmN > 4000) fmN = 4000;
    if (fmN > 0) YM2612Update(fmbuf, fmN);
    if (outLen + n > outCap) { outCap = (outLen + n) * 2; outbuf = realloc(outbuf, outCap * sizeof(short)); }
    for (long i = 0; i < n; i++) {
      double p = fmPos + i * FMRATE / 44100.0; int k = (int)p; double fr = p - k;
      double a = k == 0 ? prevFm : (fmbuf[2 * (k - 1)] + fmbuf[2 * (k - 1) + 1]) * 0.5;
      double b = k < fmN ? (fmbuf[2 * k] + fmbuf[2 * k + 1]) * 0.5 : a;
      double fm = a + (b - a) * fr;
      double ps1 = i < got ? (ps[2 * i] + ps[2 * i + 1]) * 0.5 : 0;
      static double hpIn = 0, hpOut = 0; double x = fm + ps1; hpOut = 0.9995 * (hpOut + x - hpIn); hpIn = x; double v = hpOut;
      if (v > 32767) v = 32767; if (v < -32768) v = -32768;
      outbuf[outLen++] = (short)v;
    }
    if (fmN > 0) prevFm = (fmbuf[2 * (fmN - 1)] + fmbuf[2 * (fmN - 1) + 1]) * 0.5;
    fmPos = need - fmN;
  }
}

static double pitchAt(long from, long len)
{
  if (from + len + 2048 > outLen) return 0;
  double best = 0; int bestLag = 0; double e0 = 0;
  for (long i = 0; i < len; i++) e0 += (double)outbuf[from + i] * outbuf[from + i];
  if (e0 <= 0) return 0;
  for (int lag = 20; lag < 1800; lag++) {
    double s = 0, e1 = 0;
    for (long i = 0; i < len; i++) { s += (double)outbuf[from + i] * outbuf[from + i + lag]; e1 += (double)outbuf[from + i + lag] * outbuf[from + i + lag]; }
    double r = s / sqrt(e0 * e1 + 1);
    if (r > best) { best = r; bestLag = lag; }
  }
  return best > 0.5 ? 44100.0 / bestLag : 0;
}

int main(int argc, char **argv)
{
  if (argc < 3) { fprintf(stderr, "usage: vgmrender IN.vgm OUT.wav [--seconds S]\n"); return 2; }
  double maxSec = 0; for (int a = 3; a + 1 < argc; a++) if (!strcmp(argv[a], "--seconds")) maxSec = atof(argv[++a]);
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; fseek(f, 0, SEEK_END); vgmLen = ftell(f); rewind(f);
  vgm = malloc(vgmLen); if (fread(vgm, 1, vgmLen, f) != (size_t)vgmLen) return 2; fclose(f);
  if (memcmp(vgm, "Vgm ", 4)) { fprintf(stderr, "not a VGM\n"); return 2; }
  uint32_t dataOff = 0x34 + (vgm[0x34] | vgm[0x35] << 8 | vgm[0x36] << 16 | (uint32_t)vgm[0x37] << 24);
  uint32_t loopRel = vgm[0x1c] | vgm[0x1d] << 8 | vgm[0x1e] << 16 | (uint32_t)vgm[0x1f] << 24;
  long loopOff = loopRel ? 0x1c + loopRel : -1;
  YM2612Init(); YM2612Config(YM2612_DISCRETE); YM2612ResetChip();
  psg_init(PSG_INTEGRATED); psg_reset(); psg_config(0, 150, 0xff);
  snd.blips[0] = blip_new(4096); blip_set_rates(snd.blips[0], MCLK, 44100);
  long maxSamples = maxSec > 0 ? (long)(maxSec * 44100) : -1; int loops = 0;
  long p = dataOff;
  while (p < vgmLen) {
    uint8_t c = vgm[p];
    if (maxSamples >= 0 && outLen >= maxSamples) break;
    if (c == 0x52 || c == 0x53) { int port = (c - 0x52) * 2; YM2612Write(port, vgm[p + 1]); YM2612Write(port + 1, vgm[p + 2]); p += 3; }
    else if (c == 0x50) { psg_write((unsigned)mclkNow, vgm[p + 1]); p += 2; }
    else if (c == 0x61) { emit(vgm[p + 1] | vgm[p + 2] << 8); p += 3; }
    else if (c == 0x62) { emit(735); p++; }
    else if (c == 0x63) { emit(882); p++; }
    else if ((c & 0xf0) == 0x70) { emit((c & 15) + 1); p++; }
    else if (c == 0x66) { if (loopOff >= 0 && maxSamples > outLen && loops < 8) { p = loopOff; loops++; continue; } break; }
    else { fprintf(stderr, "unsupported VGM command %02x at %lx\n", c, p); return 1; }
  }
  /* WAV */
  f = fopen(argv[2], "wb"); if (!f) return 2;
  uint32_t dataBytes = (uint32_t)(outLen * 2), riff = 36 + dataBytes, fmtLen = 16, rate = 44100, byteRate = 88200;
  uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtLen, 4, 1, f);
  fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&byteRate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f); fwrite(&dataBytes, 4, 1, f); fwrite(outbuf, 2, outLen, f); fclose(f);
  double ss = 0; int peak = 0; long nz = 0;
  for (long i = 0; i < outLen; i++) { ss += (double)outbuf[i] * outbuf[i]; if (abs(outbuf[i]) > peak) peak = abs(outbuf[i]); if (outbuf[i]) nz++; }
  printf("%s: %.2f s, loops followed %d, RMS %.1f, peak %d, non-zero samples %.1f%%\n", argv[2], outLen / 44100.0, loops, sqrt(ss / (outLen ? outLen : 1)), peak, 100.0 * nz / (outLen ? outLen : 1));
  printf("pitch estimates (Hz) at 0.05 s windows:");
  for (double t = 0.05; t < outLen / 44100.0 - 0.1 && t < 30; t += (outLen / 44100.0 > 5 ? 2.5 : 0.1)) printf(" %.2fs:%.0f", t, pitchAt((long)(t * 44100), 2205));
  printf("\n");
  return 0;
}
