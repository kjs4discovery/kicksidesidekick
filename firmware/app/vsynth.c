/* Licence: TBD (see D017) */
/* SYNTH mode's sound (vsynth.h). Ours: the host around the ANALOG engine (voice allocation, the envelopes, an LFO
 * for the pulse width), the chorus, the delay, the arpeggiator and the presets. From Felucca-Engines (MIT,
 * firmware/vendor/felucca-engines, unchanged): felucca_engine.h, dsp.c, eng_analog.c and the tables. Nothing else
 * of Felucca (GPL) is used.
 *
 * Fixed point at 44.1 kHz; the control rate is the engine's block, CTL = 32 samples (0.73 ms): envelopes, the LFO
 * and the arpeggiator step there. No 64-bit arithmetic (the CPU's 64-bit multiply must stay out of line). */
#include "vsynth.h"

#define NVOICE VS_VOICES
#define NPART 1
#include "fe_tables.h"
#include "felucca_engine.h"
enum { P_ATK, P_REL, P_FTYPE, P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_COUNT };
typedef struct track {
    int16_t p[P_COUNT];
    voice_t v[NVOICE];
} track_t;
static track_t trk[NPART];
#include "dsp.c"
#include "eng_analog.c"

/* ------------------------------------------------------------------ our presets --- */
/* wave: 0 SAW 1 SQR 2 TRI 3 SIN 4 PWM 5 SYNC 6 SUB (the engine's); det mix noise drv ktr as the engine's EDIT
 * values; then our ADSR (0..127 times, sustain level), the filter envelope amount (-64..63), the LFO's rate and
 * its depth on the pulse width, the knobs' values (cutoff resonance attack release chorus delay) and an arpeggio */
struct preset {
    const char *name;
    uint8_t wave, det, mix, noise, drv, ktr;
    uint8_t dec, sus;
    int8_t fenv;
    uint8_t lfo_rate, lfo_pwm;
    uint8_t knob[6];                      /* CUT RES ATK REL CHORUS DELAY (M6: reverb, crush, detune: their defaults) */
    uint8_t arp, rate;                    /* ARP_*, RATE_* (0 0: none) */
};
static const struct preset PRESETS[] = {
    /*  name            wv det mix noi drv ktr dec sus fenv lfo pwm   CUT RES ATK REL CHO DLY   arp       rate */
    {"NEON PAD",        0, 18, 64,  0,  0, 40,  90, 110, 10, 20,  0, {58, 18, 70, 88, 100, 40}, 0, 0},
    {"MIDNIGHT STRINGS",4,  9, 50,  0,  0, 40,  80, 112,  6, 28, 46, {72, 10, 62, 84, 110, 30}, 0, 0},
    {"OCTAVE BOUNCE",   0,  0,  0,  0, 20, 64,  45,  40, 34,  0,  0, {44, 62,  0, 18,  10, 25}, ARP_UPDOWN, RATE_16},
    {"SUNSET LEAD",     0, 12, 64,  0, 24, 64,  70, 105, 14, 36,  0, {86, 26,  4, 40,  40, 70}, 0, 0},
    {"SYNC SCREAM",     5, 70,127,  0, 16, 64,  60, 100, 24,  0,  0, {98, 22,  2, 34,  30, 60}, 0, 0},
    {"CHROME BRASS",    0,  8, 64,  0, 12, 64,  66,  90, 44,  0,  0, {40, 16, 24, 40,  50, 30}, 0, 0},
    {"GLASS PLUCK",     2,  5, 50,  0,  0, 64,  44,   0, 48,  0,  0, {34, 52,  0, 44,  60, 80}, 0, 0},
    {"VAPOR KEYS",      3,  7, 45,  0, 10, 30,  84,  40,  8, 22,  0, {120, 0,  2, 60,  90, 45}, 0, 0},
    {"RISER",           0, 22, 64, 70,  0, 30, 127, 127, 63,  0,  0, {16, 96,127, 80,  70, 70}, 0, 0},
    {"CHIP BLIP",       1,  0,  0,  0,  0, 64,  30,  60,  0,  0,  0, {127, 0,  0, 10,   0, 35}, ARP_UP, RATE_16},
    {"DRIVE BASS",      0,  6, 40,  0, 80, 64,  55,  70, 26,  0,  0, {36, 40,  0, 16,   0,  0}, 0, 0},
    {"HAZE PAD",        2, 25, 64, 12,  0, 32, 100, 115,  4, 14,  0, {64, 14, 86, 96, 120, 50}, 0, 0},
    {"LASER ARP",       1,  4, 30,  0,  0, 64,  36,  10, 50,  0,  0, {30, 70,  0, 22,  40, 75}, ARP_UP, RATE_8},
    {"ANALOG BELL",     3, 40, 64,  0,  0, 20, 104,   0,  0,  0,  0, {127, 0,  0, 92,  80, 55}, 0, 0},
    {"NIGHT DRIVE SUB", 6, 10, 80,  0, 30, 64,  50,  90, 18,  0,  0, {48, 30,  0, 20,   0,  0}, 0, 0},
    {"SOFT SAW STRINGS",0, 24, 64,  0,  0, 40,  90, 110,  0, 18,  0, {62, 10, 66, 86, 110, 35}, 0, 0},
    {"HORIZON PWM",     4, 14, 64,  0, 10, 64,  70, 100, 12, 40, 70, {80, 20,  6, 46,  60, 65}, 0, 0},
    {"TAPE CHOIR",      2, 30, 64, 16,  0, 30,  96, 112,  0, 16,  0, {52, 24, 74, 90, 127, 40}, 0, 0},
    {"STAB 84",         0, 15, 64,  0, 14, 64,  45,  20, 40,  0,  0, {58, 30,  0, 22,  60, 50}, 0, 0},
    {"DEEP SUB",        3,  0,  0,  0, 12, 20,  60, 120,  0,  0,  0, {40,  0,  0, 24,   0,  0}, 0, 0},
    {"GRID PULSE",      4,  0,  0,  0,  0, 64,  40,  30, 30, 50, 60, {54, 46,  0, 20,  50, 60}, ARP_DOWN, RATE_16},
};
#define NPRESETS (sizeof PRESETS / sizeof PRESETS[0])

/* M6: each preset's reverb and bit-crush at start (the detune: its own det) */
static const uint8_t PRESET_REV[] = {50, 44, 10, 30, 20, 26, 40, 46, 60, 0, 6, 64, 30, 70, 6, 50, 34, 64, 24, 4, 20};
static const uint8_t PRESET_CRUSH[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 30};
typedef char preset_fx_rows[(sizeof PRESET_REV == NPRESETS && sizeof PRESET_CRUSH == NPRESETS) ? 1 : -1];

uint32_t vsynth_presets(void) { return NPRESETS; }
const char *vsynth_preset_name(uint32_t i) { return i < NPRESETS ? PRESETS[i].name : ""; }

/* ------------------------------------------------------------ main loop -> interrupt --- */
static volatile uint8_t active;
static volatile uint32_t want_seq, want_preset;
static volatile int32_t want_knob[VS_NKNOB];
static volatile uint32_t want_arp, want_rate, want_bpm = 120;
#define QLEN 64u
static volatile uint16_t q[QLEN];        /* note | on << 8 */
static volatile uint32_t q_head, q_tail, off_req;
static struct vs_stats st;

static int16_t *dl_l, *dl_r, *ch_buf;    /* the delay's lines, the chorus's (in the caller's memory) */
static int16_t *rv_buf;                  /* M6: the reverb's (after them) */
#define DMAX 32768u
#define CMAX 2048u

void vsynth_start(void *mem, uint32_t bytes)
{
    uint32_t i;
    if (bytes < VS_MEM_BYTES)
        return;
    dl_l = (int16_t *)mem;
    dl_r = dl_l + DMAX;
    ch_buf = dl_r + DMAX;
    rv_buf = ch_buf + CMAX;
    for (i = 0; i < 2u * DMAX + CMAX + VS_REVERB_LEN; i++)
        dl_l[i] = 0;
    vsynth_preset(0);
    __asm__ volatile("" ::: "memory");
    active = 1;
}

int vsynth_active(void) { return active; }

void vsynth_stop(void)
{
    off_req++;                            /* (the voices: silent when it starts again) */
    __asm__ volatile("" ::: "memory");
    active = 0;
}

void vsynth_preset(uint32_t i)
{
    uint32_t k;
    if (i >= NPRESETS)
        return;
    want_preset = i;
    for (k = 0; k < 6u; k++)
        want_knob[k] = PRESETS[i].knob[k];
    want_knob[VS_REVERB] = PRESET_REV[i];
    want_knob[VS_CRUSH] = PRESET_CRUSH[i];
    want_knob[VS_DETUNE] = PRESETS[i].det;
    want_arp = PRESETS[i].arp;
    want_rate = PRESETS[i].arp ? PRESETS[i].rate : want_rate;
    __asm__ volatile("" ::: "memory");
    want_seq++;
}

uint32_t vsynth_preset_arp(uint32_t i, uint32_t *rate)
{
    if (i >= NPRESETS)
        return ARP_OFF;
    if (rate && PRESETS[i].arp)
        *rate = PRESETS[i].rate;
    return PRESETS[i].arp;
}

int32_t vsynth_knob(uint32_t k) { return k < VS_NKNOB ? want_knob[k] : 0; }

void vsynth_set_knob(uint32_t k, int32_t v)
{
    if (k >= VS_NKNOB)
        return;
    want_knob[k] = v < 0 ? 0 : v > 127 ? 127 : v;
    __asm__ volatile("" ::: "memory");
    want_seq++;
}

void vsynth_arp(uint32_t mode, uint32_t rate, uint32_t bpm)
{
    want_arp = mode < ARP_MODES ? mode : ARP_OFF;
    want_rate = rate < RATES ? rate : RATE_8;
    want_bpm = bpm < 60u ? 60u : bpm > 200u ? 200u : bpm;
    __asm__ volatile("" ::: "memory");
    want_seq++;
}

void vsynth_key(uint32_t note, int on)
{
    uint32_t h = q_head;
    if (h - q_tail >= QLEN || note > 127u)
        return;
    q[h % QLEN] = (uint16_t)(note | (on ? 0x100u : 0));
    __asm__ volatile("" ::: "memory");
    q_head = h + 1u;
}

void vsynth_all_off(void) { off_req++; }
const struct vs_stats *vsynth_stats(void) { return &st; }

/* ------------------------------------------------------------------- the interrupt's --- */
static struct preset cur;                 /* the sound in use (the preset, the knobs over it) */
static int32_t knob[VS_NKNOB];
static uint32_t seen_seq = 0xFFFFFFFFu, off_seen;
static uint32_t arp_mode, arp_rate, arp_bpm = 120;
static int32_t env_a, env_d, env_r;       /* per control tick: the attack's step (Q24), decay / release (Q16) */
static uint32_t lfo_ph, vage;

/* a time knob 0..127 -> control ticks (1 ms .. ~5.9 s, cubed) */
static int32_t ticks_of(int32_t p)
{
    int32_t ms = 1 + p * p * p / 350;
    int32_t t = ms * 1378 / 1000;          /* 44100 / 32 per second */
    return t < 1 ? 1 : t;
}

static void sound_update(void)
{
    uint32_t k, s = want_seq;
    if (s == seen_seq)
        return;
    cur = PRESETS[want_preset < NPRESETS ? want_preset : 0];
    for (k = 0; k < VS_NKNOB; k++)
        knob[k] = want_knob[k];
    arp_mode = want_arp;
    arp_rate = want_rate;
    arp_bpm = want_bpm;
    __asm__ volatile("" ::: "memory");
    if (s != want_seq)                    /* (changed while copying: again next block) */
        return;
    seen_seq = s;
    trk[0].p[P_FTYPE] = 0;
    trk[0].p[P_E0] = cur.wave;
    trk[0].p[P_E1] = (int16_t)knob[VS_DETUNE];   /* (M6: the SHIFT layer's knob; the preset's det at start) */
    trk[0].p[P_E2] = cur.mix;
    trk[0].p[P_E3] = cur.noise;
    trk[0].p[P_E4] = (int16_t)knob[VS_CUT];
    trk[0].p[P_E5] = (int16_t)knob[VS_RES];
    trk[0].p[P_E6] = cur.drv;
    trk[0].p[P_E7] = cur.ktr;
    env_a = (1 << 24) / ticks_of(knob[VS_ATK]);
    env_d = 65536 / ticks_of(cur.dec / 2);
    env_r = 65536 / ticks_of(knob[VS_REL] / 2 + 8);
}

/* voices: start (the free one, else the oldest releasing, else the oldest) and stop */
static void voice_start(uint32_t note)
{
    uint32_t i, best = 0, best_score = 0;
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &trk[0].v[i];
        uint32_t score = !v->active ? 0xFFFFFFFFu : v->stage == 3u ? 0x80000000u + (vage - v->age)
                                                                   : (vage - v->age);
        if (score >= best_score) {
            best_score = score;
            best = i;
        }
    }
    {
        voice_t *v = &trk[0].v[best];
        int sounding = v->active;
        v->note = (uint8_t)note;
        v->vel = 100;
        v->gate = 1;
        v->stage = 1;
        v->age = ++vage;
        v->pitch16 = v->pitch_cur = (int32_t)note * 16;
        v->fine = 0;
        if (!sounding) {
            v->env = 0;
            v->env_out = 0;
            v->ph[0] = v->ph[1] = v->ph[2] = 0;
            for (i = 0; i < 8u; i++)
                v->s[i] = 0;
        }
        v->active = 1;
        ENG_ANALOG.note_on(&trk[0], v);
        st.notes++;
    }
}

static void voice_stop(uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &trk[0].v[i];
        if (v->active && v->gate && v->note == note) {
            v->gate = 0;
            v->stage = 3;
        }
    }
}

static void env_tick(voice_t *v)
{
    int32_t sus = (int32_t)cur.sus << 17;  /* 0..127 -> Q24 */
    switch (v->stage) {
    case 1:
        v->env += env_a;
        if (v->env >= (1 << 24)) {
            v->env = 1 << 24;
            v->stage = 2;
        }
        break;
    case 2:
        v->env -= ((v->env - sus) >> 12) * env_d >> 4;   /* (Q24 >> 12 x Q16: below 2^31) */
        if (v->env < sus + 4096 && v->env > sus - 4096)
            v->env = sus;
        break;
    case 3:
        v->env -= (v->env >> 12) * env_r >> 4;
        if (v->env < (1 << 13)) {
            v->env = 0;
            v->stage = 0;
            v->active = 0;
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ the arpeggiator --- */
static uint8_t held[16], nheld;           /* keys held, in the order pressed */
static uint32_t arp_left, arp_gate_left, arp_i;
static int32_t arp_dir = 1;
static int32_t arp_note = -1;
static uint32_t arp_rng = 0x1234567u;

static void held_add(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < nheld; i++)
        if (held[i] == n)
            return;
    if (nheld < sizeof held)
        held[nheld++] = (uint8_t)n;
}

static void held_del(uint32_t n)
{
    uint32_t i, k = 0;
    for (i = 0; i < nheld; i++)
        if (held[i] != n)
            held[k++] = held[i];
    nheld = (uint8_t)k;
}

/* the held keys sorted up; the arpeggio's next note */
static int32_t arp_next(void)
{
    uint8_t s[16];
    uint32_t i, k, n = nheld;
    if (!n)
        return -1;
    for (i = 0; i < n; i++)
        s[i] = held[i];
    for (i = 1; i < n; i++)
        for (k = i; k > 0 && s[k - 1u] > s[k]; k--) {
            uint8_t t = s[k];
            s[k] = s[k - 1u];
            s[k - 1u] = t;
        }
    if (arp_mode == ARP_RANDOM) {         /* M6: any held note but the one just played */
        uint32_t r;
        arp_rng = arp_rng * 1664525u + 1013904223u;
        r = (arp_rng >> 16) % n;
        if (n > 1u && r == arp_i)
            r = (r + 1u + (arp_rng >> 8) % (n - 1u)) % n;
        arp_i = r;
    } else if (arp_mode == ARP_UP) {
        arp_i = arp_i + 1u >= n ? 0 : arp_i + 1u;
    } else if (arp_mode == ARP_DOWN) {
        arp_i = arp_i == 0 || arp_i >= n ? n - 1u : arp_i - 1u;
    } else {                              /* up and down, the ends once */
        if (n == 1u) {
            arp_i = 0;
        } else {
            int32_t j = (int32_t)arp_i + arp_dir;
            if (j >= (int32_t)n || j < 0) {
                arp_dir = -arp_dir;
                j = (int32_t)arp_i + arp_dir;
            }
            arp_i = (uint32_t)(j < 0 ? 0 : j >= (int32_t)n ? (int32_t)n - 1 : j);
        }
    }
    return s[arp_i < n ? arp_i : 0];
}

/* one control tick of the arpeggiator: a step every 1/4, 1/8, 1/8 triplet or 1/16 at arp_bpm, the note held half of
 * it. M6: the steps counted in 1/256 control ticks, so the triplets keep time */
static uint32_t arp_frac;
static void arp_tick(void)
{
    static const uint32_t PER_BEAT[RATES] = {1, 2, 3, 4};
    uint32_t step256 = 44100u * 60u * 8u / (arp_bpm * PER_BEAT[arp_rate < RATES ? arp_rate : 1u]) * 32u / CTL;
    uint32_t step = step256 >> 8;
    if (arp_gate_left && --arp_gate_left == 0 && arp_note >= 0) {
        voice_stop((uint32_t)arp_note);
        arp_note = -1;
    }
    if (!nheld) {
        arp_left = 0;
        return;
    }
    if (arp_left) {
        arp_left--;
        return;
    }
    if (arp_note >= 0)
        voice_stop((uint32_t)arp_note);
    arp_note = arp_next();
    if (arp_note >= 0) {
        voice_start((uint32_t)arp_note);
        st.arp_steps++;
    }
    arp_frac += step256 & 255u;          /* (the fraction carried: whole ticks on average) */
    arp_left = (step ? step - 1u : 0) + (arp_frac >> 8);
    arp_frac &= 255u;
    arp_gate_left = step / 2u ? step / 2u : 1u;
}

static void keys_in(void)
{
    uint32_t t, i;
    if (off_req != off_seen) {
        off_seen = off_req;
        for (i = 0; i < NVOICE; i++)
            if (trk[0].v[i].active)
                trk[0].v[i].gate = 0, trk[0].v[i].stage = 3;
        nheld = 0;
        arp_note = -1;
        q_tail = q_head;
    }
    for (t = q_tail; t != q_head; t++) {
        uint32_t m = q[t % QLEN], note = m & 0xFFu;
        if (m & 0x100u) {
            held_add(note);
            if (arp_mode == ARP_OFF)
                voice_start(note);
            else if (nheld == 1u)
                arp_left = 0, arp_i = arp_mode == ARP_DOWN ? 0 : 0xFFFFFFFFu, arp_dir = 1;   /* from the start */
        } else {
            held_del(note);
            if (arp_mode == ARP_OFF)
                voice_stop(note);
        }
    }
    q_tail = t;
    if (arp_mode == ARP_OFF && arp_note >= 0) {   /* the arpeggio turned off: its note ends */
        voice_stop((uint32_t)arp_note);
        arp_note = -1;
    }
}

/* ---------------------------------------------------------------- effects --- */
static uint32_t dw, cw, cl_ph;           /* write positions, the chorus's LFO */
static int32_t fb_lp_l, fb_lp_r;

/* the chorus (Juno-like): a line read at two taps, 3.5..8.5 ms, swept by a 0.5 Hz triangle in opposite phase */
static int32_t chorus_tap(int32_t tri, uint32_t w)
{
    uint32_t d = (uint32_t)(154 * 256 + tri * 220);          /* Q8 samples: 154 +- 110 (3.5 +- 2.5 ms) */
    uint32_t pos = (w << 8) - d, i = (pos >> 8) & (CMAX - 1u), f = pos & 255u;
    int32_t a = ch_buf[i], b = ch_buf[(i + 1u) & (CMAX - 1u)];
    return a + (((b - a) * (int32_t)f) >> 8);
}

/* M6: the reverb (ours, a Schroeder-Moorer kind): four damped feedback combs in parallel on the mono sum, then two
 * all-passes each for left and right (different lengths: the width). Lines in the caller's memory (rv_buf) */
static const uint16_t REV_COMB[4] = {1116, 1188, 1277, 1356};
static const uint16_t REV_AP[4] = {556, 441, 579, 464};   /* left two, right two */
static uint16_t rv_ci[4], rv_ai[4];
static int32_t rv_filt[4];

static int32_t rv_allpass(uint32_t k, int16_t *b, int32_t x)
{
    int32_t y = b[rv_ai[k]], out = y - x;
    b[rv_ai[k]] = (int16_t)clamp(x + y / 2, -32767, 32767);   /* (/ rounds toward 0: the tail reaches silence) */
    if (++rv_ai[k] >= REV_AP[k])
        rv_ai[k] = 0;
    return out;
}

static void reverb(int32_t in, int32_t *ol, int32_t *or_)
{
    int16_t *b = rv_buf;
    int32_t sum = 0, l, r;
    uint32_t k;
    for (k = 0; k < 4u; k++) {           /* feedback 0.87 (about 1.5 s to -60 dB), damping 0.25 */
        int32_t y = b[rv_ci[k]];
        rv_filt[k] = y - (y - rv_filt[k]) / 4;
        b[rv_ci[k]] = (int16_t)clamp(in + rv_filt[k] * 28508 / 32768, -32767, 32767);
        if (++rv_ci[k] >= REV_COMB[k])
            rv_ci[k] = 0;
        sum += y;
        b += REV_COMB[k];
    }
    sum >>= 2;
    l = rv_allpass(0, b, sum);
    b += REV_AP[0];
    l = rv_allpass(1, b, l);
    b += REV_AP[1];
    r = rv_allpass(2, b, sum);
    b += REV_AP[2];
    r = rv_allpass(3, b, r);
    *ol = l;
    *or_ = r;
}
typedef char rev_fits[(1116 + 1188 + 1277 + 1356 + 556 + 441 + 579 + 464 <= VS_REVERB_LEN) ? 1 : -1];

/* M6: the bit-crush: fewer bits (16 down to 7) and a held sample (1 to 8 samples) as the knob goes up */
static int32_t cr_hold, cr_n;

/* ------------------------------------------------------------------- the render --- */
void vsynth_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    static int32_t mix[CTL];
    uint32_t b, i, k, voices = 0;
    int32_t chorus = 0, send = 0, dlen, rev, crush, cr_bits, cr_every;
    if (!active || !dl_l) {
        for (i = 0; i < 2u * n; i++)
            lr[i] = 0;
        return;
    }
    sound_update();
    keys_in();
    chorus = knob[VS_CHORUS];
    send = knob[VS_DELAY];
    rev = knob[VS_REVERB];
    crush = knob[VS_CRUSH];
    cr_bits = 16 - crush * 9 / 127;                          /* 16 .. 7 bits (the voices sit well below full scale:
                                                              * fewer would leave a quiet sound nothing) */
    cr_every = 1 + crush / 18;                               /* 1 .. 8 samples held */
    dlen = (int32_t)(44100u * 60u * 3u / (arp_bpm * 4u));   /* a dotted eighth at the tempo */
    if (dlen > (int32_t)DMAX - 1)
        dlen = (int32_t)DMAX - 1;
    for (b = 0; b + CTL <= n; b += CTL) {
        int32_t lfo;
        if (arp_mode != ARP_OFF)
            arp_tick();
        lfo_ph += (uint32_t)cur.lfo_rate * 4096u;            /* ~0.04 .. 5 Hz at the control rate */
        lfo = (int32_t)(lfo_ph >> 16) - 32768;
        lfo = (lfo < 0 ? -lfo : lfo) * 2 - 32768;            /* a triangle, Q15 */
        for (i = 0; i < CTL; i++)
            mix[i] = 0;
        for (k = 0; k < NVOICE; k++) {
            voice_t *v = &trk[0].v[k];
            vmod_t m;
            int32_t e;
            if (!v->active)
                continue;
            env_tick(v);
            if (!v->active)
                continue;
            voices++;
            e = v->env >> 9;                                 /* Q15 */
            m.inc = pitch_inc((uint32_t)clamp(v->pitch16, 0, 2047));
            m.pitch16 = v->pitch16;
            m.amp0 = v->env_out;
            m.amp1 = e * v->vel / 127;
            v->env_out = m.amp1;
            m.cutoff = (cur.fenv * e) >> 6;
            m.shape = (64 << 8) + ((lfo * cur.lfo_pwm) >> 9);
            m.envq15 = e;
            m.fine = 0;
            ENG_ANALOG.render(&trk[0], v, mix, CTL, &m);
        }
        for (i = 0; i < CTL; i++) {
            int32_t x = mix[i] >> 3, wl, wr, l, r, tri, dl, dr, vl, vr;
            uint32_t rd;
            if (crush) {                                     /* M6: the bit-crush, before the effects */
                if (++cr_n >= cr_every) {
                    int32_t q = 1 << (16 - cr_bits);
                    cr_n = 0;
                    cr_hold = clamp(x, -32767, 32767) / q * q;
                }
                x = cr_hold;
            }
            /* chorus */
            ch_buf[cw & (CMAX - 1u)] = (int16_t)clamp(x, -32767, 32767);
            cl_ph += 0x0000C2EBu;                            /* 0.5 Hz at 44.1 kHz (2^32 / 88200) */
            tri = (int32_t)(cl_ph >> 16) - 32768;
            tri = ((tri < 0 ? -tri : tri) * 2 - 32768) >> 8; /* -128..127 */
            wl = chorus_tap(tri, cw);
            wr = chorus_tap(-tri, cw);
            cw++;
            l = x + ((wl * chorus) >> 7) - ((x * chorus) >> 9);
            r = x + ((wr * chorus) >> 7) - ((x * chorus) >> 9);
            /* the delay: two lines, each feeding the other (ping-pong), a low-pass in the loop */
            rd = (dw - (uint32_t)dlen) & (DMAX - 1u);
            dl = dl_l[rd];
            dr = dl_r[rd];
            fb_lp_l += (dr - fb_lp_l) >> 2;
            fb_lp_r += (dl - fb_lp_r) >> 2;
            dl_l[dw & (DMAX - 1u)] = (int16_t)clamp((((l + r) >> 1) * send >> 7) + (fb_lp_l * 45 >> 7), -32767, 32767);
            dl_r[dw & (DMAX - 1u)] = (int16_t)clamp(fb_lp_r * 90 >> 7, -32767, 32767);
            dw++;
            l += dl * 3 >> 2;
            r += dr * 3 >> 2;
            if (rev) {                                       /* M6: the reverb, last */
                reverb((l + r) / 8 * rev / 128, &vl, &vr);   /* (/: no -1 floor feeding it) */
                l += vl * 4;
                r += vr * 4;
            }
            /* to 24 bits at MASTER (as synth.c: 16-bit x master / 16), a soft limit, then the clamp */
            l = clamp(l, -98304, 98304) * (int32_t)master_q12 / 16;
            r = clamp(r, -98304, 98304) * (int32_t)master_q12 / 16;
            for (k = 0; k < 2u; k++) {
                int32_t *y = k ? &r : &l, a = *y < 0 ? -*y : *y;
                if (a > 6000000)
                    a = 6000000 + (a - 6000000) / 4;
                a = a > 8388607 ? 8388607 : a;
                if ((uint32_t)a > st.peak)
                    st.peak = (uint32_t)a;
                *y = *y < 0 ? -a : a;
            }
            lr[2u * (b + i)] = l;
            lr[2u * (b + i) + 1u] = r;
        }
    }
    st.voices = voices / (n / CTL ? n / CTL : 1u);
    if (st.voices > st.voices_max)
        st.voices_max = st.voices;
}
