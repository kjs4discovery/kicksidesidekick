/* Licence: TBD (see D017) */
/* The song player (player.h). Written for this project (D017): nothing here comes from another sequencer.
 *
 * Each layer plays one variant's stream; idx[l] is the next event to send. A block sends every event whose tick is
 * before the block's end, at the block's start: notes start up to one block (5.8 ms) early, the same as EAS's own
 * file player. Notes are tracked per layer (a 128-bit set), so a mute, a variant change, a loop jump or a stop can
 * release exactly the notes that sound, and a muted layer never leaves one hanging.
 * M2: the tempo in BPM; play from any tick (a section, a bar) with a count-in that ends where the music starts;
 * pause; a loop the position is already past jumps at the next bar line. */
#include "player.h"
#include "synth.h"
#include "plat.h"

#define BIAS_BARS 3                       /* the count-in runs below tick 0: positions are kept this far above (M3:
                                           * up to 2 bars of count-in before a pickup) */
#define PL_EVENTS_PER_BLOCK 128u          /* a block sends at most this many; the rest go in the next */
#define CLICK_CH 9u                       /* GM percussion */
/* the count-in (M3): a beep that cuts through, on a channel the song does not use: GM 81 Lead 1 (square), C6 on a
 * bar's first beat and G5 on the others, 70 ms; its channel volume the count-in volume (a setting). With no free
 * channel: M2's wood-block clicks on the percussion channel */
#define BEEP_PROGRAM 80u
#define BEEP_BAR 84u
#define BEEP_BEAT 79u
#define BEEP_VEL_BAR 127u
#define BEEP_VEL_BEAT 112u
#define BEEP_BLOCKS 12u                   /* its length: 12 blocks of 256 frames, 70 ms */
#define CLICK_BAR 76u                     /* (no free channel) hi wood block on beat 1, low on the others */
#define CLICK_BEAT 77u
#define FRAME_RATE 44100u
#define DEN (FRAME_RATE * 60u)            /* a tick is DEN units of position; a frame adds ppq * bpm */

volatile struct player_stats player_stats = {.magic = PL_STATS_MAGIC, .version = 4, .ui_view = 0xFFu, .part = 0xFFu};

/* ------------------------------------------------- requests (main loop -> interrupt) --- */
static const struct song *volatile want_song;
static volatile uint32_t song_req, song_done, play_req, play_done, loop_req, loop_done;
static volatile uint8_t play_cmd, countin_want = 1, start_countin;   /* play_cmd: 0 stop, 1 start, 2 pause */
static volatile uint8_t countin_bars_want = 1, beep_vol_want = 127;
static volatile uint8_t quiet_mask, quiet_q8;   /* M4 (D027): the layers set to QUIET play their velocities x
                                                  * quiet_q8 / 256 (M3's guide for your part, now for any layer) */
static volatile int32_t start_want;
static volatile uint32_t mute_want, tempo_want = 120;
static volatile uint8_t var_want[SONG_LAYERS] = {1, 1, 1, 1};
static volatile int8_t transpose_want, bass_oct_want;
static volatile uint8_t hold_want, pub_held;   /* M5: layers held (their streams being rebuilt) */   /* bass_oct: M5, the bass layer this far up (sound only) */
static volatile int loop_mode_want;
static volatile uint32_t loop_from_want, loop_to_want;

/* ------------------------------------------------------- the interrupt's own state --- */
static const struct song *song;
static int state = PL_STOPPED;
static uint64_t q;                        /* (tick + bias) * DEN */
static uint32_t bias, inc;                /* inc = ppq * bpm: a frame's worth */
static uint32_t bpm_cur = 120;
static int32_t music_from;                /* the count-in ends here: clicks below it, the song's events from it */
static uint32_t idx[SONG_LAYERS];
static uint8_t var_cur[SONG_LAYERS] = {1, 1, 1, 1};
static uint8_t hold_cur;
static uint32_t mute_cur;
static int32_t transpose_cur, bass_oct_cur;
static uint8_t counting;                  /* M9: a count-in is under way: set by a start that schedules one, cleared when the
                                           * position reaches music_from; clicks and PL_COUNT_IN need it, so a loop jump
                                           * back below music_from never counts in again */
static uint32_t active[SONG_LAYERS][4];   /* notes sounding, per layer */
static int loop_mode;
static int32_t loop_from, loop_to;
static uint8_t click_on;                  /* the count-in note sounding (0: none), on click_ch */
static uint8_t click_ch = CLICK_CH, click_left;   /* its channel (the beep's: a free one); blocks until it ends */
static uint32_t budget_left;
static uint32_t stop_frames;

/* -------------------------------------------------------- published (interrupt -> main loop) --- */
static volatile uint32_t frames;
static volatile int32_t pub_tick;
static volatile uint32_t pub_tpms_q16;    /* ticks per ms, Q16, at the tempo now */
static volatile int pub_state = PL_STOPPED;
static volatile int32_t pub_from;
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

void player_start(int32_t from, int count_in)
{
    start_want = from;
    start_countin = (uint8_t)(count_in != 0);
    play_cmd = 1;
    __asm__ volatile("" ::: "memory");
    play_req++;
}

void player_play(int on)
{
    const struct song *s = want_song;
    if (on && s) {
        player_start((int32_t)s->start_tick, countin_want);
        return;
    }
    play_cmd = 0;
    __asm__ volatile("" ::: "memory");
    play_req++;
}

void player_pause(void)
{
    play_cmd = 2;
    __asm__ volatile("" ::: "memory");
    play_req++;
}

uint32_t player_song_bpm(const struct song *s) { return s ? (60000000u + s->tempo_us / 2u) / s->tempo_us : 120u; }

void player_mute(uint32_t layers) { mute_want = layers & ((1u << SONG_LAYERS) - 1u); }

void player_variant(uint32_t layer, uint32_t v)
{
    if (layer < SONG_LAYERS && v >= 1u && v <= SONG_MAX_VARIANTS)
        var_want[layer] = (uint8_t)v;
}

void player_hold(uint32_t layers) { hold_want = (uint8_t)(layers & ((1u << SONG_LAYERS) - 1u)); }
uint32_t player_held(void) { return pub_held; }

void player_tempo(uint32_t b)
{
    tempo_want = b < PL_BPM_MIN ? PL_BPM_MIN : b > PL_BPM_MAX ? PL_BPM_MAX : b;
}

void player_count_in(int on) { countin_want = (uint8_t)(on != 0); }
/* QUIET at pct % of a layer's loudness: EAS's gain goes about with the velocity squared, so the velocities are scaled
 * by the square root (50 % -> x 0.71, 25 % -> x 0.5; a plain 30 % of the velocity sounded at 9 %, M3) */
void player_bass_octave(int32_t semis) { bass_oct_want = (int8_t)(semis > 12 ? 12 : semis < 0 ? 0 : semis); }

void player_quiet(uint32_t layers, uint32_t pct)
{
    uint32_t want = (pct > 100u ? 100u : pct) * 65536u / 100u, q = 0;
    while ((q + 1u) * (q + 1u) <= want && q < 255u)
        q++;
    quiet_q8 = (uint8_t)(pct ? (q ? q : 1u) : 0u);
    quiet_mask = (uint8_t)(layers & ((1u << SONG_LAYERS) - 1u));
}

void player_count_in_bars(uint32_t bars) { countin_bars_want = (uint8_t)(bars >= 2u ? 2u : 1u); }
void player_count_in_volume(uint32_t cc7) { beep_vol_want = (uint8_t)(cc7 > 127u ? 127u : cc7); }

void player_transpose(int32_t t) { transpose_want = (int8_t)(t < -11 ? -11 : t > 11 ? 11 : t); }
int32_t player_transpose_of(void) { return transpose_want; }

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

int32_t player_play_from(void) { return pub_from; }

int32_t player_tick(uint32_t lead_ms)
{
    const struct song *s = want_song;
    int32_t t = pub_tick + (int32_t)(lead_ms * pub_tpms_q16 >> 16);
    if (pub_state == PL_STOPPED || pub_state == PL_PAUSED || !s)
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

static int32_t tick_of(uint64_t v) { return (int32_t)(v / DEN) - (int32_t)bias; }
static uint64_t q_of(int32_t tick) { return mul64((uint32_t)(tick + (int32_t)bias), DEN); }

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
        synth_send_now((uint8_t)(0x80u | click_ch), click_on, 0);
        click_on = 0;
    }
}

static const struct song_stream *stream(uint32_t l) { return &song->layer[l].var[var_cur[l] - 1u]; }

static void seek_all(int32_t tick)
{
    uint32_t l;
    for (l = 0; l < SONG_LAYERS; l++)
        if (song->layer[l].variants && !(hold_cur >> l & 1u))
            idx[l] = song_seek(stream(l), tick > 0 ? (uint32_t)tick : 0);
}

/* the note as sent: transposed, except on the percussion channel (its notes are drums, not pitches) */
static uint32_t pitch(uint32_t l, uint32_t n)
{
    int32_t t = (int32_t)n + transpose_cur + (l == 1u ? bass_oct_cur : 0);   /* (layer 1: the bass) */
    if (song->layer[l].channel == CLICK_CH)
        return n;
    return (uint32_t)(t < 0 ? t + 12 * ((11 - t) / 12) : t > 127 ? t - 12 * ((t - 116) / 12) : t);
}

static void dispatch(uint32_t l, const struct song_event *e)
{
    uint32_t n = e->status == 0x90u || e->status == 0x80u ? pitch(l, e->d1) : e->d1;
    uint32_t w = n >> 5, bit = 1u << (n & 31u);
    switch (e->status) {
    case 0x90u: {
        uint32_t vel = e->d2;
        if (mute_cur >> l & 1u) {
            player_stats.notes_skipped[l]++;
            return;
        }
        if ((quiet_mask >> l & 1u) && quiet_q8) {   /* QUIET (M4, D027) */
            vel = vel * quiet_q8 / 256u;
            vel = vel ? vel : 1u;
            player_stats.notes_guided++;
        }
        if (active[l][w] & bit)
            send(l, 0x80u, n, 0);         /* restarted: end the one sounding */
        active[l][w] |= bit;
        send(l, 0x90u, n, vel);
        player_stats.notes_on[l]++;
        player_stats.note_on_last[l] = n;
        return;
    }
    case 0x80u:
        if (active[l][w] & bit) {
            active[l][w] &= ~bit;
            send(l, 0x80u, n, 0);
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
        if (!song->layer[l].variants || (hold_cur >> l & 1u))
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

/* the beep's channel: the first from 15 down that no layer, the keys (16) or the percussion (10) use; none: the clicks */
static void click_channel_pick(void)
{
    uint32_t c, l, used = 1u << CLICK_CH | 1u << SYNTH_KEYS_CH;
    for (l = 0; l < SONG_LAYERS; l++)
        if (song->layer[l].variants)
            used |= 1u << song->layer[l].channel;
    click_ch = CLICK_CH;
    for (c = 15; c-- > 0;)
        if (!(used >> c & 1u)) {
            click_ch = (uint8_t)c;
            break;
        }
}

/* before a count-in: the beep's sound and volume on its channel */
static void click_channel_setup(void)
{
    if (click_ch == CLICK_CH)
        return;
    synth_send_now((uint8_t)(0xB0u | click_ch), 121u, 0);
    synth_send_now((uint8_t)(0xC0u | click_ch), BEEP_PROGRAM, 0);
    synth_send_now((uint8_t)(0xB0u | click_ch), 7u, beep_vol_want);
    synth_send_now((uint8_t)(0xB0u | click_ch), 10u, 64u);
}

static void clicks(int32_t from, int32_t to)
{
    int32_t ppq = (int32_t)song->ppq, t;
    if (click_on && (click_left == 0 || --click_left == 0)) {
        synth_send_now((uint8_t)(0x80u | click_ch), click_on, 0);
        click_on = 0;
    }
    if (!counting || from >= music_from)
        return;
    /* the first beat at or after from (from may be negative: round towards +infinity by hand) */
    t = from >= 0 ? (from + ppq - 1) / ppq * ppq : -((-from) / ppq * ppq);
    if (t < to && t < music_from) {
        int32_t in_bar = (t + (int32_t)song->bar_ticks * BIAS_BARS) % (int32_t)song->bar_ticks;
        int beep = click_ch != CLICK_CH;
        uint32_t vel = in_bar == 0 ? (beep ? BEEP_VEL_BAR : 127u) : (beep ? BEEP_VEL_BEAT : 104u);
        if (click_on)
            synth_send_now((uint8_t)(0x80u | click_ch), click_on, 0);
        click_on = (uint8_t)(beep ? (in_bar == 0 ? BEEP_BAR : BEEP_BEAT) : (in_bar == 0 ? CLICK_BAR : CLICK_BEAT));
        click_left = beep ? (uint8_t)BEEP_BLOCKS : 1u;
        synth_send_now((uint8_t)(0x90u | click_ch), click_on, (uint8_t)vel);
        player_stats.clicks++;
        if (vel > player_stats.click_peak_vel)
            player_stats.click_peak_vel = vel;
    }
}

static void set_tempo(uint32_t b)
{
    bpm_cur = b;
    inc = song ? song->ppq * b : 0;
    pub_tpms_q16 = song ? (uint32_t)(mul64(song->ppq * b, 65536u) / 60000u) : 0;
    player_stats.bpm = b;
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
    if (song && state != PL_STOPPED && state != PL_PAUSED) {
        release_all();
        stop_frames = frames ? frames : 1u;
        player_stats.voices_after_stop = 0xFFFFFFFFu;   /* measured in player_after, 1 s on */
    }
    state = PL_STOPPED;
}

static void pause(uint32_t f0)
{
    if (state != PL_PLAYING && state != PL_COUNT_IN)
        return;
    release_all();
    state = PL_PAUSED;
    player_stats.pauses++;
    player_stats.pause_frames = f0;
    player_stats.pause_tick = tick_of(q);
    stop_frames = frames ? frames : 1u;
    player_stats.voices_after_stop = 0xFFFFFFFFu;
}

/* play from tick s0 (clamped into the song); with a count-in: from the bar before s0's bar, in clicks up to s0 */
static void start(uint32_t f0, int32_t s0, int count_in)
{
    uint32_t l;
    int32_t from, bt;
    stop();
    state = PL_STOPPED;
    bias = song->bar_ticks * BIAS_BARS;
    bt = (int32_t)song->bar_ticks;
    if (s0 < 0)
        s0 = 0;
    if (s0 >= (int32_t)song->end_tick)
        s0 = (int32_t)song->end_tick - 1;
    set_tempo(tempo_want);
    for (l = 0; l < SONG_LAYERS; l++) {
        uint32_t v = var_want[l], n = song->layer[l].variants;
        var_cur[l] = (uint8_t)(v < 1u ? 1u : v > n && n ? n : v);
    }
    mute_cur = mute_want;
    channel_setup();
    music_from = s0;
    from = count_in ? s0 / bt * bt - bt * (int32_t)countin_bars_want : s0;   /* 1 or 2 bars (a setting) */
    q = q_of(from);
    seek_all(s0);
    if (from < s0)
        click_channel_setup();
    counting = from < s0;
    state = counting ? PL_COUNT_IN : PL_PLAYING;
    if (counting)
        player_stats.countins++;
    pub_from = s0;
    player_stats.starts++;
    player_stats.start_frames = f0;
    player_stats.start_from = s0;
    player_stats.start_tick = from;
    stop_frames = 0;
}

/* M5: holding layers (their streams are being rebuilt): newly held, their notes released; let go, they go on from
 * the position now. The main loop writes a held layer's stream only once pub_held shows it */
static void holds(void)
{
    uint32_t l, h = hold_want, t;
    int32_t tk;
    if (!song) {
        hold_cur = (uint8_t)h;
        pub_held = (uint8_t)h;
        return;
    }
    for (l = 0; l < SONG_LAYERS; l++) {
        t = 1u << l;
        if (!song->layer[l].variants)
            continue;
        if ((h & t) && !(hold_cur & t) && state != PL_STOPPED)
            release(l);
        if (!(h & t) && (hold_cur & t)) {
            tk = tick_of(q);
            hold_cur &= (uint8_t)~t;
            idx[l] = song_seek(stream(l), tk > 0 && state != PL_STOPPED ? (uint32_t)tk : 0);
        }
    }
    hold_cur = (uint8_t)h;
    pub_held = (uint8_t)h;
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
            bias = song->bar_ticks * BIAS_BARS;
            music_from = (int32_t)song->start_tick;
            pub_from = music_from;
            pub_tick = music_from;
            set_tempo(tempo_want);
            click_channel_pick();
            player_stats.song_crc = song->data[12] | (uint32_t)song->data[13] << 8 |
                                    (uint32_t)song->data[14] << 16 | (uint32_t)song->data[15] << 24;
        }
    }
    holds();
    if (!song)
        return;
    if (play_req != play_done) {
        play_done = play_req;
        if (play_cmd == 1)
            start(f0, start_want, start_countin);
        else if (play_cmd == 2)
            pause(f0);
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
    if (tempo_want != bpm_cur) {
        set_tempo(tempo_want);
        if (state == PL_PLAYING || state == PL_COUNT_IN) {
            uint32_t n = player_stats.ntempo;
            if (n < PL_STATS_TEMPOS) {
                player_stats.tempo_frames[n] = f0;
                player_stats.tempo_pct[n] = bpm_cur;
                player_stats.tempo_tick[n] = tick_of(q);
            }
            player_stats.ntempo = n + 1u;
        }
    }
    if (state == PL_STOPPED || state == PL_PAUSED) {
        transpose_cur = transpose_want;
        bass_oct_cur = bass_oct_want;
        player_stats.transpose = transpose_cur;
        mute_cur = mute_want;
        for (l = 0; l < SONG_LAYERS; l++)
            var_cur[l] = var_want[l];
        if (state == PL_PAUSED)           /* a variant chosen while paused: its stream, from where it paused */
            seek_all(tick_of(q));
        return;
    }
    if (transpose_want != transpose_cur) {   /* another key: the sounding notes (at the old pitch) end now */
        for (l = 0; l < SONG_LAYERS; l++)
            if (song->layer[l].variants && song->layer[l].channel != CLICK_CH)
                release(l);
        transpose_cur = transpose_want;
        player_stats.transpose = transpose_cur;
    }
    if (bass_oct_want != bass_oct_cur) {  /* the bass sound's octave: its sounding notes end now */
        if (song->layer[1].variants && song->layer[1].channel != CLICK_CH)
            release(1);
        bass_oct_cur = bass_oct_want;
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
        if (v != var_cur[l] && !(hold_cur >> l & 1u)) {
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
    int32_t from, to, end, edge = 0;
    frames = f0 + n;
    requests(f0);
    if (!song || state == PL_STOPPED || state == PL_PAUSED) {
        pub_state = song ? state : PL_STOPPED;
        if (song && state == PL_PAUSED)
            pub_tick = tick_of(q);
        return;
    }
    budget_left = PL_EVENTS_PER_BLOCK;
    from = tick_of(q);
    q += mul64(n, inc);
    to = tick_of(q);
    clicks(from, to);
    end = (int32_t)song->end_tick;
    if (loop_mode != LOOP_OFF) {          /* the loop's end; or, playing outside the loop (it was moved), the next bar
                                           * line: the music goes there at a bar line, as a musician would */
        int32_t bt = (int32_t)song->bar_ticks;
        int outside = state == PL_PLAYING && from >= music_from && (from < loop_from || from >= loop_to);
        edge = outside ? (from + bt - 1) / bt * bt : loop_to;
        if (edge > end)
            edge = end;
    }
    if (loop_mode != LOOP_OFF && from <= edge && to >= edge && (from < edge || from < loop_from || from >= loop_to)) {
        run_events(edge);
        log_chords(from, edge, f0);
        release_all();
        q = q - q_of(edge) + q_of(loop_from);   /* (to >= edge: no underflow; the jump may go either way) */
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
        player_stats.blocks++;            /* (counted with the position, so a RAM dump sees the two agree) */
        player_stats.last_tick = end;
        stop();
        pub_tick = end;
        pub_state = PL_STOPPED;
        return;
    }
    run_events(to);
    log_chords(from, to, f0);
    if (counting && to >= music_from)
        counting = 0;                     /* (M9) the music has started: never a count-in again until player_start */
    state = counting ? PL_COUNT_IN : PL_PLAYING;
    player_stats.blocks++;
    player_stats.last_tick = to;
    pub_loop = loop_mode;
    pub_loop_from = loop_from;
    pub_loop_to = loop_to;
    pub_tick = to;
    pub_from = music_from;
    __asm__ volatile("" ::: "memory");
    pub_state = state;
}

void player_after(uint32_t render_ticks, uint32_t v)
{
    int32_t bar;
    voices_now = v;
    if (v > voices_max)
        voices_max = v;
    if (state == PL_STOPPED || state == PL_PAUSED) {
        if (stop_frames && frames - stop_frames >= FRAME_RATE && player_stats.voices_after_stop == 0xFFFFFFFFu)
            player_stats.voices_after_stop = v;
        return;
    }
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
