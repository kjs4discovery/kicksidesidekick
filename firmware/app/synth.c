/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The General MIDI synth (synth.h). EAS is not re-entrant, so only the audio interrupt calls it after start-up:
 * the main loop puts messages in a single-producer, single-consumer ring, and each render drains it into EAS's
 * MIDI stream before rendering. EAS runs at 22.05 kHz (the wt_200k_G sound set's rate); each output block is
 * twice that many frames, filled by linear interpolation.
 * The song player (player.c) sends its events into the same MIDI stream from player_block, before each render. */
#include "synth.h"
#include "eas.h"
#include "eas_data.h"
#include "eas_synth.h"
#include "plat.h"
#include "player.h"
#include "vsynth.h"

uint32_t fw_eas_pool_used(void);          /* eas/eas_host_fw.c */
uint32_t fw_eas_pool_size(void);

#define EAS_FRAMES (SYNTH_BLOCK / 2u)     /* 128: BUFFER_SIZE_IN_MONO_SAMPLES at 22.05 kHz */
#define QLEN 64u                          /* queued messages (a power of two) */

static int seq_block(uint32_t n);         /* M7: the drums (below) */
static EAS_DATA_HANDLE eas;
static EAS_HANDLE stream;
static volatile uint8_t ready;
static volatile uint8_t panic_req;
static volatile uint8_t band;             /* M8: BACKTRACK: EAS and the player render under the synth every block */

static volatile uint32_t q[QLEN];
static volatile uint32_t q_head, q_tail;  /* head: written by the main loop; tail: by the interrupt */

static EAS_PCM pcm[EAS_FRAMES * 2u];      /* one EAS buffer, stereo */
static int32_t prev_l, prev_r;            /* the last EAS frame of the previous block, for the interpolation */

void synth_send_now(uint8_t status, uint8_t d1, uint8_t d2)
{
    EAS_U8 m[3];
    m[0] = status;
    m[1] = d1;
    m[2] = d2;
    EAS_WriteMIDIStream(eas, stream, m, (status & 0xE0u) == 0xC0u ? 2 : 3);   /* program / pressure: 2 bytes */
}

int synth_init(void)
{
    const S_EAS_LIB_CONFIG *cfg = EAS_Config();
    if (!cfg || cfg->mixBufferSize != (EAS_I32)EAS_FRAMES || cfg->numChannels != 2)
        return -1;                        /* built for another rate or channel count than this file expects */
    if (EAS_Init(&eas) != EAS_SUCCESS)
        return -2;
    if (EAS_OpenMIDIStream(eas, &stream, 0) != EAS_SUCCESS)   /* one stream: the song's layers and the keys */
        return -3;
    player_stats.budget = SYNTH_BLOCK * 1000000u / 44100u * PLAT_TICKS_PER_US;   /* one block, ticks24 */
    synth_send_now(0xC0u | SYNTH_KEYS_CH, GM_ELECTRIC_PIANO_1, 0);   /* the keys' channel: Electric Piano 1 */
    ready = 1;
    return 0;
}

int synth_ready(void) { return ready; }

void synth_band(int on) { band = (uint8_t)(on != 0); }
uint32_t synth_pool_used(void) { return fw_eas_pool_used(); }
uint32_t synth_pool_size(void) { return fw_eas_pool_size(); }

void synth_midi(uint8_t status, uint8_t d1, uint8_t d2)
{
    uint32_t h = q_head;
    if (h - q_tail >= QLEN)
        return;                           /* full: dropped (not at a person's playing speed) */
    q[h % QLEN] = (uint32_t)status | (uint32_t)d1 << 8 | (uint32_t)d2 << 16;
    __asm__ volatile("" ::: "memory");    /* the message before the index that publishes it */
    q_head = h + 1u;
}

void synth_panic(void)
{
    vsynth_all_off();
    panic_req = 1;
    player_panic();
}

/* EAS and the song player: one block into lr */
static void eas_block(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, t, t0;
    EAS_I32 got = 0;
    if (panic_req) {
        panic_req = 0;
        q_tail = q_head;                  /* forget what is queued */
        for (i = 0; i < 16u; i++)
            synth_send_now((uint8_t)(0xB0u | i), 123u, 0);   /* all notes off, every channel */
    }
    for (t = q_tail; t != q_head; t++) {
        uint32_t m = q[t % QLEN];
        synth_send_now((uint8_t)m, (uint8_t)(m >> 8), (uint8_t)(m >> 16));
    }
    q_tail = t;
    t0 = plat_ticks24();
    player_block(n);
    if (EAS_Render(eas, pcm, (EAS_I32)EAS_FRAMES, &got) != EAS_SUCCESS || got != (EAS_I32)EAS_FRAMES) {
        for (i = 0; i < 2u * n; i++)
            lr[i] = 0;
        return;
    }
    /* 16-bit at 22.05 kHz -> 24-bit at 44.1 kHz: s * 256 * master / 4096 = s * master / 16 */
    for (i = 0; i < EAS_FRAMES; i++) {
        int32_t l = pcm[2u * i], r = pcm[2u * i + 1u];
        lr[4u * i] = ((prev_l + l) / 2) * (int32_t)master_q12 / 16;
        lr[4u * i + 1u] = ((prev_r + r) / 2) * (int32_t)master_q12 / 16;
        lr[4u * i + 2u] = l * (int32_t)master_q12 / 16;
        lr[4u * i + 3u] = r * (int32_t)master_q12 / 16;
        prev_l = l;
        prev_r = r;
    }
    player_after(plat_ticks24() - t0, eas->pVoiceMgr ? (uint32_t)eas->pVoiceMgr->activeVoices : 0u);
}

/* M10 (D062): a mute for the moment a preset is saved (the flash erase stalls the audio): the output fades to silence over one
 * block and back over one, so there is no click. Main loop: synth_mute(1), wait for synth_muted(), save, synth_mute(0). */
static volatile uint8_t mute_want;
static int32_t mute_cur = 4096;           /* the gain now, Q12 (the interrupt's) */
static volatile uint8_t mute_done;        /* the gain is 0 */
void synth_mute(int on) { mute_want = (uint8_t)(on != 0); }
int synth_muted(void) { return mute_done; }

static void mute_apply(int32_t *lr, uint32_t n)
{
    int32_t target = mute_want ? 0 : 4096, a = mute_cur, i;
    if (a == 4096 && target == 4096)
        return;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t g = a + (target - a) * i / (int32_t)n;
        lr[2 * i] = ((lr[2 * i] >> 6) * g) >> 6;
        lr[2 * i + 1] = ((lr[2 * i + 1] >> 6) * g) >> 6;
    }
    mute_cur = target;
    mute_done = target == 0;
}

static void synth_render_inner(int32_t *lr, uint32_t n, uint32_t master_q12);

void synth_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    synth_render_inner(lr, n, master_q12);
    mute_apply(lr, n);
}

static void synth_render_inner(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, t;
    uint32_t t0;
    if (!ready || n != SYNTH_BLOCK) {
        for (i = 0; i < 2u * n; i++)
            lr[i] = 0;
        return;
    }
    if (vsynth_active()) {                /* M5: SYNTH mode: our synth instead of EAS and the song player; M7: the
                                           * drums' steps first, and with the ACOUSTIC kit EAS under the synth */
        static int32_t vs[2u * SYNTH_BLOCK];
        int mix_eas = seq_block(n) | band;
        if (mix_eas)
            eas_block(lr, n, master_q12);
        t0 = plat_ticks24();
        vsynth_render(mix_eas ? vs : lr, n, master_q12);
        t = plat_ticks24() - t0;
        if (t > player_stats.synth_render_max)
            player_stats.synth_render_max = t;
        if (mix_eas)
            for (i = 0; i < 2u * n; i++) {
                int32_t v = lr[i] + vs[i];
                lr[i] = v > 8388607 ? 8388607 : v < -8388607 ? -8388607 : v;
            }
        return;
    }
    eas_block(lr, n, master_q12);
}

/* ------------------------------------------------------------- M7: SYNTH mode's drums --- */
/* The step sequencer (D041): the pattern (jam_patterns.h, from docs/drum-patterns.txt) at the synth's tempo, its steps
 * counted in 1/256 control ticks (CTL = 32 samples) so 12-step bars keep time. Each block (in the audio interrupt,
 * before anything renders) it finds the steps due in the block and sends each hit: ACOUSTIC to EAS's GM kit on
 * channel 10 (EAS then renders this block under the synth: on the block, about 6 ms grid), 808 / CHIP to vsynth's own
 * drum voices at their control tick. A new pattern chosen while it plays starts at the next bar. */
#include "jam_patterns.h"
#define CTL_FRAMES 32u                    /* vsynth's control tick (Felucca-Engines' CTL) */
static volatile uint8_t dr_on, dr_kit;
static volatile uint32_t dr_pat = JAM_DEFAULT, dr_vol = 89, dr_bpm = 120;
static uint8_t sq_run, sq_kit, sq_vol_sent = 0xFF;
static uint32_t sq_pat, sq_step, sq_left, sq_beat = 0xFFu, eas_tail;
static volatile uint32_t sq_hits;         /* hits sent, every kit (for the tests) */
static uint8_t sq_off[8];                 /* the GM notes sounding (note-offs at the next step) */
static uint32_t sq_noff;
static const uint8_t GM_DRUM[DR_N] = {36, 38, 42, 46, 39, 45, 50, 37};
static const uint8_t GM_VEL[4] = {0, 40, 90, 124};

void synth_drums(int on) { dr_on = (uint8_t)(on != 0); }
int synth_drums_on(void) { return dr_on; }
void synth_drum_pattern(uint32_t i) { dr_pat = i < JAM_PATTERNS ? i : JAM_DEFAULT; }
uint32_t synth_drum_pattern_of(void) { return dr_pat; }
uint32_t synth_patterns(void) { return JAM_PATTERNS; }
const char *synth_pattern_name(uint32_t i) { return JAM[i < JAM_PATTERNS ? i : 0].name; }
uint32_t synth_pattern_beats(uint32_t i) { return JAM[i < JAM_PATTERNS ? i : 0].beats; }
void synth_drum_kit(uint32_t kit) { dr_kit = (uint8_t)(kit < KITS ? kit : KIT_ACOUSTIC); }
void synth_drum_volume(uint32_t v) { dr_vol = v > 127u ? 127u : v; }
void synth_drum_tempo(uint32_t bpm) { dr_bpm = bpm < 60u ? 60u : bpm > 200u ? 200u : bpm; }
uint32_t synth_drum_beat(void) { return dr_on && sq_run ? sq_beat : 0xFFu; }
uint32_t synth_drum_hits(void) { return sq_hits; }

static void gm_offs(void)
{
    uint32_t i;
    for (i = 0; i < sq_noff; i++)
        synth_send_now(0x89u, sq_off[i], 0);
    sq_noff = 0;
}

/* one step's hits, at control tick ctl of this block */
static void seq_hits(uint32_t ctl)
{
    const struct jam_pattern *p = &JAM[sq_pat];
    uint32_t w = p->step[sq_step], i;
    if (sq_kit == KIT_ACOUSTIC)
        gm_offs();
    for (i = 0; i < DR_N; i++) {
        uint32_t v = w >> 2u * i & 3u;
        if (!v)
            continue;
        sq_hits++;
        if (sq_kit == KIT_ACOUSTIC) {
            synth_send_now(0x99u, GM_DRUM[i], GM_VEL[v]);
            if (sq_noff < sizeof sq_off)
                sq_off[sq_noff++] = GM_DRUM[i];
        } else {
            vsynth_drum(ctl, sq_kit, i, v, dr_vol);
        }
    }
}

/* the block's steps (n frames: n / CTL control ticks); 1: EAS renders under the synth this block */
static int seq_block(uint32_t n)
{
    uint32_t c, ticks = n / CTL_FRAMES;
    if (dr_kit != sq_kit) {               /* another kit: the GM kit's notes end */
        gm_offs();
        sq_kit = dr_kit;
    }
    if (!dr_on) {
        if (sq_run) {
            gm_offs();
            sq_run = 0;
            eas_tail = 44100u * 2u / n;   /* (EAS mixed 2 s more: the last hits ring out) */
        }
        if (!eas_tail)
            return 0;
        eas_tail--;
        return 1;
    }
    if (sq_kit == KIT_ACOUSTIC && sq_vol_sent != dr_vol) {
        synth_send_now(0xB9u, 7u, (uint8_t)dr_vol);
        sq_vol_sent = (uint8_t)dr_vol;
    }
    if (!sq_run) {                        /* on: from the bar's start, now */
        sq_run = 1;
        sq_pat = dr_pat;
        sq_step = 0;
        sq_left = 0;
    }
    for (c = 0; c < ticks; c++) {
        while (sq_left < 256u) {          /* a step due in this control tick */
            const struct jam_pattern *p = &JAM[sq_pat];
            uint32_t per_beat = p->steps / p->beats;
            seq_hits(c);
            sq_beat = sq_step % p->steps / per_beat;
            player_stats.drum_bar = sq_step / p->steps;
            player_stats.drum_playing = sq_pat;
            sq_left += 44100u * 60u * 8u / (dr_bpm * per_beat) * 32u / CTL_FRAMES;   /* (1/256 control ticks) */
            if (++sq_step >= (uint32_t)p->steps * p->bars)
                sq_step = 0;
            if (sq_step % p->steps == 0 && dr_pat != sq_pat) {   /* the next bar: a new pattern */
                sq_pat = dr_pat;
                sq_step = 0;
            }
        }
        sq_left -= 256u;
    }
    if (sq_kit == KIT_ACOUSTIC)           /* (and EAS on 2 s after: its last hits ring out) */
        eas_tail = 44100u * 2u / n;
    if (!eas_tail)
        return 0;
    eas_tail--;
    return 1;
}
