/* Licence: TBD (see D017) */
/* The song player (player.h). Written for this project (D017): nothing here comes from another sequencer.
 *
 * Each layer plays one variant's stream; idx[l] is the next event to send. A block sends every event whose tick is
 * before the block's end, at the block's start: notes start up to one block (5.8 ms) early, the same as EAS's own
 * file player. Notes are tracked per layer (a 128-bit set), so a mute, a variant change, a loop jump or a stop can
 * release exactly the notes that sound, and a muted layer never leaves one hanging. */
#include "player.h"
#include "synth.h"
#include "plat.h"

#define BIAS_BARS 2                       /* the count-in runs below tick 0: positions are kept this far above */
#define PL_EVENTS_PER_BLOCK 128u          /* a block sends at most this many; the rest go in the next */
#define CLICK_CH 9u                       /* GM percussion */
#define CLICK_BAR 76u                     /* hi wood block on a bar's first beat, low wood block on the others */
#define CLICK_BEAT 77u
#define FRAME_RATE 44100u

volatile struct player_stats player_stats = {.magic = PL_STATS_MAGIC, .version = 1};

/* ------------------------------------------------- requests (main loop -> interrupt) --- */
static const struct song *volatile want_song;
static volatile uint32_t song_req, song_done, play_req, play_done, loop_req, loop_done;
static volatile uint8_t play_on, countin_want = 1;
static volatile uint32_t mute_want, tempo_want = 100;
static volatile uint8_t var_want[SONG_LAYERS] = {1, 1, 1, 1};
static volatile int loop_mode_want;
static volatile uint32_t loop_from_want, loop_to_want;

/* ------------------------------------------------------- the interrupt's own state --- */
static const struct song *song;
static int state = PL_STOPPED;
static uint64_t q;                        /* (tick + bias) * den */
static uint32_t den, bias, inc;           /* den = tempo_us * 441; inc = ppq * tempo% * 100 per frame */
static uint32_t pct = 100;
static uint32_t idx[SONG_LAYERS];
static uint8_t var_cur[SONG_LAYERS] = {1, 1, 1, 1};
static uint32_t mute_cur;
static uint32_t active[SONG_LAYERS][4];   /* notes sounding, per layer */
static int loop_mode;
static int32_t loop_from, loop_to;
static uint8_t click_on;                  /* a click note to end at the next block (0: none) */
static uint32_t budget_left;
static uint32_t stop_frames;

/* -------------------------------------------------------- published (interrupt -> main loop) --- */
static volatile uint32_t frames;
static volatile int32_t pub_tick;
static volatile uint32_t pub_tpms_q16;    /* ticks per ms, Q16, at the tempo now */
static volatile int pub_state = PL_STOPPED;
static volatile int pub_loop;
static volatile int32_t pub_loop_from, pub_loop_to;
static volatile uint32_t voices_now, voices_max;

/* ------------------------------------------------------------------------ main loop --- */
void player_load(const struct song *s)
{
    want_song = s;
    __asm__ volatile("" ::: "memory");
    song_req++;
}
const struct song *player_song(void) { return want_song; }

void player_play(int on)
{
    play_on = (uint8_t)(on != 0);
    __asm__ volatile("" ::: "memory");
    play_req++;
}

void player_mute(uint32_t layers) { mute_want = layers & ((1u << SONG_LAYERS) - 1u); }

void player_variant(uint32_t layer, uint32_t v)
{
    if (layer < SONG_LAYERS && v >= 1u && v <= SONG_MAX_VARIANTS)
        var_want[layer] = (uint8_t)v;
}

void player_tempo(uint32_t p)
{
    tempo_want = p < PL_TEMPO_MIN ? PL_TEMPO_MIN : p > PL_TEMPO_MAX ? PL_TEMPO_MAX : p;
}

void player_count_in(int on) { countin_want = (uint8_t)(on != 0); }

void player_loop(int mode, uint32_t from, uint32_t to)
{
    loop_mode_want = mode;
    loop_from_want = from;
    loop_to_want = to;
    __asm__ volatile("" ::: "memory");
    loop_req++;
}

int player_state(void) { return pub_state; }
int player_idle(void) { return song_done == song_req && play_done == play_req; }
uint32_t player_frames(void) { return frames; }
uint32_t player_voices(void) { return voices_now; }
uint32_t player_voices_max(void) { return voices_max; }
uint32_t player_variant_of(uint32_t layer) { return layer < SONG_LAYERS ? var_want[layer] : 0; }
uint32_t player_tempo_of(void) { return tempo_want; }
int player_count_in_of(void) { return countin_want; }

int player_loop_of(uint32_t *from, uint32_t *to)
{
    if (from)
        *from = (uint32_t)pub_loop_from;
    if (to)
        *to = (uint32_t)pub_loop_to;
    return pub_loop;
}

int32_t player_tick(uint32_t lead_ms)
{
    const struct song *s = want_song;
    int32_t t = pub_tick + (int32_t)(lead_ms * pub_tpms_q16 >> 16);
    if (pub_state == PL_STOPPED || !s)
        return pub_tick;
    if (pub_loop && pub_loop_to > pub_loop_from && t >= pub_loop_to)
        t = pub_loop_from + (t - pub_loop_to) % (pub_loop_to - pub_loop_from);
    if (t >= (int32_t)s->end_tick)
        t = (int32_t)s->end_tick - 1;
    return t;
}

/* -------------------------------------------------------------------- the interrupt --- */
/* every 32x32->64-bit multiply goes through here, out of line: inlined beside loads they can corrupt a register on
 * this CPU (felucca-bsp docs/CPU.md; the build's check lists functions that inline one) */
__attribute__((noinline)) static uint64_t mul64(uint32_t a, uint32_t b) { return (uint64_t)a * b; }

static int32_t tick_of(uint64_t v) { return (int32_t)(v / den) - (int32_t)bias; }
static uint64_t q_of(int32_t tick) { return mul64((uint32_t)(tick + (int32_t)bias), den); }

static void send(uint32_t l, uint32_t status, uint32_t d1, uint32_t d2)
{
    synth_send_now((uint8_t)(status | song->layer[l].channel), (uint8_t)d1, (uint8_t)d2);
}

static void release(uint32_t l)
{
    uint32_t w, b;
    for (w = 0; w < 4u; w++)
        while (active[l][w]) {
            b = (uint32_t)__builtin_ctz(active[l][w]);
            active[l][w] &= active[l][w] - 1u;
            send(l, 0x80u, w * 32u + b, 0);
        }
    send(l, 0xB0u, 64u, 0);               /* and the sustain pedal up */
}

static void release_all(void)
{
    uint32_t l;
    for (l = 0; l < SONG_LAYERS; l++)
        if (song->layer[l].variants)
            release(l);
    if (click_on) {
        synth_send_now(0x80u | CLICK_CH, click_on, 0);
        click_on = 0;
    }
}

static const struct song_stream *stream(uint32_t l) { return &song->layer[l].var[var_cur[l] - 1u]; }

static void seek_all(int32_t tick)
{
    uint32_t l;
    for (l = 0; l < SONG_LAYERS; l++)
        if (song->layer[l].variants)
            idx[l] = song_seek(stream(l), tick > 0 ? (uint32_t)tick : 0);
}

static void dispatch(uint32_t l, const struct song_event *e)
{
    uint32_t w = e->d1 >> 5, bit = 1u << (e->d1 & 31u);
    switch (e->status) {
    case 0x90u:
        if (mute_cur >> l & 1u) {
            player_stats.notes_skipped[l]++;
            return;
        }
        if (active[l][w] & bit)
            send(l, 0x80u, e->d1, 0);     /* restarted: end the one sounding */
        active[l][w] |= bit;
        send(l, 0x90u, e->d1, e->d2);
        player_stats.notes_on[l]++;
        return;
    case 0x80u:
        if (active[l][w] & bit) {
            active[l][w] &= ~bit;
            send(l, 0x80u, e->d1, 0);
        }
        return;
    default:                              /* controllers and bends, muted or not: the layer's state stays right */
        send(l, e->status, e->d1, e->d2);
    }
}

/* every event with tick < limit, within this block's budget */
static void run_events(int32_t limit)
{
    uint32_t l;
    struct song_event e;
    for (l = 0; l < SONG_LAYERS; l++) {
        const struct song_stream *st;
        if (!song->layer[l].variants)
            continue;
        st = stream(l);
        while (idx[l] < st->count) {
            song_event_get(st, idx[l], &e);
            if ((int32_t)e.tick >= limit)
                break;
            if (!budget_left) {
                player_stats.deferred++;
                return;
            }
            budget_left--;
            dispatch(l, &e);
            idx[l]++;
        }
    }
}

static void log_chords(int32_t from, int32_t to, uint32_t f0)
{
    uint32_t k;
    if (to <= 0 || to <= from)
        return;
    if (from < 0)
        from = 0;
    k = song_chord_at(song, (uint32_t)to - 1u);
    for (;;) {
        uint32_t t = song_cevt_tick(song, k);
        if ((int32_t)t < from)
            break;
        if (k < PL_STATS_CHORDS && !player_stats.chord_due[k])
            player_stats.chord_due[k] = f0 ? f0 : 1u;
        if (!k)
            break;
        k--;
    }
}

static void clicks(int32_t from, int32_t to)
{
    int32_t ppq = (int32_t)song->ppq, start = (int32_t)song->start_tick, t;
    if (click_on) {
        synth_send_now(0x80u | CLICK_CH, click_on, 0);
        click_on = 0;
    }
    if (from >= start)
        return;
    /* the first beat at or after from (from may be negative: round towards +infinity by hand) */
    t = from >= 0 ? (from + ppq - 1) / ppq * ppq : -((-from) / ppq * ppq);
    if (t < to && t < start) {
        int32_t in_bar = (t + (int32_t)song->bar_ticks * BIAS_BARS) % (int32_t)song->bar_ticks;
        click_on = in_bar == 0 ? CLICK_BAR : CLICK_BEAT;
        synth_send_now(0x90u | CLICK_CH, click_on, in_bar == 0 ? 112u : 88u);
    }
}

static void set_tempo(uint32_t p)
{
    pct = p;
    inc = song ? song->ppq * pct * 100u : 0;
    pub_tpms_q16 = song ? (uint32_t)(mul64(song->ppq * pct * 10u, 65536u) / song->tempo_us) : 0;
}

static void channel_setup(void)
{
    uint32_t l;
    for (l = 0; l < SONG_LAYERS; l++) {
        const struct song_layer *ly = &song->layer[l];
        if (!ly->variants)
            continue;
        send(l, 0xB0u, 123u, 0);          /* all notes off, reset controllers */
        send(l, 0xB0u, 121u, 0);
        if (ly->program != 0xFFu)
            send(l, 0xC0u, ly->program, 0);
        send(l, 0xB0u, 7u, ly->volume);
        send(l, 0xB0u, 10u, ly->pan);
        active[l][0] = active[l][1] = active[l][2] = active[l][3] = 0;
    }
}

static void stop(void)
{
    if (song && state != PL_STOPPED) {
        release_all();
        stop_frames = frames ? frames : 1u;
        player_stats.voices_after_stop = 0xFFFFFFFFu;   /* measured in player_after, 1 s on */
    }
    state = PL_STOPPED;
}

static void start(uint32_t f0)
{
    uint32_t l;
    int32_t from;
    stop();
    den = song->tempo_us * 441u;
    bias = song->bar_ticks * BIAS_BARS;
    set_tempo(tempo_want);
    for (l = 0; l < SONG_LAYERS; l++) {
        uint32_t v = var_want[l], n = song->layer[l].variants;
        var_cur[l] = (uint8_t)(v < 1u ? 1u : v > n && n ? n : v);
    }
    mute_cur = mute_want;
    channel_setup();
    from = countin_want ? -(int32_t)song->bar_ticks : (int32_t)song->start_tick;
    q = q_of(from);
    seek_all(from);
    state = from < (int32_t)song->start_tick ? PL_COUNT_IN : PL_PLAYING;
    player_stats.starts++;
    player_stats.start_frames = f0;
    stop_frames = 0;
}

static void requests(uint32_t f0)
{
    uint32_t l, m;
    if (song_req != song_done) {
        song_done = song_req;
        stop();
        song = want_song;
        loop_mode = LOOP_OFF;
        if (song) {
            den = song->tempo_us * 441u;
            bias = song->bar_ticks * BIAS_BARS;
            set_tempo(tempo_want);
            player_stats.song_crc = song->data[12] | (uint32_t)song->data[13] << 8 |
                                    (uint32_t)song->data[14] << 16 | (uint32_t)song->data[15] << 24;
        }
    }
    if (!song)
        return;
    if (play_req != play_done) {
        play_done = play_req;
        if (play_on)
            start(f0);
        else
            stop();
    }
    if (loop_req != loop_done) {
        loop_done = loop_req;
        loop_mode = loop_mode_want;
        loop_from = (int32_t)loop_from_want;
        loop_to = (int32_t)loop_to_want;
        if (loop_to <= loop_from || loop_to > (int32_t)song->end_tick)
            loop_mode = LOOP_OFF;
    }
    if (tempo_want != pct) {
        set_tempo(tempo_want);
        if (state != PL_STOPPED) {
            uint32_t n = player_stats.ntempo;
            if (n < PL_STATS_TEMPOS) {
                player_stats.tempo_frames[n] = f0;
                player_stats.tempo_pct[n] = pct;
                player_stats.tempo_tick[n] = tick_of(q);
            }
            player_stats.ntempo = n + 1u;
        }
    }
    if (state == PL_STOPPED) {
        mute_cur = mute_want;
        for (l = 0; l < SONG_LAYERS; l++)
            var_cur[l] = var_want[l];
        return;
    }
    m = mute_want & ~mute_cur;            /* newly muted: their notes off now */
    for (l = 0; l < SONG_LAYERS; l++)
        if ((m >> l & 1u) && song->layer[l].variants) {
            if (active[l][0] | active[l][1] | active[l][2] | active[l][3])
                player_stats.mutes[l]++;
            release(l);
        }
    mute_cur = mute_want;
    for (l = 0; l < SONG_LAYERS; l++) {
        uint32_t v = var_want[l], n = song->layer[l].variants;
        if (!n)
            continue;
        if (v > n)
            v = n;
        if (v != var_cur[l]) {
            int32_t t = tick_of(q);
            release(l);
            var_cur[l] = (uint8_t)v;
            idx[l] = song_seek(stream(l), t > 0 ? (uint32_t)t : 0);
        }
    }
}

void player_block(uint32_t n)
{
    uint32_t f0 = frames;
    int32_t from, to, end;
    frames = f0 + n;
    requests(f0);
    if (!song || state == PL_STOPPED) {
        pub_state = PL_STOPPED;
        return;
    }
    budget_left = PL_EVENTS_PER_BLOCK;
    from = tick_of(q);
    q += mul64(n, inc);
    to = tick_of(q);
    clicks(from, to);
    end = (int32_t)song->end_tick;
    if (loop_mode != LOOP_OFF && from < loop_to && to >= loop_to) {
        run_events(loop_to);
        log_chords(from, loop_to, f0);
        release_all();
        q -= mul64((uint32_t)(loop_to - loop_from), den);
        from = loop_from;
        to = tick_of(q);
        seek_all(loop_from);
        player_stats.loops++;
        player_stats.loop_from = (uint32_t)loop_from;
        player_stats.loop_to = (uint32_t)loop_to;
    }
    if (to > end) {
        run_events(end + 1);              /* the note-offs at the very end too */
        log_chords(from, end, f0);
        player_stats.ends++;
        player_stats.end_frames = f0;
        player_stats.last_tick = end;
        stop();
        pub_tick = end;
        pub_state = PL_STOPPED;
        return;
    }
    run_events(to);
    log_chords(from, to, f0);
    state = to < (int32_t)song->start_tick ? PL_COUNT_IN : PL_PLAYING;
    player_stats.last_tick = to;
    pub_loop = loop_mode;
    pub_loop_from = loop_from;
    pub_loop_to = loop_to;
    pub_tick = to;
    __asm__ volatile("" ::: "memory");
    pub_state = state;
}

void player_after(uint32_t render_ticks, uint32_t v)
{
    int32_t bar;
    voices_now = v;
    if (v > voices_max)
        voices_max = v;
    if (state == PL_STOPPED) {
        if (stop_frames && frames - stop_frames >= FRAME_RATE && player_stats.voices_after_stop == 0xFFFFFFFFu)
            player_stats.voices_after_stop = v;
        return;
    }
    player_stats.blocks++;
    if (render_ticks > player_stats.render_max)
        player_stats.render_max = render_ticks;
    bar = pub_tick >= 0 ? pub_tick / (int32_t)song->bar_ticks : -1;
    if (bar >= 0 && bar < (int32_t)PL_STATS_BARS) {
        uint32_t cpu = plat_cpu_pct();
        if (v > player_stats.voices[bar])
            player_stats.voices[bar] = (uint16_t)v;
        if (cpu > player_stats.cpu[bar])
            player_stats.cpu[bar] = (uint8_t)(cpu > 255u ? 255u : cpu);
    }
}

void player_panic(void) { player_play(0); }
