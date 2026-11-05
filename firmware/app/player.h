/* Licence: TBD (see D017) */
/* The song player (step 1.5): our own scheduler, driving EAS from a compiled song's event streams (song.h).
 * The main loop asks (player_*) and reads; the audio interrupt does the work before each EAS render (synth.c calls
 * player_block), so only the interrupt talks to EAS once audio runs.
 *
 * Time: the position is a 64-bit count in units of 1 / (tempo_us * 441) tick; each 256-frame block adds
 * 256 * ppq * tempo% * 100, which is exact (44100 frames/s = 441 * 100), so no rounding builds up at any tempo
 * and a tempo change needs no re-anchoring (no drift). */
#pragma once
#include <stdint.h>
#include "song.h"

enum { PL_STOPPED, PL_COUNT_IN, PL_PLAYING };
enum { LOOP_OFF, LOOP_SONG, LOOP_SECTION };
#define PL_TEMPO_MIN 50u
#define PL_TEMPO_MAX 120u
#define PL_LEAD_CHORD_MS 100u             /* the chord is shown this far ahead of the sound (tools/m1_test.py checks it) */
#define PL_LEAD_BEAT_MS 30u               /* the beat cursor and countdown: about the drawing's own delay */

/* main loop */
void player_load(const struct song *s);   /* stops; s = 0: none */
const struct song *player_song(void);
void player_play(int on);                 /* 1: from the top (count-in if on); 0: stop */
void player_mute(uint32_t layers);        /* bit l: layer l muted (song.h order) */
void player_variant(uint32_t layer, uint32_t v);   /* 1..variants; takes effect at the next block */
void player_tempo(uint32_t pct);          /* PL_TEMPO_MIN..PL_TEMPO_MAX */
void player_count_in(int on);
void player_loop(int mode, uint32_t from, uint32_t to);   /* ticks; LOOP_OFF ignores them */
int player_state(void);
int player_idle(void);                    /* the audio interrupt has taken every request (load, play) made so far */
int32_t player_tick(uint32_t lead_ms);    /* the position after the last rendered block, plus lead_ms (negative in
                                           * the count-in), with a loop's jump already applied */
uint32_t player_frames(void);             /* audio frames rendered since start-up */
uint32_t player_voices(void);
uint32_t player_voices_max(void);
uint32_t player_variant_of(uint32_t layer);
uint32_t player_tempo_of(void);
int player_count_in_of(void);
int player_loop_of(uint32_t *from, uint32_t *to);

/* for tools/m1_test.py: in RAM behind a magic word, found in an emulator RAM dump */
#define PL_STATS_BARS 64u
#define PL_STATS_CHORDS 256u
#define PL_STATS_TEMPOS 32u
struct player_stats {
    uint32_t magic;                       /* PL_STATS_MAGIC */
    uint32_t version;                     /* 1 */
    uint32_t blocks, render_max, budget;  /* blocks while playing; the longest EAS render and one block's time (ticks24) */
    uint32_t starts, start_frames;        /* PLAY presses that started, the frame count at the last */
    uint32_t ends, end_frames;            /* the song reached its end; the frame count then */
    int32_t last_tick;                    /* the position after the last block */
    uint32_t loops, loop_from, loop_to;   /* loop jumps made; the last one's ticks */
    uint32_t ntempo;                      /* tempo changes while playing (the first PL_STATS_TEMPOS logged) */
    uint32_t tempo_frames[PL_STATS_TEMPOS], tempo_pct[PL_STATS_TEMPOS];
    int32_t tempo_tick[PL_STATS_TEMPOS];
    uint32_t notes_on[SONG_LAYERS];       /* note-ons sent, per layer */
    uint32_t notes_skipped[SONG_LAYERS];  /* note-ons not sent because the layer was muted */
    uint32_t mutes[SONG_LAYERS];          /* times the layer was muted while it held notes, and they were released */
    uint32_t deferred;                    /* blocks that left events for the next (more than PL_EVENTS_PER_BLOCK) */
    uint32_t song_crc;                    /* the loaded song's CRC field */
    uint32_t voices_after_stop;           /* EAS voices 1 s after the last stop or end */
    uint16_t voices[PL_STATS_BARS];       /* the most voices per bar */
    uint8_t cpu[PL_STATS_BARS];           /* the platform's audio load per bar */
    uint32_t chord_due[PL_STATS_CHORDS];  /* frame count at the block that played chord event k's tick (0: not yet) */
    uint32_t chord_drawn[PL_STATS_CHORDS];/* frame count when the screen had finished drawing it (the UI writes) */
};
#define PL_STATS_MAGIC 0x54534C50u        /* "PLST" */
extern volatile struct player_stats player_stats;

/* synth.c, in the audio interrupt only */
void player_block(uint32_t frames);       /* before EAS renders a block: requests, then this block's events */
void player_after(uint32_t render_ticks, uint32_t voices);
void player_panic(void);                  /* any context: stop now */
