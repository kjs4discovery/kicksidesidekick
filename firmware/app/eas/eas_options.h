/* Licence: TBD (see D017) */
/* Our configuration of Sonivox EAS (upstream generates this file from host_src/eas_options.cmake).
 * The wavetable synth only, stereo, 22.05 kHz to match the wt_200k_G sound set, the voices' filter. 8-bit samples:
 * the set as it was made (0x33CBD bytes, ~212 KB); _16_BIT_SAMPLES stores a 16-bit copy twice that size
 * (measured: 440 KB, which nearly filled the app slot).
 * Reverb and chorus on: upstream's eas_public.c uses them without the #ifdefs (it does not build without them).
 * No file formats beyond the SMF parser eas_config.c always lists (linked, unused since M1: our player, player.c,
 * sends the song's events itself). JET_INTERFACE (Step 0.7's track mute) is off again; jet.c is not linked.
 * Dynamic memory: the host layer (eas_host_fw.c) hands out a fixed pool. */
#ifndef FW_EAS_OPTIONS_H
#define FW_EAS_OPTIONS_H

#define EAS_WT_SYNTH
#define NUM_OUTPUT_CHANNELS 2
#define MAX_SYNTH_VOICES 24
#define _FILTER_ENABLED
#define _REVERB_ENABLED
#define _CHORUS_ENABLED
#define _8_BIT_SAMPLES
#define _SAMPLE_RATE_22050

#endif
