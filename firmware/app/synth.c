/* Licence: TBD (see D017) */
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

static EAS_DATA_HANDLE eas;
static EAS_HANDLE stream;
static volatile uint8_t ready;
static volatile uint8_t panic_req;
static volatile uint8_t mix_eas;          /* M6: SYNTH mode with the drum jam: EAS (the song player) under the synth */

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
void synth_mix_eas(int on) { mix_eas = (uint8_t)(on != 0); }
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

void synth_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, t;
    uint32_t t0;
    if (!ready || n != SYNTH_BLOCK) {
        for (i = 0; i < 2u * n; i++)
            lr[i] = 0;
        return;
    }
    if (vsynth_active()) {                /* M5: SYNTH mode: our synth instead of EAS and the song player; M6: with
                                           * the drum jam, EAS under it */
        static int32_t vs[2u * SYNTH_BLOCK];
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
