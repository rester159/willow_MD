/* poolmerge: merge a stage's foreground tiles only where VRAM needs it
 * (art pass, 2026-11-04; replaces the global FG_MERGE threshold of cut 5).
 *
 * The streamed pool holds the tiles of a window of columns; a tile's uses
 * form runs along the stage (a gap wider than WINDOW ends a run) and the
 * pool size is the most runs live at one column (each run lives WINDOW
 * columns past its end). While that peak is over the budget: at the
 * busiest column, merge the cheapest pair of tiles both live there (the
 * merged-away tile's uses take the other, possibly mirrored), weighted by
 * how often the tile is used (to the power WPOW, default 0.5). Cost: perceived (OKLab) squared error per
 * pixel, the tile's outer ring counting BORDER times (default 2), so
 * merged tiles keep their edges and tile borders do not show. Columns that
 * fit keep every tile exact.
 *
 * stdin (little endian): u32 N, u32 budget, u32 window; per tile: u32 weight
 * (bit 31: fixed, never merged either way, e.g. cycling water; bit 30: kept,
 * others may merge into it, it never merges away),
 * 192 bytes (r, g, b 0..7 per pixel, row-major; transparent = 255), u32 nc,
 * nc x u32 column (sorted).
 * LOADCAP_FILE=path (u16 big-endian per column: the most new tiles the
 * 4 columns from it may load; a reference pack's own counts) and LOADCAP_FROM
 * (first column checked, default 44: the stage start loads at once): after
 * the pool fits, merge further where a 4-column stretch loads more tiles
 * than that (art pass: no stretch streams more than build 05r did).
 * stdout: N x u32 representative (bit 30 H mirror, bit 31 V mirror).
 * cc -O2 -o tools/poolmerge tools/poolmerge.c -lm */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KNN 48
static unsigned N, B, WIN, *w, *nc, **cols, *rep, *rflip, maxcol;
static float wpow = 0.5f;
static float dcw;                       /* DCW: a change of the tile's mean (perceived) colour x 64 x DCW: a whole tile shifting tone shows as a square */               /* WPOW: cost = error x uses^WPOW (one wrong tile shows even if used once) */
static unsigned char *px, *fixed, *keep;
static float lab[512][3];
static float pw[64];
static int *load;
static unsigned *knn; static float *knd; static unsigned char *knf;

static float srgb_lin(float v) { return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f); }
static void oklab(int c, float *o)
{
    float r = srgb_lin((c & 7) * 36 / 255.0f), g = srgb_lin(((c >> 3) & 7) * 36 / 255.0f), b = srgb_lin(((c >> 6) & 7) * 36 / 255.0f);
    float l = cbrtf(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    float m = cbrtf(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    float s = cbrtf(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
    o[0] = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    o[1] = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    o[2] = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
}

static int code(const unsigned char *p) { return p[0] == 255 ? -1 : p[0] | p[1] << 3 | p[2] << 6; }

static float relax;

/* a shown as b mirrored by f: cost per a's pixel (transparency must match). */
static float d1(unsigned a, unsigned b, int f)
{
    const unsigned char *p = px + a * 192, *q = px + b * 192;
    float s = 0, ma[3] = {0, 0, 0}, mb[3] = {0, 0, 0};
    int nop = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int qx = f & 1 ? 7 - x : x, qy = f & 2 ? 7 - y : y;
            int ca = code(p + (y * 8 + x) * 3), cb = code(q + (qy * 8 + qx) * 3);
            if ((ca < 0) != (cb < 0)) {
                if (!relax) return 1e30f;
                s += relax * pw[y * 8 + x];     /* with kept tiles: an added tile may lose an edge pixel */
                continue;
            }
            if (ca < 0) continue;
            float d0 = lab[ca][0] - lab[cb][0], e1 = lab[ca][1] - lab[cb][1], e2 = lab[ca][2] - lab[cb][2];
            s += pw[y * 8 + x] * (d0 * d0 + e1 * e1 + e2 * e2);
            for (int k = 0; k < 3; k++) { ma[k] += lab[ca][k]; mb[k] += lab[cb][k]; }
            nop++;
        }
    if (dcw && nop) {
        float e = 0;
        for (int k = 0; k < 3; k++) { float d = (ma[k] - mb[k]) / nop; e += d * d; }
        s += dcw * 64 * e;
    }
    return s;
}

static float dist(unsigned a, unsigned b, unsigned char *fl)
{
    float s = d1(a, b, 0); *fl = 0;
    for (int f = 1; f < 4; f++) { float e = d1(a, b, f); if (e < s) { s = e; *fl = f; } }
    return s;
}

/* Runs of a column list -> load += sign over [start, end + WIN]. */
static void runs_add(const unsigned *c, unsigned n, int sign)
{
    if (!n) return;
    unsigned s = c[0], e = c[0];
    for (unsigned i = 1; i <= n; i++) {
        if (i == n || c[i] - e > WIN) {
            unsigned hi = e + WIN; if (hi > maxcol) hi = maxcol;
            for (unsigned k = s; k <= hi; k++) load[k] += sign;
            if (i < n) s = c[i];
        }
        if (i < n) e = c[i];
    }
}

static int live_at(unsigned t, unsigned col)
{
    unsigned n = nc[t]; const unsigned *c = cols[t];
    if (!n) return 0;
    unsigned s = c[0], e = c[0];
    for (unsigned i = 1; i <= n; i++) {
        if (i == n || c[i] - e > WIN) {
            if (col >= s && col <= e + WIN) return 1;
            if (i < n) s = c[i];
        }
        if (i < n) e = c[i];
    }
    return 0;
}

static unsigned *merge_cols(const unsigned *a, unsigned na, const unsigned *b, unsigned nb, unsigned *n)
{
    unsigned *o = malloc((na + nb) * 4), i = 0, j = 0, k = 0;
    while (i < na || j < nb) {
        unsigned v = (j >= nb || (i < na && a[i] < b[j])) ? a[i++] : b[j++];
        if (!k || o[k - 1] != v) o[k++] = v;
    }
    *n = k;
    return o;
}

static unsigned root(unsigned t, unsigned *fl)
{
    unsigned f = 0;
    while (rep[t] != t) { f ^= rflip[t]; t = rep[t]; }
    *fl = f;
    return t;
}

int main(void)
{
    float bw = getenv("BORDER") ? atof(getenv("BORDER")) : 2.0f;
    if (getenv("WPOW")) wpow = atof(getenv("WPOW"));
    if (getenv("DCW")) dcw = atof(getenv("DCW"));
    for (int i = 0; i < 64; i++) pw[i] = (i % 8 == 0 || i % 8 == 7 || i < 8 || i >= 56) ? bw : 1.0f;
    for (int c = 0; c < 512; c++) oklab(c, lab[c]);
    {   /* CHROMA=k: hue/colourfulness errors weigh k times lightness errors
         * (a cream highlight turned pale cyan reads worse than one a shade
         * darker); scaling a and b by sqrt(k) does it for every distance. */
        float k = getenv("CHROMA") ? sqrtf(atof(getenv("CHROMA"))) : 1.0f;
        for (int c = 0; c < 512; c++) { lab[c][1] *= k; lab[c][2] *= k; }
    }
    if (fread(&N, 4, 1, stdin) != 1 || fread(&B, 4, 1, stdin) != 1 || fread(&WIN, 4, 1, stdin) != 1) return 2;
    w = malloc(N * 4); nc = malloc(N * 4); cols = malloc(N * sizeof *cols); px = malloc(N * 192);
    rep = malloc(N * 4); rflip = calloc(N, 4); fixed = calloc(N, 1); keep = calloc(N, 1);
    for (unsigned i = 0; i < N; i++) {
        if (fread(&w[i], 4, 1, stdin) != 1 || fread(px + i * 192, 192, 1, stdin) != 1 || fread(&nc[i], 4, 1, stdin) != 1) return 2;
        cols[i] = malloc((nc[i] ? nc[i] : 1) * 4);
        if (nc[i] && fread(cols[i], 4, nc[i], stdin) != nc[i]) return 2;
        if (nc[i] && cols[i][nc[i] - 1] > maxcol) maxcol = cols[i][nc[i] - 1];
        rep[i] = i;
        if (w[i] >> 31) fixed[i] = 1;
        if (w[i] & 0x40000000) keep[i] = 1;
        w[i] &= 0x3FFFFFFF;
    }
    for (unsigned i = 0; i < N; i++) if (keep[i]) relax = 0.5f;
    maxcol += WIN + 1;
    load = calloc(maxcol + 1, sizeof *load);
    for (unsigned i = 0; i < N; i++) runs_add(cols[i], nc[i], 1);
    int peak = 0; for (unsigned k = 0; k <= maxcol; k++) if (load[k] > peak) peak = load[k];
    fprintf(stderr, "poolmerge: %u tiles, peak %d, budget %u\n", N, peak, B);
    /* Each tile's nearest others (by cost), for the merge search. */
    knn = malloc((size_t)N * KNN * 4); knd = malloc((size_t)N * KNN * 4); knf = malloc((size_t)N * KNN);
    for (unsigned a = 0; a < N; a++) {
        unsigned n = 0;
        for (unsigned b = 0; b < N; b++) {
            if (b == a) continue;
            unsigned char f; float d = dist(a, b, &f);
            if (d >= 1e29f) continue;
            if (n < KNN) n++;
            else if (d >= knd[a * KNN + KNN - 1]) continue;
            unsigned j = n - 1;
            while (j && knd[a * KNN + j - 1] > d) { knn[a * KNN + j] = knn[a * KNN + j - 1]; knd[a * KNN + j] = knd[a * KNN + j - 1]; knf[a * KNN + j] = knf[a * KNN + j - 1]; j--; }
            knn[a * KNN + j] = b; knd[a * KNN + j] = d; knf[a * KNN + j] = f;
        }
        for (unsigned j = n; j < KNN; j++) { knn[a * KNN + j] = a; knd[a * KNN + j] = 1e30f; }
    }
    unsigned merges = 0;
    double cost = 0;
    unsigned char *alive = malloc(N);
    unsigned char *tabu = NULL;
    int *saved = NULL;
    for (unsigned i = 0; i < N; i++) if (keep[i]) { tabu = calloc((size_t)N * N, 1); saved = malloc((maxcol + 2) * sizeof *saved); break; }
    while (peak > (int)B) {
        unsigned col = 0;
        for (unsigned k = 0; k <= maxcol; k++) if (load[k] > load[col]) col = k;
        memset(alive, 0, N);
        for (unsigned t = 0; t < N; t++) if (rep[t] == t && !fixed[t] && live_at(t, col)) alive[t] = 1;
        float best = 1e30f; unsigned ba = 0, bb = 0; unsigned char bf = 0;
        for (unsigned a = 0; a < N; a++) {
            if (!alive[a] || keep[a]) continue;
            for (unsigned j = 0; j < KNN; j++) {
                unsigned b = knn[a * KNN + j];
                if (b == a || knd[a * KNN + j] >= 1e29f) break;
                if (!alive[b] || (tabu && tabu[(size_t)a * N + b])) continue;
                float c = knd[a * KNN + j] * powf(w[a], wpow);
                if (c < best) { best = c; ba = a; bb = b; bf = knf[a * KNN + j]; }
                break;          /* the nearest live one */
            }
        }
        if (best >= 1e29f) {    /* no near pair: search the live set directly */
            for (unsigned a = 0; a < N; a++) {
                if (!alive[a] || keep[a]) continue;
                for (unsigned b = 0; b < N; b++) {
                    if (b == a || !alive[b] || (tabu && tabu[(size_t)a * N + b])) continue;
                    unsigned char f; float d = dist(a, b, &f) * powf(w[a], wpow);
                    if (d < best) { best = d; ba = a; bb = b; bf = f; }
                }
            }
            if (best >= 1e29f) { fprintf(stderr, "poolmerge: stuck at column %u (load %d)\n", col, load[col]); break; }
        }
        /* a -> b everywhere: a's columns join b's. */
        if (tabu) memcpy(saved, load, (maxcol + 1) * sizeof *load);
        runs_add(cols[ba], nc[ba], -1); runs_add(cols[bb], nc[bb], -1);
        unsigned n; unsigned *m = merge_cols(cols[ba], nc[ba], cols[bb], nc[bb], &n);
        if (tabu) {
            /* Merging into a kept tile lengthens its runs: refuse a merge that
             * fills a column it would push over the budget. */
            runs_add(m, n, 1);
            int bad = 0;
            for (unsigned k = 0; k <= maxcol; k++) if (load[k] > saved[k] && load[k] > (int)B) { bad = 1; break; }
            runs_add(m, n, -1);
            if (bad) {
                memcpy(load, saved, (maxcol + 1) * sizeof *load);
                free(m);
                tabu[(size_t)ba * N + bb] = 1;
                continue;
            }
        }
        free(cols[bb]); cols[bb] = m; nc[bb] = n; nc[ba] = 0;
        runs_add(cols[bb], nc[bb], 1);
        rep[ba] = bb; rflip[ba] = bf; w[bb] += w[ba];
        if (tabu) memset(tabu, 0, (size_t)N * N);   /* a refused merge may fit now */
        merges++; cost += best;
        peak = 0; for (unsigned k = 0; k <= maxcol; k++) if (load[k] > peak) peak = load[k];
    }
    fprintf(stderr, "poolmerge: %u merges, peak %d, cost %.2f\n", merges, peak, cost);
    if (getenv("LOADCAP_FILE")) {
        FILE *cf = fopen(getenv("LOADCAP_FILE"), "rb");
        unsigned from = getenv("LOADCAP_FROM") ? atoi(getenv("LOADCAP_FROM")) : 44, ncap = 0, extra = 0;
        unsigned short *cap = calloc(maxcol + 8, 2);
        unsigned char b2[2];
        while (cf && ncap < maxcol + 8 && fread(b2, 1, 2, cf) == 2) cap[ncap++] = b2[0] << 8 | b2[1];
        if (cf) fclose(cf);
        int *st = calloc(maxcol + 8, sizeof *st);
        unsigned char *skip = calloc(maxcol + 8, 1);   /* stretches nothing can fix */
        for (int guard = 0; guard < 20000; guard++) {
            memset(st, 0, (maxcol + 8) * sizeof *st);
            for (unsigned t = 0; t < N; t++) {       /* run starts */
                if (rep[t] != t || !nc[t]) continue;
                unsigned e = cols[t][0]; st[e]++;
                for (unsigned i = 1; i < nc[t]; i++) { if (cols[t][i] - e > WIN) st[cols[t][i]]++; e = cols[t][i]; }
            }
            int worst = 0; unsigned wc = 0;
            for (unsigned c = from; c + 4 <= ncap && c + 4 <= maxcol; c++) {
                int sum = st[c] + st[c + 1] + st[c + 2] + st[c + 3], over = sum - cap[c];
                if (!skip[c] && over > worst) { worst = over; wc = c; }
            }
            if (!worst) break;
            /* the cheapest merge removing a run start in [wc, wc+3]: a starts a run there, b is live at that column */
            float best = 1e30f; unsigned ba = 0, bb = 0; unsigned char bf = 0;
            for (unsigned a = 0; a < N; a++) {
                if (rep[a] != a || fixed[a] || !nc[a]) continue;
                unsigned sc = 0xFFFFFFFF, e = cols[a][0];
                if (e >= wc && e < wc + 4) sc = e;
                for (unsigned i = 1; i < nc[a] && sc == 0xFFFFFFFF; i++) { unsigned c = cols[a][i]; if (c - e > WIN && c >= wc && c < wc + 4) sc = c; e = c; }
                if (sc == 0xFFFFFFFF) continue;
                for (unsigned j = 0; j < KNN; j++) {
                    unsigned b = knn[a * KNN + j];
                    if (b == a || knd[a * KNN + j] >= 1e29f) break;
                    unsigned fl; unsigned r = root(b, &fl);
                    if (r == a || fixed[r] || !live_at(r, sc)) continue;
                    float c2 = knd[a * KNN + j] * powf(w[a], wpow);
                    if (c2 < best) { best = c2; ba = a; bb = r; bf = knf[a * KNN + j] ^ fl; }
                    break;
                }
            }
            if (best >= 1e29f) { fprintf(stderr, "poolmerge: load cap: nothing to merge at column %u (over by %d)\n", wc, worst); skip[wc] = 1; continue; }
            runs_add(cols[ba], nc[ba], -1); runs_add(cols[bb], nc[bb], -1);
            unsigned n; unsigned *m = merge_cols(cols[ba], nc[ba], cols[bb], nc[bb], &n);
            free(cols[bb]); cols[bb] = m; nc[bb] = n; nc[ba] = 0;
            runs_add(cols[bb], nc[bb], 1);
            rep[ba] = bb; rflip[ba] = bf; w[bb] += w[ba];
            extra++; cost += best;
        }
        fprintf(stderr, "poolmerge: load cap: %u more merges\n", extra);
    }
    for (unsigned i = 0; i < N; i++) {
        unsigned f, r = root(i, &f);
        unsigned o = r | f << 30;
        fwrite(&o, 4, 1, stdout);
    }
    return 0;
}
