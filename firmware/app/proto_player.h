/* Licence: TBD (see D017) */
/* PROTOTYPE (Step 0.7): one song, built in (tools/proto_song_to_c.py), played by Sonivox EAS's MIDI file player,
 * with its four layers muted one by one. Phase 1's song player replaces this file and proto_player.c.
 * The main loop asks (pp_play, pp_mute) and reads (pp_*); the audio interrupt does the EAS work (synth.c's hooks). */
#pragma once
#include <stdint.h>

enum { PP_DRUMS, PP_BASS, PP_HARMONY, PP_MELODY, PP_NLAYERS };   /* the layer buttons' order */
enum { PP_FAILED = -1, PP_STOPPED, PP_PLAYING };

/* main loop */
void pp_play(int on);                     /* 1: from the top; 0: stop (and back to the top) */
void pp_mute(uint32_t layers);            /* bit PP_*: muted; takes effect at the next audio block */
int pp_state(void);
int pp_error(void);                       /* why PP_FAILED: the EAS result (0: none) */
uint32_t pp_tick(void);                   /* the song's position in MIDI ticks, as heard (see PP_LEAD_MS) */
uint32_t pp_voices(void);                 /* EAS voices sounding now, and the most since the start */
uint32_t pp_voices_max(void);
const char *pp_layer_name(int layer);     /* "DRUMS" ... */
/* the chord at tick: its name, and the next different one's (0 at the end); its start tick in *start (may be 0) */
const char *pp_chord(uint32_t tick, const char **next, uint32_t *start);
const char *pp_title(void);
uint32_t pp_ppq(void);
uint32_t pp_beats_per_bar(void);
uint32_t pp_end_tick(void);

/* the measurement (A5): per bar, the most voices, the highest load and the longest EAS render (24 MHz ticks); in RAM
 * behind a magic word so a RAM dump of the emulator finds it (tools/proto_test.py) */
#define PP_STATS_BARS 48u
struct pp_stats {
    uint32_t magic;                       /* PP_STATS_MAGIC */
    uint32_t bars;                        /* PP_STATS_BARS */
    uint32_t blocks;                      /* audio blocks rendered while playing */
    uint32_t render_max;                  /* the longest render, ticks24 */
    uint32_t budget;                      /* one block's time, ticks24 */
    uint16_t voices[PP_STATS_BARS];
    uint8_t cpu[PP_STATS_BARS];           /* the platform's audio load (plat_cpu_pct: the whole interrupt, smoothed) */
    uint32_t render[PP_STATS_BARS];
};
#define PP_STATS_MAGIC 0x54535050u        /* "PPST" */
extern volatile struct pp_stats pp_stats;

/* synth.c's hooks, in the audio interrupt only (EAS is not re-entrant): before EAS renders a block, and after */
struct s_eas_data_tag;
struct s_eas_stream_tag;
struct s_eas_stream_tag *pp_open(struct s_eas_data_tag *eas);   /* at start-up: the song's stream, 0 = failed */
void pp_before(void);
void pp_after(uint32_t render_ticks);
void pp_panic(void);                      /* any context: stop, now */
