// opmrender: offline reference render of an opmlog (original YM2151 writes)
// with ymfm's YM2151 at 3,579,545 Hz. Writes a mono 16-bit WAV at the chip's
// native rate (clock/64 = 55,930 Hz) or resampled to --rate (default 44100).
// OKI events in the log are ignored. Timer registers are harmless (no IRQ).
// Never plays audio.
//
// usage: opmrender IN.opmlog OUT.wav [--tail SECONDS] [--rate HZ] [--loops N]
// build: clang++ -std=c++14 -O2 -Iymfm opmrender.cpp ymfm/ymfm_opm.cpp -o opmrender
#include "ymfm_opm.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

struct Ev { unsigned tick; unsigned reg, val; };

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: opmrender IN.opmlog OUT.wav [--tail S] [--rate HZ] [--loops N]\n"); return 2; }
	double tail = 0.5, outRate = 44100, rate = 3579545.0 / 14336.0; int loops = 0;
	for (int a = 3; a + 1 < argc; a++) {
		if (!strcmp(argv[a], "--tail")) tail = atof(argv[++a]);
		else if (!strcmp(argv[a], "--rate")) outRate = atof(argv[++a]);
		else if (!strcmp(argv[a], "--loops")) loops = atoi(argv[++a]);
	}
	FILE *f = fopen(argv[1], "r"); if (!f) return 2;
	std::vector<Ev> ev; char line[256]; unsigned endTick = 0; long loopStart = -1;
	while (fgets(line, sizeof line, f)) {
		unsigned t, r, v; char word[32];
		if (sscanf(line, "Y %u %x %x", &t, &r, &v) == 3) ev.push_back({t, r, v});
		else if (sscanf(line, "rate %lf", &rate) == 1) {}
		else if (sscanf(line, "end %u %31s", &t, word) == 2) endTick = t;
		else if (sscanf(line, "loop %u", &t) == 1) loopStart = t;
	}
	fclose(f);
	ymfm::ymfm_interface intf;
	ymfm::ym2151 chip(intf);
	chip.reset();
	const double clock = 3579545.0, chipRate = chip.sample_rate(3579545);
	std::vector<double> raw;
	auto runTo = [&](double seconds) {
		long target = (long)llround(seconds * chipRate);
		ymfm::ym2151::output_data out;
		while ((long)raw.size() < target) { chip.generate(&out); raw.push_back((out.data[0] + out.data[1]) * 0.5); }
	};
	(void)clock;
	auto play = [&](unsigned from, unsigned to, double base) {
		for (const Ev &e : ev) {
			if (e.tick < from || e.tick > to) continue;
			runTo(base + (e.tick - from) / rate);
			chip.write_address((uint8_t)e.reg); chip.write_data((uint8_t)e.val);
		}
	};
	play(0, endTick, 0.0);
	double t = (endTick + 1) / rate;
	for (int n = 0; n < loops && loopStart >= 0; n++) { play((unsigned)loopStart, endTick, t); t += (endTick + 1 - loopStart) / rate; }
	runTo(t + tail);
	// linear resample + DC block
	std::vector<short> pcm; double hpIn = 0, hpOut = 0;
	long n = (long)(raw.size() * outRate / chipRate);
	double ss = 0; int peak = 0;
	for (long i = 0; i < n; i++) {
		double p = i * chipRate / outRate; long k = (long)p; double fr = p - k;
		double x = raw[k] + ((k + 1 < (long)raw.size() ? raw[k + 1] : raw[k]) - raw[k]) * fr;
		hpOut = 0.9995 * (hpOut + x - hpIn); hpIn = x;
		double v = hpOut; if (v > 32767) v = 32767; if (v < -32768) v = -32768;
		pcm.push_back((short)v); ss += v * v; if (std::abs((int)v) > peak) peak = std::abs((int)v);
	}
	f = fopen(argv[2], "wb"); if (!f) return 2;
	uint32_t dataBytes = (uint32_t)pcm.size() * 2, riff = 36 + dataBytes, fmtLen = 16, sr = (uint32_t)outRate, br = sr * 2;
	uint16_t fmt = 1, ch = 1, al = 2, bits = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtLen, 4, 1, f);
	fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&al, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&dataBytes, 4, 1, f); fwrite(pcm.data(), 2, pcm.size(), f); fclose(f);
	printf("%s: %.3f s at %.0f Hz, RMS %.1f, peak %d\n", argv[2], pcm.size() / outRate, outRate, std::sqrt(ss / (pcm.empty() ? 1 : pcm.size())), peak);
	return 0;
}
