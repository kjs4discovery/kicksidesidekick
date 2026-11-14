/* Licence: TBD (see D017) */
/* M8 (D046, D048, D049), M9 (D052, D053): BACKTRACK mode. Included by ui.c (fm1-base's unity build) after synthui.c.
 *
 * The backtracks are songs of kind backtrack (format 2.2, FORM) in the song store, sent from the laptop
 * (songs/backtracks/, tools/backtracks.py); SONG LEARN lists only songs, BACKTRACK only these, by their number.
 *
 * One knob map on SETUP and on the play screen (M9, D053):
 *   K1 progression (1-36)   K2 style (11)   K3 complexity (1 SIMPLE, 2 MEDIUM, 3 BUSY)   K4 melody sound (30 presets)
 *   PRESETS the key (a semitone a click)   ALGO the structure (LOOP, VERSE/CHORUS, FULL)   SELECT the tempo
 * On SETUP they are the choices (kept until power-off); PLAY opens the backtrack. On the play screen K4 and SELECT act at
 * once, the others WAIT for a boundary and the top line says "NEXT: ...": the key and the complexity at the next bar
 * line, the progression, style and structure at the next phrase boundary (a bar where the bar in its section is a
 * multiple of 4, or the loop's wrap). Waiting changes are made together when they can be.
 *
 * The switch is SEAMLESS (docs/decisions-M9.md M9-01): the song buffer's BACKTRACK part (BT_SONG_ROOM) is two halves; the
 * version playing is in one (bt_song[bt.cur]), the next is built in the other while it plays (bt_prepare: song_open,
 * song_restyle, the three layers' level built; about 60 ms for the largest), and the player swaps to it at the bar line,
 * sample-exact (player_swap). Nothing counts in (the player's count-in rule). After the switch the music goes on from
 * the same section and bar of the new file if it has it, else from the start of the structure's loop (or the first
 * verse, FULL).
 *
 * The melody sound (D049): the 30 SYNTH presets on 4 voices, mixed over EAS (synth_band). The song buffer is split: the
 * band's two halves in its first BT_SONG_ROOM bytes, the synth's effect lines after them with the delay's maximum
 * halved (VS_MEM_BT). */

#define BT_SONG_ROOM (LIB_DEC_BYTES - VS_MEM_BT)   /* 112,896: two copies of every backtrack in every style fit (backtracks.py) */
#define BT_HALF (BT_SONG_ROOM / 2u)        /* a version of the band: 56,448 B (the largest need: 54,448) */
#define BT_VOICES 4u
enum { BS_LOOP, BS_VERSE_CHORUS, BS_FULL, BS_N };
#define BT_STYLES 11u
#define BT_NONE 0xFFFFFFFFu
static const char *const BT_STYLE_NAME[BT_STYLES] = {"POP", "ROCK", "INDIE POP", "BALLAD", "SHUFFLE", "STRIDE",
                                                     "WALTZ", "COUNTRY", "BOSSA NOVA", "FUNK", "REGGAE"};
static const uint8_t BT_STYLE_OF[BT_STYLES] = {4, 6, 7, 1, 2, 3, 0, 5, 8, 9, 10};   /* song.h STYL indices */
static const uint8_t BT_STYLE_BPM[BT_STYLES] = {100, 120, 110, 70, 96, 110, 96, 140, 130, 100, 80};
#define BT_WALTZ 6u
/* M10 (D062): the band kits, chosen on SETUP with SEQ: the drums' route (0 EAS's GM kit, 1 vsynth's 808, 2 CHIP), the bass and
 * the harmony GM programs (counted from 0: 33 Acoustic Bass = 32, 25 Nylon Guitar = 24, 39 Synth Bass 1 = 38, 90 Pad 2 = 89,
 * 5 Electric Piano 1 = 4; 0xFF: the file's own = M9's sound). Never GM 81. */
#define BT_KITS 4u
static const char *const BT_KIT_NAME[BT_KITS] = {"STUDIO", "ACOUSTIC", "ELECTRO", "CHIP"};
static const uint8_t BT_KIT_DRUMS[BT_KITS] = {0, 0, 1, 2};
static const uint8_t BT_KIT_BASS[BT_KITS] = {0xFF, 32, 38, 38};
static const uint8_t BT_KIT_HARM[BT_KITS] = {0xFF, 24, 89, 4};
static const char *const BT_STRUCT_NAME[BS_N] = {"LOOP", "VERSE/CHORUS", "FULL"};
static const char *const BT_CPLX_NAME[4] = {"", "SIMPLE", "MEDIUM", "BUSY"};
#define BT_IDLE_MS 300u                    /* the knobs still this long: the new version is built */
#define BT_PLAN_MS 250u                    /* the boundary is chosen at least this far ahead (the build takes about 60 ms) */
#define BT_GUARD_MS 100u                   /* ... and after the build the swap is asked for a line at least this far ahead */

static struct song bt_song[2];             /* the band: the version playing and the one being built */

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

/* the key the music moves to the nearest way (-5..+6) */
static int32_t bt_nearest(uint32_t key) { return ((int32_t)key + 5) % 12 - 5; }

/* the key's names: the progression's tonic moved by the key chosen ("C", "Ebm") */
static const char *const *bt_names(const struct lib_item *it, uint32_t key)
{
    uint32_t minor = it && it->key_mode == 1u;
    return names_of(minor ? 9u : 0u, minor, bt_nearest(key));
}

static void bt_key_name(char *d, const struct lib_item *it, uint32_t key, int word)
{
    uint32_t minor = it && it->key_mode == 1u, tonic = (minor ? 9u : 0u) + key;
    text_cat(d, bt_names(it, key)[tonic % 12u]);
    if (word)
        text_cat(d, minor ? " minor" : " major");
    else if (minor)
        text_cat(d, "m");
}

/* the numerals before " | " (the verse) as chords in the key chosen: "C G Am F" */
static void bt_chords(char *d, const struct lib_item *it, uint32_t max, uint32_t key)
{
    static const uint8_t DEG[7] = {0, 2, 4, 5, 7, 9, 11};
    const char *p = it ? it->numerals : "";
    uint32_t tonic = (it && it->key_mode == 1u ? 9u : 0u) + key;
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
        text_cat(d, bt_names(it, key)[(tonic + DEG[deg] + 12u - (uint32_t)flat) % 12u]);
        if (minor)
            text_cat(d, "m");
        if (p[0] == 'm' && p[1] == 'a' && p[2] == 'j' && p[3] == '7')
            text_cat(d, "maj7"), p += 4;
        else if (p[0] == '7' && p[1] == 'b' && p[2] == '5')   /* ii7b5: half-diminished (M9): Bm7b5 */
            text_cat(d, "7b5"), p += 3;
        else if (*p == '7')
            text_cat(d, "7"), p++;
        while (*p && *p != ' ' && *p != '|')
            p++;
    }
}

/* a small box with MAJOR or MINOR (text as well as colour), right edge at xr, top at y */
static void bt_badge(int32_t xr, int32_t y, const struct lib_item *it)
{
    int minor = it && it->key_mode == 1u;
    const char *t = minor ? "MINOR" : "MAJOR";
    int32_t w = text_w(&FONT_XS, t) + 8;
    cv_rect(xr - w, y, w, 14, minor ? COL_MINOR : C_HI);
    cv_text(xr - w + 4, y + 1, &FONT_XS, t, C_BLACK);
}

/* ------------------------------------------------------------------- the setup screen --- */
static void bt_row(int32_t y, const char *k, const char *label, const char *value, const char *sub)
{
    cv_text(6, y, &FONT_XS, k, C_AMB);
    cv_text(30, y, &FONT_XS, label, C_GRAY);
    cv_text(234 - text_w(&FONT_XS, value), y, &FONT_XS, value, C_WHITE);
    if (sub && sub[0])
        cv_text(234 - text_w(&FONT_XS, sub), y + 14, &FONT_XS, sub, C_HI);
}

static void draw_setup(void)
{
    const struct lib_item *it = bt_item(bt.prog);
    char t[48] = "", kn[24] = "", ch[48] = "";
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
            /* 7 rows in 240 px: the knob labels and values in the small font, the progression's name in the big one */
            cv_text(6, 27 - y0, &FONT_XS, "K1", C_AMB);
            cv_text(30, 27 - y0, &FONT_XS, "PROGRESSION", C_GRAY);
            bt_badge(234, 26 - y0, it);
            t[0] = 0;
            num_cat(t, it ? it->number : 0);
            text_cat(t, "  ");
            text_cat(t, it ? it->title : "?");
            cv_text(6, 41 - y0, &FONT_B, t, C_WHITE);
            cv_text(6, 62 - y0, &FONT_XS, it ? it->numerals : "", C_HI);
            /* (M10: the KIT row under TEMPO: the rows from STYLE on are 2 px closer, 18 px apart, KEY's chord line 14 under it) */
            bt_row(78 - y0, "K2", "STYLE", bt.style == BT_WALTZ ? "WALTZ (3/4)" : BT_STYLE_NAME[bt.style], "");
            bt_row(96 - y0, "K3", "COMPLEXITY", BT_CPLX_NAME[bt.cplx], "");
            bt_row(114 - y0, "K4", "MELODY SOUND", vsynth_stock(bt.sound)->name, "");
            kn[0] = 0;
            bt_key_name(kn, it, bt.key, 1);
            ch[0] = 0;
            bt_chords(ch, it, sizeof ch, bt.key);
            bt_row(132 - y0, "PRE", "KEY", kn, ch);
            bt_row(164 - y0, "ALG", "STRUCTURE", BT_STRUCT_NAME[bt.structure], "");
            t[0] = 0;
            num_cat(t, bt.bpm);
            text_cat(t, " BPM");
            bt_row(182 - y0, "SEL", "TEMPO", t, "");
            bt_row(198 - y0, "SEQ", "KIT", BT_KIT_NAME[bt.kit], "");
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
           (uint32_t)(plat_ms() < ui.msg_until) << 31 ^ bt.cplx << 28 ^ bt.sound << 20 ^ bt.kit * 0x9E3779B1u;
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

/* the kit chosen is given to the player (and withdrawn when BACKTRACK is left: SONG LEARN keeps EAS's GM kit and the files' programs) */
static void bt_kit_apply(int on)
{
    uint32_t k = on ? bt.kit % BT_KITS : 0u;
    player_kit(on ? BT_KIT_DRUMS[k] : 0u, on ? BT_KIT_BASS[k] : 0xFFu, on ? BT_KIT_HARM[k] : 0xFFu);
    player_stats.kit = k;
    player_stats.kit_bass_prog = on ? BT_KIT_BASS[k] : 0xFFu;
    player_stats.kit_harm_prog = on ? BT_KIT_HARM[k] : 0xFFu;
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
    if (!bt.entered) {                   /* the defaults, once a power-on: 1 POP 1, POP, C2, LOOP, C, the style's tempo */
        bt.entered = 1;
        bt.prog = bt.style = bt.structure = bt.key = 0;
        bt.cplx = 2;
        bt.kit = 0;
        bt.bpm_turned = 0;
        bt.sound = 0;
        bt_style_bpm();
    }
    bt_sound(bt.sound);
    bt_kit_apply(1);
    vsynth_tempo(bt.bpm < 60u ? 60u : bt.bpm > 200u ? 200u : bt.bpm);
    bt.on = 1;
    bt.sw_pending = 0;
    bt_list_build();
    ui.view = V_SETUP;
    ui.redraw = 1;
}

/* out of BACKTRACK (mode_leave): the song stops, the synth gives the buffer back */
static void bt_leave(void)
{
    uint32_t t = plat_ms();
    lib_release();
    bt_kit_apply(0);
    synth_band(0);
    vsynth_stop();
    while (plat_ms() - t < 20u)
        ;
    lib_room = sizeof dec;
    bt.on = 0;
}

/* ------------------------------------------------------------------ the band's versions --- */
/* a version of the band in half `half` of the song room: backtrack number `prog` in the style, each layer's level
 * `cplx` built (the choruses' tier above it comes from the file's energy) -> SONG_OK or SONG_E_*. The song playing is in
 * the other half and is not touched */
static int bt_prepare(uint32_t half, uint32_t prog, uint32_t style, uint32_t cplx)
{
    const struct store_entry *e = store_get(order[bt.list[prog]]);
    struct song *s = &bt_song[half];
    uint8_t *base = dec + half * BT_HALF;
    uint32_t l, at = 0, off[SONG_LAYERS], t0 = plat_ms();
    int r = song_open(s, store_file(e), e->len, base, BT_HALF);
    if (r == SONG_OK && song_crc32(store_file(e), e->len, 0) != e->crc)
        r = SONG_E_CRC;
    store_yield();
    if (r == SONG_OK && s->kind != SONG_KIND_BACKTRACK)
        r = SONG_E_LAYR;
    if (r == SONG_OK)
        r = song_restyle(s, BT_STYLE_OF[style], style == BT_WALTZ ? 3u : 4u);
    store_yield();
    for (l = 0; l < SONG_LAYERS && r == SONG_OK; l++) {
        off[l] = at;
        at += s->layer[l].need * 8u;
    }
    if (r == SONG_OK && at > BT_HALF)
        r = SONG_E_ROOM;
    for (l = 0; l < SONG_LAYERS && r == SONG_OK; l++)
        if (s->layer[l].variants) {
            r = song_build(s, l, cplx, base + off[l], s->layer[l].need * 8u);
            store_yield();
        }
    player_stats.bt_prepares++;
    player_stats.bt_prepare_ms = plat_ms() - t0;
    return r;
}

/* the first section of a role (from section i on), or nsect */
static uint32_t bt_section_of(const struct song *s, uint32_t role, uint32_t from)
{
    uint32_t i;
    for (i = from; i < s->nsect; i++)
        if (s->role[i] == role)
            return i;
    return s->nsect;
}

/* the structure's loop in song s: bars *a..*b (LOOP: the first verse; VERSE/CHORUS: it and the first chorus), 0: none
 * (FULL, or no verse) */
static int bt_loop_of(const struct song *s, uint32_t structure, uint32_t *a, uint32_t *b)
{
    uint32_t v = bt_section_of(s, ROLE_VERSE, 0), c = v < s->nsect ? bt_section_of(s, ROLE_CHORUS, v) : s->nsect;
    if (structure == BS_FULL || v >= s->nsect)
        return 0;
    *a = song_section_first(s, v);
    *b = structure == BS_VERSE_CHORUS && c < s->nsect ? song_section_first(s, c) + song_section_bars(s, c) - 1u :
         song_section_first(s, v) + song_section_bars(s, v) - 1u;
    return 1;
}

/* the bar of song ns that carries on from bar ob of song o: the same section and bar in it, if it has them; then the
 * structure's rules: inside the loop play on, else the loop's start; FULL without such a bar: the first verse */
static uint32_t bt_map_bar(const struct song *o, uint32_t ob, const struct song *ns, uint32_t structure, int *loop_on,
                           uint32_t *la, uint32_t *lb)
{
    uint32_t sec = song_bar_section(o, ob), k = ob - song_section_first(o, sec), bar = BT_NONE;
    if (sec < ns->nsect && k < song_section_bars(ns, sec))
        bar = song_section_first(ns, sec) + k;
    *loop_on = bt_loop_of(ns, structure, la, lb);
    if (*loop_on) {
        if (bar == BT_NONE || bar < *la || bar > *lb)
            bar = *la;
    } else if (bar == BT_NONE) {
        uint32_t v = bt_section_of(ns, ROLE_VERSE, 0);
        bar = v < ns->nsect ? song_section_first(ns, v) : 0;
    }
    return bar;
}

/* the structure's loop for the UI (ui.loop, a_bar, b_bar; the player has it from the swap or set_loop) */
static void bt_ui_loop(const struct song *s)
{
    uint32_t a, b;
    if (bt_loop_of(s, bt.structure, &a, &b)) {
        ui.loop = LOOP_BARS;
        ui.a_bar = a;
        ui.b_bar = b;
        set_from_bar(a);
    } else {
        ui.loop = LOOP_OFF;
        set_from_bar(first_bar());
        ui.from_tick = (int32_t)ui.s->start_tick;
    }
    ui.loop_step = 0;
    ui.sh_strip = -1;
    ui.sh_view = 0;
}

static void open_play(const struct song *s);   /* (ui.c) */

/* the choices (bt.prog .. bt.key) opened in half `half`, the play screen up, the player stopped */
static int bt_load(uint32_t half)
{
    const struct song *s;
    uint32_t l, a, b;
    int r;
    lib_release();
    r = bt_prepare(half, bt.prog, bt.style, bt.cplx);
    if (r) {
        app_say("BACKTRACK ERROR", song_error_name(r));
        ui.redraw = 1;
        return r;
    }
    bt.cur = half;
    s = &bt_song[half];
    open_play(s);
    for (l = 0; l < 3u; l++)
        player_variant(l, bt.cplx);
    player_stats.levels = bt.cplx | bt.cplx << 4 | bt.cplx << 8;
    player_tempo(bt.bpm);
    vsynth_tempo(bt.bpm < 60u ? 60u : bt.bpm > 200u ? 200u : bt.bpm);
    ui.key = bt_nearest(bt.key);                 /* (the nearest way: -5..+6, as the KEY mode) */
    player_transpose(ui.key);
    if (bt_loop_of(s, bt.structure, &a, &b)) {   /* LOOP / VERSE/CHORUS: the loop; FULL: none, from the intro */
        ui.a_bar = a;
        ui.b_bar = b;
        set_from_bar(a);
        set_loop(LOOP_BARS);
    }
    ui.mine = L_MELODY;                  /* (nothing is your part: the melody's scale views) */
    ui.pview = (int)view_ok(L_MELODY, PV_SCALE_KB);
    player_stats.view = (uint32_t)ui.pview;
    ui.view = V_PLAY;
    ui.redraw = 1;
    bt.w_prog = bt.prog;
    bt.w_style = bt.style;
    bt.w_structure = bt.structure;
    bt.w_cplx = bt.cplx;
    bt.w_key = bt.key;
    bt.sw_pending = 0;
    bt.swaps_seen = player_swaps();
    return 0;
}

/* PLAY on SETUP: the backtrack opened in the style, metre, key, tempo, complexity and structure chosen; the play screen */
static void bt_open(void)
{
    if (!bt.n || bt_load(0))
        return;
    player_start(start_point(), player_count_in_of());   /* (PLAY starts: the count-in as set) */
    app_say(BT_STRUCT_NAME[bt.structure], BT_STYLE_NAME[bt.style]);
}

/* ------------------------------------------------------------------- changes while it plays --- */
static uint32_t bt_pending(void)
{
    return (uint32_t)(bt.w_prog != bt.prog) | (uint32_t)(bt.w_style != bt.style) << 1 |
           (uint32_t)(bt.w_structure != bt.structure) << 2 | (uint32_t)(bt.w_cplx != bt.cplx) << 3 |
           (uint32_t)(bt.w_key != bt.key) << 4;
}
#define BT_PHRASE_CLASS 7u                 /* progression, style, structure: at the next phrase boundary */
#define BT_BAR_CLASS 24u                   /* complexity, key: at the next bar line */

/* what waits, for the top line (M10: spelled out): "NEXT: PROG 7", "NEXT: STYLE REGGAE +1" -- the first of progression, style,
 * structure, key, complexity, then "+n"; a value that does not fit `room` pixels is shortened, never the setting's word */
static void bt_next_item(char *d, uint32_t which, uint32_t shorter)
{
    const struct lib_item *it = bt_item(bt.w_prog);
    d[0] = 0;
    switch (which) {
    case 0:
        text_cat(d, "PROG ");
        num_cat(d, it ? it->number : 0);
        break;
    case 1:
        text_cat(d, "STYLE ");
        text_cat(d, shorter && bt.w_style == 8u ? "BOSSA" : shorter && bt.w_style == 2u ? "INDIE" : BT_STYLE_NAME[bt.w_style]);
        break;
    case 2:
        text_cat(d, "STRUCTURE ");
        text_cat(d, bt.w_structure == BS_VERSE_CHORUS ? (shorter == 1u ? "VERSE/CH." : shorter ? "V/C" : "VERSE/CHORUS") :
                                                        BT_STRUCT_NAME[bt.w_structure]);
        break;
    case 3:
        text_cat(d, "KEY ");
        bt_key_name(d, bt_item(bt.w_prog), bt.w_key, 0);
        break;
    default:
        text_cat(d, "COMPLEXITY ");
        text_cat(d, BT_CPLX_NAME[bt.w_cplx]);
    }
}

static void bt_next_text(char *d, int32_t room)
{
    static const uint8_t ORDER[5] = {0, 1, 2, 3, 4};      /* progression, style, structure, key, complexity */
    static const uint8_t BIT[5] = {1, 2, 4, 16, 8};         /* (bt_pending's bits) */
    uint32_t pend = bt_pending(), n = 0, first = 5, i, sh;
    char item[40], more[8] = "";
    for (i = 0; i < 5u; i++)
        if (pend & BIT[ORDER[i]]) {
            if (first == 5u)
                first = ORDER[i];
            n++;
        }
    d[0] = 0;
    if (!n)
        return;
    if (n > 1u) {
        text_cat(more, " +");
        num_cat(more, n - 1u);
    }
    for (sh = 0; sh < 3u; sh++) {
        bt_next_item(item, first, sh);
        d[0] = 0;
        text_cat(d, "NEXT: ");
        text_cat(d, item);
        text_cat(d, more);
        if (text_w(&FONT_XS, d) <= room)
            break;
    }
}

/* ------------------------------------------------------------------- the section indicator (M10, D057) --- */
/* the section's name in capitals: its role (INTRO, VERSE, CHORUS, BRIDGE, ENDING), else its name without a number */
static void bt_section_name(const struct song *s, uint32_t sec, char *d, uint32_t n)
{
    static const char *const ROLE[5] = {"INTRO", "VERSE", "CHORUS", "BRIDGE", "ENDING"};
    uint32_t i = 0;
    d[0] = 0;
    if (sec >= s->nsect)
        return;
    if (s->role[sec] < 5u) {
        text_cat(d, ROLE[s->role[sec]]);
        return;
    }
    {
        const char *nm = song_section_name(s, sec);
        for (; nm[i] && i + 1u < n; i++)
            d[i] = (char)(nm[i] >= 'a' && nm[i] <= 'z' ? nm[i] - 32 : nm[i]);
        while (i && (d[i - 1u] == ' ' || (d[i - 1u] >= '0' && d[i - 1u] <= '9')))
            i--;
        d[i] = 0;
    }
}

/* what the indicator shows: the section playing, or in the last bar before a different section starts the NEXT one,
 * blinking. *blink 1: blinking now. The phase (shown / hidden) follows the clock in 250 ms halves */
static uint32_t bt_indicator(const struct song *s, int st, int *blink)
{
    int32_t tk = player_tick(PL_LEAD_BEAT_MS);
    uint32_t bar, sec, next = 0xFFFFFFFFu, a, b;
    *blink = 0;
    if (tk < 0)
        tk = 0;
    bar = (uint32_t)tk / s->bar_ticks;
    if (bar >= s->bar_count)
        bar = s->bar_count - 1u;
    sec = song_bar_section(s, bar);
    if (st == PL_PLAYING) {
        uint32_t nb = bar + 1u;
        if (player_loop_of(&a, &b) && ui.loop == LOOP_BARS && nb * s->bar_ticks >= b)
            nb = a / s->bar_ticks;          /* the wrap: the loop's first bar carries on */
        if (nb < s->bar_count)
            next = song_bar_section(s, nb);
        if (next != 0xFFFFFFFFu && next != sec) {
            *blink = 1;
            return next;
        }
    }
    return sec;
}

/* the indicator's state for this frame (called from frame_play before the top line is drawn): the section it names, whether
 * it blinks, and the blink phase (on 250 ms, off 250 ms, from the moment the blink began, in the player's own time: the beat
 * light's base). Returns a number that changes when the top line has to be drawn again. */
static uint32_t bt_ind_update(int st)
{
    static int was_blink;
    static uint32_t was_to = 0xFFFFFFFFu;
    static int32_t t0;
    int blink;
    uint32_t sec = bt_indicator(ui.s, st, &blink), on = 1;
    int32_t tk = player_tick(PL_LEAD_BEAT_MS);
    if (blink && (!was_blink || was_to != sec)) {
        t0 = tk;
        player_stats.ind_blink_starts++;
        player_stats.ind_blink_tick = (uint32_t)tk;
        player_stats.ind_blink_to = sec;
    }
    if (blink) {
        uint32_t ms = (uint32_t)(tk - t0) * 60000u / (ui.s->ppq * player_tempo_of());
        on = (ms / 250u & 1u) ? 0u : 1u;
    }
    was_blink = blink;
    was_to = blink ? sec : 0xFFFFFFFFu;
    bt.ind_sec = sec;
    bt.ind_blink = (uint8_t)blink;
    bt.ind_on = (uint8_t)on;
    player_stats.ind_section = sec;
    player_stats.ind_blinking = (uint32_t)blink;
    player_stats.ind_on = on;
    {
        char nm[16];
        bt_section_name(ui.s, sec, nm, sizeof nm);
        vpop_text(player_stats.ind_name, nm, sizeof player_stats.ind_name);
    }
    return sec | (uint32_t)blink << 8 | on << 9;
}

/* the play screen's top lines (ui.c draw_top): the progression's number and name; the section indicator at the right end
 * of the line (M10; the MAJOR / MINOR badge is on SETUP and in the pop-up); then what waits (NEXT: ...), or the message, or
 * the style, structure and complexity. avail: the pixels before the bar label */
static void bt_top(int32_t avail, int st)
{
    const struct lib_item *it = bt_item(bt.prog);
    char t[64] = "";
    (void)st;
    num_cat(t, it ? it->number : 0);
    text_cat(t, " ");
    text_cat(t, it ? it->title : "?");
    cv_text(6, 1, &FONT_XS, t, C_GRAY);
    if (bt.ind_on) {
        char nm[16];
        int32_t w;
        bt_section_name(ui.s, bt.ind_sec, nm, sizeof nm);
        w = text_w(&FONT_XS, nm) + 8;
        cv_rect(avail - w, 1, w, 14, bt.ind_blink ? C_AMB : C_HI);
        cv_text(avail - w + 4, 2, &FONT_XS, nm, C_BLACK);
    }
    t[0] = 0;
    if (bt_pending()) {
        bt_next_text(t, avail - 6);
        cv_text(6, 16, &FONT_XS, t, C_HI);
    } else if (plat_ms() < ui.msg_until) {
        text_cat(t, ui.msg[0]);
        text_cat(t, " ");
        text_cat(t, ui.msg[1]);
        cv_text(6, 16, &FONT_XS, t, C_WHITE);
    } else {
        text_cat(t, BT_STYLE_NAME[bt.style]);
        text_cat(t, "  ");
        text_cat(t, BT_STRUCT_NAME[bt.structure]);
        text_cat(t, "  ");
        text_cat(t, BT_CPLX_NAME[bt.cplx]);
        cv_text(6, 16, &FONT_XS, t, C_AMB);
    }
}

/* the bar line b of the song (bar index) at or after bmin that a change of this class can use: a bar line (bar class), or
 * a phrase boundary (the bar in its section a multiple of 4, or the loop's wrap); BT_NONE if the song has none left */
static uint32_t bt_boundary(const struct song *s, uint32_t b, int phrase, int loop_on, uint32_t lb)
{
    uint32_t limit = loop_on ? lb + 1u : s->bar_count - 1u;
    for (; b <= limit; b++) {
        if (loop_on && b == lb + 1u)
            return b;                     /* the loop's wrap */
        if (!phrase || (b - song_section_first(s, song_bar_section(s, b))) % 4u == 0)
            return b;
    }
    return BT_NONE;
}

/* the changes taken: what plays now is what was prepared; the UI follows the song the player has (ui.s) */
static void bt_adopt(void)
{
    const struct song *s = player_song();
    bt.swaps_seen = player_swaps();
    if (bt.sw_pending) {
        bt.prog = bt.sw_prog;
        bt.style = bt.sw_style;
        bt.structure = bt.sw_structure;
        bt.cplx = bt.sw_cplx;
        bt.key = bt.sw_key;
        bt.sw_pending = 0;
    }
    bt.no_ms = bt.w_ms - 1u;                 /* (what waits may be due at the next boundary again) */
    bt.cur = s == &bt_song[1] ? 1u : 0u;
    ui.s = s;
    ui.key = bt_nearest(bt.key);
    bt_ui_loop(s);
    player_stats.levels = bt.cplx | bt.cplx << 4 | bt.cplx << 8;
    ui.sh_chord = ui.sh_next = ui.sh_section = -1;
    ui.sh_layers = -1;
    ui.sh_top_key = 0xFFFFFFFFu;
    ui.sh_status = 0xFFFFFFFFu;
    ui.redraw = 1;
}

/* a change asked for (the knobs still): the next version built in the other half and the swap asked of the player for
 * the boundary; the changes of the class that is due, together (the bar class rides along with the phrase class) */
static void bt_plan(void)
{
    const struct song *o = ui.s;
    uint32_t pend = bt_pending(), phrase = pend & BT_PHRASE_CLASS, barc = pend & BT_BAR_CLASS, bar_t = o->bar_ticks;
    uint32_t loop_on = ui.loop == LOOP_BARS, lb = ui.b_bar, la = ui.a_bar, bmin, bb, bp = BT_NONE, at_bar, use_phrase;
    uint32_t prog, style, structure, cplx, key, rebuild, ob, nb, nla = 0, nlb = 0;
    int32_t now_tick = player_tick(0), at, to;
    int nloop = 0;
    const struct song *ns;
    uint32_t bpm = player_tempo_of();
    bmin = ((uint32_t)now_tick + BT_PLAN_MS * o->ppq * bpm / 60000u + bar_t - 1u) / bar_t;
    bb = bt_boundary(o, bmin, 0, (int)loop_on, lb);
    if (phrase)
        bp = bt_boundary(o, bmin, 1, (int)loop_on, lb);
    use_phrase = phrase && bp != BT_NONE && (!barc || bp <= bb);
    if (!use_phrase && !(barc && bb != BT_NONE)) {   /* nothing can be done before the song ends */
        bt.no_ms = bt.w_ms;
        return;
    }
    prog = use_phrase ? bt.w_prog : bt.prog;
    style = use_phrase ? bt.w_style : bt.style;
    structure = use_phrase ? bt.w_structure : bt.structure;
    cplx = barc ? bt.w_cplx : bt.cplx;
    key = barc ? bt.w_key : bt.key;
    rebuild = prog != bt.prog || style != bt.style || cplx != bt.cplx;
    ns = o;
    if (rebuild) {
        uint32_t h = 1u - bt.cur;
        int r = bt_prepare(h, prog, style, cplx);
        if (r) {                          /* (not expected) the changes are dropped */
            app_say("BACKTRACK ERROR", song_error_name(r));
            bt.w_prog = bt.prog;
            bt.w_style = bt.style;
            bt.w_structure = bt.structure;
            bt.w_cplx = bt.cplx;
            bt.w_key = bt.key;
            return;
        }
        ns = &bt_song[h];
    }
    /* the line: at least BT_GUARD_MS from now, of the class due */
    now_tick = player_tick(0);
    bmin = ((uint32_t)now_tick + BT_GUARD_MS * o->ppq * bpm / 60000u + bar_t - 1u) / bar_t;
    at_bar = bt_boundary(o, bmin, (int)use_phrase, (int)loop_on, lb);
    if (at_bar == BT_NONE) {
        bt.no_ms = bt.w_ms;
        return;
    }
    if (loop_on && at_bar == lb + 1u) {   /* the wrap: the bar that carries on is the loop's first */
        at = (int32_t)((lb + 1u) * bar_t);
        ob = la;
    } else {
        at = (int32_t)(at_bar * bar_t);
        ob = at_bar;
    }
    nb = bt_map_bar(o, ob, ns, structure, &nloop, &nla, &nlb);
    to = (int32_t)(nb * ns->bar_ticks);
    bt.sw_prog = prog;
    bt.sw_style = style;
    bt.sw_structure = structure;
    bt.sw_cplx = cplx;
    bt.sw_key = key;
    bt.sw_ms = bt.w_ms;
    bt.sw_bar = at_bar;
    bt.sw_pending = 1;
    player_swap(ns, at, to, nloop ? LOOP_BARS : LOOP_OFF, nla * ns->bar_ticks, (nlb + 1u) * ns->bar_ticks,
                bt_nearest(key), cplx | cplx << 4 | cplx << 8);
}

/* the play screen's live changes, each pass of the main loop */
static void bt_live(void)
{
    uint32_t now = plat_ms();
    int st = player_state();
    static uint32_t late_seen;
    if (player_swaps() != bt.swaps_seen)                       /* (a swap the player made: also one asked to be withdrawn */
        bt_adopt();                                            /* too late: sw_pending 1 or 2 says what it brought) */
    if (bt.sw_pending == 2u && !player_swap_waiting())         /* the withdrawal has reached the player: nothing is coming */
        bt.sw_pending = 0;
    player_stats.bt_want = bt_pending();
    if (bt.sw_pending == 1u && player_swaps_late() != late_seen) {   /* the position had passed it: plan again */
        player_swap_cancel();
        bt.sw_pending = 2;
    }
    late_seen = player_swaps_late();
    if (!bt_pending()) {
        if (bt.sw_pending == 1u) {
            player_swap_cancel();
            bt.sw_pending = 2;
        }
        return;
    }
    if (bt.sw_pending == 1u && bt.w_ms != bt.sw_ms) {          /* turned again: what was prepared is withdrawn */
        player_swap_cancel();
        bt.sw_pending = 2;
    }
    if (bt.sw_pending || player_swap_waiting())                /* (a request the player still holds: wait for it) */
        return;
    if (now - bt.w_ms < BT_IDLE_MS || bt.no_ms == bt.w_ms)
        return;
    if (st == PL_STOPPED || st == PL_PAUSED) {                 /* not playing: at once (the band stops, as a new open) */
        const struct song *o = ui.s;
        uint32_t ob = 0;
        if (st == PL_PAUSED) {                                 /* M10 (M9-17 changed): a pause is kept, at the mapped bar */
            int32_t pt = player_tick(0);
            ob = pt > 0 ? (uint32_t)pt / o->bar_ticks : 0;
            if (ob >= o->bar_count)
                ob = o->bar_count - 1u;
        }
        bt.prog = bt.w_prog;
        bt.style = bt.w_style;
        bt.structure = bt.w_structure;
        bt.cplx = bt.w_cplx;
        bt.key = bt.w_key;
        bt_style_bpm();
        if (bt_load(1u - bt.cur) == 0 && st == PL_PAUSED) {
            const struct song *ns = &bt_song[bt.cur];
            uint32_t la = 0, lb = 0;
            int lo = 0;
            player_park((int32_t)(bt_map_bar(o, ob, ns, bt.structure, &lo, &la, &lb) * ns->bar_ticks));
        }
        return;
    }
    if (st == PL_PLAYING)
        bt_plan();
}

/* the tests' view (player.h, M8, M9) */
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
    player_stats.bt_cplx = bt.cplx;
    player_stats.bt_pending_bar = bt.sw_pending == 1u ? bt.sw_bar : BT_NONE;
    player_stats.synth_preset = bt.sound;
    if (bt.on && ui.view == V_PLAY && vpop.open)   /* (the screen is frozen under a pop-up: the indicator goes on) */
        (void)bt_ind_update(player_state());
    {
        char nt[48] = "";
        if (bt.on && ui.view == V_PLAY && bt_pending())
            bt_next_text(nt, 1000);
        vpop_text(player_stats.next_text, nt, sizeof player_stats.next_text);
    }
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

/* ------------------------------------------------------------------- the value pop-up (M10, D057) --- */
enum { VK_PROG, VK_STYLE, VK_CPLX, VK_SOUND, VK_KEY, VK_STRUCT, VK_TEMPO, VK_KIT };

/* knob k's pop-up: on SETUP the choice, on the play screen what waits (w_*) with its class, K4 and SELECT as they are */
static void bt_vp(uint32_t k)
{
    int play = ui.view != V_SETUP;
    uint32_t prog = play ? bt.w_prog : bt.prog, style = play ? bt.w_style : bt.style, cplx = play ? bt.w_cplx : bt.cplx,
             key = play ? bt.w_key : bt.key, structure = play ? bt.w_structure : bt.structure;
    const struct lib_item *it = bt_item(prog);
    char v[40] = "";
    const char *title = "", *sub = "";
    uint32_t badge = 0, pend = play ? bt_pending() : 0;
    switch (k) {
    case VK_PROG:
        title = "Progression";
        num_cat(v, it ? it->number : 0);
        text_cat(v, " ");
        text_cat(v, it ? it->title : "?");
        badge = it && it->key_mode == 1u ? 2u : 1u;
        sub = pend & 1u ? "at the next phrase" : "";
        break;
    case VK_STYLE:
        title = "Style";
        text_cat(v, BT_STYLE_NAME[style]);
        sub = pend & 2u ? "at the next phrase" : "";
        break;
    case VK_CPLX:
        title = "Song Complexity";
        text_cat(v, BT_CPLX_NAME[cplx]);
        sub = pend & 8u ? "at the next bar" : "";
        break;
    case VK_SOUND:
        title = "Melody Sound";
        text_cat(v, vsynth_stock(bt.sound)->name);
        break;
    case VK_KEY:
        title = "Key";
        bt_key_name(v, it, key, 1);
        sub = pend & 16u ? "at the next bar" : "";
        break;
    case VK_STRUCT:
        title = "Structure";
        text_cat(v, BT_STRUCT_NAME[structure]);
        sub = pend & 4u ? "at the next phrase" : "";
        break;
    case VK_TEMPO:
        title = "Tempo";
        num_cat(v, play ? player_tempo_of() : bt.bpm);
        text_cat(v, " BPM");
        break;
    default:
        title = "Band Kit";
        text_cat(v, BT_KIT_NAME[bt.kit % BT_KITS]);
    }
    vpop_show(title, v, sub, badge);
}

static void bt_vp_select(void) { bt_vp(VK_TEMPO); }

static int32_t bt_wrap(int32_t v, int32_t n) { return (v % n + n) % n; }

static void bt_setup_input(uint32_t down)
{
    int32_t d1 = plat_enc(EN_K1), d2 = plat_enc(EN_K2), d3 = plat_enc(EN_K3), d4 = plat_enc(EN_K4);
    int32_t dp = plat_enc(EN_PRESET), da = plat_enc(EN_ALGO), sel = plat_enc(EN_SELECT);
    if (bt.n && d1)
        bt.prog = (uint32_t)bt_wrap((int32_t)bt.prog + d1, (int32_t)bt.n);
    if (d2) {
        bt.style = (uint32_t)bt_wrap((int32_t)bt.style + d2, (int32_t)BT_STYLES);
        bt_style_bpm();
    }
    if (d3)
        bt.cplx = (uint32_t)clamp((int32_t)bt.cplx + d3, 1, 3);
    if (d4)
        bt_sound((uint32_t)bt_wrap((int32_t)bt.sound + d4, (int32_t)vsynth_presets()));
    if (dp)
        bt.key = (uint32_t)bt_wrap((int32_t)bt.key + dp, 12);
    if (da)
        bt.structure = (uint32_t)bt_wrap((int32_t)bt.structure + da, (int32_t)BS_N);
    if (sel) {
        bt.bpm = (uint32_t)clamp((int32_t)bt.bpm + sel, (int32_t)PL_BPM_MIN, (int32_t)PL_BPM_MAX);
        bt.bpm_turned = 1;
    }
    if (d1 && bt.n)
        bt_vp(VK_PROG);
    if (d2)
        bt_vp(VK_STYLE);
    if (d3)
        bt_vp(VK_CPLX);
    if (d4)
        bt_vp(VK_SOUND);
    if (dp)
        bt_vp(VK_KEY);
    if (da)
        bt_vp(VK_STRUCT);
    if (sel)
        bt_vp(VK_TEMPO);
    if (down >> B_SEQ & 1u) {              /* M10: the next band kit */
        bt.kit = (bt.kit + 1u) % BT_KITS;
        bt_kit_apply(1);
        bt_vp(VK_KIT);
    }
    if (down >> B_PLAY & 1u)
        bt_open();
    bt_stats();
}

/* the play screen's own (before input_play, which does not see these): the same knobs as SETUP. K4 and (in input_play)
 * SELECT act at once, the others wait for their boundary (bt_live). SEQ and SAVE have no job here: the pop-up */
static void bt_play_input(uint32_t down)
{
    int32_t d1 = plat_enc(EN_K1), d2 = plat_enc(EN_K2), d3 = plat_enc(EN_K3), d4 = plat_enc(EN_K4);
    int32_t dp = plat_enc(EN_PRESET), da = plat_enc(EN_ALGO);
    uint32_t now = plat_ms();
    if (bt.n && d1)
        bt.w_prog = (uint32_t)bt_wrap((int32_t)bt.w_prog + d1, (int32_t)bt.n);
    if (d2)
        bt.w_style = (uint32_t)bt_wrap((int32_t)bt.w_style + d2, (int32_t)BT_STYLES);
    if (d3)
        bt.w_cplx = (uint32_t)clamp((int32_t)bt.w_cplx + d3, 1, 3);
    if (dp)
        bt.w_key = (uint32_t)bt_wrap((int32_t)bt.w_key + dp, 12);
    if (da)
        bt.w_structure = (uint32_t)bt_wrap((int32_t)bt.w_structure + da, (int32_t)BS_N);
    if (d1 || d2 || d3 || dp || da) {
        bt.w_ms = now;
        ui.sh_top_key = 0xFFFFFFFFu;
    }
    if (d1 && bt.n)
        bt_vp(VK_PROG);
    if (d2)
        bt_vp(VK_STYLE);
    if (d3)
        bt_vp(VK_CPLX);
    if (dp)
        bt_vp(VK_KEY);
    if (da)
        bt_vp(VK_STRUCT);
    if (d4) {
        bt_sound((uint32_t)bt_wrap((int32_t)bt.sound + d4, (int32_t)vsynth_presets()));
        bt_vp(VK_SOUND);
        bt.sound_until = now + 1500u;
    }
    if (down >> B_LFO & 1u)
        app_say("MELODY", "you play it");
    bt_live();
    bt_stats();
}
