/* Licence: TBD (see D017) */
/* The General MIDI synth: Sonivox EAS (firmware/vendor/sonivox) behind a message queue.
 * The main loop queues MIDI messages; the audio interrupt feeds them to EAS and renders. */
#pragma once
#include <stdint.h>

#define SYNTH_BLOCK 256u                  /* output frames a render call (44.1 kHz): one EAS buffer at 22.05 kHz */
#define GM_ELECTRIC_PIANO_1 4u            /* GM program 5, counted from 0 */
#define SYNTH_KEYS_CH 15u                 /* the keys play on channel 16 (counted from 0), clear of a song's */

int synth_init(void);                     /* at boot, before audio runs: 0 = ready */
int synth_ready(void);
void synth_mix_eas(int on);               /* M6: SYNTH mode: EAS under the synth (M7: the ACOUSTIC drum kit) */
/* M7 (D041): SYNTH mode's drums: a step sequencer (jam_patterns.h) at the synth's tempo, its hits to a kit (vsynth.h
 * KIT_*): ACOUSTIC = EAS's GM kit on channel 10, 808 / CHIP = vsynth's own drum voices. Main loop */
void synth_drums(int on);                 /* on: from the bar's start; off: the hits stop */
int synth_drums_on(void);
void synth_drum_pattern(uint32_t i);      /* playing: from the next bar; SYNTH_DRUM_DEFAULT: ROCK1 */
#define SYNTH_DRUM_DEFAULT 0xFFFFFFFFu
uint32_t synth_drum_pattern_of(void);     /* the one chosen */
uint32_t synth_patterns(void);
const char *synth_pattern_name(uint32_t i);
uint32_t synth_pattern_beats(uint32_t i);
void synth_drum_kit(uint32_t kit);
void synth_drum_volume(uint32_t v);       /* 0..127 */
void synth_drum_tempo(uint32_t bpm);
uint32_t synth_drum_beat(void);           /* the beat playing in its bar (0..), 0xFF: off */
void synth_midi(uint8_t status, uint8_t d1, uint8_t d2);   /* main loop: queue a channel message */
void synth_send_now(uint8_t status, uint8_t d1, uint8_t d2);   /* audio interrupt only: to EAS, now */
void synth_panic(void);                   /* any context: all notes off, now */
void synth_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
uint32_t synth_pool_used(void);           /* EAS's memory in use, and the pool's size (bytes) */
uint32_t synth_pool_size(void);
