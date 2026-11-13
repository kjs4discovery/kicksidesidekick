/* Licence: TBD (see D017) */
/* M8 (D046, D048, D049): BACKTRACK mode. Included by ui.c (fm1-base's unity build) after synthui.c.
 *
 * The backtracks are songs of kind backtrack (format 2.2, FORM) in the song store, sent from the laptop
 * (songs/backtracks/, tools/backtracks.py); SONG LEARN lists only songs, BACKTRACK only these, by their number.
 *
 * SETUP (instead of a song list): K1 the progression (its name and numerals), K2 the style (11), K3 the structure
 * (LOOP, VERSE/CHORUS, FULL), K4 the key (12; the progression's chords in it), SELECT the tempo (the style's own until
 * turned), PLAY starts, GLO help, HOME held 2 s the mode screen. The choices are kept until power-off.
 * PLAY: the backtrack opened (lib_open), played in the style and metre chosen (lib_restyle: WALTZ in 3/4), in the key
 * (player_transpose), at the tempo, its structure as a loop (LOOP: the first verse; VERSE/CHORUS: the first verse and
 * chorus; FULL: the whole file to its ending). Then SONG LEARN's play screen and controls (ui.c input_play), but: HOME
 * (stopped) back to SETUP, K4 the melody sound, the keys play the melody sound.
 *
 * The melody sound (D049, Part 0's numbers): the 30 SYNTH presets on 4 voices, mixed over EAS (synth_band). The song
 * buffer is split: the backtrack's levels in its first BT_SONG_ROOM bytes (lib_room), the synth's effect lines after
 * them with the delay's maximum halved (VS_MEM_BT). */

#define BT_SONG_ROOM (LIB_DEC_BYTES - VS_MEM_BT)   /* 112,896: every backtrack in every style fits (backtracks.py) */
#define BT_VOICES 4u
enum { BS_LOOP, BS_VERSE_CHORUS, BS_FULL, BS_N };
#define BT_STYLES 11u
static const char *const BT_STYLE_NAME[BT_STYLES] = {"POP", "ROCK", "INDIE POP", "BALLAD", "SHUFFLE", "STRIDE",
                                                     "WALTZ", "COUNTRY", "BOSSA NOVA", "FUNK", "REGGAE"};
static const uint8_t BT_STYLE_OF[BT_STYLES] = {4, 6, 7, 1, 2, 3, 0, 5, 8, 9, 10};   /* song.h STYL indices */
static const uint8_t BT_STYLE_BPM[BT_STYLES] = {100, 120, 110, 70, 96, 110, 96, 140, 130, 100, 80};
#define BT_WALTZ 6u
static const char *const BT_STRUCT_NAME[BS_N] = {"LOOP", "VERSE/CHORUS", "FULL"};


static const struct lib_item *bt_item(uint32_t k)
{
    int err;
    return k < bt.n ? lib_peek(bt.list[k], &err) : 0;
}

static int lib_is_backtrack(uint32_t i)
{
    int err;
    const struct lib_item *it = lib_peek(i, &err);
    return it && it->kind == SONG_KIND_BACKTRACK;
}

/* the backtracks in the store, by number (one that does not peek: left out) */
static void bt_list_build(void)
{
    uint32_t i, k, n = lib_count();
    bt.n = 0;
    for (i = 0; i < n && bt.n < BT_MAX; i++)
        if (lib_is_backtrack(i)) {
            int err;
            const struct lib_item *a = lib_peek(i, &err);
            for (k = bt.n++; k > 0 && lib_peek(bt.list[k - 1u], &err)->number > a->number; k--)
                bt.list[k] = bt.list[k - 1u];
            bt.list[k] = (uint8_t)i;
        }
    if (bt.prog >= bt.n)
        bt.prog = 0;
    bt.shown = 0xFFFFFFFFu;
}

/* the key's name: the progression's tonic moved by the key chosen ("C", "Ebm") */
static const char *const *bt_names(const struct lib_item *it)
{
    uint32_t minor = it && it->key_mode == 1u;
    return names_of(minor ? 9u : 0u, minor, ((int32_t)bt.key + 5) % 12 - 5);
}

static void bt_key_name(char *d, const struct lib_item *it)
{
    uint32_t minor = it && it->key_mode == 1u, tonic = (minor ? 9u : 0u) + bt.key;
    text_cat(d, bt_names(it)[tonic % 12u]);
    if (minor)
        text_cat(d, "m");
}

/* the numerals before " | " (the verse) as chords in the key chosen: "C G Am F" */
static void bt_chords(char *d, const struct lib_item *it, uint32_t max)
{
    static const uint8_t DEG[7] = {0, 2, 4, 5, 7, 9, 11};
    const char *p = it ? it->numerals : "";
    uint32_t tonic = (it && it->key_mode == 1u ? 9u : 0u) + bt.key;
    while (*p && *p != '|' && text_len(d) + 8u < max) {
        int flat = 0, minor = 0, deg = -1;
        char q[8] = "";
        while (*p == ' ')
            p++;
        if (!*p || *p == '|')
            break;
        if (*p == 'b')
            flat = 1, p++;
        minor = *p == 'i' || *p == 'v';
        while (*p == 'I' || *p == 'V' || *p == 'i' || *p == 'v') {   /* I II III IV V VI VII */
            uint32_t at = text_len(q);
            if (at < 6u) {
                q[at] = (char)(*p == 'i' ? 'I' : *p == 'v' ? 'V' : *p);
                q[at + 1u] = 0;
            }
            p++;
        }
        if (same_text(q, "I")) deg = 0;
        else if (same_text(q, "II")) deg = 1;
        else if (same_text(q, "III")) deg = 2;
        else if (same_text(q, "IV")) deg = 3;
        else if (same_text(q, "V")) deg = 4;
        else if (same_text(q, "VI")) deg = 5;
        else if (same_text(q, "VII")) deg = 6;
        if (deg < 0)
            break;
        if (*d)
            text_cat(d, " ");
        text_cat(d, bt_names(it)[(tonic + DEG[deg] + 12u - (uint32_t)flat) % 12u]);
        if (minor)
            text_cat(d, "m");
        if (p[0] == 'm' && p[1] == 'a' && p[2] == 'j' && p[3] == '7')
            text_cat(d, "maj7"), p += 4;
        else if (*p == '7')
            text_cat(d, "7"), p++;
        while (*p && *p != ' ' && *p != '|')
            p++;
    }
}

/* ------------------------------------------------------------------- the setup screen --- */
static void bt_row(int32_t y, const char *k, const char *label, const char *value, const felucca_font_t *f,
                   const char *sub)
{
    cv_text(6, y, &FONT_XS, k, C_AMB);
    cv_text(30, y, &FONT_XS, label, C_GRAY);
    cv_text(234 - text_w(f, value), y - 2, f, value, C_WHITE);
    if (sub && sub[0])
        cv_text(234 - text_w(&FONT_XS, sub), y + 18, &FONT_XS, sub, C_HI);
}

static void draw_setup(void)
{
    const struct lib_item *it = bt_item(bt.prog);
    char t[48] = "", kn[16] = "", ch[48] = "";
    uint32_t band;
    for (band = 0; band < 2u; band++) {
        int32_t y0 = (int32_t)band * 120;
        cv_begin(240, 120, C_BLACK);
        cv_band(0, 120);
        if (!band) {
            cv_text(6, 2, &FONT_B, "BACKTRACK", C_WHITE);
            cv_text(234 - text_w(&FONT_XS, "GLO: help"), 6, &FONT_XS, "GLO: help", C_DIM);
            cv_rect(0, 24, 240, 1, C_LINE);
        }
        if (!bt.n) {
            text_in(0, 240, 60 - y0, &FONT_B, "NO BACKTRACKS", C_WHITE);
            text_in(0, 240, 86 - y0, &FONT_XS, "send songs/backtracks/ from", C_GRAY);
            text_in(0, 240, 102 - y0, &FONT_XS, "the laptop (in SONG LEARN)", C_GRAY);
        } else {
            t[0] = 0;
            num_cat(t, it ? it->number : 0);
            text_cat(t, "  ");
            text_cat(t, it ? it->title : "?");
            bt_row(32 - y0, "K1", "PROGRESSION", t, &FONT_B, it ? it->numerals : "");
            bt_row(76 - y0, "K2", "STYLE", BT_STYLE_NAME[bt.style], &FONT_B, bt.style == BT_WALTZ ? "in 3/4" : "");
            bt_row(112 - y0, "K3", "STRUCTURE", BT_STRUCT_NAME[bt.structure], &FONT_B, "");
            kn[0] = 0;
            bt_key_name(kn, it);
            ch[0] = 0;
            bt_chords(ch, it, sizeof ch);
            bt_row(144 - y0, "K4", "KEY", kn, &FONT_B, ch);
            t[0] = 0;
            num_cat(t, bt.bpm);
            text_cat(t, " BPM");
            bt_row(186 - y0, "SEL", "TEMPO", t, &FONT_B, "");
        }
        if (plat_ms() < ui.msg_until) {   /* a message (an error opening one) over the lower rows */
            cv_rect(10, 150 - y0, 220, 50, C_BLACK);
            frame_rect(10, 150 - y0, 220, 50, C_AMB);
            text_in(10, 220, 156 - y0, &FONT_B, ui.msg[0], C_WHITE);
            text_in(10, 220, 178 - y0, &FONT_XS, ui.msg[1], C_GRAY);
        }
        cv_blit(0, (uint32_t)y0);
    }
    draw_legend(Y_LEGEND, bt.n ? "PLAY start|GLO help|HOME hold: modes" : "HOME hold: modes|GLO help");
}

static uint32_t bt_hash(void)
{
    return bt.prog | bt.style << 6 | bt.structure << 10 | bt.key << 12 | bt.bpm << 16 | bt.n << 24 |
           (uint32_t)(plat_ms() < ui.msg_until) << 31;
}

/* the style's tempo, unless SELECT was turned */
static void bt_style_bpm(void)
{
    if (!bt.bpm_turned)
        bt.bpm = BT_STYLE_BPM[bt.style];
}

static void bt_sound(uint32_t i)
{
    bt.sound = i % vsynth_presets();
    vsynth_load(vsynth_stock(bt.sound));
    vsynth_set(VP_ARP_ON, 0);            /* (the melody: the notes played, not an arpeggio) */
    player_stats.synth_preset = bt.sound;
}

/* into BACKTRACK (the mode screen): the synth on the buffer's upper part, 4 voices, the band under it */
static void bt_enter(void)
{
    player_stats.mode = 3;
    lib_release();
    lib_room = BT_SONG_ROOM;
    synth_drums(0);
    vsynth_start(dec + BT_SONG_ROOM, sizeof dec - BT_SONG_ROOM);
    vsynth_voices(BT_VOICES);
    synth_band(1);
    if (!bt.entered) {                   /* the defaults, once a power-on: 1 POP 1, POP, LOOP, C, the style's tempo */
        bt.entered = 1;
        bt.prog = bt.style = bt.structure = bt.key = 0;
        bt.bpm_turned = 0;
        bt.sound = 0;
        bt_style_bpm();
    }
    bt_sound(bt.sound);
    vsynth_tempo(bt.bpm < 60u ? 60u : bt.bpm > 200u ? 200u : bt.bpm);
    bt.on = 1;
    bt_list_build();
    ui.view = V_SETUP;
    ui.redraw = 1;
}

/* out of BACKTRACK (mode_leave): the song stops, the synth gives the buffer back */
static void bt_leave(void)
{
    uint32_t t = plat_ms();
    lib_release();
    synth_band(0);
    vsynth_stop();
    while (plat_ms() - t < 20u)
        ;
    lib_room = sizeof dec;
    bt.on = 0;
}

/* the first section of a role (from section i on), or nsect */
static uint32_t bt_section(uint32_t role, uint32_t from)
{
    uint32_t i;
    for (i = from; i < ui.s->nsect; i++)
        if (ui.s->role[i] == role)
            return i;
    return ui.s->nsect;
}

static void open_play(const struct song *s);   /* (ui.c) */

/* PLAY on SETUP: the backtrack opened in the style, metre, key, tempo and structure chosen; the play screen */
static void bt_open(void)
{
    const struct song *s;
    uint32_t v, c;
    int r;
    if (!bt.n)
        return;
    lib_release();
    r = lib_open(bt.list[bt.prog], &s);
    if (r == SONG_OK)
        r = lib_restyle(BT_STYLE_OF[bt.style], bt.style == BT_WALTZ ? 3u : 4u);
    if (r) {
        app_say("BACKTRACK ERROR", song_error_name(r));
        ui.redraw = 1;
        return;
    }
    open_play(s);
    player_tempo(bt.bpm);
    vsynth_tempo(bt.bpm < 60u ? 60u : bt.bpm > 200u ? 200u : bt.bpm);
    ui.key = ((int32_t)bt.key + 5) % 12 - 5;   /* (the nearest way: -5..+6, as the KEY mode) */
    player_transpose(ui.key);
    v = bt_section(ROLE_VERSE, 0);
    c = v < s->nsect ? bt_section(ROLE_CHORUS, v) : s->nsect;
    if (bt.structure != BS_FULL && v < s->nsect) {   /* LOOP: the first verse; VERSE/CHORUS: it and the first chorus */
        ui.a_bar = song_section_first(s, v);
        ui.b_bar = bt.structure == BS_VERSE_CHORUS && c < s->nsect ? song_section_first(s, c) +
                   song_section_bars(s, c) - 1u : song_section_first(s, v) + song_section_bars(s, v) - 1u;
        set_from_bar(ui.a_bar);
        set_loop(LOOP_BARS);
    }
    ui.mine = L_MELODY;                  /* (Part 5: nothing is your part: the melody's scale views) */
    ui.pview = (int)view_ok(L_MELODY, PV_SCALE_KB);
    player_stats.view = (uint32_t)ui.pview;
    ui.view = V_PLAY;
    ui.redraw = 1;
    player_start(start_point(), player_count_in_of());   /* (PLAY starts: the count-in as set) */
    app_say(BT_STRUCT_NAME[bt.structure], BT_STYLE_NAME[bt.style]);
}

/* the tests' view (player.h, M8) */
static void bt_stats(void)
{
    const struct vs_stats *v = vsynth_stats();
    const struct lib_item *it = bt_item(bt.prog);
    player_stats.bt_n = bt.n;
    player_stats.bt_prog = it ? it->number : 0;
    player_stats.bt_style = bt.style;
    player_stats.bt_structure = bt.structure;
    player_stats.bt_key = bt.key;
    player_stats.bt_bpm = bt.bpm;
    player_stats.bt_sound = bt.sound;
    player_stats.synth_preset = bt.sound;
    player_stats.synth_notes = v->notes;
    player_stats.synth_voices_max = v->voices_max;
    player_stats.synth_peak = v->peak;
    player_stats.fx_active = v->fx;
    player_stats.synth_engine = vsynth_now()->engine;
    player_stats.scale_kind = SV(S_SCALE) | (ui.s && ui.s->scale == SONG_SCALE_BLUES ? 4u : 0u) |
                              (ui.s && ui.s->key_mode == 1u ? 8u : 0u);
    player_stats.bt_metre = ui.s && bt.on && ui.view != V_SETUP ? ui.s->beats_per_bar : 0;
    player_stats.bt_style_now = ui.s && bt.on && ui.view != V_SETUP ? ui.s->style : 0xFFu;
    {
        uint32_t a = 0, b = 0;
        if (!player_loop_of(&a, &b))
            a = b = 0;
        player_stats.bt_loop_from = a;
        player_stats.bt_loop_to = b;
    }
}

static void bt_setup_input(uint32_t down)
{
    static const uint8_t KE[4] = {EN_K1, EN_K2, EN_K3, EN_K4};
    int32_t d[4], sel = plat_enc(EN_SELECT);
    uint32_t k;
    for (k = 0; k < 4u; k++)
        d[k] = plat_enc(KE[k]);
    if (bt.n && d[0])
        bt.prog = (uint32_t)(((int32_t)bt.prog + d[0]) % (int32_t)bt.n + (int32_t)bt.n) % bt.n;
    if (d[1]) {
        bt.style = (uint32_t)(((int32_t)bt.style + d[1]) % (int32_t)BT_STYLES + (int32_t)BT_STYLES) % BT_STYLES;
        bt_style_bpm();
    }
    if (d[2])
        bt.structure = (uint32_t)(((int32_t)bt.structure + d[2]) % (int32_t)BS_N + (int32_t)BS_N) % BS_N;
    if (d[3])
        bt.key = (uint32_t)(((int32_t)bt.key + d[3]) % 12 + 12) % 12u;
    if (sel) {
        bt.bpm = (uint32_t)clamp((int32_t)bt.bpm + sel, (int32_t)PL_BPM_MIN, (int32_t)PL_BPM_MAX);
        bt.bpm_turned = 1;
    }
    if (down >> B_PLAY & 1u)
        bt_open();
    bt_stats();
}

/* the play screen's own (before input_play): K4 the melody sound, LFO (no melody layer): the melody is yours */
static void bt_play_input(uint32_t down)
{
    int32_t d = plat_enc(EN_K4);
    if (d) {
        int32_t n = (int32_t)vsynth_presets();
        bt_sound((uint32_t)(((int32_t)bt.sound + d) % n + n));
        app_say("MELODY SOUND", vsynth_stock(bt.sound)->name);
        bt.sound_until = plat_ms() + 1500u;
    }
    if (down >> B_LFO & 1u)
        app_say("MELODY", "you play it");
    bt_stats();
}

