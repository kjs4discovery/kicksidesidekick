/* Licence: TBD (see D017) */
/* The compiled song file's parser (song.h). Bytes are read one at a time (little-endian), so the file may sit at
 * any address; every multiplication that sizes a read is bounded first (counts <= 65535, records <= 24 bytes). */
#include "song.h"

#define HEADER 32u
#define DIRENT 12u
#define MAX_CHUNKS 24u
#define META_SIZE 76u
#define SECT_REC 24u
#define CHRD_REC 12u
#define CEVT_REC 16u
#define BARS_REC 4u
#define LAYR_REC 8u
#define EVNT_REC 8u
#define MAX_BARS 999u
#define MAX_SECT 32u
#define MAX_CHRD 64u
#define MAX_CEVT 2048u
#define MAX_EVENTS 65535u

static uint32_t u16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t u32(const uint8_t *p) { return u16(p) | u16(p + 2) << 16; }
static uint32_t fourcc(const char *c)
{
    return (uint32_t)(uint8_t)c[0] | (uint32_t)(uint8_t)c[1] << 8 | (uint32_t)(uint8_t)c[2] << 16 |
           (uint32_t)(uint8_t)c[3] << 24;
}

uint32_t song_crc32(const uint8_t *d, uint32_t n, uint32_t crc)
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    uint32_t i;
    crc = ~crc;
    for (i = 0; i < n; i++) {
        crc ^= d[i];
        crc = (crc >> 4) ^ T[crc & 15u];
        crc = (crc >> 4) ^ T[crc & 15u];
    }
    return ~crc;
}

/* a fixed text field: a NUL inside it, printable ASCII before the NUL */
static int text_ok(const uint8_t *p, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        if (p[i] == 0)
            return 1;
        if (p[i] < 32u || p[i] > 126u)
            return 0;
    }
    return 0;
}

static int header(struct song *s, const uint8_t *d, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < sizeof *s; i++)
        ((uint8_t *)s)[i] = 0;
    if (!d || n < HEADER)
        return SONG_E_SHORT;
    if (d[0] != 'F' || d[1] != 'M' || d[2] != '1' || d[3] != 'S')
        return SONG_E_MAGIC;
    if (d[4] != 1u)
        return SONG_E_VERSION;
    s->size = u32(d + 8);
    if (s->size != n || s->size > SONG_MAX_SIZE)
        return SONG_E_SIZE;
    s->data = d;
    s->ppq = u16(d + 16);
    s->beats_per_bar = d[18];
    s->tempo_us = u32(d + 20);
    s->end_tick = u32(d + 24);
    s->bar_count = u16(d + 28);
    s->pickup_beats = d[30];
    s->swing = d[31] & 1u;
    if (s->ppq < 24u || s->ppq > 960u || s->beats_per_bar < 2u || s->beats_per_bar > 7u || d[19] != 4u ||
        s->tempo_us < 240000u || s->tempo_us > 2000000u || s->bar_count < 1u || s->bar_count > MAX_BARS ||
        s->pickup_beats >= s->beats_per_bar || (d[31] & ~1u))
        return SONG_E_HEADER;
    s->bar_ticks = s->ppq * s->beats_per_bar;                 /* <= 6720 */
    if (s->end_tick != s->bar_count * s->bar_ticks)           /* <= 6.7 M: no overflow */
        return SONG_E_HEADER;
    s->start_tick = s->pickup_beats ? (s->beats_per_bar - s->pickup_beats) * s->ppq : 0;
    s->first_bar_number = s->pickup_beats ? 0 : 1;
    return SONG_OK;
}

/* the directory: chunk `id`'s place, or 0 if absent; the whole directory checked once by dir_ok */
static int dir_ok(const struct song *s)
{
    uint32_t i, k, nchunks = u16(s->data + 6), end;
    if (nchunks < 1u || nchunks > MAX_CHUNKS || HEADER + nchunks * DIRENT > s->size)
        return 0;
    end = HEADER + nchunks * DIRENT;
    for (i = 0; i < nchunks; i++) {
        const uint8_t *e = s->data + HEADER + i * DIRENT;
        uint32_t off = u32(e + 4), len = u32(e + 8);
        if ((off & 3u) || off < end || len > s->size || off > s->size - len)
            return 0;
        for (k = 0; k < i; k++)
            if (u32(s->data + HEADER + k * DIRENT) == u32(e))
                return 0;                 /* twice */
        end = off + len;
    }
    return 1;
}

static const uint8_t *chunk(const struct song *s, uint32_t id, uint32_t *len)
{
    uint32_t i, nchunks = u16(s->data + 6);
    for (i = 0; i < nchunks; i++) {
        const uint8_t *e = s->data + HEADER + i * DIRENT;
        if (u32(e) == id) {
            *len = u32(e + 8);
            return s->data + u32(e + 4);
        }
    }
    *len = 0;
    return 0;
}

/* a chunk of a u16 count (1..max), 2 more header bytes, and count records of rec bytes */
static const uint8_t *table(const struct song *s, const char *id, uint32_t rec, uint32_t max, uint32_t *count)
{
    uint32_t len;
    const uint8_t *c = chunk(s, fourcc(id), &len);
    if (!c || len < 4u)
        return 0;
    *count = u16(c);
    if (*count < 1u || *count > max || len != 4u + *count * rec)
        return 0;
    return c + 4;
}

static int meta(struct song *s)
{
    uint32_t len;
    const uint8_t *m = chunk(s, fourcc("META"), &len);
    if (!m)
        return SONG_E_MISSING;
    if (len != META_SIZE || m[0] > 11u || m[1] > 6u || !text_ok(m + 4, 24) || !text_ok(m + 28, 32) ||
        !text_ok(m + 60, 16))
        return SONG_E_META;
    s->key_root = m[0];
    s->key_mode = m[1];
    s->slug = (const char *)(m + 4);
    s->title = (const char *)(m + 28);
    s->key = (const char *)(m + 60);
    return SONG_OK;
}

int song_peek(struct song *s, const uint8_t *data, uint32_t size)
{
    int r = header(s, data, size);
    if (r)
        return r;
    if (!dir_ok(s))
        return SONG_E_DIR;
    return meta(s);
}

static int sections(struct song *s)
{
    uint32_t i, expect = s->pickup_beats ? 1u : 0u;
    s->sect = table(s, "SECT", SECT_REC, MAX_SECT, &s->nsect);
    if (!s->sect)
        return SONG_E_SECT;
    for (i = 0; i < s->nsect; i++) {
        const uint8_t *r = s->sect + i * SECT_REC;
        uint32_t first = u16(r), count = u16(r + 2), lead = u16(r + 4);
        if (first != expect || count == 0 || first + count > s->bar_count || lead >= s->bar_ticks ||
            !text_ok(r + 8, 16))
            return SONG_E_SECT;
        expect = first + count;
    }
    return expect == s->bar_count ? SONG_OK : SONG_E_SECT;
}

static int chords(struct song *s)
{
    uint32_t i;
    s->chrd = table(s, "CHRD", CHRD_REC, MAX_CHRD, &s->nchrd);
    if (!s->chrd)
        return SONG_E_CHRD;
    for (i = 0; i < s->nchrd; i++) {
        const uint8_t *r = s->chrd + i * CHRD_REC;
        uint32_t root = r[8], bass = r[9], mask = u16(r + 10);
        if (!text_ok(r, 8) || (mask >> 12) || (bass != SONG_NO_CHORD && bass > 11u))
            return SONG_E_CHRD;
        if (root == SONG_NO_CHORD ? (bass != SONG_NO_CHORD || mask) : (root > 11u || !mask))
            return SONG_E_CHRD;
    }
    return SONG_OK;
}

static int chord_events(struct song *s)
{
    uint32_t len, i, k, last = 0;
    const uint8_t *c = chunk(s, fourcc("CEVT"), &len);
    if (!c || len < 4u)
        return SONG_E_CEVT;
    s->ncevt = u16(c);
    if (s->ncevt < 1u || s->ncevt > MAX_CEVT || c[2] != 2u || c[3] != SONG_VOICING_NOTES ||
        len != 4u + s->ncevt * CEVT_REC)
        return SONG_E_CEVT;
    s->cevt = c + 4;
    for (i = 0; i < s->ncevt; i++) {
        const uint8_t *r = s->cevt + i * CEVT_REC;
        uint32_t tick = u32(r), flags = r[5];
        if ((i == 0) != (tick == 0) || (i && tick <= last) || tick >= s->end_tick || r[4] >= s->nchrd ||
            (flags & ~1u) || (flags & 1u) != (tick % s->bar_ticks == 0))
            return SONG_E_CEVT;
        for (k = 6; k < CEVT_REC; k++)
            if (r[k] > 127u)
                return SONG_E_CEVT;
        last = tick;
    }
    return SONG_OK;
}

static int bars(struct song *s)
{
    uint32_t n, b;
    s->bars = table(s, "BARS", BARS_REC, MAX_BARS, &n);
    if (!s->bars || n != s->bar_count)
        return SONG_E_BARS;
    for (b = 0; b < n; b++) {
        const uint8_t *r = s->bars + b * BARS_REC;
        uint32_t ev = u16(r), t0 = b * s->bar_ticks, et;
        if (ev >= s->ncevt || r[2] >= s->nsect || (r[3] & ~3u))
            return SONG_E_BARS;
        et = song_cevt_tick(s, ev);
        if (et > t0 || (ev + 1u < s->ncevt && song_cevt_tick(s, ev + 1u) <= t0) || ((r[3] & 1u) != 0) != (et == t0))
            return SONG_E_BARS;
    }
    return SONG_OK;
}

static int events(const struct song *s, const uint8_t *c, uint32_t len, struct song_stream *st)
{
    uint32_t i, last = 0;
    if (!c || len < 4u)
        return SONG_E_EVENTS;
    st->count = u32(c);
    if (st->count > MAX_EVENTS || len != 4u + st->count * EVNT_REC)
        return SONG_E_EVENTS;
    st->ev = c + 4;
    for (i = 0; i < st->count; i++) {
        const uint8_t *e = st->ev + i * EVNT_REC;
        uint32_t tick = u32(e), status = e[4];
        if (tick < last || tick > s->end_tick || e[5] > 127u || e[6] > 127u || e[7] ||
            (status != 0x80u && status != 0x90u && status != 0xB0u && status != 0xE0u) ||
            (status == 0x90u && (e[6] == 0 || tick >= s->end_tick)))
            return SONG_E_EVENTS;
        last = tick;
    }
    return SONG_OK;
}

static int layers(struct song *s)
{
    static const char LETTER[SONG_LAYERS] = {'D', 'B', 'H', 'M'};
    uint32_t len, l, v, any = 0;
    const uint8_t *c = chunk(s, fourcc("LAYR"), &len);
    if (!c || len != 4u + SONG_LAYERS * LAYR_REC || c[0] != SONG_LAYERS)
        return SONG_E_LAYR;
    for (l = 0; l < SONG_LAYERS; l++) {
        const uint8_t *r = c + 4 + l * LAYR_REC;
        struct song_layer *ly = &s->layer[l];
        ly->channel = r[0];
        ly->program = r[1];
        ly->variants = r[2];
        ly->volume = r[3];
        ly->pan = r[4];
        if (ly->variants == 0) {
            if (ly->channel != 0xFFu)
                return SONG_E_LAYR;
            continue;
        }
        if (ly->channel > 15u || ly->variants > SONG_MAX_VARIANTS || ly->volume > 127u || ly->pan > 127u || r[5] ||
            (ly->program > 127u && ly->program != 0xFFu))
            return SONG_E_LAYR;
        for (v = 0; v < ly->variants; v++) {
            char id[4] = {'E', 'V', LETTER[l], (char)('1' + v)};
            uint32_t elen;
            const uint8_t *e = chunk(s, fourcc(id), &elen);
            int r2 = events(s, e, elen, &ly->var[v]);
            if (r2)
                return r2;
        }
        any = 1;
    }
    return any ? SONG_OK : SONG_E_LAYR;
}

int song_open(struct song *s, const uint8_t *data, uint32_t size)
{
    int r;
    static const uint8_t zero[4] = {0, 0, 0, 0};
    uint32_t crc;
    if ((r = header(s, data, size)) != SONG_OK)
        return r;
    crc = song_crc32(data, 12, 0);       /* the whole file with the CRC field as zero */
    crc = song_crc32(zero, 4, crc);
    if (song_crc32(data + 16, size - 16u, crc) != u32(data + 12))
        return SONG_E_CRC;
    if (!dir_ok(s))
        return SONG_E_DIR;
    if ((r = meta(s)) || (r = sections(s)) || (r = chords(s)) || (r = chord_events(s)) || (r = bars(s)) ||
        (r = layers(s))) {
        s->opened = 0;
        return r;
    }
    s->opened = 1;
    return SONG_OK;
}

const char *song_error_name(int err)
{
    static const char *const N[] = {"OK", "SHORT", "MAGIC", "VERSION", "SIZE", "CRC", "HEADER", "DIRECTORY",
                                    "MISSING", "META", "SECTIONS", "CHORDS", "CHANGES", "BARS", "LAYERS", "EVENTS"};
    return err <= 0 && err >= SONG_E_EVENTS ? N[-err] : "?";
}

/* ------------------------------------------------------------------------ accessors --- */
void song_event_get(const struct song_stream *st, uint32_t i, struct song_event *e)
{
    const uint8_t *p = st->ev + i * EVNT_REC;
    e->tick = u32(p);
    e->status = p[4];
    e->d1 = p[5];
    e->d2 = p[6];
}

uint32_t song_seek(const struct song_stream *st, uint32_t tick)
{
    uint32_t lo = 0, hi = st->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        if (u32(st->ev + mid * EVNT_REC) < tick)
            lo = mid + 1u;
        else
            hi = mid;
    }
    return lo;
}

uint32_t song_cevt_tick(const struct song *s, uint32_t i) { return u32(s->cevt + i * CEVT_REC); }

uint32_t song_chord_at(const struct song *s, uint32_t tick)
{
    uint32_t lo = 0, hi = s->ncevt;      /* the last event with tick <= the given one; event 0 is at tick 0 */
    while (hi - lo > 1u) {
        uint32_t mid = lo + (hi - lo) / 2u;
        if (song_cevt_tick(s, mid) <= tick)
            lo = mid;
        else
            hi = mid;
    }
    return lo;
}

const char *song_cevt_symbol(const struct song *s, uint32_t i)
{
    return (const char *)(s->chrd + s->cevt[i * CEVT_REC + 4] * CHRD_REC);
}

const uint8_t *song_cevt_voicing(const struct song *s, uint32_t i, uint32_t level)
{
    return s->cevt + i * CEVT_REC + (level >= 2u ? 11u : 6u);
}

uint32_t song_bar_chord(const struct song *s, uint32_t bar) { return u16(s->bars + bar * BARS_REC); }
uint32_t song_bar_section(const struct song *s, uint32_t bar) { return s->bars[bar * BARS_REC + 2]; }
uint32_t song_bar_flags(const struct song *s, uint32_t bar) { return s->bars[bar * BARS_REC + 3]; }
const char *song_section_name(const struct song *s, uint32_t i) { return (const char *)(s->sect + i * SECT_REC + 8); }
uint32_t song_section_first(const struct song *s, uint32_t i) { return u16(s->sect + i * SECT_REC); }
uint32_t song_section_bars(const struct song *s, uint32_t i) { return u16(s->sect + i * SECT_REC + 2); }
uint32_t song_section_lead(const struct song *s, uint32_t i) { return u16(s->sect + i * SECT_REC + 4); }
