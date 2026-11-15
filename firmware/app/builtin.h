/* SPDX-License-Identifier: GPL-3.0-or-later */
/* M10.4 (D070 option 1): the built-in starter library. The songs and backtracks of songs/library.txt are inside the firmware,
 * each compressed on its own with a small LZ77 and a shared dictionary (tools/builtin_pack.py writes builtin_gen.h at every build),
 * and decoded one at a time into a small buffer when opened (library.c lib_file). This header and builtin.c are plain C99 with no
 * dependence on the platform: tools/builtin_test.c builds them on the laptop. */
#pragma once
#include <stdint.h>

/* a song of the list: what the list shows (M4 kept the whole struct song: too big for 200 songs). A built-in's is a const
 * generated one (builtin_gen.h), a store song's is filled by the scan (library.c; moved here in M10.4, fields unchanged) */
struct lib_item {
    const char *title, *key, *category;  /* in the file, in the flash */
    uint32_t bpm, beats_per_bar;
    const char *numerals;                /* M8 (2.2 FORM): a backtrack's numerals ("" a song) */
    uint8_t kind, number, scale, key_mode;   /* M8: SONG_KIND_*, a backtrack's number and scale; the key's mode */
};

/* a built-in's file: its slug, where its packed bytes start in bi_pack and how many, the raw length and the CRC-32 the store would
 * have recorded for it (the whole file's, as fm1song send sends it) */
struct bi_file {
    const char *slug;
    uint32_t off, clen, rawlen, crc;
};

enum { BI_OK = 0, BI_E_INPUT = -1, BI_E_OUTPUT = -2, BI_E_OFFSET = -3, BI_E_SHORT = -4 };

/* in[inlen] unpacked to exactly out[outlen] (the dictionary dict[dictlen] is the history before the first byte) -> BI_OK, or an error
 * (BI_E_*): a corrupt stream never reads outside in or dict, nor writes outside out */
int bi_unpack(const uint8_t *dict, uint32_t dictlen, const uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t outlen);
