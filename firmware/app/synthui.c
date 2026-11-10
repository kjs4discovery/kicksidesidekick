/* Licence: TBD (see D017) */
/* M5 Part 3 (D030): the mode screen and SYNTH mode's screen and controls. Included by ui.c (fm1-base's unity
 * build), after its drawing helpers. The sound is vsynth.c's.
 *
 * The mode screen comes after the splash at every power-on: the mascot (the splash's logo), LEARN and SYNTH, the
 * cursor on LEARN; SELECT moves it, PLAY or EDIT chooses. (fm1-base's power-on rescue, REC / OCT- + OCT+ held, runs
 * before the app: unaffected.) M6: HOME held 2 s from any screen, or Settings > Switch mode, comes back here (the
 * song stops, or the synth goes quiet and gives the song buffer back); the cursor then on the other mode.
 *
 * SYNTH: the keys play the synth (OCT- / OCT+ their octave). SELECT: the preset. K1 cutoff, K2 resonance, K3
 * attack, K4 release, ALGORITHM chorus, PRESET delay. ARP: the arpeggiator on / off; SEQ: its pattern (UP, DOWN,
 * UP-DOWN, M6: RANDOM); GLO: its rate (1/4, 1/8, M6: 1/8T, 1/16); ARP held + SELECT: the tempo (60..200 BPM, the
 * delay and the drum jam follow it).
 * M6: EDIT held is SHIFT: K1-K4 then set reverb, bit-crush, the arp rate and detune (the screen shows that set while
 * it is held). The drum jam: PLAY starts / stops a drum pattern from the learning engine's styles (FX: the next
 * style, SCL: its level 1-3), looped at the tempo, under the synth (library.c lib_jam).
 * Learning mode and SYNTH never run at once: the synth's delay lines take library.c's song buffer (dec; the jam the
 * rest of it), and a song sent over USB in SYNTH mode is refused (library.c). */
#include "vsynth.h"

static struct {
    uint32_t mode_sel;                   /* the mode screen's cursor: 0 LEARN, 1 SYNTH */
    uint32_t preset, arp, arp_last, rate, bpm;
    uint32_t arp_turned;                 /* the tempo turned while ARP was held: its release does not toggle */
    uint8_t key_note[NKEYS];
    uint32_t shown;                      /* what the screen shows (a hash): redrawn when it changes */
    uint32_t say_until;
    uint32_t shift;                      /* M6: EDIT held */
    uint32_t jam, jam_style, jam_level;  /* M6: the drum jam on; its style (JAM_SONGS), level 1-3 */
    int32_t rate_acc;                    /* M6: SHIFT + K3 turns, toward the next rate */
} sy = {.arp_last = ARP_UP, .rate = RATE_8, .bpm = 120, .jam_style = 4, .jam_level = 2};

static const char *const ARP_NAME[ARP_MODES] = {"OFF", "UP", "DOWN", "UP-DOWN", "RANDOM"};
static const char *const RATE_NAME[RATES] = {"1/4", "1/8", "1/8T", "1/16"};

static void synth_load(uint32_t i);
static void draw_legend(int32_t y, const char *t);   /* (ui.c, below) */

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
    synth_load(0);
    ui.view = V_SYNTH;
    sy.shown = 0xFFFFFFFFu;
}

/* M6: back to the mode screen, from either mode */
static void mode_leave(void)
{
    if (vsynth_active()) {
        uint32_t t = plat_ms();
        lib_jam_stop();                  /* M6: the drum jam stops first (it reads the song buffer too) */
        synth_mix_eas(0);
        sy.jam = 0;
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
    sy.preset = i;
    vsynth_preset(i);                    /* its knobs; an arpeggio if the preset has one */
    sy.arp = vsynth_preset_arp(i, &sy.rate);
    if (sy.arp)
        sy.arp_last = sy.arp;
    vsynth_arp(sy.arp, sy.rate, sy.bpm); /* (at the tempo set) */
    player_stats.synth_preset = i;
}

static void synth_say(const char *a, const char *b)
{
    app_say(a, b);
    sy.say_until = plat_ms() + 1200u;
}

static void synth_jam(int on)            /* M6: the drum jam on (again: the style or level now) or off */
{
    int r;
    if (!on) {
        lib_jam_stop();
        synth_mix_eas(0);
        sy.jam = 0;
        return;
    }
    synth_mix_eas(1);
    r = lib_jam(sy.jam_style, sy.jam_level, sy.bpm);
    sy.jam = r == SONG_OK;
    if (!sy.jam) {
        synth_mix_eas(0);
        synth_say("DRUMS", song_error_name(r));
    }
}

static void synth_input(uint32_t keys, uint32_t btn, uint32_t down)
{
    static const uint8_t KNOB_ENC[6] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_ALGO, EN_PRESET};
    static const uint8_t SHIFT_KNOB[4] = {VS_REVERB, VS_CRUSH, 0xFF, VS_DETUNE};   /* (K3: the arp rate) */
    uint32_t k, lit = 0;
    int32_t d;
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            sy.key_note[k] = (uint8_t)clamp(KEY_LOW + (int32_t)k + 12 * ui.oct, 0, 127);
            vsynth_key(sy.key_note[k], 1);
            player_stats.key_note_last = sy.key_note[k];
        } else if (!(keys & bit) && (ui.keys & bit)) {
            vsynth_key(sy.key_note[k], 0);
        }
    }
    if (down & (1u << B_OCTDN | 1u << B_OCTUP)) {
        char t[16] = "OCTAVE ";
        ui.oct = clamp(ui.oct + ((down >> B_OCTUP & 1u) ? 1 : 0) - ((down >> B_OCTDN & 1u) ? 1 : 0), -OCT_MAX, OCT_MAX);
        player_stats.keys_octave = ui.oct;
        snum_cat(t, ui.oct);
        synth_say("KEYS", ui.oct ? t : "OCTAVE 0");
    }
    d = plat_enc(EN_SELECT);
    if (d && (btn >> B_ARP & 1u)) {      /* ARP held: the tempo (the arp, the delay, the drum jam) */
        sy.bpm = (uint32_t)clamp((int32_t)sy.bpm + d, 60, 200);
        sy.arp_turned = 1;
        vsynth_arp(sy.arp, sy.rate, sy.bpm);
        if (sy.jam)
            player_tempo(sy.bpm);
        sy.say_until = 0;
    } else if (d) {
        int32_t n = (int32_t)vsynth_presets();
        synth_load((uint32_t)((((int32_t)sy.preset + d) % n + n) % n));
    }
    sy.shift = btn >> B_EDIT & 1u;       /* M6: SHIFT: K1-K4's second set */
    for (k = 0; k < 6u; k++) {
        d = plat_enc(KNOB_ENC[k]);
        if (!d)
            continue;
        if (sy.shift && k < 4u && SHIFT_KNOB[k] == 0xFF) {   /* the arp rate, a step every 2 clicks */
            sy.rate_acc += d;
            if (sy.rate_acc >= 2 || sy.rate_acc <= -2) {
                sy.rate = (uint32_t)clamp((int32_t)sy.rate + (sy.rate_acc > 0 ? 1 : -1), 0, (int32_t)RATES - 1);
                sy.rate_acc = 0;
                vsynth_arp(sy.arp, sy.rate, sy.bpm);
                synth_say("ARP RATE", RATE_NAME[sy.rate]);
            }
        } else if (sy.shift && k < 4u) {
            vsynth_set_knob(SHIFT_KNOB[k], vsynth_knob(SHIFT_KNOB[k]) + 2 * d);
        } else {
            vsynth_set_knob(k, vsynth_knob(k) + 2 * d);
        }
    }
    if (ui.buttons >> B_ARP & 1u && !(btn >> B_ARP & 1u)) {   /* ARP released: on / off (the last pattern), unless
                                                               * the tempo was turned while it was held */
        if (!sy.arp_turned) {
            sy.arp = sy.arp ? ARP_OFF : sy.arp_last;
            vsynth_arp(sy.arp, sy.rate, sy.bpm);
            synth_say("ARPEGGIATOR", ARP_NAME[sy.arp]);
        }
        sy.arp_turned = 0;
    }
    if (down & (1u << B_SEQ)) {          /* the pattern (M6: + RANDOM) */
        sy.arp_last = sy.arp_last % (ARP_MODES - 1u) + 1u;
        sy.arp = sy.arp_last;
        vsynth_arp(sy.arp, sy.rate, sy.bpm);
        synth_say("ARPEGGIATOR", ARP_NAME[sy.arp]);
    }
    if (down & (1u << B_GLO)) {          /* the rate */
        sy.rate = (sy.rate + 1u) % RATES;
        vsynth_arp(sy.arp, sy.rate, sy.bpm);
        synth_say("ARP RATE", RATE_NAME[sy.rate]);
    }
    if (down & (1u << B_PLAY)) {         /* M6: the drum jam on / off */
        synth_jam(!sy.jam);
        if (sy.jam) {
            char t[16] = "";
            text_cat(t, lib_jam_name(sy.jam_style));
            text_cat(t, " L");
            num_cat(t, sy.jam_level);
            synth_say("DRUMS", t);
        }
    }
    if (down & (1u << B_FX | 1u << B_SEL)) {   /* M6: the jam's style (FX), its level (SCL); playing: at once */
        char t[16] = "";
        if (down & 1u << B_FX)
            sy.jam_style = (sy.jam_style + 1u) % lib_jam_styles();
        if (down & 1u << B_SEL)
            sy.jam_level = sy.jam_level % 3u + 1u;
        if (sy.jam)
            synth_jam(1);
        text_cat(t, lib_jam_name(sy.jam_style));
        text_cat(t, " L");
        num_cat(t, sy.jam_level);
        synth_say(sy.jam ? "DRUMS" : "DRUMS (PLAY)", t);
    }
    {
        const struct vs_stats *s = vsynth_stats();
        player_stats.synth_notes = s->notes;
        player_stats.synth_voices_max = s->voices_max;
        player_stats.arp_steps = s->arp_steps;
        player_stats.synth_peak = s->peak;
        player_stats.shift = sy.shift;
        player_stats.fx_reverb = (uint32_t)vsynth_knob(VS_REVERB);
        player_stats.fx_crush = (uint32_t)vsynth_knob(VS_CRUSH);
        player_stats.arp_pattern = sy.arp;
        player_stats.arp_rate = sy.rate;
        player_stats.jam = sy.jam;
        player_stats.jam_notes = player_stats.notes_on[0];
        player_stats.jam_style = sy.jam_style;
        player_stats.jam_level = sy.jam_level;
    }
    if (sy.arp)
        lit |= 1u << B_ARP;
    if (sy.jam)
        lit |= 1u << B_PLAY;
    if (sy.shift)
        lit |= 1u << B_EDIT;
    plat_lights(lit, keys, 1u << B_ARP | 1u << B_SEQ | 1u << B_GLO | 1u << B_OCTDN | 1u << B_OCTUP | 1u << B_PLAY |
                           1u << B_EDIT | 1u << B_FX | 1u << B_SEL, 0, 1);
}

static uint32_t synth_hash(uint32_t now)
{
    uint32_t h = sy.preset * 2654435761u, k;
    for (k = 0; k < VS_NKNOB; k++)
        h = h * 31u + (uint32_t)vsynth_knob(k);
    h = h * 31u + sy.arp * 7u + sy.rate * 131u + sy.bpm * 1031u + (uint32_t)(ui.oct + 8) * 65537u;
    h = h * 31u + sy.shift * 3u + sy.jam * 5u + sy.jam_style * 17u + sy.jam_level * 257u + (now / 3000u & 1u) * 9u;
    if (sy.jam)                          /* the jam's beat light on the screen */
        h = h * 31u + (uint32_t)(player_tick(0) / (int32_t)(jam.ppq ? jam.ppq : 480u));
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

static void draw_synth(void)
{
    static const char *const KN[2][6] = {{"CUTOFF", "RESO", "ATTACK", "RELEASE", "CHORUS", "DELAY"},
                                         {"REVERB", "CRUSH", "ARP RATE", "DETUNE", "CHORUS", "DELAY"}};
    static const uint8_t KI[2][6] = {{VS_CUT, VS_RES, VS_ATK, VS_REL, VS_CHORUS, VS_DELAY},
                                     {VS_REVERB, VS_CRUSH, 0xFF, VS_DETUNE, VS_CHORUS, VS_DELAY}};
    static const char *const KC[6] = {"K1", "K2", "K3", "K4", "ALGO", "PRESET"};
    uint32_t k, now = plat_ms(), sh = sy.shift ? 1u : 0u;
    char t[48] = "";
    if (ui.redraw)
        lcd_fill(0, 0, 240, 240, C_BLACK);
    cv_begin(240, 32, C_BLACK);
    cv_text(6, 4, &FONT_B, "SYNTH", C_HI);
    if (sh) {
        cv_rect(66, 4, 40, 16, C_HI);
        cv_text(70, 6, &FONT_XS, "SHIFT", C_BLACK);
    }
    num_cat(t, sy.preset + 1u);
    text_cat(t, " / ");
    num_cat(t, vsynth_presets());
    cv_text(234 - text_w(&FONT_XS, t), 6, &FONT_XS, t, C_DIM);
    cv_rect(0, 31, 240, 1, C_LINE);
    cv_blit(0, 0);
    cv_begin(240, 40, C_BLACK);          /* the preset, or a message for a moment */
    if (now < sy.say_until) {
        text_in(0, 240, 0, &FONT_XS, ui.msg[0], C_GRAY);
        text_in(0, 240, 16, &FONT_B, ui.msg[1], C_WHITE);
    } else {
        text_in(0, 240, 6, &FONT_M, vsynth_preset_name(sy.preset), C_WHITE);
    }
    cv_blit(0, 36);
    for (k = 0; k < 6u; k++) {           /* the knobs: two rows of three, each with a bar (SHIFT: K1-K4's second set) */
        int32_t v, x = (int32_t)(k % 3u) * 80;
        int arp_rate = KI[sh][k] == 0xFF;
        v = arp_rate ? (int32_t)(sy.rate * 127u / (RATES - 1u)) : vsynth_knob(KI[sh][k]);
        t[0] = 0;
        if (arp_rate)
            text_cat(t, RATE_NAME[sy.rate]);
        else
            num_cat(t, (uint32_t)v);
        cv_begin(78, 44, C_BLACK);
        cv_text(4, 0, &FONT_XS, KN[sh][k], sh && k < 4u ? C_HI : C_GRAY);
        cv_text(74 - text_w(&FONT_XS, t), 0, &FONT_XS, t, C_WHITE);
        cv_rect(4, 18, 70, 6, C_LINE);
        cv_rect(4, 18, 70 * v / 127, 6, sh && k < 4u ? C_WHITE : C_HI);
        cv_text(4, 28, &FONT_T, KC[k], C_DIM);
        cv_blit((uint32_t)x + 1u, 78u + (k / 3u) * 46u);
    }
    cv_begin(240, 20, C_BLACK);
    t[0] = 0;
    text_cat(t, "ARP ");
    text_cat(t, ARP_NAME[sy.arp]);
    if (sy.arp) {
        text_cat(t, "  ");
        text_cat(t, RATE_NAME[sy.rate]);
    }
    text_cat(t, "   ");
    num_cat(t, sy.bpm);
    text_cat(t, " BPM   OCT ");
    snum_cat(t, ui.oct);
    cv_text(6, 2, &FONT_XS, t, sy.arp ? C_AMB : C_GRAY);
    cv_blit(0, 171);
    cv_begin(240, 22, C_BLACK);          /* M6: the drum jam: style, level, the beats */
    t[0] = 0;
    text_cat(t, "DRUMS ");
    text_cat(t, lib_jam_name(sy.jam_style));
    text_cat(t, " L");
    num_cat(t, sy.jam_level);
    cv_text(6, 3, &FONT_XS, t, sy.jam ? C_AMB : C_GRAY);
    if (sy.jam) {
        uint32_t ppq = jam.ppq ? jam.ppq : 480u, bpb = jam.beats_per_bar ? jam.beats_per_bar : 4u, b;
        int32_t tk = player_tick(0);
        uint32_t beat = tk > 0 ? (uint32_t)tk / ppq % bpb : 0;
        for (b = 0; b < bpb; b++)
            cv_rect(150 + (int32_t)b * 20, 5, 14, 10, b == beat ? (b ? C_HI : C_WHITE) : C_LINE);
    } else {
        cv_text(150, 3, &FONT_XS, "PLAY: on", C_DIM);
    }
    cv_blit(0, 192);
    draw_legend(Y_LEGEND, (now / 3000u) & 1u ? "ARP SEQ GLO arp|FX SCL drums|HOME held mode" :
                          "SELECT preset|EDIT held shift|PLAY drums");
}
