/* Licence: TBD (see D017) */
/* The song store (store.h). */
#include "store.h"
#include "song.h"

#define MAGIC 0x52314D46u                 /* "FM1R" */

uint32_t store_refused;

static struct store_entry rec[STORE_SECTORS];   /* every whole record in the area, stale copies too */
static uint32_t nrec;
static uint8_t shown[STORE_MAX_SONGS];    /* the songs: the newest record of each slug */
static uint32_t nshown;
static uint8_t used[STORE_SECTORS];       /* sector k holds a record */

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

static int in_area(uint32_t off, uint32_t n)
{
    if (off >= STORE_LO && off <= STORE_HI && n <= STORE_HI - off)
        return 1;
    store_refused++;
    return 0;
}

/* the flash, through the area check */
static int erase(uint32_t off)
{
    const uint8_t *p;
    uint32_t i;
    if ((off & (STORE_SECTOR - 1u)) || !in_area(off, STORE_SECTOR))
        return STORE_E_RANGE;
    if (store_erase(off))
        return STORE_E_ERASE;
    p = store_map(off);
    for (i = 0; p && i < STORE_SECTOR; i++)
        if (p[i] != 0xFFu)
            return STORE_E_ERASE;
    return p ? STORE_OK : STORE_E_ERASE;
}

static int prog(uint32_t off, const uint8_t *src, uint32_t n)
{
    const uint8_t *p;
    uint32_t i;
    if (!in_area(off, n))
        return STORE_E_RANGE;
    if (store_prog(off, src, n))
        return STORE_E_WRITE;
    p = store_map(off);
    for (i = 0; p && i < n; i++)
        if (p[i] != src[i])
            return STORE_E_WRITE;
    return p ? STORE_OK : STORE_E_WRITE;
}

/* a record's head at off: valid (magic, its own CRC, a length that fits) -> its file length, else 0 */
static uint32_t head_ok(uint32_t off, const uint8_t *h)
{
    uint32_t len = rd32(h + 8);
    if (rd32(h) != MAGIC || song_crc32(h, 28, 0) != rd32(h + 28))
        return 0;
    if (len < 32u || len > STORE_FILE_MAX || len > STORE_HI - off - STORE_HEAD)
        return 0;
    return len;
}

int store_scan(void)
{
    uint32_t off, k, i;
    const uint8_t *base = store_map(STORE_LO);
    nrec = nshown = 0;
    for (k = 0; k < STORE_SECTORS; k++)
        used[k] = 0;
    if (!base)
        return STORE_E_NO_FLASH;
    for (off = STORE_LO; off < STORE_HI;) {
        const uint8_t *h = store_map(off);
        uint32_t len = h ? head_ok(off, h) : 0;
        struct store_entry *e = &rec[nrec];
        struct song s;
        if (!len) {
            off += STORE_SECTOR;
            continue;
        }
        e->off = off;
        e->len = len;
        e->seq = rd32(h + 4);
        e->crc = rd32(h + 12);
        e->sectors = (STORE_HEAD + len + STORE_SECTOR - 1u) / STORE_SECTOR;
        e->slug[0] = 0;
        if (song_peek(&s, h + STORE_HEAD, len) == SONG_OK)
            for (i = 0; i < sizeof e->slug - 1u && s.slug[i]; i++)
                e->slug[i] = s.slug[i], e->slug[i + 1u] = 0;
        for (k = 0; k < e->sectors; k++)
            used[(off - STORE_LO) / STORE_SECTOR + k] = 1;
        off += e->sectors * STORE_SECTOR;
        nrec++;
    }
    /* the songs: per slug the record with the highest sequence number (an older copy is stale: a cut came between
     * writing the new one and erasing it); a record whose file does not even peek is shown on its own (SONG ERROR) */
    for (i = 0; i < nrec && nshown < STORE_MAX_SONGS; i++) {
        int newest = 1;
        for (k = 0; k < nrec && rec[i].slug[0]; k++)
            if (k != i && same(rec[k].slug, rec[i].slug) && (rec[k].seq > rec[i].seq ||
                                                             (rec[k].seq == rec[i].seq && k > i)))
                newest = 0;
        if (newest)
            shown[nshown++] = (uint8_t)i;
    }
    return (int)nshown;
}

uint32_t store_count(void) { return nshown; }
const struct store_entry *store_get(uint32_t i) { return i < nshown ? &rec[shown[i]] : 0; }
const uint8_t *store_file(const struct store_entry *e) { return store_map(e->off + STORE_HEAD); }

uint32_t store_free_sectors(void)
{
    uint32_t k, n = 0;
    for (k = 0; k < STORE_SECTORS; k++)
        n += !used[k];
    return n;
}

static uint32_t find_run(uint32_t need)
{
    uint32_t k, run = 0;
    for (k = 0; k < STORE_SECTORS; k++) {
        run = used[k] ? 0 : run + 1u;
        if (run == need)
            return STORE_LO + (k + 1u - need) * STORE_SECTOR;
    }
    return 0;
}

uint32_t store_largest_free(void)
{
    uint32_t k, run = 0, best = 0;
    for (k = 0; k < STORE_SECTORS; k++) {
        run = used[k] ? 0 : run + 1u;
        if (run > best)
            best = run;
    }
    return best;
}

/* a record erased: its head first, so a cut leaves it invalid at once */
static int erase_record(const struct store_entry *e)
{
    uint32_t k;
    int r;
    for (k = 0; k < e->sectors; k++)
        if ((r = erase(e->off + k * STORE_SECTOR)) != STORE_OK)
            return r;
    return STORE_OK;
}

/* every record of slug, except the one at keep (0: none) */
static int erase_slug(const char *slug, uint32_t keep)
{
    uint32_t i;
    int r, found = 0;
    for (i = 0; i < nrec; i++)
        if (rec[i].slug[0] && same(rec[i].slug, slug) && rec[i].off != keep) {
            if ((r = erase_record(&rec[i])) != STORE_OK)
                return r;
            found = 1;
        }
    return found ? STORE_OK : STORE_E_NOT_FOUND;
}

/* stale copies (a cut came after a newer copy was written, before they were erased) erased: they only take room */
static int tidy(void)
{
    uint32_t i, k;
    int r;
    for (i = 0; i < nrec; i++) {
        int live = 0;
        for (k = 0; k < nshown; k++)
            live |= shown[k] == i;
        if (!live && rec[i].slug[0] && (r = erase_record(&rec[i])) != STORE_OK)
            return r;
    }
    return store_scan() < 0 ? STORE_E_NO_FLASH : STORE_OK;
}

int store_add(const uint8_t *file, uint32_t len, const char *slug)
{
    uint8_t h[STORE_HEAD];
    uint32_t need, off, k, seq = 0, done;
    int r;
    if (len < 32u || len > STORE_FILE_MAX)
        return STORE_E_SIZE;
    if (store_scan() < 0)
        return STORE_E_NO_FLASH;
    if ((r = tidy()) != STORE_OK)
        return r;
    need = (STORE_HEAD + len + STORE_SECTOR - 1u) / STORE_SECTOR;
    off = find_run(need);
    if (!off) {                           /* no room beside the older copy: it goes first, but only if that makes
                                           * room (the only case a cut can lose the song) */
        for (k = 0; k < nrec; k++)
            if (rec[k].slug[0] && same(rec[k].slug, slug))
                for (done = 0; done < rec[k].sectors; done++)
                    used[(rec[k].off - STORE_LO) / STORE_SECTOR + done] = 0;
        off = find_run(need);
        store_scan();
        if (!off)
            return STORE_E_FULL;          /* nothing erased */
        if ((r = erase_slug(slug, 0)) != STORE_OK || store_scan() < 0)
            return r ? r : STORE_E_NO_FLASH;
        off = find_run(need);
        if (!off)
            return STORE_E_FULL;
    }
    for (k = 0; k < nrec; k++)
        if (rec[k].seq >= seq)
            seq = rec[k].seq + 1u;
    for (k = 0; k < need; k++)
        if ((r = erase(off + k * STORE_SECTOR)) != STORE_OK)
            return r;
    for (done = 0; done < len; done += k) {   /* the file, a sector's worth at a time */
        k = len - done < STORE_SECTOR ? len - done : STORE_SECTOR;
        if ((r = prog(off + STORE_HEAD + done, file + done, k)) != STORE_OK)
            return r;
    }
    for (k = 0; k < STORE_HEAD; k++)
        h[k] = 0xFFu;
    wr32(h, MAGIC);
    wr32(h + 4, seq);
    wr32(h + 8, len);
    wr32(h + 12, song_crc32(file, len, 0));
    wr32(h + 28, song_crc32(h, 28, 0));
    if ((r = prog(off, h, STORE_HEAD)) != STORE_OK)   /* the head last: from here on the record counts */
        return r;
    store_scan();
    r = erase_slug(slug, off);            /* the older copy, now that the new one is whole */
    store_scan();
    return r == STORE_E_NOT_FOUND ? STORE_OK : r;
}

int store_delete(const char *slug)
{
    int r;
    if (store_scan() < 0)
        return STORE_E_NO_FLASH;
    r = erase_slug(slug, 0);
    store_scan();
    return r;
}

const char *store_error_name(int err)
{
    static const char *const N[] = {"OK", "FULL", "TOO BIG", "ERASE", "WRITE", "NOT FOUND", "RANGE", "NO FLASH"};
    return err <= 0 && err >= STORE_E_NO_FLASH ? N[-err] : "?";
}
