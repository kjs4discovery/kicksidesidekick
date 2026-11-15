/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The compiled song file (.fm1s, versions 1.0 to 1.3 and 2.0; docs/song-format.md section 2): the device's parser.
 * Standalone C (no platform calls), so it is fuzzed on the laptop under ASan/UBSan (tools/song_fuzz.c).
 * song_open checks everything before anything else reads the file: every count, offset, tick and value has a bound,
 * the CRC must match, and the accessors below then never read outside the checked chunks. The file stays where it
 * is (the flash, read in place); struct song only points into it, and into the caller's decode buffer, where a 1.1
 * file's delta-coded event streams (EZ chunks) are decoded to the 8-byte records of 1.0. The checks are the same as tools/fm1song's
 * reader's (binformat.py read_compiled); tools/song_fuzz.c --diff compares the two.
 * 2.0 (M5, D029): only the melody and the hand-made or imported levels are stored (NT chunks: bars as patterns); the
 * other levels are generated on the FM-1 from the chart, metre and style (gen.c, the rules of tools/fm1song/
 * arrange.py). A 2.0 song plays one level of each layer at a time: song_build makes it into the caller's buffer. */
#pragma once
#include <stdint.h>

#define SONG_LAYERS 4u                    /* drums, bass, harmony, melody: the compiled file's order */
#define SONG_MAX_VARIANTS 5u               /* M4, format 1.3 (1.2 and older: 3) */
#define SONG_MAX_SIZE (256u * 1024u)
#define SONG_VOICING_NOTES 5u
#define SONG_NO_CHORD 0xFFu               /* a chord's root for N.C. */
#define SONG_EZ_EVENTS 16384u             /* all EZ streams together, at most: 1.3 (M4: 12 levels); 1.1, 1.2: 8192 */
#define SONG_EZ_EVENTS_OLD 8192u
#define SONG_DECODE_BYTES (SONG_EZ_EVENTS * 8u)   /* a decode buffer this big opens any valid 1.x file */
#define SONG_STYLES 11u                   /* 2.0's STYL: waltz ballad shuffle march pop train (arrange.STYLE_NAMES);
                                           * M8, 2.2: + rock indie bossa funk reggae (6..10) */
#define SONG_STYLES_20 6u                 /* (2.0 and 2.1: the first six) */
#define SONG_MAX_SECT 32u
enum { SONG_KIND_SONG, SONG_KIND_BACKTRACK };   /* M8 (2.2, FORM): what the file is */
enum { SONG_SCALE_MAJOR, SONG_SCALE_MINOR, SONG_SCALE_BLUES };   /* M8: a backtrack's scale guide */
enum { ROLE_INTRO, ROLE_VERSE, ROLE_CHORUS, ROLE_BRIDGE, ROLE_ENDING, ROLE_OTHER };   /* M8: a section's role */
#define SONG_NT_PATTERN 4096u             /* 2.0: notes in a bar pattern, at most */
#define SONG_NT_NOTES 65535u              /* 2.0: notes in a stored level, at most */
#define SONG_LEVEL_EVENTS 24576u          /* 2.0: the largest level of each layer together, at most (as generated or
                                           * decoded, before doubles are merged: 2 events a note) */
#define SONG_BUILD_BYTES (SONG_LEVEL_EVENTS * 8u)   /* a build buffer this big plays any valid 2.0 file (192 KiB) */

enum {
    SONG_OK = 0,
    SONG_E_SHORT = -1, SONG_E_MAGIC = -2, SONG_E_VERSION = -3, SONG_E_SIZE = -4, SONG_E_CRC = -5,
    SONG_E_HEADER = -6, SONG_E_DIR = -7, SONG_E_MISSING = -8, SONG_E_META = -9, SONG_E_SECT = -10,
    SONG_E_CHRD = -11, SONG_E_CEVT = -12, SONG_E_BARS = -13, SONG_E_LAYR = -14, SONG_E_EVENTS = -15,
    SONG_E_ROOM = -16,                    /* the caller's decode buffer is too small (never for SONG_DECODE_BYTES) */
};

struct song_stream {
    const uint8_t *ev;                    /* 8-byte events */
    uint32_t count;
};

struct song_layer {
    uint8_t channel;                      /* 0..15 */
    uint8_t program;                      /* 0..127, 0xFF: none */
    uint8_t variants;                     /* 0: the song has no such layer */
    uint8_t volume, pan;
    uint8_t built;                        /* 2.0: the level in var[] (song_build), 0 none yet */
    uint8_t start;                        /* M7 (2.1): the level the song opens with, 1..variants (else 1) */
    uint16_t stored;                      /* 2.0: bit v-1 = level v is in the file (NT); the rest are generated */
    uint32_t need;                        /* 2.0: events its largest level needs (song_build's buffer: need x 8) */
    struct song_stream var[SONG_MAX_VARIANTS];   /* 2.0: every entry is the built level */
};

struct song {
    const uint8_t *data;
    uint32_t size;
    uint32_t minor;                       /* format version 1.minor (2.0: 3, it has 1.3's rules) */
    uint8_t fmt2;                         /* 2.0 (and 2.1) */
    uint8_t fmt21;                        /* M7: 2.1 (LAYR's flags byte: the start level); M8: 2.2 too */
    uint8_t fmt22;                        /* M8: 2.2 (FORM: kind, roles, energy; the new styles) */
    uint8_t kind, bt_number, scale;       /* M8 (FORM): SONG_KIND_*, a backtrack's number (1..), SONG_SCALE_* */
    const char *numerals;                 /* M8: a backtrack's numerals ("I V vi IV"; "" a song), in the file */
    uint8_t role[SONG_MAX_SECT], energy[SONG_MAX_SECT];   /* M8: per section (FORM; else ROLE_OTHER, 0) */
    uint8_t any_energy;                   /* M8: a section has energy > 0 */
    uint8_t restyled;                     /* M8: song_restyle changed its style / metre (file_bar_ticks: the file's) */
    uint32_t file_bar_ticks;
    uint8_t style;                        /* 2.0: the generators' style, 0..SONG_STYLES-1 */
    uint32_t ppq, beats_per_bar, bar_ticks, tempo_us, end_tick, bar_count, pickup_beats, swing;
    uint32_t start_tick;                  /* where playback starts: after the pickup bar's empty beats */
    uint32_t first_bar_number;            /* the number musicians read for bar index 0: 0 (a pickup) or 1 */
    uint8_t key_root, key_mode;
    const char *slug, *title, *key;       /* NUL-terminated, printable ASCII, inside the file */
    const char *category;                 /* 1.2: the song's folder in the list ("" none; 1.0, 1.1: "") */
    char info[40];                        /* M10 (D062): the optional INFO chunk (2.x): the song info line, "" none */
    const uint8_t *sect, *chrd, *cevt, *bars;
    uint32_t nsect, nchrd, ncevt;
    struct song_layer layer[SONG_LAYERS];
    uint8_t opened;                       /* song_open passed (song_peek alone: 0) */
};

struct song_event {
    uint32_t tick;
    uint8_t status;                       /* 0x80 0x90 0xB0 0xE0, no channel */
    uint8_t d1, d2;
};

int song_peek(struct song *s, const uint8_t *data, uint32_t size);   /* header and META only: for a list (M8: and
                                                                     * FORM's kind, number, scale, numerals) */
/* M8 (BACKTRACK): an opened 2.x song played in another style (0..SONG_STYLES-1) and metre (beats a bar, 2..7): every
 * chord change on a bar line, no pickup, no stored level of drums, bass or harmony; a bar stays a bar (its chord the
 * same). Its levels' needs are counted again (song_layer need); every built level is gone (song_build again).
 * SONG_OK, SONG_E_LAYR (not such a song), SONG_E_EVENTS (too many notes in that style) */
int song_restyle(struct song *s, uint32_t style, uint32_t beats_per_bar);
/* everything: SONG_OK or SONG_E_*. buf (room bytes, 4-aligned) takes a 1.1 file's decoded events; it must stay put
 * and unchanged while the song is in use. A 1.0 file needs none (buf may be 0) */
int song_open(struct song *s, const uint8_t *data, uint32_t size, uint8_t *buf, uint32_t room);
/* 2.0: layer's level made into buf (room >= layer need x 8 bytes, 4-aligned; it must stay put while the level is in
 * use): every var[] of the layer then reads it. SONG_OK, SONG_E_ROOM, SONG_E_LAYR (not 2.0, no such level) */
int song_build(struct song *s, uint32_t layer, uint32_t level, uint8_t *buf, uint32_t room);
const char *song_error_name(int err);

/* everything below needs a song that song_open accepted; indices are checked by the caller against the counts */
void song_event_get(const struct song_stream *st, uint32_t i, struct song_event *e);
uint32_t song_seek(const struct song_stream *st, uint32_t tick);     /* the first event at or after tick */
uint32_t song_chord_at(const struct song *s, uint32_t tick);         /* the chord event sounding at tick */
uint32_t song_cevt_tick(const struct song *s, uint32_t i);
const char *song_cevt_symbol(const struct song *s, uint32_t i);      /* "G7", "N.C." */
uint32_t song_cevt_root(const struct song *s, uint32_t i);           /* its root's pitch class, SONG_NO_CHORD: N.C. */
uint32_t song_cevt_mask(const struct song *s, uint32_t i);           /* its pitch classes: bit k = pitch class k */
const uint8_t *song_cevt_voicing(const struct song *s, uint32_t i, uint32_t level);   /* level 1, 2: 5 notes, 0 = none */
uint32_t song_bar_chord(const struct song *s, uint32_t bar);         /* the chord event sounding on the bar's beat 1 */
uint32_t song_bar_section(const struct song *s, uint32_t bar);
uint32_t song_bar_flags(const struct song *s, uint32_t bar);         /* bit 0: a change on beat 1; bit 1: a section */
const char *song_section_name(const struct song *s, uint32_t i);
uint32_t song_section_first(const struct song *s, uint32_t i);       /* bar index */
uint32_t song_section_bars(const struct song *s, uint32_t i);
uint32_t song_section_lead(const struct song *s, uint32_t i);        /* lead-in ticks (the pickup into it) */
uint32_t song_crc32(const uint8_t *d, uint32_t n, uint32_t crc);     /* zlib's CRC-32, crc = 0 to start */
const uint8_t *song_cevt_chord(const struct song *s, uint32_t i);    /* its CHRD record: symbol[8], root, bass, mask */

/* gen.c, for song.c */
const uint8_t *song_nt_chunk(const struct song *s, uint32_t layer, uint32_t level, uint32_t *len);
int32_t song_nt_notes(const struct song *s, const uint8_t *c, uint32_t len, void *out);   /* out 0: check, count */
uint32_t song_gen_notes(const struct song *s, uint32_t layer, uint32_t level);           /* a generated level's */
