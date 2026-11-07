/* Licence: TBD (see D017) */
/* The song store (store.h). */
#include "store.h"
#include "song.h"

#define MAGIC 0x52324D46u                 /* "FM2R" (M5: blocks) */
#define MAGIC1 0x52314D46u                /* "FM1R" (M2-M4: whole sectors; still read) */
#define FILE_MAX1 (64u * 1024u)           /* M4's records: the file at most */
#ifdef __PI32V2__                         /* the FM-1's CPU */
#define STORE_POOL __attribute__((section(".pool")))   /* (fm1-base's big RAM region, as plat.h PLAT_POOL) */
#else
#define STORE_POOL
#endif
enum { B_FREE, B_LIVE, B_DIRTY };         /* a block: erased (all 0xFF), a record's, or to be erased before use */

uint32_t store_refused, store_compactions, store_moves;

static struct store_entry rec[STORE_MAX_RECORDS] STORE_POOL;   /* every whole record in the area, stale copies too */
static uint32_t nrec;
static uint16_t shown[STORE_MAX_SONGS];    /* the songs: the newest record of each slug (rec indices) */
static uint32_t nshown;
static uint8_t blk[STORE_BLOCKS] STORE_POOL;   /* B_* per block */
static struct {
    uint32_t active, off, len, span;      /* the record being received (store_begin .. store_commit) */
} pend;
static uint8_t bounce[STORE_BLOCK] __attribute__((aligned(4)));   /* a block in RAM: flash to flash goes through it */

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

static uint32_t off_of(uint32_t b) { return STORE_LO + b * STORE_BLOCK; }

/* the blocks a record of a len-byte file takes: up to a sector's worth exactly, more on whole sectors */
static uint32_t span_of(uint32_t len)
{
    uint32_t b = (STORE_HEAD + len + STORE_BLOCK - 1u) / STORE_BLOCK;
    return b <= STORE_SECTOR_BLOCKS ? b : (b + STORE_SECTOR_BLOCKS - 1u) / STORE_SECTOR_BLOCKS * STORE_SECTOR_BLOCKS;
}

/* a record of span blocks may start at block b: a short one inside its sector, a long one on a sector */
static int fits_at(uint32_t b, uint32_t span)
{
    if (span > STORE_BLOCKS - b)
        return 0;
    return span <= STORE_SECTOR_BLOCKS ? b % STORE_SECTOR_BLOCKS + span <= STORE_SECTOR_BLOCKS
                                       : b % STORE_SECTOR_BLOCKS == 0;
}

/* a head at block b -> B_LIVE (a record), B_DIRTY (a deleted record: *span still right), B_FREE (none) */
static int head_at(uint32_t b, uint32_t *len, uint32_t *span)
{
    const uint8_t *h = store_map(off_of(b));
    uint32_t m;
    if (!h)
        return B_FREE;
    m = rd32(h);
    *len = rd32(h + 8);
    if ((m == MAGIC || m == 0) && song_crc32(h + 4, 24, 0) == rd32(h + 28) && *len >= 32u && *len <= STORE_FILE_MAX) {
        *span = span_of(*len);
        if (fits_at(b, *span))
            return m == MAGIC ? B_LIVE : B_DIRTY;
    }
    if (m == MAGIC1 && b % STORE_SECTOR_BLOCKS == 0 && song_crc32(h, 28, 0) == rd32(h + 28) && *len >= 32u &&
        *len <= FILE_MAX1) {              /* M4's: on whole sectors */
        *span = (STORE_HEAD + *len + STORE_SECTOR - 1u) / STORE_SECTOR * STORE_SECTOR_BLOCKS;
        if (*span <= STORE_BLOCKS - b)
            return B_LIVE;
    }
    return B_FREE;
}

static int erased(uint32_t b)
{
    const uint8_t *p = store_map(off_of(b));
    uint32_t i;
    for (i = 0; p && i < STORE_BLOCK; i++)
        if (p[i] != 0xFFu)
            return 0;
    return p != 0;
}

int store_scan(void)
{
    uint32_t b, k, i, len, span;
    nrec = nshown = 0;
    if (!store_map(STORE_LO))
        return STORE_E_NO_FLASH;
    for (b = 0; b < STORE_BLOCKS;) {
        int kind = head_at(b, &len, &span);
        if (kind == B_LIVE && nrec < STORE_MAX_RECORDS) {
            struct store_entry *e = &rec[nrec++];
            const uint8_t *h = store_map(off_of(b));
            struct song s;
            e->off = off_of(b);
            e->len = len;
            e->seq = rd32(h + 4);
            e->crc = rd32(h + 12);
            e->blocks = span;
            e->sectors = (span + STORE_SECTOR_BLOCKS - 1u) / STORE_SECTOR_BLOCKS;
            e->slug = "";
            if (song_peek(&s, h + STORE_HEAD, len) == SONG_OK)
                e->slug = s.slug;         /* (in the file, in the flash: valid until the next change) */
        }
        if (kind != B_FREE) {             /* (a record past the table's end: kept, never erased) */
            for (k = 0; k < span; k++)
                blk[b + k] = (uint8_t)kind;
            b += span;
            continue;
        }
        blk[b] = erased(b) ? B_FREE : B_DIRTY;
        b++;
    }
    /* the songs: per slug the record with the highest sequence number (an older copy is stale: a cut came between
     * writing the new one and deleting it); a record whose file does not even peek is shown on its own (SONG ERROR) */
    for (i = 0; i < nrec && nshown < STORE_MAX_SONGS; i++) {
        int newest = 1;
        for (k = 0; k < nrec && rec[i].slug[0]; k++)
            if (k != i && same(rec[k].slug, rec[i].slug) && (rec[k].seq > rec[i].seq ||
                                                             (rec[k].seq == rec[i].seq && k > i)))
                newest = 0;
        if (newest)
            shown[nshown++] = (uint16_t)i;
    }
    return (int)nshown;
}

uint32_t store_count(void) { return nshown; }
const struct store_entry *store_get(uint32_t i) { return i < nshown ? &rec[shown[i]] : 0; }
const uint8_t *store_file(const struct store_entry *e) { return store_map(e->off + STORE_HEAD); }

uint32_t store_free_blocks(void)
{
    uint32_t b, n = 0;
    for (b = 0; b < STORE_BLOCKS; b++)
        n += blk[b] != B_LIVE;
    return n;
}

uint32_t store_free_sectors(void) { return store_free_blocks() / STORE_SECTOR_BLOCKS; }

static int sector_has(const uint8_t *map, uint32_t sec, int kind)
{
    uint32_t k;
    for (k = 0; k < STORE_SECTOR_BLOCKS; k++)
        if (map[sec * STORE_SECTOR_BLOCKS + k] == kind)
            return 1;
    return 0;
}

uint32_t store_largest_free(void)
{
    uint32_t s, run = 0, best = 0;
    for (s = 0; s < STORE_SECTORS; s++) {
        run = sector_has(blk, s, B_LIVE) ? 0 : run + 1u;
        if (run > best)
            best = run;
    }
    return best;
}

/* room for span blocks in map, outside sectors [xlo, xhi) and sector xs -> its first block, or STORE_BLOCKS if none.
 * A short record goes into the fullest sector it fits in (whole free sectors stay for long songs); a long one into
 * the first run */
#define OUT(sec) (((sec) >= xlo && (sec) < xhi) || (sec) == xs)
static uint32_t find(const uint8_t *map, uint32_t span, uint32_t xlo, uint32_t xhi, uint32_t xs)
{
    uint32_t s, b, k, best = STORE_BLOCKS, best_used = 0;
    if (span > STORE_SECTOR_BLOCKS) {
        for (b = 0; b + span <= STORE_BLOCKS; b += STORE_SECTOR_BLOCKS) {
            for (k = 0; k < span && map[b + k] == B_FREE && !OUT((b + k) / STORE_SECTOR_BLOCKS); k++)
                ;
            if (k == span)
                return b;
        }
        return STORE_BLOCKS;
    }
    for (s = 0; s < STORE_SECTORS; s++) {
        uint32_t used = 0, run = 0, at = STORE_BLOCKS;
        if (OUT(s))
            continue;
        for (k = 0; k < STORE_SECTOR_BLOCKS; k++) {
            b = s * STORE_SECTOR_BLOCKS + k;
            used += map[b] == B_LIVE ? 2u : map[b] == B_DIRTY ? 0x100u : 0;   /* (leftovers: last choice) */
            run = map[b] == B_FREE ? run + 1u : 0;
            if (run == span && at == STORE_BLOCKS)
                at = b + 1u - span;
        }
        used = used >= 0x100u ? (used & 0xFFu) : used + 0x1000u;   /* a sector without leftovers first, then the fullest */
        if (at != STORE_BLOCKS && (best == STORE_BLOCKS || used > best_used)) {
            best = at;
            best_used = used;
        }
    }
    return best;
}
#undef OUT
#define NOSEC STORE_SECTORS

/* a record deleted: its magic written to zero (the rest of its head stays readable, so the scan steps over it) */
static int kill(const struct store_entry *e)
{
    static const uint8_t zero[4] = {0, 0, 0, 0};
    return prog(e->off, zero, 4);
}

/* every record of slug, except the one at keep (0: none) */
static int kill_slug(const char *slug, uint32_t keep)
{
    uint32_t i;
    int r, found = 0;
    for (i = 0; i < nrec; i++)
        if (rec[i].slug[0] && same(rec[i].slug, slug) && rec[i].off != keep) {
            if ((r = kill(&rec[i])) != STORE_OK)
                return r;
            found = 1;
        }
    return found ? STORE_OK : STORE_E_NOT_FOUND;
}

static int rescan(void) { return store_scan() < 0 ? STORE_E_NO_FLASH : STORE_OK; }

/* stale copies (a cut came after a newer copy was written, before they were deleted) deleted: they only take room */
static int tidy(void)
{
    uint32_t i, k;
    int r, any = 0;
    for (i = 0; i < nrec; i++) {
        int live = 0;
        for (k = 0; k < nshown; k++)
            live |= shown[k] == i;
        if (!live && rec[i].slug[0]) {
            if ((r = kill(&rec[i])) != STORE_OK)
                return r;
            any = 1;
        }
    }
    return any ? rescan() : STORE_OK;
}

static uint32_t next_seq(void)
{
    uint32_t k, seq = 0;
    for (k = 0; k < nrec; k++)
        if (rec[k].seq >= seq)
            seq = rec[k].seq + 1u;
    return seq;
}

static void head(uint8_t *h, uint32_t seq, uint32_t len, uint32_t crc)
{
    uint32_t k;
    for (k = 0; k < STORE_HEAD; k++)
        h[k] = 0xFFu;
    wr32(h, MAGIC);
    wr32(h + 4, seq);
    wr32(h + 8, len);
    wr32(h + 12, crc);
    wr32(h + 28, song_crc32(h + 4, 24, 0));
}

/* ------------------------------------------------------------------ compacting --- */
/* The blocks as a model (what each is, and which record a live one belongs to), so a compaction can be tried out
 * first: the same steps are then done on the flash, one at a time, and the model taken again from the flash after
 * each. A step erases a sector with no song in it, or moves one short song. One sector is always kept free of songs
 * (a song is stored only if one stays), so a sector's songs can always be moved out of it. */
#define NONE 0xFFFFu
struct model {
    uint8_t map[STORE_BLOCKS];            /* B_* */
    uint16_t start[STORE_BLOCKS];         /* a live block: its record's first block */
};
enum { A_DONE, A_STUCK, A_ERASE, A_MOVE };
struct action {
    int what;
    uint32_t a, b;                        /* ERASE: sector a; MOVE: the record at block a to block b; DONE: room at a */
};
static struct model model_now STORE_POOL, model_try STORE_POOL;

static void model_of_flash(struct model *m)
{
    uint32_t b, i, k;
    for (b = 0; b < STORE_BLOCKS; b++) {
        m->map[b] = blk[b];
        m->start[b] = NONE;
    }
    for (i = 0; i < nrec; i++) {
        b = (rec[i].off - STORE_LO) / STORE_BLOCK;
        for (k = 0; k < rec[i].blocks; k++)
            m->start[b + k] = (uint16_t)b;
    }
}

static uint32_t rspan(const struct model *m, uint32_t s)
{
    uint32_t n = 0;
    while (s + n < STORE_BLOCKS && m->map[s + n] == B_LIVE && m->start[s + n] == s)
        n++;
    return n;
}

/* sector s: its live blocks; whether a long record (or one not in the table) is among them */
static uint32_t live_in(const struct model *m, uint32_t s, int *pinned)
{
    uint32_t k, n = 0;
    *pinned = 0;
    for (k = 0; k < STORE_SECTOR_BLOCKS; k++) {
        uint32_t b = s * STORE_SECTOR_BLOCKS + k;
        if (m->map[b] != B_LIVE)
            continue;
        n++;
        if (m->start[b] == NONE || rspan(m, m->start[b]) > STORE_SECTOR_BLOCKS)
            *pinned = 1;
    }
    return n;
}

/* with span blocks placed at b, a sector still has no song in it (the reserve) */
static int reserve_ok(const struct model *m, uint32_t b, uint32_t span)
{
    uint32_t s, k;
    for (s = 0; s < STORE_SECTORS; s++) {
        int free = 1;
        for (k = 0; k < STORE_SECTOR_BLOCKS && free; k++) {
            uint32_t x = s * STORE_SECTOR_BLOCKS + k;
            free = m->map[x] != B_LIVE && !(x >= b && x < b + span);
        }
        if (free)
            return 1;
    }
    return 0;
}

/* the next step towards room for span blocks: DONE (room at a), ERASE a sector with leftovers and no song, MOVE a song
 * out of the sector with leftovers and the fewest songs (so it can be erased next), or, for a long song once no
 * sector has leftovers, MOVE a song out of the run of sectors it will take; STUCK if none of these can be done */
static struct action next_step(const struct model *m, uint32_t span)
{
    struct action act = {A_STUCK, 0, 0};
    uint32_t s, k, n = span > STORE_SECTOR_BLOCKS ? span / STORE_SECTOR_BLOCKS : 0, wlo = 0, best;
    int pinned;
    uint32_t b = find(m->map, span, 0, 0, NOSEC);
    if (b != STORE_BLOCKS && reserve_ok(m, b, span)) {
        act.what = A_DONE;
        act.a = b;
        return act;
    }
    for (s = 0; s < STORE_SECTORS; s++)
        if (!live_in(m, s, &pinned) && sector_has(m->map, s, B_DIRTY)) {
            act.what = A_ERASE;
            act.a = s;
            return act;
        }
    for (best = 0xFFFFFFFFu, s = 0; s < STORE_SECTORS; s++) {
        uint32_t live = live_in(m, s, &pinned);
        if (pinned || !live || live >= best || !sector_has(m->map, s, B_DIRTY))
            continue;
        for (k = 0; k < STORE_SECTOR_BLOCKS; k++) {
            uint32_t x = s * STORE_SECTOR_BLOCKS + k, to;
            if (m->map[x] != B_LIVE || m->start[x] != x)
                continue;
            to = find(m->map, rspan(m, x), 0, 0, s);
            if (to != STORE_BLOCKS) {
                best = live;
                act.what = A_MOVE;
                act.a = x;
                act.b = to;
            }
            break;
        }
    }
    if (act.what != A_STUCK || !n)
        return act;
    for (best = 0xFFFFFFFFu, s = 0; s + n <= STORE_SECTORS; s++) {   /* a long song: the run with the fewest songs */
        uint32_t live = 0, ok = 1;
        for (k = s; k < s + n && ok; k++) {
            live += live_in(m, k, &pinned);
            ok = !pinned;
        }
        if (ok && live < best) {
            best = live;
            wlo = s;
        }
    }
    if (best == 0xFFFFFFFFu)
        return act;
    for (b = wlo * STORE_SECTOR_BLOCKS; b < (wlo + n) * STORE_SECTOR_BLOCKS; b++)
        if (m->map[b] == B_LIVE && m->start[b] == b) {
            uint32_t to = find(m->map, rspan(m, b), wlo, wlo + n, NOSEC);
            if (to != STORE_BLOCKS) {
                act.what = A_MOVE;
                act.a = b;
                act.b = to;
            }
            return act;
        }
    return act;
}

static void model_apply(struct model *m, const struct action *a)
{
    uint32_t k, span;
    if (a->what == A_ERASE) {
        for (k = 0; k < STORE_SECTOR_BLOCKS; k++) {
            m->map[a->a * STORE_SECTOR_BLOCKS + k] = B_FREE;
            m->start[a->a * STORE_SECTOR_BLOCKS + k] = NONE;
        }
    } else if (a->what == A_MOVE) {
        span = rspan(m, a->a);
        for (k = 0; k < span; k++) {
            m->map[a->b + k] = B_LIVE;
            m->start[a->b + k] = (uint16_t)a->b;
            m->map[a->a + k] = B_DIRTY;
            m->start[a->a + k] = NONE;
        }
    }
}

#define MAX_STEPS (2u * STORE_BLOCKS + STORE_SECTORS)

/* the compaction tried on m (changed): 1 if it ends with room for span blocks */
static int dry_run(struct model *m, uint32_t span)
{
    uint32_t i;
    for (i = 0; i < MAX_STEPS; i++) {
        struct action a = next_step(m, span);
        if (a.what == A_DONE)
            return 1;
        if (a.what == A_STUCK)
            return 0;
        model_apply(m, &a);
    }
    return 0;
}

/* a short record copied to block to (its head last, with a new sequence number), then the original deleted */
static int move(uint32_t from_off, uint32_t to)
{
    uint32_t i, len = 0, crc = 0, done, k, seq, blocks = 0;
    int r;
    for (i = 0; i < nrec; i++)
        if (rec[i].off == from_off) {
            len = rec[i].len;
            crc = rec[i].crc;
            blocks = rec[i].blocks;
        }
    if (!blocks)
        return STORE_E_RANGE;             /* (a bug if ever seen) */
    seq = next_seq();
    for (done = 0; done < len; done += k) {
        const uint8_t *p = store_map(from_off + STORE_HEAD + done);
        k = len - done < STORE_BLOCK ? len - done : STORE_BLOCK;
        for (i = 0; i < k; i++)
            bounce[i] = p[i];
        if ((r = prog(off_of(to) + STORE_HEAD + done, bounce, k)) != STORE_OK)
            return r;
    }
    head(bounce, seq, len, crc);
    if ((r = prog(off_of(to), bounce, STORE_HEAD)) != STORE_OK)
        return r;
    for (i = 0; i < nrec; i++)
        if (rec[i].off == from_off && (r = kill(&rec[i])) != STORE_OK)
            return r;
    store_moves++;
    return STORE_OK;
}

/* the compaction done on the flash -> STORE_OK and *at the room's first block, or STORE_E_FULL */
static int compact(uint32_t span, uint32_t *at)
{
    uint32_t i;
    int r;
    for (i = 0; i < MAX_STEPS; i++) {
        struct action a;
        model_of_flash(&model_now);
        a = next_step(&model_now, span);
        if (a.what == A_DONE) {
            *at = a.a;
            return STORE_OK;
        }
        if (a.what == A_STUCK)
            return STORE_E_FULL;
        if (a.what == A_ERASE) {
            r = erase(STORE_LO + a.a * STORE_SECTOR);
            store_compactions++;
        } else {
            r = move(off_of(a.a), a.b);
        }
        if (r != STORE_OK || (r = rescan()) != STORE_OK)
            return r;
    }
    return STORE_E_FULL;
}

/* ----------------------------------------------------------------- adding, deleting --- */
int store_begin(uint32_t len, const char *slug, uint32_t *file)
{
    uint32_t span, b = 0, i, k;
    int r, old = 0;
    pend.active = 0;
    if (len < 32u || len > STORE_FILE_MAX)
        return STORE_E_SIZE;
    if (store_scan() < 0)
        return STORE_E_NO_FLASH;
    if ((r = tidy()) != STORE_OK)
        return r;
    span = span_of(len);
    model_of_flash(&model_try);
    if (!dry_run(&model_try, span)) {     /* no room even by compacting: the older copy goes first, but only if that
                                           * makes room (the only case a cut can lose the song) */
        model_of_flash(&model_try);
        for (i = 0; slug && slug[0] && i < nrec; i++)
            if (rec[i].slug[0] && same(rec[i].slug, slug)) {
                b = (rec[i].off - STORE_LO) / STORE_BLOCK;
                for (k = 0; k < rec[i].blocks; k++) {
                    model_try.map[b + k] = B_DIRTY;
                    model_try.start[b + k] = NONE;
                }
                old = 1;
            }
        if (!old || !dry_run(&model_try, span))
            return STORE_E_FULL;          /* nothing written */
        if ((r = kill_slug(slug, 0)) != STORE_OK || (r = rescan()) != STORE_OK)
            return r;
    }
    if ((r = compact(span, &b)) != STORE_OK)
        return r;
    pend.active = 1;
    pend.off = off_of(b);
    pend.len = len;
    pend.span = span;
    *file = pend.off + STORE_HEAD;
    return STORE_OK;
}

int store_write(uint32_t off, const void *src, uint32_t n)
{
    if (!pend.active)
        return STORE_E_STATE;
    if (off > pend.len || n > pend.len - off) {
        store_refused++;
        return STORE_E_RANGE;
    }
    return prog(pend.off + STORE_HEAD + off, src, n);
}

void store_abort(void) { pend.active = 0; }   /* (its blocks: taken back by the next compacting) */

int store_commit(void)
{
    uint8_t h[STORE_HEAD];
    const uint8_t *f;
    struct song s;
    const char *slug = "";
    char keep[24];
    uint32_t k;
    int r;
    if (!pend.active)
        return STORE_E_STATE;
    pend.active = 0;
    f = store_map(pend.off + STORE_HEAD);
    if (!f)
        return STORE_E_NO_FLASH;
    if (song_peek(&s, f, pend.len) == SONG_OK)
        slug = s.slug;
    for (k = 0; k < sizeof keep - 1u && slug[k]; k++)   /* (a copy: the table is rebuilt below) */
        keep[k] = slug[k];
    keep[k] = 0;
    head(h, next_seq(), pend.len, song_crc32(f, pend.len, 0));
    if ((r = prog(pend.off, h, STORE_HEAD)) != STORE_OK)   /* the head last: from here on the record counts */
        return r;
    if ((r = rescan()) != STORE_OK)
        return r;
    r = keep[0] ? kill_slug(keep, pend.off) : STORE_E_NOT_FOUND;   /* the older copy, now that the new one is whole */
    if (r != STORE_OK && r != STORE_E_NOT_FOUND)
        return r;
    return rescan();
}

int store_add(const uint8_t *file, uint32_t len, const char *slug)
{
    uint32_t at, done, k;
    int r;
    if ((r = store_begin(len, slug, &at)) != STORE_OK)
        return r;
    for (done = 0; done < len; done += k) {   /* the file, a block's worth at a time */
        k = len - done < STORE_BLOCK ? len - done : STORE_BLOCK;
        if ((r = store_write(done, file + done, k)) != STORE_OK) {
            store_abort();
            return r;
        }
    }
    return store_commit();
}

int store_delete(const char *slug)
{
    int r;
    if (store_scan() < 0)
        return STORE_E_NO_FLASH;
    r = kill_slug(slug, 0);
    store_scan();
    return r;
}

const char *store_error_name(int err)
{
    static const char *const N[] = {"OK", "FULL", "TOO BIG", "ERASE", "WRITE", "NOT FOUND", "RANGE", "NO FLASH",
                                    "STATE"};
    return err <= 0 && err >= STORE_E_STATE ? N[-err] : "?";
}
