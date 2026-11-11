/* Licence: TBD (see D017) */
/* M5 Part 3 (D030); M7 (D039): the mode screen and SYNTH mode's screens and controls. Included by ui.c (fm1-base's
 * unity build), after its drawing helpers and the NOT IMPLEMENTED pop-up. The sound is vsynth.c's, the drums synth.c's.
 *
 * The mode screen comes after the splash at every power-on: the mascot (the splash's logo), LEARN and SYNTH, the
 * cursor on LEARN; SELECT moves it, PLAY or EDIT chooses. HOME held 2 s from any screen, or Settings > Switch mode,
 * comes back here (the song stops, or the synth goes quiet and gives the song buffer back).
 *
 * SYNTH (M7, the map the user approved; docs/controls.md "M7"): the main screen shows the preset (big), the drum
 * pattern and the tempo. PRESET: the preset; SELECT: the tempo (60..200: the arp, the delay, the drums); K1..K4 on the
 * main screen: cutoff, resonance, reverb mix, delay mix (the bottom row shows the value while turned). EDIT: the next
 * edit page (after the last: the main screen); ALGO: the pages forward / back (from the main screen: the first / last);
 * K1..K4 on a page: its four settings; ENV / FX: the ENVELOPE / the first FX page; HOME (a tap): the main screen.
 * SEQ: the drum pattern menu (SELECT, SEQ chooses, PLAY chooses and plays, HOME back); PLAY: the drums on / off;
 * ARP: the arpeggiator on / off; GLO: the controls, GLO again: SYNTH's settings (drum volume, drum kit); OCT- / OCT+:
 * the keys' octave. LFO, SCL, SAVE, REC: the NOT IMPLEMENTED pop-up (SAVE: M7.1).
 * Every screen is drawn from the parameter table (vsynth.h, vsynth_param): names, ranges and values. */
#include "vsynth.h"

enum { SY_MAIN, SY_PAGE, SY_MENU, SY_HELP, SY_SET };
#define QUICK_MS 1200u                   /* the bottom row shows a quick knob's value this long */
#define SY_SET_ROWS 2u                   /* SYNTH's settings: drum volume, drum kit */
static const uint8_t QUICK[4] = {VP_CUT, VP_RES, VP_REV_MIX, VP_DLY_MIX};
static const char *const KIT_NAME[KITS] = {"ACOUSTIC", "808", "CHIP"};

static struct {
    uint32_t mode_sel;                   /* the mode screen's cursor: 0 LEARN, 1 SYNTH */
    uint32_t scr, back;                  /* SY_*; the screen the help / menu returns to */
    uint32_t page;                       /* SY_PAGE: PG_* */
    uint32_t preset, bpm;
    uint32_t menu_sel, set_row;
    uint32_t quick, quick_until;         /* the quick knob turned last (QUICK[]), shown until */
    int32_t acc[4];                      /* turns toward the next value of a short-range setting */
    uint8_t key_note[NKEYS];
    uint32_t shown;                      /* what the screen shows (a hash): redrawn when it changes */
    uint32_t drawn_scr;                  /* the screen last drawn (another one: cleared first) */
    uint32_t say_until;
} sy = {.bpm = 120};

static void synth_load(uint32_t i);
static void draw_legend(int32_t y, const char *t);   /* (ui.c, below) */

/* the synth settings, in the settings record (ui.c struct settings: synth[]) */
static uint32_t drum_vol_set(void) { return ui.set.synth[0] <= 10u ? ui.set.synth[0] : 7u; }
static uint32_t drum_kit_set(void) { return ui.set.synth[1] < KITS ? ui.set.synth[1] : KIT_ACOUSTIC; }
static void drum_settings_apply(void)
{
    synth_drum_volume(drum_vol_set() * 127u / 10u);
    synth_drum_kit(drum_kit_set());
    player_stats.drum_kit = drum_kit_set();
}

/* ---------------------------------------------------------------- the mode screen --- */
static void draw_mode(void)
{
    static const char *const NAME[2] = {"LEARN", "SYNTH"};
    static const char *const SUB[2] = {"play along", "play the synth"};
    uint32_t i;
    lcd_fill(0, 0, 240, 240, LOGO_BG);
    draw_logo(4);
    for (i = 0; i < 2u; i++) {
        int sel = i == sy.mode_sel;
        cv_begin(112, 52, LOGO_BG);
        cv_rect(0, 0, 112, 52, sel ? C_HI : RGB(200, 190, 176));
        cv_rect(3, 3, 106, 46, sel ? RGB(40, 34, 28) : LOGO_BG);
        text_in(0, 112, 8, &FONT_B, NAME[i], sel ? C_WHITE : RGB(60, 50, 40));
        text_in(0, 112, 30, &FONT_XS, SUB[i], sel ? C_HI : RGB(120, 108, 94));
        cv_blit(6u + i * 116u, 182);
    }
}

static void mode_choose(uint32_t m)
{
    ui.redraw = 1;
    if (m == 0) {                        /* learning: the song list, as before */
        player_stats.mode = 1;
        ui.view = V_LIST;
        return;
    }
    player_stats.mode = 2;
    lib_release();                       /* (nothing open yet; the synth takes the song buffer) */
    vsynth_start(dec, sizeof dec);
    sy.scr = SY_MAIN;
    sy.bpm = 120;
    vsynth_tempo(sy.bpm);
    synth_drum_tempo(sy.bpm);
    synth_drum_pattern(SYNTH_DRUM_DEFAULT);   /* (ROCK1 at every start, D041) */
    drum_settings_apply();
    synth_load(0);
    ui.view = V_SYNTH;
    sy.shown = 0xFFFFFFFFu;
}

/* back to the mode screen, from either mode */
static void mode_leave(void)
{
    if (vsynth_active()) {
        uint32_t t = plat_ms();
        synth_drums(0);
        vsynth_stop();                   /* (the song buffer is free again once a render running now has ended: a
                                          * block is 5.8 ms; a song opened next reads into it) */
        while (plat_ms() - t < 20u)
            ;
        sy.mode_sel = 0;
    } else {
        lib_release();                   /* the song stops */
        sy.mode_sel = 1;
    }
    synth_panic();
    ui.keymode_until = 0;
    ui.loop_step = 0;
    player_stats.mode = 0;
    player_stats.mode_switches++;
    ui.view = V_MODE;
    ui.redraw = 1;
}

static void mode_input(uint32_t keys, uint32_t btn, uint32_t down)
{
    int32_t d = plat_enc(EN_SELECT);
    (void)keys, (void)btn;
    if (d) {
        sy.mode_sel = d > 0 ? 1u : 0u;
        ui.redraw = 1;
    }
    if (down & (1u << B_PLAY | 1u << B_EDIT))
        mode_choose(sy.mode_sel);
    plat_leds(1u << B_PLAY, 0);
}

/* -------------------------------------------------------------------- SYNTH mode --- */
static void synth_load(uint32_t i)
{
    uint32_t n = vsynth_presets();
    sy.preset = i < n ? i : 0;
    vsynth_load(vsynth_stock(sy.preset));   /* (its arpeggio on, when the preset has one) */
    player_stats.synth_preset = sy.preset;
}

static void synth_say(const char *a, const char *b)
{
    app_say(a, b);
    sy.say_until = plat_ms() + 1200u;
}

/* the buttons with a job on SYNTH's screen now (the rest open the pop-up) */
static uint32_t synth_jobs(void)
{
    uint32_t oct = 1u << B_OCTDN | 1u << B_OCTUP;
    switch (sy.scr) {
    case SY_MENU:                        /* SEQ / PLAY choose, HOME back; ARP and the octave as ever */
        return oct | 1u << B_SEQ | 1u << B_PLAY | 1u << B_HOME | 1u << B_ARP;
    case SY_HELP:                        /* any button: back (GLO: the settings) */
        return ALL_BUTTONS;
    case SY_SET:                         /* PLAY / EDIT change, GLO / HOME back */
        return oct | 1u << B_PLAY | 1u << B_EDIT | 1u << B_GLO | 1u << B_HOME;
    default:                             /* the main screen and the pages: all but LFO, SCL, SAVE, REC */
        return ALL_BUTTONS & ~(1u << B_LFO | 1u << B_SEL | 1u << B_SAVE | 1u << B_REC);
    }
}

/* a turn of d on parameter p: 0..127 ranges 2 a click; short ones (up to 16 values) one a 2 clicks */
static void param_turn(uint32_t p, int32_t d, int32_t *acc)
{
    const struct vs_param *q = vsynth_param(p);
    if (q->hi - q->lo <= 15) {
        *acc += d;
        if (*acc < 2 && *acc > -2)
            return;
        d = *acc > 0 ? 1 : -1;
        *acc = 0;
    } else {
        d *= 2;
    }
    vsynth_set(p, vsynth_get(p) + d);
}

static void synth_page(uint32_t scr, uint32_t page)
{
    sy.scr = scr;
    sy.page = page;
    sy.acc[0] = sy.acc[1] = sy.acc[2] = sy.acc[3] = 0;
    sy.quick_until = 0;
}

static void synth_input(uint32_t keys, uint32_t btn, uint32_t down, uint32_t now)
{
    static const uint8_t KENC[4] = {EN_K1, EN_K2, EN_K3, EN_K4};
    uint32_t k, lit = 0;
    int32_t d;
    (void)btn;
    for (k = 0; k < NKEYS; k++) {        /* the keys play on every SYNTH screen (and with the pop-up open) */
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            sy.key_note[k] = (uint8_t)clamp(KEY_LOW + (int32_t)k + 12 * ui.oct, 0, 127);
            vsynth_key(sy.key_note[k], 1);
            player_stats.key_note_last = sy.key_note[k];
        } else if (!(keys & bit) && (ui.keys & bit)) {
            vsynth_key(sy.key_note[k], 0);
        }
    }
    if (popup_input(down, ALL_BUTTONS & ~synth_jobs(), now))
        goto stats;
    if (down & (1u << B_OCTDN | 1u << B_OCTUP)) {
        char t[16] = "OCTAVE ";
        ui.oct = clamp(ui.oct + ((down >> B_OCTUP & 1u) ? 1 : 0) - ((down >> B_OCTDN & 1u) ? 1 : 0), -OCT_MAX, OCT_MAX);
        player_stats.keys_octave = ui.oct;
        snum_cat(t, ui.oct);
        synth_say("KEYS", ui.oct ? t : "OCTAVE 0");
    }
    if (down & 1u << B_ARP && sy.scr != SY_HELP && sy.scr != SY_SET) {   /* the arpeggiator on / off */
        vsynth_set(VP_ARP_ON, !vsynth_get(VP_ARP_ON));
        synth_say("ARPEGGIATOR", vsynth_get(VP_ARP_ON) ? "ON" : "OFF");
    }
    switch (sy.scr) {
    case SY_HELP:                        /* GLO: the settings; any other button (not the octave): back */
        if (down & 1u << B_GLO)
            sy.scr = SY_SET, sy.set_row = 0;
        else if (down & ~(1u << B_OCTDN | 1u << B_OCTUP))
            sy.scr = sy.back;
        goto stats;
    case SY_SET:                         /* SELECT the row; ALGO, PLAY or EDIT its value; GLO / HOME back (saved) */
        d = plat_enc(EN_SELECT);
        if (d)
            sy.set_row = (uint32_t)clamp((int32_t)sy.set_row + d, 0, (int32_t)SY_SET_ROWS - 1);
        d = plat_enc(EN_ALGO);
        if (down & (1u << B_PLAY | 1u << B_EDIT))
            d = 1;
        if (d) {
            if (sy.set_row == 0)
                ui.set.synth[0] = (uint8_t)clamp((int32_t)drum_vol_set() + d, 0, 10);
            else
                ui.set.synth[1] = (uint8_t)((drum_kit_set() + KITS + (d > 0 ? 1u : KITS - 1u)) % KITS);
            ui.set_dirty = 1;
            drum_settings_apply();
        }
        if (down & (1u << B_GLO | 1u << B_HOME)) {
            settings_save();
            sy.scr = sy.back;
        }
        goto stats;
    case SY_MENU:                        /* the drum patterns: SELECT; SEQ chooses, PLAY chooses and plays; HOME back */
        d = plat_enc(EN_SELECT);
        if (d)
            sy.menu_sel = (uint32_t)clamp((int32_t)sy.menu_sel + d, 0, (int32_t)synth_patterns() - 1);
        if (down & (1u << B_SEQ | 1u << B_PLAY)) {
            synth_drum_pattern(sy.menu_sel);
            if (down & 1u << B_PLAY)
                synth_drums(1);
            sy.scr = sy.back;
            synth_say(synth_drums_on() ? "DRUMS (NEXT BAR)" : "DRUMS (PLAY)", synth_pattern_name(sy.menu_sel));
        } else if (down & 1u << B_HOME) {
            sy.scr = sy.back;
        }
        goto stats;
    default:
        break;
    }
    /* the main screen and the edit pages */
    d = plat_enc(EN_PRESET);
    if (d) {
        int32_t n = (int32_t)vsynth_presets();
        synth_load((uint32_t)((((int32_t)sy.preset + d) % n + n) % n));
    }
    d = plat_enc(EN_SELECT);
    if (d) {                             /* the tempo: 1 BPM a click, turned quickly 5 */
        int32_t step = (d > 1 || d < -1 || now - ui.tempo_ms < 90u) ? 5 : 1;
        ui.tempo_ms = now;
        sy.bpm = (uint32_t)clamp((int32_t)sy.bpm + step * d, 60, 200);
        vsynth_tempo(sy.bpm);
        synth_drum_tempo(sy.bpm);
    }
    d = plat_enc(EN_ALGO);
    if (d) {                             /* the pages forward / back, through the main screen */
        int32_t p = sy.scr == SY_MAIN ? (d > 0 ? 0 : (int32_t)VS_PAGES - 1) : (int32_t)sy.page + (d > 0 ? 1 : -1);
        if (p < 0 || p >= (int32_t)VS_PAGES)
            synth_page(SY_MAIN, 0);
        else
            synth_page(SY_PAGE, (uint32_t)p);
    }
    if (down & 1u << B_EDIT)             /* the next page; after the last: the main screen */
        synth_page(sy.scr == SY_MAIN ? SY_PAGE : sy.page + 1u < VS_PAGES ? SY_PAGE : SY_MAIN,
                   sy.scr == SY_MAIN ? 0 : sy.page + 1u < VS_PAGES ? sy.page + 1u : 0);
    if (down & 1u << B_ENV)
        synth_page(SY_PAGE, PG_ENV);
    if (down & 1u << B_FX)
        synth_page(SY_PAGE, PG_FX1);
    if (down & 1u << B_HOME)             /* a tap: the main screen (held 2 s: the mode screen, ui.c) */
        synth_page(SY_MAIN, 0), sy.say_until = 0;
    for (k = 0; k < 4u; k++) {           /* K1..K4: the main screen's quick four, or the page's */
        d = plat_enc(KENC[k]);
        if (!d)
            continue;
        if (sy.scr == SY_MAIN) {
            param_turn(QUICK[k], d, &sy.acc[k]);
            sy.quick = k;
            sy.quick_until = now + QUICK_MS;
        } else {
            uint32_t p, i = 0;
            for (p = 0; p < VP_N; p++)
                if (vsynth_param(p)->page == sy.page && i++ == k)
                    break;
            if (p < VP_N)
                param_turn(p, d, &sy.acc[k]);
        }
    }
    if (down & 1u << B_SEQ) {            /* the drum pattern menu */
        sy.back = sy.scr;
        sy.menu_sel = synth_drum_pattern_of();
        sy.scr = SY_MENU;
    }
    if (down & 1u << B_PLAY) {           /* the drums on / off */
        synth_drums(!synth_drums_on());
        synth_say("DRUMS", synth_drums_on() ? synth_pattern_name(synth_drum_pattern_of()) : "OFF");
    }
    if (down & 1u << B_GLO) {            /* the controls */
        sy.back = sy.scr;
        sy.scr = SY_HELP;
    }
stats:
    {
        const struct vs_stats *s = vsynth_stats();
        player_stats.synth_notes = s->notes;
        player_stats.synth_voices_max = s->voices_max;
        player_stats.arp_steps = s->arp_steps;
        player_stats.synth_peak = s->peak;
        player_stats.shift = 0;
        player_stats.fx_reverb = (uint32_t)vsynth_get(VP_REV_MIX);
        player_stats.fx_crush = (uint32_t)vsynth_get(VP_CRUSH);
        player_stats.arp_pattern = vsynth_get(VP_ARP_ON) ? (uint32_t)vsynth_get(VP_ARP_PAT) + 1u : 0;   /* (M6's) */
        player_stats.arp_rate = (uint32_t)vsynth_get(VP_ARP_RATE);
        player_stats.jam = player_stats.drum_on = (uint32_t)synth_drums_on();
        player_stats.drum_pattern = synth_drum_pattern_of();
        player_stats.drum_hits = player_stats.jam_notes = synth_drum_hits();
        player_stats.drum_voices_max = s->drum_voices_max;
        player_stats.fx_active = s->fx;
        player_stats.synth_page = sy.scr == SY_PAGE ? sy.page + 1u : sy.scr == SY_MAIN ? 0 : 0xF0u + sy.scr;
        player_stats.synth_engine = vsynth_now()->engine;
        player_stats.synth_bpm = sy.bpm;
        player_stats.synth_level = (uint32_t)vsynth_get(VP_LEVEL);
    }
    if (vsynth_get(VP_ARP_ON))
        lit |= 1u << B_ARP;
    if (synth_drums_on())
        lit |= 1u << B_PLAY;
    plat_lights(lit, keys, synth_jobs() & ~(1u << B_HOME | 1u << B_GLO), 0, 1);
}

static uint32_t synth_hash(uint32_t now)
{
    uint32_t h = sy.scr * 7919u + sy.page * 131u + sy.preset * 2654435761u, k;
    const struct vs_preset *r = vsynth_now();
    for (k = 0; k < VP_N; k++)
        h = h * 31u + r->v[k];
    h = h * 31u + sy.bpm * 1031u + (uint32_t)(ui.oct + 8) * 65537u + sy.menu_sel * 17u + sy.set_row * 3u;
    h = h * 31u + (uint32_t)synth_drums_on() * 5u + synth_drum_pattern_of() * 257u + synth_drum_beat() * 11u;
    h = h * 31u + drum_vol_set() * 13u + drum_kit_set() * 101u + (now < sy.quick_until ? sy.quick + 1u : 0) * 999u;
    if (sy.scr == SY_HELP)
        h += now / 500u;                 /* (the CPU line) */
    return h ^ (now < sy.say_until ? 0x5A5A5A5Au : 0);
}

static int synth_dirty(uint32_t now)
{
    uint32_t h = synth_hash(now);
    if (!ui.redraw && h == sy.shown)
        return 0;
    sy.shown = h;
    return 1;
}

/* the top line: the screen's title, the preset's number (or the page's) on the right */
static void synth_top(const char *title, const char *right)
{
    cv_begin(240, 32, C_BLACK);
    cv_text(6, 4, &FONT_B, title, C_HI);
    cv_text(234 - text_w(&FONT_XS, right), 8, &FONT_XS, right, C_DIM);
    cv_rect(0, 31, 240, 1, C_LINE);
    cv_blit(0, 0);
}

/* a parameter in one of the four K1..K4 places (2 x 2): its name, its value, a bar */
static void synth_cell(uint32_t k, uint32_t p, int hi)
{
    const struct vs_param *q = vsynth_param(p);
    char t[16] = "", name[16] = "";
    int32_t v = vsynth_get(p), w;
    text_cat(name, q->name);
    vsynth_value_text(t, p, v);
    w = q->hi > q->lo ? 104 * (v - q->lo) / (q->hi - q->lo) : 0;
    cv_begin(116, 64, C_BLACK);
    cv_rect(0, 0, 116, 64, hi ? C_LINE : C_BLACK);
    cv_text(6, 4, &FONT_XS, name, C_GRAY);
    cv_text(110 - text_w(&FONT_B, t), 22, &FONT_B, t, C_WHITE);
    cv_rect(6, 46, 104, 6, RGB(60, 60, 66));
    cv_rect(6, 46, w, 6, C_HI);
    cv_text(6, 22, &FONT_T, k == 0 ? "K1" : k == 1 ? "K2" : k == 2 ? "K3" : "K4", C_DIM);
    cv_blit(2u + (k & 1u) * 120u, 40u + (k >> 1) * 70u);
}

/* the message line (an action's answer), or the drums: the pattern, its beats, on / off */
static void synth_drum_line(uint32_t now, int32_t y)
{
    char t[40] = "";
    cv_begin(240, 24, C_BLACK);
    if (now < sy.say_until) {
        text_cat(t, ui.msg[0]);
        text_cat(t, ": ");
        text_cat(t, ui.msg[1]);
        text_in(0, 240, 4, &FONT_XS, t, C_WHITE);
    } else {
        uint32_t b, nb = synth_pattern_beats(synth_drum_pattern_of()), beat = synth_drum_beat();
        text_cat(t, "DRUMS ");
        text_cat(t, synth_pattern_name(synth_drum_pattern_of()));
        cv_text(6, 4, &FONT_XS, t, synth_drums_on() ? C_AMB : C_GRAY);
        for (b = 0; b < nb && b < 7u; b++)
            cv_rect(150 + (int32_t)b * 12, 7, 9, 9, synth_drums_on() && b == beat ? (b ? C_HI : C_WHITE) : C_LINE);
    }
    cv_blit(0, (uint32_t)y);
}

static void draw_synth_main(uint32_t now)
{
    const struct vs_preset *r = vsynth_now();
    char t[40] = "", name[VS_NAME + 1];
    uint32_t k;
    for (k = 0; k < VS_NAME; k++)
        name[k] = r->name[k];
    name[VS_NAME] = 0;
    num_cat(t, sy.preset + 1u);
    text_cat(t, " / ");
    num_cat(t, vsynth_presets());
    text_cat(t, "  ");
    text_cat(t, vsynth_engine_name(r->engine));
    synth_top("SYNTH", t);
    cv_begin(240, 112, C_BLACK);         /* the preset, big */
    text_in(0, 240, 24, &FONT_L, name, C_WHITE);
    t[0] = 0;
    num_cat(t, sy.bpm);
    text_cat(t, " BPM");
    if (vsynth_get(VP_ARP_ON)) {
        text_cat(t, "   ARP ");
        text_cat(t, vsynth_param(VP_ARP_RATE)->names[vsynth_get(VP_ARP_RATE)]);
    }
    if (ui.oct) {
        text_cat(t, "   OCT ");
        snum_cat(t, ui.oct);
    }
    text_in(0, 240, 76, &FONT_XS, t, C_GRAY);
    cv_blit(0, 40);
    synth_drum_line(now, 170);
    if (now < sy.quick_until) {          /* a quick knob turned: its value in the bottom row */
        char v[16] = "";
        t[0] = 0;
        text_cat(t, vsynth_param(QUICK[sy.quick])->name);
        vsynth_value_text(v, QUICK[sy.quick], vsynth_get(QUICK[sy.quick]));
        text_cat(t, " ");
        text_cat(t, v);
        draw_legend(Y_LEGEND, t);
    } else {
        draw_legend(Y_LEGEND, (now / 3000u) & 1u ? "PRESET sound|SELECT tempo|EDIT pages" :
                              "K1-K4 quick|PLAY drums|SEQ pattern|GLO help");
    }
}

static void draw_synth_page(uint32_t now)
{
    char t[16] = "";
    uint32_t p, k = 0;
    num_cat(t, sy.page + 1u);
    text_cat(t, "/");
    num_cat(t, VS_PAGES);
    synth_top(vsynth_page_name(sy.page), t);
    for (p = 0; p < VP_N && k < 4u; p++)
        if (vsynth_param(p)->page == sy.page)
            synth_cell(k++, p, 0);
    synth_drum_line(now, 184);
    draw_legend(Y_LEGEND, "K1-K4 set|EDIT next|ALGO pages|HOME main");
}

static void draw_synth_menu(void)
{
    uint32_t i, n = synth_patterns(), top = sy.menu_sel >= 7u ? sy.menu_sel - 6u : 0;
    synth_top("DRUM PATTERN", synth_drums_on() ? "playing: next bar" : "PLAY: choose, play");
    for (i = top; i < n && i < top + 8u; i++) {
        int sel = i == sy.menu_sel, cur = i == synth_drum_pattern_of();
        char t[24] = "";
        num_cat(t, synth_pattern_beats(i));
        text_cat(t, " beats");
        cv_begin(240, SET_ROW_H, C_BLACK);
        if (sel)
            cv_rect(2, 1, 236, SET_ROW_H - 2, C_LINE);
        cv_text(10, 3, &FONT_XS, synth_pattern_name(i), sel ? C_WHITE : cur ? C_AMB : C_GRAY);
        cv_text(230 - text_w(&FONT_XS, t), 3, &FONT_XS, t, cur ? C_AMB : C_DIM);
        cv_blit(0, 34 + (i - top) * SET_ROW_H);
    }
    draw_legend(Y_LEGEND, "SEQ choose|PLAY play|HOME back");
}

static void draw_synth_help(void)
{
    static const char *const L[7][2] = {{"PRESETS", "the sound"}, {"SELECT", "tempo"}, {"ALGO", "pages"},
                                        {"K1-K4", "quick / page"}, {"OCT- OCT+", "keys' octave"}, {"MASTER", "volume"},
                                        {"keys", "play"}};
    static const char *const R[10][2] = {{"EDIT", "next page"}, {"ENV", "envelope"}, {"FX", "effects"},
                                         {"ARP", "arp on/off"}, {"PLAY", "drums"}, {"SEQ", "pattern"},
                                         {"HOME", "main / mode"}, {"GLO", "again: settings"},
                                         {"LFO SCL", "not yet"}, {"SAVE REC", "not yet"}};
    uint32_t r, band;
    char t[32] = "CPU ";
    synth_top("SYNTH CONTROLS", "any button: back");
    for (band = 0; band < 2u; band++) {  /* two bands of 90 rows (the canvas holds 144) */
        int32_t y0 = (int32_t)band * 90;
        cv_begin(240, 90, C_BLACK);
        cv_band(0, 90);
        for (r = 0; r < 7u; r++) {
            cv_text(6, 2 + (int32_t)r * 25 - y0, &FONT_XS, L[r][0], C_GRAY);
            cv_text(6, 14 + (int32_t)r * 25 - y0, &FONT_T, L[r][1], C_HI);
        }
        for (r = 0; r < 10u; r++) {
            cv_text(108, 2 + (int32_t)r * 17 - y0, &FONT_XS, R[r][0], C_GRAY);
            cv_text(160, 2 + (int32_t)r * 17 - y0, &FONT_XS, R[r][1], C_HI);
        }
        cv_blit(0, 34u + (uint32_t)y0);
    }
    cv_begin(240, 24, C_BLACK);
    num_cat(t, plat_cpu_pct());
    text_cat(t, "%   v" FW_VERSION);
    cv_text(6, 4, &FONT_XS, t, C_DIM);
    cv_blit(0, 214);
}

static void draw_synth_set(void)
{
    uint32_t i;
    char v[8] = "";
    synth_top("SYNTH SETTINGS", ui.flash_ok ? "saved when you leave" : "RAM ONLY");
    num_cat(v, drum_vol_set());
    for (i = 0; i < SY_SET_ROWS; i++) {
        int sel = i == sy.set_row;
        const char *val = i ? KIT_NAME[drum_kit_set()] : v;
        cv_begin(240, SET_ROW_H, C_BLACK);
        if (sel)
            cv_rect(2, 1, 236, SET_ROW_H - 2, C_LINE);
        cv_text(10, (SET_ROW_H - 16) / 2, &FONT_B, i ? "Drum kit" : "Drum volume", sel ? C_WHITE : C_GRAY);
        cv_text(230 - text_w(&FONT_B, val), (SET_ROW_H - 16) / 2, &FONT_B, val, sel ? C_HI : C_AMB);
        cv_blit(0, 32 + i * SET_ROW_H);
    }
    draw_legend(Y_LEGEND, "SELECT row|ALGO change|GLO back");
}

static void draw_synth(void)
{
    uint32_t now = plat_ms();
    if (ui.redraw || sy.drawn_scr != sy.scr * 16u + sy.page) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        sy.drawn_scr = sy.scr * 16u + sy.page;
    }
    switch (sy.scr) {
    case SY_PAGE:
        draw_synth_page(now);
        break;
    case SY_MENU:
        draw_synth_menu();
        break;
    case SY_HELP:
        draw_synth_help();
        break;
    case SY_SET:
        draw_synth_set();
        break;
    default:
        draw_synth_main(now);
        break;
    }
}
