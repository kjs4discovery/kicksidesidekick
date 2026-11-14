/* Licence: TBD (see D017) */
/* M5 Part 3 (D030); M7 (D039): the mode screen and SYNTH mode's screens and controls. Included by ui.c (fm1-base's
 * unity build), after its drawing helpers and the NOT IMPLEMENTED pop-up. The sound is vsynth.c's, the drums synth.c's.
 *
 * The mode screen comes after the splash at every power-on: the mascot (the splash's logo), SONG LEARN, BACKTRACK and
 * SYNTH (M8, D050), the cursor on SONG LEARN; SELECT moves it, PLAY or EDIT chooses. HOME held 2 s from any screen, or Settings > Switch mode,
 * comes back here (the song stops, or the synth goes quiet and gives the song buffer back).
 *
 * SYNTH (M7, the map the user approved; docs/controls.md "M7"): the main screen shows the preset (big), the drum
 * pattern and the tempo. PRESET: the preset; SELECT: the tempo (60..200: the arp, the delay, the drums); K1..K4 on the
 * main screen: cutoff, resonance, reverb mix, delay mix (the bottom row shows the value while turned). EDIT: the next
 * edit page (after the last: the main screen); ALGO: the pages forward / back (from the main screen: the first / last);
 * K1..K4 on a page: its four settings; ENV / FX: the ENVELOPE / the first FX page (again: the main screen / the next
 * FX page); HOME (a tap): the main screen (on it: a hint).
 * SEQ: the drum pattern menu (SELECT, SEQ chooses, PLAY chooses and plays, HOME back); PLAY: the drums on / off;
 * ARP: the arpeggiator on / off; GLO: the controls, GLO again: SYNTH's settings (drum volume, drum kit); OCT- / OCT+:
 * the keys' octave. LFO, SCL, SAVE, REC: the NOT IMPLEMENTED pop-up (SAVE: M7.1).
 * Every screen is drawn from the parameter table (vsynth.h, vsynth_param): names, ranges and values. */
#include "vsynth.h"

enum { SY_MAIN, SY_PAGE, SY_MENU, SY_HELP, SY_SET, SY_SAVE };   /* (M10: SY_SAVE) */
enum { M_LEARN, M_BACKTRACK, M_SYNTH, MODES };   /* M8: the mode screen's choices, in order */
#define QUICK_MS 1200u                   /* the bottom row shows a quick knob's value this long */
#define SY_SET_ROWS 2u                   /* SYNTH's settings: drum volume, drum kit */
static const uint8_t QUICK[4] = {VP_CUT, VP_RES, VP_REV_MIX, VP_DLY_MIX};
static const char *const KIT_NAME[KITS] = {"ACOUSTIC", "808", "CHIP"};

static struct {
    uint32_t mode_sel;                   /* the mode screen's cursor: M_LEARN, M_BACKTRACK, M_SYNTH (M8: on SONG LEARN
                                          * whenever the screen opens) */
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
    uint32_t sv_slot, sv_cursor, sv_confirm, sv_back;   /* M10: the SAVE screen: the slot, the name's cursor, the REPLACE question */
    char sv_name[VS_NAME + 1];
} sy = {.bpm = 120};

static void synth_load(uint32_t i);
static void bt_enter(void);              /* (bt.c, M8) */
static void bt_leave(void);
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
    /* M8 (D050): three choices under the mascot, a row each: SONG LEARN, BACKTRACK, SYNTH */
    static const char *const NAME[MODES] = {"SONG LEARN", "BACKTRACK", "SYNTH"};
    static const char *const SUB[MODES] = {"Play with a song", "jam over a band", "play the synth"};
    uint32_t i;
    lcd_fill(0, 0, 240, 240, LOGO_BG);
    cv_begin(240, 20, LOGO_BG);          /* M10 (D058): the name across the top, the mascot scaled 4 : 5 under it */
    text_in(0, 240, 2, text_w(&FONT_B, APP_NAME) <= 232 ? &FONT_B : &FONT_XS, APP_NAME, RGB(60, 50, 40));
    cv_blit(0, 0);
    draw_logo_s(20, 4u, 5u);
    cv_begin(120, 14, LOGO_BG);          /* M9: "modes:", in the subtitle colour, just above the first choice */
    cv_text(10, 0, &FONT_XS, "modes:", RGB(120, 108, 94));
    cv_blit(0, 158);
    for (i = 0; i < MODES; i++) {
        int sel = i == sy.mode_sel;
        cv_begin(232, 22, LOGO_BG);
        if (sel) {
            cv_rect(0, 0, 232, 22, C_HI);
            cv_rect(2, 2, 228, 18, RGB(40, 34, 28));
        }
        cv_text(10, 3, &FONT_B, NAME[i], sel ? C_WHITE : RGB(60, 50, 40));
        cv_text(222 - text_w(&FONT_XS, SUB[i]), 4, &FONT_XS, SUB[i], sel ? C_HI : RGB(120, 108, 94));
        cv_blit(4, 172u + i * 23u);
    }
}

static void mode_choose(uint32_t m)
{
    ui.redraw = 1;
    if (m == M_LEARN) {                  /* SONG LEARN: the song list, as before */
        player_stats.mode = 1;
        ui.view = V_LIST;
        return;
    }
    if (m == M_BACKTRACK) {              /* M8: BACKTRACK (bt.c): its setup */
        bt_enter();
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
    vpop_close(0);                       /* M10: a value pop-up goes with the screen */
    if (bt.on) {                         /* M8: BACKTRACK: the song and the synth */
        bt_leave();
    } else if (vsynth_active()) {
        uint32_t t = plat_ms();
        synth_drums(0);
        vsynth_stop();                   /* (the song buffer is free again once a render running now has ended: a
                                          * block is 5.8 ms; a song opened next reads into it) */
        while (plat_ms() - t < 20u)
            ;
    } else {
        lm_off();                        /* M10.1: SONG LEARN's melody synth, if on (the song still plays: its layer is held) */
        lib_release();                   /* the song stops */
    }
    sy.mode_sel = M_LEARN;               /* (M8: the cursor on SONG LEARN, as at power-on) */
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
    if (d) {                             /* (M8: three choices, a step a click) */
        sy.mode_sel = (uint32_t)clamp((int32_t)sy.mode_sel + d, 0, (int32_t)MODES - 1);
        ui.redraw = 1;
    }
    player_stats.mode_sel = sy.mode_sel;
    if (down & (1u << B_PLAY | 1u << B_EDIT))
        mode_choose(sy.mode_sel);
    plat_leds(1u << B_PLAY, 0);
}

/* -------------------------------------------------------------------- SYNTH mode --- */
static void synth_load(uint32_t i)
{
    uint32_t n = pl_count();             /* M10: the stock presets, then the saved ones (upreset.h) */
    sy.preset = i < n ? i : 0;
    vsynth_load(pl_get(sy.preset));   /* (its arpeggio on, when the preset has one) */
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
    case SY_SAVE:                        /* M10: SAVE saves (twice if the slot is taken), HOME goes back */
        return oct | 1u << B_SAVE | 1u << B_HOME;
    default:                             /* the main screen and the pages: all but LFO, SCL, REC (M10: SAVE has its job) */
        return ALL_BUTTONS & ~(1u << B_LFO | 1u << B_SEL | 1u << B_REC);
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

/* ------------------------------------------------------------------ M10 (D062): SAVE --- */
static const char SV_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";
#define SV_NCH (sizeof SV_CHARS - 1u)

static uint32_t sv_index(char c)
{
    uint32_t i;
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    for (i = 0; i < SV_NCH; i++)
        if (SV_CHARS[i] == c)
            return i;
    return SV_NCH - 2u;                  /* (a character the editor has not: a space) */
}

static void save_open(void)
{
    const struct vs_preset *r = vsynth_now();
    int32_t cur = pl_slot(sy.preset);
    uint32_t k, first = UP_SLOTS;
    for (k = 0; k < VS_NAME; k++)        /* the current preset's name, to start with (padded with spaces) */
        sy.sv_name[k] = r->name[k] ? SV_CHARS[sv_index(r->name[k])] : ' ';
    sy.sv_name[VS_NAME] = 0;
    for (k = 0; k < UP_SLOTS && first == UP_SLOTS; k++)
        if (!upreset_used(k))
            first = k;
    sy.sv_slot = cur >= 0 ? (uint32_t)cur : first < UP_SLOTS ? first : 0;   /* a saved preset: its slot; else the first empty */
    sy.sv_cursor = 0;
    sy.sv_confirm = 0;
    sy.sv_back = sy.scr;
    sy.scr = SY_SAVE;
}

static void save_do(void)
{
    struct vs_preset p = *vsynth_now();  /* (the sound as it is now, with every edit: in RAM) */
    char t[24] = "";
    uint32_t k, n = VS_NAME;
    int e = -1;
    while (n && sy.sv_name[n - 1u] == ' ')
        n--;                             /* trailing spaces are not stored */
    for (k = 0; k < VS_NAME; k++)
        p.name[k] = k < n ? sy.sv_name[k] : 0;
    if (!n)
        for (k = 0; k < 6u; k++)
            p.name[k] = "PRESET"[k];
    p.version = VS_REC_VERSION;
    if (ui.flash_ok) {
        uint32_t t0 = plat_ms();
        synth_mute(1);                   /* a fade out (one block), the flash write, a fade in: no click */
        while (!synth_muted() && plat_ms() - t0 < 50u)
            ;
        e = upreset_save(sy.sv_slot, &p);
        synth_mute(0);
    }
    t[0] = 'U';
    t[1] = (char)('0' + (sy.sv_slot + 1u) / 10u);
    t[2] = (char)('0' + (sy.sv_slot + 1u) % 10u);
    if (e == 0) {
        char ti[24] = "SAVED ";
        text_cat(ti, t);
        player_stats.preset_saves++;
        synth_load(pl_index_of_slot(sy.sv_slot));   /* the sound goes on as the saved preset, now selected as U05 */
        sy.scr = sy.sv_back;
        sy.say_until = 0;
        vpop_show(ti, vsynth_now()->name, "", 0);
    } else {
        sy.scr = sy.sv_back;
        vpop_show("NOT SAVED", ui.flash_ok ? "the flash refused" : "no flash (RAM only)", "", 0);
    }
    sy.sv_confirm = 0;
}

/* the SAVE screen's input: SELECT the slot, K1 the name's cursor, K2 its character, SAVE saves (twice when the slot is taken),
 * HOME goes back with nothing written */
static void save_input(uint32_t down)
{
    int32_t d = plat_enc(EN_SELECT);
    if (d && !sy.sv_confirm)
        sy.sv_slot = (uint32_t)clamp((int32_t)sy.sv_slot + d, 0, (int32_t)UP_SLOTS - 1);
    d = plat_enc(EN_K1);
    if (d && !sy.sv_confirm)
        sy.sv_cursor = (uint32_t)clamp((int32_t)sy.sv_cursor + d, 0, (int32_t)VS_NAME - 1);
    d = plat_enc(EN_K2);
    if (d && !sy.sv_confirm) {
        int32_t i = (int32_t)sv_index(sy.sv_name[sy.sv_cursor]);
        i = ((i + d) % (int32_t)SV_NCH + (int32_t)SV_NCH) % (int32_t)SV_NCH;
        sy.sv_name[sy.sv_cursor] = SV_CHARS[i];
    }
    (void)plat_enc(EN_K3), (void)plat_enc(EN_K4), (void)plat_enc(EN_PRESET), (void)plat_enc(EN_ALGO);
    if (down & 1u << B_HOME) {           /* cancel: nothing written */
        sy.scr = sy.sv_back;
        sy.sv_confirm = 0;
        ui.redraw = 1;
    } else if (down & 1u << B_SAVE) {
        if (upreset_used(sy.sv_slot) && !sy.sv_confirm)
            sy.sv_confirm = 1;           /* "REPLACE U05 <name>?" */
        else
            save_do();
    }
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
    case SY_SAVE:                        /* M10: SAVE TO Unn */
        save_input(down);
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
        int32_t n = (int32_t)pl_count();   /* (M10: the stock presets, then the saved ones) */
        char lab[24];
        synth_load((uint32_t)((((int32_t)sy.preset + d) % n + n) % n));
        pl_label(sy.preset, lab);
        vpop_show("Preset", lab, "", 0);   /* M10.1: the value pop-up */
    }
    d = plat_enc(EN_SELECT);
    if (d) {                             /* the tempo: 1 BPM a click, turned quickly 5 */
        int32_t step = (d > 1 || d < -1 || now - ui.tempo_ms < 90u) ? 5 : 1;
        ui.tempo_ms = now;
        sy.bpm = (uint32_t)clamp((int32_t)sy.bpm + step * d, 60, 200);
        vsynth_tempo(sy.bpm);
        synth_drum_tempo(sy.bpm);
        {
            char pv[16] = "";
            num_cat(pv, sy.bpm);
            text_cat(pv, " BPM");
            vpop_show("Tempo", pv, "", 0);   /* M10.1 */
        }
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
    if (down & 1u << B_ENV)              /* the ENVELOPE page; on it: the main screen */
        sy.scr == SY_PAGE && sy.page == PG_ENV ? synth_page(SY_MAIN, 0) : synth_page(SY_PAGE, PG_ENV);
    if (down & 1u << B_FX)               /* the first FX page; on an FX page: the next, after FX 4 the main screen */
        sy.scr == SY_PAGE && sy.page >= PG_FX1 ? (sy.page < PG_FX4 ? synth_page(SY_PAGE, sy.page + 1u)
                                                                    : synth_page(SY_MAIN, 0))
                                               : synth_page(SY_PAGE, PG_FX1);
    if (down & 1u << B_HOME) {           /* a tap: the main screen; from it the mode screen (held 2 s: the mode screen at once, ui.c) */
        if (sy.scr == SY_MAIN) {         /* M10.1: from the main screen a tap goes to the mode screen (released: a hold goes there too) */
            mode_leave();
            return;
        }
        synth_page(SY_MAIN, 0), sy.say_until = 0;
    }
    for (k = 0; k < 4u; k++) {           /* K1..K4: the main screen's quick four, or the page's */
        d = plat_enc(KENC[k]);
        if (!d)
            continue;
        if (sy.scr == SY_MAIN) {
            param_turn(QUICK[k], d, &sy.acc[k]);
            sy.quick = k;
            sy.quick_until = now + QUICK_MS;
            {                            /* M10.1: the value pop-up: the parameter's name and value (replaces the bottom row's) */
                char pv[16] = "";
                vsynth_value_text(pv, QUICK[k], vsynth_get(QUICK[k]));
                vpop_show(vsynth_param(QUICK[k])->name, pv, "", 0);
            }
        } else {
            uint32_t p, i = 0;
            for (p = 0; p < VP_N; p++)
                if (vsynth_param(p)->page == sy.page && i++ == k)
                    break;
            if (p < VP_N)
                param_turn(p, d, &sy.acc[k]);
        }
    }
    if (down & 1u << B_SAVE)             /* M10: save this sound as a user preset */
        save_open();
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
        player_stats.preset_used = upreset_count();
        player_stats.preset_sel = sy.preset;
        player_stats.save_screen = sy.scr == SY_SAVE ? 1u + sy.sv_confirm : 0;
        player_stats.save_slot = sy.sv_slot;
        {
            char lab[24];
            pl_label(sy.preset, lab);
            vpop_text(player_stats.preset_name, lab, sizeof player_stats.preset_name);
            vpop_text(player_stats.next_text, sy.scr == SY_SAVE ? sy.sv_name : "", sizeof player_stats.next_text);   /* (the name
                                          * being edited, for the tests) */
        }
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
    if (sy.scr == SY_SAVE) {             /* M10 */
        h = h * 31u + sy.sv_slot * 7u + sy.sv_cursor * 13u + sy.sv_confirm * 17u;
        for (k = 0; k < VS_NAME; k++)
            h = h * 31u + (uint8_t)sy.sv_name[k];
    }
    h = h * 31u + (now / HINT_PAGE_MS & 1u) * 3u;   /* M10.1: the bottom row's page */
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
    int32_t room = 234 - text_w(&FONT_XS, right) - 6 - 8;   /* M10.1: a title that does not fit beside the right text: the next smaller font */
    vpop_text(player_stats.title_now, title, sizeof player_stats.title_now);   /* M10.1: for the tests */
    cv_begin(240, 32, C_BLACK);
    if (text_w(&FONT_B, title) <= room)
        cv_text(6, 4, &FONT_B, title, C_HI);
    else if (text_w(&FONT_XS, title) <= room)
        cv_text(6, 8, &FONT_XS, title, C_HI);
    else
        cv_text(6, 9, &FONT_T, title, C_HI);
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
    if (pl_slot(sy.preset) >= 0) {       /* M10: a saved preset: "U05" */
        char lab[24];
        pl_label(sy.preset, lab);
        lab[3] = 0;
        text_cat(t, lab);
    } else {
        num_cat(t, sy.preset + 1u);
        text_cat(t, " / ");
        num_cat(t, vsynth_presets());
    }
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
    draw_legend(Y_LEGEND, (now / HINT_PAGE_MS) & 1u ? "PRESET sound|SELECT tempo|EDIT pages" :
                          "K1-K4 quick|PLAY drums|SEQ pattern|GLO help");
}

static void draw_synth_page(uint32_t now)
{
    char t[16] = "";
    uint32_t p, k = 0;
    num_cat(t, sy.page + 1u);
    text_cat(t, "/");
    num_cat(t, VS_PAGES);
    {                                    /* M10.1: the title names the preset being edited: "FX 2 - POTATO", "ENVELOPE - U05 ZUNJET" */
        char ti[48] = "", lab[24];
        text_cat(ti, vsynth_page_name(sy.page));
        text_cat(ti, " - ");
        pl_label(sy.preset, lab);
        text_cat(ti, lab);
        synth_top(ti, t);
    }
    {
        char cells[48] = "", v[16];
        for (p = 0; p < VP_N && k < 4u; p++)
            if (vsynth_param(p)->page == sy.page) {
                synth_cell(k++, p, 0);
                v[0] = 0;
                vsynth_value_text(v, p, vsynth_get(p));
                if (cells[0])
                    text_cat(cells, " ");
                text_cat(cells, v);
            }
        vpop_text(player_stats.cells_now, cells, sizeof player_stats.cells_now);   /* (for the tests) */
    }
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
    static const char *const R[10][2] = {{"EDIT", "next page"}, {"ENV", "envelope"}, {"FX", "effect pages"},
                                         {"ARP", "arp on/off"}, {"PLAY", "drums"}, {"SEQ", "pattern"},
                                         {"HOME", "main / mode"}, {"GLO", "again: settings"},
                                         {"SAVE", "save sound"}, {"LFO SCL REC", "not yet"}};
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

/* the SAVE screen (M10): five slots around the chosen one, the name with its cursor, or the REPLACE question */
static void draw_synth_save(void)
{
    uint32_t i, top = sy.sv_slot >= 2u ? sy.sv_slot - 2u : 0;
    char t[16] = "SAVE TO U", nm[VS_NAME + 1];
    if (top > UP_SLOTS - 5u)
        top = UP_SLOTS - 5u;
    t[9] = (char)('0' + (sy.sv_slot + 1u) / 10u);
    t[10] = (char)('0' + (sy.sv_slot + 1u) % 10u);
    t[11] = 0;
    synth_top(t, sy.sv_confirm ? "SAVE = yes   HOME = no" : "SELECT: slot");
    for (i = 0; i < 5u; i++) {
        uint32_t s = top + i;
        char u[8] = "U", name[VS_NAME + 1];
        int sel = s == sy.sv_slot;
        u[1] = (char)('0' + (s + 1u) / 10u);
        u[2] = (char)('0' + (s + 1u) % 10u);
        u[3] = 0;
        upreset_name(s, name);
        cv_begin(240, 20, C_BLACK);
        if (sel)
            cv_rect(2, 1, 236, 18, C_LINE);
        cv_text(10, 2, &FONT_XS, u, sel ? C_HI : C_GRAY);
        cv_text(48, 2, &FONT_XS, name[0] ? name : "EMPTY", name[0] ? (sel ? C_WHITE : C_GRAY) : C_DIM);
        cv_blit(0, 34u + i * 20u);
    }
    cv_begin(240, 76, C_BLACK);
    if (sy.sv_confirm) {                 /* the slot holds a preset: replace it? */
        char q[40] = "REPLACE U", name[VS_NAME + 1];
        q[9] = (char)('0' + (sy.sv_slot + 1u) / 10u);
        q[10] = (char)('0' + (sy.sv_slot + 1u) % 10u);
        q[11] = 0;
        upreset_name(sy.sv_slot, name);
        text_cat(q, " ");
        text_cat(q, name);
        text_cat(q, "?");
        text_in(0, 240, 8, &FONT_B, q, C_WHITE);
        text_in(0, 240, 40, &FONT_XS, "SAVE = yes    HOME = no", C_AMB);
    } else {
        uint32_t k;
        int32_t x = 14, cx = 14, cw = 10;
        for (k = 0; k < VS_NAME; k++)
            nm[k] = sy.sv_name[k];
        nm[VS_NAME] = 0;
        cv_text(10, 2, &FONT_XS, "NAME", C_GRAY);
        for (k = 0; k < VS_NAME; k++) {  /* a character at a time: the cursor under one */
            char c[2] = {nm[k], 0};
            int32_t w = text_w(&FONT_M, c[0] == ' ' ? "-" : c);
            if (k == sy.sv_cursor)
                cx = x, cw = w;
            cv_text(x, 20, &FONT_M, c[0] == ' ' ? " " : c, C_WHITE);
            x += w + 2;
        }
        cv_rect(cx, 48, cw, 3, C_HI);
    }
    cv_blit(0, 138);
    draw_legend(Y_LEGEND, sy.sv_confirm ? "SAVE yes|HOME no" : "K1 cursor|K2 letter|SAVE save|HOME back");
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
    case SY_SAVE:
        draw_synth_save();
        break;
    default:
        draw_synth_main(now);
        break;
    }
}
