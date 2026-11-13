/* Licence: TBD (see D017) */
/* SYNTH mode's sound (vsynth.h). Ours: the host around the engines (voice allocation, the envelopes, an LFO for the
 * pulse width), the parameter table, the presets, the effects and the arpeggiator. From Felucca-Engines (MIT,
 * firmware/vendor/felucca-engines, unchanged): felucca_engine.h, dsp.c, eng_analog.c and the tables. Nothing else
 * of Felucca (GPL) is used: not its host, voice code, effects (fx.c) or presets.
 *
 * Fixed point at 44.1 kHz; the control rate is the engine's block, CTL = 32 samples (0.73 ms): envelopes, the LFO
 * and the arpeggiator step there. No 64-bit arithmetic (the CPU's 64-bit multiply must stay out of line).
 *
 * M7 (D039): one parameter table (VS_PARAMS) and the preset record (vsynth.h struct vs_preset): the main loop edits
 * a RAM copy of the record (ram) and counts the edits (seq); the interrupt copies it when the count moved. */
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
#include "eng_phase.c"                   /* M7 (D042): the second synth engine */
#include "eng_noise.c"                   /* M7 (D041): the 808 kit's snare, hats, clap */
#include "eng_lofi.c"                    /* M7: the CHIP kit */

/* ------------------------------------------------------------------ the parameters --- */
static const char *const N_FTYPE[] = {"LP", "BP", "HP"};
static const char *const N_ARP[] = {"UP", "DOWN", "UP-DOWN", "RANDOM"};
static const char *const N_RATE[] = {"1/4", "1/8", "1/8T", "1/16"};
static const char *const N_DLY[] = {"1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2"};
static const char *const N_OFFON[] = {"OFF", "ON"};
static const char *const PAGE_NAME[VS_PAGES] = {"ENVELOPE", "FILTER", "SOUND A", "SOUND B", "ARP",
                                                "FX 1", "FX 2", "FX 3", "FX 4"};
#define NUM(n, pg) {n, pg, 0, 127, SHOW_NUM, 0}
static const struct vs_param VS_PARAMS[VP_N] = {
    NUM("ATTACK", PG_ENV), NUM("DECAY", PG_ENV), NUM("SUSTAIN", PG_ENV), NUM("RELEASE", PG_ENV),
    NUM("CUTOFF", PG_FILTER), NUM("RESONANCE", PG_FILTER), {"ENV AMOUNT", PG_FILTER, 0, 127, SHOW_BIP, 0},
    {"TYPE", PG_FILTER, 0, 2, SHOW_NAMES, N_FTYPE},
    NUM("", PG_SOUND_A), NUM("", PG_SOUND_A), NUM("", PG_SOUND_A), NUM("", PG_SOUND_A),   /* (the engine's) */
    NUM("", PG_SOUND_B), NUM("", PG_SOUND_B), NUM("", PG_SOUND_B), NUM("", PG_SOUND_B),
    {"PATTERN", PG_ARP, 0, ARP_MODES - 1u, SHOW_NAMES, N_ARP}, {"RATE", PG_ARP, 0, RATES - 1u, SHOW_NAMES, N_RATE},
    {"OCTAVES", PG_ARP, 0, 2, SHOW_PLUS1, 0}, {"GATE", PG_ARP, 1, 127, SHOW_NUM, 0},
    NUM("CHORUS DEPTH", PG_FX1), NUM("CHORUS MIX", PG_FX1), NUM("TREM RATE", PG_FX1), NUM("TREM DEPTH", PG_FX1),
    NUM("PHASER RATE", PG_FX2), NUM("PHASER DEPTH", PG_FX2), NUM("PHASER MIX", PG_FX2), NUM("CRUSH", PG_FX2),
    {"DELAY TIME", PG_FX3, 0, 7, SHOW_NAMES, N_DLY}, NUM("FEEDBACK", PG_FX3), NUM("DELAY MIX", PG_FX3),
    NUM("FUZZ", PG_FX3),
    NUM("REVERB SIZE", PG_FX4), NUM("DAMPING", PG_FX4), NUM("REVERB MIX", PG_FX4),
    NUM("LEVEL", PG_FX4),                 /* (100: as made; half a dB a step) */
    {"ARP", PG_NONE, 0, 1, SHOW_NAMES, N_OFFON},
};
typedef char vs_rec_size[sizeof(struct vs_preset) == VS_REC_BYTES ? 1 : -1];

/* SOUND A / B: the engine's eight parameters, as its descriptors name them. ANALOG's CUT and RES are the FILTER
 * page's (one place for them), so its SOUND B ends with our LFO's rate and its depth on the pulse width instead */
#define S_LFO 0xFE                        /* (host: the LFO's rate) */
#define S_PWM 0xFD                        /* (host: its depth on the pulse width) */
static const uint8_t SLOTS[VS_ENGINES][8] = {{P_E0, P_E1, P_E2, P_E3, P_E6, P_E7, S_LFO, S_PWM},   /* ANALOG */
                                             {P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7}};  /* PHASE */
static const param_desc_t D_LFO = {"LFO RATE", F_PCT, 0, 127, 0, 0, 0};
static const param_desc_t D_PWM = {"PWM DEPTH", F_PCT, 0, 127, 0, 0, 0};

static const engine_t *engine_of(uint32_t e) { return e == VS_PHASE ? &ENG_PHASE : &ENG_ANALOG; }

static const param_desc_t *slot_desc(uint32_t e, uint32_t k, const track_t *t)
{
    const engine_t *en = engine_of(e);
    uint32_t p = SLOTS[e < VS_ENGINES ? e : 0][k & 7u];
    if (p == S_LFO)
        return &D_LFO;
    if (p == S_PWM)
        return &D_PWM;
    if (en->desc && t) {
        const param_desc_t *d = en->desc(t, p - P_E0);
        if (d)
            return d;
    }
    return &en->edit[p - P_E0];
}

/* the default record (a parameter an older record lacks takes this; the SOUND slots: the engine's defaults) */
static void defaults(struct vs_preset *r, uint32_t e)
{
    static const uint8_t D[VP_N] = {2, 64, 100, 40, 90, 20, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, ARP_UP, RATE_8, 0, 64,
                                    64, 0, 40, 0, 40, 64, 0, 0, 3, 64, 0, 0, 64, 64, 0, 100, 0};
    uint32_t k;
    for (k = 0; k < sizeof *r; k++)
        ((uint8_t *)r)[k] = 0;
    r->version = VS_REC_VERSION;
    r->engine = (uint8_t)(e < VS_ENGINES ? e : VS_ANALOG);
    for (k = 0; k < VP_N; k++)
        r->v[k] = D[k];
    for (k = 0; k < 8u; k++)
        r->v[VP_S0 + k] = (uint8_t)slot_desc(r->engine, k, 0)->def;
}

/* ------------------------------------------------------------------ the presets --- */
/* the order of a record's values: ATK DEC SUS REL | CUT RES FENV(64 = 0) FTYPE | SOUND A, B (ANALOG: WAVE DTN MIX
 * NOIS DRV KTR LFO PWM) | ARP: PATTERN RATE OCTAVES(0..2) GATE | CHORUS DEPTH, MIX, TREM RATE, DEPTH | PHASER RATE,
 * DEPTH, MIX, CRUSH | DELAY TIME (N_DLY), FEEDBACK, MIX, FUZZ | REVERB SIZE, DAMPING, MIX, LEVEL | ARP ON.
 * ANALOG waves: 0 SAW 1 SQR 2 TRI 3 SIN 4 PWM 5 SYNC 6 SUB. M7 Part 2: M6's presets as records (their sound as
 * before: chorus depth 64, delay 1/8D at feedback 64, reverb size and damping 64 were M6's fixed values); Part 5:
 * DEEP SUB and ANALOG BELL out, 11 new (30 in all); every LEVEL matched (tools/vsynth_test.c LEVELS=1) */
#define PR(nm, eng, ...) {VS_REC_VERSION, eng, nm, {__VA_ARGS__}, {0}}
#define AN VS_ANALOG
#define PH VS_PHASE
static const struct vs_preset PRESETS[] = {
    /* M8 (D050): E-PIANO, E-GUITAR, E-SITAR first (presets 1-3; SYNTH starts on 1). M7 (D042): PHASE waves: 0 SAW 1 SQR 2 PLS 3 DSIN 4 SPLS 5 RSAW 6 RTRI 7 RTRP; its SOUND A, B:
     * WAVE WAVE2 DCW ENV DTN LINE SUB FOLD (the FILTER page: our filter after it) */
    PR("E-PIANO", PH, 0, 80, 30, 40, 127, 0, 64, 0, 3, 0, 36, 90, 6, 0, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 50, 50, 40, 0, 0, 0, 0, 3, 64, 0, 0, 64, 64, 40, 96, 0),
    PR("E-GUITAR", AN, 0, 70, 50, 30, 70, 20, 94, 0, 0, 6, 64, 0, 0, 64, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 0, 0, 0, 0, 0, 0, 0, 2, 50, 20, 90, 64, 64, 30, 100, 0),
    PR("E-SITAR", PH, 0, 55, 10, 30, 127, 0, 64, 0, 5, 0, 90, 60, 3, 0, 0, 0, ARP_UP, RATE_8, 0, 64,
       40, 30, 0, 0, 0, 0, 0, 0, 3, 64, 0, 0, 64, 64, 30, 119, 0),
    /* M5..M6's (M7: as records) */
    PR("NEON PAD", AN, 70, 90, 110, 88, 58, 18, 74, 0, 0, 18, 64, 0, 0, 40, 20, 0, ARP_UP, RATE_8, 0, 64,
       64, 100, 0, 0, 0, 0, 0, 0, 3, 64, 40, 0, 64, 64, 50, 95, 0),
    PR("DUSK STRINGS", AN, 62, 80, 112, 84, 72, 10, 70, 0, 4, 9, 50, 0, 0, 40, 28, 46, ARP_UP, RATE_8, 0, 64,
       64, 110, 0, 0, 0, 0, 0, 0, 3, 64, 30, 0, 64, 64, 44, 96, 0),
    PR("OCT BOUNCE", AN, 0, 45, 40, 18, 44, 62, 98, 0, 0, 0, 0, 0, 20, 64, 0, 0, ARP_UPDOWN, RATE_16, 0, 64,
       64, 10, 0, 0, 0, 0, 0, 0, 3, 64, 25, 0, 64, 64, 10, 116, 1),
    PR("SUNSET LEAD", AN, 4, 70, 105, 40, 86, 26, 78, 0, 0, 12, 64, 0, 24, 64, 36, 0, ARP_UP, RATE_8, 0, 64,
       64, 40, 0, 0, 0, 0, 0, 0, 3, 64, 70, 0, 64, 64, 30, 97, 0),
    PR("SYNC SCREAM", AN, 2, 60, 100, 34, 98, 22, 88, 0, 5, 70, 127, 0, 16, 64, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 30, 0, 0, 0, 0, 0, 0, 3, 64, 60, 0, 64, 64, 20, 93, 0),
    PR("CHROME BRASS", AN, 24, 66, 90, 40, 40, 16, 108, 0, 0, 8, 64, 0, 12, 64, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 50, 0, 0, 0, 0, 0, 0, 3, 64, 30, 0, 64, 64, 26, 102, 0),
    PR("GLASS PLUCK", AN, 0, 44, 0, 44, 34, 52, 112, 0, 2, 5, 50, 0, 0, 64, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 60, 0, 0, 0, 0, 0, 0, 3, 64, 80, 0, 64, 64, 40, 114, 0),
    PR("VAPOR KEYS", AN, 2, 84, 40, 60, 120, 0, 72, 0, 3, 7, 45, 0, 10, 30, 22, 0, ARP_UP, RATE_8, 0, 64,
       64, 90, 0, 0, 0, 0, 0, 0, 3, 64, 45, 0, 64, 64, 46, 99, 0),
    PR("RISER", AN, 127, 127, 127, 80, 16, 96, 127, 0, 0, 22, 64, 70, 0, 30, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 70, 0, 0, 0, 0, 0, 0, 3, 64, 70, 0, 64, 64, 60, 108, 0),
    PR("CHIP BLIP", AN, 0, 30, 60, 10, 127, 0, 64, 0, 1, 0, 0, 0, 0, 64, 0, 0, ARP_UP, RATE_16, 0, 64,
       64, 0, 0, 0, 0, 0, 0, 70, 3, 64, 35, 0, 64, 64, 0, 122, 1),
    PR("DRIVE BASS", AN, 0, 55, 70, 16, 36, 40, 90, 0, 0, 6, 40, 0, 80, 64, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 0, 0, 0, 0, 0, 0, 0, 3, 64, 0, 0, 64, 64, 6, 98, 0),
    PR("HAZE PAD", AN, 86, 100, 115, 96, 64, 14, 68, 0, 2, 25, 64, 12, 0, 32, 14, 0, ARP_UP, RATE_8, 0, 64,
       64, 120, 0, 0, 0, 0, 0, 0, 3, 64, 50, 0, 64, 64, 64, 93, 0),
    PR("LASER ARP", AN, 0, 36, 10, 22, 30, 70, 114, 0, 1, 4, 30, 0, 0, 64, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 40, 0, 0, 0, 0, 0, 0, 3, 64, 75, 0, 64, 64, 30, 127, 1),
    PR("NIGHT DRIVE", AN, 0, 50, 90, 20, 48, 30, 82, 0, 6, 10, 80, 0, 30, 64, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 0, 0, 0, 0, 0, 0, 0, 3, 64, 0, 0, 64, 64, 6, 100, 0),
    PR("SAW STRINGS", AN, 66, 90, 110, 86, 62, 10, 64, 0, 0, 24, 64, 0, 0, 40, 18, 0, ARP_UP, RATE_8, 0, 64,
       64, 110, 0, 0, 0, 0, 0, 0, 3, 64, 35, 0, 64, 64, 50, 96, 0),
    PR("HORIZON PWM", AN, 6, 70, 100, 46, 80, 20, 76, 0, 4, 14, 64, 0, 10, 64, 40, 70, ARP_UP, RATE_8, 0, 64,
       64, 60, 0, 0, 0, 0, 0, 0, 3, 64, 65, 0, 64, 64, 34, 98, 0),
    PR("TAPE CHOIR", AN, 74, 96, 112, 90, 52, 24, 64, 0, 2, 30, 64, 16, 0, 30, 16, 0, ARP_UP, RATE_8, 0, 64,
       64, 127, 0, 0, 0, 0, 0, 0, 3, 64, 40, 0, 64, 64, 64, 92, 0),
    PR("STAB 84", AN, 0, 45, 20, 22, 58, 30, 104, 0, 0, 15, 64, 0, 14, 64, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 60, 0, 0, 0, 0, 0, 0, 3, 64, 50, 0, 64, 64, 24, 114, 0),
    PR("GRID PULSE", AN, 0, 40, 30, 20, 54, 46, 94, 0, 4, 0, 0, 0, 0, 64, 50, 60, ARP_DOWN, RATE_16, 0, 64,
       64, 50, 0, 0, 0, 0, 0, 30, 3, 64, 60, 0, 64, 64, 20, 119, 1),
    PR("POTATO", AN, 6, 60, 100, 30, 50, 30, 74, 0, 6, 8, 70, 0, 30, 40, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 30, 30, 25, 20, 40, 30, 0, 3, 64, 20, 0, 64, 80, 30, 104, 0),
    PR("PARKER", AN, 10, 60, 90, 25, 55, 20, 104, 0, 4, 6, 40, 18, 10, 64, 30, 50, ARP_UP, RATE_8, 0, 64,
       64, 20, 0, 0, 0, 0, 0, 0, 3, 64, 20, 0, 64, 64, 35, 101, 0),
    PR("EDS", AN, 0, 40, 30, 20, 80, 40, 90, 0, 1, 0, 0, 0, 20, 64, 0, 0, ARP_UPDOWN, RATE_16, 1, 50,
       64, 0, 0, 0, 0, 0, 0, 60, 3, 64, 50, 0, 64, 64, 20, 117, 1),
    PR("OUTRUN BASS", AN, 0, 50, 60, 18, 45, 50, 94, 0, 0, 8, 64, 0, 20, 64, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 0, 0, 0, 0, 0, 0, 0, 3, 64, 0, 0, 64, 64, 0, 107, 0),
    PR("MIAMI PAD", PH, 80, 90, 110, 90, 90, 10, 64, 0, 0, 5, 50, 30, 14, 0, 0, 0, ARP_UP, RATE_8, 0, 64,
       80, 110, 0, 0, 20, 50, 40, 0, 3, 64, 30, 0, 64, 64, 55, 72, 0),
    PR("CHROME ARP", PH, 0, 40, 20, 24, 110, 20, 64, 0, 2, 0, 70, 70, 0, 0, 0, 0, ARP_UP, RATE_16, 1, 60,
       64, 30, 0, 0, 0, 0, 0, 0, 3, 64, 60, 0, 64, 64, 30, 97, 1),
    PR("NEON BELLS", PH, 0, 100, 0, 90, 127, 0, 64, 0, 6, 0, 80, 50, 4, 0, 0, 0, ARP_UP, RATE_8, 0, 64,
       64, 40, 0, 0, 0, 0, 0, 0, 3, 64, 30, 0, 90, 64, 60, 111, 0),
    PR("VHS STRINGS", AN, 60, 90, 112, 80, 60, 10, 64, 0, 0, 26, 64, 6, 0, 40, 24, 0, ARP_UP, RATE_8, 0, 64,
       100, 90, 20, 15, 0, 0, 0, 12, 3, 64, 30, 0, 64, 64, 50, 101, 0),
};
#define NPRESETS (sizeof PRESETS / sizeof PRESETS[0])

uint32_t vsynth_presets(void) { return NPRESETS; }
const struct vs_preset *vsynth_stock(uint32_t i) { return &PRESETS[i < NPRESETS ? i : 0]; }
const char *vsynth_page_name(uint32_t pg) { return pg < VS_PAGES ? PAGE_NAME[pg] : ""; }
const char *vsynth_engine_name(uint32_t e) { return engine_of(e)->name; }

int vsynth_record(struct vs_preset *dst, const void *src, uint32_t n)
{
    const uint8_t *s = (const uint8_t *)src;
    uint32_t k, np;
    if (!s || n < 2u + VS_NAME || s[0] == 0 || s[0] > VS_REC_VERSION || s[1] >= VS_ENGINES)
        return -1;
    defaults(dst, s[1]);
    for (k = 0; k < VS_NAME; k++)
        dst->name[k] = (char)(s[2 + k] >= 32u && s[2 + k] <= 126u ? s[2 + k] : 0);
    np = n - 2u - VS_NAME < VP_N ? n - 2u - VS_NAME : VP_N;   /* (version 1 has them all; a later one may have more) */
    for (k = 0; k < np; k++) {
        const struct vs_param *p = vsynth_param(k);
        uint8_t v = s[2 + VS_NAME + k];
        if (VS_PARAMS[k].page == PG_SOUND_A || VS_PARAMS[k].page == PG_SOUND_B) {
            const param_desc_t *d = slot_desc(dst->engine, k - VP_S0, 0);
            if (v < (uint8_t)d->min || v > (uint8_t)d->max)
                continue;
        } else if (v < p->lo || v > p->hi) {
            continue;                     /* (out of range: the default) */
        }
        dst->v[k] = v;
    }
    return 0;
}

/* ------------------------------------------------------------ main loop -> interrupt --- */
static volatile uint8_t active;
static struct vs_preset ram;              /* what plays (the main loop's copy; the interrupt copies its values) */
static volatile uint32_t want_seq, want_bpm = 120;
#define QLEN 64u
static volatile uint16_t q[QLEN];        /* note | on << 8 */
static volatile uint32_t q_head, q_tail, off_req;
static struct vs_stats st;

static int16_t *dl_l, *dl_r, *ch_buf;    /* the delay's lines, the chorus's (in the caller's memory) */
static int16_t *rv_buf;                  /* the reverb's (after them) */
#define DMAX 32768u
#define CMAX 2048u

/* the page's view of parameter p: SOUND slots from the engine of the record now */
const struct vs_param *vsynth_param(uint32_t p)
{
    static struct vs_param s;
    const param_desc_t *d;
    if (p >= VP_N)
        p = 0;
    if (VS_PARAMS[p].page != PG_SOUND_A && VS_PARAMS[p].page != PG_SOUND_B)
        return &VS_PARAMS[p];
    d = slot_desc(ram.engine, p - VP_S0, &trk[0]);
    s.name = d->label;
    s.page = VS_PARAMS[p].page;
    s.lo = (uint8_t)d->min;
    s.hi = (uint8_t)d->max;
    s.show = d->fmt == F_ENUM && d->names ? SHOW_NAMES : SHOW_NUM;
    s.names = d->names;
    return &s;
}

void vsynth_value_text(char *d, uint32_t p, int32_t v)
{
    const struct vs_param *q = vsynth_param(p);
    char t[8];
    uint32_t i = 0, k, u;
    int neg = 0;
    if (q->show == SHOW_NAMES && q->names) {
        const char *s = q->names[v < q->lo ? q->lo : v > q->hi ? q->hi : v];
        for (k = 0; s[k] && k < 11u; k++)
            d[k] = s[k];
        d[k] = 0;
        return;
    }
    if (q->show == SHOW_BIP)
        v -= 64;
    else if (q->show == SHOW_PLUS1)
        v += 1;
    if (v < 0)
        neg = 1, v = -v;
    u = (uint32_t)v;
    do {
        t[i++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u && i < sizeof t);
    k = 0;
    if (neg)
        d[k++] = '-';
    else if (q->show == SHOW_BIP && v)
        d[k++] = '+';
    while (i)
        d[k++] = t[--i];
    d[k] = 0;
}

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
    vsynth_load(&PRESETS[0]);
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

void vsynth_load(const struct vs_preset *p)
{
    ram = *p;
    __asm__ volatile("" ::: "memory");
    want_seq++;
}

const struct vs_preset *vsynth_now(void) { return &ram; }
int32_t vsynth_get(uint32_t p) { return p < VP_N ? ram.v[p] : 0; }

void vsynth_set(uint32_t p, int32_t v)
{
    const struct vs_param *q;
    if (p >= VP_N)
        return;
    q = vsynth_param(p);
    ram.v[p] = (uint8_t)(v < q->lo ? q->lo : v > q->hi ? q->hi : v);
    __asm__ volatile("" ::: "memory");
    want_seq++;
}

void vsynth_tempo(uint32_t bpm)
{
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
static uint8_t cv[VP_N];                  /* the values in use (a copy of ram.v) */
static uint8_t c_engine = 0xFF;
static uint32_t seen_seq = 0xFFFFFFFFu, off_seen;
static uint32_t c_bpm = 120;
static int32_t env_a, env_d, env_r;       /* per control tick: the attack's step (Q24), decay / release (Q16) */
static uint32_t lfo_ph, vage;
static const engine_t *eng = &ENG_ANALOG;
static int32_t hf_ic[NVOICE][2];          /* M7: PHASE's filter (ours, after the engine: it has none), per voice */

/* a time 0..127 -> control ticks (1 ms .. ~5.9 s, cubed) */
static int32_t ticks_of(int32_t p)
{
    int32_t ms = 1 + p * p * p / 350;
    int32_t t = ms * 1378 / 1000;          /* 44100 / 32 per second */
    return t < 1 ? 1 : t;
}

static void voices_kill(void)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        trk[0].v[i].active = 0, trk[0].v[i].stage = 0;
}

static void sound_update(void)
{
    uint32_t k, s = want_seq, e;
    if (s == seen_seq)
        return;
    for (k = 0; k < VP_N; k++)
        cv[k] = ram.v[k];
    e = ram.engine < VS_ENGINES ? ram.engine : VS_ANALOG;
    c_bpm = want_bpm;
    __asm__ volatile("" ::: "memory");
    if (s != want_seq)                    /* (changed while copying: again next block) */
        return;
    seen_seq = s;
    if (e != c_engine) {                  /* another engine: its voices' state is another's (start them afresh) */
        voices_kill();
        c_engine = (uint8_t)e;
        eng = engine_of(e);
    }
    trk[0].p[P_FTYPE] = cv[VP_FTYPE];
    for (k = 0; k < 8u; k++)
        if (SLOTS[e][k] < S_PWM)
            trk[0].p[SLOTS[e][k]] = cv[VP_S0 + k];
    if (e == VS_ANALOG) {                 /* (ANALOG: the FILTER page's cutoff and resonance are its own) */
        trk[0].p[P_E4] = cv[VP_CUT];
        trk[0].p[P_E5] = cv[VP_RES];
    }
    env_a = (1 << 24) / ticks_of(cv[VP_ATK]);
    env_d = 65536 / ticks_of(cv[VP_DEC] / 2);
    env_r = 65536 / ticks_of(cv[VP_REL] / 2 + 8);
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
            hf_ic[best][0] = hf_ic[best][1] = 0;
        }
        v->active = 1;
        eng->note_on(&trk[0], v);
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
    int32_t sus = (int32_t)cv[VP_SUS] << 17;   /* 0..127 -> Q24 */
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
static uint32_t arp_left, arp_gate_left, arp_i, arp_frac;
static int32_t arp_dir = 1;
static int32_t arp_note = -1;
static uint32_t arp_rng = 0x1234567u;
static uint8_t arp_was;                   /* the arp on in the last block */

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

/* the held keys sorted up, repeated an octave up for each of OCTAVES; the arpeggio's next note */
static int32_t arp_next(void)
{
    uint8_t s[48];
    uint32_t i, k, n = nheld, oct = cv[VP_ARP_OCT] + 1u, mode = cv[VP_ARP_PAT];
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
    for (k = 1; k < oct; k++)             /* (M7: OCTAVES 2, 3) */
        for (i = 0; i < nheld; i++)
            s[k * nheld + i] = (uint8_t)(s[i] + 12u * k > 127u ? s[i] : s[i] + 12u * k);
    n *= oct;
    if (mode == ARP_RANDOM) {             /* any note but the one just played */
        uint32_t r;
        arp_rng = arp_rng * 1664525u + 1013904223u;
        r = (arp_rng >> 16) % n;
        if (n > 1u && r == arp_i)
            r = (r + 1u + (arp_rng >> 8) % (n - 1u)) % n;
        arp_i = r;
    } else if (mode == ARP_UP) {
        arp_i = arp_i + 1u >= n ? 0 : arp_i + 1u;
    } else if (mode == ARP_DOWN) {
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

/* one control tick of the arpeggiator: a step every 1/4, 1/8, 1/8 triplet or 1/16 at the tempo, the note held GATE
 * / 128 of it. The steps counted in 1/256 control ticks, so the triplets keep time */
static void arp_tick(void)
{
    static const uint32_t PER_BEAT[RATES] = {1, 2, 3, 4};
    uint32_t r = cv[VP_ARP_RATE] < RATES ? cv[VP_ARP_RATE] : 1u;
    uint32_t step256 = 44100u * 60u * 8u / (c_bpm * PER_BEAT[r]) * 32u / CTL;
    uint32_t step = step256 >> 8, g;
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
    g = step * cv[VP_ARP_GATE] / 128u;
    arp_gate_left = g ? g : 1u;
}

static void keys_in(void)
{
    uint32_t t, i, arp = cv[VP_ARP_ON];
    if (off_req != off_seen) {
        off_seen = off_req;
        for (i = 0; i < NVOICE; i++)
            if (trk[0].v[i].active)
                trk[0].v[i].gate = 0, trk[0].v[i].stage = 3;
        nheld = 0;
        arp_note = -1;
        q_tail = q_head;
        vsynth_drums_off();               /* (M7: the drum voices too) */
    }
    for (t = q_tail; t != q_head; t++) {
        uint32_t m = q[t % QLEN], note = m & 0xFFu;
        if (m & 0x100u) {
            held_add(note);
            if (!arp)
                voice_start(note);
            else if (nheld == 1u)
                arp_left = 0, arp_i = cv[VP_ARP_PAT] == ARP_DOWN ? 0 : 0xFFFFFFFFu, arp_dir = 1;   /* from the start */
        } else {
            held_del(note);
            if (!arp)
                voice_stop(note);
        }
    }
    q_tail = t;
    if (!arp && arp_note >= 0) {          /* the arpeggio turned off: its note ends */
        voice_stop((uint32_t)arp_note);
        arp_note = -1;
    }
    if (arp && !arp_was)                  /* turned on with keys held: from the start */
        arp_left = 0, arp_i = 0xFFFFFFFFu, arp_dir = 1;
    arp_was = (uint8_t)arp;
}

/* ---------------------------------------------------------------- effects --- */
static uint32_t dw, cw, cl_ph;           /* write positions, the chorus's LFO */
static int32_t fb_lp_l, fb_lp_r;
static int32_t dly_quiet = 1, rev_quiet = 1;   /* the line has gone silent (skipped while its mix is 0) */

/* the chorus (Juno-like): a line read at two taps, around 3.5 ms, swept by a 0.5 Hz triangle in opposite phase;
 * DEPTH 64 sweeps +-2.5 ms (M6's) */
static int32_t chorus_tap(int32_t tri, uint32_t w, int32_t depth)
{
    uint32_t d = (uint32_t)(154 * 256 + tri * 220 * depth / 64);   /* Q8 samples: 154 +- 110 at 64 */
    uint32_t pos = (w << 8) - d, i = (pos >> 8) & (CMAX - 1u), f = pos & 255u;
    int32_t a = ch_buf[i], b = ch_buf[(i + 1u) & (CMAX - 1u)];
    return a + (((b - a) * (int32_t)f) >> 8);
}

/* the reverb (ours, a Schroeder-Moorer kind): four damped feedback combs in parallel on the mono sum, then two
 * all-passes each for left and right (different lengths: the width). Lines in the caller's memory (rv_buf).
 * SIZE: the combs' feedback (0.64 .. 0.95; 64: 0.87, M6's); DAMPING: their low-pass (64: 0.25, M6's) */
static const uint16_t REV_COMB[4] = {1116, 1188, 1277, 1356};
static const uint16_t REV_AP[4] = {556, 441, 579, 464};   /* left two, right two */
static uint16_t rv_ci[4], rv_ai[4];
static int32_t rv_filt[4], rv_fb = 28508, rv_damp = 64;

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
    for (k = 0; k < 4u; k++) {
        int32_t y = b[rv_ci[k]];
        rv_filt[k] = y - (y - rv_filt[k]) * rv_damp / 256;
        b[rv_ci[k]] = (int16_t)clamp(in + rv_filt[k] * rv_fb / 32768, -32767, 32767);
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

/* the bit-crush: fewer bits (16 down to 7) and a held sample (1 to 8 samples) as CRUSH goes up */
static int32_t cr_hold, cr_n;

/* M7 (D039), ours. The fuzz: the sound driven 2x .. 64x into a tanh curve (dsp.c softclip), then a one-pole tone
 * filter (~3 kHz, darker as FUZZ rises); its level follows the dry sound's (the two envelopes' ratio, a control tick
 * late), so turning FUZZ up changes the colour, not the loudness */
static int32_t fz_lp, fz_ein, fz_eout, fz_norm = 4096;

/* the tremolo: the level dips by up to DEPTH at RATE (0.1 .. 12 Hz, a sine), ramped across each control block */
static uint32_t tr_ph;
static int32_t tr_g = 32768;

/* the phaser: six first-order all-passes (no buffer) swept together by a 0.05 .. 5 Hz triangle between ~300 Hz and
 * ~300 Hz + DEPTH x 2.5 kHz, mixed with the dry sound (MIX 127: half and half, the deepest notches) */
#define PH_STAGES 6u
static int32_t ph_s[PH_STAGES], ph_a = -15000;
static uint32_t ph_ph;

/* the delay's length: the note value at the tempo, halved until it fits the line */
static int32_t delay_len(void)
{
    static const uint8_t TWELFTHS[8] = {3, 4, 6, 9, 8, 12, 18, 24};   /* of a beat: 1/16 1/8T 1/8 1/8D 1/4T 1/4 .. */
    int32_t n = (int32_t)(44100u * 60u / c_bpm * TWELFTHS[cv[VP_DLY_TIME] & 7u] / 12u);
    while (n > (int32_t)DMAX - 1)
        n /= 2;
    return n;
}

/* --------------------------------------------------------- M7 (D041): the drum kits' voices --- */
/* 808 and CHIP: up to VS_DRUM_VOICES voices of their own (never the synth's), each a sine part (our own: a pitch that
 * falls from f0 to f1, then decays: the 808's kick and toms, the snare's body, the rim) and / or an engine part (NOISE
 * for the 808's snare, hats and clap; LOFI for every CHIP sound), each with its own exponential decay. The clap
 * restarts its noise twice, 12 ms apart. The closed hat chokes the open one. Not through the synth's effects; not
 * at the preset's LEVEL: their own level (the drum volume setting, the hit's velocity). */
enum { DE_NONE, DE_NOISE, DE_LOFI };
struct dspec {
    uint8_t eng;                          /* DE_* */
    int8_t e[8];                          /* the engine's parameters (NOISE: MODE COLR FREQ RES TRK DENS DRFT CRSH;
                                           * LOFI: CHIP WAVE DUTY CRSH SWP VIB ARP TONE) */
    uint8_t note, ntau, nlev;             /* its note, decay (4 ms units), level */
    uint16_t f0, f1;                      /* the sine part: from f0 to f1 Hz (0: none) */
    uint8_t sweep, stau, slev;            /* its fall (2 ms units), decay (4 ms units), level */
    uint8_t claps;
};
static const struct dspec KIT[2][DR_N] = {
    {   /* 808 */
        {DE_NONE, {0}, 0, 0, 0, 150, 48, 20, 45, 127, 0},                                   /* kick */
        {DE_NOISE, {0, 20, 100, 30, 0, 0, 0, 0}, 60, 15, 110, 200, 175, 10, 10, 70, 0},     /* snare */
        {DE_NOISE, {0, 0, 120, 90, 0, 0, 0, 0}, 60, 4, 90, 0, 0, 0, 0, 0, 0},              /* closed hat */
        {DE_NOISE, {0, 0, 118, 90, 0, 0, 0, 0}, 60, 28, 80, 0, 0, 0, 0, 0, 0},             /* open hat */
        {DE_NOISE, {0, 10, 95, 70, 0, 0, 0, 0}, 60, 12, 110, 0, 0, 0, 0, 0, 3},            /* clap */
        {DE_NONE, {0}, 0, 0, 0, 140, 95, 30, 38, 110, 0},                                  /* tom low */
        {DE_NONE, {0}, 0, 0, 0, 220, 150, 30, 32, 110, 0},                                 /* tom high */
        {DE_NOISE, {0, 0, 110, 100, 0, 0, 0, 0}, 60, 2, 50, 1700, 1600, 2, 3, 80, 0},      /* rim */
    },
    {   /* CHIP */
        {DE_LOFI, {2, 1, 0, 0, 127, 0, 0, 100}, 40, 30, 127, 0, 0, 0, 0, 0, 0},           /* kick: triangle, swept */
        {DE_LOFI, {0, 3, 0, 0, 0, 0, 0, 110}, 72, 17, 100, 0, 0, 0, 0, 0, 0},             /* snare: noise */
        {DE_LOFI, {0, 3, 0, 0, 0, 0, 0, 127}, 100, 5, 70, 0, 0, 0, 0, 0, 0},              /* closed hat */
        {DE_LOFI, {0, 3, 0, 0, 0, 0, 0, 127}, 100, 25, 60, 0, 0, 0, 0, 0, 0},             /* open hat */
        {DE_LOFI, {0, 3, 0, 0, 0, 0, 0, 100}, 84, 10, 90, 0, 0, 0, 0, 0, 3},              /* clap */
        {DE_LOFI, {2, 1, 0, 0, 60, 0, 0, 100}, 45, 30, 110, 0, 0, 0, 0, 0, 0},            /* tom low */
        {DE_LOFI, {2, 1, 0, 0, 60, 0, 0, 100}, 52, 25, 110, 0, 0, 0, 0, 0, 0},            /* tom high */
        {DE_LOFI, {2, 0, 64, 0, 0, 0, 0, 127}, 84, 4, 70, 0, 0, 0, 0, 0, 0},              /* rim: pulse */
    },
};
static track_t dtrk;                      /* the drum voices' engine parameters (set per voice before it runs) */
static struct dvoice {
    voice_t v;
    const struct dspec *sp;
    uint8_t on, inst, clap;
    int32_t namp, namp0, ndec, samp, sdec;   /* Q15 amplitudes (namp0: the engine part's start) and their decay a
                                              * control tick (Q15) */
    uint32_t sph, sinc, sinc1, sfall;     /* the sine part's phase, increment, end increment, fall (Q15) */
    uint32_t age, ticks;
} dvx[VS_DRUM_VOICES];
static struct dhit { uint8_t ctl, kit, inst, vel, vol; } dq[16];
static uint32_t ndq, dage;

static int32_t decay_of(uint32_t units4ms)   /* the per-control-tick factor (Q15) of a time constant */
{
    int32_t t = (int32_t)units4ms * 4 * 1378 / 1000;
    return t < 1 ? 0 : 32768 - 32768 / (t + 1);
}

static void dspec_params(const struct dspec *sp)
{
    uint32_t k;
    for (k = 0; k < 8u; k++)
        dtrk.p[P_E0 + k] = sp->e[k];
    dtrk.p[P_FTYPE] = 0;
}

void vsynth_drum(uint32_t ctl, uint32_t kit, uint32_t inst, uint32_t vel, uint32_t vol)
{
    if (ndq < sizeof dq / sizeof dq[0] && kit >= KIT_808 && kit < KITS && inst < DR_N && vel)
        dq[ndq].ctl = (uint8_t)ctl, dq[ndq].kit = (uint8_t)kit, dq[ndq].inst = (uint8_t)inst,
        dq[ndq].vel = (uint8_t)vel, dq[ndq].vol = (uint8_t)vol, ndq++;
}

void vsynth_drums_off(void)
{
    uint32_t i;
    for (i = 0; i < VS_DRUM_VOICES; i++)
        dvx[i].on = 0;
}

static void drum_start(const struct dhit *h)
{
    static const uint8_t VEL[4] = {0, 45, 90, 127};
    const struct dspec *sp = &KIT[h->kit - KIT_808][h->inst];
    uint32_t i, best = 0, oldest = 0;
    int32_t a = VEL[h->vel & 3u] * h->vol;          /* 0..16129 */
    struct dvoice *d;
    if (h->inst == DR_CHH)                          /* the closed hat chokes the open one */
        for (i = 0; i < VS_DRUM_VOICES; i++)
            if (dvx[i].on && dvx[i].inst == DR_OHH)
                dvx[i].on = 0;
    for (i = 0; i < VS_DRUM_VOICES; i++) {
        if (!dvx[i].on) {
            best = i;
            break;
        }
        if (dage - dvx[i].age > oldest)
            oldest = dage - dvx[i].age, best = i;
    }
    d = &dvx[best];
    for (i = 0; i < sizeof d->v; i++)
        ((uint8_t *)&d->v)[i] = 0;
    d->sp = sp;
    d->inst = h->inst;
    d->clap = sp->claps;
    d->age = ++dage;
    d->ticks = 0;
    d->namp = d->namp0 = sp->eng ? sp->nlev * a / 63 : 0;   /* Q15 */
    d->ndec = decay_of(sp->ntau);
    d->samp = sp->f0 ? sp->slev * a / 63 : 0;
    d->sdec = decay_of(sp->stau);
    d->sph = 0;
    d->sinc = sp->f0 * 97392u;                      /* 2^32 / 44100 a Hz */
    d->sinc1 = sp->f1 * 97392u;
    d->sfall = (uint32_t)decay_of(sp->sweep);
    if (sp->eng) {
        d->v.note = sp->note;
        d->v.vel = 127;
        d->v.gate = 0;
        d->v.stage = 2;
        d->v.active = 1;
        d->v.env = 1 << 24;
        d->v.age = d->age;
        d->v.pitch16 = d->v.pitch_cur = (int32_t)sp->note * 16;
        dspec_params(sp);
        (sp->eng == DE_NOISE ? &ENG_NOISE : &ENG_LOFI)->note_on(&dtrk, &d->v);
    }
    d->on = 1;
    st.drum_hits++;
}

/* one control tick of every drum voice into out[] (CTL samples, added) */
static uint32_t drums_tick(int32_t *out)
{
    static int32_t eb[CTL];
    uint32_t k, i, n = 0;
    for (k = 0; k < VS_DRUM_VOICES; k++) {
        struct dvoice *d = &dvx[k];
        const struct dspec *sp = d->sp;
        if (!d->on)
            continue;
        n++;
        if (d->namp) {                              /* the engine part */
            vmod_t m;
            int32_t a0 = d->namp;
            d->ticks++;
            if (d->clap && d->ticks % 16u == 0) {   /* the clap: twice more, 12 ms apart */
                d->clap--;
                d->namp = d->namp0 * 4 / 5;
            } else {
                d->namp = d->namp * d->ndec / 32768;
            }
            m.inc = pitch_inc((uint32_t)clamp(d->v.pitch16, 0, 2047));
            m.pitch16 = d->v.pitch16;
            m.amp0 = a0 > 32767 ? 32767 : a0;
            m.amp1 = d->namp > 32767 ? 32767 : d->namp;
            m.cutoff = 0;
            m.shape = 64 << 8;
            m.envq15 = m.amp1;
            m.fine = 0;
            dspec_params(sp);
            for (i = 0; i < CTL; i++)
                eb[i] = 0;
            (sp->eng == DE_NOISE ? &ENG_NOISE : &ENG_LOFI)->render(&dtrk, &d->v, eb, CTL, &m);
            for (i = 0; i < CTL; i++)               /* (the engines' voices sit low beside the sine part: NOISE x 2,
                                                     * LOFI x 3.5) */
                out[i] += sp->eng == DE_LOFI ? eb[i] * 7 / 2 : eb[i] * 2;
            if (d->namp < 8 && !d->clap)
                d->namp = 0;
        }
        if (d->samp) {                              /* the sine part */
            int32_t a = d->samp, a1 = d->samp * d->sdec / 32768;
            for (i = 0; i < CTL; i++) {
                int32_t g = a + (a1 - a) * (int32_t)i / (int32_t)CTL;
                out[i] += (sine_i(d->sph) * (g > 32767 ? 32767 : g) >> 15) * 3;   /* (~ a synth voice's level) */
                d->sph += d->sinc;
            }
            d->sinc = d->sinc1 + ((d->sinc - d->sinc1) >> 15) * (uint32_t)d->sfall;   /* (f0 > f1; < 2^13 x 2^15) */
            d->samp = a1 < 8 ? 0 : a1;
        }
        if (!d->namp && !d->samp)
            d->on = 0;
    }
    return n;
}

/* ------------------------------------------------------------------- the render --- */
void vsynth_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    static int32_t mix[CTL], dmix[CTL];
    uint32_t b, i, k, voices = 0, fx = 0, dvoices = 0;
    int32_t cho_mix, cho_depth, send, dfb, dlen, rev, crush, cr_bits, cr_every, level, dpk = 0, rpk = 0;
    int32_t fuzz, fz_gain, fz_tone, tdepth, pmix, tr_g0;
    if (!active || !dl_l) {
        for (i = 0; i < 2u * n; i++)
            lr[i] = 0;
        ndq = 0;
        return;
    }
    sound_update();
    keys_in();
    cho_mix = cv[VP_CHO_MIX];
    cho_depth = cv[VP_CHO_DEPTH];
    send = cv[VP_DLY_MIX];
    dfb = 20 + cv[VP_DLY_FB] * 84 / 127;                    /* the loop's gain each way, Q7: 64 -> 62 (a round trip
                                                              * of 0.23, M6's 0.25), 127 -> 104 (0.66) */
    rev = cv[VP_REV_MIX];
    crush = cv[VP_CRUSH];
    {   /* LEVEL: 100 as made, 12 steps an octave (half a dB a step): 127 x 4.76, 0 x 0.003; Q12 */
        static const uint16_t OCT12[12] = {4096, 4340, 4598, 4871, 5161, 5468, 5793, 6137, 6502, 6889, 7298, 7732};
        int32_t e = (int32_t)cv[VP_LEVEL] - 100 + 120, ip = e / 12 - 10;   /* (e >= 20: no negative division) */
        level = ip >= 0 ? OCT12[e % 12] << ip : OCT12[e % 12] >> -ip;
    }
    {
        int32_t s = 127 - cv[VP_REV_SIZE];
        rv_fb = 31130 - s * s * 2622 / 4096;   /* 0.64 .. 0.95, 64: 0.87 */
        rv_damp = cv[VP_REV_DAMP];
    }
    cr_bits = 16 - crush * 9 / 127;                          /* 16 .. 7 bits (the voices sit well below full scale:
                                                              * fewer would leave a quiet sound nothing) */
    cr_every = 1 + crush / 18;                               /* 1 .. 8 samples held */
    dlen = delay_len();
    fuzz = cv[VP_FUZZ];
    fz_gain = 2 + fuzz * fuzz * 62 / 16129;                 /* 2 .. 64 */
    fz_tone = 14000 - fuzz * 50;                             /* the tone filter's coefficient, Q15 */
    tdepth = cv[VP_TREM_DEPTH];
    pmix = cv[VP_PHA_MIX];
    if (fuzz)
        fx |= 1u << FXB_FUZZ;
    if (tdepth)
        fx |= 1u << FXB_TREM;
    if (pmix)
        fx |= 1u << FXB_PHASER;
    if (crush)
        fx |= 1u << FXB_CRUSH;
    if (cho_mix)
        fx |= 1u << FXB_CHORUS;
    if (send || !dly_quiet)
        fx |= 1u << FXB_DELAY;
    if (rev || !rev_quiet)
        fx |= 1u << FXB_REVERB;
    for (b = 0; b + CTL <= n; b += CTL) {
        int32_t lfo;
        if (cv[VP_ARP_ON])
            arp_tick();
        lfo_ph += (uint32_t)cv[VP_S6] * 4096u;              /* ~0.04 .. 5 Hz at the control rate (ANALOG: LFO RATE) */
        lfo = (int32_t)(lfo_ph >> 16) - 32768;
        lfo = (lfo < 0 ? -lfo : lfo) * 2 - 32768;            /* a triangle, Q15 */
        for (i = 0; i < CTL; i++)
            mix[i] = dmix[i] = 0;
        for (k = 0; k < ndq; k++)                            /* M7: the drum hits due at this control tick */
            if (dq[k].ctl == b / CTL)
                drum_start(&dq[k]);
        k = drums_tick(dmix);
        dvoices = k > dvoices ? k : dvoices;
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
            m.cutoff = ((cv[VP_FENV] - 64) * e) >> 6;
            m.shape = (64 << 8) + ((lfo * cv[VP_S7]) >> 9);   /* (ANALOG: PWM DEPTH) */
            m.envq15 = e;
            m.fine = 0;
            if (c_engine == VS_PHASE) {                      /* M7: PHASE, then our filter (the FILTER page) */
                static int32_t vb[CTL];
                int32_t cut = (cv[VP_CUT] << 8) + m.cutoff, kd = ANALOG_K(cv[VP_RES]);
                uint32_t ft = cv[VP_FTYPE], j;
                m.cutoff = 0;                                /* (PHASE would add it to its DCW) */
                m.shape = 64 << 8;
                for (j = 0; j < CTL; j++)
                    vb[j] = 0;
                eng->render(&trk[0], v, vb, CTL, &m);
                if (cut >= (127 << 8) && !ft && !cv[VP_RES]) {   /* (open: no filter) */
                    for (j = 0; j < CTL; j++)
                        mix[j] += vb[j];
                } else {
                    tsvf_t f;
                    int32_t *ic = hf_ic[k];
                    tsvf_coef_k(&f, cut, kd);
                    for (j = 0; j < CTL; j++)
                        mix[j] += analog_tap(&f, kd, vb[j] >> 1, &ic[0], &ic[1], ft) * 2;
                }
            } else {
                eng->render(&trk[0], v, mix, CTL, &m);
            }
        }
        tr_g0 = tr_g;
        {   /* the tremolo's gain at the end of this block (from tr_g0 at its start), the phaser's coefficient */
            if (tdepth) {
                int32_t rt = cv[VP_TREM_RATE];
                tr_ph += (uint32_t)(100 + rt * rt * 11900 / 16129) * 3117u;   /* 0.1 .. 12 Hz a control tick */
                tr_g = 32768 - tdepth * ((32768 + sine_i(tr_ph)) >> 1) / 127;
            } else {
                tr_g = 32768;
            }
            if (pmix) {
                int32_t pr = cv[VP_PHA_RATE], tri, w;
                ph_ph += (uint32_t)(50 + pr * pr * 4950 / 16129) * 3117u;    /* 0.05 .. 5 Hz */
                tri = (int32_t)(ph_ph >> 16) - 32768;
                tri = tri < 0 ? -tri : tri;                                  /* 0..32768 */
                w = 1400 + (tri >> 3) * cv[VP_PHA_DEPTH] / 127 * 11700 / 4096;   /* 2 pi f / FS, Q15 */
                ph_a = -(16384 - (w >> 1));                                  /* Q14 */
            }
            for (i = 0; i < CTL; i++) {
                int32_t x = mix[i] >> 3;
                if (fuzz) {                                  /* M7: the fuzz, first */
                    int32_t y = softclip(clamp(x, -30000, 30000) * fz_gain), ay, ax = x < 0 ? -x : x;
                    fz_lp += (y - fz_lp) * fz_tone / 32768;  /* (|y - lp| < 65536, tone < 14001: no overflow) */
                    ay = fz_lp < 0 ? -fz_lp : fz_lp;
                    fz_ein += (ax - fz_ein) / 512;           /* (~12 ms) */
                    fz_eout += (ay - fz_eout) / 512;
                    x = fz_lp * fz_norm / 4096;
                }
                mix[i] = x;
            }
            if (fuzz)                                        /* the level: dry's x 1.2 (it sounds denser) */
                fz_norm = fz_eout > 64 ? clamp(fz_ein * 4915 / fz_eout, 0, 4 * 4096) : fz_norm;
        }
        for (i = 0; i < CTL; i++) {
            int32_t x = mix[i], l, r;
            if (crush) {                                     /* the bit-crush */
                if (++cr_n >= cr_every) {
                    int32_t qq = 1 << (16 - cr_bits);
                    cr_n = 0;
                    cr_hold = clamp(x, -32767, 32767) / qq * qq;
                }
                x = cr_hold;
            }
            if (tdepth)                                      /* M7: the tremolo (ramped across the block) */
                x = clamp(x, -100000, 100000) * ((tr_g0 + (tr_g - tr_g0) * (int32_t)i / (int32_t)CTL) >> 3) >> 12;
            if (pmix) {                                      /* M7: the phaser (Q14 coefficient, lines within +-1e5:
                                                              * no product past 2^31) */
                int32_t y = clamp(x, -100000, 100000);
                uint32_t j;
                x = y;
                for (j = 0; j < PH_STAGES; j++) {
                    int32_t o = clamp((ph_a * y >> 14) + ph_s[j], -100000, 100000);
                    ph_s[j] = clamp(y - (ph_a * o >> 14), -100000, 100000);
                    y = o;
                }
                x = x + ((y - x) * pmix >> 8);
            }
            /* chorus (its line is written always: no stale sound when it comes on) */
            ch_buf[cw & (CMAX - 1u)] = (int16_t)clamp(x, -32767, 32767);
            if (cho_mix) {
                int32_t tri, wl, wr;
                cl_ph += 0x0000C2EBu;                        /* 0.5 Hz at 44.1 kHz (2^32 / 88200) */
                tri = (int32_t)(cl_ph >> 16) - 32768;
                tri = ((tri < 0 ? -tri : tri) * 2 - 32768) >> 8;   /* -128..127 */
                wl = chorus_tap(tri, cw, cho_depth);
                wr = chorus_tap(-tri, cw, cho_depth);
                l = x + ((wl * cho_mix) >> 7) - ((x * cho_mix) >> 9);
                r = x + ((wr * cho_mix) >> 7) - ((x * cho_mix) >> 9);
            } else {
                l = r = x;
            }
            cw++;
            if (fx >> FXB_DELAY & 1u) {   /* the delay: two lines, each feeding the other (ping-pong), a low-pass in
                                           * the loop; FEEDBACK 64: a round trip of 1/4 (M6's) */
                uint32_t rd = (dw - (uint32_t)dlen) & (DMAX - 1u);
                int32_t dl = dl_l[rd], dr = dl_r[rd];
                fb_lp_l += (dr - fb_lp_l) / 4;   /* (/ rounds toward 0: the loop reaches silence, M7) */
                fb_lp_r += (dl - fb_lp_r) / 4;
                dl_l[dw & (DMAX - 1u)] = (int16_t)clamp((l + r) / 2 * send / 128 + fb_lp_l * dfb / 128, -32767, 32767);
                dl_r[dw & (DMAX - 1u)] = (int16_t)(fb_lp_r * dfb / 128);   /* (|lp| <= 32767, dfb <= 104: within
                                                              * 26,622, no clamp; a clamp here compiles to a constant
                                                              * the build's ROM check refuses) */
                dw++;
                l += dl * 3 >> 2;
                r += dr * 3 >> 2;
                dl = dl < 0 ? -dl : dl;
                dpk = dl > dpk ? dl : dpk;
                dr = dr < 0 ? -dr : dr;
                dpk = dr > dpk ? dr : dpk;
            }
            if (fx >> FXB_REVERB & 1u) {  /* the reverb, last */
                int32_t vl, vr;
                reverb((l + r) / 8 * rev / 128, &vl, &vr);   /* (/: no -1 floor feeding it) */
                l += vl * 4;
                r += vr * 4;
                vl = vl < 0 ? -vl : vl;
                rpk = vl > rpk ? vl : rpk;
            }
            /* LEVEL (100: as is), to 24 bits at MASTER (16-bit x master / 16), a soft limit, then the clamp */
            l = clamp(l, -300000, 300000);                   /* (LEVEL up to x 4.76, Q12: within 2^31) */
            r = clamp(r, -300000, 300000);
            l = clamp((l >> 2) * level / 1024 + (dmix[i] >> 3), -98304, 98304) * (int32_t)master_q12 / 16;   /* (M7:
                                                              * the drums after the preset's LEVEL) */
            r = clamp((r >> 2) * level / 1024 + (dmix[i] >> 3), -98304, 98304) * (int32_t)master_q12 / 16;
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
    if (fx >> FXB_DELAY & 1u)            /* (a line silent for a block with its mix at 0: skipped from then on) */
        dly_quiet = !send && dpk < 2;
    if (fx >> FXB_REVERB & 1u)
        rev_quiet = !rev && rpk < 2;
    ndq = 0;
    st.drum_voices = dvoices;
    if (dvoices > st.drum_voices_max)
        st.drum_voices_max = dvoices;
    st.fx = fx;
    st.voices = voices / (n / CTL ? n / CTL : 1u);
    if (st.voices > st.voices_max)
        st.voices_max = st.voices;
}
