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
void synth_midi(uint8_t status, uint8_t d1, uint8_t d2);   /* main loop: queue a channel message */
void synth_send_now(uint8_t status, uint8_t d1, uint8_t d2);   /* audio interrupt only: to EAS, now */
void synth_panic(void);                   /* any context: all notes off, now */
void synth_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
uint32_t synth_pool_used(void);           /* EAS's memory in use, and the pool's size (bytes) */
uint32_t synth_pool_size(void);
