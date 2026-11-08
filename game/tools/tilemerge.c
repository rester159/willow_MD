/* tilemerge: lossy 8x8 tile merging for stagepack.py. stdin: u32 N, u32 K,
 * u32 T, then per tile u32 weight and 192 bytes (r, g, b 0..7 per pixel,
 * row-major). stdout: N u32 representative indices.
 * K > 0: k-medoids to K tiles (farthest-point start).
 * K = 0: threshold: a tile is merged only into a representative within
 * weighted squared error T (heaviest tiles become representatives first).
 * FLIPS=1 (k-medoids mode): a tile may use a mirrored representative; the
 * output index then carries bit 30 (H) and bit 31 (V).
 * PERCEPT=1: distances in OKLab (x 10000, CHROMA=k weighs hue k times),
 * as tools/palopt.c; otherwise weighted RGB levels.
 * DCW=k: a change of the tile's mean colour counts k x 64 times more (a
 * whole tile shifting tone shows as a square; a texture change at the same
 * mean hardly shows). RGB-level mode only.
 * BORDER=k: the tile's outer ring of pixels counts k times (default 1): a
 * merged tile keeps its edges, so tile borders do not show (art pass,
 * 2026-11-04).
 * cc -O2 -o tools/tilemerge tools/tilemerge.c */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

static uint32_t n, k, T, *w, *med, *who;
static uint8_t *px;
static const int W[3] = {2, 4, 3};

static int flips;                 /* FLIPS=1: b may be mirrored (H, V, both) */
static int pw[64];                /* per pixel weight (BORDER) */
static int percept, dcw;
static float lab[8][8][8][3];
static float srgb_lin(float v) { return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f); }
static void oklab_init(void)
{
    float k = getenv("CHROMA") ? sqrtf(atof(getenv("CHROMA"))) : 1.0f;
    for (int R = 0; R < 8; R++) for (int G = 0; G < 8; G++) for (int Bb = 0; Bb < 8; Bb++) {
        float r = srgb_lin(R * 36 / 255.0f), g = srgb_lin(G * 36 / 255.0f), b = srgb_lin(Bb * 36 / 255.0f);
        float l = cbrtf(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
        float m = cbrtf(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
        float s = cbrtf(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
        float *o = lab[R][G][Bb];
        o[0] = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
        o[1] = k * (1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s);
        o[2] = k * (0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s);
    }
}
static uint8_t best_flip;

static uint32_t d1(const uint8_t *p, const uint8_t *q, int f)
{
    uint32_t s = 0;
    if (dcw && !percept) {               /* mean colour (opaque, 0..7 levels) of each tile */
        int sa[3] = {0, 0, 0}, sb[3] = {0, 0, 0};
        for (int i = 0; i < 64; i++)
            for (int c = 0; c < 3; c++) { int a = p[i * 3 + c], b = q[i * 3 + c]; sa[c] += a > 7 ? 0 : a; sb[c] += b > 7 ? 0 : b; }
        for (int c = 0; c < 3; c++) { int e = sa[c] - sb[c]; s += (uint32_t)(dcw * W[c] * e * e / 64); }
    }
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int qx = f & 1 ? 7 - x : x, qy = f & 2 ? 7 - y : y;
            const uint8_t *a = p + (y * 8 + x) * 3, *b = q + (qy * 8 + qx) * 3;
            if (percept) {
                if (a[0] > 7 || b[0] > 7) { if (a[0] != b[0] || a[1] != b[1] || a[2] != b[2]) s += pw[y * 8 + x] * 5000; continue; }
                const float *la = lab[a[0]][a[1]][a[2]], *lb = lab[b[0]][b[1]][b[2]];
                float d0 = la[0] - lb[0], d1_ = la[1] - lb[1], d2 = la[2] - lb[2];
                s += (uint32_t)(pw[y * 8 + x] * 10000.0f * (d0 * d0 + d1_ * d1_ + d2 * d2) + 0.5f);
                continue;
            }
            for (int c = 0; c < 3; c++) { int e = a[c] - b[c]; s += pw[y * 8 + x] * W[c] * e * e; }
        }
    return s;
}

static uint32_t d(uint32_t a, uint32_t b)
{
    const uint8_t *p = px + a * 192, *q = px + b * 192;
    uint32_t s = d1(p, q, 0);
    best_flip = 0;
    for (int f = 1; f < (flips ? 4 : 1); f++) { uint32_t e = d1(p, q, f); if (e < s) { s = e; best_flip = f; } }
    return s;
}

int main(void)
{
    flips = getenv("FLIPS") != 0;
    percept = getenv("PERCEPT") && atoi(getenv("PERCEPT"));
    dcw = getenv("DCW") ? atoi(getenv("DCW")) : 0;
    if (percept) oklab_init();
    {
        int b = getenv("BORDER") ? atoi(getenv("BORDER")) : 1;
        for (int i = 0; i < 64; i++) pw[i] = (i % 8 == 0 || i % 8 == 7 || i < 8 || i >= 56) ? b : 1;
    }
    if (fread(&n, 4, 1, stdin) != 1 || fread(&k, 4, 1, stdin) != 1 || fread(&T, 4, 1, stdin) != 1) return 1;
    w = malloc(n * 4); px = malloc(n * 192); med = malloc(k * 4); who = malloc(n * 4);
    uint64_t *near = malloc(n * 8);
    for (uint32_t i = 0; i < n; i++)
        if (fread(&w[i], 4, 1, stdin) != 1 || fread(px + i * 192, 192, 1, stdin) != 1) return 1;
    if (!k) {                       /* threshold mode */
        uint32_t *ord = malloc(n * 4), nl = 0;
        med = realloc(med, n * 4);
        for (uint32_t i = 0; i < n; i++) ord[i] = i;
        for (uint32_t i = 1; i < n; i++) {           /* by weight, heaviest first */
            uint32_t v = ord[i], j = i;
            while (j && w[ord[j - 1]] < w[v]) { ord[j] = ord[j - 1]; j--; }
            ord[j] = v;
        }
        for (uint32_t a = 0; a < n; a++) {
            uint32_t i = ord[a], bd = ~0u, bm = 0;
            for (uint32_t m = 0; m < nl; m++) { uint32_t e = d(i, med[m]); if (e < bd) { bd = e; bm = m; } }
            if (nl && bd <= T) who[i] = med[bm];
            else { med[nl++] = i; who[i] = i; }
        }
        fwrite(who, 4, n, stdout);
        fprintf(stderr, "tilemerge: %u tiles -> %u (T %u)\n", n, nl, T);
        return 0;
    }
    if (k > n) k = n;
    uint32_t b = 0;
    for (uint32_t i = 1; i < n; i++) if (w[i] > w[b]) b = i;
    med[0] = b;
    for (uint32_t i = 0; i < n; i++) near[i] = d(i, b);
    for (uint32_t m = 1; m < k; m++) {
        uint64_t best = 0; b = 0;
        for (uint32_t i = 0; i < n; i++) if (near[i] * w[i] > best) { best = near[i] * w[i]; b = i; }
        med[m] = b;
        for (uint32_t i = 0; i < n; i++) { uint64_t e = d(i, b); if (e < near[i]) near[i] = e; }
    }
    for (int it = 0; it < 10; it++) {
        for (uint32_t i = 0; i < n; i++) {
            uint32_t bm = 0, bd = ~0u;
            for (uint32_t m = 0; m < k; m++) { uint32_t e = d(i, med[m]); if (e < bd) { bd = e; bm = m; } }
            who[i] = bm;
        }
        int changed = 0;
        for (uint32_t m = 0; m < k; m++) {
            uint64_t bc = ~0ull; uint32_t bi = med[m];
            for (uint32_t i = 0; i < n; i++) {
                if (who[i] != m) continue;
                uint64_t c = 0;
                for (uint32_t j = 0; j < n; j++) if (who[j] == m) c += (uint64_t)d(i, j) * w[j];
                if (c < bc) { bc = c; bi = i; }
            }
            if (bi != med[m]) { med[m] = bi; changed = 1; }
        }
        if (!changed) break;
    }
    for (uint32_t i = 0; i < n; i++) {
        uint32_t bm = 0, bd = ~0u, bf = 0;
        for (uint32_t m = 0; m < k; m++) { uint32_t e = d(i, med[m]); if (e < bd) { bd = e; bm = m; bf = best_flip; } }
        uint32_t o = med[bm] | bf << 30;      /* bit 30: H mirror, bit 31: V mirror */
        fwrite(&o, 4, 1, stdout);
    }
    return 0;
}
