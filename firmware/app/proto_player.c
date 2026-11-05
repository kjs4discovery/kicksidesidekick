/* Licence: TBD (see D017) */
/* PROTOTYPE (Step 0.7): the built-in song through Sonivox EAS's MIDI file player (proto_player.h).
 * The song is a MIDI file in the flash (proto_song.h, made at build time): an EAS_FILE locator reads it in place
 * (eas/eas_host_fw.c). A layer is muted by muting its MIDI track in the parser (upstream's JET_INTERFACE track mute,
 * eas_options.h): its note-ons are dropped from then on, and its channel gets an all-notes-off so a held chord stops.
 * The tracks of the other variants (bass:2 ...) stay muted. Requests from the main loop are counters and flags the
 * audio interrupt picks up before each render, so only the interrupt calls EAS once the audio runs. */
#include "proto_player.h"
#include "proto_song.h"
#include "synth.h"
#include "eas.h"
#include "eas_data.h"
#include "eas_parser.h"
#include "eas_synth.h"
#include "plat.h"

/* eas_public.c's, not in a header (jet.c declares it the same way) */
extern EAS_RESULT EAS_IntSetStrmParam(S_EAS_DATA *pEASData, EAS_HANDLE pStream, EAS_INT param, EAS_IPTR value);

/* the screen shows the song this far ahead of the parser's position, so a chord is on the screen when its first note
 * sounds: up to a UI frame (17 ms) before the main loop sees the change, a blit still going (the top band: ~13 ms),
 * drawing, then the big chord's transfer to the LCD (84 rows at ~0.32 ms a row: ~27 ms), less the block's own wait to
 * be played (~6 ms). Measured in fm1-emu (tools/proto_test.py): 30 ms was late by up to ~20 ms */
#define PP_LEAD_MS 80u

static const char *const LAYER_NAME[PP_NLAYERS] = {"DRUMS", "BASS", "HARMONY", "MELODY"};

static int song_read(void *h, void *buf, int off, int n)
{
    int i;
    (void)h;
    if (off < 0 || n < 0 || (uint32_t)off >= sizeof SONG_MID)
        return 0;
    if ((uint32_t)(off + n) > sizeof SONG_MID)
        n = (int)(sizeof SONG_MID - (uint32_t)off);
    for (i = 0; i < n; i++)
        ((uint8_t *)buf)[i] = SONG_MID[off + i];
    return n;
}
static int song_size(void *h) { (void)h; return (int)sizeof SONG_MID; }
static EAS_FILE song_file = {0, song_read, song_size};

static S_EAS_DATA *eas;
static EAS_HANDLE song;
static volatile int state = PP_STOPPED, error;
static volatile uint32_t play_req, play_done;   /* counters: a request is pending while they differ */
static volatile uint8_t play_on;          /* what the last request asked */
static volatile uint32_t mute_want;       /* layers, from the main loop */
static uint32_t mute_set = 0xFFFFFFFFu;   /* layers as the parser has them (none applied yet) */
static volatile uint32_t pos_ms;          /* the parser's position after the last render */
static volatile uint32_t voices, voices_max;
volatile struct pp_stats pp_stats = {.magic = PP_STATS_MAGIC, .bars = PP_STATS_BARS};

/* ---------------------------------------------------------------- main loop --- */
void pp_play(int on)
{
    play_on = (uint8_t)(on != 0);
    __asm__ volatile("" ::: "memory");
    play_req++;
}
void pp_mute(uint32_t layers) { mute_want = layers & ((1u << PP_NLAYERS) - 1u); }
int pp_state(void) { return state; }
int pp_error(void) { return error; }
uint32_t pp_voices(void) { return voices; }
uint32_t pp_voices_max(void) { return voices_max; }
const char *pp_layer_name(int layer) { return layer >= 0 && layer < PP_NLAYERS ? LAYER_NAME[layer] : "?"; }
const char *pp_title(void) { return SONG_TITLE; }
uint32_t pp_ppq(void) { return SONG_PPQ; }
uint32_t pp_beats_per_bar(void) { return SONG_BEATS_PER_BAR; }
uint32_t pp_end_tick(void) { return SONG_END_TICK; }

/* ms -> ticks at the song's one tempo: ticks = ms * PPQ * 1000 / tempo_us, in 32 bits for songs of any length */
#define TICKS_PER_MS_Q16 ((uint32_t)((uint64_t)SONG_PPQ * 1000u * 65536u / SONG_TEMPO_US))
static uint32_t ms_to_tick(uint32_t ms)
{
    return ((ms >> 6) * TICKS_PER_MS_Q16 >> 10) + ((ms & 63u) * TICKS_PER_MS_Q16 >> 16);
}

uint32_t pp_tick(void)
{
    if (state != PP_PLAYING)
        return 0;
    return ms_to_tick(pos_ms + PP_LEAD_MS);
}

const char *pp_chord(uint32_t tick, const char **next, uint32_t *start)
{
    uint32_t i = 0;
    while (i + 1u < SONG_NCHORDS && SONG_CHORDS[i + 1u].tick <= tick)
        i++;
    if (next)
        *next = i + 1u < SONG_NCHORDS ? SONG_CHORD_NAME[SONG_CHORDS[i + 1u].chord] : 0;
    if (start)
        *start = SONG_CHORDS[i].tick;
    return SONG_CHORD_NAME[SONG_CHORDS[i].chord];
}

/* ---------------------------------------------------------- audio interrupt --- */
static uint32_t track_mutes(uint32_t layers)
{
    uint32_t m = SONG_MUTED_ALWAYS, l;
    for (l = 0; l < PP_NLAYERS; l++)
        if (layers >> l & 1u)
            m |= SONG_LAYER_TRACKS[l];
    return m;
}

static void apply_mutes(uint32_t layers, int force)
{
    uint32_t l, newly = layers & ~mute_set;
    if (!force && layers == mute_set)
        return;
    EAS_IntSetStrmParam(eas, song, PARSER_DATA_MUTE_FLAGS, (EAS_IPTR)track_mutes(layers));
    for (l = 0; l < PP_NLAYERS; l++)
        if (mute_set != 0xFFFFFFFFu && (newly >> l & 1u))
            synth_send_now((uint8_t)(0xB0u | SONG_LAYER_CHANNEL[l]), 123u, 0);   /* its held notes off */
    mute_set = layers;
}

static void all_off(void)
{
    uint32_t c;
    for (c = 0; c < 16u; c++)
        synth_send_now((uint8_t)(0xB0u | c), 123u, 0);
}

EAS_HANDLE pp_open(S_EAS_DATA *e)
{
    EAS_RESULT r;
    eas = e;
    pp_stats.budget = SYNTH_BLOCK * 1000000u / 44100u * PLAT_TICKS_PER_US;   /* one block, ticks24 (no overflow) */
    if ((r = EAS_OpenFile(eas, &song_file, &song)) != EAS_SUCCESS || (r = EAS_Prepare(eas, song)) != EAS_SUCCESS ||
        (r = EAS_Pause(eas, song)) != EAS_SUCCESS) {   /* opened paused: PLAY starts it */
        error = (int)r;
        state = PP_FAILED;
        song = 0;
        return 0;
    }
    apply_mutes(0, 1);
    return song;
}

static void stop_and_rewind(void)
{
    EAS_STATE s;
    if (EAS_State(eas, song, &s) == EAS_SUCCESS && s < EAS_STATE_STOPPED)
        EAS_Pause(eas, song);
    all_off();
    EAS_Locate(eas, song, 0, EAS_FALSE);  /* paused stays paused; at the end it is ready again */
    apply_mutes(mute_want, 1);            /* the parser's reset may clear the flags */
    synth_send_now(0xC0u | SYNTH_KEYS_CH, GM_ELECTRIC_PIANO_1, 0);   /* the reset may clear the keys' sound */
    state = PP_STOPPED;
}

void pp_before(void)
{
    EAS_STATE s;
    if (!song)
        return;
    if (play_req != play_done) {
        play_done = play_req;
        stop_and_rewind();
        if (play_on) {
            if (EAS_State(eas, song, &s) == EAS_SUCCESS && (s == EAS_STATE_PAUSED || s == EAS_STATE_PAUSING))
                EAS_Resume(eas, song);
            state = PP_PLAYING;
        }
        pos_ms = 0;
    }
    apply_mutes(mute_want, 0);
}

void pp_after(uint32_t render_ticks)
{
    EAS_I32 ms = 0;
    EAS_STATE s;
    uint32_t v, bar;
    if (!song)
        return;
    v = eas->pVoiceMgr ? eas->pVoiceMgr->activeVoices : 0;
    voices = v;
    if (v > voices_max)
        voices_max = v;
    if (state != PP_PLAYING)
        return;
    if (EAS_GetLocation(eas, song, &ms) == EAS_SUCCESS && ms >= 0)
        pos_ms = (uint32_t)ms;
    if (EAS_State(eas, song, &s) == EAS_SUCCESS && s == EAS_STATE_STOPPED) {
        state = PP_STOPPED;               /* the end: PLAY plays it again */
        return;
    }
    bar = ms_to_tick(pos_ms) / (SONG_PPQ * SONG_BEATS_PER_BAR);
    if (bar < PP_STATS_BARS) {
        if (v > pp_stats.voices[bar])
            pp_stats.voices[bar] = (uint16_t)v;
        if (render_ticks > pp_stats.render[bar])
            pp_stats.render[bar] = render_ticks;
        if (plat_cpu_pct() > pp_stats.cpu[bar])
            pp_stats.cpu[bar] = (uint8_t)(plat_cpu_pct() > 255u ? 255u : plat_cpu_pct());
    }
    if (render_ticks > pp_stats.render_max)
        pp_stats.render_max = render_ticks;
    pp_stats.blocks++;
}

void pp_panic(void)
{
    pp_play(0);
}
