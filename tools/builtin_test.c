/* SPDX-License-Identifier: GPL-3.0-or-later */
/* builtin_test: the built-in starter library (M10.4: firmware/app/builtin.c, builtin_gen.h by tools/builtin_pack.py) on the laptop under
 * ASan + UBSan (tools/builtin_test.sh).
 *
 *   builtin_test DIR [SECONDS]    DIR holds slug.fm1s for every library entry (compiled by fm1song, the files `send` sends).
 *   1. every built-in, unpacked into a buffer of exactly its raw length: equal to DIR/slug.fm1s byte for byte; its CRC-32 is the one the
 *      store would record; song_peek's fields equal the generated metadata; song_open succeeds and every level of every layer builds
 *   2. for SECONDS (default 30): random corruptions of the pack (bytes changed, cut, grown; lengths wild; the dictionary shortened):
 *      only error codes or a different output, never an ASan / UBSan report. Input, dictionary and output are each a malloc of exactly
 *      their size, so one byte outside is caught. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../firmware/app/song.h"
#include "builtin_gen.h"

static int failures;
#define CHECK(c, ...)                                                                                                \
    do {                                                                                                             \
        if (!(c)) {                                                                                                  \
            failures++;                                                                                              \
            printf("FAIL: " __VA_ARGS__);                                                                            \
            printf("\n");                                                                                            \
        }                                                                                                            \
    } while (0)

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

static int same(const char *a, const char *b) { return !strcmp(a ? a : "", b ? b : ""); }

static uint8_t dec[SONG_BUILD_BYTES > SONG_DECODE_BYTES ? SONG_BUILD_BYTES : SONG_DECODE_BYTES] __attribute__((aligned(4)));

static void check_all(const char *dir)
{
    uint32_t j;
    CHECK(BUILTIN_N > 0 && BUILTIN_MAX <= 2048u, "BUILTIN_N %u, BUILTIN_MAX %u", (unsigned)BUILTIN_N, (unsigned)BUILTIN_MAX);
    for (j = 0; j < BUILTIN_N; j++) {
        const struct bi_file *f = &bi_files[j];
        const struct lib_item *m = &bi_items[j];
        char path[512];
        uint32_t n = 0, l;
        uint8_t *want, *got, *in, *dict;
        struct song s;
        int r;
        snprintf(path, sizeof path, "%s/%s.fm1s", dir, f->slug);
        want = load(path, &n);
        CHECK(want != 0, "%s: no compiled file %s", f->slug, path);
        if (!want)
            continue;
        CHECK(n == f->rawlen && n <= BUILTIN_MAX, "%s: raw length %u, compiled file %u", f->slug, (unsigned)f->rawlen, (unsigned)n);
        got = malloc(f->rawlen);
        in = malloc(f->clen);
        dict = malloc(BUILTIN_DICT_LEN);
        memcpy(in, bi_pack + f->off, f->clen);
        memcpy(dict, bi_dict, BUILTIN_DICT_LEN);
        r = bi_unpack(dict, BUILTIN_DICT_LEN, in, f->clen, got, f->rawlen);
        CHECK(r == BI_OK, "%s: unpack -> %d", f->slug, r);
        CHECK(n == f->rawlen && !memcmp(got, want, n), "%s: the unpacked file differs from the compiled one", f->slug);
        CHECK(song_crc32(got, f->rawlen, 0) == f->crc, "%s: CRC-32 %08x, recorded %08x", f->slug,
              (unsigned)song_crc32(got, f->rawlen, 0), (unsigned)f->crc);
        r = song_peek(&s, got, f->rawlen);
        CHECK(r == SONG_OK, "%s: song_peek -> %d", f->slug, r);
        if (r == SONG_OK) {
            uint32_t bpm = (60000000u + s.tempo_us / 2u) / s.tempo_us;   /* player.c player_song_bpm */
            CHECK(same(s.slug, f->slug), "%s: slug in the file %s", f->slug, s.slug);
            CHECK(same(s.title, m->title), "%s: title %s / %s", f->slug, s.title, m->title);
            CHECK(same(s.key, m->key), "%s: key %s / %s", f->slug, s.key, m->key);
            CHECK(same(s.category, m->category), "%s: category %s / %s", f->slug, s.category, m->category);
            CHECK(bpm == m->bpm, "%s: bpm %u / %u", f->slug, (unsigned)bpm, (unsigned)m->bpm);
            CHECK(s.beats_per_bar == m->beats_per_bar, "%s: beats %u / %u", f->slug, (unsigned)s.beats_per_bar,
                  (unsigned)m->beats_per_bar);
            CHECK(same(s.numerals, m->numerals), "%s: numerals %s / %s", f->slug, s.numerals, m->numerals);
            CHECK(s.kind == m->kind && s.bt_number == m->number && s.scale == m->scale && s.key_mode == m->key_mode,
                  "%s: kind/number/scale/mode %d %d %d %d / %d %d %d %d", f->slug, s.kind, s.bt_number, s.scale, s.key_mode, m->kind,
                  m->number, m->scale, m->key_mode);
        }
        r = song_open(&s, got, f->rawlen, dec, sizeof dec);
        CHECK(r == SONG_OK, "%s: song_open -> %d", f->slug, r);
        for (l = 0; r == SONG_OK && s.fmt2 && l < SONG_LAYERS; l++) {
            uint32_t v;
            for (v = 1; v <= s.layer[l].variants; v++) {
                int b = song_build(&s, l, v, dec, s.layer[l].need * 8u);
                CHECK(b == SONG_OK, "%s: song_build layer %u level %u -> %d", f->slug, (unsigned)l, (unsigned)v, b);
            }
        }
        free(want);
        free(got);
        free(in);
        free(dict);
    }
    printf("%u built-ins: unpack, CRC, song_peek fields, song_open, every level built: %s\n", (unsigned)BUILTIN_N,
           failures ? "FAILURES" : "ok");
}

/* a corrupt pack: errors only */
static void corrupt(double seconds)
{
    time_t t0 = time(0);
    unsigned long runs = 0, ok = 0, errs[8] = {0};
    while (difftime(time(0), t0) < seconds) {
        const struct bi_file *f = &bi_files[rnd(BUILTIN_N)];
        uint32_t clen = f->clen, rawlen = f->rawlen, dlen = BUILTIN_DICT_LEN, k, edits = 1 + rnd(6);
        uint8_t *in, *out, *dict;
        int r;
        for (k = 0; k < edits; k++) {                 /* the lengths: wild sometimes (a pack lying about the sizes) */
            uint32_t op = rnd(100);
            if (op < 8)
                rawlen = rnd(BUILTIN_MAX + 64u);
            else if (op < 14)
                clen = rnd(f->clen + 32u);
            else if (op < 18)
                dlen = rnd(BUILTIN_DICT_LEN + 1u);
        }
        in = malloc(clen ? clen : 1u);
        out = malloc(rawlen ? rawlen : 1u);
        dict = malloc(dlen ? dlen : 1u);
        for (k = 0; k < clen; k++)
            in[k] = k < f->clen ? bi_pack[f->off + k] : (uint8_t)rnd(256);
        memcpy(dict, bi_dict, dlen);
        for (k = 0; k < edits && clen; k++) {
            uint32_t i = rnd(clen), op = rnd(100);
            if (op < 50)
                in[i] = (uint8_t)rnd(256);
            else if (op < 75)
                in[i] = (uint8_t)(in[i] ^ (1u << rnd(8)));
            else {
                static const uint8_t V[] = {0, 1, 0x0F, 0xF0, 0xFF, 0x80, 0x7F};
                in[i] = V[rnd(sizeof V)];
            }
        }
        r = bi_unpack(dict, dlen, in, clen, out, rawlen);
        runs++;
        if (r == BI_OK)
            ok++;
        else if (-r < 8)
            errs[-r]++;
        free(in);
        free(out);
        free(dict);
    }
    printf("%lu corrupt packs in %.0f s: %lu still unpacked, errors input %lu, output %lu, offset %lu, short %lu; no ASan / UBSan report\n",
           runs, seconds, ok, errs[1], errs[2], errs[3], errs[4]);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: builtin_test DIR [SECONDS]\n");
        return 2;
    }
    check_all(argv[1]);
    corrupt(argc > 2 ? atof(argv[2]) : 30.0);
    printf(failures ? "builtin_test: %d FAILURE(S)\n" : "builtin_test: all ok\n", failures);
    return failures != 0;
}
