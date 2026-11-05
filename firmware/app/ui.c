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
#include "guitar.h"

static void ui_busy(const char *a, const char *b);
#include "library.c"

#define KEY_LOW 53                       /* key 0 is F3 (plat.h) */
#define VELOCITY 100u
#define OCT_MAX 3
#define LIST_ROWS 5u
#define LIST_ROW_H 36
#define SPLASH_MS 3000u                  /* the splash stays at least this long after power-on */
#define FLASH_MS 110u                    /* the beat light's flash on beats 2.. (M3: full brightness, every beat) */
#define FLASH_BAR_MS 220u                /* and on beat 1: longer */
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
#define Y_INV 160                        /* M3: the inversion's line (the section's row is gone: the user's note) */
#define H_INV 16
#define H_VIEW 144                       /* the views' area, Y_CHORD.. (the canvas holds 144 rows) */
#define Y_LAYERS 180
#define H_LAYERS 32
#define Y_LEGEND 214
#define H_LEGEND 26
#define BOX_W 56
#define BOX_H 44
#define BOX_STEP 60

enum { V_LIST, V_PLAY, V_HELP, V_SETTINGS };
/* the play screen's views (EDIT); PV_CHORD: the part's main view (the chord, or your part's note or hit) */
enum { PV_CHORD, PV_KEYS, PV_CHART, PV_GUITAR, PV_TAB, PV_N };
static const uint8_t VIEWS[SONG_LAYERS][8] = {   /* each part's views in EDIT's order (PV_N ends a list) */
    {PV_CHORD, PV_CHART, PV_N},                                   /* drums */
    {PV_CHORD, PV_KEYS, PV_CHART, PV_TAB, PV_N},                  /* bass */
    {PV_CHORD, PV_KEYS, PV_CHART, PV_GUITAR, PV_N},               /* harmony */
    {PV_CHORD, PV_KEYS, PV_CHART, PV_N},                          /* melody */
};
static const char *const VIEW_NAME[PV_N] = {"MAIN", "KEYBOARD", "CHART", "GUITAR", "BASS TAB"};
#define L_DRUMS 0u
#define L_BASS 1u
#define L_HARM 2u
#define L_MELODY 3u
#define DARK_AMB RGB(150, 88, 20)        /* your part's keys between attacks (the keyboard view) */
#define LOOP_BG RGB(64, 38, 8)           /* the loop's bars, filled (strip, chart) */

static const uint8_t LAYER_BUTTON[SONG_LAYERS] = {B_FX, B_SEL, B_ENV, B_LFO};
static const uint8_t LAYER_ENC[SONG_LAYERS] = {EN_K1, EN_K2, EN_K3, EN_K4};
static const char *const LAYER_NAME[SONG_LAYERS] = {"DRUMS", "BASS", "HARMONY", "MELODY"};
static const char *const LAYER_SHORT[SONG_LAYERS] = {"DRUMS", "BASS", "HARM", "MELODY"};

/* the settings (M3 part 4): kept in fm1-base's settings store (plat_store, OBJ_SETTINGS: 0xFC000 / 0xFD000, two
 * copies, the newer valid one wins; the stock firmware never uses these sectors) */
#define SET_MAGIC 0x31534D46u            /* "FMS1" */
enum { S_BLINK, S_COUNT_BARS, S_COUNT_VOL, S_NOTE_NAMES, S_N };
struct settings {
    uint32_t magic, version;
    uint8_t v[8];                        /* S_* values (the rest 0) */
};
static const char *const SET_NAME[S_N] = {"Key lights blink", "Count-in length", "Count-in volume",
                                          "Note names (notation)"};
static const uint8_t SET_COUNT[S_N] = {2, 2, 3, 2};   /* how many values each has */
static const char *const SET_VAL[S_N][3] = {{"OFF", "ON", ""}, {"1 BAR", "2 BARS", ""}, {"LOW", "MEDIUM", "HIGH"},
                                            {"OFF", "ON", ""}};
static const uint8_t SET_DEFAULT[S_N] = {1, 0, 2, 1};
static const uint8_t COUNT_VOL[3] = {64, 96, 127};   /* the beep's channel volume */

static struct {
    int view, back;                      /* V_*; back: the view the help returns to */
    struct settings set;
    uint32_t set_row, set_dirty;
    uint32_t sel, top;                   /* the list's cursor and first row */
    const struct song *s;
    uint32_t keys, buttons;
    uint8_t key_note[NKEYS];             /* the note each held key sent (the octave may change while it is held) */
    int32_t oct;                         /* the keys' octave, -OCT_MAX..OCT_MAX */
    uint32_t muted;
    int loop;
    uint32_t from_bar;                   /* the start point: a bar index; from_tick its first tick of music */
    int32_t from_tick;
    uint32_t a_bar, b_bar;               /* the loop, bars A..B (M3: the one kind of loop) */
    int loop_step;                       /* setting the loop: 1 moving A, 2 moving B (SEQ confirms each); 0 not */
    uint32_t tempo_ms;                   /* the last tempo click (quick turns: bigger steps) */
    int32_t key;                         /* the transposition, -5..+6 semitones (the key: the nearest way there) */
    uint32_t keymode_until;              /* the KEY mode: SELECT changes the key until then (0: off) */
    int pview;                           /* PV_*: the view now; pv_of[l]: the last one used for each part */
    int32_t sc_x0, sc_span;              /* the scrolling view's now line and ticks ahead (scroll_setup) */
    uint8_t pv_of[SONG_LAYERS];
    uint32_t mute_seq[SONG_LAYERS], seq;  /* when each layer was last muted: your part is the latest still muted */
    uint32_t kb_lo;                      /* the keyboard view's lowest key (it follows the notes) */
    uint32_t lit_keys, mid_keys, dim_keys;   /* the key lights for your part (frame_play sets them): at an attack,
                                              * between attacks, the next chord */
    char msg[2][24];
    uint32_t msg_until;
    uint8_t flash_ok, splash, redraw, flashing, flash_keys, blink;
    uint32_t lib_seen;                   /* library.c lib_changes when the screen was last set up */
    /* what is on the screen */
    int32_t sh_chord, sh_count, sh_next, sh_left, sh_bar, sh_beat, sh_strip, sh_layers, sh_section;
    int sh_state;
    uint32_t sh_top_key, sh_stats_ms, sh_list, sh_status, sh_legend, sh_cue, sh_view, sh_part;
} ui;

/* ------------------------------------------------------------------------ settings --- */
static void settings_apply(void)
{
    ui.blink = ui.set.v[S_BLINK] != 0;
    player_count_in_bars(ui.set.v[S_COUNT_BARS] ? 2u : 1u);
    player_count_in_volume(COUNT_VOL[ui.set.v[S_COUNT_VOL] % 3u]);
    player_stats.settings = ui.set.v[0] | (uint32_t)ui.set.v[1] << 4 | (uint32_t)ui.set.v[2] << 8 |
                            (uint32_t)ui.set.v[3] << 12 | (uint32_t)ui.set.v[4] << 16;
}

static void settings_load(void)
{
    uint32_t i;
    if (plat_store_load(OBJ_SETTINGS, &ui.set, sizeof ui.set) != (int)sizeof ui.set || ui.set.magic != SET_MAGIC) {
        ui.set.magic = SET_MAGIC;        /* none yet (or not ours): the defaults */
        ui.set.version = 1;
        for (i = 0; i < 8u; i++)
            ui.set.v[i] = i < S_N ? SET_DEFAULT[i] : 0;
    }
    for (i = 0; i < S_N; i++)            /* (a value out of range: the default) */
        if (ui.set.v[i] >= SET_COUNT[i])
            ui.set.v[i] = SET_DEFAULT[i];
    settings_apply();
}

static void settings_save(void)
{
    if (!ui.set_dirty)
        return;
    ui.set_dirty = 0;
    if (ui.flash_ok && plat_store_save(OBJ_SETTINGS, &ui.set, sizeof ui.set) == 0)
        player_stats.settings_saves++;
}

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

static uint32_t text_len(const char *s)
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
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
static void inversion_cat(char *d, uint32_t k);

/* a note's name with its octave (C4 = 60), in the key now's spelling (n: as sounded) */
static void note_cat(char *d, uint32_t n)
{
    text_cat(d, names()[n % 12u]);
    num_cat(d, n / 12u - 1u);
}

/* --------------------------------------------------- your part (M3 part 2: "follow your part", all four layers) --- */
/* your part is the muted layer: the screen, the rhythm cue, the key lights and the views follow it. Several muted: the
 * one muted last (unmute it: the one muted before); none: the harmony is shown (its cue too), the keys stay dark */
static uint32_t yours(void)
{
    uint32_t l, best = L_HARM, seq = 0;
    for (l = 0; l < SONG_LAYERS; l++)
        if ((ui.muted >> l & 1u) && ui.mute_seq[l] >= seq) {
            best = l;
            seq = ui.mute_seq[l];
        }
    return best;
}

static int playing_yours(void) { return (ui.muted >> yours() & 1u) != 0; }

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

/* the notes of the layer's last attack at or before tick (dir 0), or of its next attack after tick (dir 1): up to max,
 * as in the file (not moved), in the file's order; *at the attack's tick. 0: none (within 64 events) */
static uint32_t attack_notes(uint32_t l, int32_t tick, int dir, uint8_t *out, uint32_t max, uint32_t *at)
{
    const struct song_stream *st = layer_stream(l);
    struct song_event e;
    uint32_t i, k, n = 0, a = 0, found = 0;
    if (!st || !st->count || tick < 0)
        return 0;
    i = song_seek(st, (uint32_t)tick + 1u);   /* the first event after tick */
    if (dir) {
        for (k = i; k < st->count && k < i + 64u; k++) {
            song_event_get(st, k, &e);
            if (e.status != 0x90u)
                continue;
            if (found && e.tick != a)
                break;
            a = e.tick;
            found = 1;
            if (n < max)
                out[n++] = e.d1;
        }
    } else {
        for (k = i; k > 0 && i - k < 64u; k--) {
            song_event_get(st, k - 1u, &e);
            if (e.status != 0x90u)
                continue;
            if (found && e.tick != a)
                break;
            a = e.tick;
            found = 1;
            if (n < max)
                out[n++] = e.d1;
        }
    }
    *at = a;
    return n;
}

/* the lowest (bass) or highest (melody) of an attack's notes */
static uint32_t pick_note(uint32_t l, const uint8_t *v, uint32_t n)
{
    uint32_t i, b = v[0];
    for (i = 1; i < n; i++)
        if (l == L_MELODY ? v[i] > b : v[i] < b)
            b = v[i];
    return b;
}

/* a drum's name (GM percussion), and its row in the drum views: 0 kick, 1 snare, 2 hi-hat / cymbals, 3 the others */
static const char *drum_name(uint32_t n)
{
    switch (n) {
    case 35: case 36: return "KICK";
    case 37: return "RIM";
    case 38: case 40: return "SNARE";
    case 39: return "CLAP";
    case 42: case 44: return "HAT";
    case 46: return "OPEN HAT";
    case 49: case 52: case 55: case 57: return "CRASH";
    case 51: case 53: case 59: return "RIDE";
    case 41: case 43: case 45: case 47: case 48: case 50: return "TOM";
    default: return "PERC";
    }
}

static uint32_t drum_row(uint32_t n)
{
    return n == 35u || n == 36u ? 0u : (n >= 37u && n <= 40u) ? 1u :
           n == 42u || n == 44u || n == 46u || n == 49u || n == 51u || n == 52u || n == 53u || n == 55u || n == 57u ||
           n == 59u ? 2u : 3u;
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

/* the loop's ticks: bar A's beat 1 to the end of bar B (no pickup: a loop starts on a bar line). M3: one kind of loop,
 * bars A..B (M2's section and whole-song loops are gone: a section, or the whole song, is A..B set with PRESETS) */
static void loop_region(uint32_t *from, uint32_t *to)
{
    *from = ui.a_bar * bar_ticks();
    *to = (ui.b_bar + 1u) * bar_ticks();
}

static void set_loop(int on)
{
    uint32_t from = 0, to = 0;
    ui.loop = on ? LOOP_BARS : LOOP_OFF;
    loop_region(&from, &to);
    player_loop(ui.loop, from, to);
    ui.sh_strip = -1;
    ui.sh_view = 0;
}

/* where PLAY starts from stopped: the loop's start, or the start point */
static int32_t start_point(void)
{
    if (ui.loop == LOOP_OFF)
        return ui.from_tick;
    return (int32_t)(ui.a_bar * bar_ticks());
}

/* the bars highlighted as the loop (also while it is being set: A alone, then A..B); 0: none */
static int loop_shown(uint32_t *a, uint32_t *b)
{
    *a = ui.a_bar;
    *b = ui.loop_step == 1 ? ui.a_bar : ui.b_bar;
    return ui.loop_step || ui.loop == LOOP_BARS;
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
    ui.mute_seq[0] = ui.mute_seq[1] = ui.mute_seq[2] = ui.mute_seq[3] = 0;
    player_load(ui.s);
    player_mute(0);
    for (r = 0; r < (int)SONG_LAYERS; r++)
        player_variant((uint32_t)r, 1);
    player_tempo(player_song_bpm(s));    /* every song opens at its own tempo, in its own key */
    ui.key = 0;
    ui.kb_lo = 0;
    ui.keymode_until = 0;
    player_transpose(0);
    set_from_bar(first_bar());
    ui.from_tick = (int32_t)section_from(0);
    set_loop(0);
    ui.loop_step = 0;
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
            if (ms < (beat1 ? FLASH_BAR_MS : FLASH_MS))   /* (M2's mid level for beats 2.. was not visible on the
                                                             * unit: the user saw one flash a bar) */
                lit |= LED_PLAY_GREEN;
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
        ui.lit_keys = ui.mid_keys = ui.dim_keys = 0;
    player_stats.lit_keys = ui.lit_keys;
    player_stats.dim_keys = ui.dim_keys;
    player_stats.mid_keys = ui.mid_keys;
    player_stats.loop_step = (uint32_t)ui.loop_step;
    plat_lights(lit, ui.keys | ui.lit_keys, dim, ui.dim_keys & ~(ui.lit_keys | ui.mid_keys), 1);   /* keys pressed:
                                                                                                 * lit as ever */
    plat_lights_mid(mid, ui.mid_keys & ~(ui.keys | ui.lit_keys));
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
    app_say("START", t);
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
    if ((down >> B_HOME & 1u) && ui.loop_step) {   /* setting the loop: B back to A, A: cancelled */
        ui.loop_step--;
        app_say("LOOP", ui.loop_step ? "SET A" : "NOT SET");
        ui.sh_status = 0xFFFFFFFFu;
        ui.sh_strip = -1;
        down &= ~(1u << B_HOME);
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
            uint32_t was = yours();
            ui.muted ^= 1u << l;
            ui.mute_seq[l] = ++ui.seq;
            player_mute(ui.muted);
            if (yours() != was) {        /* the view follows: the last one used for that part */
                ui.pv_of[was] = (uint8_t)ui.pview;
                ui.pview = ui.pv_of[yours()];
                player_stats.view = (uint32_t)ui.pview;
                ui.redraw = 1;
            }
            app_say(LAYER_NAME[l], ui.muted >> l & 1u ? "MUTED: YOURS" : "ON");
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
    if (down >> B_EDIT & 1u) {           /* the next of your part's views (VIEWS) */
        const uint8_t *v = VIEWS[yours()];
        uint32_t i = 0;
        while (v[i] != PV_N && v[i] != ui.pview)
            i++;
        ui.pview = v[i] == PV_N || v[i + 1u] == PV_N ? v[0] : v[i + 1u];
        ui.pv_of[yours()] = (uint8_t)ui.pview;
        player_stats.view = (uint32_t)ui.pview;
        ui.redraw = 1;
        app_say("VIEW", VIEW_NAME[ui.pview]);
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
    if (d && ui.loop_step) {             /* setting the loop: A to a section's first bar, B to a section's last */
        uint32_t *m = ui.loop_step == 1 ? &ui.a_bar : &ui.b_bar, i = song_bar_section(ui.s, *m), lo, hi;
        if (ui.loop_step == 1) {
            if (d < 0 && *m == song_section_first(ui.s, i) && i > 0)
                i--;
            else if (d > 0 && i + 1u < ui.s->nsect)
                i++;
            *m = song_section_first(ui.s, i);
        } else {
            lo = song_section_first(ui.s, i) + song_section_bars(ui.s, i) - 1u;
            if (d > 0 && *m == lo && i + 1u < ui.s->nsect)
                i++;
            else if (d < 0 && i > 0)
                i--;
            hi = song_section_first(ui.s, i) + song_section_bars(ui.s, i) - 1u;
            *m = hi < ui.a_bar ? ui.a_bar : hi;
        }
        *m = (uint32_t)clamp((int32_t)*m, (int32_t)first_bar(), (int32_t)last_bar());
        ui.sh_status = ui.sh_strip = 0xFFFFFFFFu;
        d = 0;
    }
    if (d && ui.loop == LOOP_OFF) {      /* the start point, by section */
        int32_t i = clamp((int32_t)section_at(ui.from_tick) + d, 0, (int32_t)ui.s->nsect - 1);
        set_from_bar(song_section_first(ui.s, (uint32_t)i));
        app_say("START", song_section_name(ui.s, (uint32_t)i));
        ui.sh_status = 0xFFFFFFFFu;
    } else if (d) {
        app_say("LOOP ON", "SEQ: OFF FIRST");
    }
    d = plat_enc(EN_ALGO);
    if (d && ui.loop_step) {             /* setting the loop: A or B, by bar (B at or after A) */
        if (ui.loop_step == 1)
            ui.a_bar = (uint32_t)clamp((int32_t)ui.a_bar + d, (int32_t)first_bar(), (int32_t)last_bar());
        else
            ui.b_bar = (uint32_t)clamp((int32_t)ui.b_bar + d, (int32_t)ui.a_bar, (int32_t)last_bar());
        ui.sh_status = ui.sh_strip = 0xFFFFFFFFu;
        d = 0;
    }
    if (d && ui.loop == LOOP_OFF) {      /* the start point, by bar */
        set_from_bar((uint32_t)clamp((int32_t)ui.from_bar + d, (int32_t)first_bar(), (int32_t)last_bar()));
        say_from();
        ui.sh_status = 0xFFFFFFFFu;
    } else if (d) {
        app_say("LOOP ON", "SEQ: OFF FIRST");
    }
    if (down >> B_ARP & 1u) {
        player_count_in(!player_count_in_of());
        app_say("COUNT-IN", player_count_in_of() ? "ON" : "OFF");
    }
    if (down >> B_SEQ & 1u) {            /* the loop, step by step: SEQ sets A, SEQ sets B (on); SEQ again: off */
        char t[24] = "";
        if (ui.loop == LOOP_BARS) {
            set_loop(0);
            app_say("LOOP", "OFF");
        } else if (ui.loop_step == 0) {  /* A starts at the bar playing (or the start point); the start point stays */
            ui.a_bar = st == PL_PLAYING || st == PL_PAUSED ? bar_at(player_tick(PL_LEAD_BEAT_MS)) : ui.from_bar;
            ui.loop_step = 1;
            app_say("LOOP", "SET A");
        } else if (ui.loop_step == 1) {  /* A set: B starts 4 bars on */
            ui.b_bar = ui.a_bar + 3u > last_bar() ? last_bar() : ui.a_bar + 3u;
            ui.loop_step = 2;
            text_cat(t, "A = BAR ");
            num_cat(t, ui.a_bar + ui.s->first_bar_number);
            app_say(t, "SET B");
        } else {                         /* B set: the loop is on */
            ui.loop_step = 0;
            set_loop(1);
            text_cat(t, "BARS ");
            num_cat(t, ui.a_bar + ui.s->first_bar_number);
            text_cat(t, "-");
            num_cat(t, ui.b_bar + ui.s->first_bar_number);
            app_say("LOOP ON", t);
        }
        ui.sh_status = ui.sh_strip = 0xFFFFFFFFu;
        ui.sh_legend = 0;
        ui.sh_top_key = 0xFFFFFFFFu;
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
    if (ui.view == V_HELP) {             /* GLO again: the settings; any other button closes the help */
        if (down >> B_GLO & 1u) {
            ui.view = V_SETTINGS;
            ui.redraw = 1;
        } else if (down & ~(1u << B_OCTDN | 1u << B_OCTUP)) {
            ui.view = ui.back;
            ui.redraw = 1;
        }
        lights();
        return;
    }
    if (ui.view == V_SETTINGS) {         /* SELECT the row; ALGORITHM, PLAY or EDIT its value; GLO or HOME: back */
        int32_t d = plat_enc(EN_SELECT), v = plat_enc(EN_ALGO);
        if (d) {
            ui.set_row = (uint32_t)clamp((int32_t)ui.set_row + d, 0, (int32_t)S_N - 1);
            ui.redraw = 1;
        }
        if (down & (1u << B_PLAY | 1u << B_EDIT))
            v = 1;
        if (v) {
            uint32_t r = ui.set_row, n = SET_COUNT[r];
            ui.set.v[r] = (uint8_t)(((int32_t)ui.set.v[r] + v % (int32_t)n + (int32_t)n) % (int32_t)n);
            ui.set_dirty = 1;
            settings_apply();
            ui.redraw = 1;
        }
        if (down & (1u << B_GLO | 1u << B_HOME)) {
            settings_save();
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
/* the legend: "NAME action|NAME action|..."; M3: the button names amber, the actions gray, a divider between pairs
 * (the user's note); centred; too wide: the smaller font */
static void draw_legend(int32_t y, const char *t)
{
    const felucca_font_t *f = &FONT_XS;
    char seg[8][40];
    uint32_t n = 0, i, k;
    int32_t w = 0, x, gap = 9;
    const char *p = t;
    while (*p && n < 8u) {               /* split at '|' */
        for (k = 0; *p && *p != '|' && k < 39u; p++)
            seg[n][k++] = *p;
        seg[n++][k] = 0;
        if (*p == '|')
            p++;
    }
    for (i = 0; i < n; i++)
        w += text_w(f, seg[i]) + (i ? gap : 0);
    if (w > 236) {
        f = &FONT_T;
        gap = 7;
        for (i = 0, w = 0; i < n; i++)
            w += text_w(f, seg[i]) + (i ? gap : 0);
    }
    cv_begin(240, H_LEGEND, C_BLACK);
    cv_rect(0, 1, 240, 1, C_LINE);
    x = (240 - w) / 2;
    for (i = 0; i < n; i++) {
        char name[16];
        for (k = 0; seg[i][k] && seg[i][k] != ' ' && k < 15u; k++)
            name[k] = seg[i][k];
        name[k] = 0;
        if (i)
            cv_rect(x - gap / 2 - 1, 7, 1, 14, C_DIM);
        cv_text(x, 7, f, name, C_HI);
        if (seg[i][k])
            cv_text(x + text_w(f, name), 7, f, seg[i] + k, C_GRAY);
        x += text_w(f, seg[i]) + gap;
    }
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
        text_in(0, 240, 98, &FONT_XS, "tools/fm1song.sh send songs/*/", C_HI);
        text_in(0, 240, 130, &FONT_XS, "They stay here when it is off.", C_DIM);
    }
    cv_blit(0, H_TOP);
    draw_legend(Y_LEGEND, "GLO help|keys play along");
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
    draw_legend(Y_LEGEND, "SELECT choose|PLAY open|GLO help");
}

/* -------------------------------------------------------------------------- help --- */
/* every control: the encoders (left of the screen on the panel) left, K1..K4 and the buttons (right of it) right;
 * what each does here (docs/controls.md). Two bands: the canvas holds 144 rows */
static void draw_help(void)
{
    static const char *const ENC[5][2] = {{"MASTER", "volume"}, {"SELECT", "tempo (BPM)"},
                                          {"PRESETS", "start: section"}, {"ALGORITHM", "start: bar; loop A/B"},
                                          {"OCT- OCT+", "keys' octave"}};
    static const char *const ENC_LIST[5][2] = {{"MASTER", "volume"}, {"SELECT", "choose a song"},
                                               {"PRESETS", "-"}, {"ALGORITHM", "-"}, {"OCT- OCT+", "keys' octave"}};
    static const char *const BTN[9][2] = {{"K1-K4", "levels"}, {"FX..LFO", "mute a layer"},
                                          {"PLAY", "play / pause"}, {"HOME", "stop / list"}, {"SEQ", "loop: A, B, off"},
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
            cv_text(234 - text_w(&FONT_XS, "GLO: settings, other: back"), 6, &FONT_XS, "GLO: settings, other: back",
                    C_DIM);
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
        cv_text(152, 184 - y0, &FONT_XS, "again: settings", C_HI);
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

/* ---------------------------------------------------------------------- settings --- */
static void draw_settings(void)
{
    uint32_t i;
    cv_begin(240, 32, C_BLACK);
    cv_text(6, 4, &FONT_B, "SETTINGS", C_WHITE);
    cv_text(234 - text_w(&FONT_XS, ui.flash_ok ? "saved when you leave" : "RAM ONLY: not kept"), 6, &FONT_XS,
            ui.flash_ok ? "saved when you leave" : "RAM ONLY: not kept", ui.flash_ok ? C_DIM : C_AMB);
    cv_rect(0, 31, 240, 1, C_LINE);
    cv_blit(0, 0);
    for (i = 0; i < S_N; i++) {
        int sel = i == ui.set_row;
        cv_begin(240, 32, C_BLACK);
        if (sel)
            cv_rect(2, 2, 236, 28, C_LINE);
        cv_text(10, 7, &FONT_B, SET_NAME[i], sel ? C_WHITE : C_GRAY);
        cv_text(230 - text_w(&FONT_B, SET_VAL[i][ui.set.v[i]]), 7, &FONT_B, SET_VAL[i][ui.set.v[i]], sel ? C_HI : C_AMB);
        cv_blit(0, 32 + i * 32);
    }
    lcd_fill(0, 32 + S_N * 32, 240, 214 - (32 + S_N * 32), C_BLACK);
    draw_legend(Y_LEGEND, "SELECT row|ALGO change|GLO back");
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
        int32_t x = 6;
        if (playing_yours()) {           /* your part first (M3), then the tempo, key, count-in, loop */
            char y[24] = "";
            text_cat(y, LAYER_SHORT[yours()]);
            text_cat(y, " - YOURS");
            cv_text(6, 16, &FONT_XS, y, C_HI);
            x += text_w(&FONT_XS, y) + 8;
        }
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
        if (!ui.loop && ui.from_bar != first_bar()) {   /* (M3: the start point, here now the bottom row is gone) */
            text_cat(t, "  FROM ");
            num_cat(t, ui.from_bar + s->first_bar_number);
        }
        if (ui.loop) {
            text_cat(t, "  LOOP ");
            num_cat(t, ui.a_bar + s->first_bar_number);
            text_cat(t, "-");
            num_cat(t, ui.b_bar + s->first_bar_number);
        }
        cv_text(x, 16, &FONT_XS, t, player_tempo_of() == player_song_bpm(s) && !ui.key ? C_DIM : C_AMB);
    }
    if (ui.loop_step) {                  /* setting the loop: which mark ALGORITHM moves now */
        char k[32] = "";
        text_cat(k, ui.loop_step == 1 ? "LOOP: SET A  bar " : "LOOP: SET B  bar ");
        num_cat(k, (ui.loop_step == 1 ? ui.a_bar : ui.b_bar) + s->first_bar_number);
        cv_rect(4, 15, text_w(&FONT_XS, k) + 8, 16, C_HI);
        cv_text(8, 16, &FONT_XS, k, C_BLACK);
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

/* the rhythm cue: a note symbol for how long your part's attack lasts (whole, half, quarter, eighth, sixteenth),
 * bright at the attack (CUE_MS), dim between; drawn (there is no music font). M3: right of the big name, half M2's
 * height, no level (the layers row has it). kind 0: none */
static void draw_cue(int32_t x, int32_t y, uint32_t kind, int bright)
{
    static const uint8_t W[9] = {5, 9, 11, 11, 11, 11, 11, 9, 5};   /* the head's rows (11 x 9) */
    uint16_t c = bright ? C_HI : C_GRAY;
    int32_t i, hy = y + 20;
    if (!kind)
        return;
    for (i = 0; i < 9; i++) {
        int32_t w = W[i], ox = (11 - w) / 2 + (4 - i) / 3;   /* tilted a little */
        if (kind <= 2u && i > 1 && i < 7) {                   /* whole and half: hollow */
            cv_rect(x + ox, hy + i, 2, 1, c);
            cv_rect(x + ox + w - 2, hy + i, 2, 1, c);
        } else {
            cv_rect(x + ox, hy + i, w, 1, c);
        }
    }
    if (kind >= 2u)
        cv_rect(x + 9, y, 2, 24, c);     /* the stem */
    for (i = 0; kind >= 4u && i < (int32_t)kind - 3; i++) {   /* flags: one for an eighth, two for a sixteenth */
        cv_line(x + 10, y + i * 6, x + 16, y + 8 + i * 6, c);
        cv_line(x + 11, y + i * 6, x + 17, y + 8 + i * 6, c);
    }
}

/* what the play screen shows for your part at the displayed tick (frame_play fills it) */
struct pinfo {
    uint32_t part;                       /* the layer followed (yours(): harmony when none is muted) */
    char big[24], next[24], small[24];   /* the main view: the name now, the next one, a line under it */
    uint8_t now[SONG_VOICING_NOTES], nx[SONG_VOICING_NOTES];   /* the keys now and next (as sounded) */
    uint32_t nnow, nnx;
    uint32_t cue;                        /* bits 0..3 the cue's kind (0: none), bit 4: an attack just now */
};

/* the cue for layer l at tick: kind (1 whole .. 5 sixteenth), *bright: an attack just now */
static uint32_t cue_at(uint32_t l, int32_t tick, int *bright)
{
    uint32_t at, len, next, ppq = ui.s->ppq;
    *bright = 0;
    if (!attack_at(l, tick, &at, &len, &next))
        return 0;
    *bright = (uint32_t)tick - at < CUE_MS * ppq * player_tempo_of() / 60000u;
    return len >= ppq * 7u / 2u ? 1u : len >= ppq * 7u / 4u ? 2u : len >= ppq * 7u / 8u ? 3u : len >= ppq * 7u / 16u ? 4u : 5u;
}

/* a layer's note names for one attack: "C3" (one note, with its octave), drums "KICK + HAT" */
static void attack_name(char *d, uint32_t l, const uint8_t *v, uint32_t n, int octave)
{
    uint32_t i, best = 0;
    *d = 0;
    if (!n)
        return;
    if (l != L_DRUMS) {
        uint32_t m = moved(pick_note(l, v, n));
        text_cat(d, names()[m % 12u]);
        if (octave)
            num_cat(d, m / 12u - 1u);
        return;
    }
    for (i = 1; i < n; i++)              /* the drums: the lowest row first (kick, snare, ...) */
        if (drum_row(v[i]) < drum_row(v[best]))
            best = i;
    text_cat(d, drum_name(v[best]));
}

static void draw_big(int st, const struct pinfo *pi, int32_t count, int32_t bar, int32_t beat)
{
    cv_begin(W_BIG, H_CHORD, C_BLACK);
    if (st == PL_STOPPED && ui.loop_step) {   /* setting the loop: the steps */
        char t[32] = "A = bar ";
        text_in(0, W_BIG, 2, &FONT_M, "SET THE LOOP", C_WHITE);
        if (ui.loop_step == 1) {
            text_in(0, W_BIG, 32, &FONT_XS, "1. turn ALGORITHM to bar A", C_HI);
            text_in(0, W_BIG, 48, &FONT_XS, "2. press SEQ", C_DIM);
        } else {
            num_cat(t, ui.a_bar + ui.s->first_bar_number);
            text_in(0, W_BIG, 32, &FONT_XS, t, C_DIM);
            text_in(0, W_BIG, 48, &FONT_XS, "ALGORITHM: bar B, then SEQ", C_HI);
        }
    } else if (st == PL_STOPPED) {
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
    } else if (pi->big[0]) {             /* the name in the band's left part, the cue right of it */
        int32_t w = W_BIG - W_CUE;
        if (2 * text_w(&FONT_L, pi->big) <= w - 4)
            text2x((w - 2 * text_w(&FONT_L, pi->big)) / 2, 6, &FONT_L, pi->big, C_HI);
        else
            text_in(0, w, 22, &FONT_L, pi->big, C_HI);
        draw_cue(w + 6, 20, pi->cue & 15u, (pi->cue >> 4) & 1u);
    }
    cv_blit(0, Y_CHORD);
}

static void draw_next(int st, const struct pinfo *pi, int32_t left)
{
    cv_begin(240 - W_BIG, H_CHORD, C_BLACK);
    cv_rect(0, 6, 1, H_CHORD - 12, C_LINE);
    if (st == PL_STOPPED && ui.loop_step) {
        char t[16] = "bar ";
        num_cat(t, (ui.loop_step == 1 ? ui.a_bar : ui.b_bar) + ui.s->first_bar_number);
        cv_text(8, 4, &FONT_XS, ui.loop_step == 1 ? "A" : "B", C_DIM);
        cv_text(8, 20, &FONT_M, t, C_AMB);
        cv_blit(W_BIG, Y_CHORD);
        return;
    }
    cv_text(8, 4, &FONT_XS, st == PL_STOPPED ? "FIRST" : "NEXT", C_DIM);
    if (pi->next[0])
        cv_text(8, 20, text_w(&FONT_M, pi->next) <= 56 ? &FONT_M : &FONT_XS, pi->next, C_AMB);
    else if (st != PL_STOPPED)
        cv_text(8, 24, &FONT_XS, "END", C_DIM);
    if (left > 0) {                      /* the beats until the change, in the last bar before it */
        char t[8] = "IN ";
        num_cat(t, (uint32_t)left);
        cv_text(8, 48, &FONT_M, t, C_WHITE);
    } else if (pi->small[0]) {
        cv_text(8, 52, &FONT_XS, pi->small, C_GRAY);
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

/* bar b of layer l (bass, melody, drums) in a box at x, y (w x h, the bar number's line above y): the notes' names
 * in order, as many as fit; the drums as a small grid (kick, snare, cymbals, the others; a block per hit) */
static void bar_cell(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t b, uint32_t l, uint16_t c)
{
    const struct song_stream *st = layer_stream(l);
    struct song_event e;
    uint32_t t0 = b * bar_ticks(), k, last = 0xFFFFFFFFu, nn = 0, before;
    char t[40] = "";
    if (!st)
        return;
    for (k = song_seek(st, t0); k < st->count; k++) {
        song_event_get(st, k, &e);
        if (e.tick >= t0 + bar_ticks())
            break;
        if (e.status != 0x90u)
            continue;
        if (l == L_DRUMS) {
            int32_t px = x + 3 + (int32_t)((e.tick - t0) * (uint32_t)(w - 8) / bar_ticks());
            int32_t ry = (int32_t)drum_row(e.d1), rh = h >= 30 ? 6 : 4;
            cv_rect(px, y + ry * (rh + 1), 4, rh, c);
            continue;
        }
        if (e.tick == last)              /* one name an attack */
            continue;
        last = e.tick;
        before = text_len(t);
        if (nn)
            text_cat(t, " ");
        text_cat(t, names()[moved(e.d1) % 12u]);
        if (!nn && l == L_BASS)
            num_cat(t, moved(e.d1) / 12u - 1u);   /* (one note alone: its octave too, cut below if more come) */
        if (text_w(&FONT_XS, t) > w - 4) {
            t[before] = 0;
            break;
        }
        nn++;
    }
    if (l != L_DRUMS)
        text_in(x, w, y + (h - 16) / 2, nn == 1u ? &FONT_B : &FONT_XS, t, c);
}

static void draw_strip(int32_t bar, uint32_t l)
{
    const struct song *s = ui.s;
    uint32_t i, la, lb;
    int shown = loop_shown(&la, &lb);
    cv_begin(240, H_STRIP, C_BLACK);
    for (i = 0; i < 4u; i++) {
        int32_t b = strip_bar(bar + (int32_t)i), x = 2 + (int32_t)i * BOX_STEP;
        char t[8] = "";
        uint32_t k, k_end, t0, n = 0;
        if (b < 0) {
            frame_rect(x, 0, BOX_W, BOX_H, C_LINE);
            continue;
        }
        t0 = (uint32_t)b * s->bar_ticks;
        if (shown && (uint32_t)b >= la && (uint32_t)b <= lb) {   /* the loop's bars: filled, a line under them */
            cv_rect(x, 0, BOX_W, BOX_H, LOOP_BG);
            cv_rect(x, BOX_H + 1, BOX_W + ((uint32_t)b < lb && i < 3u ? BOX_STEP - BOX_W : 0), 3, C_AMB);
        }
        frame_rect(x, 0, BOX_W, BOX_H, i == 0 ? C_HI : C_DIM);
        if (ui.loop_step && b == bar + (int32_t)i && (uint32_t)b == (ui.loop_step == 1 ? la : lb))
            frame_rect(x + 1, 1, BOX_W - 2, BOX_H - 2, C_WHITE);   /* the mark being moved */
        num_cat(t, (uint32_t)b + s->first_bar_number);
        cv_text(x + 4, 1, &FONT_XS, t, C_DIM);
        if (shown && b == bar + (int32_t)i) {   /* one A, one B: on the bars in song order (the strip repeats a
                                                 * loop's bars after its end; M2 marked each copy) */
            if ((uint32_t)b == la)
                cv_text(x + BOX_W - 12, 1, &FONT_XS, "A", C_HI);
            if ((uint32_t)b == lb && ui.loop_step != 1)
                cv_text(x + BOX_W - (la == lb ? 22 : 12), 1, &FONT_XS, "B", C_HI);
        }
        if (l != L_HARM) {               /* your part's notes (or the drums' hits) */
            bar_cell(x + 1, 16, BOX_W - 2, BOX_H - 18, (uint32_t)b, l, i == 0 ? C_WHITE : C_AMB);
            continue;
        }
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

static int is_black(uint32_t n)
{
    uint32_t pc = n % 12u;
    return pc == 1u || pc == 3u || pc == 6u || pc == 8u || pc == 10u;
}

/* the keyboard view (M3: zoomed in, the user's note): KB_WHITES white keys, 21 px each, following your part's notes;
 * its name and keys (harmony: the level's voicing; bass, melody: the note), in the key now; at an attack bright,
 * between attacks darker (in your part's rhythm); the next outlined; under it the inversion (harmony) */
#define KB_WHITES 11u
#define KB_W 21

/* the window's lowest key (a white key): moved only when a note would fall outside it, then with a key to spare */
static void kb_follow(const struct pinfo *pi)
{
    uint32_t i, lo = 127, hi = 0, top, n;
    for (i = 0; i < pi->nnow; i++) {
        lo = pi->now[i] < lo ? pi->now[i] : lo;
        hi = pi->now[i] > hi ? pi->now[i] : hi;
    }
    for (i = 0; i < pi->nnx; i++) {
        lo = pi->nx[i] < lo ? pi->nx[i] : lo;
        hi = pi->nx[i] > hi ? pi->nx[i] : hi;
    }
    if (lo > hi)
        return;                          /* nothing to show: stay */
    for (top = ui.kb_lo, n = 1; n < KB_WHITES; top++)   /* the window's highest key */
        if (!is_black(top + 1u))
            n++;
    if (ui.kb_lo && lo >= ui.kb_lo && hi <= top)
        return;
    ui.kb_lo = lo > 1u ? lo - 1u : lo;   /* a key below the lowest, if it is white */
    while (is_black(ui.kb_lo))
        ui.kb_lo--;
    for (top = ui.kb_lo, n = 1; n < KB_WHITES; top++)
        if (!is_black(top + 1u))
            n++;
    while (hi > top && ui.kb_lo < lo) {  /* too wide: up a white key at a time while the lowest still fits */
        do
            ui.kb_lo++;
        while (is_black(ui.kb_lo));
        do
            top++;
        while (is_black(top));
    }
}

static void draw_keys_view(int st, const struct pinfo *pi, int32_t left, int bright, int32_t k)
{
    uint32_t n, i, wi = 0, last;
    int32_t x0 = 4, y0 = 34;
    uint16_t on = bright ? C_HI : DARK_AMB;
    char t[48] = "";
    kb_follow(pi);
    for (last = ui.kb_lo, n = 1; n < KB_WHITES; last++)
        if (!is_black(last + 1u))
            n++;
    cv_begin(240, H_VIEW, C_BLACK);
    if (pi->big[0])
        cv_text(6, 0, &FONT_L, pi->big, C_HI);
    if (pi->next[0]) {
        text_cat(t, st == PL_STOPPED ? "first " : "next ");
        text_cat(t, pi->next);
        if (left > 0) {
            text_cat(t, "  IN ");
            num_cat(t, (uint32_t)left);
        }
        cv_text(234 - text_w(&FONT_B, t), 6, &FONT_B, t, C_AMB);
    }
    for (n = ui.kb_lo; n <= last; n++) { /* white keys first, then the black keys over them */
        uint32_t lit = 0, dim = 0;
        if (is_black(n))
            continue;
        for (i = 0; i < pi->nnow; i++)
            lit |= pi->now[i] == n;
        for (i = 0; i < pi->nnx; i++)
            dim |= pi->nx[i] == n;
        cv_rect(x0 + (int32_t)wi * KB_W, y0, KB_W - 1, 88, lit ? on : RGB(220, 220, 225));
        if (dim && !lit)
            frame_rect(x0 + (int32_t)wi * KB_W + 4, y0 + 64, KB_W - 9, 16, C_AMB);
        if (n % 12u == 0u) {
            char o[4] = "C";
            num_cat(o, n / 12u - 1u);
            cv_text(x0 + (int32_t)wi * KB_W + 3, y0 + 70, &FONT_XS, o, lit ? C_BLACK : C_DIM);
        }
        wi++;
    }
    for (n = ui.kb_lo, wi = 0; n <= last + 1u; n++) {
        uint32_t lit = 0, dim = 0;
        if (!is_black(n)) {
            wi++;
            continue;
        }
        if (!wi)
            continue;                    /* (the window starts on a white key) */
        for (i = 0; i < pi->nnow; i++)
            lit |= pi->now[i] == n;
        for (i = 0; i < pi->nnx; i++)
            dim |= pi->nx[i] == n;
        cv_rect(x0 + (int32_t)wi * KB_W - 7, y0, 13, 54, lit ? on : RGB(40, 40, 46));
        if (dim && !lit)
            frame_rect(x0 + (int32_t)wi * KB_W - 5, y0 + 36, 9, 14, C_AMB);
    }
    t[0] = 0;
    if (pi->part == L_HARM && k >= 0) {
        inversion_cat(t, (uint32_t)k);
        text_cat(t, player_variant_of(L_HARM) >= 2u ? "  (L2)" : "  (L1)");
    } else {
        text_cat(t, LAYER_NAME[pi->part]);
        text_cat(t, " L");
        num_cat(t, player_variant_of(pi->part));
        text_cat(t, ": this key");
    }
    cv_text(6, y0 + 92, &FONT_XS, t, C_GRAY);
    cv_blit(0, Y_CHORD);
}

/* ------------------------------------------------------ the guitar view (M3 part 6, harmony) --- */
/* a chord symbol's parts: the root's pitch class, the quality ("m7"), the slash bass's pitch class (-1: none) */
static int chord_parts(const char *sym, char *qual, int *bass)
{
    uint32_t n, i = 0;
    int root = pc_at(sym, &n);
    *bass = -1;
    qual[0] = 0;
    if (root < 0)
        return -1;
    for (sym += n; *sym && *sym != '/' && i < 11u; sym++)
        qual[i++] = *sym;
    qual[i] = 0;
    if (*sym == '/')
        *bass = pc_at(sym + 1, &n);
    return root;
}

static int same_text(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

struct gfound {
    char frets[7], fingers[7];
    int kind;                            /* 0 none, 1 open, 2 barre, 3 the shape without its slash bass */
    int base;                            /* the barre's fret */
};

/* a shape for a chord symbol (in the key now): an open shape from the table (matched by pitch class, so C#/Db alike),
 * else a barre (E or A form, the lower fret), else the same without the slash bass; else none */
static void guitar_find(const char *sym, struct gfound *g)
{
    char q[12], eq[12];
    int b, eb, er, r = chord_parts(sym, q, &b), pass;
    uint32_t i, k;
    g->kind = 0;
    g->base = 0;
    if (r < 0)
        return;
    for (pass = 0; pass < 2; pass++) {   /* with the slash bass, then without */
        for (i = 0; i < sizeof GUITAR_OPEN / sizeof GUITAR_OPEN[0]; i++) {
            er = chord_parts(GUITAR_OPEN[i].name, eq, &eb);
            if (er == r && same_text(eq, q) && eb == (pass ? -1 : b)) {
                for (k = 0; k < 6u; k++) {
                    g->frets[k] = GUITAR_OPEN[i].frets[k];
                    g->fingers[k] = GUITAR_OPEN[i].fingers[k];
                }
                g->kind = pass && b >= 0 ? 3 : 1;
                return;
            }
        }
        if (b < 0 || pass == 1) {        /* a barre: E form (root on the 6th string) or A form (the 5th), lower */
            int best = 99, bi = -1;
            for (i = 0; i < sizeof GUITAR_BARRE / sizeof GUITAR_BARRE[0]; i++) {
                int fret;
                if (!same_text(GUITAR_BARRE[i].quality, q))
                    continue;
                fret = (r + 12 - (GUITAR_BARRE[i].string == 6u ? 4 : 9)) % 12;
                fret = fret ? fret : 12;
                if (fret < best)
                    best = fret, bi = (int)i;
            }
            if (bi >= 0) {
                for (k = 0; k < 6u; k++) {
                    char c = GUITAR_BARRE[bi].frets[k];
                    g->frets[k] = c == 'x' ? 'x' : (char)('0' + best + (c - '0'));
                    g->fingers[k] = GUITAR_BARRE[bi].fingers[k];
                }
                g->kind = b >= 0 ? 3 : 2;
                g->base = best;
                return;
            }
        }
        if (b < 0)
            return;
    }
}

/* a chord box at x, y: 6 strings (low E left) sw apart, 4 frets fh apart; o / x above; dots with fingers; a barre */
static void draw_chord_box(int32_t x, int32_t y, const struct gfound *g, int32_t sw, int32_t fh, uint16_t c)
{
    int32_t i, lo = 99, first = -1, last = -1, base;
    for (i = 0; i < 6; i++)
        if (g->frets[i] > '0' && g->frets[i] - '0' < lo)
            lo = g->frets[i] - '0';
    base = lo <= 4 || lo == 99 ? 1 : lo; /* up the neck: the lowest fret is the box's first */
    for (i = 0; i < 6; i++)
        cv_rect(x + i * sw, y, 1, 4 * fh, C_GRAY);
    for (i = 0; i <= 4; i++)
        cv_rect(x, y + i * fh, 5 * sw + 1, i == 0 && base == 1 ? 3 : 1, C_GRAY);
    if (base > 1) {
        char t[8] = "";
        num_cat(t, (uint32_t)base);
        text_cat(t, "fr");
        cv_text(x - text_w(&FONT_XS, t) - 4, y + 2, &FONT_XS, t, C_GRAY);
    }
    for (i = 0; i < 6; i++)              /* the barre: finger 1 on the lowest fret across strings */
        if (g->fingers[i] == '1' && g->frets[i] - '0' == lo) {
            if (first < 0)
                first = i;
            last = i;
        }
    if (g->kind == 2 && last > first)
        cv_rect(x + first * sw, y + (lo - base) * fh + fh / 2 - fh / 5, (last - first) * sw + 1, 2 * (fh / 5) + 1, c);
    for (i = 0; i < 6; i++) {
        int32_t cx = x + i * sw, f = g->frets[i] - '0';
        if (g->frets[i] == 'x') {
            cv_line(cx - 3, y - 11, cx + 3, y - 5, C_DIM);
            cv_line(cx + 3, y - 11, cx - 3, y - 5, C_DIM);
        } else if (f == 0) {
            frame_rect(cx - 3, y - 11, 7, 7, C_GRAY);
        } else {
            int32_t cy = y + (f - base) * fh + fh / 2, r = sw * 2 / 5;
            cv_rect(cx - r, cy - r + 1, 2 * r + 1, 2 * r - 1, c);
            cv_rect(cx - r + 1, cy - r, 2 * r - 1, 2 * r + 1, c);
            if (sw >= 14 && g->fingers[i] > '0') {
                char t[2] = {g->fingers[i], 0};
                cv_text(cx - text_w(&FONT_XS, t) / 2, cy - 8, &FONT_XS, t, C_BLACK);
            }
        }
    }
}

static void draw_guitar_view(int st, const struct pinfo *pi, int32_t k, int32_t next)
{
    struct gfound g, gn;
    char t[48] = "";
    cv_begin(240, H_VIEW, C_BLACK);
    if (k >= 0 || st == PL_STOPPED) {
        int32_t kc = k >= 0 ? k : next;
        const char *sym = kc >= 0 ? chord_name((uint32_t)kc) : "";
        cv_text(6, 0, &FONT_L, sym, C_HI);
        guitar_find(sym, &g);
        if (g.kind) {
            draw_chord_box(96, 30, &g, 16, 22, C_HI);
            cv_text(6, 40, &FONT_XS, "x: not played", C_DIM);
            cv_text(6, 54, &FONT_XS, "o: open string", C_DIM);
            text_cat(t, g.kind == 1 ? "guitar: open shape" : g.kind == 2 ? "guitar: barre at fret " :
                        "guitar: this shape, the bass plays ");
            if (g.kind == 2)
                num_cat(t, (uint32_t)g.base);
            if (g.kind == 3) {
                const char *p = sym;
                while (*p && *p != '/')
                    p++;
                text_cat(t, *p ? p + 1 : "");
            }
        } else if (kc >= 0 && song_cevt_root(ui.s, (uint32_t)kc) != SONG_NO_CHORD) {
            uint32_t m = song_cevt_mask(ui.s, (uint32_t)kc), pc, r = song_cevt_root(ui.s, (uint32_t)kc);
            text_in(0, 182, 50, &FONT_B, "no shape in our table", C_AMB);
            text_cat(t, "notes:");
            for (pc = 0; pc < 12u; pc++)   /* the chord's notes from its root, in the key now */
                if (m >> ((r + pc) % 12u) & 1u) {
                    text_cat(t, " ");
                    text_cat(t, names()[(r + pc + 12u + (uint32_t)ui.key) % 12u]);
                }
        }
    }
    cv_rect(182, 6, 1, 112, C_LINE);
    cv_text(190, 4, &FONT_XS, st == PL_STOPPED ? "FIRST" : "NEXT", C_DIM);
    if (pi->next[0] && next >= 0) {
        cv_text(190, 18, text_w(&FONT_M, pi->next) <= 48 ? &FONT_M : &FONT_XS, pi->next, C_AMB);
        guitar_find(chord_name((uint32_t)next), &gn);
        if (gn.kind)
            draw_chord_box(196, 64, &gn, 8, 11, C_AMB);
    }
    cv_text(6, 126, &FONT_XS, t, C_GRAY);
    cv_blit(0, Y_CHORD);
}

/* ------------------------------------------- the scrolling views (M3 parts 6, 7): one engine --- */
/* time runs right to left past a now line at x0; span ticks of the song right of it. The tab: x0 44, 1.25 bars ahead
 * (about 1.5 bars on the screen). (Part 7 adds its views to this engine.) */
#define NOW_X 44

static void scroll_setup(int view)
{
    (void)view;
    ui.sc_x0 = NOW_X;
    ui.sc_span = (int32_t)(bar_ticks() * 5u / 4u);
}

static int32_t scroll_span(void) { return ui.sc_span; }

static int32_t scroll_x(int32_t tick, int32_t now)
{
    return ui.sc_x0 + (tick - now) * (240 - ui.sc_x0) / scroll_span();   /* (32 bits: a few bars of ticks x 196 fit;
                                                                          * this CPU has no 64-bit division) */
}

/* the first tick on the screen (left edge) and the last (right edge) */
static int32_t scroll_from(int32_t now) { return now - scroll_span() * ui.sc_x0 / (240 - ui.sc_x0) - 1; }
static int32_t scroll_to(int32_t now) { return now + scroll_span() + 1; }

/* the bar lines and beats (beat ticks y0..y1), the bar numbers above at y0 - 14 */
static void scroll_grid(int32_t now, int32_t y0, int32_t y1, int numbers)
{
    int32_t bt = (int32_t)bar_ticks(), ppq = (int32_t)ui.s->ppq, t;
    for (t = (scroll_from(now) > 0 ? scroll_from(now) : 0) / ppq * ppq; t <= scroll_to(now); t += ppq) {
        int32_t x = scroll_x(t, now);
        if (x < 0 || x >= 240)
            continue;
        if (t % bt == 0) {
            cv_rect(x, y0, 1, y1 - y0, C_DIM);
            if (numbers) {
                char b[8] = "";
                num_cat(b, (uint32_t)(t / bt) + ui.s->first_bar_number);
                cv_text(x + 2, y0 - 14, &FONT_T, b, C_DIM);
            }
        } else {
            cv_rect(x, y1 - 3, 1, 3, C_LINE);
        }
    }
}

/* the note-off for note n after event index i of stream st (its tick), or the next note-on's; at most 48 events on */
static uint32_t note_end(const struct song_stream *st, uint32_t i, uint32_t n, uint32_t on_tick)
{
    struct song_event e;
    uint32_t k;
    for (k = i + 1u; k < st->count && k < i + 48u; k++) {
        song_event_get(st, k, &e);
        if ((e.status == 0x80u || e.status == 0x90u) && e.d1 == n)
            return e.tick;
    }
    return on_tick + ui.s->ppq;
}

static void scroll_now_line(int32_t y0, int32_t y1)
{
    cv_rect(ui.sc_x0 - 1, y0, 2, y1 - y0, C_AMB);
}

/* the bass tab (M3 part 6): 4 strings (G top, E bottom), the bass part at its level as fret numbers on the string
 * it is played on (first position: the highest string with a fret 0..4; above that the G string), a line for how
 * long it lasts, its name under the tab; played dim, sounding white, coming amber */
static void draw_tab_view(int32_t now, const struct pinfo *pi)
{
    static const uint8_t OPEN[4] = {28, 33, 38, 43};   /* E1 A1 D2 G2 */
    static const char *const SNAME[4] = {"E", "A", "D", "G"};
    const struct song_stream *st = layer_stream(L_BASS);
    struct song_event e;
    int32_t ys[4], i, x0 = 16;
    uint32_t k;
    char t[48] = "";
    cv_begin(240, H_VIEW, C_BLACK);
    for (i = 0; i < 4; i++) {
        ys[i] = 92 - i * 20;
        cv_text(2, ys[i] - 8, &FONT_XS, SNAME[i], C_DIM);
        cv_rect(x0, ys[i], 240 - x0, 1, C_GRAY);
    }
    scroll_grid(now, ys[3] - 6, ys[0] + 6, 1);
    if (st)
        for (k = song_seek(st, (uint32_t)(scroll_from(now) - (int32_t)bar_ticks() > 0 ?
                                          scroll_from(now) - (int32_t)bar_ticks() : 0)); k < st->count; k++) {
            uint32_t n, end, s;
            int32_t x, xe, f;
            uint16_t c;
            char fr[4] = "";
            song_event_get(st, k, &e);
            if ((int32_t)e.tick > scroll_to(now))
                break;
            if (e.status != 0x90u)
                continue;
            n = moved(e.d1);
            end = note_end(st, k, e.d1, e.tick);
            while (n < OPEN[0])
                n += 12u;                /* (below the low E: an octave up) */
            for (s = 3; s > 0 && n < OPEN[s]; s--)   /* the highest string it is on: the lowest fret */
                ;
            f = (int32_t)(n - OPEN[s]);
            x = scroll_x((int32_t)e.tick, now);
            xe = scroll_x((int32_t)end, now);
            if (xe < x0 || x >= 240)
                continue;
            c = (int32_t)end <= now ? C_DIM : (int32_t)e.tick <= now ? C_WHITE : C_AMB;
            if (xe > x + 12)
                cv_rect(x + 12, ys[s] - 1, xe - x - 12, 3, (int32_t)end <= now ? C_LINE : DARK_AMB);
            num_cat(fr, (uint32_t)f);
            cv_rect(x - 1, ys[s] - 8, text_w(&FONT_B, fr) + 3, 16, C_BLACK);
            cv_text(x, ys[s] - 9, &FONT_B, fr, c);
            cv_text(x, ys[0] + 10, &FONT_XS, names()[n % 12u], c == C_WHITE ? C_WHITE : C_GRAY);
        }
    scroll_now_line(ys[3] - 10, ys[0] + 10);
    text_cat(t, "BASS L");
    num_cat(t, player_variant_of(L_BASS));
    if (pi->big[0]) {
        text_cat(t, ": ");
        text_cat(t, pi->big);
        if (pi->next[0]) {
            text_cat(t, ", next ");
            text_cat(t, pi->next);
        }
    }
    cv_text(6, 126, &FONT_XS, t, C_GRAY);
    cv_blit(0, Y_CHORD);
}

/* the count-in over a whole view (M3: the keyboard and chart views had none after a pause): the beats left, big,
 * and the first chord */
static void draw_count_view(int32_t count, const struct pinfo *pi)
{
    char t[32] = "";
    cv_begin(240, H_VIEW, C_BLACK);
    num_cat(t, (uint32_t)count);
    text_in(0, 240, 4, &FONT_XS, "COUNT-IN", C_DIM);
    text2x((240 - 2 * text_w(&FONT_L, t)) / 2, 20, &FONT_L, t, C_AMB);
    if (pi->next[0]) {
        t[0] = 0;
        text_cat(t, "first ");
        text_cat(t, pi->next);
        text_in(0, 240, 100, &FONT_B, t, C_GRAY);
    }
    cv_blit(0, Y_CHORD);
}

/* the chart view: the section's bars (16 a page), the bar now framed bright: the chords (harmony), your part's notes
 * (bass, melody) or the drums' hits */
static void draw_chart_view(int32_t bar, int32_t section, uint32_t l)
{
    const struct song *s = ui.s;
    uint32_t first = song_section_first(s, (uint32_t)section), nb = song_section_bars(s, (uint32_t)section), i;
    uint32_t page = (uint32_t)bar >= first ? ((uint32_t)bar - first) / 16u : 0, la, lb;
    int shown = loop_shown(&la, &lb);
    char t[48] = "";
    cv_begin(240, H_VIEW, C_BLACK);
    text_cat(t, song_section_name(s, (uint32_t)section));
    text_cat(t, "  (bars ");
    num_cat(t, first + s->first_bar_number);
    text_cat(t, "-");
    num_cat(t, first + nb - 1u + s->first_bar_number);
    text_cat(t, ")");
    if (l != L_HARM) {
        text_cat(t, "  ");
        text_cat(t, LAYER_NAME[l]);
    }
    cv_text(6, 0, &FONT_XS, t, C_GRAY);
    for (i = 0; i < 16u && page * 16u + i < nb; i++) {
        uint32_t b = first + page * 16u + i, k = song_bar_chord(s, b), t0 = b * s->bar_ticks, n = 1;
        int32_t x = 2 + (int32_t)(i % 4u) * 60, y = 16 + (int32_t)(i / 4u) * 28;
        int cur = (int32_t)b == bar;
        char nbuf[8] = "", sym[24] = "";
        if (shown && b >= la && b <= lb)  /* the loop's bars filled, A and B marked */
            cv_rect(x, y, 56, 26, LOOP_BG);
        frame_rect(x, y, 56, 26, cur ? C_HI : C_DIM);
        num_cat(nbuf, b + s->first_bar_number);
        cv_text(x + 2, y, &FONT_T, nbuf, C_DIM);
        if (shown && (b == la || (b == lb && ui.loop_step != 1)))
            cv_text(x + 46, y, &FONT_T, b == la ? (b == lb && ui.loop_step != 1 ? "AB" : "A") : "B", C_HI);
        if (l != L_HARM) {
            bar_cell(x + 8, l == L_DRUMS ? y + 6 : y + 4, 48, 20, b, l, cur ? C_WHITE : C_AMB);
            continue;
        }
        while (k + n < s->ncevt && song_cevt_tick(s, k + n) < t0 + s->bar_ticks)
            n++;                         /* the changes inside the bar: the first two named */
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

/* the layers: level or MUTE; your part (several muted: the one followed) with a double frame and YOURS */
static void draw_layers(void)
{
    uint32_t l, mine = playing_yours() ? yours() : 0xFFu;
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
        if (on) {
            cv_rect(x, 1, 55, H_LAYERS - 2, C_LINE);
        } else {
            frame_rect(x, 1, 55, H_LAYERS - 2, C_AMB);
            if (l == mine)
                frame_rect(x + 1, 2, 53, H_LAYERS - 4, C_AMB);
        }
        num_cat(t, player_variant_of(l) > ly->variants ? ly->variants : player_variant_of(l));
        text_in(x, 55, 2, &FONT_XS, LAYER_SHORT[l], on ? C_WHITE : C_AMB);
        text_in(x, 55, 16, &FONT_XS, on ? t : l == mine ? "YOURS" : "MUTE", on ? C_HI : C_AMB);
    }
    cv_blit(0, Y_LAYERS);
}

/* the inversion of chord event k's voicing at the harmony's level (M3): "C/E, 1st inversion", "G7, root position".
 * The voicing's lowest note against the chord's root: a third above it 1st, a fifth 2nd, a seventh (or sixth) 3rd */
static void inversion_cat(char *d, uint32_t k)
{
    uint8_t v[SONG_VOICING_NOTES];
    uint32_t n = voicing(k, v), i, low, root = song_cevt_root(ui.s, k), iv;
    const char *sym = chord_name(k), *q;
    if (!n || root == SONG_NO_CHORD) {
        text_cat(d, sym);
        return;
    }
    for (low = v[0], i = 1; i < n; i++)
        if (v[i] < low)
            low = v[i];
    root = (root + 12u + (uint32_t)ui.key) % 12u;
    iv = (low + 12u - root) % 12u;
    text_cat(d, sym);
    for (q = sym; *q && *q != '/'; q++)
        ;
    if (iv && !*q) {                     /* not a slash chord already: the voicing's bass after a slash */
        text_cat(d, "/");
        text_cat(d, names()[low % 12u]);
    }
    text_cat(d, iv == 0u ? ", root position" : iv == 3u || iv == 4u ? ", 1st inversion" :
                iv >= 6u && iv <= 8u ? ", 2nd inversion" : iv >= 9u ? ", 3rd inversion" : ", other note in the bass");
}

static void draw_inv(const struct pinfo *pi, int32_t k)
{
    char t[48] = "", u[16] = "L";
    cv_begin(240, H_INV, C_BLACK);
    if (k >= 0 && pi->part == L_HARM) {
        inversion_cat(t, (uint32_t)k);
        num_cat(u, player_variant_of(L_HARM));
        text_cat(u, player_variant_of(L_HARM) >= 2u ? " inversions" : " root pos.");
    } else if (k >= 0) {
        text_cat(t, "chord ");
        text_cat(t, chord_name((uint32_t)k));
        text_cat(t, "   ");
        text_cat(t, LAYER_NAME[pi->part]);
        text_cat(t, " L");
        num_cat(t, player_variant_of(pi->part));
        u[0] = 0;
    } else {
        u[0] = 0;
    }
    cv_text(6, 0, &FONT_XS, t, C_GRAY);
    cv_text(234 - text_w(&FONT_XS, u), 0, &FONT_XS, u, C_DIM);
    cv_blit(0, Y_INV);
}

static const char *legend_play(int st)
{
    if (ui.keymode_until)
        return "SELECT key|SAVE done";
    if (ui.loop_step == 1)
        return "ALGO bar A|PRESETS section|SEQ set|HOME cancel";
    if (ui.loop_step == 2)
        return "ALGO bar B|PRESETS section|SEQ set|HOME back";
    if (st == PL_PLAYING || st == PL_COUNT_IN)
        return ui.loop ? "PLAY pause|SEQ loop off|EDIT view|GLO help" : "PLAY pause|EDIT view|SEQ loop|GLO help";
    if (ui.loop == LOOP_BARS && st == PL_STOPPED)
        return "PLAY from A|SEQ loop off|EDIT view|GLO help";
    if (st == PL_STOPPED)
        return "PLAY|PRESETS/ALGO start|SEQ loop|GLO help";
    if (st == PL_PAUSED)
        return "PLAY go on|HOME stop|GLO help";
    return "PLAY pause|HOME stop|SEQ loop|GLO help";
}

static uint32_t hash(const char *t, uint32_t h)
{
    while (*t)
        h = (h ^ (uint8_t)*t++) * 16777619u;
    return h;
}

/* your part at the displayed tick: the names, keys and cue for the views. k: the chord sounding (-1: none yet), next:
 * the next chord event (-1: none), left: the beats to it (harmony) */
static void part_info(struct pinfo *pi, int st, int32_t tb, int32_t k, int32_t next, int32_t left)
{
    uint32_t l = yours(), at, n, i;
    uint8_t v[SONG_VOICING_NOTES];
    int bright = 0;
    pi->part = l;
    pi->big[0] = pi->next[0] = pi->small[0] = 0;
    pi->nnow = pi->nnx = 0;
    pi->cue = 0;
    if (l == L_HARM) {
        uint32_t kc = st == PL_STOPPED || k < 0 ? (uint32_t)(next >= 0 ? next : 0) : (uint32_t)k;
        if (k >= 0)
            text_cat(pi->big, chord_name((uint32_t)k));
        if (next >= 0)
            text_cat(pi->next, chord_name((uint32_t)next));
        if (st != PL_STOPPED)
            pi->nnow = voicing(kc, pi->now);
        if (next >= 0 && left > 0)
            pi->nnx = voicing((uint32_t)next, pi->nx);
    } else {
        int32_t t = st == PL_STOPPED ? start_point() - 1 : tb;
        if (st != PL_STOPPED && (n = attack_notes(l, tb, 0, v, SONG_VOICING_NOTES, &at)) != 0) {
            attack_name(pi->big, l, v, n, 1);
            if (l != L_DRUMS) {
                pi->now[0] = (uint8_t)moved(pick_note(l, v, n));
                pi->nnow = 1;
            } else {                     /* the drums: another of the attack's sounds under the next one */
                for (i = 0; i < n; i++)
                    if (hash(drum_name(v[i]), 1u) != hash(pi->big, 1u)) {
                        text_cat(pi->small, "+ ");
                        text_cat(pi->small, drum_name(v[i]));
                        break;
                    }
            }
        }
        if ((n = attack_notes(l, t, 1, v, SONG_VOICING_NOTES, &at)) != 0) {
            attack_name(pi->next, l, v, n, l != L_DRUMS);
            if (l != L_DRUMS) {
                pi->nx[0] = (uint8_t)moved(pick_note(l, v, n));
                pi->nnx = 1;
            }
        }
        if (l != L_DRUMS && k >= 0 && !pi->small[0]) {
            text_cat(pi->small, "chord ");
            text_cat(pi->small, chord_name((uint32_t)k));
        }
    }
    if (st == PL_PLAYING && k >= 0)      /* the cue: always (M3), for your part */
        pi->cue = cue_at(l, tb, &bright) | (uint32_t)bright << 4;
}

static void frame_play(uint32_t now)
{
    const struct song *s = ui.s;
    int st = player_state();
    int32_t tc = player_tick(PL_LEAD_CHORD_MS), tb = player_tick(PL_LEAD_BEAT_MS), from = player_play_from();
    int32_t k = -1, next = -1, left = 0, count = 0, bar = 0, beat = 0, section;
    uint32_t lfrom, lto, top_key, status_key, view_key, big_key, i;
    int looping = player_loop_of(&lfrom, &lto), bright;
    struct pinfo pi;
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
        bar = ui.loop_step == 1 ? (int32_t)ui.a_bar : ui.loop_step == 2 ?   /* setting B: A on the strip too if it fits */
              (int32_t)(ui.b_bar > ui.a_bar + 3u ? ui.b_bar - 3u : ui.a_bar) : start_point() / (int32_t)s->bar_ticks;
    section = (int32_t)section_at(st == PL_STOPPED ? start_point() : tb);
    part_info(&pi, st, tb, k, next, left);
    if (pi.part != L_HARM)
        left = 0;                        /* (the countdown is the chord's) */
    bright = (pi.cue >> 4) & 1u;
    /* the key lights: your part's keys, bright at each attack, darker between (M3: blinking in its rhythm); the next
     * chord dim in the last bar before it. The drums: none (the beat light is enough) */
    ui.lit_keys = ui.mid_keys = ui.dim_keys = 0;
    if (playing_yours() && pi.part != L_DRUMS && st != PL_STOPPED) {
        uint32_t keys = 0;
        for (i = 0; i < pi.nnow; i++)
            keys |= key_bit(pi.now[i]);
        if (bright || st != PL_PLAYING || !ui.blink)
            ui.lit_keys = keys;
        else
            ui.mid_keys = keys;
        if (pi.part == L_HARM)
            for (i = 0; i < pi.nnx; i++)
                ui.dim_keys |= key_bit(pi.nx[i]);
        if (ui.lit_keys && !ui.flash_keys && st == PL_PLAYING)
            player_stats.key_flashes++;
        ui.flash_keys = ui.lit_keys != 0;
    }
    player_stats.part = playing_yours() ? pi.part : 0xFFu;
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.sh_top_key = 0xFFFFFFFFu;
    }
    big_key = hash(pi.big, hash(pi.next, hash(pi.small, 2166136261u))) ^ pi.cue ^ (uint32_t)st << 5 ^
              (uint32_t)ui.loop_step << 14 ^ (ui.loop_step ? ui.a_bar << 16 ^ ui.b_bar << 22 : 0u) ^
              (uint32_t)count << 8 ^ (uint32_t)(k + 1) << 12 ^ pi.part << 28;
    if (ui.pview != PV_CHORD) {          /* the other views: the chord band, strip and cursor's place */
        int scroll = ui.pview == PV_TAB;
        if (scroll)
            scroll_setup(ui.pview);
        view_key = big_key ^ (uint32_t)left << 22 ^ (uint32_t)(ui.pview == PV_CHART ? bar : 0) << 3 ^
                   (uint32_t)bright << 30 ^ (uint32_t)ui.pview << 26 ^ (uint32_t)(next + 1) << 18;
        if (scroll)                      /* the scrolling views: a redraw for each pixel the song moves */
            view_key ^= (uint32_t)(tb * (240 - ui.sc_x0) / scroll_span()) * 2654435761u;
        if (ui.redraw || view_key != ui.sh_view) {
            if (st == PL_COUNT_IN && count > 0)
                draw_count_view(count, &pi);
            else if (ui.pview == PV_KEYS)
                draw_keys_view(st, &pi, left, bright || st != PL_PLAYING || !ui.blink, st == PL_STOPPED ? next : k);
            else if (ui.pview == PV_GUITAR)
                draw_guitar_view(st, &pi, k, next);
            else if (ui.pview == PV_TAB)
                draw_tab_view(tb, &pi);
            else
                draw_chart_view(bar, section, pi.part);
            lcd_done();
            if (k >= 0 && k < (int32_t)PL_STATS_CHORDS && (st == PL_PLAYING || st == PL_COUNT_IN) &&
                !player_stats.chord_drawn[k])
                player_stats.chord_drawn[k] = player_frames();
            ui.sh_view = view_key;
        }
        ui.sh_chord = k;
    } else {
        /* the chord first: it is what must not be late (player.h PL_LEAD_CHORD_MS; tools/m2_test.py checks) */
        if (ui.redraw || st != ui.sh_state || big_key != ui.sh_cue ||
            (st != PL_PLAYING && st != PL_COUNT_IN && ui.sh_status == 0xFFFFFFFFu)) {
            draw_big(st, &pi, count, bar, beat);
            ui.sh_cue = big_key;
            lcd_done();
            if (k >= 0 && k < (int32_t)PL_STATS_CHORDS && (st == PL_PLAYING || st == PL_COUNT_IN) &&
                !player_stats.chord_drawn[k])
                player_stats.chord_drawn[k] = player_frames();
            ui.sh_chord = k;
            ui.sh_count = count;
        }
        uint32_t nk = hash(pi.next, hash(pi.small, 7u)) ^ (uint32_t)ui.loop_step << 3 ^ ui.a_bar << 8 ^ ui.b_bar << 20;
        if (ui.redraw || st != ui.sh_state || nk != (uint32_t)ui.sh_next || left != ui.sh_left) {
            draw_next(st, &pi, left);
            ui.sh_next = (int32_t)nk;
            ui.sh_left = left;
        }
    }
    top_key = (uint32_t)(bar * 16 + beat) ^ player_tempo_of() << 16 ^ (uint32_t)ui.loop << 25 ^
              (uint32_t)player_count_in_of() << 27 ^ (uint32_t)st << 28 ^ (uint32_t)(ui.key + 8) << 10 ^
              (ui.keymode_until ? 1u << 31 : 0u) ^ (playing_yours() ? pi.part + 1u : 0u) << 6 ^
              (uint32_t)ui.loop_step << 29 ^ ui.a_bar * 7u ^ ui.b_bar * 131u ^ ui.from_bar * 8191u;
    if (ui.redraw || top_key != ui.sh_top_key) {
        draw_top(bar, beat, st);
        ui.sh_top_key = top_key;
    }
    if (ui.pview == PV_CHORD && (ui.redraw || bar != ui.sh_strip || (int32_t)(looping ? lto : 0) != ui.sh_bar ||
                                 pi.part != ui.sh_part || ui.sh_strip == -1)) {
        draw_strip(bar, pi.part);
        ui.sh_strip = bar;
        ui.sh_bar = (int32_t)(looping ? lto : 0);
        ui.sh_part = pi.part;
    }
    if (ui.pview == PV_CHORD && (ui.redraw || beat != ui.sh_beat || st != ui.sh_state)) {
        draw_cursor(beat, st);
        ui.sh_beat = beat;
    }
    if (ui.redraw || (int32_t)(ui.muted | player_variant_of(0) << 4 | player_variant_of(1) << 6 |
                               player_variant_of(2) << 8 | player_variant_of(3) << 10 | yours() << 12) != ui.sh_layers) {
        draw_layers();
        ui.sh_layers = (int32_t)(ui.muted | player_variant_of(0) << 4 | player_variant_of(1) << 6 |
                                 player_variant_of(2) << 8 | player_variant_of(3) << 10 | yours() << 12);
    }
    status_key = (uint32_t)(k + 1) ^ (uint32_t)(next + 1) << 4 ^ pi.part << 12 ^ player_variant_of(pi.part) << 16 ^ (uint32_t)(ui.key + 8) << 20 ^
                 (uint32_t)ui.pview << 26;
    if (ui.pview == PV_CHORD && (ui.redraw || status_key != ui.sh_status)) {
        draw_inv(&pi, st == PL_STOPPED ? next : k);
        ui.sh_status = status_key;
    }
    leg = legend_play(st);
    if (ui.redraw || (uint32_t)(uintptr_t)leg != ui.sh_legend) {   /* (a new text: a new pointer) */
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
    settings_load();
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
    if (ui.view == V_SETTINGS) {
        if (ui.redraw)
            draw_settings();
        ui.redraw = 0;
        return;
    }
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
