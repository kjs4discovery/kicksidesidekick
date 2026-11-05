/* Licence: TBD (see D017) */
/* The screens and controls (M1: simple; step 1.6 designs the real ones). docs/controls.md is the map.
 * Included by app.c, in fm1-base's unity build after the platform (gfx.c's cv_*, FONT_*, C_*; lcd.c; announce.c).
 *
 * Song list: SELECT turns the cursor, PLAY (or EDIT) opens the song.
 * Play screen: PLAY play/stop; FX SEL ENV LFO mute drums/bass/harmony/melody; K1..K4 their level (variant);
 *   SELECT tempo 50..120 %; ARP count-in on/off; SEQ loop off / song / section; HOME back to the list.
 * Both: MASTER volume, the keys play Electric Piano 1 on channel 16.
 * Left alone, for the platform's rescues: REC, OCT- and OCT+ (at power-on), and nothing is held at start-up. */
#include "synth.h"
#include "player.h"
#include "song.h"
#include "songs_gen.h"

#define KEY_LOW 53                       /* key 0 is F3 (plat.h) */
#define VELOCITY 100u
#define LIST_ROWS 5u
#define LIST_ROW_H 38

/* the bands of the play screen, top to bottom */
#define Y_TOP 0
#define H_TOP 32
#define Y_CHORD 32
#define H_CHORD 80
#define W_BIG 176                        /* the big chord's part of the chord band; the rest: next + countdown */
#define Y_STRIP 112
#define H_STRIP 52
#define Y_CURSOR 164
#define H_CURSOR 6
#define Y_LAYERS 170
#define H_LAYERS 44
#define Y_BOTTOM 214
#define H_BOTTOM 26
#define BOX_W 56
#define BOX_STEP 60

static const uint8_t LAYER_BUTTON[SONG_LAYERS] = {B_FX, B_SEL, B_ENV, B_LFO};
static const uint8_t LAYER_ENC[SONG_LAYERS] = {EN_K1, EN_K2, EN_K3, EN_K4};
static const char *const LAYER_NAME[SONG_LAYERS] = {"DRUMS", "BASS", "HARMONY", "MELODY"};
static const char *const LAYER_SHORT[SONG_LAYERS] = {"DRUMS", "BASS", "HARM", "MELODY"};
static const char *const LOOP_NAME[] = {"", "LOOP SONG", "LOOP SECT"};

static struct song songs[SONG_FILES_N];
static int8_t song_ok[SONG_FILES_N];     /* 0 not looked at, 1 peeked (list), 2 opened, < 0 the error */

static struct {
    int screen;                          /* 0 list, 1 play */
    uint32_t sel, top;                   /* the list's cursor and first row */
    const struct song *s;
    uint32_t keys, buttons;
    uint32_t muted;
    int loop;
    char msg[2][24];
    uint32_t msg_until;
    uint8_t flash_ok;
    uint8_t redraw;
    /* what is on the screen */
    int32_t sh_chord, sh_count, sh_next, sh_left, sh_bar, sh_beat, sh_strip, sh_layers, sh_section;
    int sh_state;
    uint32_t sh_top_key, sh_stats_ms, sh_list;
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

/* ------------------------------------------------------------------- the song's facts --- */
static uint32_t bar_ticks(void) { return ui.s->bar_ticks; }

static uint32_t section_from(uint32_t i)
{
    uint32_t t = song_section_first(ui.s, i) * bar_ticks(), lead = song_section_lead(ui.s, i);
    return t > lead ? t - lead : 0;
}

static uint32_t section_at(int32_t tick)
{
    uint32_t i = 0;
    while (i + 1u < ui.s->nsect && tick >= (int32_t)section_from(i + 1u))
        i++;
    return i;
}

/* docs/song-format.md 3.3: a loop of sections runs from start - lead to end - lead of the first section's lead */
static void loop_region(int mode, int32_t tick, uint32_t *from, uint32_t *to)
{
    const struct song *s = ui.s;
    uint32_t i = mode == LOOP_SONG ? 0 : section_at(tick), last = mode == LOOP_SONG ? s->nsect - 1u : i;
    uint32_t lead = song_section_lead(s, i);
    *from = section_from(i);
    *to = (song_section_first(s, last) + song_section_bars(s, last)) * bar_ticks();
    *to = *to > lead ? *to - lead : *to;
    if (mode == LOOP_SECTION && tick >= (int32_t)*to && i + 1u < s->nsect)   /* already past this one's end */
        loop_region(mode, (int32_t)section_from(i + 1u), from, to);
}

static void set_loop(int mode)
{
    uint32_t from = 0, to = 0;
    int32_t t = player_state() == PL_STOPPED ? 0 : player_tick(PL_LEAD_BEAT_MS);
    ui.loop = mode;
    if (mode != LOOP_OFF)
        loop_region(mode, t, &from, &to);
    player_loop(mode, from, to);
}

/* ------------------------------------------------------------------------ input --- */
static void open_song(uint32_t i)
{
    int r;
    if (song_ok[i] != 2) {               /* checked at first use, not at boot */
        r = song_open(&songs[i], SONG_FILES[i].data, SONG_FILES[i].size);
        song_ok[i] = (int8_t)(r ? r : 2);
        if (r) {
            app_say("SONG ERROR", song_error_name(r));
            ui.sh_list = 0xFFFFFFFFu;
            return;
        }
    }
    ui.s = &songs[i];
    ui.muted = 0;
    ui.loop = LOOP_OFF;
    player_load(ui.s);
    player_mute(0);
    for (r = 0; r < (int)SONG_LAYERS; r++)
        player_variant((uint32_t)r, 1);
    player_loop(LOOP_OFF, 0, 0);
    ui.screen = 1;
    ui.redraw = 1;
}

static void lights(void)
{
    uint32_t lit = 0, dim = 0, l;
    if (ui.screen == 1) {
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
        if (player_state() != PL_STOPPED)
            lit |= LED_PLAY_GREEN;
    }
    dim |= 1u << B_PLAY;
    plat_lights(lit, ui.keys, dim, 0, 1);
}

static void input_list(uint32_t down)
{
    int32_t d = plat_enc(EN_SELECT);
    if (d) {
        int32_t s = (int32_t)ui.sel + d;
        ui.sel = (uint32_t)clamp(s, 0, (int32_t)SONG_FILES_N - 1);
    }
    if ((down >> B_PLAY & 1u) || (down >> B_EDIT & 1u))
        open_song(ui.sel);
}

static void input_play(uint32_t down)
{
    uint32_t l;
    int32_t d;
    if (down >> B_PLAY & 1u) {
        int on = player_state() == PL_STOPPED;
        if (on && ui.loop)
            set_loop(ui.loop);           /* from the top: a section loop takes the first section */
        player_play(on);
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
    d = plat_enc(EN_SELECT);
    if (d) {
        player_tempo((uint32_t)clamp((int32_t)player_tempo_of() + 5 * d, PL_TEMPO_MIN, PL_TEMPO_MAX));
        ui.sh_top_key = 0xFFFFFFFFu;
    }
    if (down >> B_ARP & 1u) {
        player_count_in(!player_count_in_of());
        app_say("COUNT-IN", player_count_in_of() ? "ON" : "OFF");
    }
    if (down >> B_SEQ & 1u) {
        set_loop((ui.loop + 1) % 3);
        app_say("LOOP", ui.loop == LOOP_OFF ? "OFF" : ui.loop == LOOP_SONG ? "SONG" : "SECTION");
    }
    if (down >> B_HOME & 1u) {
        player_play(0);
        ui.screen = 0;
        ui.redraw = 1;
    }
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), down = btn & ~ui.buttons, k;
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
    if (ui.screen == 0)
        input_list(down);
    else
        input_play(down);
    lights();
}

void app_input_only(void) { input(); }

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
        cv_text(80, 6, &FONT_XS, t, C_AMB);
    } else if (!ui.flash_ok) {
        cv_text(80, 6, &FONT_XS, "RAM ONLY", C_AMB);
    }
    cv_rect(0, H_TOP - 1, 240, 1, C_LINE);
    cv_blit(0, Y_TOP);
}

static void draw_list(void)
{
    uint32_t r;
    if (ui.sel < ui.top)
        ui.top = ui.sel;
    if (ui.sel >= ui.top + LIST_ROWS)
        ui.top = ui.sel - LIST_ROWS + 1u;
    draw_list_top();
    for (r = 0; r < LIST_ROWS; r++) {
        uint32_t i = ui.top + r;
        int sel = i == ui.sel;
        cv_begin(240, LIST_ROW_H, C_BLACK);
        if (i < SONG_FILES_N) {
            struct song *s = &songs[i];
            if (!song_ok[i]) {
                int e = song_peek(s, SONG_FILES[i].data, SONG_FILES[i].size);
                song_ok[i] = (int8_t)(e ? e : 1);
            }
            if (sel)
                cv_rect(2, 1, 236, LIST_ROW_H - 2, C_LINE);
            if (song_ok[i] > 0) {
                char t[40] = "";
                text_cat(t, s->key);
                text_cat(t, "   ");
                num_cat(t, s->beats_per_bar);
                text_cat(t, "/4   ");
                num_cat(t, (60000000u + s->tempo_us / 2u) / s->tempo_us);
                text_cat(t, " BPM");
                cv_text(10, 2, &FONT_B, s->title, sel ? C_WHITE : C_GRAY);
                cv_text(10, 19, &FONT_XS, t, sel ? C_HI : C_DIM);
            } else {
                char t[32] = "SONG ";
                num_cat(t, i + 1u);
                text_cat(t, ": ERROR ");
                text_cat(t, song_error_name(song_ok[i]));
                cv_text(10, 12, &FONT_XS, t, C_AMB);
            }
        }
        cv_blit(0, H_TOP + (int32_t)r * LIST_ROW_H);
    }
    cv_begin(240, 240 - H_TOP - LIST_ROWS * LIST_ROW_H, C_BLACK);
    cv_text(6, 2, &FONT_XS, "SELECT: CHOOSE   PLAY: OPEN", C_DIM);
    cv_blit(0, H_TOP + LIST_ROWS * LIST_ROW_H);
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
        text_cat(t, "%  ");
        text_cat(t, s->key);
        if (ui.loop) {
            text_cat(t, "  ");
            text_cat(t, LOOP_NAME[ui.loop]);
        }
        if (player_count_in_of())
            text_cat(t, "  CNT");
        cv_text(6, 16, &FONT_XS, t, player_tempo_of() == 100u ? C_DIM : C_AMB);
    }
    t[0] = 0;
    if (st == PL_PLAYING) {
        num_cat(t, (uint32_t)bar + s->first_bar_number);
        text_cat(t, ":");
        num_cat(t, (uint32_t)beat + 1u);
    } else {
        text_cat(t, st == PL_COUNT_IN ? "IN" : "STOP");
    }
    cv_text(234 - text_w(&FONT_M, t), 4, &FONT_M, t, st == PL_PLAYING ? C_WHITE : C_GRAY);
    cv_blit(0, Y_TOP);
}

static void draw_big(int st, int32_t k, int32_t count)
{
    const char *sym = k >= 0 ? song_cevt_symbol(ui.s, (uint32_t)k) : 0;
    cv_begin(W_BIG, H_CHORD, C_BLACK);
    if (st == PL_STOPPED) {
        text_in(0, W_BIG, 10, &FONT_XS, ui.s->title, C_GRAY);
        text_in(0, W_BIG, 32, &FONT_M, "PRESS PLAY", C_WHITE);
        text_in(0, W_BIG, 60, &FONT_XS, "HOME: SONG LIST", C_DIM);
    } else if (st == PL_COUNT_IN && count > 0) {
        char t[4] = "";
        num_cat(t, (uint32_t)count);
        text_in(0, W_BIG, 2, &FONT_XS, "COUNT-IN", C_DIM);
        text2x((W_BIG - 2 * text_w(&FONT_L, t)) / 2, 14, &FONT_L, t, C_AMB);
    } else if (sym) {
        if (2 * text_w(&FONT_L, sym) <= W_BIG - 4)
            text2x((W_BIG - 2 * text_w(&FONT_L, sym)) / 2, 8, &FONT_L, sym, C_HI);
        else
            text_in(0, W_BIG, 24, &FONT_L, sym, C_HI);
    }
    cv_blit(0, Y_CHORD);
}

static void draw_next(int st, int32_t next, int32_t left)
{
    cv_begin(240 - W_BIG, H_CHORD, C_BLACK);
    cv_rect(0, 6, 1, H_CHORD - 12, C_LINE);
    cv_text(8, 4, &FONT_XS, st == PL_STOPPED ? "FIRST" : "NEXT", C_DIM);
    if (next >= 0)
        cv_text(8, 20, &FONT_M, song_cevt_symbol(ui.s, (uint32_t)next), C_AMB);
    else if (st != PL_STOPPED)
        cv_text(8, 24, &FONT_XS, "END", C_DIM);
    if (left > 0) {                      /* the beats until the change, in the last bar before it */
        char t[8] = "IN ";
        num_cat(t, (uint32_t)left);
        cv_text(8, 50, &FONT_M, t, C_WHITE);
    }
    cv_blit(W_BIG, Y_CHORD);
}

/* bar index b as it will be heard: past a loop's end, the bars from the loop's start again */
static int32_t strip_bar(int32_t b)
{
    uint32_t from, to;
    if (player_loop_of(&from, &to) && to > from) {
        int32_t bt = (int32_t)bar_ticks(), fb = (int32_t)from / bt, tb = ((int32_t)to + bt - 1) / bt;
        if (b >= tb && tb > fb)
            b = fb + (b - tb) % (tb - fb);
    }
    return b < (int32_t)ui.s->bar_count ? b : -1;
}

static void draw_strip(int32_t bar)
{
    const struct song *s = ui.s;
    uint32_t i;
    cv_begin(240, H_STRIP, C_BLACK);
    for (i = 0; i < 4u; i++) {
        int32_t b = strip_bar(bar + (int32_t)i), x = 2 + (int32_t)i * BOX_STEP;
        char t[8] = "";
        uint32_t k, k_end, t0, n = 0;
        if (b < 0) {
            frame_rect(x, 0, BOX_W, H_STRIP, C_LINE);
            continue;
        }
        frame_rect(x, 0, BOX_W, H_STRIP, i == 0 ? C_HI : C_DIM);
        num_cat(t, (uint32_t)b + s->first_bar_number);
        cv_text(x + 4, 1, &FONT_XS, t, C_DIM);
        t0 = (uint32_t)b * s->bar_ticks;
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
            const char *sym = song_cevt_symbol(s, k);
            int held = a == 0 && !(song_bar_flags(s, (uint32_t)b) & 1u);   /* goes on from the bar before */
            const felucca_font_t *f = n == 1u && text_w(&FONT_M, sym) <= sw ? &FONT_M : &FONT_XS;
            if (a)
                cv_rect(sx, 16, 1, H_STRIP - 20, C_DIM);
            text_in(sx, sw, n == 1u ? 22 : 26, f, sym, held ? C_DIM : i == 0 ? C_WHITE : C_AMB);
        }
    }
    cv_blit(0, Y_STRIP);
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
            frame_rect(x, 2, 55, H_LAYERS - 4, C_LINE);
            text_in(x, 55, 6, &FONT_XS, LAYER_SHORT[l], C_DIM);
            text_in(x, 55, 22, &FONT_XS, "-", C_DIM);
            continue;
        }
        if (on)
            cv_rect(x, 2, 55, H_LAYERS - 4, C_LINE);
        else
            frame_rect(x, 2, 55, H_LAYERS - 4, C_AMB);
        num_cat(t, player_variant_of(l) > ly->variants ? ly->variants : player_variant_of(l));
        text_in(x, 55, 5, &FONT_XS, LAYER_SHORT[l], on ? C_WHITE : C_AMB);
        text_in(x, 55, 22, &FONT_XS, on ? t : "MUTE", on ? C_HI : C_AMB);
    }
    cv_blit(0, Y_LAYERS);
}

static void draw_bottom(int32_t section)
{
    char t[40] = "";
    cv_begin(240, H_BOTTOM, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    if (section >= 0)
        cv_text(6, 6, &FONT_XS, song_section_name(ui.s, (uint32_t)section), C_GRAY);
    if (!ui.flash_ok)
        text_cat(t, "RAM ONLY ");
    text_cat(t, "CPU ");
    num_cat(t, plat_cpu_pct());
    text_cat(t, "% V ");
    num_cat(t, player_voices());
    text_cat(t, "/");
    num_cat(t, player_voices_max());
    cv_text(234 - text_w(&FONT_XS, t), 6, &FONT_XS, t, ui.flash_ok ? C_DIM : C_AMB);
    cv_blit(0, Y_BOTTOM);
}

static void frame_play(uint32_t now)
{
    const struct song *s = ui.s;
    int st = player_state();
    int32_t tc = player_tick(PL_LEAD_CHORD_MS), tb = player_tick(PL_LEAD_BEAT_MS);
    int32_t k = -1, next = -1, left = 0, count = 0, bar = 0, beat = 0, section;
    uint32_t from, to, top_key;
    int looping = player_loop_of(&from, &to);
    if (st == PL_STOPPED) {
        uint32_t first = song_section_first(s, 0) * s->bar_ticks;
        next = (int32_t)song_chord_at(s, first);
        tb = (int32_t)s->start_tick;
    } else {
        if (tc >= (int32_t)s->start_tick || st == PL_PLAYING) {
            uint32_t T;
            k = (int32_t)song_chord_at(s, tc > 0 ? (uint32_t)tc : 0);
            if ((uint32_t)k + 1u < s->ncevt) {
                next = k + 1;
                T = song_cevt_tick(s, (uint32_t)next);
            } else {
                T = s->end_tick;
            }
            if (looping && T >= to) {    /* the next change is the loop's jump */
                next = (int32_t)song_chord_at(s, from);
                T = to;
                if (next == k)
                    next = -1;
            }
            if (next >= 0 && (int32_t)T > tb && (int32_t)T - tb <= (int32_t)s->bar_ticks)
                left = ((int32_t)T - tb + (int32_t)s->ppq - 1) / (int32_t)s->ppq;
        } else {                         /* the count-in: beats left, the first chord next */
            count = ((int32_t)s->start_tick - tc + (int32_t)s->ppq - 1) / (int32_t)s->ppq;
            next = (int32_t)song_chord_at(s, s->start_tick);
            if (song_cevt_symbol(s, (uint32_t)next)[0] == 'N' && (uint32_t)next + 1u < s->ncevt)
                next++;                  /* N.C. in the pickup: the first real chord */
        }
        if (tb >= 0) {
            bar = tb / (int32_t)s->bar_ticks;
            beat = tb % (int32_t)s->bar_ticks / (int32_t)s->ppq;
        }
    }
    if (st == PL_COUNT_IN && tb < (int32_t)s->start_tick)
        bar = (int32_t)s->start_tick / (int32_t)s->bar_ticks;   /* the strip waits on the first bar */
    section = st == PL_STOPPED ? 0 : (int32_t)section_at(tb);
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.sh_top_key = 0xFFFFFFFFu;
    }
    /* the chord first: it is what must not be late (player.h PL_LEAD_CHORD_MS; tools/m1_test.py checks) */
    if (ui.redraw || st != ui.sh_state || k != ui.sh_chord || count != ui.sh_count) {
        draw_big(st, k, count);
        lcd_done();
        if (k >= 0 && k < (int32_t)PL_STATS_CHORDS && st != PL_STOPPED && !player_stats.chord_drawn[k])
            player_stats.chord_drawn[k] = player_frames();
        ui.sh_chord = k;
        ui.sh_count = count;
    }
    if (ui.redraw || st != ui.sh_state || next != ui.sh_next || left != ui.sh_left) {
        draw_next(st, next, left);
        ui.sh_next = next;
        ui.sh_left = left;
    }
    top_key = (uint32_t)(bar * 16 + beat) ^ player_tempo_of() << 16 ^ (uint32_t)ui.loop << 24 ^
              (uint32_t)player_count_in_of() << 27 ^ (uint32_t)st << 28;
    if (ui.redraw || top_key != ui.sh_top_key) {
        draw_top(bar, beat, st);
        ui.sh_top_key = top_key;
    }
    if (ui.redraw || bar != ui.sh_strip || (int32_t)(looping ? to : 0) != ui.sh_bar) {
        draw_strip(bar);
        ui.sh_strip = bar;
        ui.sh_bar = (int32_t)(looping ? to : 0);
    }
    if (ui.redraw || beat != ui.sh_beat || st != ui.sh_state) {
        draw_cursor(beat, st);
        ui.sh_beat = beat;
    }
    if (ui.redraw || (int32_t)(ui.muted | player_variant_of(0) << 4 | player_variant_of(1) << 6 |
                               player_variant_of(2) << 8 | player_variant_of(3) << 10) != ui.sh_layers) {
        draw_layers();
        ui.sh_layers = (int32_t)(ui.muted | player_variant_of(0) << 4 | player_variant_of(1) << 6 |
                                 player_variant_of(2) << 8 | player_variant_of(3) << 10);
    }
    if (ui.redraw || section != ui.sh_section || now - ui.sh_stats_ms >= 500u) {
        draw_bottom(section);
        ui.sh_section = section;
        ui.sh_stats_ms = now;
    }
    ui.sh_state = st;
}

void app_ui_init(void)
{
    ui.flash_ok = plat_data(PLAT_DATA_LO) != 0;   /* 0: the platform refused the chip (fm1-base main_fm1.c) */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
    ui.sh_list = 0xFFFFFFFFu;
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    input();
    ann_tick(now);
    if (ui.screen == 0) {
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
    if (ui.redraw)
        lcd_fill(0, 0, 240, 240, C_BLACK);
    frame_play(now);
    ui.redraw = 0;
}
