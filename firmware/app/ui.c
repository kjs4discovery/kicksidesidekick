/* SPDX-License-Identifier: GPL-3.0-or-later */
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
#include "mascot_gen.h"
#include "guitar.h"
#include "ukulele.h"
#include "upreset.h"

static void ui_busy(const char *a, const char *b);
static void text_cat(char *d, const char *s);
static void num_cat(char *d, uint32_t v);
static int lm_active(void);              /* M10.1: SONG LEARN's melody sound on vsynth (below input_play) */
static void lm_off(void);
static void mode_leave(void);            /* M10.1: HOME on a mode's first screen (synthui.c) */
static void keys_program(int song);      /* M10.3: the keys' sound on SYNTH_KEYS_CH: the song's melody program, or the default */
static int settings_factory(void);       /* M10.1: FACTORY RESET puts the settings back */
static void ui_reset_done(void);         /* M10.1: and shows FACTORY RESET DONE */
#include "library.c"

#define KEY_LOW 53                       /* key 0 is F3 (plat.h) */
#define VELOCITY 100u
#define OCT_MAX 3
#define LIST_ROWS 5u
#define LIST_ROW_H 36
#define SPLASH_MS 4000u                  /* the splash stays at least this long after power-on (M5: +1 s) */
/* the start-up melody (M5, the user's wish; our own motif): a rising C major 7th arpeggio and a held 9th, on electric
 * piano with a little reverb, on channel 13 (no song is loaded yet), quieter than the count-in at MEDIUM */
#define TUNE_CH 12u
static const uint16_t TUNE[5][3] = {{600, 72, 180}, {780, 76, 180}, {960, 79, 180}, {1140, 83, 180},
                                    {1320, 86, 1400}};   /* {start ms, note, length ms} */
#define FLASH_MS 110u                    /* the beat light's flash on beats 2.. (M3: full brightness, every beat) */
#define FLASH_BAR_MS 220u                /* and on beat 1: longer */
#define CUE_MS 140u                      /* the rhythm cue's flash at each attack */
#define KEYMODE_MS 4000u                 /* the TEMPO mode (REC) ends this long after its last use (M10.2: the KEY mode is gone) */
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
#define SET_ROW_H 22                     /* the settings' rows (8 above the legend) */

enum { V_LIST, V_PLAY, V_HELP, V_SETTINGS, V_VIEWSET, V_HINT_UNUSED, V_MODE, V_SYNTH,   /* (M5: the mode screen, SYNTH; M10.2: V_HINT, the layer-buttons screen, is gone: its slot stays so the other values keep their numbers) */
       V_WIPE,                           /* M5.1: WIPE SONG STORE, asked first */
       V_SETUP,                          /* M8: BACKTRACK's setup (bt.c) */
       V_RESET };                        /* M10.1: FACTORY RESET DONE - SWITCH OFF AND ON: nothing more is drawn or read */
#define WIPE_HOLD_MS 2000u               /* PLAY held this long on the WIPE screen: the songs are erased */
static void wipe_open(int back);
/* the play screen's views (EDIT); PV_CHORD: the part's main view (the chord, or your part's note or hit) */
enum { PV_CHORD, PV_KEYS, PV_CHART, PV_GUITAR, PV_TAB, PV_ROLL, PV_NOTATION, PV_GRID, PV_DNOTE,
       PV_MTAB,                          /* M6: the melody's tab (guitar, or ukulele: the setting) */
       PV_BARS,                          /* M6: the harmony's bar view (the roll: chords as stacked bars) */
       PV_SCALE_KB, PV_SCALE_FB,         /* M8: BACKTRACK's melody: the scale on the keys, on the fretboard (bt.c) */
       PV_N };
static const uint8_t VIEWS[SONG_LAYERS][8] = {   /* each part's views in EDIT's order (PV_N ends a list) */
    {PV_GRID, PV_DNOTE, PV_N},                                    /* drums (M4: only these two, the user's note) */
    {PV_CHORD, PV_KEYS, PV_CHART, PV_TAB, PV_NOTATION, PV_N},     /* bass */
    {PV_CHORD, PV_KEYS, PV_CHART, PV_GUITAR, PV_NOTATION, PV_BARS, PV_N},   /* harmony (M4: + the notation, a grand
                                                                             * staff; M6: + the bar view) */
    {PV_CHORD, PV_KEYS, PV_CHART, PV_ROLL, PV_NOTATION, PV_MTAB, PV_N},     /* melody (M6: + its tab) */
};
static const char *const VIEW_NAME[PV_N] = {"MAIN", "KEYBOARD", "CHART", "GUITAR", "BASS TAB", "MELODY ROLL",
                                            "NOTATION", "DRUM GRID", "DRUM NOTATION", "GUITAR TAB", "BAR VIEW",
                                            "SCALE KEYBOARD", "SCALE FRETBOARD"};
static const char *view_name(uint32_t v);   /* (M6: the ukulele setting renames two)
                                              * the scrolling and staff views: SELECT zooms there (M6) */
#define ZOOMS 7u
static const uint8_t ZOOM_Q[ZOOMS] = {2, 3, 4, 6, 8, 12, 16};   /* the span, in quarters of the M5 one (1.25 bars
                                                                 * ahead; the notation 36 px a beat) */
#define ZOOM_STAFF_MAX 4u                /* the notation views: up to twice the span (the noteheads keep apart) */
static int view_staff(int view);
static int view_zooms(int view);
static void scroll_setup(int view);
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
enum { S_BLINK, S_COUNT_BARS, S_COUNT_VOL, S_NOTE_NAMES, S_ZOOM, S_QUIET, S_LIGHTS, S_VIEWS, S_BASS_OCT, S_HINT,
       S_MODE,                           /* M6: back to the mode screen (LEARN / SYNTH), no value */
       S_UKE,                            /* M6: chord boxes and the melody's tab: GUITAR / UKULELE */
       S_SCALE,                          /* M8: BACKTRACK's scale guide: PENTATONIC / FULL SCALE / CHORD TONES */
       S_WIPE,                           /* M5.1: a screen (WIPE SONG STORE), no value; the last row */
       S_N };
struct settings {
    uint32_t magic, version;
    uint8_t v[8];                        /* S_* values 0..7 */
    uint8_t hide[4];                     /* M4: views hidden from EDIT, per layer (bit: PV_*; the main view is bit 0) */
    uint8_t hide2[4];                    /* (bits 8..: PV_DNOTE) */
    uint8_t v2[8];                       /* M5: S_* values 8.. */
    uint8_t hint_count, pad[3];          /* M5: times the controls hint was shown. M10.2: the firmware marker (a 32-bit hash of FW_VERSION,
                                          * little-endian over these 4 bytes): S_HINT is now "Pop-ups" and is set ON again after each update */
    uint8_t synth[4];                    /* M7: SYNTH's settings: drum volume 0..10, drum kit (KIT_*); 2 spare */
};
#define SET_SIZE_M4 24u                  /* M4's record: read too, the M5 fields then get their defaults */
#define SET_SIZE_M5 36u                  /* M5..M6's: read too, the M7 fields then get their defaults */
#define SYNTH_SET_DEFAULT {7, 0, 0, 0}   /* drum volume 7 of 10, kit ACOUSTIC */
#define SV(i) (*((uint32_t)(i) < 8u ? &ui.set.v[(i)] : &ui.set.v2[(uint32_t)(i) - 8u]))
static const char *const SET_NAME[S_N] = {"Key lights blink", "Count-in length", "Count-in volume",
                                          "Note names (notation)", "Zoom at start", "Quiet level",
                                          "Key lights", "Views (hide some)", "Bass sound", "Pop-ups",
                                          "Switch mode", "Chord boxes, tab", "Scale guide", "Wipe song store"};
static const uint8_t SET_COUNT[S_N] = {2, 2, 3, 2, 2, 2, 2, 1, 2, 2, 1, 2, 3, 1};   /* how many values each has (Views: a
                                                                                  * screen) */
static const char *const SET_VAL[S_N][3] = {{"OFF", "ON", ""}, {"1 BAR", "2 BARS", ""}, {"LOW", "MEDIUM", "HIGH"},
                                            {"OFF", "ON", ""}, {"1.5 BARS", "3 BARS", ""}, {"25 %", "50 %", ""},
                                            {"OFF", "ON", ""}, {">", "", ""}, {"REAL", "+1 OCTAVE", ""},
                                            {"OFF", "ON", ""}, {">", "", ""}, {"GUITAR", "UKULELE", ""},
                                            {"PENTATONIC", "FULL SCALE", "CHORD TONES"}, {">", "", ""}};
/* (M4: count-in volume MEDIUM, the user's note; the quiet level 50 %, "half volume") */
/* (M5: the bass +1 octave on the small speaker; the controls hint on) */
static const uint8_t SET_DEFAULT[S_N] = {1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0};
static const uint8_t QUIET_PCT[2] = {25, 50};   /* a layer set to QUIET (D027: M3's guide volume became this) */
#define HOME_HOLD_MS 2000u               /* M6: HOME held this long, from any screen: the mode screen */
#define HOLD_MS 900u                     /* a layer button held this long: your part (D027); shorter: its volume */
enum { VOL_FULL, VOL_QUIET, VOL_MUTED };
static const uint8_t COUNT_VOL[3] = {64, 96, 127};   /* the beep's channel volume */

static struct {
    int view, back;                      /* V_*; back: the view the help returns to */
    int wipe_back;                       /* M5.1: the view WIPE returns to; */
    uint32_t wipe_t0, wipe_done;         /* when PLAY went down on it (0: not held); 1 + the result once done */
    uint32_t wipe_armed;                 /* PLAY seen up since it opened: only a new press counts */
    uint32_t home_t, home_armed;         /* M6: when HOME went down (0: up); HOME seen up since power-on */
    uint32_t seen_up;                    /* M7: the buttons seen up since power-on (a held one never raises the pop-up) */
    uint32_t popup, popup_until;         /* M7 (D044): the NOT IMPLEMENTED pop-up open (1: to draw, 2: drawn) */
    uint32_t closed;                     /* M7: the buttons that closed it, still held (their release does nothing) */
    uint32_t home_free;                  /* M7: HOME went down where it has no job (its release: the pop-up) */
    uint32_t home_tap;                   /* M10.1: HOME went down (armed): a tap acts when it is released, before 2 s */
    struct settings set;
    uint32_t set_row, set_dirty;
    uint32_t sel, top;                   /* the list's cursor and first row */
    int folder;                          /* M3 part 8: the folder shown: -1 the folders, 0 all songs, k category k */
    uint32_t nfold, fsel;                /* categories found (folders 1..nfold), the folder list's cursor */
    char fname[16][16];                  /* their names, sorted; "Other": songs with none */
    uint16_t fl[LIB_MAX];                /* the folder's songs: list indices (by title); M10.4: store songs and built-ins (up to 257) */
    uint32_t nfl, fl_seen;
    const struct song *s;
    uint32_t keys, buttons;
    uint8_t key_note[NKEYS];             /* the note each held key sent (the octave may change while it is held) */
    uint8_t key_route[NKEYS];            /* M10.3: where it went: 0 the GM keys (EAS), 1 vsynth (BACKTRACK, or SONG LEARN's melody preset) */
    int32_t oct;                         /* the keys' octave, -OCT_MAX..OCT_MAX */
    uint32_t muted;
    int loop;
    uint32_t from_bar;                   /* the start point: a bar index; from_tick its first tick of music */
    int32_t from_tick;
    uint32_t a_bar, b_bar;               /* the loop, bars A..B (M3: the one kind of loop) */
    int loop_step;                       /* setting the loop: 1 moving A, 2 moving B (SEQ confirms each); 0 not */
    uint32_t tempo_ms;                   /* the last tempo click (quick turns: bigger steps) */
    int32_t key;                         /* the transposition, -5..+6 semitones (the key: the nearest way there) */
    uint8_t paused_start;                /* M10.2: ALGO moved the start while paused: PLAY goes on from the new start */
    int pview;                           /* PV_*: the view now; pv_of[l]: the last one used for each part */
    int32_t sc_x0, sc_span;              /* the scrolling view's now line and ticks ahead (scroll_setup) */
    uint32_t zoom;                       /* M6: ZOOM_Q's index (SELECT in the scrolling and staff views) */
    uint32_t tempomode_until;            /* M6: the TEMPO mode (REC): SELECT sets the tempo there until then */
    uint8_t pv_of[SONG_LAYERS];
    uint32_t vs_row;                     /* the Views settings screen's cursor (M4) */
    uint32_t mine;                       /* your part (D027; M10.1, D066: a press on its button); harmony when a song opens */
    uint8_t vol[SONG_LAYERS];            /* VOL_FULL / VOL_QUIET / VOL_MUTED: a press cycles it (D027) */
    uint32_t hold_t[SONG_LAYERS], hold_done, released;   /* a layer button's press time; held long (done); released */
    uint32_t kb_lo;                      /* the keyboard view's lowest key (it follows the notes) */
    uint32_t lit_keys, mid_keys, dim_keys;   /* the key lights for your part (frame_play sets them): at an attack,
                                              * between attacks, the next chord */
    char msg[2][24];
    uint32_t msg_until;
    uint8_t flash_ok, splash, redraw, flashing, flash_keys, blink, tune_on, tune_off;
    uint32_t lib_seen;                   /* library.c lib_changes when the screen was last set up */
    /* what is on the screen */
    int32_t sh_chord, sh_count, sh_next, sh_left, sh_bar, sh_beat, sh_strip, sh_layers, sh_section;
    int sh_state;
    uint32_t sh_top_key, sh_stats_ms, sh_list, sh_status, sh_legend, sh_cue, sh_view, sh_part;
} ui;

/* M8: BACKTRACK's state (bt.c) */
#define BT_MAX 64u
static struct {
    uint8_t on;                          /* BACKTRACK is the mode */
    uint8_t entered;                     /* entered since power-on (the defaults set) */
    uint8_t bpm_turned;                  /* SELECT turned: the tempo stays when the style changes */
    uint32_t sound_ref;                  /* M10.1: the melody sound as a reference (pl_ref), not a list position: a new save cannot shift it */
    uint32_t prog, style, structure, key, bpm, sound;   /* the choices (prog: an index into list[]); on the play screen
                                          * what is playing now */
    uint32_t kit;                        /* M10: the band kit chosen on SETUP (0 STUDIO .. 3 CHIP; kept until power-off) */
    uint32_t cplx;                       /* M9 (D053): the complexity, 1 SIMPLE .. 3 BUSY (the tier of verses, intros and
                                          * bridges; choruses one more) */
    uint32_t n;                          /* backtracks found */
    uint16_t list[BT_MAX];               /* their lib indices, by number (M10.4: uint16_t: the list has up to 257 songs) */
    uint32_t shown;                      /* the setup screen drawn (a hash) */
    uint32_t sound_until;                /* the melody sound's name shown on the play screen until */
    /* M9: the play screen's live changes (bt.c): what the knobs ask for, and the swap prepared for it */
    uint32_t w_prog, w_style, w_structure, w_cplx, w_key;
    uint32_t w_ms;                       /* the last knob turn */
    uint32_t cur;                        /* the half of the song room the version playing is in (bt_song[cur]) */
    uint32_t swaps_seen;                 /* player_swaps() as the UI has taken it */
    uint8_t sw_pending;                  /* a swap asked of the player, not made yet (1); asked to be withdrawn (2) */
    uint32_t sw_prog, sw_style, sw_structure, sw_cplx, sw_key;   /* what it brings */
    uint32_t sw_ms, sw_bar;              /* the knob time it was made for; the bar line (index) it is asked for */
    uint32_t no_ms;                      /* the knob time for which no boundary was left (the song ends first) */
    uint32_t ind_sec;                    /* M10: the section indicator: the section it names, blinking, shown (this frame) */
    uint8_t ind_blink, ind_on, ind_loop;   /* ind_loop (M10.1): the structure is LOOP: the indicator says LOOP, never blinks */
} bt;

/* M10 (D057): the VALUE POP-UP: a title and a value in a big font over the screen, for BACKTRACK's knobs (and "SAVED U05").
 * Separate from D044's NOT IMPLEMENTED pop-up: a press closes it and does its job; it closes by itself 1,500 ms after the
 * last turn; the music and the lights go on, the screen under it is frozen and drawn again when it closes. */
#define VPOP_MS 1500u
#define COL_MINOR RGB(96, 156, 240)        /* the MINOR badge (MAJOR: the amber highlight) */
static struct {
    uint8_t open, dirty, badge;          /* badge: 0 none, 1 MAJOR, 2 MINOR */
    uint8_t big_sub;                     /* M10.3: only the Start pop-up (say_from): the sub line is the section's name, big */
    uint32_t until, since;
    char title[32], value[40], sub[40];   /* (M10.2: "Harmony Complexity Level" is 24, the Track Selected hint 38 characters) */
} vpop;
static void vpop_show(const char *title, const char *value, const char *sub, uint32_t badge);   /* a value pop-up: off with Pop-ups OFF (M10.2) */
static void vpop_keep(const char *title, const char *value, const char *sub, uint32_t badge);   /* the same, always: confirmations, errors */
static void bt_vp_select(void);          /* (bt.c) */
static uint32_t bt_ind_update(int st);   /* (bt.c) */
static void vpop_text(volatile char *d, const char *t, uint32_t n)
{
    uint32_t i;
    for (i = 0; i + 1u < n && t[i]; i++)
        d[i] = t[i];
    for (; i < n; i++)
        d[i] = 0;
}

/* a part's views in EDIT's order (M8: BACKTRACK's melody, the player's: the scale views) */
static const uint8_t VIEWS_BT_MELODY[3] = {PV_SCALE_KB, PV_SCALE_FB, PV_N};
static const uint8_t *views_of(uint32_t l)
{
    return bt.on && l == L_MELODY ? VIEWS_BT_MELODY : VIEWS[l];
}

/* ------------------------------------------------------------------------ settings --- */
static void settings_apply(void)
{
    ui.blink = SV(S_BLINK) != 0;
    player_bass_octave(SV(S_BASS_OCT) ? 12 : 0);
    player_count_in_bars(SV(S_COUNT_BARS) ? 2u : 1u);
    player_count_in_volume(COUNT_VOL[SV(S_COUNT_VOL) % 3u]);
    player_stats.uke = SV(S_UKE);
    player_stats.settings = SV(0) | (uint32_t)SV(1) << 4 | (uint32_t)SV(2) << 8 |
                            (uint32_t)SV(3) << 12 | (uint32_t)SV(4) << 16 | (uint32_t)SV(5) << 20 |
                            (uint32_t)SV(6) << 24 | (uint32_t)SV(8) << 28;
}

static void settings_load(void)
{
    uint32_t i;
    static const uint8_t SYD[4] = SYNTH_SET_DEFAULT;
    int n = plat_store_load(OBJ_SETTINGS, &ui.set, sizeof ui.set);
    if ((n != (int)sizeof ui.set && n != (int)SET_SIZE_M5 && n != (int)SET_SIZE_M4) || ui.set.magic != SET_MAGIC) {
        ui.set.magic = SET_MAGIC;        /* none yet (or not ours): the defaults */
        ui.set.hide[0] = ui.set.hide[1] = ui.set.hide[2] = ui.set.hide[3] = 0;
        ui.set.hide2[0] = ui.set.hide2[1] = ui.set.hide2[2] = ui.set.hide2[3] = 0;
        for (i = 0; i < 8u; i++)
            SV(i) = SET_DEFAULT[i];
        n = (int)SET_SIZE_M4;
    }
    if (n == (int)SET_SIZE_M4) {         /* M4's record (or the defaults above): the M5 fields' defaults */
        for (i = 8; i < 16u; i++)
            SV(i) = i < S_N ? SET_DEFAULT[i] : 0;
        ui.set.hint_count = 0;
    }
    if (n != (int)sizeof ui.set)         /* M7: an older record: SYNTH's settings' defaults */
        for (i = 0; i < 4u; i++)
            ui.set.synth[i] = SYD[i];
    ui.set.version = 4;                  /* (M5: 3; M7: 4, the synth fields) */
    for (i = 0; i < S_N; i++)            /* (a value out of range: the default) */
        if (SV(i) >= SET_COUNT[i])
            SV(i) = SET_DEFAULT[i];
    settings_apply();
}

/* M10.2: Pop-ups is ON after every firmware update: the record keeps a 32-bit marker of the firmware's version (FNV-1a of FW_VERSION, never 0) in
 * hint_count + pad[0..2]. Different (an update, an older record): Pop-ups ON, the marker stored, the settings saved once. Same: the user's choice
 * stays. Called once at start-up (app_ui_init), not by a factory reset (its defaults are ON already). */
static uint32_t fw_marker(void)
{
    const char *v = FW_VERSION;
    uint32_t h = 2166136261u;
    while (*v)
        h = (h ^ (uint8_t)*v++) * 16777619u;
    return h ? h : 1u;
}

static void settings_save(void);
static void settings_marker(void)
{
    uint32_t m = fw_marker(), have = (uint32_t)ui.set.hint_count | (uint32_t)ui.set.pad[0] << 8 | (uint32_t)ui.set.pad[1] << 16 |
                                     (uint32_t)ui.set.pad[2] << 24;
    if (have == m)
        return;
    SV(S_HINT) = 1;
    ui.set.hint_count = (uint8_t)m;
    ui.set.pad[0] = (uint8_t)(m >> 8);
    ui.set.pad[1] = (uint8_t)(m >> 16);
    ui.set.pad[2] = (uint8_t)(m >> 24);
    ui.set_dirty = 1;
    settings_apply();
    settings_save();
}

/* a view hidden from EDIT (the Views settings, M4) */
static int view_hidden(uint32_t l, uint32_t v)
{
    return (v < 8u ? ui.set.hide[l] >> v : ui.set.hide2[l] >> (v - 8u)) & 1u;
}

static void view_toggle(uint32_t l, uint32_t v)
{
    if (v < 8u)
        ui.set.hide[l] ^= (uint8_t)(1u << v);
    else
        ui.set.hide2[l] ^= (uint8_t)(1u << (v - 8u));
}

/* the part's view to show: v if it is one of its views and not hidden, else its first one shown */
static uint32_t view_ok(uint32_t l, uint32_t v)
{
    uint32_t i, ok = 0;
    const uint8_t *vw = views_of(l);
    for (i = 0; vw[i] != PV_N; i++)
        ok |= vw[i] == v;
    if (ok && !view_hidden(l, v))
        return v;
    for (i = 0; vw[i] != PV_N; i++)
        if (!view_hidden(l, vw[i]))
            return vw[i];
    return vw[0];
}

static const char *view_name(uint32_t v)
{
    if (SV(S_UKE) && v == PV_GUITAR)
        return "UKULELE";
    if (SV(S_UKE) && v == PV_MTAB)
        return "UKULELE TAB";
    return v < PV_N ? VIEW_NAME[v] : "";
}

static void settings_save(void)
{
    if (!ui.set_dirty)
        return;
    ui.set_dirty = 0;
    if (ui.flash_ok && plat_store_save(OBJ_SETTINGS, &ui.set, sizeof ui.set) == 0)
        player_stats.settings_saves++;
}

/* M10.1: FACTORY RESET's settings: a record that is not ours is saved (an A/B store save: old or new, torn-write safe); reading it back is
 * "none yet", so settings_load's own defaults apply (and are applied); 0 ok */
static int settings_factory(void)
{
    struct settings z;
    uint32_t i;
    int e = 0;
    for (i = 0; i < sizeof z; i++)
        ((uint8_t *)&z)[i] = 0;
    if (ui.flash_ok)
        e = plat_store_save(OBJ_SETTINGS, &z, sizeof z);
    if (e == 0)
        settings_load();
    ui.set_dirty = 0;
    return e;
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

static int same_text(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
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
/* the artwork (logo_gen.h, mascot_gen.h: tools/logo_to_c.py from firmware/assets/src/kickside-*.jpg), drawn 1 : 1. Called
 * by the platform at power-on, before anything else of the app runs; the list comes SPLASH_MS after power-on */
struct art {                             /* M10.3: one drawing function for the splash's logo and the mode screen's mascot */
    uint16_t w, h, bg;
    const uint16_t *pal;
    const uint8_t *rle;
    uint32_t len;
};
static const struct art ART_LOGO = {LOGO_W, LOGO_H, LOGO_BG, LOGO_PAL, LOGO_RLE, sizeof LOGO_RLE};
static const struct art ART_MASCOT = {MASCOT_W, MASCOT_H, MASCOT_BG, MASCOT_PAL, MASCOT_RLE, sizeof MASCOT_RLE};

/* the picture, its left edge centred, its top at oy (stored at its display size) */
static void draw_logo_s(const struct art *a, int32_t oy)
{
    const uint8_t *p = a->rle, *end = a->rle + a->len;
    uint32_t W = a->w, H = a->h, y0 = 0, x = 0, y = 0;
    int32_t ox = (240 - (int32_t)W) / 2;
    while (y0 < H) {                     /* in bands the canvas can hold */
        uint32_t band = H - y0 < 112u ? H - y0 : 112u;
        cv_begin(W, band, a->bg);
        while (p < end && y < y0 + band) {
            uint32_t c = *p >> 4, n = (*p & 15u) + 1u;
            if ((*p & 15u) == 15u)
                n = 16u + p[1], p++;
            p++;
            if (n > W - x)
                n = W - x;
            if (y >= y0)
                cv_rect((int32_t)x, (int32_t)(y - y0), (int32_t)n, 1, a->pal[c]);
            x += n;
            if (x >= W)
                x = 0, y++;
        }
        cv_blit((uint32_t)ox, (uint32_t)oy + y0);
        y0 += band;
    }
}

static void draw_logo(int32_t oy)
{
    draw_logo_s(&ART_LOGO, oy);
}

/* M10.3: the logo (it has the name in it) at y 36, the version small at the lower right */
void app_splash(void)
{
    lcd_fill(0, 0, 240, 240, LOGO_BG);
    draw_logo(36);
    cv_begin(240, FONT_XS.h, LOGO_BG);
    cv_text(234 - text_w(&FONT_XS, "v" FW_VERSION), 0, &FONT_XS, "v" FW_VERSION, RGB(150, 136, 118));
    cv_blit(0, 240u - 6u - FONT_XS.h);
}

/* ---------------------------------------------------------------- the key (part 3, item 8) --- */
/* names for the key now: sharps or flats by the key's signature (its mode's parent major); C major mixed */
static const char *const SHARP[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const FLAT[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
static const char *const MIXED[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const uint8_t MODE_PARENT[7] = {0, 3, 10, 5, 7, 8, 1};   /* major minor dorian mixolydian lydian phrygian locrian */

static const char *const *names_of(uint32_t root, uint32_t mode, int32_t key)   /* (M8: BACKTRACK's setup too) */
{
    uint32_t parent = (root + MODE_PARENT[mode % 7u] + 12u + (uint32_t)(key + 12)) % 12u;
    if (parent == 0)
        return MIXED;
    return parent == 5 || parent == 10 || parent == 3 || parent == 8 || parent == 1 ? FLAT : SHARP;
}

static const char *const *names(void) { return names_of(ui.s->key_root, ui.s->key_mode, ui.key); }

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

/* ----------------------------------------------- your part (M4, D027: chosen on purpose, whatever its volume) --- */
/* your part: the screen, the rhythm cue, the key lights and the views follow it, muted or not (M3: the last layer
 * muted). Press its button to choose it (M10.1; was: hold it about a second); a song opens with the harmony */
static uint32_t yours(void) { return ui.mine < SONG_LAYERS ? ui.mine : L_HARM; }

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
/* the voicing level the harmony's level shows (CEVT: 1 root position, 2 inversions). M4's ladder (3 to 5 levels): L1,
 * L2 root position, L3 to L5 inversions; an older song (2 levels): L2 inversions */
static uint32_t harm_voicing_level(void)
{
    uint32_t v = player_variant_of(L_HARM), n = ui.s->layer[L_HARM].variants;
    return n >= 3u ? (v >= 3u ? 2u : 1u) : (v >= 2u ? 2u : 1u);
}

/* the harmony plays its chord a note at a time: M4's L4 (broken chords) and L5 (arpeggios) */
/* the harmony level's short name: what it plays (M4's ladder; an older song: root pos. / inversions) */
static const char *harm_level_name(void)
{
    static const char *const N[5] = {" held", " rhythm", " inversions", " broken", " arpeggio"};
    uint32_t v = player_variant_of(L_HARM);
    if (ui.s->layer[L_HARM].variants >= 3u && v >= 1u && v <= 5u)
        return N[v - 1u];
    return harm_voicing_level() >= 2u ? " inversions" : " root pos.";
}

static int harm_broken(void)
{
    return ui.s->layer[L_HARM].variants >= 4u && player_variant_of(L_HARM) >= 4u;
}

static uint32_t voicing(uint32_t k, uint8_t *out)
{
    const uint8_t *v = song_cevt_voicing(ui.s, k, harm_voicing_level());
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

/* the loop: LOOP_OFF, LOOP_BARS (A..B) or LOOP_SONG (M4, back by the user's wish: the whole song, from its first
 * note to its end, round again) */
static void set_loop(int mode)
{
    uint32_t from = 0, to = 0;
    ui.loop = mode;
    loop_region(&from, &to);
    if (mode == LOOP_SONG) {
        from = ui.s->start_tick;
        to = ui.s->end_tick;
    }
    player_loop(ui.loop, from, to);
    ui.sh_strip = -1;
    ui.sh_view = 0;
}

/* where PLAY starts from stopped: the loop's start, or the start point */
static int32_t start_point(void)
{
    if (ui.loop == LOOP_OFF || ui.loop == LOOP_SONG)
        return ui.from_tick;
    return (int32_t)(ui.a_bar * bar_ticks());
}

/* the bars highlighted as the loop (also while it is being set: A alone, then A..B); 0: none */
static int loop_shown(uint32_t *a, uint32_t *b)
{
    *a = ui.a_bar;
    *b = ui.loop_step == 1 ? ui.a_bar : ui.b_bar;
    return ui.loop_step || ui.loop == LOOP_BARS;   /* (the whole song: not filled, the top line says so) */
}

/* ------------------------------------------------------------------------ input --- */
static void open_play(const struct song *s);

static void open_song(uint32_t i)
{
    const struct song *s;
    int r;
    lm_off();                            /* M10.1: a song opened: its own melody instrument again */
    r = lib_open(i, &s);             /* checked in full at first use, not at boot */
    if (r) {
        app_say("SONG ERROR", song_error_name(r));
        ui.sh_list = 0xFFFFFFFFu;
        return;
    }
    open_play(s);
}

/* a song opened (lib_open): fresh, its play screen (M8: BACKTRACK's too, bt.c bt_open) */
static void open_play(const struct song *s)
{
    int r;
    ui.s = s;
    ui.muted = 0;
    ui.mine = L_HARM;
    ui.pv_of[0] = ui.pv_of[1] = ui.pv_of[2] = ui.pv_of[3] = 0;   /* M5: fresh, as at a first visit (the user) */
    ui.pview = (int)view_ok(L_HARM, PV_CHORD);
    player_stats.view = (uint32_t)ui.pview;
    ui.vol[0] = ui.vol[1] = ui.vol[2] = ui.vol[3] = VOL_FULL;
    ui.hold_done = 0;
    player_load(ui.s);
    player_mute(0);
    for (r = 0; r < (int)SONG_LAYERS; r++)
        lib_level((uint32_t)r, s->layer[r].start ? s->layer[r].start : 1u);   /* (2.0: level 1 is built at open;
                                          * a song opened again: built anew. M7, 2.1: the song's start level) */
    player_tempo(player_song_bpm(s));    /* every song opens at its own tempo, in its own key */
    ui.key = 0;
    ui.kb_lo = 0;
    ui.tempomode_until = 0;
    ui.zoom = SV(S_ZOOM) ? 4u : 2u;      /* M6: the zoom at start (the setting: 1.5 or 3 bars), SELECT from there */
    player_transpose(0);
    set_from_bar(first_bar());
    ui.from_tick = (int32_t)section_from(0);
    set_loop(LOOP_OFF);
    ui.loop_step = 0;
    if (!bt.on)
        keys_program(1);                 /* M10.3: the melody sound starts as SONG: the keys play the song's melody instrument */
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
        if (bt.on) {                     /* M9: the melody has no layer, but LFO held makes it your part: its light is on */
            dim |= 1u << LAYER_BUTTON[L_MELODY];
            lit |= 1u << LAYER_BUTTON[L_MELODY];
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
    if (ui.view == V_SETUP && bt.n)      /* M10.2: BACKTRACK's SETUP: PLAY full when there are backtracks to play (as on the mode screen) */
        lit |= 1u << B_PLAY;
    if (ui.oct < 0)
        lit |= 1u << B_OCTDN;
    if (ui.oct > 0)
        lit |= 1u << B_OCTUP;
    if (ui.view != V_PLAY || st == PL_STOPPED)
        ui.lit_keys = ui.mid_keys = ui.dim_keys = 0;
    player_stats.lit_keys = ui.lit_keys;
    player_stats.dim_keys = ui.dim_keys;
    player_stats.mid_keys = ui.mid_keys;
    player_stats.btn_lit = lit;
    player_stats.btn_dim = dim;
    player_stats.loop_step = (uint32_t)ui.loop_step;
    plat_lights(lit, ui.keys | ui.lit_keys, dim, ui.dim_keys & ~(ui.lit_keys | ui.mid_keys), 1);   /* keys pressed:
                                                                                                 * lit as ever */
    plat_lights_mid(mid, ui.mid_keys & ~(ui.keys | ui.lit_keys));
}

/* ---------------------------------------------------------------- song folders (M3 part 8) --- */
static int text_before(const char *p, const char *q)
{
    while (*p && lower(*p) == lower(*q))
        p++, q++;
    return lower(*p) < lower(*q);
}

/* a song's folder name: its category, or "Other"; a song that does not peek: "Other" */
static const char *song_folder(uint32_t i)
{
    int err;
    const struct lib_item *s = lib_peek(i, &err);
    if (!s)
        s = lib_raw(order[i]);
    return !lib_perr(order[i]) && s->category && s->category[0] ? s->category : "Other";
}

/* the categories (sorted, at most 16; more: in "All songs" only) and the songs of the folder shown */
static int lib_is_backtrack(uint32_t i);

/* M9: the songs in the store, the backtracks not counted (SONG LEARN with only backtracks stored is as an empty store) */
static uint32_t lib_song_count(void)
{
    uint32_t i, n = lib_count(), c = 0;
    for (i = 0; i < n; i++)
        c += !lib_is_backtrack(i);
    return c;
}

static void folders_build(void)
{
    uint32_t i, k, n = lib_count();
    ui.nfold = 0;
    for (i = 0; i < n; i++) {
        const char *c = song_folder(i);
        if (lib_is_backtrack(i))         /* M8: SONG LEARN lists only songs */
            continue;
        for (k = 0; k < ui.nfold && !same_text(ui.fname[k], c); k++)
            ;
        if (k == ui.nfold && ui.nfold < 16u) {
            for (k = ui.nfold++; k > 0 && text_before(c, ui.fname[k - 1u]); k--)   /* insertion, sorted */
                text_cat(ui.fname[k], ""), ui.fname[k][0] = 0, text_cat(ui.fname[k], ui.fname[k - 1u]);
            ui.fname[k][0] = 0;
            text_cat(ui.fname[k], c);
        }
    }
    if (ui.folder > (int)ui.nfold)
        ui.folder = 0;
    ui.nfl = 0;
    for (i = 0; i < n; i++)
        if (!lib_is_backtrack(i) && (ui.folder <= 0 || same_text(song_folder(i), ui.fname[ui.folder - 1])))
            ui.fl[ui.nfl++] = (uint16_t)i;
    ui.fl_seen = lib_changes + 1u;
    player_stats.folder = (uint32_t)(ui.folder + 1);
    player_stats.list_n = ui.nfl;
}

static void input_list(uint32_t down)
{
    int32_t d = plat_enc(EN_SELECT), n;
    if (ui.fl_seen != lib_changes + 1u)
        folders_build();
    if (ui.folder < 0) {                 /* the folders: All songs, then the categories */
        n = (int32_t)ui.nfold + 1;
        if (d)
            ui.fsel = (uint32_t)clamp((int32_t)ui.fsel + d, 0, n - 1);
        if ((down >> B_PLAY & 1u) || (down >> B_EDIT & 1u)) {
            ui.folder = (int)ui.fsel;
            ui.sel = ui.top = 0;
            folders_build();
            ui.redraw = 1;
        }
        if (down >> B_HOME & 1u)         /* M10.1: the first screen of SONG LEARN: HOME goes to the mode screen */
            mode_leave();
        return;
    }
    n = (int32_t)ui.nfl;
    if (d && n) {
        int32_t s = (int32_t)ui.sel + d;
        ui.sel = (uint32_t)clamp(s, 0, n - 1);
    }
    if (((down >> B_PLAY & 1u) || (down >> B_EDIT & 1u)) && ui.sel < (uint32_t)n)
        open_song(ui.fl[ui.sel]);
    else if ((down >> B_HOME & 1u) && lib_song_count()) {   /* HOME: up to the folders (M9: songs, not backtracks) */
        ui.fsel = (uint32_t)ui.folder;
        ui.folder = -1;
        ui.top = 0;
        ui.redraw = 1;
    } else if (down >> B_HOME & 1u) {    /* (M10.1: no songs: the folders are empty: the mode screen) */
        mode_leave();
    }
    player_stats.folder = (uint32_t)(ui.folder + 1);
}

static void say_from(void)
{
    char t[24] = "BAR ";
    num_cat(t, ui.from_bar + ui.s->first_bar_number);
    uint32_t i;
    const char *sec = "";
    app_say("START", t);
    for (i = 0; i < ui.s->nsect; i++)    /* M10.2: a section's first bar: the section's name under it */
        if (song_section_first(ui.s, i) == ui.from_bar)
            sec = song_section_name(ui.s, i);
    vpop_show("Start", t, sec, 0);       /* M10.1: the pop-up "Start" / "BAR 9" */
    if (vpop.open && sec[0]) {
        vpop.big_sub = 1;                /* M10.3 */
        player_stats.big_sub = 1;
    }
}

/* ------------------------------------------------------------------ M10.1: SONG LEARN's melody sound --- */
/* K4 steps SONG (the song's own instrument, EAS) and then the preset list (stock, then the saved ones). A preset plays the MELODY layer
 * on vsynth (the player sends that layer's notes to vsynth_key, player_melody_synth); the other layers stay on EAS (synth_band). The
 * synth's memory is the top of the song buffer, as in BACKTRACK; a song whose levels do not fit beside it keeps its own instrument.
 * The choice is a reference (pl_ref), kept until a song is opened. */
#define LM_SONG 0xFFFFu
#define LM_ROOM (LIB_DEC_BYTES - VS_MEM_BT)    /* the song's part of the buffer while the synth runs (112,896) */
#define LM_VOICES 4u
static struct { uint8_t on; uint16_t ref; } lm = {0, LM_SONG};

static int lm_active(void) { return lm.on; }

/* M10.3: the keys' own sound while the melody sound is SONG: the open song's melody layer program, else the keys' default (Electric Piano 1) */
static void keys_program(int song)
{
    uint32_t p = GM_ELECTRIC_PIANO_1;
    if (song && ui.s && ui.s->layer[L_MELODY].variants && ui.s->layer[L_MELODY].program != 0xFFu)
        p = ui.s->layer[L_MELODY].program;
    synth_midi(0xC0u | SYNTH_KEYS_CH, (uint8_t)p, 0);
    player_stats.keys_prog = p;
}

/* the melody layer's notes to vsynth (on) or EAS (off): the layer held meanwhile (its notes released the old way) */
static void lm_route(int on)
{
    uint32_t t = plat_ms();
    player_hold(1u << L_MELODY);
    while (!(player_held() >> L_MELODY & 1u) && plat_ms() - t < 200u)
        ;
    player_melody_synth(on);
    player_hold(0);
}

static void lm_off(void)
{
    uint32_t t;
    player_stats.mel_sound = 0;
    if (!lm.on) {
        lm.ref = LM_SONG;
        return;
    }
    if (current_i >= 0)
        lm_route(0);
    else
        player_melody_synth(0);
    synth_band(0);
    vsynth_stop();
    t = plat_ms();
    while (plat_ms() - t < 20u)
        ;
    lib_room = sizeof dec;
    lm.on = 0;
    lm.ref = LM_SONG;
}

/* do the levels of the open song leave the synth's memory free? */
static int lm_fits(void)
{
    uint32_t l, at = 0;
    if (current_i < 0 || !current.fmt2)
        return 0;
    for (l = 0; l < SONG_LAYERS; l++)
        at += current.layer[l].need * 8u;
    return at <= LM_ROOM;
}

static void lm_set(uint32_t ref)
{
    if (ref == LM_SONG) {
        lm_off();
        player_stats.mel_sound = 0;
        keys_program(1);                 /* M10.3: K4 back to SONG: the keys follow the song's melody instrument */
        return;
    }
    if (!lm.on) {
        if (!lm_fits()) {
            lm.ref = LM_SONG;
            return;
        }
        lib_room = LM_ROOM;
        vsynth_start(dec + LM_ROOM, sizeof dec - LM_ROOM);
        vsynth_voices(LM_VOICES);
        synth_band(1);
        lm_route(1);
        lm.on = 1;
    }
    vsynth_load(pl_get(pl_index_of_ref(ref)));
    vsynth_set(VP_ARP_ON, 0);            /* (the melody is played by the song: no arpeggiator over it) */
    vsynth_tempo(player_tempo_of() < 60u ? 60u : player_tempo_of() > 200u ? 200u : player_tempo_of());
    lm.ref = (uint16_t)ref;
}

static void lm_step(int32_t d)
{
    int32_t n = (int32_t)pl_count() + 1;    /* 0: SONG, then the list */
    int32_t p = lm.ref == LM_SONG ? 0 : (int32_t)pl_index_of_ref(lm.ref) + 1, want;
    char v[24] = "";
    p = ((p + d) % n + n) % n;          /* (turned fast: several steps at once, as BACKTRACK's K4) */
    want = p;
    lm_set(p ? pl_ref((uint32_t)p - 1u) : LM_SONG);
    if (lm.ref == LM_SONG)
        text_cat(v, "SONG");
    else
        pl_label(pl_index_of_ref(lm.ref), v);
    vpop_show("Melody Sound", v, want && lm.ref == LM_SONG ? "no room: keeps SONG" : "", 0);
    player_stats.mel_sound = lm.ref == LM_SONG ? 0u : 1u + pl_index_of_ref(lm.ref);
}

static void input_play(uint32_t down, uint32_t now)
{
    uint32_t l;
    int32_t d;
    int st = player_state();
    if (st != PL_PAUSED)
        ui.paused_start = 0;             /* (M10.2: set by an ALGO start change while paused; any start or stop clears it) */
    if (down >> B_PLAY & 1u) {
        if (st == PL_STOPPED) {
            player_start(start_point(), player_count_in_of());
        } else if (st == PL_PAUSED && ui.paused_start) {   /* M10.2: ALGO moved the start while paused: from the new start */
            ui.paused_start = 0;
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
        if (bt.on && st != PL_STOPPED)   /* M10.3: BACKTRACK, playing / count-in / paused: stop, then SETUP in the same press */
            player_play(0);
        if (st == PL_STOPPED || bt.on) {
            ui.view = bt.on ? V_SETUP : V_LIST;   /* (M8: BACKTRACK: back to SETUP) */
            if (!bt.on)
                keys_program(0);         /* M10.3: the song is closed: the keys' default program */
            if (bt.on)
                lib_release();
            ui.redraw = 1;
            return;
        }
        player_play(0);                  /* stopped: back to the start point */
    }
    for (l = 0; l < SONG_LAYERS; l++) {
        int has = ui.s->layer[l].variants || (bt.on && l == L_MELODY);   /* (M8: BACKTRACK's melody: yours) */
        if ((down >> LAYER_BUTTON[l] & 1u) && has) {   /* M10.1 (D066): a press, acted on at once (no hold): a layer that is not your
                                          * part becomes it (its volume stays); on your part the volume goes FULL -> QUIET -> MUTED */
            static const char *const LT[SONG_LAYERS] = {"Drums", "Bass", "Harmony", "Melody"};
            if (yours() != l) {
                ui.pv_of[yours()] = (uint8_t)ui.pview;
                ui.mine = l;
                ui.pview = (int)view_ok(l, ui.pv_of[l]);
                player_stats.view = (uint32_t)ui.pview;
                ui.redraw = 1;
                app_say(LAYER_NAME[l], "TRACK SELECTED");
                vpop_show("Track Selected", LT[l], bt.on && l == L_MELODY ? "" : "(press again to control volume level)", 0);   /* M10.2 */
                player_stats.part = l;   /* (the frame, which sets it, does not run under a pop-up) */
            } else if (ui.s->layer[l].variants) {   /* (BACKTRACK's melody is the keys: no volume) */
                char tt[24] = "";
                ui.vol[l] = (uint8_t)((ui.vol[l] + 1u) % 3u);
                if (ui.vol[l] == VOL_MUTED)
                    ui.muted |= 1u << l;
                else
                    ui.muted &= ~(1u << l);
                player_mute(ui.muted);
                app_say(LAYER_NAME[l], ui.vol[l] == VOL_FULL ? "FULL" : ui.vol[l] == VOL_QUIET ? "QUIET" : "MUTED");
                text_cat(tt, LT[l]);
                text_cat(tt, " Volume");
                vpop_show(tt, ui.vol[l] == VOL_FULL ? "FULL" : ui.vol[l] == VOL_QUIET ? "QUIET" : "MUTED", "", 0);
                player_stats.vols = (uint32_t)ui.vol[0] | (uint32_t)ui.vol[1] << 2 | (uint32_t)ui.vol[2] << 4 | (uint32_t)ui.vol[3] << 6;
            } else {
                vpop_show("Track Selected", LT[l], bt.on && l == L_MELODY ? "" : "(press again to control volume level)", 0);
                player_stats.part = l;
            }
        }
        d = bt.on ? 0 : plat_enc(LAYER_ENC[l]);   /* (M9: BACKTRACK's knobs are bt.c's alone) */
        if (d && l == L_MELODY && ui.s->layer[l].variants == 1u) {   /* M10.1: K4 = Melody Sound (a melody has one level; a song with
                                          * more would keep the level job) */
            lm_step(d);
            d = 0;
        }
        if (d && ui.s->layer[l].variants) {
            int32_t v = clamp((int32_t)player_variant_of(l) + (d > 0 ? 1 : -1), 1, ui.s->layer[l].variants);
            char t[8] = "LEVEL ";
            lib_level(l, (uint32_t)v);       /* (2.0: built now; the other layers play on) */
            t[6] = (char)('0' + v);
            t[7] = 0;
            app_say(LAYER_NAME[l], t);
            {                            /* M10.1: the value pop-up "Drums Level" / "2 of 3" */
                static const char *const TITLE[SONG_LAYERS] = {"Drums Complexity Level", "Bass Complexity Level", "Harmony Complexity Level", "Melody Complexity Level"};
                char pv[16] = "";
                num_cat(pv, (uint32_t)v);
                text_cat(pv, " of ");
                num_cat(pv, ui.s->layer[l].variants);
                vpop_show(TITLE[l], pv, "", 0);
            }
        }
    }
    if (ui.tempomode_until && now >= ui.tempomode_until) {
        ui.tempomode_until = 0;
        ui.sh_legend = 0;
    }
    if (down >> B_REC & 1u) {            /* M6: the TEMPO mode on / off (in the zooming views SELECT zooms) */
        ui.tempomode_until = ui.tempomode_until ? 0 : now + KEYMODE_MS;
        ui.sh_legend = 0;
        ui.sh_top_key = 0xFFFFFFFFu;
        if (ui.tempomode_until) {
            char t[16] = "";
            num_cat(t, player_tempo_of());
            text_cat(t, " BPM");
            app_say("TEMPO: SELECT", t);
        }
    }
    if (down >> B_EDIT & 1u) {           /* the next of your part's views (VIEWS), the hidden ones skipped (M4) */
        const uint8_t *v = views_of(yours());
        uint32_t i = 0, n = 0, k;
        while (v[n] != PV_N)
            n++;
        while (i < n && v[i] != ui.pview)
            i++;
        for (k = 1; k <= n; k++) {
            uint32_t c = v[(i + k) % n];
            if (!view_hidden(yours(), c)) {
                ui.pview = (int)c;
                break;
            }
        }
        ui.pv_of[yours()] = (uint8_t)ui.pview;
        player_stats.view = (uint32_t)ui.pview;
        ui.redraw = 1;
        app_say("VIEW", view_name((uint32_t)ui.pview));
    }
    d = plat_enc(EN_SELECT);
    if (d && !ui.tempomode_until && view_zooms(ui.pview)) {   /* M6: the scrolling and staff views: zoom */
        uint32_t z = (uint32_t)clamp((int32_t)ui.zoom + (d > 0 ? 1 : -1), 0,
                                     (int32_t)(view_staff(ui.pview) ? ZOOM_STAFF_MAX : ZOOMS - 1u));
        char t[24] = "";
        if (z == ui.zoom && view_staff(ui.pview) && ui.zoom > ZOOM_STAFF_MAX)
            z = ZOOM_STAFF_MAX;
        ui.zoom = z;
        scroll_setup(ui.pview);
        num_cat(t, player_stats.zoom * 10u / bar_ticks() / 10u);
        text_cat(t, ".");
        num_cat(t, player_stats.zoom * 10u / bar_ticks() % 10u);
        text_cat(t, " BARS");
        app_say(d > 0 ? "ZOOM OUT" : "ZOOM IN", t);
        ui.redraw = 1;
        d = 0;
    }
    if (d && ui.tempomode_until)
        ui.tempomode_until = now + KEYMODE_MS;
    if (d) {                             /* tempo: 1 BPM a click; turned quickly, 5 */
        int32_t step = (d > 1 || d < -1 || now - ui.tempo_ms < 90u) ? 5 : 1;
        ui.tempo_ms = now;
        player_tempo((uint32_t)clamp((int32_t)player_tempo_of() + step * d, PL_BPM_MIN, PL_BPM_MAX));
        ui.sh_top_key = 0xFFFFFFFFu;
        if (lm_active())                 /* M10.1: the melody's synth follows (its delay and arpeggiator times) */
            vsynth_tempo(player_tempo_of() < 60u ? 60u : player_tempo_of() > 200u ? 200u : player_tempo_of());
        if (!bt.on) {                    /* M10.1: the pop-up "Tempo" in SONG LEARN (BACKTRACK's is bt_vp_select) */
            char pv[16] = "";
            num_cat(pv, player_tempo_of());
            text_cat(pv, " BPM");
            vpop_show("Tempo", pv, "", 0);
        }
        if (bt.on) {                     /* M8: the synth's arp and delay follow; SETUP keeps it */
            bt.bpm = player_tempo_of();
            bt.bpm_turned = 1;
            vsynth_tempo(bt.bpm < 60u ? 60u : bt.bpm > 200u ? 200u : bt.bpm);
            bt_vp_select();              /* M10: the pop-up "Tempo" */
        }
    }
    d = bt.on ? 0 : plat_enc(EN_PRESET);   /* (M9: PRESETS is the key, bt.c's) */
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
    if (d) {                             /* M10.2: PRESETS is the key (as in BACKTRACK): a semitone a click, all twelve keys, the nearest
                                          * way (-5..+6); the KEY mode (SAVE, SELECT) is gone */
        char t[24] = "";
        ui.key = ((ui.key + d) % 12 + 12 + 5) % 12 - 5;
        player_transpose(ui.key);
        ui.redraw = 1;
        text_cat(t, spell(ui.s->key));
        text_cat(t, ui.key ? " (" : " (song key)");
        if (ui.key) {
            snum_cat(t, ui.key);
            text_cat(t, ")");
        }
        app_say("KEY", t);
        vpop_show("Key", t, "", 0);
    }
    d = bt.on ? 0 : plat_enc(EN_ALGO);     /* (M9: ALGO is the structure, bt.c's) */
    if (d && ui.loop_step) {             /* setting the loop: A or B, by bar (B at or after A) */
        if (ui.loop_step == 1)
            ui.a_bar = (uint32_t)clamp((int32_t)ui.a_bar + d, (int32_t)first_bar(), (int32_t)last_bar());
        else
            ui.b_bar = (uint32_t)clamp((int32_t)ui.b_bar + d, (int32_t)ui.a_bar, (int32_t)last_bar());
        ui.sh_status = ui.sh_strip = 0xFFFFFFFFu;
        d = 0;
    }
    if (d && ui.loop != LOOP_BARS) {     /* the start point, by bar */
        set_from_bar((uint32_t)clamp((int32_t)ui.from_bar + d, (int32_t)first_bar(), (int32_t)last_bar()));
        if (st == PL_PAUSED)             /* M10.2: PLAY goes on from the new start, not from where it paused */
            ui.paused_start = 1;
        say_from();
        ui.sh_status = 0xFFFFFFFFu;
    } else if (d) {
        app_say("LOOP ON", "SEQ: OFF FIRST");
    }
    if (down >> B_ARP & 1u) {
        player_count_in(!player_count_in_of());
        app_say("COUNT-IN", player_count_in_of() ? "ON" : "OFF");
    }
    if (down >> B_SEQ & 1u) {            /* the loop, step by step: SEQ the whole song (M4); SEQ sets A, SEQ sets B
                                          * (A-B on); SEQ again: off */
        char t[24] = "";
        if (ui.loop == LOOP_BARS) {
            set_loop(LOOP_OFF);
            app_say("LOOP", "OFF");
        } else if (ui.loop == LOOP_OFF && ui.loop_step == 0) {
            set_loop(LOOP_SONG);
            app_say("LOOP", "WHOLE SONG");
        } else if (ui.loop_step == 0) {  /* (from the song loop) A starts at the bar playing or the start point */
            set_loop(LOOP_OFF);
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
            set_loop(LOOP_BARS);
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

/* ------------------------------------------------------------ M7: the NOT IMPLEMENTED pop-up (D044) --- */
/* A button with no job on the screen shown opens a small pop-up; any button closes it (that press does nothing else,
 * nor its release), and so does POPUP_MS. Never for a button held since power-on; never on the WIPE question, the mode
 * screen, the splash or the hint (they have a job for every button); HOME held 2 s works with it open; the keys keep
 * playing; the encoders never open it (while it is open their turns are dropped). docs/controls.md, "M7", lists each
 * screen's free buttons (the census). */
#define POPUP_MS 2500u
#define ALL_BUTTONS ((1u << NB) - 1u)

/* the learning screens' buttons with a job (the rest are free) */
static uint32_t learn_jobs(void)
{
    uint32_t j = 1u << B_OCTDN | 1u << B_OCTUP | 1u << B_GLO, l;   /* everywhere: the octave, the help */
    switch (ui.view) {
    case V_LIST:                         /* PLAY / EDIT open; HOME: up to the folders (the song list only) */
        j |= 1u << B_PLAY | 1u << B_EDIT;
        j |= 1u << B_HOME;               /* (M10.1: up to the folders; from the folders: the mode screen) */
        return j;
    case V_PLAY:                         /* every button, but the layer buttons of layers the song does not have */
        j = ALL_BUTTONS & ~(1u << B_SAVE);   /* (M10.2: SAVE has no job on the play screen: the controls pop-up) */
        /* (M10.3: BACKTRACK keeps SEQ: it steps the band kit, bt_play_input) */
        for (l = 0; l < SONG_LAYERS; l++)
            if (!ui.s->layer[l].variants && !(bt.on && l == L_MELODY))   /* (M8: BACKTRACK's melody: yours) */
                j &= ~(1u << LAYER_BUTTON[l]);
        return j;
    case V_SETUP:                        /* M8: PLAY starts (with backtracks); HOME: held only; M10: SEQ steps the band kit */
        return j | (bt.n ? 1u << B_PLAY : 0u) | 1u << B_SEQ | 1u << B_HOME;   /* (M10.1: HOME: the mode screen) */
    case V_SETTINGS:
    case V_VIEWSET:                      /* PLAY / EDIT change, GLO / HOME back */
        return j | 1u << B_PLAY | 1u << B_EDIT | 1u << B_HOME;
    default:                             /* the help (any button: back), the hint, WIPE, the mode screen: all */
        return ALL_BUTTONS;
    }
}

static void popup_open(uint32_t b)
{
    if (!SV(S_HINT))                     /* M10.2: Pop-ups OFF: a button with no job does nothing */
        return;
    ui.popup = 1;
    ui.popup_until = plat_ms() + POPUP_MS;
    player_stats.popups++;
    player_stats.popup_open = 1;
    player_stats.popup_btn = b;
}

/* free: the buttons with no job now. 1: the pop-up has this input (open now, or just closed) */
static int popup_input(uint32_t down, uint32_t free, uint32_t now)
{
    uint32_t b, e;
    if (ui.popup) {
        for (e = 0; e < NE; e++)
            (void)plat_enc(e);           /* (dropped) */
        if (down || (int32_t)(now - ui.popup_until) >= 0) {
            ui.popup = 0;
            player_stats.popup_open = 0;
            ui.redraw = 1;
            ui.hold_done = 0xFu;         /* (a layer button that closed it: neither a hold nor a press) */
            ui.closed |= down;           /* (its release does nothing either) */
            ui.sh_status = 0xFFFFFFFFu;
        }
        ui.released &= ~ui.closed;
        ui.closed &= ui.buttons;
        return 1;
    }
    ui.released &= ~ui.closed;
    ui.closed &= ui.buttons;
    free &= ui.seen_up;
    if (down & free & 1u << B_HOME)      /* (HOME free: on release, a short press that began on this screen; a hold is
                                          * the mode screen's) */
        ui.home_free = 1;
    if (ui.released & 1u << B_HOME) {
        int f = ui.home_free && (free & 1u << B_HOME);
        ui.home_free = 0;
        if (f) {
            popup_open(B_HOME);
            return 1;
        }
    }
    free &= ~(1u << B_HOME);
    for (b = 0; b < NB; b++)
        if (down & free & 1u << b) {
            popup_open(b);
            return 1;
        }
    return 0;
}

static void draw_popup(void)
{
    cv_begin(200, 64, C_BLACK);
    frame_rect(0, 0, 200, 64, C_AMB);
    text_in(0, 200, 10, &FONT_B, "NOT IMPLEMENTED", C_WHITE);
    text_in(0, 200, 38, &FONT_XS, "any button: close", C_GRAY);
    cv_blit(20, 88);
}

/* the value pop-up opened (or its text changed): title, value, an optional second line (FONT_XS) and a MAJOR / MINOR badge */
static void vpop_show(const char *title, const char *value, const char *sub, uint32_t badge)
{
    if (SV(S_HINT))                      /* M10.2: the setting "Pop-ups" (S_HINT, the old "Controls hint" slot) */
        vpop_keep(title, value, sub, badge);
}

static void vpop_keep(const char *title, const char *value, const char *sub, uint32_t badge)
{
    uint32_t now = plat_ms(), i;
    if (!vpop.open) {
        vpop.since = now;
        player_stats.vp_count++;
    }
    vpop.open = 1;
    vpop.dirty = 1;
    vpop.big_sub = 0;                    /* (say_from sets it after this) */
    player_stats.big_sub = 0;
    vpop.badge = (uint8_t)badge;
    vpop.until = now + VPOP_MS;
    for (i = 0; i < sizeof vpop.title - 1u && title[i]; i++)
        vpop.title[i] = title[i];
    vpop.title[i] = 0;
    for (i = 0; i < sizeof vpop.value - 1u && value[i]; i++)
        vpop.value[i] = value[i];
    vpop.value[i] = 0;
    for (i = 0; i < sizeof vpop.sub - 1u && sub && sub[i]; i++)
        vpop.sub[i] = sub[i];
    vpop.sub[i] = 0;
    player_stats.vp_open = 1;
    vpop_text(player_stats.vp_title, vpop.title, sizeof player_stats.vp_title);
    vpop_text(player_stats.vp_value, vpop.value, sizeof player_stats.vp_value);
    vpop_text(player_stats.vp_sub, vpop.sub, sizeof player_stats.vp_sub);
}

static void vpop_close(int by_button)
{
    if (!vpop.open)
        return;
    vpop.open = 0;
    player_stats.vp_open = 0;
    player_stats.vp_close_ms = plat_ms() - vpop.since;
    player_stats.vp_btn_closed = (uint32_t)by_button;
    ui.redraw = 1;                       /* (the screen under it is drawn again) */
    ui.sh_status = 0xFFFFFFFFu;
}

static void draw_vpop(void)
{
    const felucca_font_t *f = text_w(&FONT_M, vpop.value) <= 196 ? &FONT_M : text_w(&FONT_B, vpop.value) <= 196 ? &FONT_B :
                              &FONT_XS;
    int32_t y = 10;
    cv_begin(216, 104, C_BLACK);
    frame_rect(0, 0, 216, 104, C_AMB);
    text_in(0, 216, y + (text_w(&FONT_XS, vpop.title) <= 208 ? 0 : 2), text_w(&FONT_XS, vpop.title) <= 208 ? &FONT_XS : &FONT_T,
            vpop.title, C_GRAY);   /* (M10.2: a title that does not fit goes one font smaller, never cut) */
    y += 22;
    text_in(0, 216, y, f, vpop.value, C_WHITE);
    y += f->h + 8;
    if (vpop.badge) {
        const char *t = vpop.badge == 2u ? "MINOR" : "MAJOR";
        int32_t w = text_w(&FONT_XS, t) + 8;
        cv_rect((216 - w) / 2, y, w, 14, vpop.badge == 2u ? COL_MINOR : C_HI);
        cv_text((216 - w) / 2 + 4, y + 1, &FONT_XS, t, C_BLACK);
    }
    if (vpop.sub[0] && vpop.big_sub)     /* M10.3: the Start pop-up's section name: FONT_B, white, 6 px under the value */
        text_in(0, 216, 32 + f->h + 6, text_w(&FONT_B, vpop.sub) <= 208 ? &FONT_B : &FONT_XS, vpop.sub, C_WHITE);
    else if (vpop.sub[0])
        text_in(0, 216, 84, text_w(&FONT_XS, vpop.sub) <= 208 ? &FONT_XS : &FONT_T, vpop.sub, C_DIM);
    cv_blit(12, 68);
}

#include "synthui.c"
#include "bt.c"

/* how long since a hold began at t (t = now | 1u, so 0 means "not held"; t can be 1 ms ahead of now: 0, not a wrap) */
static uint32_t held_ms(uint32_t t, uint32_t now)
{
    return (int32_t)(now - t) > 0 ? now - t : 0u;
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), down = btn & ~ui.buttons, k, now = plat_ms();
    if (ui.view == V_RESET)              /* M10.1: after a FACTORY RESET: switch off and on (nothing is read) */
        return;
    ui.seen_up |= ~btn & ALL_BUTTONS;    /* M7: a button held since power-on never opens the pop-up */
    /* M10.1: HOME acts when it is RELEASED, as a tap (under 2 s): a press that starts the 2 s hold never acts as a single press first */
    if (down >> B_HOME & 1u) {
        ui.home_tap = ui.home_armed && !ui.splash;
        down &= ~(1u << B_HOME);
    }
    if ((ui.buttons & ~btn) >> B_HOME & 1u) {
        if (ui.home_tap)
            down |= 1u << B_HOME;
        ui.home_tap = 0;
    }
    if (!(btn >> B_HOME & 1u)) {         /* M6: HOME held 2 s, from any screen (not the WIPE question): the mode
                                          * screen. A HOME held since power-on (GLO + HOME: WIPE) never counts */
        ui.home_t = 0;
        ui.home_armed = 1;
    } else if (!ui.home_t) {
        ui.home_t = now | 1u;
    } else if (ui.home_armed && !ui.splash && ui.view != V_MODE && ui.view != V_WIPE &&
               held_ms(ui.home_t, now) >= HOME_HOLD_MS) {
        ui.home_armed = 0;               /* (once a hold) */
        ui.home_tap = 0;
        if (ui.view == V_SETTINGS || ui.view == V_VIEWSET)
            settings_save();
        mode_leave();
        ui.keys = keys;
        ui.buttons = btn;
        return;
    }
    if (!ui.splash && (ui.view == V_MODE || ui.view == V_SYNTH)) {   /* M5: the mode screen, SYNTH mode */
        if (vpop.open && (down || (int32_t)(now - vpop.until) >= 0))   /* (M10: "SAVED U05" closes like BACKTRACK's) */
            vpop_close(down != 0);
        ui.released = ui.buttons & ~btn;
        if (ui.view == V_MODE)
            mode_input(keys, btn, down);
        else
            synth_input(keys, btn, down, now);
        ui.keys = keys;
        ui.buttons = btn;
        return;
    }
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            uint8_t note = (uint8_t)(KEY_LOW + (int32_t)k + 12 * ui.oct);
            ui.key_note[k] = note;
            ui.key_route[k] = (uint8_t)(bt.on || lm_active());   /* M10.3: SONG LEARN's melody preset plays on the keys too */
            player_stats.key_route = ui.key_route[k];
            if (ui.key_route[k])         /* M8: BACKTRACK: the melody sound (the synth, 4 voices) */
                vsynth_key(note, 1);
            else
                synth_midi(0x90u | SYNTH_KEYS_CH, note, (uint8_t)VELOCITY);
            player_stats.key_note_last = note;
        } else if (!(keys & bit) && (ui.keys & bit)) {
            if (ui.key_route[k])         /* (released the way it was pressed) */
                vsynth_key(ui.key_note[k], 0);
            else
                synth_midi(0x80u | SYNTH_KEYS_CH, ui.key_note[k], 0);
        }
    }
    ui.keys = keys;
    ui.released = ui.buttons & ~btn;
    ui.buttons = btn;
    if (ui.splash) {                     /* the splash is still up: only the keys */
        lights();
        return;
    }
    if (vpop.open && (down || (int32_t)(now - vpop.until) >= 0))   /* M10: closed by a press (which then does its job) or by time */
        vpop_close(down != 0);
    if (ui.view != V_WIPE && popup_input(down, ALL_BUTTONS & ~learn_jobs(), now)) {   /* M7 */
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
    if (ui.view == V_VIEWSET) {          /* SELECT a view; ALGORITHM, PLAY or EDIT shows / hides it; GLO, HOME: back */
        int32_t d = plat_enc(EN_SELECT), v = plat_enc(EN_ALGO);
        uint32_t nrows = 0, l, i, rl = 0, rv = 0, shown = 0;
        for (l = 0; l < SONG_LAYERS; l++)
            for (i = 0; VIEWS[l][i] != PV_N; i++, nrows++)
                if (nrows == ui.vs_row)
                    rl = l, rv = VIEWS[l][i];
        if (d) {
            ui.vs_row = (uint32_t)clamp((int32_t)ui.vs_row + d, 0, (int32_t)nrows - 1);
            ui.redraw = 1;
        }
        if ((down & (1u << B_PLAY | 1u << B_EDIT)) || v) {
            for (i = 0; VIEWS[rl][i] != PV_N; i++)
                shown += !view_hidden(rl, VIEWS[rl][i]);
            if (!view_hidden(rl, rv) && shown <= 1u) {
                app_say("ONE VIEW", "MUST STAY");   /* each layer keeps at least one */
            } else {
                view_toggle(rl, rv);
                ui.set_dirty = 1;
            }
            ui.redraw = 1;
        }
        if (down & (1u << B_GLO | 1u << B_HOME)) {
            ui.view = V_SETTINGS;
            ui.redraw = 1;
        }
        lights();
        return;
    }
    if (ui.view == V_WIPE) {             /* M5.1: PLAY held 2 s wipes; HOME (or any other button) keeps the songs */
        if (ui.wipe_done) {
            if (down) {
                ui.view = ui.wipe_back;
                ui.redraw = 1;
                lcd_fill(0, 0, 240, 240, C_BLACK);
            }
        } else if ((btn >> B_PLAY & 1u) && ui.wipe_armed) {
            if (!ui.wipe_t0)
                ui.wipe_t0 = now | 1u;
            else if (held_ms(ui.wipe_t0, now) >= WIPE_HOLD_MS) {
                int r = lib_wipe();
                ui.wipe_done = 1u + (uint32_t)(r == STORE_OK ? 0 : 1);
                ui.wipe_t0 = 0;
                ui.view = V_WIPE;          /* (lib_wipe's progress screen moved it) */
                app_say(r == STORE_OK ? "SONG STORE WIPED" : "NOT WIPED", r == STORE_OK ? "built-in songs only" :
                        store_error_name(r));
                ui.redraw = 1;
            }
        } else {
            ui.wipe_t0 = 0;
            ui.wipe_armed |= !(btn >> B_PLAY & 1u);
            if (down & ~(1u << B_PLAY)) {
                ui.view = ui.wipe_back;
                ui.redraw = 1;
                lcd_fill(0, 0, 240, 240, C_BLACK);
            }
        }
        ui.buttons = btn;
        ui.keys = keys;
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
        if (v && ui.set_row == S_VIEWS) {   /* the Views screen */
            ui.view = V_VIEWSET;
            ui.vs_row = 0;
            ui.redraw = 1;
            v = 0;
        }
        if (v && ui.set_row == S_MODE) {    /* M6: the mode screen */
            settings_save();
            mode_leave();
            return;
        }
        if (v && ui.set_row == S_WIPE) {    /* M5.1: WIPE SONG STORE, asked first */
            wipe_open(V_SETTINGS);
            v = 0;
        }
        if (v) {
            uint32_t r = ui.set_row, n = SET_COUNT[r];
            SV(r) = (uint8_t)(((int32_t)SV(r) + v % (int32_t)n + (int32_t)n) % (int32_t)n);
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
    else if (ui.view == V_SETUP)         /* M8 */
        bt_setup_input(down);
    else {
        if (bt.on) {                     /* M9: BACKTRACK's own knobs (K1-K4, PRESETS, ALGO); SEQ and SAVE: the pop-up */
            bt_play_input(down);
            down &= ~(1u << B_SEQ | 1u << B_SAVE);   /* (M10.3: SEQ was taken by bt_play_input: the kit) */
        }
        input_play(down, now);
    }
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
    vpop_text(player_stats.legend_now, t, sizeof player_stats.legend_now);   /* M10.1: for the tests */
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
    char f[24] = "";
    cv_begin(240, H_TOP, C_BLACK);
    if (ui.folder < 0)
        text_cat(f, "FOLDERS");
    else if (ui.folder == 0)
        text_cat(f, "ALL SONGS");
    else
        text_cat(f, ui.fname[ui.folder - 1]);
    cv_text(6, 4, &FONT_B, f, C_WHITE);
    cv_text(234 - text_w(&FONT_XS, "v" FW_VERSION), 6, &FONT_XS, "v" FW_VERSION, C_DIM);
    if (plat_ms() < ui.msg_until) {
        char t[48] = "";
        text_cat(t, ui.msg[0]);
        text_cat(t, " ");
        text_cat(t, ui.msg[1]);
        cv_text(110, 6, &FONT_XS, t, C_AMB);
    } else if (!ui.flash_ok) {
        cv_text(110, 6, &FONT_XS, "RAM ONLY", C_AMB);
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
        text_in(0, 240, 56, &FONT_XS, "Please visit our website", C_GRAY);   /* M10.3 (D070 option 4) */
        text_in(0, 240, 72, &FONT_XS, "to download songs.", C_GRAY);
        text_in(0, 240, 130, &FONT_XS, "They stay here when it is off.", C_DIM);
    }
    cv_blit(0, H_TOP);
    draw_legend(Y_LEGEND, "GLO help|keys play along");
}

/* the folders: All songs (the count), then each category (its count) */
static void draw_folders(void)
{
    uint32_t r, i, k, n = ui.nfold + 1u, cnt;
    if (ui.fsel < ui.top)
        ui.top = ui.fsel;
    if (ui.fsel >= ui.top + LIST_ROWS)
        ui.top = ui.fsel - LIST_ROWS + 1u;
    draw_list_top();
    for (r = 0; r < LIST_ROWS; r++) {
        char t[24] = "";
        i = ui.top + r;
        cv_begin(240, LIST_ROW_H, C_BLACK);
        if (i < n) {
            if (i == ui.fsel)
                cv_rect(2, 1, 236, LIST_ROW_H - 2, C_LINE);
            for (k = 0, cnt = 0; k < lib_count(); k++)   /* (M9: the backtracks are not songs: folders_build skips them too) */
                cnt += !lib_is_backtrack(k) && (!i || same_text(song_folder(k), ui.fname[i - 1u]));
            if (i == ui.fsel)
                player_stats.fold_shown = cnt;
            frame_rect(10, 9, 16, 12, i == ui.fsel ? C_HI : C_DIM);   /* a folder */
            cv_rect(10, 7, 7, 3, i == ui.fsel ? C_HI : C_DIM);
            cv_text(34, 1, &FONT_B, i ? ui.fname[i - 1u] : "All songs", i == ui.fsel ? C_WHITE : C_GRAY);
            num_cat(t, cnt);
            text_cat(t, cnt == 1u ? " song" : " songs");
            cv_text(34, 19, &FONT_XS, t, i == ui.fsel ? C_HI : C_DIM);
        }
        cv_blit(0, H_TOP + (int32_t)r * LIST_ROW_H);
    }
    draw_legend(Y_LEGEND, "SELECT choose|PLAY open|GLO help");
}

static void draw_list(void)
{
    uint32_t r, n;
    if (ui.fl_seen != lib_changes + 1u)
        folders_build();
    if (ui.folder < 0) {
        draw_folders();
        return;
    }
    n = ui.nfl;
    if (ui.sel < ui.top)
        ui.top = ui.sel;
    {                                    /* M10.3: the footer (row n) must be fully visible when the cursor is on the last song */
        uint32_t last = ui.sel + (ui.sel + 1u == n ? 1u : 0u);
        if (last >= ui.top + LIST_ROWS)
            ui.top = last - LIST_ROWS + 1u;
    }
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
            uint32_t li = ui.fl[i];
            const struct lib_item *s = lib_peek(li, &err);
            if (sel)
                cv_rect(2, 1, 236, LIST_ROW_H - 2, C_LINE);
            if (s) {
                char t[40] = "";
                text_cat(t, s->key);
                text_cat(t, "   ");
                num_cat(t, s->beats_per_bar);
                text_cat(t, "/4   ");
                num_cat(t, s->bpm);
                text_cat(t, " BPM");
                cv_text(10, 1, &FONT_B, s->title, sel ? C_WHITE : C_GRAY);
                cv_text(10, 19, &FONT_XS, t, sel ? C_HI : C_DIM);
            } else {
                char t[48] = "";
                const struct lib_item *pk = lib_raw(order[li]);
                text_cat(t, lib_perr(order[li]) ? "(unreadable song)" : pk->title);
                cv_text(10, 1, &FONT_B, t, sel ? C_WHITE : C_GRAY);
                t[0] = 0;
                text_cat(t, "SONG ERROR: ");
                text_cat(t, song_error_name(err));
                text_cat(t, "  (send it again)");
                cv_text(10, 19, &FONT_XS, t, C_AMB);
            }
        }
        else if (i == n) {               /* M10.3: the footer: the cursor never lands on it (a pop-up it is not: Pop-ups OFF keeps it) */
            text_in(0, 240, 3, &FONT_XS, "Download and install more", C_GRAY);
            text_in(0, 240, 19, &FONT_XS, "songs from our website", C_GRAY);
        }
        cv_blit(0, H_TOP + (int32_t)r * LIST_ROW_H);
    }
    draw_legend(Y_LEGEND, "SELECT song|PLAY open|HOME folders");
}

/* -------------------------------------------------------------------------- help --- */
/* every control: the encoders (left of the screen on the panel) left, K1..K4 and the buttons (right of it) right;
 * what each does here (docs/controls.md). Two bands: the canvas holds 144 rows */
static void draw_help(void)
{
    static const char *const ENC[5][2] = {{"MASTER", "volume"}, {"SELECT", "tempo / zoom"},
                                          {"PRESETS", "key"}, {"ALGORITHM", "start: bar; loop A/B"},
                                          {"OCT- OCT+", "keys' octave"}};
    static const char *const ENC_LIST[5][2] = {{"MASTER", "volume"}, {"SELECT", "choose a song"},
                                               {"PRESETS", "-"}, {"ALGORITHM", "-"}, {"OCT- OCT+", "keys' octave"}};
    static const char *const BTN[10][2] = {{"K1-K4", "levels"}, {"FX..LFO", "your part, volume"},
                                          {"PLAY", "play / pause"}, {"HOME", "stop, list"}, {"SEQ", "loop: A, B, off"},
                                          {"ARP", "count-in"}, {"EDIT", "view"}, {"HOME", "held: mode"},
                                          {"", ""}, {"", ""}};   /* (M10.2: SAVE is gone from the list: it has no job here) */   /* (M6; the keys play along: plain) */
    static const char *const BTN_LIST[10][2] = {{"PLAY", "open"}, {"EDIT", "open"},
                                               {"HOME", "folders"}, {"keys", "play along"}, {"", ""},
                                               {"HOME", "held: mode"},
                                               {"", ""}, {"", ""}, {"", ""}, {"", ""}};   /* (M10.4: ten rows: M10.3 left the tenth a NULL string, a crash on this help) */
    static const char *const ENC_SETUP[5][2] = {{"SELECT", "tempo"}, {"PRESETS", "key"},
                                                {"ALGORITHM", "structure"}, {"MASTER", "volume"},
                                                {"OCT- OCT+", "keys' octave"}};   /* M9: BACKTRACK, the one knob map */
    static const char *const BTN_SETUP[10][2] = {{"K1", "progression"}, {"K2", "style"}, {"K3", "complexity"},
                                                {"K4", "melody sound"}, {"PLAY", "start"}, {"HOME", "held: mode"},
                                                {"keys", "melody sound"}, {"", ""}, {"", ""}, {"", ""}};
    static const char *const ENC_BT[5][2] = {{"SELECT", "tempo"}, {"PRESETS", "key (bar)"},
                                             {"ALGORITHM", "structure*"}, {"MASTER", "volume"},
                                             {"OCT- OCT+", "keys' octave"}};
    static const char *const BTN_BT[10][2] = {{"K1", "progression*"}, {"K2", "style*"}, {"K3", "complexity"},
                                              {"K4", "melody sound"}, {"PLAY", "play / pause"},
                                              {"HOME", "stop, setup"}, {"SEQ", "band kit"}, {"ARP", "count-in"},
                                              {"EDIT", "view"}, {"LFO", "held: melody"}};   /* M10.3: SEQ; ten rows, 15 px apart */
    const char *const (*e)[2] = ui.back == V_PLAY ? (bt.on ? ENC_BT : ENC) : ui.back == V_SETUP ? ENC_SETUP : ENC_LIST;
    const char *const (*b)[2] = ui.back == V_PLAY ? (bt.on ? BTN_BT : BTN) : ui.back == V_SETUP ? BTN_SETUP : BTN_LIST;
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
        for (r = 0; r < 10u; r++) {      /* the buttons, a line each (M10.3: ten rows, 15 px apart) */
            int32_t y = 30 + (int32_t)r * 15 - y0;
            cv_text(116, y, &FONT_XS, b[r][0], C_GRAY);
            cv_text(152, y, &FONT_XS, b[r][1], C_HI);
        }
        cv_text(116, 184 - y0, &FONT_XS, "GLO", C_GRAY);
        cv_text(152, 184 - y0, &FONT_XS, "again: settings", C_HI);
        if (bt.on && ui.back == V_PLAY)  /* M9: when the changes take effect */
            cv_text(6, 184 - y0, &FONT_XS, "* next phrase", C_DIM);

        if (band) {
            cv_rect(0, 200 - y0, 240, 1, C_LINE);
            num_cat(t, plat_cpu_pct());
            text_cat(t, "%   VOICES ");
            num_cat(t, player_voices());
            text_cat(t, "/");
            num_cat(t, player_voices_max());
            cv_text(6, 204 - y0, &FONT_XS, t, C_DIM);
            cv_text(234 - text_w(&FONT_XS, "v" FW_VERSION), 204 - y0, &FONT_XS, "v" FW_VERSION, C_DIM);
            cv_text(6, 220 - y0, &FONT_XS, ui.flash_ok ? APP_NAME : APP_NAME "  RAM ONLY", C_DIM);
        }
        cv_blit(0, (uint32_t)y0);
    }
}

/* ---------------------------------------------------------------------- settings --- */

/* M5.1: the WIPE SONG STORE screen (from the settings, or GLO + HOME held at power-on) */
static void wipe_open(int back)
{
    ui.wipe_back = back;
    ui.wipe_t0 = ui.wipe_done = ui.wipe_armed = 0;
    ui.buttons = plat_buttons();         /* (the buttons held now, as GLO + HOME at power-on: not a press) */
    ui.view = V_WIPE;
    ui.redraw = 1;
}

static void draw_wipe(uint32_t now)
{
    char t[24] = "";
    lcd_fill(0, 0, 240, 240, C_BLACK);
    cv_begin(240, 150, C_BLACK);
    frame_rect(8, 4, 224, 142, ui.wipe_done == 1u ? C_HI : C_AMB);
    text_in(0, 240, 14, &FONT_B, ui.wipe_done == 1u ? "SONG STORE WIPED" : ui.wipe_done ? "NOT WIPED" :
            "WIPE SONG STORE?", C_WHITE);
    if (ui.wipe_done) {
        text_in(0, 240, 46, &FONT_XS, ui.wipe_done == 1u ? "Only the built-in songs now." : "The flash refused an erase.",
                C_GRAY);
        text_in(0, 240, 66, &FONT_XS, "Send them again from the laptop:", C_GRAY);
        text_in(0, 240, 86, &FONT_XS, "tools/fm1song.sh send songs/*/", C_HI);
    } else {
        text_in(0, 240, 46, &FONT_XS, "Every song on this FM-1 is erased.", C_GRAY);
        text_in(0, 240, 64, &FONT_XS, "Settings and firmware stay.", C_GRAY);
        text_in(0, 240, 82, &FONT_XS, "(the laptop can send them again)", C_GRAY);
        if (ui.wipe_t0) {
            uint32_t h = held_ms(ui.wipe_t0, now);
            num_cat(t, (WIPE_HOLD_MS - (h > WIPE_HOLD_MS ? WIPE_HOLD_MS : h) + 999u) / 1000u);
            text_cat(t, " s ...");
        }
        text_in(0, 240, 112, &FONT_B, ui.wipe_t0 ? t : "hold PLAY 2 s to wipe", ui.wipe_t0 ? C_AMB : C_HI);
    }
    cv_blit(0, 40);
    draw_legend(Y_LEGEND, ui.wipe_done ? "any button|back" : "PLAY hold 2 s: wipe|HOME: keep the songs");
}

/* the Views screen (M4): every part's views, each SHOWN or HIDDEN from EDIT; 8 rows a page */
static void draw_viewset(void)
{
    static const char *const LN[SONG_LAYERS] = {"Drums", "Bass", "Harmony", "Melody"};
    uint32_t l, i, row = 0, top = ui.vs_row >= 8u ? ui.vs_row - 7u : 0;
    cv_begin(240, 32, C_BLACK);
    cv_text(6, 4, &FONT_B, "VIEWS", C_WHITE);
    cv_text(234 - text_w(&FONT_XS, "hidden: EDIT skips them"), 6, &FONT_XS, "hidden: EDIT skips them", C_DIM);
    cv_rect(0, 31, 240, 1, C_LINE);
    cv_blit(0, 0);
    lcd_fill(0, 32, 240, 182, C_BLACK);
    for (l = 0; l < SONG_LAYERS; l++)
        for (i = 0; VIEWS[l][i] != PV_N; i++, row++) {
            int sel = row == ui.vs_row, hid = view_hidden(l, VIEWS[l][i]);
            char t[32] = "";
            if (row < top || row >= top + 8u)
                continue;
            text_cat(t, LN[l]);
            text_cat(t, ": ");
            text_cat(t, VIEWS[l][i] == PV_CHORD ? (l == L_HARM ? "CHORD" : "NOTE") : view_name(VIEWS[l][i]));
            cv_begin(240, SET_ROW_H, C_BLACK);
            if (sel)
                cv_rect(2, 1, 236, SET_ROW_H - 2, C_LINE);
            cv_text(10, 3, &FONT_XS, t, sel ? C_WHITE : C_GRAY);
            cv_text(230 - text_w(&FONT_XS, hid ? "HIDDEN" : "SHOWN"), 3, &FONT_XS, hid ? "HIDDEN" : "SHOWN",
                    hid ? C_DIM : sel ? C_HI : C_AMB);
            cv_blit(0, 34 + (row - top) * SET_ROW_H);
        }
    draw_legend(Y_LEGEND, "SELECT view|ALGO show/hide|GLO back");
}
static void draw_settings(void)
{
    uint32_t i;
    /* M10 (D062): from a song's play screen the song's info line is the last line (FONT_XS, dimmed): the rows are 2 px
     * shorter then to make room; nothing when no song is loaded or it has none; never in BACKTRACK or SYNTH */
    const char *info = !bt.on && ui.back == V_PLAY && ui.s && ui.s->info[0] ? ui.s->info : 0;
    int32_t rh = info ? SET_ROW_H - 2 : SET_ROW_H;
    cv_begin(240, 32, C_BLACK);
    cv_text(6, 4, &FONT_B, "SETTINGS", C_WHITE);
    cv_text(234 - text_w(&FONT_XS, ui.flash_ok ? "saved when you leave" : "RAM ONLY: not kept"), 6, &FONT_XS,
            ui.flash_ok ? "saved when you leave" : "RAM ONLY: not kept", ui.flash_ok ? C_DIM : C_AMB);
    cv_rect(0, 31, 240, 1, C_LINE);
    cv_blit(0, 0);
    for (i = ui.set_row >= 8u ? ui.set_row - 7u : 0; i < S_N && i < (ui.set_row >= 8u ? ui.set_row + 1u : 8u); i++) {
        int sel = i == ui.set_row;
        uint32_t row = i - (ui.set_row >= 8u ? ui.set_row - 7u : 0);
        cv_begin(240, (uint32_t)rh, C_BLACK);
        if (sel)
            cv_rect(2, 1, 236, rh - 2, C_LINE);
        cv_text(10, (rh - 16) / 2, &FONT_B, SET_NAME[i], sel ? C_WHITE : C_GRAY);
        cv_text(230 - text_w(&FONT_B, SET_VAL[i][SV(i)]), (rh - 16) / 2, &FONT_B, SET_VAL[i][SV(i)], sel ? C_HI : C_AMB);
        cv_blit(0, 32 + row * (uint32_t)rh);
    }
    lcd_fill(0, 32 + 8 * rh, 240, 214 - (32 + 8 * rh), C_BLACK);
    if (info)
        cv_begin(240, 15, C_BLACK), cv_text(6, 0, &FONT_XS, info, C_DIM), cv_blit(0, (uint32_t)(32 + 8 * rh + 3));
    player_stats.info_shown = info != 0;
    vpop_text(player_stats.info_text, info ? info : "", sizeof player_stats.info_text);
    draw_legend(Y_LEGEND, "SELECT row|ALGO change|GLO back");
}

/* ------------------------------------------------------------------------ play --- */
static void draw_top(int32_t bar, int32_t beat, int st)
{
    char t[48] = "";
    const struct song *s = ui.s;
    char lab[16] = "";
    int32_t lab_x;
    if (st == PL_PLAYING) {              /* the label on the right first: the two lines stop before it */
        num_cat(lab, (uint32_t)bar + s->first_bar_number);
        text_cat(lab, ":");
        num_cat(lab, (uint32_t)beat + 1u);
    } else {
        text_cat(lab, st == PL_COUNT_IN ? "IN" : st == PL_PAUSED ? "PAUSE" : "STOP");
    }
    lab_x = 234 - text_w(&FONT_M, lab);
    cv_begin(240, H_TOP, C_BLACK);
    if (bt.on) {                         /* M9: BACKTRACK: the progression, its badge, the style, structure, complexity */
        bt_top(lab_x - 12, st);
        cv_rect(lab_x - 6, 0, 240 - lab_x + 6, H_TOP - 1, C_BLACK);
        cv_text(lab_x, 4, &FONT_M, lab, st == PL_PLAYING ? C_WHITE : C_GRAY);
        cv_blit(0, Y_TOP);
        return;
    }
    cv_text(6, 1, &FONT_XS, s->title, C_GRAY);
    if (plat_ms() < ui.msg_until) {
        text_cat(t, ui.msg[0]);
        text_cat(t, " ");
        text_cat(t, ui.msg[1]);
        cv_text(6, 16, &FONT_XS, t, C_WHITE);
    } else {
        int32_t x = 6;
        {                                /* your part first (M3; M4: always), then the tempo, key, count-in, loop */
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
        if (ui.loop == LOOP_SONG) {
            text_cat(t, "  LOOP SONG");
        } else if (ui.loop) {
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
    cv_rect(lab_x - 6, 0, 240 - lab_x + 6, H_TOP - 1, C_BLACK);   /* (anything longer ends under a clear edge) */
    cv_text(lab_x, 4, &FONT_M, lab, st == PL_PLAYING ? C_WHITE : C_GRAY);
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
    uint8_t ctx[10];                     /* M4: L4, L5: the chord around the note played (shown darker); M5: every
                                          * note the harmony plays in this chord */
    uint32_t nctx;
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
        /* M10.2: no song title (it is at the top of the screen); PRESS PLAY where the title was, the start line 8 px under it
         * (FONT_M is 24 px high: y 38) in FONT_B and C_GRAY, or FONT_XS (same colour) if it does not fit W_BIG */
        text_in(0, W_BIG, 6, &FONT_M, "PRESS PLAY", C_WHITE);
        text_in(0, W_BIG, 38, text_w(&FONT_B, t) <= W_BIG - 4 ? &FONT_B : &FONT_XS, t, C_GRAY);
    } else if (st == PL_PAUSED) {
        char t[24] = "PAUSED ", u[40] = "PLAY: on";
        num_cat(t, (uint32_t)bar + ui.s->first_bar_number);
        text_cat(t, ":");
        num_cat(t, (uint32_t)beat + 1u);
        if (ui.paused_start) {           /* M10.2: ALGO moved the start: PLAY goes on from there */
            u[0] = 0;
            text_cat(u, "PLAY: from bar ");
            num_cat(u, ui.from_bar + ui.s->first_bar_number);
        } else {
            text_cat(u, player_count_in_of() ? " (count-in, bar " : " (from here)");
            if (player_count_in_of()) {
                num_cat(u, (uint32_t)bar + ui.s->first_bar_number);
                text_cat(u, ")");
            }
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
    for (i = 0; i < pi->nctx; i++) {
        lo = pi->ctx[i] < lo ? pi->ctx[i] : lo;
        hi = pi->ctx[i] > hi ? pi->ctx[i] : hi;
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
        for (i = 0; i < pi->nctx; i++)
            lit |= (pi->ctx[i] == n) << 1;   /* (2: the chord around, darker) */
        cv_rect(x0 + (int32_t)wi * KB_W, y0, KB_W - 1, 88, lit & 1u ? on : lit ? DARK_AMB : RGB(220, 220, 225));
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
        for (i = 0; i < pi->nctx; i++)
            lit |= (pi->ctx[i] == n) << 1;
        cv_rect(x0 + (int32_t)wi * KB_W - 7, y0, 13, 54, lit & 1u ? on : lit ? DARK_AMB : RGB(40, 40, 46));
        if (dim && !lit)
            frame_rect(x0 + (int32_t)wi * KB_W - 5, y0 + 36, 9, 14, C_AMB);
    }
    t[0] = 0;
    if (pi->part == L_HARM && k >= 0) {
        inversion_cat(t, (uint32_t)k);
        text_cat(t, "  (L");
        num_cat(t, player_variant_of(L_HARM));
        text_cat(t, ")");
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

struct gfound {
    char frets[7], fingers[7];
    int kind;                            /* 0 none, 1 open, 2 barre, 3 the shape without its slash bass */
    int base;                            /* the barre's fret */
    int n;                               /* M6: strings (6 guitar, 4 ukulele) */
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
    g->n = 6;
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

/* M6: a ukulele shape for a chord symbol (ukulele.h): an open one (by pitch class), else the lowest movable one; a
 * slash chord: its chord's shape (kind 3; a uke has no bass to play it with) */
static void uke_find(const char *sym, struct gfound *g)
{
    char q[12], eq[12];
    int b, eb, er, r = chord_parts(sym, q, &b), best = 99;
    uint32_t i, k, bi = 0;
    g->kind = 0;
    g->base = 0;
    g->n = 4;
    if (r < 0)
        return;
    for (i = 0; i < sizeof UKE_OPEN / sizeof UKE_OPEN[0]; i++) {
        er = chord_parts(UKE_OPEN[i].name, eq, &eb);
        if (er == r && same_text(eq, q)) {
            for (k = 0; k < 4u; k++) {
                g->frets[k] = UKE_OPEN[i].frets[k];
                g->fingers[k] = UKE_OPEN[i].fingers[k];
            }
            g->kind = b >= 0 ? 3 : 1;
            return;
        }
    }
    for (i = 0; i < sizeof UKE_MOVE / sizeof UKE_MOVE[0]; i++) {
        int fret;
        if (!same_text(UKE_MOVE[i].quality, q))
            continue;
        fret = (r + 12 - (int)UKE_MOVE[i].root) % 12;
        fret = fret ? fret : 12;
        if (fret < best)
            best = fret, bi = i;
    }
    if (best == 99)
        return;
    for (k = 0; k < 4u; k++) {
        g->frets[k] = (char)('0' + best + (UKE_MOVE[bi].frets[k] - '0'));
        g->fingers[k] = UKE_MOVE[bi].fingers[k];
    }
    g->kind = b >= 0 ? 3 : 2;
    g->base = best;
}

static void fret_find(const char *sym, struct gfound *g)   /* M6: the setting's instrument */
{
    if (SV(S_UKE))
        uke_find(sym, g);
    else
        guitar_find(sym, g);
}

/* a chord box at x, y: 6 strings (low E left; M6: or a ukulele's 4, G left) sw apart, 4 frets fh apart; o / x above;
 * dots with fingers; a barre */
static void draw_chord_box(int32_t x, int32_t y, const struct gfound *g, int32_t sw, int32_t fh, uint16_t c)
{
    int32_t i, lo = 99, first = -1, last = -1, base, n = g->n;
    for (i = 0; i < n; i++)
        if (g->frets[i] > '0' && g->frets[i] - '0' < lo)
            lo = g->frets[i] - '0';
    base = lo <= 4 || lo == 99 ? 1 : lo; /* up the neck: the lowest fret is the box's first */
    for (i = 0; i < n; i++)
        cv_rect(x + i * sw, y, 1, 4 * fh, C_GRAY);
    for (i = 0; i <= 4; i++)
        cv_rect(x, y + i * fh, (n - 1) * sw + 1, i == 0 && base == 1 ? 3 : 1, C_GRAY);
    if (base > 1) {
        char t[8] = "";
        num_cat(t, (uint32_t)base);
        text_cat(t, "fr");
        cv_text(x - text_w(&FONT_XS, t) - 4, y + 2, &FONT_XS, t, C_GRAY);
    }
    for (i = 0; i < n; i++)              /* the barre: finger 1 on the lowest fret across strings */
        if (g->fingers[i] == '1' && g->frets[i] - '0' == lo) {
            if (first < 0)
                first = i;
            last = i;
        }
    if (g->kind == 2 && last > first)
        cv_rect(x + first * sw, y + (lo - base) * fh + fh / 2 - fh / 5, (last - first) * sw + 1, 2 * (fh / 5) + 1, c);
    for (i = 0; i < n; i++) {
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
        fret_find(sym, &g);
        if (g.kind) {
            draw_chord_box(g.n == 4 ? 112 : 96, 30, &g, 16, 22, C_HI);
            cv_text(6, 40, &FONT_XS, "x: not played", C_DIM);
            cv_text(6, 54, &FONT_XS, "o: open string", C_DIM);
            if (k >= 0 && (pi->cue & 15u))   /* M6: the rhythm icon under them, blinking at each attack */
                draw_cue(12, 74, pi->cue & 15u, (pi->cue >> 4) & 1u);
            text_cat(t, g.n == 4 ? (g.kind == 1 ? "ukulele: open shape" : g.kind == 2 ? "ukulele: from fret " :
                                    "ukulele: this shape, without ") :
                        g.kind == 1 ? "guitar: open shape" : g.kind == 2 ? "guitar: barre at fret " :
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
        fret_find(chord_name((uint32_t)next), &gn);
        if (gn.kind)
            draw_chord_box(gn.n == 4 ? 204 : 196, 64, &gn, 8, 11, C_AMB);
    }
    cv_text(6, 126, &FONT_XS, t, C_GRAY);
    cv_blit(0, Y_CHORD);
}

/* ------------------------------------------- the scrolling views (M3 parts 6, 7): one engine --- */
/* time runs right to left past a now line at x0; span ticks of the song right of it. The tab, roll and drum grid: x0
 * 44, 1.25 bars ahead (about 1.5 bars on the screen); the notation: x0 84, 36 px a beat (the approved mock). The
 * zoom setting doubles the span (about 3 bars) */
#define NOW_X 44

static int view_staff(int view) { return view == PV_NOTATION || view == PV_DNOTE; }
static int view_zooms(int view)          /* M6: SELECT zooms these */
{
    return view_staff(view) || view == PV_TAB || view == PV_ROLL || view == PV_GRID || view == PV_MTAB ||
           view == PV_BARS;
}

static uint32_t zoom_of(int view)
{
    return view_staff(view) && ui.zoom > ZOOM_STAFF_MAX ? ZOOM_STAFF_MAX : ui.zoom;
}

static void scroll_setup(int view)
{
    uint32_t q = ZOOM_Q[zoom_of(view)];
    int st = view_staff(view);
    ui.sc_x0 = st ? 84 : NOW_X;
    ui.sc_span = (int32_t)(st ? (uint32_t)(240 - 84) * ui.s->ppq / 36u * q / 4u : bar_ticks() * 5u * q / 16u);
    player_stats.zoom = (uint32_t)ui.sc_span * 240u / (uint32_t)(240 - ui.sc_x0);   /* (the ticks on the screen) */
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

/* where a bass player plays note k of stream st (M5, the user's note): a hand covers 4 frets; a note inside the
 * hand's position is played there (the nearest string), an open string costs nothing, a shift costs by its length.
 * The position comes from the 8 notes before, so a note always gets the same string and fret, wherever the view
 * starts. *s, *f: the string (0 E .. 3 G) and fret */
/* M6: a fretted instrument's strings for the tabs: the bass (M3), the guitar and the ukulele (the melody's tab) */
struct tuning {
    uint8_t n, top;                      /* strings; the highest fret used */
    uint8_t open[6];                     /* lowest string first (as a tab draws them: bottom line first) */
    const char *name[6];
};
static const struct tuning T_BASS = {4, 15, {28, 33, 38, 43}, {"E", "A", "D", "G"}};   /* E1 A1 D2 G2 */
static const struct tuning T_GUITAR = {6, 15, {40, 45, 50, 55, 59, 64}, {"E", "A", "D", "G", "B", "e"}};
static const struct tuning T_UKE = {4, 12, {67, 60, 64, 69}, {"G", "C", "E", "A"}};
/* (the ukulele, as a uke tab draws it: A on top, the re-entrant G4 at the bottom) */

/* a playable fingering for note k of stream st (M5 for the bass, M6 any tuning): a hand position over up to 4 frets,
 * moved as little as it can, open strings when they fit; the notes before k (up to 8) set where the hand is.
 * Notes out of range move by octaves into it */
static void tab_fingering(const struct tuning *tu, const struct song_stream *st, uint32_t k, uint32_t *s, uint32_t *f)
{
    struct song_event e;
    uint32_t j = k, back = 0, pos = 0, ps = 0, lo = 127, hi = 0, c;
    for (c = 0; c < tu->n; c++) {
        lo = tu->open[c] < lo ? tu->open[c] : lo;
        hi = tu->open[c] + tu->top > hi ? tu->open[c] + tu->top : hi;
    }
    *s = *f = 0;
    while (j > 0 && back < 8u) {         /* back to 8 notes before */
        song_event_get(st, --j, &e);
        if (e.status == 0x90u)
            back++;
    }
    for (; j <= k; j++) {
        uint32_t n, best = 0xFFFFFFFFu, bs = 0, bf = 0;
        song_event_get(st, j, &e);
        if (e.status != 0x90u)
            continue;
        n = moved(e.d1);
        while (n < lo)
            n += 12u;                    /* (below the lowest string: an octave up) */
        while (n > hi)
            n -= 12u;
        for (c = 0; c < tu->n; c++) {
            uint32_t fr, cost;
            if (n < tu->open[c] || n - tu->open[c] > tu->top)
                continue;
            fr = n - tu->open[c];
            if (fr == 0u)
                cost = 2u;               /* an open string */
            else if (pos && fr >= pos && fr <= pos + 3u)
                cost = 0u;               /* inside the hand */
            else
                cost = pos ? 10u + 4u * (fr > pos ? fr - pos : pos - fr) : fr;   /* (no position yet: low frets) */
            cost += c > ps ? c - ps : ps - c;   /* the nearest string */
            if (cost < best)
                best = cost, bs = c, bf = fr;
        }
        if (bf && (!pos || bf < pos || bf > pos + 3u))
            pos = bf > 3u && pos && bf > pos ? bf - 3u : bf;   /* the hand moves to cover it */
        ps = bs;
        *s = bs;
        *f = bf;
    }
}

/* the tabs (M3 part 6: the bass; M6: the melody on guitar or ukulele): a line a string (the lowest at the bottom),
 * the part at its level as fret numbers on the string it is played on (tab_fingering), a line for how long it lasts,
 * its name under the tab; played dim, sounding white, coming amber */
static void draw_tab_view(int32_t now, const struct pinfo *pi)
{
    const struct tuning *tu = pi->part == L_BASS ? &T_BASS : SV(S_UKE) ? &T_UKE : &T_GUITAR;
    uint32_t l = pi->part == L_BASS ? L_BASS : L_MELODY;
    const struct song_stream *st = layer_stream(l);
    struct song_event e;
    int32_t ys[6], i, x0 = 16, gap = tu->n == 6u ? 14 : 20, yb = tu->n == 6u ? 98 : 92;
    uint32_t k;
    char t[48] = "";
    cv_begin(240, H_VIEW, C_BLACK);
    for (i = 0; i < (int32_t)tu->n; i++) {
        ys[i] = yb - i * gap;
        cv_text(2, ys[i] - 8, &FONT_XS, tu->name[i], C_DIM);
        cv_rect(x0, ys[i], 240 - x0, 1, C_GRAY);
    }
    scroll_grid(now, ys[tu->n - 1u] - 6, ys[0] + 6, 1);
    if (st)
        for (k = song_seek(st, (uint32_t)(scroll_from(now) - (int32_t)bar_ticks() > 0 ?
                                          scroll_from(now) - (int32_t)bar_ticks() : 0)); k < st->count; k++) {
            uint32_t n, end, s, uf;
            int32_t x, xe;
            uint16_t c;
            char fr[4] = "";
            song_event_get(st, k, &e);
            if ((int32_t)e.tick > scroll_to(now))
                break;
            if (e.status != 0x90u)
                continue;
            end = note_end(st, k, e.d1, e.tick);
            tab_fingering(tu, st, k, &s, &uf);   /* (M5: a hand position; the real notes) */
            n = tu->open[s] + uf;
            x = scroll_x((int32_t)e.tick, now);
            xe = scroll_x((int32_t)end, now);
            if (xe < x0 || x >= 240)
                continue;
            c = (int32_t)end <= now ? C_DIM : (int32_t)e.tick <= now ? C_WHITE : C_AMB;
            if (xe > x + 12)
                cv_rect(x + 12, ys[s] - 1, xe - x - 12, 3, (int32_t)end <= now ? C_LINE : DARK_AMB);
            num_cat(fr, uf);
            cv_rect(x - 1, ys[s] - 8, text_w(&FONT_B, fr) + 3, 16, C_BLACK);
            cv_text(x, ys[s] - 9, &FONT_B, fr, c);
            cv_text(x, ys[0] + 10, &FONT_XS, names()[n % 12u], c == C_WHITE ? C_WHITE : C_GRAY);
        }
    scroll_now_line(ys[tu->n - 1u] - 10, ys[0] + 10);
    text_cat(t, l == L_BASS ? "BASS L" : tu == &T_UKE ? "UKULELE, MELODY L" : "GUITAR, MELODY L");
    num_cat(t, player_variant_of(l));
    if (pi->big[0]) {
        text_cat(t, ": ");
        text_cat(t, pi->big);
        if (pi->next[0] && l == L_BASS) {
            text_cat(t, ", next ");
            text_cat(t, pi->next);
        }
    }
    cv_text(6, 126, &FONT_XS, t, C_GRAY);
    cv_blit(0, Y_CHORD);
}

/* the chord names above a scrolling view, at their changes (the one sounding amber) */
static void scroll_chords(int32_t now, int32_t y, int32_t clip)
{
    const struct song *s = ui.s;
    uint32_t k = song_chord_at(s, (uint32_t)(scroll_from(now) > 0 ? scroll_from(now) : 0));
    for (; k < s->ncevt && (int32_t)song_cevt_tick(s, k) <= scroll_to(now); k++) {
        int32_t t = (int32_t)song_cevt_tick(s, k), x = scroll_x(t, now), xe;
        const char *sym = chord_name(k);
        int sounding = t <= now && (k + 1u >= s->ncevt || (int32_t)song_cevt_tick(s, k + 1u) > now);
        if (sym[0] == 'N')
            continue;
        if (x < clip)
            x = clip;                    /* the one sounding stays in view at the left */
        xe = k + 1u < s->ncevt ? scroll_x((int32_t)song_cevt_tick(s, k + 1u), now) : 240;
        if (x + text_w(&FONT_B, sym) > xe && !sounding)
            continue;
        if (x < 238)
            cv_text(x, y, &FONT_B, sym, sounding ? C_HI : C_WHITE);
    }
}

/* ------------------------------------------------------ the melody roll (M3 part 7, melody) --- */
/* the melody's notes as bars sliding to the now line, higher notes higher (the song's melody range at this level, in
 * the key now, fills the height), their names above them; the chords above. M6: on the left a vertical piano
 * keyboard, a key for each row (the rows are pitches, as the drum grid's labels are its drums): the notes sounding
 * light their keys, each C is named */
#define RK_W 22                          /* the keyboard's width; the notes are drawn right of it */
#define RK_WHITE RGB(196, 196, 204)
#define RK_BLACK RGB(16, 16, 20)
static void draw_roll_view(int32_t now, const struct pinfo *pi)
{
    const struct song_stream *st = layer_stream(pi->part);
    struct song_event e;
    uint32_t k, lo = 127, hi = 0, n, lit[4] = {0, 0, 0, 0};
    int32_t y0 = 26, y1 = 118, clip = RK_W + 2;
    char t[48] = "";
    cv_begin(240, H_VIEW, C_BLACK);
    if (st)
        for (k = 0; k < st->count; k++) { /* the range (a melody: a few hundred events) */
            song_event_get(st, k, &e);
            if (e.status == 0x90u) {
                n = moved(e.d1);
                lo = n < lo ? n : lo;
                hi = n > hi ? n : hi;
            }
        }
    if (lo > hi)
        lo = 60, hi = 72;
    if (hi - lo < 12u)
        lo = hi > 12u ? hi - 12u : 0;
    if (hi - lo > 120u)
        lo = hi - 120u;
    for (n = lo; n <= hi; n++)           /* a faint line for each C and G */
        if (n % 12u == 0u || n % 12u == 7u)
            cv_rect(clip, y1 - (int32_t)((n - lo) * (uint32_t)(y1 - y0) / (hi - lo)), 240 - clip, 1, C_LINE);
    scroll_grid(now, y0 - 4, y1 + 6, 0);
    scroll_chords(now, 0, clip);
    if (st)
        for (k = song_seek(st, (uint32_t)(scroll_from(now) - (int32_t)bar_ticks() > 0 ?
                                          scroll_from(now) - (int32_t)bar_ticks() : 0)); k < st->count; k++) {
            int32_t x, xe, y;
            uint32_t end;
            uint16_t c;
            song_event_get(st, k, &e);
            if ((int32_t)e.tick > scroll_to(now))
                break;
            if (e.status != 0x90u)
                continue;
            n = moved(e.d1);
            end = note_end(st, k, e.d1, e.tick);
            if ((int32_t)e.tick <= now && now < (int32_t)end && n >= lo && n <= hi)
                lit[(n - lo) / 32u] |= 1u << ((n - lo) % 32u);
            x = scroll_x((int32_t)e.tick, now);
            xe = scroll_x((int32_t)end, now);
            if (xe < clip || x >= 240)
                continue;
            y = y1 - (int32_t)((n - lo) * (uint32_t)(y1 - y0) / (hi - lo));
            c = (int32_t)end <= now ? C_DIM : (int32_t)e.tick <= now ? C_HI : C_AMB;
            cv_rect(x < clip ? clip : x, y - 3, xe - (x < clip ? clip : x) - 1 > 2 ? xe - (x < clip ? clip : x) - 1 : 2,
                    7, c);
            if (x >= clip && pi->part != L_HARM)   /* (M6: the bar view's chords: their notes stacked, no names) */
                cv_text(x + 1, y - 18, &FONT_XS, names()[n % 12u], (int32_t)end <= now ? C_DIM : C_GRAY);
        }
    {                                    /* M6: the keyboard: each pitch's key spans halfway to its neighbours */
        int32_t top = y1 + (int32_t)((uint32_t)(y1 - y0) / (hi - lo) / 2u) + 1;
        for (n = lo; n <= hi; n++) {
            int32_t yc = y1 - (int32_t)((n - lo) * (uint32_t)(y1 - y0) / (hi - lo));
            int32_t yt = n == hi ? yc - (int32_t)((uint32_t)(y1 - y0) / (hi - lo) / 2u) - 1 :
                         y1 - (int32_t)((2u * (n - lo) + 1u) * (uint32_t)(y1 - y0) / (2u * (hi - lo)));
            int on = (lit[(n - lo) / 32u] >> ((n - lo) % 32u)) & 1u;
            if (top - yt < 1)
                yt = top - 1;
            if (is_black(n)) {
                cv_rect(0, yt, RK_W, top - yt, RK_WHITE);
                cv_rect(0, yt, RK_W * 3 / 5, top - yt, on ? C_HI : RK_BLACK);
            } else {
                cv_rect(0, yt, RK_W, top - yt, on ? C_HI : RK_WHITE);
                if (n % 12u == 0u || n % 12u == 5u)   /* between E-F and B-C: a line */
                    cv_rect(0, top - 1, RK_W, 1, C_DIM);
            }
            top = yt;
        }
        for (n = lo; n <= hi; n++)       /* each C named, on its key (after the keys: a name may overlap the next) */
            if (n % 12u == 0u) {
                int32_t yc = y1 - (int32_t)((n - lo) * (uint32_t)(y1 - y0) / (hi - lo));
                char c[4] = "C";
                int on = (lit[(n - lo) / 32u] >> ((n - lo) % 32u)) & 1u;
                num_cat(c, n / 12u - 1u);
                cv_rect(RK_W - text_w(&FONT_T, c) - 2, yc - 4, text_w(&FONT_T, c) + 2, 9, on ? C_HI : RK_WHITE);
                cv_text(RK_W - text_w(&FONT_T, c) - 1, yc - 4, &FONT_T, c, C_BLACK);
            }
        player_stats.roll_keys = hi - lo + 1u;
    }
    scroll_now_line(y0 - 8, y1 + 8);
    text_cat(t, LAYER_NAME[pi->part]);
    text_cat(t, " L");
    num_cat(t, player_variant_of(pi->part));
    text_cat(t, ": the bars slide to the line");
    cv_text(6, 126, &FONT_XS, t, C_GRAY);
    cv_blit(0, Y_CHORD);
}

/* ------------------------------------------------------- the drum grid (M3 part 7, drums) --- */
static void draw_grid_view(int32_t now)
{
    static const char *const ROW[4] = {"KICK", "SNARE", "HATS", "OTHER"};
    const struct song_stream *st = layer_stream(L_DRUMS);
    struct song_event e;
    uint32_t k;
    int32_t i, y0 = 20, rh = 24;
    char t[32] = "DRUMS L";
    cv_begin(240, H_VIEW, C_BLACK);
    for (i = 0; i < 4; i++) {
        cv_text(2, y0 + i * rh + 3, &FONT_T, ROW[i], C_DIM);
        cv_rect(30, y0 + i * rh + rh - 3, 210, 1, C_LINE);
    }
    scroll_grid(now, y0 - 2, y0 + 4 * rh, 1);
    if (st)
        for (k = song_seek(st, (uint32_t)(scroll_from(now) > 0 ? scroll_from(now) : 0)); k < st->count; k++) {
            int32_t x, w;
            song_event_get(st, k, &e);
            if ((int32_t)e.tick > scroll_to(now))
                break;
            if (e.status != 0x90u)
                continue;
            x = scroll_x((int32_t)e.tick, now);
            w = scroll_x((int32_t)(e.tick + ui.s->ppq / 4u), now) - x;   /* a sixteenth wide */
            w = w < 4 ? 4 : w > 14 ? 14 : w;
            if (x < 30 || x >= 240)
                continue;
            cv_rect(x, y0 + (int32_t)drum_row(e.d1) * rh + 2, w, rh - 8,
                    (int32_t)e.tick + (int32_t)ui.s->ppq / 8 < now ? C_DIM : (int32_t)e.tick <= now ? C_WHITE :
                    e.d2 >= 80u ? C_HI : C_AMB);
        }
    scroll_now_line(y0 - 6, y0 + 4 * rh + 2);
    num_cat(t, player_variant_of(L_DRUMS));
    text_cat(t, ": the hits slide to the line");
    cv_text(6, 126, &FONT_XS, t, C_GRAY);
    cv_blit(0, Y_CHORD);
}

/* --------------------------------------- the notation view (M3 part 7, D025: simplified notation) --- */
/* one staff: the treble clef for the melody, the bass clef for the bass; the key signature; noteheads (hollow from a
 * half note), stems, dots, beams for eighth pairs (a flag for a lone eighth), ledger lines, bar lines; the chords
 * above; the note names under the staff (a setting). Played dim, sounding amber, coming white. Each note is drawn as
 * the nearest simple value (whole, dotted half, half, dotted quarter, quarter, eighth): no ties, no rests (D025) */
#define ST_TOP 34                        /* the staff's top line (view rows) */
#define ST_SP 12                         /* line spacing */
#define ST_CLIP 52                       /* notes are drawn right of the clef and key signature */
#define ST_NOW 84
static const int8_t TREBLE[48][2] = {
    {-2, 36}, {-2, 35}, {-3, 35}, {-3, 34}, {-2, 33}, {-2, 32}, {-1, 32}, {-1, 31}, {1, 31}, {2, 30}, {3, 31},
    {4, 31}, {5, 32}, {7, 33}, {7, 34}, {8, 36}, {8, 38}, {8, 40}, {7, 42}, {6, 43}, {5, 45}, {3, 46}, {1, 47},
    {-1, 48}, {-4, 48}, {-6, 47}, {-8, 46}, {-10, 44}, {-12, 42}, {-13, 39}, {-14, 36}, {-14, 33}, {-13, 30},
    {-12, 27}, {-10, 24}, {-8, 22}, {-5, 20}, {-2, 19}, {2, 18}, {5, 19}, {6, 2}, {7, -8}, {3, -14}, {-1, -8},
    {0, 6}, {4, 58}, {2, 63}, {-3, 61}};   /* the planning chat's mock's clef (docs/m3-design/notation_mock.py) */
static const int8_t KEY_SIG[12] = {0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5};   /* by the parent major: + sharps, - flats */
static const uint8_t SHARP_STEPS[7] = {8, 5, 9, 6, 3, 7, 4};   /* F5 C5 G5 D5 A4 E5 B4: steps above E4 (treble) */
static const uint8_t FLAT_STEPS[7] = {4, 7, 3, 6, 2, 5, 1};    /* B4 E5 A4 D5 G4 C5 F4 */

/* a note's staff step (0: the bottom line; each step half a space) and its accidental as spelled (1 #, -1 b, 0) */
static int32_t staff_step(uint32_t n, int bass, int *acc)
{
    static const uint8_t LETTER[7] = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};
    const char *nm = names()[n % 12u];
    int32_t l = 0, oct = (int32_t)(n / 12u) - 1;
    while (l < 6 && LETTER[l] != (uint8_t)nm[0])
        l++;
    *acc = nm[1] == '#' ? 1 : nm[1] == 'b' ? -1 : 0;
    if (nm[0] == 'C' && nm[1] == 'b')
        oct++;
    return oct * 7 + l - (bass ? 2 * 7 + 4 : 4 * 7 + 2);   /* minus G2 (bass) or E4 (treble) */
}

/* the key signature's accidental on a letter (step mod 7 from E): 1, -1 or 0 */
static int sig_acc(int32_t step, int sig, int bass)
{
    int32_t i, s;
    for (i = 0; i < (sig > 0 ? sig : -sig); i++) {
        s = (sig > 0 ? SHARP_STEPS[i] : FLAT_STEPS[i]) - (bass ? 2 : 0);   /* G2 is two steps above E2 */
        if ((((s - step) % 7) + 7) % 7 == 0)
            return sig > 0 ? 1 : -1;
    }
    return 0;
}

static void draw_acc(int32_t x, int32_t y, int acc, uint16_t c)
{
    if (acc > 0) {                       /* sharp */
        cv_rect(x + 1, y - 6, 1, 12, c);
        cv_rect(x + 4, y - 7, 1, 12, c);
        cv_rect(x - 1, y - 3, 8, 1, c);
        cv_rect(x - 1, y + 2, 8, 1, c);
    } else if (acc < 0) {                /* flat */
        cv_rect(x + 1, y - 9, 1, 13, c);
        cv_rect(x + 2, y - 1, 3, 1, c);
        cv_rect(x + 4, y, 1, 3, c);
        cv_rect(x + 2, y + 3, 2, 1, c);
    } else {                             /* natural */
        cv_rect(x + 1, y - 7, 1, 11, c);
        cv_rect(x + 5, y - 4, 1, 11, c);
        cv_rect(x + 1, y - 2, 5, 2, c);
        cv_rect(x + 1, y + 2, 5, 2, c);
    }
}

static void draw_head(int32_t x, int32_t y, int hollow, uint16_t c)
{
    static const uint8_t W[9] = {6, 10, 12, 12, 12, 12, 12, 10, 6};   /* 12 x 9, tilted a little */
    int32_t i;
    for (i = 0; i < 9; i++) {
        int32_t w = W[i], ox = (12 - w) / 2 + (4 - i) / 3 - 6;
        if (hollow && i > 1 && i < 7) {
            cv_rect(x + ox, y - 4 + i, 3, 1, c);
            cv_rect(x + ox + w - 3, y - 4 + i, 3, 1, c);
        } else {
            cv_rect(x + ox, y - 4 + i, w, 1, c);
        }
    }
}

struct snote { int32_t x, y, step; uint32_t q; uint16_t c; };   /* q: the value in eighths (1..8) */

/* the clef and key signature at a staff's left (its top line top, line spacing sp), over the notes scrolled past */
static void staff_head(int32_t top, int32_t sp, int bass, int sig)
{
    int32_t i, bot = top + 4 * sp;
    cv_rect(0, top - 2 * sp, ST_CLIP - 2, 8 * sp, C_BLACK);
    for (i = 0; i < 5; i++)
        cv_rect(4, top + i * sp, ST_CLIP - 6, 1, C_GRAY);
    if (!bass) {
        for (i = 0; i + 1 < 48; i++)
            if (TREBLE[i + 1][1] - TREBLE[i][1] < 40 && TREBLE[i][1] - TREBLE[i + 1][1] < 40) {
                int32_t x0 = TREBLE[i][0] * sp / 12, y0 = TREBLE[i][1] * sp / 12;
                int32_t x1 = TREBLE[i + 1][0] * sp / 12, y1 = TREBLE[i + 1][1] * sp / 12;
                cv_line(18 + x0, top + y0, 18 + x1, top + y1, C_WHITE);
                cv_line(19 + x0, top + y0, 19 + x1, top + y1, C_WHITE);
            }
    } else {                             /* the bass clef: a hook from the F line, two dots */
        static const int8_t BC[9][2] = {{0, 15}, {2, 9}, {7, 7}, {12, 9}, {14, 15}, {13, 22}, {9, 29}, {3, 35},
                                        {-2, 39}};
        for (i = 0; i + 1 < 9; i++) {
            cv_line(8 + BC[i][0] * sp / 12, top + (BC[i][1] - 6) * sp / 12, 8 + BC[i + 1][0] * sp / 12,
                    top + (BC[i + 1][1] - 6) * sp / 12, C_WHITE);
            cv_line(8 + BC[i][0] * sp / 12, top + (BC[i][1] - 5) * sp / 12, 8 + BC[i + 1][0] * sp / 12,
                    top + (BC[i + 1][1] - 5) * sp / 12, C_WHITE);
        }
        cv_rect(6, top + sp - 3, 5, 5, C_WHITE);
        cv_rect(26, top + sp / 2 - 2, 3, 3, C_WHITE);
        cv_rect(26, top + sp * 3 / 2 - 2, 3, 3, C_WHITE);
    }
    for (i = 0; i < (sig > 0 ? sig : -sig) && i < 7; i++) {   /* the key signature */
        int32_t step = (sig > 0 ? SHARP_STEPS[i] : FLAT_STEPS[i]) - (bass ? 2 : 0);
        draw_acc(32 + i * 5, bot - step * sp / 2, sig > 0 ? 1 : -1, C_WHITE);
    }
}

static int key_sig_now(void)
{
    return KEY_SIG[(ui.s->key_root + MODE_PARENT[ui.s->key_mode % 7u] + 12u + (uint32_t)ui.key) % 12u];
}

/* a note's value: the nearest simple one, in eighths (8 whole, 6 dotted half, 4 half, 3 dotted quarter, 2, 1) */
static uint32_t simple_value(uint32_t ticks)
{
    uint32_t len = ticks * 8u / ui.s->ppq;   /* in 32nd notes */
    return len >= 28u ? 8u : len >= 20u ? 6u : len >= 14u ? 4u : len >= 10u ? 3u : len >= 6u ? 2u : 1u;
}

/* -------------------------------------- the harmony notation (M4 part 3): a grand staff, chords stacked --- */
/* treble and bass staff (the harmony's notes from C4 up on the treble, below it on the bass), the harmony at its level
 * as played: block chords stacked on one stem a staff, broken chords and arpeggios as single notes; chord names above.
 * The same simplifications as D025 (nearest simple value, no ties, no rests); flags, no beams */
#define GS_SP 9
#define GS_TREBLE 24                     /* the treble staff's top line; the bass staff's 14 rows under its bottom */
#define GS_BASS (GS_TREBLE + 4 * GS_SP + 18)

static void draw_harmony_notation(int32_t now)
{
    const struct song_stream *st = layer_stream(L_HARM);
    struct song_event e;
    int sig = key_sig_now();
    int32_t i, x;
    uint32_t k = 0, j;
    cv_begin(240, H_VIEW, C_BLACK);
    for (i = 0; i < 5; i++) {
        cv_rect(4, GS_TREBLE + i * GS_SP, 236, 1, C_GRAY);
        cv_rect(4, GS_BASS + i * GS_SP, 236, 1, C_GRAY);
    }
    scroll_grid(now, GS_TREBLE, GS_BASS + 4 * GS_SP + 1, 0);
    scroll_chords(now, 2, ST_CLIP);
    if (st)
        k = song_seek(st, (uint32_t)(scroll_from(now) > 0 ? scroll_from(now) : 0));
    while (st && k < st->count) {        /* an attack: the note-ons at one tick */
        uint8_t n[8];
        uint32_t nn = 0, t0, end, q, s;
        uint16_t c;
        song_event_get(st, k, &e);
        if ((int32_t)e.tick > scroll_to(now))
            break;
        if (e.status != 0x90u) {
            k++;
            continue;
        }
        t0 = e.tick;
        end = note_end(st, k, e.d1, e.tick);
        for (; k < st->count; k++) {
            song_event_get(st, k, &e);
            if (e.tick != t0)
                break;
            if (e.status == 0x90u && nn < 8u)
                n[nn++] = (uint8_t)moved(e.d1);
        }
        x = scroll_x((int32_t)t0, now) + 10;
        if (x < ST_CLIP + 4 || x > 236)
            continue;
        q = simple_value(end - t0);
        c = (int32_t)end <= now ? C_DIM : (int32_t)t0 <= now ? C_HI : C_WHITE;
        for (s = 0; s < 2u; s++) {       /* the treble staff, then the bass staff */
            int32_t top = s ? GS_BASS : GS_TREBLE, bot = top + 4 * GS_SP, lo = 99, hi = -99, prev = -99;
            for (j = 0; j < nn; j++) {
                int acc;
                int32_t step, y, dx;
                if ((n[j] >= 60u) != (s == 0u))
                    continue;
                step = staff_step(n[j], (int)s, &acc);
                y = bot - step * GS_SP / 2;
                for (i = -2; i >= step; i -= 2)   /* ledger lines */
                    cv_rect(x - 8, bot - i * GS_SP / 2, 16, 1, C_GRAY);
                for (i = 10; i <= step; i += 2)
                    cv_rect(x - 8, bot - i * GS_SP / 2, 16, 1, C_GRAY);
                dx = step - prev == 1 || prev - step == 1 ? 11 : 0;   /* a second: the head beside */
                prev = dx ? -99 : step;
                if (acc != sig_acc(step, sig, (int)s))
                    draw_acc(x - 17, y, acc, c);
                draw_head(x + dx, y, q >= 4u, c);
                if (q == 3u || q == 6u)
                    cv_rect(x + dx + 8, y - 1, 3, 3, c);
                lo = step < lo ? step : lo;
                hi = step > hi ? step : hi;
            }
            if (lo > hi || q == 8u)
                continue;                /* no note on this staff, or a whole note: no stem */
            if (s == 0u) {               /* treble: the stem up from the lowest to above the highest */
                int32_t yl = bot - lo * GS_SP / 2, yh = bot - hi * GS_SP / 2 - 22;
                cv_rect(x + 5, yh, 2, yl - yh, c);
                if (q == 1u)
                    cv_line(x + 6, yh, x + 12, yh + 8, c);
            } else {                     /* bass: down from the highest to below the lowest */
                int32_t yh = bot - hi * GS_SP / 2, yl = bot - lo * GS_SP / 2 + 22;
                cv_rect(x - 6, yh, 2, yl - yh, c);
                if (q == 1u)
                    cv_line(x - 5, yl, x + 1, yl - 8, c);
            }
        }
    }
    staff_head(GS_TREBLE, GS_SP, 0, sig);
    staff_head(GS_BASS, GS_SP, 1, sig);
    cv_rect(4, GS_TREBLE, 2, GS_BASS + 4 * GS_SP - GS_TREBLE + 1, C_GRAY);   /* the brace's line */
    cv_rect(ST_NOW, GS_TREBLE - 8, 2, GS_BASS + 4 * GS_SP - GS_TREBLE + 16, C_AMB);
    cv_rect(ST_NOW - 3, GS_TREBLE - 12, 8, 2, C_AMB);
    cv_text(6, 126, &FONT_XS, "HARMONY", C_GRAY);
    cv_blit(0, Y_CHORD);
}

/* ------------------------------------------ the drum notation (M4 part 3): a percussion staff --- */
/* 5 lines; the kick in the bottom space, the low tom, the snare (third space), the high tom, the hi-hat, ride and crash
 * above (x heads); a stem up for the hands' notes of one hit, down for the kick; the nearest simple value; no rests */
static int32_t drum_step(uint32_t n, int *x_head)
{
    *x_head = 0;
    if (n == 35u || n == 36u)
        return 1;                        /* kick: F4's space */
    if (n == 37u || (n >= 38u && n <= 40u))
        return 5;                        /* snare (and rim, clap): C5's space */
    if (n == 41u || n == 43u || n == 45u)
        return 3;                        /* low toms */
    if (n == 47u || n == 48u || n == 50u)
        return 7;                        /* high toms */
    *x_head = 1;
    if (n == 49u || n == 52u || n == 55u || n == 57u)
        return 10;                       /* crash: A5, a ledger line */
    if (n == 51u || n == 53u || n == 59u)
        return 8;                        /* ride: F5, the top line */
    return 9;                            /* hi-hat and the others: G5 */
}

static void draw_drum_notation(int32_t now)
{
    const struct song_stream *st = layer_stream(L_DRUMS);
    struct song_event e;
    int32_t i, bot = ST_TOP + 4 * ST_SP;
    uint32_t k = 0;
    char t[24] = "DRUMS L";
    cv_begin(240, H_VIEW, C_BLACK);
    for (i = 0; i < 5; i++)
        cv_rect(4, ST_TOP + i * ST_SP, 236, 1, C_GRAY);
    scroll_grid(now, ST_TOP, bot + 1, 1);
    if (st)
        k = song_seek(st, (uint32_t)(scroll_from(now) > 0 ? scroll_from(now) : 0));
    while (st && k < st->count) {
        uint32_t t0, q = 1, j;
        int32_t x, hands_hi = -99, hands_lo = 99, kick = 0;
        uint16_t c;
        song_event_get(st, k, &e);
        if ((int32_t)e.tick > scroll_to(now))
            break;
        if (e.status != 0x90u) {
            k++;
            continue;
        }
        t0 = e.tick;
        x = scroll_x((int32_t)t0, now) + 10;
        c = (int32_t)t0 + (int32_t)ui.s->ppq / 4 < now ? C_DIM : (int32_t)t0 <= now ? C_HI : C_WHITE;
        for (j = k + 1u; j < st->count && j < k + 32u; j++) {   /* its value: up to the next hit */
            struct song_event f;
            song_event_get(st, j, &f);
            if (f.status == 0x90u && f.tick > t0) {
                q = simple_value(f.tick - t0);
                break;
            }
        }
        for (; k < st->count; k++) {
            int xh;
            int32_t step, y;
            song_event_get(st, k, &e);
            if (e.tick != t0)
                break;
            if (e.status != 0x90u || x < ST_CLIP - 30 || x > 236)
                continue;
            step = drum_step(e.d1, &xh);
            y = bot - step * ST_SP / 2;
            if (step >= 10)
                cv_rect(x - 9, bot - 10 * ST_SP / 2, 18, 1, C_GRAY);
            if (xh) {                    /* an x head */
                cv_line(x - 4, y - 4, x + 4, y + 4, c);
                cv_line(x - 4, y + 4, x + 4, y - 4, c);
                cv_line(x - 3, y - 4, x + 5, y + 4, c);
                cv_line(x - 3, y + 4, x + 5, y - 4, c);
            } else {
                draw_head(x, y, q >= 4u, c);
            }
            if (step == 1) {
                kick = y;
            } else {
                hands_hi = step > hands_hi ? step : hands_hi;
                hands_lo = step < hands_lo ? step : hands_lo;
            }
        }
        if (x < ST_CLIP - 30 || x > 236 || q >= 8u)
            continue;
        if (hands_hi >= hands_lo) {      /* the hands: one stem up */
            int32_t yl = bot - hands_lo * ST_SP / 2, yh = bot - hands_hi * ST_SP / 2 - 24;
            cv_rect(x + 5, yh, 2, yl - yh, c);
            if (q == 1u)
                cv_line(x + 6, yh, x + 12, yh + 8, c);
        }
        if (kick) {                      /* the kick: down */
            cv_rect(x - 6, kick, 2, 24, c);
            if (q == 1u && hands_hi < hands_lo)
                cv_line(x - 5, kick + 24, x + 1, kick + 16, c);
        }
    }
    cv_rect(0, ST_TOP - 2, ST_CLIP - 30, 4 * ST_SP + 4, C_BLACK);   /* the percussion clef: two bars */
    for (i = 0; i < 5; i++)
        cv_rect(4, ST_TOP + i * ST_SP, ST_CLIP - 34, 1, C_GRAY);
    cv_rect(10, ST_TOP + ST_SP, 3, 2 * ST_SP, C_WHITE);
    cv_rect(16, ST_TOP + ST_SP, 3, 2 * ST_SP, C_WHITE);
    cv_rect(ST_NOW, ST_TOP - 10, 2, 4 * ST_SP + 22, C_AMB);
    cv_rect(ST_NOW - 3, ST_TOP - 14, 8, 2, C_AMB);
    num_cat(t, player_variant_of(L_DRUMS));
    text_cat(t, ": kick low, snare mid, cymbals x");
    cv_text(6, 126, &FONT_XS, t, C_GRAY);
    cv_blit(0, Y_CHORD);
}

static void draw_notation_view(int32_t now, const struct pinfo *pi)
{
    const struct song_stream *st = layer_stream(pi->part);
    struct song_event e;
    int bass = pi->part == L_BASS, sig = key_sig_now();
    int32_t bot = ST_TOP + 4 * ST_SP, i, ppq = (int32_t)ui.s->ppq;
    struct snote nt[24];
    uint32_t nn = 0, k, last = 0xFFFFFFFFu;
    cv_begin(240, H_VIEW, C_BLACK);
    for (i = 0; i < 5; i++)
        cv_rect(4, ST_TOP + i * ST_SP, 236, 1, C_GRAY);
    scroll_grid(now, ST_TOP, bot + 1, 0);
    scroll_chords(now, 4, ST_CLIP);
    if (st)
        for (k = song_seek(st, (uint32_t)(scroll_from(now) > 0 ? scroll_from(now) : 0)); k < st->count && nn < 24u;
             k++) {
            uint32_t n, end, len, q;
            int acc;
            int32_t x, step, y;
            uint16_t c;
            song_event_get(st, k, &e);
            if ((int32_t)e.tick > scroll_to(now))
                break;
            if (e.status != 0x90u)
                continue;
            if (e.tick == last)          /* (two notes at once in the part: the first) */
                continue;
            last = e.tick;
            n = moved(e.d1);
            end = note_end(st, k, e.d1, e.tick);
            len = (end - e.tick) * 8u / (uint32_t)ppq;   /* in 32nd notes */
            /* the nearest simple value, in eighths: 8 whole, 6 dotted half, 4 half, 3 dotted quarter, 2, 1 */
            q = len >= 28u ? 8u : len >= 20u ? 6u : len >= 14u ? 4u : len >= 10u ? 3u : len >= 6u ? 2u : 1u;
            x = scroll_x((int32_t)e.tick, now) + 10;
            if (x < ST_CLIP + 4 || x > 236)
                continue;
            step = staff_step(bass ? n + 12u : n, bass, &acc);   /* (M5: the bass written an octave up, as bass
                                                               * guitar music is) */
            y = bot - step * ST_SP / 2;
            c = (int32_t)end <= now ? C_DIM : (int32_t)e.tick <= now ? C_HI : C_WHITE;
            for (i = -2; i >= step; i -= 2)  /* ledger lines below and above */
                cv_rect(x - 9, bot - i * ST_SP / 2, 18, 1, C_GRAY);
            for (i = 10; i <= step; i += 2)
                cv_rect(x - 9, bot - i * ST_SP / 2, 18, 1, C_GRAY);
            if (acc != sig_acc(step, sig, bass))
                draw_acc(x - 18, y, acc, c);
            draw_head(x, y, q >= 4u, c);
            if (q == 3u || q == 6u)
                cv_rect(x + 8, y - 1, 3, 3, c);
            if (SV(S_NOTE_NAMES)) {
                char nm[4] = "";
                text_cat(nm, names()[n % 12u]);
                cv_text(x - text_w(&FONT_XS, nm) / 2, bot + 26, &FONT_XS, nm, c == C_WHITE ? C_GRAY : c);
            }
            nt[nn].x = x;                /* q: the value, 16: on a beat, 32: on its "and" */
            nt[nn].y = y;
            nt[nn].step = step;
            nt[nn].c = c;
            nt[nn++].q = q | (e.tick % (uint32_t)ppq == 0u ? 16u : 0u) | (e.tick % (uint32_t)ppq * 2u == (uint32_t)ppq ?
                                                                         32u : 0u);
        }
    for (k = 0; k < nn; k++) {           /* stems, flags and beams: eighths on a beat and its "and" are a pair */
        uint32_t q = nt[k].q & 15u;
        int pair = q == 1u && (nt[k].q & 16u) && k + 1u < nn && (nt[k + 1u].q & 15u) == 1u && (nt[k + 1u].q & 32u) &&
                   nt[k + 1u].x - nt[k].x < 60;
        int32_t up, sx, sy, sx2, sy2, top;
        if (q == 8u)
            continue;                    /* a whole note: no stem */
        if (pair) {
            up = nt[k].step + nt[k + 1u].step < 8;   /* below the middle line on average: stems up */
            sx = up ? nt[k].x + 5 : nt[k].x - 6;
            sx2 = up ? nt[k + 1u].x + 5 : nt[k + 1u].x - 6;
            sy = nt[k].y;
            sy2 = nt[k + 1u].y;
            top = up ? (sy < sy2 ? sy : sy2) - 28 : (sy > sy2 ? sy : sy2) + 28;
            cv_rect(sx, up ? top : sy, 2, up ? sy - top : top - sy, nt[k].c);
            cv_rect(sx2, up ? top : sy2, 2, up ? sy2 - top : top - sy2, nt[k + 1u].c);
            cv_rect(sx, up ? top : top - 3, sx2 - sx + 2, 4, nt[k + 1u].c);
            k++;
            continue;
        }
        up = nt[k].step < 4;
        sx = up ? nt[k].x + 5 : nt[k].x - 6;
        cv_rect(sx, up ? nt[k].y - 28 : nt[k].y, 2, 28, nt[k].c);
        if (q == 1u) {                   /* a lone eighth: a flag */
            int32_t fy = up ? nt[k].y - 28 : nt[k].y + 28;
            cv_line(sx + 1, fy, sx + 7, fy + (up ? 10 : -10), nt[k].c);
            cv_line(sx + 2, fy, sx + 8, fy + (up ? 10 : -10), nt[k].c);
        }
    }
    staff_head(ST_TOP, ST_SP, bass, sig);
    cv_rect(ST_NOW, ST_TOP - 10, 2, 4 * ST_SP + 22, C_AMB);  /* the now line */
    cv_rect(ST_NOW - 3, ST_TOP - 14, 8, 2, C_AMB);
    cv_rect(ST_NOW - 2, ST_TOP - 12, 6, 2, C_AMB);
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

/* the layers (M4, D027): the level and FULL / QUIET, or MUTE; your part with an amber bar on top and a double frame */
/* M6: the layer boxes' colours (each text at least 4.5:1 on its box: tools/m6_test.py reads these lines) */
#define LB_QUIET_BG RGB(64, 46, 14)      /* QUIET: a dark amber box ... */
#define LB_QUIET_FG RGB(255, 214, 150)   /* ... and light amber text (M5's grey on brown: 1.5:1, unreadable) */
#define LB_NONE_FG RGB(128, 128, 136)    /* a layer the song has none of (on black) */
static void draw_layers(void)
{
    uint32_t l, mine = yours();
    cv_begin(240, H_LAYERS, C_BLACK);
    for (l = 0; l < SONG_LAYERS; l++) {
        int32_t x = 4 + (int32_t)l * 59;
        const struct song_layer *ly = &ui.s->layer[l];
        int on = !(ui.muted >> l & 1u);
        char t[8] = "L";
        if (!ly->variants) {
            int you = bt.on && l == L_MELODY;   /* (M8: BACKTRACK: the melody is the player's) */
            frame_rect(x, 1, 55, H_LAYERS - 2, you && l == mine ? C_HI : C_LINE);
            text_in(x, 55, 2, &FONT_XS, LAYER_SHORT[l], you ? (l == mine ? C_HI : C_WHITE) : LB_NONE_FG);
            text_in(x, 55, 16, &FONT_XS, you ? "YOU" : "-", you ? C_AMB : LB_NONE_FG);
            continue;
        }
        if (on)
            cv_rect(x, 3, 55, H_LAYERS - 4, ui.vol[l] == VOL_QUIET ? LB_QUIET_BG : C_LINE);
        else
            frame_rect(x, 3, 55, H_LAYERS - 4, C_AMB);
        if (l == mine) {                 /* your part */
            cv_rect(x, 0, 55, 2, C_HI);
            frame_rect(x + 1, 4, 53, H_LAYERS - 6, C_HI);
        }
        num_cat(t, player_variant_of(l) > ly->variants ? ly->variants : player_variant_of(l));
        if (bt.on) {                     /* M9: the tier now playing: the level and the section's energy (a chorus: one more) */
            uint32_t tier = player_variant_of(l) + ui.s->energy[song_bar_section(ui.s, bar_at(player_tick(0)))];
            t[1] = (char)('0' + (tier > ly->variants ? ly->variants : tier));
            t[2] = 0;
            if (l == 0)
                player_stats.bt_tier_now = tier > ly->variants ? ly->variants : tier;
        }
        text_in(x, 55, 3, &FONT_XS, LAYER_SHORT[l], l == mine ? C_HI : on ? C_WHITE : C_AMB);
        if (!on) {
            text_in(x, 55, 16, &FONT_XS, "MUTE", C_AMB);
        } else {                         /* M5: the level and the volume apart, in two colours */
            const char *vtx = ui.vol[l] == VOL_QUIET ? "QUIET" : "FULL";
            int32_t w = text_w(&FONT_XS, t) + 5 + text_w(&FONT_XS, vtx), x0 = x + (55 - w) / 2;
            cv_text(x0, 16, &FONT_XS, t, C_HI);
            cv_text(x0 + text_w(&FONT_XS, t) + 5, 16, &FONT_XS, vtx, ui.vol[l] == VOL_QUIET ? LB_QUIET_FG : C_GRAY);
        }
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
        text_cat(u, harm_level_name());
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
    if (bt.on) {                         /* M9: BACKTRACK: no loops, no key mode */
        if (st == PL_STOPPED)
            return (plat_ms() / HINT_PAGE_MS) & 1u ? "FX-LFO: pick part|again: volume" : "PLAY start|HOME setup|GLO help";
        if (st == PL_PAUSED)
            return "PLAY go on|HOME stop|SEQ band kit|GLO help";
        return "PLAY pause|HOME stop|SEQ band kit|GLO help";   /* M10.3: SEQ steps the band kit (EDIT's view is in the help) */
    }
    if (ui.tempomode_until)              /* M6 */
        return "SELECT tempo|REC done";
    if (view_zooms(ui.pview) && st != PL_STOPPED && (plat_ms() / HINT_PAGE_MS) % 3u == 2u)
        return "SELECT zoom|REC tempo|EDIT view";
    if (ui.loop_step == 1)
        return "ALGO bar A|PRESETS section|SEQ set|HOME cancel";
    if (ui.loop_step == 2)
        return "ALGO bar B|PRESETS section|SEQ set|HOME back";
    if (st == PL_PLAYING || st == PL_COUNT_IN)
        return ui.loop == LOOP_BARS ? "PLAY pause|SEQ loop off|EDIT view|GLO help" : ui.loop == LOOP_SONG ?
               "PLAY pause|SEQ set A-B|EDIT view|GLO help" : "PLAY pause|EDIT view|SEQ loop|GLO help";
    if (ui.loop == LOOP_BARS && st == PL_STOPPED)
        return "PLAY from A|SEQ loop off|EDIT view|GLO help";
    if (st == PL_STOPPED) {              /* M10.2: three pages (the full line does not fit one row at FONT_XS without cutting words) */
        uint32_t pg = (plat_ms() / HINT_PAGE_MS) % 3u;
        if (pg == 1u)
            return "FX-LFO: pick part|again: volume";   /* M5: the layer buttons, in short (M10.1: D066) */
        if (pg == 2u)
            return ui.loop == LOOP_SONG ? "SEQ set A-B|SELECT tempo|EDIT view|GLO help" : "SEQ loop|SELECT tempo|EDIT view|GLO help";
        return "PLAY|ALGO start|PRESETS key|GLO help";
    }
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
    pi->nnow = pi->nnx = pi->nctx = 0;
    pi->cue = 0;
    if (bt.on && l == L_MELODY) {        /* M8: BACKTRACK's melody (the player's): the chords, for the scale views */
        if (k >= 0)
            text_cat(pi->big, chord_name((uint32_t)k));
        if (next >= 0)
            text_cat(pi->next, chord_name((uint32_t)next));
        return;
    }
    if (l == L_HARM) {
        uint32_t kc = st == PL_STOPPED || k < 0 ? (uint32_t)(next >= 0 ? next : 0) : (uint32_t)k;
        if (k >= 0)
            text_cat(pi->big, chord_name((uint32_t)k));
        if (next >= 0)
            text_cat(pi->next, chord_name((uint32_t)next));
        if (st != PL_STOPPED)
            pi->nnow = voicing(kc, pi->now);
        if (st != PL_STOPPED && harm_broken()) {   /* L4, L5: the note just played; the chord around it darker */
            uint8_t a[SONG_VOICING_NOTES];
            uint32_t na = attack_notes(L_HARM, tb, 0, a, SONG_VOICING_NOTES, &at), t0, t1, kk, j;
            const struct song_stream *hs = layer_stream(L_HARM);
            struct song_event he;
            t0 = song_cevt_tick(ui.s, kc);
            t1 = kc + 1u < ui.s->ncevt ? song_cevt_tick(ui.s, kc + 1u) : ui.s->end_tick;
            pi->nctx = 0;
            for (kk = hs ? song_seek(hs, t0) : 0; hs && kk < hs->count && pi->nctx < 10u; kk++) {
                uint8_t m;
                song_event_get(hs, kk, &he);
                if (he.tick >= t1)
                    break;
                if (he.status != 0x90u)
                    continue;
                m = (uint8_t)moved(he.d1);
                for (j = 0; j < pi->nctx && pi->ctx[j] != m; j++)
                    ;
                if (j == pi->nctx)
                    pi->ctx[pi->nctx++] = m;
            }
            for (i = 0, pi->nnow = 0; i < na; i++)
                pi->now[pi->nnow++] = (uint8_t)moved(a[i]);
        }
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

#include "btview.c"

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
        } else {                         /* the count-in: the beat in the bar (M10.2: 1 2 3 4 | 1 2 3, not the beats left), the first chord next */
            int32_t bt = (int32_t)s->bar_ticks;
            count = ((tc % bt) + bt) % bt / (int32_t)s->ppq + 1;   /* (the bar lines are at multiples of bar_ticks, as the clicks) */
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
    {                                    /* M8: the scale views' marking (BACKTRACK's melody) */
        uint32_t sc = 0, tn = 0, nx = 0;
        if (bt.on && pi.part == L_MELODY) {
            sc = bt_scale_mask();
            tn = chord_pcs(st == PL_STOPPED ? next : k);
            nx = left == 1 && next >= 0 && next != k ? chord_pcs(next) : 0;   /* (its last beat) */
        }
        player_stats.scale_mask = sc;
        player_stats.tone_mask = tn;
        player_stats.next_mask = nx;
        if (bt.on)
            player_stats.bt_energy = ui.s->energy[song_bar_section(ui.s, (uint32_t)(bar < 0 ? 0 : bar) <
                                                  s->bar_count ? (uint32_t)bar : s->bar_count - 1u)];
    }
    if (pi.part != L_HARM)
        left = 0;                        /* (the countdown is the chord's) */
    bright = (pi.cue >> 4) & 1u;
    /* the key lights: your part's keys, bright at each attack, darker between (M3: blinking in its rhythm); the next
     * chord dim in the last bar before it. The drums: none (the beat light is enough) */
    ui.lit_keys = ui.mid_keys = ui.dim_keys = 0;
    if (bt.on && pi.part == L_MELODY) {  /* M8: the chord's tones bright, the scale's notes at the middle level, the next
                                          * chord's dim in its last beat */
        if (SV(S_LIGHTS) && st != PL_STOPPED) {
            ui.lit_keys = keys_of(player_stats.tone_mask);
            ui.mid_keys = keys_of(player_stats.scale_mask & ~player_stats.tone_mask);
            ui.dim_keys = keys_of(player_stats.next_mask) & ~ui.lit_keys;
        }
    } else if (SV(S_LIGHTS) && pi.part != L_DRUMS && st != PL_STOPPED) {   /* (M4: muted or not; the setting) */
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
        for (i = 0; i < pi.nctx; i++)    /* L4, L5: the chord around the note, at the middle level */
            ui.mid_keys |= key_bit(pi.ctx[i]) & ~ui.lit_keys;
        if (ui.lit_keys && !ui.flash_keys && st == PL_PLAYING)
            player_stats.key_flashes++;
        ui.flash_keys = ui.lit_keys != 0;
    }
    player_stats.part = pi.part;
    player_stats.vols = (uint32_t)ui.vol[0] | (uint32_t)ui.vol[1] << 2 | (uint32_t)ui.vol[2] << 4 | (uint32_t)ui.vol[3] << 6;
    {
        uint32_t l, q = 0;
        for (l = 0; l < SONG_LAYERS; l++)
            q |= (uint32_t)(ui.vol[l] == VOL_QUIET) << l;
        player_quiet(q, QUIET_PCT[SV(S_QUIET) % 2u]);
    }
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.sh_top_key = 0xFFFFFFFFu;
    }
    big_key = hash(pi.big, hash(pi.next, hash(pi.small, 2166136261u))) ^ pi.cue ^ (uint32_t)st << 5 ^
              (uint32_t)ui.loop_step << 14 ^ (ui.loop_step ? ui.a_bar << 16 ^ ui.b_bar << 22 : 0u) ^
              (uint32_t)count << 8 ^ (uint32_t)(k + 1) << 12 ^ pi.part << 28;
    if (ui.pview != PV_CHORD) {          /* the other views: the chord band, strip and cursor's place */
        int scroll = view_zooms(ui.pview);
        if (scroll)
            scroll_setup(ui.pview);
        view_key = big_key ^ (uint32_t)left << 22 ^ (uint32_t)(ui.pview == PV_CHART ? bar : 0) << 3 ^
                   (uint32_t)bright << 30 ^ (uint32_t)ui.pview << 26 ^ (uint32_t)(next + 1) << 18;
        if (ui.pview == PV_SCALE_KB || ui.pview == PV_SCALE_FB)   /* (M8: the marking) */
            view_key ^= (player_stats.scale_mask ^ player_stats.tone_mask << 12 ^ player_stats.next_mask << 20) *
                        2654435761u ^ SV(S_UKE) << 31;
        if (scroll)                      /* the scrolling views: a redraw for each pixel the song moves */
            view_key ^= (uint32_t)(tb * (240 - ui.sc_x0) / scroll_span()) * 2654435761u;
        if (ui.redraw || view_key != ui.sh_view) {
            if (st == PL_COUNT_IN && count > 0)
                draw_count_view(count, &pi);
            else if (ui.pview == PV_SCALE_KB)   /* M8 */
                draw_scale_kb(&pi, player_stats.scale_mask, player_stats.tone_mask, player_stats.next_mask);
            else if (ui.pview == PV_SCALE_FB)
                draw_scale_fb(&pi, player_stats.scale_mask, player_stats.tone_mask, player_stats.next_mask);
            else if (ui.pview == PV_KEYS)
                draw_keys_view(st, &pi, left, bright || st != PL_PLAYING || !ui.blink, st == PL_STOPPED ? next : k);
            else if (ui.pview == PV_GUITAR)
                draw_guitar_view(st, &pi, k, next);
            else if (ui.pview == PV_TAB)
                draw_tab_view(tb, &pi);
            else if (ui.pview == PV_ROLL || ui.pview == PV_BARS)
                draw_roll_view(tb, &pi);
            else if (ui.pview == PV_MTAB)
                draw_tab_view(tb, &pi);
            else if (ui.pview == PV_NOTATION && pi.part == L_HARM)
                draw_harmony_notation(tb);
            else if (ui.pview == PV_NOTATION)
                draw_notation_view(tb, &pi);
            else if (ui.pview == PV_DNOTE)
                draw_drum_notation(tb);
            else if (ui.pview == PV_GRID)
                draw_grid_view(tb);
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
              (uint32_t)ui.paused_start << 31 ^ (pi.part + 1u) << 6 ^
              (uint32_t)ui.loop_step << 29 ^ ui.a_bar * 7u ^ ui.b_bar * 131u ^ ui.from_bar * 8191u;
    if (bt.on)                           /* M10: the section indicator (its name, blinking, phase) */
        top_key ^= bt_ind_update(st) * 2654435761u;
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
    {
        int32_t lk = (int32_t)(ui.muted | player_variant_of(0) << 4 | player_variant_of(1) << 7 |
                               player_variant_of(2) << 10 | player_variant_of(3) << 13 | yours() << 16 |
                               (uint32_t)ui.vol[0] << 18 | (uint32_t)ui.vol[1] << 20 | (uint32_t)ui.vol[2] << 22 |
                               (uint32_t)ui.vol[3] << 24);
        if (bt.on)                       /* M9: the tier shown moves with the section's energy (a chorus: one more) */
            lk ^= (int32_t)(ui.s->energy[song_bar_section(ui.s, bar_at(player_tick(0)))] << 27);
        if (ui.redraw || lk != ui.sh_layers) {
            draw_layers();
            ui.sh_layers = lk;
        }
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
static void ui_reset_done(void)
{
    ui_busy("FACTORY RESET DONE", "SWITCH OFF AND ON");
    ui.view = V_RESET;                   /* (input() and app_frame() then do nothing more) */
}

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
    settings_marker();                   /* M10.2: Pop-ups ON after an update */
    upreset_boot();                      /* M10: the user presets (a read; never a write at start-up) */
    ui.splash = plat_ms() < SPLASH_MS;   /* (after a restore or a resource install: no splash, straight back) */
    ui.view = ui.splash ? V_MODE : V_LIST;   /* M5: after the splash, LEARN or SYNTH (no splash: learning, as before) */
    player_stats.mode = ui.splash ? 0 : 1u;
    if (!ui.splash)
        lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
    ui.sh_list = 0xFFFFFFFFu;
    player_stats.key_note_last = 0xFFu;
    player_stats.ui_view = 16u;
    /* M5.1: GLO + HOME held at power-on: WIPE SONG STORE, asked first (before anything reads the store; not a
     * rescue combination: those are REC, and OCT- + OCT+, fm1-base's) */
    if ((plat_buttons() & (1u << B_GLO | 1u << B_HOME)) == (1u << B_GLO | 1u << B_HOME)) {
        ui.splash = 0;
        player_stats.mode = 0;
        wipe_open(V_MODE);
    }
}

/* the start-up melody: each note on and off at its time (while the splash shows) */
static void splash_tune(uint32_t now)
{
    if (ui.tune_on == 0 && ui.tune_off == 0) {   /* the sound: electric piano, a little reverb, softer than a song */
        synth_midi(0xB0u | TUNE_CH, 121u, 0);
        synth_midi(0xC0u | TUNE_CH, 4u, 0);
        synth_midi(0xB0u | TUNE_CH, 7u, 80u);
        synth_midi(0xB0u | TUNE_CH, 91u, 70u);
    }
    while (ui.tune_on < 5u && now >= TUNE[ui.tune_on][0]) {
        synth_midi(0x90u | TUNE_CH, (uint8_t)TUNE[ui.tune_on][1], ui.tune_on == 4u ? 70u : 84u);
        ui.tune_on++;
        player_stats.splash_notes++;
    }
    while (ui.tune_off < ui.tune_on && now >= TUNE[ui.tune_off][0] + TUNE[ui.tune_off][2]) {
        synth_midi(0x80u | TUNE_CH, (uint8_t)TUNE[ui.tune_off][1], 0);
        ui.tune_off++;
    }
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    if (ui.splash || ui.tune_off < ui.tune_on)
        splash_tune(now);
    if (ui.splash && now >= SPLASH_MS) { /* the splash has had its time: the list */
        ui.splash = 0;
        ui.redraw = 1;
    }
    if (lib_poll()) {                    /* M5.1: room being made (a step at a time; the screen shows it) */
        ui.redraw = 1;
        return;
    }
    if (ui.view == V_RESET) {            /* M10.1: FACTORY RESET DONE stays on the screen */
        player_stats.ui_view = (uint32_t)V_RESET;
        return;
    }
    if (ui.lib_seen != lib_changes && ui.view != V_MODE && ui.view != V_SYNTH && ui.view != V_WIPE) {   /* songs sent or deleted: back to
                                                                                  * the list, from the top */
        ui.lib_seen = lib_changes;
        ui.view = bt.on ? V_SETUP : V_LIST;   /* (M8: BACKTRACK: its setup, the list again) */
        if (bt.on)
            bt_list_build();
        ui.sel = ui.top = 0;
        ui.redraw = 1;
    }
    if (lm.on)                           /* M10.1: SONG LEARN's melody synth: its voices started (for the tests) */
        player_stats.synth_notes = vsynth_stats()->notes;
    input();
    ann_tick(now);
    player_stats.ui_view = (uint32_t)ui.view + (ui.splash ? 16u : 0u);
    if (ui.splash)
        return;
    if (ui.popup) {                      /* M7: the pop-up over the screen (frozen under it until it closes) */
        if (ui.popup == 1u)
            draw_popup();
        ui.popup = 2;
        return;
    }
    if (vpop.open) {                     /* M10: the value pop-up (the screen under it frozen) */
        if (vpop.dirty)
            draw_vpop();
        vpop.dirty = 0;
        return;
    }
    if (ui.view == V_MODE || ui.view == V_SYNTH) {
        if (ui.view == V_MODE ? ui.redraw : synth_dirty(now))
            ui.view == V_MODE ? draw_mode() : draw_synth();
        ui.redraw = 0;
        return;
    }
    if (ui.view == V_WIPE) {
        uint32_t k = ui.wipe_t0 ? (now - ui.wipe_t0) / 1000u + 1u : 0;
        if (ui.redraw || k != ui.sh_list) {
            draw_wipe(now);
            ui.sh_list = k;
        }
        ui.redraw = 0;
        return;
    }
    if (ui.view == V_VIEWSET) {
        if (ui.redraw)
            draw_viewset();
        ui.redraw = 0;
        return;
    }
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
    if (ui.view == V_SETUP) {            /* M8 */
        uint32_t key = bt_hash();
        if (ui.redraw || key != bt.shown) {
            if (ui.redraw)
                lcd_fill(0, 0, 240, 240, C_BLACK);
            draw_setup();
            bt.shown = key;
        }
        ui.redraw = 0;
        return;
    }
    if (ui.view == V_LIST) {
        uint32_t key = ui.sel | (now < ui.msg_until) << 16 | ui.fsel << 20 | (uint32_t)(ui.folder + 1) << 26;
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
