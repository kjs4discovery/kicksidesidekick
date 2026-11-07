/* Licence: TBD (see D017) */
/* SYNTH mode's sound (M5 Part 3, D030): a polyphonic synth on the ANALOG engine of Felucca-Engines (MIT,
 * firmware/vendor/felucca-engines), with our own voices, envelopes, chorus, delay, arpeggiator and presets.
 * The main loop sets (vsynth_*); the audio interrupt renders (vsynth_render, from synth.c instead of EAS once
 * vsynth_start ran). Learning mode and SYNTH mode never run at once: the delay lines live in memory the caller gives
 * (library.c's song buffer, unused in SYNTH mode). */
#pragma once
#include <stdint.h>

#define VS_VOICES 8u
#define VS_MEM_BYTES (2u * 32768u * 2u + 2048u * 2u)   /* the delay's two lines and the chorus's (int16) */
enum { VS_CUT, VS_RES, VS_ATK, VS_REL, VS_CHORUS, VS_DELAY, VS_NKNOB };   /* the knobs, 0..127 */
enum { ARP_OFF, ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_MODES };
enum { RATE_4, RATE_8, RATE_16, RATES };

struct vs_stats {
    uint32_t notes;                       /* voices started */
    uint32_t voices, voices_max;          /* sounding now, the most at once */
    uint32_t arp_steps;
    uint32_t render_max;                  /* the longest render (ticks24; the caller measures) */
    uint32_t peak;                        /* the loudest output sample's magnitude (24-bit) since start */
};

uint32_t vsynth_presets(void);
const char *vsynth_preset_name(uint32_t i);
void vsynth_start(void *mem, uint32_t bytes);   /* bytes >= VS_MEM_BYTES, 4-aligned; from then on synth.c renders this */
int vsynth_active(void);
void vsynth_preset(uint32_t i);           /* load preset i: its sound, its knob values and its arpeggio (if any) */
uint32_t vsynth_preset_arp(uint32_t i, uint32_t *rate);   /* preset i's arpeggio: ARP_* (ARP_OFF: none), its rate */
int32_t vsynth_knob(uint32_t k);          /* the knob's value now */
void vsynth_set_knob(uint32_t k, int32_t v);
void vsynth_arp(uint32_t mode, uint32_t rate, uint32_t bpm);   /* ARP_*, RATE_*, 60..200 */
void vsynth_key(uint32_t note, int on);   /* a key: played, or given to the arpeggiator */
void vsynth_all_off(void);
const struct vs_stats *vsynth_stats(void);
void vsynth_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
