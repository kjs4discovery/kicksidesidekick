/* Licence: TBD (see D017) */
/* M8 Part 5: BACKTRACK's melody views and key lights. Included by ui.c before frame_play (it needs struct pinfo). */
/* ------------------------------------------------- Part 5 (D049): what can I play now --- */
/* The scale (the setting "Scale guide": PENTATONIC, FULL SCALE, CHORD TONES): pentatonic by default (major pentatonic
 * for a major progression, minor pentatonic for a minor one, the blues scale for the blues forms); the full scale:
 * major, natural minor, mixolydian (a major-key blues). The current chord's tones bright, changing at each chord; the
 * next chord's tones dim in its last beat. On the screen (SCALE KEYBOARD, SCALE FRETBOARD) and the FM-1's key lights
 * (chord tones bright, scale notes at the middle level). */
enum { SG_PENT, SG_FULL, SG_CHORD };
static uint32_t bt_tonic(void)            /* the tonic's pitch class, sounding (the key moved) */
{
    return ((ui.s && ui.s->key_mode == 1u ? 9u : 0u) + (uint32_t)(ui.key + 12)) % 12u;
}

static uint32_t rot(uint32_t mask, uint32_t by)   /* pitch classes moved up by semitones */
{
    by %= 12u;
    return ((mask << by) | (mask >> (12u - by))) & 0xFFFu;
}

static const char *bt_scale_name(void)
{
    uint32_t minor = ui.s && ui.s->key_mode == 1u, blues = ui.s && ui.s->scale == SONG_SCALE_BLUES;
    switch (SV(S_SCALE)) {
    case SG_FULL:
        return blues ? (minor ? "NATURAL MINOR" : "MIXOLYDIAN") : minor ? "NATURAL MINOR" : "MAJOR SCALE";
    case SG_CHORD:
        return "CHORD TONES";
    default:
        return blues ? "BLUES SCALE" : minor ? "MINOR PENTATONIC" : "MAJOR PENTATONIC";
    }
}

static uint32_t bt_scale_mask(void)       /* bit pc: in the scale shown (CHORD TONES: none) */
{
    /* intervals from the tonic, bit k = k semitones */
    static const uint16_t PENT_MAJ = 0x295, PENT_MIN = 0x4A9, BLUES = 0x4E9, MAJOR = 0xAB5, MINOR = 0x5AD,
                          MIXO = 0x6B5;
    uint32_t minor = ui.s && ui.s->key_mode == 1u, blues = ui.s && ui.s->scale == SONG_SCALE_BLUES, m;
    switch (SV(S_SCALE)) {
    case SG_FULL:
        m = minor ? MINOR : blues ? MIXO : MAJOR;
        break;
    case SG_CHORD:
        return 0;
    default:
        m = blues ? BLUES : minor ? PENT_MIN : PENT_MAJ;
    }
    return rot(m, bt_tonic());
}

static uint32_t chord_pcs(int32_t k)      /* chord event k's pitch classes, sounding; -1: none */
{
    return k < 0 ? 0 : rot(song_cevt_mask(ui.s, (uint32_t)k), (uint32_t)(ui.key + 12));
}

/* the keys (27, with the keys' octave) whose notes are in mask */
static uint32_t keys_of(uint32_t mask)
{
    uint32_t k, out = 0;
    for (k = 0; k < NKEYS; k++)
        if (mask >> ((uint32_t)(KEY_LOW + (int32_t)k + 12 * ui.oct + 120) % 12u) & 1u)
            out |= 1u << k;
    return out;
}

static void disc(int32_t cx, int32_t cy, int32_t r, uint16_t c, int ring)
{
    int32_t x, y;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++) {
            int32_t d = x * x + y * y;
            if (d <= r * r + r && (!ring || d >= (r - 1) * (r - 1)))
                cv_pset(cx + x, cy + y, c);
        }
}

/* the view's top: the key and scale; the chord now (big) and the next */
static void scale_head(const struct pinfo *pi, int last_beat)
{
    char t[40] = "KEY ";
    text_cat(t, names()[bt_tonic()]);
    if (ui.s->key_mode == 1u)
        text_cat(t, "m");
    text_cat(t, "  ");
    text_cat(t, bt_scale_name());
    cv_text(6, 0, &FONT_XS, t, C_GRAY);
    if (pi->big[0])
        cv_text(6, 16, &FONT_L, pi->big, C_HI);
    if (pi->next[0]) {
        t[0] = 0;
        text_cat(t, "next ");
        text_cat(t, pi->next);
        cv_text(234 - text_w(&FONT_B, t), 24, &FONT_B, t, last_beat ? C_WHITE : C_DIM);
    }
}

#define SC_GRAYKEY RGB(196, 196, 204)
/* SCALE KEYBOARD: the FM-1's 27 keys (F3..G5): the scale's notes marked, the chord's tones bright, the next chord's
 * tones (in the last beat) a ring */
static void draw_scale_kb(const struct pinfo *pi, uint32_t scale, uint32_t tones, uint32_t nx)
{
    uint32_t k, wi = 0;
    int32_t y0 = 58, wh = 84, bh = 50, ww = 15;
    cv_begin(240, H_VIEW, C_BLACK);
    scale_head(pi, nx != 0);
    for (k = 0; k < NKEYS; k++) {        /* the white keys first */
        uint32_t pc = (uint32_t)(KEY_LOW + (int32_t)k) % 12u;
        int32_t x;
        uint16_t c;
        if (is_black(pc + 60u))
            continue;
        x = (int32_t)wi * ww;
        c = tones >> pc & 1u ? C_HI : scale >> pc & 1u ? DARK_AMB : SC_GRAYKEY;
        cv_rect(x, y0, ww - 1, wh, c);
        if (nx >> pc & 1u)
            disc(x + ww / 2, y0 + wh - 10, 4, C_BLACK, 1);
        wi++;
    }
    for (k = 0, wi = 0; k < NKEYS; k++) {   /* the black keys over them */
        uint32_t pc = (uint32_t)(KEY_LOW + (int32_t)k) % 12u;
        int32_t x;
        uint16_t c;
        if (!is_black(pc + 60u)) {
            wi++;
            continue;
        }
        x = (int32_t)wi * ww - 5;
        c = tones >> pc & 1u ? C_HI : scale >> pc & 1u ? RGB(110, 64, 14) : RGB(16, 16, 20);
        cv_rect(x - 1, y0, 11, bh + 1, C_BLACK);
        cv_rect(x, y0, 9, bh, c);
        if (nx >> pc & 1u)
            disc(x + 4, y0 + bh - 9, 3, C_WHITE, 1);
    }
    cv_blit(0, Y_CHORD);
}

/* SCALE FRETBOARD: the guitar (or the ukulele: the setting), frets 0..12, the same marking */
static void draw_scale_fb(const struct pinfo *pi, uint32_t scale, uint32_t tones, uint32_t nx)
{
    static const uint8_t GTR[6] = {64, 59, 55, 50, 45, 40}, UKE[4] = {69, 64, 60, 67};   /* high string on top */
    const uint8_t *str = SV(S_UKE) ? UKE : GTR;
    uint32_t ns = SV(S_UKE) ? 4u : 6u, s, f;
    int32_t top = 66, sp = SV(S_UKE) ? 20 : 14, nut = 22, fw = 17;
    cv_begin(240, H_VIEW, C_BLACK);
    scale_head(pi, nx != 0);
    cv_text(234 - text_w(&FONT_XS, SV(S_UKE) ? "UKULELE" : "GUITAR"), 0, &FONT_XS, SV(S_UKE) ? "UKULELE" : "GUITAR",
            C_DIM);
    for (f = 0; f <= 12u; f++)           /* the frets, the nut thicker; the markers 3 5 7 9 12 */
        cv_rect(nut + (int32_t)f * fw, top - 4, f ? 1 : 3, (int32_t)(ns - 1u) * sp + 9, f ? C_DIM : C_GRAY);
    for (f = 3; f <= 12u; f += f == 9u ? 3u : 2u)
        disc(nut + (int32_t)f * fw - fw / 2, top + (int32_t)(ns - 1u) * sp + 12, 2, C_DIM, 0);
    for (s = 0; s < ns; s++)
        cv_rect(nut, top + (int32_t)s * sp, 12 * fw, 1, C_GRAY);
    for (s = 0; s < ns; s++)
        for (f = 0; f <= 12u; f++) {
            uint32_t pc = (str[s] + f) % 12u;
            int32_t x = f ? nut + (int32_t)f * fw - fw / 2 : nut - 11, y = top + (int32_t)s * sp;
            if (tones >> pc & 1u)
                disc(x, y, 5, C_HI, 0);
            else if (scale >> pc & 1u)
                disc(x, y, 3, DARK_AMB, 0);
            if (nx >> pc & 1u)
                disc(x, y, 6, C_WHITE, 1);
        }
    cv_blit(0, Y_CHORD);
}
