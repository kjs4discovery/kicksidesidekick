/* SPDX-License-Identifier: GPL-3.0-or-later */
/* store_test: the song store (firmware/app/store.c) on the laptop, on a simulated flash, under ASan/UBSan
 * (tools/store_test.sh). The flash behaves as NOR: an erase sets a sector to 0xFF, a write can only clear bits. A
 * power cut at the Nth erase or write leaves that operation half done (half the sector erased, half the bytes
 * written) and nothing after it; then the "FM-1" starts again and scans.
 *
 *   store_test A.fm1s B.fm1s ...    A: a song in two versions (a short file, then a long one: same slug), the rest
 *                                   other songs. Checks, printing one line each:
 *   1. a flash full of junk (another firmware's leftovers): no songs, nothing written by a scan
 *   2. add, list, read back, delete, add again; a file is stored byte for byte
 *   3. the area fills up: STORE_E_FULL, and nothing that was stored is lost
 *   4. replacing a song (room for both copies), cut at every operation: the others intact, the song old or new
 *   5. replacing a song when only its own room is left, cut at every operation: the others intact, the song old,
 *      new or gone (the one case it may go)
 *   6. deleting, cut at every operation: the others intact, the song there or gone
 *   7. adding a new song, cut at every operation: the others intact, the song there or not
 *   M5 (blocks): 8. short songs share sectors (how many fit); 9. a long song into an area with room only after
 *   compacting (sectors erased, songs moved), cut at every operation: every song intact, the new one there or not;
 *   10. receiving: a refused file (store_abort) leaves nothing, a write outside the room is refused; 11. M4's
 *   records (FM1R, whole sectors) are read, deleted, and their room taken back
 *   and after every cut: a new add of the song works. Nothing is ever erased or written outside the area.
 *   M5.1: 12. room made a step at a time (store_begin answers STORE_BUSY and writes nothing; each store_step is one
 *   erase or one short song moved); 13. WIPE SONG STORE; and after every operation the table the store keeps up to
 *   date (no rescans now) equals a full rescan of the flash. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../firmware/app/song.h"
#include "../firmware/app/store.h"

#define FLASH_SIZE (1024u * 1024u)

static uint8_t *flash;
static long ops, cut_at = -1;               /* the operation (counted from 1) that is cut; -1 none */
static int dead;                            /* after a cut: no flash operation does anything */
static int failures;
static long yields, table_checks, table_wrong;

void store_yield(void) { yields++; }

/* M5.1: the table the store keeps up to date equals a fresh scan of the flash (which it then is) */
static void table_check(void)
{
    static struct store_entry was[STORE_MAX_SONGS];
    static char slugs[STORE_MAX_SONGS][24];
    uint32_t n = store_count(), fb = store_free_blocks(), i, ok;
    if (dead)
        return;
    for (i = 0; i < n; i++) {
        was[i] = *store_get(i);
        snprintf(slugs[i], sizeof slugs[i], "%s", was[i].slug);
    }
    store_scan();
    ok = store_count() == n && store_free_blocks() == fb;
    for (i = 0; ok && i < n; i++) {
        const struct store_entry *e = store_get(i);
        ok = e->off == was[i].off && e->len == was[i].len && e->seq == was[i].seq && e->blocks == was[i].blocks &&
             !strcmp(e->slug, slugs[i]);
    }
    table_checks++;
    if (!ok && table_wrong++ < 3)
        printf("  the table differed from a rescan (check %ld)\n", table_checks);
}

const uint8_t *store_map(uint32_t off) { return off < FLASH_SIZE ? flash + off : 0; }

int store_erase(uint32_t off)
{
    if (dead)
        return -1;
    if (++ops == cut_at) {
        memset(flash + off, 0xFF, 2048);   /* half of it */
        dead = 1;
        return -1;
    }
    memset(flash + off, 0xFF, STORE_SECTOR);
    return 0;
}

int store_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i, m = n;
    if (dead)
        return -1;
    if (++ops == cut_at) {
        m = n / 2u;
        dead = 1;
    }
    for (i = 0; i < m; i++)
        flash[off + i] &= s[i];
    return dead ? -1 : 0;
}

static void power_on(void)
{
    dead = 0;
    cut_at = -1;
}

struct file {
    uint8_t *d;
    uint32_t n;
    char slug[24];
};

static struct file load(const char *path)
{
    struct file f = {0};
    struct song s;
    FILE *fp = fopen(path, "rb");
    long len;
    if (!fp) {
        fprintf(stderr, "%s: cannot read\n", path);
        exit(2);
    }
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    f.d = malloc((size_t)len);
    if (fread(f.d, 1, (size_t)len, fp) != (size_t)len)
        exit(2);
    fclose(fp);
    f.n = (uint32_t)len;
    if (song_peek(&s, f.d, f.n) != SONG_OK) {
        fprintf(stderr, "%s: not a song\n", path);
        exit(2);
    }
    strcpy(f.slug, s.slug);
    return f;
}

static void check(int ok, const char *what)
{
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    failures += !ok;
}

/* the stored copy of slug: 1 = equal to f, 0 = absent, -1 = something else */
static int stored_as(const char *slug, const struct file *f)
{
    uint32_t i;
    for (i = 0; i < store_count(); i++) {
        const struct store_entry *e = store_get(i);
        if (!strcmp(e->slug, slug))
            return e->len == f->n && !memcmp(store_file(e), f->d, f->n) ? 1 : -1;
    }
    return 0;
}

static int add(const struct file *f)
{
    int r = store_add(f->d, f->n, f->slug);
    table_check();
    return r;
}

static void wipe(void)
{
    memset(flash, 0xFF, FLASH_SIZE);
    store_scan();
}

/* the cut test: setup() makes the flash, act() is the operation cut at every point; ok(after) judges the outcome */
static void cuts(const char *name, void (*setup)(void), int (*act)(void), int (*ok)(void))
{
    uint8_t *before = malloc(FLASH_SIZE);
    long total, n, bad = 0;
    setup();
    memcpy(before, flash, FLASH_SIZE);
    ops = 0;
    act();
    total = ops;
    for (n = 1; n <= total; n++) {
        memcpy(flash, before, FLASH_SIZE);
        store_stop();                     /* (the flash changed behind the store's back: it starts again) */
        store_abort();
        store_scan();
        ops = 0;
        cut_at = n;
        dead = 0;
        act();
        power_on();
        store_stop();                     /* the FM-1 starts again (its RAM: no room being made) */
        store_abort();
        store_scan();
        if (!ok()) {
            if (bad < 3)
                printf("  cut at operation %ld of %ld: wrong outcome\n", n, total);
            bad++;
        }
    }
    {
        char t[160];
        snprintf(t, sizeof t, "%s: cut at each of %ld operations, %ld wrong", name, total, bad);
        check(!bad && total > 0, t);
    }
    free(before);
}

static struct file A11, A10, others[8];
static uint32_t nothers;

static int others_intact(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (stored_as(others[i].slug, &others[i]) != 1)
            return 0;
    return 1;
}

static const struct file *newf;          /* what the operation under test stores */

/* after a cut: the song can be stored again, and then is exactly the new file */
static int recovers(void)
{
    uint8_t *keep = malloc(FLASH_SIZE);
    int r;
    memcpy(keep, flash, FLASH_SIZE);
    r = add(newf) == STORE_OK && stored_as(newf->slug, newf) == 1;
    memcpy(flash, keep, FLASH_SIZE);
    store_scan();
    free(keep);
    return r;
}

static void setup_replace(void)
{
    uint32_t i;
    wipe();
    add(&A11);
    for (i = 0; i < nothers && i < 2u; i++)
        add(&others[i]);
}
static int act_replace(void) { return add(newf); }
static int ok_replace(void)
{
    int a = stored_as(A10.slug, &A10) == 1 || stored_as(A11.slug, &A11) == 1;
    return a && others_intact(nothers < 2u ? nothers : 2u) && recovers();
}

static uint32_t tight_others;
static void setup_tight(void)
{
    uint8_t *pad;
    uint32_t i, k;
    wipe();
    add(&A10);                            /* the big version stored; the small one (A11) must take its room */
    for (i = 0; i < nothers; i++)
        add(&others[i]);
    /* fill the rest with copies of the short song under new slugs, until A11 beside A10 has no room */
    pad = malloc(STORE_FILE_MAX);
    for (k = 0;; k++) {
        struct song s;
        memcpy(pad, A11.d, A11.n);
        song_peek(&s, pad, A11.n);
        snprintf((char *)s.slug, 24, "pad%u", k);   /* inside the copy: a new slug (the store reads it, not the CRC) */
        if (store_add(pad, A11.n, (const char *)s.slug) != STORE_OK)
            break;
    }
    free(pad);
    tight_others = nothers;
    (void)i;
}
static int ok_tight(void)
{
    int old = stored_as(A10.slug, &A10), now = stored_as(A11.slug, &A11);
    return (old == 1 || now == 1 || old == 0) && others_intact(tight_others) && recovers();   /* old, new or gone */
}

static void setup_new(void)
{
    wipe();
    add(&others[0]);
    add(&others[1]);
}
static int ok_new(void)
{
    int a = stored_as(A11.slug, &A11);
    return a >= 0 && others_intact(2) && recovers();
}

static void setup_delete(void) { setup_replace(); }
static int act_delete(void) { return store_delete(A11.slug); }
static int ok_delete(void)
{
    int a = stored_as(A11.slug, &A11);
    return a >= 0 && others_intact(nothers < 2u ? nothers : 2u);
}

static void wr32t(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* fill songs: copies of a short song under the slugs fill0, fill1, ... (the bytes are kept, to compare) */
static struct file fills[STORE_MAX_SONGS];
static uint8_t fill_gone[STORE_MAX_SONGS];
static uint32_t nfill;

static int add_fill(const struct file *base, uint32_t k)
{
    struct song s;
    struct file *f = &fills[k];
    free(f->d);
    f->d = malloc(base->n);
    f->n = base->n;
    memcpy(f->d, base->d, base->n);
    song_peek(&s, f->d, f->n);
    snprintf((char *)s.slug, 24, "fill%u", k);
    strcpy(f->slug, s.slug);
    fill_gone[k] = 0;
    nfill = k + 1u;
    if (store_add(f->d, f->n, f->slug) == STORE_OK)
        return STORE_OK;
    fill_gone[k] = 1;                     /* (not stored: full) */
    return STORE_E_FULL;
}

static int fills_intact(void)
{
    uint32_t k;
    for (k = 0; k < nfill; k++)
        if (stored_as(fills[k].slug, &fills[k]) != (fill_gone[k] ? 0 : 1))
            return 0;
    return 1;
}

/* the area filled with short songs, then every other one deleted: room in many sectors, no whole free run */
static void setup_compact(void)
{
    uint32_t k;
    wipe();
    for (k = 0; k < STORE_MAX_SONGS; k++)
        if (add_fill(&others[k % nothers], k) != STORE_OK)
            break;
    for (k = 0; k + 1u < nfill; k += 2u) {
        store_delete(fills[k].slug);
        fill_gone[k] = 1;
    }
}
static int ok_compact(void)
{
    int a = stored_as(newf->slug, newf);
    return a >= 0 && fills_intact() && recovers();
}

int main(int argc, char **argv)
{
    uint32_t i, k, n;
    int r;
    if (argc < 4) {
        fprintf(stderr, "usage: store_test A11.fm1s A10.fm1s OTHER.fm1s...\n");
        return 2;
    }
    flash = malloc(FLASH_SIZE);
    A11 = load(argv[1]);
    A10 = load(argv[2]);
    if (strcmp(A11.slug, A10.slug) || A11.n == A10.n) {
        fprintf(stderr, "the first two must be the same song in two versions\n");
        return 2;
    }
    for (i = 3; i < (uint32_t)argc && nothers < 8u; i++)
        others[nothers++] = load(argv[i]);

    /* 1 */
    srand(7);
    for (i = 0; i < FLASH_SIZE; i++)
        flash[i] = (uint8_t)rand();
    memcpy(flash + STORE_LO + 0x3000u, "FM1R", 4);   /* a head-like start in the junk */
    ops = 0;
    r = store_scan();
    check(r == 0 && ops == 0, "junk in the area: no songs, and the scan wrote nothing");
    r = add(&A11);
    check(r == STORE_OK && store_count() == 1u && stored_as(A11.slug, &A11) == 1, "a song stored over junk");

    /* 2 */
    wipe();
    check(store_scan() == 0 && store_free_sectors() == STORE_SECTORS, "an empty area: no songs, 69 sectors free");
    for (i = 0; i < nothers; i++)
        add(&others[i]);
    add(&A11);
    n = store_count();
    for (k = 0, r = 1; k < nothers; k++)
        r &= stored_as(others[k].slug, &others[k]) == 1;
    check(n == nothers + 1u && r && stored_as(A11.slug, &A11) == 1, "songs stored byte for byte, listed once each");
    check(add(&A10) == STORE_OK && store_count() == n && stored_as(A10.slug, &A10) == 1,
          "the same song again (another version): replaced, still listed once");
    check(store_delete(A10.slug) == STORE_OK && store_count() == n - 1u && stored_as(A10.slug, &A10) == 0,
          "deleted: gone from the list");
    check(store_delete(A10.slug) == STORE_E_NOT_FOUND, "deleting it again: not found");
    check(add(&A11) == STORE_OK && store_count() == n, "added again");

    /* 3 */
    for (k = 0; k < 200u; k++) {
        static uint8_t pad[STORE_FILE_MAX];
        struct song s;
        memcpy(pad, others[k % nothers].d, others[k % nothers].n);
        song_peek(&s, pad, others[k % nothers].n);
        snprintf((char *)s.slug, 24, "fill%u", k);
        r = store_add(pad, others[k % nothers].n, s.slug);
        if (r != STORE_OK)
            break;
    }
    for (k = 0, n = 1; k < nothers; k++)
        n &= stored_as(others[k].slug, &others[k]) == 1;
    check(r == STORE_E_FULL && n && stored_as(A11.slug, &A11) == 1,
          "the area full: STORE_E_FULL, every song stored before still whole");
    {
        char t[96];
        snprintf(t, sizeof t, "  (%u songs in %u sectors)", store_count(), STORE_SECTORS);
        puts(t);
    }

    /* 4, 5, 6, 7 */
    newf = &A10;
    cuts("replace a song", setup_replace, act_replace, ok_replace);
    newf = &A11;
    setup_tight();
    check(add(&A11) == STORE_OK && stored_as(A11.slug, &A11) == 1, "no room for both copies: the older one makes way");
    cuts("replace a song with no room for both copies", setup_tight, act_replace, ok_tight);
    cuts("delete a song", setup_delete, act_delete, ok_delete);
    cuts("add a new song", setup_new, act_replace, ok_new);

    /* 8 */
    wipe();
    nfill = 0;
    for (k = 0; k < STORE_MAX_SONGS; k++)
        if (add_fill(&others[k % nothers], k) != STORE_OK)
            break;
    {
        char t[128];
        snprintf(t, sizeof t, "short songs share sectors: %u of %u-byte songs fit (M4: 69 at most)", store_count(),
                 others[0].n);
        check(store_count() > 69u && fills_intact(), t);
    }

    /* 9 */
    setup_compact();
    check(store_largest_free() * STORE_SECTOR < STORE_HEAD + A10.n && store_free_blocks() * STORE_BLOCK >
          STORE_HEAD + A10.n, "the long song fits only by compacting (setup)");
    store_compactions = store_moves = 0;
    check(add(&A10) == STORE_OK && stored_as(A10.slug, &A10) == 1 && fills_intact() && store_moves > 0,
          "compacting: songs moved, sectors erased, the long song stored, every other song intact");
    {
        char t[96];
        snprintf(t, sizeof t, "  (%u songs moved, %u sectors erased)", store_moves, store_compactions);
        puts(t);
    }
    newf = &A10;
    cuts("add a long song by compacting", setup_compact, act_replace, ok_compact);
    newf = &A11;
    cuts("add a short song by compacting", setup_compact, act_replace, ok_compact);

    /* 10 */
    wipe();
    add(&others[0]);
    {
        uint32_t at;
        static const uint8_t junk[64];
        r = store_begin(A11.n, A11.slug, &at);
        n = r == STORE_OK && store_write(0, A11.d, A11.n / 2u) == STORE_OK;
        store_abort();
        store_scan();
        check(n && stored_as(A11.slug, &A11) == 0 && store_count() == 1u && others_intact(1),
              "receiving: a refused file leaves no song");
        r = store_begin(A11.n, A11.slug, &at);
        k = store_refused;
        check(r == STORE_OK && store_write(A11.n - 8u, junk, 64) == STORE_E_RANGE && store_refused == k + 1u,
              "receiving: a write past the room given is refused");
        store_refused = 0;
        store_abort();
        check(store_write(0, junk, 4) == STORE_E_STATE && store_commit() == STORE_E_STATE,
              "receiving: nothing to write or commit without store_begin");
        check(add(&A11) == STORE_OK && stored_as(A11.slug, &A11) == 1, "then a song stored over the leftovers");
    }

    /* 11 */
    wipe();
    {
        uint8_t h[STORE_HEAD];
        uint32_t at = STORE_LO + 5u * STORE_SECTOR;
        memset(h, 0xFF, sizeof h);
        memcpy(h, "FM1R", 4);
        wr32t(h + 4, 3);
        wr32t(h + 8, A11.n);
        wr32t(h + 12, song_crc32(A11.d, A11.n, 0));
        wr32t(h + 28, song_crc32(h, 28, 0));
        memcpy(flash + at, h, sizeof h);
        memcpy(flash + at + STORE_HEAD, A11.d, A11.n);
        store_scan();
        check(store_count() == 1u && stored_as(A11.slug, &A11) == 1, "an M4 record (FM1R) is read");
        check(store_delete(A11.slug) == STORE_OK && store_count() == 0, "an M4 record is deleted");
        for (k = 0; k < STORE_MAX_SONGS; k++)
            if (add_fill(&others[k % nothers], k) != STORE_OK)
                break;
        check(flash[at] == 0xFFu || !memcmp(flash + at, "FM2R", 4) || flash[at] == 0,
              "its room is taken back when the area fills");
    }

    /* 12 (M5.1): room made a step at a time, outside SEND */
    {
        uint32_t at, steps = 0, d = 0, t = 0;
        long most = 0, o;
        int busy;
        setup_compact();
        o = ops = 0;
        r = store_begin(A10.n, A10.slug, &at);
        check(r == STORE_BUSY && ops == 0 && store_busy(&d, &t) && t > 0,
              "no room without compacting: store_begin answers BUSY at once and writes nothing");
        while ((busy = store_step()) == STORE_BUSY) {
            if (ops - o > most)
                most = ops - o;
            o = ops;
            steps++;
        }
        if (ops - o > most)
            most = ops - o;
        table_check();
        {
            char t2[160];
            snprintf(t2, sizeof t2, "the room made in %u steps (planned about %u), at most %ld flash operations a step "
                     "(one erase, or one short song: its blocks' pages and two heads)", steps, t, most);
            check(busy == STORE_OK && steps > 0 && most <= 2 * STORE_SECTOR_BLOCKS + 2, t2);
        }
        r = store_begin(A10.n, A10.slug, &at);
        n = r == STORE_OK && store_write(0, A10.d, A10.n) == STORE_OK && store_commit() == STORE_OK;
        table_check();
        check(n && stored_as(A10.slug, &A10) == 1 && fills_intact(), "then the song is stored, every other one intact");
        setup_compact();
        r = store_begin(A10.n, A10.slug, &at);
        store_step();
        store_stop();
        table_check();
        check(r == STORE_BUSY && fills_intact() && !store_busy(0, 0) && add(&A10) == STORE_OK &&
              stored_as(A10.slug, &A10) == 1, "making room stopped half way (a delete, a power cut): nothing lost, it "
              "goes on next time");
    }

    /* 13 (M5.1): WIPE SONG STORE */
    {
        uint32_t sec;
        int bad = 0, blank = 1;
        setup_compact();
        for (sec = 0; sec < STORE_SECTORS; sec++)
            bad |= store_wipe_sector(sec) != STORE_OK;
        for (k = STORE_LO; k < STORE_HI; k++)
            blank &= flash[k] == 0xFFu;
        check(!bad && blank && store_scan() == 0 && store_free_blocks() == STORE_BLOCKS && add(&A11) == STORE_OK,
              "wipe: every sector of the area blank, no songs, and a song stores again");
        memset(flash + STORE_LO, 0x5A, STORE_HI - STORE_LO);
        ops = 0;
        for (sec = 0; sec < STORE_SECTORS; sec++)
            bad |= store_wipe_sector(sec) != STORE_OK;
        check(!bad && ops == (long)STORE_SECTORS && store_scan() == 0 && flash[STORE_LO - 1u] == 0xFFu &&
              flash[STORE_HI] == 0xFFu, "wipe over junk: 69 erases, nothing outside the area");
        check(store_wipe_sector(STORE_SECTORS) == STORE_E_RANGE, "wipe: a sector past the area is refused");
    }

    {
        char t[160];
        snprintf(t, sizeof t, "the table kept up to date equals a full rescan, after each of %ld operations (and the "
                 "store's longer loops yielded %ld times)", table_checks, yields);
        check(!table_wrong && table_checks > 100, t);
    }
    check(store_refused == 0, "never an erase or write outside 0x97000..0xDBFFF");
    printf("%s\n", failures ? "FAILURES" : "ALL PASS");
    free(flash);
    return failures != 0;
}
