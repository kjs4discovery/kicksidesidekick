/* Licence: TBD (see D017) */
/* The user presets (upreset.h). The container functions are pure; the device functions use plat_store_* (the host test
 * replaces them: tools/upreset_test.c). */
#include "upreset.h"
#include "plat.h"
#include "app.h"

static uint8_t slots[UP_SLOTS][UP_REC];           /* in .bss: built and kept in RAM */
static uint8_t obj[UP_BYTES] __attribute__((aligned(4)));   /* the object being written / read */
static struct vs_preset tmp;

void up_empty(uint8_t s[UP_SLOTS][UP_REC])
{
    uint32_t i, k;
    for (i = 0; i < UP_SLOTS; i++)
        for (k = 0; k < UP_REC; k++)
            s[i][k] = 0;
}

uint32_t up_pack(uint8_t *d, const uint8_t s[UP_SLOTS][UP_REC])
{
    uint32_t i, k;
    d[0] = (uint8_t)UP_MAGIC;
    d[1] = (uint8_t)(UP_MAGIC >> 8);
    d[2] = (uint8_t)(UP_MAGIC >> 16);
    d[3] = (uint8_t)(UP_MAGIC >> 24);
    d[4] = UP_VERSION;
    d[5] = UP_SLOTS;
    d[6] = d[7] = 0;
    for (i = 0; i < UP_SLOTS; i++)
        for (k = 0; k < UP_REC; k++)
            d[UP_HEAD + i * UP_REC + k] = s[i][k];
    return UP_BYTES;
}

int up_unpack(uint8_t s[UP_SLOTS][UP_REC], const uint8_t *d, int32_t n)
{
    uint32_t i, k;
    up_empty(s);
    if (n != (int32_t)UP_BYTES || d[0] != (uint8_t)UP_MAGIC || d[1] != (uint8_t)(UP_MAGIC >> 8) ||
        d[2] != (uint8_t)(UP_MAGIC >> 16) || d[3] != (uint8_t)(UP_MAGIC >> 24) || d[4] != UP_VERSION || d[5] != UP_SLOTS ||
        d[6] || d[7])
        return 0;
    for (i = 0; i < UP_SLOTS; i++)
        for (k = 0; k < UP_REC; k++)
            s[i][k] = d[UP_HEAD + i * UP_REC + k];
    return 1;
}

void upreset_boot(void)
{
    int32_t n = plat_store_load(OBJ_PRESETS, obj, sizeof obj);   /* (a read; < 0: none or bad) */
    uint32_t i;
    if (n < 0 || !up_unpack(slots, obj, n))
        up_empty(slots);
    for (i = 0; i < UP_SLOTS; i++)                 /* a record that does not check out is an empty slot (an older version
                                                    * loads with defaults for the new parameters: vsynth_record) */
        if (slots[i][0] && vsynth_record(&tmp, slots[i], UP_REC))
            slots[i][0] = 0, slots[i][1] = 0;
}

int upreset_save(uint32_t slot, const struct vs_preset *p)
{
    static uint8_t next[UP_SLOTS][UP_REC];
    const uint8_t *r = (const uint8_t *)p;
    uint32_t i, k;
    int e;
    if (slot >= UP_SLOTS)
        return -100;
    for (i = 0; i < UP_SLOTS; i++)
        for (k = 0; k < UP_REC; k++)
            next[i][k] = slots[i][k];
    for (k = 0; k < UP_REC; k++)
        next[slot][k] = r[k];
    up_pack(obj, next);
    e = plat_store_save(OBJ_PRESETS, obj, UP_BYTES);
    if (e == 0)
        for (k = 0; k < UP_REC; k++)
            slots[slot][k] = next[slot][k];
    return e;
}

void upreset_clear_ram(void) { up_empty(slots); }

int upreset_used(uint32_t slot) { return slot < UP_SLOTS && slots[slot][0] != 0; }

uint32_t upreset_count(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < UP_SLOTS; i++)
        n += slots[i][0] != 0;
    return n;
}

void upreset_name(uint32_t slot, char *d)
{
    uint32_t k;
    d[0] = 0;
    if (!upreset_used(slot))
        return;
    for (k = 0; k < VS_NAME && slots[slot][2 + k]; k++)
        d[k] = (char)slots[slot][2 + k];
    d[k] = 0;
}

/* ------------------------------------------------------------------ the list --- */
uint32_t pl_count(void) { return vsynth_presets() + upreset_count(); }

int32_t pl_slot(uint32_t i)
{
    uint32_t n = vsynth_presets(), s;
    if (i < n)
        return -1;
    i -= n;
    for (s = 0; s < UP_SLOTS; s++)
        if (slots[s][0] && i-- == 0)
            return (int32_t)s;
    return -1;
}

const struct vs_preset *pl_get(uint32_t i)
{
    int32_t s = pl_slot(i);
    if (s < 0)
        return vsynth_stock(i < vsynth_presets() ? i : 0);
    if (vsynth_record(&tmp, slots[s], UP_REC))
        return vsynth_stock(0);
    return &tmp;
}

void pl_label(uint32_t i, char *d)
{
    int32_t s = pl_slot(i);
    uint32_t k = 0, j;
    if (s < 0) {
        const char *nm = vsynth_stock(i < vsynth_presets() ? i : 0)->name;
        for (; k < VS_NAME && nm[k]; k++)
            d[k] = nm[k];
        d[k] = 0;
        return;
    }
    d[0] = 'U';
    d[1] = (char)('0' + (uint32_t)(s + 1) / 10u);
    d[2] = (char)('0' + (uint32_t)(s + 1) % 10u);
    d[3] = ' ';
    for (j = 0; j < VS_NAME && slots[s][2 + j]; j++)
        d[4 + j] = (char)slots[s][2 + j];
    d[4 + j] = 0;
}

uint32_t pl_index_of_slot(uint32_t slot)
{
    uint32_t s, idx = vsynth_presets();
    if (!upreset_used(slot))
        return 0;
    for (s = 0; s < slot; s++)
        idx += slots[s][0] != 0;
    return idx;
}

/* M10.1: a sound as a reference (a K4 choice kept in RAM): the stock preset's position in vsynth.c's table, or 0x100 + the user slot */
uint32_t pl_ref(uint32_t i)
{
    int32_t s = pl_slot(i);
    return s < 0 ? (i < vsynth_presets() ? i : 0u) : 0x100u + (uint32_t)s;
}

uint32_t pl_index_of_ref(uint32_t ref)
{
    if (ref >= 0x100u)
        return pl_index_of_slot(ref - 0x100u);   /* (a slot no longer used: the first sound) */
    return ref < vsynth_presets() ? ref : 0u;
}
