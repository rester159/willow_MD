/* gprof: headless CPU profile of a Genesis ROM on the profiling Genesis
 * Plus GX core (cycles per program address: wprof_self / incl / calls),
 * written for analyze.py (candidate-renderer-v1/perf). No window.
 *   gprof CORE ROM FRAMES OUT.bin     KEYS as below; WPROF_START=frame (default 300)
 * Derived from the B1 harness menutest.c (only its input/env plumbing).
 *   menutest CORE ROM FRAMES
 * KEYS="from-to:BUTTONS;..." pad 1 held over frames [from, to) (U D L R A B C S)
 * DUMP="f,f,..." DIR=dir      frame f as dir/fNNNNNN.ppm
 * LOG=n                       every n frames: port state, task slots, game bytes
 * SRAM=file                   save RAM loaded before the run, written after (a new process = power cycle)
 * POKE="f:addr=hex;..."       work RAM bytes (addr FFxxxx) written after frame f
 * AUDIO=file                  raw 16-bit stereo output (44.1 kHz as the core reports)
 * cc -O2 -o out/menutest menutest.c */
#include <stdint.h>
#include <stdbool.h>
#include "/Users/rester159/genesis ports/_capcom/willow/tools/vendor/libretro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x800000];
static unsigned pad;
static uint16_t fb[256 * 512];
static unsigned fbw, fbh;
static bool env(unsigned cmd, void *data) {
  if (cmd == RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY || cmd == RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY) { *(const char **)data = "/tmp"; return true; }
  if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE) { *(bool *)data = false; return true; }
  if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE) { ((struct retro_variable *)data)->value = NULL; return false; }
  if (cmd == RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION) { *(unsigned *)data = 0; return true; }
  return cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT || cmd == RETRO_ENVIRONMENT_SET_VARIABLES ||
         cmd == RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS || cmd == RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void *d, unsigned w, unsigned h, size_t p) {
  if (!d || w > 512 || h > 256) return;
  fbw = w; fbh = h;
  for (unsigned y = 0; y < h; y++) memcpy(fb + y * w, (const uint8_t *)d + y * p, w * 2);
}
static void audio(int16_t l, int16_t r) { (void)l; (void)r; }
static FILE *audf;
static size_t batch(const int16_t *d, size_t n) { if (audf) fwrite(d, 4, n, audf); return n; }
static void poll(void) {}
static int16_t input(unsigned port, unsigned dev, unsigned idx, unsigned id) {
  (void)dev; (void)idx;
  if (port) return 0;
  if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t)pad;
  return (pad >> id) & 1;
}
/* Genesis buttons -> libretro ids (gpgx: B=0 A=8 Y=1 for A, Start=3, U4 D5 L6 R7) */
static unsigned buttons(const char *s, const char *e) {
  unsigned m = 0;
  for (; s < e; s++) switch (*s) {
    case 'U': m |= 1u << 4; break; case 'D': m |= 1u << 5; break;
    case 'L': m |= 1u << 6; break; case 'R': m |= 1u << 7; break;
    case 'A': m |= 1u << 1; break; case 'B': m |= 1u << 0; break;
    case 'C': m |= 1u << 8; break; case 'S': m |= 1u << 3; break;
  }
  return m;
}
static unsigned keys_at(unsigned f) {
  const char *k = getenv("KEYS");
  unsigned m = 0;
  while (k && *k) {
    unsigned a = strtoul(k, (char **)&k, 10), b = a + 1;
    if (*k == '-') b = strtoul(k + 1, (char **)&k, 10);
    const char *e = strchr(k, ';');
    if (!e) e = k + strlen(k);
    if (*k == ':' && f >= a && f < b) m |= buttons(k + 1, e);
    k = *e ? e + 1 : e;
  }
  return m;
}
static int dump_at(unsigned f) {
  const char *k = getenv("DUMP");
  while (k && *k) {
    if (strtoul(k, (char **)&k, 10) == f) return 1;
    if (*k == ',') k++; else break;
  }
  return 0;
}
static void save(unsigned f) {
  char q[4096];
  snprintf(q, sizeof q, "%s/f%06u.ppm", getenv("DIR") ? getenv("DIR") : ".", f);
  FILE *o = fopen(q, "wb");
  if (!o) return;
  fprintf(o, "P6\n%u %u\n255\n", fbw, fbh);
  for (unsigned k = 0; k < fbw * fbh; k++) {
    uint16_t c = fb[k];
    uint8_t px[3] = {(uint8_t)((c >> 11) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
    fwrite(px, 1, 3, o);
  }
  fclose(o);
}
#define FN(ret, n, args) ret(*n) args = dlsym(core, #n); if (!n) { fprintf(stderr, "missing %s\n", #n); return 2; }
int main(int argc, char **argv) {
  if (argc != 5) return 2;
  FILE *f = fopen(argv[2], "rb");
  if (!f) return 2;
  size_t size = fread(rom, 1, sizeof rom, f);
  fclose(f);
  void *core = dlopen(argv[1], RTLD_NOW);
  if (!core) { puts(dlerror()); return 2; }
  FN(void, retro_set_environment, (retro_environment_t)); FN(void, retro_set_video_refresh, (retro_video_refresh_t));
  FN(void, retro_set_audio_sample, (retro_audio_sample_t)); FN(void, retro_set_audio_sample_batch, (retro_audio_sample_batch_t));
  FN(void, retro_set_input_poll, (retro_input_poll_t)); FN(void, retro_set_input_state, (retro_input_state_t));
  FN(void, retro_init, (void)); FN(bool, retro_load_game, (const struct retro_game_info *)); FN(void, retro_run, (void));
  retro_set_environment(env); retro_set_video_refresh(video); retro_set_audio_sample(audio);
  retro_set_audio_sample_batch(batch); retro_set_input_poll(poll); retro_set_input_state(input); retro_init();
  struct retro_game_info game = {argv[2], rom, size, 0};
  if (!retro_load_game(&game)) return 3;
  unsigned *self = dlsym(core, "wprof_self"), *incl = dlsym(core, "wprof_incl"), *calls = dlsym(core, "wprof_calls");
  unsigned *on = dlsym(core, "wprof_enabled");
  if (!self || !on) { fprintf(stderr, "not the profiling core\n"); return 4; }
  unsigned frames = atoi(argv[3]), start = getenv("WPROF_START") ? atoi(getenv("WPROF_START")) : 300;
  /* PERFRAME="lo:hi": per profiled frame, the 68000 cycles spent outside
   * the PC range [lo, hi) (the VBlank wait), from the self counters. */
  unsigned lo = 0, hi = 0;
  unsigned long long last = 0;
  if (getenv("PERFRAME")) { lo = strtoul(getenv("PERFRAME"), 0, 16); hi = strtoul(strchr(getenv("PERFRAME"), ':') + 1, 0, 16); }
  for (unsigned n = 0; n < frames; n++) {
    if (n == start) *on = 1;
    pad = keys_at(n);
    retro_run();
    if (hi && n >= start) {
      unsigned long long busy = 0;
      for (unsigned k = 0; k < 0x300000; k++) if (k * 2 < lo || k * 2 >= hi) busy += self[k];
      printf("frame %u busy %llu\n", n, (busy - last) / 7);
      last = busy;
    }
  }
  f = fopen(argv[4], "wb");
  fwrite(self, 4, 0x300000, f); fwrite(incl, 4, 0x300000, f); fwrite(calls, 4, 0x300000, f);
  fclose(f);
  printf("profiled frames %u..%u\n", start, frames);
  return 0;
}
