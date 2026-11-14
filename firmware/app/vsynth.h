/* Licence: TBD (see D017) */
/* SYNTH mode's sound (M5 Part 3, D030; M7 D039, D041, D042): a polyphonic synth on Felucca-Engines' ANALOG or PHASE
 * engine (MIT, firmware/vendor/felucca-engines), with our own voices, envelopes, effects, arpeggiator, presets and the
 * drum kits' synthesized voices (808: NOISE + our sine-sweep kick, CHIP: LOFI). The main loop sets (vsynth_*); the
 * audio interrupt renders (vsynth_render, from synth.c instead of EAS once vsynth_start ran). Learning mode and SYNTH
 * mode never run at once: the delay and reverb lines live in memory the caller gives (library.c's song buffer).
 *
 * M7: ONE parameter table (VS_PARAMS in vsynth.c: name, range, page, how it is shown) drives the edit pages
 * (synthui.c), the stock presets and the preset record; a preset's defaults are its record. The sound plays from a RAM
 * copy of the record (M7.1 saves from RAM only: the M5 lesson). */
#pragma once
#include <stdint.h>

#define VS_VOICES 8u                      /* the synth's voices */
#define VS_DRUM_VOICES 6u                 /* the drum kits' own (808, CHIP), never the synth's */
#define VS_REVERB_LEN 7040u               /* the reverb's lines (int16): 4 combs, 2 x 2 all-passes */
#define VS_MEM_BYTES (2u * 32768u * 2u + 2048u * 2u + VS_REVERB_LEN * 2u)   /* the delay's two lines, the chorus's,
                                                                           * the reverb's (int16) */
#define VS_MEM_BT (2u * 16384u * 2u + 2048u * 2u + VS_REVERB_LEN * 2u)   /* M8: BACKTRACK's: the delay's maximum
                                                                          * halved (0.37 s; a longer time halved) */
enum { VS_ANALOG, VS_PHASE, VS_ENGINES };   /* a preset's engine */

/* the parameters, in the record's order (vsynth.c VS_PARAMS has each one's name, range, page and display) */
enum {
    VP_ATK, VP_DEC, VP_SUS, VP_REL,                       /* ENVELOPE */
    VP_CUT, VP_RES, VP_FENV, VP_FTYPE,                    /* FILTER (PHASE: our filter after the engine) */
    VP_S0, VP_S1, VP_S2, VP_S3, VP_S4, VP_S5, VP_S6, VP_S7,   /* SOUND A, SOUND B: the engine's (vsynth_param) */
    VP_ARP_PAT, VP_ARP_RATE, VP_ARP_OCT, VP_ARP_GATE,     /* ARP */
    VP_CHO_DEPTH, VP_CHO_MIX, VP_TREM_RATE, VP_TREM_DEPTH,   /* FX 1 */
    VP_PHA_RATE, VP_PHA_DEPTH, VP_PHA_MIX, VP_CRUSH,      /* FX 2 */
    VP_DLY_TIME, VP_DLY_FB, VP_DLY_MIX, VP_FUZZ,          /* FX 3 */
    VP_REV_SIZE, VP_REV_DAMP, VP_REV_MIX, VP_LEVEL,       /* FX 4 */
    VP_ARP_ON,                                            /* no page: the ARP button */
    VP_N
};
enum { PG_ENV, PG_FILTER, PG_SOUND_A, PG_SOUND_B, PG_ARP, PG_FX1, PG_FX2, PG_FX3, PG_FX4, VS_PAGES, PG_NONE = 0xFF };
enum { SHOW_NUM, SHOW_BIP, SHOW_NAMES, SHOW_PLUS1 };   /* 0..hi; v - 64; names[v]; v + 1 */
struct vs_param {
    const char *name;
    uint8_t page, lo, hi, show;
    const char *const *names;             /* SHOW_NAMES */
};
enum { ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, ARP_MODES };   /* VP_ARP_PAT */
enum { RATE_4, RATE_8, RATE_8T, RATE_16, RATES };               /* VP_ARP_RATE */
enum { FXB_FUZZ, FXB_CRUSH, FXB_TREM, FXB_PHASER, FXB_CHORUS, FXB_DELAY, FXB_REVERB };   /* vs_stats.fx: ran */

/* the preset record (RAM; stock presets are this record in a const table; M7.1 saves it) */
#define VS_REC_VERSION 1u
#define VS_REC_BYTES 64u
#define VS_NAME 12u
struct vs_preset {
    uint8_t version, engine;              /* VS_REC_VERSION; VS_ANALOG / VS_PHASE */
    char name[VS_NAME];                   /* NUL-padded, up to 12 characters */
    uint8_t v[VP_N];                      /* the parameters, VP_* order */
    uint8_t pad[VS_REC_BYTES - 2u - VS_NAME - VP_N];
};

struct vs_stats {
    uint32_t notes;                       /* voices started */
    uint32_t voices, voices_max;          /* sounding now, the most at once */
    uint32_t arp_steps;
    uint32_t peak;                        /* the loudest output sample's magnitude (24-bit) since start */
    uint32_t fx;                          /* FXB_* bits: the effects that ran in the last block */
    uint32_t drum_hits, drum_voices, drum_voices_max;   /* the synthesized kits' */
};

/* the stock presets and the parameters */
uint32_t vsynth_presets(void);
const struct vs_preset *vsynth_stock(uint32_t i);
int vsynth_record(struct vs_preset *dst, const void *src, uint32_t n);   /* a record read back (M7.1): checked, an
                                                                           * older version's new parameters from the
                                                                           * defaults; 0 ok */
const struct vs_param *vsynth_param(uint32_t p);   /* VP_S0..S7: the engine's names and ranges */
const char *vsynth_page_name(uint32_t pg);
const char *vsynth_engine_name(uint32_t e);
void vsynth_value_text(char *d, uint32_t p, int32_t v);   /* v as the page shows it (d: >= 12 bytes) */

/* main loop */
void vsynth_start(void *mem, uint32_t bytes);   /* bytes >= VS_MEM_BYTES, 4-aligned; from then on synth.c renders this
                                                 * (M8: VS_MEM_BT..: the delay's maximum halved); 8 voices */
void vsynth_voices(uint32_t n);           /* M8: the voices the keys may take, 1..VS_VOICES (BACKTRACK: 4) */
int vsynth_active(void);
void vsynth_stop(void);                   /* EAS renders again; the memory is free once a render running now ends */
void vsynth_load(const struct vs_preset *p);   /* the sound now: a copy into RAM */
const struct vs_preset *vsynth_now(void); /* the RAM copy (what plays) */
int32_t vsynth_get(uint32_t p);
void vsynth_set(uint32_t p, int32_t v);   /* clamped to its range */
void vsynth_tempo(uint32_t bpm);          /* 60..200: the arp, the delay and the drums */
void vsynth_key(uint32_t note, int on);   /* a key: played, or given to the arpeggiator */
void vsynth_all_off(void);
void vsynth_gain(uint32_t q8);            /* M10.1: an overall gain on the output, 256 = 1 (SONG LEARN's QUIET melody on this synth); any context */
const struct vs_stats *vsynth_stats(void);

/* the drums' synthesized kits (synth.c's sequencer calls these in the audio interrupt, before vsynth_render) */
enum { KIT_ACOUSTIC, KIT_808, KIT_CHIP, KITS };
enum { DR_KICK, DR_SNARE, DR_CHH, DR_OHH, DR_CLAP, DR_TOMLO, DR_TOMHI, DR_RIM, DR_N };
void vsynth_drum(uint32_t ctl, uint32_t kit, uint32_t inst, uint32_t vel, uint32_t vol);   /* a hit at control tick
                                                                           * ctl (0..7) of the next block; vel 1..3;
                                                                           * vol 0..127 */
void vsynth_drums_off(void);              /* (any context) */

void vsynth_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
