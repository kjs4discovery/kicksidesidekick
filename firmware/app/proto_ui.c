/* Licence: TBD (see D017) */
/* PROTOTYPE (Step 0.7): the play-along screen and controls for the one built-in song (proto_player.h).
 * Included by app.c, in fm1-base's unity build after the platform (gfx.c's cv_*, FONT_*, C_*; announce.c).
 * Phase 1 replaces it. The 0.6 hello world is commit 4629478 (tools/build_hello.sh builds it).
 *
 *   PLAY            play from the top / stop
 *   FX SEL ENV LFO  mute or unmute drums, bass, harmony, melody (lit: the layer plays)
 *   MASTER          volume (the platform's gain)
 *   the keys        Electric Piano 1 on channel 16, over the song
 * Left alone, for the platform's rescues: REC, OCT- and OCT+ (at power-on), and nothing is held at start-up. */
#include "synth.h"
#include "proto_player.h"

#define KEY_LOW 53                       /* key 0 is F3 (plat.h) */
#define VELOCITY 100u

static const uint8_t LAYER_BUTTON[PP_NLAYERS] = {B_FX, B_SEL, B_ENV, B_LFO};
static const char *const LAYER_SHORT[PP_NLAYERS] = {"DRUMS", "BASS", "HARM", "MELODY"};

static struct {
    uint32_t keys, buttons;              /* held at the last input */
    uint32_t muted;                      /* bit PP_*: muted layers */
    char msg[2][24];                     /* a short message, for a moment */
    uint32_t msg_until;
    uint8_t flash_ok;                    /* the platform accepted the flash chip (else RAM only) */
    /* what is on the screen, to redraw a band only when it changes */
    int shown_state;
    uint32_t shown_beat;                 /* bar * 16 + beat */
    const char *shown_chord, *shown_next;
    uint32_t shown_muted, shown_stats_ms;
    uint8_t redraw;                      /* everything */
} ui = {.shown_state = -2};

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
    ui.shown_stats_ms = ui.msg_until - 2000u;   /* the top band now */
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

/* ------------------------------------------------------------------------ input --- */
static void lights(void)
{
    uint32_t lit = 0, dim = 0, l;
    for (l = 0; l < PP_NLAYERS; l++) {
        dim |= 1u << LAYER_BUTTON[l];
        if (!(ui.muted >> l & 1u))
            lit |= 1u << LAYER_BUTTON[l];
    }
    dim |= 1u << B_PLAY;
    if (pp_state() == PP_PLAYING)
        lit |= LED_PLAY_GREEN;
    plat_lights(lit, ui.keys, dim, 0, 1);
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), down = btn & ~ui.buttons, k, l;
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        uint8_t note = (uint8_t)(KEY_LOW + (int32_t)k);
        if ((keys & bit) && !(ui.keys & bit))
            synth_midi(0x90u | SYNTH_KEYS_CH, note, (uint8_t)VELOCITY);
        else if (!(keys & bit) && (ui.keys & bit))
            synth_midi(0x80u | SYNTH_KEYS_CH, note, 0);
    }
    ui.keys = keys;
    ui.buttons = btn;
    if (down >> B_PLAY & 1u)
        pp_play(pp_state() != PP_PLAYING);
    for (l = 0; l < PP_NLAYERS; l++)
        if (down >> LAYER_BUTTON[l] & 1u) {
            ui.muted ^= 1u << l;
            pp_mute(ui.muted);
            app_say(pp_layer_name((int)l), ui.muted >> l & 1u ? "MUTED" : "ON");
        }
    lights();
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------------ screen --- */
static void text_center(int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text((240 - text_w(f, s)) / 2, y, f, s, c);
}

/* text at twice a font's size (the chord), on a black canvas: each pixel a 2x2 block, its coverage as brightness */
static void text2x_center(int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    uint32_t r = c >> 11, g = (c >> 5) & 63u, b = c & 31u;
    int32_t x = (240 - 2 * text_w(f, s)) / 2;
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

static void draw_top(uint32_t bar, uint32_t beat, int playing)
{
    char t[32];
    uint32_t i, bpb = pp_beats_per_bar();
    cv_begin(240, 40, C_BLACK);
    cv_text(6, 3, &FONT_XS, pp_title(), C_GRAY);
    t[0] = 0;
    if (plat_ms() < ui.msg_until) {      /* a message (the platform's, or ours) where the numbers are */
        text_cat(t, ui.msg[0]);
        text_cat(t, " ");
        text_cat(t, ui.msg[1]);
        cv_text(6, 21, &FONT_XS, t, C_WHITE);
    } else if (pp_state() == PP_FAILED) {
        text_cat(t, "SONG ERROR ");
        num_cat(t, (uint32_t)-pp_error());
        cv_text(6, 21, &FONT_XS, t, C_AMB);
    } else {
        text_cat(t, ui.flash_ok ? "" : "RAM ONLY  ");
        text_cat(t, "CPU ");
        num_cat(t, plat_cpu_pct());
        text_cat(t, "%  V ");
        num_cat(t, pp_voices());
        text_cat(t, "/");
        num_cat(t, pp_voices_max());
        cv_text(6, 21, &FONT_XS, t, ui.flash_ok ? C_DIM : C_AMB);
    }
    t[0] = 0;                            /* bar:beat, right */
    if (playing) {
        num_cat(t, bar);
        text_cat(t, ":");
        num_cat(t, beat + 1u);
    } else {
        text_cat(t, "STOP");
    }
    cv_text(234 - text_w(&FONT_M, t), 2, &FONT_M, t, playing ? C_WHITE : C_GRAY);
    for (i = 0; i < bpb && i < 8u; i++)  /* the beats of the bar, this one lit */
        cv_rect(234 - (int32_t)(bpb - i) * 12 + 4, 31, 8, 5, playing && i == beat ? C_HI : C_LINE);
    cv_blit(0, 0);
}

/* the chords in two blits, the big one first: what the player reads must reach the LCD soonest (a blit's transfer
 * takes ~0.32 ms a row) */
static void draw_chords(const char *now, const char *next, int playing)
{
    cv_begin(240, 84, C_BLACK);
    if (!playing && pp_state() != PP_FAILED) {
        text_center(6, &FONT_XS, "v" FW_VERSION, C_DIM);
        text_center(30, &FONT_M, "PRESS PLAY", C_GRAY);
        text_center(64, &FONT_XS, "FX SEL ENV LFO: MUTE A LAYER", C_DIM);
    } else if (now) {
        text2x_center(8, &FONT_L, now, C_HI);
    }
    cv_blit(0, 40);
    cv_begin(240, 52, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    if (pp_state() != PP_FAILED) {
        cv_text(10, 20, &FONT_XS, playing ? "NEXT" : "FIRST", C_DIM);
        if (next)
            cv_text(60, 14, &FONT_M, next, C_AMB);
        else if (playing)
            cv_text(60, 20, &FONT_XS, "END", C_DIM);
    }
    cv_blit(0, 124);
}

static void draw_layers(void)
{
    uint32_t l;
    cv_begin(240, 64, C_BLACK);
    for (l = 0; l < PP_NLAYERS; l++) {
        int32_t x = 4 + (int32_t)l * 59;
        int on = !(ui.muted >> l & 1u);
        cv_rect(x, 6, 55, 50, on ? C_LINE : C_BLACK);
        if (!on) {                       /* muted: an outline */
            cv_rect(x, 6, 55, 1, C_AMB);
            cv_rect(x, 55, 55, 1, C_AMB);
            cv_rect(x, 6, 1, 50, C_AMB);
            cv_rect(x + 54, 6, 1, 50, C_AMB);
        }
        cv_text(x + (55 - text_w(&FONT_XS, LAYER_SHORT[l])) / 2, 14, &FONT_XS, LAYER_SHORT[l],
                on ? C_WHITE : C_AMB);
        cv_text(x + (55 - text_w(&FONT_XS, on ? "ON" : "MUTE")) / 2, 34, &FONT_XS, on ? "ON" : "MUTE",
                on ? C_HI : C_AMB);
    }
    cv_blit(0, 176);
}

void app_ui_init(void)
{
    ui.flash_ok = plat_data(PLAT_DATA_LO) != 0;   /* 0: the platform refused the chip (fm1-base main_fm1.c) */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    uint32_t now = plat_ms(), tick, bar, beat, start, bpb = pp_beats_per_bar(), ppq = pp_ppq();
    const char *chord, *next;
    int st, playing;
    input();
    ann_tick(now);
    st = pp_state();
    playing = st == PP_PLAYING;
    tick = pp_tick();
    chord = pp_chord(tick, &next, &start);
    if (!playing) {                      /* stopped: the first chord after the pickup is "next" */
        chord = 0;
        pp_chord(0, &next, 0);
    }
    bar = tick / (ppq * bpb);
    beat = tick / ppq % bpb;
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.shown_stats_ms = now - 1000u;  /* the numbers back */
    }
    /* the chord first: it is what must not be late */
    if (ui.redraw || st != ui.shown_state || chord != ui.shown_chord || next != ui.shown_next) {
        draw_chords(chord, next, playing);
        ui.shown_chord = chord;
        ui.shown_next = next;
    }
    if (ui.redraw || st != ui.shown_state || (bar * 16u + beat) != ui.shown_beat || now - ui.shown_stats_ms >= 250u) {
        draw_top(bar, beat, playing);
        ui.shown_beat = bar * 16u + beat;
        ui.shown_stats_ms = now;
    }
    if (ui.redraw || ui.muted != ui.shown_muted) {
        draw_layers();
        ui.shown_muted = ui.muted;
    }
    ui.shown_state = st;
    ui.redraw = 0;
}
