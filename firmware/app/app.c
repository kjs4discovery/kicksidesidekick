/* Licence: TBD (see D017) */
/* fm1-accompaniment, first firmware: its name and version, whether the flash chip was accepted, and the keys
 * playing General MIDI Electric Piano 1 (synth.c), the held key's name shown large. Part of fm1-base's unity
 * build, after the platform: its drawing (gfx.c: cv_*, FONT_*, C_*) and announce.c are in scope. */
#include "synth.h"

#define KEY_LOW 53                       /* key 0 is F3 (plat.h) */
#define VELOCITY 100u                    /* fixed for now: velocity from the keys comes later (APP_TOUCH) */

static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static struct {
    uint32_t keys;                       /* held at the last input */
    int32_t shown;                       /* the key whose name is shown (the last one pressed), -1 none */
    char msg[2][24];                     /* a short message, for a moment */
    uint32_t msg_until;
    uint8_t flash_ok;                    /* the platform accepted the flash chip (else RAM only) */
    uint8_t redraw;
} ui = {.shown = -1};

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

static void note_text(int32_t note, char *b)   /* "C4", "F#3" (the keys span octaves 3 to 5) */
{
    char o[2] = {(char)('0' + note / 12 - 1), 0};
    b[0] = 0;
    text_cat(b, NOTE_NAME[note % 12]);
    text_cat(b, o);
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
    ui.redraw = 1;
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
static void input(void)
{
    uint32_t keys = plat_keys(), k;
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        uint8_t note = (uint8_t)(KEY_LOW + (int32_t)k);
        if ((keys & bit) && !(ui.keys & bit)) {
            synth_midi(0x90u, note, (uint8_t)VELOCITY);
            ui.shown = (int32_t)k;
        } else if (!(keys & bit) && (ui.keys & bit)) {
            synth_midi(0x80u, note, 0);
        }
    }
    if (keys != ui.keys) {
        if (keys && (ui.shown < 0 || !(keys >> ui.shown & 1u))) {   /* the shown key let go, others held: the lowest */
            for (k = 0; !(keys >> k & 1u); k++)
                ;
            ui.shown = (int32_t)k;
        }
        ui.redraw = 1;
    }
    ui.keys = keys;
    plat_leds(0, keys);                  /* the held keys lit */
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------------ screen --- */
static void text_center(int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text((240 - text_w(f, s)) / 2, y, f, s, c);
}

static void draw(void)
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key each black one follows */
    char t[32];
    uint32_t i;
    int held = ui.shown >= 0 && (ui.keys >> ui.shown & 1u);

    /* the header: name, version, the flash chip */
    cv_begin(240, 64, C_BLACK);
    text_center(10, &FONT_B, APP_NAME, C_WHITE);
    t[0] = 0;
    text_cat(t, "v" FW_VERSION "  ");
    text_cat(t, APP_VERSION);
    text_center(30, &FONT_XS, t, C_GRAY);
    text_center(46, &FONT_XS, ui.flash_ok ? "FLASH OK" : "FLASH: RAM ONLY", ui.flash_ok ? C_DIM : C_AMB);
    cv_blit(0, 0);

    /* the middle: the held key's name, large, or what to do */
    cv_begin(240, 96, C_BLACK);
    if (plat_ms() < ui.msg_until) {
        text_center(24, &FONT_M, ui.msg[0], C_WHITE);
        text_center(56, &FONT_B, ui.msg[1], C_HI);
    } else if (held) {
        note_text(KEY_LOW + ui.shown, t);
        text_center(16, &FONT_L, t, C_HI);
        text_center(60, &FONT_XS, "ELECTRIC PIANO 1", C_GRAY);
    } else {
        text_center(40, &FONT_B, synth_ready() ? "Hold a key" : "Synth not ready", synth_ready() ? C_GRAY : C_AMB);
    }
    cv_blit(0, 64);

    /* the keys, black above white: held ones lit */
    cv_begin(240, 64, C_BLACK);
    for (i = 0; i < 16u; i++)
        cv_rect(8 + (int32_t)i * 14, 22, 13, 30, (ui.keys >> WK[i] & 1u) ? C_HI : C_LINE);
    for (i = 0; i < 11u; i++)
        cv_rect(8 + (int32_t)AT[i] * 14 + 8, 4, 11, 18, (ui.keys >> BK[i] & 1u) ? C_HI : C_DIM);
    cv_blit(0, 168);
}

void app_ui_init(void)
{
    ui.flash_ok = plat_data(PLAT_DATA_LO) != 0;   /* 0: the platform refused the chip (fm1-base main_fm1.c) */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    input();
    ann_tick(now);
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}
