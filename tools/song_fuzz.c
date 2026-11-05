/* Licence: TBD (see D017) */
/* song_fuzz: the device's song parser (firmware/app/song.c) on the laptop, under ASan/UBSan (tools/song_fuzz.sh).
 *
 *   song_fuzz SECONDS SEED.fm1s...     mutate the seeds for SECONDS, open each result, and walk every accessor of
 *                                      the ones the parser accepts; any out-of-bounds read or UB aborts
 *   song_fuzz --diff FILE...           print "<rc> FILE" per file: tools/fm1song's tests compare it with the
 *                                      Python reader (the two must accept and refuse the same files)
 * Each mutated file is a malloc of exactly its size, so a read one byte past it is caught. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../firmware/app/song.h"

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(uint32_t n)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return n ? (uint32_t)(rng % n) : 0;
}

static uint8_t *load(const char *path, uint32_t *n)
{
    FILE *f = fopen(path, "rb");
    long len;
    uint8_t *d;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = malloc(len > 0 ? (size_t)len : 1u);
    if (len > 0 && fread(d, 1, (size_t)len, f) != (size_t)len)
        len = 0;
    fclose(f);
    *n = (uint32_t)len;
    return d;
}

static void fix_crc(uint8_t *d, uint32_t n)
{
    static const uint8_t zero[4] = {0, 0, 0, 0};
    uint32_t crc;
    if (n < 16)
        return;
    crc = song_crc32(zero, 4, song_crc32(d, 12, 0));
    crc = song_crc32(d + 16, n - 16, crc);
    d[12] = (uint8_t)crc;
    d[13] = (uint8_t)(crc >> 8);
    d[14] = (uint8_t)(crc >> 16);
    d[15] = (uint8_t)(crc >> 24);
}

/* the length too: header field 8 follows the new size half the time, so size changes get past the size check */
static uint8_t *mutate(const uint8_t *src, uint32_t n, uint32_t *out_n)
{
    uint32_t cap = n + 64, len = n, k, edits = 1 + rnd(8);
    uint8_t *d = malloc(cap);
    memcpy(d, src, n);
    for (k = 0; k < edits && len; k++) {
        uint32_t i = rnd(len), op = rnd(100), m;
        if (op < 45)
            d[i] = (uint8_t)rnd(256);
        else if (op < 60) {
            static const uint8_t V[] = {0, 1, 2, 3, 4, 0x7F, 0x80, 0xFE, 0xFF};
            d[i] = V[rnd(sizeof V)];
        } else if (op < 72) {           /* a field near a boundary: bump a 16- or 32-bit number by +-1 */
            d[i] = (uint8_t)(d[i] + (rnd(2) ? 1 : 0xFF));
        } else if (op < 82) {           /* delete */
            m = 1 + rnd(16);
            if (m > len - i)
                m = len - i;
            memmove(d + i, d + i + m, len - i - m);
            len -= m;
        } else if (op < 90 && len + 16 <= cap) {   /* insert */
            m = 1 + rnd(16);
            memmove(d + i + m, d + i, len - i);
            for (uint32_t j = 0; j < m; j++)
                d[i + j] = (uint8_t)rnd(256);
            len += m;
        } else {                        /* copy 4 bytes from elsewhere */
            uint32_t j = rnd(len);
            for (m = 0; m < 4 && i + m < len && j + m < len; m++)
                d[i + m] = d[j + m];
        }
    }
    if (len >= 12 && rnd(2)) {
        d[8] = (uint8_t)len;
        d[9] = (uint8_t)(len >> 8);
        d[10] = (uint8_t)(len >> 16);
        d[11] = (uint8_t)(len >> 24);
    }
    {
        uint8_t *exact = malloc(len ? len : 1u);   /* exactly len bytes: ASan sees any read past the end */
        memcpy(exact, d, len);
        free(d);
        *out_n = len;
        return exact;
    }
}

static volatile uint32_t sink;

static void walk(const struct song *s)
{
    uint32_t l, v, i, b;
    struct song_event e;
    for (l = 0; l < SONG_LAYERS; l++)
        for (v = 0; v < s->layer[l].variants; v++) {
            const struct song_stream *st = &s->layer[l].var[v];
            for (i = 0; i < st->count; i++) {
                song_event_get(st, i, &e);
                sink += e.tick + e.status + e.d1 + e.d2;
            }
            sink += song_seek(st, rnd(s->end_tick + 2));
            sink += song_seek(st, 0) + song_seek(st, s->end_tick);
        }
    for (i = 0; i < s->ncevt; i++) {
        const uint8_t *vo = song_cevt_voicing(s, i, 1 + (i & 1));
        sink += song_cevt_tick(s, i) + (uint32_t)strlen(song_cevt_symbol(s, i)) + vo[0] + vo[4];
    }
    for (b = 0; b < s->bar_count; b++) {
        uint32_t c = song_bar_chord(s, b), sec = song_bar_section(s, b);
        sink += c + sec + song_bar_flags(s, b) + (uint32_t)strlen(song_cevt_symbol(s, c));
        sink += (uint32_t)strlen(song_section_name(s, sec));
    }
    for (i = 0; i < s->nsect; i++)
        sink += song_section_first(s, i) + song_section_bars(s, i) + song_section_lead(s, i);
    for (i = 0; i < 16; i++)
        sink += song_chord_at(s, rnd(s->end_tick + 100));
    sink += (uint32_t)strlen(s->title) + (uint32_t)strlen(s->slug) + (uint32_t)strlen(s->key);
}

int main(int argc, char **argv)
{
    struct song s;
    uint8_t *seed[16];
    uint32_t seed_n[16], nseeds = 0, i;
    unsigned long long runs = 0, accepted = 0, per_err[16] = {0};
    time_t until;
    if (argc >= 3 && !strcmp(argv[1], "--diff")) {
        for (i = 2; i < (uint32_t)argc; i++) {
            uint32_t n;
            uint8_t *d = load(argv[i], &n);
            int r = d ? song_open(&s, d, n) : -99;
            if (r == SONG_OK)
                walk(&s);
            printf("%d %s\n", r, argv[i]);
            free(d);
        }
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "usage: song_fuzz SECONDS SEED.fm1s... | song_fuzz --diff FILE...\n");
        return 2;
    }
    for (i = 2; i < (uint32_t)argc && nseeds < 16; i++) {
        seed[nseeds] = load(argv[i], &seed_n[nseeds]);
        if (!seed[nseeds] || song_open(&s, seed[nseeds], seed_n[nseeds]) != SONG_OK) {
            fprintf(stderr, "%s: not a valid seed\n", argv[i]);
            return 2;
        }
        walk(&s);
        nseeds++;
    }
    rng ^= (uint64_t)time(0);
    until = time(0) + atol(argv[1]);
    while (time(0) < until) {
        for (int k = 0; k < 2000; k++) {
            uint32_t w = rnd(nseeds), n;
            uint8_t *d = mutate(seed[w], seed_n[w], &n);
            int r;
            if (rnd(4))                   /* mostly past the CRC, into the structure checks */
                fix_crc(d, n);
            r = song_open(&s, d, n);
            if (r == SONG_OK) {
                accepted++;
                walk(&s);
            }
            per_err[-r < 16 && r <= 0 ? -r : 15]++;
            runs++;
            free(d);
        }
    }
    printf("song_fuzz: %llu files, %llu accepted, by result:", runs, accepted);
    for (i = 0; i < 16; i++)
        if (per_err[i])
            printf(" %s %llu", song_error_name(-(int)i), per_err[i]);
    printf("\n");
    for (i = 0; i < nseeds; i++)
        free(seed[i]);
    return 0;
}
