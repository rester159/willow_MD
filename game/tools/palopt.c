/* palopt: scenery palette optimiser for stagepack.py (art pass, 2026-11-04).
 *
 * Two CRAM lines of K colours each are shared by a stage's scenery tiles;
 * every tile pattern uses one line. Cost = sum over patterns of its pixels'
 * perceived error (OKLab, each pixel shown as the line's nearest colour)
 * + LAMBDA * the seams it makes with its map neighbours: for pixel pairs
 * across a tile border, how far the shown colour step differs from the
 * source's (a border that was smooth in the arcade stays smooth, an edge it
 * had stays). Search: each line entry in turn tries all 512 Genesis colours;
 * tiles re-pick their line (best error, then two ICM passes with the seam
 * term). Several sweeps until nothing improves.
 *
 * stdin (little endian):
 *   u32 K, u32 NC, NC x u16 colour (Genesis 9-bit: r | g << 3 | b << 6)
 *   u32 NP; per pattern: f32 weight, i32 pin (-1, 0, 1; + 0x100: the pattern
 *   may use KA entries, see KA), u32 nh, nh x (u32 colour index, f32 count)
 *   u32 NE; per edge: u32 pattern a, u32 pattern b, f32 count, u32 colour index p (a side), u32 q (b side)
 *   2 x K u16 initial lines (Genesis 9-bit)
 * stdout: L x K u16 lines, then NP x u8 line per pattern.
 * LINES=L (env, default 2, at most 4): how many lines (full-screen pictures: 4).
 * KA=n (env, n > K): lines hold n entries; patterns flagged 0x100 (seen only
 * where the palette cycle is off: stagepack CYCLE_FROM) may use all n, the
 * others the first K (the cycle owns pens K+1.. where they show).
 * cc -O2 -o tools/palopt tools/palopt.c -lm */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float lab[512][3];
static float D[512][512];           /* squared OKLab distance */
static float DS[512][512];          /* OKLab distance */
static unsigned K, NC, NP, NE;
static unsigned short *col;
typedef struct { float w; int pin, wide; unsigned nh, h0; unsigned near; } Pat;
static unsigned KA;                     /* entries per line for "wide" patterns (KA env; default K) */
static Pat *pat;
static unsigned *hc; static float *hn;
typedef struct { unsigned a, b, p, q; float n; } Edge;
static Edge *edge;
static unsigned *adj0, *adjn, *adjl;    /* per pattern: its edges */
static unsigned short line[4][64];
static int NL = 2;                      /* lines (LINES) */
static unsigned char *side, *bestside;
static float lambda;

static float srgb_lin(float v) { return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f); }

static void oklab(int c, float *o)
{
    /* Genesis level 0..7 -> 0..252 (the console's DAC is close to linear steps) */
    float r = srgb_lin((c & 7) * 36 / 255.0f), g = srgb_lin(((c >> 3) & 7) * 36 / 255.0f), b = srgb_lin(((c >> 6) & 7) * 36 / 255.0f);
    float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
    float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
    float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;
    l = cbrtf(l); m = cbrtf(m); s = cbrtf(s);
    o[0] = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    o[1] = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    o[2] = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
}

static unsigned char shownk[2][4][512]; /* [wide][line]: source colour index -> entry index */
#define shown shownk[0]

static void map_line(int l)
{
    for (int wd = 0; wd < 2; wd++) {
        unsigned n = wd ? KA : K;
        for (unsigned i = 0; i < NC; i++) {
            unsigned best = 0; float bd = 1e30f;
            for (unsigned j = 0; j < n; j++) { float d = D[col[i]][line[l][j]]; if (d < bd) { bd = d; best = j; } }
            shownk[wd][l][i] = best;
        }
    }
}

/* POW=k (default 1): a tile's mean error counts to the power k, so no
 * area (e.g. a meadow seen at one spot) goes badly wrong to save a little
 * everywhere else; scaled to equal POW 1 at a mean error of 0.01. */
static float errpow = 1.0f;
static float pat_err(unsigned p, int l)
{
    float e = 0, n = 0;
    for (unsigned k = 0; k < pat[p].nh; k++) {
        unsigned c = hc[pat[p].h0 + k];
        e += hn[pat[p].h0 + k] * D[col[c]][line[l][shownk[pat[p].wide][l][c]]];
        n += hn[pat[p].h0 + k];
    }
    if (errpow != 1.0f && n > 0) e = n * powf(e / n, errpow) / powf(0.01f, errpow - 1.0f);
    return e * pat[p].w;
}

/* The border's gradient as shown against the source's (OKLab vectors): an
 * edge the arcade did not have, or one it had and lost, both count. */
static float edge_seam(const Edge *e, int sa, int sb)
{
    const float *oa = lab[line[sa][shownk[pat[e->a].wide][sa][e->p]]], *ob = lab[line[sb][shownk[pat[e->b].wide][sb][e->q]]];
    const float *sa_ = lab[col[e->p]], *sb_ = lab[col[e->q]];
    float d0 = (oa[0] - ob[0]) - (sa_[0] - sb_[0]), d1 = (oa[1] - ob[1]) - (sa_[1] - sb_[1]), d2 = (oa[2] - ob[2]) - (sa_[2] - sb_[2]);
    return (d0 * d0 + d1 * d1 + d2 * d2) * e->n;           /* squared, as the colour error */
}

static float *perr[4];

static float total(void)
{
    float t = 0;
    for (unsigned p = 0; p < NP; p++) t += perr[side[p]][p];
    for (unsigned i = 0; i < NE; i++) t += lambda * edge_seam(&edge[i], side[edge[i].a], side[edge[i].b]);
    return t;
}

/* Lines changed: every pattern's error per line, its side (best error,
 * then ICM with the seam term), the total. */
static float evaluate(void)
{
    for (int l = 0; l < NL; l++) map_line(l);
    for (unsigned p = 0; p < NP; p++) {
        int bs = 0;
        for (int l = 0; l < NL; l++) { perr[l][p] = pat_err(p, l); if (perr[l][p] < perr[bs][p]) bs = l; }
        side[p] = pat[p].pin >= 0 ? pat[p].pin : bs;
    }
    for (int it = 0; it < 2 && lambda > 0; it++)
        for (unsigned p = 0; p < NP; p++) {
            if (pat[p].pin >= 0) continue;
            float c[4];
            for (int s = 0; s < NL; s++) {
                c[s] = perr[s][p];
                for (unsigned k = 0; k < adjn[p]; k++) {
                    const Edge *e = &edge[adjl[adj0[p] + k]];
                    if (e->a == p && e->b == p) c[s] += lambda * edge_seam(e, s, s);
                    else if (e->a == p) c[s] += lambda * edge_seam(e, s, side[e->b]);
                    else c[s] += lambda * edge_seam(e, side[e->a], s);
                }
            }
            { int bs = 0; for (int s = 1; s < NL; s++) if (c[s] < c[bs]) bs = s; side[p] = bs; }
        }
    return total();
}

static unsigned rd(void) { unsigned v; if (fread(&v, 4, 1, stdin) != 1) exit(2); return v; }
static float rdf(void) { float v; if (fread(&v, 4, 1, stdin) != 1) exit(2); return v; }

int main(void)
{
    lambda = getenv("LAMBDA") ? atof(getenv("LAMBDA")) : 1.0f;
    if (getenv("POW")) errpow = atof(getenv("POW"));
    int sweeps = getenv("SWEEPS") ? atoi(getenv("SWEEPS")) : 4;
    for (int c = 0; c < 512; c++) oklab(c, lab[c]);
    {   /* CHROMA=k: hue/colourfulness errors weigh k times lightness errors
         * (a cream highlight turned pale cyan reads worse than one a shade
         * darker); scaling a and b by sqrt(k) does it for every distance. */
        float k = getenv("CHROMA") ? sqrtf(atof(getenv("CHROMA"))) : 1.0f;
        for (int c = 0; c < 512; c++) { lab[c][1] *= k; lab[c][2] *= k; }
    }
    for (int a = 0; a < 512; a++)
        for (int b = 0; b < 512; b++) {
            float d0 = lab[a][0] - lab[b][0], d1 = lab[a][1] - lab[b][1], d2 = lab[a][2] - lab[b][2];
            D[a][b] = d0 * d0 + d1 * d1 + d2 * d2; DS[a][b] = sqrtf(D[a][b]);
        }
    K = rd(); NC = rd();
    col = malloc(NC * 2);
    if (fread(col, 2, NC, stdin) != NC) return 2;
    NP = rd();
    pat = calloc(NP, sizeof *pat);
    unsigned cap = 1 << 16, nhs = 0;
    hc = malloc(cap * 4); hn = malloc(cap * 4);
    for (unsigned p = 0; p < NP; p++) {
        pat[p].w = rdf(); pat[p].pin = (int)rd(); pat[p].nh = rd(); pat[p].h0 = nhs;
        if (pat[p].pin >= 0x80) { pat[p].wide = 1; pat[p].pin -= 0x100; }
        for (unsigned k = 0; k < pat[p].nh; k++) {
            if (nhs == cap) { cap *= 2; hc = realloc(hc, cap * 4); hn = realloc(hn, cap * 4); }
            hc[nhs] = rd(); hn[nhs] = rdf(); nhs++;
        }
    }
    NE = rd();
    edge = malloc((NE ? NE : 1) * sizeof *edge);
    for (unsigned i = 0; i < NE; i++) { edge[i].a = rd(); edge[i].b = rd(); edge[i].n = rdf(); edge[i].p = rd(); edge[i].q = rd(); }
    KA = getenv("KA") ? atoi(getenv("KA")) : K;
    if (KA < K) KA = K;
    NL = getenv("LINES") ? atoi(getenv("LINES")) : 2;
    if (NL < 1 || NL > 4) NL = 2;
    for (int l = 0; l < NL; l++) if (fread(line[l], 2, KA, stdin) != KA) return 2;
    /* PIN="rgb[,rgb]": entry 0 of line 0 (and 1) fixed to that Genesis
     * colour (digits 0-7), e.g. a dark neutral both lines keep for outlines. */
    int pinned[4] = { 0, 0, 0, 0 };
    if (getenv("PIN")) {
        const char *p = getenv("PIN");
        /* "rgb+rgb,..." pins entries 0, 1, ... of a line (art pass). */
        for (int l = 0; l < 2 && p[0] && p[1] && p[2]; l++) {
            for (;;) {
                line[l][pinned[l]] = (p[0] - '0') | (p[1] - '0') << 3 | (p[2] - '0') << 6; pinned[l]++;
                p += 3;
                if (*p != '+' || !p[1] || !p[2] || !p[3]) break;
                p++;
            }
            if (*p == ',') p++;
        }
    }
    adj0 = calloc(NP + 1, 4); adjn = calloc(NP, 4); adjl = malloc((2 * NE + 1) * 4);
    for (unsigned i = 0; i < NE; i++) { adjn[edge[i].a]++; if (edge[i].b != edge[i].a) adjn[edge[i].b]++; }
    for (unsigned p = 0; p < NP; p++) adj0[p + 1] = adj0[p] + adjn[p];
    memset(adjn, 0, NP * 4);
    for (unsigned i = 0; i < NE; i++) {
        adjl[adj0[edge[i].a] + adjn[edge[i].a]++] = i;
        if (edge[i].b != edge[i].a) adjl[adj0[edge[i].b] + adjn[edge[i].b]++] = i;
    }
    side = malloc(NP); bestside = malloc(NP);
    for (int l = 0; l < 4; l++) perr[l] = malloc(NP * 4);
    float best = evaluate();
    fprintf(stderr, "palopt: %u colours, %u patterns, %u edge pairs, K %u, lambda %.2f: start %.1f\n", NC, NP, NE, K, lambda, best);
    /* Restarts (RESTARTS=n): from the best lines so far, a few entries per
     * line jump to random source colours, then the sweeps run again; the
     * best result is kept. */
    int restarts = getenv("RESTARTS") ? atoi(getenv("RESTARTS")) : 0;
    unsigned short keepl[4][64]; float keepb = 1e30f;
    unsigned seed = 12345;
    for (int rs = 0; rs <= restarts; rs++) {
    if (rs) {
        memcpy(line, keepl, sizeof line);
        for (int l = 0; l < NL; l++)
            for (int k = 0; k < 3; k++) {
                seed = seed * 1103515245u + 12345u;
                unsigned j = (seed >> 8) % KA;
                if (j < (unsigned)pinned[l]) j = pinned[l];
                seed = seed * 1103515245u + 12345u;
                line[l][j] = col[(seed >> 8) % NC];
            }
        best = evaluate();
    }
    for (int sw = 0; sw < sweeps; sw++) {
        int improved = 0;
        for (int l = 0; l < NL; l++)
            for (unsigned j = 0; j < KA; j++) {
                if (j < (unsigned)pinned[l]) continue;
                unsigned short keep = line[l][j], bc = keep;
                for (int c = 0; c < 512; c++) {
                    if (c == keep) continue;
                    line[l][j] = c;
                    float t = evaluate();
                    if (t < best * (1 - 1e-6f)) { best = t; bc = c; improved = 1; }
                }
                line[l][j] = bc;
            }
        evaluate();
        fprintf(stderr, "palopt: restart %d sweep %d: %.3f\n", rs, sw, best);
        if (!improved) break;
    }
    if (best < keepb) { keepb = best; memcpy(keepl, line, sizeof line); }
    }
    memcpy(line, keepl, sizeof line);
    best = evaluate();
    fprintf(stderr, "palopt: best %.3f\n", best);
    for (int l = 0; l < NL; l++) fwrite(line[l], 2, KA, stdout);
    fwrite(side, 1, NP, stdout);
    return 0;
}
