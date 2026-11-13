/* Licence: TBD (see D017) */
/* Format 2.0 (M5, D029): a layer's level built for playing (song.h song_build). A stored level comes from its NT
 * chunk (bars as patterns); a generated one is made here from the chord chart, the metre and the style, by the same
 * rules as tools/fm1song/arrange.py (harmony L1-5, bass L1-4, drums L1-3). Either way the notes then go through the
 * same canonical step as arrange.canonical, so the events are exactly the laptop's: tools/gen_golden.c prints them
 * and tools/fm1song's tests compare them with the Python generators for every song and level.
 *
 * Standalone C like song.c (fuzzed with it). No 64-bit division (the CPU has none); every tick fits 32 bits (a song
 * is at most 999 bars of 7 beats at 480 ticks). The output is written into the caller's buffer only: first the
 * notes as 16-byte records (start, end, pitch, velocity), then, in the same place, their events as the 8-byte
 * records of song.h (song_event_get): a note-on and its note-off take the note's 16 bytes. So a level of n notes
 * needs 16 n bytes, and song_open has counted n for every level (struct song_layer need). */
#include "song.h"

#define PPQ 480u
#define NOTE_REC 16u
#define EV_REC 8u
enum { KICK = 36, STICK = 37, SNARE = 38, HAT = 42, RIDE = 51, CRASH = 49, TOM_LO = 45, TOM_HI = 50 };
/* arrange.STYLES: per style (STYLE_NAMES order) what harmony, bass and drums do. M8 (D048, 2.2): five more, table
 * driven (T_*: the tables below; the same as arrange.RHYTHM, BASS_LINE, GROOVE) */
enum { H_WALTZ, H_BALLAD, H_SHUFFLE, H_STRIDE, H_EIGHTHS, H_TABLE };
enum { D_WALTZ, D_BALLAD, D_SHUFFLE, D_MARCH, D_POP, D_TRAIN, D_TABLE };
static const uint8_t H_OF[SONG_STYLES] = {H_WALTZ, H_BALLAD, H_SHUFFLE, H_STRIDE, H_EIGHTHS, H_EIGHTHS,
                                          H_TABLE, H_TABLE, H_TABLE, H_TABLE, H_TABLE};
static const uint8_t BOOGIE[SONG_STYLES] = {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t D_OF[SONG_STYLES] = {D_WALTZ, D_BALLAD, D_SHUFFLE, D_MARCH, D_POP, D_TRAIN,
                                          D_TABLE, D_TABLE, D_TABLE, D_TABLE, D_TABLE};
#define FIRST_TABLE 6u                    /* rock, indie, bossa, funk, reggae */
#define STEP (PPQ / 4u)                   /* the tables' grid: a 16th note */
struct rstep { uint8_t st, len, vel; };
struct bstep { uint8_t st, len, what, vel; };
enum { BR, BF, BO, BN, BA };              /* the bass line's notes: root, fifth, octave, next root, a step into it */
/* harmony L2 / L3 (per style: up to 2 bars of up to 8 hits; len 0 ends a bar); UPPER: the lowest note left out */
static const struct rstep RHYTHM[5][2][9] = {
    {{{0, 2, 62}, {2, 2, 50}, {4, 2, 58}, {6, 2, 50}, {8, 2, 60}, {10, 2, 50}, {12, 2, 58}, {14, 2, 50}}},   /* rock */
    {{{0, 6, 62}, {6, 6, 56}, {12, 4, 58}}},                                                                 /* indie */
    {{{0, 2, 58}, {3, 2, 52}, {6, 3, 54}, {10, 2, 52}, {13, 3, 54}},
     {{2, 2, 52}, {5, 2, 54}, {8, 2, 56}, {11, 3, 52}}},                                                     /* bossa */
    {{{0, 1, 70}, {3, 1, 56}, {6, 1, 62}, {7, 1, 50}, {10, 1, 64}, {12, 1, 56}, {14, 1, 60}}},               /* funk */
    {{{2, 1, 64}, {6, 1, 64}, {10, 1, 64}, {14, 1, 64}}},                                                    /* reggae */
};
static const uint8_t RHYTHM_BARS[5] = {1, 1, 2, 1, 1};
static const uint8_t UPPER[5] = {0, 0, 0, 1, 1};
static const struct bstep BASS_LINE[5][9] = {
    {{0, 2, BR, 88}, {2, 2, BR, 74}, {4, 2, BR, 80}, {6, 2, BR, 74}, {8, 2, BR, 86}, {10, 2, BR, 74}, {12, 2, BF, 80},
     {14, 2, BA, 76}},
    {{0, 6, BR, 90}, {6, 6, BR, 80}, {12, 4, BF, 82}},
    {{0, 6, BR, 90}, {6, 2, BF, 74}, {8, 6, BF, 84}, {14, 2, BN, 76}},
    {{0, 2, BR, 96}, {3, 1, BO, 72}, {6, 1, BR, 84}, {7, 1, BR, 70}, {10, 2, BF, 84}, {12, 1, BO, 72}, {14, 2, BA, 80}},
    {{0, 4, BR, 92}, {6, 2, BF, 78}, {8, 4, BR, 88}, {13, 1, BO, 70}, {14, 2, BF, 76}},
};
/* drums L2: per style up to 2 bars, a row per drum in GROOVE_ORDER (KICK SNARE CHH OHH CLAP TOMLO TOMHI RIM): 16 steps
 * of '.', 'x' (92), 'o' (70), 'g' (34) */
static const uint8_t GROOVE_NOTE[8] = {36, 38, 42, 46, 39, 45, 50, 37};
static const char *const GROOVE[5][2][8] = {
    {{"x.......x.o.....", "....x.......x...", "x.o.x.o.x.o.x.o.", 0, 0, 0, 0, 0}},                       /* ROCK1 */
    {{"x.....o...o.....", "....x.......x...", "o.o.o.o.o.o.o.o.", 0, "....o.......o...", 0, 0, 0},
     {"x.....o...o..o..", "....x.......x...", "o.o.o.o.o.o.o.o.", "..............o.", "....o.......o...", 0, 0,
      0}},                                                                                                /* POP3 */
    {{"x..ox..ox..ox..o", 0, "o.o.o.o.o.o.o.o.", 0, 0, 0, 0, "x..x..x...x..x.."},
     {"x..ox..ox..ox..o", 0, "o.o.o.o.o.o.o.o.", 0, 0, 0, 0, "..x..x..x..x...."}},                        /* BOSSA */
    {{"x.x.......x..o..", "....x..g.g..x..g", "xoxoxoxoxoxoxoxo", 0, 0, 0, 0, 0}},                       /* FUNK1 */
    {{"........x.......", "........x.......", "..o...o...o...o.", 0, 0, 0, 0, "........x......."}},       /* REGGAE */
};
static const uint8_t GROOVE_BARS[5] = {1, 2, 2, 1, 1};

static uint32_t u16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static void w32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static uint32_t r32(const uint8_t *p) { return u16(p) | u16(p + 2) << 16; }

/* ------------------------------------------------------------------- the notes out --- */
struct out {
    uint8_t *buf;                         /* 0: count only */
    uint32_t room, n;                     /* notes */
    uint32_t level;                       /* the level wanted (the generators make every level and keep this one) */
    uint32_t swing;
    const struct song *s;                 /* M8 (2.2): energy on: a bar's section's energy adds to the level (capped at
                                           * top), the bar a note starts in; 0: off (a stored level, or none) */
    uint32_t top;
};

/* the level a note starting at tick start is kept at */
static uint32_t level_at(const struct out *o, uint32_t start)
{
    uint32_t bar, lv;
    if (!o->s)
        return o->level;
    bar = start / o->s->bar_ticks;
    if (bar >= o->s->bar_count)
        bar = o->s->bar_count - 1u;
    lv = o->level + o->s->energy[song_bar_section(o->s, bar)];
    return lv < o->top ? lv : o->top;
}

/* arrange.swing_tick: straight eighths -> shuffle eighths (the off-beat from 1/2 to 2/3 of the beat) */
static uint32_t swing_tick(uint32_t t)
{
    uint32_t beat = t / PPQ, frac = t % PPQ;
    if (frac <= PPQ / 2u)
        frac = frac * (2u * PPQ / 3u) / (PPQ / 2u);
    else
        frac = 2u * PPQ / 3u + (frac - PPQ / 2u) * (PPQ / 3u) / (PPQ / 2u);
    return beat * PPQ + frac;
}

static void raw(struct out *o, uint32_t lv, uint32_t start, uint32_t end, uint32_t pitch, uint32_t vel)
{
    uint8_t *r;
    if (lv != level_at(o, start))
        return;
    if (o->buf && (o->n + 1u) * NOTE_REC <= o->room) {
        r = o->buf + o->n * NOTE_REC;
        w32(r, start);
        w32(r + 4, end);
        r[8] = (uint8_t)pitch;
        r[9] = (uint8_t)vel;
    }
    o->n++;
}

/* arrange._Out.note: swung, and ended 15 ticks early (at least 10 ticks long) */
static void note(struct out *o, uint32_t lv, uint32_t start, uint32_t dur, uint32_t pitch, uint32_t vel)
{
    uint32_t a = start, b = start + dur;
    if (o->swing) {
        a = swing_tick(a);
        b = swing_tick(b);
    }
    raw(o, lv, a, b >= a + 25u ? b - 15u : a + 10u, pitch, vel);
}

/* ------------------------------------------------------- the chart, cell by cell --- */
/* binformat.gen_cells: per bar the chords sounding in it, in order */
struct cell {
    uint32_t bar, beat, len;              /* len in beats */
    uint32_t ev;                          /* the chord event */
    int has;                              /* a chord (not N.C.) */
};

struct cells {
    const struct song *s;
    uint32_t bar, k;                      /* the next cell: bar, chord event */
};

static void cells_start(struct cells *it, const struct song *s)
{
    it->s = s;
    it->bar = 0;
    it->k = 0;
}

static int cells_next(struct cells *it, struct cell *c)
{
    const struct song *s = it->s;
    uint32_t t0, t1, a, z;
    if (it->bar >= s->bar_count)
        return 0;
    t0 = it->bar * s->bar_ticks;
    t1 = t0 + s->bar_ticks;
    a = song_cevt_tick(s, it->k);
    z = it->k + 1u < s->ncevt ? song_cevt_tick(s, it->k + 1u) : t1;
    a = a > t0 ? a : t0;
    z = z < t1 ? z : t1;
    c->bar = it->bar;
    c->beat = (a - t0) / s->ppq;
    c->len = (z - a) / s->ppq;
    c->ev = it->k;
    c->has = song_cevt_root(s, it->k) != SONG_NO_CHORD;
    if (it->k + 1u < s->ncevt && song_cevt_tick(s, it->k + 1u) < t1) {
        it->k++;                          /* the next chord starts in this bar */
    } else {
        it->bar++;                        /* the next bar starts with the chord sounding at its beat 1 */
        if (it->bar < s->bar_count)
            it->k = song_bar_chord(s, it->bar);
    }
    return 1;
}

/* a voicing's notes (the zeros left out), in order */
static uint32_t voicing(const struct song *s, uint32_t ev, uint32_t level, uint8_t *v)
{
    const uint8_t *p = song_cevt_voicing(s, ev, level);
    uint32_t i, n = 0;
    for (i = 0; i < SONG_VOICING_NOTES; i++)
        if (p[i])
            v[n++] = p[i];
    return n;
}

static void sort8(uint8_t *v, uint32_t n)
{
    uint32_t i, k;
    for (i = 1; i < n; i++)
        for (k = i; k > 0 && v[k - 1u] > v[k]; k--) {
            uint8_t t = v[k];
            v[k] = v[k - 1u];
            v[k - 1u] = t;
        }
}

/* ------------------------------------------------------------------------ harmony --- */
static void harmony(const struct song *s, struct out *o)
{
    struct cells it;
    struct cell c;
    uint32_t bpb = s->beats_per_bar, final = s->bar_count - 1u, hs = H_OF[s->style], lv, k, h, i;
    cells_start(&it, s);
    while (cells_next(&it, &c)) {
        uint8_t v1[SONG_VOICING_NOTES], v2[SONG_VOICING_NOTES], pool[2 * SONG_VOICING_NOTES];
        uint8_t cyc[4 * SONG_VOICING_NOTES], seq[6];   /* (pool: 2 x 5 notes; cyc: up and down, 18) */
        uint32_t n1, n2, np, nc, nseq, start, dur;
        if (!c.has)
            continue;
        start = c.bar * bpb * PPQ + c.beat * PPQ;
        dur = c.len * PPQ;
        n1 = voicing(s, c.ev, 1, v1);
        n2 = voicing(s, c.ev, 2, v2);
        if (c.bar == final) {             /* the last bar: held, at every level */
            for (lv = 1; lv <= 5u; lv++)
                for (i = 0; i < (lv <= 2u ? n1 : n2); i++)
                    note(o, lv, start, dur, lv <= 2u ? v1[i] : v2[i], 64);
            continue;
        }
        for (i = 0; i < n1; i++)          /* L1: held */
            note(o, 1, start, dur, v1[i], 64);
        for (lv = 2; lv <= 3u; lv++) {    /* L2, L3: the style's rhythm */
            const uint8_t *v = lv == 2u ? v1 : v2;
            uint32_t nv = lv == 2u ? n1 : n2;
            if (hs == H_TABLE) {          /* M8: a table style, on its 16th grid (arrange._steps) */
                uint32_t ts = s->style - FIRST_TABLE, rep, j, t, cend = start + dur, bar0 = c.bar * bpb * PPQ;
                const struct rstep *tab = RHYTHM[ts][c.bar % RHYTHM_BARS[ts]];
                uint8_t vv[SONG_VOICING_NOTES];
                uint32_t nvv = 0;
                for (i = 0; i < nv; i++)
                    vv[nvv++] = v[i];
                if (UPPER[ts] && nvv >= 4u) {   /* the lowest note left out */
                    sort8(vv, nvv);
                    for (i = 1; i < nvv; i++)
                        vv[i - 1u] = vv[i];
                    nvv--;
                }
                for (rep = 0; rep * 16u < bpb * 4u; rep++)
                    for (j = 0; j < 9u && tab[j].len; j++) {
                        uint32_t st = rep * 16u + tab[j].st, d;
                        if (st >= bpb * 4u)
                            continue;
                        t = bar0 + st * STEP;
                        if (t < start || t >= cend)
                            continue;
                        d = tab[j].len * STEP < cend - t ? tab[j].len * STEP : cend - t;
                        for (i = 0; i < nvv; i++)
                            note(o, lv, t, d, vv[i], tab[j].vel);
                    }
                continue;
            }
            for (k = 0; k < c.len; k++) {
                uint32_t bib = c.beat + k, t = start + k * PPQ;
                for (i = 0; i < nv; i++)
                    switch (hs) {
                    case H_WALTZ:
                        if (bib % 3u == 0)
                            note(o, lv, t, PPQ - 40u, v[i], 64);
                        else
                            note(o, lv, t, PPQ / 2u, v[i], 50);
                        break;
                    case H_SHUFFLE:
                        note(o, lv, t, PPQ / 3u, v[i], bib % 2u ? 58u : 50u);
                        break;
                    case H_STRIDE:
                        if (bib % 2u)
                            note(o, lv, t, PPQ / 2u, v[i], 60);
                        break;
                    case H_BALLAD:
                        note(o, lv, t, PPQ - 40u, v[i], bib % 2u == 0 ? 60u : 50u);
                        break;
                    default:              /* eighths */
                        break;
                    }
                if (hs == H_EIGHTHS)
                    for (h = 0; h < 2u; h++)
                        for (i = 0; i < nv; i++)
                            note(o, lv, t + h * (PPQ / 2u), PPQ / 3u, v[i], h == 0 ? 56u : 44u);
            }
        }
        /* (rooted chords have notes in both voicings: song_open's rule for 2.0) */
        sort8(v2, n2);                    /* L4: broken chords, in eighths: low, mid, high */
        if (bpb % 3u == 0) {
            uint8_t q[6] = {v2[0], v2[n2 / 2u], v2[n2 - 1u], v2[n2 / 2u], v2[n2 - 1u], v2[n2 / 2u]};
            for (i = 0; i < 6u; i++)
                seq[i] = q[i];
            nseq = 6;
        } else {
            seq[0] = v2[0];
            seq[1] = v2[n2 - 1u];
            seq[2] = v2[n2 / 2u];
            seq[3] = v2[n2 - 1u];
            nseq = 4;
        }
        for (k = 0; k < c.len * 2u; k++)
            note(o, 4, start + k * (PPQ / 2u), PPQ / 2u + 40u, seq[(c.beat * 2u + k) % nseq], k % 2u == 0 ? 70u : 58u);
        np = 0;                           /* L5: the voicing and an octave up, within 19 semitones; up and down */
        for (i = 0; i < n2; i++) {
            pool[np++] = v2[i];
            pool[np++] = (uint8_t)(v2[i] + 12u);
        }
        sort8(pool, np);
        for (i = 0, k = 0; i < np; i++)   /* each note once, up to the lowest + 19 */
            if ((!k || pool[i] != pool[k - 1u]) && pool[i] <= (uint32_t)v2[0] + 19u)
                pool[k++] = pool[i];
        np = k;
        nc = 0;
        for (i = 0; i < np; i++)
            cyc[nc++] = pool[i];
        for (i = np >= 2u ? np - 2u : 0; i >= 1u && np >= 2u; i--)
            cyc[nc++] = pool[i];
        for (k = 0; k < c.len * 2u; k++)
            note(o, 5, start + k * (PPQ / 2u), PPQ / 2u + 60u, cyc[k % nc], k % 2u == 0 ? 70u : 56u);
    }
}

/* ---------------------------------------------------------------------------- bass --- */
static uint32_t bass_root(const struct song *s, uint32_t ev)
{
    const uint8_t *r = song_cevt_chord(s, ev);
    uint32_t pc = r[9] != SONG_NO_CHORD ? r[9] : r[8];
    return pc >= 4u ? 24u + pc : 36u + pc;    /* E1..D#2: the bass's real octave */
}

static void bass(const struct song *s, struct out *o)
{
    static const uint8_t LINE[8] = {0, 4, 7, 9, 10, 9, 7, 4};
    struct cells it, ahead;
    struct cell c, nc;
    uint32_t bpb = s->beats_per_bar, final = s->bar_count - 1u, lv, k;
    cells_start(&it, s);
    while (cells_next(&it, &c)) {
        uint32_t start, dur, root, fifth, third, nxt, mask, rootpc;
        if (!c.has)
            continue;
        start = c.bar * bpb * PPQ + c.beat * PPQ;
        dur = c.len * PPQ;
        root = bass_root(s, c.ev);
        fifth = root + 7u <= 40u ? root + 7u : root - 5u;
        rootpc = song_cevt_root(s, c.ev);
        mask = song_cevt_mask(s, c.ev);
        third = root + ((mask >> ((rootpc + 3u) % 12u) & 1u) ? 3u : 4u);
        nxt = root;                       /* the next chord's (N.C. skipped) */
        ahead = it;
        while (cells_next(&ahead, &nc))
            if (nc.has) {
                nxt = bass_root(s, nc.ev);
                break;
            }
        if (c.bar == final) {
            for (lv = 1; lv <= 4u; lv++)
                note(o, lv, start, dur, root, 84);
            continue;
        }
        note(o, 1, start, dur, root, 97);                                    /* L1: held */
        for (k = 0; k < c.len; k++) {                                        /* L2: the root in rhythm */
            uint32_t bib = c.beat + k, d = PPQ;
            if (bpb % 3u == 0) {                                             /* (3/4: on 1, held 2 beats, and on 3) */
                if (bib % 3u == 1u && k > 0)
                    continue;
                d = bib % 3u == 0 && k + 1u < c.len ? 2u * PPQ : PPQ;
            }
            note(o, 2, start + k * PPQ, d - 30u, root, bib % bpb == 0 ? 86u : 74u);
        }
        for (k = 0; k < c.len; k += bpb % 2u == 0 ? 2u : 1u) {             /* L3: root and fifth */
            uint32_t bib = c.beat + k, p, d;
            if (bpb % 3u == 0) {
                if (bib % 3u == 1u)
                    continue;
                p = bib % 3u == 0 ? root : fifth;
                d = (bib % 3u == 0 ? 2u : 1u) * PPQ - 30u;
            } else {
                p = k == 0 || bib % bpb == 0 ? root : fifth;
                d = (c.len - k < 2u ? c.len - k : 2u) * PPQ - 30u;
            }
            note(o, 3, start + k * PPQ, d, p, p == root ? 88u : 76u);
        }
        if (s->style >= FIRST_TABLE) {                                       /* L4: the style's line (M8) */
            const struct bstep *bl = BASS_LINE[s->style - FIRST_TABLE];
            uint32_t rep, j, t, cend = start + dur, bar0 = c.bar * bpb * PPQ;
            for (rep = 0; rep * 16u < bpb * 4u; rep++)
                for (j = 0; j < 9u && bl[j].len; j++) {
                    uint32_t st = rep * 16u + bl[j].st, d, p;
                    if (st >= bpb * 4u)
                        continue;
                    t = bar0 + st * STEP;
                    if (t < start || t >= cend)
                        continue;
                    d = bl[j].len * STEP < cend - t ? bl[j].len * STEP : cend - t;
                    p = bl[j].what == BR ? root : bl[j].what == BF ? fifth : bl[j].what == BO ? root + 12u :
                        bl[j].what == BN ? nxt : nxt == root ? fifth : nxt < root ? nxt + 1u : nxt - 1u;
                    note(o, 4, t, d, p, bl[j].vel);
                }
        } else if (BOOGIE[s->style]) {                                       /* L4: more motion */
            for (k = 0; k < c.len * 2u; k++)
                note(o, 4, start + k * (PPQ / 2u), PPQ / 2u, root + LINE[(c.beat * 2u + k) % 8u], k % 2u == 0 ? 84u : 70u);
        } else if (bpb % 3u == 0) {                                          /* 3/4: root, fifth, octave */
            for (k = 0; k < c.len; k++) {
                uint32_t w = (c.beat + k) % 3u, p = w == 0 ? root : w == 1u ? fifth : root + 12u;
                note(o, 4, start + k * PPQ, PPQ - 30u, p <= 43u ? p : p - 12u, w == 0 ? 84u : 72u);
            }
        } else {                                                             /* walking quarters into the next root */
            uint32_t walk[4] = {root, third, fifth, nxt < root ? nxt + 1u : nxt - 1u};
            for (k = 0; k < c.len; k++) {
                uint32_t p = k + 1u < c.len || c.len < 2u ? walk[k % 4u] : walk[3];
                if (k + 1u == c.len && c.len >= 2u && nxt == root)
                    p = root + 9u <= 43u ? root + 9u : fifth;                /* the same chord next: the sixth */
                note(o, 4, start + k * PPQ, PPQ - 30u, p, k == 0 ? 84u : 72u);
            }
        }
    }
}

/* --------------------------------------------------------------------------- drums --- */
struct hit {
    uint32_t t;
    uint8_t p, v;
};
#define MAX_HITS 96u                     /* (M8: a table groove: 28 steps of at most 3 hits; M5..M7: 7 x 6 + 1) */

static void drums(const struct song *s, struct out *o)
{
    uint32_t bpb = s->beats_per_bar, final = s->bar_count - 1u, ds = D_OF[s->style], b, k, i, lv;
    for (b = 0; b < s->bar_count; b++) {
        struct hit h1[MAX_HITS], h2[MAX_HITS], h3[MAX_HITS + 5u];
        uint32_t n1 = 0, n2 = 0, n3 = 0, t0 = b * bpb * PPQ, cym = ds == D_SHUFFLE ? RIDE : HAT, bar_no, sec = 0;
#define HIT(arr, n, tt, pp, vv) do { arr[n].t = (tt); arr[n].p = (uint8_t)(pp); arr[n].v = (uint8_t)(vv); n++; } while (0)
#define SW(t) (s->swing ? swing_tick(t) : (t))
        if (b == 0 && s->pickup_beats)
            continue;                     /* the pickup bar is silent */
        if (b == final) {                 /* the last bar: a kick and a crash */
            for (lv = 1; lv <= 3u; lv++) {
                raw(o, lv, t0, t0 + 60u, KICK, 96);
                raw(o, lv, t0, t0 + 60u, CRASH, 90);
            }
            continue;
        }
        for (k = 0; k < bpb; k++) {                                          /* L1: the pulse */
            uint32_t tk = t0 + k * PPQ;
            if (k == 0 || (bpb == 4u && k == 2u))
                HIT(h1, n1, tk, KICK, k == 0 ? 88 : 80);
            else
                HIT(h1, n1, tk, SNARE, bpb == 3u ? 62 : 80);
            HIT(h1, n1, tk, cym, k == 0 ? 64 : 56);
        }
        if (ds == D_TABLE) {                                                 /* L2: a table groove (M8) */
            uint32_t ts = s->style - FIRST_TABLE, st, r;
            const char *const *g = GROOVE[ts][b % GROOVE_BARS[ts]];
            for (st = 0; st < bpb * 4u; st++)
                for (r = 0; r < 8u; r++) {
                    char ch = g[r] ? g[r][st % 16u] : '.';
                    if (ch != '.' && n2 < MAX_HITS)
                        HIT(h2, n2, SW(t0 + st * STEP), GROOVE_NOTE[r], ch == 'x' ? 92 : ch == 'o' ? 70 : 34);
                }
        }
        for (k = 0; k < (ds == D_TABLE ? 0 : bpb); k++) {                    /* L2: the style's groove */
            uint32_t tk = t0 + k * PPQ;
            if (ds == D_WALTZ) {
                if (k == 0)
                    HIT(h2, n2, tk, KICK, 84);
                else
                    HIT(h2, n2, tk, STICK, 58);
                HIT(h2, n2, tk, HAT, k == 0 ? 62 : 50);
                HIT(h2, n2, SW(tk + PPQ / 2u), HAT, 36);
                continue;
            }
            if (k == 0 || k == 2u)
                HIT(h2, n2, tk, KICK, k == 0 ? 92 : 80);
            else
                HIT(h2, n2, tk, ds == D_BALLAD ? STICK : SNARE, ds == D_BALLAD ? 70 : 88);
            HIT(h2, n2, tk, cym, k % 2u == 0 ? 66 : 58);
            HIT(h2, n2, SW(tk + PPQ / 2u), cym, 40);
            if (ds == D_SHUFFLE && k % 2u == 0)
                HIT(h2, n2, SW(tk + PPQ / 2u), SNARE, 26);
            if (ds == D_POP && k == 2u)
                HIT(h2, n2, tk + PPQ / 2u, KICK, 62);
            if (ds == D_TRAIN) {
                HIT(h2, n2, tk + PPQ / 4u, SNARE, 24);
                HIT(h2, n2, tk + 3u * PPQ / 4u, SNARE, 30);
            }
            if (ds == D_MARCH && k == 3u && b % 2u)
                HIT(h2, n2, SW(tk + PPQ / 2u), SNARE, 50);
        }
        bar_no = b + 1u - (s->pickup_beats ? 1u : 0u);
        for (i = 0; i < n2; i++)                                             /* L3: + crashes and fills */
            h3[n3++] = h2[i];
        for (i = 1; i < s->nsect; i++)
            sec |= song_section_first(s, i) == b;
        if (sec)
            HIT(h3, n3, t0, CRASH, 84);
        if (bar_no % 4u == 0) {
            static const uint8_t FILL[4] = {SNARE, SNARE, TOM_HI, TOM_LO};
            uint32_t base = t0 + (bpb - 1u) * PPQ, m = 0;
            for (i = 0; i < n3; i++)
                if (h3[i].t < base)
                    h3[m++] = h3[i];
            n3 = m;
            for (i = 0; i < 4u; i++)
                HIT(h3, n3, base + i * (PPQ / 4u), FILL[i], 64u + 6u * i);
        }
        for (i = 0; i < n1; i++)
            raw(o, 1, h1[i].t, h1[i].t + 50u, h1[i].p, h1[i].v);
        for (i = 0; i < n2; i++)
            raw(o, 2, h2[i].t, h2[i].t + 50u, h2[i].p, h2[i].v);
        for (i = 0; i < n3; i++)
            raw(o, 3, h3[i].t, h3[i].t + 50u, h3[i].p, h3[i].v);
#undef HIT
#undef SW
    }
}

/* ------------------------------------------------------------------ stored levels --- */
static uint32_t rd_varint(const uint8_t *c, uint32_t len, uint32_t *p, int *bad)
{
    uint32_t n = 0, k;
    for (k = 0; k < 3u; k++) {
        uint32_t b;
        if (*p >= len) {
            *bad = 1;
            return 0;
        }
        b = c[(*p)++];
        n |= (b & 0x7Fu) << (7u * k);
        if (!(b & 0x80u))
            return n;
    }
    *bad = 1;                             /* longer than 3 bytes */
    return 0;
}

/* an NT chunk (binformat.nt_encode): u16 patterns, u16 bars, u16 per bar (its pattern, 0xFFFF none), then each
 * pattern: u16 notes, per note varint offset, varint length, pitch, velocity. With o: its notes into o (in pattern
 * order: canonical sorts them); without: checked, as binformat.nt_decode -> the note count, or -1 */
int32_t song_nt_notes(const struct song *s, const uint8_t *c, uint32_t len, void *out_)
{
    struct out *o = out_;
    uint32_t npat, nb, p, k, j, b, total = 0;
    int bad = 0;
    if (!c || len < 4u)
        return -1;
    npat = u16(c);
    nb = u16(c + 2);
    if (nb != s->bar_count || npat > nb || len < 4u + 2u * nb)
        return -1;
    for (b = 0; b < nb; b++) {
        uint32_t x = u16(c + 4 + 2 * b);
        if (x != 0xFFFFu && x >= npat)
            return -1;
    }
    p = 4u + 2u * nb;
    for (k = 0; k < npat; k++) {
        uint32_t n, q, ext = 0, uses = 0;
        if (p + 2u > len)
            return -1;
        n = u16(c + p);
        p += 2u;
        if (n < 1u || n > SONG_NT_PATTERN)
            return -1;
        q = p;
        for (j = 0; j < n; j++) {
            uint32_t off = rd_varint(c, len, &p, &bad), d = rd_varint(c, len, &p, &bad), pi, v;
            if (bad || p + 2u > len)
                return -1;
            pi = c[p];
            v = c[p + 1u];
            p += 2u;
            if (off >= s->bar_ticks || d < 1u || pi > 127u || v < 1u || v > 127u)
                return -1;
            if (off + d > ext)
                ext = off + d;
        }
        for (b = 0; b < nb; b++) {
            uint32_t t0;
            if (u16(c + 4 + 2 * b) != k)
                continue;
            t0 = b * s->bar_ticks;
            if (ext > s->end_tick - t0)   /* a note runs past the end of the song */
                return -1;
            uses++;
            if (o) {                      /* (checked above: no failure past here) */
                uint32_t r = q;
                for (j = 0; j < n; j++) {
                    uint32_t off = rd_varint(c, len, &r, &bad), d = rd_varint(c, len, &r, &bad);
                    raw(o, o->level, t0 + off, t0 + off + d, c[r], c[r + 1u]);
                    r += 2u;
                }
            }
        }
        total += uses * n;                /* <= 999 x 4096 */
    }
    if (p != len || total > SONG_NT_NOTES)
        return -1;
    return (int32_t)total;
}

/* ------------------------------------------------------------------- the canonical --- */
/* arrange.canonical: notes sorted by (start, pitch, louder first, end); one note per pitch and start; a note ends
 * where the next of its pitch starts; events sorted by (tick, note-offs first, pitch). Heapsort: no extra memory */
static int note_before(const uint8_t *a, const uint8_t *b)
{
    uint32_t x = r32(a), y = r32(b);
    if (x != y)
        return x < y;
    if (a[8] != b[8])
        return a[8] < b[8];
    if (a[9] != b[9])
        return a[9] > b[9];
    return r32(a + 4) < r32(b + 4);
}

static int event_before(const uint8_t *a, const uint8_t *b)
{
    uint32_t x = r32(a), y = r32(b);
    if (x != y)
        return x < y;
    if (a[4] != b[4])
        return a[4] < b[4];               /* 0x80 before 0x90 */
    return a[5] < b[5];
}

static void swap(uint8_t *a, uint8_t *b, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; i++) {
        uint8_t t = a[i];
        a[i] = b[i];
        b[i] = t;
    }
}

static void heapsort(uint8_t *base, uint32_t n, uint32_t size, int (*before)(const uint8_t *, const uint8_t *))
{
    uint32_t start = n / 2u, end = n, root, child;
    if (n < 2u)
        return;
    for (;;) {
        if (start > 0) {
            start--;
        } else {
            end--;
            if (end == 0)
                return;
            swap(base, base + end * size, size);
        }
        root = start;
        while ((child = 2u * root + 1u) < end) {
            if (child + 1u < end && before(base + child * size, base + (child + 1u) * size))
                child++;
            if (!before(base + root * size, base + child * size))
                break;
            swap(base + root * size, base + child * size, size);
            root = child;
        }
    }
}

static uint32_t canonical(uint8_t *buf, uint32_t n)
{
    static uint32_t nxt[256];             /* per pitch: the start of its next note (pitches up to 127 + 12) */
    uint32_t i, m = 0;
    heapsort(buf, n, NOTE_REC, note_before);
    for (i = 0; i < n; i++) {             /* one note per (start, pitch): the first, the loudest */
        const uint8_t *r = buf + i * NOTE_REC;
        if (m && r32(r) == r32(buf + (m - 1u) * NOTE_REC) && r[8] == buf[(m - 1u) * NOTE_REC + 8u])
            continue;
        if (m != i) {
            uint32_t k;
            for (k = 0; k < NOTE_REC; k++)
                buf[m * NOTE_REC + k] = r[k];
        }
        m++;
    }
    for (i = 0; i < 256u; i++)
        nxt[i] = 0xFFFFFFFFu;
    for (i = m; i-- > 0;) {               /* a note ends where the next of its pitch starts */
        uint8_t *r = buf + i * NOTE_REC;
        if (nxt[r[8]] < r32(r + 4))
            w32(r + 4, nxt[r[8]]);
        nxt[r[8]] = r32(r);
    }
    for (i = 0; i < m; i++) {             /* a note's 16 bytes -> its note-on and note-off */
        uint8_t *r = buf + i * NOTE_REC;
        uint32_t end = r32(r + 4), p = r[8], v = r[9];
        r[4] = 0x90u;
        r[5] = (uint8_t)p;
        r[6] = (uint8_t)v;
        r[7] = 0;
        w32(r + 8, end);
        r[12] = 0x80u;
        r[13] = (uint8_t)p;
        r[14] = 0;
        r[15] = 0;
    }
    heapsort(buf, 2u * m, EV_REC, event_before);
    return 2u * m;
}

/* ---------------------------------------------------------------------- song.h API --- */
static void make(const struct song *s, uint32_t l, struct out *o)
{
    uint32_t len;
    o->s = 0;
    if (s->layer[l].stored >> (o->level - 1u) & 1u) {
        const uint8_t *c = song_nt_chunk(s, l, o->level, &len);
        song_nt_notes(s, c, len, o);
        return;
    }
    if (s->any_energy && !s->layer[l].stored) {   /* M8: energy, for a layer with no stored level */
        o->s = s;
        o->top = s->layer[l].variants;
    }
    if (l == 0)
        drums(s, o);
    else if (l == 1u)
        bass(s, o);
    else if (l == 2u)
        harmony(s, o);
}

uint32_t song_gen_notes(const struct song *s, uint32_t layer, uint32_t level)
{
    struct out o = {0, 0, 0, level, s->swing, 0, 0};
    make(s, layer, &o);
    return o.n;
}

int song_build(struct song *s, uint32_t layer, uint32_t level, uint8_t *buf, uint32_t room)
{
    struct song_layer *ly = &s->layer[layer];
    struct out o = {buf, room, 0, level, s->swing, 0, 0};
    uint32_t n, v;
    if (!s->opened || !s->fmt2 || layer >= SONG_LAYERS || level < 1u || level > ly->variants)
        return SONG_E_LAYR;
    if (!buf || room < ly->need * EV_REC)
        return SONG_E_ROOM;
    make(s, layer, &o);
    n = canonical(buf, o.n < ly->need / 2u ? o.n : ly->need / 2u);
    for (v = 0; v < ly->variants; v++) {  /* every level reads the one built (the player plays the chosen one) */
        ly->var[v].ev = buf;
        ly->var[v].count = n;
    }
    ly->built = (uint8_t)level;
    return SONG_OK;
}
