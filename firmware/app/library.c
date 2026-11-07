/* Licence: TBD (see D017) */
/* The song library (M2 part 1): the songs in the flash's song store (store.h), the one song open for playing, and
 * the laptop's commands over USB (tools/fm1song send / list / delete). Included by ui.c (fm1-base's unity build).
 *
 * Nothing here runs at boot: the store is scanned when the song list is first drawn (after the splash), each song
 * is checked in full (song_open) when it is opened, and a bad one shows SONG ERROR while the rest work.
 *
 * USB (fm1-base APP_USB_CMD; the update's SysEx framing, F0 pack7(00 59 cmd len24 body ~sum) F7). The laptop sends:
 *   0x70 LIST                    -> 0x71 ENTRY per song {index, count, status, sectors, len u24, slug[23], blocks
 *                                   u16}, then 0x72 END {count, free sectors, longest free run, sectors, minor,
 *                                   result, free blocks u16, blocks u16, major} (M5: the fields after result; major 2:
 *                                   it reads 2.0 too)
 *   0x73 SEND {len u32, crc u32[, slug[24]]} -> the FM-1 makes room (store_begin), pulls the file with the update's
 *                                   read requests (plat_host_read) straight into the flash (M5; M4: into RAM, 64 KiB
 *                                   at most), checks its CRC and the whole song (song_open) in place, then commits it
 *                                   (store_commit). The slug (M5) lets the song's older copy make way if it must
 *   0x74 DELETE {slug}
 * and gets 0x7F RESULT {cmd, result (SR_*), detail (a SONG_E_* or STORE_E_* number), songs, free sectors, free blocks
 * u16 (M5)}.
 *
 * M5 (format 2.0, D029 A): a 2.0 song plays one level of each layer at a time. dec holds each layer's level, in a
 * region of its own sized for its largest level (song_layer need); lib_level builds another level into it, with the
 * player holding that layer meanwhile (player_hold). 1.x songs decode into dec as before, every level at once.
 *
 * A send or delete stops the song playing and goes back to the list (the file being played may be rewritten). */
#include "store.h"

#define SC_LIST 0x70u
#define SC_ENTRY 0x71u
#define SC_END 0x72u
#define SC_SEND 0x73u
#define SC_DELETE 0x74u
#define SC_RESULT 0x7Fu
enum { SR_OK, SR_NO_FLASH, SR_SIZE, SR_TRANSFER, SR_CRC, SR_INVALID, SR_STORE, SR_NOT_FOUND, SR_BAD };

#define LIB_DEC_BYTES (SONG_BUILD_BYTES > SONG_DECODE_BYTES ? SONG_BUILD_BYTES : SONG_DECODE_BYTES)
static uint8_t piece[512] __attribute__((aligned(4)));                          /* a song being received: a piece */
static uint8_t dec[LIB_DEC_BYTES] PLAT_POOL __attribute__((aligned(4)));        /* the open song's events */
static uint32_t region[SONG_LAYERS];     /* 2.0: each layer's level in dec, from here */
/* a song of the list: what the list shows (M4 kept the whole struct song: too big for 200 songs) */
struct lib_item {
    const char *title, *key, *category;  /* in the file, in the flash */
    uint32_t bpm, beats_per_bar;
};
static struct lib_item peeked[STORE_MAX_SONGS] PLAT_POOL;
static int8_t peek_err[STORE_MAX_SONGS];
static int8_t open_err[STORE_MAX_SONGS];  /* found when opened (the full check): the list keeps showing it */
static uint8_t order[STORE_MAX_SONGS];   /* the list's order: by title */
static uint32_t nsongs;
static int lib_state;                    /* 0 not scanned, 1 scanned, < 0 STORE_E_NO_FLASH */
static struct song current;              /* the song open for playing (its events in dec) */
static int32_t current_i = -1;
static uint32_t lib_changes;             /* counts sends and deletes: the UI goes back to the list */

/* the store's flash: fm1-base's data area (store.c checks the song area's bounds before each erase and write) */
const uint8_t *store_map(uint32_t off) { return plat_data(off); }
int store_erase(uint32_t off) { return plat_data_erase(off); }
int store_prog(uint32_t off, const void *src, uint32_t n) { return plat_data_write(off, src, n); }

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int title_before(const struct lib_item *a, const struct lib_item *b)
{
    const char *p = a->title, *q = b->title;
    while (*p && lower(*p) == lower(*q))
        p++, q++;
    return lower(*p) < lower(*q);
}

static void lib_scan(void)
{
    uint32_t i, k;
    int r = store_scan();
    current_i = -1;
    nsongs = 0;
    if (r < 0) {
        lib_state = r;
        return;
    }
    lib_state = 1;
    nsongs = store_count();
    for (i = 0; i < nsongs; i++) {
        const struct store_entry *e = store_get(i);
        struct song s;
        peek_err[i] = (int8_t)song_peek(&s, store_file(e), e->len);
        peeked[i].title = peek_err[i] ? "" : s.title;
        peeked[i].key = peek_err[i] ? "" : s.key;
        peeked[i].category = peek_err[i] ? "" : s.category;
        peeked[i].bpm = peek_err[i] ? 0 : player_song_bpm(&s);
        peeked[i].beats_per_bar = s.beats_per_bar;
        open_err[i] = 0;
        order[i] = (uint8_t)i;
    }
    for (i = 1; i < nsongs; i++)          /* insertion sort by title; songs that do not peek last */
        for (k = i; k > 0; k--) {
            uint8_t a = order[k - 1u], b = order[k];
            int swap = peek_err[a] ? !peek_err[b] : !peek_err[b] && title_before(&peeked[b], &peeked[a]);
            if (!swap)
                break;
            order[k - 1u] = b;
            order[k] = a;
        }
}

static int lib_ready(void)
{
    if (!lib_state)
        lib_scan();
    return lib_state > 0;
}

static uint32_t lib_count(void) { return lib_ready() ? nsongs : 0; }

/* song i of the list (by title): its header and META, or 0 (and *err: why it cannot be played) */
static const struct lib_item *lib_peek(uint32_t i, int *err)
{
    uint32_t k = order[i];
    *err = peek_err[k] ? peek_err[k] : open_err[k];
    return *err ? 0 : &peeked[k];
}

/* the player lets go of the song before its events are overwritten (the audio interrupt reads them) */
static void lib_release(void)
{
    uint32_t t = plat_ms();
    player_load(0);
    while (!player_idle() && plat_ms() - t < 200u)
        ;
    current_i = -1;
}

/* song i opened for playing: checked in full now (its first use) -> SONG_OK or SONG_E_* */
static int lib_open(uint32_t i, const struct song **out)
{
    const struct store_entry *e;
    int r;
    *out = 0;
    if (i >= lib_count())
        return SONG_E_SHORT;
    if (current_i == (int32_t)order[i]) {
        *out = &current;
        return SONG_OK;
    }
    lib_release();
    e = store_get(order[i]);
    r = song_open(&current, store_file(e), e->len, dec, sizeof dec);
    if (r == SONG_OK && song_crc32(store_file(e), e->len, 0) != e->crc)
        r = SONG_E_CRC;                   /* (song_open checked the file's own CRC; this is the record's) */
    if (r == SONG_OK && current.fmt2) {   /* 2.0: each layer's region, and level 1 built in it */
        uint32_t l, at = 0;
        for (l = 0; l < SONG_LAYERS && r == SONG_OK; l++) {
            region[l] = at;
            at += current.layer[l].need * 8u;   /* (song_open: the sum is at most SONG_BUILD_BYTES) */
            if (current.layer[l].variants)
                r = song_build(&current, l, 1, dec + region[l], current.layer[l].need * 8u);
        }
    }
    if (r) {
        open_err[order[i]] = (int8_t)r;
        return r;
    }
    current_i = (int32_t)order[i];
    *out = &current;
    return SONG_OK;
}

/* layer's level chosen (ui: a knob, a song opened): 2.0 builds it first, the player holding that layer meanwhile
 * (the interrupt plays the other layers on; this layer's notes are released and it goes on from where the song is) */
static void lib_level(uint32_t l, uint32_t v)
{
    uint32_t t;
    struct song_layer *ly = &current.layer[l];
    if (current_i >= 0 && current.fmt2 && l < SONG_LAYERS && ly->variants && v >= 1u && v <= ly->variants &&
        ly->built != v) {
        player_hold(1u << l);
        t = plat_ms();
        while (!(player_held() >> l & 1u) && plat_ms() - t < 200u)
            ;
        song_build(&current, l, v, dec + region[l], ly->need * 8u);
        player_variant(l, v);
        player_hold(0);
        return;
    }
    player_variant(l, v);
}

/* ------------------------------------------------------------------- USB commands --- */
#ifndef PLAT_HOST
#define APP_HAS_USB_CMD 1
static uint32_t rd32le(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void sc_result(uint32_t cmd, uint32_t result, int detail)
{
    uint8_t b[7];
    lib_ready();
    b[0] = (uint8_t)cmd;
    b[1] = (uint8_t)result;
    b[2] = (uint8_t)(int8_t)detail;
    b[3] = (uint8_t)nsongs;
    b[4] = (uint8_t)store_free_sectors();
    b[5] = (uint8_t)store_free_blocks();
    b[6] = (uint8_t)(store_free_blocks() >> 8);
    ota_send_msg(SC_RESULT, b, sizeof b);
}

static void sc_list(void)
{
    uint8_t b[32];
    uint32_t i, k, n = lib_count();
    for (i = 0; i < n; i++) {
        const struct store_entry *e = store_get(i);
        for (k = 0; k < sizeof b; k++)
            b[k] = 0;
        b[0] = (uint8_t)i;
        b[1] = (uint8_t)n;
        b[2] = peek_err[i] ? 2u : song_crc32(store_file(e), e->len, 0) != e->crc ? 1u : 0u;
        b[3] = (uint8_t)e->sectors;
        b[4] = (uint8_t)e->len;
        b[5] = (uint8_t)(e->len >> 8);
        b[6] = (uint8_t)(e->len >> 16);
        for (k = 0; k < 22u && e->slug[k]; k++)   /* (slugs have up to 20 characters) */
            b[7 + k] = (uint8_t)e->slug[k];
        b[30] = (uint8_t)e->blocks;
        b[31] = (uint8_t)(e->blocks >> 8);
        ota_send_msg(SC_ENTRY, b, sizeof b);
    }
    b[0] = (uint8_t)n;
    b[1] = (uint8_t)store_free_sectors();
    b[2] = (uint8_t)store_largest_free();
    b[3] = (uint8_t)STORE_SECTORS;
    b[4] = 3;                            /* the format versions it reads: 1.0 .. 1.this (M3: 1.2, M4: 1.3) */
    b[5] = lib_state > 0 ? SR_OK : SR_NO_FLASH;
    b[6] = (uint8_t)store_free_blocks();
    b[7] = (uint8_t)(store_free_blocks() >> 8);
    b[8] = (uint8_t)STORE_BLOCKS;
    b[9] = (uint8_t)(STORE_BLOCKS >> 8);
    b[10] = 2;                           /* M5: and 2.0 */
    ota_send_msg(SC_END, b, 11);
}

static void sc_send(const uint8_t *body, uint32_t n)
{
    uint32_t len, crc, off, k, at;
    char slug[24] = "";
    const uint8_t *f;
    struct song s;
    int r;
    if (n != 8u && n != 32u) {
        sc_result(SC_SEND, SR_BAD, 0);
        return;
    }
    len = rd32le(body);
    crc = rd32le(body + 4);
    for (k = 0; n == 32u && k < 23u && body[8 + k]; k++)
        slug[k] = (char)body[8 + k];
    slug[k] = 0;
    if (!lib_ready()) {
        sc_result(SC_SEND, SR_NO_FLASH, STORE_E_NO_FLASH);
        return;
    }
    if (len < 32u || len > STORE_FILE_MAX) {
        sc_result(SC_SEND, SR_SIZE, 0);
        return;
    }
    lib_release();                       /* the song playing stops: the flash is about to change */
    lib_changes++;
    ui_busy("MAKING ROOM", "");
    r = store_begin(len, slug, &at);     /* (may compact: songs moved, sectors erased) */
    lib_state = 0;
    if (r) {
        sc_result(SC_SEND, SR_STORE, r);
        app_say("SONG NOT STORED", store_error_name(r));
        return;
    }
    ui_busy("RECEIVING SONG", "");
    plat_host_patience(2000u, 120000u);
    for (off = 0; off < len; off += k) {  /* straight into the flash; it counts only once checked and committed */
        k = len - off < sizeof piece ? len - off : sizeof piece;
        if (plat_host_read(off, piece, k)) {
            store_abort();
            sc_result(SC_SEND, SR_TRANSFER, 0);
            app_say("SONG NOT RECEIVED", "transfer stopped");
            return;
        }
        if ((r = store_write(off, piece, k)) != STORE_OK) {
            store_abort();
            sc_result(SC_SEND, SR_STORE, r);
            app_say("SONG NOT STORED", store_error_name(r));
            return;
        }
    }
    f = plat_data(at);
    if (!f || song_crc32(f, len, 0) != crc) {
        store_abort();
        sc_result(SC_SEND, SR_CRC, 0);
        app_say("SONG NOT STORED", "damaged in transfer");
        return;
    }
    r = song_open(&s, f, len, dec, sizeof dec);   /* the whole song checked, in place, as the player will read it */
    if (r == SONG_OK && slug[0])
        for (k = 0; k < 24u && r == SONG_OK; k++) {
            if (s.slug[k] != slug[k])
                r = SONG_E_META;          /* not the song the laptop said */
            if (!slug[k])
                break;
        }
    if (r) {
        store_abort();
        sc_result(SC_SEND, SR_INVALID, r);
        app_say("SONG REFUSED", song_error_name(r));
        return;
    }
    ui_busy("STORING SONG", s.title);
    r = store_commit();
    lib_state = 0;
    if (r) {
        sc_result(SC_SEND, SR_STORE, r);
        app_say("SONG NOT STORED", store_error_name(r));
        return;
    }
    sc_result(SC_SEND, SR_OK, 0);
    app_say("SONG STORED", s.title);
}

static void sc_delete(const uint8_t *body, uint32_t n)
{
    char slug[24];
    uint32_t i;
    int r;
    if (n < 1u || n > 24u) {
        sc_result(SC_DELETE, SR_BAD, 0);
        return;
    }
    for (i = 0; i < n && i < 23u && body[i]; i++)
        slug[i] = (char)body[i];
    slug[i] = 0;
    if (!lib_ready()) {
        sc_result(SC_DELETE, SR_NO_FLASH, STORE_E_NO_FLASH);
        return;
    }
    lib_release();
    lib_changes++;
    r = store_delete(slug);
    lib_state = 0;
    sc_result(SC_DELETE, r == STORE_OK ? SR_OK : r == STORE_E_NOT_FOUND ? SR_NOT_FOUND : SR_STORE, r);
    app_say(r == STORE_OK ? "SONG DELETED" : "NOT DELETED", r == STORE_OK ? slug : store_error_name(r));
}

void app_usb_cmd(const uint8_t *d, uint32_t n)
{
    uint32_t len = (uint32_t)d[3] | (uint32_t)d[4] << 8 | (uint32_t)d[5] << 16;
    if (n < 7u || len + 7u != n)
        return;
    switch (d[2]) {
    case SC_LIST:
        sc_list();
        break;
    case SC_SEND:
        sc_send(d + 6, len);
        break;
    case SC_DELETE:
        sc_delete(d + 6, len);
        break;
    default:
        break;
    }
}
#endif
