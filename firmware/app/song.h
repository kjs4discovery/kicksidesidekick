/* Licence: TBD (see D017) */
/* The compiled song file (.fm1s, version 1.x; docs/song-format.md section 2): the device's parser.
 * Standalone C (no platform calls), so it is fuzzed on the laptop under ASan/UBSan (tools/song_fuzz.c).
 * song_open checks everything before anything else reads the file: every count, offset, tick and value has a bound,
 * the CRC must match, and the accessors below then never read outside the checked chunks. The file stays where it
 * is (the flash, read in place); struct song only points into it. The checks are the same as tools/fm1song's
 * reader's (binformat.py read_compiled); tools/song_fuzz.c --diff compares the two. */
#pragma once
#include <stdint.h>

#define SONG_LAYERS 4u                    /* drums, bass, harmony, melody: the compiled file's order */
#define SONG_MAX_VARIANTS 3u
#define SONG_MAX_SIZE (256u * 1024u)
#define SONG_VOICING_NOTES 5u
#define SONG_NO_CHORD 0xFFu               /* a chord's root for N.C. */

enum {
    SONG_OK = 0,
    SONG_E_SHORT = -1, SONG_E_MAGIC = -2, SONG_E_VERSION = -3, SONG_E_SIZE = -4, SONG_E_CRC = -5,
    SONG_E_HEADER = -6, SONG_E_DIR = -7, SONG_E_MISSING = -8, SONG_E_META = -9, SONG_E_SECT = -10,
    SONG_E_CHRD = -11, SONG_E_CEVT = -12, SONG_E_BARS = -13, SONG_E_LAYR = -14, SONG_E_EVENTS = -15,
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
    struct song_stream var[SONG_MAX_VARIANTS];
};

struct song {
    const uint8_t *data;
    uint32_t size;
    uint32_t ppq, beats_per_bar, bar_ticks, tempo_us, end_tick, bar_count, pickup_beats, swing;
    uint32_t start_tick;                  /* where playback starts: after the pickup bar's empty beats */
    uint32_t first_bar_number;            /* the number musicians read for bar index 0: 0 (a pickup) or 1 */
    uint8_t key_root, key_mode;
    const char *slug, *title, *key;       /* NUL-terminated, printable ASCII, inside the file */
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

int song_peek(struct song *s, const uint8_t *data, uint32_t size);   /* header and META only: for a list */
int song_open(struct song *s, const uint8_t *data, uint32_t size);   /* everything: SONG_OK or SONG_E_* */
const char *song_error_name(int err);

/* everything below needs a song that song_open accepted; indices are checked by the caller against the counts */
void song_event_get(const struct song_stream *st, uint32_t i, struct song_event *e);
uint32_t song_seek(const struct song_stream *st, uint32_t tick);     /* the first event at or after tick */
uint32_t song_chord_at(const struct song *s, uint32_t tick);         /* the chord event sounding at tick */
uint32_t song_cevt_tick(const struct song *s, uint32_t i);
const char *song_cevt_symbol(const struct song *s, uint32_t i);      /* "G7", "N.C." */
const uint8_t *song_cevt_voicing(const struct song *s, uint32_t i, uint32_t level);   /* level 1, 2: 5 notes, 0 = none */
uint32_t song_bar_chord(const struct song *s, uint32_t bar);         /* the chord event sounding on the bar's beat 1 */
uint32_t song_bar_section(const struct song *s, uint32_t bar);
uint32_t song_bar_flags(const struct song *s, uint32_t bar);         /* bit 0: a change on beat 1; bit 1: a section */
const char *song_section_name(const struct song *s, uint32_t i);
uint32_t song_section_first(const struct song *s, uint32_t i);       /* bar index */
uint32_t song_section_bars(const struct song *s, uint32_t i);
uint32_t song_section_lead(const struct song *s, uint32_t i);        /* lead-in ticks (the pickup into it) */
uint32_t song_crc32(const uint8_t *d, uint32_t n, uint32_t crc);     /* zlib's CRC-32, crc = 0 to start */
