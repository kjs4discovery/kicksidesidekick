/* Licence: TBD (see D017) */
/* store_test: the song store (firmware/app/store.c) on the laptop, on a simulated flash, under ASan/UBSan
 * (tools/store_test.sh). The flash behaves as NOR: an erase sets a sector to 0xFF, a write can only clear bits. A
 * power cut at the Nth erase or write leaves that operation half done (half the sector erased, half the bytes
 * written) and nothing after it; then the "FM-1" starts again and scans.
 *
 *   store_test A.fm1s B.fm1s ...    A: a song in two versions (the 1.1 file, then its 1.0 file: same slug), the rest
 *                                   other songs. Checks, printing one line each:
 *   1. a flash full of junk (another firmware's leftovers): no songs, nothing written by a scan
 *   2. add, list, read back, delete, add again; a file is stored byte for byte
 *   3. the area fills up: STORE_E_FULL, and nothing that was stored is lost
 *   4. replacing a song (room for both copies), cut at every operation: the others intact, the song old or new
 *   5. replacing a song when only its own room is left, cut at every operation: the others intact, the song old,
 *      new or gone (the one case it may go)
 *   6. deleting, cut at every operation: the others intact, the song there or gone
 *   7. adding a new song, cut at every operation: the others intact, the song there or not
 *   and after every cut: a new add of the song works. Nothing is ever erased or written outside the area. */
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

static int add(const struct file *f) { return store_add(f->d, f->n, f->slug); }

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
        ops = 0;
        cut_at = n;
        dead = 0;
        act();
        power_on();
        store_scan();                     /* the FM-1 starts again */
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
    /* fill the rest with copies of a small song under new slugs, until A11 beside A10 has no room */
    pad = malloc(STORE_FILE_MAX);
    for (k = 0; store_largest_free() * STORE_SECTOR >= STORE_HEAD + A11.n; k++) {
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
    check(store_largest_free() * STORE_SECTOR < STORE_HEAD + A11.n && add(&A11) == STORE_OK &&
          stored_as(A11.slug, &A11) == 1, "no room for both copies: the older one makes way");
    cuts("replace a song with no room for both copies", setup_tight, act_replace, ok_tight);
    cuts("delete a song", setup_delete, act_delete, ok_delete);
    cuts("add a new song", setup_new, act_replace, ok_new);

    check(store_refused == 0, "never an erase or write outside 0x97000..0xDBFFF");
    printf("%s\n", failures ? "FAILURES" : "ALL PASS");
    free(flash);
    return failures != 0;
}
