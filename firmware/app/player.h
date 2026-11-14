/* Licence: TBD (see D017) */
/* The song player (step 1.5): our own scheduler, driving EAS from a compiled song's event streams (song.h).
 * The main loop asks (player_*) and reads; the audio interrupt does the work before each EAS render (synth.c calls
 * player_block), so only the interrupt talks to EAS once audio runs.
 *
 * Time (M2: the tempo in BPM): the position is a 64-bit count in units of 1 / 2,646,000 tick (44100 frames/s x
 * 60 s); each frame adds ppq * bpm, which is exact, so no rounding builds up at any tempo and a tempo change needs no
 * re-anchoring (no drift). A song's own tempo is its file's, rounded to whole BPM (all ours are whole). */
#pragma once
#include <stdint.h>
#include "song.h"

enum { PL_STOPPED, PL_COUNT_IN, PL_PLAYING, PL_PAUSED };
enum { LOOP_OFF, LOOP_SECTION, LOOP_BARS, LOOP_SONG };
#define PL_BPM_MIN 30u
#define PL_BPM_MAX 250u
#define PL_LEAD_CHORD_MS 100u             /* the chord is shown this far ahead of the sound (tools/m1_test.py checks it) */
#define PL_LEAD_BEAT_MS 30u               /* the beat cursor and countdown: about the drawing's own delay */

/* main loop */
void player_load(const struct song *s);   /* stops; s = 0: none */
const struct song *player_song(void);
void player_start(int32_t from, int count_in);   /* play from tick from (count-in: the bar before from's bar, and
                                                   * from's bar up to it, in clicks) */
void player_play(int on);                 /* 1: player_start(the song's start, the count-in setting); 0: stop */
void player_park(int32_t at);             /* M10: the song stopped at tick `at`, held PAUSED there (no sound); PLAY goes on */
/* M10 (BACKTRACK band kits): the drums to vsynth's drum voices (kit_drum 0: EAS's GM kit as before; 1: the 808; 2: CHIP) and
 * the bass / harmony programs (0xFF: the song's own); they apply when the player starts. Main loop. */
void player_kit(uint32_t kit_drum, uint32_t bass_prog, uint32_t harm_prog);
void player_pause(void);                  /* playing: hold the position, sound off (PL_PAUSED); player_start goes on */
void player_mute(uint32_t layers);        /* bit l: layer l muted (song.h order) */
void player_variant(uint32_t layer, uint32_t v);   /* 1..variants; takes effect at the next block */
void player_hold(uint32_t layers);        /* M5: bit l: layer l's events left alone (its notes released) while its
                                           * stream is rebuilt (2.0, library.c lib_level); 0: on again, from where the
                                           * song is */
uint32_t player_held(void);               /* the layers the interrupt holds now (it reads none of their events) */
void player_tempo(uint32_t bpm);          /* PL_BPM_MIN..PL_BPM_MAX */
uint32_t player_song_bpm(const struct song *s);   /* its own tempo, whole BPM */
void player_count_in(int on);
void player_count_in_bars(uint32_t bars); /* M3: the count-in's length, 1 or 2 bars */
void player_count_in_volume(uint32_t cc7);/* M3: the count-in beep's channel volume, 0..127 */
void player_quiet(uint32_t layers, uint32_t pct);  /* M4: the layers (bit l) play at pct % of their loudness */
void player_bass_octave(int32_t semis);   /* M5: the bass layer sounds this far up (12: the setting +1 OCTAVE) */
void player_transpose(int32_t semitones); /* every layer but the percussion channel's, -11..+11; at once (sounding notes
                                           * released) */
int32_t player_transpose_of(void);
void player_loop(int mode, uint32_t from, uint32_t to);   /* ticks; LOOP_OFF ignores them */
int player_state(void);
int player_idle(void);                    /* the audio interrupt has taken every request (load, play) made so far */
int32_t player_tick(uint32_t lead_ms);    /* the position after the last rendered block, plus lead_ms (below the start
                                           * in the count-in), with a loop's jump already applied */
int32_t player_play_from(void);           /* where the count-in ends: the first tick of music (player_start's from) */
uint32_t player_frames(void);             /* audio frames rendered since start-up */
uint32_t player_voices(void);
uint32_t player_voices_max(void);
uint32_t player_variant_of(uint32_t layer);
uint32_t player_tempo_of(void);           /* BPM */
int player_count_in_of(void);
int player_loop_of(uint32_t *from, uint32_t *to);
/* M9 (BACKTRACK, D053): from the bar line at tick `at` of the song playing, play `next` (another version of the band, or
 * the same song in another key / at other levels) from tick `to` of it, sample-exact, with the loop and transposition
 * given and `levels` (4 bits a layer, drums low) as each layer's level; the notes sounding are released. The song
 * playing and `next` must both stay put and unchanged until player_swaps() counts it (the old one after that no more
 * than the main loop reads it). A request the position has passed (or while not PL_PLAYING) is dropped:
 * player_swaps() stays, player_swaps_late() counts it. player_swap_cancel() withdraws a waiting request. */
void player_swap(const struct song *next, int32_t at, int32_t to, int loop_mode, uint32_t loop_from, uint32_t loop_to,
                 int32_t transpose, uint32_t levels);
void player_swap_cancel(void);
uint32_t player_swaps(void);
uint32_t player_swaps_late(void);
uint32_t player_swap_waiting(void);       /* the interrupt holds a swap (not made, not dropped) */

/* for tools/m1_test.py: in RAM behind a magic word, found in an emulator RAM dump */
#define PL_STATS_BARS 64u
#define PL_STATS_CHORDS 256u
#define PL_STATS_TEMPOS 64u
struct player_stats {
    uint32_t magic;                       /* PL_STATS_MAGIC */
    uint32_t version;                     /* 4 (M2 3: tempo_pct is BPM, the fields after chord_drawn; M3 4: the fields
                                           * after view) */
    uint32_t blocks, render_max, budget;  /* blocks while playing; the longest EAS render and one block's time (ticks24) */
    uint32_t starts, start_frames;        /* PLAY presses that started, the frame count at the last */
    uint32_t ends, end_frames;            /* the song reached its end; the frame count then */
    int32_t last_tick;                    /* the position after the last block */
    uint32_t loops, loop_from, loop_to;   /* loop jumps made; the last one's ticks */
    uint32_t ntempo;                      /* tempo changes while playing (the first PL_STATS_TEMPOS logged) */
    uint32_t tempo_frames[PL_STATS_TEMPOS], tempo_pct[PL_STATS_TEMPOS];   /* (tempo_pct: BPM since version 2) */
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
    /* version 2 (M2) */
    int32_t start_from;                   /* the last start's first tick of music */
    int32_t start_tick;                   /* the last start's first tick (below start_from with a count-in) */
    uint32_t pauses, pause_frames;        /* pauses; the frame count at the last */
    int32_t pause_tick;                   /* the position it paused at */
    uint32_t bpm;                         /* the tempo now */
    uint32_t clicks, click_peak_vel;      /* count-in clicks sent; the loudest's velocity */
    uint32_t key_note_last;               /* the last note the keys played (with the octave), 0xFF: none */
    int32_t keys_octave;
    uint32_t ui_view;                     /* the UI's screen (ui.c): 0 list, 1 play, 2 help; +16 the splash still up;
                                           * 0xFF the app's screens never started (a power-on rescue took over) */
    uint32_t flash_bars, flash_beats;     /* the beat light's flashes (ui.c): bright (beat 1), mid (the others) */
    int32_t transpose;                    /* the transposition in use (semitones) */
    uint32_t note_on_last[SONG_LAYERS];   /* the last note-on sent per layer (the note as sent, transposed) */
    uint32_t lit_keys, dim_keys;          /* the key lights (ui.c): bit k = key k (F3 = 0) */
    uint32_t view;                        /* the play screen's view (ui.c): 0 chord, 1 keyboard, 2 chart */
    /* version 4 (M3) */
    uint32_t part;                        /* your part (ui.c): the layer followed, 0xFF: none muted */
    uint32_t key_flashes;                 /* the key lights' bright flashes (an attack of your part) */
    uint32_t mid_keys;                    /* the key lights between attacks (ui.c) */
    uint32_t loop_step;                   /* setting the loop (ui.c): 1 moving A, 2 moving B, 0 not */
    uint32_t settings;                    /* the settings in use (ui.c): 4 bits each, S_* order */
    uint32_t settings_saves;              /* settings saved to the flash since start-up */
    uint32_t notes_guided;                /* note-ons sent quietly (M3: the guide; M4: a QUIET layer) */
    uint32_t folder;                      /* the song list's folder (ui.c): 0 the folders, 1 all songs, 1 + k category k */
    uint32_t hints;                       /* M5: the controls hint shown (ui.c) */
    uint32_t splash_notes;                /* M5: the start-up melody's notes played (ui.c) */
    /* M5 part 3: SYNTH mode (ui.c, synth.c, vsynth.c) */
    uint32_t mode;                        /* 0 not chosen yet (the mode screen), 1 LEARN, 2 SYNTH */
    uint32_t synth_notes, synth_voices_max, synth_preset, arp_steps, synth_peak;
    uint32_t synth_render_max;            /* the longest SYNTH render (ticks24; budget as player_stats.budget) */
    /* M6 (16 words: tools/m6_test.py reads them by these names) */
    uint32_t mode_switches;               /* back to the mode screen (HOME held, Settings > Switch mode) */
    uint32_t zoom;                        /* the scrolling / staff views' zoom: the ticks a 240-pixel screen shows */
    uint32_t uke;                         /* the chord boxes: 0 guitar, 1 ukulele (the setting) */
    uint32_t roll_keys;                   /* the melody roll's piano-key labels drawn (rows, the last frame) */
    uint32_t jam, jam_notes;              /* SYNTH's drum jam: on; its drum notes played */
    uint32_t shift;                       /* SYNTH: the SHIFT layer held (the K1-K4 second set) */
    uint32_t fx_reverb, fx_crush;         /* SYNTH: the reverb and bit-crush amounts (0..127) */
    uint32_t arp_pattern, arp_rate;       /* SYNTH: ARP_*, RATE_* now */
    uint32_t jam_style, jam_level;        /* SYNTH: the drum jam's style (tools/jam_songs.py order), level */
    uint32_t m6_pad[3];
    /* M7 (16 words: tools/m7_test.py reads them by these names) */
    uint32_t popups, popup_open, popup_btn;   /* the "NOT IMPLEMENTED" pop-up: times shown, open now, its button */
    uint32_t synth_page;                  /* SYNTH: 0 the main screen, 1.. the edit pages; 0xFF the pattern menu */
    uint32_t drum_on, drum_pattern, drum_kit, drum_hits, drum_voices_max, drum_bar;   /* SYNTH's drums */
    uint32_t fx_active;                   /* SYNTH: bit k: effect k ran in the last block (vsynth.h FXB_*) */
    uint32_t synth_engine;                /* SYNTH: the preset's engine, 0 ANALOG, 1 PHASE */
    uint32_t synth_bpm;                   /* SYNTH: the tempo */
    uint32_t synth_level;                 /* SYNTH: the preset level parameter */
    uint32_t levels;                      /* LEARN: the level of each layer, 4 bits each (drums low) */
    uint32_t drum_playing;                /* SYNTH: the pattern playing now (drum_pattern: the one chosen) */
    /* M8 (16 words: tools/m8_test.py reads them by these names) */
    uint32_t bt_n, bt_prog, bt_style, bt_structure, bt_key, bt_bpm, bt_sound;   /* BACKTRACK: backtracks found, the
                                           * choices (bt.c: the setup's order; prog: the number) */
    uint32_t bt_metre;                    /* the backtrack open: its beats a bar (WALTZ 3) */
    uint32_t scale_mask, tone_mask, next_mask;   /* the scale views: bit pc of the scale, the chord's tones, the next
                                           * chord's (its last beat only) */
    uint32_t scale_kind;                  /* the setting: 0 PENTATONIC, 1 FULL SCALE, 2 CHORD TONES (+ 4: blues, + 8 minor) */
    uint32_t bt_energy;                   /* the energy of the section playing */
    uint32_t bt_style_now;                /* the open backtrack's STYL index (song.h), 0xFF none */
    uint32_t mode_sel;                    /* the mode screen's cursor: 0 SONG LEARN, 1 BACKTRACK, 2 SYNTH */
    uint32_t list_n;                      /* SONG LEARN: the songs the list's folder shows (backtracks never) */
    uint32_t bt_loop_from, bt_loop_to;    /* the loop set now (player_loop_of; 0 0: none) */
    /* M9 (tools/m9_test.py reads them by these names) */
    uint32_t countins;                    /* times the player went into PL_COUNT_IN (a start with a count-in, ...) */
    uint32_t fold_shown;                  /* SONG LEARN's folders: the count shown for the row the cursor is on */
    uint32_t btn_lit, btn_dim;            /* the buttons' lights as last set (bit b: button b, ui.c B_*) */
    uint32_t swaps, swaps_late;           /* BACKTRACK: swaps made at a bar line; requests the position had passed */
    uint32_t swap_at, swap_to;            /* the last swap's old tick and new tick */
    uint32_t bt_cplx, bt_want;            /* BACKTRACK: the complexity playing; changes waiting (bit 0 progression,
                                           * 1 style, 2 structure, 3 complexity, 4 key) */
    uint32_t bt_tier_now;                 /* the tier the drums play now (the layers row's L: level + the section's energy) */
    uint32_t bt_prepares, bt_prepare_ms;  /* new versions built; the last one's time (ms) */
    uint32_t bt_pending_bar;              /* the bar line (bar index) the next swap is requested for, 0xFFFFFFFF: none */
    /* M10 (24 words, then the texts: tools/m10_test.py reads them by these names) */
    uint32_t vp_open, vp_count, vp_close_ms, vp_btn_closed;   /* the value pop-up: open now, times shown, ms it was open at
                                           * the last close, the last close was a button press */
    uint32_t ind_section, ind_blinking, ind_blink_starts, ind_blink_tick, ind_blink_to, ind_on;   /* the section indicator:
                                           * the section it names, blinking now, blinks started, the tick the last began,
                                           * the section it blinked to, shown now (blink phase) */
    uint32_t kit, kit_hits, kit_bass_prog, kit_harm_prog;     /* BACKTRACK's band kit (0 STUDIO..3 CHIP), its drum hits */
    uint32_t preset_used, preset_saves, preset_loaded, preset_sel, save_screen, save_slot, preset_junk;   /* user presets */
    uint32_t info_shown;                  /* the Settings screen shows a song info line */
    uint32_t m10_pad[2];
    char vp_title[24], vp_value[40], vp_sub[32], next_text[48], ind_name[16], info_text[40], preset_name[16];
    /* M10.1 (tools/m101_test.py): after the M10 texts */
    char title_now[48], legend_now[64];   /* the SYNTH screen's title as drawn last; the bottom instruction row's text as drawn last */
};
#define PL_STATS_MAGIC 0x54534C50u        /* "PLST" */
extern volatile struct player_stats player_stats;

/* synth.c, in the audio interrupt only */
void player_block(uint32_t frames);       /* before EAS renders a block: requests, then this block's events */
void player_after(uint32_t render_ticks, uint32_t voices);
void player_panic(void);                  /* any context: stop now */
