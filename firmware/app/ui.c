/* Licence: TBD (see D017) */
/* The screens and controls. docs/controls.md is the map (M2), with mock-ups and the reasons.
 * Included by app.c, in fm1-base's unity build after the platform (gfx.c's cv_*, FONT_*, C_*; lcd.c; announce.c).
 *
 * Splash (>= 3 s, the logo: tools/logo_to_c.py), then the song list: SELECT turns the cursor, PLAY (or EDIT) opens.
 * Play screen: PLAY play / pause / go on; HOME stop (back to the start point), stopped: the list; FX SEL ENV LFO mute
 *   drums / bass / harmony / melody; K1..K4 their level; SELECT tempo (BPM); PRESETS the start point by section (in
 *   LOOP BARS: bar A); ALGORITHM by bar (LOOP BARS: bar B); SEQ loop off / section / bars / song; ARP count-in.
 * Part 3: EDIT the view (chord / keyboard / chart); SAVE the KEY mode (SELECT changes the key, SAVE again or 4 s:
 *   done); the rhythm cue left of the chord, the bass notes when the bass is muted, the FM-1's keys lit for the part
 *   being played (docs/controls.md).
 * Everywhere: GLO the controls (help), OCT- / OCT+ the keys' octave, MASTER volume; the keys play Electric Piano 1
 *   on channel 16. REC is not used (held at power-on: the update mode), nothing is held at start-up. */
#include "synth.h"
#include "player.h"
#include "song.h"
#include "logo_gen.h"

static void ui_busy(const char *a, const char *b);
#include "library.c"

#define KEY_LOW 53                       /* key 0 is F3 (plat.h) */
#define VELOCITY 100u
#define OCT_MAX 3
#define LIST_ROWS 5u
#define LIST_ROW_H 36
#define SPLASH_MS 3000u                  /* the splash stays at least this long after power-on */
#define FLASH_MS 110u                    /* the beat light's flash */
#define CUE_MS 140u                      /* the rhythm cue's flash at each attack */
#define KEYMODE_MS 4000u                 /* the KEY mode ends this long after its last use */
#define W_CUE 34                         /* the rhythm cue's part of the chord band, left of the big chord */

/* the bands of the play screen, top to bottom */
#define Y_TOP 0
#define H_TOP 32
#define Y_CHORD 32
#define H_CHORD 74
#define W_BIG 176                        /* the big chord's part of the chord band; the rest: next + countdown */
#define Y_STRIP 106
#define H_STRIP 48                       /* the boxes (44) and the loop's line under them */
#define Y_CURSOR 154
#define H_CURSOR 6
#define Y_LAYERS 160
#define H_LAYERS 32
#define Y_STATUS 192
#define H_STATUS 22
#define Y_LEGEND 214
#define H_LEGEND 26
#define BOX_W 56
#define BOX_H 44
#define BOX_STEP 60

enum { V_LIST, V_PLAY, V_HELP };
enum { PV_CHORD, PV_KEYS, PV_CHART, PV_N };   /* the play screen's views (EDIT) */
enum { PART_NONE, PART_HARMONY, PART_BASS };  /* the part being played on the keys: the cue, the lights */
#define L_BASS 1u
#define L_HARM 2u

static const uint8_t LAYER_BUTTON[SONG_LAYERS] = {B_FX, B_SEL, B_ENV, B_LFO};
static const uint8_t LAYER_ENC[SONG_LAYERS] = {EN_K1, EN_K2, EN_K3, EN_K4};
static const char *const LAYER_NAME[SONG_LAYERS] = {"DRUMS", "BASS", "HARMONY", "MELODY"};
static const char *const LAYER_SHORT[SONG_LAYERS] = {"DRUMS", "BASS", "HARM", "MELODY"};
static const char *const LOOP_NAME[] = {"", "LOOP SECTION", "LOOP BARS", "LOOP SONG"};

static struct {
    int view, back;                      /* V_*; back: the view the help returns to */
    uint32_t sel, top;                   /* the list's cursor and first row */
    const struct song *s;
    uint32_t keys, buttons;
    uint8_t key_note[NKEYS];             /* the note each held key sent (the octave may change while it is held) */
    int32_t oct;                         /* the keys' octave, -OCT_MAX..OCT_MAX */
    uint32_t muted;
    int loop;
    uint32_t from_bar;                   /* the start point: a bar index; from_tick its first tick of music */
    int32_t from_tick;
    uint32_t a_bar, b_bar;               /* LOOP BARS */
    uint32_t tempo_ms;                   /* the last tempo click (quick turns: bigger steps) */
    int32_t key;                         /* the transposition, -5..+6 semitones (the key: the nearest way there) */
    uint32_t keymode_until;              /* the KEY mode: SELECT changes the key until then (0: off) */
    int pview;                           /* PV_* */
    uint32_t lit_keys, dim_keys;         /* the key lights for the part being played (frame_play sets them) */
    char msg[2][24];
    uint32_t msg_until;
    uint8_t flash_ok, splash, redraw, flashing;
    uint32_t lib_seen;                   /* library.c lib_changes when the screen was last set up */
    /* what is on the screen */
    int32_t sh_chord, sh_count, sh_next, sh_left, sh_bar, sh_beat, sh_strip, sh_layers, sh_section;
    int sh_state;
    uint32_t sh_top_key, sh_stats_ms, sh_list, sh_status, sh_legend, sh_cue, sh_view;
} ui;

/* -------------------------------------------------------------- the platform's calls --- */
void app_init(void)
{
    synth_init();
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    synth_render(lr, n, master_q12);
}

void app_panic(void)
{
    synth_panic();
}

/* ------------------------------------------------------------------------ text --- */
static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void num_cat(char *d, uint32_t v)
{
    char b[11];
    int i = 10;
    b[i] = 0;
    do {
        b[--i] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    text_cat(d, b + i);
}

static void snum_cat(char *d, int32_t v)
{
    text_cat(d, v < 0 ? "-" : v > 0 ? "+" : "");
    num_cat(d, (uint32_t)(v < 0 ? -v : v));
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.sh_top_key = 0xFFFFFFFFu;
    ui.sh_list = 0xFFFFFFFFu;
    if (ann_on()) {                      /* and to a screen reader (fm1-base announce.c) */
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void text_in(int32_t x, int32_t w, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(x + (w - text_w(f, s)) / 2, y, f, s, c);
}

/* text at twice a font's size, on a black canvas: each pixel a 2x2 block, its coverage as brightness */
static void text2x(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    uint32_t r = c >> 11, g = (c >> 5) & 63u, b = c & 31u;
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), gx, gy, w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi];
        for (gy = 0; gy < f->h; gy++)
            for (gx = 0; gx < w; gx++) {
                uint32_t a = gd[gy * bpr + gx / 2u];
                a = (gx & 1u) ? (a & 15u) : (a >> 4);
                if (a)
                    cv_rect(x + 2 * ((int32_t)gx - f->pad), y + 2 * (int32_t)gy, 2, 2,
                            (uint16_t)((r * a / 15u) << 11 | (g * a / 15u) << 5 | (b * a / 15u)));
            }
        x += 2 * f->adv[gi];
    }
}

static void frame_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    cv_rect(x, y, w, 1, c);
    cv_rect(x, y + h - 1, w, 1, c);
    cv_rect(x, y, 1, h, c);
    cv_rect(x + w - 1, y, 1, h, c);
}

static void lcd_done(void)               /* the last blit has reached the LCD (it runs by DMA) */
{
#ifndef PLAT_HOST
    lcd_sync();
#endif
}

/* -------------------------------------------------------------------------- splash --- */
/* the logo (logo_gen.h: tools/logo_to_c.py from firmware/assets/tmp-logo.jpg), its name and version under it. Called
 * by the platform at power-on, before anything else of the app runs; the list comes SPLASH_MS after power-on */
void app_splash(void)
{
    const uint8_t *p = LOGO_RLE, *end = LOGO_RLE + sizeof LOGO_RLE;
    uint32_t y0 = 0, x = 0, y = 0, band;
    int32_t ox = (240 - (int32_t)LOGO_W) / 2, oy = 12;
    lcd_fill(0, 0, 240, 240, LOGO_BG);
    while (y0 < LOGO_H) {                /* in bands the canvas can hold */
        band = LOGO_H - y0 < 112u ? LOGO_H - y0 : 112u;
        cv_begin(LOGO_W, band, LOGO_BG);
        while (p < end && y < y0 + band) {
            uint32_t c = *p >> 4, n = (*p & 15u) + 1u;
            if ((*p & 15u) == 15u)
                n = 16u + p[1], p++;
            p++;
            cv_rect((int32_t)x, (int32_t)(y - y0), (int32_t)n, 1, LOGO_PAL[c]);
            x += n;
            if (x >= LOGO_W)
                x = 0, y++;
        }
        cv_blit((uint32_t)ox, (uint32_t)oy + y0);
        y0 += band;
    }
    cv_begin(240, 40, LOGO_BG);
    text_in(0, 240, 0, &FONT_B, APP_NAME, RGB(60, 50, 40));
    text_in(0, 240, 22, &FONT_XS, "v" FW_VERSION, RGB(150, 136, 118));
    cv_blit(0, (uint32_t)oy + LOGO_H + 6u);
}

/* ---------------------------------------------------------------- the key (part 3, item 8) --- */
/* names for the key now: sharps or flats by the key's signature (its mode's parent major); C major mixed */
static const char *const SHARP[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const FLAT[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
static const char *const MIXED[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const uint8_t MODE_PARENT[7] = {0, 3, 10, 5, 7, 8, 1};   /* major minor dorian mixolydian lydian phrygian locrian */

static const char *const *names(void)
{
    uint32_t parent = (ui.s->key_root + MODE_PARENT[ui.s->key_mode % 7u] + 12u + (uint32_t)ui.key) % 12u;
    if (parent == 0)
        return MIXED;
    return parent == 5 || parent == 10 || parent == 3 || parent == 8 || parent == 1 ? FLAT : SHARP;
}

/* a note name at p (C, C#, Db, ...): its pitch class, *n its length; -1 if none */
static int pc_at(const char *p, uint32_t *n)
{
    static const uint8_t PC[7] = {9, 11, 0, 2, 4, 5, 7};   /* A..G */
    int pc;
    if (*p < 'A' || *p > 'G')
        return -1;
    pc = PC[*p - 'A'];
    *n = 1;
    if (p[1] == '#' || p[1] == 'b') {
        pc += p[1] == '#' ? 1 : 11;
        *n = 2;
    }
    return pc % 12;
}

/* a chord symbol (or a key's name: "Em", "D dorian") in the key now: the root and a slash bass moved and spelled,
 * the rest as written. Four rotating buffers: up to four in use at once */
static const char *spell(const char *sym)
{
    static char buf[4][24];
    static uint32_t k;
    const char *const *nm;
    char *o, *d;
    uint32_t n;
    int pc;
    if (!ui.key || (pc = pc_at(sym, &n)) < 0)
        return sym;                      /* not moved, or N.C. */
    nm = names();
    o = d = buf[k++ & 3u];
    *d = 0;
    text_cat(d, nm[(uint32_t)(pc + 12 + ui.key) % 12u]);
    while (*d)
        d++;
    for (sym += n; *sym && *sym != '/' && d < o + 18;)
        *d++ = *sym++;
    *d = 0;
    if (*sym == '/' && (pc = pc_at(sym + 1, &n)) >= 0) {
        text_cat(o, "/");
        text_cat(o, nm[(uint32_t)(pc + 12 + ui.key) % 12u]);
    }
    return o;
}

static const char *chord_name(uint32_t k) { return spell(song_cevt_symbol(ui.s, k)); }

/* a note's name with its octave (C4 = 60), in the key now's spelling (n: as sounded) */
static void note_cat(char *d, uint32_t n)
{
    text_cat(d, names()[n % 12u]);
    num_cat(d, n / 12u - 1u);
}

/* ---------------------------------------------------- the part being played (part 3, items 10, F, G) --- */
/* the part the keys play: the muted one of harmony and bass; both muted: harmony (the bass is still named) */
static int part(void)
{
    if (ui.muted >> L_HARM & 1u)
        return PART_HARMONY;
    if (ui.muted >> L_BASS & 1u)
        return PART_BASS;
    return PART_NONE;
}

static const struct song_stream *layer_stream(uint32_t l)
{
    const struct song_layer *ly = &ui.s->layer[l];
    uint32_t v = player_variant_of(l);
    if (!ly->variants)
        return 0;
    return &ly->var[(v > ly->variants ? ly->variants : v < 1u ? 1u : v) - 1u];
}

/* the layer's attack at or before tick (*at), how long its notes last (*len, ticks), the next attack (*next);
 * 0: none yet */
static int attack_at(uint32_t l, int32_t tick, uint32_t *at, uint32_t *len, uint32_t *next)
{
    const struct song_stream *st = layer_stream(l);
    struct song_event e;
    uint32_t i, k, a = 0, found = 0, end = 0;
    if (!st || !st->count || tick < 0)
        return 0;
    i = song_seek(st, (uint32_t)tick + 1u);   /* the first event after tick */
    *next = 0xFFFFFFFFu;
    for (k = i; k < st->count && k < i + 64u; k++) {
        song_event_get(st, k, &e);
        if (e.status == 0x90u) {
            *next = e.tick;
            break;
        }
    }
    for (k = i; k > 0 && i - k < 64u; k--) {   /* back to the last note-on */
        song_event_get(st, k - 1u, &e);
        if (e.status == 0x90u) {
            a = e.tick;
            found = 1;
            break;
        }
    }
    if (!found)
        return 0;
    for (k = song_seek(st, a); k < st->count && end == 0; k++) {   /* its first note-off: how long it is held */
        song_event_get(st, k, &e);
        if (e.status == 0x80u && e.tick > a)
            end = e.tick;
    }
    *at = a;
    *len = (end ? end : *next) - a;
    return 1;
}

/* the layer's note sounding at tick (its lowest), and the next one; 0xFF: none */
static void bass_notes(int32_t tick, uint32_t *now, uint32_t *next)
{
    const struct song_stream *st = layer_stream(L_BASS);
    struct song_event e;
    uint32_t i, k;
    *now = *next = 0xFFu;
    if (!st || !st->count || tick < 0)
        return;
    i = song_seek(st, (uint32_t)tick + 1u);
    for (k = i; k < st->count && k < i + 64u; k++) {
        song_event_get(st, k, &e);
        if (e.status == 0x90u) {
            *next = e.d1;
            break;
        }
    }
    for (k = i; k > 0 && i - k < 64u; k--) {
        song_event_get(st, k - 1u, &e);
        if (e.status == 0x90u) {
            *now = e.d1;
            break;
        }
    }
}

/* a note as the player sends it: transposed, folded into MIDI's range */
static uint32_t moved(uint32_t n)
{
    int32_t t = (int32_t)n + ui.key;
    while (t < 0)
        t += 12;
    while (t > 127)
        t -= 12;
    return (uint32_t)t;
}

/* a note's key on the FM-1 (with the keys' octave), folded into its 27 keys */
static uint32_t key_bit(uint32_t n)
{
    int32_t k = (int32_t)n - KEY_LOW - 12 * ui.oct;
    while (k < 0)
        k += 12;
    while (k >= (int32_t)NKEYS)
        k -= 12;
    return 1u << k;
}

/* the voicing of chord event k for the harmony's level (L1: level 1, L2 and L3: level 2), moved to the key now */
static uint32_t voicing(uint32_t k, uint8_t *out)
{
    const uint8_t *v = song_cevt_voicing(ui.s, k, player_variant_of(L_HARM) >= 2u ? 2u : 1u);
    uint32_t i, n = 0;
    for (i = 0; i < SONG_VOICING_NOTES; i++)
        if (v[i])
            out[n++] = (uint8_t)moved(v[i]);
    return n;
}

/* ------------------------------------------------------------------- the song's facts --- */
static uint32_t bar_ticks(void) { return ui.s->bar_ticks; }
static uint32_t first_bar(void) { return song_section_first(ui.s, 0); }   /* the first bar of music (1: a pickup) */
static uint32_t last_bar(void) { return ui.s->bar_count - 1u; }

/* where section i's music starts: its first bar, minus the pickup into it (docs/song-format.md 3.3) */
static uint32_t section_from(uint32_t i)
{
    uint32_t t = song_section_first(ui.s, i) * bar_ticks(), lead = song_section_lead(ui.s, i);
    return t > lead ? t - lead : 0;
}

static uint32_t section_end(uint32_t i)
{
    return (song_section_first(ui.s, i) + song_section_bars(ui.s, i)) * bar_ticks();
}

static uint32_t section_at(int32_t tick)
{
    uint32_t i = 0;
    while (i + 1u < ui.s->nsect && tick >= (int32_t)section_from(i + 1u))
        i++;
    return i;
}

static uint32_t bar_at(int32_t tick)
{
    uint32_t b = tick > 0 ? (uint32_t)tick / bar_ticks() : 0;
    return b < first_bar() ? first_bar() : b > last_bar() ? last_bar() : b;
}

/* the start point at bar b: a section's first bar keeps its pickup; another bar starts on its beat 1 */
static void set_from_bar(uint32_t b)
{
    uint32_t i;
    b = b < first_bar() ? first_bar() : b > last_bar() ? last_bar() : b;
    ui.from_bar = b;
    ui.from_tick = (int32_t)(b * bar_ticks());
    for (i = 0; i < ui.s->nsect; i++)
        if (song_section_first(ui.s, i) == b)
            ui.from_tick = (int32_t)section_from(i);
}

/* the loop's ticks (docs/song-format.md 3.3: a section loop runs from start - lead to end - lead) */
static void loop_region(int mode, uint32_t *from, uint32_t *to)
{
    const struct song *s = ui.s;
    uint32_t i = mode == LOOP_SONG ? 0 : section_at(ui.from_tick), last = mode == LOOP_SONG ? s->nsect - 1u : i;
    uint32_t lead = song_section_lead(s, i);
    if (mode == LOOP_BARS) {
        *from = ui.a_bar * bar_ticks();
        *to = (ui.b_bar + 1u) * bar_ticks();
        return;
    }
    *from = section_from(i);
    *to = section_end(last);
    *to = *to > lead ? *to - lead : *to;
}

static void set_loop(int mode)
{
    uint32_t from = 0, to = 0;
    ui.loop = mode;
    if (mode != LOOP_OFF)
        loop_region(mode, &from, &to);
    player_loop(mode, from, to);
    ui.sh_strip = -1;
}

/* where PLAY starts from stopped: the loop's start, or the start point */
static int32_t start_point(void)
{
    uint32_t from = 0, to = 0;
    if (ui.loop == LOOP_OFF)
        return ui.from_tick;
    loop_region(ui.loop, &from, &to);
    return (int32_t)from;
}

/* ------------------------------------------------------------------------ input --- */
static void open_song(uint32_t i)
{
    const struct song *s;
    int r = lib_open(i, &s);             /* checked in full at first use, not at boot */
    if (r) {
        app_say("SONG ERROR", song_error_name(r));
        ui.sh_list = 0xFFFFFFFFu;
        return;
    }
    ui.s = s;
    ui.muted = 0;
    player_load(ui.s);
    player_mute(0);
    for (r = 0; r < (int)SONG_LAYERS; r++)
        player_variant((uint32_t)r, 1);
    player_tempo(player_song_bpm(s));    /* every song opens at its own tempo, in its own key */
    ui.key = 0;
    ui.keymode_until = 0;
    player_transpose(0);
    set_from_bar(first_bar());
    ui.from_tick = (int32_t)section_from(0);
    set_loop(LOOP_OFF);
    ui.view = V_PLAY;
    ui.redraw = 1;
}

static void lights(void)
{
    uint32_t lit = 0, dim = 0, l, mid = 0;
    int st = player_state();
    if (ui.view == V_PLAY) {
        for (l = 0; l < SONG_LAYERS; l++) {
            if (!ui.s->layer[l].variants)
                continue;
            dim |= 1u << LAYER_BUTTON[l];
            if (!(ui.muted >> l & 1u))
                lit |= 1u << LAYER_BUTTON[l];
        }
        dim |= 1u << B_ARP | 1u << B_SEQ | 1u << B_HOME;
        if (player_count_in_of())
            lit |= 1u << B_ARP;
        if (ui.loop)
            lit |= 1u << B_SEQ;
        if (st == PL_PLAYING || st == PL_COUNT_IN) {   /* the beat light: a flash each beat, brighter on beat 1 */
            const struct song *s = ui.s;
            int32_t t = player_tick(PL_LEAD_BEAT_MS) + (int32_t)(s->bar_ticks * 2u);
            uint32_t in_beat = (uint32_t)t % s->ppq, beat1 = (uint32_t)t % s->bar_ticks < s->ppq;
            uint32_t ms = in_beat * 60000u / (s->ppq * player_tempo_of());
            if (ms < FLASH_MS) {
                if (beat1)
                    lit |= LED_PLAY_GREEN;
                else
                    mid |= LED_PLAY_GREEN;
            }
            if (((lit | mid) & LED_PLAY_GREEN) && !ui.flashing) {   /* (counted for the tests) */
                if (beat1)
                    player_stats.flash_bars++;
                else
                    player_stats.flash_beats++;
            }
            ui.flashing = (uint8_t)(((lit | mid) & LED_PLAY_GREEN) != 0);
        }
    }
    dim |= 1u << B_PLAY | 1u << B_GLO;
    if (ui.oct < 0)
        lit |= 1u << B_OCTDN;
    if (ui.oct > 0)
        lit |= 1u << B_OCTUP;
    if (ui.view != V_PLAY || st == PL_STOPPED)
        ui.lit_keys = ui.dim_keys = 0;
    player_stats.lit_keys = ui.lit_keys;
    player_stats.dim_keys = ui.dim_keys;
    plat_lights(lit, ui.keys | ui.lit_keys, dim, ui.dim_keys & ~ui.lit_keys, 1);   /* keys pressed: lit as ever */
    plat_lights_mid(mid, 0);
}

static void input_list(uint32_t down)
{
    int32_t d = plat_enc(EN_SELECT), n = (int32_t)lib_count();
    if (d && n) {
        int32_t s = (int32_t)ui.sel + d;
        ui.sel = (uint32_t)clamp(s, 0, n - 1);
    }
    if (((down >> B_PLAY & 1u) || (down >> B_EDIT & 1u)) && ui.sel < (uint32_t)n)
        open_song(ui.sel);
}

static void say_from(void)
{
    char t[24] = "BAR ";
    num_cat(t, ui.from_bar + ui.s->first_bar_number);
    app_say(ui.loop == LOOP_BARS ? "LOOP FROM" : "START", t);
}

static void input_play(uint32_t down, uint32_t now)
{
    uint32_t l;
    int32_t d;
    int st = player_state();
    if (down >> B_PLAY & 1u) {
        if (st == PL_STOPPED) {
            player_start(start_point(), player_count_in_of());
        } else if (st == PL_PAUSED) {     /* go on: count-in on, from the paused bar's beat 1; off, from right there */
            int32_t p = player_tick(0), bt = (int32_t)bar_ticks();
            if (player_count_in_of()) {
                p = p / bt * bt;
                if (p < (int32_t)ui.s->start_tick)
                    p = (int32_t)ui.s->start_tick;
            }
            player_start(p, player_count_in_of());
        } else {
            player_pause();
        }
    }
    if (down >> B_HOME & 1u) {
        if (st == PL_STOPPED) {
            ui.view = V_LIST;
            ui.redraw = 1;
            return;
        }
        player_play(0);                  /* stopped: back to the start point */
    }
    for (l = 0; l < SONG_LAYERS; l++) {
        if ((down >> LAYER_BUTTON[l] & 1u) && ui.s->layer[l].variants) {
            ui.muted ^= 1u << l;
            player_mute(ui.muted);
            app_say(LAYER_NAME[l], ui.muted >> l & 1u ? "MUTED" : "ON");
        }
        d = plat_enc(LAYER_ENC[l]);
        if (d && ui.s->layer[l].variants) {
            int32_t v = clamp((int32_t)player_variant_of(l) + (d > 0 ? 1 : -1), 1, ui.s->layer[l].variants);
            char t[8] = "LEVEL ";
            player_variant(l, (uint32_t)v);
            t[6] = (char)('0' + v);
            t[7] = 0;
            app_say(LAYER_NAME[l], t);
        }
    }
    if (ui.keymode_until && now >= ui.keymode_until) {
        ui.keymode_until = 0;
        ui.sh_legend = 0;
        ui.sh_top_key = 0xFFFFFFFFu;
    }
    if (down >> B_SAVE & 1u) {           /* the KEY mode on / off */
        ui.keymode_until = ui.keymode_until ? 0 : now + KEYMODE_MS;
        ui.sh_legend = 0;
        ui.sh_top_key = 0xFFFFFFFFu;
    }
    if (down >> B_EDIT & 1u) {           /* the next view */
        ui.pview = (ui.pview + 1) % PV_N;
        player_stats.view = (uint32_t)ui.pview;
        ui.redraw = 1;
        app_say("VIEW", ui.pview == PV_CHORD ? "CHORD" : ui.pview == PV_KEYS ? "KEYBOARD" : "CHART");
    }
    d = plat_enc(EN_SELECT);
    if (d && ui.keymode_until) {         /* the key: a semitone a click, all twelve keys, the nearest way (-5..+6) */
        char t[24] = "";
        ui.key = ((ui.key + d) % 12 + 12 + 5) % 12 - 5;
        player_transpose(ui.key);
        ui.keymode_until = now + KEYMODE_MS;
        ui.redraw = 1;
        text_cat(t, spell(ui.s->key));
        text_cat(t, " (");
        snum_cat(t, ui.key);
        text_cat(t, ")");
        app_say("KEY", t);
        d = 0;
    }
    if (d) {                             /* tempo: 1 BPM a click; turned quickly, 5 */
        int32_t step = (d > 1 || d < -1 || now - ui.tempo_ms < 90u) ? 5 : 1;
        ui.tempo_ms = now;
        player_tempo((uint32_t)clamp((int32_t)player_tempo_of() + step * d, PL_BPM_MIN, PL_BPM_MAX));
        ui.sh_top_key = 0xFFFFFFFFu;
    }
    d = plat_enc(EN_PRESET);
    if (d) {
        if (ui.loop == LOOP_BARS) {      /* A, by bar (B moves along to stay at or after A) */
            ui.a_bar = (uint32_t)clamp((int32_t)ui.a_bar + d, (int32_t)first_bar(), (int32_t)last_bar());
            if (ui.b_bar < ui.a_bar)
                ui.b_bar = ui.a_bar;
            set_from_bar(ui.a_bar);
            set_loop(LOOP_BARS);
        } else {                         /* the start point, by section */
            int32_t i = clamp((int32_t)section_at(ui.from_tick) + d, 0, (int32_t)ui.s->nsect - 1);
            set_from_bar(song_section_first(ui.s, (uint32_t)i));
            if (ui.loop == LOOP_SECTION)
                set_loop(LOOP_SECTION);
            app_say("START", song_section_name(ui.s, (uint32_t)i));
        }
        if (ui.loop == LOOP_BARS)
            say_from();
        ui.sh_status = 0xFFFFFFFFu;
    }
    d = plat_enc(EN_ALGO);
    if (d) {
        if (ui.loop == LOOP_BARS) {      /* B, by bar */
            char t[24] = "BAR ";
            ui.b_bar = (uint32_t)clamp((int32_t)ui.b_bar + d, (int32_t)ui.a_bar, (int32_t)last_bar());
            set_loop(LOOP_BARS);
            num_cat(t, ui.b_bar + ui.s->first_bar_number);
            app_say("LOOP TO", t);
        } else {                         /* the start point, by bar */
            uint32_t sec = section_at(ui.from_tick);
            set_from_bar((uint32_t)clamp((int32_t)ui.from_bar + d, (int32_t)first_bar(), (int32_t)last_bar()));
            if (ui.loop == LOOP_SECTION && section_at(ui.from_tick) != sec)
                set_loop(LOOP_SECTION);
            say_from();
        }
        ui.sh_status = 0xFFFFFFFFu;
    }
    if (down >> B_ARP & 1u) {
        player_count_in(!player_count_in_of());
        app_say("COUNT-IN", player_count_in_of() ? "ON" : "OFF");
    }
    if (down >> B_SEQ & 1u) {
        int mode = ui.loop == LOOP_OFF ? LOOP_SECTION : ui.loop == LOOP_SECTION ? LOOP_BARS :
                   ui.loop == LOOP_BARS ? LOOP_SONG : LOOP_OFF;
        if (st == PL_PLAYING || st == PL_PAUSED) {   /* playing: the loop takes the bar / section playing */
            set_from_bar(bar_at(player_tick(PL_LEAD_BEAT_MS)));
            if (mode == LOOP_SECTION)
                set_from_bar(song_section_first(ui.s, section_at(player_tick(PL_LEAD_BEAT_MS))));
        }
        if (mode == LOOP_BARS) {
            ui.a_bar = ui.from_bar;
            ui.b_bar = ui.a_bar + 3u > last_bar() ? last_bar() : ui.a_bar + 3u;
        }
        set_loop(mode);
        app_say("LOOP", mode == LOOP_OFF ? "OFF" : mode == LOOP_SECTION ? "SECTION" : mode == LOOP_BARS ?
                "BARS A-B" : "SONG");
    }
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), down = btn & ~ui.buttons, k, now = plat_ms();
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            uint8_t note = (uint8_t)(KEY_LOW + (int32_t)k + 12 * ui.oct);
            ui.key_note[k] = note;
            synth_midi(0x90u | SYNTH_KEYS_CH, note, (uint8_t)VELOCITY);
            player_stats.key_note_last = note;
        } else if (!(keys & bit) && (ui.keys & bit)) {
            synth_midi(0x80u | SYNTH_KEYS_CH, ui.key_note[k], 0);
        }
    }
    ui.keys = keys;
    ui.buttons = btn;
    if (ui.splash) {                     /* the splash is still up: only the keys */
        lights();
        return;
    }
    if (down & (1u << B_OCTDN | 1u << B_OCTUP)) {   /* the keys' octave (D021; held at power-on: fm1-base's) */
        char t[16] = "OCTAVE ";
        ui.oct = clamp(ui.oct + ((down >> B_OCTUP & 1u) ? 1 : 0) - ((down >> B_OCTDN & 1u) ? 1 : 0), -OCT_MAX, OCT_MAX);
        player_stats.keys_octave = ui.oct;
        snum_cat(t, ui.oct);
        app_say("KEYS", ui.oct ? t : "OCTAVE 0");
    }
    if (ui.view == V_HELP) {             /* any button closes the help */
        if (down & ~(1u << B_OCTDN | 1u << B_OCTUP)) {
            ui.view = ui.back;
            ui.redraw = 1;
        }
        lights();
        return;
    }
    if (down >> B_GLO & 1u) {
        ui.back = ui.view;
        ui.view = V_HELP;
        ui.redraw = 1;
        lights();
        return;
    }
    if (ui.view == V_LIST)
        input_list(down);
    else
        input_play(down, now);
    lights();
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------------ legend --- */
static void draw_legend(int32_t y, const char *t)
{
    cv_begin(240, H_LEGEND, C_BLACK);
    cv_rect(0, 1, 240, 1, C_LINE);
    text_in(0, 240, 7, &FONT_XS, t, C_DIM);
    cv_blit(0, (uint32_t)y);
}

/* ------------------------------------------------------------------------ list --- */
static void draw_list_top(void)
{
    cv_begin(240, H_TOP, C_BLACK);
    cv_text(6, 4, &FONT_B, "SONGS", C_WHITE);
    cv_text(234 - text_w(&FONT_XS, "v" FW_VERSION), 6, &FONT_XS, "v" FW_VERSION, C_DIM);
    if (plat_ms() < ui.msg_until) {
        char t[48] = "";
        text_cat(t, ui.msg[0]);
        text_cat(t, " ");
        text_cat(t, ui.msg[1]);
        cv_text(70, 6, &FONT_XS, t, C_AMB);
    } else if (!ui.flash_ok) {
        cv_text(80, 6, &FONT_XS, "RAM ONLY", C_AMB);
    }
    cv_rect(0, H_TOP - 1, 240, 1, C_LINE);
    cv_blit(0, Y_TOP);
}

/* nothing to list: why, and what to do (no crash, no blank screen) */
static void draw_list_empty(void)
{
    cv_begin(240, 240 - H_TOP - H_LEGEND, C_BLACK);
    if (lib_state < 0) {
        text_in(0, 240, 30, &FONT_B, "NO SONG STORAGE", C_AMB);
        text_in(0, 240, 60, &FONT_XS, "The flash chip is not usable", C_GRAY);
        text_in(0, 240, 76, &FONT_XS, "(RAM ONLY): songs cannot be kept.", C_GRAY);
    } else {
        text_in(0, 240, 24, &FONT_B, "NO SONGS YET", C_WHITE);
        text_in(0, 240, 56, &FONT_XS, "Send songs from the computer,", C_GRAY);
        text_in(0, 240, 72, &FONT_XS, "with this FM-1 on USB:", C_GRAY);
        text_in(0, 240, 98, &FONT_XS, "tools/fm1song.sh send songs/*", C_HI);
        text_in(0, 240, 130, &FONT_XS, "They stay here when it is off.", C_DIM);
    }
    cv_blit(0, H_TOP);
    draw_legend(Y_LEGEND, "GLO: CONTROLS   KEYS: PLAY ALONG");
}

static void draw_list(void)
{
    uint32_t r, n = lib_count();
    if (ui.sel < ui.top)
        ui.top = ui.sel;
    if (ui.sel >= ui.top + LIST_ROWS)
        ui.top = ui.sel - LIST_ROWS + 1u;
    draw_list_top();
    if (!n) {
        draw_list_empty();
        return;
    }
    for (r = 0; r < LIST_ROWS; r++) {
        uint32_t i = ui.top + r;
        int sel = i == ui.sel;
        cv_begin(240, LIST_ROW_H, C_BLACK);
        if (i < n) {
            int err;
            const struct song *s = lib_peek(i, &err);
            if (sel)
                cv_rect(2, 1, 236, LIST_ROW_H - 2, C_LINE);
            if (s) {
                char t[40] = "";
                text_cat(t, s->key);
                text_cat(t, "   ");
                num_cat(t, s->beats_per_bar);
                text_cat(t, "/4   ");
                num_cat(t, player_song_bpm(s));
                text_cat(t, " BPM");
                cv_text(10, 1, &FONT_B, s->title, sel ? C_WHITE : C_GRAY);
                cv_text(10, 19, &FONT_XS, t, sel ? C_HI : C_DIM);
            } else {
                char t[48] = "";
                const struct song *pk = &peeked[order[i]];
                text_cat(t, peek_err[order[i]] ? "(unreadable song)" : pk->title);
                cv_text(10, 1, &FONT_B, t, sel ? C_WHITE : C_GRAY);
                t[0] = 0;
                text_cat(t, "SONG ERROR: ");
                text_cat(t, song_error_name(err));
                text_cat(t, "  (send it again)");
                cv_text(10, 19, &FONT_XS, t, C_AMB);
            }
        }
        cv_blit(0, H_TOP + (int32_t)r * LIST_ROW_H);
    }
    draw_legend(Y_LEGEND, "SELECT: CHOOSE   PLAY: OPEN   GLO: ?");
}

/* -------------------------------------------------------------------------- help --- */
/* every control: the encoders (left of the screen on the panel) left, K1..K4 and the buttons (right of it) right;
 * what each does here (docs/controls.md). Two bands: the canvas holds 144 rows */
static void draw_help(void)
{
    static const char *const ENC[5][2] = {{"MASTER", "volume"}, {"SELECT", "tempo (BPM)"},
                                          {"PRESETS", "start: section / A"}, {"ALGORITHM", "start: bar / B"},
                                          {"OCT- OCT+", "keys' octave"}};
    static const char *const ENC_LIST[5][2] = {{"MASTER", "volume"}, {"SELECT", "choose a song"},
                                               {"PRESETS", "-"}, {"ALGORITHM", "-"}, {"OCT- OCT+", "keys' octave"}};
    static const char *const BTN[9][2] = {{"K1-K4", "levels"}, {"FX..LFO", "mute a layer"},
                                          {"PLAY", "play / pause"}, {"HOME", "stop / list"}, {"SEQ", "loop"},
                                          {"ARP", "count-in"}, {"EDIT", "view"}, {"SAVE", "key (SELECT)"},
                                          {"keys", "play along"}};
    static const char *const BTN_LIST[9][2] = {{"PLAY", "open the song"}, {"EDIT", "open the song"},
                                               {"keys", "play along"}, {"", ""}, {"", ""}, {"", ""}, {"", ""},
                                               {"", ""}, {"", ""}};
    const char *const (*e)[2] = ui.back == V_PLAY ? ENC : ENC_LIST;
    const char *const (*b)[2] = ui.back == V_PLAY ? BTN : BTN_LIST;
    uint32_t r, band;
    char t[48] = "CPU ";
    for (band = 0; band < 2u; band++) {  /* rows 0..119, 120..239 */
        int32_t y0 = (int32_t)band * 120;
        cv_begin(240, 120, C_BLACK);
        cv_band(0, 120);
        if (!band) {
            cv_text(6, 2, &FONT_B, "CONTROLS", C_WHITE);
            cv_text(234 - text_w(&FONT_XS, "any button: back"), 6, &FONT_XS, "any button: back", C_DIM);
            cv_rect(0, 24, 240, 1, C_LINE);
        }
        for (r = 0; r < 5u; r++) {       /* the encoders, 2 lines each */
            int32_t y = 30 + (int32_t)r * 30 - y0;
            cv_text(6, y, &FONT_XS, e[r][0], C_GRAY);
            cv_text(6, y + 13, &FONT_XS, e[r][1], C_HI);
        }
        for (r = 0; r < 9u; r++) {       /* the buttons, a line each */
            int32_t y = 30 + (int32_t)r * 17 - y0;
            cv_text(116, y, &FONT_XS, b[r][0], C_GRAY);
            cv_text(152, y, &FONT_XS, b[r][1], C_HI);
        }
        cv_text(116, 184 - y0, &FONT_XS, "GLO", C_GRAY);
        cv_text(152, 184 - y0, &FONT_XS, "this help", C_HI);
        if (band) {
            cv_rect(0, 200 - y0, 240, 1, C_LINE);
            num_cat(t, plat_cpu_pct());
            text_cat(t, "%   VOICES ");
            num_cat(t, player_voices());
            text_cat(t, "/");
            num_cat(t, player_voices_max());
            cv_text(6, 204 - y0, &FONT_XS, t, C_DIM);
            cv_text(234 - text_w(&FONT_XS, "v" FW_VERSION), 204 - y0, &FONT_XS, "v" FW_VERSION, C_DIM);
            cv_text(6, 220 - y0, &FONT_XS, ui.flash_ok ? "FM-1 Accompaniment" : "FM-1 Accompaniment  RAM ONLY", C_DIM);
        }
        cv_blit(0, (uint32_t)y0);
    }
}

/* ------------------------------------------------------------------------ play --- */
static void draw_top(int32_t bar, int32_t beat, int st)
{
    char t[48] = "";
    const struct song *s = ui.s;
    cv_begin(240, H_TOP, C_BLACK);
    cv_text(6, 1, &FONT_XS, s->title, C_GRAY);
    if (plat_ms() < ui.msg_until) {
        text_cat(t, ui.msg[0]);
        text_cat(t, " ");
        text_cat(t, ui.msg[1]);
        cv_text(6, 16, &FONT_XS, t, C_WHITE);
    } else {
        num_cat(t, player_tempo_of());
        text_cat(t, " BPM  ");
        text_cat(t, spell(s->key));
        if (ui.key) {
            text_cat(t, " (");
            snum_cat(t, ui.key);
            text_cat(t, ")");
        }
        if (player_count_in_of())
            text_cat(t, "  CNT");
        if (ui.loop) {
            text_cat(t, "  ");
            text_cat(t, LOOP_NAME[ui.loop]);
        }
        cv_text(6, 16, &FONT_XS, t, player_tempo_of() == player_song_bpm(s) && !ui.key ? C_DIM : C_AMB);
    }
    if (ui.keymode_until) {              /* the KEY mode: what SELECT does now */
        char k[32] = "KEY: ";
        text_cat(k, spell(s->key));
        text_cat(k, " (");
        snum_cat(k, ui.key);
        text_cat(k, ")  SELECT");
        cv_rect(4, 15, text_w(&FONT_XS, k) + 8, 16, C_HI);
        cv_text(8, 16, &FONT_XS, k, C_BLACK);
    }
    t[0] = 0;
    if (st == PL_PLAYING) {
        num_cat(t, (uint32_t)bar + s->first_bar_number);
        text_cat(t, ":");
        num_cat(t, (uint32_t)beat + 1u);
    } else {
        text_cat(t, st == PL_COUNT_IN ? "IN" : st == PL_PAUSED ? "PAUSE" : "STOP");
    }
    cv_text(234 - text_w(&FONT_M, t), 4, &FONT_M, t, st == PL_PLAYING ? C_WHITE : C_GRAY);
    cv_blit(0, Y_TOP);
}

/* the rhythm cue: a note symbol for how long the part's attack lasts (whole, half, quarter, eighth, sixteenth),
 * bright at the attack (CUE_MS), dim between; drawn (there is no music font). kind 0: none */
static void draw_cue(int32_t x, int32_t y, uint32_t kind, int bright)
{
    static const uint8_t W[9] = {5, 9, 11, 11, 11, 11, 11, 9, 5};   /* the head's rows (11 x 9), drawn twice the size */
    uint16_t c = bright ? C_HI : C_GRAY;
    int32_t i, hy = y + 44;
    if (!kind)
        return;
    for (i = 0; i < 9; i++) {
        int32_t w = W[i], ox = (11 - w) / 2 + (4 - i) / 3;   /* tilted a little */
        if (kind <= 2u && i > 1 && i < 7) {                   /* whole and half: hollow */
            cv_rect(x + 2 * ox, hy + 2 * i, 6, 2, c);
            cv_rect(x + 2 * (ox + w) - 6, hy + 2 * i, 6, 2, c);
        } else {
            cv_rect(x + 2 * ox, hy + 2 * i, 2 * w, 2, c);
        }
    }
    if (kind >= 2u)
        cv_rect(x + 19, y, 3, 50, c);     /* the stem */
    for (i = 0; kind >= 4u && i < (int32_t)kind - 3; i++) {   /* flags: one for an eighth, two for a sixteenth */
        cv_line(x + 21, y + i * 10, x + 31, y + 14 + i * 10, c);
        cv_line(x + 22, y + i * 10, x + 32, y + 14 + i * 10, c);
    }
}

/* the cue for the part being played at tick: kind (1 whole .. 5 sixteenth), bright (an attack just now) */
static uint32_t cue_at(int32_t tick, int *bright)
{
    uint32_t at, len, next, ppq = ui.s->ppq, l = part() == PART_BASS ? L_BASS : L_HARM;
    *bright = 0;
    if (!attack_at(l, tick, &at, &len, &next))
        return 0;
    *bright = (uint32_t)tick - at < CUE_MS * ppq * player_tempo_of() / 60000u;
    return len >= ppq * 7u / 2u ? 1u : len >= ppq * 7u / 4u ? 2u : len >= ppq * 7u / 8u ? 3u : len >= ppq * 7u / 16u ? 4u : 5u;
}

static void draw_big(int st, int32_t k, int32_t count, int32_t bar, int32_t beat, uint32_t cue)
{
    const char *sym = k >= 0 ? chord_name((uint32_t)k) : 0;
    cv_begin(W_BIG, H_CHORD, C_BLACK);
    if (st == PL_STOPPED) {
        char t[48] = "from bar ";
        num_cat(t, ui.from_bar + ui.s->first_bar_number);
        text_cat(t, " (");
        text_cat(t, song_section_name(ui.s, section_at(start_point())));
        text_cat(t, ")");
        text_in(0, W_BIG, 6, &FONT_XS, ui.s->title, C_GRAY);
        text_in(0, W_BIG, 24, &FONT_M, "PRESS PLAY", C_WHITE);
        text_in(0, W_BIG, 52, &FONT_XS, t, C_DIM);
    } else if (st == PL_PAUSED) {
        char t[24] = "PAUSED ", u[40] = "PLAY: on";
        num_cat(t, (uint32_t)bar + ui.s->first_bar_number);
        text_cat(t, ":");
        num_cat(t, (uint32_t)beat + 1u);
        text_cat(u, player_count_in_of() ? " (count-in, bar " : " (from here)");
        if (player_count_in_of()) {
            num_cat(u, (uint32_t)bar + ui.s->first_bar_number);
            text_cat(u, ")");
        }
        text_in(0, W_BIG, 8, &FONT_M, t, C_WHITE);
        text_in(0, W_BIG, 38, &FONT_XS, u, C_DIM);
        text_in(0, W_BIG, 54, &FONT_XS, "HOME: stop", C_DIM);
    } else if (st == PL_COUNT_IN && count > 0) {
        char t[4] = "";
        num_cat(t, (uint32_t)count);
        text_in(0, W_BIG, 2, &FONT_XS, "COUNT-IN", C_DIM);
        text2x((W_BIG - 2 * text_w(&FONT_L, t)) / 2, 12, &FONT_L, t, C_AMB);
    } else if (sym) {                    /* the cue (if any) on the left, the chord in the rest */
        int32_t x0 = cue & 15u ? W_CUE : 0, w = W_BIG - x0;
        if (2 * text_w(&FONT_L, sym) <= w - 4)
            text2x(x0 + (w - 2 * text_w(&FONT_L, sym)) / 2, 6, &FONT_L, sym, C_HI);
        else
            text_in(x0, w, 22, &FONT_L, sym, C_HI);
        if (cue & 15u) {
            char t[4] = "L";
            uint32_t l = part() == PART_BASS ? L_BASS : L_HARM;
            draw_cue(2, 4, cue & 15u, (cue >> 4) & 1u);
            num_cat(t, player_variant_of(l));
            cv_text(26, 56, &FONT_XS, t, C_DIM);
        }
    }
    cv_blit(0, Y_CHORD);
}

static void draw_next(int st, int32_t next, int32_t left)
{
    cv_begin(240 - W_BIG, H_CHORD, C_BLACK);
    cv_rect(0, 6, 1, H_CHORD - 12, C_LINE);
    cv_text(8, 4, &FONT_XS, st == PL_STOPPED ? "FIRST" : "NEXT", C_DIM);
    if (next >= 0)
        cv_text(8, 20, text_w(&FONT_M, chord_name((uint32_t)next)) <= 56 ? &FONT_M : &FONT_XS,
                chord_name((uint32_t)next), C_AMB);
    else if (st != PL_STOPPED)
        cv_text(8, 24, &FONT_XS, "END", C_DIM);
    if (left > 0) {                      /* the beats until the change, in the last bar before it */
        char t[8] = "IN ";
        num_cat(t, (uint32_t)left);
        cv_text(8, 48, &FONT_M, t, C_WHITE);
    }
    cv_blit(W_BIG, Y_CHORD);
}

/* bar index b as it will be heard: past a loop's end, the bars from the loop's start again */
static int32_t strip_bar(int32_t b)
{
    uint32_t from, to;
    if (player_state() != PL_STOPPED && player_loop_of(&from, &to) && to > from) {
        int32_t bt = (int32_t)bar_ticks(), fb = (int32_t)from / bt, tb = ((int32_t)to + bt - 1) / bt;
        if (b >= tb && tb > fb)
            b = fb + (b - tb) % (tb - fb);
    }
    return b < (int32_t)ui.s->bar_count ? b : -1;
}

static void draw_strip(int32_t bar)
{
    const struct song *s = ui.s;
    uint32_t i, from = 0, to = 0;
    if (ui.loop)
        loop_region(ui.loop, &from, &to);
    cv_begin(240, H_STRIP, C_BLACK);
    for (i = 0; i < 4u; i++) {
        int32_t b = strip_bar(bar + (int32_t)i), x = 2 + (int32_t)i * BOX_STEP;
        char t[8] = "";
        uint32_t k, k_end, t0, n = 0;
        if (b < 0) {
            frame_rect(x, 0, BOX_W, BOX_H, C_LINE);
            continue;
        }
        frame_rect(x, 0, BOX_W, BOX_H, i == 0 ? C_HI : C_DIM);
        num_cat(t, (uint32_t)b + s->first_bar_number);
        cv_text(x + 4, 1, &FONT_XS, t, C_DIM);
        t0 = (uint32_t)b * s->bar_ticks;
        if (ui.loop && t0 + s->bar_ticks > from && t0 < to)   /* the loop's bars: a line under them */
            cv_rect(x, BOX_H + 1, BOX_W + (t0 + s->bar_ticks < to && i < 3u ? BOX_STEP - BOX_W : 0), 3, C_AMB);
        if (ui.loop == LOOP_BARS && ((uint32_t)b == ui.a_bar || (uint32_t)b == ui.b_bar))
            cv_text(x + BOX_W - 12, 1, &FONT_XS, (uint32_t)b == ui.a_bar ? "A" : "B", C_HI);
        k = song_bar_chord(s, (uint32_t)b);
        k_end = k;                       /* the chord events inside the bar: k (sounding at beat 1) .. k_end */
        while (k_end + 1u < s->ncevt && song_cevt_tick(s, k_end + 1u) < t0 + s->bar_ticks)
            k_end++;
        n = k_end - k + 1u;
        for (; k <= k_end; k++) {
            uint32_t a = song_cevt_tick(s, k) > t0 ? song_cevt_tick(s, k) - t0 : 0;
            uint32_t z = k < k_end ? song_cevt_tick(s, k + 1u) - t0 : s->bar_ticks;
            int32_t sx = x + 1 + (int32_t)(a * (BOX_W - 2) / s->bar_ticks);
            int32_t sw = (int32_t)((z - a) * (BOX_W - 2) / s->bar_ticks);
            const char *sym = chord_name(k);
            const felucca_font_t *f = n == 1u && text_w(&FONT_M, sym) <= sw ? &FONT_M : &FONT_XS;
            if (a)
                cv_rect(sx, 16, 1, BOX_H - 20, C_DIM);
            text_in(sx, sw, n == 1u ? 18 : 22, f, sym, i == 0 ? C_WHITE : C_AMB);   /* (M2: no dimming) */
        }
    }
    cv_blit(0, Y_STRIP);
}

/* the keyboard view: the chord, its keys (the harmony level's voicing, in the key now) on a drawn keyboard C3..B5;
 * the next chord's keys outlined in the last bar before it; the bass note (bass muted) amber */
static void draw_keys_view(int st, int32_t k, int32_t next, int32_t left, uint32_t bass)
{
    uint8_t now[SONG_VOICING_NOTES], nx[SONG_VOICING_NOTES];
    uint32_t nn = k >= 0 ? voicing((uint32_t)k, now) : 0, nnx = next >= 0 && left > 0 ? voicing((uint32_t)next, nx) : 0;
    uint32_t n, i, wi = 0;
    int32_t x0 = 4, y0 = 32, wk = 11;
    char t[32] = "";
    cv_begin(240, Y_LAYERS - Y_CHORD, C_BLACK);
    if (k >= 0)
        cv_text(6, 0, &FONT_L, chord_name((uint32_t)k), C_HI);
    if (next >= 0) {
        text_cat(t, st == PL_STOPPED ? "first " : "next ");
        text_cat(t, chord_name((uint32_t)next));
        if (left > 0) {
            text_cat(t, "  IN ");
            num_cat(t, (uint32_t)left);
        }
        cv_text(234 - text_w(&FONT_B, t), 6, &FONT_B, t, C_AMB);
    }
    for (n = 48; n < 84u; n++) {         /* white keys first, then the black keys over them */
        uint32_t pc = n % 12u, lit = 0, dim = 0;
        if (pc == 1u || pc == 3u || pc == 6u || pc == 8u || pc == 10u)
            continue;
        for (i = 0; i < nn; i++)
            lit |= now[i] == n;
        for (i = 0; i < nnx; i++)
            dim |= nx[i] == n;
        cv_rect(x0 + (int32_t)wi * wk, y0, wk - 1, 76, lit ? C_HI : bass == n ? C_AMB : RGB(220, 220, 225));
        if (dim && !lit)
            frame_rect(x0 + (int32_t)wi * wk + 2, y0 + 56, wk - 5, 16, C_AMB);
        if (pc == 0u) {
            char o[4] = "C";
            num_cat(o, n / 12u - 1u);
            cv_text(x0 + (int32_t)wi * wk + 1, y0 + 62, &FONT_T, o, lit ? C_BLACK : C_DIM);
        }
        wi++;
    }
    for (n = 48, wi = 0; n < 84u; n++) {
        uint32_t pc = n % 12u, lit = 0, dim = 0;
        if (!(pc == 1u || pc == 3u || pc == 6u || pc == 8u || pc == 10u)) {
            wi++;
            continue;
        }
        for (i = 0; i < nn; i++)
            lit |= now[i] == n;
        for (i = 0; i < nnx; i++)
            dim |= nx[i] == n;
        cv_rect(x0 + (int32_t)wi * wk - 4, y0, 7, 46, lit ? C_HI : bass == n ? C_AMB : RGB(40, 40, 46));
        if (dim && !lit)
            frame_rect(x0 + (int32_t)wi * wk - 3, y0 + 30, 5, 12, C_AMB);
    }
    cv_text(6, y0 + 78, &FONT_XS, part() == PART_BASS ? "bass muted: its note amber" :
            player_variant_of(L_HARM) >= 2u ? "harmony L2: these keys (inversions)" : "harmony L1: these keys", C_DIM);
    cv_blit(0, Y_CHORD);
}

/* the chart view: the section's bars (16 a page) with their chords, the bar now framed bright */
static void draw_chart_view(int32_t bar, int32_t section)
{
    const struct song *s = ui.s;
    uint32_t first = song_section_first(s, (uint32_t)section), nb = song_section_bars(s, (uint32_t)section), i;
    uint32_t page = (uint32_t)bar >= first ? ((uint32_t)bar - first) / 16u : 0;
    char t[48] = "";
    cv_begin(240, Y_LAYERS - Y_CHORD, C_BLACK);
    text_cat(t, song_section_name(s, (uint32_t)section));
    text_cat(t, "  (bars ");
    num_cat(t, first + s->first_bar_number);
    text_cat(t, "-");
    num_cat(t, first + nb - 1u + s->first_bar_number);
    text_cat(t, ")");
    cv_text(6, 0, &FONT_XS, t, C_GRAY);
    for (i = 0; i < 16u && page * 16u + i < nb; i++) {
        uint32_t b = first + page * 16u + i, k = song_bar_chord(s, b), t0 = b * s->bar_ticks, n = 1;
        int32_t x = 2 + (int32_t)(i % 4u) * 60, y = 16 + (int32_t)(i / 4u) * 28;
        int cur = (int32_t)b == bar;
        char nbuf[8] = "", sym[24] = "";
        while (k + n < s->ncevt && song_cevt_tick(s, k + n) < t0 + s->bar_ticks)
            n++;                         /* the changes inside the bar: the first two named */
        frame_rect(x, y, 56, 26, cur ? C_HI : C_DIM);
        num_cat(nbuf, b + s->first_bar_number);
        cv_text(x + 2, y, &FONT_T, nbuf, C_DIM);
        text_cat(sym, chord_name(k));
        if (n > 1u) {
            text_cat(sym, " ");
            text_cat(sym, chord_name(k + 1u));
        }
        text_in(x + 6, 50, y + 6, text_w(&FONT_B, sym) <= 48 ? &FONT_B : &FONT_XS, sym, cur ? C_WHITE : C_AMB);
    }
    cv_blit(0, Y_CHORD);
}

static void draw_cursor(int32_t beat, int st)
{
    uint32_t i, bpb = ui.s->beats_per_bar;
    int32_t w = (BOX_W - 2) / (int32_t)bpb;
    cv_begin(240, H_CURSOR, C_BLACK);
    for (i = 0; i < bpb; i++)
        cv_rect(3 + (int32_t)i * w, 1, w - 2, 4, st == PL_PLAYING && (int32_t)i == beat ? C_HI : C_LINE);
    cv_blit(0, Y_CURSOR);
}

static void draw_layers(void)
{
    uint32_t l;
    cv_begin(240, H_LAYERS, C_BLACK);
    for (l = 0; l < SONG_LAYERS; l++) {
        int32_t x = 4 + (int32_t)l * 59;
        const struct song_layer *ly = &ui.s->layer[l];
        int on = !(ui.muted >> l & 1u);
        char t[8] = "L";
        if (!ly->variants) {
            frame_rect(x, 1, 55, H_LAYERS - 2, C_LINE);
            text_in(x, 55, 2, &FONT_XS, LAYER_SHORT[l], C_DIM);
            text_in(x, 55, 16, &FONT_XS, "-", C_DIM);
            continue;
        }
        if (on)
            cv_rect(x, 1, 55, H_LAYERS - 2, C_LINE);
        else
            frame_rect(x, 1, 55, H_LAYERS - 2, C_AMB);
        num_cat(t, player_variant_of(l) > ly->variants ? ly->variants : player_variant_of(l));
        text_in(x, 55, 2, &FONT_XS, LAYER_SHORT[l], on ? C_WHITE : C_AMB);
        text_in(x, 55, 16, &FONT_XS, on ? t : "MUTE", on ? C_HI : C_AMB);
    }
    cv_blit(0, Y_LAYERS);
}

static void draw_status(int32_t section, uint32_t bass_now, uint32_t bass_next)
{
    char t[40] = "";
    cv_begin(240, H_STATUS, C_BLACK);
    if ((ui.muted >> L_BASS & 1u) && bass_now != 0xFFu) {   /* the bass muted: its note now, and the next */
        char b[32] = "BASS ";
        note_cat(b, bass_now);
        if (bass_next != 0xFFu) {
            text_cat(b, "  then ");
            note_cat(b, bass_next);
        }
        cv_text(6, 3, &FONT_B, b, C_HI);
    } else if (section >= 0) {
        cv_text(6, 4, &FONT_XS, song_section_name(ui.s, (uint32_t)section), C_GRAY);
    }
    if (ui.loop == LOOP_BARS) {
        text_cat(t, "LOOP BARS ");
        num_cat(t, ui.a_bar + ui.s->first_bar_number);
        text_cat(t, "-");
        num_cat(t, ui.b_bar + ui.s->first_bar_number);
    } else if (ui.loop) {
        text_cat(t, LOOP_NAME[ui.loop]);
    } else if (ui.from_bar != first_bar()) {
        text_cat(t, "START BAR ");
        num_cat(t, ui.from_bar + ui.s->first_bar_number);
    }
    cv_text(234 - text_w(&FONT_XS, t), 4, &FONT_XS, t, C_HI);
    cv_blit(0, Y_STATUS);
}

static const char *legend_play(int st)
{
    if (ui.keymode_until)
        return "SELECT: KEY    SAVE: DONE";
    if (st == PL_PLAYING || st == PL_COUNT_IN)
        return "PLAY: PAUSE  EDIT: VIEW  SAVE: KEY  GLO: ?";
    if (ui.loop == LOOP_BARS && st == PL_STOPPED)
        return "PLAY  PRESETS: A  ALGO: B  SEQ  GLO: ?";
    if (st == PL_STOPPED)
        return "PLAY  PRESETS/ALGO: START  SEQ: LOOP  GLO: ?";
    if (st == PL_PAUSED)
        return "PLAY: GO ON   HOME: STOP   GLO: ?";
    return "PLAY: PAUSE   HOME: STOP   SEQ: LOOP   GLO: ?";
}

static void frame_play(uint32_t now)
{
    const struct song *s = ui.s;
    int st = player_state();
    int32_t tc = player_tick(PL_LEAD_CHORD_MS), tb = player_tick(PL_LEAD_BEAT_MS), from = player_play_from();
    int32_t k = -1, next = -1, left = 0, count = 0, bar = 0, beat = 0, section;
    uint32_t lfrom, lto, top_key, status_key, cue = 0, bass_now = 0xFFu, bass_next = 0xFFu, view_key;
    int looping = player_loop_of(&lfrom, &lto);
    const char *leg;
    if (st == PL_STOPPED) {
        int32_t sp = start_point();
        next = (int32_t)song_chord_at(s, (uint32_t)(sp > (int32_t)s->start_tick ? sp : (int32_t)s->start_tick));
        if (song_cevt_symbol(s, (uint32_t)next)[0] == 'N' && (uint32_t)next + 1u < s->ncevt)
            next++;                      /* N.C. in the pickup: the first real chord */
        tb = sp;
    } else if (st == PL_PAUSED) {
        k = (int32_t)song_chord_at(s, tb > 0 ? (uint32_t)tb : 0);
        next = (uint32_t)k + 1u < s->ncevt ? k + 1 : -1;
    } else {
        if (tc >= from || st == PL_PLAYING) {
            uint32_t T;
            k = (int32_t)song_chord_at(s, tc > 0 ? (uint32_t)tc : 0);
            if ((uint32_t)k + 1u < s->ncevt) {
                next = k + 1;
                T = song_cevt_tick(s, (uint32_t)next);
            } else {
                T = s->end_tick;
            }
            if (looping && T >= lto) {    /* the next change is the loop's jump */
                next = (int32_t)song_chord_at(s, lfrom);
                T = lto;
                if (next == k)
                    next = -1;
            }
            if (next >= 0 && (int32_t)T > tb && (int32_t)T - tb <= (int32_t)s->bar_ticks)
                left = ((int32_t)T - tb + (int32_t)s->ppq - 1) / (int32_t)s->ppq;
        } else {                         /* the count-in: beats left, the first chord next */
            count = (from - tc + (int32_t)s->ppq - 1) / (int32_t)s->ppq;
            next = (int32_t)song_chord_at(s, (uint32_t)from);
            if (song_cevt_symbol(s, (uint32_t)next)[0] == 'N' && (uint32_t)next + 1u < s->ncevt)
                next++;                  /* N.C. in the pickup: the first real chord */
        }
    }
    if (tb >= 0) {
        bar = tb / (int32_t)s->bar_ticks;
        beat = tb % (int32_t)s->bar_ticks / (int32_t)s->ppq;
    }
    if (st == PL_COUNT_IN && tb < from)
        bar = from / (int32_t)s->bar_ticks;   /* the strip waits on the first bar */
    if (st == PL_STOPPED)
        bar = start_point() / (int32_t)s->bar_ticks;
    section = (int32_t)section_at(st == PL_STOPPED ? start_point() : tb);
    {   /* the part being played: the cue, the bass notes, the key lights */
        int bright = 0;
        uint8_t v[SONG_VOICING_NOTES];
        uint32_t i, nv, kc = st == PL_STOPPED || k < 0 ? (uint32_t)(next >= 0 ? next : 0) : (uint32_t)k;
        cue = (st == PL_PLAYING && k >= 0 && part() != PART_NONE) ? cue_at(tb, &bright) : 0;
        cue |= (uint32_t)bright << 4;
        bass_now = bass_next = 0xFFu;
        if (ui.muted >> L_BASS & 1u)
            bass_notes(tb, &bass_now, &bass_next);
        if (bass_now != 0xFFu)
            bass_now = moved(bass_now);
        if (bass_next != 0xFFu)
            bass_next = moved(bass_next);
        ui.lit_keys = ui.dim_keys = 0;
        if (part() == PART_HARMONY && st != PL_STOPPED) {
            nv = voicing(st == PL_COUNT_IN && k < 0 ? (uint32_t)(next >= 0 ? next : 0) : kc, v);
            for (i = 0; i < nv; i++)
                ui.lit_keys |= key_bit(v[i]);
            if (next >= 0 && left > 0)
                for (i = 0, nv = voicing((uint32_t)next, v); i < nv; i++)
                    ui.dim_keys |= key_bit(v[i]);
        } else if (part() == PART_BASS && st != PL_STOPPED && bass_now != 0xFFu) {
            ui.lit_keys = key_bit(bass_now);
        }
    }
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.sh_top_key = 0xFFFFFFFFu;
    }
    if (ui.pview != PV_CHORD) {          /* the keyboard and chart views: the chord band, strip and cursor's place */
        view_key = (uint32_t)(k + 1) ^ (uint32_t)(next + 1) << 11 ^ (uint32_t)left << 22 ^ bass_now << 25 ^
                   (uint32_t)st << 29 ^ (uint32_t)(ui.pview == PV_CHART ? bar : 0) << 3;
        if (ui.redraw || view_key != ui.sh_view) {
            if (ui.pview == PV_KEYS)
                draw_keys_view(st, k, next, left, ui.muted >> L_BASS & 1u ? bass_now : 0xFFu);
            else
                draw_chart_view(bar, section);
            lcd_done();
            if (k >= 0 && k < (int32_t)PL_STATS_CHORDS && (st == PL_PLAYING || st == PL_COUNT_IN) &&
                !player_stats.chord_drawn[k])
                player_stats.chord_drawn[k] = player_frames();
            ui.sh_view = view_key;
        }
        ui.sh_chord = k;
    } else {
    /* the chord first: it is what must not be late (player.h PL_LEAD_CHORD_MS; tools/m2_test.py checks) */
    if (ui.redraw || st != ui.sh_state || k != ui.sh_chord || count != ui.sh_count || cue != ui.sh_cue ||
        (st != PL_PLAYING && st != PL_COUNT_IN && ui.sh_status == 0xFFFFFFFFu)) {
        draw_big(st, k, count, bar, beat, cue);
        ui.sh_cue = cue;
        lcd_done();
        if (k >= 0 && k < (int32_t)PL_STATS_CHORDS && (st == PL_PLAYING || st == PL_COUNT_IN) &&
            !player_stats.chord_drawn[k])
            player_stats.chord_drawn[k] = player_frames();
        ui.sh_chord = k;
        ui.sh_count = count;
    }
    if (ui.redraw || st != ui.sh_state || next != ui.sh_next || left != ui.sh_left) {
        draw_next(st, next, left);
        ui.sh_next = next;
        ui.sh_left = left;
    }
    }
    top_key = (uint32_t)(bar * 16 + beat) ^ player_tempo_of() << 16 ^ (uint32_t)ui.loop << 25 ^
              (uint32_t)player_count_in_of() << 27 ^ (uint32_t)st << 28 ^ (uint32_t)(ui.key + 8) << 10 ^
              (ui.keymode_until ? 1u << 31 : 0u);
    if (ui.redraw || top_key != ui.sh_top_key) {
        draw_top(bar, beat, st);
        ui.sh_top_key = top_key;
    }
    if (ui.pview == PV_CHORD && (ui.redraw || bar != ui.sh_strip || (int32_t)(looping ? lto : 0) != ui.sh_bar)) {
        draw_strip(bar);
        ui.sh_strip = bar;
        ui.sh_bar = (int32_t)(looping ? lto : 0);
    }
    if (ui.pview == PV_CHORD && (ui.redraw || beat != ui.sh_beat || st != ui.sh_state)) {
        draw_cursor(beat, st);
        ui.sh_beat = beat;
    }
    if (ui.redraw || (int32_t)(ui.muted | player_variant_of(0) << 4 | player_variant_of(1) << 6 |
                               player_variant_of(2) << 8 | player_variant_of(3) << 10) != ui.sh_layers) {
        draw_layers();
        ui.sh_layers = (int32_t)(ui.muted | player_variant_of(0) << 4 | player_variant_of(1) << 6 |
                                 player_variant_of(2) << 8 | player_variant_of(3) << 10);
    }
    status_key = (uint32_t)section ^ (uint32_t)ui.loop << 8 ^ ui.from_bar << 10 ^ ui.a_bar << 20 ^ ui.b_bar << 26 ^
                 bass_now << 4 ^ bass_next << 13;
    if (ui.redraw || status_key != ui.sh_status) {
        draw_status(section, bass_now, bass_next);
        ui.sh_status = status_key;
    }
    leg = legend_play(st);
    if (ui.redraw || (uint32_t)(uintptr_t)leg != ui.sh_legend) {
        draw_legend(Y_LEGEND, leg);
        ui.sh_legend = (uint32_t)(uintptr_t)leg;
    }
    ui.sh_state = st;
}

/* a message over the whole screen, drawn now (the main loop is busy: a song arriving over USB) */
static void ui_busy(const char *a, const char *b)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    cv_begin(240, 80, C_BLACK);
    text_in(0, 240, 14, &FONT_B, a, C_WHITE);
    text_in(0, 240, 46, &FONT_XS, b, C_GRAY);
    cv_blit(0, 80);
    lcd_done();
    ui.view = V_LIST;
    ui.redraw = 1;
}

void app_ui_init(void)
{
    ui.flash_ok = plat_data(PLAT_DATA_LO) != 0;   /* 0: the platform refused the chip (fm1-base main_fm1.c) */
    ui.splash = plat_ms() < SPLASH_MS;   /* (after a restore or a resource install: no splash, straight back) */
    if (!ui.splash)
        lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
    ui.sh_list = 0xFFFFFFFFu;
    player_stats.key_note_last = 0xFFu;
    player_stats.ui_view = 16u;
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    if (ui.splash && now >= SPLASH_MS) { /* the splash has had its time: the list */
        ui.splash = 0;
        ui.redraw = 1;
    }
    if (ui.lib_seen != lib_changes) {     /* songs sent or deleted: back to the list, from the top */
        ui.lib_seen = lib_changes;
        ui.view = V_LIST;
        ui.sel = ui.top = 0;
        ui.redraw = 1;
    }
    input();
    ann_tick(now);
    player_stats.ui_view = (uint32_t)ui.view + (ui.splash ? 16u : 0u);
    if (ui.splash)
        return;
    if (ui.view == V_HELP) {
        if (ui.redraw || now - ui.sh_stats_ms >= 500u) {
            draw_help();
            ui.sh_stats_ms = now;
        }
        ui.redraw = 0;
        return;
    }
    if (ui.view == V_LIST) {
        uint32_t key = ui.sel | (now < ui.msg_until) << 16;
        if (ui.redraw || key != ui.sh_list) {
            if (ui.redraw)
                lcd_fill(0, 0, 240, 240, C_BLACK);
            draw_list();
            ui.sh_list = key;
        }
        ui.redraw = 0;
        return;
    }
    if (ui.redraw) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.sh_status = 0xFFFFFFFEu;
    }
    frame_play(now);
    ui.redraw = 0;
}
