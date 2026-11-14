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
 *                                   result, free blocks u16, blocks u16, major, mode} (M5: the fields after result;
 *                                   major 2: it reads 2.0 too; mode 2: SYNTH, sends refused; M7: + minor2, 1:
 *                                   it reads 2.1)
 *   0x73 SEND {len u32, crc u32[, slug[24]]} -> the FM-1 makes room (store_begin), pulls the file with the update's
 *                                   read requests (plat_host_read) straight into the flash (M5; M4: into RAM, 64 KiB
 *                                   at most), checks its CRC and the whole song (song_open) in place, then commits it
 *                                   (store_commit). The slug (M5) lets the song's older copy make way if it must
 *   0x74 DELETE {slug}
 *   0x75 WIPE {"WIPE"}           (M5.1) every song erased: the whole song store, sector by sector
 * and gets 0x7F RESULT {cmd, result (SR_*), detail (a SONG_E_* or STORE_E_* number), songs, free sectors, free blocks
 * u16 (M5)}.
 *
 * M5 (format 2.0, D029 A): a 2.0 song plays one level of each layer at a time. dec holds each layer's level, in a
 * region of its own sized for its largest level (song_layer need); lib_level builds another level into it, with the
 * player holding that layer meanwhile (player_hold). 1.x songs decode into dec as before, every level at once.
 *
 * A send or delete stops the song playing and goes back to the list (the file being played may be rewritten).
 *
 * M5.1 (the 2026-11-06 incident: a watchdog reset inside SEND): nothing in a command runs long. When a song needs room
 * that only compacting gives, SEND answers SR_BUSY ("making room", detail: the steps, about) at once; the main loop
 * then compacts a step at a time (lib_poll, progress on the screen, the watchdog fed every round) and the laptop
 * sends again. Every flash write's source is copied to RAM first (store_prog): the flash driver switches the XIP
 * window off while it writes. */
#include "store.h"
#include "vsynth.h"

#define SC_LIST 0x70u
#define SC_ENTRY 0x71u
#define SC_END 0x72u
#define SC_SEND 0x73u
#define SC_DELETE 0x74u
#define SC_RESULT 0x7Fu
#define SC_WIPE 0x75u
#define SC_FACTORY 0x78u                 /* M10.1: FACTORY RESET (fm1song factory-reset): songs, saved presets, settings */
enum { SR_OK, SR_NO_FLASH, SR_SIZE, SR_TRANSFER, SR_CRC, SR_INVALID, SR_STORE, SR_NOT_FOUND, SR_BAD,
       SR_SYNTH,                         /* M5: in SYNTH mode (the song buffer is the synth's): restart in LEARN */
       SR_BUSY };                        /* M5.1: making room (compacting from the main loop): send again soon */

#define LIB_DEC_BYTES (SONG_BUILD_BYTES > SONG_DECODE_BYTES ? SONG_BUILD_BYTES : SONG_DECODE_BYTES)
static uint8_t piece[512] __attribute__((aligned(4)));                          /* a song being received: a piece */
static uint8_t dec[LIB_DEC_BYTES] PLAT_POOL __attribute__((aligned(4)));        /* the open song's events */
static uint32_t region[SONG_LAYERS];     /* 2.0: each layer's level in dec, from here */
/* a song of the list: what the list shows (M4 kept the whole struct song: too big for 200 songs) */
struct lib_item {
    const char *title, *key, *category;  /* in the file, in the flash */
    uint32_t bpm, beats_per_bar;
    const char *numerals;                /* M8 (2.2 FORM): a backtrack's numerals ("" a song) */
    uint8_t kind, number, scale, key_mode;   /* M8: SONG_KIND_*, a backtrack's number and scale; the key's mode */
};
static struct lib_item peeked[STORE_MAX_SONGS] PLAT_POOL;
static int8_t peek_err[STORE_MAX_SONGS];
static int8_t open_err[STORE_MAX_SONGS];  /* found when opened (the full check): the list keeps showing it */
static uint8_t order[STORE_MAX_SONGS];   /* the list's order: by title */
static uint32_t nsongs;
static int lib_state;                    /* 0 not scanned, 1 scanned, < 0 STORE_E_NO_FLASH */
static struct song current;              /* the song open for playing (its events in dec) */
static int32_t current_i = -1;
static uint32_t lib_room = sizeof dec;    /* M8: the part of dec a song may use (BACKTRACK: the rest is the synth's) */
static uint32_t lib_changes;             /* counts sends and deletes: the UI goes back to the list */

/* the store's flash: fm1-base's data area (store.c checks the song area's bounds before each erase and write) */
const uint8_t *store_map(uint32_t off) { return plat_data(off); }
int store_erase(uint32_t off) { return plat_data_erase(off); }
/* M5.1: the source copied to RAM first, whatever it is (the flash driver turns the XIP window off while it writes:
 * a source in the flash, as M5's kill() zeros in .rodata were, stalls the core until the watchdog resets it) */
static uint8_t prog_ram[256] __attribute__((aligned(4)));
int store_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = src;
    uint32_t k, i;
    int r;
    for (; n; n -= k, off += k, p += k) {
        k = n < sizeof prog_ram ? n : sizeof prog_ram;
        for (i = 0; i < k; i++)
            prog_ram[i] = p[i];
        if ((r = plat_data_write(off, prog_ram, k)) != 0)
            return r;
    }
    return 0;
}
/* the store's longer loops (a scan, planning a compaction): the watchdog fed */
#ifdef PLAT_HOST
void store_yield(void) {}
#else
void store_yield(void) { fm1_wdt_feed(); }
#endif

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
    int r = store_open();                /* (M5.1: the store's table is kept up to date: scanned once) */
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
        if ((i & 15u) == 15u)
            store_yield();
        peek_err[i] = (int8_t)song_peek(&s, store_file(e), e->len);
        peeked[i].title = peek_err[i] ? "" : s.title;
        peeked[i].key = peek_err[i] ? "" : s.key;
        peeked[i].category = peek_err[i] ? "" : s.category;
        peeked[i].bpm = peek_err[i] ? 0 : player_song_bpm(&s);
        peeked[i].beats_per_bar = s.beats_per_bar;
        peeked[i].numerals = peek_err[i] ? "" : s.numerals;
        peeked[i].kind = peek_err[i] ? SONG_KIND_SONG : s.kind;
        peeked[i].number = s.bt_number;
        peeked[i].scale = s.scale;
        peeked[i].key_mode = s.key_mode;
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
    if (lm_active())                     /* M10.1: SONG LEARN's melody synth ends with the song (its layer still held by lm_off) */
        lm_off();
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
    if (r == SONG_OK && current.fmt2) {   /* 2.0: each layer's region, and level 1 built in it (M7, 2.1: its start
                                           * level) */
        uint32_t l, at = 0;
        for (l = 0; l < SONG_LAYERS; l++)
            at += current.layer[l].need * 8u;
        if (at > lib_room)                /* (M8: BACKTRACK's part of the buffer) */
            r = SONG_E_ROOM;
        at = 0;
        for (l = 0; l < SONG_LAYERS && r == SONG_OK; l++) {
            region[l] = at;
            at += current.layer[l].need * 8u;   /* (song_open: the sum is at most SONG_BUILD_BYTES) */
            if (current.layer[l].variants)
                r = song_build(&current, l, current.layer[l].start ? current.layer[l].start : 1u, dec + region[l],
                               current.layer[l].need * 8u);
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

/* M8 (BACKTRACK): the open song in another style and metre (song_restyle), each layer's region again and its level 1
 * built; the player must not hold it (lib_open's song, before player_load) -> SONG_OK or SONG_E_*; on an error the song
 * is closed (opened again next time) */
static int lib_restyle(uint32_t style, uint32_t beats_per_bar)
{
    uint32_t l, at = 0;
    int r;
    if (current_i < 0)
        return SONG_E_LAYR;
    r = song_restyle(&current, style, beats_per_bar);
    for (l = 0; l < SONG_LAYERS && r == SONG_OK; l++) {
        region[l] = at;
        at += current.layer[l].need * 8u;
    }
    if (r == SONG_OK && at > lib_room)
        r = SONG_E_ROOM;
    for (l = 0; l < SONG_LAYERS && r == SONG_OK; l++)
        if (current.layer[l].variants)
            r = song_build(&current, l, 1u, dec + region[l], current.layer[l].need * 8u);
    if (r)
        current_i = -1;
    return r;
}

/* layer's level chosen (ui: a knob, a song opened): 2.0 builds it first, the player holding that layer meanwhile
 * (the interrupt plays the other layers on; this layer's notes are released and it goes on from where the song is) */
static void lib_level(uint32_t l, uint32_t v)
{
    uint32_t t;
    struct song_layer *ly = &current.layer[l];
    if (l < SONG_LAYERS)                  /* (M7: for the tests) */
        player_stats.levels = (player_stats.levels & ~(15u << 4u * l)) | (v & 15u) << 4u * l;
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

/* M5.1: room being made (a SEND answered SR_BUSY): a few steps each round of the main loop, about 40 ms of them, the
 * progress on the screen -> 1 while busy (the UI does nothing else meanwhile), 0 */
static uint32_t lib_poll_ms;
static int lib_poll(void)
{
    uint32_t t = plat_ms(), done, total;
    int r = STORE_BUSY;
    char b[24];
    if (!store_busy(0, 0))
        return 0;
    while (r == STORE_BUSY && plat_ms() - t < 40u)
        r = store_step();
    lib_state = 0;
    if (r != STORE_BUSY) {
        lib_changes++;
        if (r == STORE_OK)
            app_say("ROOM MADE", "the laptop sends again");
        else
            app_say("NO ROOM", store_error_name(r));
        return 0;
    }
    if (t - lib_poll_ms >= 250u) {
        store_busy(&done, &total);
        b[0] = 0;
        text_cat(b, "step ");
        num_cat(b, done);
        text_cat(b, " of about ");
        num_cat(b, total);
        ui_busy("MAKING ROOM", b);
        lib_poll_ms = t;
    }
    return 1;
}

/* M5.1: WIPE SONG STORE (the settings, a power-on combination, fm1song wipe): every sector of the song store erased,
 * the progress on the screen; the watchdog is fed by every erase -> STORE_OK or the first error */
static int lib_wipe(void)
{
    uint32_t s;
    int r = STORE_OK, e;
    char b[24];
    lib_release();
    store_stop();
    for (s = 0; s < STORE_SECTORS; s++) {
        if (s % 8u == 0) {
            b[0] = 0;
            num_cat(b, s * 100u / STORE_SECTORS);
            text_cat(b, " %");
            ui_busy("WIPING SONG STORE", b);
        }
        e = store_wipe_sector(s);
        if (e != STORE_OK && r == STORE_OK)
            r = e;
        store_yield();
    }
    store_scan();
    lib_state = 0;
    lib_changes++;
    return r;
}

/* M10.1: FACTORY RESET: the song store erased (lib_wipe), the preset object's two sectors erased (a blank one is left), the settings saved
 * as "none yet" (settings_factory). Each part starts on junk or an empty area at the next boot, so a power cut at any flash operation
 * leaves a unit that boots with each part old or empty; the panel calibration (0xDC000) and the firmware are never touched -> 0 or the first
 * error (a store error is negative, the platform's erase result positive) */
static int lib_factory_reset(void)
{
    int r = lib_wipe(), e;
    uint32_t o, k;
    for (o = UP_FLASH_LO; o < UP_FLASH_LO + UP_FLASH_SECTORS * 4096u; o += 4096u) {
        const uint8_t *p = plat_data(o);
        int blank = p != 0;
        for (k = 0; blank && k < 4096u; k++)
            blank = p[k] == 0xFFu;
        if (!blank) {
            e = plat_data_erase(o);
            if (e && !r)
                r = e;
        }
        store_yield();
    }
    upreset_clear_ram();
    e = settings_factory();
    if (e && !r)
        r = e;
    return r;
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
    b[11] = (uint8_t)player_stats.mode;  /* M5 part 3: 0 the mode screen, 1 LEARN, 2 SYNTH (sends refused) */
    b[12] = 2;                           /* M7: 2.0 .. 2.this (2.1: a start level per layer; M8: 2.2, FORM) */
    ota_send_msg(SC_END, b, 13);
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
    if (lm_active())                     /* M10.1: SONG LEARN's melody synth gives the buffer back first */
        lm_off();
    if (vsynth_active()) {               /* (song_open would decode into the synth's delay lines) */
        sc_result(SC_SEND, SR_SYNTH, 0);
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
    r = store_begin(len, slug, &at);     /* (M5.1: never compacts here: STORE_BUSY, and lib_poll makes the room) */
    lib_state = 0;
    if (r == STORE_BUSY) {
        uint32_t done, total;
        store_busy(&done, &total);
        sc_result(SC_SEND, SR_BUSY, (int)(total - done > 127u ? 127u : total - done));
        ui_busy("MAKING ROOM", "");
        return;
    }
    if (r) {
        sc_result(SC_SEND, SR_STORE, r);
        app_say("SONG NOT STORED", store_error_name(r));
        return;
    }
    ui_busy("RECEIVING SONG", "");
    plat_host_patience(2000u, 120000u);
    for (off = 0; off < len; off += k) {  /* straight into the flash; it counts only once checked and committed */
        k = len - off < sizeof piece ? len - off : sizeof piece;
        store_yield();
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

static void sc_wipe(const uint8_t *body, uint32_t n)
{
    int r;
    if (n != 4u || body[0] != 'W' || body[1] != 'I' || body[2] != 'P' || body[3] != 'E') {
        sc_result(SC_WIPE, SR_BAD, 0);
        return;
    }
    if (lib_state < 0 || (!lib_state && store_open() < 0)) {
        sc_result(SC_WIPE, SR_NO_FLASH, STORE_E_NO_FLASH);
        return;
    }
    r = lib_wipe();
    sc_result(SC_WIPE, r == STORE_OK ? SR_OK : SR_STORE, r);
    app_say(r == STORE_OK ? "SONG STORE WIPED" : "NOT WIPED", r == STORE_OK ? "no songs now" : store_error_name(r));
}

static void sc_factory(const uint8_t *body, uint32_t n)
{
    int r;
    if (n != 5u || body[0] != 'R' || body[1] != 'E' || body[2] != 'S' || body[3] != 'E' || body[4] != 'T') {
        sc_result(SC_FACTORY, SR_BAD, 0);
        return;
    }
    if (lm_active())                     /* (SONG LEARN's melody synth gives the buffer back first, as for a send) */
        lm_off();
    if (vsynth_active()) {               /* SYNTH and BACKTRACK: refused as a send is (result 9) */
        sc_result(SC_FACTORY, SR_SYNTH, 0);
        return;
    }
    if (lib_state < 0 || (!lib_state && store_open() < 0)) {
        sc_result(SC_FACTORY, SR_NO_FLASH, STORE_E_NO_FLASH);
        return;
    }
    r = lib_factory_reset();
    sc_result(SC_FACTORY, r == 0 ? SR_OK : SR_STORE, r);
    if (r == 0) {
        ui_reset_done();
    } else {
        app_say("NOT RESET", "see fm1song");
    }
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
    case SC_WIPE:
        sc_wipe(d + 6, len);
        break;
    case SC_FACTORY:
        sc_factory(d + 6, len);
        break;
    default:
        break;
    }
}
#endif

/* (M6's drum jam, a song played by the song player under the synth, was here: M7 (D041) has its own step
 * sequencer, synth.c) */
