/* SPDX-License-Identifier: GPL-3.0-or-later */
/* upreset_test: the user presets' store (firmware/app/upreset.c on fm1-base's device/storage.c) on a simulated NOR flash, under
 * ASan/UBSan (tools/upreset_test.sh). A power cut at the Nth flash operation of a save leaves that operation half done and
 * nothing after it; then the "FM-1" boots again (upreset_boot: reads only). Checks:
 *  1. junk in 0xDE000..0xDFFFF (and a flash of junk everywhere): all 30 slots empty, no crash, NOTHING written or erased
 *  2. save slots in turn, boot again: every one is there, the list is the stock presets then the used slots in slot order
 *  3. a save cut at EVERY flash operation, over both copies of the A/B pair (the first save, the second, the third that
 *     erases the first copy again): after the cut and a boot the object is wholly the old one or wholly the new one; the
 *     older copy survives a cut
 *  4. a stock slot index is refused; an object with a bad CRC / a wrong length / a wrong magic loads as all empty
 *  5. nothing outside 0xDE000..0xDFFFF is ever erased or written */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t nor[0x100000];
static long ops, cut_at = -1, erases, progs;
static int dead, fails;

static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static void outside(uint32_t off)
{
    if (off < 0xDE000u || off >= 0xE0000u) {
        printf("FAIL: flash write / erase outside the object: %#x\n", off);
        fails++;
    }
}
static int st_erase(uint32_t off)
{
    if (dead)
        return -9;
    outside(off);
    ops++;
    erases++;
    if (cut_at > 0 && ops == cut_at) {
        memset(nor + off, 0xFF, 2048);               /* half erased */
        dead = 1;
        return -9;
    }
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i, m = n;
    if (dead)
        return -9;
    outside(off);
    ops++;
    progs++;
    if (cut_at > 0 && ops == cut_at) {
        m = n / 2;                                   /* half the bytes */
        dead = 1;
    }
    if ((off & 0xFFu) + n > 256u) {
        printf("FAIL: page wrap at %#x\n", off);
        exit(1);
    }
    for (i = 0; i < m; i++)
        nor[off + i] &= s[i];
    return dead ? -9 : 0;
}

#include "../firmware/vendor/fm1-base/include/plat.h"
#include "../firmware/app/app.h"
#include "../firmware/vendor/fm1-base/device/storage.c"

int plat_store_load(uint32_t o, void *d, uint32_t max) { return st_load(o, d, max); }
int plat_store_save(uint32_t o, const void *s, uint32_t n) { return st_save(o, s, n); }

#include "../firmware/app/vsynth.h"
/* the engine side, stubbed: 30 stock presets, the record check as vsynth_record's first test */
uint32_t vsynth_presets(void) { return 30; }
static struct vs_preset stock[30];
const struct vs_preset *vsynth_stock(uint32_t i) { return &stock[i < 30 ? i : 0]; }
int vsynth_record(struct vs_preset *dst, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    if (!s || n < 14 || s[0] == 0 || s[0] > 1 || s[1] >= 2)
        return -1;
    memcpy(dst, src, sizeof *dst);
    return 0;
}
#include "../firmware/app/upreset.c"

#define CHECK(what, cond) do { int ok_ = (cond); printf("%-78s %s\n", what, ok_ ? "ok" : "FAIL"); if (!ok_) fails++; } while (0)

static struct vs_preset mk(const char *name, uint8_t seed)
{
    struct vs_preset p;
    uint32_t k;
    memset(&p, 0, sizeof p);
    p.version = 1;
    p.engine = seed & 1;
    for (k = 0; name[k] && k < 12; k++)
        p.name[k] = name[k];
    for (k = 0; k < VP_N; k++)
        p.v[k] = (uint8_t)(seed + k);
    return p;
}

static uint8_t *snapshot(void)             /* the 30 records as the RAM has them */
{
    static uint8_t buf[UP_SLOTS * UP_REC];
    uint32_t i;
    for (i = 0; i < UP_SLOTS; i++)
        memcpy(buf + i * UP_REC, slots[i], UP_REC);
    return buf;
}

int main(void)
{
    uint8_t before[UP_SLOTS * UP_REC], after[UP_SLOTS * UP_REC];
    uint32_t i;
    long total, k;
    for (i = 0; i < 30; i++) {
        stock[i] = mk("STOCK", (uint8_t)i);
        stock[i].name[5] = (char)('A' + i % 26);
    }
    /* 1 */
    srand(7);
    for (i = 0; i < sizeof nor; i++)
        nor[i] = (uint8_t)rand();
    ops = erases = progs = 0;
    upreset_boot();
    CHECK("1. junk everywhere: all 30 slots empty, nothing written or erased", upreset_count() == 0 && erases == 0 && progs == 0);
    memset(nor, 0xFF, sizeof nor);
    upreset_boot();
    CHECK("1. a blank flash: all empty", upreset_count() == 0 && pl_count() == 30);
    {   /* a valid header over junk records, the wrong length, a bad CRC, a wrong magic */
        uint8_t o[UP_BYTES];
        struct vs_preset p = mk("A", 1);
        memset(nor, 0xFF, sizeof nor);
        upreset_save(3, &p);
        upreset_boot();
        CHECK("2. one saved: boot finds it", upreset_count() == 1 && upreset_used(3));
        CHECK("2. the list: 30 stock, then U04", pl_count() == 31 && pl_slot(30) == 3 && pl_slot(5) == -1);
        up_pack(o, slots);
        CHECK("4. the container: the right length unpacks, a short or a long one does not",
              up_unpack(slots, o, UP_BYTES) == 1 && up_unpack(slots, o, UP_BYTES - 1) == 0 && up_unpack(slots, o, UP_BYTES + 1) == 0);
        o[0] ^= 1;
        CHECK("4. a wrong magic: all empty", up_unpack(slots, o, UP_BYTES) == 0 && upreset_count() == 0);
    }
    CHECK("4. a stock-range slot is refused", upreset_save(30, &stock[0]) < 0 && upreset_save(1000, &stock[0]) < 0);
    /* 4: a bit flipped in the stored payload: the A/B copy with the bad CRC is not used (storage.c), the other copy or nothing */
    memset(nor, 0xFF, sizeof nor);
    { struct vs_preset p = mk("ONE", 1); upreset_save(0, &p); }
    nor[0xDE000 + 256 + 40] ^= 0x04;
    nor[0xDF000 + 256 + 40] ^= 0x04;
    upreset_boot();
    CHECK("4. a rotten object (bad CRC in both copies): all empty", upreset_count() == 0);
    /* 2: fill the slots in turn */
    memset(nor, 0xFF, sizeof nor);
    for (i = 0; i < 30; i++) {
        char nm[13];
        struct vs_preset p;
        snprintf(nm, sizeof nm, "SOUND %02u", i + 1);
        p = mk(nm, (uint8_t)(10 + i));
        if (upreset_save(i, &p)) {
            printf("FAIL: save %u\n", i);
            fails++;
        }
    }
    upreset_boot();
    {
        char lab[24];
        int ok = upreset_count() == 30 && pl_count() == 60;
        pl_label(30 + 4, lab);
        ok = ok && !strcmp(lab, "U05 SOUND 05") && pl_slot(59) == 29 && pl_index_of_slot(29) == 59;
        CHECK("2. all 30 saved, a fresh boot: 30 + 30 in the list, U05 is \"U05 SOUND 05\"", ok);
    }
    /* 3: cuts */
    {
        struct vs_preset p1 = mk("NEW ONE", 90), p2 = mk("NEWER", 91), p3 = mk("NEWEST", 92);
        struct vs_preset *seq[3] = {&p1, &p2, &p3};
        uint32_t slot_of[3] = {7, 7, 8};
        int round, bad = 0, n_cuts = 0;
        for (round = 0; round < 3; round++) {      /* round 0 writes copy B, 1 copy A (erased again), 2 B again */
            static uint8_t flash_keep[sizeof nor];
            upreset_boot();
            memcpy(before, snapshot(), sizeof before);
            memcpy(flash_keep, nor, sizeof nor);
            ops = 0;
            cut_at = -1;
            dead = 0;
            upreset_save(slot_of[round], seq[round]);
            total = ops;
            memcpy(after, snapshot(), sizeof after);
            for (k = 1; k <= total; k++) {
                memcpy(nor, flash_keep, sizeof nor);
                ops = 0;
                cut_at = k;
                dead = 0;
                upreset_save(slot_of[round], seq[round]);
                cut_at = -1;
                dead = 0;
                ops = 0;
                upreset_boot();                    /* the FM-1 starts again */
                if (memcmp(snapshot(), before, sizeof before) && memcmp(snapshot(), after, sizeof after))
                    bad++;
                n_cuts++;
            }
            memcpy(nor, flash_keep, sizeof nor);    /* the full save, for the next round */
            upreset_boot();
            upreset_save(slot_of[round], seq[round]);
        }
        CHECK("3. a save cut at every flash operation, three rounds (both copies): old or new, never mixed", bad == 0 && n_cuts > 6);
        printf("   (%d cuts; a save is %ld operations)\n", n_cuts, total);
    }
    /* 5: nothing outside the object (checked in every erase / program above) */
    CHECK("5. no flash operation outside 0xDE000..0xDFFFF", fails == 0);
    printf(fails ? "upreset_test: FAILED (%d)\n" : "upreset_test: all ok\n", fails);
    return fails != 0;
}
