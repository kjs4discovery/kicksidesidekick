/* Licence: TBD (see D017) */
/* The song library (M2 part 1): the songs in the flash's song store (store.h), the one song open for playing, and
 * the laptop's commands over USB (tools/fm1song send / list / delete). Included by ui.c (fm1-base's unity build).
 *
 * Nothing here runs at boot: the store is scanned when the song list is first drawn (after the splash), each song
 * is checked in full (song_open) when it is opened, and a bad one shows SONG ERROR while the rest work.
 *
 * USB (fm1-base APP_USB_CMD; the update's SysEx framing, F0 pack7(00 59 cmd len24 body ~sum) F7). The laptop sends:
 *   0x70 LIST                    -> 0x71 ENTRY per song {index, count, status, sectors, len u24, slug[24], 0},
 *                                   then 0x72 END {count, free sectors, longest free run, sectors, minor, result}
 *   0x73 SEND {len u32, crc u32} -> the FM-1 pulls the file with the update's read requests (plat_host_read) into RAM,
 *                                   checks its CRC and the whole song (song_open), then stores it (store_add)
 *   0x74 DELETE {slug}
 * and gets 0x7F RESULT {cmd, result (SR_*), detail (a SONG_E_* or STORE_E_* number), songs, free sectors}.
 * A send or delete stops the song playing and goes back to the list (the file being played may be rewritten). */
#include "store.h"

#define SC_LIST 0x70u
#define SC_ENTRY 0x71u
#define SC_END 0x72u
#define SC_SEND 0x73u
#define SC_DELETE 0x74u
#define SC_RESULT 0x7Fu
enum { SR_OK, SR_NO_FLASH, SR_SIZE, SR_TRANSFER, SR_CRC, SR_INVALID, SR_STORE, SR_NOT_FOUND, SR_BAD };

static uint8_t rx[STORE_FILE_MAX] PLAT_POOL __attribute__((aligned(4)));          /* a song being received */
static uint8_t dec[SONG_DECODE_BYTES] PLAT_POOL __attribute__((aligned(4)));      /* the open song's events */
static struct song peeked[STORE_MAX_SONGS] PLAT_POOL;
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

static int title_before(const struct song *a, const struct song *b)
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
        peek_err[i] = (int8_t)song_peek(&peeked[i], store_file(e), e->len);
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
static const struct song *lib_peek(uint32_t i, int *err)
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
    if (r) {
        open_err[order[i]] = (int8_t)r;
        return r;
    }
    current_i = (int32_t)order[i];
    *out = &current;
    return SONG_OK;
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
    uint8_t b[5];
    lib_ready();
    b[0] = (uint8_t)cmd;
    b[1] = (uint8_t)result;
    b[2] = (uint8_t)(int8_t)detail;
    b[3] = (uint8_t)nsongs;
    b[4] = (uint8_t)store_free_sectors();
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
        for (k = 0; k < 23u && e->slug[k]; k++)
            b[7 + k] = (uint8_t)e->slug[k];
        ota_send_msg(SC_ENTRY, b, sizeof b);
    }
    b[0] = (uint8_t)n;
    b[1] = (uint8_t)store_free_sectors();
    b[2] = (uint8_t)store_largest_free();
    b[3] = (uint8_t)STORE_SECTORS;
    b[4] = 2;                            /* the format versions it reads: 1.0 .. 1.this (M3: 1.2) */
    b[5] = lib_state > 0 ? SR_OK : SR_NO_FLASH;
    ota_send_msg(SC_END, b, 6);
}

static void sc_send(const uint8_t *body, uint32_t n)
{
    uint32_t len, crc, off, k;
    struct song s;
    int r;
    if (n != 8u) {
        sc_result(SC_SEND, SR_BAD, 0);
        return;
    }
    len = rd32le(body);
    crc = rd32le(body + 4);
    if (!lib_ready()) {
        sc_result(SC_SEND, SR_NO_FLASH, STORE_E_NO_FLASH);
        return;
    }
    if (len < 32u || len > STORE_FILE_MAX) {
        sc_result(SC_SEND, SR_SIZE, 0);
        return;
    }
    lib_release();                       /* the song playing stops: rx and the flash are about to change */
    lib_changes++;
    ui_busy("RECEIVING SONG", "");
    plat_host_patience(2000u, 120000u);
    for (off = 0; off < len; off += k) {  /* the whole file into RAM first: nothing is written before it is checked */
        k = len - off < 512u ? len - off : 512u;
        if (plat_host_read(off, rx + off, k)) {
            sc_result(SC_SEND, SR_TRANSFER, 0);
            app_say("SONG NOT RECEIVED", "transfer stopped");
            return;
        }
    }
    if (song_crc32(rx, len, 0) != crc) {
        sc_result(SC_SEND, SR_CRC, 0);
        app_say("SONG NOT STORED", "damaged in transfer");
        return;
    }
    r = song_open(&s, rx, len, dec, sizeof dec);   /* the whole song checked, as the player will read it */
    if (r) {
        sc_result(SC_SEND, SR_INVALID, r);
        app_say("SONG REFUSED", song_error_name(r));
        return;
    }
    ui_busy("STORING SONG", s.title);
    r = store_add(rx, len, s.slug);
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
